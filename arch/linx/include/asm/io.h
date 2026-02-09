/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_IO_H
#define _ASM_LINX_IO_H

/*
 * LinxISA bring-up I/O mapping.
 *
 * QEMU `virt` places MMIO devices (UART/exit register) in the low physical
 * address space and the current Linx MMU profile keeps a direct map of low
 * physical memory. For early bring-up, treat ioremap as a direct-map cast.
 *
 * This is sufficient for the `virt` platform and keeps the kernel build
 * unblocked while the full vmalloc-based ioremap path is brought up.
 */

#include <asm-generic/io.h>

#ifdef CONFIG_MMU

#ifndef ioremap
static inline void __iomem *ioremap(phys_addr_t phys_addr, size_t size)
{
	(void)size;
	return (void __iomem *)(unsigned long)phys_addr;
}
#endif

#ifndef iounmap
static inline void iounmap(volatile void __iomem *addr)
{
	(void)addr;
}
#endif

#endif /* CONFIG_MMU */

#endif /* _ASM_LINX_IO_H */
