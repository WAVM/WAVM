"""LibFuzzer fuzzing campaign management."""

import argparse
import os
import shutil
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from .context import CommandContext
from .output import output
from .platform import EXE_EXT, run_command
from .build import WAVMConfig, create_wavm_build_tasks
from .toolchain import get_primary_toolchain
from .task_graph import execute


# =============================================================================
# Fuzzer Configuration
# =============================================================================


@dataclass
class FuzzerConfig:
    """Per-fuzzer libFuzzer configuration."""

    name: str
    max_total_time: int  # seconds
    max_len: int  # max input size in bytes
    seed_dirs: list[str] = field(default_factory=list)  # relative to fuzz_dir
    dict_file: Optional[str] = None  # relative to WAVM/Test/fuzz/
    rss_limit_mb: Optional[int] = None
    reduce_inputs: Optional[int] = None


FUZZER_CONFIGS: dict[str, FuzzerConfig] = {
    "assemble": FuzzerConfig(
        name="assemble",
        max_total_time=600,
        max_len=50000,
        seed_dirs=["seed-corpus/wast", "translated/wast"],
        dict_file="wastFuzzDictionary.txt",
        rss_limit_mb=2750,
    ),
    "disassemble": FuzzerConfig(
        name="disassemble",
        max_total_time=600,
        max_len=5000,
        seed_dirs=["seed-corpus/wasm", "translated/wasm"],
    ),
    "compile-model": FuzzerConfig(
        name="compile-model",
        max_total_time=600,
        max_len=20,
        reduce_inputs=0,
    ),
    "instantiate": FuzzerConfig(
        name="instantiate",
        max_total_time=600,
        max_len=5000,
        seed_dirs=["seed-corpus/wasm", "translated/wasm"],
    ),
}

DEFAULT_TARGET_ORDER = ["assemble", "disassemble", "compile-model", "instantiate"]


# =============================================================================
# Build
# =============================================================================


def get_libfuzzer_config() -> WAVMConfig:
    """Construct the WAVMConfig for a libFuzzer-instrumented build."""
    return WAVMConfig(
        name="Fuzz",
        base_name="Fuzz",
        toolchain=get_primary_toolchain("clang"),
        build_type="RelWithDebInfo",
        llvm_config="Sanitized",
        cmake_args=[
            "-DWAVM_ENABLE_LIBFUZZER=ON",
            "-DWAVM_ENABLE_ASAN=ON",
            "-DWAVM_ENABLE_UBSAN=ON",
            "-DWAVM_ENABLE_STATIC_LINKING=ON",
            "-DWAVM_ENABLE_RELEASE_ASSERTS=ON",
            "-DWAVM_ENABLE_FUZZ_TARGETS=ON",
        ],
        uses_asan=True,
    )


def build_libfuzzer(
    work_dir: Path,
    source_dir: Path,
    llvm_source: bool,
    offline: bool,
) -> Path:
    """Build WAVM with libFuzzer instrumentation. Returns the build directory."""
    config = get_libfuzzer_config()
    try:
        build_task = create_wavm_build_tasks(
            config,
            work_dir,
            source_dir,
            build_llvm_from_source=llvm_source,
            offline=offline,
        )
    except ValueError as e:
        raise RuntimeError(str(e)) from e
    if not execute([build_task], suppress_summary=True):
        raise RuntimeError("Fuzz build failed")
    return build_task.build_dir


# =============================================================================
# Seed Corpus Generation
# =============================================================================


def generate_seed_corpus(
    build_dir: Path,
    source_dir: Path,
    fuzz_dir: Path,
) -> None:
    """Generate seed corpora from WAVM test files.

    Replaces generate-seed-corpus.sh.
    """
    output.status("Generating seed corpus...")

    wavm_bin = build_dir / "bin" / f"wavm{EXE_EXT}"

    wast_seed_dir = fuzz_dir / "seed-corpus" / "wast"
    wasm_seed_dir = fuzz_dir / "seed-corpus" / "wasm"

    # Clean and recreate seed directories
    for seed_dir in (wast_seed_dir, wasm_seed_dir):
        if seed_dir.exists():
            shutil.rmtree(seed_dir)
        seed_dir.mkdir(parents=True)

    # Find all .wast test files, excluding problematic ones
    test_dir = source_dir / "Test"
    excluded = {"skip-stack-guard-page.wast", "br_table.wast"}
    wast_files = sorted(
        f for f in test_dir.rglob("*.wast") if f.name not in excluded
    )

    for wast_file in wast_files:
        for fmt, out_dir in [("--wast", wast_seed_dir), ("--wasm", wasm_seed_dir)]:
            result = run_command(
                [wavm_bin, "test", "dumpmodules", fmt, "--output-dir", out_dir, wast_file]
            )
            if result.returncode != 0:
                output.verbose(f"dumpmodules {fmt} failed for {wast_file.name}: {result.stderr}")

    output.clear_status()
    output.print(f"Seed corpus generated in {fuzz_dir / 'seed-corpus'}")


