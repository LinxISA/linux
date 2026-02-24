// SPDX-License-Identifier: GPL-2.0-only

#include <linux/sched/task.h>
#include <linux/start_kernel.h>
#include <linux/types.h>

#include <asm/ssr.h>
#include <asm/thread_info.h>

#define LINX_VIRT_UART_BASE 0x10000000UL

void *linx_dtb_early_va;
phys_addr_t linx_dtb_early_pa;
unsigned long linx_boot_hartid;

asmlinkage void __noreturn _start(unsigned long hartid, void *fdt);

extern void linx_trap_vector(void);
extern void linx_trap_vector_acr0(void);

/* BSS bounds (provided by the linker script). */
extern char __bss_start[];
extern char __bss_stop[];

#ifdef CONFIG_LINX_VIRT_UART_MARKERS
static inline void linx_virt_uart_putc(char c)
{
	*(volatile unsigned char *)(LINX_VIRT_UART_BASE + 0x0) = (unsigned char)c;
}

static inline void linx_virt_uart_puts(const char *s)
{
	for (; *s; ++s)
		linx_virt_uart_putc(*s);
}

static inline void linx_virt_uart_puthex_u64(unsigned long v)
{
	static const char hexdigits[] = "0123456789abcdef";
	int i;

	linx_virt_uart_puts("0x");
	for (i = (int)(sizeof(v) * 2) - 1; i >= 0; i--) {
		unsigned int nibble = (v >> (i * 4)) & 0xf;

		linx_virt_uart_putc(hexdigits[nibble]);
	}
}
#endif

/*
 * QEMU LinxISA `virt` enters the kernel at `_start` with:
 *   a0: hartid, a1: fdt pointer, sp: valid stack, ra: exit trampoline.
 *
 * For now, just emit a visible marker on the UART and tail-call into
 * start_kernel(). Real bring-up should move this to an assembly head.S that
 * sets up the full execution environment (traps/MMU/early DT parsing).
 */
__attribute__((section(".head.text"), used))
asmlinkage void __noreturn _start(unsigned long hartid, void *fdt)
{
	unsigned long new_sp;
	char *p;

	/*
	 * QEMU's LinxISA loader does not guarantee BSS is cleared for us.
	 * Clear it before any subsystem touches BSS-resident state (e.g. init_net).
	 */
	for (p = __bss_start; p < __bss_stop; ++p)
		*p = 0;

	linx_boot_hartid = hartid;
	linx_dtb_early_va = fdt;
	linx_dtb_early_pa = (phys_addr_t)(unsigned long)fdt;

#ifdef CONFIG_LINX_VIRT_UART_MARKERS
	{
		unsigned int fdt_magic = 0;

		if (fdt)
			fdt_magic = *(volatile unsigned int *)fdt;

		linx_virt_uart_puts("\n[LinxISA] _start -> start_kernel()\n");
		linx_virt_uart_puts("[LinxISA] hartid=");
		linx_virt_uart_puthex_u64(hartid);
		linx_virt_uart_puts(" fdt=");
		linx_virt_uart_puthex_u64((unsigned long)fdt);
		linx_virt_uart_puts(" magic=");
		linx_virt_uart_puthex_u64(fdt_magic);
		linx_virt_uart_puts("\n");
	}
#endif

	/*
	 * QEMU sets up a temporary stack near the end of RAM. Switch to the
	 * kernel's init stack so current_thread_info()/current works correctly.
	 */
	new_sp = (unsigned long)&init_thread_union + THREAD_SIZE;
	asm volatile("c.movr %0, ->sp" : : "r"(new_sp) : "memory");

	/*
	 * Install trap vectors and start with interrupts disabled.
	 *
	 * Bring-up alignment:
	 * - ACR1 is the Linux kernel ring (interrupts + syscalls route here).
	 * - ACR0 is reserved for monitor/secure services; keep a minimal handler.
	 */
	linx_ssr_write_evbase_acr0((unsigned long)&linx_trap_vector_acr0);
	linx_ssr_write_evbase_acr1((unsigned long)&linx_trap_vector);
	linx_ssr_write_etemp0_acr1(new_sp);
	{
		unsigned long cstate = linx_ssr_read_cstate();

		/* Disable interrupts and switch into the kernel ring (ACR1). */
		cstate &= ~LINX_CSTATE_I_BIT;
		cstate = (cstate & ~0xfull) | 1ul;
		linx_ssr_write_cstate(cstate);
	}

	start_kernel();
}
