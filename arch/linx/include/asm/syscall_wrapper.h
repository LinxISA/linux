/* SPDX-License-Identifier: GPL-2.0 */
/*
 * syscall_wrapper.h - LinxISA syscall wrappers
 *
 * This follows the same structure as other in-kernel ports (e.g. RISC-V):
 * the syscall table contains per-syscall wrapper functions with the uniform
 * signature:
 *
 *   asmlinkage long __linx_sys_<name>(const struct pt_regs *regs)
 *
 * The wrappers extract arguments from pt_regs and invoke the real syscall
 * implementation with correctly typed arguments, avoiding undefined behavior
 * from calling sys_* through an untyped function pointer.
 */

#ifndef __ASM_LINX_SYSCALL_WRAPPER_H
#define __ASM_LINX_SYSCALL_WRAPPER_H

#include <asm/ptrace.h>

asmlinkage long __linx_sys_ni_syscall(const struct pt_regs *);

#define __SYSCALL_SE_DEFINEx(x, prefix, name, ...)				\
	static long __se_##prefix##name(__MAP(x, __SC_LONG, __VA_ARGS__));	\
	static long __se_##prefix##name(__MAP(x, __SC_LONG, __VA_ARGS__))

/*
 * Syscall ABI (LP64):
 * - a0..a5: args 0..5  (R2..R7)
 * - a7:     syscall nr (R9)
 *
 * pt_regs preserves the original a0 in regs->orig_a0.
 */
#define SC_LINX_REGS_TO_ARGS(x, ...)						\
	__MAP(x, __SC_ARGS							\
	      ,,regs->orig_a0,,regs->regs[PTR_R3],,regs->regs[PTR_R4]		\
	      ,,regs->regs[PTR_R5],,regs->regs[PTR_R6],,regs->regs[PTR_R7]	\
	      ,,regs->regs[PTR_R8])

#define __SYSCALL_DEFINEx(x, name, ...)					\
	asmlinkage long __linx_sys##name(const struct pt_regs *regs);		\
	ALLOW_ERROR_INJECTION(__linx_sys##name, ERRNO);				\
	static inline long __do_sys##name(__MAP(x, __SC_DECL, __VA_ARGS__));	\
	__SYSCALL_SE_DEFINEx(x, sys, name, __VA_ARGS__)				\
	{									\
		long ret = __do_sys##name(__MAP(x, __SC_CAST, __VA_ARGS__));	\
		__MAP(x, __SC_TEST, __VA_ARGS__);				\
		__PROTECT(x, ret, __MAP(x, __SC_ARGS, __VA_ARGS__));		\
		return ret;							\
	}									\
	asmlinkage long __linx_sys##name(const struct pt_regs *regs)		\
	{									\
		return __se_sys##name(SC_LINX_REGS_TO_ARGS(x, __VA_ARGS__));	\
	}									\
	static inline long __do_sys##name(__MAP(x, __SC_DECL, __VA_ARGS__))

#define SYSCALL_DEFINE0(sname)							\
	SYSCALL_METADATA(_##sname, 0);						\
	asmlinkage long __linx_sys_##sname(const struct pt_regs *__unused);	\
	ALLOW_ERROR_INJECTION(__linx_sys_##sname, ERRNO);			\
	asmlinkage long __linx_sys_##sname(const struct pt_regs *__unused)

#define COND_SYSCALL(name)							\
	asmlinkage long __weak __linx_sys_##name(const struct pt_regs *regs);	\
	asmlinkage long __weak __linx_sys_##name(const struct pt_regs *regs)	\
	{									\
		return sys_ni_syscall();					\
	}

#endif /* __ASM_LINX_SYSCALL_WRAPPER_H */

