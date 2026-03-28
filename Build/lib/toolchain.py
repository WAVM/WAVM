"""Toolchain detection, configuration, and CMake argument generation."""

import functools
import shutil
import sys
from collections.abc import Callable
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional, Union

from .llvm import get_llvm_toolchain_task
from .platform import (
    WINDOWS, MACOS, LINUX, EXE_EXT,
    get_host_arch,
    get_gcc_clang_major_version,
    get_msvc_major_version,
)
from .task_graph import Task


# =============================================================================
# Platform-specific compiler/linker names
# =============================================================================

if WINDOWS:
    LLD_EXECUTABLE = "lld-link.exe"
    CLANG_C_COMPILER = "clang-cl.exe"
    CLANG_CXX_COMPILER = "clang-cl.exe"
elif MACOS:
    LLD_EXECUTABLE = "ld64.lld"
    CLANG_C_COMPILER = "clang"
    CLANG_CXX_COMPILER = "clang++"
elif LINUX:
    LLD_EXECUTABLE = "ld.lld"
    CLANG_C_COMPILER = "clang"
    CLANG_CXX_COMPILER = "clang++"
else:
    raise RuntimeError(f"Unsupported platform: {sys.platform}")

# Short identity for PRIMARY toolchains (clang is the unnamed default)
COMPILER_IDENTITY: dict[str, str] = {
    "clang": "",
    "msvc": "msvc",
    "appleclang": "AppleClang",
    "gcc": "gcc",
}

# Display name for the "default" system linker on each platform
if WINDOWS:
    DEFAULT_LINKER_DISPLAY = "link"
elif MACOS:
    DEFAULT_LINKER_DISPLAY = "ld64"
elif LINUX:
    DEFAULT_LINKER_DISPLAY = "ld"


# =============================================================================
# CMake Utilities
# =============================================================================


@dataclass
class CMakeVarArg:
    """A CMake -D variable argument with automatic path conversion for values."""

    name: str
    value: Union[str, Path]

    def to_arg(self) -> str:
        # Convert to string and replace backslashes with forward slashes for CMake
        safe_value = str(self.value).replace("\\", "/")
        return f"-D{self.name}={safe_value}"


def build_cmake_cmd(args: list[Union[str, Path, CMakeVarArg]]) -> list[str]:
    """Build a CMake command, converting CMakeVarArg and Path instances to strings.

    CMakeVarArg values have backslashes converted to forward slashes to avoid
    CMake interpreting them as escape sequences.
    """
    result = []
    for arg in args:
        if isinstance(arg, CMakeVarArg):
            result.append(arg.to_arg())
        elif isinstance(arg, Path):
            result.append(str(arg))
        else:
            result.append(arg)
    return result


# =============================================================================
# Toolchain Detection and Configuration
# =============================================================================


def is_command_available(command: str) -> bool:
    """Check if a command is available on PATH."""
    return shutil.which(command) is not None


class ToolchainScope(Enum):
    """Whether a toolchain is used with all config templates or only those that expand toolchains."""

    PRIMARY = "primary"  # Used with all config templates (the preferred linker for each compiler)
    SECONDARY = "secondary"  # Only used with templates that have expand_secondary_toolchains=True


def _get_wavm_llvm_compiler_cmake_args(llvm_dir: Path) -> list[CMakeVarArg]:
    """Compiler + archiver cmake args from a WAVM-LLVM install."""
    args: list[CMakeVarArg] = [
        CMakeVarArg("CMAKE_C_COMPILER", llvm_dir / "bin" / CLANG_C_COMPILER),
        CMakeVarArg("CMAKE_CXX_COMPILER", llvm_dir / "bin" / CLANG_CXX_COMPILER),
    ]

    # On Windows, use llvm-lib (understands MSVC-style flags).
    # On other platforms, use llvm-ar/llvm-ranlib (the system
    # ar/ranlib may not understand LLVM bitcode format).
    if WINDOWS:
        llvm_lib_path = llvm_dir / "bin" / f"llvm-lib{EXE_EXT}"
        if llvm_lib_path.exists():
            args.append(CMakeVarArg("CMAKE_AR", llvm_lib_path))
    else:
        llvm_ar_path = llvm_dir / "bin" / f"llvm-ar{EXE_EXT}"
        llvm_ranlib_path = llvm_dir / "bin" / f"llvm-ranlib{EXE_EXT}"
        if llvm_ar_path.exists():
            args.append(CMakeVarArg("CMAKE_AR", llvm_ar_path))
        if llvm_ranlib_path.exists():
            args.append(CMakeVarArg("CMAKE_RANLIB", llvm_ranlib_path))

    return args


