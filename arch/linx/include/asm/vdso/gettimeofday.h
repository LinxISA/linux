/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_VDSO_GETTIMEOFDAY_H
#define __ASM_VDSO_GETTIMEOFDAY_H

#ifndef __ASSEMBLER__

#include <asm/barrier.h>
#include <asm/unistd.h>
#include <asm/ssr.h>
#include <uapi/linux/time.h>

#define VDSO_HAS_CLOCK_GETRES	1

static __always_inline
int gettimeofday_fallback(struct __kernel_old_timeval *_tv,
			  struct timezone *_tz)
{
	register struct __kernel_old_timeval *tv asm("a0") = _tv;
	register struct timezone *tz asm("a1") = _tz;
	register long nr asm("a7") = __NR_gettimeofday;

	asm volatile ("BSTART.sys fall\n"
		      "acrc 1\n"
		      : "+r" (tv), "+r" (tz)
		      : "r" (nr)
		      : "memory");

	return (long)tv;
}

static __always_inline
long clock_gettime_fallback(clockid_t _clkid, struct __kernel_timespec *_ts)
{
	register clockid_t clkid asm("a0") = _clkid;
	register struct __kernel_timespec *ts asm("a1") = _ts;
	register long nr asm("a7") = __NR_clock_gettime;

	asm volatile ("BSTART.sys fall\n"
		      "acrc 1\n"
		      : "+r" (clkid), "+r" (ts)
		      : "r" (nr)
		      : "memory");

	return (long)clkid;
}

static __always_inline
int clock_getres_fallback(clockid_t _clkid, struct __kernel_timespec *_ts)
{
	register clockid_t clkid asm("a0") = _clkid;
	register struct __kernel_timespec *ts asm("a1") = _ts;
	register long nr asm("a7") = __NR_clock_getres;

	asm volatile ("BSTART.sys fall\n"
		      "acrc 1\n"
		      : "+r" (clkid), "+r" (ts)
		      : "r" (nr)
		      : "memory");

	return (long)clkid;
}

static __always_inline u64 __arch_get_hw_counter(s32 clock_mode,
						 const struct vdso_time_data *vd)
{
	/*
	 * The purpose of ssr_read(SSR_TIME) is to trap the system into
	 * ACR0 to obtain the value of SSR_TIME. Hence, unlike other
	 * architecture, no fence instructions surround the ssr_read()
	 */
	return ssr_read(SSR_TIME);
}

#endif /* !__ASSEMBLER__ */

#endif /* __ASM_VDSO_GETTIMEOFDAY_H */
