/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PROCESSOR_H
#define _ASM_RISCV_PROCESSOR_H

#include <linux/const.h>

#include <vdso/processor.h>

#include <asm/ptrace.h>

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#ifdef CONFIG_LINX_INTC
#define DEFAULT_MAP_WINDOW	TASK_SIZE_MIN
#else
#define DEFAULT_MAP_WINDOW	TASK_SIZE
#endif

#define arch_get_mmap_end(addr, len, flags)			\
({								\
	STACK_TOP_MAX;						\
})

#define arch_get_mmap_base(addr, base)				\
({								\
	base;							\
})

#define TASK_UNMAPPED_BASE	PAGE_ALIGN(DEFAULT_MAP_WINDOW / 3)

#define STACK_TOP		DEFAULT_MAP_WINDOW
#define STACK_TOP_MAX		TASK_SIZE
#define STACK_ALIGN		16

#ifndef __ASSEMBLY__

struct task_struct;
struct pt_regs;

/* CPU-specific state of a task */
struct thread_struct {
	/* Callee-saved registers */
	unsigned long ra;
	unsigned long sp;	/* Kernel mode stack */
	unsigned long s[9];	/* s[0]: frame pointer */
	struct __riscv_d_ext_state fstate;
	unsigned long bad_cause;
};

/* Whitelist the fstate from the task_struct for hardened usercopy */
static inline void arch_thread_struct_whitelist(unsigned long *offset,
						unsigned long *size)
{
	*offset = offsetof(struct thread_struct, fstate);
	*size = sizeof_field(struct thread_struct, fstate);
}

#define INIT_THREAD {					\
	.sp = sizeof(init_stack) + (long)&init_stack,	\
}

#define task_pt_regs(tsk)						\
	((struct pt_regs *)(task_stack_page(tsk) + THREAD_SIZE		\
			    - ALIGN(sizeof(struct pt_regs), STACK_ALIGN)))

#define KSTK_EIP(tsk)		(task_pt_regs(tsk)->tpc)
#define KSTK_ESP(tsk)		(task_pt_regs(tsk)->sp)


/* Do necessary setup to start up a newly executed thread. */
extern void start_thread(struct pt_regs *regs,
			unsigned long pc, unsigned long sp);

/* Free all resources held by a thread. */
extern void release_thread(struct task_struct *dead_task);

extern unsigned long __get_wchan(struct task_struct *p);

#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */
static inline void wait_for_interrupt(void)
{
	__asm__ __volatile__ ("wfi");
}

#else
static inline void wait_for_interrupt(void)
{
	return;
	__asm__ __volatile__(
		"BSTART.sys fall\n" \
			"bwi zero\n"
		);
}
#endif

struct device_node;
int riscv_of_processor_hartid(struct device_node *node);
int riscv_of_parent_hartid(struct device_node *node);

extern void riscv_fill_hwcap(void);
extern int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src);

#endif /* __ASSEMBLY__ */

#endif /* _ASM_RISCV_PROCESSOR_H */
