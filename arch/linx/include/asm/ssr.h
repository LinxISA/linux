/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2022 Huawei Technologies Co., Ltd.
 */

#ifndef _LINX_SSR_H
#define _LINX_SSR_H

#include <asm/asm.h>
#include <linux/const.h>
#include <asm/block-def.h>


/* CSTATE flags */
#define CSTATE_ACR0	_AC(0x0000000000000000, UL)
#define CSTATE_ACR1	_AC(0x0000000000000001, UL)
#define CSTATE_ACR2	_AC(0x0000000000000002, UL)
#define CSTATE_ACR_MASK	_AC(0x000000000000000F, UL)

#define CSTATE_I	_AC(0x0000000000000010, UL) /* interrupt enable */
#define CSTATE_P	_AC(0x0000000000000200, UL) /* user access permit */
#define CSTATE_E	_AC(0x0000000000000020, UL) /* endian control bit */
#define CSTATE_BI	_AC(0x0000000000000040, UL) /* valid bit of EBSTATE */

#define EVBASE_ADDR_MASK	_AC(0xFFFFFFFFFFFFF000, UL) /* exception vector base address mask */

#define ECAUSE_TRAPNUM_INSN_EXP 		0 /* Instruction exception */
#define ECAUSE_TRAPNUM_DATA_EXP 		1 /* Data exception */
#define ECAUSE_TRAPNUM_BLOCK_EXP		4 /* Block exception */
#define ECAUSE_TRAPNUM_FLOAT_EXP		5 /* Float exception */
#define ECAUSE_TRAPNUM_ASSERT			15 /* Assertion exception */
#define ECAUSE_TRAPNUM_SCALL_EXP		16 /* SysCall. */
#define ECAUSE_TRAPNUM_BREAKPOINT_EXP		17 /* ebreak */
#define ECAUSE_TRAPNUM_ILLEGAL_SSR_EXP		62 /* ILLEGAL ssr exception */
#define ECAUSE_TRAPNUM_INVALID_EXP 		63 /* unsupported exception */

#define ECAUSE_TRAPNUM_ACR0_EXT_INT             0 /* ACR0 external interrupt */
#define ECAUSE_TRAPNUM_ACR0_TIMER_INT           1 /* ACR0 timer interrupt */
#define ECAUSE_TRAPNUM_ACR0_SOFT_INT            2 /* ACR0 soft interrupt */	// #! no software interrupt in larm, is this needed ?
#define ECAUSE_TRAPNUM_ACR1_EXT_INT             3 /* ACR1 external interrupt */
#define ECAUSE_TRAPNUM_ACR1_TIMER_INT           4 /* ACR1 timer interrupt */
#define ECAUSE_TRAPNUM_ACR1_SOFT_INT            5 /* ACR1 soft interrupt */

#define ECAUSE_INSN_SYD_ACCESS_FAULT		0 /* Instruction access fault */
#define ECAUSE_INSN_SYD_TRANS_FAULT		1 /* Instruction translation fault */
#define ECAUSE_INSN_SYD_MISALIGN_FAULT		2 /* Instruction misaligned fault */
#define ECAUSE_INSN_SYD_ILLEGAL_FAULT		3 /* Instruction illegal fault */
#define ECAUSE_INSN_SYD_PERM_FAULT		4 /* Instruction permission fault */
#define ECAUSE_INSN_SYD_PAGE_FAULT		5 /* Instruction page fault */

#define ECAUSE_DATA_SYD_LD_ACCESS_FAULT		0 /* Load access fault */
#define ECAUSE_DATA_SYD_LD_MISALIGN_FAULT	1 /* Load misaligned fault */
#define ECAUSE_DATA_SYD_LD_PAGE_FAULT		2 /* Load page fault */
#define ECAUSE_DATA_SYD_ST_AOP_ACCESS_FAULT	3 /* Store/Atomic access fault */
#define ECAUSE_DATA_SYD_ST_AOP_MISALIGN_FAULT	4 /* Store/Atomic misaligned fault */
#define ECAUSE_DATA_SYD_ST_AOP_PAGE_FAULT	5 /* Store/Atomic page fault */
#define ECAUSE_DATA_SYD_RANGE				6 /* Invalid access range */
#define ECAUSE_BUS							7 /* Bus exception */

