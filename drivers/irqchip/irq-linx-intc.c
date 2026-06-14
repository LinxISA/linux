// SPDX-License-Identifier: GPL-2.0-only
/*
 * Huawei LinxISA Interrupt Controller Driver
 *
 * Copyright (C) 2022 Huawei Technologies Co, Ltd.
 *
 * Author: 
 * Liao Chang (liaochang1@huawei.com)
 * Wang Zhu (wangzhu9@huawei.com)
 * Ruan Jinjie (ruanjinjie@huawei.com)
 */
#define pr_fmt(fmt) "lxintc: " fmt
#include <linux/iommu.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/cpuhotplug.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>

/* 2 domains: ACR0, ACR1 */
#define DOM_ACR0		0
#define DOM_ACR1		1

/* IRQ number */
#define MAX_CPU_NUM		8
#define FDT_LXIC_NIRQ		256
/* irq number is 0 - 254, 255 is invalid irq */
#define LXIC_NIRQ		(FDT_LXIC_NIRQ - 1)

/* pending and enable register bit and size */
#define CTL_PENDING_BIT		1
#define CTL_ENABLE_BIT		1
#define CTL_TYPE_BIT		4
#define CTL_PENDING_WORD	((FDT_LXIC_NIRQ * CTL_PENDING_BIT) >> 5)
#define CTL_ENABLE_WORD		((FDT_LXIC_NIRQ * CTL_ENABLE_BIT) >> 5)
#define CTL_TYPE_WORD		((FDT_LXIC_NIRQ * CTL_TYPE_BIT) >> 5)

/* mmio registers */
#define CTL_PENDING_BASE	0
#define CTL_PENDING_END		(CTL_PENDING_BASE + CTL_PENDING_WORD * 4)
#define CTL_ENABLE_BASE		CTL_PENDING_END
#define CTL_ENABLE_END		(CTL_ENABLE_BASE + CTL_ENABLE_WORD * 4)
#define CTL_TYPE_BASE		CTL_ENABLE_END
#define CTL_TYPE_END		(CTL_TYPE_BASE + CTL_TYPE_WORD * 4)
#define CTL_SETVEC_BASE		CTL_TYPE_END
#define CTL_SETVEC_END		(CTL_SETVEC_BASE + 4)
#define CTL_CLRVEC_BASE		CTL_SETVEC_END
#define CTL_CLRVEC_END		(CTL_CLRVEC_BASE + 4)
#define CTL_ENVEC_BASE		CTL_CLRVEC_END
#define CTL_ENVEC_END		(CTL_ENVEC_BASE + 4)
#define CTL_DISVEC_BASE		CTL_ENVEC_END
#define CTL_DISVEC_END		(CTL_DISVEC_BASE + 4)
#define CTL_THRESHOLD_BASE	CTL_DISVEC_END
#define CTL_THRESHOLD_END	(CTL_THRESHOLD_BASE + 4)
#define CTL_IPI_BASE		CTL_THRESHOLD_END
#define CTL_IPI_END		(CTL_IPI_BASE + 4)
#define CTL_SETTYPE_BASE	CTL_IPI_END
#define CTL_SETTYPE_END		(CTL_SETTYPE_BASE + 4)
#define CTL_SETDOM_BASE		CTL_SETTYPE_END
#define CTL_SETDOM_END		(CTL_SETDOM_BASE + 4)
#define CTL_GETDOM_BASE		CTL_SETDOM_END
#define CTL_GETDOM_END		(CTL_GETDOM_BASE + 4)

/* IRQ type */
#define IRQ_TYPE_NONE		0x0
#define IRQ_TYPE_EDGE_RISING	0x1
#define IRQ_TYPE_EDGE_FALLING	0x2
#define IRQ_TYPE_EDGE_BOTH	(IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)
#define IRQ_TYPE_LEVEL_HIGH	0x4
#define IRQ_TYPE_LEVEL_LOW	0x8
#define IRQ_TYPE_LEVEL_MASK	(IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH)
#define IRQ_TYPE_SENSE_MASK	0xf

#define MAX_VIRQ	1024

