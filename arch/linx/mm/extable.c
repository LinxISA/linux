// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Lennox Wu <lennox.wu@sunplusct.com>
 *  Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2013 Regents of the University of California
 */


#include <linux/extable.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/block_head.h>
#include <asm/trap.h>

int fixup_exception(struct pt_regs *regs)
{
	/* Since battr.fixup has been introduced in larm v0.13,
	 * we can parse block header(epc) to get fixup handler
	 * in stead of traversing __extable.
	 * So there are some scenars that we shouldn't dereference
	 * epc, because of in that time the epc was invaild.
	 */
#if 0 /* FUTO相关功能待重新定义 */
	if (regs->cause == EXC_INST_PAGE_FAULT ||
	    regs->cause == EXC_INST_ACCESS ||
	    regs->cause == EXC_INST_MISALIGNED)
		return 0;
#endif
	if (blockhead_is_vaild((unsigned long*)(regs->bpc)) < 0)
		return 0;

	if (blockhead_is_fixup((unsigned long*)(regs->bpc)) < 0)
		return 0;

	/*
	 * Skip over the exception block and goto the fixup block,
	 * discard the bstate of exception block as well.
	 */
	regs->bpc = blockhead_get_fixup_handler((unsigned long*)(regs->bpc));
	//regs->ebstate.rra = RRAT_DEFAULT;

	return 1;
}
