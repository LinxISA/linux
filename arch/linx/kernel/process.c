// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Chen Liqin <liqin.chen@sunplusct.com>
 *  Lennox Wu <lennox.wu@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/task_stack.h>
#include <linux/init.h>
#include <linux/tick.h>
#include <linux/ptrace.h>
#include <linux/uaccess.h>

#include <asm/unistd.h>
#include <asm/processor.h>
#include <asm/ssr.h>
#include <asm/stacktrace.h>
#include <asm/string.h>
#include <asm/switch_to.h>
#include <asm/thread_info.h>


#if defined(CONFIG_STACKPROTECTOR) && !defined(CONFIG_STACKPROTECTOR_PER_TASK)
#include <linux/stackprotector.h>
unsigned long __stack_chk_guard __read_mostly;
EXPORT_SYMBOL(__stack_chk_guard);
#endif

extern asmlinkage void ret_from_fork(void);
extern asmlinkage void ret_from_kernel_thread(void);

static struct pt_regs *linx_child_pt_regs(struct task_struct *p)
{
	unsigned long regs = (unsigned long)task_pt_regs(p);

	if (!IS_ENABLED(CONFIG_VMAP_STACK) && regs < PAGE_OFFSET)
		regs = (unsigned long)__va(regs);

	return (struct pt_regs *)regs;
}

void arch_cpu_idle(void)
{
	wait_for_interrupt();
	raw_local_irq_enable();
}

void __show_regs(struct pt_regs *regs)
{
	show_regs_print_info(KERN_DEFAULT);

	if (!user_mode(regs)) {
		pr_cont("bpc : %pS\n", (void *)regs->bpc);
		pr_cont(" ra : %pS\n", (void *)regs->ra);
	}

	pr_cont(" tpc : " REG_FMT " bpc : " REG_FMT " bpcn : " REG_FMT "\n",
		regs->tpc, regs->bpc, regs->bpcn);
	pr_cont(" ebarg : " REG_FMT "\n",
		regs->ebarg);
	pr_cont(" gp : " REG_FMT " tp : " REG_FMT "\n",
		regs->gp, regs->tp );
	pr_cont(" t1 : " REG_FMT " t2 : " REG_FMT " t3 : " REG_FMT " t4 : " REG_FMT "\n",
		regs->elpr0, regs->elpr1, regs->elpr2, regs->elpr3);
	pr_cont(" u1 : " REG_FMT " u2 : " REG_FMT " u3 : " REG_FMT " u4 : " REG_FMT "\n",
		regs->elpr4, regs->elpr5, regs->elpr6, regs->elpr7);
	pr_cont(" sp : " REG_FMT " a0 : " REG_FMT " a1 : " REG_FMT "\n",
		regs->sp, regs->a0, regs->a1);
	pr_cont(" a2 : " REG_FMT " a3 : " REG_FMT " a4 : " REG_FMT "\n",
		regs->a2, regs->a3, regs->a4);
	pr_cont(" a5 : " REG_FMT " a6 : " REG_FMT " a7 : " REG_FMT "\n",
		regs->a5, regs->a6, regs->a7);
	pr_cont(" ra : " REG_FMT " s0 : " REG_FMT " s1 : " REG_FMT "\n",
		regs->ra, regs->s0, regs->s1);
	pr_cont(" s2 : " REG_FMT " s3 : " REG_FMT " s4 : " REG_FMT "\n",
		regs->s2, regs->s3, regs->s4);
	pr_cont(" s5 : " REG_FMT " s6 : " REG_FMT " s7 : " REG_FMT "\n",
		regs->s5, regs->s6, regs->s7);
	pr_cont(" s8 : " REG_FMT " x0 : " REG_FMT " x1: " REG_FMT "\n",
		regs->s8, regs->x0, regs->x1);
	pr_cont(" x2: " REG_FMT " x3 : " REG_FMT "\n",
		regs->x2, regs->x3);

	pr_cont(" cstate : " REG_FMT " traparg0 : " REG_FMT " trapno : " REG_FMT "\n",
		regs->cstate, regs->traparg0, regs->trapno);
	pr_cont(" orig_a0 : " REG_FMT " orig_bpc : " REG_FMT " orig_tpc : " REG_FMT "\n",
		regs->orig_a0, regs->orig_bpc, regs->orig_tpc);

}
void show_regs(struct pt_regs *regs)
{
	__show_regs(regs);
	if (!user_mode(regs))
		dump_backtrace(regs, NULL, KERN_DEFAULT);
}

