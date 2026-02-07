/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PGTABLE_H
#define _ASM_LINX_PGTABLE_H

#include <asm/page.h>

/*
 * NOMMU bring-up: provide the minimal pgtable definitions required by
 * include/linux/pgtable.h and core headers.
 */

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

#endif /* _ASM_LINX_PGTABLE_H */
