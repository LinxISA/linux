#!/usr/bin/env python3
"""Contract tests for fail-closed initramfs gate process handling."""

import unittest

from gate_contract import qemu_exit_error


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
