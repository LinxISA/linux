/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_PTO_RUNTIME_H
#define _ASM_LINX_PTO_RUNTIME_H

#include <linux/types.h>

#define PTO_RUNTIME_GROUP_CONTEXT_MAGIC 0x50544f47u
#define PTO_RUNTIME_GROUP_CONTEXT_VERSION 1u
#define PTO_RUNTIME_GROUP_PARTICIPANTS 4u

enum pto_runtime_group_state {
	PTO_RUNTIME_GROUP_STATE_READY = 0,
	PTO_RUNTIME_GROUP_STATE_RUNNING = 1,
	PTO_RUNTIME_GROUP_STATE_DONE = 2,
	PTO_RUNTIME_GROUP_STATE_ABORT = 3,
};

struct pto_runtime_group_context {
	u32 magic;
	u16 version;
	u16 participant_count;
	volatile u32 *state;
	volatile s32 *result;
	volatile u32 *error;
};

/* state and result reference four PE-indexed elements owned by the launcher. */

/* This is deliberately separate from the Linux syscall trap path. Linux
 * syscalls keep their hosted ABI; a PTO manager installs these callbacks to
 * map a completed group to the architecture's End/Abort lifecycle. */
struct pto_runtime_manager_ops {
	int (*validate)(const struct pto_runtime_group_context *context);
	void (*end)(const struct pto_runtime_group_context *context, int status);
	void (*abort)(const struct pto_runtime_group_context *context, int status);
};

int pto_runtime_register_manager(const struct pto_runtime_manager_ops *ops);
void pto_runtime_unregister_manager(const struct pto_runtime_manager_ops *ops);
void pto_runtime_group_complete(
	const struct pto_runtime_group_context *context, int status);

#endif
