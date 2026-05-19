/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Limited
 * Copyright (C) 2014 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_LINX_VDSO_H
#define _ASM_LINX_VDSO_H

/*
 * All systems with an MMU have a VDSO, but systems without an MMU don't
 * support shared libraries and therefor don't have one.
 */
#ifdef CONFIG_MMU

#define __VDSO_PAGES    2
#define __VVAR_PAGES    __VDSO_PAGES

#ifndef __ASSEMBLER__
#include <generated/vdso-offsets.h>

#define VDSO_SYMBOL(base, name)							\
	(void __user *)((unsigned long)(base) + __vdso_##name##_offset)
#endif /* !__ASSEMBLER__ */

#endif /* CONFIG_MMU */

#endif /* _ASM_LINX_VDSO_H */
