# LinxISA bring-up workspace

This Linux source tree is used for a LinxISA Linux port bring-up.

- Baseline: Linux `6.18.8` (kernel.org tarball)
- Working branch: `linxisa/bringup`
- Upstream reference remote: `upstream` (linux-stable)

## Host prerequisites

- Linux kernel builds require **GNU Make >= 4.0**. On macOS, you typically want
  Homebrew `make` (often invoked as `gmake`).

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
2. Validate the Linux boot ABI + DTB handoff using the bootstub:
   `/Users/zhoubot/qemu/scripts/linxisa/run-linux-bootstub.sh`.
3. Define and iterate the LinxISA ABI docs in `Documentation/linxisa/abi.md`.
4. Add `arch/linx/` incrementally until `vmlinux` boots on QEMU `virt`.

For the detailed milestone checklist, see `/Users/zhoubot/qemu/docs/linxisa/bringup-plan.md`.

For the current boot status, known issues, and next steps, see
`Documentation/linxisa/bringup-status.md`.

## Current boot command (bring-up)

Latest validated bring-up run (kernel + initramfs):

```bash
cd /Users/zhoubot/linux/tools/linxisa/initramfs
./build.sh

/Users/zhoubot/qemu/build/qemu-system-linx64 \
  -nographic -monitor none -machine virt -m 512M -smp 1 \
  -kernel /Users/zhoubot/linux/build-linx-fixed/vmlinux \
  -initrd /Users/zhoubot/linux/build-linx-fixed/linx-initramfs/initramfs.cpio \
  -append "lpj=1000000 loglevel=1"
```

You should land at a `# ` prompt from the bring-up initramfs `/init`. Try
`help`, `ls /`, `probe /init`, `cat /no-such`.

Smoke test (kills QEMU after a timeout because the guest currently halts
without triggering a QEMU shutdown request):

```bash
cd /Users/zhoubot/linux
SKIP_BUILD=1 TIMEOUT=25 python3 tools/linxisa/initramfs/smoke.py
```

## Bootstub helper

The bring-up bootstub lives in `tools/linxisa/bootstub/` and is intended to be
loaded via QEMU `-kernel` as an `ET_REL` object (`bootstub.o`).
