#include <linux/linkage.h>
#include <linux/export.h>
#include <asm/asm.h>
#include <asm/ssr.h>

.macro enable_um
	addi zero, CSTATE_P, -> t
	ssrget SSR_CSTATE, -> t
	or t#1, t#2, -> t
	ssrset t#1, SSR_CSTATE
.endm

.macro disable_um
	addi zero, CSTATE_P, -> t
	ssrget SSR_CSTATE, -> t
	and t#1, t#2.not -> t
	ssrset t#1, SSR_CSTATE
.endm

.macro fixupload op reg addr offset lbl
	L.BSTART.STD FALL, \lbl
	/*	todo: wait for fixup */
	\op [\addr, \offset], -> \reg
.endm

.macro fixupstore op reg addr offset lbl
	L.BSTART.STD FALL, \lbl
	\op \reg, [\addr, \offset]
.endm

.macro enable_fixup
	BSTART.sys fall
	ori zero, E_DATA_EC_LOAD_ACCESS, -> t
	ori t#1, E_DATA_EC_STORE_A_ACCESS, -> t
	hl.ssrset t#1, SSR_FUTO
.endm

.macro disable_fixup
	BSTART.sys fall
	hl.ssrset zero, SSR_FUTO
.endm

ENTRY(__asm_copy_to_user)
ENTRY(__asm_copy_from_user)
	enable_fixup
	BSTART.std fall
	/* Save for return value */
	subi sp, 8, -> t
	REG_S a2, [t#1, 0]

	/* Enable access to user memory */
	enable_um

	/*
	* Register allocation for code below:
	* a0 - start of uncopied dst
	* a1 - start of uncopied src
	* a2 - size
	* x0 - end of uncopied dst
	*/
	BSTART.std cond, .Lbyte_copy_tail
	add a0, a2, -> x0

	/*
	* Use byte copy only if too small.
	* SZREG holds 4 for RV32 and 8 for RV64
	*/
	addi zero, 9*SZREG, -> t
	setc.ltu a2, t#1

	/*
	 * Copy first bytes until dst is aligned to word boundary.
	 * a0 - start of dst
	 * x1 - end of aligned dst
	 */
	BSTART.std cond, .Lskip_align_dst
	addi zero, SZREG-1, -> t
	add a0, t#1, -> t
	and t#1, t#2.not, -> x1
	/* dst is already aligned, skip */
	setc.eq a0, x1


.Lcopy_head_one_by_one:
	/* a5 - one byte for copying data */
	fixupload lbi, a5, a1, 0, .Lcopy_exception_ret

	BSTART.std fall
	addi a1, 1, -> a1

	fixupstore sbi, a5, a0, 0, .Lcopy_exception_ret

	BSTART.std cond, .Lcopy_head_one_by_one
	addi a0, 1, -> t
	addi t#1, 0, -> a0
	setc.ltu t#1, x1


.Lskip_align_dst:
	/*
	* a0 - start of aligned dst
	* a1 - start of uncopied src
	*
	* Now dst is aligned.
	* Use shift-copy if src is misaligned.
	* Use word-copy if both src and dst are aligned because
	* can not use shift-copy which do not require shifting
	*/

	BSTART.std cond, .Lshift_copy
	andi a1, SZREG-1, -> t
	addi t#1, 0, -> a3
	setc.nei t#1, 0


.Lword_copy:
	/*
	* Both src and dst are aligned, unrolled word copy
	*
	* a0 - start of aligned dst
	* a1 - start of aligned src
	* x0 - end of aligned dst
	*/
	/* not to over run */
	BSTART.std direct, .Lcopy_pre_32
	subi x0, 8*SZREG, -> x0


.Lcopy_pre_32:
/* copy 8 * size of reg */
	fixupload REG_L a2, a1, 0, .Lcopy_exception_ret
	fixupload REG_L a3, a1, SZREG, .Lcopy_exception_ret
	fixupload REG_L a4, a1, 2*SZREG, .Lcopy_exception_ret
	fixupload REG_L a5, a1, 3*SZREG, .Lcopy_exception_ret
	fixupstore REG_S a2, a0, 0, .Lcopy_exception_ret
	fixupstore REG_S a3, a0, SZREG, .Lcopy_exception_ret
	fixupstore REG_S a4, a0, 2*SZREG, .Lcopy_exception_ret
	fixupstore REG_S a5, a0, 3*SZREG, .Lcopy_exception_ret
	fixupload REG_L a2, a1, 4*SZREG, .Lcopy_exception_ret
	fixupload REG_L a3, a1, 5*SZREG, .Lcopy_exception_ret
	fixupload REG_L a4, a1, 6*SZREG, .Lcopy_exception_ret
	fixupload REG_L a5, a1, 7*SZREG, .Lcopy_exception_ret
	fixupstore REG_S a2, a0, 4*SZREG, .Lcopy_exception_ret
	fixupstore REG_S a3, a0, 5*SZREG, .Lcopy_exception_ret
	fixupstore REG_S a4, a0, 6*SZREG, .Lcopy_exception_ret
	fixupstore REG_S a5, a0, 7*SZREG, .Lcopy_exception_ret

	BSTART.std fall
	addi a0, 8*SZREG, -> a0
	addi a1, 8*SZREG, -> a1

	BSTART.std cond, .Lcopy_pre_32
	setc.ltu a0, x0

	/* revert to original value */
	BSTART.std direct .Lbyte_copy_tail
	addi x0, 8*SZREG, -> x0


.Lshift_copy:
	/*
	 * Word copy with shifting.
	 * For misaligned copy we still perform aligned word copy, but
	 * we need to use the value fetched from the previous iteration and
	 * do some shifts.
	 * This is safe because reading is less than a word size.
	 *
	 * a0 - start of aligned dst
	 * a1 - start of src
	 * a3 - a1 & mask:(SZREG-1)
	 * x0 - end of uncopied dst
	 * x1 - end of aligned dst
	 */
	BSTART.std fall
	addi zero, SZREG-1, -> t
	/* calculating aligned word boundary for dst */
	and x0, t#1.not, -> x1
	/* Converting unaligned src to aligned src */
	and a1, t#1.not, -> a1

	/*
	 * Calculate shifts (bit num)
	 * x2 - prev shift
	 * x3 - current shift
	 */
	slli a3, 3, -> x2
	addi zero, SZREG*8, -> t
	sub t#1, x2, -> x3

	/* Load the first word to combine with second word */
	fixupload REG_L a5, a1, 0, .Lcopy_exception_ret

.Lcopy_tail_word:
	/* Main shifting copy
	*
	* a0 - start of aligned dst
	* a1 - start of aligned src
	* x1 - end of aligned dst
	*/
	BSTART.std fall
	/* a5 is loaded from a1, current bit position is at a1 + x2 */
	srl a5, x2, -> a4	/* lower bits */

	fixupload REG_L a5, a1, SZREG, .Lcopy_exception_ret

	BSTART.std fall
	addi a1, SZREG, -> a1
	sll a5, x3, -> t	/* upper bits */
	or t#1, a4, -> a2

	fixupstore REG_S a2, a0, 0, .Lcopy_exception_ret

	BSTART.std cond, .Lcopy_tail_word
	addi a0, SZREG, -> t
	addi t#1, 0, -> a0
	setc.ltu t#1, x1

	BSTART.std fall
	/* Revert src to original unaligned value  */
	add a1, a3, -> a1


.Lbyte_copy_tail:
	/*
	 * Byte copy anything left.
	 *
	 * a0 - start of remaining dst
	 * a1 - start of remaining src
	 * x0 - end of remaining dst
	 */
	BSTART.std cond .Lout_copy_user
	setc.geu a0, x0


.Lcopy_tail_one_by_one:
	fixupload lbi, a5, a1, 0, .Lcopy_exception_ret
	fixupstore sbi, a5, a0, 0, .Lcopy_exception_ret

	BSTART.std cond, .Lcopy_tail_one_by_one
	addi a1, 1, -> a1
	addi a0, 1, -> t
	addi t#1, 0, -> a0
	setc.ltu t#1, x0


.Lout_copy_user:
	disable_fixup
	BSTART.std ret
	/* Disable access to user memory */
	disable_um
	ori zero, 0, -> a0
	setc.tgt ra
ENDPROC(__asm_copy_to_user)
ENDPROC(__asm_copy_from_user)
EXPORT_SYMBOL(__asm_copy_to_user)
EXPORT_SYMBOL(__asm_copy_from_user)

ENTRY(__clear_user)
	/* __clear_user(void __user *to, unsigned long n)
	 * a0 - start of target region
	 * a1 - length of target region
	 */
	enable_fixup
	BSTART.std cond, .Lclear_is_end
	/* Enable access to user memory */
	enable_um

	addi zero, SZREG-1, -> t
	add a0, t#1, -> t
	add a0, a1, -> t
	addi t#1, 0, -> a3
	and t#2, t#3.not, -> x0
	and t#1, t#3.not, -> x1
	/*
	 * a3: terminal address of target region
	 * x0: lowest doubleword-aligned address in target region
	 * x1: highest doubleword-aligned address in target region
	 */
	setc.geu x0, x1

	BSTART.std cond, .Lclear_one_by_one
	setc.ltu a0, x0


.Lclear_head_by_word:
	fixupstore REG_S, zero, a0, 0, .Lclear_exception_ret

	BSTART.std cond, .Lclear_head_by_word
	addi a0, SZREG, -> t
	addi t#1, 0, -> a0
	setc.ltu t#1, x1


.Lclear_is_end:
	BSTART.std cond, .Lclear_remainder_one_by_one
	setc.ltu a0, a3

.Lclear_resume:
	disable_fixup
	BSTART.std ret
	/* Disable access to user memory */
	disable_um
	ori zero, 0, -> a0
	setc.tgt ra


.Lclear_one_by_one: /* Edge case: unalignment */
	/*	clear from a0 to x0 */
	fixupstore sbi, zero, a0, 0, .Lclear_exception_ret

	BSTART.std cond, .Lclear_one_by_one
	addi a0, 1, -> t
	addi t#1, 0, -> a0
	setc.ltu t#1, x0

	block_next_direct .Lclear_head_by_word

.Lclear_remainder_one_by_one: /* Edge case: remainder */
	fixupstore sbi, zero, a0, 0, .Lclear_exception_ret

	BSTART.std cond, .Lclear_remainder_one_by_one
	addi a0, 1, -> t
	addi t#1, 0, -> a0
	setc.ltu t#1, a3

	block_next_direct .Lclear_resume
ENDPROC(__clear_user)
EXPORT_SYMBOL(__clear_user)

.section .fixup,"ax"
	.balign 16
	/* Fixup code for __copy_user(10) and __clear_user(11) */
.Lcopy_exception_ret:
	disable_fixup
	BSTART.std ret	/* Disable access to user memory */
	disable_um
	subi sp, 8, -> t
	ldi [t#1, 0], -> a0  /* get the return value */
	setc.tgt ra

.Lclear_exception_ret:
	disable_fixup
	BSTART.std ret
	/* Disable access to user memory */
	disable_um
	addi a1, 0, -> a0
	setc.tgt ra
.previous
