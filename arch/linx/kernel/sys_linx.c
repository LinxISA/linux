// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

/* Not defined using SYSCALL_DEFINE0 to avoid error injection. */
asmlinkage long __linx_sys_ni_syscall(const struct pt_regs *__unused)
{
	(void)__unused;
	return -ENOSYS;
}

SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, offset)
{
	bool trace = current->pid == 1 && fd != (unsigned long) -1 &&
		     !(flags & MAP_ANONYMOUS);
	long ret;

	if (trace)
		pr_info("linx: mmap pid=%d addr=%#lx len=%#lx prot=%#lx flags=%#lx fd=%lu offset=%#lx\n",
			current->pid, addr, len, prot, flags, fd, offset);

	if (offset & ~PAGE_MASK)
		return -EINVAL;

	ret = ksys_mmap_pgoff(addr, len, prot, flags, fd, offset >> PAGE_SHIFT);

	if (trace)
		pr_info("linx: mmap-ret pid=%d ret=%#lx pgoff=%#lx\n",
			current->pid, ret, offset >> PAGE_SHIFT);

	return ret;
}

#ifndef CONFIG_MMU
SYSCALL_DEFINE3(mprotect, unsigned long, start, size_t, len,
		unsigned long, prot)
{
	(void)start;
	(void)len;
	(void)prot;
	/*
	 * !MMU builds keep mprotect(2) as a no-op so userspace startup paths
	 * that probe memory protection APIs do not fail spuriously.
	 */
	return 0;
}
#endif

/* Signal support is implemented in arch/linx/kernel/signal.c. */
