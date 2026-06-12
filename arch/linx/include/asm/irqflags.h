/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */


#ifndef _ASM_LINX_IRQFLAGS_H
#define _ASM_LINX_IRQFLAGS_H

#include <asm/processor.h>
#include <asm/ssr.h>

/* read interrupt enabled status */
static inline unsigned long arch_local_save_flags(void)
{
	return ssr_read(SSR_CSTATE);
}

/* unconditionally enable interrupts */
static inline void arch_local_irq_enable(void)
{
	ssr_set(SSR_CSTATE, CSTATE_I);
}

/* unconditionally disable interrupts */
static inline void arch_local_irq_disable(void)
{
	ssr_clear(SSR_CSTATE, CSTATE_I);
}

/* get status and disable interrupts */
static inline unsigned long arch_local_irq_save(void)
{
	return ssr_read_clear(SSR_CSTATE, CSTATE_I);
}

/* test flags */
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & CSTATE_I);
}

/* test hardware interrupt enable bit */
static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}

/* set interrupt enabled status */
static inline void arch_local_irq_restore(unsigned long flags)
{
	if (flags & CSTATE_I)
		ssr_set(SSR_CSTATE, CSTATE_I);
	else
		ssr_clear(SSR_CSTATE, CSTATE_I);
}

#endif /* _ASM_LINX_IRQFLAGS_H */
