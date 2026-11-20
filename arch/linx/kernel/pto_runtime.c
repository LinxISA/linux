// SPDX-License-Identifier: GPL-2.0-only
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/limits.h>
#include <asm/pto_runtime.h>

static DEFINE_MUTEX(pto_runtime_manager_lock);
static const struct pto_runtime_manager_ops *pto_runtime_manager;
static const struct pto_runtime_group_context *active_context;
static u64 active_invocation, next_invocation;
static struct pto_runtime_launch active_launch;

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

	if (!ops || !ops->end || !ops->abort ||
	    (ops->launch && (!ops->quiesce || !ops->validate)))
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

int pto_runtime_unregister_manager(const struct pto_runtime_manager_ops *ops)
{
	int ret = 0;

	mutex_lock(&pto_runtime_manager_lock);
	/* An active backend must remain alive until formal End/Abort quiesces. */
	if (pto_runtime_manager == ops && !active_context)
		WRITE_ONCE(pto_runtime_manager, NULL);
	else if (pto_runtime_manager == ops)
		ret = -EBUSY;
	else
		ret = -ENOENT;
	mutex_unlock(&pto_runtime_manager_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(pto_runtime_unregister_manager);

void pto_runtime_group_complete(
	const struct pto_runtime_group_context *context, int status)
{
	const struct pto_runtime_manager_ops *ops;

	mutex_lock(&pto_runtime_manager_lock);
	ops = pto_runtime_manager;
	/* Legacy completion-only managers have no admission identity. Never let
	 * this compatibility entry retire a launch admitted by the new API. */
	if (!ops || ops->launch || !pto_runtime_context_valid(context))
		goto out;
	if (ops->validate && ops->validate(context))
		goto out;
	if (status)
		ops->abort(context, status);
	else
		ops->end(context, 0);
 out:
	mutex_unlock(&pto_runtime_manager_lock);
}
EXPORT_SYMBOL_GPL(pto_runtime_group_complete);

static bool private_ranges_valid(const struct pto_runtime_launch *launch)
{
	u64 base[8], end[8];
	unsigned int i, j;

	if (!launch->entry || (launch->entry & 1))
		return false;
	for (i = 0; i < PTO_RUNTIME_GROUP_PARTICIPANTS; ++i) {
		const struct pto_runtime_private_context *p = &launch->pe[i];

		base[2*i] = p->stack_base;
		end[2*i] = p->stack_base + p->stack_size;
		base[2*i+1] = p->tls_base;
		end[2*i+1] = p->tls_base + p->tls_size;
		if (!p->stack_base || !p->tls_base ||
		    end[2*i] <= base[2*i] || end[2*i+1] <= base[2*i+1] ||
		    p->stack_pointer <= base[2*i] ||
		    p->stack_pointer > end[2*i] || (p->stack_pointer & 15) ||
		    p->thread_pointer < base[2*i+1] ||
		    p->thread_pointer >= end[2*i+1])
			return false;
	}
	for (i = 0; i < 8; ++i)
		for (j = i + 1; j < 8; ++j)
			if (base[i] < end[j] && base[j] < end[i])
				return false;
	return true;
}

int pto_runtime_group_launch(const struct pto_runtime_launch *launch,
			     u64 *invocation)
{
	const struct pto_runtime_manager_ops *ops;
	int ret;
	u64 id;

	if (!launch || !invocation || !pto_runtime_context_valid(launch->context) ||
	    !private_ranges_valid(launch))
		return -EINVAL;
	mutex_lock(&pto_runtime_manager_lock);
	ops = pto_runtime_manager;
	ret = -EOPNOTSUPP;
	if (!ops || !ops->launch)
		goto out;
	ret = -EBUSY;
	if (active_context)
		goto out;
	ret = ops->validate(launch->context);
	if (ret)
		goto out;
	ret = -EOVERFLOW;
	if (next_invocation == U64_MAX)
		goto out;
	id = ++next_invocation;
	active_launch = *launch;
	ret = ops->launch(&active_launch, id);
	if (!ret) {
		active_context = launch->context;
		active_invocation = id;
		*invocation = id;
	}
 out:
	mutex_unlock(&pto_runtime_manager_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(pto_runtime_group_launch);

int pto_runtime_group_finish(const struct pto_runtime_group_context *context,
			     u64 invocation, int status)
{
	const struct pto_runtime_manager_ops *ops;
	int ret = -ESTALE;

	mutex_lock(&pto_runtime_manager_lock);
	ops = pto_runtime_manager;
	/* Reject stale completion before dereferencing its old descriptor. */
	if (!ops || !active_context || context != active_context ||
	    invocation != active_invocation)
		goto out;
	ret = ops->validate(context);
	if (ret && !status)
		status = ret;
	ops->quiesce(context, invocation, status != 0);
	if (status)
		ops->abort(context, status);
	else
		ops->end(context, 0);
	active_context = NULL;
	active_invocation = 0;
	ret = 0;
 out:
	mutex_unlock(&pto_runtime_manager_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(pto_runtime_group_finish);
