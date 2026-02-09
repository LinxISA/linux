// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>

#include <asm/debug_uart.h>
#include <asm/irq_regs.h>
#include <asm/ptrace.h>
#include <asm/ssr.h>

/*
 * Called from the EVBASE trap vector.
 *
 * Current bring-up model:
 * - Linux runs in ACR1 (kernel ring). Interrupts and syscalls route to ACR1.
 * - QEMU raises a single timer interrupt (IRQ0) via IPENDING_ACR1 bit0.
 * - The trap vector saves GPRs into pt_regs and passes the pointer here.
 */

extern void linx_timer_handle_irq(void);

asmlinkage void linx_do_trap(struct pt_regs *regs);

extern void * const sys_call_table[__NR_syscalls];

/* Trap number encoding (bring-up profile; keep in sync with QEMU). */
#define LINX_TRAPNO_E_BIT		(1ull << 63)
#define LINX_TRAPNO_CAUSE_SHIFT		8u
#define LINX_TRAPNO_TRAPNUM_MASK	0xffu

enum {
	LINX_TRAPNUM_E_INST	= 1,
	LINX_TRAPNUM_E_DATA	= 2,
	LINX_TRAPNUM_E_BLOCK	= 3,
	LINX_TRAPNUM_E_SCALL	= 16,
};

enum {
	LINX_SCT_SYS		= 1,
};

static inline u8 linx_trapno_trapnum(u64 trapno)
{
	return (u8)(trapno & LINX_TRAPNO_TRAPNUM_MASK);
}

static inline u8 linx_trapno_cause(u64 trapno)
{
	return (u8)((trapno >> LINX_TRAPNO_CAUSE_SHIFT) & 0xffu);
}

static void linx_handle_syscall(struct pt_regs *regs)
{
	unsigned long nr = regs->regs[PTR_R9];
	unsigned long arg0 = regs->regs[PTR_R2];

	/* Preserve arg0; a0 is also the return value register. */
	regs->orig_a0 = arg0;

	if (nr < __NR_syscalls) {
		long ret;
		asmlinkage long (*fn)(const struct pt_regs *);

		fn = (asmlinkage long (*)(const struct pt_regs *))sys_call_table[nr];
		ret = fn(regs);
		regs->regs[PTR_R2] = (unsigned long)ret;
	} else {
		regs->regs[PTR_R2] = (unsigned long)-ENOSYS;
	}

	/*
	 * QEMU saves EBPC=block start and EBPCN=pc_next. Returning via ACRE
	 * resumes at EBPC, so advance EBPC to EBPCN to skip the trapping insn.
	 */
	linx_ssr_write_ebpc_acr1(regs->ebpcn);
	regs->regs[PTR_PC] = regs->ebpcn;
}

asmlinkage void linx_do_trap(struct pt_regs *regs)
{
	static bool once;
	const u64 trapno = regs->trapno;
	const bool is_e = (trapno & LINX_TRAPNO_E_BIT) != 0;
	const u8 trapnum = linx_trapno_trapnum(trapno);
	const u8 cause = linx_trapno_cause(trapno);

	if (!once) {
		once = true;
		linx_debug_uart_puts("[TRAP]\n");
	}

	u64 pending = linx_ssr_read_ipending_acr1();

	if (pending & 1ull) {
		struct pt_regs *old_regs = set_irq_regs(regs);

		/* Acknowledge IRQ0 (timer0). */
		linx_ssr_write_eoiei_acr1(0);

		irq_enter();
		linx_timer_handle_irq();
		irq_exit();

		set_irq_regs(old_regs);

		/*
		 * QEMU models EBPC=block start and EBPCN=pc_next for trap
		 * delivery. Returning via ACRE resumes at EBPC, so advance EBPC
		 * to EBPCN to resume after the interrupted instruction/block.
		 */
		linx_ssr_write_ebpc_acr1(regs->ebpcn);
		regs->regs[PTR_PC] = regs->ebpcn;
		return;
	}

	if (is_e && trapnum == LINX_TRAPNUM_E_SCALL) {
		/*
		 * Bring-up syscall path: treat ACRC service requests as Linux
		 * syscalls.
		 */
		if (regs->traparg0 != LINX_SCT_SYS) {
			pr_err("linx: unexpected service request: traparg0=%lu etpc=0x%lx\n",
			       regs->traparg0, regs->etpc);
			return;
		}
		if (user_mode(regs)) {
			linx_handle_syscall(regs);
			return;
		}

		pr_err("linx: syscall trap from non-user context: etpc=0x%lx ecstate=0x%lx\n",
		       regs->etpc, regs->ecstate);
		return;
	}

	if (is_e && trapnum == LINX_TRAPNUM_E_DATA) {
		pr_err("linx: E_DATA: cause=0x%x traparg0=0x%lx ebpc=0x%lx etpc=0x%lx\n",
		       cause, regs->traparg0, regs->ebpc, regs->etpc);
		return;
	}

	if (is_e && trapnum == LINX_TRAPNUM_E_INST) {
		pr_err("linx: E_INST: cause=0x%x traparg0=0x%lx ebpc=0x%lx etpc=0x%lx\n",
		       cause, regs->traparg0, regs->ebpc, regs->etpc);
		panic("linx: E_INST");
	}

	if (is_e && trapnum == LINX_TRAPNUM_E_BLOCK) {
		pr_err("linx: E_BLOCK: cause=0x%x traparg0=0x%lx ebpc=0x%lx etpc=0x%lx\n",
		       cause, regs->traparg0, regs->ebpc, regs->etpc);
		panic("linx: E_BLOCK");
	}

	pr_err("linx: unexpected trap/irq: trapno=0x%llx ipending=0x%llx ebpc=0x%lx etpc=0x%lx\n",
	       trapno, pending, regs->ebpc, regs->etpc);
}
