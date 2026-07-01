#!/usr/bin/env python3
import json
import os
import pathlib
import pty
import re
import select
import shlex
import subprocess
import sys
import time
from typing import Any, Optional


ROOTFS_CONFIG_REQUIREMENTS: tuple[str, ...] = (
    "CONFIG_BLOCK",
    "CONFIG_DEVTMPFS",
    "CONFIG_DEVTMPFS_MOUNT",
    "CONFIG_EXT2_FS",
    "CONFIG_PROC_FS",
    "CONFIG_SYSFS",
    "CONFIG_VIRTIO",
    "CONFIG_VIRTIO_MMIO",
    "CONFIG_VIRTIO_BLK",
)


def _parse_kernel_config(path: pathlib.Path) -> dict[str, str]:
    options: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line:
            continue
        disabled = re.match(r"# (CONFIG_[A-Za-z0-9_]+) is not set$", line)
        if disabled:
            options[disabled.group(1)] = "n"
            continue
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            options[key] = value
    return options


def _kernel_config_path(kernel: pathlib.Path, o_dir: pathlib.Path) -> pathlib.Path:
    explicit = os.environ.get("KERNEL_CONFIG", "")
    if explicit:
        return pathlib.Path(explicit)
    kernel_parent = kernel.parent
    if (kernel_parent / ".config").is_file():
        return kernel_parent / ".config"
    return o_dir / ".config"


def _check_kernel_config(kernel: pathlib.Path, o_dir: pathlib.Path) -> bool:
    flag = os.environ.get("LINX_BUSYBOX_ROOTFS_CONFIG_PREFLIGHT", "1").lower()
    if flag in {"0", "false", "no"}:
        return True

    config_path = _kernel_config_path(kernel, o_dir)
    if not config_path.is_file():
        sys.stderr.write(
            "error: kernel config preflight failed; .config not found for rootfs boot\n"
        )
        sys.stderr.write("kernel: %s\n" % kernel)
        sys.stderr.write("expected config: %s\n" % config_path)
        sys.stderr.write(
            "set KERNEL_CONFIG=/path/to/.config or LINX_BUSYBOX_ROOTFS_CONFIG_PREFLIGHT=0 "
            "for externally managed kernels\n"
        )
        return False

    options = _parse_kernel_config(config_path)
    missing = [key for key in ROOTFS_CONFIG_REQUIREMENTS if options.get(key) != "y"]
    if not missing:
        return True

    sys.stderr.write(
        "error: kernel config preflight failed; rootfs boot requires: %s\n"
        % ", ".join(missing)
    )
    sys.stderr.write("kernel: %s\n" % kernel)
    sys.stderr.write("config: %s\n" % config_path)
    return False


def _timer_irq_count(text: str) -> Optional[int]:
    # Linx virt exposes the timer on a platform IRQ row, not necessarily IRQ 0.
    m = re.search(r"(?m)^\s*\d+:\s+(\d+)\b.*\blinx-timer\b", text)
    if m is None:
        # Historical logs used IRQ 0 for timer smoke; keep it as a fallback.
        m = re.search(r"(?m)^\s*0:\s+(\d+)\b", text)
    if not m:
        return None
    try:
        return int(m.group(1), 10)
    except ValueError:
        return None


def _attempt_appends(base_append: str) -> list[tuple[str, Optional[str]]]:
    attempts: list[tuple[str, Optional[str]]] = [(base_append, None)]

    retry_flag = os.environ.get("LINX_BUSYBOX_BOOT_RETRY", "1").lower()
    if retry_flag not in {"0", "false", "no"}:
        attempts.append((base_append, "same-config retry"))

    fallback_flag = os.environ.get("LINX_VIRTIO_MMIO_FALLBACK", "0").lower()
    if fallback_flag in {"1", "true", "yes"} and "virtio_mmio.device=" not in base_append:
        attempts.append((f"{base_append} virtio_mmio.device=0x200@0x30001000:1".strip(), "virtio-mmio cmdline fallback"))

    return attempts


def _drain_stdout(proc: subprocess.Popen, out_chunks: list[bytes]) -> None:
    if proc.stdout is None:
        return
    while True:
        try:
            chunk = os.read(proc.stdout.fileno(), 4096)
        except OSError:
            return
        if not chunk:
            return
        out_chunks.append(chunk)


