/* Minimal PID1 for LinxISA NOMMU Linux bring-up (no libc).
 *
 * - Exercises syscall trap routing via `acrc 1` (SCT_SYS -> ACR1/kernel).
 * - Keeps PID 1 alive (avoid "Attempted to kill init").
 *
 * Syscall ABI (current kernel bring-up):
 * - a7: syscall number
 * - a0..a5: args
 * - a0: return value
 */

typedef unsigned long ulong;
typedef long slong;

enum {
	__NR_write = 64,
};

#define LINX_VIRT_UART_BASE 0x10000000UL

static inline void uart_putc(char c)
{
	*(volatile unsigned char *)(LINX_VIRT_UART_BASE + 0x0) =
		(unsigned char)c;
}

static inline void uart_puthex_nibble(unsigned int nibble)
{
	char c = (nibble < 10) ? ('0' + (char)nibble)
			       : ('a' + (char)(nibble - 10));

	uart_putc(c);
}

static inline void uart_puthex_ulong(ulong v)
{
	int i;

	uart_putc('0');
	uart_putc('x');
	for (i = (int)(sizeof(v) * 2) - 1; i >= 0; i--) {
		unsigned int nibble = (v >> (i * 4)) & 0xf;

		uart_puthex_nibble(nibble);
	}
}

static inline slong sys_write(slong fd, const void *buf, ulong count)
{
	slong ret;

	/*
	 * Clang's fixed-register variables for Linx are not reliable yet during
	 * bring-up. Move args into the Linux syscall ABI registers explicitly.
	 */
	__asm__ volatile(
		"c.movr %1, ->a0\n"
		"c.movr %2, ->a1\n"
		"c.movr %3, ->a2\n"
		"addi zero, 64, ->a7\n"
		"acrc 1\n"
		"c.movr a0, ->%0\n"
		: "=r"(ret)
		: "r"(fd), "r"(buf), "r"(count)
		: "a0", "a1", "a2", "a7", "memory");
	return ret;
}

__attribute__((noreturn)) void _start(void)
{
	union {
		ulong w;
		char c[sizeof(ulong)];
	} msg;
	slong ret;
	slong fd;

	/* Prove we've reached userspace even if syscalls are broken. */
	uart_putc('U');
	uart_putc('\n');

	/* Keep the user buffer naturally aligned (memcpy may use word ops). */
	msg.c[0] = '!';
	msg.c[1] = '\n';

	for (fd = 0; fd < 6; fd++) {
		ret = sys_write(fd, (const void *)msg.c, 2);
		uart_putc('f');
		uart_putc('0' + (char)fd);
		uart_putc('=');
		uart_puthex_ulong((ulong)ret);
		uart_putc('\n');
	}
	for (;;)
		__asm__ volatile("" : : : "memory");
}
