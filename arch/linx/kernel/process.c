// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/sched/debug.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/switch_to.h>
#include <asm/debug_uart.h>

unsigned long __get_wchan(struct task_struct *p)
{
	(void)p;
	return 0;
}

extern void __noreturn linx_enter_user(struct pt_regs *regs);
extern void __noreturn linx_restore_user(struct pt_regs *regs);

asmlinkage void __noreturn linx_ret_from_fork(struct task_struct *prev)
{
	int (*fn)(void *);
	void *fn_arg;
	bool fork_child;
	int ret = 0;
	static unsigned int dbg_left = 1024;
	static unsigned int dbg_post_left = 1024;
	struct pt_regs *regs = task_pt_regs(current);

	if (unlikely(current->pid == 20)) {
		pr_err("linx: ret_from_fork pid20 entry fn=%px arg=%px pc=0x%lx sp=0x%lx r10=0x%lx r2=0x%lx orig_a0=0x%lx ecstate=0x%lx eb_tpc=0x%lx eb_lra=0x%lx\n",
		       (void *)current->thread.kthread_fn,
		       (void *)current->thread.kthread_arg,
		       regs->regs[PTR_PC], regs->regs[PTR_R1],
		       regs->regs[PTR_R10], regs->regs[PTR_R2],
		       regs->orig_a0, regs->ecstate,
		       regs->ebarg_tpc, regs->ebarg_lra);
	}

#ifdef CONFIG_LINX_DEBUG
	pr_err("Linx dbg: ret_from_fork enter pid=%d prev=%px fn=%px arg=%px\n",
	       current->pid, prev,
	       (void *)current->thread.kthread_fn,
	       (void *)current->thread.kthread_arg);
#endif
	if (unlikely(dbg_left > 0)) {
		pr_err("linx: ret_from_fork enter pid=%d prev=%px fn=%px arg=%px pc=0x%lx sp=0x%lx r2=0x%lx orig_a0=0x%lx flags=0x%lx\n",
		       current->pid, prev,
		       (void *)current->thread.kthread_fn,
		       (void *)current->thread.kthread_arg,
		       regs->regs[PTR_PC], regs->regs[PTR_R1],
		       regs->regs[PTR_R2], regs->orig_a0, current->flags);
		dbg_left--;
	}

	schedule_tail(prev);

	if (unlikely(dbg_post_left > 0)) {
		pr_err("linx: ret_from_fork post-tail pid=%d fn=%px arg=%px pc=0x%lx sp=0x%lx r2=0x%lx ecstate=0x%lx trapno=0x%lx traparg0=0x%lx\n",
		       current->pid,
		       (void *)current->thread.kthread_fn,
		       (void *)current->thread.kthread_arg,
		       regs->regs[PTR_PC], regs->regs[PTR_R1],
		       regs->regs[PTR_R2], regs->ecstate, regs->trapno, regs->traparg0);
		dbg_post_left--;
	}

#ifdef CONFIG_LINX_DEBUG
	pr_err("Linx dbg: ret_from_fork after schedule_tail pid=%d\n", current->pid);
#endif

	fn = (int (*)(void *))current->thread.kthread_fn;
	fn_arg = (void *)current->thread.kthread_arg;
	fork_child = !fn;

	current->thread.kthread_fn = 0;
	current->thread.kthread_arg = 0;

	if (fn)
		ret = fn(fn_arg);

	if (current->flags & PF_KTHREAD)
		do_exit(ret);

	/*
	 * Userspace fork/clone children must observe return value 0.
	 * Reinstate it at the final user-entry handoff in case any
	 * intermediate path clobbered the child pt_regs return slot.
	 */
	if (fork_child) {
		regs->regs[PTR_R2] = 0;
		regs->orig_a0 = (unsigned long)-1;
	}

	/* user_mode_thread()/fork/clone: transition into userspace context. */
	if (unlikely(current->pid == 20)) {
		pr_err("linx: ret_from_fork pid20 pre-enter fork_child=%d ret=%d pc=0x%lx sp=0x%lx r10=0x%lx r2=0x%lx orig_a0=0x%lx ecstate=0x%lx eb_tpc=0x%lx eb_lra=0x%lx bpc_cur=0x%lx bpc_tgt=0x%lx\n",
		       fork_child, ret,
		       regs->regs[PTR_PC], regs->regs[PTR_R1], regs->regs[PTR_R10],
		       regs->regs[PTR_R2], regs->orig_a0, regs->ecstate,
		       regs->ebarg_tpc, regs->ebarg_lra,
		       regs->ebarg_bpc_cur, regs->ebarg_bpc_tgt);
	}
	if (unlikely(dbg_post_left > 0)) {
		pr_err("linx: ret_from_fork enter-user pid=%d pc=0x%lx sp=0x%lx r2=0x%lx ecstate=0x%lx eb_tpc=0x%lx eb_lra=0x%lx\n",
		       current->pid,
		       regs->regs[PTR_PC], regs->regs[PTR_R1],
		       regs->regs[PTR_R2], regs->ecstate,
		       regs->ebarg_tpc, regs->ebarg_lra);
		dbg_post_left--;
	}
	if (fork_child)
		linx_restore_user(task_pt_regs(current));
	linx_enter_user(task_pt_regs(current));
}

