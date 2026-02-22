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

	/* Return to next context at restored RA. */
	c.setc.tgt ra
	C.BSTOP

