"""LLVM download, build, and version management."""

import functools
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Union
from .platform import LINUX, MACOS, WINDOWS, get_target_arch, run_command, download_and_extract
from .task_graph import Task, TaskResult


# =============================================================================
# LLVM Configuration
# =============================================================================


@dataclass
class LLVMConfig:
    """LLVM build configuration.

    CMake arguments are handled by WAVM-LLVM's build-llvm.py and cmake cache files.
    This class only tracks config names and platform restrictions.
    """

    name: str
    # Platform restrictions
    posix_only: bool = False
    windows_only: bool = False


# LLVM configurations (matching WAVM-LLVM repo)
# CMake args are defined in WAVM-LLVM/Build/cmake/*.cmake
LLVM_CONFIGS = [
    LLVMConfig(name="LTO"),
    LLVMConfig(name="RelWithDebInfo"),
    LLVMConfig(name="Debug"),
    LLVMConfig(name="Checked"),
    LLVMConfig(name="Sanitized", posix_only=True),  # Sanitizers work best on POSIX
]


def get_applicable_llvm_configs() -> list[LLVMConfig]:
    """Get LLVM configs applicable for the current platform."""
    return [
        c
        for c in LLVM_CONFIGS
        if not (c.posix_only and WINDOWS) and not (c.windows_only and not WINDOWS)
    ]


# =============================================================================
# LLVM Constants
# =============================================================================

LLVM_VERSION = "21"

# WAVM-LLVM repository for patches and pre-built binaries
WAVM_LLVM_GIT_URL = "https://github.com/WAVM/WAVM-LLVM.git"
WAVM_LLVM_RELEASES_URL = "https://github.com/WAVM/WAVM-LLVM/releases"
LLVM_PROJECT_GIT_URL = "https://github.com/llvm/llvm-project.git"


# =============================================================================
# LLVM Utilities
# =============================================================================


def get_platform_name() -> str:
    """Get the platform name for LLVM release downloads (e.g. "macos-arm64")."""
    if LINUX:
        system_name = "linux"
    elif MACOS:
        system_name = "macos"
    elif WINDOWS:
        system_name = "windows"
    else:
        raise RuntimeError(f"Unsupported system: {sys.platform}")

    arch_name = {"x86_64": "x64", "aarch64": "arm64"}[get_target_arch()]

    return f"{system_name}-{arch_name}"


def _get_release_tag_for_commit(commit_hash: str) -> str:
    """Construct the expected release tag for a WAVM-LLVM commit."""
    return f"{LLVM_VERSION}.x-{commit_hash}"


def get_llvm_download_url(release_tag: str, config_name: str) -> str:
    """Get the download URL for a pre-built LLVM configuration."""
    platform_name = get_platform_name()
    ext = "zip" if platform_name.startswith("windows") else "tar.xz"
    filename = f"llvm-{LLVM_VERSION}-{platform_name}-{config_name}.{ext}"
    return f"{WAVM_LLVM_RELEASES_URL}/download/{release_tag}/{filename}"


# =============================================================================
# LLVM Tasks
# =============================================================================


class CloneWAVMLLVMTask(Task):
    """Clone or update the WAVM-LLVM repository.

    This task only clones/updates WAVM-LLVM which contains:
    - build-llvm.py script for building LLVM
    - patches/ directory with WAVM-specific patches
    - LLVM_COMMIT file with pinned commit hash
    - Build/cmake/ directory with cmake cache files
    """

    def __init__(self, scratch_dir: Path, offline: bool = False):
        super().__init__(
            name="clone_wavm_llvm",
            description="WAVM-LLVM: clone",
        )
        self.scratch_dir = scratch_dir
        self.offline = offline

    def run(self) -> TaskResult:
        wavm_llvm_dir = self.scratch_dir / "WAVM-LLVM"

        if self.offline:
            if not wavm_llvm_dir.exists():
                return TaskResult(
                    False,
                    f"WAVM-LLVM not found at {wavm_llvm_dir} (--offline requires a prior clone)",
                )
        elif wavm_llvm_dir.exists():
            run_command(["git", "fetch", "origin"], cwd=wavm_llvm_dir)
            run_command(["git", "checkout", f"release/{LLVM_VERSION}.x"], cwd=wavm_llvm_dir)
            result = run_command(["git", "pull", "--ff-only"], cwd=wavm_llvm_dir)
            if result.returncode != 0:
                # Pull failed, try reset
                run_command(
                    ["git", "reset", "--hard", f"origin/release/{LLVM_VERSION}.x"],
                    cwd=wavm_llvm_dir,
                )
        else:
            result = run_command(
                [
                    "git",
                    "clone",
                    "--depth",
                    "1",
                    "--branch",
                    f"release/{LLVM_VERSION}.x",
                    WAVM_LLVM_GIT_URL,
                    wavm_llvm_dir,
                ]
            )
            if result.returncode != 0:
                return TaskResult(False, result.format_failure("Failed to clone WAVM-LLVM"))

        # Verify build-llvm.py exists
        build_script = wavm_llvm_dir / "build-llvm.py"
        if not build_script.exists():
            return TaskResult(False, "build-llvm.py not found in WAVM-LLVM repo")

        # Get the short commit hash for release tag lookup
        result = run_command(
            ["git", "rev-parse", "--short=7", "HEAD"], cwd=wavm_llvm_dir
        )
        if result.returncode != 0:
            return TaskResult(False, result.format_failure("Failed to get WAVM-LLVM commit hash"))
        commit_hash = result.stdout.strip()

        return TaskResult(
            True,
            artifacts={
                "wavm_llvm_dir": wavm_llvm_dir,
                "commit_hash": commit_hash,
            },
        )


