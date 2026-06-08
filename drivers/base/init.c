// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/memory.h>
#include <linux/of.h>
#include <linux/backing-dev.h>

#include "base.h"

/**
 * driver_init - initialize driver model.
 *
 * Call the driver model init functions to initialize their
 * subsystems. Called early from init/main.c.
 */
void __init driver_init(void)
{
	pr_err("Linx dbg: driver_init enter\n");
	/* These are the core pieces */
	bdi_init(&noop_backing_dev_info);
	pr_err("Linx dbg: driver_init after bdi_init\n");
	devtmpfs_init();
	pr_err("Linx dbg: driver_init after devtmpfs_init\n");
	devices_init();
	pr_err("Linx dbg: driver_init after devices_init\n");
	buses_init();
	pr_err("Linx dbg: driver_init after buses_init\n");
	classes_init();
	pr_err("Linx dbg: driver_init after classes_init\n");
	firmware_init();
	pr_err("Linx dbg: driver_init after firmware_init\n");
	hypervisor_init();
	pr_err("Linx dbg: driver_init after hypervisor_init\n");

	/* These are also core pieces, but must come after the
	 * core core pieces.
	 */
#ifdef CONFIG_LINX_INTC
	/*
	 * Faux bus registration currently wedges the Linx bring-up path inside
	 * driver_register(). Keep it disabled until the driver-core interaction
	 * is debugged; it is not required for the early initramfs boot lane.
	 */
	pr_warn("Linx dbg: skipping faux_bus_init during bring-up\n");
#else
	faux_bus_init();
	pr_err("Linx dbg: driver_init after faux_bus_init\n");
#endif
	of_core_init();
	pr_err("Linx dbg: driver_init after of_core_init\n");
	platform_bus_init();
	pr_err("Linx dbg: driver_init after platform_bus_init\n");
	auxiliary_bus_init();
	pr_err("Linx dbg: driver_init after auxiliary_bus_init\n");
	memory_dev_init();
	pr_err("Linx dbg: driver_init after memory_dev_init\n");
	node_dev_init();
	pr_err("Linx dbg: driver_init after node_dev_init\n");
	cpu_dev_init();
	pr_err("Linx dbg: driver_init after cpu_dev_init\n");
	container_dev_init();
	pr_err("Linx dbg: driver_init after container_dev_init\n");
}
