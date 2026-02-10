// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/fdtable.h>
#include <linux/resource.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>

#include <asm/irq_regs.h>
#include <asm/ptrace.h>
#include <asm/ssr.h>
#include <asm/thread_info.h>

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
#define LINX_TRAPNO_ARGV_BIT		(1ull << 62)
#define LINX_TRAPNO_CAUSE_SHIFT		24u
#define LINX_TRAPNO_CAUSE_MASK		0xffffffu
#define LINX_TRAPNO_TRAPNUM_MASK	0x3fu

enum {
	LINX_TRAPNUM_EXEC_STATE_CHECK	= 0,
	LINX_TRAPNUM_ILLEGAL_INST	= 4,
	LINX_TRAPNUM_BLOCK_TRAP		= 5,
	LINX_TRAPNUM_SYSCALL		= 6,
	LINX_TRAPNUM_INST_PC_FAULT	= 32,
	LINX_TRAPNUM_INST_PAGE_FAULT	= 33,
	LINX_TRAPNUM_DATA_ALIGN_FAULT	= 34,
	LINX_TRAPNUM_DATA_PAGE_FAULT	= 35,
	LINX_TRAPNUM_INTERRUPT		= 44,
	LINX_TRAPNUM_HW_BREAKPOINT	= 49,
	LINX_TRAPNUM_SW_BREAKPOINT	= 50,
	LINX_TRAPNUM_HW_WATCHPOINT	= 51,
};

enum {
	LINX_SCT_SYS		= 1,
};

static inline u8 linx_trapno_trapnum(u64 trapno)
{
	return (u8)(trapno & LINX_TRAPNO_TRAPNUM_MASK);
}

static inline u32 linx_trapno_cause(u64 trapno)
{
	return (u32)((trapno >> LINX_TRAPNO_CAUSE_SHIFT) & LINX_TRAPNO_CAUSE_MASK);
}

static bool linx_try_handle_page_fault(struct pt_regs *regs, unsigned long addr,
					       bool is_write, bool is_instruction)
{
	struct mm_struct *mm = current->mm ? current->mm : current->active_mm;
	struct vm_area_struct *vma;
	vm_fault_t fault;
	unsigned int flags = 0;

	if (!mm)
		return false;

	mmap_read_lock(mm);

	vma = find_vma(mm, addr);
	if (!vma || addr < vma->vm_start) {
		mmap_read_unlock(mm);
		return false;
	}

	if (is_instruction)
		flags |= FAULT_FLAG_INSTRUCTION;
	if (is_write)
		flags |= FAULT_FLAG_WRITE;
	if (user_mode(regs))
		flags |= FAULT_FLAG_USER;

	fault = handle_mm_fault(vma, addr, flags, regs);
	mmap_read_unlock(mm);

	if (!(fault & VM_FAULT_ERROR))
		return true;

	if (fault & VM_FAULT_OOM) {
		pagefault_out_of_memory();
		return true;
	}

	return false;
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
}

