/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PROCESSOR_H
#define _ASM_LINX_PROCESSOR_H

#include <linux/const.h>

#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/ptrace.h>
#include <asm/ssr.h>

#define STACK_ALIGN 16

#ifdef CONFIG_MMU
/*
 * v0.1 bring-up profile: 48-bit canonical VA, user space in the low half
 * (VA[47]=0). Keep kernel mappings at low addresses for now, but place user
 * mappings and stacks far from the kernel image by using the default mmap base
 * near TASK_SIZE/3.
 */
#define TASK_SIZE	(UL(1) << 47)
#define TASK_UNMAPPED_BASE	PAGE_ALIGN(TASK_SIZE / 3)
#else
/*
 * NOMMU: user mappings are allocated from available RAM and returned as
 * addresses in the flat address space. validate_mmap_request() rejects any
 * mapping with len > TASK_SIZE, so TASK_SIZE must be non-zero.
 */
#define TASK_SIZE	(-PAGE_SIZE)
#endif

#define STACK_TOP	TASK_SIZE
#define STACK_TOP_MAX	STACK_TOP

#ifndef __ASSEMBLER__

struct task_struct;
struct pt_regs;

struct thread_struct {
	unsigned long ra;
	unsigned long sp;
	unsigned long s[9]; /* s0..s8 */
	unsigned long kthread_fn;
	unsigned long kthread_arg;
};

#define INIT_THREAD {			\
	.ra = 0,			\
	.sp = 0,			\
	.s = { 0 },			\
	.kthread_fn = 0,		\
	.kthread_arg = 0,		\
}

#define task_pt_regs(p) \
	((struct pt_regs *)((unsigned long)task_stack_page(p) + THREAD_SIZE) - 1)

#define KSTK_EIP(tsk)	(task_pt_regs(tsk)->regs[PTR_PC])
#define KSTK_ESP(tsk)	(task_pt_regs(tsk)->regs[PTR_R1])

static inline void start_thread(struct pt_regs *regs, unsigned long pc,
				unsigned long sp)
{
	unsigned long ecstate;
#ifdef CONFIG_BINFMT_ELF_FDPIC
	unsigned long fdpic_exec_map = regs->regs[PTR_R3];
	unsigned long fdpic_interp_map = regs->regs[PTR_R4];
	unsigned long fdpic_dyn = regs->regs[PTR_R5];
#endif

	memset(regs, 0, sizeof(*regs));
	regs->regs[PTR_PC] = pc;
	regs->regs[PTR_R1] = sp;

#ifdef CONFIG_BINFMT_ELF_FDPIC
	/* Preserve FDPIC ABI registers set by ELF_FDPIC_PLAT_INIT(). */
	regs->regs[PTR_R3] = fdpic_exec_map;
	regs->regs[PTR_R4] = fdpic_interp_map;
	regs->regs[PTR_R5] = fdpic_dyn;
#endif

	/*
	 * Execve prepares a userspace pt_regs frame. Populate ECSTATE with a
	 * user ACR level (2) so linx_enter_user() can transition with ACRE.
	 */
	ecstate = linx_ssr_read_cstate();
	ecstate |= LINX_CSTATE_I_BIT;
	ecstate = (ecstate & ~LINX_CSTATE_ACR_MASK) | LINX_CSTATE_ACR_USER;
	regs->ecstate = ecstate;
}

#define cpu_relax()	barrier()

unsigned long __get_wchan(struct task_struct *p);

#endif /* !__ASSEMBLER__ */

#endif /* _ASM_LINX_PROCESSOR_H */
