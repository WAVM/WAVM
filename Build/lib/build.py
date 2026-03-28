"""Build configuration classes and CMake utilities."""

import argparse
import hashlib
import json
import shutil
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional, Union

from .context import CommandContext
from .output import output
from .platform import (
    MACOS,
    WINDOWS,
    CPACK_CONFIG,
    get_build_dir,
    get_cross_compile_cmake_args,
    run_command,
)
from .toolchain import (
    Toolchain,
    ToolchainScope,
    CMakeVarArg,
    build_cmake_cmd,
    detect_toolchains,
    get_primary_toolchain,
)
from .task_graph import Task, TaskResult, execute
from .llvm import get_llvm_task


# =============================================================================
# WAVM Configuration
# =============================================================================


@dataclass
class WAVMConfigTemplate:
    """Template for WAVM build configuration (before compiler is determined)."""

    name: str
    build_type: str  # CMAKE_BUILD_TYPE
    llvm_config: Optional[str]  # Which LLVM config to use (None if LLVM not needed)
    cmake_args: list[str] = field(default_factory=list)
    # Platform/compiler restrictions
    exclude_gcc: bool = False
    exclude_windows: bool = False
    exclude_macos: bool = False
    # Whether to test fuzz corpora on this config
    test_fuzz_corpora: bool = False
    # Whether to expand this config across secondary toolchains (e.g. non-preferred linkers)
    expand_secondary_toolchains: bool = False
    # Whether to adjust LTO args based on compiler (MSVC doesn't support ThinLTO)
    use_lto: bool = False
    # Whether to enable code coverage on clang builds of this config
    enable_coverage_on_clang: bool = False


@dataclass
class WAVMConfig:
    """WAVM build configuration with toolchain specified."""

    name: str  # Full name including toolchain suffix (e.g., "Release-clang")
    base_name: str  # Base config name (e.g., "Release")
    toolchain: Toolchain  # Compiler-linker pair to use
    build_type: str  # CMAKE_BUILD_TYPE
    llvm_config: Optional[str]  # Which LLVM config to use (None if LLVM not needed)
    cmake_args: list[str] = field(default_factory=list)
    uses_asan: bool = False  # Whether this config uses AddressSanitizer
    uses_tsan: bool = False  # Whether this config uses ThreadSanitizer
    test_fuzz_corpora: bool = False  # Whether to test fuzz corpora on this config
    enable_coverage: bool = False  # Whether code coverage instrumentation is enabled


# WAVM configuration templates
WAVM_CONFIG_TEMPLATES = [
    WAVMConfigTemplate(
        name="RelWithDebInfo",
        build_type="RelWithDebInfo",
        llvm_config="RelWithDebInfo",
        cmake_args=[],
        expand_secondary_toolchains=True,
    ),
    WAVMConfigTemplate(
        name="Debug",
        build_type="Debug",
        llvm_config="Debug",
        cmake_args=[],
    ),
    WAVMConfigTemplate(
        name="Checked",
        build_type="RelWithDebInfo",
        llvm_config="Checked",
        cmake_args=["-DWAVM_ENABLE_RELEASE_ASSERTS=ON", "-DWAVM_ENABLE_FUZZ_TARGETS=ON"],
        test_fuzz_corpora=True,
        enable_coverage_on_clang=True,
    ),
    WAVMConfigTemplate(
        name="Static",
        build_type="Release",
        llvm_config="RelWithDebInfo",
        cmake_args=["-DWAVM_ENABLE_STATIC_LINKING=ON"],
    ),
    WAVMConfigTemplate(
        name="LTO",
        build_type="Release",
        llvm_config="LTO",
        cmake_args=["-DWAVM_ENABLE_FUZZ_TARGETS=ON"],
        test_fuzz_corpora=True,
        use_lto=True,
    ),
    WAVMConfigTemplate(
        name="StaticLTO",
        build_type="Release",
        llvm_config="LTO",
        cmake_args=[
            "-DWAVM_ENABLE_STATIC_LINKING=ON",
            "-DWAVM_ENABLE_FUZZ_TARGETS=OFF",
        ],
        exclude_windows=True,
        use_lto=True,
    ),
    WAVMConfigTemplate(
        name="UBASAN",
        build_type="RelWithDebInfo",
        llvm_config="Sanitized",
        cmake_args=[
            "-DWAVM_ENABLE_STATIC_LINKING=ON",
            "-DWAVM_ENABLE_ASAN=ON",
            "-DWAVM_ENABLE_UBSAN=ON",
        ],
        exclude_macos=True,
        exclude_windows=True,
    ),
    WAVMConfigTemplate(
        name="TSAN",
        build_type="Release",
        llvm_config="RelWithDebInfo",
        cmake_args=["-DWAVM_ENABLE_TSAN=ON"],
        exclude_gcc=True,
        exclude_windows=True,
    ),
    WAVMConfigTemplate(
        name="NoRuntime",
        build_type="RelWithDebInfo",
        llvm_config=None,
        cmake_args=["-DWAVM_ENABLE_RUNTIME=NO"],
    ),
]


