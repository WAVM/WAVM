"""Test execution, task management, and test history."""

import argparse
import functools
import os
import re
import signal
import json
import shutil
import tempfile
import threading
import time
from dataclasses import dataclass, field, replace
from pathlib import Path
from collections.abc import Sequence
from typing import Optional, TYPE_CHECKING

from .build import (
    needs_aslr_workaround,
    get_applicable_wavm_configs,
    find_wavm_config,
    get_lint_config,
    create_wavm_configure_task,
    create_wavm_build_tasks,
)
from .format import create_format_tasks
from .lint import create_lint_tasks
from .context import CommandContext
from .coverage import CoverageReportTask
from .llvm import get_llvm_toolchain_task
from .output import output
from .platform import (
    LINUX,
    MACOS,
    WINDOWS,
    get_cross_compile_cmake_args,
    get_target_arch,
    get_wavm_bin_path,
    is_rosetta,
    run_command,
)
from .tidy import create_tidy_tasks
from .benchmark import get_benchmark_test_defs
from .toolchain import is_command_available
from .task_graph import Task, TaskResult, execute
from .test_def import TestContext, TestDef, TestResult, TestStep
from .wasi import (
    WASI_TESTS,
    DownloadWasiSdkTask,
    CompileWasiTestTask,
)

if TYPE_CHECKING:
    from .build import WAVMConfig


# =============================================================================
# Test Definition Constants
# =============================================================================

def _wavm_version_expected_output() -> str:
    """Build the expected output regex for the 'wavm version' test."""
    wavm_version = (Path(__file__).parent.parent.parent / "VERSION").read_text().strip()
    arch = get_target_arch()
    os_name = "macos" if MACOS else "linux" if LINUX else "windows"
    return (
        rf"(?s)WAVM version {re.escape(wavm_version)}.*"
        rf"LLVM version:\s+\d+\.\d+\.\d+.*"
        rf"Host target:\s+arch={arch} os={os_name}"
    )


