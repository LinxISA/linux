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
cd /Users/zhoubot/linux/tools/linxisa/initramfs
./build.sh

/Users/zhoubot/qemu/build/qemu-system-linx64 \
  -nographic -monitor none -machine virt -m 512M -smp 1 \
  -kernel /Users/zhoubot/linux/build-linx-fixed/vmlinux \
  -initrd /Users/zhoubot/linux/build-linx-fixed/linx-initramfs/initramfs.cpio \
  -append "lpj=1000000 loglevel=8"
```

Once booted, you should land at a `#` prompt. Try `help`, `ls`, `cat`, `echo`.

## Regression scripts

Baseline smoke:

```bash
cd /Users/zhoubot/linux
python3 tools/linxisa/initramfs/smoke.py
```

Full userspace boot checks (`/proc` + `/sys` + exception applets):

```bash
cd /Users/zhoubot/linux
python3 tools/linxisa/initramfs/full_boot.py
```

Virtio disk smoke (`virtio-mmio` transport + `virtio-blk` enumeration):

```bash
cd /Users/zhoubot/linux
python3 tools/linxisa/initramfs/virtio_disk_smoke.py
```
