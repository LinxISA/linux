/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_LINX_BRINGUP_H
#define _LINUX_LINX_BRINGUP_H

#include <linux/init.h>

#ifdef CONFIG_LINX_INTC
int __init linx_ksysfs_init(void);
int __init linx_proc_cpuinfo_init(void);
int __init linx_proc_interrupts_init(void);
int __init linx_proc_meminfo_init(void);
#endif

#endif
