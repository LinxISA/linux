/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_ATOMIC_H
#define _ASM_LINX_ATOMIC_H

/*
 * Early bring-up: rely on the asm-generic implementations.
 *
 * - atomic_t uses irq disabling (UP) or cmpxchg (SMP).
 * - atomic64_t uses the generic spinlock-backed helpers.
 */

#include <asm-generic/atomic.h>

#ifdef CONFIG_64BIT

#define ATOMIC64_INIT(i)	{ (i) }

extern s64 generic_atomic64_read(const atomic64_t *v);
extern void generic_atomic64_set(atomic64_t *v, s64 i);

extern void generic_atomic64_add(s64 a, atomic64_t *v);
extern s64 generic_atomic64_add_return(s64 a, atomic64_t *v);
extern s64 generic_atomic64_fetch_add(s64 a, atomic64_t *v);
extern void generic_atomic64_sub(s64 a, atomic64_t *v);
extern s64 generic_atomic64_sub_return(s64 a, atomic64_t *v);
extern s64 generic_atomic64_fetch_sub(s64 a, atomic64_t *v);

extern void generic_atomic64_and(s64 a, atomic64_t *v);
extern s64 generic_atomic64_fetch_and(s64 a, atomic64_t *v);
extern void generic_atomic64_or(s64 a, atomic64_t *v);
extern s64 generic_atomic64_fetch_or(s64 a, atomic64_t *v);
extern void generic_atomic64_xor(s64 a, atomic64_t *v);
extern s64 generic_atomic64_fetch_xor(s64 a, atomic64_t *v);

extern s64 generic_atomic64_dec_if_positive(atomic64_t *v);
extern s64 generic_atomic64_cmpxchg(atomic64_t *v, s64 o, s64 n);
extern s64 generic_atomic64_xchg(atomic64_t *v, s64 new);
extern s64 generic_atomic64_fetch_add_unless(atomic64_t *v, s64 a, s64 u);

#define arch_atomic64_read		generic_atomic64_read
#define arch_atomic64_set		generic_atomic64_set
#define arch_atomic64_set_release	generic_atomic64_set

#define arch_atomic64_add		generic_atomic64_add
#define arch_atomic64_add_return	generic_atomic64_add_return
#define arch_atomic64_fetch_add		generic_atomic64_fetch_add
#define arch_atomic64_sub		generic_atomic64_sub
#define arch_atomic64_sub_return	generic_atomic64_sub_return
#define arch_atomic64_fetch_sub		generic_atomic64_fetch_sub

#define arch_atomic64_and		generic_atomic64_and
#define arch_atomic64_fetch_and		generic_atomic64_fetch_and
#define arch_atomic64_or		generic_atomic64_or
#define arch_atomic64_fetch_or		generic_atomic64_fetch_or
#define arch_atomic64_xor		generic_atomic64_xor
#define arch_atomic64_fetch_xor		generic_atomic64_fetch_xor

#define arch_atomic64_dec_if_positive	generic_atomic64_dec_if_positive
#define arch_atomic64_cmpxchg		generic_atomic64_cmpxchg
#define arch_atomic64_xchg		generic_atomic64_xchg
#define arch_atomic64_fetch_add_unless	generic_atomic64_fetch_add_unless

#endif /* CONFIG_64BIT */

#endif /* _ASM_LINX_ATOMIC_H */
