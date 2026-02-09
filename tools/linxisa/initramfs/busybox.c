/*
 * Minimal BusyBox-like multicall binary for LinxISA Linux bring-up (no libc).
 *
 * Goals:
 * - Provide a working PID1 (/init) that mounts /proc and /sys and drops into an
 *   interactive shell on the console.
 * - Provide a few basic "applets" via symlinks (sh, echo, cat, ls, reboot,
 *   poweroff) to validate common syscalls on LinxISA.
 *
 * Toolchain constraint (current bring-up):
 * - Avoid global/static data and string literals. The Linx LLVM+LLD stack still
 *   has relocation issues for PIE/FDPIC when accessing local data symbols.
 *
 * Syscall ABI (kernel bring-up):
 * - a7: syscall number
 * - a0..a5: args
 * - a0: return value
 * - trap via `acrc 1`
 */

typedef unsigned long ulong;
typedef long slong;

enum {
	LINX_VIRT_UART_BASE = 0x10000000UL,
};

enum {
	UART_DATA = 0x0,
	UART_STATUS = 0x4,
};

enum {
	UART_STATUS_TX_READY = 0x01,
	UART_STATUS_RX_READY = 0x02,
};

enum {
	__NR_read = 63,
	__NR_write = 64,
	__NR_openat = 56,
	__NR_close = 57,
	__NR_getdents64 = 61,
	__NR_mount = 40,
	__NR_reboot = 142,
	__NR_exit = 93,
	__NR_exit_group = 94,
};

enum {
	AT_FDCWD = -100,
};

enum {
	O_RDONLY = 0,
	O_DIRECTORY = 00200000,
	O_CLOEXEC = 02000000,
};

enum {
	LINUX_REBOOT_MAGIC1 = 0xfee1dead,
	LINUX_REBOOT_MAGIC2 = 672274793,
	LINUX_REBOOT_CMD_RESTART = 0x01234567,
	LINUX_REBOOT_CMD_POWER_OFF = 0x4321fedc,
};

static inline slong sys_call6(int nr, ulong a0, ulong a1, ulong a2, ulong a3,
			      ulong a4, ulong a5)
{
	slong ret;

	__asm__ volatile(
		"c.movr %1, ->a0\n"
		"c.movr %2, ->a1\n"
		"c.movr %3, ->a2\n"
		"c.movr %4, ->a3\n"
		"c.movr %5, ->a4\n"
		"c.movr %6, ->a5\n"
		"c.movr %7, ->a7\n"
		"acrc 1\n"
		"c.movr a0, ->%0\n"
		: "=r"(ret)
		: "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
		  "r"((ulong)nr)
		: "a0", "a1", "a2", "a3", "a4", "a5", "a7", "memory");

	return ret;
}

static inline slong sys_call5(int nr, ulong a0, ulong a1, ulong a2, ulong a3,
			      ulong a4)
{
	return sys_call6(nr, a0, a1, a2, a3, a4, 0);
}

static inline slong sys_call4(int nr, ulong a0, ulong a1, ulong a2, ulong a3)
{
	return sys_call6(nr, a0, a1, a2, a3, 0, 0);
}

static inline slong sys_call3(int nr, ulong a0, ulong a1, ulong a2)
{
	return sys_call6(nr, a0, a1, a2, 0, 0, 0);
}

static inline slong sys_call1(int nr, ulong a0)
{
	return sys_call6(nr, a0, 0, 0, 0, 0, 0);
}

static inline slong sys_read(int fd, void *buf, ulong count)
{
	return sys_call3(__NR_read, (ulong)fd, (ulong)buf, count);
}

static inline slong sys_write(int fd, const void *buf, ulong count)
{
	return sys_call3(__NR_write, (ulong)fd, (ulong)buf, count);
}

static inline slong sys_openat(int dirfd, const char *path, int flags, int mode)
{
	return sys_call4(__NR_openat, (ulong)dirfd, (ulong)path, (ulong)flags, (ulong)mode);
}

static inline slong sys_close(int fd)
{
	return sys_call1(__NR_close, (ulong)fd);
}

static inline slong sys_getdents64(int fd, void *dirp, ulong count)
{
	return sys_call3(__NR_getdents64, (ulong)fd, (ulong)dirp, count);
}

