"""Platform detection, command execution, and compiler/linker utilities."""

import functools
import os
import platform as platform_mod
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path
from typing import Literal, Optional, Sequence, Union

from .output import output


# =============================================================================
# Platform Detection
# =============================================================================

# Platform detection (evaluated once at module load)
WINDOWS = sys.platform == "win32"
MACOS = sys.platform == "darwin"
LINUX = sys.platform.startswith("linux")

# Executable extension
EXE_EXT = ".exe" if WINDOWS else ""

# Platform-specific configuration
if WINDOWS:
    LIB_PATH_VAR = None
    CPACK_CONFIG = {"ZIP": ".zip"}
    if shutil.which("makensis"):
        CPACK_CONFIG["NSIS"] = ".exe"
elif MACOS:
    LIB_PATH_VAR = "DYLD_LIBRARY_PATH"
    CPACK_CONFIG = {"TGZ": ".tar.gz"}
elif LINUX:
    LIB_PATH_VAR = "LD_LIBRARY_PATH"
    CPACK_CONFIG = {"TGZ": ".tar.gz", "DEB": ".deb"}
    if shutil.which("rpmbuild"):
        CPACK_CONFIG["RPM"] = ".rpm"
else:
    raise RuntimeError(f"Unsupported platform: {sys.platform}")


def get_wavm_bin_path(build_dir: Union[str, Path]) -> Path:
    """Get path to wavm binary given a build or install directory."""
    return Path(build_dir) / "bin" / f"wavm{EXE_EXT}"


# =============================================================================
# Windows MSVC Environment
# =============================================================================


def find_vcvarsall() -> Optional[Path]:
    """Find vcvarsall.bat from Visual Studio installation."""
    if not WINDOWS:
        return None

    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if not vswhere.exists():
        return None

    try:
        result = subprocess.run(
            [vswhere, "-latest", "-property", "installationPath"],
            capture_output=True,
            text=True,
        )
        if result.returncode == 0:
            vs_path = Path(result.stdout.strip())
            vcvarsall = vs_path / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
            if vcvarsall.exists():
                return vcvarsall
    except Exception:
        pass

    return None


@functools.cache
def get_host_arch() -> str:
    """Get the true host CPU architecture, normalized to a canonical name.

    Returns "x86_64" or "aarch64".

    On ARM64 Windows running emulated x64 Python, platform.machine() incorrectly
    reports 'AMD64'. This function uses IsWow64Process2 to detect the true native
    architecture on Windows.
    """
    if WINDOWS:
        import ctypes
        import ctypes.wintypes

        kernel32 = ctypes.windll.kernel32
        kernel32.GetCurrentProcess.restype = ctypes.wintypes.HANDLE
        kernel32.IsWow64Process2.restype = ctypes.wintypes.BOOL
        kernel32.IsWow64Process2.argtypes = [
            ctypes.wintypes.HANDLE,
            ctypes.POINTER(ctypes.c_ushort),
            ctypes.POINTER(ctypes.c_ushort),
        ]

        IMAGE_FILE_MACHINE_ARM64 = 0xAA64
        process_machine = ctypes.c_ushort()
        native_machine = ctypes.c_ushort()
        if not kernel32.IsWow64Process2(
            kernel32.GetCurrentProcess(),
            ctypes.byref(process_machine),
            ctypes.byref(native_machine),
        ):
            raise RuntimeError("IsWow64Process2 failed")
        arch = "aarch64" if native_machine.value == IMAGE_FILE_MACHINE_ARM64 else "x86_64"
        output.verbose(f"Host architecture: {arch} (native_machine=0x{native_machine.value:04X})")
        return arch

    machine = platform_mod.machine()
    if machine in ("x86_64", "AMD64"):
        return "x86_64"
    elif machine in ("arm64", "aarch64"):
        return "aarch64"
    else:
        raise RuntimeError(f"Unsupported host architecture: {machine}")


_target_arch: Optional[str] = None


def set_target_arch(arch: str) -> None:
    """Set the target architecture for cross-compilation."""
    global _target_arch
    if arch not in ("x86_64", "aarch64", "x64", "arm64"):
        raise ValueError(f"Unsupported target architecture: {arch}")
    _target_arch = {"x64": "x86_64", "arm64": "aarch64"}.get(arch, arch)


def get_target_arch() -> str:
    """Get the target architecture (defaults to host architecture)."""
    return _target_arch or get_host_arch()


