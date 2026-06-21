#!/usr/bin/env python3
import os
import pathlib
import select
import shlex
import subprocess
import sys
import tempfile
import time
from typing import Optional


def _retry_with_virtio_cmdline_fallback(append: str) -> Optional[int]:
    fallback_flag = os.environ.get("LINX_VIRTIO_MMIO_FALLBACK", "0").lower()
    if fallback_flag not in {"1", "true", "yes"}:
        return None
    if "virtio_mmio.device=" in append:
        return None

    env = dict(os.environ)
    env["APPEND"] = f"{append} virtio_mmio.device=0x200@0x30001000:1".strip()
    env["SKIP_BUILD"] = "1"
    env["LINX_VIRTIO_MMIO_FALLBACK"] = "0"
    rerun = subprocess.run(
        [sys.executable, str(pathlib.Path(__file__).resolve())],
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if rerun.returncode == 0:
        sys.stderr.write("note: virtio_disk_smoke.py used virtio-mmio cmdline fallback\n")
        if rerun.stdout:
            sys.stdout.write(rerun.stdout)
            sys.stdout.flush()
    else:
        sys.stderr.write("note: virtio-mmio cmdline fallback failed\n")
        if rerun.stderr:
            sys.stderr.write(rerun.stderr)
    return rerun.returncode


def _retry_once_same_config(append: str) -> Optional[int]:
    retry_flag = os.environ.get("LINX_VIRTIO_DISK_SMOKE_RETRY", "1").lower()
    if retry_flag in {"0", "false", "no"}:
        return None

    env = dict(os.environ)
    env["APPEND"] = append
    env["SKIP_BUILD"] = "1"
    env["LINX_VIRTIO_DISK_SMOKE_RETRY"] = "0"
    env["LINX_VIRTIO_MMIO_FALLBACK"] = "0"
    rerun = subprocess.run(
        [sys.executable, str(pathlib.Path(__file__).resolve())],
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if rerun.returncode == 0:
        sys.stderr.write("note: virtio_disk_smoke.py recovered after same-config retry\n")
        if rerun.stdout:
            sys.stdout.write(rerun.stdout)
            sys.stdout.flush()
    return rerun.returncode


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
    initrd = pathlib.Path(
        os.environ.get("INITRD", str(o_dir / "linx-initramfs" / "initramfs.cpio"))
    )
    disk_default = pathlib.Path(tempfile.gettempdir()) / f"linx-disk-smoke-{os.getpid()}.img"
    disk_img = pathlib.Path(os.environ.get("DISK_IMG", str(disk_default)))

    mem = os.environ.get("MEM", "512M")
    smp = os.environ.get("SMP", "1")
    append = os.environ.get(
        "APPEND", "lpj=1000000 loglevel=1 console=ttyS0 linx_storage_init=1"
    )
    disable_timer_irq = os.environ.get("LINX_DISABLE_TIMER_IRQ", "").lower() in {"1", "true", "yes"}
    if disable_timer_irq and "linx_disable_timer_irq=" not in append:
        append = f"{append} linx_disable_timer_irq=1".strip()
    timeout_s = int(os.environ.get("TIMEOUT", "90"))
    prompt_settle_s = float(os.environ.get("PROMPT_SETTLE", "2.0"))
    disk_mb = int(os.environ.get("DISK_MB", "64"))
    debug_log_lines = int(os.environ.get("DEBUG_LOG_LINES", "240"))
    require_dev_node = os.environ.get("LINX_VIRTIO_DISK_REQUIRE_DEV", "0").lower() in {
        "1",
        "true",
        "yes",
    }
    qemu_extra_args = shlex.split(os.environ.get("QEMU_EXTRA_ARGS", ""))
    if "-bios" not in qemu_extra_args and not any(arg.startswith("-bios=") for arg in qemu_extra_args):
        qemu_extra_args.extend(["-bios", "none"])
    virtio_device = os.environ.get("VIRTIO_BLK_DEVICE", "virtio-blk-device,drive=vd0")

    script = os.environ.get(
        "SCRIPT",
        "help\n"
        "ls /dev\n"
        "ls /sys\n"
        "ls /sys/block\n"
        "probe /sys/block/vda/dev\n"
        "poweroff\n",
    )

    if os.environ.get("SKIP_BUILD", "") not in {"1", "true", "yes"}:
        build_sh = pathlib.Path(__file__).with_name("build.sh")
        subprocess.run([str(build_sh)], check=True)

    disk_img.parent.mkdir(parents=True, exist_ok=True)
    with open(disk_img, "wb") as f:
        f.truncate(disk_mb * 1024 * 1024)

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
        "-drive",
        f"if=none,id=vd0,file={disk_img},format=raw",
        "-device",
        virtio_device,
        "-append",
        append,
    ]
    cmd.extend(qemu_extra_args)

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
                if prompt_settle_s > 0:
                    time.sleep(prompt_settle_s)
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
    lines = text.splitlines()

    def emit_debug_log() -> None:
        if debug_log_lines <= 0:
            body = text
        else:
            body = "\n".join(lines[-debug_log_lines:])
        sys.stderr.write(body)
        if not body.endswith("\n"):
            sys.stderr.write("\n")

    want = [
        "# ls /dev",
        "# ls /sys/block",
        "# probe /sys/block/vda/dev",
    ]
    missing = [w for w in want if w not in text]
    if missing:
        retry_rc = _retry_once_same_config(append)
        if retry_rc == 0:
            return 0
        retry_rc = _retry_with_virtio_cmdline_fallback(append)
        if retry_rc == 0:
            return 0
        sys.stderr.write("error: virtio disk smoke failed; missing: %s\n" % ", ".join(missing))
        sys.stderr.write("kernel: %s\n" % kernel)
        sys.stderr.write("initrd: %s\n" % initrd)
        sys.stderr.write("disk_img: %s\n" % disk_img)
        sys.stderr.write("qemu: %s\n" % qemu)
        sys.stderr.write("cmd: %s\n" % " ".join(cmd))
        sys.stderr.write("\n")
        emit_debug_log()
        sys.stderr.flush()
        return 2

    if not any(line.strip() == "vda" for line in lines):
        retry_rc = _retry_once_same_config(append)
        if retry_rc == 0:
            return 0
        retry_rc = _retry_with_virtio_cmdline_fallback(append)
        if retry_rc == 0:
            return 0
        sys.stderr.write("error: virtio disk smoke failed; '/sys/block' did not enumerate vda\n")
        emit_debug_log()
        sys.stderr.flush()
        return 2

    def section_lines_for(cmd: str) -> list[str]:
        probe_idx = -1
        for i, line in enumerate(lines):
            if line.strip() == cmd:
                probe_idx = i
                break
        probe_lines: list[str] = []
        if probe_idx >= 0:
            for line in lines[probe_idx + 1 :]:
                if line.startswith("# "):
                    break
                if line.strip():
                    probe_lines.append(line.strip())
        return probe_lines

    dev_lines = section_lines_for("# ls /dev")
    if require_dev_node and "vda" not in dev_lines:
        retry_rc = _retry_once_same_config(append)
        if retry_rc == 0:
            return 0
        retry_rc = _retry_with_virtio_cmdline_fallback(append)
        if retry_rc == 0:
            return 0
        sys.stderr.write("error: virtio disk smoke failed; '/dev' did not enumerate vda\n")
        sys.stderr.write("\n".join(text.splitlines()[-240:]))
        sys.stderr.write("\n")
        sys.stderr.flush()
        return 2

    probe_lines = section_lines_for("# probe /sys/block/vda/dev")
    probe_blob = "\n".join(probe_lines)
    if (
        not probe_lines
        or "E:" in probe_blob
        or "fffffffffffffff7" in probe_blob
        or "ffffffffffffffff" in probe_blob
    ):
        retry_rc = _retry_once_same_config(append)
        if retry_rc == 0:
            return 0
        retry_rc = _retry_with_virtio_cmdline_fallback(append)
        if retry_rc == 0:
            return 0
        sys.stderr.write("error: virtio disk smoke failed; '/sys/block/vda/dev' probe did not return valid data\n")
        emit_debug_log()
        sys.stderr.flush()
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
            or s.startswith("n1=")
            or s.startswith("E:")
            or s.startswith("reboot:")
        ):
            keep.append(ln)
        elif s in {"sys", "block", "vda", "devices", "kernel", "dev"}:
            keep.append(ln)
    sys.stdout.write("\n".join(keep[-240:]) + "\n")
    sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
