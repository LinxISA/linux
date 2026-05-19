/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_BUG_H
#define _ASM_RISCV_BUG_H

#include <linux/compiler.h>
#include <linux/const.h>
#include <linux/types.h>

#include <asm/asm.h>
#include <asm/block-def.h>
#include <asm/qemu_debug.h>

#define __INSN_LENGTH_MASK  _UL(0x1)
#define __INSN_LENGTH_32    _UL(0x1)
#define __COMPRESSED_INSN_MASK	_UL(0xffff)

#define __EBREAK_IMM_32_MASK	_UL(0xF000000)
#define __EBREAK_IMM_16_MASK	_UL(0x7C0)

#define __EBREAK_IMM_32_SHIFT	(24)
#define __EBREAK_IMM_16_SHIFT	(6)

#define EBREAK_IMM_32(insn)	(((insn) & __EBREAK_IMM_32_MASK) >> __EBREAK_IMM_32_SHIFT)
#define EBREAK_IMM_16(insn)	(((insn) & __EBREAK_IMM_16_MASK) >> __EBREAK_IMM_16_SHIFT)

#define __BUG_INSN_32	_UL(0x0010102B) /* ebreak 0 */
#define __BUG_INSN_16	_UL(0xC02C) /* c.ebreak 0 */

#define GET_INSN_LENGTH(insn)						\
({									\
	unsigned long __len;						\
	__len = ((insn & __INSN_LENGTH_MASK) == __INSN_LENGTH_32) ?	\
		4UL : 2UL;						\
	__len;								\
})

typedef u32 bug_insn_t;

#ifdef CONFIG_GENERIC_BUG_RELATIVE_POINTERS
#define __BUG_ENTRY_ADDR	RISCV_INT " 1b - 2b"
#define __BUG_ENTRY_FILE	RISCV_INT " %0 - 2b"
#else
#define __BUG_ENTRY_ADDR	RISCV_PTR " 1b"
#define __BUG_ENTRY_FILE	RISCV_PTR " %0"
#endif

#ifdef CONFIG_DEBUG_BUGVERBOSE
#define __BUG_ENTRY			\
	__BUG_ENTRY_ADDR "\n\t"		\
	__BUG_ENTRY_FILE "\n\t"		\
	RISCV_SHORT " %1\n\t"		\
	RISCV_SHORT " %2"
#else
#define __BUG_ENTRY			\
	__BUG_ENTRY_ADDR "\n\t"		\
	RISCV_SHORT " %2"
#endif

#define ASM_EBREAK_BLOCK	\
	"BSTART.sys fall\n"	\
		"ebreak 0\n"	\

#ifdef CONFIG_QEMU_BUG_FLAGS

#define __BUG_FLAGS(flags) do {					\
	qemu_debug_string(__LINE__, __FILE__);			\
	qemu_debug_state(__LINE__);				\
	qemu_debug_stop(__LINE__);				\
} while (0)

#else /* CONFIG_QEMU_BUG_FLAGS */

#ifdef CONFIG_GENERIC_BUG
#define __BUG_FLAGS(flags)					\
do {								\
	__asm__ __volatile__ (					\
		"1:\n\t"					\
			ASM_EBREAK_BLOCK			\
			".pushsection __bug_table,\"aw\"\n\t"	\
		"2:\n\t"					\
			__BUG_ENTRY "\n\t"			\
			".org 2b + %3\n\t"			\
			".popsection"				\
		:						\
		: "i" (__FILE__), "i" (__LINE__),		\
		  "i" (flags),					\
		  "i" (sizeof(struct bug_entry)));		\
} while (0)
#else /* CONFIG_GENERIC_BUG */
#define __BUG_FLAGS(flags) do {					\
	__asm__ __volatile__ (ASM_EBREAK_BLOCK);		\
} while (0)
#endif /* CONFIG_GENERIC_BUG */

#endif /* CONFIG_QEMU_BUG_FLAGS */

#define BUG() do {						\
	__BUG_FLAGS(0);						\
	unreachable();						\
} while (0)

#define BUG_ON(condition) do { if (unlikely(condition)) BUG(); } while (0)

#define __WARN_FLAGS(flags) __BUG_FLAGS(BUGFLAG_WARNING|(flags))

#define HAVE_ARCH_BUG
#define HAVE_ARCH_BUG_ON

#include <asm-generic/bug.h>

struct pt_regs;
struct task_struct;

void __show_regs(struct pt_regs *regs);
void die(struct pt_regs *regs, const char *str);
void do_trap(struct pt_regs *regs, int signo, int code, unsigned long addr);

#endif /* _ASM_RISCV_BUG_H */
