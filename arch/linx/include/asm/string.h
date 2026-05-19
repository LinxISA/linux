/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2013 Regents of the University of California
 */

#ifndef _ASM_RISCV_STRING_H
#define _ASM_RISCV_STRING_H

#include <linux/types.h>
#include <linux/linkage.h>

#define __HAVE_ARCH_MEMSET
extern asmlinkage void *memset(void *, int, size_t);
extern asmlinkage void *__memset(void *, int, size_t);
/* TODO: memcpy memmove */
/* For those files which don't want to check by kasan. */
#if defined(CONFIG_KASAN) && !defined(__SANITIZE_ADDRESS__)
#define memset(s, c, n) __memset(s, c, n)

#ifndef __NO_FORTIFY
#define __NO_FORTIFY /* FORTIFY_SOURCE uses __builtin_memcpy, etc. */
#endif

#endif
#endif /* _ASM_RISCV_STRING_H */
