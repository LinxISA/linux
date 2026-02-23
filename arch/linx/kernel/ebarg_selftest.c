// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/delay.h>

#include <asm/debug_uart.h>
#include <asm/irqflags.h>
#include <asm/processor.h>
#include <asm/ssr.h>

/* Defined in traps.c */
extern volatile u64 linx_async_irq_count;

/* ACR1 banked EBARG SSR IDs (bring-up profile). */
#define SSR_EBARG0_ACR1         0x1f40
#define SSR_EBARG_BPC_CUR_ACR1  0x1f41
#define SSR_EBARG_BPC_TGT_ACR1  0x1f42
#define SSR_EBARG_TPC_ACR1      0x1f43
#define SSR_EBARG_LRA_ACR1      0x1f44
#define SSR_EBARG_TQ0_ACR1      0x1f45
#define SSR_EBARG_TQ1_ACR1      0x1f46
#define SSR_EBARG_TQ2_ACR1      0x1f47
#define SSR_EBARG_TQ3_ACR1      0x1f48
#define SSR_EBARG_UQ0_ACR1      0x1f49
#define SSR_EBARG_UQ1_ACR1      0x1f4a
#define SSR_EBARG_UQ2_ACR1      0x1f4b
#define SSR_EBARG_UQ3_ACR1      0x1f4c
#define SSR_EBARG_LB_ACR1       0x1f4d
#define SSR_EBARG_LC_ACR1       0x1f4e
#define SSR_EBARG_EXT_PTR_ACR1  0x1f4f
#define SSR_EBARG_EXT_META_ACR1 0x1f50

#define EBARG_SENTINEL_BASE 0x5a5a000000000000ull

static __always_inline void ebarg_write(int ssrid, u64 v)
{
	switch (ssrid) {
	case SSR_EBARG0_ACR1:         linx_hl_ssrset(v, SSR_EBARG0_ACR1); break;
	case SSR_EBARG_BPC_CUR_ACR1:  linx_hl_ssrset(v, SSR_EBARG_BPC_CUR_ACR1); break;
	case SSR_EBARG_BPC_TGT_ACR1:  linx_hl_ssrset(v, SSR_EBARG_BPC_TGT_ACR1); break;
	case SSR_EBARG_TPC_ACR1:      linx_hl_ssrset(v, SSR_EBARG_TPC_ACR1); break;
	case SSR_EBARG_LRA_ACR1:      linx_hl_ssrset(v, SSR_EBARG_LRA_ACR1); break;
	case SSR_EBARG_TQ0_ACR1:      linx_hl_ssrset(v, SSR_EBARG_TQ0_ACR1); break;
	case SSR_EBARG_TQ1_ACR1:      linx_hl_ssrset(v, SSR_EBARG_TQ1_ACR1); break;
	case SSR_EBARG_TQ2_ACR1:      linx_hl_ssrset(v, SSR_EBARG_TQ2_ACR1); break;
	case SSR_EBARG_TQ3_ACR1:      linx_hl_ssrset(v, SSR_EBARG_TQ3_ACR1); break;
	case SSR_EBARG_UQ0_ACR1:      linx_hl_ssrset(v, SSR_EBARG_UQ0_ACR1); break;
	case SSR_EBARG_UQ1_ACR1:      linx_hl_ssrset(v, SSR_EBARG_UQ1_ACR1); break;
	case SSR_EBARG_UQ2_ACR1:      linx_hl_ssrset(v, SSR_EBARG_UQ2_ACR1); break;
	case SSR_EBARG_UQ3_ACR1:      linx_hl_ssrset(v, SSR_EBARG_UQ3_ACR1); break;
	case SSR_EBARG_LB_ACR1:       linx_hl_ssrset(v, SSR_EBARG_LB_ACR1); break;
	case SSR_EBARG_LC_ACR1:       linx_hl_ssrset(v, SSR_EBARG_LC_ACR1); break;
	case SSR_EBARG_EXT_PTR_ACR1:  linx_hl_ssrset(v, SSR_EBARG_EXT_PTR_ACR1); break;
	case SSR_EBARG_EXT_META_ACR1: linx_hl_ssrset(v, SSR_EBARG_EXT_META_ACR1); break;
	default: break;
	}
}