# =============================================================================
# Build Stamp Utilities
# =============================================================================


def get_build_stamp(config_hash: str, llvm_commit: str) -> str:
    """Create a build stamp for incremental builds."""
    return f"{config_hash}:{llvm_commit}"


def read_build_stamp(build_dir: Path) -> Optional[str]:
    """Read the build stamp from a previous build."""
    stamp_file = build_dir / ".build_stamp"
    if stamp_file.exists():
        return stamp_file.read_text().strip()
    return None


def write_build_stamp(build_dir: Path, stamp: str) -> None:
    """Write the build stamp for a completed build."""
    stamp_file = build_dir / ".build_stamp"
    stamp_file.write_text(stamp)


def hash_config(cmake_args: list[str]) -> str:
    """Create a hash of the configuration for incremental builds."""
    config_str = json.dumps(sorted(cmake_args))
    return hashlib.sha256(config_str.encode()).hexdigest()[:12]


def get_config_string(cmake_args: list[str]) -> str:
    """Create a string representation of cmake args for comparison."""
    return "\n".join(cmake_args)


def needs_cmake_configure(build_dir: Path, cmake_args: list[str]) -> bool:
    """Check if cmake configuration needs to run.

    Returns True if cmake needs to run, False if config is unchanged and can be skipped.
    """
    config_str = get_config_string(cmake_args)
    args_file = build_dir / ".cmake_config_args"
    cache_file = build_dir / "CMakeCache.txt"

    if args_file.exists() and cache_file.exists():
        stored_config = args_file.read_text()
        if stored_config == config_str:
            output.verbose("Config unchanged, skipping cmake")
            return False  # Config unchanged, skip cmake
        output.verbose("Config changed, running cmake --fresh")
        output.verbose(f"  Stored:\n    {stored_config.replace(chr(10), chr(10) + '    ')}")
        output.verbose(f"  Current:\n    {config_str.replace(chr(10), chr(10) + '    ')}")
    else:
        if not cache_file.exists():
            output.verbose("No CMakeCache.txt, running cmake --fresh")
        else:
            output.verbose("No config args file, running cmake --fresh")

    return True  # Need to run cmake


def write_cmake_config_args(build_dir: Path, cmake_args: list[str]) -> None:
    """Write the cmake config args after successful configuration."""
    config_str = get_config_string(cmake_args)
    args_file = build_dir / ".cmake_config_args"
    build_dir.mkdir(parents=True, exist_ok=True)
    args_file.write_text(config_str)


# =============================================================================
# Configuration Generators
# =============================================================================


