/* SPDX-License-Identifier: GPL-2.0-only */

#include <asm/asm-offsets.h>

/*
 * LinxISA Linux bring-up: trap vector + userspace entry.
 *
 * The QEMU bring-up model does not automatically switch stacks on trap entry.
 * The kernel maintains the current task's kernel-stack-top in ETEMP0_ACR1.
 * Trap entry from ACR2 (userspace) switches to that kernel stack.
 *
 * The managing ACR's banked SSRs provide the trap metadata (ECSTATE/TRAPNO/
 * TRAPARG0/EBARG*). We snapshot them into pt_regs and then dispatch into C.
 */

/* ACR1 (kernel) banked SSR IDs (bring-up profile). */
.set SSR_ECSTATE_ACR1,         0x1f00
.set SSR_TRAPNO_ACR1,          0x1f02
.set SSR_TRAPARG0_ACR1,        0x1f03
.set SSR_ETEMP_ACR1,           0x1f05
.set SSR_ETEMP0_ACR1,          0x1f06

.set SSR_EBARG0_ACR1,          0x1f40
.set SSR_EBARG_BPC_CUR_ACR1,   0x1f41
.set SSR_EBARG_BPC_TGT_ACR1,   0x1f42
.set SSR_EBARG_TPC_ACR1,       0x1f43
.set SSR_EBARG_LRA_ACR1,       0x1f44
.set SSR_EBARG_TQ0_ACR1,       0x1f45
.set SSR_EBARG_TQ1_ACR1,       0x1f46
.set SSR_EBARG_TQ2_ACR1,       0x1f47
.set SSR_EBARG_TQ3_ACR1,       0x1f48
.set SSR_EBARG_UQ0_ACR1,       0x1f49
.set SSR_EBARG_UQ1_ACR1,       0x1f4a
.set SSR_EBARG_UQ2_ACR1,       0x1f4b
.set SSR_EBARG_UQ3_ACR1,       0x1f4c
.set SSR_EBARG_LB_ACR1,        0x1f4d
.set SSR_EBARG_LC_ACR1,        0x1f4e
.set SSR_EBARG_EXT_PTR_ACR1,   0x1f4f
.set SSR_EBARG_EXT_META_ACR1,  0x1f50

	.text

	.globl linx_trap_vector_acr0
	.type linx_trap_vector_acr0,@function
linx_trap_vector_acr0:
	/* ACR0 is not used by the Linux bring-up port; hang if vectored here. */
	C.BSTART DIRECT, linx_trap_vector_acr0
	C.BSTOP

	.globl linx_trap_vector
	.type linx_trap_vector,@function
linx_trap_vector:
	/*
	 * Decide whether we trapped from userspace (ACR2). ECSTATE_ACR1.ACR holds
	 * the trapped-from ACR. Preserve the incoming SP in ETEMP_ACR1 while we
	 * use SP as a scratch register for the decision.
	 */
	C.BSTART COND, .Llinx_trap_from_user
	hl.ssrset sp, SSR_ETEMP_ACR1
	hl.ssrget SSR_ECSTATE_ACR1, ->sp
	andi sp, 15, ->sp
	subi sp, 2, ->sp
	c.setc.eq sp, zero
	C.BSTOP

.Llinx_trap_from_kernel:
	/* Trapped from kernel ACR1: restore original SP and jump to common path. */
	C.BSTART DIRECT, .Llinx_trap_common
	hl.ssrget SSR_ETEMP_ACR1, ->sp
	C.BSTOP

.Llinx_trap_from_user:
	/* Trapped from userspace ACR2: switch to kernel stack top and continue. */
	C.BSTART DIRECT, .Llinx_trap_common
	hl.ssrget SSR_ETEMP0_ACR1, ->sp
	C.BSTOP

