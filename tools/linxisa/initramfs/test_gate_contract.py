#!/usr/bin/env python3
"""Contract tests for fail-closed initramfs gate process handling."""

import io
import unittest

from gate_contract import PromptScriptDriver, qemu_exit_error


class PromptScriptDriverTest(unittest.TestCase):
    def test_split_prompt_and_echo_drive_commands_in_order(self) -> None:
        stdin = io.BytesIO()
        driver = PromptScriptDriver("help\npoweroff\n", retry_s=1.0)

        driver.feed(b"boot\n#", 0.0, stdin)
        self.assertEqual(stdin.getvalue(), b"")
        driver.feed(b" ", 1.0, stdin)
        self.assertEqual(stdin.getvalue(), b"h")

        for now, value in enumerate(b"help\n", start=2):
            driver.feed(b"\r\n" if value == ord("\n") else bytes((value,)), float(now), stdin)
        self.assertFalse(driver.complete)
        driver.feed(b"cmds: ...\r\n# ", 7.0, stdin)
        for now, value in enumerate(b"poweroff\n", start=8):
            driver.feed(b"\r\n" if value == ord("\n") else bytes((value,)), float(now), stdin)

        self.assertTrue(driver.complete)
        self.assertEqual(stdin.getvalue(), b"help\npoweroff\n")

    def test_early_exit_leaves_driver_incomplete_without_extra_writes(self) -> None:
        stdin = io.BytesIO()
        driver = PromptScriptDriver("help\npoweroff\n", retry_s=2.0)

        driver.feed(b"\n# ", 0.0, stdin)
        driver.tick(1.0, stdin)

        self.assertFalse(driver.complete)
        self.assertEqual(stdin.getvalue(), b"h")
        driver.tick(2.0, stdin)
        self.assertEqual(stdin.getvalue(), b"hh")

    def test_coalesced_newline_echo_and_prompt_starts_next_command(self) -> None:
        stdin = io.BytesIO()
        driver = PromptScriptDriver("a\nb\n")

        driver.feed(b"\n# ", 0.0, stdin)
        driver.feed(b"a", 0.1, stdin)
        driver.feed(b"\r\n# ", 0.2, stdin)

        self.assertEqual(stdin.getvalue(), b"a\nb")
        self.assertFalse(driver.complete)


class QemuExitContractTest(unittest.TestCase):
    def test_timeout_overrides_successful_process_status(self) -> None:
        self.assertEqual(
            qemu_exit_error(timed_out=True, returncode=0, timeout_s=60),
            "qemu timed out after TIMEOUT=60s",
        )

    def test_nonzero_exit_is_failure(self) -> None:
        self.assertEqual(
            qemu_exit_error(timed_out=False, returncode=3, timeout_s=60),
            "qemu exited with status 3",
        )

    def test_clean_exit_has_no_error(self) -> None:
        self.assertIsNone(
            qemu_exit_error(timed_out=False, returncode=0, timeout_s=60)
        )


if __name__ == "__main__":
    unittest.main()
