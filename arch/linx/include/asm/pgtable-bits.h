/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PGTABLE_BITS_H
#define _ASM_LINX_PGTABLE_BITS_H

/*
 * LinxISA page table entry bits (v0.2 bring-up profile).
 *
 * This matches the QEMU Linx MMU page-walk model:
 * - Descriptor type in bits[1:0]
 * - Permissions in bits[5:2]
 * - AF in bit[6] (required to be 1 in QEMU bring-up)
 * - AttrIdx in bits[9:7] (0..2)
 * - Base PA in bits[47:12] (4 KiB aligned)
 */

#define LINX_PTE_TYPE_MASK		0x3ull
#define LINX_PTE_TYPE_INVALID		0x0ull
#define LINX_PTE_TYPE_PAGE		0x1ull
#define LINX_PTE_TYPE_BLOCK		0x2ull
#define LINX_PTE_TYPE_TABLE		0x3ull

#define LINX_PTE_R			(1ull << 2)
#define LINX_PTE_W			(1ull << 3)
#define LINX_PTE_X			(1ull << 4)
#define LINX_PTE_U			(1ull << 5)
#define LINX_PTE_AF			(1ull << 6)

#define LINX_PTE_ATTRIDX_SHIFT		7u
#define LINX_PTE_ATTRIDX_MASK		(0x7ull << LINX_PTE_ATTRIDX_SHIFT)

#define LINX_PTE_ADDR_MASK		0x0000fffffffff000ull

#define LINX_MAIR_ATTR_DEVICE		0u
#define LINX_MAIR_ATTR_NORMAL_WB	1u
#define LINX_MAIR_ATTR_NORMAL_NC	2u

#endif /* _ASM_LINX_PGTABLE_BITS_H */