#define ECAUSE_BLOCK_SYD_INVALID_SET_REGS	0 /* Invalid set_regs detected */
#define ECAUSE_BLOCK_SYD_INVALID_GET_REGS	1 /* Invalid get_regs detected */
#define ECAUSE_BLOCK_SYD_INVALID_PARA		2 /* Invalid parameter */
#define ECAUSE_BLOCK_SYD_INVALID_DOUBLESET	3 /* Invalid set regs repeatedly */
#define ECAUSE_BLOCK_SYD_INVALID_FIXUP_BLOCK	4 /* Invalid fixup block */

#define ECAUSE_E_MASK		_AC(0x8000000000000000, UL)
#define ECAUSE_BI_MASK		_AC(0x4000000000000000, UL)
#define ECAUSE_SYNDROME_MASK	_AC(0x0000FFFFFF000000, UL)
#define ECAUSE_TRAPNUM_MASK	_AC(0x000000000000003F, UL)

#define ECAUSE_BI_SHIFT		(62)
#define ECAUSE_SYNDROME_SHIFT	(24)
#define ECAUSE_TRAPNUM_SHIFT	(0)

#define ECAUSE_SYNDROME(ecause)	(((ecause) & ECAUSE_SYNDROME_MASK) >> ECAUSE_SYNDROME_SHIFT)
#define ECAUSE_TRAPNUM(ecause)	(((ecause) & ECAUSE_TRAPNUM_MASK) >> ECAUSE_TRAPNUM_SHIFT)


#define ECONFIG_EXTERNAL	_AC(0x0000000000000000, UL)
#define ECONFIG_TIMER		_AC(0x0000000000000001, UL)
#define ECONFIG_SOFTWARE	_AC(0x0000000000000002, UL)
#define ECONFIG_VECTOR		_AC(0x0000000100000000, UL)
#define ECONFIG_CUBE		_AC(0x0000000200000000, UL)

#define IPENDING_EXTERNAL	_AC(0x0000000000000000, UL)
#define IPENDING_TIMER		_AC(0x0000000000000001, UL)
#define IPENDING_SOFTWARE	_AC(0x0000000000000002, UL)

/* Flags */
#define BARG_REGDST0_MASK	_AC(0x000000000000001F, UL)
#define BARG_REGDST1_MASK	_AC(0x00000000000003E0, UL)
#define BARG_REGDST2_MASK	_AC(0x0000000000007C00, UL)
#define BARG_REGDST3_MASK	_AC(0x00000000000F8000, UL)
#define BARG_BLOCKTYPE_MASK	_AC(0x0000001F00000000, UL)
#define BARG_TYPE_MASK	_AC(0x0000006000000000, UL)
#define BARG_TAKE_MASK	_AC(0x0000008000000080, UL)
#define BARG_AQ_MASK	_AC(0x0000010000000000, UL)
#define BARG_RL_MASK	_AC(0x0000020000000000, UL)
#define BARG_GROUP_ID	_AC(0xFE00000000000000, UL)
/* BPC and BPCN */
#define BARG_BPC_MASK	_AC(0x0000FFFFFFFFFFFF, UL)
#define BARG_BPCN_MASK	_AC(0xFFFF000000000000, UL)

/* LCFR: Linx Core Feature Register */
#define LCFR_C	_AC(0x0000000000000001, UL) /* compressed instructions supported */
#define LCFR_G	_AC(0x0000000000000002, UL) /* GQM instructions supported */
#define LCFR_V	_AC(0x0000000000000004, UL) /* vector instructions supported */
#define LCFR_F	_AC(0x0000000000000008, UL) /* float instructions supported */
#define LCFR_GROUP_NUM_MASK	_AC(0x0000000F00000000, UL) /* group num mask */
#define LCFR_GROUP_NUM_SHIFT	32 /* group num shift */

/* IntID mask for topei and eoiei */
#define INTID_MASK	_AC(0x000000000000FFFF, UL)

#define MMTBASE_PPN		_AC(0x000000FFFFFFFFFC, UL)
#define MMTBASE_PPN_ALIGN_SIZE	16384 /* 16KB align */
#define MMTBASE_PPN_SHIFT	2
#define MMTBASE_PBYP_ENABLE	1
#define MMTBASE_ASID_BITS	24
#define MMTBASE_ASID_SHIFT	40
#define MMTBASE_ASID_MASK	_AC(0xFFFFFF, UL)

