/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Built-in initramfs image.
 *
 * Kbuild generates `usr/initramfs_inc_data` (a cpio archive, optionally
 * compressed). This assembly file embeds that blob into the vmlinux image and
 * publishes `__initramfs_size` for init/initramfs.c.
 *
 * `__initramfs_start` is defined by the linker script (see INIT_RAM_FS in
 * include/asm-generic/vmlinux.lds.h).
 */

	.section .init.ramfs, "a"
	.balign 4
.L__initramfs_blob_start:
	.incbin "usr/initramfs_inc_data"
.L__initramfs_blob_end:

	.section .init.ramfs.info, "a"
	.balign 8
	.globl __initramfs_size
	.type __initramfs_size, @object
__initramfs_size:
	.quad .L__initramfs_blob_end - .L__initramfs_blob_start
	.size __initramfs_size, 8
