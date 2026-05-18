#ifndef _ASM_RISCV_FENCE_H
#define _ASM_RISCV_FENCE_H

#include <asm/block-def.h>

/*
 * define an stringify memory fence block used in extended asm
 *
 * @pred, @succ:
 *     memory ordering flag, e.g. rw, r, w, iorw
 */
#define ASM_FENCE_BLOCK(pred, succ)			\
	"BSTART.sys fall\n" \
		"fence.d " #pred ", " #succ "	\n"

#define ASM_RELEASE_BARRIER_BLOCK ASM_FENCE_BLOCK(rw, r)
#define ASM_ACQUIRE_BARRIER_BLOCK ASM_FENCE_BLOCK(r, rw)
#define ASM_FULL_BARRIER_BLOCK    ASM_FENCE_BLOCK(rw, rw)

#ifdef CONFIG_SMP
#define RISCV_ACQUIRE_BARRIER	ASM_RELEASE_BARRIER_BLOCK
#define RISCV_RELEASE_BARRIER	ASM_ACQUIRE_BARRIER_BLOCK
#else
#define RISCV_ACQUIRE_BARRIER
#define RISCV_RELEASE_BARRIER
#endif

#endif	/* _ASM_RISCV_FENCE_H */
