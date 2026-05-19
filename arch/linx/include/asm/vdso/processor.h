/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_VDSO_PROCESSOR_H
#define __ASM_VDSO_PROCESSOR_H

#ifndef __ASSEMBLY__

#include <asm/barrier.h>

static inline void cpu_relax(void)
{
#ifdef __riscv_muldiv
	#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */
	int dummy;
	/* In lieu of a halt instruction, induce a long-latency stall. */
	__asm__ __volatile__ ("div %0, %0, zero" : "=r" (dummy));
	#else
	#endif
#endif
	barrier();
}

#endif /* __ASSEMBLY__ */

#endif /* __ASM_VDSO_PROCESSOR_H */