static inline slong sys_mount(const char *source, const char *target,
			      const char *fstype, ulong flags, const void *data)
{
	return sys_call5(__NR_mount, (ulong)source, (ulong)target, (ulong)fstype,
			 flags, (ulong)data);
}

static inline slong sys_reboot(int magic1, int magic2, int cmd, const void *arg)
{
	return sys_call4(__NR_reboot, (ulong)magic1, (ulong)magic2, (ulong)cmd, (ulong)arg);
}

__attribute__((noreturn)) static inline void sys_exit_group(slong code)
{
	(void)sys_call1(__NR_exit_group, (ulong)code);
	for (;;)
		__asm__ volatile("" : : : "memory");
}

static ulong c_strlen(const char *s)
{
	ulong n = 0;
	while (s[n])
		n++;
	return n;
}

static int is_errno(slong rc)
{
	return rc < 0;
}

static int is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char *make_abs_path(const char *in, char *out, ulong outsz)
{
	ulong n = 0;

	if (!in || !in[0] || (in[0] == '.' && in[1] == 0)) {
		if (outsz < 2)
			return in;
		out[0] = '/';
		out[1] = 0;
		return out;
	}

	if (in[0] == '/')
		return in;

	if (outsz < 2)
		return in;

	out[0] = '/';
	n = 1;
	while (*in && n + 1 < outsz)
		out[n++] = *in++;
	out[n] = 0;
	return out;
}

static inline void uart_putc(char c)
{
	volatile unsigned int *uart = (volatile unsigned int *)LINX_VIRT_UART_BASE;

	while ((uart[UART_STATUS / 4] & UART_STATUS_TX_READY) == 0)
		;
	uart[UART_DATA / 4] = (unsigned int)(unsigned char)c;
}

static inline int uart_getc(void)
{
	volatile unsigned int *uart = (volatile unsigned int *)LINX_VIRT_UART_BASE;

	while ((uart[UART_STATUS / 4] & UART_STATUS_RX_READY) == 0)
		;
	return (int)(uart[UART_DATA / 4] & 0xffu);
}

static void write_all(const void *buf, ulong count)
{
	const unsigned char *p = (const unsigned char *)buf;
	ulong i;

	for (i = 0; i < count; i++)
		uart_putc((char)p[i]);
}

static void write_ch(char c)
{
	uart_putc(c);
}

static void write_nl(void)
{
	write_ch('\n');
}

static void write_uhex(ulong v)
{
	int i;
	for (i = (int)(sizeof(v) * 2) - 1; i >= 0; i--) {
		unsigned int nibble = (unsigned int)((v >> (i * 4)) & 0xf);
		char c = (nibble < 10) ? ('0' + (char)nibble)
				       : ('a' + (char)(nibble - 10));
		write_ch(c);
	}
}

static const char *basename_cstr(const char *s)
{
	const char *last = s;
	const char *p = s;

	while (*p) {
		if (*p == '/')
			last = p + 1;
		p++;
	}
	return last;
}

static int name_is_busybox(const char *s)
{
	return s[0] == 'b' && s[1] == 'u' && s[2] == 's' && s[3] == 'y' &&
	       s[4] == 'b' && s[5] == 'o' && s[6] == 'x' && s[7] == 0;
}

static int name_is_init(const char *s)
{
	return s[0] == 'i' && s[1] == 'n' && s[2] == 'i' && s[3] == 't' &&
	       s[4] == 0;
}

static int name_is_sh(const char *s)
{
	return s[0] == 's' && s[1] == 'h' && s[2] == 0;
}

static int name_is_echo(const char *s)
{
	return s[0] == 'e' && s[1] == 'c' && s[2] == 'h' && s[3] == 'o' &&
	       s[4] == 0;
}

static int name_is_cat(const char *s)
{
	return s[0] == 'c' && s[1] == 'a' && s[2] == 't' && s[3] == 0;
}

static int name_is_ls(const char *s)
{
	return s[0] == 'l' && s[1] == 's' && s[2] == 0;
}

static int name_is_probe(const char *s)
{
	return s[0] == 'p' && s[1] == 'r' && s[2] == 'o' && s[3] == 'b' &&
	       s[4] == 'e' && s[5] == 0;
}

static int name_is_help(const char *s)
{
	return s[0] == 'h' && s[1] == 'e' && s[2] == 'l' && s[3] == 'p' &&
	       s[4] == 0;
}

