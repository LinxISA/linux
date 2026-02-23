#!/usr/bin/env python3
import os
import pathlib
import re
import select
import subprocess
import sys
import time


def _irq0_count(text: str) -> int | None:
    # Typical /proc/interrupts row: "  0:       123   ...".
    m = re.search(r"(?m)^\s*0:\s+(\d+)\b", text)
    if not m:
        return None
    try:
        return int(m.group(1), 10)
    except ValueError:
        return None


def main() -> int:
    linux_root = pathlib.Path(__file__).resolve().parents[3]
    super_root = linux_root.parents[1]
    o_dir = pathlib.Path(os.environ.get("O", str(linux_root / "build-linx-fixed")))

    qemu_default_candidates = [
        super_root / "emulator" / "qemu" / "build" / "qemu-system-linx64",
    ]
    qemu_default = next((p for p in qemu_default_candidates if p.exists()), qemu_default_candidates[0])
    qemu = pathlib.Path(os.environ.get("QEMU", str(qemu_default)))

    kernel = pathlib.Path(os.environ.get("KERNEL", str(o_dir / "vmlinux")))
    rootfs = pathlib.Path(os.environ.get("ROOTFS_IMG", str(o_dir / "linx-busybox-rootfs" / "rootfs.ext4")))
    mem = os.environ.get("MEM", "512M")
    smp = os.environ.get("SMP", "1")
    timeout_s = int(os.environ.get("TIMEOUT", "120"))
    append = os.environ.get(
        "APPEND",
        "lpj=1000000 loglevel=1 console=ttyS0 root=/dev/vda rw init=/sbin/init "
        "virtio_mmio.device=0x200@0x30001000:1",
    )
    disable_timer_irq = os.environ.get("LINX_DISABLE_TIMER_IRQ", "").lower() in {"1", "true", "yes"}
    if disable_timer_irq and "linx_disable_timer_irq=" not in append:
        append = f"{append} linx_disable_timer_irq=1".strip()

    script = os.environ.get(
        "SCRIPT",
        "help\n"
        "ls /\n"
        "ls /sbin\n"
        "cat /proc/interrupts\n"
        "cat /proc/interrupts\n"
        "poweroff\n",
    )

    if os.environ.get("SKIP_BUILD", "") not in {"1", "true", "yes"}:
        build_sh = pathlib.Path(__file__).with_name("build_rootfs.sh")
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
        "-drive",
        f"if=none,id=vd0,file={rootfs},format=raw",
        "-device",
        "virtio-blk-device,drive=vd0",
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

    want = [
        "cmds:",
        "# ls /",
        "# ls /sbin",
        "init",
        "# cat /proc/interrupts",
    ]
    missing = [w for w in want if w not in text]
    if missing:
        sys.stderr.write("error: busybox rootfs boot failed; missing: %s\n" % ", ".join(missing))
        sys.stderr.write("kernel: %s\n" % kernel)
        sys.stderr.write("rootfs: %s\n" % rootfs)
        sys.stderr.write("qemu: %s\n" % qemu)
        sys.stderr.write("cmd: %s\n\n" % " ".join(cmd))
        sys.stderr.write("\n".join(text.splitlines()[-240:]))
        sys.stderr.write("\n")
        return 2

    irq_counts = [_irq0_count(block) for block in text.split("# cat /proc/interrupts")]
    irq_counts = [v for v in irq_counts if v is not None]
    if not disable_timer_irq and len(irq_counts) >= 2 and irq_counts[-1] < irq_counts[-2]:
        sys.stderr.write(
            "error: timer IRQ count regressed: %d -> %d\n" % (irq_counts[-2], irq_counts[-1])
        )
        return 2

    if timed_out:
        sys.stderr.write("note: qemu did not exit; killed after TIMEOUT=%ds\n" % timeout_s)
        sys.stderr.flush()

    keep = []
    for ln in text.splitlines():
        s = ln.strip()
        if (
            s.startswith("#")
            or s.startswith("cmds:")
            or s.startswith("reboot:")
            or s.startswith("Power down")
            or s in {"bin", "dev", "etc", "proc", "run", "sbin", "sys", "tmp", "init", "poweroff"}
        ):
            keep.append(ln)
    sys.stdout.write("\n".join(keep[-240:]) + "\n")
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
