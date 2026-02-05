# LinxISA bring-up workspace

This Linux source tree is used for a LinxISA Linux port bring-up.

- Baseline: Linux `6.18.8` (kernel.org tarball)
- Working branch: `linxisa/bringup`
- Upstream reference remote: `upstream` (linux-stable)

## QEMU reference

The LinxISA QEMU target lives in `/Users/zhoubot/qemu`:

- Machine: `-machine virt`
- Kernel load base: `0x00010000`
- UART MMIO: `0x10000000` (write `+0x0` to print)
- Exit register: `0x10000004` (write to shutdown)

See `/Users/zhoubot/qemu/docs/linxisa/README.md` for the full machine notes and
quick-start scripts.

## Bring-up workflow

1. Validate toolchain + QEMU: run `/Users/zhoubot/qemu/scripts/linxisa/run-hello-uart.sh`.
2. Define a LinxISA Linux boot ABI (register passing for DTB/initrd/cmdline).
3. Add `arch/linx/` incrementally until `vmlinux` boots on QEMU `virt`.

For the detailed milestone checklist, see `/Users/zhoubot/qemu/docs/linxisa/bringup-plan.md`.

