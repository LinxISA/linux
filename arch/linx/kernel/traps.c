// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <linux/cpu.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/signal.h>
#include <linux/kdebug.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/irq.h>

#include <asm/asm-prototypes.h>
#include <asm/bug.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

int show_unhandled_signals = 1;

static DEFINE_SPINLOCK(die_lock);

void die(struct pt_regs *regs, const char *str)
{
	static int die_counter;
	int ret;

	oops_enter();

	spin_lock_irq(&die_lock);
	console_verbose();
	bust_spinlocks(1);

	pr_emerg("%s [#%d]\n", str, ++die_counter);
	print_modules();
	show_regs(regs);

	ret = notify_die(DIE_OOPS, str, regs, 0, regs->trapno, SIGSEGV);

	bust_spinlocks(0);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	spin_unlock_irq(&die_lock);
	oops_exit();

	if (in_interrupt())
		panic("Fatal exception in interrupt");
	if (panic_on_oops)
		panic("Fatal exception");
	if (ret != NOTIFY_STOP)
		do_exit(SIGSEGV);
}

void do_trap(struct pt_regs *regs, int signo, int code, unsigned long addr)
{
	struct task_struct *tsk = current;

	if (show_unhandled_signals && unhandled_signal(tsk, signo)
	    && printk_ratelimit()) {
		pr_info("%s[%d]: unhandled signal %d code 0x%x at 0x" REG_FMT,
			tsk->comm, task_pid_nr(tsk), signo, code, addr);
		print_vma_addr(KERN_CONT " in ", instruction_pointer(regs));
		pr_cont("\n");
		__show_regs(regs);
	}

	force_sig_fault(signo, code, (void __user *)addr);
}

static void do_trap_error(struct pt_regs *regs, int signo, int code,
	unsigned long addr, const char *str)
{
	current->thread.bad_cause = regs->trapno;

	if (user_mode(regs)) {
		do_trap(regs, signo, code, addr);
	} else {
		if (!fixup_exception(regs))
			die(regs, str);
	}
}

asmlinkage __visible void do_trap_unknown(struct pt_regs *regs)
{
	do_trap_error(regs, SIGILL, ILL_ILLTRP, regs->tpc, "Oops - unknow exception");
}

static inline unsigned long get_break_insn_length(unsigned long pc)
{
	bug_insn_t insn;

	if (get_kernel_nofault(insn, (bug_insn_t *)pc))
		return 0;

	return GET_INSN_LENGTH(insn);
}

static void skip_over_break(struct pt_regs *regs)
{
	/*
	 * Skip over breakpoint exception block and goto following block,
	 * discard the bstate of the exception block too.
	 * NOTE that this only work for single ebreak instruction block.
	 */
	if (ECAUSE_TRAPNUM(regs->trapno) == ECAUSE_TRAPNUM_BREAKPOINT_EXP)
	{
		regs->bpc = regs->bpcn;
		regs->tpc = regs->bpcn;
	} else {
		regs->tpc += get_break_insn_length(regs->tpc);
	}

	//regs->ebstate.rra = RRAT_DEFAULT;
}

static int copy_insn_32(struct pt_regs *regs, u32 *insn, unsigned long pc)
{
	const void __user *uaddr = (__force const void __user *)pc;

	if (!user_mode(regs))
		return get_kernel_nofault(*insn, (void *)pc);

	if (regs != task_pt_regs(current))
		return -EPERM;

	return copy_from_user_nofault(insn, uaddr, sizeof(*insn));
}

static int get_ebreak_imm(struct pt_regs *regs, u8 *imm)
{
	u32 insn;

	if (copy_insn_32(regs, &insn, regs->tpc))
		return -EFAULT;

	if ((insn & __INSN_LENGTH_MASK) == __INSN_LENGTH_32)
		*imm = EBREAK_IMM_32(insn);
	else
		*imm = EBREAK_IMM_16(insn);

	return 0;
}

