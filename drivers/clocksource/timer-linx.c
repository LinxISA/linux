// SPDX-License-Identifier: GPL-2.0
/*
 * Huawei LinxISA Timer Driver
 * Copyright (C) 2022 Huawei Technologies Co, Ltd.
 *
 * Author:
 * Ruan Jinjie (ruanjinjie@huawei.com)
 */

#define pr_fmt(fmt) "linx-timer: " fmt
#include <linux/bitops.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/sched_clock.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/smp.h>
#include <linux/timex.h>
#include <asm/lisc.h>

static unsigned long linx_timer_freq;
static unsigned int linx_timer_irq;
static int linx_dbg_next_event_count;
static int linx_dbg_interrupt_count;

static u64 linx_clocksource_rdtime(struct clocksource *cs)
{
	return get_cycles64();
}

static u64 notrace linx_sched_clock(void)
{
	return get_cycles64();
}

static struct clocksource linx_clocksource = {
	.name		= "linx_clocksource",
	.rating		= 300,
	.mask		= CLOCKSOURCE_MASK(64),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.read		= linx_clocksource_rdtime,
};

static int linx_clock_next_event(unsigned long delta,
				   struct clock_event_device *ce)
{
	if (linx_dbg_next_event_count < 8) {
		pr_err("Linx dbg: timer next_event delta=%lu now=%llu irq=%u count=%d\n",
		       delta, get_cycles64(), linx_timer_irq,
		       linx_dbg_next_event_count);
		linx_dbg_next_event_count++;
	}
	ssr_set(SSR_A1_ECONFIG, BIT(ECONFIG_TIMER));
	ssr_write(SSR_TIMER_TIMECMP, get_cycles64() + delta);
	return 0;
}

static DEFINE_PER_CPU(struct clock_event_device, linx_clock_event) = {
	.name		= "linx_timer_clockevent",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.rating		= 100,
	.set_next_event	= linx_clock_next_event,
};

static int linx_timer_starting_cpu(unsigned int cpu)
{
	struct clock_event_device *ce = per_cpu_ptr(&linx_clock_event, cpu);

	ce->cpumask = cpumask_of(cpu);
	ce->irq = linx_timer_irq;
	clockevents_config_and_register(ce, linx_timer_freq, 100, 0x7fffffff);

	enable_percpu_irq(linx_timer_irq,
			  irq_get_trigger_type(linx_timer_irq));
	return 0;
}

static int linx_timer_dying_cpu(unsigned int cpu)
{
	disable_percpu_irq(linx_timer_irq);
	return 0;
}

static irqreturn_t linx_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evdev = this_cpu_ptr(&linx_clock_event);

	if (linx_dbg_interrupt_count < 8) {
		pr_err("Linx dbg: timer irq irq=%d now=%llu count=%d\n",
		       irq, get_cycles64(), linx_dbg_interrupt_count);
		linx_dbg_interrupt_count++;
	}
	ssr_clear(SSR_A1_ECONFIG, BIT(ECONFIG_TIMER));
	evdev->event_handler(evdev);

	return IRQ_HANDLED;
}

/*
 * timer {
 *      interrupts-extended = <0x04 0x04 0x02 0x04>;
 *      always-on;
 *      compatible = "linx,linx-timer";
 * };
 */
static int __init linx_timer_init_dt(struct device_node *np)
{
	int rc;
	u32 i, nr_irqs;
	struct of_phandle_args oirq;

	/*
	 * Ensure that LINX-timer device interrupts is RV_IRQ_TIMER.
	 * If it's anything else then we ignore the device.
	 */
	nr_irqs = of_irq_count(np);
	for (i = 0; i < nr_irqs; i++) {
		if (of_irq_parse_one(np, i, &oirq)) {
			pr_err("%pOFP: failed to parse irq %d.\n", np, i);
			continue;
		}

		if (oirq.args[0] != ECAUSE_TRAPNUM_ACR1_TIMER_INT) {
			pr_err("%pOFP: invalid irq %d (hwirq %d)\n",
			       np, i, oirq.args[0]);
			return -ENODEV;
		}

		/* Find parent irq domain and map timer irq */
		if (!linx_timer_irq &&
		    oirq.args[0] == ECAUSE_TRAPNUM_ACR1_TIMER_INT &&
		    irq_find_host(oirq.np)) {
			linx_timer_irq = irq_of_parse_and_map(np, i);
		}
	}

	linx_timer_freq = linx_timebase;

	pr_info("%pOFP: timer running at %ld Hz\n", np, linx_timer_freq);

	rc = clocksource_register_hz(&linx_clocksource, linx_timer_freq);
	if (rc) {
		pr_err("%pOFP: clocksource register failed [%d]\n", np, rc);
		return rc;
	}

	sched_clock_register(linx_sched_clock, 64, linx_timer_freq);

	rc = request_percpu_irq(linx_timer_irq, linx_timer_interrupt,
				 "linx-timer", &linx_clock_event);
	if (rc) {
		pr_err("registering percpu irq failed [%d]\n", rc);
		return rc;
	}
	rc = cpuhp_setup_state(CPUHP_AP_LINX_TIMER_STARTING,
				"clockevents/linx/timer:starting",
				linx_timer_starting_cpu,
				linx_timer_dying_cpu);
	if (rc) {
		pr_err("%pOFP: cpuhp setup state failed [%d]\n", np, rc);
		goto fail_free_irq;
	}

	return 0;

fail_free_irq:
	free_irq(linx_timer_irq, &linx_clock_event);
	return rc;
}

TIMER_OF_DECLARE(linx_timer, "linx,linx-timer", linx_timer_init_dt);
