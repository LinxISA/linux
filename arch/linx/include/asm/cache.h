/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_LINX_CACHE_H
#define _ASM_LINX_CACHE_H

#include <linux/const.h>

#define L1_CACHE_SHIFT 6
/*
 * Keep this an 'int' like other architectures so varargs users (eg. SLUB's
 * pr_info("... HWalign=%d ...", cache_line_size())) don't get type-mismatched.
 */
#define L1_CACHE_BYTES (1 << L1_CACHE_SHIFT)

#endif /* _ASM_LINX_CACHE_H */