_fuzz_max_threads = str(max(1, (os.cpu_count() or 1) // 2))


@functools.cache
def get_tests() -> list[TestDef]:
    """Static tests (non-WAST, non-fuzz). Deferred to a function so that
    get_target_arch() and get_cross_compile_cmake_args() see the target arch
    set by command-line arguments."""
    return [
        # Built-in tests (wavm test <name>)
        TestDef(
            "HashMap",
            steps=[TestStep(command=["{wavm_bin}", "test", "hashmap"])],
            requires_runtime=False,
        ),
        TestDef(
            "HashSet",
            steps=[TestStep(command=["{wavm_bin}", "test", "hashset"])],
            requires_runtime=False,
        ),
        TestDef(
            "I128", steps=[TestStep(command=["{wavm_bin}", "test", "i128"])], requires_runtime=False
        ),
        TestDef(
            "LEB128",
            steps=[TestStep(command=["{wavm_bin}", "test", "leb128"])],
            requires_runtime=False,
        ),
        TestDef("ObjectLinker", steps=[TestStep(command=["{wavm_bin}", "test", "objectlinker"])]),
        TestDef("DWARF", steps=[TestStep(command=["{wavm_bin}", "test", "dwarf"])]),
        TestDef("C-API", steps=[TestStep(command=["{wavm_bin}", "test", "c-api"])]),
        TestDef("API", steps=[TestStep(command=["{wavm_bin}", "test", "api"])]),
        TestDef(
            "version",
            steps=[TestStep(
                command=["{wavm_bin}", "version"],
                expected_output=_wavm_version_expected_output(),
            )],
        ),
        # Run tests (wavm run <args>)
        *get_benchmark_test_defs(),
        TestDef(
            "examples_trap",
            steps=[
                TestStep(
                    command=["{wavm_bin}", "run", "--abi=wasi",
                            "{source_dir}/Examples/trap.wast"],
                    expected_returncode=1 if WINDOWS else -signal.SIGABRT,
                    expected_output=r"(?s)Runtime exception: wavm\.outOfBoundsMemoryAccess\(\+65536\).*?Call stack:.*?wasm!.*?trap\.wast!store\+2.*?wasm!.*?trap\.wast!main\+1",
                )
            ],
        ),
        *WASI_TESTS,
        # Embedder example tests - build and run examples that link against libWAVM
        TestDef(
            name="embedder-c",
            requires_shared_build=True,
            requires_unsanitized_build=True,
            requires_cmake_and_ninja=True,
            create_temp_dir=True,
            steps=[
                TestStep(
                    name="configure",
                    collects_coverage=False,
                    command=[
                        "cmake",
                        "-S",
                        "{source_dir}/Examples/embedder/c",
                        "-B",
                        "{temp_dir}",
                        "-G",
                        "Ninja",
                        "-DCMAKE_BUILD_TYPE={build_type}",
                        "-DCMAKE_PREFIX_PATH={build_dir}",
                        *get_cross_compile_cmake_args(),
                    ],
                ),
                TestStep(name="build", collects_coverage=False, command=["ninja", "-C", "{temp_dir}"]),
                TestStep(
                    name="run",
                    command=["{temp_dir}/embedder-example{exe_ext}"],
                    expected_output=r"Hello, embedder-example\.c user!\nWASM call returned: 23",
                    env={},
                ),
            ],
        ),
        TestDef(
            name="embedder-cpp",
            requires_shared_build=True,
            requires_unsanitized_build=True,
            requires_cmake_and_ninja=True,
            create_temp_dir=True,
            steps=[
                TestStep(
                    name="configure",
                    collects_coverage=False,
                    command=[
                        "cmake",
                        "-S",
                        "{source_dir}/Examples/embedder/cpp",
                        "-B",
                        "{temp_dir}",
                        "-G",
                        "Ninja",
                        "-DCMAKE_BUILD_TYPE={build_type}",
                        "-DCMAKE_PREFIX_PATH={build_dir}",
                        *get_cross_compile_cmake_args(),
                    ],
                ),
                TestStep(name="build", collects_coverage=False, command=["ninja", "-C", "{temp_dir}"]),
                TestStep(
                    name="run",
                    command=["{temp_dir}/embedder-example{exe_ext}"],
                    expected_output=r"Hello, embedder-example\.cpp user!\nWASM call returned: 25",
                    env={},
                ),
            ],
        ),
        TestDef(
            name="embedder-cpp-wasi",
            requires_shared_build=True,
            requires_unsanitized_build=True,
            requires_cmake_and_ninja=True,
            test_wasi_cpp_sources=["stdout"],
            create_temp_dir=True,
            steps=[
                TestStep(
                    name="configure",
                    collects_coverage=False,
                    command=[
                        "cmake",
                        "-S",
                        "{source_dir}/Examples/embedder/cpp-wasi",
                        "-B",
                        "{temp_dir}",
                        "-G",
                        "Ninja",
                        "-DCMAKE_BUILD_TYPE={build_type}",
                        "-DCMAKE_PREFIX_PATH={build_dir}",
                        *get_cross_compile_cmake_args(),
                    ],
                ),
                TestStep(name="build", collects_coverage=False, command=["ninja", "-C", "{temp_dir}"]),
                TestStep(
                    name="run",
                    command=["{temp_dir}/embedder-example{exe_ext}", "{wasi_wasm_dir}/stdout.wasm"],
                    expected_output=r"Hello world!",
                    env={},
                ),
            ],
        ),
        # Compile and run precompiled module
        TestDef(
            name="compile_and_run_precompiled",
            create_temp_dir=True,
            steps=[
                TestStep(
                    name="compile",
                    command=[
                        "{wavm_bin}",
                        "compile",
                        "{source_dir}/Examples/helloworld.wast",
                        "{temp_dir}/helloworld.wasm",
                    ],
                ),
                TestStep(
                    name="run",
                    command=["{wavm_bin}", "run", "--precompiled", "{temp_dir}/helloworld.wasm"],
                    expected_output=r"Hello World!",
                ),
            ],
        ),
        # Object cache hit/miss verification
        TestDef(
            name="object_cache",
            create_temp_dir=True,
            steps=[
                TestStep(
                    name="cold_run",
                    command=["{wavm_bin}", "run", "{source_dir}/Test/object-cache/module_a.wast"],
                    expected_output=r"Object cache miss: fe1a07d742ee311117fd8aa867b099bd",
                    unexpected_output=r"Object cache hit",
                    env={"WAVM_OUTPUT": "trace-object-cache", "WAVM_OBJECT_CACHE_DIR": "{temp_dir}"},
                ),
                TestStep(
                    name="warm_run",
                    command=["{wavm_bin}", "run", "{source_dir}/Test/object-cache/module_a.wast"],
                    expected_output=r"Object cache hit: fe1a07d742ee311117fd8aa867b099bd",
                    unexpected_output=r"Object cache miss",
                    env={"WAVM_OUTPUT": "trace-object-cache", "WAVM_OBJECT_CACHE_DIR": "{temp_dir}"},
                ),
                TestStep(
                    name="invalidated_run",
                    command=["{wavm_bin}", "run", "{source_dir}/Test/object-cache/module_b.wast"],
                    expected_output=r"Object cache miss: 73a1a9dced2e363da054326fef90bd86",
                    env={"WAVM_OUTPUT": "trace-object-cache", "WAVM_OBJECT_CACHE_DIR": "{temp_dir}"},
                ),
            ],
        ),
        # Fuzz corpus tests - run fuzz targets on the corpora from the Git repo
        TestDef(
            "fuzz-assemble",
            steps=[
                TestStep(
                    command=[
                        "{build_dir}/bin/fuzz-assemble{exe_ext}",
                        "--max-threads=" + _fuzz_max_threads,
                        "{corpora_dir}/assemble",
                    ]
                )
            ],
            requires_fuzz_corpora_and_fuzz_targets=True,
            requires_runtime=False,
        ),
        TestDef(
            "fuzz-disassemble",
            steps=[
                TestStep(
                    command=[
                        "{build_dir}/bin/fuzz-disassemble{exe_ext}",
                        "--max-threads=" + _fuzz_max_threads,
                        "{corpora_dir}/disassemble",
                    ]
                )
            ],
            requires_fuzz_corpora_and_fuzz_targets=True,
            requires_runtime=False,
        ),
        TestDef(
            "fuzz-compile-model",
            steps=[
                TestStep(
                    command=[
                        "{build_dir}/bin/fuzz-compile-model{exe_ext}",
                        "--max-threads=" + _fuzz_max_threads,
                        "{corpora_dir}/compile-model",
                    ]
                )
            ],
            requires_fuzz_corpora_and_fuzz_targets=True,
        ),
        TestDef(
            "fuzz-instantiate",
            steps=[
                TestStep(
                    command=[
                        "{build_dir}/bin/fuzz-instantiate{exe_ext}",
                        "--max-threads=" + _fuzz_max_threads,
                        "{corpora_dir}/instantiate",
                    ],
                    timeout=1200,
                )
            ],
            requires_fuzz_corpora_and_fuzz_targets=True,
        ),
    ]


# GDB backtrace test - verifies GDB can see JIT-compiled function names (Linux only)
GDB_BACKTRACE_TEST = TestDef(
    name="gdb_backtrace",
    steps=[
        TestStep(
            command=[
                "gdb",
                "-batch",
                "-ex",
                "run",
                "-ex",
                "bt",
                "--args",
                "{wavm_bin}",
                "run",
                "{source_dir}/Examples/trap.wast",
            ],
            expected_output=r"(?i)functiondef",
            timeout=120,
        )
    ],
)

# Fuzz corpora repository
FUZZ_CORPORA_URL = "https://github.com/WAVM/wavm-fuzz-corpora.git"

@dataclass
class WASTTestDir:
    """A directory of WAST script tests with shared arguments and per-test overrides."""

    directory: str
    name_prefix: str
    extra_args: list[str]
    asan_env_overrides: dict[str, dict[str, str]] = field(default_factory=dict)
    tsan_env_overrides: dict[str, dict[str, str]] = field(default_factory=dict)
    env_overrides: dict[str, dict[str, str]] = field(default_factory=dict)


WAST_TEST_DIRS = [
    WASTTestDir(
        "Test/wavm",
        "wavm/",
        [
            "--test-cloning",
            "--strict-assert-invalid",
            "--strict-assert-malformed",
            "--enable",
            "all",
        ],
        asan_env_overrides={"wavm/exceptions.wast": {"ASAN_OPTIONS": "detect_leaks=0"}},
        tsan_env_overrides={},
        env_overrides={
            "wavm/exceptions.wast": {"WAVM_OUTPUT": "trace-unwind"},
            "wavm/infinite-recursion.wast": {"WAVM_OUTPUT": "trace-unwind"},
        },
    ),
    WASTTestDir(
        "Test/wavm/issues",
        "wavm/issues/",
        [
            "--strict-assert-invalid",
            "--strict-assert-malformed",
        ],
    ),
    WASTTestDir(
        "Test/WebAssembly/spec",
        "WebAssembly/spec/",
        ["--test-cloning"],
    ),
    WASTTestDir(
        "Test/WebAssembly/spec/simd",
        "",
        ["--test-cloning", "--strict-assert-invalid", "--strict-assert-malformed"],
    ),
    WASTTestDir(
        "Test/WebAssembly/memory64",
        "WebAssembly/memory64/",
        ["--test-cloning", "--enable", "memory64", "--disable", "ref-types"],
    ),
    WASTTestDir(
        "Test/WebAssembly/threads",
        "WebAssembly/threads/",
        ["--test-cloning", "--enable", "atomics", "--disable", "ref-types"],
    ),
    WASTTestDir(
        "Test/WebAssembly/multi-memory",
        "WebAssembly/multi-memory/",
        ["--test-cloning", "--enable", "multi-memory"],
    ),
]


# =============================================================================
# Test History System
# =============================================================================


@dataclass
class TestHistoryEntry:
    """History entry for a single test."""

    passed: bool
    duration_seconds: float


# Test priority levels (lower = higher priority, sorted ascending)
PRIORITY_FAILED = 0  # Previously failed tests run first
PRIORITY_DEFAULT = 1  # Default priority for tasks without history
PRIORITY_PASSED = 2  # Previously passed tests run last


# Test history loaded from disk (used for prioritization)
_test_history: dict[str, TestHistoryEntry] = {}
# Test results from this run (will be saved on exit)
_test_results: dict[str, TestHistoryEntry] = {}
_test_history_path: Optional[Path] = None
_test_history_lock = threading.Lock()


def load_test_history(scratch_dir: Path) -> None:
    """Load test history from disk."""
    global _test_history, _test_results, _test_history_path
    _test_history_path = scratch_dir / "test_history.json"
    _test_history = {}
    _test_results = {}

    if _test_history_path.exists():
        try:
            data = json.loads(_test_history_path.read_text())
            for name, entry in data.items():
                _test_history[name] = TestHistoryEntry(
                    passed=entry["passed"],
                    duration_seconds=entry["duration_seconds"],
                )
        except (json.JSONDecodeError, KeyError, IOError):
            _test_history = {}


def save_test_history() -> None:
    """Save test results from this run to disk."""
    if _test_history_path is None:
        return

    with _test_history_lock:
        data = {
            name: {"passed": entry.passed, "duration_seconds": entry.duration_seconds}
            for name, entry in _test_results.items()
        }

    try:
        _test_history_path.write_text(json.dumps(data, indent=2))
    except IOError:
        pass


def update_test_history(
    config_name: str, test_name: str, passed: bool, duration_seconds: float
) -> None:
    """Record a test result from this run."""
    key = f"{config_name}:{test_name}"
    with _test_history_lock:
        _test_results[key] = TestHistoryEntry(passed=passed, duration_seconds=duration_seconds)


def get_test_priority(config_name: str, test_name: str) -> tuple[int, float]:
    """Get sort key for a test: (priority, -duration).

    PRIORITY_FAILED = previously failed (run first)
    PRIORITY_DEFAULT = no history
    PRIORITY_PASSED = previously passed

    Within same priority, sort by duration descending (slowest first).
    """
    key = f"{config_name}:{test_name}"
    entry = _test_history.get(key)
    if entry is None:
        return (PRIORITY_DEFAULT, 0.0)
    elif not entry.passed:
        return (PRIORITY_FAILED, -entry.duration_seconds)
    else:
        return (PRIORITY_PASSED, -entry.duration_seconds)


# =============================================================================
# Test Tasks
# =============================================================================


@functools.cache
def is_gdb_available() -> bool:
    """Check if GDB is installed and available (cached)."""
    return is_command_available("gdb")


class RunTestTask(Task):
    """Run a single test."""

    def __init__(
        self,
        test: TestDef,
        context: TestContext,
        parent_task: Task,
        extra_dependencies: Optional[list[Task]] = None,
        coverage_profraw_prefix: Optional[str] = None,
    ):
        dependencies = [parent_task]
        if extra_dependencies:
            dependencies.extend(extra_dependencies)
        super().__init__(
            name=f"test_{context.config.name}_{test.name}",
            description=f"{context.config.name}: {test.name}",
            dependencies=dependencies,
            priority=get_test_priority(context.config.name, test.name),
        )
        self.test = test
        self.context = context

        # Precompute per-step profraw paths
        self.step_profraw_paths: list[Optional[str]] = []
        for step_index, step in enumerate(test.steps):
            if coverage_profraw_prefix and step.collects_coverage:
                path = f"{coverage_profraw_prefix}-{step_index}.profraw"
                self.step_profraw_paths.append(path.replace("\\", "/"))
            else:
                self.step_profraw_paths.append(None)

    def run(self) -> TaskResult:
        start_time = time.monotonic()

        context = self.context
        test = self.test

        if test.create_temp_dir:
            temp_dir = Path(tempfile.mkdtemp(prefix=f"{test.name}_"))
            try:
                context = replace(context, temp_dir=temp_dir)
                test_result = self._run_steps(context)
            finally:
                shutil.rmtree(temp_dir, ignore_errors=True)
        else:
            test_result = self._run_steps(context)

        duration = time.monotonic() - start_time
        update_test_history(context.config.name, test.name, test_result.success, duration)
        return TaskResult(test_result.success, test_result.message)

    def _run_steps(self, context: TestContext) -> TestResult:
        for step, profraw_path in zip(self.test.steps, self.step_profraw_paths):
            step_label = step.name or self.test.name
            result = step.run(context, step_label, coverage_profraw_path=profraw_path)
            if not result.success:
                prefix = f"[{step.name}] " if step.name else ""
                return TestResult(False, f"{prefix}{result.message}")
        return TestResult(True)


class DiscoverWASTTestsTask(Task):
    """Discover WAST script test files (shared across all configs)."""

    def __init__(self, source_dir: Path, trace_tests: bool = False):
        super().__init__(
            name="discover_wast_tests",
            description="Discover WAST test files",
            dependencies=[],
        )
        self.source_dir = source_dir
        self.trace_tests = trace_tests
        self.wast_tests: list[TestDef] = []

    # Tests that trigger synchronous exceptions (stack overflow via guard page SIGSEGV) in
    # JIT code. Rosetta 2 can't deliver these signals, causing an "EmulateForward" crash.
    # See: https://github.com/JuliaLang/julia/issues/42398
    ROSETTA_SKIP_TESTS = {
        "wavm/infinite-recursion.wast",
        "WebAssembly/spec/fac.wast",
        "WebAssembly/memory64/fac.wast",
        "WebAssembly/threads/fac.wast",
        "WebAssembly/multi-memory/fac.wast",
    }

    def run(self) -> TaskResult:
        self.wast_tests = []
        skip_tests = self.ROSETTA_SKIP_TESTS if is_rosetta() else set()

        for wast_dir in WAST_TEST_DIRS:
            dir_path = self.source_dir / wast_dir.directory
            if not dir_path.exists():
                continue

            for wast_file in sorted(dir_path.glob("*.wast")):
                rel_path = wast_file.relative_to(self.source_dir)
                test_name = f"{wast_dir.name_prefix}{wast_file.name}"

                if test_name in skip_tests:
                    continue

                # Get test-specific environment overrides
                test_env = wast_dir.env_overrides.get(test_name, {})
                test_asan_env = wast_dir.asan_env_overrides.get(test_name, {})
                test_tsan_env = wast_dir.tsan_env_overrides.get(test_name, {})

                # Build command with placeholders and optional --trace-tests
                command = ["{wavm_bin}", "test", "script", "{source_dir}/" + str(rel_path)]
                if self.trace_tests:
                    command.append("--trace-tests")
                command.extend(wast_dir.extra_args)

                self.wast_tests.append(
                    TestDef(
                        name=test_name,
                        steps=[
                            TestStep(
                                command=command,
                                env=test_env,
                                asan_env=test_asan_env,
                                tsan_env=test_tsan_env,
                            )
                        ],
                    )
                )

        return TaskResult(True, artifacts={"test_count": len(self.wast_tests)})


class CloneFuzzCorporaTask(Task):
    """Clone the fuzz corpora repository (shared across all configs)."""

    def __init__(self, working_dir: Path, offline: bool = False):
        super().__init__(
            name="clone_fuzz_corpora",
            description="Clone fuzz corpora",
            dependencies=[],
        )
        self.working_dir = working_dir
        self.corpora_dir = self.working_dir / "wavm-fuzz-corpora"
        self.offline = offline

    def run(self) -> TaskResult:
        if self.offline:
            if not self.corpora_dir.exists():
                return TaskResult(
                    False,
                    f"Fuzz corpora not found at {self.corpora_dir}"
                    " (--offline requires a prior clone)",
                )
            return TaskResult(True, artifacts={"corpora_dir": self.corpora_dir})

        if self.corpora_dir.exists():
            # Already cloned, do a pull to update
            result = run_command(["git", "-C", self.corpora_dir, "pull", "--ff-only"])
            if result.returncode != 0:
                # Pull failed, try a fresh clone
                try:
                    shutil.rmtree(self.corpora_dir)
                except OSError as e:
                    return TaskResult(False, f"Failed to remove stale fuzz corpora directory: {e}")

        if not self.corpora_dir.exists():
            result = run_command(["git", "clone", "--depth=1", FUZZ_CORPORA_URL, self.corpora_dir])
            if result.returncode != 0:
                return TaskResult(False, result.format_failure("Failed to clone fuzz corpora"))

        return TaskResult(True, artifacts={"corpora_dir": self.corpora_dir})


class CreateTestTasksTask(Task):
    """Create test tasks for a specific config."""

    def __init__(
        self,
        config: "WAVMConfig",
        build_dir: Path,
        discover_task: DiscoverWASTTestsTask,
        source_dir: Path,
        wasi_wasm_dir: Path,
        wasi_compile_tasks: dict[str, CompileWasiTestTask],
        complete_task: Task,
        build_wavm_task: Optional[Task] = None,
        fuzz_corpora_task: Optional[CloneFuzzCorporaTask] = None,
        work_dir: Optional[Path] = None,
        llvm_toolchain_task: Optional[Task] = None,
    ):
        dependencies: list[Task] = [discover_task]

        super().__init__(
            name=f"wavm_create_tests_{config.name}",
            description=f"{config.name}: create tests",
            dependencies=dependencies,
            priority=(0, 0.0) # prioritize creating all the tasks before anything else.
        )
        self.config = config
        self.build_dir = build_dir
        self.build_wavm_task = build_wavm_task
        self.discover_task = discover_task
        self.source_dir = source_dir
        self.wasi_wasm_dir = wasi_wasm_dir
        self.wasi_compile_tasks = wasi_compile_tasks
        self.complete_task = complete_task
        self.fuzz_corpora_task = fuzz_corpora_task
        self.work_dir = work_dir
        self.llvm_toolchain_task = llvm_toolchain_task

    def run(self) -> TaskResult:
        corpora_dir = self.fuzz_corpora_task.corpora_dir if self.fuzz_corpora_task else None

        has_runtime = "-DWAVM_ENABLE_RUNTIME=NO" not in self.config.cmake_args
        is_static = "-DWAVM_ENABLE_STATIC_LINKING=ON" in self.config.cmake_args
        has_sanitizers = any(
            arg in self.config.cmake_args
            for arg in ["-DWAVM_ENABLE_ASAN=ON", "-DWAVM_ENABLE_UBSAN=ON", "-DWAVM_ENABLE_TSAN=ON"]
        )
        has_gdb = LINUX and is_gdb_available()
        has_cmake_and_ninja = shutil.which("cmake") is not None and shutil.which("ninja") is not None
        use_setarch = needs_aslr_workaround(self.config)
        test_fuzz_corpora = self.config.test_fuzz_corpora and corpora_dir

        def should_include_test(t: TestDef) -> bool:
            if not has_runtime and t.requires_runtime:
                return False
            if is_static and t.requires_shared_build:
                return False
            if has_sanitizers and t.requires_unsanitized_build:
                return False
            if not test_fuzz_corpora and t.requires_fuzz_corpora_and_fuzz_targets:
                return False
            if not has_cmake_and_ninja and t.requires_cmake_and_ninja:
                return False
            return True

        # Collect all tests: static tests + discovered WAST tests + conditional tests
        all_tests: list[TestDef] = []
        all_tests.extend(t for t in get_tests() if should_include_test(t))
        all_tests.extend(t for t in self.discover_task.wast_tests if should_include_test(t))

        if has_gdb and has_runtime:
            all_tests.append(GDB_BACKTRACE_TEST)

        if not all_tests:
            return TaskResult(True, artifacts={"test_count": 0})

        # Set up coverage directory if coverage is enabled
        coverage_dir: Optional[Path] = None
        if self.config.enable_coverage and self.work_dir:
            coverage_dir = self.work_dir / "coverage" / self.config.name
            if coverage_dir.exists():
                shutil.rmtree(coverage_dir)
            coverage_dir.mkdir(parents=True)

        # Create shared context for all tests in this config
        context = TestContext(
            config=self.config,
            source_dir=self.source_dir,
            build_dir=self.build_dir,
            corpora_dir=corpora_dir,
            use_setarch=use_setarch,
            wasi_wasm_dir=self.wasi_wasm_dir,
        )

        test_tasks: list[Task] = []
        for test in all_tests:
            safe_name = test.name.replace("/", "_").replace("\\", "_")
            profraw_prefix = str(coverage_dir / safe_name) if coverage_dir else None

            extra_deps: list[Task] = []
            if self.build_wavm_task:
                extra_deps += [self.build_wavm_task]
            elif test.requires_fuzz_corpora_and_fuzz_targets:
                assert self.fuzz_corpora_task
                extra_deps += [self.fuzz_corpora_task]

            for stem in test.test_wasi_cpp_sources:
                extra_deps += [self.wasi_compile_tasks[stem]]

            test_tasks.append(
                RunTestTask(
                    test=test,
                    context=context,
                    parent_task=self,
                    extra_dependencies=extra_deps,
                    coverage_profraw_prefix=profraw_prefix,
                )
            )

        # Add test tasks (and optional coverage report) as deps of the completion task
        self.complete_task.dependencies.extend(test_tasks)
        if coverage_dir and self.work_dir:
            report_task = CoverageReportTask(
                config_name=self.config.name,
                build_dir=self.build_dir,
                coverage_dir=coverage_dir,
                test_names=[t.name for t in all_tests],
                llvm_toolchain_task=self.llvm_toolchain_task,
                work_dir=self.work_dir,
                source_dir=self.source_dir,
                test_tasks=test_tasks,
            )
            self.complete_task.dependencies.append(report_task)

        return TaskResult(True, artifacts={"test_count": len(test_tasks)})


# =============================================================================
# Test Pipeline Setup
# =============================================================================


def setup_test_tasks(
    configs: Sequence[tuple["WAVMConfig", Path, Optional[Task]]],
    source_dir: Path,
    work_dir: Path,
    llvm_toolchain_task: Optional[Task] = None,
    offline: bool = False,
    test_fuzz_corpora: bool = True,
) -> list[Task]:
    """Create shared test infrastructure and per-config test creation tasks.

    Returns a list of tasks to demand (completion tasks plus shared infra
    tasks that should start immediately for parallelism).

    If test_fuzz_corpora is False, fuzz corpora regression tests are skipped
    (e.g. when testing an installed build that doesn't include fuzz binaries).
    """
    if not configs:
        return []

    demanded: list[Task] = []

    # Shared: WAST discovery
    discover_task = DiscoverWASTTestsTask(source_dir=source_dir)

    # Shared: WASI SDK download and compile tasks
    wasi_wasm_dir = work_dir / "wasi-wasm"
    wasi_sdk_task = DownloadWasiSdkTask(working_dir=work_dir, offline=offline)
    demanded.append(wasi_sdk_task)

    wasi_cpp_dir = source_dir / "Test" / "wasi" / "cpp"
    wasi_compile_tasks: dict[str, CompileWasiTestTask] = {}
    all_test_defs = list(get_tests())
    if LINUX and is_gdb_available():
        all_test_defs.append(GDB_BACKTRACE_TEST)
    for test_def in all_test_defs:
        for stem in test_def.test_wasi_cpp_sources:
            if stem not in wasi_compile_tasks:
                task = CompileWasiTestTask(
                    cpp_file=wasi_cpp_dir / f"{stem}.cpp",
                    output_dir=wasi_wasm_dir,
                    wasi_sdk_task=wasi_sdk_task,
                )
                wasi_compile_tasks[stem] = task
                demanded.append(task)

    # Shared: fuzz corpora (if any config needs it and not excluded)
    fuzz_corpora_task: Optional[CloneFuzzCorporaTask] = None
    if test_fuzz_corpora and any(config.test_fuzz_corpora for config, _, _ in configs):
        fuzz_corpora_task = CloneFuzzCorporaTask(working_dir=work_dir, offline=offline)
        demanded.append(fuzz_corpora_task)

    # Per-config: completion task + test creation task
    for config, build_dir, build_task in configs:
        complete_task = Task(
            f"test_complete_{config.name}",
            f"{config.name}: test completion",
        )

        create_tests_task = CreateTestTasksTask(
            config=config,
            build_dir=build_dir,
            discover_task=discover_task,
            source_dir=source_dir,
            wasi_wasm_dir=wasi_wasm_dir,
            wasi_compile_tasks=wasi_compile_tasks,
            complete_task=complete_task,
            build_wavm_task=build_task,
            fuzz_corpora_task=fuzz_corpora_task,
            work_dir=work_dir,
            llvm_toolchain_task=llvm_toolchain_task,
        )
        complete_task.dependencies.append(create_tests_task)
        demanded.append(complete_task)

    return demanded


# =============================================================================
# Command Handlers
# =============================================================================


def cmd_test(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Build and test WAVM configurations."""
    all_wavm_configs = get_applicable_wavm_configs()

    if not all_wavm_configs:
        output.error("Error: No compilers found")
        return 1

    # Filter configs if specified
    if args.config:
        config_names = set(args.config)
        wavm_configs = [c for c in all_wavm_configs if c.name in config_names]
        if not wavm_configs:
            output.error(f"Error: No matching WAVM configs found for: {args.config}")
            output.error(f"Available: {[c.name for c in all_wavm_configs]}")
            return 1
    else:
        wavm_configs = all_wavm_configs

    # Set up test history
    load_test_history(ctx.work_dir)

    lint_config = get_lint_config()

    # Create build tasks for each config
    test_configs = []
    for wavm_config in wavm_configs:
        build_task = create_wavm_build_tasks(
            wavm_config, ctx.work_dir, ctx.source_dir,
            build_llvm_from_source=args.llvm_source,
            offline=args.offline,
        )
        test_configs.append((wavm_config, build_task.build_dir, build_task))

    # Configure the lint config (configure only, not built)
    lint_configure_task = create_wavm_configure_task(
        lint_config, ctx.work_dir, ctx.source_dir,
        build_llvm_from_source=args.llvm_source,
        offline=args.offline,
    )

    # Create check and tidy tasks
    toolchain_task = get_llvm_toolchain_task(
        ctx.work_dir, args.llvm_source, offline=args.offline
    )

    demanded: list[Task] = [
        create_format_tasks(toolchain_task, ctx.source_dir, False),
        *([create_lint_tasks(ctx.source_dir)] if not args.no_lint else []),
        create_tidy_tasks(
            toolchain_task, ctx.source_dir, False,
            configure_task=lint_configure_task,
        ),
        *setup_test_tasks(
            test_configs, ctx.source_dir, ctx.work_dir, toolchain_task, offline=args.offline
        ),
    ]

    success = execute(demanded)
    save_test_history()

    return 0 if success else 1


def register_test(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser("test", help="Build and test WAVM configurations")
    p.add_argument(
        "--config",
        action="append",
        help="Config to build/test (can be repeated; default: all)",
    )
    p.add_argument(
        "--no-lint",
        action="store_true",
        help="Skip Python linting (ty, ruff) even if uvx is available",
    )
    p.set_defaults(func=cmd_test)


def cmd_test_install(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Test a pre-built WAVM installation."""
    install_dir = Path(args.install_dir).resolve()

    # Validate the config name
    wavm_config, all_configs = find_wavm_config(args.config)
    if not wavm_config:
        output.error(f"Error: Config '{args.config}' not found")
        if all_configs:
            output.error(f"Available: {[c.name for c in all_configs]}")
        return 1

    # Validate install directory
    wavm_bin = get_wavm_bin_path(install_dir)
    if not wavm_bin.exists():
        output.error(f"Error: wavm binary not found at {wavm_bin}")
        return 1

    demanded = setup_test_tasks(
        [(wavm_config, install_dir, None)], ctx.source_dir, ctx.work_dir,
        offline=args.offline,
        test_fuzz_corpora=False,
    )

    success = execute(demanded)
    return 0 if success else 1


def register_test_install(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser("test-install", help="Test a pre-built WAVM installation")
    p.add_argument(
        "--config",
        required=True,
        help="Config to test (required)",
    )
    p.add_argument(
        "install_dir",
        help="Directory containing the WAVM installation to test",
    )
    p.set_defaults(func=cmd_test_install)