def _run_once(cmd: list[str], script: str, timeout_s: int) -> tuple[str, bool]:
    master_fd, slave_fd = pty.openpty()
    proc = subprocess.Popen(
        cmd,
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
    )
    os.close(slave_fd)
    timed_out = False
    out_chunks: list[bytes] = []
    prompt_seen = False
    script_sent = False
    boot_ready_seen = False
    prompt = b"# "
    deadline = time.monotonic() + timeout_s
    started_at = time.monotonic()
    last_output_at = time.monotonic()
    blind_send_after_s = float(os.environ.get("LINX_BUSYBOX_BOOT_BLIND_SEND_AFTER", "2.0"))

    def try_send_script() -> None:
        nonlocal script_sent
        if script_sent:
            return
        os.write(master_fd, script.encode("utf-8"))
        script_sent = True

    while True:
        now = time.monotonic()
        if now >= deadline:
            timed_out = True
            proc.kill()
            break

        wait_s = min(0.25, max(0.0, deadline - now))
        r, _, _ = select.select([master_fd], [], [], wait_s)
        if r:
            try:
                chunk = os.read(master_fd, 4096)
            except OSError:
                chunk = b""
            if not chunk:
                break
            out_chunks.append(chunk)
            last_output_at = time.monotonic()
            joined = b"".join(out_chunks[-8:])
            if (
                b"Run /sbin/init as init process" in joined
                or b"Run /init as init process" in joined
            ):
                boot_ready_seen = True
            if not prompt_seen and (
                b"\n# " in joined
                or b"\r# " in joined
                or joined.endswith(prompt)
            ):
                prompt_seen = True
                try_send_script()

        if (
            boot_ready_seen
            and not script_sent
            and (time.monotonic() - last_output_at) >= 0.5
        ):
            try_send_script()

        if (
            not script_sent
            and blind_send_after_s > 0.0
            and (time.monotonic() - started_at) >= blind_send_after_s
        ):
            try_send_script()

        if proc.poll() is not None:
            break

    if proc.poll() is None:
        proc.kill()

    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=5)
    while True:
        try:
            chunk = os.read(master_fd, 4096)
        except OSError:
            break
        if not chunk:
            break
        out_chunks.append(chunk)
    try:
        os.close(master_fd)
    except OSError:
        pass

    return b"".join(out_chunks).decode("utf-8", errors="replace"), timed_out


def _artifact_path(env_name: str, fallback_env_name: str = "") -> Optional[pathlib.Path]:
    value = os.environ.get(env_name, "")
    if not value and fallback_env_name:
        value = os.environ.get(fallback_env_name, "")
    if not value:
        return None
    return pathlib.Path(value)


def _rootfs_path(default: pathlib.Path) -> pathlib.Path:
    rootfs_img = os.environ.get("ROOTFS_IMG", "")
    legacy_rootfs = os.environ.get("ROOTFS", "")

    if rootfs_img:
        if legacy_rootfs and legacy_rootfs != rootfs_img:
            sys.stderr.write(
                "note: ROOTFS is ignored because ROOTFS_IMG is set; "
                "ROOTFS_IMG is the canonical rootfs selector\n"
            )
        return pathlib.Path(rootfs_img)

    if legacy_rootfs:
        sys.stderr.write(
            "note: ROOTFS is accepted as a compatibility alias; prefer ROOTFS_IMG\n"
        )
        return pathlib.Path(legacy_rootfs)

    return default


def _write_text(path: Optional[pathlib.Path], text: str) -> None:
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", errors="replace")


def _write_json(path: Optional[pathlib.Path], payload: dict[str, Any]) -> None:
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _default_transcript_path(report_path: Optional[pathlib.Path]) -> Optional[pathlib.Path]:
    if report_path is None:
        return None
    return report_path.with_suffix(".transcript.txt")