asmlinkage __visible void do_trap_break(struct pt_regs *regs)
{
	u8 ebreak_imm;
	enum bug_trap_type bug = BUG_TRAP_TYPE_NONE;

	current->thread.bad_cause = regs->trapno;

	if (get_ebreak_imm(regs, &ebreak_imm)) {
		skip_over_break(regs);
		return;
	}

	if (user_mode(regs))
		force_sig_fault(SIGTRAP, TRAP_BRKPT, (void __user *)regs->tpc);
	else {
		bug = report_bug(regs->tpc, regs);
		if (bug == BUG_TRAP_TYPE_NONE && regs->tpc >= 4)
			bug = report_bug(regs->tpc - 4, regs);
	}

	if (!user_mode(regs) && bug == BUG_TRAP_TYPE_WARN)
		skip_over_break(regs);
	else if (!user_mode(regs) && bug == BUG_TRAP_TYPE_NONE && ebreak_imm == 0)
		skip_over_break(regs);
	else if (!user_mode(regs))
		die(regs, "Kernel BUG");
}
NOKPROBE_SYMBOL(do_trap_break);

#ifdef CONFIG_GENERIC_BUG
int is_valid_bugaddr(unsigned long pc)
{
	bug_insn_t insn;

	if (pc < VMALLOC_START)
		return 0;
	if (get_kernel_nofault(insn, (bug_insn_t *)pc))
		return 0;
	if ((insn & __INSN_LENGTH_MASK) == __INSN_LENGTH_32)
		return (insn == __BUG_INSN_32);
	else
		return ((insn & __COMPRESSED_INSN_MASK) == __BUG_INSN_16);
}
#endif /* CONFIG_GENERIC_BUG */

asmlinkage __visible void do_trap_data_exception(struct pt_regs *regs)
{
	unsigned int syndrome = ECAUSE_SYNDROME(regs->trapno);

	current->thread.bad_cause = regs->trapno;

	switch (syndrome) {
	case ECAUSE_DATA_SYD_LD_MISALIGN_FAULT:
		do_trap_error(regs, SIGBUS, BUS_ADRALN, regs->tpc,
				"Oops - load address misaligned");
		break;
	case ECAUSE_DATA_SYD_ST_AOP_MISALIGN_FAULT:
		do_trap_error(regs, SIGBUS, BUS_ADRALN, regs->tpc,
				"Oops - Store (or Atomic Operation) address misaligned");
		break;
	case ECAUSE_DATA_SYD_LD_ACCESS_FAULT:
		do_trap_error(regs, SIGSEGV, SEGV_ACCERR, regs->tpc,
				"Oops - load access fault");
		break;
	case ECAUSE_DATA_SYD_ST_AOP_ACCESS_FAULT:
		do_trap_error(regs, SIGSEGV, SEGV_ACCERR, regs->tpc,
				"Oops - Store (or Atomic Operation) address fault");
		break;
	case ECAUSE_DATA_SYD_LD_PAGE_FAULT:
	case ECAUSE_DATA_SYD_ST_AOP_PAGE_FAULT:
		do_page_fault(regs);
		break;
	default:
		do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->tpc,
				"Oops - instruction unknow trap");
		break;
	}

	return ;
}

asmlinkage __visible void do_trap_insn_exception(struct pt_regs *regs)
{
	unsigned int syndrome = ECAUSE_SYNDROME(regs->trapno);

	current->thread.bad_cause = regs->trapno;

	switch (syndrome) {
	case ECAUSE_INSN_SYD_ACCESS_FAULT:
		do_trap_error(regs, SIGSEGV, SEGV_ACCERR, regs->tpc,
				"Oops - instruction access fault");
		break;
	case ECAUSE_INSN_SYD_MISALIGN_FAULT:
		do_trap_error(regs, SIGBUS, BUS_ADRALN, regs->tpc,
				"Oops - instruction address misaligned");
		break;
	case ECAUSE_INSN_SYD_ILLEGAL_FAULT:
		do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->tpc,
				"Oops - instruction illegal");
		break;
	case ECAUSE_INSN_SYD_TRANS_FAULT: /* 这两个异常的意义待明确是否有必要 */
	case ECAUSE_INSN_SYD_PERM_FAULT:
		do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->tpc,
				"Oops - instruction trans/perm fault");
		break;
	case ECAUSE_INSN_SYD_PAGE_FAULT:
		do_page_fault(regs);
		break;
	default:
		do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->tpc,
				"Oops - instruction unknow trap");
		break;
	}

	return ;
}

asmlinkage __visible void do_trap_block_exception(struct pt_regs *regs)
{
	do_trap_error(regs, SIGILL, ILL_ILLOPC, regs->tpc, "Oops - block exception");
	return;
}