struct lxic_priv {
	void __iomem *virt_base;
	phys_addr_t phys_base;
	resource_size_t size;
	u32 nirq;
	u32 ndev;
	u32 hstride; /* hart stride */
	u32 dstride; /* domain stride */
	struct cpumask lmask;
	struct irq_domain *irqdomain;
	struct irq_domain *base_domain;
	struct irq_domain *pci_domain;

	unsigned int *virq_target_cpu;
	unsigned int *virq_vector;
	bool wbi_msi_flag[MAX_CPU_NUM][FDT_LXIC_NIRQ];
	raw_spinlock_t map_lock;
	raw_spinlock_t set_target_lock;
};

struct lxic_handler {
	bool			present;
	raw_spinlock_t		enable_lock;
	void __iomem		*enable_base;
	void __iomem		*disable_base;
	void __iomem		*threshold_base;
	void __iomem		*ipi_base;
	void __iomem		*settype_base;
	phys_addr_t 		msi_pa;
	struct lxic_priv	*priv;

	raw_spinlock_t ids_lock;
	unsigned long *ids_used_bimap;
	unsigned int *vector_virq;
};

static int lxic_parent_irq __ro_after_init;
static bool lxic_cpuhp_setup_done __ro_after_init;
static DEFINE_PER_CPU(struct lxic_handler, lxic_handlers);

static inline void lxic_irq_toggle(const struct cpumask *mask,
				   irq_hw_number_t hwirq, int enable)
{
	uint32_t cpu, vector;
	struct lxic_handler *handler = NULL;

	vector = hwirq & 0xff;
	for_each_cpu(cpu, mask) {
		handler = per_cpu_ptr(&lxic_handlers, cpu);

		if (handler->present &&
		    cpumask_test_cpu(cpu, &handler->priv->lmask)) {
			raw_spin_lock(&handler->enable_lock);
			if (enable)
				writel(vector, handler->enable_base);
			else
				writel(vector, handler->disable_base);
			raw_spin_unlock(&handler->enable_lock);
		}
	}
}

static void lxic_irq_unmask(struct irq_data *d)
{
	struct lxic_priv *priv = irq_data_get_irq_chip_data(d);

	if (priv != NULL)
		lxic_irq_toggle(irq_data_get_affinity_mask(d), d->hwirq, 1);
}

static void lxic_irq_mask(struct irq_data *d)
{
	struct lxic_priv *priv = irq_data_get_irq_chip_data(d);

	if (priv != NULL)
		lxic_irq_toggle(irq_data_get_affinity_mask(d), d->hwirq, 0);
}

static void lxic_irq_eoi(struct irq_data *d)
{
	uint32_t vector;

	if (d == NULL) {
		pr_err("Parameter irq_data is NULL\n");
		return;
	}

	vector = d->hwirq & 0xff;
	ssr_write(SSR_EOIEI, vector);
}

static int lxic_get_cpu(struct lxic_priv *priv,
			 const struct cpumask *mask_val, bool force,
			 unsigned int *out_target_cpu)
{
	struct cpumask amask;
	unsigned int cpu;

	cpumask_and(&amask, &priv->lmask, mask_val);

	if (force)
		cpu = cpumask_first(&amask);
	else {
		cpu = cpumask_any_and(&amask, cpu_online_mask);
	}

	if (cpu >= nr_cpu_ids)
		return -EINVAL;

	if (out_target_cpu)
		*out_target_cpu = cpu;

	return 0;
}

static void lxic_virq_set_target(struct lxic_priv *priv,
				 unsigned int virq, unsigned int vector, unsigned int target_cpu)
{
	struct lxic_handler *handler = per_cpu_ptr(&lxic_handlers, target_cpu);

	raw_spin_lock(&priv->set_target_lock);
	priv->virq_target_cpu[virq] = target_cpu;
	priv->virq_vector[virq] = vector;
	handler->vector_virq[vector] = virq;
	raw_spin_unlock(&priv->set_target_lock);
}

static unsigned int lxic_virq_get_target(struct lxic_priv *priv,
					unsigned int virq, unsigned int *vector)
{
	unsigned int ret;
	raw_spin_lock(&priv->set_target_lock);
	ret = priv->virq_target_cpu[virq];
	*vector = priv->virq_vector[virq];
	raw_spin_unlock(&priv->set_target_lock);

	return ret;
}