def get_applicable_wavm_configs() -> list[WAVMConfig]:
    """Get WAVM configs for all detected toolchains and the current platform.

    The default WAVM-LLVM toolchain produces bare template names (e.g. "Debug").
    Other toolchains append an identity suffix (e.g. "Debug-msvc", "Debug-gcc").
    Templates with expand_secondary_toolchains are expanded across all toolchains.
    Other templates use only PRIMARY toolchains.
    """
    toolchains = detect_toolchains()
    configs = []

    for toolchain in toolchains:
        seen_base_names: set[str] = set()

        for template in WAVM_CONFIG_TEMPLATES:
            # Skip secondary toolchains unless the template expands them
            if (
                toolchain.scope == ToolchainScope.SECONDARY
                and not template.expand_secondary_toolchains
            ):
                continue

            # Check platform restrictions
            if template.exclude_windows and WINDOWS:
                continue
            if template.exclude_macos and MACOS:
                continue

            # Check compiler restrictions
            if template.exclude_gcc and toolchain.compiler == "gcc":
                continue

            # Avoid duplicates (e.g., LTO has separate Windows/POSIX versions)
            if template.name in seen_base_names:
                continue
            seen_base_names.add(template.name)

            identity = toolchain.identity
            full_name = f"{template.name}-{identity}" if identity else template.name

            # Check if this config uses ASAN or TSAN
            uses_asan = "-DWAVM_ENABLE_ASAN=ON" in template.cmake_args
            uses_tsan = "-DWAVM_ENABLE_TSAN=ON" in template.cmake_args

            # Check if coverage should be enabled for this toolchain
            enable_coverage = template.enable_coverage_on_clang and toolchain.supports_coverage

            # Handle LTO toolchain-specific args
            cmake_args = list(template.cmake_args)
            llvm_config = template.llvm_config
            if template.use_lto:
                cmake_args.append(f"-DWAVM_ENABLE_LTO={toolchain.lto_mode}")
                if toolchain.lto_mode == "ON":
                    # For toolchains that support non-thin LTO, don't link against the ThinLTO-compiled LLVM libraries.
                    llvm_config = "RelWithDebInfo"

            if enable_coverage:
                cmake_args.append("-DWAVM_ENABLE_COVERAGE=ON")

            configs.append(
                WAVMConfig(
                    name=full_name,
                    base_name=template.name,
                    toolchain=toolchain,
                    build_type=template.build_type,
                    llvm_config=llvm_config,
                    cmake_args=cmake_args,
                    uses_asan=uses_asan,
                    uses_tsan=uses_tsan,
                    test_fuzz_corpora=template.test_fuzz_corpora,
                    enable_coverage=enable_coverage,
                )
            )

    return configs


def find_wavm_config(config_name: str) -> tuple[Optional[WAVMConfig], list[WAVMConfig]]:
    """Find a WAVM config by exact name.

    Returns (config, all_configs) where config may be None if not found.
    """
    all_configs = get_applicable_wavm_configs()
    config = next((c for c in all_configs if c.name == config_name), None)
    return config, all_configs


def get_lint_config() -> WAVMConfig:
    """Get the Lint config for clangd/clang-tidy.

    This config is only configured (not built or tested). It always uses
    clang with the Debug LLVM config.
    """
    return WAVMConfig(
        name="Lint",
        base_name="Lint",
        toolchain=get_primary_toolchain("clang"),
        build_type="Debug",
        llvm_config="Debug",
        cmake_args=["-DWAVM_COMPILE_HEADERS=ON"],
    )


def needs_aslr_workaround(config: WAVMConfig) -> bool:
    """Check if we need to disable ASLR for sanitizer tests.

    Older versions of clang (< 18) don't handle high-entropy ASLR well,
    causing ASAN/TSAN to fail with DEADLYSIGNAL or unexpected memory mapping errors.
    """
    if not config.toolchain.needs_aslr_workaround:
        return False
    has_sanitizer = (
        "-DWAVM_ENABLE_ASAN=ON" in config.cmake_args or "-DWAVM_ENABLE_TSAN=ON" in config.cmake_args
    )
    return has_sanitizer


# =============================================================================
# WAVM Build Tasks
# =============================================================================


