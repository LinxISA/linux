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
 *   - Managing-ACR bank 0/1: EVBASE, IPENDING, EOIEI, EBPC/ETPC/EBPCN, TIMER_TIMECMP, ...
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

static __always_inline u64 linx_hl_ssrget(unsigned int ssrid)
{
	u64 v;

	asm volatile("hl.ssrget %1, ->%0" : "=r"(v) : "i"(ssrid) : "memory");
	return v;
}

static __always_inline void linx_hl_ssrset(u64 v, unsigned int ssrid)
{
	asm volatile("hl.ssrset %0, %1" : : "r"(v), "i"(ssrid) : "memory");
}

static __always_inline void linx_ssr_write_evbase_acr1(unsigned long v)
{
	linx_hl_ssrset(v, 0x1f01);
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

static __always_inline u64 linx_ssr_read_ebpc_acr1(void)
{
	return linx_hl_ssrget(0x1f0b);
}

static __always_inline void linx_ssr_write_ebpc_acr1(u64 v)
{
	linx_hl_ssrset(v, 0x1f0b);
}

static __always_inline u64 linx_ssr_read_etpc_acr1(void)
{
	return linx_hl_ssrget(0x1f0d);
}

static __always_inline u64 linx_ssr_read_ebpcn_acr1(void)
{
	return linx_hl_ssrget(0x1f0e);
}

static __always_inline void linx_ssr_write_timecmp_acr1(u64 abs_ns)
{
	linx_hl_ssrset(abs_ns, 0x1f21);
}

#endif /* _ASM_LINX_SSR_H */