static __always_inline u64 ebarg_read(int ssrid)
{
	switch (ssrid) {
	case SSR_EBARG0_ACR1:         return linx_hl_ssrget(SSR_EBARG0_ACR1);
	case SSR_EBARG_BPC_CUR_ACR1:  return linx_hl_ssrget(SSR_EBARG_BPC_CUR_ACR1);
	case SSR_EBARG_BPC_TGT_ACR1:  return linx_hl_ssrget(SSR_EBARG_BPC_TGT_ACR1);
	case SSR_EBARG_TPC_ACR1:      return linx_hl_ssrget(SSR_EBARG_TPC_ACR1);
	case SSR_EBARG_LRA_ACR1:      return linx_hl_ssrget(SSR_EBARG_LRA_ACR1);
	case SSR_EBARG_TQ0_ACR1:      return linx_hl_ssrget(SSR_EBARG_TQ0_ACR1);
	case SSR_EBARG_TQ1_ACR1:      return linx_hl_ssrget(SSR_EBARG_TQ1_ACR1);
	case SSR_EBARG_TQ2_ACR1:      return linx_hl_ssrget(SSR_EBARG_TQ2_ACR1);
	case SSR_EBARG_TQ3_ACR1:      return linx_hl_ssrget(SSR_EBARG_TQ3_ACR1);
	case SSR_EBARG_UQ0_ACR1:      return linx_hl_ssrget(SSR_EBARG_UQ0_ACR1);
	case SSR_EBARG_UQ1_ACR1:      return linx_hl_ssrget(SSR_EBARG_UQ1_ACR1);
	case SSR_EBARG_UQ2_ACR1:      return linx_hl_ssrget(SSR_EBARG_UQ2_ACR1);
	case SSR_EBARG_UQ3_ACR1:      return linx_hl_ssrget(SSR_EBARG_UQ3_ACR1);
	case SSR_EBARG_LB_ACR1:       return linx_hl_ssrget(SSR_EBARG_LB_ACR1);
	case SSR_EBARG_LC_ACR1:       return linx_hl_ssrget(SSR_EBARG_LC_ACR1);
	case SSR_EBARG_EXT_PTR_ACR1:  return linx_hl_ssrget(SSR_EBARG_EXT_PTR_ACR1);
	case SSR_EBARG_EXT_META_ACR1: return linx_hl_ssrget(SSR_EBARG_EXT_META_ACR1);
	default: return 0;
	}
}

struct ebarg_pair {
	int id;
	u64 expected;
};

static int __init linx_ebarg_selftest_init(void)
{
	/* Keep the test lightweight; it is meant to catch EBARG restore bugs. */
	struct ebarg_pair pairs[] = {
		{ SSR_EBARG0_ACR1,         EBARG_SENTINEL_BASE | 0x40 },
		{ SSR_EBARG_BPC_CUR_ACR1,  EBARG_SENTINEL_BASE | 0x41 },
		{ SSR_EBARG_BPC_TGT_ACR1,  EBARG_SENTINEL_BASE | 0x42 },
		{ SSR_EBARG_TPC_ACR1,      EBARG_SENTINEL_BASE | 0x43 },
		{ SSR_EBARG_LRA_ACR1,      EBARG_SENTINEL_BASE | 0x44 },
		{ SSR_EBARG_TQ0_ACR1,      EBARG_SENTINEL_BASE | 0x45 },
		{ SSR_EBARG_TQ1_ACR1,      EBARG_SENTINEL_BASE | 0x46 },
		{ SSR_EBARG_TQ2_ACR1,      EBARG_SENTINEL_BASE | 0x47 },
		{ SSR_EBARG_TQ3_ACR1,      EBARG_SENTINEL_BASE | 0x48 },
		{ SSR_EBARG_UQ0_ACR1,      EBARG_SENTINEL_BASE | 0x49 },
		{ SSR_EBARG_UQ1_ACR1,      EBARG_SENTINEL_BASE | 0x4a },
		{ SSR_EBARG_UQ2_ACR1,      EBARG_SENTINEL_BASE | 0x4b },
		{ SSR_EBARG_UQ3_ACR1,      EBARG_SENTINEL_BASE | 0x4c },
		{ SSR_EBARG_LB_ACR1,       EBARG_SENTINEL_BASE | 0x4d },
		{ SSR_EBARG_LC_ACR1,       EBARG_SENTINEL_BASE | 0x4e },
		{ SSR_EBARG_EXT_PTR_ACR1,  EBARG_SENTINEL_BASE | 0x4f },
		{ SSR_EBARG_EXT_META_ACR1, EBARG_SENTINEL_BASE | 0x50 },
	};
	const size_t n = sizeof(pairs) / sizeof(pairs[0]);
	u64 before;
	u64 deadline;
	size_t i;

	linx_debug_uart_puts("[linx] EBARG selftest: start\n");

	/* Program sentinel EBARG values. */
	for (i = 0; i < n; i++)
		ebarg_write(pairs[i].id, pairs[i].expected);

	/* Ensure interrupts are enabled so the timer IRQ can preempt us. */
	arch_local_irq_enable();

	before = linx_async_irq_count;
	/* Arm a one-shot timer interrupt ~1ms in the future. */
	linx_ssr_write_timecmp_acr1(linx_ssr_read_time() + 1000000ull);

	deadline = linx_ssr_read_time() + 50000000ull; /* 50ms */
	while (linx_async_irq_count == before && linx_ssr_read_time() < deadline)
		cpu_relax();

	/* Disarm timer to avoid repeated interrupts during boot. */
	linx_ssr_write_timecmp_acr1(0);

	if (linx_async_irq_count == before) {
		linx_debug_uart_puts("[linx] EBARG selftest: FAIL (no timer irq)\n");
		return 0;
	}

	/* Verify EBARG values survived an async interrupt + trap return. */
	for (i = 0; i < n; i++) {
		u64 got = ebarg_read(pairs[i].id);

		if (got != pairs[i].expected) {
			linx_debug_uart_puts("[linx] EBARG selftest: FAIL id=");
			linx_debug_uart_puthex_ulong((unsigned long)pairs[i].id);
			linx_debug_uart_puts(" exp=");
			linx_debug_uart_puthex_ulong((unsigned long)pairs[i].expected);
			linx_debug_uart_puts(" got=");
			linx_debug_uart_puthex_ulong((unsigned long)got);
			linx_debug_uart_puts("\n");
			return 0;
		}
	}

	linx_debug_uart_puts("[linx] EBARG selftest: PASS\n");
	return 0;
}
late_initcall(linx_ebarg_selftest_init);