static int name_is_fd0(const char *s)
{
	return s[0] == 'f' && s[1] == 'd' && s[2] == '0' && s[3] == 0;
}

static int name_is_exit(const char *s)
{
	return s[0] == 'e' && s[1] == 'x' && s[2] == 'i' && s[3] == 't' &&
	       s[4] == 0;
}

static int name_is_reboot(const char *s)
{
	return s[0] == 'r' && s[1] == 'e' && s[2] == 'b' && s[3] == 'o' &&
	       s[4] == 'o' && s[5] == 't' && s[6] == 0;
}

static int name_is_poweroff(const char *s)
{
	return s[0] == 'p' && s[1] == 'o' && s[2] == 'w' && s[3] == 'e' &&
	       s[4] == 'r' && s[5] == 'o' && s[6] == 'f' && s[7] == 'f' &&
	       s[8] == 0;
}

static void mount_basic(void)
{
	char proc[5];
	char sysfs[6];
	char path_proc[6];
	char path_sys[5];
	int rc;

	/* "proc" */
	proc[0] = 'p';
	proc[1] = 'r';
	proc[2] = 'o';
	proc[3] = 'c';
	proc[4] = 0;
	/* "sysfs" */
	sysfs[0] = 's';
	sysfs[1] = 'y';
	sysfs[2] = 's';
	sysfs[3] = 'f';
	sysfs[4] = 's';
	sysfs[5] = 0;
	/* "/proc" */
	path_proc[0] = '/';
	path_proc[1] = 'p';
	path_proc[2] = 'r';
	path_proc[3] = 'o';
	path_proc[4] = 'c';
	path_proc[5] = 0;
	/* "/sys" */
	path_sys[0] = '/';
	path_sys[1] = 's';
	path_sys[2] = 'y';
	path_sys[3] = 's';
	path_sys[4] = 0;

	rc = sys_mount(proc, path_proc, proc, 0, 0);
	if (rc) {
		/* Print: mproc=<hex> */
		write_ch('m'); write_ch('p'); write_ch('r'); write_ch('o'); write_ch('c'); write_ch('=');
		write_uhex((ulong)rc);
		write_nl();
	}
	rc = sys_mount(sysfs, path_sys, sysfs, 0, 0);
	if (rc) {
		/* Print: msys=<hex> */
		write_ch('m'); write_ch('s'); write_ch('y'); write_ch('s'); write_ch('=');
		write_uhex((ulong)rc);
		write_nl();
	}
}

static int applet_help(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* Prints a compact help without string literals. */
	write_ch('c');
	write_ch('m');
	write_ch('d');
	write_ch('s');
	write_ch(':');
	write_ch(' ');
	/* echo cat ls help fd0 exit reboot poweroff sh */
	write_ch('e'); write_ch('c'); write_ch('h'); write_ch('o'); write_ch(' ');
	write_ch('c'); write_ch('a'); write_ch('t'); write_ch(' ');
	write_ch('l'); write_ch('s'); write_ch(' ');
	write_ch('h'); write_ch('e'); write_ch('l'); write_ch('p'); write_ch(' ');
	write_ch('f'); write_ch('d'); write_ch('0'); write_ch(' ');
	write_ch('e'); write_ch('x'); write_ch('i'); write_ch('t'); write_ch(' ');
	write_ch('r'); write_ch('e'); write_ch('b'); write_ch('o'); write_ch('o'); write_ch('t'); write_ch(' ');
	write_ch('p'); write_ch('o'); write_ch('w'); write_ch('e'); write_ch('r'); write_ch('o'); write_ch('f'); write_ch('f'); write_ch(' ');
	write_ch('s'); write_ch('h');
	write_nl();

	return 0;
}

static int applet_fd0(int argc, char **argv)
{
	char tmp;
	int rc;

	(void)argc;
	(void)argv;

	/* read(0, ..., 0) should return 0 if fd0 is valid. */
	rc = sys_read(0, &tmp, 0);

	write_ch('f'); write_ch('d'); write_ch('0'); write_ch('=');
	write_uhex((ulong)rc);
	write_nl();

	return 0;
}

static int applet_echo(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		ulong n = c_strlen(argv[i]);
		if (n)
			write_all(argv[i], n);
		if (i + 1 < argc)
			write_ch(' ');
	}
	write_nl();
	return 0;
}

