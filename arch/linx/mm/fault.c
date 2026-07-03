// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Lennox Wu <lennox.wu@sunplusct.com>
 *  Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 */


#include <linux/mm.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/perf_event.h>
#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/kfence.h>

#include <asm/trap.h>
#include <asm/ptrace.h>
#include <asm/tlbflush.h>
#include <asm/debug_uart.h>

#include "../kernel/head.h"

static bool linx_vm_trace;
static unsigned long linx_vm_trace_addr;
static bool linx_vm_trace_addr_set;

static int __init linx_vm_trace_setup(char *str)
{
	if (!str || !*str)
		linx_vm_trace = true;
	else if (kstrtobool(str, &linx_vm_trace))
		linx_vm_trace = true;

	return 1;
}
__setup("linx_vm_trace=", linx_vm_trace_setup);

static int __init linx_vm_trace_addr_setup(char *str)
{
	if (!str || kstrtoul(str, 0, &linx_vm_trace_addr))
		return 1;

	linx_vm_trace_addr_set = true;
	return 1;
}
__setup("linx_vm_trace_addr=", linx_vm_trace_addr_setup);

static bool linx_vm_trace_match(unsigned long addr)
{
	if (!linx_vm_trace)
		return false;

	if (linx_vm_trace_addr_set &&
	    ((addr ^ linx_vm_trace_addr) & PAGE_MASK))
		return false;

	return true;
}

static void linx_vm_trace_puthex(const char *name, unsigned long value)
{
	linx_debug_uart_puts(name);
	linx_debug_uart_puts("=0x");
	linx_debug_uart_puthex_ulong(value);
}

static unsigned long linx_vm_trace_fault_pgoff(struct vm_area_struct *vma,
					       unsigned long addr)
{
	if (!vma || addr < vma->vm_start)
		return 0;

	return vma->vm_pgoff + ((addr - vma->vm_start) >> PAGE_SHIFT);
}

static void linx_vm_trace_uart(const char *stage, struct pt_regs *regs,
			       unsigned long addr, unsigned long cause,
			       unsigned int flags, struct vm_area_struct *vma,
			       vm_fault_t fault, bool have_fault)
{
	unsigned long fault_pgoff = linx_vm_trace_fault_pgoff(vma, addr);

	linx_debug_uart_puts("LINX_VM_FAULT stage=");
	linx_debug_uart_puts(stage);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("pid", current->pid);
	linx_debug_uart_puts(" comm=");
	linx_debug_uart_puts(current->comm);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("addr", addr);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("cause", cause);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("flags", flags);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("tpc", regs->tpc);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("bpc", regs->bpc);
	linx_debug_uart_puts(" ");
	linx_vm_trace_puthex("sp", regs->sp);
	if (vma) {
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("vma_start", vma->vm_start);
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("vma_end", vma->vm_end);
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("vm_flags", vma->vm_flags);
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("page_prot", pgprot_val(vma->vm_page_prot));
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("vm_pgoff", vma->vm_pgoff);
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("fault_pgoff", fault_pgoff);
	} else {
		linx_debug_uart_puts(" vma=none");
	}
	if (have_fault) {
		linx_debug_uart_puts(" ");
		linx_vm_trace_puthex("fault", fault);
	}
	linx_debug_uart_puts("\n");
}

static void linx_vm_trace_vma(const char *stage, struct pt_regs *regs,
			      unsigned long addr, unsigned long cause,
			      unsigned int flags, struct vm_area_struct *vma)
{
	if (!linx_vm_trace_match(addr))
		return;

	linx_vm_trace_uart(stage, regs, addr, cause, flags, vma, 0, false);

	if (vma) {
		unsigned long fault_pgoff =
			linx_vm_trace_fault_pgoff(vma, addr);

		pr_info("linx: vm-fault stage=%s pid=%d comm=%s addr=%#lx cause=%#lx flags=%#x tpc=%#lx bpc=%#lx sp=%#lx vma=%#lx-%#lx vm_flags=%#lx page_prot=%#lx vm_pgoff=%#lx fault_pgoff=%#lx file=%px\n",
			stage, current->pid, current->comm, addr, cause, flags,
			regs->tpc, regs->bpc, regs->sp, vma->vm_start,
			vma->vm_end, (unsigned long)vma->vm_flags,
			pgprot_val(vma->vm_page_prot),
			(unsigned long)vma->vm_pgoff, fault_pgoff,
			(void *)vma->vm_file);
	} else {
		pr_info("linx: vm-fault stage=%s pid=%d comm=%s addr=%#lx cause=%#lx flags=%#x tpc=%#lx bpc=%#lx sp=%#lx vma=none\n",
			stage, current->pid, current->comm, addr, cause, flags,
			regs->tpc, regs->bpc, regs->sp);
	}
}

