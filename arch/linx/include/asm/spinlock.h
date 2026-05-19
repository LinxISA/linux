/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_LINX_SPINLOCK_H
#define _ASM_LINX_SPINLOCK_H

#include <linux/kernel.h>
#include <asm/current.h>
#include <asm/fence.h>

/*
 * Simple spin lock operations.  These provide no fairness guarantees.
 */

/* FIXME: Replace this with a ticket lock, like MIPS. */

#define arch_spin_is_locked(x)	(READ_ONCE((x)->locked) != 0)

static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	smp_store_release(&lock->locked, 0);
}

#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	int tmp = 1, busy;

	__asm__ __volatile__ (
		"	amoswap.w %0, %2, %1\n"
		RISCV_ACQUIRE_BARRIER
		: "=r" (busy), "+A" (lock->lock)
		: "r" (tmp)
		: "memory");

	return !busy;
}

#else

static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	int busy, tmp = 1;
	__typeof__(lock->val.counter) *ptr = &lock->val.counter;

	__asm__ __volatile__(
		"BSTART.sys fall\n"
		"swapw.aq [%1], %2, -> %0\n"
		: [busy] "=r" (busy)
		: [ptr] "r" (ptr), [tmp] "r" (tmp)
		: "memory");

	return !busy;
}

#endif

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	while (1) {
		if (arch_spin_is_locked(lock))
			continue;

		if (arch_spin_trylock(lock))
			break;
	}
}

/***********************************************************/

#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */
static inline void arch_read_lock(arch_rwlock_t *lock)
{
	int tmp;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bltz	%1, 1b\n"
		"	addi	%1, %1, 1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		: "+A" (lock->lock), "=&r" (tmp)
		:: "memory");
}

static inline void arch_write_lock(arch_rwlock_t *lock)
{
	int tmp;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bnez	%1, 1b\n"
		"	li	%1, -1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		: "+A" (lock->lock), "=&r" (tmp)
		:: "memory");
}

static inline int arch_read_trylock(arch_rwlock_t *lock)
{
	int busy;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bltz	%1, 1f\n"
		"	addi	%1, %1, 1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		"1:\n"
		: "+A" (lock->lock), "=&r" (busy)
		:: "memory");

	return !busy;
}

static inline int arch_write_trylock(arch_rwlock_t *lock)
{
	int busy;

	__asm__ __volatile__(
		"1:	lr.w	%1, %0\n"
		"	bnez	%1, 1f\n"
		"	li	%1, -1\n"
		"	sc.w	%1, %1, %0\n"
		"	bnez	%1, 1b\n"
		RISCV_ACQUIRE_BARRIER
		"1:\n"
		: "+A" (lock->lock), "=&r" (busy)
		:: "memory");

	return !busy;
}

static inline void arch_read_unlock(arch_rwlock_t *lock)
{
	__asm__ __volatile__(
		RISCV_RELEASE_BARRIER
		"	amoadd.w x0, %1, %0\n"
		: "+A" (lock->lock)
		: "r" (-1)
		: "memory");
}

#else

static inline void arch_read_lock(arch_rwlock_t *lock)
{
	__typeof__(lock->cnts.counter) *ptr = &lock->cnts.counter;
	unsigned int tmp;
	__asm__ __volatile__(
		"1:\n"
		"BSTART.sys fall\n"
		"lr.w	[%[ptr]], -> %[tmp]\n"
		"BSTART.std cond, 1b\n"
		"setc.lt %[tmp], zero\n"
		"BSTART.sys fall\n"
		"addi	%[tmp], 1, -> t\n"
		"sc.w	t#1, [%[ptr]], -> %[tmp]\n"
		"BSTART.std cond, 1b\n"
		"setc.ne	%[tmp], zero\n"
		ASM_ACQUIRE_BARRIER_BLOCK
		: [tmp] "+r" (tmp)
		: [ptr] "r" (ptr)
		: "memory");
}

static inline void arch_write_lock(arch_rwlock_t *lock)
{
	__typeof__(lock->cnts.counter) *ptr = &lock->cnts.counter;
	int lr_res, sc_res;

	__asm__ __volatile__(
		"1:\n"
		"BSTART.sys fall\n"
		"lr.w	[%[ptr]], -> %[lr_res]\n"
		"BSTART.std cond, 1b\n"
		"setc.ne	%[lr_res], zero\n"
		"BSTART.sys fall\n"
		"subi	zero, 1, -> t\n"
		"sc.w	t#1, [%[ptr]], -> %[sc_res]\n"
		"BSTART.std cond, 1b\n"
		"setc.ne	%[sc_res], zero\n"
		ASM_ACQUIRE_BARRIER_BLOCK
		: [lr_res] "+&r" (lr_res), [sc_res] "+&r" (sc_res)
		: [ptr] "r" (ptr)
		: "memory");
}

static inline int arch_read_trylock(arch_rwlock_t *lock)
{
	int busy;
	int sc_res; /* sys block can't do conditional jump, need pass the result to the next std block and jump */
	__typeof__(lock->cnts.counter) *ptr = &lock->cnts.counter;

	__asm__ __volatile__(
		"1:\n"
		"BSTART.sys fall\n"
		"lr.w	[%[ptr]], -> %[busy]\n"
		"BSTART.std, cond 2f\n"
		"setc.lt	%[busy], zero\n"
		"BSTART.sys fall\n"
		"addi	%[busy], 1, -> %[busy]\n"
		"sc.w	%[busy], [%[ptr]], -> %[sc_res]\n"
		"BSTART.std cond, 1b\n"
		"setc.ne	%[sc_res], zero\n"
		ASM_ACQUIRE_BARRIER_BLOCK
		"2:\n"
		: [busy] "=&r" (busy), [sc_res] "+r" (sc_res)
		: [ptr] "r" (ptr)
		: "memory");

	return !busy;
}

static inline int arch_write_trylock(arch_rwlock_t *lock)
{
	int busy;
	__typeof__(lock->cnts.counter) *ptr = &lock->cnts.counter;

	__asm__ __volatile__(
		"BSTART.sys fall\n"
		"hl.casw [%[ptr]] %[expect] %[val], -> %[busy]"
		: [busy] "=r" (busy)
		: [ptr] "r" (ptr), [expect] "r" (0), [val] "r" (-1)
		: "memory");

	return !busy;
}

static inline void arch_read_unlock(arch_rwlock_t *lock)
{
	__typeof__(lock->cnts.counter) *ptr = &lock->cnts.counter;

	__asm__ __volatile__(
		"BSTART.sys fall\n"
		"subi zero, 1, -> t\n"
		"sw.add.rl [%0], t#1\n"
		:
		: [ptr] "r" (ptr)
		: "memory");
}

#endif

static inline void arch_write_unlock(arch_rwlock_t *lock)
{
	smp_store_release(&lock->cnts.counter, 0);
}

#endif /* _ASM_LINX_SPINLOCK_H */
