// SPDX-License-Identifier: GPL-2.0-only

#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/init.h>

#include <asm/ssr.h>

static u64 linx_clocksource_read(struct clocksource *cs)
{
	(void)cs;
	return linx_ssr_read_time();
}

static struct clocksource linx_clocksource = {
	.name	= "linx-time",
	.rating	= 300,
	.read	= linx_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int linx_clockevent_shutdown(struct clock_event_device *ced)
{
	(void)ced;
	linx_ssr_write_timecmp_acr1(0);
	return 0;
}

static int linx_clockevent_set_next_event(unsigned long delta,
					  struct clock_event_device *ced)
{
	u64 now;

	(void)ced;
	now = linx_ssr_read_time();
	linx_ssr_write_timecmp_acr1(now + (u64)delta);
	return 0;
}

static struct clock_event_device linx_clockevent = {
	.name			= "linx-timer0",
	.features		= CLOCK_EVT_FEAT_ONESHOT,
	.rating			= 300,
	.set_state_shutdown	= linx_clockevent_shutdown,
	.set_next_event		= linx_clockevent_set_next_event,
};

void linx_timer_handle_irq(void);

void linx_timer_handle_irq(void)
{
	/*
	 * Called from the trap vector. The tick framework installs the actual
	 * event handler when the clock event device is registered.
	 */
	if (linx_clockevent.event_handler)
		linx_clockevent.event_handler(&linx_clockevent);
}

void __init time_init(void)
{
	/* Ensure no stale timer interrupt remains armed across resets. */
	linx_ssr_write_timecmp_acr1(0);

	linx_clockevent.cpumask = cpumask_of(0);

	clocksource_register_hz(&linx_clocksource, 1000000000);
	clockevents_config_and_register(&linx_clockevent, 1000000000, 1,
					(unsigned long)-1);
}
