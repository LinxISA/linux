/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PGTABLE_H
#define _ASM_LINX_PGTABLE_H

#include <asm/page.h>
#include <asm/pgtable-bits.h>

/*
 * Page table format (v0.2 bring-up):
 * - 4 KiB pages, 48-bit canonical VA, 48-bit PA.
 * - 4-level walk (L0..L3) with 512 entries per table.
 *
 * Linux uses 5-level naming (PGD/P4D/PUD/PMD/PTE). For a 4-level hardware walk
 * the P4D level is folded.
 */

#ifdef CONFIG_MMU

#define VA_BITS		48

#define LINX_PGPROT_USER_BASE						\
	(LINX_PTE_AF | LINX_PTE_U |					\
	 ((unsigned long)LINX_MAIR_ATTR_NORMAL_WB << LINX_PTE_ATTRIDX_SHIFT))

#define PAGE_NONE	__pgprot(LINX_PGPROT_USER_BASE)
#define PAGE_READ	__pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_R)
#define PAGE_COPY	__pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_R)
#define PAGE_SHARED	__pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_R | LINX_PTE_W)

#define PAGE_EXEC	__pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_X)
#define PAGE_READ_EXEC	__pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_R | LINX_PTE_X)
#define PAGE_COPY_EXEC	__pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_R | LINX_PTE_X)
#define PAGE_SHARED_EXEC __pgprot(LINX_PGPROT_USER_BASE | LINX_PTE_R | LINX_PTE_W | LINX_PTE_X)

/*
 * Generic ioremap() asks the architecture to provide raw PTE bits for MMIO
 * mappings via _PAGE_IOREMAP.
 */
#define _PAGE_IOREMAP \
	(LINX_PTE_AF | LINX_PTE_R | LINX_PTE_W | \
	 ((unsigned long)LINX_MAIR_ATTR_DEVICE << LINX_PTE_ATTRIDX_SHIFT))

#define PTE_SHIFT	PAGE_SHIFT
#define PMD_SHIFT	(PAGE_SHIFT + 9)
#define PUD_SHIFT	(PMD_SHIFT + 9)
#define PGDIR_SHIFT	(PUD_SHIFT + 9)

#define PTE_SIZE	(1UL << PTE_SHIFT)
#define PTE_MASK	(~(PTE_SIZE - 1))

#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE - 1))

#define PUD_SIZE	(1UL << PUD_SHIFT)
#define PUD_MASK	(~(PUD_SIZE - 1))

#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE - 1))

#define PTRS_PER_PTE	512
#define PTRS_PER_PMD	512
#define PTRS_PER_PUD	512
#define PTRS_PER_PGD	512

#define __PAGETABLE_P4D_FOLDED	1
#define P4D_SHIFT		PGDIR_SHIFT
#define PTRS_PER_P4D		1
#define P4D_SIZE		(1UL << P4D_SHIFT)
#define P4D_MASK		(~(P4D_SIZE - 1))

#ifndef __ASSEMBLER__

#include <linux/init.h>
#include <linux/mm_types.h>

typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pud; } pud_t;
typedef struct { unsigned long p4d; } p4d_t;

#ifndef __ASSEMBLER__
struct vm_area_struct;
#endif

#define pmd_val(x)	((x).pmd)
#define pud_val(x)	((x).pud)
#define p4d_val(x)	((x).p4d)

#define __pmd(x)	((pmd_t) { (x) })
#define __pud(x)	((pud_t) { (x) })
#define __p4d(x)	((p4d_t) { (x) })

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void __init paging_init(void);

/* Bring-up: keep bad-entry reporting minimal but present. */
#ifndef pgd_ERROR
#define pgd_ERROR(pgd)	do { (void)(pgd); } while (0)
#endif
#ifndef p4d_ERROR
#define p4d_ERROR(p4d)	do { (void)(p4d); } while (0)
#endif
#ifndef pud_ERROR
#define pud_ERROR(pud)	do { (void)(pud); } while (0)
#endif
#ifndef pmd_ERROR
#define pmd_ERROR(pmd)	do { (void)(pmd); } while (0)
#endif

