// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/fdtable.h>
#include <linux/resource.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>

#include <asm/irq_regs.h>
#include <asm/debug_uart.h>
#include <asm/ptrace.h>
#include <asm/ssr.h>
#include <asm/thread_info.h>
#include <asm/linx_ctx_tu_test.h>

/*
 * Called from the EVBASE trap vector.
 *
 * Current bring-up model:
 * - Linux runs in ACR1 (kernel ring). Interrupts and syscalls route to ACR1.
 * - QEMU raises a single timer interrupt (IRQ0) via IPENDING_ACR1 bit0.
 * - The trap vector saves GPRs into pt_regs and passes the pointer here.
 */

extern void linx_timer_handle_irq(void);

/* Debug/test hook: count async interrupts seen by the trap handler. */
volatile u64 linx_async_irq_count;

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

#define LINX_TRAP_DEBUG_LIMIT 0u
#define LINX_SYSCALL_DEBUG_LIMIT 0u
#define LINX_DEBUG_MIRROR_USER_WRITE 0u
/* merge resolved */
#define LINX_ECSTATE_BI_BIT (1ull << 62)

static bool linx_disable_timer_irq;
static bool linx_ctx_tu_test;
static bool linx_ctx_tu_step_test;
static atomic_t linx_ctx_tu_pending_reason = ATOMIC_INIT(LINX_CTX_TU_PENDING_NONE);
static atomic64_t linx_ctx_tu_step_bi_fail_count = ATOMIC64_INIT(0);

enum {
	SSR_EBARG_BPC_CUR_ACR1 = 0x1f41,
	SSR_EBARG_BPC_TGT_ACR1 = 0x1f42,
	SSR_EBARG_TPC_ACR1 = 0x1f43,
	SSR_EBARG_TQ0_ACR1 = 0x1f45,
	SSR_EBARG_TQ1_ACR1 = 0x1f46,
	SSR_EBARG_TQ2_ACR1 = 0x1f47,
	SSR_EBARG_TQ3_ACR1 = 0x1f48,
	SSR_EBARG_UQ0_ACR1 = 0x1f49,
	SSR_EBARG_UQ1_ACR1 = 0x1f4a,
	SSR_EBARG_UQ2_ACR1 = 0x1f4b,
	SSR_EBARG_UQ3_ACR1 = 0x1f4c,
	SSR_EBARG_LB_ACR1 = 0x1f4d,
	SSR_EBARG_LC_ACR1 = 0x1f4e,
};

enum {
	LINX_CTX_TU_POISON_BPC_CUR = 0xfeed000000000111ull,
	LINX_CTX_TU_POISON_BPC_TGT = 0xfeed000000000222ull,
	LINX_CTX_TU_POISON_TPC = 0xfeed000000000333ull,
	LINX_CTX_TU_POISON_TQ0 = 0x1111111111111111ull,
	LINX_CTX_TU_POISON_TQ1 = 0x2222222222222222ull,
	LINX_CTX_TU_POISON_TQ2 = 0x3333333333333333ull,
	LINX_CTX_TU_POISON_TQ3 = 0x4444444444444444ull,
	LINX_CTX_TU_POISON_UQ0 = 0x5555555555555555ull,
	LINX_CTX_TU_POISON_UQ1 = 0x6666666666666666ull,
	LINX_CTX_TU_POISON_UQ2 = 0x7777777777777777ull,
	LINX_CTX_TU_POISON_UQ3 = 0x8888888888888888ull,
	LINX_CTX_TU_POISON_LB = 0x9999999999999999ull,
	LINX_CTX_TU_POISON_LC = 0xaaaaaaaaaaaaaaa1ull,
};

static int __init linx_disable_timer_irq_setup(char *arg)
{
	if (!arg)
		linx_disable_timer_irq = true;
	else if (arg[0] == '1' || arg[0] == 'y' || arg[0] == 'Y' ||
		 arg[0] == 't' || arg[0] == 'T')
		linx_disable_timer_irq = true;
	else
		linx_disable_timer_irq = false;
	return 1;
}
early_param("linx_disable_timer_irq", linx_disable_timer_irq_setup);

static int __init linx_ctx_tu_test_setup(char *arg)
{
	if (!arg)
		linx_ctx_tu_test = true;
	else if (arg[0] == '1' || arg[0] == 'y' || arg[0] == 'Y' ||
		 arg[0] == 't' || arg[0] == 'T')
		linx_ctx_tu_test = true;
	else
		linx_ctx_tu_test = false;
	return 1;
}
__setup("linx_ctx_tu_test=", linx_ctx_tu_test_setup);

static int __init linx_ctx_tu_step_test_setup(char *arg)
{
	if (!arg)
		linx_ctx_tu_step_test = true;
	else if (arg[0] == '1' || arg[0] == 'y' || arg[0] == 'Y' ||
		 arg[0] == 't' || arg[0] == 'T')
		linx_ctx_tu_step_test = true;
	else
		linx_ctx_tu_step_test = false;
	return 1;
}
__setup("linx_ctx_tu_step_test=", linx_ctx_tu_step_test_setup);