# =============================================================================
# Fuzzer Execution
# =============================================================================


def _count_files(dir_path: Path) -> int:
    """Count files in a directory (non-recursive)."""
    if not dir_path.exists():
        return 0
    return sum(1 for f in dir_path.iterdir() if f.is_file())


def run_fuzzer(
    fc: FuzzerConfig,
    build_dir: Path,
    fuzz_dir: Path,
    source_dir: Path,
    jobs: int,
    workers: int,
    max_total_time_override: Optional[int] = None,
    use_fork: bool = False,
) -> None:
    """Run a single fuzzer. Replaces run-fuzzer.sh + run-fuzz-*.sh."""
    corpus_dir = fuzz_dir / "corpora" / fc.name
    artifact_dir = fuzz_dir / "artifacts" / fc.name
    logs_dir = fuzz_dir / "logs" / fc.name
    corpus_dir.mkdir(parents=True, exist_ok=True)
    artifact_dir.mkdir(parents=True, exist_ok=True)
    logs_dir.mkdir(parents=True, exist_ok=True)

    fuzzer_bin = build_dir / "bin" / f"fuzz-{fc.name}{EXE_EXT}"

    cmd: list[str] = [str(fuzzer_bin), str(corpus_dir)]

    # Add seed directories (read-only corpus dirs for libFuzzer)
    for seed_rel in fc.seed_dirs:
        seed_path = fuzz_dir / seed_rel
        if seed_path.exists():
            cmd.append(str(seed_path))

    # Common libFuzzer flags
    cmd.extend([
        f"-artifact_prefix={artifact_dir}/",
        "-use_value_profile=1",
        "-reduce_depth=1",
        "-shrink=1",
    ])

    # Time and size limits
    max_time = max_total_time_override if max_total_time_override is not None else fc.max_total_time
    cmd.append(f"-max_total_time={max_time}")
    cmd.append(f"-max_len={fc.max_len}")

    # Per-fuzzer overrides
    if fc.dict_file:
        dict_path = source_dir / "Test" / "fuzz" / fc.dict_file
        cmd.append(f"-dict={dict_path}")

    if fc.rss_limit_mb is not None:
        cmd.append(f"-rss_limit_mb={fc.rss_limit_mb}")

    if fc.reduce_inputs is not None:
        cmd.append(f"-reduce_inputs={fc.reduce_inputs}")

    # Parallelism mode
    if use_fork:
        cmd.extend([
            f"-fork={jobs}",
            "-ignore_crashes=1",
            "-ignore_timeouts=1",
            "-ignore_ooms=1",
        ])
    else:
        cmd.extend([
            f"-jobs={jobs}",
            f"-workers={workers}",
        ])

    # Run with output suppressed; use cwd=logs_dir so -jobs log files don't pollute CWD.
    proc = subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=logs_dir,
    )
    start = time.monotonic()
    try:
        while proc.poll() is None:
            elapsed = int(time.monotonic() - start)
            minutes, seconds = divmod(elapsed, 60)
            corpus_count = _count_files(corpus_dir)
            artifact_count = _count_files(artifact_dir)
            output.status(
                f"fuzz {fc.name}: {minutes}:{seconds:02d},"
                f" {corpus_count} corpus, {artifact_count} artifacts"
            )
            # Sleep in small increments so we notice process exit promptly
            for _ in range(50):
                if proc.poll() is not None:
                    break
                time.sleep(0.1)
    except KeyboardInterrupt:
        proc.terminate()
        proc.wait()
        raise
    finally:
        output.clear_status()


