// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/ptrace.h>
#include <linux/resume_user_mode.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

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

	return 0;
}

static void handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	int ret = setup_rt_frame(ksig, regs);

	signal_setup_done(ret, ksig, 0);
}

static void linx_do_signal(struct pt_regs *regs)
{
	struct ksignal ksig;

	if (get_signal(&ksig)) {
		handle_signal(&ksig, regs);
		return;
	}

	restore_saved_sigmask();
}

asmlinkage void do_notify_resume(struct pt_regs *regs)
{
	if (!user_mode(regs))
		return;

	if (test_thread_flag(TIF_SIGPENDING) ||
	    test_thread_flag(TIF_NOTIFY_SIGNAL)) {
		linx_do_signal(regs);
		return;
	}

	if (test_thread_flag(TIF_NOTIFY_RESUME))
		resume_user_mode_work(regs);
}

