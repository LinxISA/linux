/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_IRQ_H
#define _ASM_LINX_IRQ_H

/*
 * LinxISA virt bring-up interrupt space:
 * keep enough static descriptors for ACR IPENDING[63:0].
 */
#define NR_IRQS 64

#include <asm-generic/irq.h>

#endif /* _ASM_LINX_IRQ_H */