#define MMTCONFIG_MODE_SHIFT	0
#define MMTCONFIG_MODE_36	0 /* Q = 1 */
#define MMTCONFIG_MODE_44	1 /* Q = 1 */
#define MMTCONFIG_MODE_52	2 /* Q = 1 */
#define MMTCONFIG_MODE_39	0 /* Q = 0 */
#define MMTCONFIG_MODE_48	1 /* Q = 0 */
#define MMTCONFIG_MODE_57	2 /* Q = 0 */
#define MMTCONFIG_Q_SHIFT	7
#define MMTCONFIG_Q_LPTE	0
#define MMTCONFIG_Q_QPTE	1
#define MMTCONFIG_ENABLE	_AC(0x8000000000000000, UL)
#define MMTCONFIG_DEFAULT	((MMTCONFIG_MODE_57 << MMTCONFIG_MODE_SHIFT) | \
				 (MMTCONFIG_Q_LPTE << MMTCONFIG_Q_SHIFT) | \
				 (MMTCONFIG_ENABLE))


/* SSR */
#define SSR_TP		0x0000	/* Thread Pointer */
#define SSR_GP		0x0001	/* Global Pointer */

#define SSR_TIME	0x0010	/* Timer count register */
#define SSR_CYCLE	0x0011	/* Cycle Counter Register */

#define SSR_CSTATE	0x0020	/* common state */
#define SSR_LXLCID	0x0021	/* Linx Logical Core ID Register */
#define SSR_VENDOR	0x0022	/* Vendor ID Register*/
#define SSR_VERSION	0x0023	/* Linx Core Version Register*/
#define SSR_LCFR	0x0024	/* Linx Core Features Register*/
#define SSR_LCFR_EN	0x0025	/* Linx Core Feature Enable Register	*/

#define SSR_SYSCNT	0x0810 /* System Counter */
#define SSR_CW	0x0820 /* Canary Word */
#define SSR_MSGBCR	0x0830 /* Message Buffer Ctrl Register */
#define SSR_MSGBD1	0x0831 /* Message Buffer Data Register 1 */
#define SSR_MSGBD2	0x0832 /* Message Buffer Data Register 2 */
#define SSR_MSGBD3	0x0833 /* Message Buffer Data Register 3 */
#define SSR_MSGBD4	0x0834 /* Message Buffer Data Register 4 */
#define SSR_MSGBD5	0x0835 /* Message Buffer Data Register 5 */
#define SSR_MSGBD6	0x0836 /* Message Buffer Data Register 6 */
#define SSR_MSGBD7	0x0837 /* Message Buffer Data Register 7 */
#define SSR_MSGBD8	0x0838 /* Message Buffer Data Register 8 */
#define SSR_MSGBD9	0x0839 /* Message Buffer Data Register 9 */
#define SSR_MSGBD10	0x08310 /* Message Buffer Data Register 10 */

#define SSR_A0_ECSTATE	0x0f00 /* acr0's exception store state */
#define SSR_A0_EVBASE	0x0f01 /* acr0's exception vector base */
#define SSR_A0_TRAPNO	0x0f02 /* acr0's exception cause */
#define SSR_A0_TRAPARG0	0x0f03 /* acr0's exception agrument0 */

#define SSR_A0_ETEMP	0x0f05 /* acr0's exception context saving temporary (tp)*/
#define SSR_A0_FUTO		0x0f06 /* acr0's fixup takeover */
#define SSR_A0_ECONFIG	0x0f07 /* acr0's interrupt enable */
#define SSR_A0_IPENDING	0x0f08 /* acr0's interrupt pending */
#define SSR_A0_TOPEI    0x0f09 /* acr0's top interrupt id */
#define SSR_A0_EOIEI    0x0f0a /* acr0's end interrupt id */
#define SSR_A0_EBPC		0X0f0b /* (EBARG) acr0's exception current bpc */
#define SSR_A0_EBARG	0x0f0c /* (EBARG) acr0's BARG besides bpc and tpc */
#define SSR_A0_ETPC		0x0f0d /* (EBARG) acr0's exception current tpc */
#define SSR_A0_EBPCN	0x0f0e /* (EBARG) acr0's exception next bpc */

#define SSR_A0_TIMER_TIME		0x0f20	/* acr0's timer register */
#define SSR_A0_TIMER_TIMECMP	0x0f21	/* acr0's timer configuration register */

#define SSR_A0_XBINFO			0x0f30 /* acr0's XB base register */
#define SSR_A0_ACR_PARAM		0x0f31 /* acr0's LxLc argument register */

#define SSR_A0_ELPR0	0x0f40 /* acr0's exception t1 */
#define SSR_A0_ELPR1	0x0f41 /* acr0's exception t2 */
#define SSR_A0_ELPR2	0x0f42 /* acr0's exception t3 */
#define SSR_A0_ELPR3	0x0f43 /* acr0's exception t4 */
#define SSR_A0_ELPR4	0x0f44 /* acr0's exception u1 */
#define SSR_A0_ELPR5	0x0f45 /* acr0's exception u2 */
#define SSR_A0_ELPR6	0x0f46 /* acr0's exception u3 */
#define SSR_A0_ELPR7	0x0f47 /* acr0's exception u4 */


