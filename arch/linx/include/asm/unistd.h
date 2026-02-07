/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * LinxISA syscall feature selection for asm-generic syscall definitions.
 *
 * This header is included by include/uapi/linux/unistd.h for in-kernel builds
 * and controls which optional syscalls are compiled in.
 */

#define __ARCH_WANT_SYS_CLONE

#if defined(__LP64__) && !defined(__SYSCALL_COMPAT)
#define __ARCH_WANT_NEW_STAT
#endif /* __LP64__ */

#include <uapi/asm/unistd.h>

#define NR_syscalls (__NR_syscalls)

