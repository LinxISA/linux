/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_DEBUG_UART_H
#define _ASM_LINX_DEBUG_UART_H

#include <linux/linx_vuart.h>

static inline void linx_debug_uart_putc(char c)
{
	linx_vuart_debug_putc(c);
}

static inline void linx_debug_uart_puts(const char *s)
{
	while (*s)
		linx_debug_uart_putc(*s++);
}

static inline void linx_debug_uart_puthex_ulong(unsigned long v)
{
	static const char hexdigits[] = "0123456789abcdef";
	int i;

	for (i = (int)(sizeof(v) * 2) - 1; i >= 0; i--)
		linx_debug_uart_putc(hexdigits[(v >> (i * 4)) & 0xf]);
}

#endif /* _ASM_LINX_DEBUG_UART_H */
