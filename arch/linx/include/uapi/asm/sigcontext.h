/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */

#ifndef _UAPI_ASM_LINX_SIGCONTEXT_H
#define _UAPI_ASM_LINX_SIGCONTEXT_H

#include <asm/ptrace.h>

#ifndef __ASSEMBLER__

/*
 * Minimal signal context for bring-up.
 *
 * This will evolve as the LinxISA port gains full signal/FPU state support.
 */
struct sigcontext {
	struct user_pt_regs sc_regs;
};

#endif /* __ASSEMBLER__ */

#endif /* _UAPI_ASM_LINX_SIGCONTEXT_H */

