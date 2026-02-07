/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_CMPXCHG_H
#define _ASM_LINX_CMPXCHG_H

/*
 * Early bring-up: use the generic UP cmpxchg implementation.
 * This requires a working (even if stubbed) irqflags.h.
 */

#include <asm-generic/cmpxchg.h>

#endif /* _ASM_LINX_CMPXCHG_H */
