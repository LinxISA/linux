#!/usr/bin/env bash
set -euo pipefail

LINUX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
REPO_ROOT="$(cd "$LINUX_ROOT/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

O="${O:-$LINUX_ROOT/build-linx-fixed}"
LLVM_BUILD="${LLVM_BUILD:-$REPO_ROOT/compiler/llvm/build-linxisa-clang}"

CLANG="${CLANG:-$LLVM_BUILD/bin/clang}"
GEN_INIT_CPIO="${GEN_INIT_CPIO:-$O/usr/gen_init_cpio}"

OUT_DIR="${OUT_DIR:-$O/linx-initramfs}"
BUSYBOX_BIN="$OUT_DIR/busybox"
CPIO_LIST="$OUT_DIR/initramfs.list"
CPIO_OUT="$OUT_DIR/initramfs.cpio"
LINX_PERF_BUILD="${LINX_PERF_BUILD:-0}"
INIT_VARIANT="${INIT_VARIANT:-busybox}"

need() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "error: missing required tool: $1" >&2
    exit 2
  }
}

need "$CLANG"
need cc

export PATH="$LLVM_BUILD/bin:$PATH"

mkdir -p "$OUT_DIR"

if [[ "$LINX_PERF_BUILD" == "1" || "$LINX_PERF_BUILD" == "true" || "$LINX_PERF_BUILD" == "yes" ]]; then
  OPT_FLAGS=(-O3 -falign-functions=32 -falign-loops=16)
else
  OPT_FLAGS=(-Oz)
fi

case "$INIT_VARIANT" in
  busybox)
    INIT_MAIN_SRC="$SCRIPT_DIR/busybox.c"
    INIT_EXTRA_SRCS=("$SCRIPT_DIR/sig_tramp.c")
    ;;
  tiny)
    INIT_MAIN_SRC="$SCRIPT_DIR/init.c"
    INIT_EXTRA_SRCS=()
    ;;
  tinytrap)
    INIT_MAIN_SRC="$SCRIPT_DIR/trap_init.c"
    INIT_EXTRA_SRCS=()
    ;;
  *)
    echo "error: unsupported INIT_VARIANT=$INIT_VARIANT (expected busybox, tiny, or tinytrap)" >&2
    exit 2
    ;;
esac

echo "[1/3] Ensuring usr/gen_init_cpio exists (O=$O) ..."
if [[ ! -x "$GEN_INIT_CPIO" ]]; then
  mkdir -p "$(dirname "$GEN_INIT_CPIO")"
  cc -O2 -Wall -Wextra -o "$GEN_INIT_CPIO" \
    "$LINUX_ROOT/usr/gen_init_cpio.c"
fi

echo "[2/3] Building minimal busybox (/init + /bin/sh, no-libc, static PIE ET_DYN) ..."
if [[ "$INIT_VARIANT" == "busybox" ]]; then
  "$CLANG" -target linx64-unknown-linux-gnu \
    "${OPT_FLAGS[@]}" -ffreestanding -fno-builtin -fpie -fpic \
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
    -nostdlib -static -fuse-ld=lld -Wl,-pie -Wl,-e,_start \
    -Wl,--build-id=none \
    -o "$BUSYBOX_BIN" "$INIT_MAIN_SRC" "${INIT_EXTRA_SRCS[@]-}"
else
  "$CLANG" -target linx64-unknown-linux-gnu \
    "${OPT_FLAGS[@]}" -ffreestanding -fno-builtin -fno-pic -fno-pie \
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables \
    -nostdlib -static -fuse-ld=lld -Wl,-e,_start -Wl,-Ttext=0x10000 \
    -Wl,--build-id=none \
    -o "$BUSYBOX_BIN" "$INIT_MAIN_SRC" "${INIT_EXTRA_SRCS[@]-}"
fi

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
dir /opt 0755 0 0
dir /opt/share 0755 0 0
file /bin/busybox ${BUSYBOX_BIN} 0755 0 0
file /init ${BUSYBOX_BIN} 0755 0 0
slink /bin/sh /bin/busybox 0755 0 0
slink /bin/echo /bin/busybox 0755 0 0
slink /bin/cat /bin/busybox 0755 0 0
slink /bin/ls /bin/busybox 0755 0 0
slink /bin/put /bin/busybox 0755 0 0
slink /bin/getdents64_probe /bin/busybox 0755 0 0
slink /bin/sigill_test /bin/busybox 0755 0 0
slink /bin/sigsegv_test /bin/busybox 0755 0 0
slink /sbin/reboot /bin/busybox 0755 0 0
slink /sbin/poweroff /bin/busybox 0755 0 0
EOF

"$GEN_INIT_CPIO" -o "$CPIO_OUT" "$CPIO_LIST"

echo "ok: $CPIO_OUT"
