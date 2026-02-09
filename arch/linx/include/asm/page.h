/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_LINX_PAGE_H
#define _ASM_LINX_PAGE_H

#include <linux/const.h>
#include <linux/types.h>

#include <asm/pgtable-bits.h>

#define PAGE_SHIFT 12
#define PAGE_SIZE (_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))

#define PAGE_OFFSET _AC(0, UL)
#define PHYS_OFFSET _AC(0, UL)

/*
 * QEMU LinxISA `virt` loads the kernel at 0x10000 and uses a flat virt==phys
 * mapping during NOMMU bring-up. Avoid allocating early DT/memblock objects at
 * physical address 0 which would appear as a NULL pointer.
 */
#define MIN_MEMBLOCK_ADDR _AC(0x10000, UL)

#ifndef __ASSEMBLER__

#include <linux/string.h>
#include <linux/pfn.h>

#define clear_page(page)	memset((page), 0, PAGE_SIZE)
#define copy_page(to, from)	memcpy((to), (from), PAGE_SIZE)

struct page;

extern struct page *mem_map;

typedef struct page *pgtable_t;
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;

#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) })
#define __pgd(x)	((pgd_t) { (x) })
#define __pgprot(x)	((pgprot_t) { (x) })

/*
 * NOMMU bring-up: no arch-specific page protection bits. Core code still
 * expects PAGE_KERNEL to exist (e.g. vmap()).
 */
#ifdef CONFIG_MMU
#define PAGE_KERNEL	__pgprot(LINX_PTE_AF | LINX_PTE_R | LINX_PTE_W | LINX_PTE_X | \
				 ((unsigned long)LINX_MAIR_ATTR_NORMAL_WB << LINX_PTE_ATTRIDX_SHIFT))
#else
#define PAGE_KERNEL	__pgprot(0)
#endif

#define __pa(x)		((phys_addr_t)(unsigned long)(x))
#define __va(x)		((void *)(unsigned long)(x))

#define virt_to_page(addr)	pfn_to_page(PFN_DOWN(__pa(addr)))

/*
 * Bring-up uses a flat (virt==phys) mapping on QEMU `virt`.
 * Keep virt_addr_valid() conservative but functional for core helpers.
 */
#define virt_addr_valid(kaddr)	pfn_valid(PFN_DOWN(__pa(kaddr)))

/*
 * Early bring-up: avoid wiring up an actual shared zero page until the basic
 * memory model is in place.
 */
#ifdef CONFIG_MMU
extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))
#else
#define ZERO_PAGE(vaddr)	((struct page *)0)
#endif

static inline void clear_user_page(void *addr, unsigned long vaddr,
				   struct page *page)
{
	(void)vaddr;
	(void)page;
	clear_page(addr);
}

static inline void copy_user_page(void *to, void *from, unsigned long vaddr,
				  struct page *page)
{
	(void)vaddr;
	(void)page;
	copy_page(to, from);
}

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* !__ASSEMBLER__ */

#endif /* _ASM_LINX_PAGE_H */