int copy_thread(struct task_struct *p, const struct kernel_clone_args *args)
{
	struct pt_regs *regs = task_pt_regs(p);
	static unsigned int dbg_user_copy_left = 32;

	if (args->fn) {
		/*
		 * kernel_thread() and user_mode_thread() both start by running a
		 * kernel function. The difference is that user_mode_thread()
		 * expects to transition into userspace after execve().
		 */
		memset(regs, 0, sizeof(*regs));
		p->thread.kthread_fn = (unsigned long)args->fn;
		p->thread.kthread_arg = (unsigned long)args->fn_arg;
	} else {
		/* fork/clone from userspace: copy pt_regs and return 0 in child. */
		*regs = *task_pt_regs(current);
		{
			unsigned long resume_pc = regs->regs[PTR_PC];
			u16 hw = 0xffffu;

			/*
			 * Clone children are entered through linx_enter_user()
			 * (ACRE path), not the direct syscall-return path.
			 *
			 * If the trapped resume PC points at C.BSTOP (0x0000),
			 * skipping that terminator avoids replaying a stale
			 * post-syscall block footer in the freshly cloned child.
			 */
			if (!copy_from_user(&hw, (const void __user *)resume_pc, sizeof(hw)) &&
			    hw == 0x0000u)
				resume_pc += 2;
			regs->regs[PTR_PC] = resume_pc;
		}
		regs->regs[PTR_R2] = 0;
		/*
		 * Child enters userspace via linx_enter_user() (not syscall-return),
		 * so clear syscall-inflight markers and sanitize resume state.
		 */
		regs->orig_a0 = (unsigned long)-1;
		regs->trapno = 0;
		regs->traparg0 = 0;
		regs->ebarg_bpc_cur = regs->regs[PTR_PC];
		regs->ebarg_bpc_tgt = regs->regs[PTR_PC];
		regs->ebarg_tpc = regs->regs[PTR_PC];
		/*
		 * Fork/clone children resume in the parent's libc wrapper after
		 * syscall return; seed LRA from saved user RA so FRET paths do not
		 * observe a zero link target.
		 */
		regs->ebarg_lra = regs->regs[PTR_R10];
		do {
			unsigned long ecstate = linx_ssr_read_cstate();

			ecstate |= LINX_CSTATE_I_BIT;
			ecstate = (ecstate & ~LINX_CSTATE_ACR_MASK) | LINX_CSTATE_ACR_USER;
			regs->ecstate = ecstate;
		} while (0);
		p->thread.kthread_fn = 0;
		p->thread.kthread_arg = 0;
		if (unlikely(dbg_user_copy_left > 0)) {
			unsigned long usp_w0 = ~0UL;
			unsigned long usp_w1 = ~0UL;
			unsigned long usp_w2 = ~0UL;
			void __user *usp = (void __user *)regs->regs[PTR_R1];

			if (copy_from_user(&usp_w0, usp, sizeof(usp_w0)))
				usp_w0 = ~0UL;
			if (copy_from_user(&usp_w1, (void __user *)((unsigned long)usp + sizeof(unsigned long)), sizeof(usp_w1)))
				usp_w1 = ~0UL;
			if (copy_from_user(&usp_w2, (void __user *)((unsigned long)usp + 2 * sizeof(unsigned long)), sizeof(usp_w2)))
				usp_w2 = ~0UL;
			pr_err("linx: copy_thread user pid=%d parent=%d child_regs=%px parent_regs=%px parent_r2=0x%lx parent_uq0=0x%lx parent_uq1=0x%lx parent_tq0=0x%lx child_pc=0x%lx child_sp=0x%lx child_ra=0x%lx child_r2=0x%lx child_ecstate=0x%lx child_trapno=0x%lx child_targ0=0x%lx child_eb_tpc=0x%lx child_eb_lra=0x%lx child_uq0=0x%lx child_uq1=0x%lx child_tq0=0x%lx\n",
			       p->pid, current->pid,
			       regs, task_pt_regs(current), task_pt_regs(current)->regs[PTR_R2],
			       task_pt_regs(current)->ebarg_uq[0], task_pt_regs(current)->ebarg_uq[1],
			       task_pt_regs(current)->ebarg_tq[0],
			       regs->regs[PTR_PC], regs->regs[PTR_R1], regs->regs[PTR_R10], regs->regs[PTR_R2],
			       regs->ecstate, regs->trapno, regs->traparg0, regs->ebarg_tpc,
			       regs->ebarg_lra, regs->ebarg_uq[0], regs->ebarg_uq[1], regs->ebarg_tq[0]);
			pr_err("linx: copy_thread user-stack usp=0x%lx w0=0x%lx w1=0x%lx w2=0x%lx\n",
			       regs->regs[PTR_R1], usp_w0, usp_w1, usp_w2);
			dbg_user_copy_left--;
		}
	}

	/* New tasks start in linx_ret_from_fork() on their kernel stack. */
	p->thread.ra = (unsigned long)linx_ret_from_fork;
	p->thread.sp = (unsigned long)regs;
	memset(p->thread.s, 0, sizeof(p->thread.s));
	p->thread.ebarg0 = 0;
	/*
	 * Fresh tasks first resume in linx_ret_from_fork() via __switch_to.
	 * Seed the kernel-side EBARG PC/LRA fields to that entry point so the
	 * first schedule_tail()/finish_task_switch() return chain does not
	 * observe a zero link target before userspace pt_regs state is restored.
	 */
	p->thread.ebarg_bpc_cur = p->thread.ra;
	p->thread.ebarg_bpc_tgt = p->thread.ra;
	p->thread.ebarg_tpc = p->thread.ra;
	p->thread.ebarg_lra = p->thread.ra;
	memset(p->thread.ebarg_tq, 0, sizeof(p->thread.ebarg_tq));
	memset(p->thread.ebarg_uq, 0, sizeof(p->thread.ebarg_uq));
	p->thread.ebarg_lb = 0;
	p->thread.ebarg_lc = 0;
	p->thread.ebarg_ext_ptr = 0;
	p->thread.ebarg_ext_meta = 0;

	/*
	 * Early bring-up probe: log a few copy_thread products so we can
	 * correlate scheduler next-task pointers with thread context setup.
	 */
	do {
		static int dbg_left = 8;

		if (dbg_left <= 0)
			break;
		dbg_left--;
		linx_debug_uart_puts("\n[linx copy_thread] p=");
		linx_debug_uart_puthex_ulong((unsigned long)p);
		linx_debug_uart_puts(" ra=");
		linx_debug_uart_puthex_ulong(p->thread.ra);
		linx_debug_uart_puts(" sp=");
		linx_debug_uart_puthex_ulong(p->thread.sp);
		linx_debug_uart_puts(" fn=");
		linx_debug_uart_puthex_ulong((unsigned long)args->fn);
		linx_debug_uart_puts(" idle=");
		linx_debug_uart_puthex_ulong((unsigned long)args->idle);
		linx_debug_uart_puts("\n");
	} while (0);

	return 0;
}

void flush_thread(void)
{
}

void show_regs(struct pt_regs *regs)
{
	if (!regs) {
		pr_info("show_regs: (null)\n");
		return;
	}

	pr_info("show_regs: pc=%lx sp=%lx\n",
		regs->regs[PTR_PC], regs->regs[PTR_R1]);
}

void show_stack(struct task_struct *task, unsigned long *sp, const char *loglvl)
{
	(void)task;
	(void)sp;
	(void)loglvl;
}
