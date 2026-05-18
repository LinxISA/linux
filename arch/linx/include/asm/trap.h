/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2022 Huawei Technologies Co., Ltd.
 */

#ifndef _LINX_TRAP_H
#define _LINX_TRAP_H

#include <asm/ssr.h>

static inline bool is_insn_abort(unsigned long ecause)
{
	return (ECAUSE_TRAPNUM(ecause) == ECAUSE_TRAPNUM_INSN_EXP);
}

static inline bool is_insn_page_fault(unsigned long ecause)
{
	return (is_insn_abort(ecause) && (ECAUSE_SYNDROME(ecause) == ECAUSE_INSN_SYD_PAGE_FAULT));
}

static inline bool is_data_abort(unsigned long ecause)
{
	return (ECAUSE_TRAPNUM(ecause) == ECAUSE_TRAPNUM_DATA_EXP);
}

static inline bool is_write_abort(unsigned long ecause)
{
	return (is_data_abort(ecause) && 
		((ECAUSE_SYNDROME(ecause) == ECAUSE_DATA_SYD_ST_AOP_ACCESS_FAULT) ||
		 (ECAUSE_SYNDROME(ecause) == ECAUSE_DATA_SYD_ST_AOP_MISALIGN_FAULT) ||
		 (ECAUSE_SYNDROME(ecause) == ECAUSE_DATA_SYD_ST_AOP_PAGE_FAULT)));
}

static inline bool is_data_page_fault(unsigned long ecause)
{
	return (is_data_abort(ecause) && 
		((ECAUSE_SYNDROME(ecause) == ECAUSE_DATA_SYD_LD_PAGE_FAULT) ||
		 (ECAUSE_SYNDROME(ecause) == ECAUSE_DATA_SYD_ST_AOP_PAGE_FAULT)));
}

static inline bool is_write_page_fault(unsigned long ecause)
{
	return (is_data_abort(ecause) && 
		(ECAUSE_SYNDROME(ecause) == ECAUSE_DATA_SYD_ST_AOP_PAGE_FAULT));
}

static inline bool is_syscall(unsigned long ecause)
{
	return (ECAUSE_TRAPNUM(ecause) == ECAUSE_TRAPNUM_SCALL_EXP);
}

#endif /* _LINX_TRAP_H */