class DownloadLLVMTask(Task):
    """Download pre-built LLVM from WAVM-LLVM GitHub releases.

    Uses the cloned WAVM-LLVM commit hash to find the matching release.
    Falls back to the latest release if no matching release exists yet.
    """

    def __init__(
        self,
        config: LLVMConfig,
        clone_task: CloneWAVMLLVMTask,
        scratch_dir: Path,
    ):
        platform_name = get_platform_name()
        super().__init__(
            name=f"llvm_download_{platform_name}_{config.name}",
            description=f"LLVM {config.name} ({platform_name}): download",
            dependencies=[clone_task],
        )
        self.config = config
        self.clone_task = clone_task
        self.scratch_dir = scratch_dir

    def run(self) -> TaskResult:
        assert self.clone_task.result is not None
        commit_hash: str = self.clone_task.result.artifacts["commit_hash"]

        install_dir = self.scratch_dir / "llvm-install" / f"{get_platform_name()}-{self.config.name}"

        release_tag = _get_release_tag_for_commit(commit_hash)

        stamp_file = install_dir / ".download_stamp"
        if stamp_file.exists():
            if stamp_file.read_text().strip() == release_tag:
                cmake_dir = install_dir / "lib" / "cmake" / "llvm"
                if cmake_dir.exists():
                    return TaskResult(True, artifacts={"llvm_install_dir": install_dir})

        # Clean and download
        if install_dir.exists():
            shutil.rmtree(install_dir)

        try:
            url = get_llvm_download_url(release_tag, self.config.name)
            download_and_extract(url, install_dir)
        except Exception as e:
            return TaskResult(False, f"Failed to download LLVM {self.config.name}: {e}")

        # Write stamp file
        stamp_file.write_text(release_tag)

        cmake_dir = install_dir / "lib" / "cmake" / "llvm"
        if not cmake_dir.exists():
            return TaskResult(False, f"Downloaded LLVM missing cmake config: {cmake_dir}")

        return TaskResult(True, artifacts={"llvm_install_dir": install_dir})


class UseLocalLLVMTask(Task):
    """Use a previously downloaded LLVM installation (offline mode)."""

    def __init__(self, config: LLVMConfig, scratch_dir: Path):
        platform_name = get_platform_name()
        super().__init__(
            name=f"llvm_local_{platform_name}_{config.name}",
            description=f"LLVM {config.name} ({platform_name}): use local",
        )
        self.config = config
        self.scratch_dir = scratch_dir

    def run(self) -> TaskResult:
        install_dir = self.scratch_dir / "llvm-install" / f"{get_platform_name()}-{self.config.name}"
        cmake_dir = install_dir / "lib" / "cmake" / "llvm"
        if not cmake_dir.exists():
            return TaskResult(
                False,
                f"LLVM {self.config.name} not found at {install_dir}"
                " (--offline requires a prior download or build)",
            )
        return TaskResult(True, artifacts={"llvm_install_dir": install_dir})


