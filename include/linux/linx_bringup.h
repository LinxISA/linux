/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_LINX_BRINGUP_H
#define _LINUX_LINX_BRINGUP_H

#include <linux/init.h>

#ifdef CONFIG_LINX_INTC
int __init linx_bio_init(void);
int __init linx_bdi_class_init(void);
int __init linx_blk_ioc_init(void);
int __init linx_blk_mq_init(void);
int __init linx_blkdev_init(void);
int __init linx_cgwb_init(void);
int __init linx_default_bdi_init(void);
int __init linx_ext2_init_fs(void);
int __init linx_filelock_init(void);
int __init linx_genhd_device_init(void);
int __init linx_ksysfs_init(void);
int __init linx_p9_init(void);
int __init linx_p9_virtio_init(void);
int __init linx_proc_cpuinfo_init(void);
int __init linx_proc_interrupts_init(void);
int __init linx_proc_meminfo_init(void);
int __init linx_v9fs_init(void);
bool __init linx_root_device_requested(void);
bool __init linx_storage_init_requested(void);
int __init linx_virtio_blk_init(void);
int __init linx_virtio_init(void);
int __init linx_virtio_mmio_init(void);
int __init linx_virtio_mmio_populate_of(void);
#endif

#endif
