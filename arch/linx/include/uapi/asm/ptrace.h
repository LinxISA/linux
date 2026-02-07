/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */

#ifndef _UAPI_ASM_LINX_PTRACE_H
#define _UAPI_ASM_LINX_PTRACE_H

#ifndef __ASSEMBLER__

#include <linux/types.h>

/*
 * Minimal bring-up view of LinxISA registers for ptrace-style interfaces.
 *
 * Register file: R0..R23 (see Documentation/linxisa/abi.md).
 */
#define PTR_R0  0
#define PTR_R1  1
#define PTR_R2  2
#define PTR_R3  3
#define PTR_R4  4
#define PTR_R5  5
#define PTR_R6  6
#define PTR_R7  7
#define PTR_R8  8
#define PTR_R9  9
#define PTR_R10 10
#define PTR_R11 11
#define PTR_R12 12
#define PTR_R13 13
#define PTR_R14 14
#define PTR_R15 15
#define PTR_R16 16
#define PTR_R17 17
#define PTR_R18 18
#define PTR_R19 19
#define PTR_R20 20
#define PTR_R21 21
#define PTR_R22 22
#define PTR_R23 23

#define PTR_PC 24

#define NUM_PTRACE_REG 25

/*
 * FDPIC ptrace hooks: keep the request numbers aligned with other 64-bit ports
 * (e.g. RISC-V) so generic code can compile cleanly.
 */
#define PTRACE_GETFDPIC		33
#define PTRACE_GETFDPIC_EXEC	0
#define PTRACE_GETFDPIC_INTERP	1

struct user_pt_regs {
	__u64 regs[NUM_PTRACE_REG];
};

#endif /* __ASSEMBLER__ */

#endif /* _UAPI_ASM_LINX_PTRACE_H */
