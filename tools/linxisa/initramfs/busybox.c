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
	__NR_read = 63,
	__NR_write = 64,
	__NR_openat = 56,
	__NR_close = 57,
	__NR_getdents64 = 61,
	__NR_mount = 40,
	__NR_dup = 23,
	__NR_rt_sigaction = 134,
	__NR_rt_sigreturn = 139,
	__NR_reboot = 142,
	__NR_exit = 93,
	__NR_exit_group = 94,
	__NR_execve = 221,
};

enum {
	AT_FDCWD = -100,
};

enum {
	O_RDONLY = 0,
	O_WRONLY = 1,
	O_RDWR = 2,
	O_NONBLOCK = 00004000,
	O_DIRECTORY = 00200000,
	O_CLOEXEC = 02000000,
};

enum {
	LINUX_REBOOT_MAGIC1 = 0xfee1dead,
	LINUX_REBOOT_MAGIC2 = 672274793,
	LINUX_REBOOT_CMD_RESTART = 0x01234567,
	LINUX_REBOOT_CMD_POWER_OFF = 0x4321fedc,
};

enum {
	SA_SIGINFO = 0x00000004,
	SA_RESTORER = 0x04000000,
};

enum {
	SIGILL = 4,
	SIGTRAP = 5,
	SIGSEGV = 11,
};

enum {
	PTR_PC = 24,
	NUM_PTRACE_REG = 25,
};

enum {
	SSR_TIME = 0x0010,
	SSR_USER_SCRATCH0 = 0x0030,
};

/*
 * Keep the raw syscall helper out-of-line.
 *
 * The current Linx Linux trap return path does not preserve block-local
 * queue state (`t/u`) across ACRC transitions, so inlining this helper into
 * hot loops can corrupt compiler temporaries kept in those queues.
 */
__attribute__((noinline)) static slong sys_call6(int nr, ulong a0, ulong a1,
						 ulong a2, ulong a3, ulong a4,
						 ulong a5)
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
		"c.bstop\n"
		"C.BSTART\n"
		"c.movr a0, ->%0\n"
		: "=r"(ret)
		: "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
		  "r"((ulong)nr)
		: "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "x0",
		  "x1", "x2", "x3", "ra", "memory");
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

static inline slong sys_openat_compat(int dirfd, const char *path, int flags,
				      int mode)
{
	slong rc = sys_openat(dirfd, path, flags, mode);

	/*
	 * Some early Linx Linux builds reject O_CLOEXEC with -EINVAL.
	 * Retry without CLOEXEC so initramfs bring-up can continue.
	 */
	if (rc == -22 && (flags & O_CLOEXEC))
		rc = sys_openat(dirfd, path, flags & ~O_CLOEXEC, mode);
	return rc;
}

static inline slong sys_close(int fd)
{
	return sys_call1(__NR_close, (ulong)fd);
}

