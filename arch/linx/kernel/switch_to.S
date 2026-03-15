/* SPDX-License-Identifier: GPL-2.0-only */

#include <asm/asm-offsets.h>

/*
 * LinxISA context switch.
 *
 * Save callee-saved state into prev->thread and restore from next->thread.
 * Update ETEMP0_ACR1 to the next task's kernel stack top so traps from ACR2
 * (userspace) can switch to the correct kernel stack in the trap vector.
 */

.set SSR_ETEMP0_ACR1, 0x1f06
.set SSR_EBARG0_ACR1, 0x1f40
.set SSR_EBARG_BPC_CUR_ACR1, 0x1f41
.set SSR_EBARG_BPC_TGT_ACR1, 0x1f42
.set SSR_EBARG_TPC_ACR1, 0x1f43
.set SSR_EBARG_LRA_ACR1, 0x1f44
.set SSR_EBARG_TQ0_ACR1, 0x1f45
.set SSR_EBARG_TQ1_ACR1, 0x1f46
.set SSR_EBARG_TQ2_ACR1, 0x1f47
.set SSR_EBARG_TQ3_ACR1, 0x1f48
.set SSR_EBARG_UQ0_ACR1, 0x1f49
.set SSR_EBARG_UQ1_ACR1, 0x1f4a
.set SSR_EBARG_UQ2_ACR1, 0x1f4b
.set SSR_EBARG_UQ3_ACR1, 0x1f4c
.set SSR_EBARG_LB_ACR1, 0x1f4d
.set SSR_EBARG_LC_ACR1, 0x1f4e
.set SSR_EBARG_EXT_PTR_ACR1, 0x1f4f
.set SSR_EBARG_EXT_META_ACR1, 0x1f50

	.text
	.globl __switch_to
	.type __switch_to,@function
