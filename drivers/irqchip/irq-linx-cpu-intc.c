// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017-2018 SiFive
 * Copyright (C) 2020 Western Digital Corporation or its affiliates.
 */

#define pr_fmt(fmt) "linx-intc: " fmt
#include <linux/atomic.h>
#include <linux/bits.h>
#include <linux/cpu.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/smp.h>

static struct irq_domain *intc_domain;

static asmlinkage void linx_intc_irq(struct pt_regs *regs)
{
	unsigned long cause = regs->trapno & ECAUSE_TRAPNUM_MASK;

	if (unlikely(cause >= ECAUSE_TRAPNUM_ACR1_SOFT_INT + 1))
		panic("unexpected interrupt cause");

	switch (cause) {
#ifdef CONFIG_SMP
	case ECAUSE_TRAPNUM_ACR1_SOFT_INT:
		/*
		 * We only use software interrupts to pass IPIs, so if a
		 * non-SMP system gets one, then we don't know what to do.
		 */
		handle_IPI(regs);
		break;
#endif
	default:
		generic_handle_domain_irq(intc_domain, cause);
		break;
	}
}

/*
 * On RISC-V systems local interrupts are masked or unmasked by writing
 * the SIE (Supervisor Interrupt Enable) CSR.  As CSRs can only be written
 * on the local hart, these functions can only be called on the hart that
 * corresponds to the IRQ chip.
 */

static void linx_intc_irq_mask(struct irq_data *d)
{
	ssr_clear(SSR_A1_ECONFIG, BIT(d->hwirq));
}

static void linx_intc_irq_unmask(struct irq_data *d)
{
	ssr_set(SSR_A1_ECONFIG, BIT(d->hwirq));
}

static int linx_intc_cpu_starting(unsigned int cpu)
{
	ssr_set(SSR_A1_ECONFIG, BIT(ECONFIG_EXTERNAL));
	ssr_set(SSR_A1_ECONFIG, BIT(ECONFIG_SOFTWARE));
	return 0;
}

static int linx_intc_cpu_dying(unsigned int cpu)
{
	ssr_clear(SSR_A1_ECONFIG, BIT(ECONFIG_EXTERNAL));
	ssr_clear(SSR_A1_ECONFIG, BIT(ECONFIG_SOFTWARE));
	return 0;
}

static struct irq_chip linx_intc_chip = {
	.name = "Linx CPU INTC",
	.irq_mask = linx_intc_irq_mask,
	.irq_unmask = linx_intc_irq_unmask,
};

static int linx_intc_domain_map(struct irq_domain *d, unsigned int irq,
				 irq_hw_number_t hwirq)
{
	irq_set_percpu_devid(irq);
	irq_domain_set_info(d, irq, hwirq, &linx_intc_chip, d->host_data,
			    handle_percpu_devid_irq, NULL, NULL);

	return 0;
}

static const struct irq_domain_ops linx_intc_domain_ops = {
	.map	= linx_intc_domain_map,
	.xlate	= irq_domain_xlate_onecell,
};

static int __init linx_intc_init(struct device_node *node,
				  struct device_node *parent)
{
	int rc, hartid;

	hartid = riscv_of_parent_hartid(node);
	if (hartid < 0) {
		pr_warn("unable to find hart id for %pOF\n", node);
		return 0;
	}

	/*
	 * The DT will have one INTC DT node under each CPU (or HART)
	 * DT node so linx_intc_init() function will be called once
	 * for each INTC DT node. We only need to do INTC initialization
	 * for the INTC DT node belonging to boot CPU (or boot HART).
	 */
	if (riscv_hartid_to_cpuid(hartid) != smp_processor_id())
		return 0;

	intc_domain = irq_domain_add_linear(node, ECAUSE_TRAPNUM_ACR1_SOFT_INT + 1,
					    &linx_intc_domain_ops, NULL);
	if (!intc_domain) {
		pr_err("unable to add IRQ domain\n");
		return -ENXIO;
	}

	rc = set_handle_irq(&linx_intc_irq);
	if (rc) {
		pr_err("failed to set irq handler\n");
		return rc;
	}

	cpuhp_setup_state(CPUHP_AP_IRQ_LINX_CPU_INTC_STARTING,
			  "irqchip/linx/cpu-intc:starting",
			  linx_intc_cpu_starting,
			  linx_intc_cpu_dying);

	pr_info("%d local interrupts mapped\n", ECAUSE_TRAPNUM_ACR1_SOFT_INT + 1);

	return 0;
}

IRQCHIP_DECLARE(huawei_lxcpuintc, "linx,cpu-intc", linx_intc_init);
