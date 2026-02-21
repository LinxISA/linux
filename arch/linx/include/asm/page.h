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

static inline void clear_page(void *page)
{
#if defined(__clang__)
	uintptr_t p = (uintptr_t)page;

	for (unsigned long i = 0; i < (PAGE_SIZE / sizeof(uint64_t)); ++i)
		asm volatile("hl.sdi.po zero, [%0, 8], ->%0" : "+r"(p) : : "memory");
#else
	memset(page, 0, PAGE_SIZE);
#endif
}

static inline void copy_page(void *to, const void *from)
{
#if defined(__clang__)
	uintptr_t d = (uintptr_t)to;
	uintptr_t s = (uintptr_t)from;

	for (unsigned long i = 0; i < (PAGE_SIZE / 16u); ++i) {
		uint64_t lo, hi;

		asm volatile("hl.ldip [%2, 0], ->%0, %1"
			     : "=&r"(lo), "=&r"(hi)
			     : "r"(s)
			     : "memory");
		asm volatile("hl.sdip %0, %1, [%2, 0]"
			     :
			     : "r"(lo), "r"(hi), "r"(d)
			     : "memory");

		s += 16u;
		d += 16u;
	}
#else
	memcpy(to, from, PAGE_SIZE);
#endif
}

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
#if defined(__clang__)
	uintptr_t d = (uintptr_t)to;
	uintptr_t s = (uintptr_t)from;

	for (unsigned long i = 0; i < (PAGE_SIZE / sizeof(uint64_t)); ++i) {
		uint64_t v;

		asm volatile("hl.ldi.po [%1, 8], ->%0, %1"
			     : "=&r"(v), "+r"(s)
			     :
			     : "memory");
		asm volatile("hl.sdi.po %1, [%0, 8], ->%0"
			     : "+&r"(d)
			     : "r"(v)
			     : "memory");
	}
#else
	copy_page(to, from);
#endif
}

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* !__ASSEMBLER__ */

#endif /* _ASM_LINX_PAGE_H */
