/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2022 Huawei Technologies Co., Ltd.
 */

#ifndef _LINX_BLOCK_DEF_H
#define _LINX_BLOCK_DEF_H

/*
 * By default, the block text body is placed in ".text.body" section.
 * To place block text body in default section,
 * unset HAVE_BLOCK_TEXT_BODY_SECTION before include this file,
 * e.g. through AFLAGS or CFLAGS in Makefile.
 */
#ifndef HAVE_BLOCK_TEXT_BODY_SECTION
#define HAVE_BLOCK_TEXT_BODY_SECTION 1
#endif

#ifdef __ASSEMBLER__

#if HAVE_BLOCK_TEXT_BODY_SECTION
#define PUSH_BLOCK_TEXT_BODY_SECTION .pushsection ".text.body","ax"
#define POP_BLOCK_TEXT_BODY_SECTION  .popsection
#else
#define PUSH_BLOCK_TEXT_BODY_SECTION
#define POP_BLOCK_TEXT_BODY_SECTION
#endif

#else /* !__ASSEMBLER__ */

#if HAVE_BLOCK_TEXT_BODY_SECTION
#define PUSH_BLOCK_TEXT_BODY_SECTION ".pushsection \".text.body\",\"ax\"\n"
#define POP_BLOCK_TEXT_BODY_SECTION  ".popsection\n"
#else
#define PUSH_BLOCK_TEXT_BODY_SECTION
#define POP_BLOCK_TEXT_BODY_SECTION
#endif

#endif /* __ASSEMBLER__ */

/* Defines fancy numeric labels used to define block text boundaries */

#ifdef __ASSEMBLER__

#define BSTART_LABEL_DEF 202302220:
#define BSTART_LABEL_REF 202302220f
#define BSTOP_LABEL_DEF  202302221:
#define BSTOP_LABEL_REF  202302221f

#else /* !__ASSEMBLER__ */

#define BSTART_LABEL_DEF "202302220:\n"
#define BSTART_LABEL_REF "202302220f"
#define BSTOP_LABEL_DEF  "202302221:\n"
#define BSTOP_LABEL_REF  "202302221f"

#endif /* __ASSEMBLER__ */

/*
 * Define block definition macros to define a block by the following pattern:
 * block_{std, sys, aux}_head
 * [block attributes]
 * block_text_begin
 * [block micro-instructions]
 * block_text_end
 */

#ifdef __ASSEMBLER__

/************************Block Head Definition*******************/
/*
 * Standard block head definition
 */
.macro block_std_head
	bstart BSTART_LABEL_REF
	b.stdd
.endm

/*
 * Standard Hyper block head definition
 */
.macro block_stdh_head
	bstart BSTART_LABEL_REF
	b.stdhh
.endm

/*
 * Standard Compressd block head definition
 */
.macro block_stdc_head
	bstart BSTART_LABEL_REF
	b.stdcc
.endm

/*
 * Standard Compressd Hyper block head definition
 */
.macro block_stdhc_head
	bstart BSTART_LABEL_REF
	b.stdhcc
.endm

/*
 * Floating-point block head definition
 */
.macro block_fp_head
	bstart BSTART_LABEL_REF
	b.fpp
.endm

/*
 * Floating-point Hyper block head definition
 */
.macro block_fph_head
	bstart BSTART_LABEL_REF
	b.fphh
.endm

/*
 * Control block head definition
 * LBREF: long jump block
 */
.macro block_lbref_head
	bstart BSTART_LABEL_REF
	b.lbreff
.endm

/*
 * Control block head definition
 * BLBAR: Block Load Speculation Barrier
 */
.macro block_blbar_head
	bstart BSTART_LABEL_REF
	b.lbarr
.endm

/*
 * Control block head definition
 * BSBAR: Block Store Speculation Barrier
 */
.macro block_bsbar_head
	bstart BSTART_LABEL_REF
	b.sbarr
.endm

/*
 * Control block head definition
 * REPEAT: Block repeat counter
 */
