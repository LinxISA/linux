// SPDX-License-Identifier: GPL-2.0-only
/*
 * LinxISA QEMU "virt" UART
 *
 * Device tree:
 *   compatible = "linx,virt-uart"
 *
 * MMIO register layout:
 *   0x0: DATA   (write: TX byte, read: RX byte)
 *   0x4: STATUS (bit0: TX_READY, bit1: RX_READY)
 */

#include <linux/console.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/timer.h>
#include <linux/tty_flip.h>

#include <asm/debug_uart.h>

#define LINX_VUART_DATA_REG	0x0
#define LINX_VUART_STATUS_REG	0x4

#define LINX_VUART_STATUS_TX_READY	0x1
#define LINX_VUART_STATUS_RX_READY	0x2

struct linx_vuart_port {
	struct uart_port port;
	struct timer_list poll_timer;
	bool poll_enabled;
};

static struct uart_driver linx_vuart_uart_driver;
static struct linx_vuart_port *linx_vuart_ports[1];
static struct linx_vuart_port linx_vuart_port_storage[1];
static bool linx_vuart_port_inuse[1];

static inline u32 linx_vuart_read_status(struct uart_port *port)
{
	return readl(port->membase + LINX_VUART_STATUS_REG);
}

static inline u8 linx_vuart_read_data(struct uart_port *port)
{
	return readb(port->membase + LINX_VUART_DATA_REG);
}

static inline void linx_vuart_write_data(struct uart_port *port, u8 v)
{
	writeb(v, port->membase + LINX_VUART_DATA_REG);
}

static void linx_vuart_rx_poll(struct linx_vuart_port *lport)
{
	struct uart_port *port = &lport->port;
	unsigned long flags;
	int pushed = 0;

	spin_lock_irqsave(&port->lock, flags);
	if (!port->state || !READ_ONCE(lport->poll_enabled)) {
		spin_unlock_irqrestore(&port->lock, flags);
		return;
	}

	while (linx_vuart_read_status(port) & LINX_VUART_STATUS_RX_READY) {
		u8 ch = linx_vuart_read_data(port);

		port->icount.rx++;
		if (uart_handle_sysrq_char(port, ch))
			continue;
		uart_insert_char(port, 0, 0, ch, TTY_NORMAL);
		pushed = 1;
	}

	if (pushed)
		tty_flip_buffer_push(&port->state->port);
	spin_unlock_irqrestore(&port->lock, flags);
}

static void linx_vuart_poll_timer(struct timer_list *t)
{
	struct linx_vuart_port *lport = timer_container_of(lport, t, poll_timer);

	linx_vuart_rx_poll(lport);

	if (READ_ONCE(lport->poll_enabled))
		mod_timer(&lport->poll_timer, jiffies + msecs_to_jiffies(10));
}

static unsigned int linx_vuart_tx_empty(struct uart_port *port)
{
	return (linx_vuart_read_status(port) & LINX_VUART_STATUS_TX_READY) ?
		       TIOCSER_TEMT : 0;
}

static void linx_vuart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	(void)port;
	(void)mctrl;
}

static unsigned int linx_vuart_get_mctrl(struct uart_port *port)
{
	(void)port;
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void linx_vuart_stop_tx(struct uart_port *port)
{
	(void)port;
}

static void linx_vuart_start_tx(struct uart_port *port)
{
	unsigned char ch;

	if (!port->state)
		return;

	uart_port_tx(port, ch,
		     (linx_vuart_read_status(port) & LINX_VUART_STATUS_TX_READY),
		     linx_vuart_write_data(port, ch));
}

static void linx_vuart_stop_rx(struct uart_port *port)
{
	(void)port;
}

static void linx_vuart_break_ctl(struct uart_port *port, int break_state)
{
	(void)port;
	(void)break_state;
}

static int linx_vuart_startup(struct uart_port *port)
{
	struct linx_vuart_port *lport = container_of(port, struct linx_vuart_port, port);

	linx_debug_uart_putc('P');
	linx_debug_uart_puthex_ulong((unsigned long)port);
	linx_debug_uart_putc('O');
	linx_debug_uart_puthex_ulong((unsigned long)port->ops);
	linx_debug_uart_putc('S');
	linx_debug_uart_puthex_ulong((unsigned long)(port->ops ? port->ops->startup : NULL));
	linx_debug_uart_putc('\n');

	WRITE_ONCE(lport->poll_enabled, true);
	mod_timer(&lport->poll_timer, jiffies + msecs_to_jiffies(10));
	return 0;
}

static void linx_vuart_shutdown(struct uart_port *port)
{
	struct linx_vuart_port *lport = container_of(port, struct linx_vuart_port, port);

	WRITE_ONCE(lport->poll_enabled, false);
	timer_delete_sync(&lport->poll_timer);
}

static void linx_vuart_set_termios(struct uart_port *port, struct ktermios *new,
				   const struct ktermios *old)
{
	unsigned int baud;

	baud = uart_get_baud_rate(port, new, old, 1200, 115200);
	uart_update_timeout(port, new->c_cflag, baud);
}

static const char *linx_vuart_type(struct uart_port *port)
{
	(void)port;
	return "linx-virt-uart";
}

static void linx_vuart_release_port(struct uart_port *port)
{
	(void)port;
}

static int linx_vuart_request_port(struct uart_port *port)
{
	(void)port;
	return 0;
}

static void linx_vuart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_16550;
}

