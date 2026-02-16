/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_TLBFLUSH_H
#define _ASM_LINX_TLBFLUSH_H

#include <linux/mm_types.h>

/*
 * v0.2 bring-up: single-core, no ASIDs. Keep the implementation conservative
 * by invalidating the entire TLB for all flush operations.
 */
static inline void linx_tlb_iall(void)
{
	asm volatile("tlb.iall" : : : "memory");
}

static inline void flush_tlb_all(void)
{
	linx_tlb_iall();
}

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	(void)mm;
	linx_tlb_iall();
}

static inline void flush_tlb_mm_range(struct mm_struct *mm,
				      unsigned long start, unsigned long end,
				      unsigned int stridend, bool freed_tables)
{
	(void)mm;
	(void)start;
	(void)end;
	(void)stridend;
	(void)freed_tables;
	linx_tlb_iall();
}

static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	(void)vma;
	(void)start;
	(void)end;
	linx_tlb_iall();
}

static inline void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)
{
	(void)vma;
	(void)addr;
	linx_tlb_iall();
}

static inline void flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
	(void)start;
	(void)end;
	linx_tlb_iall();
}

#endif /* _ASM_LINX_TLBFLUSH_H */