static void linx_vm_trace_fault_result(struct pt_regs *regs,
				       unsigned long addr,
				       unsigned long cause,
				       unsigned int flags,
				       struct vm_area_struct *vma,
				       vm_fault_t fault)
{
	if (!linx_vm_trace_match(addr))
		return;

	linx_vm_trace_uart("handled", regs, addr, cause, flags, vma, fault,
			   true);

	pr_info("linx: vm-fault stage=handled pid=%d comm=%s addr=%#lx cause=%#lx flags=%#x fault=%#x tpc=%#lx bpc=%#lx sp=%#lx vma=%#lx-%#lx vm_flags=%#lx page_prot=%#lx vm_pgoff=%#lx fault_pgoff=%#lx file=%px\n",
		current->pid, current->comm, addr, cause, flags,
		(unsigned int)fault, regs->tpc, regs->bpc, regs->sp,
		vma->vm_start, vma->vm_end, (unsigned long)vma->vm_flags,
		pgprot_val(vma->vm_page_prot),
		(unsigned long)vma->vm_pgoff,
		linx_vm_trace_fault_pgoff(vma, addr), (void *)vma->vm_file);
}

static void die_kernel_fault(const char *msg, unsigned long addr,
		struct pt_regs *regs)
{
	bust_spinlocks(1);

	pr_alert("Unable to handle kernel %s at virtual address " REG_FMT "\n", msg,
		addr);

	bust_spinlocks(0);
	die(regs, "Oops");
	do_exit(SIGKILL);
}

static inline void no_context(struct pt_regs *regs, unsigned long addr)
{
	const char *msg;

	/* Are we prepared to handle this kernel fault? */
	if (fixup_exception(regs))
		return;

	/*
	 * Oops. The kernel tried to access some bad page. We'll have to
	 * terminate things with extreme prejudice.
	 */
	if (addr < PAGE_SIZE)
		msg = "NULL pointer dereference";
	else {
		if (kfence_handle_page_fault(addr, is_write_page_fault(regs->trapno), regs))
			return;

		msg = "paging request";
	}

	die_kernel_fault(msg, addr, regs);
}

static inline void mm_fault_error(struct pt_regs *regs, unsigned long addr, vm_fault_t fault)
{
	if (fault & VM_FAULT_OOM) {
		/*
		 * We ran out of memory, call the OOM killer, and return the userspace
		 * (which will retry the fault, or kill us if we got oom-killed).
		 */
		if (!user_mode(regs)) {
			no_context(regs, addr);
			return;
		}
		pagefault_out_of_memory();
		return;
	} else if (fault & VM_FAULT_SIGBUS) {
		/* Kernel mode? Handle exceptions or die */
		if (!user_mode(regs)) {
			no_context(regs, addr);
			return;
		}
		do_trap(regs, SIGBUS, BUS_ADRERR, addr);
		return;
	}
	BUG();
}

static inline void bad_area(struct pt_regs *regs, struct mm_struct *mm, int code, unsigned long addr)
{
	/*
	 * Something tried to access memory that isn't in our memory map.
	 * Fix it, but check if it's kernel or user first.
	 */
	mmap_read_unlock(mm);
	/* User mode accesses just cause a SIGSEGV */
	if (user_mode(regs)) {
		do_trap(regs, SIGSEGV, code, addr);
		return;
	}

	no_context(regs, addr);
}

