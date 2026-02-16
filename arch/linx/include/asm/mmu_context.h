/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_MMU_CONTEXT_H
#define _ASM_LINX_MMU_CONTEXT_H

struct task_struct;
struct mm_struct;

#ifdef CONFIG_MMU

#include <asm/ssr.h>
#include <asm-generic/mm_hooks.h>

static inline void linx_flush_tlb_all(void)
{
	asm volatile("tlb.iall" : : : "memory");
}

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	(void)mm;
	(void)tsk;
}

static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	(void)tsk;
	(void)mm;
	return 0;
}

static inline void destroy_context(struct mm_struct *mm)
{
	(void)mm;
}

static inline void activate_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm)
{
	u64 ttbr0;

	(void)prev_mm;

	ttbr0 = (u64)__pa(next_mm->pgd);
	linx_hl_ssrset(ttbr0, 0x1f10); /* TTBR0_ACR1 */
	linx_flush_tlb_all();
}

static inline void deactivate_mm(struct task_struct *tsk, struct mm_struct *mm)
{
	(void)tsk;
	(void)mm;
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	u64 ttbr0;

	(void)tsk;

	if (prev == next)
		return;

	ttbr0 = (u64)__pa(next->pgd);
	linx_hl_ssrset(ttbr0, 0x1f10); /* TTBR0_ACR1 */
	linx_flush_tlb_all();
}

#else /* !CONFIG_MMU */

/*
 * NOMMU bring-up: mm context switches are a no-op.
 */
static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk) {}
static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm) { return 0; }
static inline void destroy_context(struct mm_struct *mm) {}
static inline void activate_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm) {}
static inline void deactivate_mm(struct task_struct *tsk, struct mm_struct *mm) {}
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk) {}

#endif /* CONFIG_MMU */

#endif /* _ASM_LINX_MMU_CONTEXT_H */
