"""Shared fail-closed process contract for Linx initramfs gates."""

from typing import BinaryIO


class PromptScriptDriver:
    """Deliver echoed bytes for one command at each guest prompt."""

    def __init__(
        self, script: str, prompt: bytes = b"# ", retry_s: float = 2.0
    ) -> None:
        self._commands = [
            line.encode("utf-8") + b"\n"
            for line in script.splitlines()
            if line.strip()
        ]
        self._prompt = prompt
        self._prompt_tail = b""
        self._command_index = 0
        self._byte_index = 0
        self._pending_echo: int | None = None
        self._echo_buffer = b""
        self._waiting_for_prompt = True
        self._retry_at = 0.0
        self._retry_s = retry_s
        self.complete = not self._commands

    def _write_pending_byte(self, stdin: BinaryIO, now: float) -> None:
        command = self._commands[self._command_index]
        value = command[self._byte_index]
        stdin.write(bytes((value,)))
        stdin.flush()
        self._pending_echo = value
        self._echo_buffer = b""
        self._retry_at = now + self._retry_s

    def feed(self, chunk: bytes, now: float, stdin: BinaryIO) -> None:
        """Consume split prompts and echoed input without blocking the deadline loop."""
        if self.complete or not chunk:
            return

        if self._waiting_for_prompt:
            data = self._prompt_tail + chunk
            prompt_at = data.find(self._prompt)
            if prompt_at < 0:
                keep = max(0, len(self._prompt) - 1)
                self._prompt_tail = data[-keep:] if keep else b""
                return

            self._prompt_tail = b""
            self._waiting_for_prompt = False
            self._write_pending_byte(stdin, now)
            return

        if self._pending_echo is None:
            return

        self._echo_buffer += chunk
        expected = self._pending_echo
        if expected == ord("\n"):
            echoed = b"\n" in self._echo_buffer
        else:
            tail = self._echo_buffer.rsplit(b"\n", 1)[-1].lstrip(b"\r")
            echoed = tail.startswith(bytes((expected,)))
        if not echoed:
            return

        self._pending_echo = None
        self._echo_buffer = b""
        command = self._commands[self._command_index]
        self._byte_index += 1
        if self._byte_index < len(command):
            self._write_pending_byte(stdin, now)
            return

        self._command_index += 1
        self._byte_index = 0
        if self._command_index == len(self._commands):
            self.complete = True
            return

        self._waiting_for_prompt = True

    def tick(self, now: float, stdin: BinaryIO) -> None:
        """Retry a dropped byte while leaving timeout enforcement to the caller."""
        if self._pending_echo is not None and now >= self._retry_at:
            stdin.write(bytes((self._pending_echo,)))
            stdin.flush()
            self._echo_buffer = b""
            self._retry_at = now + self._retry_s


def qemu_exit_error(*, timed_out: bool, returncode: int, timeout_s: int) -> str | None:
    """Return the terminal process error that must override guest markers."""
    if timed_out:
        return f"qemu timed out after TIMEOUT={timeout_s}s"
    if returncode != 0:
        return f"qemu exited with status {returncode}"
    return None