static inline void vmalloc_fault(struct pt_regs *regs, int code, unsigned long addr)
{
	pgd_t *pgd, *pgd_k;
	pud_t *pud, *pud_k;
	p4d_t *p4d, *p4d_k;
	pmd_t *pmd, *pmd_k;
	pte_t *pte_k;
	int index;
	unsigned long pfn;

	/* User mode accesses just cause a SIGSEGV */
	if (user_mode(regs))
		return do_trap(regs, SIGSEGV, code, addr);

	/*
	 * Synchronize this task's top level page-table
	 * with the 'reference' page table.
	 *
	 * Do _not_ use "tsk->active_mm->pgd" here.
	 * We might be inside an interrupt in the middle
	 * of a task switch.
	 */
	index = pgd_index(addr);
	pfn = ssr_read(SSR_MMTBASE) & MMTBASE_PPN;
	pgd = (pgd_t *)pfn_to_virt(pfn) + index;
	pgd_k = init_mm.pgd + index;

	if (!pgd_present(*pgd_k)) {
		no_context(regs, addr);
		return;
	}
	set_pgd(pgd, *pgd_k);

	p4d = p4d_offset(pgd, addr);
	p4d_k = p4d_offset(pgd_k, addr);
	if (!p4d_present(*p4d_k)) {
		no_context(regs, addr);
		return;
	}

	pud = pud_offset(p4d, addr);
	pud_k = pud_offset(p4d_k, addr);
	if (!pud_present(*pud_k)) {
		no_context(regs, addr);
		return;
	}

	/*
	 * Since the vmalloc area is global, it is unnecessary
	 * to copy individual PTEs
	 */
	pmd = pmd_offset(pud, addr);
	pmd_k = pmd_offset(pud_k, addr);
	if (!pmd_present(*pmd_k)) {
		no_context(regs, addr);
		return;
	}
	set_pmd(pmd, *pmd_k);

	/*
	 * Make sure the actual PTE exists as well to
	 * catch kernel vmalloc-area accesses to non-mapped
	 * addresses. If we don't do this, this will just
	 * silently loop forever.
	 */
	pte_k = pte_offset_kernel(pmd_k, addr);
	if (!pte_present(*pte_k)) {
		no_context(regs, addr);
		return;
	}

	/*
	 * The kernel assumes that TLBs don't cache invalid
	 * entries, but in RISC-V, SFENCE.VMA specifies an
	 * ordering constraint, not a cache flush; it is
	 * necessary even after writing invalid entries.
	 */
	local_flush_tlb_page(addr);
}

static inline bool access_error(unsigned long cause, struct vm_area_struct *vma)
{
	if (is_insn_abort(cause)) {
		if (!(vma->vm_flags & VM_EXEC))
			return true;
	}

	if (is_data_abort(cause)) {
		if (ECAUSE_SYNDROME(cause) == ECAUSE_DATA_SYD_LD_PAGE_FAULT){
			 if (!(vma->vm_flags & VM_READ))
				 return true;
		} else if (ECAUSE_SYNDROME(cause) == ECAUSE_DATA_SYD_ST_AOP_PAGE_FAULT) {
			 if (!(vma->vm_flags & VM_WRITE))
				 return true;
		}
	}

	return false;
}

/*
 * This routine handles page faults.  It determines the address and the
 * problem, and then passes it off to one of the appropriate routines.
 */