/* Table entry predicates. */
static inline int linx_pte_is_valid(unsigned long v)
{
	return (v & LINX_PTE_TYPE_MASK) != LINX_PTE_TYPE_INVALID;
}

static inline int linx_pte_is_table(unsigned long v)
{
	return (v & LINX_PTE_TYPE_MASK) == LINX_PTE_TYPE_TABLE;
}

static inline int linx_pte_is_block(unsigned long v)
{
	return (v & LINX_PTE_TYPE_MASK) == LINX_PTE_TYPE_BLOCK;
}

static inline int linx_pte_is_page(unsigned long v)
{
	return (v & LINX_PTE_TYPE_MASK) == LINX_PTE_TYPE_PAGE;
}

static inline unsigned long linx_pte_addr(unsigned long v)
{
	return (unsigned long)(v & LINX_PTE_ADDR_MASK);
}

/* PGD (L0). */
static inline int pgd_none(pgd_t pgd)		{ return pgd_val(pgd) == 0; }
static inline int pgd_present(pgd_t pgd)	{ return !pgd_none(pgd); }
static inline int pgd_bad(pgd_t pgd)		{ return pgd_present(pgd) && !linx_pte_is_table(pgd_val(pgd)); }
static inline void pgd_clear(pgd_t *pgd)	{ *pgd = __pgd(0); }

/* Folded P4D. */
static inline p4d_t *p4d_offset(pgd_t *pgd, unsigned long address)
{
	(void)address;
	return (p4d_t *)pgd;
}

static inline int p4d_none(p4d_t p4d)		{ return p4d_val(p4d) == 0; }
static inline int p4d_present(p4d_t p4d)	{ return !p4d_none(p4d); }
static inline int p4d_bad(p4d_t p4d)		{ return p4d_present(p4d) && !linx_pte_is_table(p4d_val(p4d)); }
static inline void p4d_clear(p4d_t *p4d)	{ *p4d = __p4d(0); }
static inline void set_p4d(p4d_t *p4dp, p4d_t p4d) { *p4dp = p4d; }

/* PUD (L1). */
static inline int pud_none(pud_t pud)		{ return pud_val(pud) == 0; }
static inline int pud_present(pud_t pud)	{ return !pud_none(pud); }
static inline int pud_bad(pud_t pud)
{
	const unsigned long v = pud_val(pud);

	return pud_present(pud) && !linx_pte_is_table(v) && !linx_pte_is_block(v);
}
static inline void pud_clear(pud_t *pud)	{ *pud = __pud(0); }
static inline void set_pud(pud_t *pudp, pud_t pud) { *pudp = pud; }
static inline int pud_leaf(pud_t pud)		{ return linx_pte_is_block(pud_val(pud)); }

/* PMD (L2). */
static inline int pmd_none(pmd_t pmd)		{ return pmd_val(pmd) == 0; }
static inline int pmd_present(pmd_t pmd)	{ return !pmd_none(pmd); }
static inline int pmd_bad(pmd_t pmd)
{
	const unsigned long v = pmd_val(pmd);

	return pmd_present(pmd) && !linx_pte_is_table(v) && !linx_pte_is_block(v);
}
static inline void pmd_clear(pmd_t *pmd)	{ *pmd = __pmd(0); }
static inline void set_pmd(pmd_t *pmdp, pmd_t pmd) { *pmdp = pmd; }
static inline int pmd_leaf(pmd_t pmd)		{ return linx_pte_is_block(pmd_val(pmd)); }

/* PTE (L3). */
static inline int pte_none(pte_t pte)		{ return pte_val(pte) == 0; }
static inline int pte_present(pte_t pte)	{ return linx_pte_is_page(pte_val(pte)); }
static inline void pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	(void)mm;
	(void)addr;
	*ptep = __pte(0);
}

/* Address translation helpers for next-level tables. */
static inline pud_t *p4d_pgtable(p4d_t p4d)
{
	return (pud_t *)(unsigned long)__va(linx_pte_addr(p4d_val(p4d)));
}

static inline pmd_t *pud_pgtable(pud_t pud)
{
	return (pmd_t *)(unsigned long)__va(linx_pte_addr(pud_val(pud)));
}

static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return (unsigned long)__va(linx_pte_addr(pmd_val(pmd)));
}

