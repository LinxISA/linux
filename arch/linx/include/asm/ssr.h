/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_LINX_SSR_H
#define _ASM_LINX_SSR_H

#include <linux/types.h>

/*
 * LinxISA System Status Registers (SSR) helpers for the bring-up port.
 *
 * QEMU models a small subset:
 *   - TIME: nanoseconds counter (read-only)
 *   - CSTATE: common state (ACR + interrupt enable bit)
 *   - Managing-ACR bank 0: EVBASE, IPENDING, EOIEI, EBPC, TIMER_TIMECMP, ...
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

static __always_inline u64 linx_ssr_read_ebpc_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f0b, ->%0" : "=r"(v));
	return v;
}

static __always_inline void linx_ssr_write_ebpc_acr0(u64 v)
{
	asm volatile("ssrset %0, 0x0f0b" : : "r"(v) : "memory");
}

static __always_inline u64 linx_ssr_read_etpc_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f0d, ->%0" : "=r"(v));
	return v;
}

static __always_inline u64 linx_ssr_read_ebpcn_acr0(void)
{
	u64 v;

	asm volatile("ssrget 0x0f0e, ->%0" : "=r"(v));
	return v;
}

static __always_inline void linx_ssr_write_timecmp_acr0(u64 abs_ns)
{
	asm volatile("ssrset %0, 0x0f21" : : "r"(abs_ns) : "memory");
}

#endif /* _ASM_LINX_SSR_H */
