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
            "LinxISA bring-up test runner (initial scaffolding). "
            "Today this primarily delegates to the initramfs smoke test."
        )
    )
    parser.parse_args()

    root = linux_root()
    smoke = os.path.join(root, "tools", "linxisa", "initramfs", "smoke.py")
    if not os.path.exists(smoke):
        print(f"error: missing smoke test: {smoke}", file=sys.stderr)
        return 2

    # The planned microtests listed in Documentation/linxisa/bringup-plan.md are
    # not implemented yet; keep this runner useful by delegating to the
    # known-good smoke test entrypoint.
    return subprocess.call([sys.executable, smoke])


if __name__ == "__main__":
    raise SystemExit(main())
