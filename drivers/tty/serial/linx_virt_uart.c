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
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/linx_vuart.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/timer.h>
#include <linux/tty_flip.h>
#include <asm/early_ioremap.h>

#define LINX_VUART_DATA_REG	0x0
#define LINX_VUART_STATUS_REG	0x4
#define LINX_VUART_BASE		0x10000000UL
#define LINX_VUART_SIZE		0x1000UL

#define LINX_VUART_STATUS_TX_READY	0x1
#define LINX_VUART_STATUS_RX_READY	0x2
#define LINX_VUART_RX_BUFSZ		256

struct linx_vuart_port {
	struct uart_port port;
	struct timer_list poll_timer;
	bool poll_enabled;
};

static struct uart_driver linx_vuart_uart_driver;
static struct linx_vuart_port *linx_vuart_ports[1];
static struct linx_vuart_port linx_vuart_port_storage[1];
static bool linx_vuart_port_inuse[1];
static void __iomem *linx_vuart_early_membase;

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

void linx_vuart_debug_putc(char c)
{
	struct linx_vuart_port *lport = READ_ONCE(linx_vuart_ports[0]);
	struct uart_port *port;

	if (!lport)
		return;

	port = &lport->port;
	if (!port->membase)
		return;

	linx_vuart_write_data(port, (u8)c);
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

	for (;;) {
		u32 status = linx_vuart_read_status(port);
		u8 ch;

		if ((status & LINX_VUART_STATUS_RX_READY) ==
		    LINX_VUART_STATUS_RX_READY) {
			ch = linx_vuart_read_data(port);
		} else {
			break;
		}

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

bool linx_vuart_tty_is_active(struct tty_struct *tty)
{
	struct linx_vuart_port *lport;
	int line;

	if (!tty || tty->driver != linx_vuart_uart_driver.tty_driver)
		return false;

	line = tty->index;
	if (line < 0 || line >= ARRAY_SIZE(linx_vuart_ports))
		return false;

	lport = READ_ONCE(linx_vuart_ports[line]);
	return lport && lport->port.state && READ_ONCE(lport->poll_enabled);
}

int linx_vuart_read_tty_char(struct tty_struct *tty, u8 *out)
{
	struct linx_vuart_port *lport;
	struct uart_port *port;
	u8 ch;
	int line;

	if (!out || !tty || tty->driver != linx_vuart_uart_driver.tty_driver)
		return -ENODEV;

	line = tty->index;
	if (line < 0 || line >= ARRAY_SIZE(linx_vuart_ports))
		return -ENODEV;

	lport = READ_ONCE(linx_vuart_ports[line]);
	if (!lport || !lport->port.state || !READ_ONCE(lport->poll_enabled))
		return -ENODEV;

	port = &lport->port;
	ch = linx_vuart_read_data(port);
	if (!ch)
		return 0;

	port->icount.rx++;
	if (uart_handle_sysrq_char(port, ch))
		return 0;

	*out = ch;
	return 1;
}

unsigned int linx_vuart_poll_tty_rx(struct tty_struct *tty)
{
	struct linx_vuart_port *lport;
	struct uart_port *port;
	u8 buf[LINX_VUART_RX_BUFSZ];
	unsigned long flags;
	unsigned int count = 0;
	int line;

	if (!tty || tty->driver != linx_vuart_uart_driver.tty_driver)
		return 0;

	line = tty->index;
	if (line < 0 || line >= ARRAY_SIZE(linx_vuart_ports))
		return 0;

	lport = READ_ONCE(linx_vuart_ports[line]);
	if (!lport)
		return 0;

	port = &lport->port;
	spin_lock_irqsave(&port->lock, flags);
	if (!port->state || !READ_ONCE(lport->poll_enabled)) {
		spin_unlock_irqrestore(&port->lock, flags);
		return 0;
	}

	while (count < ARRAY_SIZE(buf)) {
		u32 status = linx_vuart_read_status(port);
		u8 ch;

		if ((status & LINX_VUART_STATUS_RX_READY) ==
		    LINX_VUART_STATUS_RX_READY) {
			ch = linx_vuart_read_data(port);
		} else {
			break;
		}

		port->icount.rx++;
		if (uart_handle_sysrq_char(port, ch))
			continue;
		buf[count++] = ch;
	}
	spin_unlock_irqrestore(&port->lock, flags);

	if (count && tty->ldisc)
		return tty_ldisc_receive_buf(tty->ldisc, buf, NULL, count);

	return count;
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
	struct tty_port *tport;
	unsigned char ch;

	if (!port->state)
		return;

	tport = &port->state->port;
	while (port->x_char || !kfifo_is_empty(&tport->xmit_fifo)) {
		if (!(linx_vuart_read_status(port) & LINX_VUART_STATUS_TX_READY))
			break;
		if (port->x_char) {
			ch = port->x_char;
			port->x_char = 0;
		} else if (!kfifo_get(&tport->xmit_fifo, &ch)) {
			break;
		}
		linx_vuart_write_data(port, ch);
		port->icount.tx++;
	}
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

	if (linx_vuart_read_status(port) & LINX_VUART_STATUS_TX_READY)
		linx_vuart_write_data(port, 'S');
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

	if (co->index < 0 || co->index >= ARRAY_SIZE(linx_vuart_ports))
		return;

	lport = linx_vuart_ports[co->index];
	if (!lport)
		return;

	port = &lport->port;

	uart_port_lock_irqsave(port, &flags);
	uart_console_write(port, s, count, linx_vuart_console_putchar);
	uart_port_unlock_irqrestore(port, flags);
}

static void linx_vuart_early_write(struct console *con, const char *s,
				   unsigned int count)
{
	struct earlycon_device *device = con->data;

	uart_console_write(&device->port, s, count, linx_vuart_console_putchar);
}

static int __init linx_vuart_early_setup(struct earlycon_device *device,
					 const char *options)
{
	if (!device->port.membase)
		return -ENODEV;

	linx_vuart_early_membase = device->port.membase;
	device->con->write = linx_vuart_early_write;
	return 0;
}

OF_EARLYCON_DECLARE(linx_vuart, "linx,virt-uart", linx_vuart_early_setup);

static int __init linx_vuart_console_setup(struct console *co, char *options)
{
	struct linx_vuart_port *lport;
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index < 0 || co->index >= ARRAY_SIZE(linx_vuart_ports))
		return -ENODEV;

	lport = linx_vuart_ports[co->index];
	if (!lport)
		return -ENODEV;

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

static int __init linx_vuart_console_init(void)
{
	register_console(&linx_vuart_console);
	return 0;
}
console_initcall(linx_vuart_console_init);

static int __init linx_vuart_late_console_init(void)
{
	if (!console_is_registered(&linx_vuart_console))
		register_console(&linx_vuart_console);
	return 0;
}
core_initcall(linx_vuart_late_console_init);
#endif

static int linx_vuart_probe(struct platform_device *pdev)
{
	struct linx_vuart_port *lport;
	struct resource *res;
	resource_size_t size;
	int id;
	int rc;

	pr_err("Linx dbg: linx_vuart_probe start\n");
	id = of_alias_get_id(pdev->dev.of_node, "serial");
	if (id < 0)
		id = 0;
	pr_err("Linx dbg: linx_vuart_probe id=%d\n", id);
	if (id >= linx_vuart_uart_driver.nr)
		return -EINVAL;
	if (linx_vuart_port_inuse[id])
		return -EBUSY;

	lport = &linx_vuart_port_storage[id];
	memset(lport, 0, sizeof(*lport));
	linx_vuart_port_inuse[id] = true;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		linx_vuart_port_inuse[id] = false;
		return -ENODEV;
	}
	pr_err("Linx dbg: linx_vuart_probe resource start=%pa end=%pa\n",
	       &res->start, &res->end);
	size = resource_size(res);
	lport->port.mapbase = res->start;
#ifdef CONFIG_MMU
	lport->port.membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(lport->port.membase)) {
		linx_vuart_port_inuse[id] = false;
		return PTR_ERR(lport->port.membase);
	}
#else
	if (!devm_request_mem_region(&pdev->dev, res->start, size,
				     dev_name(&pdev->dev))) {
		linx_vuart_port_inuse[id] = false;
		return -EBUSY;
	}
	lport->port.membase = (void __iomem *)(unsigned long)res->start;
#endif

	lport->port.dev = &pdev->dev;
	lport->port.iotype = UPIO_MEM;
	lport->port.fifosize = 1;
	lport->port.ops = &linx_vuart_uart_ops;
	lport->port.flags = UPF_BOOT_AUTOCONF;
	lport->port.line = id;
	pr_err("Linx dbg: linx_vuart_probe before tx-ready test membase=%px\n",
	       lport->port.membase);

	pr_err("Linx dbg: linx_vuart_probe before timer_setup\n");
	timer_setup(&lport->poll_timer, linx_vuart_poll_timer, 0);
	lport->poll_enabled = false;

	pr_err("Linx dbg: linx_vuart_probe before uart_add_one_port\n");
	linx_vuart_ports[id] = lport;
	rc = uart_add_one_port(&linx_vuart_uart_driver, &lport->port);
	pr_err("Linx dbg: linx_vuart_probe after uart_add_one_port rc=%d\n", rc);
	if (rc) {
		linx_vuart_ports[id] = NULL;
		linx_vuart_port_inuse[id] = false;
		return rc;
	}

	platform_set_drvdata(pdev, lport);
	pr_err("Linx dbg: linx_vuart_probe done\n");

	return 0;
}