def is_cross_compiling() -> bool:
    """Return True if the target architecture differs from the host."""
    return _target_arch is not None and _target_arch != get_host_arch()


def is_rosetta() -> bool:
    """Return True if targeting x86-64 on a macOS ARM64 host (Rosetta 2 emulation)."""
    return MACOS and get_host_arch() == "aarch64" and get_target_arch() == "x86_64"


def get_cross_compile_cmake_args() -> list[str]:
    """Extra CMake arguments needed when cross-compiling on macOS."""
    if MACOS and is_cross_compiling():
        osx_arch = {"x86_64": "x86_64", "aarch64": "arm64"}[get_target_arch()]
        return [f"-DCMAKE_OSX_ARCHITECTURES={osx_arch}"]
    return []


def get_build_dir(work_dir: Path, subdir: str, config_name: str) -> Path:
    """Construct a build/install directory path.

    When cross-compiling, inserts the target arch as a subdirectory so that
    native and cross-compiled builds don't clobber each other.
    """
    base = work_dir / subdir
    if is_cross_compiling():
        base = base / get_target_arch()
    return base / config_name


_vcvarsall_cmd: Optional[str] = None


def init_env() -> None:
    """On Windows, run vcvarsall.bat and merge MSVC environment into os.environ."""
    global _vcvarsall_cmd

    if not WINDOWS:
        return

    vcvarsall = find_vcvarsall()
    if not vcvarsall:
        return

    # Use the host architecture for vcvarsall.bat so we get native tools
    arch = {"x86_64": "amd64", "aarch64": "arm64"}[get_host_arch()]

    # Run vcvarsall.bat and capture the resulting environment
    vcvarsall_cmd = f'"{vcvarsall}" {arch}'
    _vcvarsall_cmd = vcvarsall_cmd
    cmd = f"{vcvarsall_cmd} && set"

    output.verbose(f"Running vcvarsall: {vcvarsall_cmd}")

    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True,
    )
    vcvarsall_stderr = result.stderr.strip()

    if result.returncode != 0:
        raise RuntimeError(
            f"vcvarsall.bat failed for architecture '{arch}'.\n"
            f"{vcvarsall_stderr}\n"
            f"You may need to install the MSVC '{arch}' build tools"
            f" in the Visual Studio Installer."
        )

    # Merge overrides into os.environ.
    for line in result.stdout.splitlines():
        if "=" in line:
            key, _, value = line.partition("=")
            if os.environ.get(key) != value:
                os.environ[key] = value
                output.verbose(f"Extracted environment variable from vcvarsall.bat: {key}={value}")

    # Verify the LIB paths actually contain the target architecture.
    # vcvarsall may report success but silently configure the wrong architecture
    # (e.g. x64 instead of arm64). MSVC library paths contain the architecture
    # as a directory component: ...\lib\x64\, ...\lib\arm64\, etc.
    lib_env = os.environ.get("LIB", "")
    if lib_env:
        lib_paths_lower = lib_env.lower()
        # Map vcvarsall arch names to the directory names used in MSVC lib paths
        arch_dir = {"amd64": "x64", "arm64": "arm64", "x86": "x86"}.get(arch, arch)
        has_target = f"\\{arch_dir}\\" in lib_paths_lower or lib_paths_lower.endswith(
            f"\\{arch_dir}"
        )
        if not has_target:
            msg = (
                f"MSVC library paths do not contain '{arch_dir}' directories.\n"
                f"vcvarsall.bat was called with '{arch}' but LIB contains:\n"
                f"  {lib_env}\n"
            )
            if vcvarsall_stderr:
                msg += f"vcvarsall.bat stderr:\n  {vcvarsall_stderr}\n"
            msg += (
                f"You may need to install the MSVC '{arch}' build tools"
                f" in the Visual Studio Installer."
            )
            raise RuntimeError(msg)


# =============================================================================
# Command Execution
# =============================================================================


def _decode_text(data: bytes) -> str:
    """Decode subprocess output bytes to str, normalizing line endings."""
    return data.decode("utf-8", errors="replace").replace("\r\n", "\n")