/* Offset helpers. */
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))

/* Populate helpers (table descriptors). */
static inline pgd_t linx_mk_pgd_table(pud_t *pud)
{
	return __pgd((unsigned long)__pa(pud) | LINX_PTE_TYPE_TABLE);
}

static inline p4d_t linx_mk_p4d_table(pud_t *pud)
{
	return __p4d((unsigned long)__pa(pud) | LINX_PTE_TYPE_TABLE);
}

static inline pud_t linx_mk_pud_table(pmd_t *pmd)
{
	return __pud((unsigned long)__pa(pmd) | LINX_PTE_TYPE_TABLE);
}

static inline void p4d_populate(struct mm_struct *mm, p4d_t *p4d, pud_t *pud)
{
	(void)mm;
	set_p4d(p4d, linx_mk_p4d_table(pud));
}

static inline void pud_populate(struct mm_struct *mm, pud_t *pud, pmd_t *pmd)
{
	(void)mm;
	set_pud(pud, linx_mk_pud_table(pmd));
}

/* Leaf entry constructors. */
static inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot)
{
	return __pte(((pfn << PAGE_SHIFT) & LINX_PTE_ADDR_MASK) |
		     (pgprot_val(prot) & ~LINX_PTE_TYPE_MASK) |
		     LINX_PTE_TYPE_PAGE);
}

static inline pmd_t pfn_pmd(unsigned long pfn, pgprot_t prot)
{
	return __pmd(((pfn << PAGE_SHIFT) & LINX_PTE_ADDR_MASK) |
		     (pgprot_val(prot) & ~LINX_PTE_TYPE_MASK) |
		     LINX_PTE_TYPE_BLOCK);
}

static inline unsigned long pte_pfn(pte_t pte)
{
	return (unsigned long)(linx_pte_addr(pte_val(pte)) >> PAGE_SHIFT);
}

static inline struct page *pte_page(pte_t pte)
{
	return pfn_to_page(pte_pfn(pte));
}

static inline unsigned long pmd_pfn(pmd_t pmd)
{
	return (unsigned long)(linx_pte_addr(pmd_val(pmd)) >> PAGE_SHIFT);
}

static inline struct page *pmd_page(pmd_t pmd)
{
	return pfn_to_page(pmd_pfn(pmd));
}

static inline unsigned long pud_pfn(pud_t pud)
{
	return (unsigned long)(linx_pte_addr(pud_val(pud)) >> PAGE_SHIFT);
}

static inline struct page *pud_page(pud_t pud)
{
	return pfn_to_page(pud_pfn(pud));
}

static inline unsigned long p4d_pfn(p4d_t p4d)
{
	return (unsigned long)(linx_pte_addr(p4d_val(p4d)) >> PAGE_SHIFT);
}

static inline struct page *p4d_page(p4d_t p4d)
{
	return pfn_to_page(p4d_pfn(p4d));
}

/*
 * v0.2 bring-up: no software-managed TLB caching; flushes are explicit via
 * tlbflush.h and TTBR/TCR writes.
 */
#ifndef update_mmu_cache
#define update_mmu_cache(vma, addr, ptep)	do { (void)(vma); (void)(addr); (void)(ptep); } while (0)
#endif

static inline void update_mmu_cache_range(struct vm_fault *vmf,
					 struct vm_area_struct *vma,
					 unsigned long address,
					 pte_t *ptep,
					 unsigned int nr)
{
	(void)vmf;
	(void)vma;
	(void)address;
	(void)ptep;
	(void)nr;
}

/* Basic PTE manipulation helpers. Keep AF always set in the bring-up profile. */
static inline int pte_write(pte_t pte)		{ return (pte_val(pte) & LINX_PTE_W) != 0; }
static inline int pte_exec(pte_t pte)		{ return (pte_val(pte) & LINX_PTE_X) != 0; }
static inline int pte_user(pte_t pte)		{ return (pte_val(pte) & LINX_PTE_U) != 0; }

