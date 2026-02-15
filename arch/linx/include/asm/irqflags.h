/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_IRQFLAGS_H
#define _ASM_LINX_IRQFLAGS_H

/*
 * LinxISA bring-up: use CSTATE.I as the interrupt enable bit (as modeled by
 * the QEMU LinxISA CPU).
 */

#include <asm/ssr.h>

static __always_inline unsigned long arch_local_save_flags(void)
{
	return linx_ssr_read_cstate();
}

static __always_inline void arch_local_irq_enable(void)
{
	unsigned long cstate = linx_ssr_read_cstate();

	linx_ssr_write_cstate(cstate | LINX_CSTATE_I_BIT);
}

static __always_inline void arch_local_irq_disable(void)
{
	unsigned long cstate = linx_ssr_read_cstate();

	linx_ssr_write_cstate(cstate & ~LINX_CSTATE_I_BIT);
}

static __always_inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags = arch_local_save_flags();

	arch_local_irq_disable();
	return flags;
}

static __always_inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & LINX_CSTATE_I_BIT);
}

static __always_inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}

static __always_inline void arch_local_irq_restore(unsigned long flags)
{
	unsigned long cstate = linx_ssr_read_cstate();

	if (flags & LINX_CSTATE_I_BIT)
		linx_ssr_write_cstate(cstate | LINX_CSTATE_I_BIT);
	else
		linx_ssr_write_cstate(cstate & ~LINX_CSTATE_I_BIT);
}

static __always_inline void arch_safe_halt(void)
{
}

#endif /* _ASM_LINX_IRQFLAGS_H */
