/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_TIMEX_H
#define _ASM_RISCV_TIMEX_H

#include <asm/ssr.h>

typedef unsigned long cycles_t;

static inline cycles_t get_cycles(void)
{
	return ssr_read(SSR_TIMER_TIME);
}
#define get_cycles get_cycles

static inline u64 get_cycles64(void)
{
	return get_cycles();
}

#define ARCH_HAS_READ_CURRENT_TIMER
static inline int read_current_timer(unsigned long *timer_val)
{
	*timer_val = get_cycles();
	return 0;
}

extern void time_init(void);

#endif /* _ASM_RISCV_TIMEX_H */
