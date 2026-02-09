// SPDX-License-Identifier: GPL-2.0-only

#include <linux/syscalls.h>
#include <linux/unistd.h>

#include <asm-generic/syscalls.h>

/*
 * System call table for the LinxISA bring-up port.
 *
 * For now, use the global scripts/syscall.tbl "common,64" numbering and
 * map to the arch syscall wrapper entry points (__linx_sys_*).
 */

#undef __SYSCALL
#define __SYSCALL(nr, call)		asmlinkage long __linx_##call(const struct pt_regs *);
#define __SYSCALL_WITH_COMPAT(nr, native, compat) __SYSCALL(nr, native)
#define __SYSCALL_NORETURN(nr, call)	__SYSCALL(nr, call)
#define __SYSCALL_COMPAT_NORETURN(nr, native, compat) __SYSCALL(nr, native)

#include <asm/syscall_table_64.h>

void * const sys_call_table[__NR_syscalls] = {
	[0 ... __NR_syscalls - 1] = __linx_sys_ni_syscall,
#undef __SYSCALL
#define __SYSCALL(nr, call)		[nr] = __linx_##call,
#define __SYSCALL_WITH_COMPAT(nr, native, compat) __SYSCALL(nr, native)
#define __SYSCALL_NORETURN(nr, call)	__SYSCALL(nr, call)
#define __SYSCALL_COMPAT_NORETURN(nr, native, compat) __SYSCALL(nr, native)
#include <asm/syscall_table_64.h>
};
