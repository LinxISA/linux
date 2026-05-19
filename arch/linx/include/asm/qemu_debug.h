/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2022 Huawei Technologies Co., Ltd.
 */

#ifndef _QEMU_DEBUG_H
#define _QEMU_DEBUG_H

#ifndef __ASSEMBLY__

#include <linux/compiler.h>
#include <linux/const.h>
#include <linux/types.h>

#ifdef CONFIG_HAVE_QEMU_DEBUG

extern void qemu_debug_stop(int id);
extern void qemu_debug_state(int id);
extern void qemu_debug_hit(int id);
extern void qemu_debug_memory(int id, unsigned long addr, unsigned int size);
extern void qemu_debug_string(int id, const char *str);
extern void qemu_debug_preempt_report(int id, int count);
extern void qemu_debug_log_enable(int id);
extern void qemu_debug_log_disable(int id);

extern __printf(3, 4)
int snprintf(char *buf, size_t size, const char *fmt, ...);

#define qemu_debug_line() qemu_debug_string(__LINE__, __FILE__)

#define qemu_debug_snprintf(buf, size, fmt, ...) do {			\
	snprintf((buf), (size), "[%s@%s:%d] "fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__);		\
	qemu_debug_string(__LINE__, buf);				\
} while (0)

#define qemu_debug_nprintf(size, fmt, ...) do {				\
	char __buf[size];						\
	qemu_debug_snprintf(__buf, (size), fmt, ##__VA_ARGS__);		\
} while (0)

#define qemu_debug_printf(fmt, ...) do {				\
	qemu_debug_nprintf((512 + 2 * sizeof(fmt""#__VA_ARGS__)),	\
			fmt, ##__VA_ARGS__);				\
} while (0)

#else /* CONFIG_HAVE_QEMU_DEBUG */

#define qemu_debug_stop(...)
#define qemu_debug_state(...)
#define qemu_debug_hit(...)
#define qemu_debug_memory(...)
#define qemu_debug_string(...)
#define qemu_debug_line(...)
#define qemu_debug_snprintf(...)
#define qemu_debug_nprintf(...)
#define qemu_debug_printf(...)
#define qemu_debug_preempt_report(...)
#define qemu_debug_log_enable(...)
#define qemu_debug_log_disable(...)

#endif /* CONFIG_HAVE_QEMU_DEBUG */

#else /* __ASSEMBLY__ */

#ifdef CONFIG_HAVE_QEMU_DEBUG

/* NOTE: qemu_debug_insn block encode refer to LinxBlockModel */
#define LD_ATTR_BIT_STOP_VM        0x001
#define LD_ATTR_BIT_DUMP_STATE     0x002
#define LD_ATTR_BIT_DUMP_MEM       0x004
#define LD_ATTR_BIT_SHOW_ID        0x008
#define LD_ATTR_BIT_DUMP_STRING    0x010
#define LD_ATTR_BIT_PREEMPT_REPORT 0x020
#define LD_ATTR_BIT_LOG_ENABLE     0x040
#define LD_ATTR_BIT_LOG_DISABLE    0x080

.macro qemu_debug_insn battr, param
	.4byte 0b0001011
	.4byte (\battr) | 0x200
	.8byte \param
.endm

.macro riscv_nop_ret
	.word 0x00018082, 0x00010001, 0x00010001, 0x00010001
.endm

.macro block_qemu_debug_stop param
	qemu_debug_insn LD_ATTR_BIT_STOP_VM, \param
	/* no ret */
.endm

.macro block_qemu_debug_state param
	qemu_debug_insn LD_ATTR_BIT_DUMP_STATE, \param
	/* no ret */
.endm

#else /* CONFIG_HAVE_QEMU_DEBUG */

.macro block_qemu_debug_stop param
	/* empty */
.endm

.macro block_qemu_debug_state param
	/* empty */
.endm

#endif /* CONFIG_HAVE_QEMU_DEBUG */

#endif /* __ASSEMBLY__ */

#endif /* _QEMU_DEBUG_H */
