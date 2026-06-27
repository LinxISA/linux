# LinxISA initramfs helpers (Linux bring-up)

This directory contains small, reproducible scripts used during LinxISA Linux
bring-up to generate a working initramfs for QEMU `virt`.

Current stage:

- `busybox.c` builds a minimal **no-libc** BusyBox-like multicall binary that
  runs as `/init` (PID 1), mounts `/proc` and `/sys`, and provides a tiny
  interactive shell on the console.
- `init.c` remains as a minimal syscall/ABI exerciser during early bring-up.
- `build.sh` builds `/init` and an initramfs `newc` archive using the kernel's
  `usr/gen_init_cpio` tool, including the required `/dev/console` node.

## Build + run (QEMU)

```bash
cd /Users/zhoubot/linx-isa/kernel/linux/tools/linxisa/initramfs
./build.sh

/Users/zhoubot/linx-isa/emulator/qemu/build-linx/qemu-system-linx64 \
  -nographic -monitor none -machine virt -m 512M -smp 1 \
  -kernel /Users/zhoubot/linx-isa/kernel/linux/build-linx-fixed/vmlinux \
  -initrd /Users/zhoubot/linx-isa/kernel/linux/build-linx-fixed/linx-initramfs/initramfs.cpio \
  -append "lpj=1000000 loglevel=8"
```

Once booted, you should land at a `#` prompt. Try `help`, `ls`, `cat`, `echo`.
`help` now also lists `ctx_tq_irq_test`, which validates the `BI=1` interrupt +
context-switch + EBARG(TQ/UQ) restore path, and `ctx_ri_step_trap_test`, which
validates `ri + ebreak(single-step trap) + kernel EBARG pollution + resume`.

## Regression scripts

Baseline smoke:

```bash
cd /Users/zhoubot/linx-isa
python3 kernel/linux/tools/linxisa/initramfs/smoke.py
```

Full userspace boot checks (`/proc` + `/sys` + exception applets):

```bash
cd /Users/zhoubot/linx-isa
python3 kernel/linux/tools/linxisa/initramfs/full_boot.py
```

Dedicated `BI=1 + context switch + t#1 restore` smoke:

```bash
cd /Users/zhoubot/linx-isa
LINX_DISABLE_TIMER_IRQ=0 \
QEMU=/Users/zhoubot/linx-isa/emulator/qemu/build-linx/qemu-system-linx64 \
python3 kernel/linux/tools/linxisa/initramfs/ctx_tq_irq_smoke.py
```

Dedicated `ri + step-trap + kernel pollution + resume` smoke:

```bash
cd /Users/zhoubot/linx-isa
LINX_DISABLE_TIMER_IRQ=1 \
QEMU=/Users/zhoubot/linx-isa/emulator/qemu/build-linx/qemu-system-linx64 \
python3 kernel/linux/tools/linxisa/initramfs/ctx_ri_step_trap_smoke.py
```

Negative guard (must fail fast):

```bash
cd /Users/zhoubot/linx-isa
LINX_DISABLE_TIMER_IRQ=1 \
QEMU=/Users/zhoubot/linx-isa/emulator/qemu/build-linx/qemu-system-linx64 \
python3 kernel/linux/tools/linxisa/initramfs/ctx_tq_irq_smoke.py
```

Virtio disk smoke (`virtio-mmio` transport + `virtio-blk` enumeration):

```bash
cd /Users/zhoubot/linx-isa
python3 kernel/linux/tools/linxisa/initramfs/virtio_disk_smoke.py
```
