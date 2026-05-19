/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of huawei
 */

#ifndef _ASM_LINX_PGTABLE_BITS_H
#define _ASM_LINX_PGTABLE_BITS_H

/*
 * |63   32|31   23| 22 | 21 |20   16|15  12|11   9| 8 |7   5| 4 | 3 | 2 | 1 | 0
 *   PFN    RES.     A    D   FSWU    MT     RES.    G  RES.   PV  R   W   X   V
 */

#define _PAGE_ACCESSED_OFFSET 22

#define _PAGE_PRESENT   (1 << 0)
#define _PAGE_READ      (1 << 3)     /* Readable */
#define _PAGE_WRITE     (1 << 2)     /* Writable */
#define _PAGE_EXEC      (1 << 1)     /* Executable */
#define _PAGE_USER      (1 << 4)     /* User */
#define _PAGE_GLOBAL    (1 << 8)     /* Global */
#define _PAGE_ACCESSED  (1 << 22)    /* Set by hardware on any access */
#define _PAGE_DIRTY     (1 << 21)    /* Set by hardware on any write */
#define _PAGE_SOFT      (1 << 16)    /* Reserved for software */

#define _PAGE_MT_SHIFT	12
#define _PAGE_MTMASK 	_AC(0x000000000000F000, UL)
#define _PAGE_NOCACHE	 (_AC(0x1, UL) << _PAGE_MT_SHIFT)
#define _PAGE_IO	 (_AC(0x3, UL) << _PAGE_MT_SHIFT)

#define _PAGE_SPECIAL   _PAGE_SOFT
#define _PAGE_TABLE     _PAGE_PRESENT

/*
 * _PAGE_PROT_NONE is set on not-present pages (and ignored by the hardware) to
 * distinguish them from swapped out pages
 */
#define _PAGE_PROT_NONE _PAGE_READ

#define _PAGE_PFN_SHIFT 32

/* Set of bits to preserve across pte_modify() */
#define _PAGE_CHG_MASK  (~(unsigned long)(_PAGE_PRESENT | _PAGE_READ |	\
					  _PAGE_WRITE | _PAGE_EXEC |	\
					  _PAGE_USER | _PAGE_GLOBAL))
/*
 * when all of R/W/X are zero, the PTE is a pointer to the next level
 * of the page table; otherwise, it is a leaf PTE.
 */
#define _PAGE_LEAF (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)

#endif /* _ASM_LINX_PGTABLE_BITS_H */