void linx_ctx_tu_test_note_timer_irq(struct pt_regs *regs, u64 irq_id)
{
	if (!READ_ONCE(linx_ctx_tu_test) || irq_id != 0 || !regs)
		return;
	if (!user_mode(regs))
		return;
	if (!(regs->ecstate & LINX_ECSTATE_BI_BIT))
		return;
	atomic_set(&linx_ctx_tu_pending_reason, LINX_CTX_TU_PENDING_TIMER_IRQ);
}

void linx_ctx_tu_test_note_step_trap(struct pt_regs *regs, u8 trapnum)
{
	u64 fail_count;

	if (!READ_ONCE(linx_ctx_tu_step_test) || !regs)
		return;
	if (!user_mode(regs))
		return;
	if (trapnum != LINX_TRAPNUM_SW_BREAKPOINT)
		return;

	if (!(regs->ecstate & LINX_ECSTATE_BI_BIT)) {
		fail_count = (u64)atomic64_inc_return(&linx_ctx_tu_step_bi_fail_count);
		pr_err_ratelimited("linx_ctx_tu_step_test: BI requirement failed trapnum=%u pc=0x%lx ecstate=0x%lx fail_count=%llu\n",
				   trapnum, regs->regs[PTR_PC], regs->ecstate,
				   fail_count);
	}

	atomic_set(&linx_ctx_tu_pending_reason, LINX_CTX_TU_PENDING_STEP_TRAP);
}

enum linx_ctx_tu_test_pending_reason linx_ctx_tu_test_take_pending_reason(void)
{
	return (enum linx_ctx_tu_test_pending_reason)
		atomic_xchg(&linx_ctx_tu_pending_reason, LINX_CTX_TU_PENDING_NONE);
}

void linx_ctx_tu_test_poison_manager_state(void)
{
	if (!READ_ONCE(linx_ctx_tu_test) &&
	    !READ_ONCE(linx_ctx_tu_step_test))
		return;

	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_BPC_CUR, SSR_EBARG_BPC_CUR_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_BPC_TGT, SSR_EBARG_BPC_TGT_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_TPC, SSR_EBARG_TPC_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_TQ0, SSR_EBARG_TQ0_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_TQ1, SSR_EBARG_TQ1_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_TQ2, SSR_EBARG_TQ2_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_TQ3, SSR_EBARG_TQ3_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_UQ0, SSR_EBARG_UQ0_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_UQ1, SSR_EBARG_UQ1_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_UQ2, SSR_EBARG_UQ2_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_UQ3, SSR_EBARG_UQ3_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_LB, SSR_EBARG_LB_ACR1);
	linx_hl_ssrset((u64)LINX_CTX_TU_POISON_LC, SSR_EBARG_LC_ACR1);
}
/* merge resolved: timer IRQ is controlled by linx_disable_timer_irq + TU hooks */

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
	unsigned long arg1 = regs->regs[PTR_R3];
	unsigned long arg2 = regs->regs[PTR_R4];
	unsigned long arg3 = regs->regs[PTR_R5];
	unsigned long arg4 = regs->regs[PTR_R6];
	static unsigned int proc_sys_debug_left = 96;
	static unsigned int child20_sys_debug_left = 128;

	/* Preserve arg0; a0 is also the return value register. */
	regs->orig_a0 = arg0;

	if (unlikely(proc_sys_debug_left > 0) &&
	    (nr == __NR_clone || nr == __NR_wait4 || nr == __NR_waitid ||
	     nr == __NR_execve)) {
		pr_err("linx: sys-enter nr=%lu a0=0x%lx a1=0x%lx a2=0x%lx a3=0x%lx a4=0x%lx pc=0x%lx pid=%d\n",
		       nr, arg0, arg1, arg2, arg3, arg4, regs->regs[PTR_PC], current->pid);
	}
	if (unlikely(current->pid == 20 && child20_sys_debug_left > 0)) {
		pr_err("linx: pid20 sys-enter nr=%lu a0=0x%lx a1=0x%lx a2=0x%lx a3=0x%lx a4=0x%lx pc=0x%lx\n",
		       nr, arg0, arg1, arg2, arg3, arg4, regs->regs[PTR_PC]);
	}

	if (nr < __NR_syscalls) {
		long ret;
		asmlinkage long (*fn)(const struct pt_regs *);

		fn = (asmlinkage long (*)(const struct pt_regs *))sys_call_table[nr];
		ret = fn(regs);
		regs->regs[PTR_R2] = (unsigned long)ret;
		if (unlikely(proc_sys_debug_left > 0) &&
		    (nr == __NR_clone || nr == __NR_wait4 || nr == __NR_waitid ||
		     nr == __NR_execve)) {
			pr_err("linx: sys-exit  nr=%lu ret=%ld a0=0x%lx pc=0x%lx pid=%d\n",
			       nr, ret, regs->regs[PTR_R2], regs->regs[PTR_PC], current->pid);
			proc_sys_debug_left--;
		}
		if (unlikely(current->pid == 20 && child20_sys_debug_left > 0)) {
			pr_err("linx: pid20 sys-exit  nr=%lu ret=%ld a0=0x%lx pc=0x%lx\n",
			       nr, ret, regs->regs[PTR_R2], regs->regs[PTR_PC]);
			child20_sys_debug_left--;
		}
	} else {
		regs->regs[PTR_R2] = (unsigned long)-ENOSYS;
	}
}