static int linx_vuart_verify_port(struct uart_port *port,
				  struct serial_struct *ser)
{
	(void)port;
	(void)ser;
	return 0;
}

static const struct uart_ops linx_vuart_uart_ops = {
	.tx_empty = linx_vuart_tx_empty,
	.set_mctrl = linx_vuart_set_mctrl,
	.get_mctrl = linx_vuart_get_mctrl,
	.stop_tx = linx_vuart_stop_tx,
	.start_tx = linx_vuart_start_tx,
	.stop_rx = linx_vuart_stop_rx,
	.break_ctl = linx_vuart_break_ctl,
	.startup = linx_vuart_startup,
	.shutdown = linx_vuart_shutdown,
	.set_termios = linx_vuart_set_termios,
	.type = linx_vuart_type,
	.release_port = linx_vuart_release_port,
	.request_port = linx_vuart_request_port,
	.config_port = linx_vuart_config_port,
	.verify_port = linx_vuart_verify_port,
};

#ifdef CONFIG_SERIAL_LINX_VIRT_UART_CONSOLE
static void linx_vuart_console_putchar(struct uart_port *port, unsigned char ch)
{
	while (!(linx_vuart_read_status(port) & LINX_VUART_STATUS_TX_READY))
		cpu_relax();
	linx_vuart_write_data(port, ch);
}

static void linx_vuart_console_write(struct console *co, const char *s,
				     unsigned int count)
{
	struct linx_vuart_port *lport;
	struct uart_port *port;
	unsigned long flags;

	lport = linx_vuart_ports[co->index];
	if (!lport)
		return;

	port = &lport->port;

	uart_port_lock_irqsave(port, &flags);
	uart_console_write(port, s, count, linx_vuart_console_putchar);
	uart_port_unlock_irqrestore(port, flags);
}

