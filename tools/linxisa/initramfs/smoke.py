#!/usr/bin/env python3
import os
import pathlib
import select
import subprocess
import sys
import time


def main() -> int:
    linux_root = pathlib.Path(__file__).resolve().parents[3]

    o_dir = pathlib.Path(os.environ.get("O", str(linux_root / "build-linx-fixed")))
    qemu_default_candidates = [
        pathlib.Path("/Users/zhoubot/qemu/build/qemu-system-linx64"),
        pathlib.Path("/Users/zhoubot/qemu/build-tci/qemu-system-linx64"),
    ]
    qemu_default = next((p for p in qemu_default_candidates if p.exists()), qemu_default_candidates[0])
    qemu = pathlib.Path(os.environ.get("QEMU", str(qemu_default)))

    kernel = pathlib.Path(os.environ.get("KERNEL", str(o_dir / "vmlinux")))
    initrd = pathlib.Path(
        os.environ.get("INITRD", str(o_dir / "linx-initramfs" / "initramfs.cpio"))
    )

    mem = os.environ.get("MEM", "512M")
    smp = os.environ.get("SMP", "1")
    append = os.environ.get("APPEND", "lpj=1000000 loglevel=1 console=ttyS0")
    timeout_s = int(os.environ.get("TIMEOUT", "60"))

    script = os.environ.get(
        "SCRIPT",
        "help\n"
        "ls /\n"
        "ls /proc\n"
        "ls /sys\n"
        "getdents64_probe /proc\n"
        "getdents64_probe /sys\n"
        "probe /init\n"
        "cat /no-such\n"
        "sigill_test\n"
        "sigsegv_test\n"
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
    out_chunks: list[bytes] = []
    prompt_seen = False
    script_sent = False
    prompt = b"# "
    deadline = time.monotonic() + timeout_s

    while True:
        now = time.monotonic()
        if now >= deadline:
            timed_out = True
            proc.kill()
            break

        wait_s = min(0.25, max(0.0, deadline - now))
        r, _, _ = select.select([proc.stdout], [], [], wait_s)
        if r:
            chunk = os.read(proc.stdout.fileno(), 4096)
            if not chunk:
                break
            out_chunks.append(chunk)

            joined = b"".join(out_chunks[-8:])
            if not prompt_seen and (b"\n# " in joined or joined.endswith(prompt)):
                prompt_seen = True
                if proc.stdin and not proc.stdin.closed:
                    proc.stdin.write(script.encode("utf-8"))
                    proc.stdin.flush()
                    script_sent = True

        if proc.poll() is not None:
            break

    if proc.poll() is None:
        proc.kill()

    tail_out = proc.stdout.read() if proc.stdout else b""
    if tail_out:
        out_chunks.append(tail_out)

    out = b"".join(out_chunks)
    if not script_sent and proc.stdin and not proc.stdin.closed:
        try:
            proc.stdin.close()
        except Exception:
            pass

    text = out.decode("utf-8", errors="replace")

    # Minimal invariants: interactive initramfs + applets work.
    want = [
        "cmds:",
        "# ls /",
        "dev",
        "# cat /no-such",
        "E:",
        "dents_ok=0000000000000001",
        "sigill: ok",
        "sigsegv: ok",
    ]
    missing = [w for w in want if w not in text]
    if missing:
        sys.stderr.write("error: smoke check failed; missing: %s\n" % ", ".join(missing))
        sys.stderr.write("kernel: %s\n" % kernel)
        sys.stderr.write("initrd: %s\n" % initrd)
        sys.stderr.write("qemu: %s\n" % qemu)
        sys.stderr.write("cmd: %s\n" % " ".join(cmd))
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
            or s.startswith("dents_ok=")
            or s.startswith("sigill:")
            or s.startswith("sigsegv:")
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
