/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_RISCV_PROTOTYPES_H
#define _ASM_RISCV_PROTOTYPES_H

#include <linux/ftrace.h>
#include <asm-generic/asm-prototypes.h>

long long __lshrti3(long long a, int b);
long long __ashrti3(long long a, int b);
long long __ashlti3(long long a, int b);

asmlinkage void do_page_fault(struct pt_regs *regs);

#define DECLARE_DO_ERROR_INFO(name)	asmlinkage void name(struct pt_regs *regs)

DECLARE_DO_ERROR_INFO(do_trap_unknown);
DECLARE_DO_ERROR_INFO(do_trap_break);
DECLARE_DO_ERROR_INFO(do_trap_block_exception);
DECLARE_DO_ERROR_INFO(do_trap_data_exception);
DECLARE_DO_ERROR_INFO(do_trap_insn_exception);

#endif /* _ASM_RISCV_PROTOTYPES_H */
