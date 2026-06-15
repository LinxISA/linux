#!/usr/bin/env python3
import os
import pathlib
import subprocess
import sys
import time


def resolve_kernel_symbol(out_dir: pathlib.Path, name: str) -> str | None:
    system_map = out_dir / "System.map"
    if not system_map.exists():
        return None

    with system_map.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            fields = line.split()
            if len(fields) >= 3 and fields[2] == name:
                return "0x" + fields[0].lower()
    return None


def append_watchpoints(env: dict[str, str], watches: list[str]) -> None:
    current = [
        item.strip().lower()
        for item in env.get("LINX_DEBUG_PC_WATCH", "").split(",")
        if item.strip()
    ]
    for watch in watches:
        if watch.lower() not in current:
            current.append(watch.lower())
    env["LINX_DEBUG_PC_WATCH"] = ",".join(current)


def main() -> int:
    linux_root = pathlib.Path(__file__).resolve().parents[3]
    super_root = linux_root.parents[1]
    out_dir = pathlib.Path(os.environ.get("O", str(linux_root / "build-linx-fixed")))

    qemu_candidates = [
        pathlib.Path(os.environ.get("QEMU", "")) if os.environ.get("QEMU") else None,
        pathlib.Path("/tmp/linx-qemu-direct-build/qemu-system-linx64"),
        super_root / "emulator" / "qemu" / "build" / "qemu-system-linx64",
    ]
    qemu = next((p for p in qemu_candidates if p and p.exists()), None)
    if qemu is None:
        sys.stderr.write("error: qemu-system-linx64 not found; set QEMU\n")
        return 2

    kernel = out_dir / "vmlinux"
    initrd_out_dir = out_dir / "linx-initramfs-poweroff-proof"
    initrd = initrd_out_dir / "initramfs.cpio"
    build_sh = pathlib.Path(__file__).with_name("build.sh")

    build_env = os.environ.copy()
    build_env["INIT_VARIANT"] = "tiny"
    build_env["OUT_DIR"] = str(initrd_out_dir)
    subprocess.run([str(build_sh)], check=True, env=build_env)

    cmd = [
        str(qemu),
        "-nographic",
        "-monitor",
        "none",
        "-machine",
        "virt",
        "-m",
        os.environ.get("MEM", "512M"),
        "-smp",
        os.environ.get("SMP", "1"),
        "-kernel",
        str(kernel),
        "-initrd",
        str(initrd),
        "-append",
        os.environ.get(
            "APPEND",
            "lpj=1000000 loglevel=1 console=ttyS0 panic=-1 linx_disable_timer_irq=1",
        ),
        "-bios",
        "none",
    ]

    run_env = os.environ.copy()
    run_env.setdefault("LINX_DISABLE_TIMER_IRQ", "1")

    required = ["linx_pc_watch: pc=0x10030"]
    watchpoints = ["0x10030"]
    shutdown_pc = resolve_kernel_symbol(out_dir, "lisc_shutdown")
    if shutdown_pc is not None:
        watchpoints.append(shutdown_pc)
        required.append(f"linx_pc_watch: pc={shutdown_pc}")
    append_watchpoints(run_env, watchpoints)

    start = time.time()
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=run_env,
    )
    try:
        out, _ = proc.communicate(timeout=int(os.environ.get("TIMEOUT", "10")))
        timed_out = False
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
        timed_out = True

    text = out.decode("utf-8", errors="replace")
    elapsed = time.time() - start

    missing = [item for item in required if item not in text]

    if timed_out or proc.returncode != 0 or missing:
        sys.stderr.write(
            "error: native poweroff proof failed"
            f" rc={proc.returncode} timeout={timed_out} missing={missing}\n"
        )
        sys.stderr.write("cmd: %s\n" % " ".join(cmd))
        sys.stderr.write("elapsed: %.2fs\n" % elapsed)
        sys.stderr.write(text)
        return 2

    sys.stdout.write("ok: linux booted and powered off on qemu\n")
    sys.stdout.write("elapsed: %.2fs\n" % elapsed)
    for line in text.splitlines():
        if "linx_pc_watch:" in line:
            sys.stdout.write(line + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