class ConfigureWAVMTask(Task):
    """Configure WAVM with CMake."""

    def __init__(
        self,
        config: WAVMConfig,
        build_dir: Path,
        install_dir: Path,
        source_dir: Path,
        llvm_task: Optional[Task],
        toolchain_prerequisite_tasks: list[Task],
    ):
        dependencies: list[Task] = []
        if llvm_task:
            dependencies.append(llvm_task)
        for task in toolchain_prerequisite_tasks:
            if task not in dependencies:
                dependencies.append(task)

        super().__init__(
            name=f"wavm_configure_{config.name}",
            description=f"{config.name}: configure",
            dependencies=dependencies,
        )
        self.config = config
        self.build_dir = build_dir
        self.install_dir = install_dir
        self.source_dir = source_dir
        self.llvm_task = llvm_task

    def run(self) -> TaskResult:
        # Get LLVM directory if this config uses LLVM
        llvm_cmake_dir: Optional[Path] = None
        if self.llvm_task:
            assert self.llvm_task.result is not None
            llvm_install_dir: Path = self.llvm_task.result.artifacts["llvm_install_dir"]
            llvm_cmake_dir = llvm_install_dir / "lib" / "cmake" / "llvm"

        self.build_dir.mkdir(parents=True, exist_ok=True)

        cmake_args: list[Union[str, Path, CMakeVarArg]] = [
            "cmake",
            "--fresh",
            self.source_dir,
            "-G",
            "Ninja",
            CMakeVarArg("CMAKE_BUILD_TYPE", self.config.build_type),
            CMakeVarArg("CMAKE_INSTALL_PREFIX", self.install_dir),
            CMakeVarArg("CMAKE_EXPORT_COMPILE_COMMANDS", "ON"),
        ]

        if llvm_cmake_dir:
            cmake_args.append(CMakeVarArg("LLVM_DIR", llvm_cmake_dir))

        cmake_args.extend(self.config.toolchain.get_cmake_args())

        cmake_args.extend(self.config.cmake_args)

        cmake_args.extend(get_cross_compile_cmake_args())

        # Set CPack generators based on platform
        cpack_generators = ";".join(CPACK_CONFIG.keys())
        cmake_args.append(CMakeVarArg("CPACK_GENERATOR", cpack_generators))

        # Convert CMakeVarArg objects to strings
        cmake_cmd = build_cmake_cmd(cmake_args)

        if needs_cmake_configure(self.build_dir, cmake_cmd):
            # Args changed or first configure: run cmake --fresh
            result = run_command(cmake_cmd, cwd=self.build_dir)
            if result.returncode != 0:
                return TaskResult(False, result.format_failure())

            write_cmake_config_args(self.build_dir, cmake_cmd)
            return TaskResult(True, warning_message=result.stderr)

        # Args unchanged: use ninja's mtime checking to reconfigure only if
        # CMakeLists.txt or other cmake inputs changed.
        result = run_command(["ninja", "rebuild_cache"], cwd=self.build_dir)
        if result.returncode != 0:
            return TaskResult(False, result.format_failure())

        return TaskResult(True, warning_message=result.stderr)


class BuildWAVMTask(Task):
    """Build WAVM."""

    def __init__(
        self,
        config: WAVMConfig,
        build_dir: Path,
        install_dir: Path,
        configure_task: "ConfigureWAVMTask",
        ninja_targets: Optional[list[str]] = None,
    ):
        super().__init__(
            name=f"wavm_build_{config.name}",
            description=f"{config.name}: build",
            dependencies=[configure_task],
        )
        self.config = config
        self.build_dir = build_dir
        self.install_dir = install_dir
        self.configure_task = configure_task
        self.ninja_targets = ninja_targets

    def run(self) -> TaskResult:
        cmd = ["ninja", "--quiet"] + (self.ninja_targets or [])
        result = run_command(cmd, cwd=self.build_dir)

        if result.returncode != 0:
            return TaskResult(False, result.format_failure())

        return TaskResult(True, warning_message=result.stderr)


def create_wavm_configure_task(
    wavm_config: WAVMConfig,
    work_dir: Path,
    source_dir: Path,
    build_llvm_from_source: bool,
    install_dir: Optional[Path] = None,
    offline: bool = False,
) -> ConfigureWAVMTask:
    """Create a configure-only task for a WAVM configuration."""
    llvm_task = (
        get_llvm_task(
            work_dir, build_llvm_from_source, wavm_config.llvm_config, offline=offline
        )
        if wavm_config.llvm_config
        else None
    )
    toolchain_prerequisite_tasks = wavm_config.toolchain.get_or_create_prerequisite_tasks(
        work_dir, build_llvm_from_source, offline=offline
    )

    build_dir = get_build_dir(work_dir, "build", wavm_config.name)
    if install_dir is None:
        install_dir = get_build_dir(work_dir, "install", wavm_config.name)
    configure_task = ConfigureWAVMTask(
        config=wavm_config,
        build_dir=build_dir,
        install_dir=install_dir,
        source_dir=source_dir,
        llvm_task=llvm_task,
        toolchain_prerequisite_tasks=toolchain_prerequisite_tasks,
    )
    return configure_task