static inline int pte_dirty(pte_t pte)		{ (void)pte; return 1; }
static inline pte_t pte_mkdirty(pte_t pte)	{ return pte; }
static inline pte_t pte_mkclean(pte_t pte)	{ return pte; }
static inline pte_t pte_mkyoung(pte_t pte)	{ return pte; }

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & (LINX_PTE_ADDR_MASK | LINX_PTE_TYPE_MASK)) |
		     (pgprot_val(newprot) & ~(LINX_PTE_ADDR_MASK | LINX_PTE_TYPE_MASK)));
}

static inline pte_t pte_mkwrite_novma(pte_t pte) { return __pte(pte_val(pte) | LINX_PTE_W); }
static inline pte_t pte_wrprotect(pte_t pte)	{ return __pte(pte_val(pte) & ~LINX_PTE_W); }
static inline pte_t pte_mkexec(pte_t pte)	{ return __pte(pte_val(pte) | LINX_PTE_X); }
static inline pte_t pte_mknexec(pte_t pte)	{ return __pte(pte_val(pte) & ~LINX_PTE_X); }
static inline pte_t pte_mkuser(pte_t pte)	{ return __pte(pte_val(pte) | LINX_PTE_U); }
static inline pte_t pte_mkold(pte_t pte)	{ return pte; }
static inline int pte_young(pte_t pte)		{ (void)pte; return 1; }

static inline void set_pte(pte_t *ptep, pte_t pte)
{
	*ptep = pte;
}

#define PFN_PTE_SHIFT	PAGE_SHIFT

/*
 * Swap entry encoding (v0.2 bring-up):
 * - Store the arch-independent swp_entry_t value in bits[63:LINX_SWP_PTE_SHIFT].
 * - Keep Desc[1:0]=00 so the hardware sees the PTE as not-present.
 * - Use bit[2] as the swap-exclusive marker.
 */
#define LINX_SWP_PTE_SHIFT	5u
#define LINX_SWP_EXCLUSIVE_BIT	(1ull << 2)

#define __pte_to_swp_entry(pte)	((swp_entry_t){ .val = (unsigned long)(pte_val(pte) >> LINX_SWP_PTE_SHIFT) })
#define __swp_entry_to_pte(x)	__pte(((x).val << LINX_SWP_PTE_SHIFT))
#define __swp_type(x)		((unsigned long)((x).val >> SWP_TYPE_SHIFT))
#define __swp_offset(x)		((pgoff_t)((x).val & SWP_OFFSET_MASK))
#define __swp_entry(type, offset)	((swp_entry_t){ .val = (((unsigned long)(type) << SWP_TYPE_SHIFT) | ((unsigned long)(offset) & SWP_OFFSET_MASK)) })

static inline int pte_swp_exclusive(pte_t pte)
{
	return (pte_val(pte) & LINX_SWP_EXCLUSIVE_BIT) != 0;
}

static inline pte_t pte_swp_clear_exclusive(pte_t pte)
{
	return __pte(pte_val(pte) & ~LINX_SWP_EXCLUSIVE_BIT);
}

static inline pte_t pte_swp_mkexclusive(pte_t pte)
{
	return __pte(pte_val(pte) | LINX_SWP_EXCLUSIVE_BIT);
}

#endif /* !__ASSEMBLER__ */

#else /* !CONFIG_MMU */

#define PTE_SHIFT	PAGE_SHIFT
#define PMD_SHIFT	PAGE_SHIFT
#define PUD_SHIFT	PAGE_SHIFT
#define PGDIR_SHIFT	PAGE_SHIFT

#define PTRS_PER_PTE	1
#define PTRS_PER_PMD	1
#define PTRS_PER_PUD	1
#define PTRS_PER_PGD	1

#ifndef __ASSEMBLER__

#include <linux/init.h>

typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pud; } pud_t;
typedef struct { unsigned long p4d; } p4d_t;

#define pmd_val(x)	((x).pmd)
#define pud_val(x)	((x).pud)
#define p4d_val(x)	((x).p4d)

#define __pmd(x)	((pmd_t) { (x) })
#define __pud(x)	((pud_t) { (x) })
#define __p4d(x)	((p4d_t) { (x) })

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void __init paging_init(void);

#endif /* !__ASSEMBLER__ */

#endif /* CONFIG_MMU */

#endif /* _ASM_LINX_PGTABLE_H */