void do_page_fault(struct pt_regs *regs)
{
	struct task_struct *tsk;
	struct vm_area_struct *vma;
	struct mm_struct *mm;
	unsigned long addr, cause;
	unsigned int flags = FAULT_FLAG_DEFAULT;
	int code = SEGV_MAPERR;
	vm_fault_t fault;

	cause = regs->trapno;
	addr = regs->traparg0;

	tsk = current;
	mm = tsk->mm;

	if (kprobe_page_fault(regs, cause))
		return;

	/*
	 * Fault-in kernel-space virtual memory on-demand.
	 * The 'reference' page table is init_mm.pgd.
	 *
	 * NOTE! We MUST NOT take any locks for this case. We may
	 * be in an interrupt or a critical region, and should
	 * only copy the information from the master page table,
	 * nothing more.
	 */
	if (unlikely((addr >= VMALLOC_START) && (addr <= VMALLOC_END))) {
		vmalloc_fault(regs, code, addr);
		return;
	}

#ifdef CONFIG_64BIT
	/*
	 * Modules in 64bit kernels lie in their own virtual region which is not
	 * in the vmalloc region, but dealing with page faults in this region
	 * or the vmalloc region amounts to doing the same thing: checking that
	 * the mapping exists in init_mm.pgd and updating user page table, so
	 * just use vmalloc_fault.
	 */
	if (unlikely(addr >= MODULES_VADDR && addr < MODULES_END)) {
		vmalloc_fault(regs, code, addr);
		return;
	}
#endif
	/* Enable interrupts if they were enabled in the parent context. */
	if (likely(regs->cstate & CSTATE_I))
		local_irq_enable();

	/*
	 * If we're in an interrupt, have no user context, or are running
	 * in an atomic region, then we must not take the fault.
	 */
	if (unlikely(faulthandler_disabled() || !mm)) {
		tsk->thread.bad_cause = cause;
		no_context(regs, addr);
		return;
	}

	if (user_mode(regs))
		flags |= FAULT_FLAG_USER;

	if (!user_mode(regs) && addr < TASK_SIZE &&
			unlikely(!(regs->cstate & CSTATE_P)))
		die_kernel_fault("access to user memory without uaccess routines",
				addr, regs);

	perf_sw_event(PERF_COUNT_SW_PAGE_FAULTS, 1, regs, addr);

	if (is_write_abort(cause))
		flags |= FAULT_FLAG_WRITE;
	else if (is_insn_abort(cause))
		flags |= FAULT_FLAG_INSTRUCTION;
retry:
	mmap_read_lock(mm);
	vma = find_vma(mm, addr);
	if (unlikely(!vma)) {
		linx_vm_trace_vma("no-vma", regs, addr, cause, flags, NULL);
		tsk->thread.bad_cause = cause;
		bad_area(regs, mm, code, addr);
		return;
	}
	if (likely(vma->vm_start <= addr)) {
		linx_vm_trace_vma("good-vma", regs, addr, cause, flags, vma);
		goto good_area;
	}
	if (unlikely(!(vma->vm_flags & VM_GROWSDOWN))) {
		linx_vm_trace_vma("vma-gap", regs, addr, cause, flags, vma);
		tsk->thread.bad_cause = cause;
		bad_area(regs, mm, code, addr);
		return;
	}
	vma = expand_stack(mm, addr);
	if (unlikely(!vma)) {
		linx_vm_trace_vma("grow-fail", regs, addr, cause, flags, NULL);
		tsk->thread.bad_cause = cause;
		bad_area(regs, mm, code, addr);
		return;
	}
	linx_vm_trace_vma("grow-ok", regs, addr, cause, flags, vma);

	/*
	 * Ok, we have a good vm_area for this memory access, so
	 * we can handle it.
	 */
good_area:
	code = SEGV_ACCERR;

	if (unlikely(access_error(cause, vma))) {
		linx_vm_trace_vma("access-error", regs, addr, cause, flags, vma);
		tsk->thread.bad_cause = cause;
		bad_area(regs, mm, code, addr);
		return;
	}

	/*
	 * If for any reason at all we could not handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */
	fault = handle_mm_fault(vma, addr, flags, regs);
	linx_vm_trace_fault_result(regs, addr, cause, flags, vma, fault);

	/*
	 * If we need to retry but a fatal signal is pending, handle the
	 * signal first. We do not need to release the mmap_lock because it
	 * would already be released in __lock_page_or_retry in mm/filemap.c.
	 */
	if (fault_signal_pending(fault, regs)) {
		return;
	}

	if (unlikely((fault & VM_FAULT_RETRY) && (flags & FAULT_FLAG_ALLOW_RETRY))) {
		flags |= FAULT_FLAG_TRIED;

		/*
		 * No need to mmap_read_unlock(mm) as we would
		 * have already released it in __lock_page_or_retry
		 * in mm/filemap.c.
		 */
		goto retry;
	}

	mmap_read_unlock(mm);

	if (unlikely(fault & VM_FAULT_ERROR)) {
		tsk->thread.bad_cause = cause;
		mm_fault_error(regs, addr, fault);
		return;
	}
	return;
}
NOKPROBE_SYMBOL(do_page_fault);
