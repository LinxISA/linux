/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PGALLOC_H
#define _ASM_LINX_PGALLOC_H

#include <linux/mm.h>
#include <linux/string.h>

#include <asm/pgtable.h>
#include <asm/tlb.h>

#include <asm-generic/pgalloc.h>

#ifdef CONFIG_MMU

static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd,
				       pte_t *pte)
{
	unsigned long pa = (unsigned long)__pa(pte);

	(void)mm;
	set_pmd(pmd, __pmd((pa & LINX_PTE_ADDR_MASK) | LINX_PTE_TYPE_TABLE));
}

static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t pte)
{
	unsigned long pa = (unsigned long)page_to_phys(pte);

	(void)mm;
	set_pmd(pmd, __pmd((pa & LINX_PTE_ADDR_MASK) | LINX_PTE_TYPE_TABLE));
}

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = __pgd_alloc(mm, 0);

	if (!pgd)
		return NULL;

	/*
	 * Bring-up: copy the kernel mappings into every user pgd so trap entry
	 * and direct physical mappings remain accessible while TTBR0 points at
	 * the process page table.
	 */
	memcpy(pgd, swapper_pg_dir, sizeof(swapper_pg_dir));
	return pgd;
}

static inline void __pte_free_tlb(struct mmu_gather *tlb, pgtable_t pte,
				  unsigned long addr)
{
	(void)addr;
	tlb_remove_ptdesc(tlb, page_ptdesc(pte));
}

static inline void __pmd_free_tlb(struct mmu_gather *tlb, pmd_t *pmd,
				  unsigned long addr)
{
	(void)addr;
	tlb_remove_ptdesc(tlb, virt_to_ptdesc(pmd));
}

static inline void __pud_free_tlb(struct mmu_gather *tlb, pud_t *pud,
				  unsigned long addr)
{
	(void)addr;
	tlb_remove_ptdesc(tlb, virt_to_ptdesc(pud));
}

static inline void __p4d_free_tlb(struct mmu_gather *tlb, p4d_t *p4d,
				  unsigned long addr)
{
	(void)tlb;
	(void)p4d;
	(void)addr;
}

#endif /* CONFIG_MMU */

#endif /* _ASM_LINX_PGALLOC_H */
