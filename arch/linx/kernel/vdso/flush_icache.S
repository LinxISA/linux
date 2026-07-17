/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 SiFive
 */

#include <linux/linkage.h>
#include <asm/unistd.h>
#include <asm/ssr.h>

	.text
/* int __vdso_flush_icache(void *start, void *end, unsigned long flags); */
ENTRY(__vdso_flush_icache)
	.cfi_startproc
#ifdef CONFIG_SMP
	BSTART.sys fall
	addi zero, __NR_riscv_flush_icache, -> a7
	acrc SCT_SYS
#else
	BSTART.sys fall
	fence.i
	add zero, zero, -> a0
#endif
	block_next_ret
	.cfi_endproc
ENDPROC(__vdso_flush_icache)
