// SPDX-License-Identifier: GPL-2.0-only
/*
 *
 * Copyright (c) 2022 Huawei.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <asm/cpu_ops.h>
#include <asm/lisc.h>
#include <asm/smp.h>

extern char secondary_start_common[];
const struct cpu_operations cpu_ops_lisc;

#define LISC_SERVICE_ID_BASIC	0x1
#define LISC_FUNCTION_ID_GET_VERSION	0X1
#define LISC_FUNCTION_ID_EXIST_SID	0X2

#define LISC_SERVICE_ID_STATE	0x2
#define LISC_FUNCTION_ID_CPU_START	0x1
#define LISC_FUNCTION_ID_CPU_STOP	0x2

static int lisc_etrap(int service_id, unsigned long fid,
			unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	register uintptr_t r7 asm ("r7") = (uintptr_t)(service_id);
	register uintptr_t r6 asm ("r6") = (uintptr_t)(fid);
	register uintptr_t r1 asm ("r1") = (uintptr_t)(arg0);
	register uintptr_t r2 asm ("r2") = (uintptr_t)(arg1);
	register uintptr_t r3 asm ("r3") = (uintptr_t)(arg2);
	asm volatile (	"BSTART.sys fall\n"					\
			"acrc 0\n"					\
			: "+r" (r1), "+r" (r2)				\
			: "r" (r7), "r" (r6), "r" (r3)				\
			: "memory");
	return r1;
}

static int lisc_lxlc_start(unsigned long hartid, unsigned long saddr,
			   unsigned long priv)
{
	return lisc_etrap(LISC_SERVICE_ID_STATE, LISC_FUNCTION_ID_CPU_START, hartid, saddr, priv);
}

static int lisc_cpu_start(unsigned int cpuid, struct task_struct *tidle)
{
	/* start a cpu. */
	int rc;
	unsigned long boot_addr = __pa_symbol(secondary_start_common);
	int hartid = cpuid_to_hartid_map(cpuid);

	cpu_update_secondary_bootdata(cpuid, tidle);

	rc = lisc_lxlc_start(hartid, boot_addr, 0);

	return rc;
}

static int lisc_cpu_prepare(unsigned int cpuid)
{
	if (!cpu_ops_lisc.cpu_start) {
		pr_err("cpu start method not defined for CPU [%d]\n", cpuid);
		return -ENODEV;
	}
	// TODO: implement function
	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU
static int lisc_cpu_disable(unsigned int cpuid)
{
	if (!cpu_ops_lisc.cpu_stop)
		return -EOPNOTSUPP;
	// TODO: implement function
	return 0;
}

static int lisc_lxlc_stop(unsigned long hartid)
{
	return lisc_etrap(LISC_SERVICE_ID_STATE, LISC_FUNCTION_ID_CPU_STOP, hartid, 0, 0);
}

static void lisc_cpu_stop(void)
{
	// TODO: implement function
	// pr_crit("Unable to stop the cpu %u (%d)\n", smp_processor_id(), ret);
	int ret;
	ret = lisc_cpu_stop();

	return ret;
}

static int lisc_cpu_is_stopped(unsigned int cpuid)
{
	// TODO: implement function
	return 0;
}
#endif

const struct cpu_operations cpu_ops_lisc = {
	.name		= "lisc",
	.cpu_prepare	= lisc_cpu_prepare,
	.cpu_start	= lisc_cpu_start,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable	= lisc_cpu_disable,
	.cpu_stop	= lisc_cpu_stop,
	.cpu_is_stopped	= lisc_cpu_is_stopped,
#endif
};
