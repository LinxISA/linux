/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2022 Huawei Technologies Co., Ltd.
 */

#ifndef _ASM_LINX_BLOCK_HEAD_H
#define _ASM_LINX_BLOCK_HEAD_H

/*
* Definition linx block header br_type encoding.
*/
#define LINX_HEAD_BR_TYPE_FALL			_AC(0x0000000000000000, UL)
#define LINX_HEAD_BR_TYPE_DIRECT		_AC(0x0000000000000001, UL)
#define LINX_HEAD_BR_TYPE_CALL			_AC(0x0000000000000002, UL)
#define LINX_HEAD_BR_TYPE_COND			_AC(0x0000000000000003, UL)
#define LINX_HEAD_BR_TYPE_IND			_AC(0x0000000000000004, UL)
#define LINX_HEAD_BR_TYPE_INDCALL		_AC(0x0000000000000005, UL)
#define LINX_HEAD_BR_TYPE_RET			_AC(0x0000000000000006, UL)
#define LINX_HEAD_BR_TYPE_CONCAT		_AC(0x0000000000000007, UL)

/*
* Definition linx block header type encoding.
*/
#define LINX_HEAD_TYPE_STD			_AC(0x0000000000000000, UL)
#define LINX_HEAD_TYPE_AUX			_AC(0x0000000000000001, UL)
#define LINX_HEAD_TYPE_FP			_AC(0x0000000000000002, UL)
#define LINX_HEAD_TYPE_HYP			_AC(0x0000000000000006, UL)
#define LINX_HEAD_TYPE_SYS			_AC(0x0000000000000007, UL)

/*
* Definition linx block header attrs encoding.
*/
#define LINX_HEAD_ATTRS_ATOMIC_NONE		_AC(0x0000000000000000, UL)
#define LINX_HEAD_ATTRS_ATOMIC_RLA		_AC(0x0000000000000001, UL)
#define LINX_HEAD_ATTRS_ATOMIC_AC		_AC(0x0000000000000002, UL)
#define LINX_HEAD_ATTRS_ATOMIC_RLS		_AC(0x0000000000000030, UL)
#define LINX_HEAD_ATTRS_ATOMIC_AC_RLS		_AC(0x0000000000000040, UL)
#define LINX_HEAD_ATTRS_REPEAT			_AC(0x0000000000000010, UL)
#define LINX_HEAD_ATTRS_FIXUP			_AC(0x0000000000000020, UL)

/*
* Definition linx block header br_offset encoding.
*/
#define BR_OFFSET_MASK				_AC(0x000007FFFFFFFFFF, UL)
#define CONCAT_BR_OFFSET_SHIFT			_AC(12, UL)
/* FIXUP_BPC = BPC + (fixup_offset * 16) */
#define BR_OFFSET_POWER				4

/*
* Definition linx block header b_offset encoding.
*/
#define B_OFFSET_MASK				_AC(0x0003FFFFFFFFFFFF, UL)
#define CONCAT_B_OFFSET_SHIFT			_AC(19, UL)

/*
* Definition linx block header encoding.
* A general header is 128-bit length
*/
#define BLOCK_HEAD_UNIT_SHIFT			4

#define LINX_HEAD_OPCODE_MAGIC_WORD		_AC(0x0000000000000000B, UL)
#define LINX_HEAD_OPCODE_SHIFT			_AC(0, UL)
#define LINX_HEAD_OPCODE_MASK			_AC(0x0000000000000007F, UL)

#define LINX_HEAD_TYPE_SHIFT			_AC(7, UL)
#define LINX_HEAD_TYPE_MASK			_AC(0x00000000000000007, UL)

#define LINX_HEAD_BR_TYPE_SHIFT			_AC(10, UL)
#define LINX_HEAD_BR_TYPE_MASK			_AC(0x00000000000000007, UL)

#define LINX_HEAD_B_OFFSET_SHIFT		_AC(13, UL)
#define LINX_HEAD_B_OFFSET_MASK			_AC(0x0000000000007FFFF, UL)

