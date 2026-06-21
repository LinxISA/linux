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
#ifdef CONFIG_ARCH_LINX
#include <asm/debug_uart.h>
#endif
#include <asm/processor.h>
#include <asm/ptrace.h>

int show_unhandled_signals = 1;

static DEFINE_SPINLOCK(die_lock);

#ifdef CONFIG_ARCH_LINX
static void linx_regs_debug_dump(const char *tag, struct pt_regs *regs)
{
	linx_debug_uart_puts(tag);
	linx_debug_uart_puts(" tpc=0x");
	linx_debug_uart_puthex_ulong(regs->tpc);
	linx_debug_uart_puts(" bpc=0x");
	linx_debug_uart_puthex_ulong(regs->bpc);
	linx_debug_uart_puts(" bpcn=0x");
	linx_debug_uart_puthex_ulong(regs->bpcn);
	linx_debug_uart_puts(" orig_tpc=0x");
	linx_debug_uart_puthex_ulong(regs->orig_tpc);
	linx_debug_uart_puts(" orig_bpc=0x");
	linx_debug_uart_puthex_ulong(regs->orig_bpc);
	linx_debug_uart_puts(" sp=0x");
	linx_debug_uart_puthex_ulong(user_stack_pointer(regs));
	linx_debug_uart_puts(" ra=0x");
	linx_debug_uart_puthex_ulong(regs->ra);
	linx_debug_uart_puts(" a0=0x");
	linx_debug_uart_puthex_ulong(regs->a0);
	linx_debug_uart_puts(" a1=0x");
	linx_debug_uart_puthex_ulong(regs->a1);
	linx_debug_uart_puts(" a2=0x");
	linx_debug_uart_puthex_ulong(regs->a2);
	linx_debug_uart_puts(" a3=0x");
	linx_debug_uart_puthex_ulong(regs->a3);
	linx_debug_uart_puts(" a7=0x");
	linx_debug_uart_puthex_ulong(regs->a7);
	linx_debug_uart_puts(" cstate=0x");
	linx_debug_uart_puthex_ulong(regs->cstate);
	linx_debug_uart_puts(" traparg0=0x");
	linx_debug_uart_puthex_ulong(regs->traparg0);
	linx_debug_uart_puts(" trapno=0x");
	linx_debug_uart_puthex_ulong(regs->trapno);
	linx_debug_uart_puts("\n");
}

static void linx_user_trap_debug_dump(struct pt_regs *regs, int signo,
				      int code, unsigned long addr)
{
	struct task_struct *tsk = current;

	linx_debug_uart_puts("LINX_USER_TRAP pid=0x");
	linx_debug_uart_puthex_ulong(task_pid_nr(tsk));
	linx_debug_uart_puts(" signo=0x");
	linx_debug_uart_puthex_ulong(signo);
	linx_debug_uart_puts(" code=0x");
	linx_debug_uart_puthex_ulong(code);
	linx_debug_uart_puts(" addr=0x");
	linx_debug_uart_puthex_ulong(addr);
	linx_regs_debug_dump("", regs);
}
#endif

void die(struct pt_regs *regs, const char *str)
{
	static int die_counter;
	int ret;

	oops_enter();

#ifdef CONFIG_ARCH_LINX
	linx_debug_uart_puts("LINX_DIE msg=");
	linx_debug_uart_puts(str);
	linx_regs_debug_dump("", regs);
#endif

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

	if (user_mode(regs) &&
	    (signo == SIGSEGV || signo == SIGBUS || signo == SIGILL)) {
#ifdef CONFIG_ARCH_LINX
		linx_user_trap_debug_dump(regs, signo, code, addr);
#endif
		pr_info("Linx dbg: user fault comm=%s pid=%d signo=%d code=0x%x addr=0x" REG_FMT " tpc=0x" REG_FMT " bpc=0x" REG_FMT " sp=0x" REG_FMT " trapno=0x" REG_FMT "\n",
			tsk->comm, task_pid_nr(tsk), signo, code, addr,
			regs->tpc, regs->bpc, user_stack_pointer(regs),
			regs->trapno);
	}

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

static void add_linx_bugaddr(unsigned long *candidates, unsigned int *count,
			     unsigned int max, unsigned long addr)
{
	if (addr && *count < max)
		candidates[(*count)++] = addr;
}

static void add_linx_bugaddr_before(unsigned long *candidates,
				    unsigned int *count, unsigned int max,
				    unsigned long base, unsigned long delta)
{
	if (base >= delta)
		add_linx_bugaddr(candidates, count, max, base - delta);
}

static enum bug_trap_type report_linx_bug(struct pt_regs *regs)
{
	unsigned long candidates[32];
	unsigned int count = 0;
	unsigned int i, j;

	/*
	 * Linx blockified BUG sites can report the ebreak TPC, the following
	 * block TPC, or one of the block PCs. The bug table is anchored at the
	 * inline BUG asm label, which is the BSTART.sys header immediately
	 * before ebreak.
	 */
	for (i = 0; i <= 16; i += 2)
		add_linx_bugaddr_before(candidates, &count,
					ARRAY_SIZE(candidates),
					regs->tpc, i);

	add_linx_bugaddr(candidates, &count, ARRAY_SIZE(candidates), regs->bpc);
	add_linx_bugaddr(candidates, &count, ARRAY_SIZE(candidates), regs->bpc + 2);
	add_linx_bugaddr(candidates, &count, ARRAY_SIZE(candidates), regs->bpc + 4);
	add_linx_bugaddr(candidates, &count, ARRAY_SIZE(candidates), regs->bpcn);
	for (i = 2; i <= 16; i += 2)
		add_linx_bugaddr_before(candidates, &count,
					ARRAY_SIZE(candidates),
					regs->bpcn, i);

	for (i = 0; i < count; i++) {
		enum bug_trap_type bug;

		for (j = 0; j < i; j++) {
			if (candidates[j] == candidates[i])
				break;
		}
		if (j != i)
			continue;

		bug = report_bug(candidates[i], regs);
		if (bug != BUG_TRAP_TYPE_NONE)
			return bug;
	}

	return BUG_TRAP_TYPE_NONE;
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
	else
		bug = report_linx_bug(regs);

	if (!user_mode(regs) && bug == BUG_TRAP_TYPE_WARN)
		skip_over_break(regs);
	else if (!user_mode(regs) && bug == BUG_TRAP_TYPE_NONE && ebreak_imm == 0)
		skip_over_break(regs);
	else if (!user_mode(regs))
		die(regs, "Kernel BUG");
}
NOKPROBE_SYMBOL(do_trap_break);

#ifdef CONFIG_GENERIC_BUG
static int is_linx_bug_insn(unsigned long pc)
{
	bug_insn_t insn;

	if (get_kernel_nofault(insn, (bug_insn_t *)pc))
		return 0;

	if ((insn & __INSN_LENGTH_MASK) == __INSN_LENGTH_32)
		return insn == __BUG_INSN_32;

	return (insn & __COMPRESSED_INSN_MASK) == __BUG_INSN_16;
}

int is_valid_bugaddr(unsigned long pc)
{
	if (pc < VMALLOC_START)
		return 0;

	/*
	 * __BUG_FLAGS labels the BSTART.sys header before ebreak, so bug table
	 * entries can name the block header while the real trap instruction is
	 * one instruction later.
	 */
	return is_linx_bug_insn(pc) || is_linx_bug_insn(pc + 2) ||
	       is_linx_bug_insn(pc + 4);
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
		do_page_fault(regs);
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
