#!/usr/bin/env python3
"""Fail-closed contract tests for the BusyBox rootfs boot gate."""

from __future__ import annotations

import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import boot


def passing_output(first: int = 10, second: int = 11) -> str:
    return (
        "# help\n"
        "cmds: echo cat ls help poweroff\n"
        "# ls /\ninit\n"
        "# ls /sbin\ninit\npoweroff\n"
        "# cat /proc/interrupts\n"
        f"  4: {first} Linx CPU INTC 4 Edge linx-timer\n"
        "# cat /proc/interrupts\n"
        f"  4: {second} Linx CPU INTC 4 Edge linx-timer\n"
        "# poweroff\n"
        "LINX_REBOOT lisc_shutdown\n"
    )


class BusyboxBootContractTest(unittest.TestCase):
    def result(
        self, *, output: str | None = None, timed_out: bool = False, returncode: int = 0
    ) -> boot.AttemptResult:
        return boot.AttemptResult(
            output=passing_output() if output is None else output,
            timed_out=timed_out,
            returncode=returncode,
            duration_seconds=1.0,
            send_reason="prompt",
        )

    def test_complete_markers_cannot_override_timeout(self) -> None:
        status, _, _, _ = boot._classify_attempt(
            self.result(timed_out=True), disable_timer_irq=False
        )
        self.assertEqual(status, "timeout")

    def test_nonzero_qemu_exit_cannot_pass(self) -> None:
        status, _, _, _ = boot._classify_attempt(
            self.result(returncode=3), disable_timer_irq=False
        )
        self.assertEqual(status, "qemu-error")

    def test_timer_missing_or_equal_delta_fails(self) -> None:
        status, _, _, error = boot._classify_attempt(
            self.result(output=passing_output(10, 10)), disable_timer_irq=False
        )
        self.assertEqual(status, "fail")
        self.assertIn("did not advance", error or "")

        status, _, _, error = boot._classify_attempt(
            self.result(output=passing_output().replace("linx-timer", "timer", 2)),
            disable_timer_irq=False,
        )
        self.assertEqual(status, "fail")
        self.assertIn("two samples", error or "")

    def test_poweroff_marker_is_required(self) -> None:
        output = passing_output().replace("LINX_REBOOT lisc_shutdown\n", "")
        status, missing, _, _ = boot._classify_attempt(
            self.result(output=output), disable_timer_irq=False
        )
        self.assertEqual(status, "fail")
        self.assertIn("LINX_REBOOT lisc_shutdown", missing)

    def test_retry_is_opt_in(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(boot._attempt_appends("console=ttyS0"), [("console=ttyS0", None)])

    def _fixture(self, base: Path) -> dict[str, str]:
        kernel = base / "vmlinux"
        config = base / ".config"
        rootfs = base / "rootfs.ext2"
        qemu = base / "qemu-system-linx64"
        kernel.write_bytes(b"kernel")
        rootfs.write_bytes(b"pristine-rootfs")
        qemu.write_bytes(b"qemu")
        config.write_text(
            "".join(f"{name}=y\n" for name in boot.ROOTFS_CONFIG_REQUIREMENTS),
            encoding="utf-8",
        )
        return {
            "O": str(base),
            "KERNEL": str(kernel),
            "KERNEL_CONFIG": str(config),
            "ROOTFS_IMG": str(rootfs),
            "QEMU": str(qemu),
            "SKIP_BUILD": "1",
            "LINX_BUSYBOX_BOOT_REPORT": str(base / "report.json"),
            "LINX_BUSYBOX_BOOT_TRANSCRIPT": str(base / "transcript.txt"),
            "LINX_BUSYBOX_BOOT_BLIND_SEND_AFTER": "0",
        }

    def test_retry_uses_snapshot_and_preserves_backing_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            env = self._fixture(base)
            env["LINX_BUSYBOX_BOOT_RETRY"] = "1"
            before = boot._sha256_file(Path(env["ROOTFS_IMG"]))
            failed = self.result(output="not ready\n")
            passed = self.result()
            with mock.patch.dict(os.environ, env, clear=True), mock.patch.object(
                boot, "_run_once", side_effect=[failed, passed]
            ) as run_once:
                self.assertEqual(boot.main(), 0)

            self.assertEqual(run_once.call_count, 2)
            for call in run_once.call_args_list:
                command = call.args[0]
                drive = command[command.index("-drive") + 1]
                self.assertIn("snapshot=on", drive)
            self.assertEqual(boot._sha256_file(Path(env["ROOTFS_IMG"])), before)

    def test_timeout_main_writes_schema_v2_and_filters_environment(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            env = self._fixture(base)
            env["TOP_SECRET_TOKEN"] = "must-not-leak"
            env["LINX_DEBUG_PC_WATCH"] = "0x1000"
            with mock.patch.dict(os.environ, env, clear=True), mock.patch.object(
                boot, "_run_once", return_value=self.result(timed_out=True)
            ):
                self.assertEqual(boot.main(), 2)

            report = json.loads((base / "report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["schema_version"], 2)
            self.assertEqual(report["status"], "timeout")
            self.assertFalse(report["ok"])
            self.assertEqual(report["rootfs_pre_sha256"], report["rootfs_post_sha256"])
            self.assertEqual(
                report["effective_config"]["qemu_debug_env"],
                {"LINX_DEBUG_PC_WATCH": "0x1000"},
            )
            self.assertNotIn("must-not-leak", json.dumps(report))
            self.assertEqual(report["effective_config"]["blind_send_after_seconds"], 0.0)


if __name__ == "__main__":
    unittest.main()