class CommandResult:
    """Result of a command execution, with helpers for formatting output."""

    def __init__(
        self,
        cmd: Sequence[Union[str, Path]],
        returncode: int,
        stdout: str,
        stderr: str,
        stdout_bytes: bytes,
        stderr_bytes: bytes,
        cwd: Optional[Union[str, Path]] = None,
        env: Optional[dict[str,str]] = None,
        timed_out: bool = False,
    ):
        self.cmd = cmd
        self.cwd = cwd
        self.env = env
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.stdout_bytes = stdout_bytes
        self.stderr_bytes = stderr_bytes
        self.timed_out = timed_out

    @staticmethod
    def from_completed(
        cmd: Sequence[Union[str, Path]],
        result: subprocess.CompletedProcess,
        cwd: Optional[Union[str, Path]],
        env: Optional[dict[str,str]],
    ) -> "CommandResult":
        stdout_bytes: bytes = result.stdout
        stderr_bytes: bytes = result.stderr
        return CommandResult(
            cmd, result.returncode,
            _decode_text(stdout_bytes),
            _decode_text(stderr_bytes),
            stdout_bytes, stderr_bytes,
            cwd=cwd, env=env,
        )

    @staticmethod
    def from_timeout(
        cmd: Sequence[Union[str, Path]],
        exc: subprocess.TimeoutExpired,
        cwd: Optional[Union[str, Path]],
        env: Optional[dict[str,str]],
    ) -> "CommandResult":
        def to_bytes(data: object) -> bytes:
            if data is None:
                return b""
            if isinstance(data, bytes):
                return data
            if isinstance(data, str):
                return data.encode("utf-8")
            return str(data).encode("utf-8")

        stdout_bytes = to_bytes(exc.stdout)
        stderr_bytes = to_bytes(exc.stderr)
        return CommandResult(
            cmd, -1,
            _decode_text(stdout_bytes),
            _decode_text(stderr_bytes),
            stdout_bytes, stderr_bytes,
            cwd=cwd, env=env, timed_out=True,
        )

    @property
    def output(self) -> str:
        """Combine stdout and stderr, including both when both are non-empty."""
        parts = []
        if self.stderr:
            parts.append(self.stderr)
        if self.stdout:
            parts.append(self.stdout)
        return "\n".join(parts) if parts else "(no output)"

    def format_failure(
        self,
        context: str = "",
        hint: Optional[str] = None,
        expected_returncode: Optional[int] = None,
    ) -> str:
        """Format a failure message including output, exit code, and command.

        The "Command:" line is a single copy-pasteable shell command that reproduces
        the failure, including cwd changes and extra environment variables.
        """
        parts = []
        if context:
            parts.append(context)
        parts.append(self.output)
        if self.timed_out:
            parts.append("TIMEOUT")
        elif expected_returncode is not None:
            parts.append(
                f"Exit code: {self.returncode} (expected {expected_returncode})"
            )
        else:
            parts.append(f"Exit code: {self.returncode}")

        parts.append(f"Command: {self._format_reproducible_command()}")

        if hint is not None:
            parts.append(hint)
        parts.append("-" * 80)
        return "\n".join(parts)

    def _format_reproducible_command(self) -> str:
        """Format a single copy-pasteable shell command to reproduce this invocation."""
        if WINDOWS:
            return self._format_windows_command()
        else:
            return self._format_posix_command()

    def _format_posix_command(self) -> str:
        """Format a POSIX shell command: (cd /path && VAR=val command 'args')"""
        needs_cwd = self.cwd is not None and Path(self.cwd).resolve() != Path.cwd()
        env_vars = self._get_extra_env_vars()

        cmd_str = " ".join(shlex.quote(str(arg)) for arg in self.cmd)

        # Build inline env prefix
        if env_vars:
            env_prefix = " ".join(
                f"{key}={shlex.quote(value)}" for key, value in env_vars
            )
            cmd_str = f"{env_prefix} {cmd_str}"

        if needs_cwd:
            return f"(cd {shlex.quote(str(self.cwd))} && {cmd_str})"
        return cmd_str

    def _format_windows_command(self) -> str:
        """Format a Windows cmd.exe command with setlocal/endlocal scoping."""
        needs_cwd = self.cwd is not None and Path(self.cwd).resolve() != Path.cwd()
        env_vars = self._get_extra_env_vars()
        needs_scope = bool(_vcvarsall_cmd) or bool(env_vars) or needs_cwd

        cmd_str = subprocess.list2cmdline(str(arg) for arg in self.cmd)

        fragments: list[str] = []
        if needs_scope:
            fragments.append("setlocal")
        if _vcvarsall_cmd:
            fragments.append(_vcvarsall_cmd)
        if needs_cwd:
            fragments.append(f'cd /d "{self.cwd}"')
        for key, value in env_vars:
            fragments.append(f'set "{key}={value}"')
        fragments.append(cmd_str)

        if needs_scope:
            # Use single & so endlocal runs regardless of exit code
            return " && ".join(fragments) + " & endlocal"
        return cmd_str

    def _get_extra_env_vars(self) -> list[tuple[str, str]]:
        """Return sorted list of (key, value) pairs for env vars that differ from os.environ."""
        if not self.env:
            return []
        return sorted(
            (key, value)
            for key, value in self.env.items()
            if os.environ.get(key) != value
        )


