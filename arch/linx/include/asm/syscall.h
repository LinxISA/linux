/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * LinxISA syscall register accessors.
 *
 * Bring-up convention (LP64):
 * - syscall nr: a7 (R9)
 * - args 0..5: a0..a5 (R2..R7)
 * - return value / errno: a0 (R2), negative errno on error
 */
#ifndef _ASM_LINX_SYSCALL_H
#define _ASM_LINX_SYSCALL_H

#include <linux/err.h>
#include <linux/sched.h>

#include <asm/ptrace.h>

static inline int syscall_get_nr(struct task_struct *task, struct pt_regs *regs)
{
	(void)task;
	return (int)regs->regs[PTR_R9];
}

static inline void syscall_set_nr(struct task_struct *task,
				  struct pt_regs *regs, int nr)
{
	(void)task;
	regs->regs[PTR_R9] = (unsigned long)(unsigned int)nr;
}

static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
	(void)task;
	(void)regs;
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	unsigned long error = regs->regs[PTR_R2];

	(void)task;
	return IS_ERR_VALUE(error) ? (long)error : 0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	(void)task;
	return (long)regs->regs[PTR_R2];
}

static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs, int error,
					    long val)
{
	(void)task;
	regs->regs[PTR_R2] = error ? (unsigned long)(long)error : (unsigned long)val;
}

static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned long *args)
{
	(void)task;
	args[0] = regs->regs[PTR_R2];
	args[1] = regs->regs[PTR_R3];
	args[2] = regs->regs[PTR_R4];
	args[3] = regs->regs[PTR_R5];
	args[4] = regs->regs[PTR_R6];
	args[5] = regs->regs[PTR_R7];
}

static inline void syscall_set_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 const unsigned long *args)
{
	(void)task;
	regs->regs[PTR_R2] = args[0];
	regs->regs[PTR_R3] = args[1];
	regs->regs[PTR_R4] = args[2];
	regs->regs[PTR_R5] = args[3];
	regs->regs[PTR_R6] = args[4];
	regs->regs[PTR_R7] = args[5];
}

static inline int syscall_get_arch(struct task_struct *task)
{
	(void)task;
	return 0;
}

#endif /* _ASM_LINX_SYSCALL_H */
