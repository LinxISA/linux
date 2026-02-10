// SPDX-License-Identifier: GPL-2.0-only
#define COMPILE_OFFSETS

#include <linux/kbuild.h>
#include <linux/sched.h>

#include <asm/processor.h>
#include <asm/ptrace.h>

void asm_offsets(void);

void asm_offsets(void)
{
	/* struct pt_regs */
	DEFINE(PT_REGS_SIZE, sizeof(struct pt_regs));
	DEFINE(PT_REGS_R0, offsetof(struct pt_regs, regs[PTR_R0]));
	DEFINE(PT_REGS_R1, offsetof(struct pt_regs, regs[PTR_R1]));
	DEFINE(PT_REGS_R2, offsetof(struct pt_regs, regs[PTR_R2]));
	DEFINE(PT_REGS_R3, offsetof(struct pt_regs, regs[PTR_R3]));
	DEFINE(PT_REGS_R4, offsetof(struct pt_regs, regs[PTR_R4]));
	DEFINE(PT_REGS_R5, offsetof(struct pt_regs, regs[PTR_R5]));
	DEFINE(PT_REGS_R6, offsetof(struct pt_regs, regs[PTR_R6]));
	DEFINE(PT_REGS_R7, offsetof(struct pt_regs, regs[PTR_R7]));
	DEFINE(PT_REGS_R8, offsetof(struct pt_regs, regs[PTR_R8]));
	DEFINE(PT_REGS_R9, offsetof(struct pt_regs, regs[PTR_R9]));
	DEFINE(PT_REGS_R10, offsetof(struct pt_regs, regs[PTR_R10]));
	DEFINE(PT_REGS_R11, offsetof(struct pt_regs, regs[PTR_R11]));
	DEFINE(PT_REGS_R12, offsetof(struct pt_regs, regs[PTR_R12]));
	DEFINE(PT_REGS_R13, offsetof(struct pt_regs, regs[PTR_R13]));
	DEFINE(PT_REGS_R14, offsetof(struct pt_regs, regs[PTR_R14]));
	DEFINE(PT_REGS_R15, offsetof(struct pt_regs, regs[PTR_R15]));
	DEFINE(PT_REGS_R16, offsetof(struct pt_regs, regs[PTR_R16]));
	DEFINE(PT_REGS_R17, offsetof(struct pt_regs, regs[PTR_R17]));
	DEFINE(PT_REGS_R18, offsetof(struct pt_regs, regs[PTR_R18]));
	DEFINE(PT_REGS_R19, offsetof(struct pt_regs, regs[PTR_R19]));
	DEFINE(PT_REGS_R20, offsetof(struct pt_regs, regs[PTR_R20]));
	DEFINE(PT_REGS_R21, offsetof(struct pt_regs, regs[PTR_R21]));
	DEFINE(PT_REGS_R22, offsetof(struct pt_regs, regs[PTR_R22]));
	DEFINE(PT_REGS_R23, offsetof(struct pt_regs, regs[PTR_R23]));
	DEFINE(PT_REGS_PC, offsetof(struct pt_regs, regs[PTR_PC]));
	DEFINE(PT_REGS_ORIG_A0, offsetof(struct pt_regs, orig_a0));
	DEFINE(PT_REGS_ECSTATE, offsetof(struct pt_regs, ecstate));
	DEFINE(PT_REGS_TRAPNO, offsetof(struct pt_regs, trapno));
	DEFINE(PT_REGS_TRAPARG0, offsetof(struct pt_regs, traparg0));
	DEFINE(PT_REGS_EBARG0, offsetof(struct pt_regs, ebarg0));
	DEFINE(PT_REGS_EBARG_BPC_CUR, offsetof(struct pt_regs, ebarg_bpc_cur));
	DEFINE(PT_REGS_EBARG_BPC_TGT, offsetof(struct pt_regs, ebarg_bpc_tgt));
	DEFINE(PT_REGS_EBARG_TPC, offsetof(struct pt_regs, ebarg_tpc));
	DEFINE(PT_REGS_EBARG_LRA, offsetof(struct pt_regs, ebarg_lra));
	DEFINE(PT_REGS_EBARG_LB, offsetof(struct pt_regs, ebarg_lb));
	DEFINE(PT_REGS_EBARG_LC, offsetof(struct pt_regs, ebarg_lc));

	DEFINE(TASK_THREAD_RA, offsetof(struct task_struct, thread.ra));
	DEFINE(TASK_THREAD_SP, offsetof(struct task_struct, thread.sp));
	DEFINE(TASK_THREAD_S0, offsetof(struct task_struct, thread.s[0]));
	DEFINE(TASK_THREAD_S1, offsetof(struct task_struct, thread.s[1]));
	DEFINE(TASK_THREAD_S2, offsetof(struct task_struct, thread.s[2]));
	DEFINE(TASK_THREAD_S3, offsetof(struct task_struct, thread.s[3]));
	DEFINE(TASK_THREAD_S4, offsetof(struct task_struct, thread.s[4]));
	DEFINE(TASK_THREAD_S5, offsetof(struct task_struct, thread.s[5]));
	DEFINE(TASK_THREAD_S6, offsetof(struct task_struct, thread.s[6]));
	DEFINE(TASK_THREAD_S7, offsetof(struct task_struct, thread.s[7]));
	DEFINE(TASK_THREAD_S8, offsetof(struct task_struct, thread.s[8]));
	DEFINE(TASK_THREAD_KTHREAD_FN, offsetof(struct task_struct, thread.kthread_fn));
	DEFINE(TASK_THREAD_KTHREAD_ARG, offsetof(struct task_struct, thread.kthread_arg));

	/* Kernel stack base (thread_info*) for per-task ETEMP0 setup. */
	DEFINE(TASK_STACK, offsetof(struct task_struct, stack));
	DEFINE(THREAD_SIZE, THREAD_SIZE);
}