#define SSR_ACR_SIZE	0x1000
#define SSR_A1_ECSTATE	0x1f00	/* acr1's exception store state */
#define SSR_A1_EVBASE	0x1f01	/* acr1's exception vector base */
#define SSR_A1_TRAP_NO	0x1f02	/* acr1's exception cause */
#define SSR_A1_TRAPARG0	0x1f03	/* acr1's exception agrument0 */

#define SSR_A1_ETEMP	0x1f05	/* acr1's exception context saving temporary (tp)*/
#define SSR_A1_FUTO		0x1f06	/* acr1's fixup takeover */
#define SSR_A1_ECONFIG	0x1f07	/* acr1's interrupt enable */
#define SSR_A1_IPENDING	0x1f08	/* acr1's interrupt pending */
#define SSR_A1_TOPEI    0x1f09	/* acr1's top interrupt id */
#define SSR_A1_EOIEI    0x1f0a	/* acr1's end interrupt id */
#define SSR_A1_EBPC		0X1f0b	/* (EBARG) acr1's exception current bpc */
#define SSR_A1_EBARG	0x1f0c	/* (EBARG) acr1's BARG besides bpc and tpc */
#define SSR_A1_ETPC		0x1f0d	/* (EBARG) acr1's exception current tpc */
#define SSR_A1_EBPCN	0x1f0e	/* (EBARG) acr1's exception next bpc */

#define SSR_A1_MMTBASE	0x1f10 /* acr1's memory management translation base (only supported in ACR1) */
#define SSR_A1_MMCONFIG	0x1f11 /* acr1's memory management translation config (only supported in ACR1)*/

#define SSR_A1_TIMER_TIME		0x1f20	/* acr1's timer register */
#define SSR_A1_TIMER_TIMECMP	0x1f21	/* acr1's timer configuration register */

#define SSR_A1_XBINFO			0x1f30	/* acr0's XB base register */
#define SSR_A1_ACR_PARAM		0x1f30	/* acr0's LxLc argument register */

#define SSR_A1_ELPR0	0x1f40 /* acr1's exception t1 */
#define SSR_A1_ELPR1	0x1f41 /* acr1's exception t2 */
#define SSR_A1_ELPR2	0x1f42 /* acr1's exception t3 */
#define SSR_A1_ELPR3	0x1f43 /* acr1's exception t4 */
#define SSR_A1_ELPR4	0x1f44 /* acr1's exception u1 */
#define SSR_A1_ELPR5	0x1f45 /* acr1's exception u2 */
#define SSR_A1_ELPR6	0x1f46 /* acr1's exception u3 */
#define SSR_A1_ELPR7	0x1f47 /* acr1's exception u4 */


#define SSR_ECSTATE	SSR_A1_ECSTATE  /* acr1's exception store state */
#define SSR_EVBASE	SSR_A1_EVBASE	/* acr1's exception vector base */
#define SSR_TRAPNO	SSR_A1_TRAP_NO	/* acr1's exception cause */
#define SSR_TRAPARG0	SSR_A1_TRAPARG0	/* acr1's exception agrument0 */
#define SSR_ETEMP	SSR_A1_ETEMP	/* acr1's exception context saving temporary (tp)*/
#define SSR_FUTO	SSR_A1_FUTO	/* acr1's fixup takeover */
#define SSR_ECONFIG	SSR_A1_ECONFIG	/* acr1's interrupt enable */
#define SSR_IPENDING	SSR_A1_IPENDING /* acr1's interrupt pending */
#define SSR_TOPEI	SSR_A1_TOPEI	/* acr1's top interrupt id */
#define SSR_EOIEI	SSR_A1_EOIEI	/* acr1's end interrupt id */
#define SSR_EBPC	SSR_A1_EBPC	/* (EBARG) acr1's exception current bpc */
#define SSR_ETPC	SSR_A1_ETPC	/* (EBARG) acr1's exception current tpc */
#define SSR_EBARG	SSR_A1_EBARG	/* (EBARG) acr1's BARG besides bpc and tpc */
#define SSR_EBPCN	SSR_A1_EBPCN	/* (EBARG) acr1's exception next bpc */

#define SSR_MMTBASE	SSR_A1_MMTBASE		/* memory management translation base */
#define SSR_MMCONFIG	SSR_A1_MMCONFIG	/* memory management configuration */

