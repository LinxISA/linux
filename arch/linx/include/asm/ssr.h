/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_SSR_H
#define _ASM_LINX_SSR_H

#include <linux/types.h>
#include <linux/build_bug.h>

/*
 * LinxISA System Status Registers (SSR) helpers for the bring-up port.
 *
 * QEMU models a small subset:
 *   - TIME: nanoseconds counter (read-only)
 *   - CSTATE: common state (ACR + interrupt enable bit)
 *   - Managing-ACR bank 0/1: ECSTATE, EVBASE, TRAPNO, TRAPARG0, ETEMP/ETEMP0,
 *     EBARG group (0xnf40+), IPENDING, EOIEI, TIMER_TIMECMP, ...
 */

/* CSTATE (bring-up encoding; keep in sync with QEMU). */
#define LINX_CSTATE_I_BIT	(1UL << 4)

static __always_inline u64 linx_ssr_read_time(void)
{
	u64 v;

	asm volatile("ssrget TIME, ->%0" : "=r"(v));
	return v;
}

static __always_inline unsigned long linx_ssr_read_cstate(void)
{
	unsigned long v;

	asm volatile("ssrget CSTATE, ->%0" : "=r"(v) : : "memory");
	return v;
}

static __always_inline void linx_ssr_write_cstate(unsigned long v)
{
	asm volatile("ssrset %0, CSTATE" : : "r"(v) : "memory");
}

static __always_inline void linx_ssr_write_evbase_acr0(unsigned long v)
{
	asm volatile("ssrset %0, 0x0f01" : : "r"(v) : "memory");
}

/*
 * HL.SSRGET/HL.SSRSET encode the SSR ID as an immediate in the instruction.
 * Enforce compile-time constant IDs to keep callsites honest.
 */
#define linx_hl_ssrget(ssrid_imm)                                           \
	({                                                                  \
		u64 __v;                                                    \
		BUILD_BUG_ON(!__builtin_constant_p(ssrid_imm));              \
		asm volatile("hl.ssrget %1, ->%0"                            \
			     : "=r"(__v)                                     \
			     : "i"(ssrid_imm)                                 \
			     : "memory");                                     \
		__v;                                                        \
	})

#define linx_hl_ssrset(v, ssrid_imm)                                         \
	do {                                                                  \
		BUILD_BUG_ON(!__builtin_constant_p(ssrid_imm));              \
		asm volatile("hl.ssrset %0, %1"                              \
			     :                                               \
			     : "r"(v), "i"(ssrid_imm)                          \
			     : "memory");                                     \
	} while (0)

static __always_inline void linx_ssr_write_evbase_acr1(unsigned long v)
{
	linx_hl_ssrset(v, 0x1f01);
}

static __always_inline u64 linx_ssr_read_etemp_acr1(void)
{
	return linx_hl_ssrget(0x1f05);
}

static __always_inline void linx_ssr_write_etemp_acr1(u64 v)
{
	linx_hl_ssrset(v, 0x1f05);
}

static __always_inline u64 linx_ssr_read_etemp0_acr1(void)
{
	return linx_hl_ssrget(0x1f06);
}

static __always_inline void linx_ssr_write_etemp0_acr1(u64 v)
{
	linx_hl_ssrset(v, 0x1f06);
}

static __always_inline u64 linx_ssr_read_ecstate_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f00, ->%0" : "=r"(v));
	return v;
}

static __always_inline u64 linx_ssr_read_trapno_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f02, ->%0" : "=r"(v));
	return v;
}

static __always_inline u64 linx_ssr_read_traparg0_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f03, ->%0" : "=r"(v));
	return v;
}

static __always_inline u64 linx_ssr_read_ipending_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f08, ->%0" : "=r"(v));
	return v;
}

static __always_inline void linx_ssr_write_eoiei_acr0(u64 irq_id)
{
	asm volatile("ssrset %0, 0x0f0a" : : "r"(irq_id) : "memory");
}

static __always_inline void linx_ssr_write_timecmp_acr0(u64 abs_ns)
{
	asm volatile("ssrset %0, 0x0f21" : : "r"(abs_ns) : "memory");
}

static __always_inline u64 linx_ssr_read_ecstate_acr1(void)
{
	return linx_hl_ssrget(0x1f00);
}

static __always_inline u64 linx_ssr_read_trapno_acr1(void)
{
	return linx_hl_ssrget(0x1f02);
}

static __always_inline u64 linx_ssr_read_traparg0_acr1(void)
{
	return linx_hl_ssrget(0x1f03);
}

static __always_inline u64 linx_ssr_read_ipending_acr1(void)
{
	return linx_hl_ssrget(0x1f08);
}

static __always_inline void linx_ssr_write_eoiei_acr1(u64 irq_id)
{
	linx_hl_ssrset(irq_id, 0x1f0a);
}

static __always_inline void linx_ssr_write_timecmp_acr1(u64 abs_ns)
{
	linx_hl_ssrset(abs_ns, 0x1f21);
}

/*
 * v0.2 trap ABI: EBARG register group (ACR-scoped, 0xnf40+).
 *
 * Software-visible trap/restore inputs are read from this block.
 */
static __always_inline u64 linx_ssr_read_ebarg0_acr1(void)
{
	return linx_hl_ssrget(0x1f40);
}

static __always_inline u64 linx_ssr_read_ebarg_bpc_cur_acr1(void)
{
	return linx_hl_ssrget(0x1f41);
}

static __always_inline void linx_ssr_write_ebarg_bpc_cur_acr1(u64 v)
{
	linx_hl_ssrset(v, 0x1f41);
}

static __always_inline u64 linx_ssr_read_ebarg_tpc_acr1(void)
{
	return linx_hl_ssrget(0x1f43);
}

static __always_inline void linx_ssr_write_ebarg_tpc_acr1(u64 v)
{
	linx_hl_ssrset(v, 0x1f43);
}

#endif /* _ASM_LINX_SSR_H */