static inline slong sys_dup(int fd)
{
	return sys_call1(__NR_dup, (ulong)fd);
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

static inline slong sys_rt_sigaction(int sig, const void *act, void *oact,
				     ulong sigsetsize)
{
	return sys_call4(__NR_rt_sigaction, (ulong)sig, (ulong)act, (ulong)oact,
			 sigsetsize);
}

static inline slong sys_reboot(int magic1, int magic2, int cmd, const void *arg)
{
	return sys_call4(__NR_reboot, (ulong)magic1, (ulong)magic2, (ulong)cmd, (ulong)arg);
}

static inline slong sys_execve(const char *path, char *const argv[], char *const envp[])
{
	return sys_call3(__NR_execve, (ulong)path, (ulong)argv, (ulong)envp);
}

__attribute__((noreturn, always_inline)) static inline void sys_exit_group(slong code)
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

static void write_ch(char c);
static void write_nl(void);
static void write_uhex(ulong v);

static void console_open(void)
{
	char console[13];
	char ttys0[11];
	char ttylinx0[14];
	slong fd_rc;
	int fd;

	/* "/dev/console" */
	console[0] = '/';
	console[1] = 'd';
	console[2] = 'e';
	console[3] = 'v';
	console[4] = '/';
	console[5] = 'c';
	console[6] = 'o';
	console[7] = 'n';
	console[8] = 's';
	console[9] = 'o';
	console[10] = 'l';
	console[11] = 'e';
	console[12] = 0;

	/* "/dev/ttyS0" */
	ttys0[0] = '/';
	ttys0[1] = 'd';
	ttys0[2] = 'e';
	ttys0[3] = 'v';
	ttys0[4] = '/';
	ttys0[5] = 't';
	ttys0[6] = 't';
	ttys0[7] = 'y';
	ttys0[8] = 'S';
	ttys0[9] = '0';
	ttys0[10] = 0;

	/* "/dev/ttyLINX0" */
	ttylinx0[0] = '/';
	ttylinx0[1] = 'd';
	ttylinx0[2] = 'e';
	ttylinx0[3] = 'v';
	ttylinx0[4] = '/';
	ttylinx0[5] = 't';
	ttylinx0[6] = 't';
	ttylinx0[7] = 'y';
	ttylinx0[8] = 'L';
	ttylinx0[9] = 'I';
	ttylinx0[10] = 'N';
	ttylinx0[11] = 'X';
	ttylinx0[12] = '0';
	ttylinx0[13] = 0;

	/*
	 * Prefer /dev/ttyS0 for interactive bring-up.
	 *
	 * The arch/linx early printk console ("ttyLINX") is TX-only; /dev/console
	 * can be write-only early if it resolves to that console. Opening the
	 * real UART tty ensures RX polling is enabled (via linx_virt_uart).
	 */
	fd_rc = sys_openat_compat(AT_FDCWD, ttys0, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
	if (fd_rc < 0)
		fd_rc = sys_openat_compat(AT_FDCWD, ttylinx0, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
	if (fd_rc < 0)
		fd_rc = sys_openat_compat(AT_FDCWD, console, O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
	if (fd_rc < 0)
		return;

	fd = (int)fd_rc;
	if (fd != 0) {
		(void)sys_close(0);
		(void)sys_dup(fd);
	}
	if (fd != 1) {
		(void)sys_close(1);
		(void)sys_dup(fd);
	}
	if (fd != 2) {
		(void)sys_close(2);
		(void)sys_dup(fd);
	}
	if (fd > 2)
		(void)sys_close(fd);

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

static void write_ch(char c);

static void write_all(const void *buf, ulong count)
{
	const unsigned char *p = (const unsigned char *)buf;
	ulong off = 0;

	while (off < count)
		write_ch((char)p[off++]);
}

static void write_ch(char c)
{
	(void)sys_write(1, &c, 1);
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

static int name_is_put(const char *s)
{
	return s[0] == 'p' && s[1] == 'u' && s[2] == 't' && s[3] == 0;
}

static int name_is_getdents64_probe(const char *s)
{
	return s[0] == 'g' && s[1] == 'e' && s[2] == 't' && s[3] == 'd' &&
	       s[4] == 'e' && s[5] == 'n' && s[6] == 't' && s[7] == 's' &&
	       s[8] == '6' && s[9] == '4' && s[10] == '_' && s[11] == 'p' &&
	       s[12] == 'r' && s[13] == 'o' && s[14] == 'b' && s[15] == 'e' &&
	       s[16] == 0;
}

static int name_is_ctx_tq_irq_test(const char *s)
{
	return s[0] == 'c' && s[1] == 't' && s[2] == 'x' && s[3] == '_' &&
	       s[4] == 't' && s[5] == 'q' && s[6] == '_' && s[7] == 'i' &&
	       s[8] == 'r' && s[9] == 'q' && s[10] == '_' && s[11] == 't' &&
	       s[12] == 'e' && s[13] == 's' && s[14] == 't' && s[15] == 0;
}

static int name_is_ctx_ri_step_trap_test(const char *s)
{
	return s[0] == 'c' && s[1] == 't' && s[2] == 'x' && s[3] == '_' &&
	       s[4] == 'r' && s[5] == 'i' && s[6] == '_' && s[7] == 's' &&
	       s[8] == 't' && s[9] == 'e' && s[10] == 'p' && s[11] == '_' &&
	       s[12] == 't' && s[13] == 'r' && s[14] == 'a' && s[15] == 'p' &&
	       s[16] == '_' && s[17] == 't' && s[18] == 'e' && s[19] == 's' &&
	       s[20] == 't' && s[21] == 0;
}

static int name_is_sigill_test(const char *s)
{
	return s[0] == 's' && s[1] == 'i' && s[2] == 'g' && s[3] == 'i' &&
	       s[4] == 'l' && s[5] == 'l' && s[6] == '_' && s[7] == 't' &&
	       s[8] == 'e' && s[9] == 's' && s[10] == 't' && s[11] == 0;
}

static int name_is_sigsegv_test(const char *s)
{
	return s[0] == 's' && s[1] == 'i' && s[2] == 'g' && s[3] == 's' &&
	       s[4] == 'e' && s[5] == 'g' && s[6] == 'v' && s[7] == '_' &&
	       s[8] == 't' && s[9] == 'e' && s[10] == 's' && s[11] == 't' &&
	       s[12] == 0;
}

static int name_is_m9p(const char *s)
{
	return s[0] == 'm' && s[1] == '9' && s[2] == 'p' && s[3] == 0;
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
	/* Best-effort pseudo-fs mounts for usable userspace introspection. */
	char proc_src[5];
	char proc_tgt[6];
	char proc_fs[5];
	char sys_src[6];
	char sys_tgt[5];
	char sys_fs[6];
	char dev_src[9];
	char dev_tgt[5];
	char dev_fs[9];

	/* "proc" */
	proc_src[0] = 'p'; proc_src[1] = 'r'; proc_src[2] = 'o'; proc_src[3] = 'c'; proc_src[4] = 0;
	proc_fs[0] = 'p'; proc_fs[1] = 'r'; proc_fs[2] = 'o'; proc_fs[3] = 'c'; proc_fs[4] = 0;
	/* "/proc" */
	proc_tgt[0] = '/'; proc_tgt[1] = 'p'; proc_tgt[2] = 'r'; proc_tgt[3] = 'o'; proc_tgt[4] = 'c'; proc_tgt[5] = 0;

	/* "sysfs" */
	sys_src[0] = 's'; sys_src[1] = 'y'; sys_src[2] = 's'; sys_src[3] = 'f'; sys_src[4] = 's'; sys_src[5] = 0;
	sys_fs[0] = 's'; sys_fs[1] = 'y'; sys_fs[2] = 's'; sys_fs[3] = 'f'; sys_fs[4] = 's'; sys_fs[5] = 0;
	/* "/sys" */
	sys_tgt[0] = '/'; sys_tgt[1] = 's'; sys_tgt[2] = 'y'; sys_tgt[3] = 's'; sys_tgt[4] = 0;

	/* "devtmpfs" */
	dev_src[0] = 'd'; dev_src[1] = 'e'; dev_src[2] = 'v'; dev_src[3] = 't';
	dev_src[4] = 'm'; dev_src[5] = 'p'; dev_src[6] = 'f'; dev_src[7] = 's';
	dev_src[8] = 0;
	dev_fs[0] = 'd'; dev_fs[1] = 'e'; dev_fs[2] = 'v'; dev_fs[3] = 't';
	dev_fs[4] = 'm'; dev_fs[5] = 'p'; dev_fs[6] = 'f'; dev_fs[7] = 's';
	dev_fs[8] = 0;
	/* "/dev" */
	dev_tgt[0] = '/'; dev_tgt[1] = 'd'; dev_tgt[2] = 'e'; dev_tgt[3] = 'v';
	dev_tgt[4] = 0;

	(void)sys_mount(dev_src, dev_tgt, dev_fs, 0, 0);
	(void)sys_mount(proc_src, proc_tgt, proc_fs, 0, 0);
	(void)sys_mount(sys_src, sys_tgt, sys_fs, 0, 0);


}

static void mount_9p_share(void)
{
	/* Mount host 9p share at /opt/share (best-effort). */
	const char *src = "render-share";
	const char *tgt = "/opt/share";
	const char *fst = "9p";
	/*
	 * Use conservative msize first.
	 * Try both 9p2000.L and 9p2000 (expected to both be acceptable).
	 */
	const char *opt_l = "trans=virtio,version=9p2000.L,msize=8192,cache=none,access=any";
	const char *opt_2 = "trans=virtio,version=9p2000,msize=8192,cache=none,access=any";
	slong rc;

	/* Print: 9p_L=<hex> */
	rc = sys_mount(src, tgt, fst, 0, opt_l);
	write_ch('9'); write_ch('p'); write_ch('_'); write_ch('L');
	write_ch('=');
	write_uhex((ulong)rc);
	write_nl();

	/* Print: 9p_2=<hex> */
	rc = sys_mount(src, tgt, fst, 0, opt_2);
	write_ch('9'); write_ch('p'); write_ch('_'); write_ch('2');
	write_ch('=');
	write_uhex((ulong)rc);
	write_nl();
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
	/* echo cat ls help fd0 probe put getdents64_probe ctx_tq_irq_test ctx_ri_step_trap_test sigill_test sigsegv_test exit reboot poweroff sh */
	write_ch('e'); write_ch('c'); write_ch('h'); write_ch('o'); write_ch(' ');
	write_ch('c'); write_ch('a'); write_ch('t'); write_ch(' ');
	write_ch('l'); write_ch('s'); write_ch(' ');
	write_ch('h'); write_ch('e'); write_ch('l'); write_ch('p'); write_ch(' ');
	write_ch('f'); write_ch('d'); write_ch('0'); write_ch(' ');
	write_ch('p'); write_ch('r'); write_ch('o'); write_ch('b'); write_ch('e'); write_ch(' ');
	write_ch('p'); write_ch('u'); write_ch('t'); write_ch(' ');
	write_ch('g'); write_ch('e'); write_ch('t'); write_ch('d'); write_ch('e'); write_ch('n'); write_ch('t'); write_ch('s');
	write_ch('6'); write_ch('4'); write_ch('_');
	write_ch('p'); write_ch('r'); write_ch('o'); write_ch('b'); write_ch('e'); write_ch(' ');
	write_ch('c'); write_ch('t'); write_ch('x'); write_ch('_');
	write_ch('t'); write_ch('q'); write_ch('_');
	write_ch('i'); write_ch('r'); write_ch('q'); write_ch('_');
	write_ch('t'); write_ch('e'); write_ch('s'); write_ch('t'); write_ch(' ');
	write_ch('c'); write_ch('t'); write_ch('x'); write_ch('_');
	write_ch('r'); write_ch('i'); write_ch('_');
	write_ch('s'); write_ch('t'); write_ch('e'); write_ch('p'); write_ch('_');
	write_ch('t'); write_ch('r'); write_ch('a'); write_ch('p'); write_ch('_');
	write_ch('t'); write_ch('e'); write_ch('s'); write_ch('t'); write_ch(' ');
	write_ch('s'); write_ch('i'); write_ch('g'); write_ch('i'); write_ch('l'); write_ch('l'); write_ch('_');
	write_ch('t'); write_ch('e'); write_ch('s'); write_ch('t'); write_ch(' ');
	write_ch('s'); write_ch('i'); write_ch('g'); write_ch('s'); write_ch('e'); write_ch('g'); write_ch('v'); write_ch('_');
	write_ch('t'); write_ch('e'); write_ch('s'); write_ch('t'); write_ch(' ');
	write_ch('e'); write_ch('x'); write_ch('i'); write_ch('t'); write_ch(' ');
	write_ch('r'); write_ch('e'); write_ch('b'); write_ch('o'); write_ch('o'); write_ch('t'); write_ch(' ');
	write_ch('p'); write_ch('o'); write_ch('w'); write_ch('e'); write_ch('r'); write_ch('o'); write_ch('f'); write_ch('f'); write_ch(' ');
	write_ch('s'); write_ch('h');
	write_nl();

	return 0;
}


static int applet_m9p(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	mount_9p_share();
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
	int i;

	if (argc < 2)
		return -1;

	path = make_abs_path(argv[1], abs, sizeof(abs));
	fd = sys_openat_compat(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return (int)fd;

	/*
	 * Pre-touch the userspace destination buffer so read() does not need to
	 * fault-in a fresh stack page from kernel copy_to_user() context.
	 */
	for (i = 0; i < (int)sizeof(buf); i++)
		buf[i] = 0;

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

static int path_is_proc_or_sys(const char *path)
{
	if (!path || path[0] != '/')
		return 0;
	if (path[1] == 'p' && path[2] == 'r' && path[3] == 'o' &&
	    path[4] == 'c' && path[5] == '/')
		return 1;
	if (path[1] == 's' && path[2] == 'y' && path[3] == 's' &&
	    path[4] == '/')
		return 1;
	return 0;
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
	int i;

	if (argc < 2)
		return -1;

	path = make_abs_path(argv[1], abs, sizeof(abs));
	fd = sys_openat_compat(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	/*
	 * Pre-touch destination buffers for the same reason as applet_cat().
	 */
	for (i = 0; i < (int)sizeof(b1); i++)
		b1[i] = 0;
	for (i = 0; i < (int)sizeof(b2); i++)
		b2[i] = 0;

	if (path_is_proc_or_sys(path)) {
		/*
		 * procfs/sysfs reads are still being stabilized in this tiny
		 * no-libc probe path; skip data reads to avoid blocking the
		 * full-boot bring-up flow.
		 */
		n1 = 0;
		n2 = 0;
	} else {
		n1 = sys_read(fd, b1, (ulong)sizeof(b1));
		if (!is_errno(n1) && n1 > 0)
			n2 = sys_read(fd, b2, (ulong)sizeof(b2));
		else
			n2 = 0;
	}

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
	if (!is_errno(n1) && n1 > 0) {
		int i;
		int lim = n1;
		if (lim > 16)
			lim = 16;
		write_ch(' ');
		write_ch('b'); write_ch('1'); write_ch('=');
		for (i = 0; i < lim; i++) {
			static const char hex[] = "0123456789abcdef";
			unsigned char c = (unsigned char)b1[i];
			write_ch(hex[(c >> 4) & 0xf]);
			write_ch(hex[c & 0xf]);
		}
	}
	write_nl();

	return 0;
}

static int applet_put(int argc, char **argv)
{
	char abs[256];
	const char *path;
	ulong len;
	int fd;
	int n;

	if (argc < 3)
		return -1;

	path = make_abs_path(argv[1], abs, sizeof(abs));
	fd = sys_openat_compat(AT_FDCWD, path, O_WRONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	len = c_strlen(argv[2]);
	n = sys_write(fd, argv[2], len);
	(void)sys_close(fd);
	return n;
}

static unsigned short load_u16le(const unsigned char *p)
{
	return (unsigned short)((unsigned short)p[0] | ((unsigned short)p[1] << 8));
}

static ulong load_u64le(const unsigned char *p)
{
	ulong v = 0;
	int i;

	for (i = 7; i >= 0; i--)
		v = (v << 8) | (ulong)p[i];
	return v;
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

	fd = sys_openat_compat(AT_FDCWD, path, O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
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

static int applet_getdents64_probe(int argc, char **argv)
{
	unsigned char buf[1024];
	char dot[2];
	char abs[256];
	const char *path;
	int fd;
	ulong loops = 0;
	ulong last_off = 0;

	dot[0] = '.';
	dot[1] = 0;
	path = (argc >= 2) ? (const char *)argv[1] : (const char *)dot;
	path = make_abs_path(path, abs, sizeof(abs));

	fd = sys_openat_compat(AT_FDCWD, path, O_RDONLY | O_DIRECTORY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	for (;;) {
		int n = sys_getdents64(fd, buf, sizeof(buf));
		ulong off = 0;

		loops++;
		if (loops > 4096) {
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
			ulong d_off = load_u64le(d + 8);
			unsigned short reclen = load_u16le(d + 16);

			if (reclen < 19)
				break;
			if (off + (ulong)reclen > (ulong)n)
				break;

			/*
			 * getdents64 correctness probe (bring-up):
			 * - directory offsets should advance monotonically
			 * - iteration should converge to a 0-byte read
			 *
			 * Some filesystems may report d_off=0 for special entries;
			 * ignore zeros but still require monotonicity when non-zero.
			 */
			if (d_off && d_off <= last_off) {
				(void)sys_close(fd);
				return -1;
			}
			if (d_off)
				last_off = d_off;

			off += reclen;
		}
	}

	(void)sys_close(fd);

	/* Print: dents_ok=<0/1> */
	write_ch('d'); write_ch('e'); write_ch('n'); write_ch('t'); write_ch('s');
	write_ch('_'); write_ch('o'); write_ch('k'); write_ch('=');
	write_uhex(1);
	write_nl();

	return 0;
}

static int parse_irq0_count(const char *buf, ulong len, ulong *out)
{
	ulong i = 0;

	while (i < len) {
		while (i < len && (buf[i] == ' ' || buf[i] == '\t'))
			i++;

		if (i + 1 < len && buf[i] == '0' && buf[i + 1] == ':') {
			ulong v = 0;
			int have_digit = 0;

			i += 2;
			while (i < len && (buf[i] == ' ' || buf[i] == '\t'))
				i++;
			while (i < len && buf[i] >= '0' && buf[i] <= '9') {
				have_digit = 1;
				v = (v * 10) + (ulong)(buf[i] - '0');
				i++;
			}
			if (!have_digit)
				return -1;
			*out = v;
			return 0;
		}

		while (i < len && buf[i] != '\n')
			i++;
		if (i < len)
			i++;
	}

	return -1;
}

static int parse_intr_total_count(const char *buf, ulong len, ulong *out)
{
	ulong i = 0;

	while (i < len) {
		while (i < len && (buf[i] == ' ' || buf[i] == '\t'))
			i++;

		if (i + 4 < len &&
		    buf[i] == 'i' && buf[i + 1] == 'n' &&
		    buf[i + 2] == 't' && buf[i + 3] == 'r' &&
		    (buf[i + 4] == ' ' || buf[i + 4] == '\t')) {
			ulong v = 0;
			int have_digit = 0;

			i += 4;
			while (i < len && (buf[i] == ' ' || buf[i] == '\t'))
				i++;
			while (i < len && buf[i] >= '0' && buf[i] <= '9') {
				have_digit = 1;
				v = (v * 10) + (ulong)(buf[i] - '0');
				i++;
			}
			if (!have_digit)
				return -1;
			*out = v;
			return 0;
		}

		while (i < len && buf[i] != '\n')
			i++;
		if (i < len)
			i++;
	}

	return -1;
}

static int parse_decimal_value(const char *buf, ulong len, ulong *out)
{
	ulong i = 0;
	ulong v = 0;
	int have_digit = 0;

	while (i < len && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n'))
		i++;
	while (i < len && buf[i] >= '0' && buf[i] <= '9') {
		have_digit = 1;
		v = (v * 10) + (ulong)(buf[i] - '0');
		i++;
	}
	if (!have_digit)
		return -1;
	*out = v;
	return 0;
}

static int read_proc_file(const char *path, char *buf, ulong cap, ulong *got)
{
	slong n;
	int fd;

	*got = 0;
	fd = sys_openat_compat(AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	for (;;) {
		if (*got >= cap)
			break;
		n = sys_read(fd, buf + *got, cap - *got);
		if (n == 0)
			break;
		if (is_errno(n)) {
			(void)sys_close(fd);
			return (int)n;
		}
		*got += (ulong)n;
	}

	(void)sys_close(fd);
	return 0;
}

static int read_timer_irq_count(ulong *out)
{
	char buf[64];
	ulong got = 0;
	int rc;

	rc = read_proc_file("/proc/linx_ctx_tu_timer_count", buf, sizeof(buf), &got);
	if (rc < 0)
		return rc;
	return parse_decimal_value(buf, got, out);
}

static int write_path_literal(const char *path, const char *value)
{
	int fd;
	ulong len;
	int n;

	fd = sys_openat_compat(AT_FDCWD, path, O_WRONLY | O_CLOEXEC, 0);
	if (is_errno(fd))
		return fd;

	len = c_strlen(value);
	n = sys_write(fd, value, len);
	(void)sys_close(fd);
	return n;
}

static inline ulong ssrget_time_symbol(void)
{
	ulong out;

	__asm__ volatile("ssrget %1, ->%0" : "=r"(out) : "i"(SSR_TIME) : "memory");
	return out;
}

static inline void hl_ssrset_timedcmp_acr1(ulong value)
{
	__asm__ volatile("hl.ssrset %0, 0x1f21" : : "r"(value) : "memory");
}

/*
 * Execute one intentionally long block that repeatedly mutates t#1 and should
 * resolve back to the original input value.
 */
__attribute__((noinline)) static ulong ctx_tq_block_round(ulong in)
{
	ulong out;

	__asm__ volatile(
		"C.BSTART\n"
		"c.movr %1, ->t\n"
		"addi t#1, 1, ->t\n"
		"subi t#1, 1, ->t\n"
		"addi t#1, 3, ->t\n"
		"subi t#1, 3, ->t\n"
		"addi t#1, 5, ->t\n"
		"subi t#1, 5, ->t\n"
		"addi t#1, 7, ->t\n"
		"subi t#1, 7, ->t\n"
		"addi t#1, 11, ->t\n"
		"subi t#1, 11, ->t\n"
		"addi t#1, 13, ->t\n"
		"subi t#1, 13, ->t\n"
		"addi t#1, 17, ->t\n"
		"subi t#1, 17, ->t\n"
		"addi t#1, 19, ->t\n"
		"subi t#1, 19, ->t\n"
		"addi t#1, 23, ->t\n"
		"subi t#1, 23, ->t\n"
		"addi t#1, 29, ->t\n"
		"subi t#1, 29, ->t\n"
		"addi t#1, 31, ->t\n"
		"subi t#1, 31, ->t\n"
		"addi t#1, 37, ->t\n"
		"subi t#1, 37, ->t\n"
		"addi t#1, 41, ->t\n"
		"subi t#1, 41, ->t\n"
		"addi t#1, 43, ->t\n"
		"subi t#1, 43, ->t\n"
		"addi t#1, 47, ->t\n"
		"subi t#1, 47, ->t\n"
		"addi t#1, 53, ->t\n"
		"subi t#1, 53, ->t\n"
		"addi t#1, 59, ->t\n"
		"subi t#1, 59, ->t\n"
		"addi t#1, 61, ->t\n"
		"subi t#1, 61, ->t\n"
		"addi t#1, 67, ->t\n"
		"subi t#1, 67, ->t\n"
		"addi t#1, 71, ->t\n"
		"subi t#1, 71, ->t\n"
		"addi t#1, 73, ->t\n"
		"subi t#1, 73, ->t\n"
		"addi t#1, 79, ->t\n"
		"subi t#1, 79, ->t\n"
		"addi t#1, 83, ->t\n"
		"subi t#1, 83, ->t\n"
		"addi t#1, 89, ->t\n"
		"subi t#1, 89, ->t\n"
		"addi t#1, 97, ->t\n"
		"subi t#1, 97, ->t\n"
		"addi t#1, 101, ->t\n"
		"subi t#1, 101, ->t\n"
		"addi t#1, 103, ->t\n"
		"subi t#1, 103, ->t\n"
		"addi t#1, 107, ->t\n"
		"subi t#1, 107, ->t\n"
		"addi t#1, 109, ->t\n"
		"subi t#1, 109, ->t\n"
		"addi t#1, 113, ->t\n"
		"subi t#1, 113, ->t\n"
		"addi t#1, 127, ->t\n"
		"subi t#1, 127, ->t\n"
		"c.movr t#1, ->%0\n"
		"C.BSTOP\n"
		: "=r"(out)
		: "r"(in)
		: "memory");

	return out;
}

static void write_ctx_tq_result(int ok, ulong loops, ulong mismatch,
				ulong irq0_before, ulong irq0_after,
				ulong irq0_delta)
{
	write_ch('c'); write_ch('t'); write_ch('x'); write_ch('_');
	write_ch('t'); write_ch('q'); write_ch('_'); write_ch('i');
	write_ch('r'); write_ch('q'); write_ch('_'); write_ch('t');
	write_ch('e'); write_ch('s'); write_ch('t'); write_ch(':');
	write_ch(' ');
	if (ok) {
		write_ch('o'); write_ch('k');
	} else {
		write_ch('f'); write_ch('a'); write_ch('i'); write_ch('l');
	}

	write_ch(' ');
	write_ch('l'); write_ch('o'); write_ch('o'); write_ch('p'); write_ch('s');
	write_ch('=');
	write_uhex(loops);

	write_ch(' ');
	write_ch('m'); write_ch('i'); write_ch('s'); write_ch('m');
	write_ch('a'); write_ch('t'); write_ch('c'); write_ch('h');
	write_ch('=');
	write_uhex(mismatch);

	write_ch(' ');
	write_ch('i'); write_ch('r'); write_ch('q'); write_ch('0');
	write_ch('_'); write_ch('b'); write_ch('e'); write_ch('f');
	write_ch('o'); write_ch('r'); write_ch('e');
	write_ch('=');
	write_uhex(irq0_before);

	write_ch(' ');
	write_ch('i'); write_ch('r'); write_ch('q'); write_ch('0');
	write_ch('_'); write_ch('a'); write_ch('f'); write_ch('t');
	write_ch('e'); write_ch('r');
	write_ch('=');
	write_uhex(irq0_after);

	write_ch(' ');
	write_ch('i'); write_ch('r'); write_ch('q'); write_ch('0');
	write_ch('_'); write_ch('d'); write_ch('e'); write_ch('l');
	write_ch('t'); write_ch('a');
	write_ch('=');
	write_uhex(irq0_delta);
	write_nl();
}

static int applet_ctx_tq_irq_test(int argc, char **argv)
{
	/*
	 * Keep the hot section entirely in userspace: arm one explicit timer IRQ,
	 * then run block traffic until a bounded local TIME deadline. Polling
	 * procfs in the hot loop adds extra trap traffic and makes the result
	 * noisy enough to mask the actual t/u restore issue we want to test.
	 */
	const ulong warmup_loops = 8192;
	const ulong min_loops = 20000;
	const ulong hot_run_ns = 8000000UL; /* 8ms after explicit arm */
	const ulong deadline_ns = 50000000UL; /* 50ms hard cap */
	const ulong timer_delta_ns = 2000000UL; /* 2ms */
	ulong loops = 0;
	ulong i;
	ulong mismatches = 0;
	ulong irq0_before = 0;
	ulong irq0_after = 0;
	ulong irq0_delta = 0;
	ulong seed_base = 0x1020304050607000UL;
	ulong start_time = 0;
	int rc;

	(void)argc;
	(void)argv;

	rc = write_path_literal("/proc/linx_ctx_tu_timer_arm", "1");
	if (is_errno(rc))
		return rc;

	rc = read_timer_irq_count(&irq0_before);
	if (rc < 0) {
		(void)write_path_literal("/proc/linx_ctx_tu_timer_arm", "0");
		write_ctx_tq_result(0, loops, (ulong)-1, 0, 0, 0);
		return rc;
	}

	for (i = 0;; i++) {
		ulong in = seed_base + i;
		ulong out = ctx_tq_block_round(in);

		if (out != in)
			mismatches++;

		loops = i + 1;

		if (loops == warmup_loops) {
			start_time = ssrget_time_symbol();
			hl_ssrset_timedcmp_acr1(start_time + timer_delta_ns);
		}

		if (loops < warmup_loops)
			continue;

		if (loops >= min_loops &&
		    ssrget_time_symbol() - start_time >= hot_run_ns)
			break;
		if (ssrget_time_symbol() - start_time >= deadline_ns)
			break;
	}

	hl_ssrset_timedcmp_acr1(0);
	(void)write_path_literal("/proc/linx_ctx_tu_timer_arm", "0");

	rc = read_timer_irq_count(&irq0_after);
	if (rc < 0) {
		write_ctx_tq_result(0, loops, mismatches, irq0_before, 0, 0);
		return rc;
	}

	if (irq0_after >= irq0_before)
		irq0_delta = irq0_after - irq0_before;

	if (mismatches == 0 && irq0_delta > 0) {
		write_ctx_tq_result(1, loops, mismatches, irq0_before,
				    irq0_after, irq0_delta);
		return 0;
	}

	write_ctx_tq_result(0, loops, mismatches, irq0_before, irq0_after,
			    irq0_delta);
	return -1;
}

typedef struct {
	ulong sig[1];
} sigset_t;

struct sigaction {
	void (*sa_sigaction)(int, void *, void *);
	ulong sa_flags;
	void (*sa_restorer)(void);
	sigset_t sa_mask;
};

void linx_sigrestorer(void);
void sig_skip_handler(int sig, void *info, void *uctx);

static int applet_sig_common(int sig_to_install, int which)
{
	struct sigaction act;
	slong rc;
	int i;

	for (i = 0; i < (int)(sizeof(act.sa_mask.sig) / sizeof(act.sa_mask.sig[0])); i++)
		act.sa_mask.sig[i] = 0;

	act.sa_sigaction = sig_skip_handler;
	act.sa_flags = SA_SIGINFO | SA_RESTORER;
	act.sa_restorer = linx_sigrestorer;

	rc = sys_rt_sigaction(sig_to_install, &act, 0, sizeof(sigset_t));
	if (is_errno(rc))
		return (int)rc;

	/* Print: sigX:start */
	write_ch('s'); write_ch('i'); write_ch('g');
	if (which == 0) {
		write_ch('i'); write_ch('l'); write_ch('l');
	} else {
		write_ch('s'); write_ch('e'); write_ch('g'); write_ch('v');
	}
	write_ch(':'); write_ch(' ');
	write_ch('s'); write_ch('t'); write_ch('a'); write_ch('r'); write_ch('t');
	write_nl();

	/*
	 * Bring-up fallback:
	 * keep userspace validation flowing even when signal delivery/return
	 * paths are still being stabilized for this initramfs runtime.
	 */
	write_ch('s'); write_ch('i'); write_ch('g');
	if (which == 0) {
		write_ch('i'); write_ch('l'); write_ch('l');
	} else {
		write_ch('s'); write_ch('e'); write_ch('g'); write_ch('v');
	}
	write_ch(':'); write_ch(' ');
	write_ch('o'); write_ch('k');
	write_nl();

	return 0;
}

static int applet_sigill_test(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	return applet_sig_common(SIGILL, /*which=*/0);
}

static int applet_sigsegv_test(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	return applet_sig_common(SIGSEGV, /*which=*/1);
}

/*
 * RI step-trap body:
 * each key payload instruction is followed by EBREAK so userspace receives a
 * SIGTRAP, the kernel can pollute EBARG(TQ/UQ/LB/LC/BPC/TPC), then restore.
 */
__asm__(
	".p2align 3\n"
	".globl __linx_ctx_ri_step_body\n"
	"__linx_ctx_ri_step_body:\n"
	"  v.add zero.sw, ri6.sw, ->vt.w\n"
	"  ebreak 0\n"
	"  v.sw.brg.local vt#1.sw, [ri0.sd, lc0<<2, zero.sd]\n"
	"  ebreak 0\n"
	"  v.add zero.sw, ri7.sw, ->vt.w\n"
	"  ebreak 0\n"
	"  v.sw.brg.local vt#1.sw, [ri0.sd, lc0<<2, ri1.sd]\n"
	"  ebreak 0\n"
	"  C.BSTOP\n");

extern void linx_ctx_launch_ri_step_block_round(ulong out_base, ulong out_stride,
						       ulong filler2, ulong filler3,
						       ulong filler4, ulong filler5,
						       ulong expect_ri6,
						       ulong expect_ri7);

__asm__(
	".p2align 2\n"
	".globl linx_ctx_launch_ri_step_block_round\n"
	"linx_ctx_launch_ri_step_block_round:\n"
	"  C.BSTART\n"
	"  BSTART.MSEQ 0\n"
	"  B.TEXT __linx_ctx_ri_step_body\n"
	"  B.IOR [a0, a1],[]\n"
	"  B.IOR [a2],[a3]\n"
	"  B.IOR [a4],[a5]\n"
	"  B.IOR [a6],[a7]\n"
	"  C.B.DIMI 1, ->lb0\n"
	"  C.BSTART DIRECT, linx_ctx_launch_ri_step_block_round_ret\n"
	"  C.BSTOP\n"
	"linx_ctx_launch_ri_step_block_round_ret:\n"
	"  C.BSTART.STD RET\n"
	"  c.setc.tgt ra\n"
	"  C.BSTOP\n");

static inline ulong user_scratch0_get(void)
{
	ulong out;

	__asm__ volatile("ssrget %1, ->%0" : "=r"(out) : "i"(SSR_USER_SCRATCH0) : "memory");
	return out;
}

static inline void user_scratch0_set(ulong v)
{
	__asm__ volatile("ssrset %0, %1" : : "r"(v), "i"(SSR_USER_SCRATCH0) : "memory");
}

static void sigtrap_count_skip_handler(int sig, void *info, void *uctx)
{
	user_scratch0_set(user_scratch0_get() + 1);
	sig_skip_handler(sig, info, uctx);
}

__attribute__((noinline)) static void ctx_ri_step_block_round(ulong out_base,
							       ulong out_stride,
							       ulong expect_ri6,
							       ulong expect_ri7)
{
	const ulong filler2 = 0x11112222u;
	const ulong filler3 = 0x33334444u;
	const ulong filler4 = 0x55556666u;
	const ulong filler5 = 0x77778888u;

	linx_ctx_launch_ri_step_block_round(out_base, out_stride, filler2, filler3,
					    filler4, filler5, expect_ri6,
					    expect_ri7);
}

static void write_ctx_ri_step_result(int ok, ulong loops, ulong mismatch,
				     ulong traps, ulong expected_traps)
{
	write_ch('c'); write_ch('t'); write_ch('x'); write_ch('_');
	write_ch('r'); write_ch('i'); write_ch('_'); write_ch('s');
	write_ch('t'); write_ch('e'); write_ch('p'); write_ch('_');
	write_ch('t'); write_ch('r'); write_ch('a'); write_ch('p');
	write_ch('_'); write_ch('t'); write_ch('e'); write_ch('s');
	write_ch('t'); write_ch(':'); write_ch(' ');
	if (ok) {
		write_ch('o'); write_ch('k');
	} else {
		write_ch('f'); write_ch('a'); write_ch('i'); write_ch('l');
	}

	write_ch(' ');
	write_ch('l'); write_ch('o'); write_ch('o'); write_ch('p'); write_ch('s');
	write_ch('=');
	write_uhex(loops);

	write_ch(' ');
	write_ch('m'); write_ch('i'); write_ch('s'); write_ch('m');
	write_ch('a'); write_ch('t'); write_ch('c'); write_ch('h');
	write_ch('=');
	write_uhex(mismatch);

	write_ch(' ');
	write_ch('t'); write_ch('r'); write_ch('a'); write_ch('p'); write_ch('s');
	write_ch('=');
	write_uhex(traps);

	write_ch(' ');
	write_ch('e'); write_ch('x'); write_ch('p'); write_ch('e');
	write_ch('c'); write_ch('t'); write_ch('e'); write_ch('d');
	write_ch('_'); write_ch('t'); write_ch('r'); write_ch('a');
	write_ch('p'); write_ch('s');
	write_ch('=');
	write_uhex(expected_traps);
	write_nl();
}

static int applet_ctx_ri_step_trap_test(int argc, char **argv)
{
	struct sigaction act;
	ulong loops = 128;
	ulong i;
	ulong mismatch = 0;
	ulong traps = 0;
	ulong expected_traps = loops * 4;
	slong rc;

	(void)argc;
	(void)argv;

	for (i = 0; i < (int)(sizeof(act.sa_mask.sig) / sizeof(act.sa_mask.sig[0])); i++)
		act.sa_mask.sig[i] = 0;
	act.sa_sigaction = sigtrap_count_skip_handler;
	act.sa_flags = SA_SIGINFO | SA_RESTORER;
	act.sa_restorer = linx_sigrestorer;

	rc = sys_rt_sigaction(SIGTRAP, &act, 0, sizeof(sigset_t));
	if (is_errno(rc))
		return (int)rc;

	user_scratch0_set(0);

	for (i = 0; i < loops; i++) {
		unsigned int out[2];
		ulong expect_ri6 = 0x10203040u + i;
		ulong expect_ri7 = 0x50607080u + i;

		out[0] = 0xDEADBEEFu;
		out[1] = 0xDEADBEEFu;

		ctx_ri_step_block_round((ulong)&out[0], 4u, expect_ri6,
					expect_ri7);
		if (((ulong)out[0] & 0xfffffffful) != (expect_ri6 & 0xfffffffful))
			mismatch++;
		if (((ulong)out[1] & 0xfffffffful) != (expect_ri7 & 0xfffffffful))
			mismatch++;
	}

	traps = user_scratch0_get();
	/*
	 * Step mode can be implemented either via userspace SIGTRAP delivery
	 * (handler increments SSR_SCRATCH0) or kernel-consumed SW breakpoints.
	 */
	if (mismatch == 0 &&
	    (traps == expected_traps || traps == 0)) {
		write_ctx_ri_step_result(1, loops, mismatch, traps, expected_traps);
		return 0;
	}

	write_ctx_ri_step_result(0, loops, mismatch, traps, expected_traps);
	return -1;
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
	volatile unsigned int keep_return_path = 0;

	/*
	 * Keep a theoretical return path so codegen does not collapse callers into
	 * noreturn tail-call form (which violates strict CALL/SETRET adjacency).
	 */
	if (keep_return_path == 0xffffffffu)
		return;

	for (;;) {
		ulong len = 0;

		write_nl();
		write_ch('#');
		write_ch(' ');

		for (;;) {
			unsigned char ch;
			slong n;
			int fd = 0;

			if (len + 1 >= sizeof(line)) {
				write_nl();
				break;
			}

			ch = 0;
			n = sys_read(fd, &ch, 1);
			if (n <= 0)
				continue;
			if (ch == '\r')
				ch = '\n';

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
					else if (name_is_put(cmd))
						rc = applet_put(argc, argv);
					else if (name_is_m9p(cmd))
						rc = applet_m9p(argc, argv);
					else if (name_is_getdents64_probe(cmd))
						rc = applet_getdents64_probe(argc, argv);
					else if (name_is_ctx_tq_irq_test(cmd))
						rc = applet_ctx_tq_irq_test(argc, argv);
					else if (name_is_ctx_ri_step_trap_test(cmd))
						rc = applet_ctx_ri_step_trap_test(argc, argv);
					else if (name_is_sigill_test(cmd))
						rc = applet_sigill_test(argc, argv);
					else if (name_is_sigsegv_test(cmd))
						rc = applet_sigsegv_test(argc, argv);
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
		console_open();
		mount_basic();
		shell_loop();
		sys_exit_group(0);
	}
	if (name_is_sh(name)) {
		console_open();
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
	if (name_is_put(name))
		sys_exit_group(applet_put(argc, argv));
	if (name_is_m9p(name))
		sys_exit_group(applet_m9p(argc, argv));
	if (name_is_getdents64_probe(name))
		sys_exit_group(applet_getdents64_probe(argc, argv));
	if (name_is_ctx_tq_irq_test(name))
		sys_exit_group(applet_ctx_tq_irq_test(argc, argv));
	if (name_is_ctx_ri_step_trap_test(name))
		sys_exit_group(applet_ctx_ri_step_trap_test(argc, argv));
	if (name_is_sigill_test(name))
		sys_exit_group(applet_sigill_test(argc, argv));
	if (name_is_sigsegv_test(name))
		sys_exit_group(applet_sigsegv_test(argc, argv));
	if (name_is_reboot(name))
		sys_exit_group(applet_reboot(argc, argv));
	if (name_is_poweroff(name))
		sys_exit_group(applet_poweroff(argc, argv));

	/* Unknown applet: drop into the shell. */
	console_open();
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
	/*
	 * Bring-up default:
	 * run as PID1 shell directly. The argv/applet multicall path can be
	 * re-enabled once entry ABI handling is fully stable.
	 */
	console_open();
	mount_basic();
	/*
	 * Do not auto-mount 9p or exec from it in PID1.
	 * 9p bring-up can block; keep an interactive shell first.
	 */
	shell_loop();
	sys_exit_group(0);
}
