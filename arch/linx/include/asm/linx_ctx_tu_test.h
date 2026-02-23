/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_CTX_TU_TEST_H
#define _ASM_LINX_CTX_TU_TEST_H

#include <linux/types.h>

struct pt_regs;

enum linx_ctx_tu_test_pending_reason {
	LINX_CTX_TU_PENDING_NONE = 0,
	LINX_CTX_TU_PENDING_TIMER_IRQ = 1,
	LINX_CTX_TU_PENDING_STEP_TRAP = 2,
};

void linx_ctx_tu_test_note_timer_irq(struct pt_regs *regs, u64 irq_id);
void linx_ctx_tu_test_note_step_trap(struct pt_regs *regs, u8 trapnum);
enum linx_ctx_tu_test_pending_reason linx_ctx_tu_test_take_pending_reason(void);
void linx_ctx_tu_test_poison_manager_state(void);

#endif /* _ASM_LINX_CTX_TU_TEST_H */
