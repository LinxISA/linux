/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ASM_SECCOMP_H
#define _ASM_SECCOMP_H

#include <asm/unistd.h>

#include <asm-generic/seccomp.h>

#ifdef CONFIG_64BIT
# define SECCOMP_ARCH_NATIVE		AUDIT_ARCH_LINX64
# define SECCOMP_ARCH_NATIVE_NR		NR_syscalls
# define SECCOMP_ARCH_NATIVE_NAME	"linx64"
#else /* !CONFIG_64BIT */
/* no 32bit supported for linx now */
# define SECCOMP_ARCH_NATIVE		AUDIT_ARCH_RISCV32
# define SECCOMP_ARCH_NATIVE_NR		NR_syscalls
# define SECCOMP_ARCH_NATIVE_NAME	"riscv32"
#endif

#endif /* _ASM_SECCOMP_H */