def _get_wavm_llvm_linker_cmake_args(llvm_dir: Path) -> list[CMakeVarArg]:
    """Linker cmake args from a WAVM-LLVM install."""
    lld_path = llvm_dir / "bin" / LLD_EXECUTABLE
    if lld_path.exists():
        return [CMakeVarArg("WAVM_USE_LINKER", lld_path)]
    return []


def _get_wavm_llvm_all_cmake_args(llvm_dir: Path) -> list[CMakeVarArg]:
    """Full toolchain cmake args (compiler + linker + archiver) from a WAVM-LLVM install."""
    return _get_wavm_llvm_compiler_cmake_args(llvm_dir) + _get_wavm_llvm_linker_cmake_args(llvm_dir)


class Toolchain:
    """A compiler-linker pair detected on the system.

    cmake_args are composed of static args (known at construction) plus optional
    dynamic args resolved from a WAVM-LLVM install directory. If resolve_llvm_cmake_args
    is set, prerequisite tasks must complete before get_cmake_args() can be called.
    """

    def __init__(
        self,
        compiler: str,
        linker_identity: Optional[str],
        cmake_args: tuple[CMakeVarArg, ...],
        scope: ToolchainScope,
        needs_aslr_workaround: bool,
        resolve_llvm_cmake_args: Optional[Callable[[Path], list[CMakeVarArg]]] = None,
    ):
        self.compiler = compiler
        self.linker_identity = linker_identity
        self._cmake_args = cmake_args
        self.scope = scope
        self.needs_aslr_workaround = needs_aslr_workaround
        self._resolve_llvm_cmake_args = resolve_llvm_cmake_args
        self._prerequisite_tasks: Optional[list[Task]] = None

    @property
    def identity(self) -> str:
        """Config name suffix. Empty string for the default WAVM-LLVM toolchain."""
        compiler_id = COMPILER_IDENTITY.get(self.compiler, self.compiler)
        if self.linker_identity is None:
            return compiler_id
        compiler_display = compiler_id or self.compiler
        return f"{compiler_display}-{self.linker_identity}"

    @property
    def lto_mode(self) -> str:
        """The LTO mode to use for this toolchain.

        Returns "Thin" for the pure WAVM-LLVM toolchain (whose lld can link
        ThinLTO-compiled LLVM libraries), "ON" for all others (which need
        non-ThinLTO LLVM libraries).
        """
        if self.compiler == "clang" and self.linker_identity is None:
            return "Thin"
        return "ON"

    @property
    def supports_coverage(self) -> bool:
        return self.compiler == "clang"

    def get_or_create_prerequisite_tasks(
        self, work_dir: Path, build_from_source: bool, offline: bool = False
    ) -> list[Task]:
        """Get prerequisite tasks for this toolchain, creating them on first call.

        Toolchains that use WAVM-LLVM components create the LLVM toolchain
        build/download task. Pure system toolchains return an empty list.
        """
        if self._prerequisite_tasks is None:
            if self._resolve_llvm_cmake_args is not None:
                task = get_llvm_toolchain_task(
                    work_dir, build_from_source, offline=offline
                )
                self._prerequisite_tasks = [task]
            else:
                self._prerequisite_tasks = []
        return self._prerequisite_tasks

    def get_cmake_args(self) -> list[CMakeVarArg]:
        """Get all compiler/linker/archiver CMake args.

        If this toolchain uses WAVM-LLVM components, prerequisite tasks must
        have been created and completed before calling this.
        """
        result = list(self._cmake_args)
        if self._resolve_llvm_cmake_args is not None:
            assert self._prerequisite_tasks is not None and len(self._prerequisite_tasks) > 0, \
                "get_or_create_prerequisite_tasks must be called before get_cmake_args"
            task = self._prerequisite_tasks[0]
            assert task.result is not None, \
                "prerequisite tasks must complete before get_cmake_args"
            llvm_dir: Path = task.result.artifacts["llvm_install_dir"]
            result.extend(self._resolve_llvm_cmake_args(llvm_dir))
        return result