asmlinkage void linx_do_trap(struct pt_regs *regs)
{
	const u64 trapno = regs->trapno;
	const bool is_async = (trapno & LINX_TRAPNO_E_BIT) != 0;
	const bool argv = (trapno & LINX_TRAPNO_ARGV_BIT) != 0;
	const u8 trapnum = linx_trapno_trapnum(trapno);
	const u32 cause = linx_trapno_cause(trapno);
	const u64 pending = linx_ssr_read_ipending_acr1();

	/*
	 * IRQ handling (bring-up, v0.2):
	 * - TRAPNO.E=1 indicates asynchronous interrupt.
	 * - TRAPNO.TRAPNUM=44 is the interrupt class.
	 * - TRAPARG0 carries irq_id.
	 */
	if (is_async && trapnum == LINX_TRAPNUM_INTERRUPT) {
		struct pt_regs *old_regs = set_irq_regs(regs);
		const u64 irq_id = regs->traparg0;
		const u64 irq_mask = (irq_id < 64) ? (1ull << irq_id) : 0;

		if (!irq_mask) {
			pr_err("linx: invalid irq id: irq_id=%llu ipending=0x%llx trapno=0x%llx\n",
			       irq_id, pending, trapno);
			set_irq_regs(old_regs);
			return;
		}

		if (!(pending & irq_mask)) {
			pr_warn_ratelimited("linx: irq pending mismatch: irq_id=%llu ipending=0x%llx trapno=0x%llx\n",
					    irq_id, pending, trapno);
		}

		if (irq_id == 0) {
			/*
			 * Early bring-up guard:
			 * ignore timer ticks before the system reaches RUNNING.
			 *
			 * Otherwise the boot idle task can be preempted while still
			 * carrying __init return frames, and a later reschedule may
			 * jump back into freed init text.
			 */
			if (system_state < SYSTEM_RUNNING) {
				/* Acknowledge and drop early boot timer ticks. */
				linx_ssr_write_eoiei_acr1(irq_id);
				set_irq_regs(old_regs);
				return;
			}

			irq_enter();
			linx_timer_handle_irq();
			irq_exit();
		} else {
			irq_enter();
			generic_handle_irq((unsigned int)irq_id);
			irq_exit();
		}

		/*
		 * Acknowledge after handler execution so level-style devices
		 * (e.g. virtio-mmio) can complete register-side deassertion
		 * before IPENDING is cleared.
		 */
		linx_ssr_write_eoiei_acr1(irq_id);

		set_irq_regs(old_regs);
		return;
	}

	if (!is_async && trapnum == LINX_TRAPNUM_SYSCALL) {
		/*
		 * Bring-up syscall path: treat ACRC service requests as Linux
		 * syscalls.
		 */
		if (!argv || regs->traparg0 != LINX_SCT_SYS || cause != LINX_SCT_SYS) {
			pr_err("linx: unexpected service request: argv=%d traparg0=%lu cause=%u\n",
			       argv, regs->traparg0, cause);
			return;
		}
		if (user_mode(regs)) {
			/*
			 * Trap entry arrives with CSTATE.I masked by hardware.
			 * Linux syscalls must run with normal interrupt/preemption
			 * behavior, otherwise blocking paths (e.g. procfs or
			 * block I/O waits) can stall indefinitely.
			 */
			local_irq_enable();
			linx_handle_syscall(regs);
			local_irq_disable();
			return;
		}

		pr_err("linx: syscall trap from non-user context: pc=0x%lx ecstate=0x%lx\n",
		       regs->regs[PTR_PC], regs->ecstate);
		return;
	}

	if (!is_async && (trapnum == LINX_TRAPNUM_DATA_ALIGN_FAULT ||
			  trapnum == LINX_TRAPNUM_DATA_PAGE_FAULT)) {
		if (trapnum == LINX_TRAPNUM_DATA_PAGE_FAULT) {
			const bool is_write = (cause & 0xfu) == 1u;
			if (linx_try_handle_page_fault(regs, regs->traparg0, is_write, false))
				return;
		}
		if (user_mode(regs)) {
			force_sig_fault(SIGSEGV, SEGV_MAPERR,
					(void __user *)regs->traparg0);
			return;
		}
		pr_err("linx: kernel data fault: trapnum=%u cause=0x%x traparg0=0x%lx pc=0x%lx ipending=0x%llx\n",
		       trapnum, cause, regs->traparg0, regs->regs[PTR_PC], pending);
		panic("linx: kernel data fault");
	}

	if (!is_async && (trapnum == LINX_TRAPNUM_INST_PC_FAULT ||
			  trapnum == LINX_TRAPNUM_INST_PAGE_FAULT ||
			  trapnum == LINX_TRAPNUM_ILLEGAL_INST)) {
		if (user_mode(regs)) {
			if (trapnum == LINX_TRAPNUM_INST_PAGE_FAULT) {
				if (linx_try_handle_page_fault(regs, regs->traparg0, false, true))
					return;
			}
			force_sig_fault(SIGILL, ILL_ILLOPC,
					(void __user *)regs->regs[PTR_PC]);
			return;
		}
		pr_err("linx: kernel inst fault: trapnum=%u cause=0x%x traparg0=0x%lx pc=0x%lx\n",
		       trapnum, cause, regs->traparg0, regs->regs[PTR_PC]);
		panic("linx: kernel inst fault");
	}

	if (!is_async && trapnum == LINX_TRAPNUM_BLOCK_TRAP) {
		if (user_mode(regs)) {
			force_sig_fault(SIGILL, ILL_ILLOPC,
					(void __user *)regs->regs[PTR_PC]);
			return;
		}
		pr_err("linx: kernel block trap: cause=0x%x traparg0=0x%lx pc=0x%lx\n",
		       cause, regs->traparg0, regs->regs[PTR_PC]);
		panic("linx: kernel E_BLOCK");
	}

	if (!is_async && (trapnum == LINX_TRAPNUM_HW_BREAKPOINT ||
			  trapnum == LINX_TRAPNUM_HW_WATCHPOINT ||
			  trapnum == LINX_TRAPNUM_SW_BREAKPOINT)) {
		if (user_mode(regs)) {
			int code = (trapnum == LINX_TRAPNUM_SW_BREAKPOINT) ? TRAP_BRKPT : TRAP_HWBKPT;
			force_sig_fault(SIGTRAP, code, (void __user *)regs->traparg0);
			return;
		}
		pr_err("linx: kernel debug trap: trapnum=%u cause=0x%x traparg0=0x%lx pc=0x%lx\n",
		       trapnum, cause, regs->traparg0, regs->regs[PTR_PC]);
		panic("linx: kernel debug trap");
	}

	pr_err("linx: unexpected trap/irq: trapno=0x%llx ipending=0x%llx pc=0x%lx\n",
	       trapno, pending, regs->regs[PTR_PC]);
}
