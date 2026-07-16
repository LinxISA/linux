/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014 Regents of the University of California
 */

#include <linux/linkage.h>
#include <asm/unistd.h>
#include <asm/ssr.h>

	.text
ENTRY(__vdso_rt_sigreturn)
	.cfi_startproc
	.cfi_signal_frame
	BSTART.sys fall
	addi zero, __NR_rt_sigreturn, -> a7
	acrc SCT_SYS
	.cfi_endproc
ENDPROC(__vdso_rt_sigreturn)