@functools.cache
def detect_toolchains() -> list[Toolchain]:
    """Detect all available compiler-linker toolchains on this system.

    Each compiler gets a PRIMARY toolchain (its preferred/default linker) and
    optionally SECONDARY toolchains for alternative linkers (e.g. clang with
    the system default linker, AppleClang with WAVM-LLVM lld, gcc with gold).

    Results are cached since detection runs subprocesses.
    """
    # Precompute ASLR workaround flag: Linux + clang + version < 18
    clang_version = get_gcc_clang_major_version("clang") if LINUX else None
    clang_needs_aslr_workaround = clang_version is not None and clang_version < 18

    toolchains: list[Toolchain] = []

    # MSVC (Windows only)
    cl_version = get_msvc_major_version()
    if WINDOWS and cl_version is not None and cl_version >= 19:
        msvc_args = (
            CMakeVarArg("CMAKE_C_COMPILER", "cl"),
            CMakeVarArg("CMAKE_CXX_COMPILER", "cl"),
            CMakeVarArg("CMAKE_ASM_COMPILER", "cl"),
        )

        # MSVC + link.exe — PRIMARY
        toolchains.append(Toolchain(
            compiler="msvc",
            linker_identity=None,
            cmake_args=msvc_args,
            scope=ToolchainScope.PRIMARY,
            needs_aslr_workaround=False,
        ))

        # MSVC + WAVM-LLVM lld-link — SECONDARY
        toolchains.append(Toolchain(
            compiler="msvc",
            linker_identity="lld",
            cmake_args=msvc_args,
            scope=ToolchainScope.SECONDARY,
            needs_aslr_workaround=False,
            resolve_llvm_cmake_args=_get_wavm_llvm_linker_cmake_args,
        ))

    # Clang (WAVM-LLVM compiler + lld) — PRIMARY
    toolchains.append(Toolchain(
        compiler="clang",
        linker_identity=None,
        cmake_args=(),
        scope=ToolchainScope.PRIMARY,
        needs_aslr_workaround=LINUX and clang_needs_aslr_workaround,
        resolve_llvm_cmake_args=_get_wavm_llvm_all_cmake_args,
    ))

    # Clang (WAVM-LLVM compiler + system default linker) — SECONDARY
    toolchains.append(Toolchain(
        compiler="clang",
        linker_identity=DEFAULT_LINKER_DISPLAY,
        cmake_args=(),
        scope=ToolchainScope.SECONDARY,
        needs_aslr_workaround=LINUX and clang_needs_aslr_workaround,
        resolve_llvm_cmake_args=_get_wavm_llvm_compiler_cmake_args,
    ))

    # AppleClang (macOS only, system clang)
    if MACOS and is_command_available("clang"):
        apple_clang_args = (
            CMakeVarArg("CMAKE_C_COMPILER", "clang"),
            CMakeVarArg("CMAKE_CXX_COMPILER", "clang++"),
        )

        # AppleClang + system default linker — PRIMARY
        toolchains.append(Toolchain(
            compiler="appleclang",
            linker_identity=None,
            cmake_args=apple_clang_args,
            scope=ToolchainScope.PRIMARY,
            needs_aslr_workaround=False,
        ))

        # AppleClang + WAVM-LLVM lld — SECONDARY
        toolchains.append(Toolchain(
            compiler="appleclang",
            linker_identity="lld",
            cmake_args=apple_clang_args,
            scope=ToolchainScope.SECONDARY,
            needs_aslr_workaround=False,
            resolve_llvm_cmake_args=_get_wavm_llvm_linker_cmake_args,
        ))

    # GCC (Linux only, with linker variants)
    # GCC on AArch64 is excluded due to GCC 14 being unable to pass the test
    # suite with several different functions miscompiled in optimized builds.
    gcc_version = get_gcc_clang_major_version("gcc")
    if LINUX and get_host_arch() != "aarch64" and gcc_version is not None and gcc_version >= 8:
        gcc_base_args = (
            CMakeVarArg("CMAKE_C_COMPILER", "gcc"),
            CMakeVarArg("CMAKE_CXX_COMPILER", "g++"),
        )

        # Detect available system linkers
        system_lld_available = LINUX and is_command_available("ld.lld")

        # (linker_identity, extra_cmake_args)
        linker_options: list[tuple[str, tuple[CMakeVarArg, ...]]] = [
            (DEFAULT_LINKER_DISPLAY, ()),
        ]
        if system_lld_available:
            linker_options.append(("lld", (CMakeVarArg("WAVM_USE_LINKER", "lld"),)))

        # Default linker is PRIMARY for GCC (lld doesn't support GCC's GIMPLE LTO)
        primary_identity = DEFAULT_LINKER_DISPLAY

        for linker_identity, extra_args in linker_options:
            is_primary = linker_identity == primary_identity
            toolchains.append(Toolchain(
                compiler="gcc",
                linker_identity=None if is_primary else linker_identity,
                cmake_args=gcc_base_args + extra_args,
                scope=ToolchainScope.PRIMARY if is_primary else ToolchainScope.SECONDARY,
                needs_aslr_workaround=False,
            ))

    return toolchains


def get_primary_toolchain(compiler: str) -> Toolchain:
    """Get the PRIMARY toolchain for a compiler."""
    for tc in detect_toolchains():
        if tc.compiler == compiler and tc.scope == ToolchainScope.PRIMARY:
            return tc
    raise ValueError(f"No PRIMARY toolchain found for compiler {compiler!r}")