static int applet_cat(int argc, char **argv)
{
	char buf[256];
	char abs[256];
	const char *path;
	int fd;

	if (argc < 2)
		return -1;

	path = make_abs_path(argv[1], abs, sizeof(abs));
	fd = sys_openat(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return (int)fd;

	for (;;) {
		int n = sys_read(fd, buf, sizeof(buf));
		if (n == 0)
			break;
		if (is_errno(n)) {
			(void)sys_close(fd);
			return n;
		}
		write_all(buf, (ulong)n);
	}

	(void)sys_close(fd);
	return 0;
}

static int bufs_equal(const char *a, const char *b, ulong n)
{
	ulong i;

	for (i = 0; i < n; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
}

static int applet_probe(int argc, char **argv)
{
	char abs[256];
	const char *path;
	int fd;
	char b1[64];
	char b2[64];
	int n1;
	int n2;
	int same;

	if (argc < 2)
		return -1;

	path = make_abs_path(argv[1], abs, sizeof(abs));
	fd = sys_openat(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	n1 = sys_read(fd, b1, (ulong)sizeof(b1));
	n2 = sys_read(fd, b2, (ulong)sizeof(b2));

	same = 0;
	if (!is_errno(n1) && n1 != 0 && n1 == n2) {
		ulong n = (ulong)n1;
		if (n > sizeof(b1))
			n = sizeof(b1);
		same = bufs_equal(b1, b2, n);
	}

	(void)sys_close(fd);

	/* Print: n1=<hex> n2=<hex> same=<0/1> */
	write_ch('n'); write_ch('1'); write_ch('=');
	write_uhex((ulong)n1);
	write_ch(' '); write_ch('n'); write_ch('2'); write_ch('=');
	write_uhex((ulong)n2);
	write_ch(' '); write_ch('s'); write_ch('a'); write_ch('m'); write_ch('e'); write_ch('=');
	write_uhex((ulong)same);
	write_nl();

	return 0;
}

static unsigned short load_u16le(const unsigned char *p)
{
	return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static int applet_ls(int argc, char **argv)
{
	unsigned char buf[1024];
	char dot[2];
	char abs[256];
	const char *path;
	int fd;
	ulong loops = 0;

	dot[0] = '.';
	dot[1] = 0;
	path = (argc >= 2) ? (const char *)argv[1] : (const char *)dot;
	path = make_abs_path(path, abs, sizeof(abs));

	fd = sys_openat(AT_FDCWD, path, O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	for (;;) {
		int n = sys_getdents64(fd, buf, sizeof(buf));
		ulong off = 0;

		loops++;
		if (loops > 256) {
			(void)sys_close(fd);
			return -1;
		}

		if (n == 0)
			break;
		if (is_errno(n)) {
			(void)sys_close(fd);
			return n;
		}

		while (off + 19 < (ulong)n) {
			const unsigned char *d = buf + off;
			unsigned short reclen = load_u16le(d + 16);
			const char *name = (const char *)(d + 19);
			ulong name_len = 0;
			ulong max_name;

			if (reclen < 19)
				break;
			if (off + (ulong)reclen > (ulong)n)
				break;

			max_name = (ulong)reclen - 19;
			if (max_name == 0)
				break;

			/* Skip "." and ".." */
			if (name[0] == '.' &&
			    (name[1] == 0 || (name[1] == '.' && name[2] == 0))) {
				off += reclen;
				continue;
			}

			while (name_len < max_name && name[name_len])
				name_len++;
			/* Require a terminating NUL within the record. */
			if (name_len == max_name)
				break;
			if (name_len)
				write_all(name, name_len);
			write_nl();

			off += reclen;
		}
	}

	(void)sys_close(fd);
	return 0;
}

static int applet_reboot_common(int cmd)
{
	(void)sys_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, cmd, 0);
	return -1;
}

static int applet_reboot(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	return applet_reboot_common(LINUX_REBOOT_CMD_RESTART);
}

static int applet_poweroff(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	return applet_reboot_common(LINUX_REBOOT_CMD_POWER_OFF);
}

static void shell_loop(void)
{
	char line[256];

	for (;;) {
		ulong len = 0;

		write_ch('#');
		write_ch(' ');

		for (;;) {
			int ch;

			if (len + 1 >= sizeof(line)) {
				write_nl();
				break;
			}

			ch = uart_getc();
			if (ch == '\r')
				continue;

			line[len++] = (char)ch;
			write_ch((char)ch);

			if (ch == '\n') {
				int argc = 0;
				char *argv[16];
				char *p;

				line[len - 1] = 0;

				p = line;
				while (*p && argc < (int)(sizeof(argv) / sizeof(argv[0]))) {
					while (*p && is_space(*p))
						p++;
					if (!*p)
						break;
					argv[argc++] = p;
					while (*p && !is_space(*p))
						p++;
					if (*p)
						*p++ = 0;
				}

				if (argc > 0) {
					char *cmd = argv[0];
					int rc = -1;

					if (name_is_help(cmd))
						rc = applet_help(argc, argv);
					else if (name_is_fd0(cmd))
						rc = applet_fd0(argc, argv);
					else if (name_is_echo(cmd))
						rc = applet_echo(argc, argv);
					else if (name_is_cat(cmd))
						rc = applet_cat(argc, argv);
					else if (name_is_ls(cmd))
						rc = applet_ls(argc, argv);
					else if (name_is_probe(cmd))
						rc = applet_probe(argc, argv);
					else if (name_is_reboot(cmd))
						rc = applet_reboot(argc, argv);
					else if (name_is_poweroff(cmd))
						rc = applet_poweroff(argc, argv);
					else if (name_is_exit(cmd))
						sys_exit_group(0);
					else if (name_is_sh(cmd))
						rc = 0;
					else
						rc = -1;

					if (rc < 0) {
						/* Print "E:" + hex errno-ish */
						write_ch('E');
						write_ch(':');
						write_ch(' ');
						write_uhex((ulong)rc);
						write_nl();
					}
				}
				break;
			}
		}
	}
}

static __attribute__((noreturn)) void run_applet(const char *name, int argc,
						 char **argv)
{
	if (name_is_init(name)) {
		mount_basic();
		shell_loop();
		sys_exit_group(0);
	}
	if (name_is_sh(name)) {
		shell_loop();
		sys_exit_group(0);
	}
	if (name_is_help(name))
		sys_exit_group(applet_help(argc, argv));
	if (name_is_fd0(name))
		sys_exit_group(applet_fd0(argc, argv));
	if (name_is_echo(name))
		sys_exit_group(applet_echo(argc, argv));
	if (name_is_cat(name))
		sys_exit_group(applet_cat(argc, argv));
	if (name_is_ls(name))
		sys_exit_group(applet_ls(argc, argv));
	if (name_is_probe(name))
		sys_exit_group(applet_probe(argc, argv));
	if (name_is_reboot(name))
		sys_exit_group(applet_reboot(argc, argv));
	if (name_is_poweroff(name))
		sys_exit_group(applet_poweroff(argc, argv));

	/* Unknown applet: drop into the shell. */
	shell_loop();
	sys_exit_group(0);
}

static inline ulong get_sp(void)
{
	ulong sp;
	__asm__ volatile("c.movr sp, ->%0\n" : "=r"(sp) : : "memory");
	return sp;
}

__attribute__((noreturn)) void _start(void)
{
	ulong sp;
	ulong a0, a1;
	int argc = 0;
	char **argv = 0;
	const char *argv0 = 0;
	const char *base = 0;

	/*
	 * Entry ABI bring-up: prefer (a0=argc, a1=argv) if it looks sane, else
	 * fall back to the traditional stack layout.
	 */
	__asm__ volatile("c.movr a0, ->%0\n" : "=r"(a0) : : "memory");
	__asm__ volatile("c.movr a1, ->%0\n" : "=r"(a1) : : "memory");

	if (a0 > 0 && a0 < 128 && a1) {
		argc = (int)a0;
		argv = (char **)a1;
	} else {
		sp = get_sp();
		if (sp) {
			argc = (int)(((ulong *)sp)[0]);
			argv = (char **)(((ulong *)sp) + 1);
		}
	}

	if (argc >= 1 && argv)
		argv0 = argv[0];
	if (argv0)
		base = basename_cstr(argv0);

	/* If invoked as "busybox <applet> ...", dispatch on argv[1]. */
	if (base && name_is_busybox(base) && argc >= 2 && argv[1]) {
		run_applet(argv[1], argc - 1, argv + 1);
	}

	if (base)
		run_applet(base, argc, argv);

	/* Fallback: act as PID1 (/init). */
	mount_basic();
	shell_loop();
	sys_exit_group(0);
}