.Llinx_trap_common:
	C.BSTART.STD
	/* Allocate pt_regs on the (kernel) stack. */
	subi sp, PT_REGS_SIZE, ->sp

	/* Save GPRs. (SP is captured separately from ETEMP_ACR1.) */
	sdi zero, [sp, PT_REGS_R0]
	sdi a0,   [sp, PT_REGS_R2]
	sdi a1,   [sp, PT_REGS_R3]
	sdi a2,   [sp, PT_REGS_R4]
	sdi a3,   [sp, PT_REGS_R5]
	sdi a4,   [sp, PT_REGS_R6]
	sdi a5,   [sp, PT_REGS_R7]
	sdi a6,   [sp, PT_REGS_R8]
	sdi a7,   [sp, PT_REGS_R9]
	sdi ra,   [sp, PT_REGS_R10]
	sdi s0,   [sp, PT_REGS_R11]
	sdi s1,   [sp, PT_REGS_R12]
	sdi s2,   [sp, PT_REGS_R13]
	sdi s3,   [sp, PT_REGS_R14]
	sdi s4,   [sp, PT_REGS_R15]
	sdi s5,   [sp, PT_REGS_R16]
	sdi s6,   [sp, PT_REGS_R17]
	sdi s7,   [sp, PT_REGS_R18]
	sdi s8,   [sp, PT_REGS_R19]
	sdi x0,   [sp, PT_REGS_R20]
	sdi x1,   [sp, PT_REGS_R21]
	sdi x2,   [sp, PT_REGS_R22]
	sdi x3,   [sp, PT_REGS_R23]

	/* Save trapped SP from ETEMP_ACR1 into pt_regs. */
	hl.ssrget SSR_ETEMP_ACR1, ->x0
	sdi x0, [sp, PT_REGS_R1]

	/* Snapshot trap metadata from ACR1 banked SSRs into pt_regs. */
	hl.ssrget SSR_ECSTATE_ACR1, ->x0
	sdi x0, [sp, PT_REGS_ECSTATE]
	hl.ssrget SSR_TRAPNO_ACR1, ->x0
	sdi x0, [sp, PT_REGS_TRAPNO]
	hl.ssrget SSR_TRAPARG0_ACR1, ->x0
	sdi x0, [sp, PT_REGS_TRAPARG0]

	hl.ssrget SSR_EBARG0_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG0]
	hl.ssrget SSR_EBARG_BPC_CUR_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_BPC_CUR]
	hl.ssrget SSR_EBARG_BPC_TGT_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_BPC_TGT]
	hl.ssrget SSR_EBARG_TPC_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_TPC]
	/* Bring-up: treat EBARG_TPC as the resume PC exposed via pt_regs->pc. */
	sdi x0, [sp, PT_REGS_PC]
	hl.ssrget SSR_EBARG_LRA_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_LRA]

	hl.ssrget SSR_EBARG_TQ0_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_TQ0]
	hl.ssrget SSR_EBARG_TQ1_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_TQ1]
	hl.ssrget SSR_EBARG_TQ2_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_TQ2]
	hl.ssrget SSR_EBARG_TQ3_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_TQ3]

	hl.ssrget SSR_EBARG_UQ0_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_UQ0]
	hl.ssrget SSR_EBARG_UQ1_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_UQ1]
	hl.ssrget SSR_EBARG_UQ2_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_UQ2]
	hl.ssrget SSR_EBARG_UQ3_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_UQ3]

	hl.ssrget SSR_EBARG_LB_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_LB]
	hl.ssrget SSR_EBARG_LC_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_LC]
	hl.ssrget SSR_EBARG_EXT_PTR_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_EXT_PTR]
	hl.ssrget SSR_EBARG_EXT_META_ACR1, ->x0
	sdi x0, [sp, PT_REGS_EBARG_EXT_META]

	/* Default orig_a0 = -1 ("not in syscall") for generic core code. */
	addi zero, -1, ->x0
	sdi x0, [sp, PT_REGS_ORIG_A0]

	/* Dispatch into C trap handler: linx_do_trap(struct pt_regs *regs). */
	BSTART CALL, linx_do_trap, ra=.Llinx_after_do_trap
	c.movr sp, ->a0
	C.BSTOP

.Llinx_after_do_trap:
	C.BSTART.STD
	/* Deliver signals / resume-work before returning to user mode (no-op for kernel). */
	BSTART CALL, do_notify_resume, ra=.Llinx_after_notify
	c.movr sp, ->a0
	C.BSTOP

.Llinx_after_notify:
	/*
	 * If do_notify_resume() modified pt_regs->pc (e.g. signal delivery),
	 * return with RRAT_DEFAULT and update EBARG to the new PC. Otherwise,
	 * return with RRAT_RESTORE to preserve BSTATE for normal syscall/trap return.
	 */
	C.BSTART COND, .Llinx_return_default
	c.movr sp, ->x3
	ldi [x3, PT_REGS_PC], ->x0
	hl.ssrget SSR_EBARG_TPC_ACR1, ->x1
	c.setc.ne x0, x1
	C.BSTOP

.Llinx_return_restore:
	C.BSTART.STD
	/* Restore registers from pt_regs and return via ACRE RRAT_RESTORE (1). */
	c.movr sp, ->x3

	ldi [x3, PT_REGS_R2],  ->a0
	ldi [x3, PT_REGS_R3],  ->a1
	ldi [x3, PT_REGS_R4],  ->a2
	ldi [x3, PT_REGS_R5],  ->a3
	ldi [x3, PT_REGS_R6],  ->a4
	ldi [x3, PT_REGS_R7],  ->a5
	ldi [x3, PT_REGS_R8],  ->a6
	ldi [x3, PT_REGS_R9],  ->a7
	ldi [x3, PT_REGS_R10], ->ra
	ldi [x3, PT_REGS_R11], ->s0
	ldi [x3, PT_REGS_R12], ->s1
	ldi [x3, PT_REGS_R13], ->s2
	ldi [x3, PT_REGS_R14], ->s3
	ldi [x3, PT_REGS_R15], ->s4
	ldi [x3, PT_REGS_R16], ->s5
	ldi [x3, PT_REGS_R17], ->s6
	ldi [x3, PT_REGS_R18], ->s7
	ldi [x3, PT_REGS_R19], ->s8
	ldi [x3, PT_REGS_R20], ->x0
	ldi [x3, PT_REGS_R21], ->x1
	ldi [x3, PT_REGS_R22], ->x2
	ldi [x3, PT_REGS_R23], ->x3
	ldi [sp, PT_REGS_R1],  ->sp
	acre 1

