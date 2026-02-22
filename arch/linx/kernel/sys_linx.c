// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/mman.h>
#include <linux/mm.h>
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
	if (offset & ~PAGE_MASK)
		return -EINVAL;

	return ksys_mmap_pgoff(addr, len, prot, flags, fd, offset >> PAGE_SHIFT);
}

SYSCALL_DEFINE3(mprotect, unsigned long, start, size_t, len,
		unsigned long, prot)
{
	(void)start;
	(void)len;
	(void)prot;
	/*
	 * Linx Linux bring-up currently runs without an MMU. Userspace (musl)
	 * expects mprotect(2) to exist for PIE/FDPIC startup even when memory
	 * permissions are effectively not enforced. Treat it as a no-op.
	 */
	return 0;
}

/* Signal support is implemented in arch/linx/kernel/signal.c. */
