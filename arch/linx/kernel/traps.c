// SPDX-License-Identifier: GPL-2.0-only

#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/errno.h>
#include <linux/fdtable.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>

#include <asm/debug_uart.h>
#include <asm/irq_regs.h>
#include <asm/ptrace.h>
#include <asm/ssr.h>

/*
 * Called from the EVBASE trap vector.
 *
 * Current bring-up model:
 * - QEMU raises a single timer interrupt (IRQ0) via IPENDING_ACR0 bit0.
 * - The trap vector saves GPRs into pt_regs and passes the pointer here.
 */

extern void linx_timer_handle_irq(void);

asmlinkage void linx_do_trap(struct pt_regs *regs);

extern void * const sys_call_table[__NR_syscalls];

typedef long (*linx_syscall_fn_t)(unsigned long, unsigned long, unsigned long,
				  unsigned long, unsigned long, unsigned long);

static void linx_handle_syscall(struct pt_regs *regs)
{
	unsigned long nr = regs->regs[PTR_R9];
	static bool once;
	unsigned long ksp;
	unsigned long cur_sp;
	unsigned long arg0, arg1, arg2, arg3, arg4, arg5;

	if (!once) {
		once = true;
		linx_debug_uart_puts("[SYSC]\n");
		asm volatile("ssrget 0x0f04, ->%0" : "=r"(ksp));
		asm volatile("c.movr sp, ->%0" : "=r"(cur_sp));
		linx_debug_uart_puts("sp=");
		linx_debug_uart_puthex_ulong(cur_sp);
		linx_debug_uart_puts("\n");
		linx_debug_uart_puts("ksp=");
		linx_debug_uart_puthex_ulong(ksp);
		linx_debug_uart_puts("\n");
		linx_debug_uart_puts("cur=");
		linx_debug_uart_puthex_ulong((unsigned long)current);
		linx_debug_uart_puts(" files=");
		linx_debug_uart_puthex_ulong((unsigned long)current->files);
		linx_debug_uart_puts("\n");
		linx_debug_uart_puts("tinfo=");
		linx_debug_uart_puthex_ulong((unsigned long)current_thread_info());
		linx_debug_uart_puts("\n");
		linx_debug_uart_puts("nr=");
		linx_debug_uart_puthex_ulong(nr);
		if (nr < __NR_syscalls) {
			linx_debug_uart_puts(" fn=");
			linx_debug_uart_puthex_ulong((unsigned long)sys_call_table[nr]);
		}
		linx_debug_uart_puts(" a0=");
		linx_debug_uart_puthex_ulong(regs->regs[PTR_R2]);
		linx_debug_uart_puts(" a1=");
		linx_debug_uart_puthex_ulong(regs->regs[PTR_R3]);
		linx_debug_uart_puts(" a2=");
		linx_debug_uart_puthex_ulong(regs->regs[PTR_R4]);
		linx_debug_uart_puts("\n");
		if (current->files) {
			const struct fdtable *fdt = files_fdtable(current->files);
			const int files_count = atomic_read(&current->files->count);
			const int files_count_acq = atomic_read_acquire(&current->files->count);
			const unsigned int next_fd = current->files->next_fd;
			struct file *raw0 = files_lookup_fd_raw(current->files, 0);

			linx_debug_uart_puts("files.count=");
			linx_debug_uart_puthex_ulong((unsigned long)files_count);
			linx_debug_uart_puts(" acq=");
			linx_debug_uart_puthex_ulong((unsigned long)files_count_acq);
			linx_debug_uart_puts(" next_fd=");
			linx_debug_uart_puthex_ulong((unsigned long)next_fd);
			linx_debug_uart_puts(" raw_fd0=");
			linx_debug_uart_puthex_ulong((unsigned long)raw0);
			linx_debug_uart_puts("\n");

			linx_debug_uart_puts("fd[0]=");
			linx_debug_uart_puthex_ulong((unsigned long)fdt->fd[0]);
			linx_debug_uart_puts(" fd[1]=");
			linx_debug_uart_puthex_ulong((unsigned long)fdt->fd[1]);
			linx_debug_uart_puts(" fd[2]=");
			linx_debug_uart_puthex_ulong((unsigned long)fdt->fd[2]);
			linx_debug_uart_puts("\n");
			if (fdt->fd[0]) {
				struct file *f = fdt->fd[0];

				linx_debug_uart_puts("f_mode=");
				linx_debug_uart_puthex_ulong((unsigned long)f->f_mode);
				linx_debug_uart_puts(" f_flags=");
				linx_debug_uart_puthex_ulong((unsigned long)f->f_flags);
				linx_debug_uart_puts(" f_op=");
				linx_debug_uart_puthex_ulong((unsigned long)f->f_op);
				linx_debug_uart_puts("\n");
			}
		}
	}

	arg0 = regs->regs[PTR_R2];
	arg1 = regs->regs[PTR_R3];
	arg2 = regs->regs[PTR_R4];
	arg3 = regs->regs[PTR_R5];
	arg4 = regs->regs[PTR_R6];
	arg5 = regs->regs[PTR_R7];

	/* Preserve arg0; a0 is also the return value register. */
	regs->orig_a0 = arg0;
	regs->regs[PTR_R2] = (unsigned long)-ENOSYS;

	if (nr < __NR_syscalls) {
		long ret = -ENOSYS;

		/*
		 * Linx bring-up: avoid calling sys_* entry points through an
		 * untyped function pointer. Some syscalls (e.g. openat) take
		 * narrower argument types and have been observed to misbehave.
		 */
		switch (nr) {
		case __NR_openat: {
			static bool once_openat;

			if (!once_openat) {
				once_openat = true;
				linx_debug_uart_puts("[openat] a0=");
				linx_debug_uart_puthex_ulong(arg0);
				linx_debug_uart_puts(" a1=");
				linx_debug_uart_puthex_ulong(arg1);
				linx_debug_uart_puts(" a2=");
				linx_debug_uart_puthex_ulong(arg2);
				linx_debug_uart_puts(" a3=");
				linx_debug_uart_puthex_ulong(arg3);
				linx_debug_uart_puts("\n");
			}

			ret = sys_openat((int)arg0, (const char __user *)arg1,
					 (int)arg2, (umode_t)arg3);
			if (once_openat) {
				linx_debug_uart_puts("[openat] ret=");
				linx_debug_uart_puthex_ulong((unsigned long)ret);
				linx_debug_uart_puts("\n");
			}
			break;
		}
		case __NR_close:
			ret = sys_close((unsigned int)arg0);
			break;
		case __NR_read:
		{
			static int dbg_reads;
			struct file *file = NULL;
			int ok = 0;

			if (dbg_reads < 8) {
				if (current->files)
					file = files_lookup_fd_raw(current->files,
								   (unsigned int)arg0);
				ok = access_ok((void __user *)arg1, (unsigned long)arg2);

				linx_debug_uart_puts("[read] fd=");
				linx_debug_uart_puthex_ulong(arg0);
				linx_debug_uart_puts(" buf=");
				linx_debug_uart_puthex_ulong(arg1);
				linx_debug_uart_puts(" cnt=");
				linx_debug_uart_puthex_ulong(arg2);
				linx_debug_uart_puts(" ok=");
				linx_debug_uart_puthex_ulong((unsigned long)ok);
				linx_debug_uart_puts(" file=");
				linx_debug_uart_puthex_ulong((unsigned long)file);
				if (file) {
					linx_debug_uart_puts(" f_pos=");
					linx_debug_uart_puthex_ulong((unsigned long)file->f_pos);
					linx_debug_uart_puts(" f_mode=");
					linx_debug_uart_puthex_ulong((unsigned long)file->f_mode);
					linx_debug_uart_puts(" f_op=");
					linx_debug_uart_puthex_ulong((unsigned long)file->f_op);
					if (file->f_op) {
						linx_debug_uart_puts(" read=");
						linx_debug_uart_puthex_ulong((unsigned long)file->f_op->read);
						linx_debug_uart_puts(" read_iter=");
						linx_debug_uart_puthex_ulong((unsigned long)file->f_op->read_iter);
					}
				}
				linx_debug_uart_puts("\n");
			}
			ret = sys_read((unsigned int)arg0, (char __user *)arg1,
				       (size_t)arg2);
			if (dbg_reads < 8) {
				if (file) {
					linx_debug_uart_puts("[read] f_pos_after=");
					linx_debug_uart_puthex_ulong((unsigned long)file->f_pos);
					linx_debug_uart_puts(" ");
				}
				linx_debug_uart_puts("[read] ret=");
				linx_debug_uart_puthex_ulong((unsigned long)ret);
				linx_debug_uart_puts("\n");
			}
			dbg_reads++;
			break;
		}
		case __NR_getdents64:
		{
			static int dbg_getdents;

			if (dbg_getdents < 8) {
				linx_debug_uart_puts("[getdents] fd=");
				linx_debug_uart_puthex_ulong(arg0);
				linx_debug_uart_puts(" dirp=");
				linx_debug_uart_puthex_ulong(arg1);
				linx_debug_uart_puts(" cnt=");
				linx_debug_uart_puthex_ulong(arg2);
				linx_debug_uart_puts("\n");
			}
			ret = sys_getdents64((unsigned int)arg0,
					     (struct linux_dirent64 __user *)arg1,
					     (unsigned int)arg2);
			if (dbg_getdents < 8) {
				linx_debug_uart_puts("[getdents] ret=");
				linx_debug_uart_puthex_ulong((unsigned long)ret);
				linx_debug_uart_puts("\n");
			}
			dbg_getdents++;
			break;
		}
		case __NR_mount:
			ret = sys_mount((char __user *)arg0, (char __user *)arg1,
					(char __user *)arg2, arg3,
					(void __user *)arg4);
			break;
		case __NR_reboot:
			ret = sys_reboot((int)arg0, (int)arg1, (unsigned int)arg2,
					 (void __user *)arg3);
			break;
		case __NR_exit:
			sys_exit((int)arg0);
			break;
		case __NR_exit_group:
			sys_exit_group((int)arg0);
			break;
		default: {
			linx_syscall_fn_t fn =
				(linx_syscall_fn_t)sys_call_table[nr];

			ret = fn(arg0, arg1, arg2, arg3, arg4, arg5);
			break;
		}
		}

		regs->regs[PTR_R2] = (unsigned long)ret;
	}

	/*
	 * QEMU ACRC saves EBPC=block start and EBPCN=pc_next. Returning via ACRE
	 * resumes at EBPC, so advance EBPC to EBPCN to skip the trapping insn.
	 */
	linx_ssr_write_ebpc_acr0(regs->ebpcn);
	regs->regs[PTR_PC] = regs->ebpcn;
}

