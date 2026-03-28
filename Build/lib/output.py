"""Console output management with support for status lines and verbosity levels."""

import io
import os
import sys


class Output:
    """Manages console output with support for status lines and verbosity levels."""

    def __init__(self) -> None:
        self.verbose_enabled: bool = False
        assert isinstance(sys.stdout, io.TextIOWrapper)
        assert isinstance(sys.stderr, io.TextIOWrapper)
        self.stdout: io.TextIOWrapper = sys.stdout
        self.stderr: io.TextIOWrapper = sys.stderr
        self.interactive: bool = self.stdout.isatty()
        self._status_lines: list[str] = []

    def _get_terminal_width(self) -> int:
        try:
            return os.get_terminal_size(self.stdout.fileno()).columns
        except OSError:
            return 80

    def _write_atomic(self, data: str, file: io.TextIOWrapper) -> None:
        """Write to a stream and flush, bypassing line buffering for atomicity."""
        # Write to the underlying binary buffer to avoid line-buffered flushes
        # on each newline. This ensures the terminal only sees the final
        # state, not intermediate frames.
        file.flush()
        file.buffer.write(data.encode(file.encoding or "utf-8", errors="replace"))
        file.buffer.flush()

    def _build_clear(self) -> str:
        """Build escape sequence to erase the current status region."""
        n = len(self._status_lines)
        if n == 0:
            return ""
        return f"\033[{n}A\033[J"

    def _build_status(self, lines: list[str]) -> str:
        """Build the string for rendering status lines."""
        if not lines:
            return ""
        width = self._get_terminal_width()
        parts: list[str] = []
        for line in lines:
            if len(line) > width - 1:
                line = line[: width - 4] + "..."
            parts.append(line + "\n")
        return "".join(parts)

    def status(self, *lines: str) -> None:
        """Print one or more status lines.

        In interactive mode: overwrites the previous status region atomically.
        In non-interactive mode: prints nothing.
        """
        if not self.interactive:
            return

        line_list = list(lines)
        buf = self._build_clear() + self._build_status(line_list)
        self._status_lines = line_list
        self._write_atomic(buf, self.stdout)

    def clear_status(self) -> None:
        """Clear the status region."""
        if not self.interactive or not self._status_lines:
            return
        buf = self._build_clear()
        self._status_lines = []
        self._write_atomic(buf, self.stdout)

    def print(self, msg: str) -> None:
        """Print a permanent message, then reprint the status lines below it."""
        if self.interactive:
            saved = list(self._status_lines)
            buf = self._build_clear() + msg + "\n" + self._build_status(saved)
            self._status_lines = saved
            self._write_atomic(buf, self.stdout)
        else:
            self._write_atomic(msg + "\n", self.stdout)

    def error(self, msg: str) -> None:
        """Print a permanent error message."""
        if self.interactive and self._status_lines:
            self.clear_status()
        self._write_atomic(msg + "\n", self.stderr)

    def verbose(self, msg: str) -> None:
        """Print a message only in verbose mode, with status line handling."""
        if self.verbose_enabled:
            self.print(msg)


# Global singleton instance
output = Output()