static int lxic_vectors_alloc(struct lxic_priv *priv, unsigned int cpu,
			   unsigned int max_id, unsigned int order)
{
	int ret;

	struct lxic_handler *handler = per_cpu_ptr(&lxic_handlers, cpu);

	if ((priv->nirq < max_id) || (max_id < BIT(order)))
		return -EINVAL;

	raw_spin_lock(&handler->ids_lock);
	ret = bitmap_find_free_region(handler->ids_used_bimap,
				      max_id + 1, order);
	raw_spin_unlock(&handler->ids_lock);

	return ret;
}

static void lxic_vectors_free(struct lxic_priv *priv, unsigned int cpu,
				unsigned int virq, unsigned int vector,
				unsigned int nr_irqs)
{
	struct lxic_handler *handler = per_cpu_ptr(&lxic_handlers, cpu);
	int i;

	raw_spin_lock(&handler->ids_lock);
	bitmap_release_region(handler->ids_used_bimap, vector,
						  get_count_order(nr_irqs));
	raw_spin_unlock(&handler->ids_lock);

	raw_spin_lock(&priv->set_target_lock);
	for(i = virq; i< virq + nr_irqs; i++) {
		priv->virq_target_cpu[i] = UINT_MAX;
		priv->virq_vector[i] = UINT_MAX;
	}

	for(i = vector; i < vector + nr_irqs; i++)
		handler->vector_virq[i] = 0;
	raw_spin_unlock(&priv->set_target_lock);
}

#ifdef CONFIG_SMP
static int lxic_set_affinity(struct irq_data *d,
				const struct cpumask *mask_val, bool force)
{
	return IRQ_SET_MASK_OK_DONE;
}

static int lxic_MSI_set_affinity(struct irq_data *d,
				const struct cpumask *mask_val, bool force)
{
	struct lxic_priv *priv = irq_data_get_irq_chip_data(d);
	unsigned int target_cpu;
	unsigned int cur_cpu = priv->virq_target_cpu[d->irq];
	unsigned int cur_vector = priv->virq_vector[d->irq];
	int rc, vector;

	rc = lxic_get_cpu(priv, mask_val, force, &target_cpu);
	if (rc)
		return rc;

	if(target_cpu == cur_cpu)
		return IRQ_SET_MASK_OK;

	raw_spin_lock(&priv->map_lock);
	vector = lxic_vectors_alloc(priv, target_cpu, priv->nirq, get_count_order(1));
	if (vector < 0) {
		pr_info("lxic_MSI_set_affinity cannot alloc vector on cpu 0x%x", target_cpu);
		raw_spin_unlock(&priv->map_lock);
		return vector;
	}

	lxic_vectors_free(priv, cur_cpu, d->irq, cur_vector, 1);
	lxic_virq_set_target(priv, d->irq, vector, target_cpu);
	raw_spin_unlock(&priv->map_lock);

	irq_data_update_effective_affinity(d, cpumask_of(target_cpu));
	pr_info("lxic_MSI_set_affinity, virq: 0x%x, target vector: 0x%x, target cpu: 0x%x, \
current vector: 0x%x, current cpu: 0x%x ", d->irq, vector, target_cpu, cur_vector, cur_cpu);

	return IRQ_SET_MASK_OK;
}
#endif

static struct irq_chip lxic_chip = {
        .name		= "Linx INTC",
        .irq_mask	= lxic_irq_mask,
        .irq_unmask	= lxic_irq_unmask,
	.irq_eoi	= lxic_irq_eoi,
#ifdef CONFIG_SMP
	.irq_set_affinity = lxic_set_affinity,
#endif
};

static void lxic_set_type(uint32_t cpu, irq_hw_number_t vector,
			  uint32_t type)
{
	unsigned int value;
	struct lxic_handler *handler = NULL;

	handler = per_cpu_ptr(&lxic_handlers, cpu);
	value = (vector & 0xff) | ((type & 0xf) << 8);
	writel(value, handler->settype_base);
}

static int lxic_irq_domain_map(struct irq_domain *d, uint32_t virq,
				irq_hw_number_t hwirq)
{
	irq_domain_set_info(d, virq, hwirq, &lxic_chip, d->host_data,
			handle_fasteoi_irq, NULL, NULL);
	irq_set_noprobe(virq);

	return 0;
}