.macro block_repeat_head
	bstart BSTART_LABEL_REF
	b.repeat
.endm

/*
 * Control block head definition
 * HINT: hint block
 */
.macro block_hint_head
	bstart BSTART_LABEL_REF
	b.hintt
.endm

/*
 * Template block head definition
 * MCOPY: memory copy
 */
.macro block_mcopy_head dst src len
	bstart BSTART_LABEL_REF
	b.mcopys \dst \src \len
.endm

/*
 * Template block head definition
 * MSET: memory set
 */
.macro block_mset_head dst data_byte nr_byte
	bstart BSTART_LABEL_REF
	b.msets \dst \data_byte \nr_byte
.endm

/*
 * Template block head definition
 * MPUSH: memory push
 */
.macro block_mpush_head dst gpr:vararg
	bstart BSTART_LABEL_REF
	b.mpushs \dst \gpr
.endm

/*
 * Template block head definition
 * MPOP: memory pop
 */
.macro block_mpop_head dst gpr:vararg
	bstart BSTART_LABEL_REF
	b.mpops \dst \gpr
.endm

/*
 * Template block head definition
 * F.ENTRY: Function Prologue
 */
.macro block_fentry_head imm32 dst gpr:vararg
	bstart BSTART_LABEL_REF
	b.fentrys \gpr \dst \imm32
.endm

/*
 * Template block head definition
 * F.EXIT: Function Epilogue RA used
 */
.macro block_fexit_head imm32 dst gpr:vararg
	bstart BSTART_LABEL_REF
	b.fexits \gpr \dst \imm32
.endm

/*
 * Template block head definition
 * F.TEXIT: Function Epilogue RegRet used
 */
.macro block_ftexit_head imm32 ret dst gpr:vararg
	bstart BSTART_LABEL_REF
	b.ftexits \gpr \dst \ret \imm32
.endm

/*
 * System block head definition
 */
.macro block_sys_head
	bstart BSTART_LABEL_REF
	b.syss
.endm

/*
 * System Hyper block head definition
 */
.macro block_sysh_head
	bstart BSTART_LABEL_REF
	b.syssh
.endm
/*************Block Head Definition End*************/

.macro block_text_begin
	bstop BSTOP_LABEL_REF
	PUSH_BLOCK_TEXT_BODY_SECTION
	BSTART_LABEL_DEF
.endm

.macro block_text_end
	bend
	BSTOP_LABEL_DEF
	POP_BLOCK_TEXT_BODY_SECTION
.endm

.macro block_text_empty
	block_text_begin
	block_text_end
.endm

/*************Block Branch Definition*************/

/*
 * BPC = link register
 */
.macro block_next_ret
	bstart.std ret
	setc.tgt ra
	bstop
.endm

/*
 * BPC += branch, link register = next BPC
 */
.macro block_next_call branch
	BSTART.CALL \branch, 1f, ->ra
	bstop
	1:
.endm

/*
 * BPC += branch
 */
.macro block_next_direct branch
	bstart.std direct, \branch
	bstop
.endm

/*
 * BPC = symbol
 */
.macro block_next_ind symbol
	bstart.std ind
	1: addtpc %tpcrel_hi(\symbol), -> t
	addi t#1, %tpcrel_lo(1b), -> t
	setc.tgt t#1
	bstop
.endm

/*
 * BPC = symbol, link register = next BPC
 */
.macro block_next_indcall symbol
	BSTART.ICALL 2f, ->ra
	1: addtpc %tpcrel_hi(\symbol), -> t
	addi t#1, %tpcrel_lo(1b), -> t
	setc.tgt t#1
	bstop
	2:
.endm

/*
 * BPC = gpr[rs], link register = next BPC
 */
.macro block_next_indcall_reg rs
	BSTART.ICALL 2f, ->ra
	setc.tgt \rs
	bstop
	2:
.endm

/*
 * if gpr[rs] != 0, then BPC += branch
 */
.macro block_next_bnez rs, branch
	bstart.std cond, \branch
	setc.ne \rs, zero
