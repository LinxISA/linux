/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_RISCV_ATOMIC_H
#define _ASM_RISCV_ATOMIC_H

#ifdef CONFIG_GENERIC_ATOMIC64
# include <asm-generic/atomic64.h>
#else
# ifndef __linx_xlen
#  if __SIZEOF_POINTER__ == 8
#   define __linx_xlen 64
#  elif __SIZEOF_POINTER__ == 4
#   define __linx_xlen 32
#  endif
# endif
# if (__linx_xlen < 64)
#  error "64-bit atomics require XLEN to be at least 64"
# endif
#endif

#include <asm/cmpxchg.h>
#include <asm/barrier.h>
#include <asm/block-def.h>

#define __atomic_acquire_fence()					\
	__asm__ __volatile__(RISCV_ACQUIRE_BARRIER "" ::: "memory")

#define __atomic_release_fence()					\
	__asm__ __volatile__(RISCV_RELEASE_BARRIER "" ::: "memory");

static __always_inline int arch_atomic_read(const atomic_t *v)
{
	return READ_ONCE(v->counter);
}
static __always_inline void arch_atomic_set(atomic_t *v, int i)
{
	WRITE_ONCE(v->counter, i);
}

#ifndef CONFIG_GENERIC_ATOMIC64
#define ATOMIC64_INIT(i) { (i) }
static __always_inline s64 arch_atomic64_read(const atomic64_t *v)
{
	return READ_ONCE(v->counter);
}
static __always_inline void arch_atomic64_set(atomic64_t *v, s64 i)
{
	WRITE_ONCE(v->counter, i);
}
#endif

/*
 * First, the atomic ops that have no ordering constraints and therefor don't
 * have the AQ or RL bits set.  These don't return anything, so there's only
 * one version to worry about.
 */
#define ATOMIC_OP(op, asm_op, I, asm_type, c_type, prefix)		\
static __always_inline							\
void arch_atomic##prefix##_##op(c_type i, atomic##prefix##_t *v)	\
{									\
	c_type *p_counter = &(v->counter);				\
	__asm__ __volatile__ (						\
		"BSTART.sys fall\n"					\
		"b.attr atomic\n"					\
			"l" #asm_type "i [%0, 0], -> t\n"			\
			"" #asm_op " t#1, %1, -> t\n"			\
			"s" #asm_type "i t#1, [%0, 0]\n"		\
		: "+r" (p_counter)					\
		: "r" (I)						\
		: "memory");						\
}

#ifdef CONFIG_GENERIC_ATOMIC64
#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_OP (op, asm_op, I, w, int,   )
#else
#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_OP (op, asm_op, I, w, int,   )				\
        ATOMIC_OP (op, asm_op, I, d, s64, 64)
#endif

ATOMIC_OPS(add, add,  i)
ATOMIC_OPS(sub, add, -i)
ATOMIC_OPS(and, and,  i)
ATOMIC_OPS( or,  or,  i)
ATOMIC_OPS(xor, xor,  i)

#undef ATOMIC_OP
#undef ATOMIC_OPS

/*
 * Atomic ops that have ordered, relaxed, acquire, and release variants.
 * There's two flavors of these: the arithmatic ops have both fetch and return
 * versions, while the logical ops only have fetch versions.
 */
#define ATOMIC_FETCH_OP(op, asm_op, I, asm_type, c_type, prefix)		\
static __always_inline								\
c_type arch_atomic##prefix##_fetch_##op##_relaxed(c_type i,			\
					     atomic##prefix##_t *v)		\
{										\
	register c_type ret;							\
	c_type *p_counter = &(v->counter);					\
	__asm__ __volatile__ (							\
		"BSTART.sys fall\n"					\
		"b.attr atomic\n"					\
			"l" #asm_type "i [%0, 0], -> t\n"			\
			"" #asm_op " t#1, %2, -> t\n"			\
			"s" #asm_type "i t#1, [%0, 0]\n"		\
			"addi t#2, 0, -> %1\n"						\
		: "+r" (p_counter), "=r" (ret)					\
		: "r" (I)							\
		: "memory");							\
	return ret;								\
}										\
static __always_inline								\
c_type arch_atomic##prefix##_fetch_##op(c_type i, atomic##prefix##_t *v)	\
{										\
	register c_type ret;							\
	c_type *p_counter = &(v->counter);					\
	__asm__ __volatile__ (							\
		"BSTART.sys fall\n"							\
		"b.attr aqrl\n"						\
			"l" #asm_type "i [%0, 0], -> t\n"			\
			"" #asm_op " t#1, %2, -> t\n"			\
			"s" #asm_type "i t#1, [%0, 0]\n"		\
			"addi t#2, 0, -> %1\n"						\
		: "+r" (p_counter), "=r" (ret)					\
		: "r" (I)							\
		: "memory");							\
	return ret;								\
}