static void hartmask_to_cpumask(uint32_t hartmask, struct cpumask *mask)
{
	int i, cpu;

	memset((void*)mask, 0, sizeof(struct cpumask));

	for (i = 0; i < 32; i++) {
		if (hartmask & (1 << i)) {
			cpu = riscv_hartid_to_cpuid(i);
			cpumask_set_cpu(cpu, mask);
		}
	}
}

static int lxic_irq_domain_translate(struct irq_domain *d,
                                 struct irq_fwspec *fwspec,
                                 unsigned long *out_hwirq,
                                 unsigned int *out_type)
{
	struct cpumask cpumask;
	unsigned int cpu, vector, hartmask;

	if (WARN_ON(fwspec->param_count < 2))
		return -EINVAL;

	vector = fwspec->param[0];
	hartmask = fwspec->param[1] >> 4;
	hartmask_to_cpumask(hartmask, &cpumask);
	cpu = cpumask_first(&cpumask);

	*out_hwirq = vector | (cpu << 8);
	*out_type = fwspec->param[1] & IRQ_TYPE_SENSE_MASK;

	return 0;
}

static int lxic_irq_domain_alloc(struct irq_domain *domain, unsigned int virq,
		unsigned int nr_irqs, void *arg)
{
	int i, ret;
	irq_hw_number_t hwirq;
	unsigned int hartmask;
	struct cpumask cpumask;
	struct irq_fwspec *fwspec = arg;
	uint32_t cpu, vector, type;
	struct lxic_priv *priv = domain->host_data;
        struct lxic_handler *handler = NULL;

	ret = lxic_irq_domain_translate(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	hartmask = fwspec->param[1] >> 4;
	hartmask_to_cpumask(hartmask, &cpumask);
	vector = hwirq & 0xff;
	cpu = hwirq >> 8;

	for (i = 0; i < nr_irqs; i++) {
		ret = lxic_irq_domain_map(domain, virq + i, hwirq + i);
		if (ret)
			return ret;

		irq_data_update_effective_affinity(irq_get_irq_data(virq + i),
						   &cpumask);

		priv->wbi_msi_flag[cpu][vector + i] = true;
		handler = per_cpu_ptr(&lxic_handlers, cpu);
		raw_spin_lock(&handler->ids_lock);
		bitmap_set(handler->ids_used_bimap, vector + i, 1);
		raw_spin_unlock(&handler->ids_lock);
		lxic_set_type(cpu, vector + i, type);
	}

	return 0;
}

static const struct irq_domain_ops lxic_irqdomain_ops = {
	.translate = lxic_irq_domain_translate,
	.alloc = lxic_irq_domain_alloc,
	.free  = irq_domain_free_irqs_top,
};

static void lxic_send_ipi(const struct cpumask *target)
{
	uint32_t cpu;
	struct lxic_handler *handler;

	for_each_cpu(cpu, target) {
		handler = per_cpu_ptr(&lxic_handlers, cpu);
		writel(100, handler->ipi_base);
	}

}

const struct riscv_ipi_ops lxic_ipi_ops = {
	.ipi_inject = lxic_send_ipi,
};

void lxic_handle_irq(struct irq_desc *desc)
{
	int err;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct lxic_handler *handler = this_cpu_ptr(&lxic_handlers);
	struct lxic_priv *priv = handler->priv;
	irq_hw_number_t vector;
	unsigned int hwirq;
	uint32_t cpu = smp_processor_id();
	uint32_t virq = 0;

	WARN_ON_ONCE(!handler->present);

	chained_irq_enter(chip, desc);

	while (1) {
		vector = ssr_read(SSR_TOPEI) & INTID_MASK;
		if (vector >= LXIC_NIRQ)
			break;

		if (priv->wbi_msi_flag[cpu][vector]) {
			hwirq = vector | (cpu << 8);

			err = generic_handle_domain_irq(handler->priv->irqdomain, hwirq);
			if (unlikely(err))
				pr_warn_ratelimited("irqdomain can't find mapping for vector %lu on cpu %d\n", vector, cpu);
		} else {
			virq = handler->vector_virq[vector];
			if(!virq)
				pr_info("cannot find virq for vector 0x%lx on cpu 0x%x\n", vector, cpu);

#ifdef CONFIG_PCI
			err = generic_handle_domain_irq(handler->priv->base_domain, virq);
			if (unlikely(err))
				pr_warn_ratelimited("msi base_domain can't find mapping for vector %lu\n", vector);
#else
			pr_warn_ratelimited("msi vector %lu received with PCI disabled\n", vector);
			break;
#endif
		}
	}
	chained_irq_exit(chip, desc);
}

static void lxic_set_threshold(struct lxic_handler *handler, u32 threshold)
{
	writel(threshold, handler->threshold_base);
}

static int lxic_dying_cpu(unsigned int cpu)
{
	if (lxic_parent_irq)
		disable_percpu_irq(lxic_parent_irq);

	ssr_clear(SSR_A1_ECONFIG, BIT(ECONFIG_EXTERNAL));
	return 0;
}

static int lxic_starting_cpu(unsigned int cpu)
{
	struct lxic_handler *handler = this_cpu_ptr(&lxic_handlers);

	if (lxic_parent_irq)
		enable_percpu_irq(lxic_parent_irq,
				  irq_get_trigger_type(lxic_parent_irq));
	else
		pr_warn("cpu%d: parent irq not available\n", cpu);

	lxic_set_threshold(handler, LXIC_NIRQ);

	ssr_set(SSR_A1_ECONFIG, BIT(ECONFIG_EXTERNAL));
        return 0;
}

static int lxic_get_msi_msg(unsigned int cpu, unsigned int vector, struct msi_msg *msg)
{
	struct lxic_handler *handler = per_cpu_ptr(&lxic_handlers, cpu);
	phys_addr_t msi_addr = handler->msi_pa;

	msg->address_hi = upper_32_bits(msi_addr);
	msg->address_lo = lower_32_bits(msi_addr);
	msg->data = vector;

	return 0;
}

#ifdef CONFIG_PCI
static void lxic_irq_compose_msi_msg(struct irq_data *d,
				      struct msi_msg *msg)
{
	//irq_data should contain CPU ID to distinguish same vector for different LXIC
	struct lxic_priv *priv = irq_data_get_irq_chip_data(d);
	unsigned int cpu, vector;
	int err;
	
	cpu = lxic_virq_get_target(priv, d->irq, &vector);
	WARN_ON(cpu == UINT_MAX);

	err = lxic_get_msi_msg(cpu, vector, msg);
	WARN_ON(err);

	pr_info("lxic_irq_compose_msi_msg, cpu: 0x%x, addr_hi: 0x%x, addr_lo: 0x%x, data: 0x%x\n", cpu, msg->address_hi, msg->address_lo, msg->data);

	msi_msg_set_addr(irq_data_get_msi_desc(d), msg,
			 ((u64)msg->address_hi << 32) | msg->address_lo);
}

static struct irq_chip lxic_irq_base_chip = {
	.name			= "LINX INTC-BASE",
	.irq_mask		= lxic_irq_mask,
	.irq_unmask		= lxic_irq_unmask,
	.irq_eoi		= lxic_irq_eoi,
#ifdef CONFIG_SMP
	.irq_set_affinity	= lxic_MSI_set_affinity,
#endif
	.irq_compose_msi_msg = lxic_irq_compose_msi_msg,
};

static int lxic_irq_base_domain_alloc(struct irq_domain *domain,
				  unsigned int virq,
				  unsigned int nr_irqs,
				  void *args)
{
	struct lxic_priv *priv = domain->host_data;
	msi_alloc_info_t *info = args;
	struct lxic_handler *handler = NULL;
	phys_addr_t msi_addr;
	int i, err = 0, vector;
	unsigned int cpu;

	err = lxic_get_cpu(priv, &priv->lmask, false, &cpu);
	if (err)
		return err;

	handler = per_cpu_ptr(&lxic_handlers, cpu);
	msi_addr = handler->msi_pa;
	
	vector = lxic_vectors_alloc(priv, cpu, priv->nirq, get_count_order(nr_irqs));
	if (vector < 0)
		return vector;

	pr_info("virq: 0x%x, target cpu: 0x%x, msi_addr: 0x%llx, vector: 0x%x\n", virq, cpu, msi_addr, vector);

	err = iommu_dma_prepare_msi(info->desc, msi_addr);
	if (err)
		goto fail;

	for (i = 0; i < nr_irqs; i++) {
		lxic_virq_set_target(priv, virq + i, vector + i, cpu);
		irq_domain_set_info(domain, virq + i, virq + i,
				    &lxic_irq_base_chip, priv,
					handle_fasteoi_irq, NULL, NULL);
		irq_set_noprobe(virq + i);
	}

	return 0;

fail:
	lxic_vectors_free(priv, cpu, virq, vector, nr_irqs);
	return err;
}

static void lxic_irq_base_domain_free(struct irq_domain *domain,
				  unsigned int virq,
				  unsigned int nr_irqs)
{
	struct lxic_priv *priv = domain->host_data;
	unsigned int cpu =  priv->virq_target_cpu[virq];
	unsigned int vector = priv->virq_vector[virq];

	lxic_vectors_free(priv, cpu, virq, vector, nr_irqs);
	irq_domain_free_irqs_parent(domain, virq, nr_irqs);
}

static const struct irq_domain_ops lxic_base_domain_ops = {
	.alloc			= lxic_irq_base_domain_alloc,
	.free			= lxic_irq_base_domain_free,
};

static void lxic_pci_mask_irq(struct irq_data *d)
{
	pci_msi_mask_irq(d);
	irq_chip_mask_parent(d);
}

static void lxic_pci_unmask_irq(struct irq_data *d)
{
	pci_msi_unmask_irq(d);
	irq_chip_unmask_parent(d);
}

static struct irq_chip lxic_pci_irq_chip = {
	.name			= "LINX INTC-PCI",
	.irq_mask		= lxic_pci_mask_irq,
	.irq_unmask		= lxic_pci_unmask_irq,
	.irq_eoi		= irq_chip_eoi_parent,
};

static struct msi_domain_ops lxic_pci_domain_ops = {
};

static struct msi_domain_info lxic_pci_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		   MSI_FLAG_PCI_MSIX | MSI_FLAG_MULTI_PCI_MSI),
	.ops	= &lxic_pci_domain_ops,
	.chip	= &lxic_pci_irq_chip,
};