def _format_transcript(attempts: list[dict[str, Any]]) -> str:
    parts: list[str] = []
    for attempt in attempts:
        note = attempt.get("recovery_note") or "primary"
        parts.append(f"===== attempt {attempt['index']}: {note} =====")
        parts.append("cmd: %s" % " ".join(attempt["command"]))
        parts.append("append: %s" % attempt["append"])
        parts.append("timed_out: %s" % attempt["timed_out"])
        parts.append("missing: %s" % ", ".join(attempt["missing"]))
        if attempt.get("irq_error"):
            parts.append("irq_error: %s" % attempt["irq_error"])
        parts.append("")
        parts.append(attempt.get("output", ""))
        parts.append("")
    return "\n".join(parts)


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
    rootfs = _rootfs_path(o_dir / "linx-busybox-rootfs" / "rootfs.ext2")
    mem = os.environ.get("MEM", "512M")
    smp = os.environ.get("SMP", "1")
    timeout_s = int(os.environ.get("TIMEOUT", "120"))
    append = os.environ.get(
        "APPEND",
        "lpj=1000000 loglevel=1 console=ttyS0 root=/dev/vda rw rootfstype=ext2 init=/sbin/init",
    )
    disable_timer_irq = os.environ.get("LINX_DISABLE_TIMER_IRQ", "").lower() in {"1", "true", "yes"}
    if disable_timer_irq and "linx_disable_timer_irq=" not in append:
        append = f"{append} linx_disable_timer_irq=1".strip()
    qemu_extra_args = shlex.split(os.environ.get("QEMU_EXTRA_ARGS", ""))
    if "-bios" not in qemu_extra_args and not any(arg.startswith("-bios=") for arg in qemu_extra_args):
        qemu_extra_args.extend(["-bios", "none"])
    virtio_device = os.environ.get("VIRTIO_BLK_DEVICE", "virtio-blk-device,drive=vd0")

    script = os.environ.get(
        "SCRIPT",
        "help\n"
        "ls /\n"
        "ls /sbin\n"
        "cat /proc/interrupts\n"
        "cat /proc/interrupts\n"
        "poweroff\n",
    )
    report_path = _artifact_path("LINX_BUSYBOX_BOOT_REPORT", "LINX_FLOW_COMMAND_REPORT")
    transcript_path = _artifact_path(
        "LINX_BUSYBOX_BOOT_TRANSCRIPT", "LINX_FLOW_COMMAND_TRANSCRIPT"
    )
    if transcript_path is None:
        transcript_path = _default_transcript_path(report_path)

    if os.environ.get("SKIP_BUILD", "") not in {"1", "true", "yes"}:
        build_sh = pathlib.Path(__file__).with_name("build_rootfs.sh")
        subprocess.run([str(build_sh)], check=True)

    if not _check_kernel_config(kernel, o_dir):
        return 2

    want = [
        "cmds:",
        "# ls /",
        "# ls /sbin",
        "init",
        "# cat /proc/interrupts",
    ]
    last_text = ""
    last_cmd: list[str] = []
    last_missing: list[str] = []
    last_timed_out = False
    last_irq_error: Optional[str] = None
    attempts_report: list[dict[str, Any]] = []

    for attempt_index, (attempt_append, recovery_note) in enumerate(
        _attempt_appends(append), start=1
    ):
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
            virtio_device,
            "-append",
            attempt_append,
        ]
        cmd.extend(qemu_extra_args)

        text, timed_out = _run_once(cmd, script, timeout_s)
        missing = [w for w in want if w not in text]
        irq_counts = [_timer_irq_count(block) for block in text.split("# cat /proc/interrupts")]
        irq_counts = [v for v in irq_counts if v is not None]
        irq_error = None
        if not disable_timer_irq and len(irq_counts) >= 2 and irq_counts[-1] < irq_counts[-2]:
            irq_error = "error: timer IRQ count regressed: %d -> %d\n" % (irq_counts[-2], irq_counts[-1])

        last_text = text
        last_cmd = cmd
        last_missing = missing
        last_timed_out = timed_out
        last_irq_error = irq_error
        attempts_report.append(
            {
                "index": attempt_index,
                "recovery_note": recovery_note,
                "append": attempt_append,
                "command": cmd,
                "timed_out": timed_out,
                "missing": missing,
                "irq_counts": irq_counts,
                "irq_error": irq_error,
                "output_lines": len(text.splitlines()),
                "output_tail": text.splitlines()[-80:],
                "output": text,
            }
        )

        if missing or irq_error:
            continue

        _write_text(transcript_path, _format_transcript(attempts_report))
        _write_json(
            report_path,
            {
                "schema_version": 1,
                "ok": True,
                "status": "pass",
                "kernel": str(kernel),
                "rootfs": str(rootfs),
                "qemu": str(qemu),
                "timeout_seconds": timeout_s,
                "disable_timer_irq": disable_timer_irq,
                "attempts": [
                    {k: v for k, v in attempt.items() if k != "output"}
                    for attempt in attempts_report
                ],
                "transcript": str(transcript_path) if transcript_path is not None else None,
            },
        )
        if recovery_note:
            sys.stderr.write(f"note: boot.py used {recovery_note}\n")
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

    if last_irq_error:
        sys.stderr.write(last_irq_error)
    else:
        sys.stderr.write("error: busybox rootfs boot failed; missing: %s\n" % ", ".join(last_missing))
    sys.stderr.write("kernel: %s\n" % kernel)
    sys.stderr.write("rootfs: %s\n" % rootfs)
    sys.stderr.write("qemu: %s\n" % qemu)
    sys.stderr.write("cmd: %s\n" % " ".join(last_cmd))
    if last_timed_out:
        sys.stderr.write("note: qemu did not exit; killed after TIMEOUT=%ds\n" % timeout_s)
    _write_text(transcript_path, _format_transcript(attempts_report))
    _write_json(
        report_path,
        {
            "schema_version": 1,
            "ok": False,
            "status": "fail",
            "kernel": str(kernel),
            "rootfs": str(rootfs),
            "qemu": str(qemu),
            "timeout_seconds": timeout_s,
            "disable_timer_irq": disable_timer_irq,
            "missing": last_missing,
            "timed_out": last_timed_out,
            "irq_error": last_irq_error,
            "attempts": [
                {k: v for k, v in attempt.items() if k != "output"}
                for attempt in attempts_report
            ],
            "transcript": str(transcript_path) if transcript_path is not None else None,
        },
    )
    sys.stderr.write("\n")
    sys.stderr.write("\n".join(last_text.splitlines()[-240:]))
    sys.stderr.write("\n")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
