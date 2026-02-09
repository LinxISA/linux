#!/usr/bin/env bash
set -euo pipefail

LINUX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

SRC="$LINUX_ROOT/Documentation/linxisa/bringup-plan.md"
QEMU_PLAN="/Users/zhoubot/qemu/docs/linxisa/bringup-plan.md"

if [[ ! -f "$SRC" ]]; then
  echo "error: missing source plan: $SRC" >&2
  exit 2
fi

if [[ ! -f "$QEMU_PLAN" ]]; then
  if [[ "${SKIP_QEMU:-0}" == "1" ]]; then
    echo "skip: missing QEMU plan path ($QEMU_PLAN) and SKIP_QEMU=1" >&2
    exit 0
  fi
  echo "error: missing QEMU plan path: $QEMU_PLAN" >&2
  echo "hint: set SKIP_QEMU=1 to skip syncing the QEMU mirror" >&2
  exit 2
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

cp "$SRC" "$tmp"
cp "$tmp" "$QEMU_PLAN"

if ! cmp -s "$SRC" "$QEMU_PLAN"; then
  echo "error: sync failed: $QEMU_PLAN does not match $SRC" >&2
  exit 1
fi

echo "ok: synced:"
echo "  $SRC"
echo "  -> $QEMU_PLAN"