void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long sp)
{
	unsigned long old_cstate = regs->cstate;
	unsigned long old_tpc = regs->tpc;
	unsigned long old_bpc = regs->bpc;

	/* called by `load_elf_binary` */
	pr_err("Linx dbg: start_thread enter pc=%lx sp=%lx old_cstate=%lx old_tpc=%lx old_bpc=%lx\n",
	       pc, sp, old_cstate, old_tpc, old_bpc);

	memset(regs, 0, sizeof(*regs));

	regs->cstate = CSTATE_ACR2 | CSTATE_I;
	/*
	 * Since the epc have been changed, the bstate of the exception block
	 * should be discarded also.
	 */
	regs->bpc = pc;
	regs->tpc = pc;
	// regs->ebstate.rra = RRAT_DEFAULT;
	regs->sp = sp;
#ifdef CONFIG_LINX_INTC
	current_thread_info()->flags &= ~_TIF_WORK_MASK;
#endif
	pr_err("Linx dbg: start_thread exit new_cstate=%lx new_tpc=%lx new_bpc=%lx new_sp=%lx\n",
	       regs->cstate, regs->tpc, regs->bpc, regs->sp);
}

void flush_thread(void)
{
}

#ifdef CONFIG_LINX_INTC
static bool linx_trace_ret_from_exception_enabled;

static int __init linx_trace_ret_from_exception_setup(char *str)
{
	linx_trace_ret_from_exception_enabled =
		!str || !(str[0] == '0' && str[1] == '\0');
	return 0;
}
early_param("linx_trace_ret_from_exception",
	    linx_trace_ret_from_exception_setup);

asmlinkage void linx_trace_ret_from_exception(struct pt_regs *regs)
{
	if (linx_trace_ret_from_exception_enabled && current && current->pid == 1) {
		pr_err("Linx dbg: ret_from_exception trace pid=1 flags=%lx pt_cstate=%lx pt_tpc=%lx pt_bpc=%lx pt_sp=%lx live_ssr_cstate=%lx user_mode=%d\n",
		       current_thread_info()->flags,
		       regs->cstate, regs->tpc, regs->bpc, regs->sp,
		       ssr_read(SSR_CSTATE), user_mode(regs) ? 1 : 0);
	}
}
#endif

int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src)
{
	*dst = *src;
	return 0;
}

int copy_thread(struct task_struct *p, const struct kernel_clone_args *args)
{
	unsigned long clone_flags = args->flags;
	unsigned long usp = args->stack;
	unsigned long tls = args->tls;
	struct pt_regs *childregs = linx_child_pt_regs(p);

	/* p->thread holds context to be restored by __switch_to() */
	if (unlikely(args->fn)) {
		/* Kernel thread */
		memset(childregs, 0, sizeof(struct pt_regs));
		childregs->gp = ssr_read(SSR_GP);

		childregs->cstate = CSTATE_ACR1 | CSTATE_I;

		p->thread.ra = (unsigned long)ret_from_kernel_thread;
		p->thread.s[0] = (unsigned long)args->fn;
		p->thread.s[1] = (unsigned long)args->fn_arg;
	} else {
		*childregs = *(current_pt_regs());
		if (usp) /* User fork */
			childregs->sp = usp;
		if (clone_flags & CLONE_SETTLS)
			childregs->tp = tls;
		childregs->a0 = 0; /* Return value of fork() */
		childregs->cstate &= ~ECAUSE_BI_MASK;
		p->thread_info.user_sp = childregs->sp;
		p->thread.ra = (unsigned long)ret_from_fork;
	}
	p->thread.sp = (unsigned long)childregs; /* kernel sp */
	p->thread_info.kernel_sp = (unsigned long)childregs + sizeof(*childregs);
	return 0;
}