static int lxic_allocate_msi_domains(struct lxic_priv *priv, struct irq_domain *parent, struct fwnode_handle *fwnode)
{

	/* Create Base IRQ domain */
	priv->base_domain = irq_domain_create_tree(fwnode,
					      &lxic_base_domain_ops, priv);
	if (!priv->base_domain) {
		irq_domain_remove(priv->irqdomain);
		return -ENOMEM;
	}

	irq_domain_update_bus_token(priv->base_domain, DOMAIN_BUS_NEXUS);

	/* Create PCI MSI domain */
	priv->pci_domain = pci_msi_create_irq_domain(fwnode,
						&lxic_pci_domain_info,
						priv->base_domain);
	if (!priv->pci_domain) {
		pr_err("Failed to create LXIC PCI domain\n");
		irq_domain_remove(priv->base_domain);
		irq_domain_remove(priv->irqdomain);
		return -ENOMEM;
	}

	return 0;
}
#else
static int lxic_allocate_msi_domains(struct lxic_priv *priv, struct irq_domain *parent,
				     struct fwnode_handle *fwnode)
{
	return 0;
}
#endif

void irq_domain_cleanup(struct lxic_priv *priv)
{
#ifdef CONFIG_PCI
	irq_domain_remove(priv->pci_domain);
	irq_domain_remove(priv->base_domain);
#endif
	irq_domain_remove(priv->irqdomain);
}

