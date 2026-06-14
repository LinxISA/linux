/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 Huawei Technologies Co, Ltd.
 */

#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/asm.h>
#include <asm/ssr.h>
#include <asm/unistd.h>
#include <asm/thread_info.h>
#include <asm/asm-offsets.h>
#include <asm/errata_list.h>
#include <asm/block-def.h>
#include <asm/block_head.h>

.set resume_kernel, restore_all

/*
 * if (TASK_TI_FLAGS(SSR_TP) & flag) then BPC += branch
 */
.macro block_next_bnez_tif flag, branch
	BSTART.std cond, \branch
		ssrget SSR_TP, -> t
		ldi [t#1, TASK_TI_FLAGS], -> t	/* TASK_TI_FLAGS(SSR_TP) */
		andi t#1, \flag, -> t
		setc.ne t#1, zero
.endm

/*
 * if (gpr[rs] & ECAUSE_TRAPNUM_MASK) == imm, then BPC += branch
 */
.macro block_next_beqi_trapnum rs, imm, branch
	BSTART.std cond \branch
		andi \rs, ECAUSE_TRAPNUM_MASK, -> t
		addi zero, \imm, -> t
		setc.eq t#1, t#2
.endm

/*
 * mem[rd + offset2] = mem[rs + offset1]
 */
.macro load_and_store rs, offset1, rd, offset2
	ldi [\rs, \offset1], -> t
	sdi t#1, [\rd, \offset2]
.endm

.macro restore_user_gprs
	ldi [sp, PT_A0], -> a0
	ldi [sp, PT_A1], -> a1
	ldi [sp, PT_A2], -> a2
	ldi [sp, PT_A3], -> a3
	ldi [sp, PT_A4], -> a4
	ldi [sp, PT_A5], -> a5
	ldi [sp, PT_A6], -> a6
	ldi [sp, PT_A7], -> a7
	ldi [sp, PT_RA], -> ra
	ldi [sp, PT_S0], -> s0
	ldi [sp, PT_S1], -> s1
	ldi [sp, PT_S2], -> s2
	ldi [sp, PT_S3], -> s3
	ldi [sp, PT_S4], -> s4
	ldi [sp, PT_S5], -> s5
	ldi [sp, PT_S6], -> s6
	ldi [sp, PT_S7], -> s7
	ldi [sp, PT_S8], -> s8
	ldi [sp, PT_X0], -> x0
	ldi [sp, PT_X1], -> x1
	ldi [sp, PT_X2], -> x2
	ldi [sp, PT_X3], -> x3
.endm

ENTRY(handle_exception)
    /*
	 * Distinguish user-origin vs kernel-origin traps from the saved source
	 * ACR in SSR_ECSTATE. QEMU currently commits the conditional BSTART at
	 * the next block header, so the fallthrough path must be the user-origin
	 * case and the branch target must skip over it to the kernel-origin
	 * prologue.
	 */
    BSTART.std cond, from_kernel
	hl.ssrget SSR_ECSTATE, -> t
	andi t#1, CSTATE_ACR_MASK, -> t
	ssrget SSR_CSTATE, -> u
	andi u#1, CSTATE_ACR_MASK, -> u
	setc.eq t#1, u#1

from_user:
/*
 * current state:
 *	SSR_TP holds thread_info
 *	ETEMP holds the manager thread_info snapshot for user return
 *	x1 holds the interrupted user x1
 *	sp hold (user sp)
 */
	BSTART.std fall
		ssrget SSR_TP, -> t
		sdi sp, [t#1, TASK_TI_USER_SP]

	block_next_direct from_user_switch_stack
from_user_switch_stack:
	BSTART.std direct, _save_context
		ssrget SSR_TP, -> t
		ldi [t#1, TASK_TI_KERNEL_SP], -> sp
		ssrswap x1, SSR_A0_ETEMP, -> x1
		BSTOP

from_kernel:
/*
 * current state:
 *	x1 hold (0)
 *	tp hold (thread info)
 *	etemp hold (old x1)
 *	sp hold (kernel sp)
 */
    BSTART.sys fall
	ssrswap x1, SSR_TP, -> x1
	sdi sp, [x1, TASK_TI_KERNEL_SP]
	sdi sp, [x1, TASK_TI_USER_SP]
	block_next_direct _save_context

_save_context:
/*
 * current state:
 *	x1 hold (thread info)
 *	tp hold (0/tls)
 *	etemp hold (old x1)
 *	sp hold (kernel sp), old sp is saved
 */

	BSTART.sys fall
	ssrswap x1, SSR_TP, -> x1
	ssrswap x1, SSR_A0_ETEMP, -> x1

/*
 * current state:
 *	x1 hold(old x1)
 *	tp hold(ti)
 *	etemp hold(0/tls)
 */

	/* allocate pt_regs's space */
	subi sp, PT_SIZE_ON_STACK, -> sp

save_level2_state:
/* 只有ecstate.bi == 1才需要保存，现在先不管，都一把保存了 */
	BSTART.std fall
	sdi t#1, [sp, PT_ELPR0]
	sdi t#2, [sp, PT_ELPR1]
	sdi t#3, [sp, PT_ELPR2]
	sdi t#4, [sp, PT_ELPR3]
	sdi u#1, [sp, PT_ELPR4]
	sdi u#2, [sp, PT_ELPR5]
	sdi u#3, [sp, PT_ELPR6]
	sdi u#4, [sp, PT_ELPR7]

save_level1_state:
	/* directly copy from registers to pt_regs, only for necessary UL_GPRs */
	BSTART.sys fall
	ssrget SSR_TP, -> t
	ldi [t#1, TASK_TI_USER_SP], -> t
	sdi t#1, [sp, PT_SP]	/* PT_SP(sp) = user sp */
	sdi a0, [sp, PT_A0]
	sdi a1, [sp, PT_A1]
	sdi a2, [sp, PT_A2]
	sdi a3, [sp, PT_A3]
	sdi a4, [sp, PT_A4]
	sdi a5, [sp, PT_A5]
	sdi a6, [sp, PT_A6]
	sdi a7, [sp, PT_A7]
	sdi ra, [sp, PT_RA]
	sdi s0, [sp, PT_S0]
	sdi s1, [sp, PT_S1]
	sdi s2, [sp, PT_S2]
	sdi s3, [sp, PT_S3]
	sdi s4, [sp, PT_S4]
	sdi s5, [sp, PT_S5]
	sdi s6, [sp, PT_S6]
	sdi s7, [sp, PT_S7]
	sdi s8, [sp, PT_S8]
	sdi x0, [sp, PT_X0]
	sdi x1, [sp, PT_X1]
	sdi x2, [sp, PT_X2]
	sdi x3, [sp, PT_X3]

	/* save BARGs and etpc */
	hl.ssrget SSR_EBPC, -> t
	sdi t#1, [sp, PT_BPC]
	hl.ssrget SSR_EBPCN, -> t
	sdi t#1, [sp, PT_BPCN]
	hl.ssrget SSR_EBARG, -> t
	sdi t#1, [sp, PT_EBARG]
	hl.ssrget SSR_ETPC, -> t
	sdi t#1, [sp, PT_TPC]


after_save_bstate:
	BSTART.sys fall
	ssrget SSR_GP, -> t
	sdi t#1, [sp, PT_GP]	/* PT_GP(sp) = gp */

	/*
	 * Disable user-mode memory access as it should only be set in the
	 * actual user copy routines.
	 *
	 * Disable the FPU to detect illegal usage of floating point in kernel
	 * space.
	 */

	BSTART.std fall
	addi zero, CSTATE_P, -> t
	ssrget SSR_CSTATE, -> t
	and t#1, t#2.not -> t
	ssrset t#1, SSR_CSTATE

	hl.ssrget SSR_ECSTATE, -> s1
	hl.ssrget SSR_TRAPARG0, -> s3	/* indicate different block trap when instruction permission exception */
	hl.ssrget SSR_TRAPNO, -> s4
	sdi s1, [sp, PT_CSTATE]
	sdi s3, [sp, PT_TRAPARG0]
	sdi s4, [sp, PT_TRAPNO]

/* construct PT_TP(sp) */
 /*
  * current state:
  *	tp holds thread_info
  *	etemp holds user tls for user-origin traps, but interrupted x1 for
  *	kernel-origin traps.  Use the saved source ACR to decide which value
  *	must be restored into SSR_TP.
  */
	BSTART.std cond, PT_TP_FROM_KERNEL
	andi s1, CSTATE_ACR_MASK, -> t
	addi zero, CSTATE_ACR2, -> u
	setc.ne t#1, u#1

PT_TP_FROM_USER:
	BSTART.sys fall
	hl.ssrget SSR_ETEMP, -> s5
	block_next_direct PT_TP_READY

PT_TP_FROM_KERNEL:
	BSTART.std fall
	ssrget SSR_TP, -> s5

PT_TP_READY:
 /*
  * s5 holds thread_info for kernel-origin traps or user tls for user-origin
  * traps, matching the value that restore_all must put back into SSR_TP.
  */
	BSTART.sys fall
	sdi s5, [sp, PT_TP] /* pt_regs(tp) keeps the old tp, which can be restored directly */
	/*
	 * Set the ETEMP register to 0, so that if a recursive exception
	 * occurs, the exception vector knows it came from the kernel
	 */
	hl.ssrset zero, SSR_ETEMP // it's already zero

	/* Load the global pointer */
	addtpc %tpcrel_hi(__global_pointer$), -> t
	addi t#1, %tpcrel_lo(__global_pointer$), -> t
	ssrset t#1, SSR_GP

	/*
	 * MSB (SSR_TRAPNO.E) of SSR_TRAPNO
	 * differentiates between interrupts and exceptions:
	 * SSR_TRAPNO.E == 0: Interrupt
	 * SSR_TRAPNO.E == 1: Exception
	 */
	block_next_bltz s4, handle_exceptions

handle_interrupt:
	BSTART.std fall
	addtpc %tpcrel_hi(ret_from_exception), -> t
	addi t#1, %tpcrel_lo(ret_from_exception), -> ra

	/* Handle interrupts, return to ret_from_exception */
	BSTART.std ind
	addi sp, 0 -> a0	/* pt_regs as the 1th argument */
	addtpc %tpcrel_hi(generic_handle_arch_irq), -> t
	addi t#1, %tpcrel_lo(generic_handle_arch_irq), -> t
	setc.tgt t#1

handle_exceptions:
	/*
	 * Exceptions run with interrupts enabled or disabled depending on the
	 * state of SSR_ECSTATE.I.
	 */
	BSTART.std cond, handle_syscalls
	andi s1, CSTATE_I, -> t
	setc.eqi t#1, 0

	/* kprobes, entered via ebreak, must have interrupts disabled. */
	block_next_beqi_trapnum s4, ECAUSE_TRAPNUM_BREAKPOINT_EXP, handle_syscalls

	/* Enable interrupts */
	BSTART.std fall
	ssrget SSR_CSTATE, -> t
	ori t#1, CSTATE_I, -> t
	ssrset t#1, SSR_CSTATE

handle_syscalls:
	/* set return address for both branches */
	BSTART.std fall
	addtpc %tpcrel_hi(ret_from_exception), -> t
	addi t#1, %tpcrel_lo(ret_from_exception), -> ra

	/* Handle syscalls, return to ret_from_exception */
	block_next_beqi_trapnum s4, ECAUSE_TRAPNUM_SCALL_EXP, handle_syscall

	/* Handle other exceptions, return to ret_from_exception */
	/* if (&excp_vect_table[SSR_ECAUSE.trapnum] >= excp_vect_table_end) {
	 * 	goto handle_unknown_exceptions;
	 * } else {
	 * 	call excp_vect_table[SSR_ECAUSE.trapnum];
	 * }
	 */
	BSTART.std cond, handle_unknown_exceptions
	addi sp, 0, -> a0
	andi s4, ECAUSE_TRAPNUM_MASK, -> t	/* SSR_TRAPNO.trapnum */
	slli t#1, RISCV_LGPTR, -> t
	addtpc %tpcrel_hi(excp_vect_table), -> t
	addi t#1, %tpcrel_lo(excp_vect_table), -> t
	add t#3, t#1, -> x0	/* &excp_vect_table[SSR_TRAPNO.trapnum] */
	addtpc %tpcrel_hi(excp_vect_table_end), -> t
	addi t#1, %tpcrel_lo(excp_vect_table_end), -> t
	/* Check if exception code lies within bounds */
	setc.geu x0, t#1

	/* jump to excp_vect_table[SSR_TRAPNO.trapnum] */
	/* return to ret_from_exception */
	BSTART.std ind
	ldi [x0, 0], -> t
	setc.tgt t#1

handle_unknown_exceptions:
	/* return to ret_from_exception */
	block_next_ind do_trap_unknown

handle_syscall:
	/*
	 * Save the initial A0 value (needed in signal handlers).
	 * Advance tpc to avoid executing the original
	 * syscall instruction on acri
	 */
	BSTART.std fall
	sdi a0, [sp, PT_ORIG_A0]
	/* preserve ebarg.ebpc and ebarg.etpc */
	ldi [sp, PT_TPC], -> t
	sdi t#1, [sp, PT_ORIG_TPC]
	ldi [sp, PT_BPC], -> t
	sdi t#1, [sp, PT_ORIG_BPC]
	/* advance bpc and tpc with length of `acrc`, because it always ends a block */
	addi t#2, 4, -> t
	sdi t#1, [sp, PT_TPC]
	sdi t#1, [sp, PT_BPC]

	/* Trace syscalls, but only if requested by the user. */
	/*
	 * if (TASK_TI_FLAGS(SSR_TP) & _TIF_SYSCALL_WORK)
	 * 	goto handle_syscall_trace_enter;
	 */
	block_next_bnez_tif _TIF_SYSCALL_WORK, handle_syscall_trace_enter

check_syscall_nr:
	/*
	 * Check to make sure we don't jump to a bogus syscall number.
	 * Syscall number held in a7.
	 * If syscall number is above allowed value, redirect to ni_syscall.
	 */
	/*
	 * if (a7 >= __NR_syscalls) {
	 * 	s0 = sys_ni_syscall;
	 * } else {
	 * 	s0 = sys_call_table[a7];
	 * }
	 */
	BSTART.std cond, call_syscall
	addtpc %tpcrel_hi(sys_ni_syscall), -> t
	addi t#1, %tpcrel_lo(sys_ni_syscall), -> s0
	addi zero, __NR_syscalls, -> t
	setc.geu, a7, t#1

	BSTART.std fall
	addtpc %tpcrel_hi(sys_call_table), -> t
	addi t#1, %tpcrel_lo(sys_call_table), -> t
	ld [t#1, a7<<RISCV_LGPTR], -> s0	/* s0 = sys_call_table[a7] */

call_syscall:
	/* Call syscall */
	block_next_indcall_reg s0

ret_from_syscall:
	/* Set user a0 to kernel a0 */
	BSTART.std fall
	sdi a0, [sp, PT_A0]	/* save the syscall return value */

	/*
	 * We didn't execute the actual syscall.
	 * Seccomp already set return value for the current task pt_regs.
	 * (If it was configured with SECCOMP_RET_ERRNO/TRACE)
	 */
ret_from_syscall_rejected:
	/*
	 * if (TASK_TI_FLAGS(SSR_TP) & _TIF_SYSCALL_WORK)
	 * 	goto handle_syscall_trace_exit;
	 */
	block_next_bnez_tif _TIF_SYSCALL_WORK, handle_syscall_trace_exit

ret_from_exception:
#ifdef CONFIG_LINX_INTC
	BSTART.std fall
	addi sp, 0, -> a0
	block_next_indcall linx_trace_ret_from_exception
#endif
	/* clear SSR_CSTATE.I */
	/* disable interrupt */
	BSTART.sys fall
	ssrget SSR_CSTATE, -> t
	addi, zero, CSTATE_I, -> t
	and t#2, t#1.not, -> t
	ssrset t#1, SSR_CSTATE

	/* if (SSR_ECSTATE.ACR == SSR_CSTATE.ACR) goto resume_kernel; */
	BSTART.std cond resume_kernel
	ldi [sp, PT_CSTATE], -> t	/* SSR_ECSTATE */
	andi t#1, CSTATE_ACR_MASK, -> t
	ssrget SSR_CSTATE, -> t
	andi t#1, CSTATE_ACR_MASK, -> t
	setc.eq t#1, t#3

resume_userspace:
	/* Interrupts must be disabled here so flags are checked atomically */
	BSTART.std cond, work_pending
	ssrget SSR_TP, -> t
	ldi [t#1, TASK_TI_FLAGS], -> s0	/* s0 = TASK_TI_FLAGS(SSR_TP) */
	andi s0, _TIF_WORK_MASK, -> s1	/* s1 = TASK_TI_FLAGS(SSR_TP) & _TIF_WORK_MASK */
	setc.nei s1, 0

	/*
	 * Save unwound kernel stack pointer in thread_info.
	 * Save SSR_TP into the SSR_ETEMP register, so we can find the kernel data
	 * structures again.
	 */
	/* restore ETEMP and thread_info(sp) */
	BSTART.sys fall
	ssrget SSR_TP, -> t
	hl.ssrset t#1, SSR_ETEMP
	addi sp, PT_SIZE_ON_STACK, -> t
	sdi t#1, [t#2, TASK_TI_KERNEL_SP]	/* TASK_TI_KERNEL_SP(SSR_TP) = sp + PT_SIZE_ON_STACK */

restore_all:
	/*
	 * The current load reservation is effectively part of the processor's
	 * state, in the sense that load reservations cannot be shared between
	 * different hart contexts.  We can't actually save and restore a load
	 * reservation, so instead here we clear any existing reservation --
	 * it's always legal for implementations to clear load reservations at
	 * any point (as long as the forward progress guarantee is kept, but
	 * we'll ignore that here).
	 *
	 * Dangling load reservations can be the result of taking a trap in the
	 * middle of an LR/SC sequence, but can also be the result of a taken
	 * forward branch around an SC -- which is how we implement CAS.  As a
	 * result we need to clear reservations between the last CAS and the
	 * jump back to the new context.  While it is unlikely the store
	 * completes, implementations are allowed to expand reservations to be
	 * arbitrarily large.
	 */
	/* clear load reservations */
	BSTART.sys fall
	addi sp, PT_BPC, -> t
	ldi [t#1, 0], -> t	/* sp holds &pt_regs */
	/* Bring-up lane: skip explicit LR/SC reservation clear until SC parsing
	 * support is fully propagated through the production compiler build.
	 */
	addi zero, 0, -> t

	/* restore register except ebstate */
	/* restore gp, tp, ecstate */
	ldi [sp, PT_GP], -> t
	ssrset t#1, SSR_GP
	ldi [sp, PT_TP], -> t
	ssrset t#1, SSR_TP
	ldi [sp, PT_CSTATE], -> t
	hl.ssrset t#1, SSR_ECSTATE

restore_level1_state:
	BSTART.sys fall

	/* User GPRs are restored in the final ACRE block. */

	/* restore BARGs */
	ldi [sp, PT_TPC], -> t
	hl.ssrset t#1, SSR_ETPC
	ldi [sp, PT_BPC], -> t
	hl.ssrset t#1, SSR_EBPC
	ldi [sp, PT_BPCN], -> t
	hl.ssrset t#1, SSR_EBPCN
	/*
	 * Do not preload user EBARG before the kernel's own return blocks.
	 * RRAT_RESTORE restores the saved target block state in QEMU; pushing
	 * user EBARG/ELPR through kernel BSTART boundaries corrupts live sp.
	 */

	/* check ebstate status */
	/* if ecstate.bi == 0, don't need restore level2 */
	BSTART.std cond, restore_level2_state
	hl.ssrget SSR_ECSTATE, -> t
	srli t#1, ECAUSE_BI_SHIFT, -> t
	andi t#1, 1, -> t
	setc.nei t#1, 0

/* restored block-head exception */
	BSTART.sys, fall
	block_next_direct restore_default_acre_enter
restore_default_acre_enter:
	BSTART.sys fall
	restore_user_gprs
	ldi [sp, PT_SP], -> sp
	acre RRAT_DEFAULT


restore_level2_state:
	BSTART.sys fall
	block_next_direct restore_level2_acre_enter
restore_level2_acre_enter:
	BSTART.sys fall
	restore_user_gprs
	ldi [sp, PT_SP], -> sp
	acre RRAT_RESTORE

work_pending:
	/* Enter slow path for supplementary processing */
	/* if (s1 = (TASK_TI_FLAGS(SSR_TP) & _TIF_NEED_RESCHED))
	 * 	goto work_resched;
	 */
	BSTART.std cond, work_resched
	addtpc %tpcrel_hi(ret_from_exception), -> t
	addi t#1, %tpcrel_lo(ret_from_exception), -> ra
	andi s0, _TIF_NEED_RESCHED, -> s1	/* s1 = TASK_TI_FLAGS(SSR_TP) & _TIF_NEED_RESCHED */
	setc.nei s1, 0

work_notifysig:
	/* Handle pending signals and notify-resume requests */

	/* Enable interrupts for do_notify_resume(): SSR_CSTATE |= CSTATE_I */
	BSTART.std fall
	ssrget SSR_CSTATE, -> t
	ori t#1, CSTATE_I, -> t
	ssrset t#1, SSR_CSTATE

	/* tail call do_notify_resume, and return to ret_from_exception */
	BSTART.std ind
	/* prepare arguments for do_notify_resume
	 * 1: pointer to pt_regs
	 * 2: thread_info_flags
	 */
	addi sp, 0, -> a0
	addi s0, 0, -> a1
	addtpc %tpcrel_hi(do_notify_resume), -> t
	addi t#1, %tpcrel_lo(do_notify_resume), -> t
	setc.tgt t#1

work_resched:
	/* tall call schedule, and return to ret_from_exception */
	block_next_ind schedule

/* before real syscall execution. Slow paths for ptrace. */
handle_syscall_trace_enter:
	BSTART.std fall
	addi sp, 0, -> a0

	block_next_indcall do_syscall_trace_enter

	/* restore syscall argument registers */
	BSTART.std cond, ret_from_syscall_rejected
	setc.nei a0, 0
	ldi [sp, PT_A0], -> a0
	ldi [sp, PT_A1], -> a1
	ldi [sp, PT_A2], -> a2
	ldi [sp, PT_A3], -> a3
	ldi [sp, PT_A4], -> a4
	ldi [sp, PT_A5], -> a5
	ldi [sp, PT_X1], -> x1

	block_next_ind check_syscall_nr

handle_syscall_trace_exit:
	BSTART.std fall
	addi sp, 0, -> a0

	block_next_indcall do_syscall_trace_exit

	block_next_ind ret_from_exception

END(handle_exception)

/* used in copy_thread */
ENTRY(ret_from_fork)
	BSTART.std ind
	addtpc %tpcrel_hi(ret_from_exception), -> t
	addi t#1, %tpcrel_lo(ret_from_exception), -> ra
	addtpc %tpcrel_hi(schedule_tail), -> t
	addi t#1, %tpcrel_lo(schedule_tail), -> t
	setc.tgt t#1
ENDPROC(ret_from_fork)

ENTRY(ret_from_kernel_thread)
	block_next_indcall schedule_tail
	BSTART.std ind
	addtpc %tpcrel_hi(ret_from_exception), -> t
	addi t#1, %tpcrel_lo(ret_from_exception), -> ra
	/* Call s0(s1) */
	addi s1, 0, -> a0
	setc.tgt s0
ENDPROC(ret_from_kernel_thread)


/*
 * Integer register context switch
 * The callee-saved registers must be saved and restored.
 *
 *   a0: previous task_struct (must be preserved across the switch)
 *   a1: next task_struct
 *
 * The value of a0 and a1 must be preserved by this function, as that's how
 * arguments are passed to schedule_tail.
 */
ENTRY(__switch_to)
	BSTART.std fall
	addi a0, TASK_THREAD_RA, -> t
	addi a1, TASK_THREAD_RA, -> t

	/* Save context into prev->thread */
	/* save the callee-saved registers */
	sdi ra, [t#2, TASK_THREAD_RA_RA]
	sdi sp, [t#2, TASK_THREAD_SP_RA]
	sdi s0, [t#2, TASK_THREAD_S0_RA]
	sdi s1, [t#2, TASK_THREAD_S1_RA]
	sdi s2, [t#2, TASK_THREAD_S2_RA]
	sdi s3, [t#2, TASK_THREAD_S3_RA]
	sdi s4, [t#2, TASK_THREAD_S4_RA]
	sdi s5, [t#2, TASK_THREAD_S5_RA]
	sdi s6, [t#2, TASK_THREAD_S6_RA]
	sdi s7, [t#2, TASK_THREAD_S7_RA]
	sdi s8, [t#2, TASK_THREAD_S8_RA]

	/* Restore context from next->thread */
	ldi [t#1, TASK_THREAD_RA_RA], -> ra
	ldi [t#1, TASK_THREAD_SP_RA], -> sp
	ldi [t#1, TASK_THREAD_S0_RA], -> s0
	ldi [t#1, TASK_THREAD_S1_RA], -> s1
	ldi [t#1, TASK_THREAD_S2_RA], -> s2
	ldi [t#1, TASK_THREAD_S3_RA], -> s3
	ldi [t#1, TASK_THREAD_S4_RA], -> s4
	ldi [t#1, TASK_THREAD_S5_RA], -> s5
	ldi [t#1, TASK_THREAD_S6_RA], -> s6
	ldi [t#1, TASK_THREAD_S7_RA], -> s7
	ldi [t#1, TASK_THREAD_S8_RA], -> s8

	/* The offset of thread_info in task_struct is zero. */
	ssrset a1, SSR_TP

	block_next_ret

ENDPROC(__switch_to)

	.section ".rodata"
	.p2align LGREG
	/* Exception vector table */
ENTRY(excp_vect_table)
	RISCV_PTR do_trap_insn_exception
	RISCV_PTR do_trap_data_exception
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_block_exception
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_unknown	/* not support yet */
	RISCV_PTR do_trap_break
excp_vect_table_end:
END(excp_vect_table)