static int __init linx_vuart_console_setup(struct console *co, char *options)
{
	struct linx_vuart_port *lport;
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	lport = linx_vuart_ports[co->index];
	if (!lport)
		return 0;

	port = &lport->port;
	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static struct console linx_vuart_console = {
	.name = "ttyS",
	.write = linx_vuart_console_write,
	.device = uart_console_device,
	.setup = linx_vuart_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &linx_vuart_uart_driver,
};
#endif

static int linx_vuart_probe(struct platform_device *pdev)
{
	struct linx_vuart_port *lport;
	struct resource *res;
	int id;
	int rc;

	id = of_alias_get_id(pdev->dev.of_node, "serial");
	if (id < 0)
		id = 0;
	if (id >= linx_vuart_uart_driver.nr)
		return -EINVAL;
	if (linx_vuart_port_inuse[id])
		return -EBUSY;

	lport = &linx_vuart_port_storage[id];
	memset(lport, 0, sizeof(*lport));
	linx_vuart_port_inuse[id] = true;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	lport->port.mapbase = res->start;
	lport->port.membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(lport->port.membase)) {
		linx_vuart_port_inuse[id] = false;
		return PTR_ERR(lport->port.membase);
	}

	lport->port.dev = &pdev->dev;
	lport->port.iotype = UPIO_MEM;
	lport->port.fifosize = 1;
	lport->port.ops = &linx_vuart_uart_ops;
	lport->port.flags = UPF_BOOT_AUTOCONF;
	lport->port.line = id;

	linx_debug_uart_putc('U');
	linx_debug_uart_puthex_ulong((unsigned long)&lport->port);
	linx_debug_uart_putc('O');
	linx_debug_uart_puthex_ulong((unsigned long)lport->port.ops);
	linx_debug_uart_putc('S');
	linx_debug_uart_puthex_ulong((unsigned long)(lport->port.ops ?
				      lport->port.ops->startup : NULL));
	linx_debug_uart_putc('\n');

	pr_err("linx-vuart: probe port=%px ops=%px startup=%px mapbase=0x%llx membase=%px line=%d\n",
	       &lport->port, lport->port.ops,
	       lport->port.ops ? lport->port.ops->startup : NULL,
	       (unsigned long long)lport->port.mapbase,
	       lport->port.membase, lport->port.line);

	timer_setup(&lport->poll_timer, linx_vuart_poll_timer, 0);
	lport->poll_enabled = false;

	rc = uart_add_one_port(&linx_vuart_uart_driver, &lport->port);
	if (rc) {
		linx_debug_uart_putc('u');
		linx_debug_uart_puthex_ulong((unsigned long)rc);
		linx_debug_uart_putc('\n');
		linx_vuart_port_inuse[id] = false;
		return rc;
	}

	platform_set_drvdata(pdev, lport);
	linx_vuart_ports[id] = lport;

	return 0;
}

static void linx_vuart_remove(struct platform_device *pdev)
{
	struct linx_vuart_port *lport = platform_get_drvdata(pdev);

	linx_debug_uart_putc('X');
	linx_debug_uart_puthex_ulong((unsigned long)lport);
	linx_debug_uart_putc('\n');

	uart_remove_one_port(&linx_vuart_uart_driver, &lport->port);
	if (lport->port.line >= 0 && lport->port.line < ARRAY_SIZE(linx_vuart_ports) &&
	    linx_vuart_ports[lport->port.line] == lport)
		linx_vuart_ports[lport->port.line] = NULL;
	if (lport->port.line >= 0 && lport->port.line < ARRAY_SIZE(linx_vuart_port_inuse))
		linx_vuart_port_inuse[lport->port.line] = false;
}

static const struct of_device_id linx_vuart_of_match[] = {
	{ .compatible = "linx,virt-uart" },
	{ }
};
MODULE_DEVICE_TABLE(of, linx_vuart_of_match);

static struct platform_driver linx_vuart_platform_driver = {
	.driver = {
		.name = "linx-virt-uart",
		.of_match_table = linx_vuart_of_match,
	},
	.probe = linx_vuart_probe,
	.remove = linx_vuart_remove,
};

static struct uart_driver linx_vuart_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "linx-virt-uart",
	.dev_name = "ttyS",
	.major = TTY_MAJOR,
	.minor = 64,
	.nr = 1,
#ifdef CONFIG_SERIAL_LINX_VIRT_UART_CONSOLE
	.cons = &linx_vuart_console,
#endif
};

static int __init linx_vuart_init(void)
{
	int rc;

	rc = uart_register_driver(&linx_vuart_uart_driver);
	if (rc)
		return rc;

#ifdef CONFIG_SERIAL_LINX_VIRT_UART_CONSOLE
	register_console(&linx_vuart_console);
#endif

	rc = platform_driver_register(&linx_vuart_platform_driver);
	if (rc) {
#ifdef CONFIG_SERIAL_LINX_VIRT_UART_CONSOLE
		unregister_console(&linx_vuart_console);
#endif
		uart_unregister_driver(&linx_vuart_uart_driver);
		return rc;
	}

	return 0;
}

static void __exit linx_vuart_exit(void)
{
	platform_driver_unregister(&linx_vuart_platform_driver);
#ifdef CONFIG_SERIAL_LINX_VIRT_UART_CONSOLE
	unregister_console(&linx_vuart_console);
#endif
	uart_unregister_driver(&linx_vuart_uart_driver);
}

module_init(linx_vuart_init);
module_exit(linx_vuart_exit);

MODULE_DESCRIPTION("LinxISA QEMU virt UART driver");
MODULE_LICENSE("GPL");
