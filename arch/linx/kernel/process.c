// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kernel.h>
#include <linux/sched/debug.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/string.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/switch_to.h>

unsigned long __get_wchan(struct task_struct *p)
{
	(void)p;
	return 0;
}

extern void __noreturn linx_enter_user(struct pt_regs *regs);

asmlinkage void __noreturn linx_ret_from_fork(struct task_struct *prev)
{
	int (*fn)(void *);
	void *fn_arg;
	int ret = 0;

#ifdef CONFIG_LINX
	pr_err("Linx dbg: ret_from_fork enter pid=%d prev=%px fn=%px arg=%px\n",
	       current->pid, prev,
	       (void *)current->thread.kthread_fn,
	       (void *)current->thread.kthread_arg);
#endif

	schedule_tail(prev);

#ifdef CONFIG_LINX
	pr_err("Linx dbg: ret_from_fork after schedule_tail pid=%d\n", current->pid);
#endif

	fn = (int (*)(void *))current->thread.kthread_fn;
	fn_arg = (void *)current->thread.kthread_arg;

	current->thread.kthread_fn = 0;
	current->thread.kthread_arg = 0;

	if (fn)
		ret = fn(fn_arg);

	if (current->flags & PF_KTHREAD)
		do_exit(ret);

	/* user_mode_thread()/fork/clone: transition into userspace context. */
	linx_enter_user(task_pt_regs(current));
}

int copy_thread(struct task_struct *p, const struct kernel_clone_args *args)
{
	struct pt_regs *regs = task_pt_regs(p);

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
		regs->regs[PTR_R2] = 0;
		regs->orig_a0 = 0;
		p->thread.kthread_fn = 0;
		p->thread.kthread_arg = 0;
	}

	/* New tasks start in linx_ret_from_fork() on their kernel stack. */
	p->thread.ra = (unsigned long)linx_ret_from_fork;
	p->thread.sp = (unsigned long)regs;
	memset(p->thread.s, 0, sizeof(p->thread.s));

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
