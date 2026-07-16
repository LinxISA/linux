/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Huawei Technologies Co., Ltd
 */

#include <linux/linkage.h>
#include <asm/qemu_debug.h>

/* NOTE: qemu_debug_xxx block encode refer to LinxBlockModel */
/*
 * void qemu_debug_stop(int id);
 * stop qemu and can use monitor to diagnose
 */
ENTRY(qemu_debug_stop)
	qemu_debug_insn LD_ATTR_BIT_STOP_VM, 0
	riscv_nop_ret
ENDPROC(qemu_debug_stop)

/*
 * void qemu_debug_state(int id);
 * print cpu state by qemu
 */
ENTRY(qemu_debug_state)
	qemu_debug_insn LD_ATTR_BIT_DUMP_STATE, 0
	riscv_nop_ret
ENDPROC(qemu_debug_state)

/*
 * void qemu_debug_memory(int id, unsigned long addr, unsigned int size);
 * print memory contents from @addr upto @size bytes with a simple @id by qemu
 */
ENTRY(qemu_debug_memory)
	qemu_debug_insn LD_ATTR_BIT_DUMP_MEM, 0
	riscv_nop_ret
ENDPROC(qemu_debug_memory)

/*
 * void qemu_debug_hit(int id);
 * print a simple @id by qemu
 */
ENTRY(qemu_debug_hit)
	qemu_debug_insn LD_ATTR_BIT_SHOW_ID, 0
	riscv_nop_ret
ENDPROC(qemu_debug_hit)

/*
 * void qemu_debug_string(int id, const char *str);
 * print null-terminating @str with a simple @id by qemu
 */
ENTRY(qemu_debug_string)
	qemu_debug_insn LD_ATTR_BIT_DUMP_STRING, 0
	riscv_nop_ret
ENDPROC(qemu_debug_string)

/*
 * void qemu_debug_preempt_report(int id, int count);
 * report the preempt count @count to qemu
 */
ENTRY(qemu_debug_preempt_report)
	qemu_debug_insn LD_ATTR_BIT_PREEMPT_REPORT, 0
	riscv_nop_ret
ENDPROC(qemu_debug_preempt_report)

/*
 * void qemu_debug_log_disable(int id);
 * disable qemu log
 */
ENTRY(qemu_debug_log_disable)
	qemu_debug_insn LD_ATTR_BIT_LOG_DISABLE, 0
	riscv_nop_ret
ENDPROC(qemu_debug_log_disable)

/*
 * void qemu_debug_log_enable(int id);
 * enable qemu log
 */
ENTRY(qemu_debug_log_enable)
	qemu_debug_insn LD_ATTR_BIT_LOG_ENABLE, 0
	riscv_nop_ret
ENDPROC(qemu_debug_log_enable)
