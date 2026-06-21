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

Set `ROOTFS_IMG=/path/to/rootfs.ext2` to boot a specific rootfs image.
`ROOTFS` is accepted as a compatibility alias only when `ROOTFS_IMG` is unset;
when both are set, `ROOTFS_IMG` wins.

`boot.py` validates:
- `/sbin/init` entry
- interactive shell command execution
- `/proc/interrupts` readability
- poweroff path