static void linx_vuart_remove(struct platform_device *pdev)
{
	struct linx_vuart_port *lport = platform_get_drvdata(pdev);

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

#ifdef CONFIG_LINX_INTC
static int __init linx_vuart_register_builtin_port(void)
{
	struct linx_vuart_port *lport = &linx_vuart_port_storage[0];
	int rc;

	if (linx_vuart_port_inuse[0])
		return -EBUSY;

	memset(lport, 0, sizeof(*lport));
	linx_vuart_port_inuse[0] = true;
	lport->port.mapbase = LINX_VUART_BASE;
#ifdef CONFIG_MMU
	/*
	 * The reduced built-in path registers before platform probing, and the
	 * regular ioremap/vmalloc path is not stable enough yet for this bring-up
	 * lane. Keep one persistent fixmap-backed MMIO window for ttyS0 runtime
	 * callbacks instead of carrying the raw physical address past MMU enable.
	 */
	lport->port.membase = early_ioremap(LINX_VUART_BASE, LINX_VUART_SIZE);
	if (!lport->port.membase) {
		linx_vuart_port_inuse[0] = false;
		return -ENOMEM;
	}
#else
	if (linx_vuart_early_membase) {
		lport->port.membase = linx_vuart_early_membase;
	} else {
		lport->port.membase =
			(void __iomem *)(unsigned long)LINX_VUART_BASE;
	}
#endif
	lport->port.iotype = UPIO_MEM;
	lport->port.fifosize = 1;
	lport->port.ops = &linx_vuart_uart_ops;
	lport->port.flags = UPF_BOOT_AUTOCONF;
	lport->port.line = 0;

	timer_setup(&lport->poll_timer, linx_vuart_poll_timer, 0);
	lport->poll_enabled = false;

	linx_vuart_ports[0] = lport;
#ifdef CONFIG_SERIAL_LINX_VIRT_UART_CONSOLE
	linx_vuart_console.index = 0;
#endif
	rc = uart_add_one_port(&linx_vuart_uart_driver, &lport->port);
	if (rc) {
		linx_vuart_ports[0] = NULL;
#ifdef CONFIG_MMU
		early_iounmap(lport->port.membase, LINX_VUART_SIZE);
#endif
		linx_vuart_port_inuse[0] = false;
		return rc;
	}

	return 0;
}
#endif

static int __init linx_vuart_init(void)
{
	int rc;

	rc = uart_register_driver(&linx_vuart_uart_driver);
	if (rc)
		return rc;

#ifdef CONFIG_LINX_INTC
	rc = linx_vuart_register_builtin_port();
	if (rc)
		uart_unregister_driver(&linx_vuart_uart_driver);
	return rc;
#endif

	rc = platform_driver_register(&linx_vuart_platform_driver);
	if (rc) {
		uart_unregister_driver(&linx_vuart_uart_driver);
		return rc;
	}

	return 0;
}

#ifdef CONFIG_LINX_INTC
int __init linx_vuart_bringup_init(void)
{
	return linx_vuart_init();
}
#endif

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