.endm

/*
 * if gpr[rs] == 0, then BPC += branch
 */
.macro block_next_beqz rs, branch
	bstart.std cond, \branch
	setc.eq \rs, zero
.endm

/*
 * if gpr[rs] >= 0, then BPC += branch
 */
.macro block_next_bgez rs, branch
	bstart.std cond, \branch
	setc.ge \rs, zero
.endm

/*
 * if gpr[rs] < 0, then BPC += branch
 */
.macro block_next_bltz rs, branch
	bstart.std cond, \branch
	setc.lt \rs, zero
.endm

/*************Block Branch Definition End*******/

#else /* !__ASSEMBLER__ */

/*************Block Head Definition*************/

/*
 * Standard block head definition
 */
#define block_std_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.stds\n"

/*
 * Standard Hyper block head definition
 */
#define block_stdh_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.stdsh\n"

/*
 * Standard Compressd block head definition
 */
#define block_stdc_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.stdsc\n"

/*
 * Standard Compressd Hyper block head definition
 */
#define block_stdhc_head		\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.stdshc\n"

/*
 * Floating-point block head definition
 */
#define block_fp_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.fps\n"

/*
 * Floating-point Hyper block head definition
 */
#define block_fph_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.fpsh\n"

/*
 * inline block head definition
 */
#define block_inl_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.inls\n"

/*
 * Control block head definition
 * LBREF: long jump block
 */
#define block_lbref_head		\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.lbrefs\n"

/*
 * Control block head definition
 * BLBAR: Block Load Speculation Barrier
 */
#define block_blbar_head		\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.lbars\n"

/*
 * Control block head definition
 * BSBAR: Block Store Speculation Barrier
 */
#define block_bsbar_head		\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.sbars\n"

/*
 * Control block head definition
 * REPEAT: Block repeat counter
 */
#define block_repeat_head		\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.repeats\n"

/*
 * Control block head definition
 * HINT: hint block
 */
#define block_hint_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.hints\n"

/*
 * Template block head definition
 * MCOPY: memory copy
 */
#define block_mcopy_head dst src len	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.mcopy \\dst \\src \\len \n"

/*
 * Template block head definition
 * MSET: memory set
 */
#define block_mset_head dst data_byte nr_byte	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.mset \\dst \\data_byte \\nr_byte\n"

/*
 * Template block head definition
 * MPUSH: memory push
 */
#define block_mpush_head dst gpr:vararg 	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.mpushs \\dst \\gpr\n"

/*
 * Template block head definition
 * MPOP: memory pop
 */
#define block_mpop_head dst gpr:vararg	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.mpops \\dst \\gpr\n"

/*
 * Template block head definition
 * F.ENTRY: Function Prologue
 */
#define block_fentry_head imm32 dst gpr:vararg	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.fentry \\gpr \\dst \\imm32\n"

/*
 * Template block head definition
 * F.EXIT: Function Epilogue RA used
 */
#define block_fexit_head imm32 dst gpr:vararg	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.fexit \\gpr \\dst \\imm32\n"

/*
 * Template block head definition
 * F.TEXIT: Function Epilogue RegRet used
 */
#define block_ftexit_head imm32 ret dst gpr:vararg	\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.ftexit \\gpr \\dst \\ret \\imm32\n"

/*
 * System block head definition
 */
#define block_sys_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.sys\n"

/*
 * System Hyper block head definition
 */
#define block_sysh_head			\
	"bstart "BSTART_LABEL_REF"\n"	\
	"b.sysh\n"

/*************Block Head Definition End*************/

#define block_text_begin		\
	"bstop "BSTOP_LABEL_REF"\n"	\
	PUSH_BLOCK_TEXT_BODY_SECTION	\
	BSTART_LABEL_DEF

#define block_text_end			\
	"bend\n"			\
	BSTOP_LABEL_DEF			\
	POP_BLOCK_TEXT_BODY_SECTION

#endif /* __ASSEMBLER__ */

#endif /* _LINX_BLOCK_DEF_H */
