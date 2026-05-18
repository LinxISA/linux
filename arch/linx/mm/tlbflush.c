// SPDX-License-Identifier: GPL-2.0

#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <asm/lisc.h>
#include <asm/mmu_context.h>

static inline void local_flush_tlb_all_asid(unsigned long asid)
{
	__asm__ __volatile__ (
			"BSTART.sys fall\n"
				"tlb.ia %[asid]\n"
			:
			: [asid] "r" (asid)
			: "memory");
}

static inline void local_flush_tlb_page_asid(unsigned long addr,
		unsigned long asid)
{
	__asm__ __volatile__ (
			"BSTART.sys fall\n"
				"addi zero, 1, -> t\n"
				"slli t#1, 44, -> t\n"
				"subi t#1, 1, -> t\n"
				"and %[addr], t#1, -> t\n"
				"slli %[asid], 48, -> t\n"
				"add t#1, t#2, -> t\n"
				"tlb.iav t#1\n"
			:
			: [addr] "r" (addr), [asid] "r" (asid)
			: "memory");
}

static void ipi_remote_sfence_vma_all_asid(void *info)
{
	return local_flush_tlb_all_asid(*(unsigned long *)info);
}

static void ipi_remote_sfence_vma_all(void *info)
{
	return local_flush_tlb_all();
}

void flush_tlb_all(void)
{
	on_each_cpu(ipi_remote_sfence_vma_all, NULL, 1);
}

static void __sbi_tlb_flush_range(struct mm_struct *mm, unsigned long start,
				  unsigned long size, unsigned long stride)
{
	struct cpumask *cmask = mm_cpumask(mm);
	struct cpumask hmask;
	unsigned int cpuid;
	bool broadcast;

	if (cpumask_empty(cmask))
		return;

	cpuid = get_cpu();
	/* check if the tlbflush needs to be sent to other CPUs */
	broadcast = cpumask_any_but(cmask, cpuid) < nr_cpu_ids;
	if (static_branch_unlikely(&use_asid_allocator)) {
		unsigned long asid = atomic_long_read(&mm->context.id);

		if (broadcast) {
			riscv_cpuid_to_hartid_mask(cmask, &hmask);
			on_each_cpu_mask(&hmask,
					 ipi_remote_sfence_vma_all_asid,
					 (void *)&asid, 1);
		} else if (size <= stride) {
			local_flush_tlb_page_asid(start, asid);
		} else {
			local_flush_tlb_all_asid(asid);
		}
	} else {
		if (broadcast) {
			riscv_cpuid_to_hartid_mask(cmask, &hmask);
			on_each_cpu_mask(&hmask,
					 ipi_remote_sfence_vma_all,
					 NULL, 1);
		} else if (size <= stride) {
			local_flush_tlb_page(start);
		} else {
			local_flush_tlb_all();
		}
	}

	put_cpu();
}

void flush_tlb_mm(struct mm_struct *mm)
{
	__sbi_tlb_flush_range(mm, 0, -1, PAGE_SIZE);
}

void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)
{
	__sbi_tlb_flush_range(vma->vm_mm, addr, PAGE_SIZE, PAGE_SIZE);
}

void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
		     unsigned long end)
{
	__sbi_tlb_flush_range(vma->vm_mm, start, end - start, PAGE_SIZE);
}
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
void flush_pmd_tlb_range(struct vm_area_struct *vma, unsigned long start,
			unsigned long end)
{
	__sbi_tlb_flush_range(vma->vm_mm, start, end - start, PMD_SIZE);
}
#endif
