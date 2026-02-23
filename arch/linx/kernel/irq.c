// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <asm/irqflags.h>
#include <asm/ssr.h>

enum {
	LINX_NR_VIRT_IRQS = 64,
};

static void linx_irq_mask(struct irq_data *d)
{
	(void)d;
}

static void linx_irq_unmask(struct irq_data *d)
{
	(void)d;
}

static void linx_irq_eoi(struct irq_data *d)
{
	/* QEMU models EOIEI as an SSR write on the managing ACR bank. */
	linx_ssr_write_eoiei_acr1((u64)d->hwirq);
}

static struct irq_chip linx_irq_chip = {
	.name = "linx-virt",
	.irq_mask = linx_irq_mask,
	.irq_unmask = linx_irq_unmask,
	.irq_eoi = linx_irq_eoi,
};

void __init init_IRQ(void)
{
	unsigned int irq;

	for (irq = 0; irq < LINX_NR_VIRT_IRQS; irq++)
		irq_set_chip_and_handler(irq, &linx_irq_chip, handle_fasteoi_irq);
}