.Llinx_return_default:
	C.BSTART.STD
	/* Update EBARG resume PC to pt_regs->pc, then return via ACRE RRAT_DEFAULT (0). */
	c.movr sp, ->x3
	ldi [x3, PT_REGS_PC], ->x0
	hl.ssrset x0, SSR_EBARG_BPC_CUR_ACR1
	hl.ssrset x0, SSR_EBARG_BPC_TGT_ACR1
	hl.ssrset x0, SSR_EBARG_TPC_ACR1

	ldi [x3, PT_REGS_R2],  ->a0
	ldi [x3, PT_REGS_R3],  ->a1
	ldi [x3, PT_REGS_R4],  ->a2
	ldi [x3, PT_REGS_R5],  ->a3
	ldi [x3, PT_REGS_R6],  ->a4
	ldi [x3, PT_REGS_R7],  ->a5
	ldi [x3, PT_REGS_R8],  ->a6
	ldi [x3, PT_REGS_R9],  ->a7
	ldi [x3, PT_REGS_R10], ->ra
	ldi [x3, PT_REGS_R11], ->s0
	ldi [x3, PT_REGS_R12], ->s1
	ldi [x3, PT_REGS_R13], ->s2
	ldi [x3, PT_REGS_R14], ->s3
	ldi [x3, PT_REGS_R15], ->s4
	ldi [x3, PT_REGS_R16], ->s5
	ldi [x3, PT_REGS_R17], ->s6
	ldi [x3, PT_REGS_R18], ->s7
	ldi [x3, PT_REGS_R19], ->s8
	ldi [x3, PT_REGS_R20], ->x0
	ldi [x3, PT_REGS_R21], ->x1
	ldi [x3, PT_REGS_R22], ->x2
	ldi [x3, PT_REGS_R23], ->x3
	ldi [sp, PT_REGS_R1],  ->sp
	acre 0

	.globl linx_enter_user
	.type linx_enter_user,@function
linx_enter_user:
	/*
	 * Transition from the kernel ring (ACR1) into the user ring (ACR2) using
	 * a pt_regs frame prepared by execve()/start_thread() or fork/clone.
	 *
	 * The pt_regs pointer is passed in a0. We rebase SP to that frame so we
	 * can restore registers without needing an extra base register, then we
	 * seed ECSTATE/EBARG for ACRE and enter userspace via ACRE RRAT_DEFAULT.
	 */
	C.BSTART.STD
	c.movr a0, ->sp

	/* Seed ACR-enter state from the pt_regs snapshot. */
	ldi [sp, PT_REGS_ECSTATE], ->x0
	hl.ssrset x0, SSR_ECSTATE_ACR1

	ldi [sp, PT_REGS_EBARG0], ->x0
	hl.ssrset x0, SSR_EBARG0_ACR1
	ldi [sp, PT_REGS_EBARG_BPC_CUR], ->x0
	hl.ssrset x0, SSR_EBARG_BPC_CUR_ACR1
	ldi [sp, PT_REGS_EBARG_BPC_TGT], ->x0
	hl.ssrset x0, SSR_EBARG_BPC_TGT_ACR1
	ldi [sp, PT_REGS_EBARG_TPC], ->x0
	hl.ssrset x0, SSR_EBARG_TPC_ACR1

	/* Restore user GPRs from pt_regs and enter user ACR via ACRE. */
	ldi [sp, PT_REGS_R2],  ->a0
	ldi [sp, PT_REGS_R3],  ->a1
	ldi [sp, PT_REGS_R4],  ->a2
	ldi [sp, PT_REGS_R5],  ->a3
	ldi [sp, PT_REGS_R6],  ->a4
	ldi [sp, PT_REGS_R7],  ->a5
	ldi [sp, PT_REGS_R8],  ->a6
	ldi [sp, PT_REGS_R9],  ->a7
	ldi [sp, PT_REGS_R10], ->ra
	ldi [sp, PT_REGS_R11], ->s0
	ldi [sp, PT_REGS_R12], ->s1
	ldi [sp, PT_REGS_R13], ->s2
	ldi [sp, PT_REGS_R14], ->s3
	ldi [sp, PT_REGS_R15], ->s4
	ldi [sp, PT_REGS_R16], ->s5
	ldi [sp, PT_REGS_R17], ->s6
	ldi [sp, PT_REGS_R18], ->s7
	ldi [sp, PT_REGS_R19], ->s8
	ldi [sp, PT_REGS_R20], ->x0
	ldi [sp, PT_REGS_R21], ->x1
	ldi [sp, PT_REGS_R22], ->x2
	ldi [sp, PT_REGS_R23], ->x3
	ldi [sp, PT_REGS_R1],  ->sp
	acre 0

