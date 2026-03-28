"""Test definition types: TestContext, TestResult, TestStep, TestDef."""

import hashlib
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional, TYPE_CHECKING

from .platform import EXE_EXT, get_wavm_bin_path, run_command

if TYPE_CHECKING:
    from .build import WAVMConfig


@dataclass
class TestContext:
    """Context passed to test run methods."""

    config: "WAVMConfig"
    source_dir: Path
    build_dir: Path  # Build or install directory root
    corpora_dir: Optional[Path]
    use_setarch: bool
    wasi_wasm_dir: Optional[Path] = None
    temp_dir: Optional[Path] = None


@dataclass
class TestResult:
    """Result of running a test."""

    success: bool
    message: str = ""


@dataclass
class TestStep:
    """A single command to run as part of a test."""

    command: list[str] = field(default_factory=list)
    expected_output: Optional[str] = None  # Regex that MUST match (via re.search)
    unexpected_output: Optional[str] = None  # Regex that must NOT match
    expected_returncode: int = 0  # Expected process exit code
    env: dict[str, str] = field(default_factory=dict)  # Extra env vars (values support placeholders)
    asan_env: dict[str, str] = field(default_factory=dict)  # Extra env vars for ASAN builds only
    tsan_env: dict[str, str] = field(default_factory=dict)  # Extra env vars for TSAN builds only
    timeout: int = 600
    name: Optional[str] = None  # Optional label for multi-step error messages
    collects_coverage: bool = True  # Whether to set LLVM_PROFILE_FILE for coverage
    expected_binary_output_sha256: Optional[str] = None  # SHA-256 hex digest of expected stdout bytes

    def run(
        self,
        context: TestContext,
        label: str,
        coverage_profraw_path: Optional[str] = None,
    ) -> TestResult:
        placeholders: dict[str, object] = {
            "wavm_bin": get_wavm_bin_path(context.build_dir),
            "source_dir": context.source_dir,
            "build_dir": context.build_dir,
            "build_type": context.config.build_type,
            "corpora_dir": context.corpora_dir,
            "exe_ext": EXE_EXT,
        }
        if context.wasi_wasm_dir is not None:
            placeholders["wasi_wasm_dir"] = context.wasi_wasm_dir
        if context.temp_dir is not None:
            placeholders["temp_dir"] = context.temp_dir

        # Substitute placeholders in command and env values
        command = [arg.format(**placeholders) for arg in self.command]
        resolved_env = {k: v.format(**placeholders) for k, v in self.env.items()}

        # Prepend setarch if needed
        if context.use_setarch:
            command = ["setarch", "x86_64", "-R"] + command

        # Build test environment: config env + test env + ASAN env (if config uses ASAN)
        test_env = resolved_env
        if context.config.uses_asan:
            test_env.update(self.asan_env)
        if context.config.uses_tsan:
            test_env.update(self.tsan_env)

        # Inject coverage profraw path if provided
        if coverage_profraw_path:
            test_env["LLVM_PROFILE_FILE"] = coverage_profraw_path

        result = run_command(
            command, cwd=context.source_dir, env=test_env, timeout=self.timeout
        )

        if result.timed_out or result.returncode != self.expected_returncode:
            return TestResult(
                False,
                result.format_failure(expected_returncode=self.expected_returncode),
            )

        # Check expected binary output hash if specified
        if self.expected_binary_output_sha256:
            actual = hashlib.sha256(result.stdout_bytes).hexdigest()
            if actual != self.expected_binary_output_sha256:
                return TestResult(
                    False,
                    result.format_failure(
                        f"SHA-256 mismatch: expected {self.expected_binary_output_sha256}, got {actual}"
                    ),
                )

        combined_output = result.stdout + result.stderr

        # Check expected output pattern if specified
        if self.expected_output:
            if not re.search(self.expected_output, combined_output):
                return TestResult(
                    False,
                    result.format_failure(
                        f"Output did not match pattern: {self.expected_output}"
                    ),
                )

        # Check unexpected output pattern if specified
        if self.unexpected_output:
            if re.search(self.unexpected_output, combined_output):
                return TestResult(
                    False,
                    result.format_failure(
                        f"Output matched unexpected pattern: {self.unexpected_output}"
                    ),
                )

        return TestResult(True)


@dataclass
class TestDef:
    """A test consisting of one or more steps."""

    name: str
    steps: list[TestStep] = field(default_factory=list)
    requires_runtime: bool = True  # Whether this test requires the runtime
    requires_shared_build: bool = False  # Whether this test requires a shared (non-static) build
    requires_unsanitized_build: bool = False  # Whether this test cannot run with sanitizers
    create_temp_dir: bool = False  # Whether to create a temporary directory for this test
    test_wasi_cpp_sources: list[str] = field(default_factory=list)  # .cpp stem names to compile with WASI SDK
    requires_fuzz_corpora_and_fuzz_targets: bool = False  # Whether this test needs the fuzz corpora as an input
    requires_cmake_and_ninja: bool = False  # Whether this test needs cmake and ninja to build
