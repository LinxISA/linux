/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_VMALLOC_H
#define _ASM_LINX_VMALLOC_H

/*
 * Define a conservative vmalloc window for bring-up.
 */

#ifdef CONFIG_MMU
/*
 * Keep vmalloc well above the direct-mapped low RAM region (0..RAM end) and
 * below the default user mmap base (TASK_SIZE/3).
 */
#define VMALLOC_START	0x40000000UL
#define VMALLOC_END	0x0000100000000000UL
#else
#define VMALLOC_START	0UL
#define VMALLOC_END	0UL
#endif

#endif /* _ASM_LINX_VMALLOC_H */
