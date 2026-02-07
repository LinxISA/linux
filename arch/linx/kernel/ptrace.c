// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/regset.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/elf.h>

#include <asm/processor.h>
#include <asm/ptrace.h>

static int linx_gpr_get(struct task_struct *target,
			const struct user_regset *regset,
			struct membuf to)
{
	const struct pt_regs *regs = task_pt_regs(target);

	return membuf_write(&to, regs->regs, sizeof(regs->regs));
}

static const struct user_regset linx_regsets[] = {
	[0] = {
		USER_REGSET_NOTE_TYPE(PRSTATUS),
		.n = ELF_NGREG,
		.size = sizeof(elf_greg_t),
		.align = sizeof(elf_greg_t),
		.regset_get = linx_gpr_get,
	},
};

static const struct user_regset_view linx_user_view = {
	.name = "linx",
	.e_machine = EM_LINXISA,
	.regsets = linx_regsets,
	.n = ARRAY_SIZE(linx_regsets),
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	(void)task;
	return &linx_user_view;
}

long arch_ptrace(struct task_struct *child, long request, unsigned long addr,
		 unsigned long data)
{
	(void)child;
	(void)request;
	(void)addr;
	(void)data;
	return -EINVAL;
}

void ptrace_disable(struct task_struct *child)
{
	(void)child;
}