static void setup_lxic(struct device_node *node, u32 *nvec, u32 *ndev,
		       struct lxic_priv *priv)
{
	int interrupts, i, cpu, hart;
	struct of_phandle_args parent;
	struct lxic_handler *handler;
	bool alloc_failed = false;

	lxic_parent_irq = 0;
	*nvec = 0;
	*ndev = 0;
	/* interrupts-extended= <xxx xxx ...>
	 * odd:  phandle of <riscv,cpu-intc> node
	 * even: hwirq in <riscv,cpu-intc> node
	 */
	interrupts = of_irq_count(node);
	WARN_ON(!interrupts);

	for (i = 0; i < interrupts; i++) {
		if (of_irq_parse_one(node, i, &parent)) {
			pr_err("failed to parse parent for interrupts-extended<%d>\n", i);
			continue;
		}

		hart = riscv_of_parent_hartid(parent.np);
		if (hart < 0) {
			pr_warn("failed to parse hart ID for context %d.\n", i);
			continue;
		}

		cpu = riscv_hartid_to_cpuid(hart);
		if (cpu < 0) {
			pr_warn("Invalid cpuid for context %d\n", i);
			continue;
		}
		cpumask_set_cpu(cpu, &priv->lmask);

		if (parent.args[0] != ECAUSE_TRAPNUM_ACR1_EXT_INT)
			continue;

		if (!lxic_parent_irq && parent.args[0] == ECAUSE_TRAPNUM_ACR1_EXT_INT &&irq_find_host(parent.np)) {
			lxic_parent_irq = irq_of_parse_and_map(node, i);
			irq_set_chained_handler(lxic_parent_irq, lxic_handle_irq);
		}

		handler = per_cpu_ptr(&lxic_handlers, cpu);
		if (handler->present) {
			pr_warn("handler already present for context %d.\n", i);
			lxic_set_threshold(handler, 0);
			continue;
		}
		handler->present = true;
		raw_spin_lock_init(&handler->enable_lock);
		handler->enable_base = priv->virt_base + hart * priv->hstride + DOM_ACR1 * priv->dstride + CTL_ENVEC_BASE;
		handler->disable_base = priv->virt_base + hart * priv->hstride + DOM_ACR1 * priv->dstride + CTL_DISVEC_BASE;
		handler->threshold_base = priv->virt_base + hart * priv->hstride + DOM_ACR1 * priv->dstride + CTL_THRESHOLD_BASE;
		handler->ipi_base = priv->virt_base + hart * priv->hstride + DOM_ACR1 * priv->dstride + CTL_IPI_BASE;
		handler->settype_base = priv->virt_base + hart * priv->hstride + DOM_ACR1 * priv->dstride + CTL_SETTYPE_BASE;
		handler->msi_pa = priv->phys_base + hart * priv->hstride + DOM_ACR1 * priv->dstride + CTL_SETVEC_BASE;
		pr_info("cpu: 0x%x, msi_pa: 0x%llx\n", cpu, handler->msi_pa);

		/* Initialize interrupt identity management */
		raw_spin_lock_init(&handler->ids_lock);

		/* Allocate used bitmap */
		handler->ids_used_bimap = kcalloc(BITS_TO_LONGS(priv->nirq + 1),
					sizeof(unsigned long), GFP_KERNEL);
		if (!handler->ids_used_bimap) {
			alloc_failed = true;
			pr_err("%pOFP: failed to alloc ids_used_bimap\n", node);
		}

		handler->vector_virq = kcalloc((priv->nirq + 1),
					sizeof(unsigned int), GFP_KERNEL);
		if (!handler->vector_virq) {
			alloc_failed = true;
			pr_err("%pOFP: failed to alloc vector_virq\n", node);
		}

		handler->priv = priv;
	}

