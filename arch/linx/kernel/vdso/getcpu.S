/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 SiFive
 */

#include <linux/linkage.h>
#include <asm/unistd.h>
#include <asm/ssr.h>

	.text
/* int __vdso_getcpu(unsigned *cpu, unsigned *node, void *unused); */
ENTRY(__vdso_getcpu)
	.cfi_startproc
	/* For now, just do the syscall. */
	BSTART.sys fall
	addi zero, __NR_getcpu, -> a7
	acrc SCT_SYS

	block_next_ret
	.cfi_endproc
ENDPROC(__vdso_getcpu)
