// SPDX-License-Identifier: GPL-2.0-only
/*
 * Linx Init Call(LISC) implementation.
 *
 * Copyright (c) 2022 Huawei.
 */

#include <linux/init.h>
#include <linux/pm.h>
#include <asm/lisc.h>
#include <asm/smp.h>
#include <asm/block-def.h>
#include <asm/debug_uart.h>

struct lisc_ret lisc_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct lisc_ret ret;

	// TODO: implement function
	ret.error = 0;
	ret.value = 0;
	return ret;
}
EXPORT_SYMBOL(lisc_ecall);

/**
 * lisc_shutdown() - stop all cpu form running
 */
void lisc_shutdown(void)
{
	linx_debug_uart_puts("LINX_REBOOT lisc_shutdown\n");
	pr_info("lisc shut down.\n");
	__asm__ __volatile__(
		"BSTART.std fall\n"
		"  ebreak 1\n"
		"  c.bstop\n");
	while (1) {}
}

#ifdef CONFIG_LINX_INTC
extern const struct riscv_ipi_ops lxic_ipi_ops;
#endif

void __init lisc_init(void)
{
	pm_power_off = lisc_shutdown;

#ifdef CONFIG_LINX_INTC
	riscv_set_ipi_ops(&lxic_ipi_ops);
#endif
	pr_info("Power by LISC.\n");
}