def translate_compile_model_corpus(build_dir: Path, fuzz_dir: Path) -> None:
    """Translate compile-model corpus to WASM/WAST files.

    Replaces the translation step from run-all-fuzzers.sh.
    """
    corpus_dir = fuzz_dir / "corpora" / "compile-model"
    wasm_dir = fuzz_dir / "translated" / "wasm"
    wast_dir = fuzz_dir / "translated" / "wast"

    corpus_dir.mkdir(parents=True, exist_ok=True)
    for d in (wasm_dir, wast_dir):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)

    output.status("Translating compile-model corpus...")
    translate_bin = build_dir / "bin" / f"translate-compile-model-corpus{EXE_EXT}"
    result = run_command([
        translate_bin,
        corpus_dir,
        wasm_dir,
        wast_dir,
    ])
    output.clear_status()
    if result.returncode != 0:
        output.error(f"translate-compile-model-corpus failed (exit code {result.returncode})")


# =============================================================================
# Corpus Reduction
# =============================================================================


def reduce_corpus(fc: FuzzerConfig, build_dir: Path, fuzz_dir: Path) -> None:
    """Reduce a fuzzer corpus via merge. Replaces reduce-corpus.sh."""
    corpus_dir = fuzz_dir / "corpora" / fc.name
    reduced_dir = fuzz_dir / "corpora" / f"{fc.name}-reduced"
    artifact_dir = fuzz_dir / "artifacts" / fc.name

    if not corpus_dir.exists():
        return

    # Clean and create reduced directory
    if reduced_dir.exists():
        shutil.rmtree(reduced_dir)
    reduced_dir.mkdir(parents=True)
    artifact_dir.mkdir(parents=True, exist_ok=True)

    fuzzer_bin = build_dir / "bin" / f"fuzz-{fc.name}{EXE_EXT}"

    cmd = [
        str(fuzzer_bin),
        str(reduced_dir),
        str(corpus_dir),
        f"-artifact_prefix={artifact_dir}/",
        "-use_value_profile=1",
        "-merge=1",
    ]

    output.status(f"Reducing corpus: {fc.name}")
    run_command(cmd)
    output.clear_status()

    # Replace original corpus with reduced version
    shutil.rmtree(corpus_dir)
    reduced_dir.rename(corpus_dir)


# =============================================================================
# Helpers
# =============================================================================


def _needs_seed_corpus(fuzz_dir: Path) -> bool:
    """Check whether the seed corpus directories are missing."""
    wast_dir = fuzz_dir / "seed-corpus" / "wast"
    wasm_dir = fuzz_dir / "seed-corpus" / "wasm"
    return not wast_dir.exists() or not wasm_dir.exists()


def _resolve_targets(target_list: Optional[list[str]]) -> list[str]:
    """Resolve --target arguments to a list of fuzzer names in execution order."""
    if not target_list:
        return list(DEFAULT_TARGET_ORDER)
    for t in target_list:
        if t not in FUZZER_CONFIGS:
            raise RuntimeError(
                f"Unknown fuzz target: {t}. "
                f"Available: {list(FUZZER_CONFIGS.keys())}"
            )
    # Preserve the canonical order for targets that were requested
    return [t for t in DEFAULT_TARGET_ORDER if t in target_list]


def _snapshot_corpus_counts(fuzz_dir: Path, targets: list[str]) -> dict[str, int]:
    """Snapshot the current corpus file counts for the given targets."""
    return {
        name: _count_files(fuzz_dir / "corpora" / name)
        for name in targets
    }


def _print_final_report(
    fuzz_dir: Path,
    targets: list[str],
    corpus_before: dict[str, int],
) -> None:
    """Print corpus changes and artifact files for the given targets."""
    corpus_after = _snapshot_corpus_counts(fuzz_dir, targets)

    output.print("Corpus:")
    for name in targets:
        before = corpus_before.get(name, 0)
        after = corpus_after.get(name, 0)
        delta = after - before
        sign = "+" if delta > 0 else ""
        output.print(f"  {name}: {before} -> {after} ({sign}{delta})")

    output.print("Artifacts:")
    any_found = False
    for name in targets:
        artifact_dir = fuzz_dir / "artifacts" / name
        if not artifact_dir.exists():
            continue
        for artifact in sorted(artifact_dir.iterdir()):
            if artifact.is_file():
                output.print(f"  {artifact}")
                any_found = True
    if not any_found:
        output.print("  (none)")


# =============================================================================
# Command Handler
# =============================================================================