#define SSR_TIMER_TIME	SSR_A1_TIMER_TIME	/* acr1's timer register */
#define SSR_TIMER_TIMECMP	SSR_A1_TIMER_TIMECMP	/* acr1's timer configuration register */

#define SSR_XBINFO	SSR_A1_XBINFO	/* acr0's XB base register */
#define SSR_ACR_PARAM	SSR_A1_ACR_PARAM	/* acr0's LxLc argument register */

#define SSR_ELPR0	SSR_A1_ELPR0 /* acr1's exception t1 */
#define SSR_ELPR1	SSR_A1_ELPR1 /* acr1's exception t2 */
#define SSR_ELPR2	SSR_A1_ELPR2 /* acr1's exception t3 */
#define SSR_ELPR3	SSR_A1_ELPR3 /* acr1's exception t4 */
#define SSR_ELPR4	SSR_A1_ELPR4 /* acr1's exception u1 */
#define SSR_ELPR5	SSR_A1_ELPR5 /* acr1's exception u2 */
#define SSR_ELPR6	SSR_A1_ELPR6 /* acr1's exception u3 */
#define SSR_ELPR7	SSR_A1_ELPR7 /* acr1's exception u4 */


/* acre's Return Request Argument */
#define RRAT_DEFAULT	0
#define RRAT_RESTORE	1

/* acrc's request type */
#define SCT_MAC	0
#define SCT_SYS	1
#define SCT_SEC	2

/* futo's exception types */
#define E_DATA_EC_LOAD_ACCESS		0
#define E_DATA_EC_LOAD_MISALIGNED	1
#define E_DATA_EC_STORE_A_ACCESS	2
#define E_DATA_EC_STORE_A_MISALIGN	3

#ifndef __ASSEMBLY__
/*
 * t = SSRs[ssr]; SSRs[ssr] = val; return t;
 */
#define ssr_swap(ssr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrget " __ASM_STR(ssr)", -> t\n"		\
			"hl.ssrset %1," __ASM_STR(ssr)"\n"	\
			"add t#1, zero, -> %0\n"				\
		: "=r" (__v)					\
		: "r" (__v)					\
		: "memory");					\
	__v;							\
})

/*
 *  return SSRs[ssr];
 */
#define ssr_read(ssr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrget " __ASM_STR(ssr)", -> %0\n"		\
		: "=r" (__v) :					\
		: "memory");					\
	__v;							\
})

/*
 * SSRs[ssr] = val;
 */
#define ssr_write(ssr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrset %0," __ASM_STR(ssr)"\n"	\
		: : "r" (__v)					\
		: "memory");					\
})

/*
 * t = SSRs[ssr]; SSRs[ssr] = t | val; return t;
 */
#define ssr_read_set(ssr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrget " __ASM_STR(ssr)"-> t\n"		\
			"or t#1, %1 -> t\n"				\
			"hl.ssrset t#1," __ASM_STR(ssr)"\n"	\
			"add t#2, zero, -> %0\n"				\
		: "=r" (__v)					\
		: "r" (__v)					\
		: "memory");					\
	__v;							\
})

/*
 * t = SSRs[ssr]; SSRs[ssr] = t | val;
 */
#define ssr_set(ssr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrget " __ASM_STR(ssr)", -> t\n"		\
			"or t#1, %0, -> t\n"				\
			"hl.ssrset t#1," __ASM_STR(ssr)"\n"	\
		: : "r" (__v)					\
		: "memory");					\
})

/*
 * t = SSRs[ssr]; SSRs[ssr] = t | ~val; return t;
 */
#define ssr_read_clear(ssr, val)				\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrget " __ASM_STR(ssr)", -> t\n"		\
			"and t#1, %1.not, -> t\n"			\
			"hl.ssrset t#1," __ASM_STR(ssr)"\n"	\
			"add t#2, zero, -> %0\n"				\
		: "=r" (__v)					\
		: "r" (__v)					\
		: "memory");					\
	__v;							\
})

/*
 * t = SSRs[ssr]; SSRs[ssr] = t | ~val;
 */
#define ssr_clear(ssr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ (					\
		"BSTART.sys fall\n"					\
			"hl.ssrget " __ASM_STR(ssr)", -> t\n"		\
			"and t#1, %0.not, -> t\n"			\
			"hl.ssrset t#1," __ASM_STR(ssr)"\n"	\
		: : "r" (__v)					\
		: "memory");					\
})
#endif /* __ASSEMBLY__ */

#endif /* _LINX_SSR_H */
