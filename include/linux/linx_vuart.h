/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_LINX_VUART_H
#define _LINUX_LINX_VUART_H

#include <linux/init.h>
#include <linux/types.h>

struct tty_struct;

#if defined(CONFIG_LINX_INTC) && defined(CONFIG_SERIAL_LINX_VIRT_UART)
int __init linx_vuart_bringup_init(void);
bool linx_vuart_tty_is_active(struct tty_struct *tty);
unsigned int linx_vuart_poll_tty_rx(struct tty_struct *tty);
int linx_vuart_read_tty_char(struct tty_struct *tty, u8 *out);
void linx_vuart_debug_putc(char c);
#else
static inline void linx_vuart_debug_putc(char c)
{
	(void)c;
}

static inline bool linx_vuart_tty_is_active(struct tty_struct *tty)
{
	(void)tty;
	return false;
}

static inline unsigned int linx_vuart_poll_tty_rx(struct tty_struct *tty)
{
	(void)tty;
	return 0;
}

static inline int linx_vuart_read_tty_char(struct tty_struct *tty, u8 *out)
{
	(void)tty;
	(void)out;
	return 0;
}
#endif

#endif /* _LINUX_LINX_VUART_H */