def create_wavm_build_tasks(
    wavm_config: WAVMConfig,
    work_dir: Path,
    source_dir: Path,
    build_llvm_from_source: bool,
    install_dir: Optional[Path] = None,
    ninja_targets: Optional[list[str]] = None,
    offline: bool = False,
) -> BuildWAVMTask:
    """Create configure and build tasks for a WAVM configuration."""
    configure_task = create_wavm_configure_task(
        wavm_config, work_dir, source_dir, build_llvm_from_source, install_dir,
        offline=offline,
    )

    build_task = BuildWAVMTask(
        config=wavm_config,
        build_dir=configure_task.build_dir,
        install_dir=configure_task.install_dir,
        configure_task=configure_task,
        ninja_targets=ninja_targets,
    )

    return build_task


def build_single_config(
    work_dir: Path,
    source_dir: Path,
    config_name: str,
    llvm_source: bool = False,
    offline: bool = False,
    ninja_targets: Optional[list[str]] = None,
    suppress_summary: bool = False,
) -> tuple[Optional[Path], Optional[int]]:
    """Build a single WAVM configuration.

    Returns (build_dir, None) on success, or (None, exit_code) on error.
    """
    # Look up configs
    wavm_config, all_configs = find_wavm_config(config_name)
    if not wavm_config:
        if not all_configs:
            output.error("Error: No compilers found")
        else:
            output.error(f"Error: Config '{config_name}' not found")
            output.error(f"Available: {[c.name for c in all_configs]}")
        return None, 1

    install_dir = get_build_dir(work_dir, "install", config_name)

    build_task = create_wavm_build_tasks(
        wavm_config,
        work_dir,
        source_dir,
        build_llvm_from_source=llvm_source,
        install_dir=install_dir,
        ninja_targets=ninja_targets,
        offline=offline,
    )

    if not execute([build_task], suppress_summary=suppress_summary):
        return None, 1

    return build_task.build_dir, None


# =============================================================================
# Command Handlers
# =============================================================================


def cmd_list_configs(args: argparse.Namespace, ctx: CommandContext) -> int:
    """List available configurations for this platform."""
    wavm_configs = get_applicable_wavm_configs()
    for config in wavm_configs:
        print(config.name)

    return 0


def register_list_configs(subparsers: argparse._SubParsersAction) -> None:
    subparsers.add_parser(
        "list-configs", help="List available configurations"
    ).set_defaults(func=cmd_list_configs)


def cmd_package(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Build and package a WAVM configuration."""
    build_dir, err = build_single_config(
        ctx.work_dir, ctx.source_dir, args.config,
        llvm_source=args.llvm_source, offline=args.offline,
        ninja_targets=["package"],
    )
    if err is not None:
        return err
    assert build_dir is not None

    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    # Copy generated packages to output directory
    output.status("Copying packages to output directory...")

    packages_found = []
    for ext in CPACK_CONFIG.values():
        filename = f"wavm-package{ext}"
        package_path = build_dir / filename
        if package_path.exists():
            dest_path = output_dir / filename
            shutil.copy2(package_path, dest_path)
            packages_found.append(filename)
            output.print(f"  {filename}")

    if not packages_found:
        output.error("No packages were generated")
        return 1

    output.clear_status()
    output.print(f"Packages created in {output_dir}")
    return 0


def register_package(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser("package", help="Build and package a WAVM configuration")
    p.add_argument(
        "--config",
        required=True,
        help="Config to build and package (required)",
    )
    p.add_argument(
        "output_dir",
        help="Directory to copy packages to",
    )
    p.set_defaults(func=cmd_package)


