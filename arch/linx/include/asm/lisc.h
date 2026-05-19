/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_LISC_H
#define _ASM_LISC_H

#include <linux/types.h>

#ifdef CONFIG_LISC
struct lisc_ret {
	long error;
	long value;
};

void lisc_init(void);

struct lisc_ret lisc_ecall(int ext, int fid, unsigned long arg0,
			   unsigned long arg1, unsigned long arg2,
			   unsigned long arg3, unsigned long arg4,
			   unsigned long arg5);
#else /* CONFIG_LISC */
static inline void lisc_init(void) {}
#endif /* CONFIG_LISC */
#endif /* _ASM_LISC_H */
