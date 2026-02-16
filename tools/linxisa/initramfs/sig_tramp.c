/*
 * Signal trampoline helpers for LinxISA bring-up initramfs (no libc).
 *
 * Keep these in a separate translation unit so function-pointer materialization
 * can use the external-symbol relocation model (required for ET_DYN bring-up).
 */

typedef unsigned long ulong;

enum {
	PTR_PC = 24,
	NUM_PTRACE_REG = 25,
};

typedef struct {
	ulong sig[1];
} sigset_t;

struct user_pt_regs {
	ulong regs[NUM_PTRACE_REG];
};

struct sigcontext {
	struct user_pt_regs sc_regs;
};

typedef struct {
	void *ss_sp;
	int ss_flags;
	ulong ss_size;
} stack_t;

struct ucontext {
	ulong uc_flags;
	struct ucontext *uc_link;
	stack_t uc_stack;
	sigset_t uc_sigmask;
	unsigned char __unused[1024 / 8 - sizeof(sigset_t)];
	struct sigcontext uc_mcontext;
};

__attribute__((naked)) void linx_sigrestorer(void)
{
	__asm__ volatile(
		"C.BSTART\n"
		"addi zero, 139, ->a7\n"
		"acrc 1\n"
		"C.BSTOP\n"
		: : : "memory");
}

void sig_skip_handler(int sig, void *info, void *uctx)
{
	struct ucontext *uc;
	(void)sig;
	(void)info;

	uc = (struct ucontext *)uctx;
	uc->uc_mcontext.sc_regs.regs[PTR_PC] =
		uc->uc_mcontext.sc_regs.regs[PTR_PC] + 4;
}
