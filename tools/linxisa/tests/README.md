# LinxISA bring-up microtests (planned)

This directory is the landing zone for small, reproducible tests used to
validate LinxISA Linux bring-up milestones.

Current state (as of 2026-02-07):

- The bring-up initramfs provides an interactive shell and a smoke test exists:
  `/Users/zhoubot/linux/tools/linxisa/initramfs/smoke.py`.
- The actual conformance microtests listed in
  `Documentation/linxisa/bringup-plan.md` are not implemented yet.

When adding tests here, keep them:

- Small and single-purpose (one bug class per test).
- Runnable under QEMU `virt`.
- Suitable for QEMU-vs-RTL difftest gating (produce deterministic outcomes, or
  define comparison rules).

