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
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/unistd.h>

#include <asm/irq_regs.h>
#include <asm/debug_uart.h>
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

static void linx_debug_dump_writev(unsigned long fd, unsigned long iov_addr,
				       unsigned long iovcnt)
{
	struct iovec iov[8];
	unsigned long nvec = iovcnt > 8 ? 8 : iovcnt;
	unsigned long i;

	if (!nvec)
		return;
	if (copy_from_user(iov, (const void __user *)iov_addr,
			   nvec * sizeof(struct iovec)))
		return;

	for (i = 0; i < nvec; i++) {
		char msg[97];
		size_t take = iov[i].iov_len;
		size_t j;

		if (!take)
			continue;
		if (take > sizeof(msg) - 1)
			take = sizeof(msg) - 1;

		if (copy_from_user(msg, iov[i].iov_base, take))
			continue;
		msg[take] = '\0';
		for (j = 0; j < take; j++) {
			if (msg[j] < 0x20 || msg[j] > 0x7e)
				msg[j] = '.';
		}

		pr_err("linx: writev fd=%lu iov[%lu] len=%zu text=\"%s\"\n",
		       fd, i, (size_t)iov[i].iov_len, msg);
	}
}

static bool linx_try_handle_page_fault(struct pt_regs *regs, unsigned long addr,
					       bool is_write, bool is_instruction)
{
	struct mm_struct *mm = current->mm ? current->mm : current->active_mm;
	struct vm_area_struct *vma;
	vm_fault_t fault;
	unsigned int flags = FAULT_FLAG_DEFAULT;
	static int pf_debug_count;

	if (!mm)
		return false;

	if (is_instruction)
		flags |= FAULT_FLAG_INSTRUCTION;
	if (is_write)
		flags |= FAULT_FLAG_WRITE;
	if (user_mode(regs))
		flags |= FAULT_FLAG_USER;

retry:
	mmap_read_lock(mm);

	vma = find_vma(mm, addr);
	if (!vma) {
		if (pf_debug_count < 64) {
			pr_err("linx: pf miss addr=0x%lx write=%d insn=%d flags=0x%x pc=0x%lx\n",
			       addr, is_write, is_instruction, flags, regs->regs[PTR_PC]);
			pf_debug_count++;
		}
		mmap_read_unlock(mm);
		return false;
	}

	if (addr < vma->vm_start) {
		if (!(vma->vm_flags & VM_GROWSDOWN)) {
			mmap_read_unlock(mm);
			return false;
		}

		mmap_read_unlock(mm);
		mmap_write_lock(mm);

		vma = find_vma(mm, addr);
		if (!vma) {
			mmap_write_unlock(mm);
			return false;
		}

		if (addr < vma->vm_start) {
			if (!(vma->vm_flags & VM_GROWSDOWN)) {
				mmap_write_unlock(mm);
				return false;
			}
			vma = expand_stack(mm, addr);
			if (!vma) {
				mmap_write_unlock(mm);
				return false;
			}
		}

		mmap_write_downgrade(mm);
	}

	fault = handle_mm_fault(vma, addr, flags, regs);
	if (pf_debug_count < 64) {
		pr_err("linx: pf addr=0x%lx write=%d insn=%d vma=[0x%lx,0x%lx) vm_flags=0x%lx fault=0x%x pc=0x%lx\n",
		       addr, is_write, is_instruction, vma->vm_start, vma->vm_end,
		       vma->vm_flags, fault, regs->regs[PTR_PC]);
		pf_debug_count++;
	}
	if (fault_signal_pending(fault, regs))
		return true;

	/* mmap lock already released by the core fault path. */
	if (fault & VM_FAULT_COMPLETED)
		return true;

	if (fault & VM_FAULT_RETRY) {
		flags |= FAULT_FLAG_TRIED;
		goto retry;
	}

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
	static unsigned int trap_debug_count;
	static unsigned int syscall_debug_count;
	const u64 trapno = regs->trapno;
	const bool is_async = (trapno & LINX_TRAPNO_E_BIT) != 0;
	const bool argv = (trapno & LINX_TRAPNO_ARGV_BIT) != 0;
	const u8 trapnum = linx_trapno_trapnum(trapno);
	const u32 cause = linx_trapno_cause(trapno);
	const u64 pending = linx_ssr_read_ipending_acr1();

	if (trap_debug_count < 64) {
		linx_debug_uart_puts("\n[linx trap] n=");
		linx_debug_uart_puthex_ulong((unsigned long)trapnum);
		linx_debug_uart_puts(" c=");
		linx_debug_uart_puthex_ulong((unsigned long)cause);
		linx_debug_uart_puts(" a=");
		linx_debug_uart_puthex_ulong((unsigned long)regs->traparg0);
		linx_debug_uart_puts(" pc=");
		linx_debug_uart_puthex_ulong((unsigned long)regs->regs[PTR_PC]);
		linx_debug_uart_puts(" e=");
		linx_debug_uart_puthex_ulong((unsigned long)is_async);
		linx_debug_uart_puts("\n");
		trap_debug_count++;
	}

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
			 * Linx bring-up runtime gate:
			 * keep timer IRQs masked to avoid scheduler tick paths
			 * while trap/MMU return semantics are being stabilized.
			 */
			linx_ssr_write_eoiei_acr1(irq_id);
			set_irq_regs(old_regs);
			return;
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
			static unsigned int writev_debug_count;
			const unsigned long nr = regs->regs[PTR_R9];
			const unsigned long arg0 = regs->regs[PTR_R2];
			const unsigned long arg1 = regs->regs[PTR_R3];
			const unsigned long arg2 = regs->regs[PTR_R4];

			if (syscall_debug_count < 24) {
				linx_debug_uart_puts("\n[linx sys] nr=");
				linx_debug_uart_puthex_ulong(nr);
				linx_debug_uart_puts(" a0=");
				linx_debug_uart_puthex_ulong(arg0);
				linx_debug_uart_puts(" a1=");
				linx_debug_uart_puthex_ulong(arg1);
				linx_debug_uart_puts(" a2=");
				linx_debug_uart_puthex_ulong(arg2);
				linx_debug_uart_puts(" pc=");
				linx_debug_uart_puthex_ulong(regs->regs[PTR_PC]);
				linx_debug_uart_puts("\n");
			}
			if (nr == __NR_writev && writev_debug_count < 64) {
				linx_debug_dump_writev(arg0, arg1, arg2);
				writev_debug_count++;
			}

			/*
			 * Trap entry arrives with CSTATE.I masked by hardware.
			 * Linux syscalls must run with normal interrupt/preemption
			 * behavior, otherwise blocking paths (e.g. procfs or
			 * block I/O waits) can stall indefinitely.
			 */
			local_irq_enable();
			linx_handle_syscall(regs);
			local_irq_disable();

			if (syscall_debug_count < 24) {
				linx_debug_uart_puts("[linx sys] ret=");
				linx_debug_uart_puthex_ulong(regs->regs[PTR_R2]);
				linx_debug_uart_puts("\n");
				syscall_debug_count++;
			}
			return;
		}

		pr_err("linx: syscall trap from non-user context: pc=0x%lx ecstate=0x%lx\n",
		       regs->regs[PTR_PC], regs->ecstate);
		return;
	}

	if (!is_async && (trapnum == LINX_TRAPNUM_DATA_ALIGN_FAULT ||
			  trapnum == LINX_TRAPNUM_DATA_PAGE_FAULT)) {
		static unsigned int user_data_fault_dump_count;
		if (trapnum == LINX_TRAPNUM_DATA_PAGE_FAULT) {
			const bool is_write = (cause & 0xfu) == 1u;
			if (linx_try_handle_page_fault(regs, regs->traparg0, is_write, false))
				return;
		}
		if (user_mode(regs)) {
			if (user_data_fault_dump_count < 4) {
				unsigned int i;
				for (i = 0; i < NUM_PTRACE_REG; i++)
					pr_err("linx: regs[%u]=0x%lx\n", i, regs->regs[i]);
				user_data_fault_dump_count++;
			}
			pr_err("linx: user data fault -> SIGSEGV trapnum=%u cause=0x%x addr=0x%lx pc=0x%lx\n",
			       trapnum, cause, regs->traparg0, regs->regs[PTR_PC]);
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
			pr_err("linx: user inst fault -> SIGILL trapnum=%u cause=0x%x traparg0=0x%lx pc=0x%lx\n",
			       trapnum, cause, regs->traparg0, regs->regs[PTR_PC]);
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
			pr_err("linx: user block trap -> SIGILL cause=0x%x traparg0=0x%lx pc=0x%lx\n",
			       cause, regs->traparg0, regs->regs[PTR_PC]);
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
			pr_err("linx: user debug trap -> SIGTRAP trapnum=%u cause=0x%x traparg0=0x%lx pc=0x%lx\n",
			       trapnum, cause, regs->traparg0, regs->regs[PTR_PC]);
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
