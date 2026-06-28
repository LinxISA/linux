// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Lennox Wu <lennox.wu@sunplusct.com>
 *  Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2013 Regents of the University of California
 */


#include <linux/extable.h>
#include <linux/module.h>
#include <linux/unaligned.h>
#include <linux/uaccess.h>
#include <asm/block_head.h>
#include <asm/trap.h>

/*
 * Current v0.56 BSTART FALL encodings carry the fixup target directly in the
 * branch immediate.  The older Linux port only understood legacy 128-bit block
 * headers with LINX_HEAD_ATTRS_FIXUP, so relaxed HL.BSTART fixup blocks from
 * compiler-generated uaccess code were treated as ordinary faults.
 */
#define LINX_BSTART32_FALL_MASK		0x00007fffU
#define LINX_BSTART32_STD_FALL		0x00001001U
#define LINX_BSTART32_SYS_FALL		0x00001081U
#define LINX_BSTART32_FP_FALL		0x00001101U

#define LINX_HL_BSTART48_FALL_MASK	0x00007fff000fULL
#define LINX_HL_BSTART48_STD_FALL	0x00001001000eULL
#define LINX_HL_BSTART48_SYS_FALL	0x00001081000eULL
#define LINX_HL_BSTART48_FP_FALL	0x00001101000eULL

static long linx_sign_extend(unsigned long value, unsigned int bits)
{
	unsigned long sign = 1UL << (bits - 1);

	return (long)((value ^ sign) - sign);
}

static bool linx_is_bstart32_fall_fixup(u32 insn)
{
	u32 form = insn & LINX_BSTART32_FALL_MASK;

	return form == LINX_BSTART32_STD_FALL ||
	       form == LINX_BSTART32_SYS_FALL ||
	       form == LINX_BSTART32_FP_FALL;
}

static bool linx_is_hl_bstart48_fall_fixup(u64 insn)
{
	u64 form = insn & LINX_HL_BSTART48_FALL_MASK;

	return form == LINX_HL_BSTART48_STD_FALL ||
	       form == LINX_HL_BSTART48_SYS_FALL ||
	       form == LINX_HL_BSTART48_FP_FALL;
}

static bool linx_v056_fixup_handler(unsigned long bpc, unsigned long *handler)
{
	u32 insn32 = get_unaligned_le32((void *)bpc);
	u64 insn48 = get_unaligned_le32((void *)bpc) |
		     ((u64)get_unaligned_le16((void *)(bpc + 4)) << 32);

	if (linx_is_bstart32_fall_fixup(insn32)) {
		unsigned long raw = (insn32 >> 15) & 0x1ffffUL;
		long offset = linx_sign_extend(raw, 17) << 1;

		if (offset == 0)
			return false;

		*handler = bpc + offset;
		return true;
	}

	if (linx_is_hl_bstart48_fall_fixup(insn48)) {
		unsigned long raw = (((insn48 >> 4) & 0xfffUL) << 18) |
				    (((insn48 >> 31) & 0x1ffffUL) << 1);
		long offset = linx_sign_extend(raw, 30);

		if (offset == 0)
			return false;

		*handler = bpc + offset;
		return true;
	}

	return false;
}

int fixup_exception(struct pt_regs *regs)
{
	unsigned long handler;

	if (linx_v056_fixup_handler(regs->bpc, &handler)) {
		regs->bpc = handler;
		return 1;
	}

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
