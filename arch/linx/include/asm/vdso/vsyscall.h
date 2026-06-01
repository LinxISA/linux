/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_VDSO_VSYSCALL_H
#define __ASM_VDSO_VSYSCALL_H

#ifndef __ASSEMBLER__

#include <linux/timekeeper_internal.h>
#include <vdso/datapage.h>

/*
 * Keep the VDSO-internal header independent from generated vdso offsets.
 * Fresh O= builds need to compile vgettimeofday.o before vdso-offsets.h exists.
 */
#define __VDSO_PAGES 2

extern struct vdso_data *vdso_data;
/*
 * Update the vDSO data page to keep in sync with kernel timekeeping.
 */
static __always_inline struct vdso_data *__riscv_get_k_vdso_data(void)
{
	return vdso_data;
}

#define __arch_get_k_vdso_data __riscv_get_k_vdso_data

#define __arch_get_vdso_u_time_data __linx_get_vdso_u_time_data
static __always_inline const struct vdso_time_data *__linx_get_vdso_u_time_data(void)
{
	__label__ __linx_vdso_here;
	unsigned long text_addr;

__linx_vdso_here:
	text_addr = (unsigned long)&&__linx_vdso_here;
	unsigned long text_page = text_addr & PAGE_MASK;

	return (const struct vdso_time_data *)(text_page - (__VDSO_PAGES * PAGE_SIZE));
}

/* The asm-generic header needs to be included after the definitions above */
#include <asm-generic/vdso/vsyscall.h>

#endif /* !__ASSEMBLER__ */

#endif /* __ASM_VDSO_VSYSCALL_H */
