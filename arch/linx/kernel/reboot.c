// SPDX-License-Identifier: GPL-2.0-only

#include <linux/reboot.h>

#include <asm/processor.h>

void machine_restart(char *cmd)
{
	do_kernel_restart(cmd);
	while (1)
		cpu_relax();
}

void machine_halt(void)
{
	while (1)
		cpu_relax();
}

void machine_power_off(void)
{
	do_kernel_power_off();
	while (1)
		cpu_relax();
}