	*ndev = cpumask_weight(&priv->lmask);
	*nvec = interrupts;
	priv->ndev = *ndev;

	/*
	 * We can have multiple LXIC instances so setup cpuhp state only
	 * when context handler for current/boot CPU is present.
	 */
	handler = this_cpu_ptr(&lxic_handlers);
	if (handler->present && !lxic_cpuhp_setup_done) {
		cpuhp_setup_state(CPUHP_AP_IRQ_LINX_INTC_STARTING,
				  "irqchip/linx/intc:starting",
				  lxic_starting_cpu, lxic_dying_cpu);
		lxic_cpuhp_setup_done = true;
	}

	if(alloc_failed) {
		for_each_possible_cpu(cpu) {
			handler = per_cpu_ptr(&lxic_handlers, cpu);
			if (handler->ids_used_bimap)
				kfree(handler->ids_used_bimap);
			if(handler->vector_virq)
				kfree(handler->vector_virq);
		}
	}

}

static void run_self_test(struct lxic_priv *priv)
{
	/* place self testing code here. */
	pr_info("***************LXIC******************\n");
	pr_info("virt_base is %px\n", priv->virt_base);
	pr_info("phys_base is %llx\n", priv->phys_base);
	pr_info("size is %llx\n", priv->size);
	pr_info("nirq is %x\n", priv->nirq);
	pr_info("ndev is %x\n", priv->ndev);
	pr_info("hart stride is %x\n", priv->hstride);
	pr_info("domain stride is %x\n", priv->dstride);
}