#define LINX_HEAD_ATTRS_SHIFT			_AC(32, UL)
#define LINX_HEAD_ATTRS_MASK			_AC(0x00000000000003FF, UL)

#define LINX_HEAD_B_SIZE_SHIFT			_AC(42, UL)
#define LINX_HEAD_B_SIZE_MASK			_AC(0x00000000000003FF, UL)

#define LINX_HEAD_BR_OFFSET_SHIFT		_AC(52, UL)
#define LINX_HEAD_BR_OFFSET_MASK		_AC(0x0000000000000FFF, UL)

#define LINX_HEAD_SET_REGS_SHIFT		_AC(64, UL)
#define LINX_HEAD_SET_REGS_MASK			_AC(0x00000000FFFFFFFF, UL)

#define LINX_HEAD_GET_REGS_SHIFT		_AC(96, UL)
#define LINX_HEAD_GET_REGS_MASK			_AC(0x00000000FFFFFFFF, UL)

/* CONCAT Block Header */
/* Block header size 16 Byte */
#define LINX_HEAD_GET_CONCAT(header)		((header) + 2)

#define LINX_HEAD_CONCAT_BR_TYPE_SHIFT		_AC(10, UL)
#define LINX_HEAD_CONCAT_BR_TYPE_MASK		_AC(0x0000000000000007, UL)

#define LINX_HEAD_CONCAT_BR_OFFSET_SHIFT	_AC(64, UL)
#define LINX_HEAD_CONCAT_BR_OFFSET_MASK		_AC(0x00000000FFFFFFFF, UL)

#define LINX_HEAD_CONCAT_B_OFFSET_SHIFT		_AC(96, UL)
#define LINX_HEAD_CONCAT_B_OFFSET_MASK		_AC(0x00000000FFFFFFFF, UL)

#ifndef __ASSEMBLY__
static inline unsigned long blockhead_value(unsigned long *header,
						unsigned long shift,
						unsigned long mask)
{
	if (shift >= sizeof(header) * 8)
		shift -= sizeof(header) * 8;

	return (*(header) >> shift) & mask;
}

static inline int blockhead_is_vaild(unsigned long *header)
{
	if (blockhead_value(header, LINX_HEAD_OPCODE_SHIFT,
			    LINX_HEAD_OPCODE_MASK) != LINX_HEAD_OPCODE_MAGIC_WORD)
		return -1;

	return 0;
}

static inline int blockhead_is_fixup(unsigned long *header)
{
	if ((blockhead_value(header, LINX_HEAD_ATTRS_SHIFT, LINX_HEAD_ATTRS_MASK) &
			     LINX_HEAD_ATTRS_FIXUP) != 0)
		return 0;

	return -1;
}

static inline unsigned long blockhead_get_fixup_handler(unsigned long *header)
{
	unsigned long br_type;
	unsigned long br_offset;
	unsigned long concat_br_offset = 0;

	br_offset = blockhead_value(header, LINX_HEAD_BR_OFFSET_SHIFT, LINX_HEAD_BR_OFFSET_MASK);
	br_type = blockhead_value(header, LINX_HEAD_BR_TYPE_SHIFT, LINX_HEAD_BR_TYPE_MASK);
	if (br_type == LINX_HEAD_BR_TYPE_CONCAT) {
		concat_br_offset = blockhead_value(LINX_HEAD_GET_CONCAT(header),
						   LINX_HEAD_CONCAT_BR_OFFSET_SHIFT,
						   LINX_HEAD_CONCAT_BR_OFFSET_MASK);
	}
	br_offset = (((concat_br_offset << CONCAT_BR_OFFSET_SHIFT) | br_offset) &
		     BR_OFFSET_MASK) << BR_OFFSET_POWER;

	return (br_offset + (unsigned long)header);
}
#endif /* __ASSEMBLY__ */

#endif /* _ASM_LINX_BLOCK_HEAD_H */
