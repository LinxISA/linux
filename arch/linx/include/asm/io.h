/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_IO_H
#define _ASM_LINX_IO_H

/*
 * LinxISA uses the generic MMU ioremap implementation. The previous bring-up
 * direct-cast ioremap shortcut cannot safely cover QEMU virt MMIO ranges such
 * as 0x30001000 and causes kernel faults when those addresses are not part of
 * the linear mapping.
 */

#include <asm-generic/io.h>

#endif /* _ASM_LINX_IO_H */