def run_command(
    cmd: Sequence[Union[str, Path]],
    cwd: Optional[Union[str, Path]] = None,
    env: Optional[dict[str,str]] = None,
    timeout: Optional[float] = None,
) -> CommandResult:
    """Run a command and return the result.

    Both cmd arguments and cwd can be Path objects - subprocess.run handles them automatically.
    env, if provided, contains extra environment variables merged on top of os.environ.
    Timeouts are caught and returned as a CommandResult with timed_out=True.
    """
    merged_env = dict(os.environ) | (env or {})

    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            env=merged_env,
            capture_output=True,
            timeout=timeout,
        )
        return CommandResult.from_completed(cmd, result, cwd, env)
    except subprocess.TimeoutExpired as exc:
        return CommandResult.from_timeout(cmd, exc, cwd, env)


# =============================================================================
# Download Utilities
# =============================================================================


_DOWNLOAD_MAX_RETRIES = 3
_DOWNLOAD_RETRY_DELAY = 5  # seconds


def _download_with_retry(url: str, dest: Path) -> None:
    """Download a URL to a local file, retrying on transient errors."""
    for attempt in range(_DOWNLOAD_MAX_RETRIES):
        try:
            urllib.request.urlretrieve(url, dest)
            return
        except (urllib.error.URLError, ConnectionError, OSError) as e:
            if attempt + 1 < _DOWNLOAD_MAX_RETRIES:
                output.print(f"download: {e} (retrying in {_DOWNLOAD_RETRY_DELAY}s)")
                time.sleep(_DOWNLOAD_RETRY_DELAY)
            else:
                raise


def download_and_extract(url: str, dest_dir: Path) -> None:
    """Download and extract an archive to the destination directory."""
    # Determine archive type from URL
    if url.endswith(".tar.xz"):
        archive_ext = ".tar.xz"
        tar_mode: Optional[Literal["r:xz", "r:gz"]] = "r:xz"
    elif url.endswith(".tar.gz"):
        archive_ext = ".tar.gz"
        tar_mode = "r:gz"
    elif url.endswith(".zip"):
        archive_ext = ".zip"
        tar_mode = None
    else:
        raise RuntimeError(f"Unknown archive type: {url}")

    # Download to temporary file
    temp_file = dest_dir / f"download{archive_ext}"
    dest_dir.mkdir(parents=True, exist_ok=True)

    try:
        _download_with_retry(url, temp_file)

        # Extract
        if tar_mode is not None:
            with tarfile.open(str(temp_file), tar_mode) as tar:
                # Use filter='data' on Python 3.12+ to avoid deprecation warning
                if sys.version_info >= (3, 12):
                    tar.extractall(dest_dir, filter="data")
                else:
                    tar.extractall(dest_dir)
        else:
            with zipfile.ZipFile(temp_file, "r") as zip_ref:
                zip_ref.extractall(dest_dir)
    finally:
        # Clean up temp file
        if temp_file.exists():
            temp_file.unlink()


# =============================================================================
# Compiler/Linker Detection
# =============================================================================


def get_compiler_major_version(
    command: list[str], version_pattern: str
) -> Optional[int]:
    """Run a compiler command and extract its major version.

    Returns the first capture group of version_pattern as an int, or None if the
    compiler is not found or the pattern doesn't match.
    """
    try:
        result = run_command(command)
        match = re.search(version_pattern, result.stdout + result.stderr)
        if match:
            return int(match.group(1))
    except FileNotFoundError:
        pass
    return None


def get_gcc_clang_major_version(compiler: str) -> Optional[int]:
    """Get the major version of a gcc/clang-compatible compiler."""
    return get_compiler_major_version([compiler, "--version"], r"(\d+)\.\d+")


def get_msvc_major_version() -> Optional[int]:
    """Get the major version of MSVC (cl.exe)."""
    return get_compiler_major_version(["cl"], r"Version (\d+)\.")