asmlinkage void linx_do_trap(struct pt_regs *regs)
{
	static bool once;

	if (!once) {
		once = true;
		linx_debug_uart_puts("[TRAP]\n");
	}

	u64 pending = linx_ssr_read_ipending_acr0();

	if (pending & 1ull) {
		struct pt_regs *old_regs = set_irq_regs(regs);

		/* Acknowledge IRQ0 (timer0). */
		linx_ssr_write_eoiei_acr0(0);

		irq_enter();
		linx_timer_handle_irq();
		irq_exit();

		set_irq_regs(old_regs);

		/*
		 * QEMU models EBPC=block start and EBPCN=pc_next for trap
		 * delivery. Returning via ACRE resumes at EBPC, so advance EBPC
		 * to EBPCN to resume after the interrupted instruction/block.
		 */
		linx_ssr_write_ebpc_acr0(regs->ebpcn);
		regs->regs[PTR_PC] = regs->ebpcn;
		return;
	}

	if (regs->trapno == 16 /* E_SCALL */) {
		/*
		 * Bring-up syscall path: treat ACRC service requests as Linux
		 * syscalls. (request_type is currently ignored.)
		 */
		if (user_mode(regs)) {
			linx_handle_syscall(regs);
			return;
		}

		pr_err("linx: syscall trap from non-user context: etpc=0x%llx ecstate=0x%lx\n",
		       regs->etpc, regs->ecstate);
		return;
	}

	pr_err("linx: unexpected trap/irq: ipending=0x%llx ebpc=0x%llx etpc=0x%llx\n",
	       pending, linx_ssr_read_ebpc_acr0(), linx_ssr_read_etpc_acr0());
}
