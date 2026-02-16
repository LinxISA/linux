#!/usr/bin/env bash
set -euo pipefail

LINUX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

O="${O:-$LINUX_ROOT/build-linx-fixed}"
LLVM_BUILD="${LLVM_BUILD:-/Users/zhoubot/llvm-project/build-linxisa-clang}"

CLANG="${CLANG:-$LLVM_BUILD/bin/clang}"
GEN_INIT_CPIO="${GEN_INIT_CPIO:-$O/usr/gen_init_cpio}"

OUT_DIR="${OUT_DIR:-$O/linx-initramfs}"
BUSYBOX_BIN="$OUT_DIR/busybox"
CPIO_LIST="$OUT_DIR/initramfs.list"
CPIO_OUT="$OUT_DIR/initramfs.cpio"

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: missing required tool: $1" >&2
    exit 2
  }
}

need "$CLANG"
need /usr/bin/clang

export PATH="$LLVM_BUILD/bin:$PATH"

mkdir -p "$OUT_DIR"

echo "[1/3] Ensuring usr/gen_init_cpio exists (O=$O) ..."
if [[ ! -x "$GEN_INIT_CPIO" ]]; then
  mkdir -p "$(dirname "$GEN_INIT_CPIO")"
  /usr/bin/clang -O2 -Wall -Wextra -o "$GEN_INIT_CPIO" \
    "$LINUX_ROOT/usr/gen_init_cpio.c"
fi

echo "[2/3] Building minimal busybox (/init + /bin/sh, no-libc, static PIE ET_DYN) ..."
"$CLANG" -target linx64-unknown-linux-gnu \
  -Oz -ffreestanding -fno-builtin -fpie -fpic \
  -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -nostdlib -static -fuse-ld=lld -Wl,-pie -Wl,-e,_start \
  -Wl,--build-id=none \
  -o "$BUSYBOX_BIN" "$SCRIPT_DIR/busybox.c" "$SCRIPT_DIR/sig_tramp.c"

echo "[3/3] Generating initramfs (newc) ..."
cat >"$CPIO_LIST" <<EOF
# Minimal initramfs for LinxISA Linux bring-up.
#
# /dev/console must exist for console_on_rootfs() to wire up stdin/out/err.
dir /dev 0755 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/null 0666 0 0 c 1 3
nod /dev/ttyS0 0600 0 0 c 4 64
dir /proc 0755 0 0
dir /sys 0755 0 0
dir /run 0755 0 0
dir /tmp 1777 0 0
dir /etc 0755 0 0
dir /bin 0755 0 0
dir /sbin 0755 0 0
file /bin/busybox ${BUSYBOX_BIN} 0755 0 0
file /init ${BUSYBOX_BIN} 0755 0 0
slink /bin/sh /bin/busybox 0755 0 0
slink /bin/echo /bin/busybox 0755 0 0
slink /bin/cat /bin/busybox 0755 0 0
slink /bin/ls /bin/busybox 0755 0 0
slink /bin/getdents64_probe /bin/busybox 0755 0 0
slink /bin/sigill_test /bin/busybox 0755 0 0
slink /bin/sigsegv_test /bin/busybox 0755 0 0
slink /sbin/reboot /bin/busybox 0755 0 0
slink /sbin/poweroff /bin/busybox 0755 0 0
EOF

"$GEN_INIT_CPIO" -o "$CPIO_OUT" "$CPIO_LIST"

echo "ok: $CPIO_OUT"
