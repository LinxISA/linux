/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_LINX_VUART_H
#define _LINUX_LINX_VUART_H

#include <linux/init.h>

#if defined(CONFIG_LINX_INTC) && defined(CONFIG_SERIAL_LINX_VIRT_UART)
int __init linx_vuart_bringup_init(void);
#endif

#endif /* _LINUX_LINX_VUART_H */