def cmd_fuzz(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Dispatch fuzz sub-actions (run, seed, reduce)."""
    work_dir = ctx.work_dir
    source_dir = ctx.source_dir
    fuzz_dir = work_dir / "fuzz"

    fuzz_command = args.fuzz_command

    if fuzz_command == "seed":
        build_dir = build_libfuzzer(work_dir, source_dir, args.llvm_source, args.offline)
        generate_seed_corpus(build_dir, source_dir, fuzz_dir)
        return 0

    if fuzz_command == "reduce":
        targets = _resolve_targets(args.target)
        build_dir = build_libfuzzer(work_dir, source_dir, args.llvm_source, args.offline)
        for name in targets:
            reduce_corpus(FUZZER_CONFIGS[name], build_dir, fuzz_dir)
        return 0

    if fuzz_command == "run":
        if args.pull and not args.continuous:
            output.error("--pull requires --continuous")
            return 1

        targets = _resolve_targets(args.target)
        jobs = args.jobs or os.cpu_count() or 1
        workers = args.workers or jobs
        continuous = args.continuous
        # Fork mode by default in continuous; jobs mode by default otherwise
        use_fork = continuous and not args.no_fork

        corpus_before = _snapshot_corpus_counts(fuzz_dir, targets)

        try:
            iteration = 0
            while True:
                iteration += 1
                if continuous:
                    output.print(f"\n{'=' * 60}")
                    output.print(f"Fuzzing iteration {iteration}")
                    output.print(f"{'=' * 60}")

                    if args.pull:
                        output.status("Pulling latest source...")
                        subprocess.run(["git", "pull"], cwd=source_dir, check=True)
                        output.clear_status()

                build_dir = build_libfuzzer(work_dir, source_dir, args.llvm_source, args.offline)

                if not args.skip_seed and _needs_seed_corpus(fuzz_dir):
                    generate_seed_corpus(build_dir, source_dir, fuzz_dir)

                for name in targets:
                    fc = FUZZER_CONFIGS[name]
                    run_fuzzer(
                        fc, build_dir, fuzz_dir, source_dir,
                        jobs, workers,
                        max_total_time_override=args.max_total_time,
                        use_fork=use_fork,
                    )

                    if continuous:
                        reduce_corpus(fc, build_dir, fuzz_dir)

                    # Translate compile-model corpus before running instantiate
                    if name == "compile-model" and "instantiate" in targets:
                        translate_compile_model_corpus(build_dir, fuzz_dir)

                if not continuous:
                    break
        except KeyboardInterrupt:
            output.clear_status()
            output.print("\nInterrupted")

        _print_final_report(fuzz_dir, targets, corpus_before)
        return 0

    output.error(f"Unknown fuzz command: {fuzz_command}")
    return 1


# =============================================================================
# Argparse Registration
# =============================================================================


def register_fuzz_commands(subparsers: argparse._SubParsersAction) -> None:
    """Register the fuzz subcommand and its sub-subparsers."""
    fuzz_parser = subparsers.add_parser("fuzz", help="Run libFuzzer fuzzing campaigns")
    fuzz_subparsers = fuzz_parser.add_subparsers(dest="fuzz_command", required=True)

    fuzz_run_parser = fuzz_subparsers.add_parser("run", help="Build and run fuzzers")
    fuzz_run_parser.add_argument(
        "--target", action="append",
        help="Fuzz target (assemble, disassemble, compile-model, instantiate). Repeatable. Default: all.",
    )
    fuzz_run_parser.add_argument(
        "--max-total-time", type=int,
        help="Override max_total_time for all fuzzers (seconds)",
    )
    fuzz_run_parser.add_argument(
        "--jobs", type=int,
        help="Number of parallel fuzzer jobs (default: CPU count)",
    )
    fuzz_run_parser.add_argument(
        "--workers", type=int,
        help="Number of fuzzer workers (default: CPU count)",
    )
    fuzz_run_parser.add_argument(
        "--skip-seed", action="store_true",
        help="Skip seed corpus generation",
    )
    fuzz_run_parser.add_argument(
        "--continuous", action="store_true",
        help="Loop indefinitely: rebuild, fuzz, reduce corpus, repeat",
    )
    fuzz_run_parser.add_argument(
        "--pull", action="store_true",
        help="Git pull WAVM source between iterations (only with --continuous)",
    )
    fuzz_run_parser.add_argument(
        "--no-fork", action="store_true",
        help="Use -jobs/-workers instead of -fork mode (fork is default in --continuous)",
    )

    fuzz_subparsers.add_parser("seed", help="Generate seed corpus from test files")

    fuzz_reduce_parser = fuzz_subparsers.add_parser("reduce", help="Reduce fuzzer corpus via merge")
    fuzz_reduce_parser.add_argument(
        "--target", action="append",
        help="Fuzz target to reduce. Repeatable. Default: all.",
    )

    fuzz_parser.set_defaults(func=cmd_fuzz)
