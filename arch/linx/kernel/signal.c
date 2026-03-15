// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/irqflags.h>
#include <linux/linkage.h>
#include <linux/ptrace.h>
#include <linux/resume_user_mode.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>

#include <asm/linx_ctx_tu_test.h>
#include <asm/ptrace.h>
#include <asm/ucontext.h>

struct rt_sigframe {
	struct siginfo info;
	struct ucontext uc;
};

static int linx_setup_sigcontext(struct sigcontext __user *sc,
				 const struct pt_regs *regs)
{
	/*
	 * LinxISA bring-up: user-visible register state is a flat array
	 * `regs[NUM_PTRACE_REG]`, matching `struct user_pt_regs`.
	 */
	return __copy_to_user(sc->sc_regs.regs, regs->regs, sizeof(regs->regs));
}

static int linx_restore_sigcontext(struct pt_regs *regs,
				   const struct sigcontext __user *sc)
{
	return __copy_from_user(regs->regs, sc->sc_regs.regs, sizeof(regs->regs));
}

static inline void linx_sync_resume_pc_ebarg(struct pt_regs *regs)
{
	unsigned long pc = regs->regs[PTR_PC];

	regs->ebarg_bpc_cur = pc;
	regs->ebarg_bpc_tgt = pc;
	regs->ebarg_tpc = pc;
}

