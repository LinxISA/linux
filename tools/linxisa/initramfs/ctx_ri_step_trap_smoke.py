#!/usr/bin/env python3
import os
import pathlib
import re
import select
import subprocess
import sys
import time


def is_true(v: str) -> bool:
    return v.lower() in {"1", "true", "yes", "y"}


def main() -> int:
    linux_root = pathlib.Path(__file__).resolve().parents[3]
    super_root = linux_root.parents[1]
    o_dir = pathlib.Path(os.environ.get("O", str(linux_root / "build-linx-fixed")))

    qemu_default_candidates = [
        super_root / "emulator" / "qemu" / "build-linx" / "qemu-system-linx64",
        super_root / "emulator" / "qemu" / "build" / "qemu-system-linx64",
    ]
    qemu_default = next((p for p in qemu_default_candidates if p.exists()), qemu_default_candidates[0])
    qemu = pathlib.Path(os.environ.get("QEMU", str(qemu_default)))

    kernel = pathlib.Path(os.environ.get("KERNEL", str(o_dir / "vmlinux")))
    initrd = pathlib.Path(
        os.environ.get("INITRD", str(o_dir / "linx-initramfs" / "initramfs.cpio"))
    )

    mem = os.environ.get("MEM", "512M")
    smp = os.environ.get("SMP", "1")
    append = os.environ.get("APPEND", "lpj=1000000 loglevel=1 console=ttyS0 panic=-1")
    timeout_s = int(os.environ.get("TIMEOUT", "120"))

    if not is_true(os.environ.get("LINX_DISABLE_TIMER_IRQ", "")):
        sys.stderr.write(
            "error: ctx_ri_step_trap_smoke requires LINX_DISABLE_TIMER_IRQ=1 "
            "(step-trap lane is validated in timer-off mode)\n"
        )
        return 2
    if "linx_disable_timer_irq=0" in append:
        sys.stderr.write(
            "error: ctx_ri_step_trap_smoke requires timer IRQ disabled; APPEND "
            "contains linx_disable_timer_irq=0\n"
        )
        return 2
    if "linx_ctx_tu_step_test=" not in append:
        append = f"{append} linx_ctx_tu_step_test=1".strip()

    script = os.environ.get(
        "SCRIPT",
        "help\n"
        "ctx_ri_step_trap_test\n"
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
    out_chunks: list[bytes] = []
    timed_out = False
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
        "# ctx_ri_step_trap_test",
    ]
    missing = [w for w in want if w not in text]
    if missing:
        sys.stderr.write("error: ctx_ri_step_trap_smoke failed; missing: %s\n" % ", ".join(missing))
        sys.stderr.write("cmd: %s\n" % " ".join(cmd))
        sys.stderr.write("\n".join(text.splitlines()[-240:]))
        sys.stderr.write("\n")
        return 2

    if "linx_ctx_tu_step_test: BI requirement failed" in text:
        sys.stderr.write("error: ctx_ri_step_trap_smoke failed; kernel reported BI requirement failure\n")
        sys.stderr.write("cmd: %s\n" % " ".join(cmd))
        sys.stderr.write("\n".join(text.splitlines()[-240:]))
        sys.stderr.write("\n")
        return 2

    m = re.search(r"ctx_ri_step_trap_test:\s+ok\b", text)
    if not m:
        sys.stderr.write("error: ctx_ri_step_trap_smoke failed; no passing marker\n")
        sys.stderr.write("cmd: %s\n" % " ".join(cmd))
        sys.stderr.write("\n".join(text.splitlines()[-240:]))
        sys.stderr.write("\n")
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
            or s.startswith("ctx_ri_step_trap_test:")
            or "linx_ctx_tu_step_test:" in s
            or "Kernel panic" in s
            or "panic:" in s
        ):
            keep.append(ln)
    sys.stdout.write("\n".join(keep[-240:]) + "\n")
    sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
