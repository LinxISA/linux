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

static inline void linx_virt_uart_putc(char c)
{
	*(volatile unsigned char *)(LINX_VIRT_UART_BASE + 0x0) =
		(unsigned char)c;
}

static inline void linx_virt_uart_puthex_u64(unsigned long v)
{
	static const char hexdigits[] = "0123456789abcdef";
	int i;

	for (i = (int)(sizeof(v) * 2) - 1; i >= 0; i--) {
		unsigned int nibble = (v >> (i * 4)) & 0xf;

		linx_virt_uart_putc(hexdigits[nibble]);
	}
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
	register_console(&linx_virt_console);
}

static void __init parse_dtb(void)
{
	linx_virt_uart_putc('D');
	if (!linx_dtb_early_va) {
		linx_virt_uart_putc('0');
		goto no_dtb;
	}

	linx_virt_uart_putc('1');
	if (early_init_dt_scan(linx_dtb_early_va, linx_dtb_early_pa)) {
		const char *name = of_flat_dt_get_machine_name();

		linx_virt_uart_putc('T');
		if (name)
			pr_info("Machine model: %s\n", name);
		return;
	}

	linx_virt_uart_putc('F');
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

	linx_virt_uart_putc('s');
	linx_register_console();
	linx_virt_uart_putc('a');
	parse_dtb();
	linx_virt_uart_putc('b');

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

	linx_virt_uart_putc('M');
	linx_virt_uart_puthex_u64(memblock.memory.cnt);
	linx_virt_uart_putc(',');
	linx_virt_uart_puthex_u64(memblock.memory.total_size);
	if (memblock.memory.cnt) {
		linx_virt_uart_putc('B');
		linx_virt_uart_puthex_u64(memblock.memory.regions[0].base);
		linx_virt_uart_putc(',');
		linx_virt_uart_puthex_u64(memblock.memory.regions[0].size);
		linx_virt_uart_putc('b');
	}
	linx_virt_uart_putc('m');

	linx_virt_uart_putc('c');
	early_init_fdt_reserve_self();
	linx_virt_uart_putc('d');
	memblock_reserve(__pa(_stext), (phys_addr_t)(_end - _stext));
	linx_virt_uart_putc('e');
	early_init_fdt_scan_reserved_mem();
	linx_virt_uart_putc('f');

		linx_virt_uart_putc('g');
		setup_initial_init_mm(_stext, _etext, _edata, _end);
		linx_virt_uart_putc('h');

		if (cmdline_p)
			*cmdline_p = boot_command_line;

	linx_virt_uart_putc('i');
	unflatten_device_tree();
	linx_virt_uart_putc('j');

	/*
	 * Initialize the Linx paging/MMU and the buddy allocator data structures.
	 *
	 * Must run before core mm initialization (mm_core_init -> memblock_free_all)
	 * so that zones, struct page metadata, and the direct map are ready.
	 */
	paging_init();

	linx_virt_uart_putc('\n');
}

static int show_cpuinfo(struct seq_file *m, void *v)
{
	(void)v;
	seq_puts(m, "processor\t: 0\n");
	seq_puts(m, "isa\t\t: LinxISA\n");
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
