/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_VDSO_GETTIMEOFDAY_H
#define __ASM_VDSO_GETTIMEOFDAY_H

#ifndef __ASSEMBLY__

#include <asm/barrier.h>
#include <asm/unistd.h>
#include <asm/ssr.h>
#include <uapi/linux/time.h>

#define VDSO_HAS_CLOCK_GETRES	1

static __always_inline
int gettimeofday_fallback(struct __kernel_old_timeval *_tv,
			  struct timezone *_tz)
{
#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */
	register struct __kernel_old_timeval *tv asm("a0") = _tv;
	register struct timezone *tz asm("a1") = _tz;
	register long ret asm("a0");
	register long nr asm("a7") = __NR_gettimeofday;

	asm volatile ("scall 0\n"
		      : "=r" (ret)
		      : "r"(tv), "r"(tz), "r"(nr)
		      : "memory");
#else
	register long ret asm("a0");
	_tv = 0;
	_tz = 0;
#endif
	return ret;
}

static __always_inline
long clock_gettime_fallback(clockid_t _clkid, struct __kernel_timespec *_ts)
{
#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */
	register clockid_t clkid asm("a0") = _clkid;
	register struct __kernel_timespec *ts asm("a1") = _ts;
	register long ret asm("a0");
	register long nr asm("a7") = __NR_clock_gettime;

	asm volatile ("scall 0\n"
		      : "=r" (ret)
		      : "r"(clkid), "r"(ts), "r"(nr)
		      : "memory");
#else
	register long ret asm("a0");
	_clkid = 0;
	_ts = 0;
#endif
	return ret;
}

static __always_inline
int clock_getres_fallback(clockid_t _clkid, struct __kernel_timespec *_ts)
{
#if 0 /* TODO: 先注释掉，优先保证内核整体编译通过 */
	register clockid_t clkid asm("a0") = _clkid;
	register struct __kernel_timespec *ts asm("a1") = _ts;
	register long ret asm("a0");
	register long nr asm("a7") = __NR_clock_getres;

	asm volatile ("scall 0\n"
		      : "=r" (ret)
		      : "r"(clkid), "r"(ts), "r"(nr)
		      : "memory");
#else
	register long ret asm("a0");
	_clkid = 0;
	_ts = 0;
#endif
	return ret;
}

static __always_inline u64 __arch_get_hw_counter(s32 clock_mode,
						 const struct vdso_data *vd)
{
	/*
	 * The purpose of ssr_read(SSR_TIME) is to trap the system into
	 * ACR0 to obtain the value of SSR_TIME. Hence, unlike other
	 * architecture, no fence instructions surround the ssr_read()
	 */
	return ssr_read(SSR_TIME);
}

static __always_inline const struct vdso_data *__arch_get_vdso_data(void)
{
	return _vdso_data;
}

#ifdef CONFIG_TIME_NS
static __always_inline
const struct vdso_data *__arch_get_timens_vdso_data(const struct vdso_data *vd)
{
	return _timens_data;
}
#endif
#endif /* !__ASSEMBLY__ */

#endif /* __ASM_VDSO_GETTIMEOFDAY_H */