SYSCALL_DEFINE0(rt_sigreturn)
{
	struct pt_regs *regs = current_pt_regs();
	struct rt_sigframe __user *frame;
	sigset_t set;

	/* Always make any pending restarted system calls return -EINTR. */
	current->restart_block.fn = do_no_restart_syscall;

	frame = (struct rt_sigframe __user *)user_stack_pointer(regs);
	if (!access_ok(frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;
	set_current_blocked(&set);

	if (linx_restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;
	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;

	/*
	 * Disable syscall restart heuristics for the restored context.
	 *
	 * Linx pt_regs uses orig_a0 for syscall arg0 preservation; use -1 as
	 * "not in syscall" (bring-up convention).
	 */
	regs->orig_a0 = (unsigned long)-1;
	linx_sync_resume_pc_ebarg(regs);

	return (long)regs->regs[PTR_R2];

badframe:
	force_sig(SIGSEGV);
	return -EFAULT;
}

static void __user *get_sigframe(struct ksignal *ksig, struct pt_regs *regs,
				 size_t frame_size)
{
	unsigned long sp = user_stack_pointer(regs);

	sp = sigsp(sp, ksig);
	sp = round_down(sp - frame_size, 16);
	return (void __user *)sp;
}

static int setup_rt_frame(struct ksignal *ksig, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	sigset_t *set = sigmask_to_save();
	int err = 0;

	/*
	 * Bring-up requirement: userspace must provide a restorer.
	 * This keeps NOMMU/FDPIC bring-up simple and avoids vDSO dependencies.
	 */
	if (!(ksig->ka.sa.sa_flags & SA_RESTORER) || !ksig->ka.sa.sa_restorer)
		return -EINVAL;

	frame = get_sigframe(ksig, regs, sizeof(*frame));
	if (!access_ok(frame, sizeof(*frame)))
		return -EFAULT;

	if (ksig->ka.sa.sa_flags & SA_SIGINFO)
		err |= copy_siginfo_to_user(&frame->info, &ksig->info);

	/* Create the ucontext. */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	err |= __save_altstack(&frame->uc.uc_stack, user_stack_pointer(regs));
	err |= linx_setup_sigcontext(&frame->uc.uc_mcontext, regs);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		return -EFAULT;

	/* Set up registers for the signal handler. */
	regs->regs[PTR_R1] = (unsigned long)frame;
	regs->regs[PTR_R2] = (unsigned long)ksig->sig;
	regs->regs[PTR_R3] = (unsigned long)&frame->info;
	regs->regs[PTR_R4] = (unsigned long)&frame->uc;
	regs->regs[PTR_R10] = (unsigned long)ksig->ka.sa.sa_restorer; /* ra */
	regs->regs[PTR_PC] = (unsigned long)ksig->ka.sa.sa_handler;
	linx_sync_resume_pc_ebarg(regs);

	return 0;
}

static void handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	int ret = setup_rt_frame(ksig, regs);

	signal_setup_done(ret, ksig, 0);
}

static void linx_do_signal_or_restart(struct pt_regs *regs)
{
	unsigned long continue_addr = 0;
	unsigned long restart_addr = 0;
	long retval = 0;
	struct ksignal ksig;
	const bool in_syscall = regs->orig_a0 != (unsigned long)-1;

	/*
	 * Mirror Linux arch restart handling:
	 * - syscall return PC in pt_regs already points to the post-ACRC slot.
	 * - ACRC is a 4-byte instruction in the current Linx userspace ABI.
	 */
	if (in_syscall) {
		continue_addr = regs->regs[PTR_PC];
		restart_addr = continue_addr - 4;
		retval = (long)regs->regs[PTR_R2];

		switch (retval) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
		case -ERESTART_RESTARTBLOCK:
			regs->regs[PTR_R2] = regs->orig_a0;
			regs->regs[PTR_PC] = restart_addr;
			break;
		default:
			break;
		}

		/* Exit path consumed syscall context from this trap frame. */
		regs->orig_a0 = (unsigned long)-1;
		linx_sync_resume_pc_ebarg(regs);
	}

	if (get_signal(&ksig)) {
		/*
		 * If restart was prepared but this signal should interrupt it,
		 * roll back to the post-syscall PC and report EINTR.
		 */
		if (in_syscall &&
		    regs->regs[PTR_PC] == restart_addr &&
		    (retval == -ERESTARTNOHAND ||
		     retval == -ERESTART_RESTARTBLOCK ||
		     (retval == -ERESTARTSYS &&
		      !(ksig.ka.sa.sa_flags & SA_RESTART)))) {
			regs->regs[PTR_R2] = (unsigned long)-EINTR;
			regs->regs[PTR_PC] = continue_addr;
		}

		handle_signal(&ksig, regs);
		return;
	}

	/*
	 * restart_block syscall switch for the RESTARTBLOCK class.
	 * This keeps semantics aligned with generic Linux expectations.
	 */
	if (in_syscall &&
	    regs->regs[PTR_PC] == restart_addr &&
	    retval == -ERESTART_RESTARTBLOCK)
		regs->regs[PTR_R9] = __NR_restart_syscall;

	restore_saved_sigmask();
}

asmlinkage void do_notify_resume(struct pt_regs *regs)
{
	enum linx_ctx_tu_test_pending_reason pending_reason;

	if (!user_mode(regs))
		return;

	if (test_thread_flag(TIF_NOTIFY_RESUME))
		resume_user_mode_work(regs);

	/*
	 * Always run signal/restart arbitration on user-return:
	 * restart errno fixups must run even when no pending signal flags are set.
	 */
	linx_do_signal_or_restart(regs);

	/*
	 * Context-switch stress hook for t/u queue restore validation. Consume one
	 * pending trigger and pollute manager-bank EBARG state before handing off:
	 * - timer-irq mode: one timed sleep handoff
	 * - step-trap mode: one unconditional schedule() handoff
	 */
	pending_reason = linx_ctx_tu_test_take_pending_reason();
	if (pending_reason != LINX_CTX_TU_PENDING_NONE) {
		bool irq_was_disabled = irqs_disabled();

		if (irq_was_disabled)
			local_irq_enable();
		if (pending_reason == LINX_CTX_TU_PENDING_STEP_TRAP) {
			set_current_state(TASK_RUNNING);
			schedule();
		} else {
			/*
			 * The timer-IRQ stress only needs one scheduler handoff
			 * after the async user preemption. Using the generic
			 * timeout path currently pulls in hrtimer state that is
			 * not yet stable on Linx bring-up.
			 */
			set_current_state(TASK_RUNNING);
			schedule();
		}
		if (irq_was_disabled)
			local_irq_disable();
		/*
		 * Pollute manager-bank return state only after the scheduler
		 * handoff has finished. entry.S will immediately overwrite it
		 * from pt_regs before ACRE back to userspace.
		 */
		linx_ctx_tu_test_request_poison_on_user_return();
	}
}
