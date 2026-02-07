// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>

#include <asm/debug_uart.h>

#define LINX_VIRT_UART_BASE 0x10000000UL

void linx_debug_uart_putc(char c)
{
	*(volatile unsigned char *)(LINX_VIRT_UART_BASE + 0x0) =
		(unsigned char)c;
}

void linx_debug_uart_puts(const char *s)
{
	for (; *s; ++s)
		linx_debug_uart_putc(*s);
}

void linx_debug_uart_puthex_ulong(unsigned long v)
{
	static const char hexdigits[] = "0123456789abcdef";
	int i;

	linx_debug_uart_putc('0');
	linx_debug_uart_putc('x');
	for (i = (int)(sizeof(v) * 2) - 1; i >= 0; i--) {
		unsigned int nibble = (v >> (i * 4)) & 0xf;

		linx_debug_uart_putc(hexdigits[nibble]);
	}
}
