/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2006  Ralf Baechle (ralf@linux-mips.org)
 * Copyright (c) 2018  Jim Wilson (jimw@sifive.com)
 */

#ifndef _ASM_RISCV_FUTEX_H
#define _ASM_RISCV_FUTEX_H

#include <linux/futex.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <asm/asm.h>
#include <asm/block-def.h>

/* We don't even really need the extable code, but for now keep it simple */
#ifndef CONFIG_MMU
#define __enable_user_access()		do { } while (0)
#define __disable_user_access()		do { } while (0)
#endif

/* "amoswap.w.aqrl %[ov],%z[op],%[u]" */
#define AMOSWAP_W_INSN			\
	"addi %[u], 0, -> t	\n"	\
	"lwi  [t#1, 0], -> t	\n"	\
	"addi %[op], 0, -> t	\n"	\
	"swi  t#1, [t#3, 0]	\n"	\
	"addi t#3, 0, -> %[ov]	\n"	\

/* "amoadd.w.aqrl %[ov],%z[op],%[u]" */
#define AMOADD_W_INSN			\
	"addi %[u], 0, -> t	\n"	\
	"lwi  [t#1, 0], -> t	\n"	\
	"addi %[op], 0, -> t	\n"	\
	"add t#1, t#2, -> t	\n"	\
	"swi  t#1, [t#4, 0]	\n"	\
	"addi t#4, 0, -> %[ov]	\n"	\

/* "amoor.w.aqrl %[ov],%z[op],%[u]" */
#define AMOOR_W_INSN			\
	"addi %[u], 0, -> t	\n"	\
	"lwi  [t#1, 0], -> t	\n"	\
	"addi %[op], 0, -> t	\n"	\
	"or  t#1, t#2, -> t	\n"	\
	"swi  t#1, [t#4, 0]	\n"	\
	"addi t#4, 0, -> %[ov]	\n"	\

/* "amoand.w.aqrl %[ov],%z[op],%[u]" */
#define AMOAND_W_INSN			\
	"addi %[u], 0, -> t	\n"	\
	"lwi [t#1, 0], -> t	\n"	\
	"addi %[op], 0, -> t	\n"	\
	"and t#1, t#2, -> t	\n"	\
	"swi  t#1, [t#4, 0]	\n"	\
	"addi t#4, 0, -> %[ov]	\n"	\

/* "amoxor.w.aqrl %[ov],%z[op],%[u]" */
#define AMOXOR_W_INSN			\
	"addi %[u], 0, -> t	\n"	\
	"lwi [t#1, 0], -> t	\n"	\
	"addi %[op], 0, -> t	\n"	\
	"xor t#1, t#2, -> t	\n"	\
	"swi  t#1, [t#4, 0]	\n"	\
	"addi t#4, 0, -> %[ov]	\n"	\

#define __futex_atomic_op(insn, ret, oldval, uaddr, oparg)	\
{								\
	__enable_user_access();					\
	__asm__ __volatile__ (					\
		"1:					\n"	\
		"BSTART.std fall, 3f			\n"	\
		"B.CATR aqrl				\n"	\
			"" insn "			\n"	\
		"2:					\n"	\
		".section .fixup,\"ax\"			\n"	\
		".balign 16				\n"	\
		"3:					\n"	\
		"BSTART.std direct, 2b			\n"	\
			"subi zero, %[e], -> %[r]	\n"	\
		".previous				\n"	\
		: [r] "+r" (ret), [ov] "=&r" (oldval)		\
		: [u] "r" (uaddr), [op] "r" (oparg),		\
		  [e] "i" (EFAULT)				\
		: "memory");					\
	__disable_user_access();				\
}

static inline int
arch_futex_atomic_op_inuser(int op, int oparg, int *oval, u32 __user *uaddr)
{
	int oldval = 0, ret = 0;

	if (!access_ok(uaddr, sizeof(u32)))
		return -EFAULT;

	switch (op) {
	case FUTEX_OP_SET:
		__futex_atomic_op(AMOSWAP_W_INSN,
				  ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ADD:
		__futex_atomic_op(AMOADD_W_INSN,
				  ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_OR:
		__futex_atomic_op(AMOOR_W_INSN,
				  ret, oldval, uaddr, oparg);
		break;
	case FUTEX_OP_ANDN:
		__futex_atomic_op(AMOAND_W_INSN,
				  ret, oldval, uaddr, ~oparg);
		break;
	case FUTEX_OP_XOR:
		__futex_atomic_op(AMOXOR_W_INSN,
				  ret, oldval, uaddr, oparg);
		break;
	default:
		ret = -ENOSYS;
	}

	if (!ret)
		*oval = oldval;

	return ret;
}

#undef AMOSWAP_W_INSN
#undef AMOADD_W_INSN
#undef AMOOR_W_INSN
#undef AMOAND_W_INSN
#undef AMOXOR_W_INSN

static inline int
futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
			      u32 oldval, u32 newval)
{
	int ret = 0;
	int equal;
	u32 val;

	if (!access_ok(uaddr, sizeof(u32)))
		return -EFAULT;

	__enable_user_access();
	__asm__ __volatile__ (
		"10:					\n"
		"BSTART.sys fall, 3f			\n"
			"lr.w.aqrl [%[u]], -> t		\n"
			"addi t#1, 0, -> %[v]		\n"
			"sub %[ov], t#1, -> %[equ]	\n"
		"BSTART.std cond, 1f			\n"
			"setc.ne %[equ], zero		\n"
		"BSTART.std cond, 10b			\n"
			"sc.w.aqrl %[nv], [%[u]] -> t	\n"
			"setc.ne t#1, zero		\n"
		"BSTART.std direct, 1f			\n"
			"bstop				\n"
		"3:					\n"
		"BSTART.std ret\n"
			"subi zero, %[e], -> %[r]	\n"
			"setc.tgt ra\n"
		".previous\n"
		"1:\n"
		: [r] "+r" (ret), [v] "=&r" (val), [equ] "+r" (equal)
		: [u] "r" (uaddr), [ov] "r" (oldval),
		  [nv] "r" (newval), [e] "i" (EFAULT)
		: "memory");
	__disable_user_access();

	*uval = val;
	return ret;
}

#endif /* _ASM_RISCV_FUTEX_H */
