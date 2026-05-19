/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014 Regents of the University of California
 */

#ifndef _ASM_RISCV_CMPXCHG_H
#define _ASM_RISCV_CMPXCHG_H

#include <linux/bug.h>

#include <asm/barrier.h>
#include <asm/fence.h>
#include <asm/block-def.h>

/*
 * define an stringify atomic swap block used in extended asm
 *
 * @type:
 *     w for 4 bytes integer and d for 8 bytes integer
 * @order:
 *     optional instruction atomic.order attribute, e.g. .aqrl
 * @ret, @new, @ptr:
 *     ret = *ptr, *ptr = new, return @ret
 */
#define ASM_AMOSWAP(type, order, ret, new, ptr)			\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"swap" #type #order " [%[p]], %[n] -> %[r]\n" \
		: [r] "=r" (ret)				\
		: [p] "r" (ptr), [n] "r" (new)			\
		: "memory")

#define __xchg_relaxed(ptr, new, size)					\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(new) __new = (new);					\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 4:								\
		ASM_AMOSWAP(w, , __ret, __new, __ptr);		\
		break;							\
	case 8:								\
		ASM_AMOSWAP(d, , __ret, __new, __ptr);		\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_xchg_relaxed(ptr, x)					\
({									\
	__typeof__(*(ptr)) _x_ = (x);					\
	(__typeof__(*(ptr))) __xchg_relaxed((ptr),			\
					    _x_, sizeof(*(ptr)));	\
})

#define __xchg_acquire(ptr, new, size)					\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(new) __new = (new);					\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 4:								\
		ASM_AMOSWAP(w, .aq,		\
			     __ret, __new, __ptr);			\
		break;							\
	case 8:								\
		ASM_AMOSWAP(d, .aq, 	\
			     __ret, __new, __ptr);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_xchg_acquire(ptr, x)					\
({									\
	__typeof__(*(ptr)) _x_ = (x);					\
	(__typeof__(*(ptr))) __xchg_acquire((ptr),			\
					    _x_, sizeof(*(ptr)));	\
})

#define __xchg_release(ptr, new, size)					\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(new) __new = (new);					\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 4:								\
		ASM_AMOSWAP(w, .rl,		\
			     __ret, __new, __ptr);			\
		break;							\
	case 8:								\
		ASM_AMOSWAP(d, .rl,		\
			     __ret, __new, __ptr);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_xchg_release(ptr, x)					\
({									\
	__typeof__(*(ptr)) _x_ = (x);					\
	(__typeof__(*(ptr))) __xchg_release((ptr),			\
					    _x_, sizeof(*(ptr)));	\
})

#define __xchg(ptr, new, size)						\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(new) __new = (new);					\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 4:								\
		ASM_AMOSWAP(w, .aqrl, __ret, __new, __ptr);		\
		break;							\
	case 8:								\
		ASM_AMOSWAP(d, .aqrl, __ret, __new, __ptr);		\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_xchg(ptr, x)						\
({									\
	__typeof__(*(ptr)) _x_ = (x);					\
	(__typeof__(*(ptr))) __xchg((ptr), _x_, sizeof(*(ptr)));	\
})

#define xchg32(ptr, x)							\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 4);				\
	arch_xchg((ptr), (x));						\
})

#define xchg64(ptr, x)							\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_xchg((ptr), (x));						\
})

/*
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */

/*
 * define an stringify atomic compare and exchange block used in extended asm.
 *
 * @type:
 *     w for 4 bytes integer and d for 8 bytes integer
 * @order:
 *     optional instruction atomic.order attribute, e.g. .rl
 * @ret, @new, @old, @ptr:
 *     ret = *ptr, if (old == ret) *ptr = new, return @ret
 */
#define ASM_CMPXCHG(type, order, ret, new, old, ptr)			\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n" \
			"hl.cas" #type #order " [%[p]], %[o], %[n], -> %[r]\n" \
		: [r] "=&r" (ret)				\
		: [p] "r" (ptr), [o] "r" (old), [n] "r" (new)	\
		: "memory")

