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

#include <asm/ptrace.h>
#include <asm/ssr.h>

#define LINX_CPU_INTC_NR_IRQS BITS_PER_LONG

static struct irq_domain *intc_domain;
static int linx_dbg_irq_dispatch_count;
static int linx_dbg_irq_eoi_count;

static asmlinkage void linx_intc_irq(struct pt_regs *regs)
{
	unsigned long hwirq = regs->traparg0 & (LINX_CPU_INTC_NR_IRQS - 1);

	if (linx_dbg_irq_dispatch_count < 8) {
		pr_err("Linx dbg: intc dispatch hwirq=%lu trapno=0x%lx traparg0=0x%lx ip=0x%lx ecfg=0x%lx count=%d\n",
		       hwirq, regs->trapno, regs->traparg0,
		       ssr_read(SSR_A1_IPENDING), ssr_read(SSR_A1_ECONFIG),
		       linx_dbg_irq_dispatch_count);
		linx_dbg_irq_dispatch_count++;
	}

	switch (hwirq) {
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
		generic_handle_domain_irq(intc_domain, hwirq);
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

static void linx_intc_irq_eoi(struct irq_data *d)
{
	if (linx_dbg_irq_eoi_count < 8) {
		pr_err("Linx dbg: intc eoi hwirq=%lu ip_before=0x%lx count=%d\n",
		       d->hwirq, ssr_read(SSR_A1_IPENDING),
		       linx_dbg_irq_eoi_count);
		linx_dbg_irq_eoi_count++;
	}
	ssr_write(SSR_EOIEI, d->hwirq);
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
	.irq_eoi = linx_intc_irq_eoi,
};

static int linx_intc_domain_map(struct irq_domain *d, unsigned int irq,
				 irq_hw_number_t hwirq)
{
	if (hwirq == ECAUSE_TRAPNUM_ACR1_TIMER_INT ||
	    hwirq == ECAUSE_TRAPNUM_ACR1_SOFT_INT) {
		irq_set_percpu_devid(irq);
		irq_domain_set_info(d, irq, hwirq, &linx_intc_chip, d->host_data,
				    handle_percpu_devid_irq, NULL, NULL);
	} else {
		irq_domain_set_info(d, irq, hwirq, &linx_intc_chip, d->host_data,
				    handle_fasteoi_irq, NULL, NULL);
	}

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

	intc_domain = irq_domain_add_linear(node, LINX_CPU_INTC_NR_IRQS,
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

	pr_info("%d local interrupts mapped\n", LINX_CPU_INTC_NR_IRQS);

	return 0;
}

IRQCHIP_DECLARE(huawei_lxcpuintc, "linx,cpu-intc", linx_intc_init);
