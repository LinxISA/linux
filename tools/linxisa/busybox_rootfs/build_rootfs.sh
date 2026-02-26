#!/usr/bin/env bash
set -euo pipefail

LINUX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
INITRAMFS_DIR="$LINUX_ROOT/tools/linxisa/initramfs"

O="${O:-$LINUX_ROOT/build-linx-fixed}"
OUT_DIR="${OUT_DIR:-$O/linx-busybox-rootfs}"
ROOTFS_DIR="$OUT_DIR/rootfs"
ROOTFS_IMG="${ROOTFS_IMG:-$OUT_DIR/rootfs.ext2}"
ROOTFS_SIZE_MB="${ROOTFS_SIZE_MB:-64}"

MKFS_EXT4="$(command -v mkfs.ext2 || true)"
if [[ -z "$MKFS_EXT4" ]]; then
  MKFS_EXT4="$(command -v mke2fs || true)"
fi
if [[ -z "$MKFS_EXT4" ]]; then
  # Homebrew installs e2fsprogs as keg-only on macOS; prefer its sbin.
  if command -v brew >/dev/null 2>&1; then
    BREW_E2FS="$(brew --prefix e2fsprogs 2>/dev/null || true)"
    if [[ -n "$BREW_E2FS" && -x "$BREW_E2FS/sbin/mke2fs" ]]; then
      MKFS_EXT4="$BREW_E2FS/sbin/mke2fs"
    fi
  fi
fi
if [[ -z "$MKFS_EXT4" ]]; then
  echo "error: mkfs.ext2/mke2fs not found; install e2fsprogs first" >&2
  exit 2
fi

# Reuse the existing no-libc BusyBox-like multicall binary from initramfs bring-up.
bash "$INITRAMFS_DIR/build.sh"

BUSYBOX_BIN="$O/linx-initramfs/busybox"
if [[ ! -x "$BUSYBOX_BIN" ]]; then
  echo "error: expected busybox binary not found: $BUSYBOX_BIN" >&2
  exit 1
fi

rm -rf "$ROOTFS_DIR"
mkdir -p \
  "$ROOTFS_DIR/bin" \
  "$ROOTFS_DIR/sbin" \
  "$ROOTFS_DIR/dev" \
  "$ROOTFS_DIR/proc" \
  "$ROOTFS_DIR/sys" \
  "$ROOTFS_DIR/run" \
  "$ROOTFS_DIR/tmp" \
  "$ROOTFS_DIR/etc"

cp "$BUSYBOX_BIN" "$ROOTFS_DIR/bin/busybox"
chmod 0755 "$ROOTFS_DIR/bin/busybox"

ln -sf /bin/busybox "$ROOTFS_DIR/sbin/init"
ln -sf /bin/busybox "$ROOTFS_DIR/bin/sh"
ln -sf /bin/busybox "$ROOTFS_DIR/bin/help"
ln -sf /bin/busybox "$ROOTFS_DIR/bin/ls"
ln -sf /bin/busybox "$ROOTFS_DIR/bin/cat"
ln -sf /bin/busybox "$ROOTFS_DIR/sbin/poweroff"
ln -sf /bin/busybox "$ROOTFS_DIR/sbin/reboot"

mkdir -p "$(dirname "$ROOTFS_IMG")"
rm -f "$ROOTFS_IMG"

if [[ "$(basename "$MKFS_EXT4")" == "mke2fs" ]]; then
  "$MKFS_EXT4" -q -t ext2 -d "$ROOTFS_DIR" -F "$ROOTFS_IMG" "${ROOTFS_SIZE_MB}M"
else
  "$MKFS_EXT4" -q -d "$ROOTFS_DIR" -F "$ROOTFS_IMG" "${ROOTFS_SIZE_MB}M"
fi

echo "ok: $ROOTFS_IMG"
