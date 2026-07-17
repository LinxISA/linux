/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2013 Regents of the University of California
 */

#include <linux/linkage.h>
#include <asm/asm.h>
#include <asm/block-def.h>

/* void *memset(void *, int, size_t) */
ENTRY(__memset)
WEAK(memset)
    mset [a0, a1, a2]
    block_next_ret
END(__memset)