class BuildLLVMTask(Task):
    """Build an LLVM configuration from source using build-llvm.py.

    Delegates to WAVM-LLVM's build-llvm.py script which handles:
    - Cloning llvm-project at the pinned commit
    - Applying patches
    - Incremental build detection
    - Configuring and building with cmake cache files
    """

    def __init__(
        self,
        config: LLVMConfig,
        clone_task: "CloneWAVMLLVMTask",
        scratch_dir: Path,
        force_rebuild: bool,
        prev_build_task: Optional[Task] = None,
    ):
        # Chain builds to serialize access to shared llvm-project directory
        deps: list[Task] = [clone_task]
        if prev_build_task:
            deps.append(prev_build_task)
        platform_name = get_platform_name()
        super().__init__(
            name=f"llvm_build_{platform_name}_{config.name}",
            description=f"LLVM {config.name} ({platform_name}): build",
            dependencies=deps,
        )
        self.config = config
        self.clone_task = clone_task
        self.scratch_dir = scratch_dir
        self.force_rebuild = force_rebuild

    def run(self) -> TaskResult:
        assert self.clone_task.result is not None
        wavm_llvm_dir: Path = self.clone_task.result.artifacts["wavm_llvm_dir"]
        build_script = wavm_llvm_dir / "build-llvm.py"

        # Use llvm-install/{platform}-{config} to match DownloadLLVMTask's directory structure
        install_dir = self.scratch_dir / "llvm-install" / f"{get_platform_name()}-{self.config.name}"

        # Build command
        cmd: list[Union[str, Path]] = [
            sys.executable,
            build_script,
            "--config",
            self.config.name,
            "--work-dir",
            self.scratch_dir,
            "--install-dir",
            install_dir,
        ]

        if self.force_rebuild:
            cmd.append("--clean")

        result = run_command(cmd)

        if result.returncode != 0:
            return TaskResult(False, result.format_failure("build-llvm.py failed"))

        # Verify install directory exists
        cmake_dir = install_dir / "lib" / "cmake" / "llvm"
        if not cmake_dir.exists():
            return TaskResult(
                False, f"LLVM build completed but cmake config not found: {cmake_dir}"
            )

        return TaskResult(True, artifacts={"llvm_install_dir": install_dir})


@dataclass
class _LLVMState:
    """Mutable state for the LLVM task factory, created once per (work_dir, build_from_source, offline)."""

    llvm_work_dir: Path
    clone_task: Optional[CloneWAVMLLVMTask] = None
    prev_build_task: Optional[Task] = None


@functools.cache
def _init_llvm(
    work_dir: Path, build_from_source: bool, offline: bool
) -> _LLVMState:
    """One-time LLVM infrastructure setup. Cached per (work_dir, build_from_source, offline)."""
    llvm_work_dir = work_dir / "llvm"
    state = _LLVMState(llvm_work_dir=llvm_work_dir)

    # Clone WAVM-LLVM for both source builds and downloads (to get the commit hash
    # for release tag lookup). Only skip for offline downloads (UseLocalLLVMTask).
    if not (offline and not build_from_source):
        state.clone_task = CloneWAVMLLVMTask(llvm_work_dir, offline=offline)

    return state


@functools.cache
def get_llvm_task(
    work_dir: Path,
    build_from_source: bool,
    config_name: str,
    offline: bool = False,
) -> Task:
    """Get or create an LLVM task by config name. Cached per unique argument set."""
    state = _init_llvm(work_dir, build_from_source, offline)

    llvm_config = next(
        (c for c in get_applicable_llvm_configs() if c.name == config_name), None
    )
    if llvm_config is None:
        raise ValueError(f"Unknown LLVM config: {config_name!r}")

    if build_from_source:
        assert state.clone_task is not None
        task: Task = BuildLLVMTask(
            config=llvm_config,
            clone_task=state.clone_task,
            scratch_dir=state.llvm_work_dir,
            force_rebuild=False,
            prev_build_task=state.prev_build_task,
        )
        state.prev_build_task = task
    elif offline:
        task = UseLocalLLVMTask(
            config=llvm_config,
            scratch_dir=state.llvm_work_dir,
        )
    else:
        assert state.clone_task is not None
        task = DownloadLLVMTask(
            config=llvm_config,
            clone_task=state.clone_task,
            scratch_dir=state.llvm_work_dir,
        )

    return task


# The LLVM toolchain config provides clang, clang-format, clang-tidy, lld, etc.
LLVM_TOOLCHAIN_CONFIG = "LTO"


def get_llvm_toolchain_task(
    work_dir: Path,
    build_from_source: bool,
    offline: bool = False,
) -> Task:
    """Get the LLVM toolchain task (provides clang, lld, clang-format, etc.)."""
    return get_llvm_task(work_dir, build_from_source, LLVM_TOOLCHAIN_CONFIG, offline=offline)