static void linx_debug_mirror_user_write(unsigned long fd, unsigned long buf_addr,
					 unsigned long len)
{
	char buf[128];
	size_t to_copy;
	size_t i;
	const char __user *ubuf = (const char __user *)buf_addr;

	if (!(fd == 1 || fd == 2))
		return;
	if (!len)
		return;

	to_copy = len;
	if (to_copy > sizeof(buf))
		to_copy = sizeof(buf);

	if (copy_from_user(buf, ubuf, to_copy))
		return;

	for (i = 0; i < to_copy; i++)
		linx_debug_uart_putc(buf[i]);
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

	if (trap_debug_count < LINX_TRAP_DEBUG_LIMIT) {
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
		bool eoi_done = false;

		linx_async_irq_count++;

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
			linx_ctx_tu_test_note_timer_irq(regs, irq_id);
			if (!linx_disable_timer_irq) {
				int irq_rc;

				irq_enter();
				irq_rc = generic_handle_irq((unsigned int)irq_id);
				if (unlikely(irq_rc)) {
					/*
					 * Keep bring-up alive if IRQ0 wiring regresses:
					 * run the clockevent path directly and issue EOI.
					 */
					linx_timer_handle_irq();
					linx_ssr_write_eoiei_acr1(irq_id);
				}
				irq_exit();
				eoi_done = true;
			}
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
		if (!eoi_done)
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
			const unsigned long nr = regs->regs[PTR_R9];
			const unsigned long arg0 = regs->regs[PTR_R2];
			const unsigned long arg1 = regs->regs[PTR_R3];
			const unsigned long arg2 = regs->regs[PTR_R4];

				if (syscall_debug_count < LINX_SYSCALL_DEBUG_LIMIT) {
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
				if (LINX_DEBUG_MIRROR_USER_WRITE && nr == __NR_write && arg2 <= 4096)
					linx_debug_mirror_user_write(arg0, arg1, arg2);
				/*
				 * Trap entry arrives with CSTATE.I masked by hardware.
			 * Linux syscalls must run with normal interrupt/preemption
			 * behavior, otherwise blocking paths (e.g. procfs or
			 * block I/O waits) can stall indefinitely.
			 */
			local_irq_enable();
			linx_handle_syscall(regs);
			local_irq_disable();

				if (syscall_debug_count < LINX_SYSCALL_DEBUG_LIMIT) {
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
		pr_err("linx: kernel block trap: cause=0x%x traparg0=0x%lx pc=0x%lx sp=0x%lx ra=0x%lx ecstate=0x%lx ipending=0x%llx\n",
		       cause, regs->traparg0, regs->regs[PTR_PC], regs->regs[PTR_R1],
		       regs->regs[PTR_R10], regs->ecstate, pending);
		pr_err("linx: kernel block trap ebarg: bpc_cur=0x%lx bpc_tgt=0x%lx tpc=0x%lx lra=0x%lx lb=0x%lx lc=0x%lx ext_ptr=0x%lx ext_meta=0x%lx\n",
		       regs->ebarg_bpc_cur, regs->ebarg_bpc_tgt, regs->ebarg_tpc,
		       regs->ebarg_lra, regs->ebarg_lb, regs->ebarg_lc,
		       regs->ebarg_ext_ptr, regs->ebarg_ext_meta);
		panic("linx: kernel E_BLOCK");
	}

	if (!is_async && (trapnum == LINX_TRAPNUM_HW_BREAKPOINT ||
			  trapnum == LINX_TRAPNUM_HW_WATCHPOINT ||
			  trapnum == LINX_TRAPNUM_SW_BREAKPOINT)) {
		if (user_mode(regs)) {
			int code = (trapnum == LINX_TRAPNUM_SW_BREAKPOINT) ? TRAP_BRKPT : TRAP_HWBKPT;

			if (trapnum == LINX_TRAPNUM_SW_BREAKPOINT &&
			    READ_ONCE(linx_ctx_tu_step_test)) {
				unsigned long resume_pc;

				linx_ctx_tu_test_note_step_trap(regs, trapnum);
				resume_pc = regs->traparg0 ? regs->traparg0 + 4 :
					    regs->regs[PTR_PC] + 4;
				regs->regs[PTR_PC] = resume_pc;
				regs->ebarg_bpc_cur = resume_pc;
				regs->ebarg_bpc_tgt = resume_pc;
				regs->ebarg_tpc = resume_pc;
				return;
			}

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
