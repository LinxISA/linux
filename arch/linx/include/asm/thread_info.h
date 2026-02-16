/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ASM_LINX_THREAD_INFO_H
#define _ASM_LINX_THREAD_INFO_H

#include <asm/page.h>
#include <linux/compiler.h>

#define THREAD_SHIFT 17
#define THREAD_SIZE (_AC(1, UL) << THREAD_SHIFT)
#define THREAD_SIZE_ORDER (THREAD_SHIFT - PAGE_SHIFT)

#ifndef __ASSEMBLER__

struct task_struct;

struct thread_info {
	struct task_struct *task;
	unsigned long flags;
	int preempt_count;
	int cpu;
};

#define INIT_THREAD_INFO(tsk)			\
{						\
	.task		= &tsk,			\
	.flags		= 0,			\
	.preempt_count	= INIT_PREEMPT_COUNT,	\
	.cpu		= 0,			\
}

static __always_inline struct thread_info *current_thread_info(void)
{
	unsigned long sp = (unsigned long)&sp;

	return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}

#endif /* !__ASSEMBLER__ */

#include <asm-generic/thread_info_tif.h>

/*
 * Arch-specific thread flags (bits 16..31).
 *
 * These are used on architectures that do not opt into CONFIG_GENERIC_ENTRY
 * and therefore use TIF_* flags for syscall tracing/auditing work.
 */
#define TIF_SYSCALL_TRACEPOINT	16
#define TIF_SYSCALL_TRACE	17
#define TIF_SYSCALL_AUDIT	18
#define TIF_SYSCALL_USER_DISPATCH 19

#define _TIF_SYSCALL_TRACEPOINT	(1UL << TIF_SYSCALL_TRACEPOINT)
#define _TIF_SYSCALL_TRACE	(1UL << TIF_SYSCALL_TRACE)
#define _TIF_SYSCALL_AUDIT	(1UL << TIF_SYSCALL_AUDIT)
#define _TIF_SYSCALL_USER_DISPATCH (1UL << TIF_SYSCALL_USER_DISPATCH)

#endif /* _ASM_LINX_THREAD_INFO_H */