#define ASM_CMPXCHG_MASKED(ret, ptr, old, new)				\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	unsigned int *__ptr32 = (unsigned int *)((unsigned long)__ptr & ~0x3UL); \
	unsigned long __shift =					\
		((unsigned long)__ptr & (0x4 - sizeof(*__ptr))) * 8;	\
	unsigned int __mask =					\
		(((1U << (sizeof(*__ptr) * 8)) - 1U) << __shift);	\
	unsigned int __oldword, __newword, __prevword;			\
									\
	for (;;) {							\
		__oldword = *(volatile unsigned int *)__ptr32;		\
		(ret) = (__typeof__(ret))((__oldword & __mask) >> __shift); \
		if ((ret) != (old))					\
			break;						\
		__newword = (__oldword & ~__mask) |			\
			    ((((unsigned int)(new)) << __shift) & __mask); \
		ASM_CMPXCHG(w, .aqrl, __prevword, __newword,		\
			    (long)__oldword, __ptr32);			\
		if (__prevword == __oldword)				\
			break;						\
	}								\
})

#define __cmpxchg_relaxed(ptr, old, new, size)				\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 1:								\
	case 2:								\
		ASM_CMPXCHG_MASKED(__ret, __ptr, __old, __new);		\
		break;							\
	case 4:								\
		ASM_CMPXCHG(w, .aqrl, __ret, __new, (long)__old, __ptr);	\
		break;							\
	case 8:								\
		ASM_CMPXCHG(d, .aqrl, __ret, __new, __old, __ptr);	\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_cmpxchg_relaxed(ptr, o, n)					\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg_relaxed((ptr),			\
					_o_, _n_, sizeof(*(ptr)));	\
})

#define __cmpxchg_acquire(ptr, old, new, size)				\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 1:								\
	case 2:								\
		ASM_CMPXCHG_MASKED(__ret, __ptr, __old, __new);		\
		break;							\
	case 4:								\
		ASM_CMPXCHG(w, .aq,		\
			__ret, __new, (long)__old, __ptr);		\
		break;							\
	case 8:								\
		ASM_CMPXCHG(d, .aq,		\
			__ret, __new, __old, __ptr);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_cmpxchg_acquire(ptr, o, n)					\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg_acquire((ptr),			\
					_o_, _n_, sizeof(*(ptr)));	\
})

#define __cmpxchg_release(ptr, old, new, size)				\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 1:								\
	case 2:								\
		ASM_CMPXCHG_MASKED(__ret, __ptr, __old, __new);		\
		break;							\
	case 4:								\
		ASM_CMPXCHG(w, .rl, 		\
			__ret, __new, (long)__old, __ptr);		\
		break;							\
	case 8:								\
		ASM_CMPXCHG(d, .rl, 		\
			__ret, __new, __old, __ptr);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_cmpxchg_release(ptr, o, n)					\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg_release((ptr),			\
					_o_, _n_, sizeof(*(ptr)));	\
})

#define __cmpxchg(ptr, old, new, size)					\
({									\
	__typeof__(ptr) __ptr = (ptr);					\
	__typeof__(*(ptr)) __old = (old);				\
	__typeof__(*(ptr)) __new = (new);				\
	__typeof__(*(ptr)) __ret;					\
	switch (size) {							\
	case 1:								\
	case 2:								\
		ASM_CMPXCHG_MASKED(__ret, __ptr, __old, __new);		\
		break;							\
	case 4:								\
		ASM_CMPXCHG(w, .aqrl, 		\
			__ret, __new, (long)__old, __ptr);		\
		break;							\
	case 8:								\
		ASM_CMPXCHG(d, .aqrl, 		\
			__ret, __new, __old, __ptr);			\
		break;							\
	default:							\
		BUILD_BUG();						\
	}								\
	__ret;								\
})

#define arch_cmpxchg(ptr, o, n)						\
({									\
	__typeof__(*(ptr)) _o_ = (o);					\
	__typeof__(*(ptr)) _n_ = (n);					\
	(__typeof__(*(ptr))) __cmpxchg((ptr),				\
				       _o_, _n_, sizeof(*(ptr)));	\
})

#define arch_cmpxchg_local(ptr, o, n)					\
	(__cmpxchg_relaxed((ptr), (o), (n), sizeof(*(ptr))))

#define cmpxchg32(ptr, o, n)						\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 4);				\
	arch_cmpxchg((ptr), (o), (n));					\
})

#define cmpxchg32_local(ptr, o, n)					\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 4);				\
	arch_cmpxchg_relaxed((ptr), (o), (n))				\
})

#define arch_cmpxchg64(ptr, o, n)					\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_cmpxchg((ptr), (o), (n));					\
})

#define arch_cmpxchg64_local(ptr, o, n)					\
({									\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);				\
	arch_cmpxchg_relaxed((ptr), (o), (n));				\
})

#endif /* _ASM_RISCV_CMPXCHG_H */
