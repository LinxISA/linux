# Timer IRQ "architectural state pollution" (EBARG restore) — Linx Linux bring-up

## Symptom

Enabling the timer interrupt (“timer on”) caused user-mode architectural state to become corrupted after returning from an async interrupt.
This presented as non-deterministic behavior and state drift across timer IRQs.

## Root cause

The Linx trap-return path only re-seeded **a small subset** of the EBARG restore group before `acre` back to user ACR.

In particular, the return path restored:

- `SSR_ECSTATE_ACR1`
- `SSR_EBARG_BPC_CUR_ACR1`, `SSR_EBARG_BPC_TGT_ACR1`, `SSR_EBARG_TPC_ACR1`

…but did **not** restore the rest of EBARG that had been snapshotted on trap entry (e.g. `EBARG0`, `LRA`, `TQ*`, `UQ*`, `LB/LC`, `EXT_PTR/EXT_META`).

Because timer/IRQ nesting can clobber restore state, returning without restoring the full EBARG group allowed stale/garbage values to leak into the next userspace block execution, producing architectural state pollution.

## Fix

Commit: `8035b415164de7e1fca36d397d84a54497e1439`

1) **Restore full EBARG group on trap return**

- File: `arch/linx/kernel/entry.S`
- Label: `.Llinx_return_default` (see around lines 217–264)

Now restores:

- `EBARG0`
- `BPC_CUR`, `BPC_TGT`, `TPC`
- `LRA`
- `TQ0..3`, `UQ0..3`
- `LB`, `LC`
- `EXT_PTR`, `EXT_META`

from the `pt_regs` snapshot into the corresponding `SSR_EBARG*_ACR1` registers.

2) **Seed full EBARG group on first entry to userspace**

- File: `arch/linx/kernel/entry.S`
- Function: `linx_enter_user` (see around lines 304–343)

This avoids entering user ACR with uninitialized EBARG values.

3) **Enable timer IRQ and add a regression selftest**

- File: `arch/linx/kernel/traps.c` — enables timer IRQ path and tracks `linx_async_irq_count`.
- File: `arch/linx/kernel/ebarg_selftest.c` — late initcall test that:
  - writes sentinel values into the EBARG SSRs,
  - arms a one-shot timer interrupt via `TIMECMP`,
  - verifies all EBARG fields are unchanged after the async interrupt/return.

On success it prints:

- `[linx] EBARG selftest: PASS`

On failure it prints the first mismatched EBARG id + expected/got.

## How to run / validate

1) Build kernel with the Linx toolchain.
2) Boot under `qemu-system-linx64` and observe UART output.
3) Confirm the selftest prints `PASS` after a timer IRQ fires.

## Notes

- The approach is consistent with block-structured migration rules: asynchronous events must not leak partial restore state across block boundaries.
- If further architectural state pollution remains with timer enabled, the next suspects are: incomplete trap snapshot on entry, missing/incorrect EOIEI behavior, or additional banked SSR groups that require analogous restore.