static int __init lxic_init(struct device_node *node,
		struct device_node *parent)
{
	struct lxic_priv *priv;
	struct resource res;
	u32 hstride, dstride, nirq, ndev, nvec;
        void __iomem *base;
        int ret, error = 0;
	int i, j;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	of_property_read_u32(node, "lxic,nirq", &nirq);
	WARN_ON(!nirq);

	of_property_read_u32(node, "lxic,stride", &hstride);
	if (WARN_ON(!hstride)) {
		error = -EINVAL;
		goto out_free_priv;
	}

	of_property_read_u32(node, "lxic,domain-stride", &dstride);
	if (WARN_ON(!dstride)) {
		error = -EINVAL;
		goto out_free_priv;
	}

	/* reg = <xxxx xxxx xxxx xxxx>
	 * odd pair:  lxic mmio base
	 * even pair: lxic mmio size
	 */
	ret = of_address_to_resource(node, 0, &res);
	base = of_iomap(node, 0);
	if (ret || !base) {
		pr_err("%pOF: unable to map lxic region\n", node);
		error = -ENODEV;
		goto out_free_priv;
	}

        priv->virt_base = base;
        priv->phys_base = res.start;
        priv->size = res.end - res.start + 1;
	priv->hstride = hstride;
	priv->dstride = dstride;
	priv->nirq = nirq;

	setup_lxic(node, &nvec, &ndev, priv);

	pr_info("%pOFP [%llx:%llx]: mapped %d interrupts with %d vectors for"
		" %d Core.\n", node, res.start, res.end, nirq, nvec, ndev);

	priv->irqdomain = irq_domain_add_linear(node, nirq,
			&lxic_irqdomain_ops, priv);
	if (WARN_ON(!priv->irqdomain))
		goto out_iounmap;

	irq_domain_update_bus_token(priv->irqdomain, DOMAIN_BUS_WIRED);

	ret = lxic_allocate_msi_domains(priv, priv->irqdomain, &node->fwnode);
	if (ret)
		goto out_iounmap;

	for (i = 0; i < MAX_CPU_NUM; i++)
		for (j = 0; j < FDT_LXIC_NIRQ; j++)
			priv->wbi_msi_flag[i][j] = false;

	/* Allocate target CPU array */
	priv->virq_target_cpu = kcalloc(MAX_VIRQ,
				       sizeof(unsigned int), GFP_KERNEL);
	if (!priv->virq_target_cpu) {
		pr_info("virq_target_cpu allocate failed!\n");
		error = -ENOMEM;
		goto out_domain_cleanup;
	}

	priv->virq_vector = kcalloc(MAX_VIRQ,
				       sizeof(unsigned int), GFP_KERNEL);
	if (!priv->virq_vector) {
		pr_info("virq_vector allocate failed!\n");
		error = -ENOMEM;
		goto out_free_virq_cpu;
	}
	for (i = 0; i < MAX_VIRQ; i++) {
		priv->virq_target_cpu[i] = UINT_MAX;
		priv->virq_vector[i] = UINT_MAX;
	}

	raw_spin_lock_init(&priv->map_lock);
	raw_spin_lock_init(&priv->set_target_lock);
	run_self_test(priv);

	return 0;

out_free_virq_cpu:
	kfree(priv->virq_target_cpu);
out_domain_cleanup:
	irq_domain_cleanup(priv);
out_iounmap:
	iounmap(priv->virt_base);
out_free_priv:
	kfree(priv);
	return error;
}

IRQCHIP_DECLARE(huawei_lxintc, "huawei,lxic", lxic_init);
