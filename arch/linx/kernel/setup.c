// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/console.h>
#include <linux/cpumask.h>
#include <linux/initrd.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/of_fdt.h>
#include <linux/printk.h>
#include <linux/seq_file.h>

#include <asm/page.h>
#include <asm/pgtable.h>

extern void *linx_dtb_early_va;
extern phys_addr_t linx_dtb_early_pa;

extern char _stext[], _etext[], _edata[], _end[];

#define LINX_VIRT_UART_BASE 0x10000000UL
#define LINX_CPUINFO_ISA_PROFILE "linx64"
#define LINX_CPUINFO_ISA_EXTENSIONS                                           \
	"lnx-s32 lnx-s64 lnx-c lnx-f lnx-a lnx-sys lnx-v lnx-m"

static inline void linx_virt_uart_putc(char c)
{
	*(volatile unsigned char *)(LINX_VIRT_UART_BASE + 0x0) =
		(unsigned char)c;
}

static void linx_virt_console_write(struct console *con, const char *s,
				    unsigned int count)
{
	(void)con;
	while (count--) {
		char c = *s++;

		if (c == '\n')
			linx_virt_uart_putc('\r');
		linx_virt_uart_putc(c);
	}
}

static struct console linx_virt_console = {
	.name = "ttyLINX",
	.write = linx_virt_console_write,
	.flags = CON_PRINTBUFFER,
	.index = -1,
};

static void __init linx_register_console(void)
{
	/*
	 * LinxISA bring-up: prefer the virt UART for the kernel console even if
	 * the device tree provides a different default.
	 */
	(void)add_preferred_console("ttyLINX", 0, NULL);
	register_console(&linx_virt_console);
}

static void __init parse_dtb(void)
{
	if (!linx_dtb_early_va) {
		goto no_dtb;
	}

	if (early_init_dt_scan(linx_dtb_early_va, linx_dtb_early_pa)) {
		const char *name = of_flat_dt_get_machine_name();

		if (name)
			pr_info("Machine model: %s\n", name);
		return;
	}

no_dtb:
	pr_err("No DTB passed to the kernel\n");
}

void __init setup_arch(char **cmdline_p)
{
	/*
	 * QEMU LinxISA `virt` bring-up uses a single hart. Make sure CPU0 is marked
	 * possible/present so core code computes nr_cpu_ids correctly.
	 */
	set_cpu_possible(0, true);
	set_cpu_present(0, true);

	linx_register_console();
	parse_dtb();

	/*
	 * Allow memblock to grow its region arrays. Early bring-up may reserve more
	 * regions than the initial static tables can hold, and failing early here
	 * makes debugging much harder.
	 */
	memblock_allow_resize();

	/*
	 * Reserve the external initrd/initramfs (from DTB /chosen) so early
	 * memblock allocations don't overwrite it before wait_for_initramfs().
	 */
	reserve_initrd_mem();

	early_init_fdt_reserve_self();
	memblock_reserve(__pa(_stext), (phys_addr_t)(_end - _stext));
	early_init_fdt_scan_reserved_mem();
	setup_initial_init_mm(_stext, _etext, _edata, _end);

	if (cmdline_p)
		*cmdline_p = boot_command_line;

	unflatten_device_tree();

	/*
	 * Initialize the Linx paging/MMU and the buddy allocator data structures.
	 *
	 * Must run before core mm initialization (mm_core_init -> memblock_free_all)
	 * so that zones, struct page metadata, and the direct map are ready.
	 */
	paging_init();

}

static int show_cpuinfo(struct seq_file *m, void *v)
{
	(void)v;
	seq_puts(m, "processor\t: 0\n");
	seq_printf(m, "isa\t\t: %s\n", LINX_CPUINFO_ISA_PROFILE);
	seq_printf(m, "isa_extensions\t: %s\n", LINX_CPUINFO_ISA_EXTENSIONS);
	seq_puts(m, "\n");
	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	(void)m;
	if (*pos == 0)
		return (void *)1;
	return NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	(void)v;
	(*pos)++;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
	(void)m;
	(void)v;
}

const struct seq_operations cpuinfo_op = {
	.start = c_start,
	.next  = c_next,
	.stop  = c_stop,
	.show  = show_cpuinfo,
};
