# LinxISA BusyBox Rootfs Boot Helpers

This directory builds and boots a minimal ext4 rootfs backed by a virtio-blk
disk, using the existing no-libc BusyBox-like multicall binary.

## Build rootfs image

```bash
bash kernel/linux/tools/linxisa/busybox_rootfs/build_rootfs.sh
```

## Boot gate

```bash
python3 kernel/linux/tools/linxisa/busybox_rootfs/boot.py
```

Set `QEMU=/path/to/a/clean/qemu-system-linx64` explicitly. Set
`ROOTFS_IMG=/path/to/rootfs.ext2` to boot a specific rootfs image; `ROOTFS`
is not accepted as an alias.

`boot.py` validates:
- `/sbin/init` entry
- interactive shell command execution
- `/proc/interrupts` readability
- poweroff path
