#!/usr/bin/env python3
import os
import pathlib
import subprocess
import sys


def main() -> int:
    linux_root = pathlib.Path(__file__).resolve().parents[3]

    o_dir = pathlib.Path(os.environ.get("O", str(linux_root / "build-linx-fixed")))
    qemu = pathlib.Path(os.environ.get("QEMU", "/Users/zhoubot/qemu/build/qemu-system-linx64"))

    kernel = pathlib.Path(os.environ.get("KERNEL", str(o_dir / "vmlinux")))
    initrd = pathlib.Path(
        os.environ.get("INITRD", str(o_dir / "linx-initramfs" / "initramfs.cpio"))
    )

    mem = os.environ.get("MEM", "512M")
    smp = os.environ.get("SMP", "1")
    append = os.environ.get("APPEND", "lpj=1000000 loglevel=1")
    timeout_s = int(os.environ.get("TIMEOUT", "60"))

    script = os.environ.get(
        "SCRIPT",
        "help\n"
        "ls /\n"
        "probe /init\n"
        "cat /no-such\n"
        "poweroff\n",
    )

    if os.environ.get("SKIP_BUILD", "") not in {"1", "true", "yes"}:
        build_sh = pathlib.Path(__file__).with_name("build.sh")
        subprocess.run([str(build_sh)], check=True)

    cmd = [
        str(qemu),
        "-nographic",
        "-monitor",
        "none",
        "-machine",
        "virt",
        "-m",
        mem,
        "-smp",
        smp,
        "-kernel",
        str(kernel),
        "-initrd",
        str(initrd),
        "-append",
        append,
    ]

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    timed_out = False
    try:
        out, _ = proc.communicate(script.encode("utf-8"), timeout=timeout_s)
    except subprocess.TimeoutExpired:
        timed_out = True
        proc.kill()
        out, _ = proc.communicate()

    text = out.decode("utf-8", errors="replace")

    # Minimal invariants: interactive initramfs + applets work.
    want = [
        "cmds:",
        "# ls /",
        "dev",
        "# cat /no-such",
        "E: fffffffffffffffe",
    ]
    missing = [w for w in want if w not in text]
    if missing:
        sys.stderr.write("error: smoke check failed; missing: %s\n" % ", ".join(missing))
        sys.stderr.write("\n")
        sys.stderr.write("\n".join(text.splitlines()[-200:]))
        sys.stderr.write("\n")
        sys.stderr.flush()
        return 2

    if timed_out:
        # The guest may halt without triggering a QEMU shutdown request.
        sys.stderr.write("note: qemu did not exit; killed after TIMEOUT=%ds\n" % timeout_s)
        sys.stderr.flush()

    # Print a focused excerpt for human inspection.
    keep = []
    for ln in text.splitlines():
        s = ln.strip()
        if (
            s.startswith("#")
            or s.startswith("cmds:")
            or s.startswith("n1=")
            or s.startswith("E:")
            or s.startswith("reboot:")
        ):
            keep.append(ln)
        elif s in {"bin", "dev", "etc", "init", "proc", "run", "sbin", "sys", "tmp"}:
            keep.append(ln)
    sys.stdout.write("\n".join(keep[-200:]) + "\n")
    sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
