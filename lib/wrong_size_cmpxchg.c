// SPDX-License-Identifier: GPL-2.0

#include <linux/bug.h>
#include <linux/compiler.h>

#include <asm-generic/cmpxchg-local.h>

__noreturn unsigned long wrong_size_cmpxchg(volatile void *ptr)
{
	(void)ptr;
	BUG();
}

__noreturn void __generic_xchg_called_with_bad_pointer(void)
{
	BUG();
}
