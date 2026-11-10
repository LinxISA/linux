// SPDX-License-Identifier: GPL-2.0-only
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <asm/pto_runtime.h>

static DEFINE_MUTEX(pto_runtime_manager_lock);
static const struct pto_runtime_manager_ops *pto_runtime_manager;

static bool pto_runtime_context_valid(
	const struct pto_runtime_group_context *context)
{
	return context && context->magic == PTO_RUNTIME_GROUP_CONTEXT_MAGIC &&
		context->version == PTO_RUNTIME_GROUP_CONTEXT_VERSION &&
		context->participant_count == PTO_RUNTIME_GROUP_PARTICIPANTS &&
		context->state && context->result && context->error;
}

int pto_runtime_register_manager(const struct pto_runtime_manager_ops *ops)
{
	int ret = 0;

	if (!ops || !ops->end || !ops->abort)
		return -EINVAL;
	mutex_lock(&pto_runtime_manager_lock);
	if (pto_runtime_manager)
		ret = -EBUSY;
	else
		WRITE_ONCE(pto_runtime_manager, ops);
	mutex_unlock(&pto_runtime_manager_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(pto_runtime_register_manager);

void pto_runtime_unregister_manager(const struct pto_runtime_manager_ops *ops)
{
	mutex_lock(&pto_runtime_manager_lock);
	if (pto_runtime_manager == ops)
		WRITE_ONCE(pto_runtime_manager, NULL);
	mutex_unlock(&pto_runtime_manager_lock);
}
EXPORT_SYMBOL_GPL(pto_runtime_unregister_manager);

void pto_runtime_group_complete(
	const struct pto_runtime_group_context *context, int status)
{
	const struct pto_runtime_manager_ops *ops =
		READ_ONCE(pto_runtime_manager);

	if (!pto_runtime_context_valid(context) || !ops ||
		(ops->validate && ops->validate(context))) {
		if (context && context->error)
			WRITE_ONCE(*context->error, 1);
		if (ops)
			ops->abort(context, status ? status : -EINVAL);
		return;
	}

	/* The manager implementation owns GM-drain and the final End/Abort call. */
	if (status)
		ops->abort(context, status);
	else
		ops->end(context, 0);
}
EXPORT_SYMBOL_GPL(pto_runtime_group_complete);
