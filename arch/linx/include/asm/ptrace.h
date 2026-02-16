/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PTRACE_H
#define _ASM_LINX_PTRACE_H

#include <uapi/asm/ptrace.h>

#ifndef __ASSEMBLER__

/*
 * Early bring-up pt_regs: enough to satisfy generic code and basic tracing.
 * The exact layout is not yet ABI-stable.
 */
struct pt_regs {
	unsigned long regs[NUM_PTRACE_REG];
	/* Syscall entry uses a0 for return value; preserve original arg0. */
	unsigned long orig_a0;

	/*
	 * Trap metadata saved from the managing ACR's banked SSRs.
	 *
	 * For QEMU bring-up, ECSTATE is the pre-trap CSTATE (includes ACR level).
	 */
	unsigned long ecstate;
	unsigned long trapno;
	unsigned long traparg0;
	unsigned long ebarg0;
	unsigned long ebarg_bpc_cur;
	unsigned long ebarg_bpc_tgt;
	unsigned long ebarg_tpc;
	unsigned long ebarg_lra;
	unsigned long ebarg_tq[4];
	unsigned long ebarg_uq[4];
	unsigned long ebarg_lb;
	unsigned long ebarg_lc;
	unsigned long ebarg_ext_ptr;
	unsigned long ebarg_ext_meta;
};

/*
 * Bring-up privilege model (QEMU): ACR=2 is "user". ECSTATE encodes the
 * trapped-from CSTATE, so check its ACR field rather than current CSTATE.
 */
#define LINX_CSTATE_ACR_MASK		(0xFULL)
#define LINX_CSTATE_ACR_USER		(2UL)

#define user_mode(regs_ptr)		(((regs_ptr)->ecstate & LINX_CSTATE_ACR_MASK) == LINX_CSTATE_ACR_USER)

#define instruction_pointer(regs_ptr)	((regs_ptr)->regs[PTR_PC])
#define profile_pc(regs_ptr)		instruction_pointer(regs_ptr)
#define user_stack_pointer(regs_ptr)	((regs_ptr)->regs[PTR_R1])

#endif /* !__ASSEMBLER__ */

#endif /* _ASM_LINX_PTRACE_H */
