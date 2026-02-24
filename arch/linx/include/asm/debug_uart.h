/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_DEBUG_UART_H
#define _ASM_LINX_DEBUG_UART_H

/*
 * QEMU LinxISA `virt` debug UART helpers.
 *
 * Keep these simple and safe to call from early/fragile bring-up code paths
 * (including allocator debugging) where printk() may not be reliable.
 */

#ifdef CONFIG_LINX_DEBUG
void linx_debug_uart_putc(char c);
void linx_debug_uart_puts(const char *s);
void linx_debug_uart_puthex_ulong(unsigned long v);
#else
static inline void linx_debug_uart_putc(char c)
{
	(void)c;
}

static inline void linx_debug_uart_puts(const char *s)
{
	(void)s;
}

static inline void linx_debug_uart_puthex_ulong(unsigned long v)
{
	(void)v;
}
#endif

#endif /* _ASM_LINX_DEBUG_UART_H */
