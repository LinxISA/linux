#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys


def linux_root() -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "LinxISA bring-up test runner. "
            "Runs initramfs smoke + full userspace boot checks."
        )
    )
    parser.add_argument(
        "--smoke-only",
        action="store_true",
        help="Run only the baseline initramfs smoke check.",
    )
    parser.add_argument(
        "--skip-virtio-disk",
        action="store_true",
        help="Skip virtio-mmio + virtio-blk disk smoke validation.",
    )
    args = parser.parse_args()

    root = linux_root()
    smoke = os.path.join(root, "tools", "linxisa", "initramfs", "smoke.py")
    full_boot = os.path.join(root, "tools", "linxisa", "initramfs", "full_boot.py")
    virtio_disk = os.path.join(root, "tools", "linxisa", "initramfs", "virtio_disk_smoke.py")
    if not os.path.exists(smoke):
        print(f"error: missing smoke test: {smoke}", file=sys.stderr)
        return 2
    if not os.path.exists(full_boot):
        print(f"error: missing full userspace boot test: {full_boot}", file=sys.stderr)
        return 2
    if not os.path.exists(virtio_disk):
        print(f"error: missing virtio disk smoke test: {virtio_disk}", file=sys.stderr)
        return 2

    rc = subprocess.call([sys.executable, smoke])
    if rc != 0 or args.smoke_only:
        return rc

    rc = subprocess.call([sys.executable, full_boot])
    if rc != 0:
        return rc

    if args.skip_virtio_disk:
        return 0
    return subprocess.call([sys.executable, virtio_disk])


if __name__ == "__main__":
    raise SystemExit(main())