__switch_to:
	/* a0=prev, a1=next; return prev in a0. */
	C.BSTART.STD RET

	/* Save callee-saved state into prev->thread. */
	sdi ra, [a0, TASK_THREAD_RA]
	sdi sp, [a0, TASK_THREAD_SP]
	sdi s0, [a0, TASK_THREAD_S0]
	sdi s1, [a0, TASK_THREAD_S1]
	sdi s2, [a0, TASK_THREAD_S2]
	sdi s3, [a0, TASK_THREAD_S3]
	sdi s4, [a0, TASK_THREAD_S4]
	sdi s5, [a0, TASK_THREAD_S5]
	sdi s6, [a0, TASK_THREAD_S6]
	sdi s7, [a0, TASK_THREAD_S7]
	sdi s8, [a0, TASK_THREAD_S8]
	hl.ssrget SSR_EBARG0_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG0]
	hl.ssrget SSR_EBARG_BPC_CUR_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_BPC_CUR]
	hl.ssrget SSR_EBARG_BPC_TGT_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_BPC_TGT]
	hl.ssrget SSR_EBARG_TPC_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_TPC]
	hl.ssrget SSR_EBARG_LRA_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_LRA]
	hl.ssrget SSR_EBARG_TQ0_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_TQ0]
	hl.ssrget SSR_EBARG_TQ1_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_TQ1]
	hl.ssrget SSR_EBARG_TQ2_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_TQ2]
	hl.ssrget SSR_EBARG_TQ3_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_TQ3]
	hl.ssrget SSR_EBARG_UQ0_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_UQ0]
	hl.ssrget SSR_EBARG_UQ1_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_UQ1]
	hl.ssrget SSR_EBARG_UQ2_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_UQ2]
	hl.ssrget SSR_EBARG_UQ3_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_UQ3]
	hl.ssrget SSR_EBARG_LB_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_LB]
	hl.ssrget SSR_EBARG_LC_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_LC]
	hl.ssrget SSR_EBARG_EXT_PTR_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_EXT_PTR]
	hl.ssrget SSR_EBARG_EXT_META_ACR1, ->x0
	sdi x0, [a0, TASK_THREAD_EBARG_EXT_META]

	/* ETEMP0_ACR1 = next->stack + THREAD_SIZE (kernel stack top). */
	ldi [a1, TASK_STACK], ->x0
	lui 32, ->x1              /* 0x20000 (THREAD_SIZE) */
	add x0, x1, ->x0
	hl.ssrset x0, SSR_ETEMP0_ACR1

	/* Restore callee-saved state from next->thread. */
	ldi [a1, TASK_THREAD_RA], ->ra
	ldi [a1, TASK_THREAD_SP], ->sp
	ldi [a1, TASK_THREAD_S0], ->s0
	ldi [a1, TASK_THREAD_S1], ->s1
	ldi [a1, TASK_THREAD_S2], ->s2
	ldi [a1, TASK_THREAD_S3], ->s3
	ldi [a1, TASK_THREAD_S4], ->s4
	ldi [a1, TASK_THREAD_S5], ->s5
	ldi [a1, TASK_THREAD_S6], ->s6
	ldi [a1, TASK_THREAD_S7], ->s7
	ldi [a1, TASK_THREAD_S8], ->s8
	ldi [a1, TASK_THREAD_EBARG0], ->x0
	hl.ssrset x0, SSR_EBARG0_ACR1
	ldi [a1, TASK_THREAD_EBARG_BPC_CUR], ->x0
	hl.ssrset x0, SSR_EBARG_BPC_CUR_ACR1
	ldi [a1, TASK_THREAD_EBARG_BPC_TGT], ->x0
	hl.ssrset x0, SSR_EBARG_BPC_TGT_ACR1
	ldi [a1, TASK_THREAD_EBARG_TPC], ->x0
	hl.ssrset x0, SSR_EBARG_TPC_ACR1
	ldi [a1, TASK_THREAD_EBARG_LRA], ->x0
	hl.ssrset x0, SSR_EBARG_LRA_ACR1
	ldi [a1, TASK_THREAD_EBARG_TQ0], ->x0
	hl.ssrset x0, SSR_EBARG_TQ0_ACR1
	ldi [a1, TASK_THREAD_EBARG_TQ1], ->x0
	hl.ssrset x0, SSR_EBARG_TQ1_ACR1
	ldi [a1, TASK_THREAD_EBARG_TQ2], ->x0
	hl.ssrset x0, SSR_EBARG_TQ2_ACR1
	ldi [a1, TASK_THREAD_EBARG_TQ3], ->x0
	hl.ssrset x0, SSR_EBARG_TQ3_ACR1
	ldi [a1, TASK_THREAD_EBARG_UQ0], ->x0
	hl.ssrset x0, SSR_EBARG_UQ0_ACR1
	ldi [a1, TASK_THREAD_EBARG_UQ1], ->x0
	hl.ssrset x0, SSR_EBARG_UQ1_ACR1
	ldi [a1, TASK_THREAD_EBARG_UQ2], ->x0
	hl.ssrset x0, SSR_EBARG_UQ2_ACR1
	ldi [a1, TASK_THREAD_EBARG_UQ3], ->x0
	hl.ssrset x0, SSR_EBARG_UQ3_ACR1
	ldi [a1, TASK_THREAD_EBARG_LB], ->x0
	hl.ssrset x0, SSR_EBARG_LB_ACR1
	ldi [a1, TASK_THREAD_EBARG_LC], ->x0
	hl.ssrset x0, SSR_EBARG_LC_ACR1
	ldi [a1, TASK_THREAD_EBARG_EXT_PTR], ->x0
	hl.ssrset x0, SSR_EBARG_EXT_PTR_ACR1
	ldi [a1, TASK_THREAD_EBARG_EXT_META], ->x0
	hl.ssrset x0, SSR_EBARG_EXT_META_ACR1

	/* Return to next context at restored RA. */
	c.setc.tgt ra
	C.BSTOP
