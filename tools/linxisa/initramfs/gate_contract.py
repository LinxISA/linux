"""Shared fail-closed process contract for Linx initramfs gates."""


def qemu_exit_error(*, timed_out: bool, returncode: int, timeout_s: int) -> str | None:
    """Return the terminal process error that must override guest markers."""
    if timed_out:
        return f"qemu timed out after TIMEOUT={timeout_s}s"
    if returncode != 0:
        return f"qemu exited with status {returncode}"
    return None