#define ATOMIC_OP_RETURN(op, asm_op, c_op, I, asm_type, c_type, prefix)	\
static __always_inline							\
c_type arch_atomic##prefix##_##op##_return_relaxed(c_type i,		\
					      atomic##prefix##_t *v)	\
{									\
        return arch_atomic##prefix##_fetch_##op##_relaxed(i, v) c_op I;	\
}									\
static __always_inline							\
c_type arch_atomic##prefix##_##op##_return(c_type i, atomic##prefix##_t *v)	\
{									\
        return arch_atomic##prefix##_fetch_##op(i, v) c_op I;		\
}

#ifdef CONFIG_GENERIC_ATOMIC64
#define ATOMIC_OPS(op, asm_op, c_op, I)					\
        ATOMIC_FETCH_OP( op, asm_op,       I, w, int,   )		\
        ATOMIC_OP_RETURN(op, asm_op, c_op, I, w, int,   )
#else
#define ATOMIC_OPS(op, asm_op, c_op, I)					\
        ATOMIC_FETCH_OP( op, asm_op,       I, w, int,   )		\
        ATOMIC_OP_RETURN(op, asm_op, c_op, I, w, int,   )		\
        ATOMIC_FETCH_OP( op, asm_op,       I, d, s64, 64)		\
        ATOMIC_OP_RETURN(op, asm_op, c_op, I, d, s64, 64)
#endif

ATOMIC_OPS(add, add, +,  i)
ATOMIC_OPS(sub, add, +, -i)

#define arch_atomic_add_return_relaxed	arch_atomic_add_return_relaxed
#define arch_atomic_sub_return_relaxed	arch_atomic_sub_return_relaxed
#define arch_atomic_add_return		arch_atomic_add_return
#define arch_atomic_sub_return		arch_atomic_sub_return

#define arch_atomic_fetch_add_relaxed	arch_atomic_fetch_add_relaxed
#define arch_atomic_fetch_sub_relaxed	arch_atomic_fetch_sub_relaxed
#define arch_atomic_fetch_add		arch_atomic_fetch_add
#define arch_atomic_fetch_sub		arch_atomic_fetch_sub

#ifndef CONFIG_GENERIC_ATOMIC64
#define arch_atomic64_add_return_relaxed	arch_atomic64_add_return_relaxed
#define arch_atomic64_sub_return_relaxed	arch_atomic64_sub_return_relaxed
#define arch_atomic64_add_return		arch_atomic64_add_return
#define arch_atomic64_sub_return		arch_atomic64_sub_return

#define arch_atomic64_fetch_add_relaxed	arch_atomic64_fetch_add_relaxed
#define arch_atomic64_fetch_sub_relaxed	arch_atomic64_fetch_sub_relaxed
#define arch_atomic64_fetch_add		arch_atomic64_fetch_add
#define arch_atomic64_fetch_sub		arch_atomic64_fetch_sub
#endif

#undef ATOMIC_OPS

#ifdef CONFIG_GENERIC_ATOMIC64
#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_FETCH_OP(op, asm_op, I, w, int,   )
#else
#define ATOMIC_OPS(op, asm_op, I)					\
        ATOMIC_FETCH_OP(op, asm_op, I, w, int,   )			\
        ATOMIC_FETCH_OP(op, asm_op, I, d, s64, 64)
#endif

ATOMIC_OPS(and, and, i)
ATOMIC_OPS( or,  or, i)
ATOMIC_OPS(xor, xor, i)

#define arch_atomic_fetch_and_relaxed	arch_atomic_fetch_and_relaxed
#define arch_atomic_fetch_or_relaxed	arch_atomic_fetch_or_relaxed
#define arch_atomic_fetch_xor_relaxed	arch_atomic_fetch_xor_relaxed
#define arch_atomic_fetch_and		arch_atomic_fetch_and
#define arch_atomic_fetch_or		arch_atomic_fetch_or
#define arch_atomic_fetch_xor		arch_atomic_fetch_xor

#ifndef CONFIG_GENERIC_ATOMIC64
#define arch_atomic64_fetch_and_relaxed	arch_atomic64_fetch_and_relaxed
#define arch_atomic64_fetch_or_relaxed	arch_atomic64_fetch_or_relaxed
#define arch_atomic64_fetch_xor_relaxed	arch_atomic64_fetch_xor_relaxed
#define arch_atomic64_fetch_and		arch_atomic64_fetch_and
#define arch_atomic64_fetch_or		arch_atomic64_fetch_or
#define arch_atomic64_fetch_xor		arch_atomic64_fetch_xor
#endif

#undef ATOMIC_OPS

#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN

/*
 * atomic_{cmp,}xchg is required to have exactly the same ordering semantics as
 * {cmp,}xchg and the operations that return, so they need a full barrier.
 */
#define ATOMIC_OP(c_t, prefix, size)					\
static __always_inline							\
c_t arch_atomic##prefix##_xchg_relaxed(atomic##prefix##_t *v, c_t n)	\
{									\
	return __xchg_relaxed(&(v->counter), n, size);			\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_xchg_acquire(atomic##prefix##_t *v, c_t n)	\
{									\
	return __xchg_acquire(&(v->counter), n, size);			\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_xchg_release(atomic##prefix##_t *v, c_t n)	\
{									\
	return __xchg_release(&(v->counter), n, size);			\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_xchg(atomic##prefix##_t *v, c_t n)		\
{									\
	return __xchg(&(v->counter), n, size);				\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_cmpxchg_relaxed(atomic##prefix##_t *v,	\
				     c_t o, c_t n)			\
{									\
	return __cmpxchg_relaxed(&(v->counter), o, n, size);		\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_cmpxchg_acquire(atomic##prefix##_t *v,	\
				     c_t o, c_t n)			\
{									\
	return __cmpxchg_acquire(&(v->counter), o, n, size);		\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_cmpxchg_release(atomic##prefix##_t *v,	\
				     c_t o, c_t n)			\
{									\
	return __cmpxchg_release(&(v->counter), o, n, size);		\
}									\
static __always_inline							\
c_t arch_atomic##prefix##_cmpxchg(atomic##prefix##_t *v, c_t o, c_t n)	\
{									\
	return __cmpxchg(&(v->counter), o, n, size);			\
}

#ifdef CONFIG_GENERIC_ATOMIC64
#define ATOMIC_OPS()							\
	ATOMIC_OP(int,   , 4)
#else
#define ATOMIC_OPS()							\
	ATOMIC_OP(int,   , 4)						\
	ATOMIC_OP(s64, 64, 8)
#endif

ATOMIC_OPS()

#define arch_atomic_xchg_relaxed	arch_atomic_xchg_relaxed
#define arch_atomic_xchg_acquire	arch_atomic_xchg_acquire
#define arch_atomic_xchg_release	arch_atomic_xchg_release
#define arch_atomic_xchg		arch_atomic_xchg
#define arch_atomic_cmpxchg_relaxed	arch_atomic_cmpxchg_relaxed
#define arch_atomic_cmpxchg_acquire	arch_atomic_cmpxchg_acquire
#define arch_atomic_cmpxchg_release	arch_atomic_cmpxchg_release
#define arch_atomic_cmpxchg		arch_atomic_cmpxchg

#undef ATOMIC_OPS
#undef ATOMIC_OP

#define ASM_ATOMIC_SUB_IF_POSITIVE(type, ret, ptr, offset, tmp)	\
	__asm__ __volatile__ (					\
		"1:					\n"	\
		"BSTART.sys fall	\n" \
			"lr." #type " [%[ptr]] ,-> %[tmp]	\n"	\
		"BSTART.std cond, 4f				\n"	\
			"sub %[tmp], %[offset], -> t		\n"	\
			"addi t#1, 0, -> %[ret]		\n"	\
			"setc.lti t#1, 0		\n"	\
		"2:					\n"	\
		"BSTART.sys fall	\n" \
			"sc." #type ".rl %[ret], [%[ptr]], -> %[tmp]\n"	\
		"BSTART.std cond, 1b				\n"	\
			"setc.nei %[tmp], 0		\n"	\
		ASM_FULL_BARRIER_BLOCK				\
		"4:					\n"	\
		: [ret] "=&r" (ret), [tmp] "=&r" (tmp)				\
		: [ptr] "r" (ptr), [offset] "r" (offset)	\
		: "memory")

static __always_inline int arch_atomic_sub_if_positive(atomic_t *v, int offset)
{
	int ret, tmp;
	__typeof__(&v->counter) ptr = &v->counter;

	ASM_ATOMIC_SUB_IF_POSITIVE(w, ret, ptr, offset, tmp);

	return ret;
}

#define arch_atomic_dec_if_positive(v)	arch_atomic_sub_if_positive(v, 1)

#ifndef CONFIG_GENERIC_ATOMIC64
static __always_inline s64 arch_atomic64_sub_if_positive(atomic64_t *v, s64 offset)
{
	s64 ret, tmp;
	__typeof__(&v->counter) ptr = &v->counter;

	ASM_ATOMIC_SUB_IF_POSITIVE(d, ret, ptr, offset, tmp);

	return ret;
}

#define arch_atomic64_dec_if_positive(v)	arch_atomic64_sub_if_positive(v, 1)
#endif

#endif /* _ASM_RISCV_ATOMIC_H */
