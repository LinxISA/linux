/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_MMU_CONTEXT_H
#define _ASM_LINX_MMU_CONTEXT_H

/*
 * NOMMU bring-up: mm context switches are a no-op.
 */

struct task_struct;
struct mm_struct;

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
}

static inline int init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	return 0;
}

static inline void destroy_context(struct mm_struct *mm)
{
}

static inline void activate_mm(struct mm_struct *prev_mm, struct mm_struct *next_mm)
{
}

static inline void deactivate_mm(struct task_struct *tsk, struct mm_struct *mm)
{
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
}

#endif /* _ASM_LINX_MMU_CONTEXT_H */
