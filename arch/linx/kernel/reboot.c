// SPDX-License-Identifier: GPL-2.0-only

#include <linux/reboot.h>
#include <linux/types.h>

#include <asm/processor.h>

#define LINX_VIRT_EXIT_REG 0x10000004UL

static __always_inline void linx_virt_request_shutdown(u32 code)
{
	*(volatile u32 *)(LINX_VIRT_EXIT_REG) = code;
}

void machine_restart(char *cmd)
{
	do_kernel_restart(cmd);
	linx_virt_request_shutdown(1);
	while (1)
		cpu_relax();
}

void machine_halt(void)
{
	linx_virt_request_shutdown(2);
	while (1)
		cpu_relax();
}

void machine_power_off(void)
{
	do_kernel_power_off();
	linx_virt_request_shutdown(0);
	while (1)
		cpu_relax();
}
