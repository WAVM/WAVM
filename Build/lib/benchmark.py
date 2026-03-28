"""Benchmark execution, result collection, and comparison."""

import argparse
import json
import math
import platform
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from .build import build_single_config
from .context import CommandContext
from .output import output
from .platform import EXE_EXT, get_host_arch, get_wavm_bin_path, run_command
from .task_graph import Task, TaskResult, execute
from .test_def import TestDef, TestStep
from .wasi import DownloadWasiSdkTask


# =============================================================================
# Benchmark Program Definitions
# =============================================================================


@dataclass
class BenchmarkProgram:
    """Definition of a benchmark program."""

    name: str
    path: str  # Relative to WAVM source dir
    args: list[str] = field(default_factory=list)  # Arguments passed to the WASI program
    wavm_run_args: list[str] = field(default_factory=list)  # Extra args for `wavm run`
    expected_output: Optional[str] = None  # Regex matched against test output (re.search)
    expected_binary_output_sha256: Optional[str] = None  # SHA-256 hex digest of expected stdout bytes


_CLBG = "ThirdParty/Benchmarks/ComputerLanguageBenchmarksGame"

_BLAKE2B_HASH = (
    "be4b115872d4373ef8c1b2f40f53680b33aa6c553dc9e0737c4ce3c3cbe0e34b"
    "0df31acb59a4d6492981c6a6c6a6f49f595bce7b7edf45bd4a08410b847f34ab"
)

BENCHMARK_PROGRAMS: list[BenchmarkProgram] = [
    BenchmarkProgram(name="blake2b", path="Benchmarks/blake2b.wast",
                     wavm_run_args=["--abi=wasi"],
                     expected_output=_BLAKE2B_HASH),
    BenchmarkProgram(name="blake2b-memory64", path="Benchmarks/blake2b-memory64.wast",
                     wavm_run_args=["--abi=wasi", "--enable", "memory64"],
                     expected_output=_BLAKE2B_HASH),
    BenchmarkProgram(name="zlib", path="Benchmarks/zlib.wasm",
                     expected_output=r"sizes: 100000,25906\nok\."),
    BenchmarkProgram(name="coremark", path="Benchmarks/coremark.wasm",
                     args=["0", "0", "0x66", "20000"],
                     expected_output=r"seedcrc\s+: 0xe9f5"),
    BenchmarkProgram(name="fannkuch-redux", path="Benchmarks/fannkuch-redux.wasm",
                     args=["10"],
                     expected_output=r"Pfannkuchen\(10\) = 38"),
    BenchmarkProgram(name="nbody", path="Benchmarks/nbody.wasm",
                     args=["5000000"],
                     expected_output=r"-0\.169075164\n-0\.169083134"),
    BenchmarkProgram(name="spectral-norm", path="Benchmarks/spectral-norm.wasm",
                     args=["2000"],
                     expected_output=r"1\.274224152"),
    BenchmarkProgram(name="mandelbrot", path="Benchmarks/mandelbrot.wasm",
                     args=["4000"],
                     expected_binary_output_sha256="d7c903ec07c9bbbb7747b58cd1f174d7ba88b61ad172fb306288bd0fe9140a07"),
    BenchmarkProgram(name="fasta", path="Benchmarks/fasta.wasm",
                     args=["2500000"],
                     expected_output=r">ONE Homo sapiens alu"),
    BenchmarkProgram(name="binary-trees", path="Benchmarks/binary-trees.wasm",
                     args=["17"],
                     expected_output=r"long lived tree of depth 17\s+check: 262143"),
]


def get_benchmark_test_defs() -> list[TestDef]:
    """Generate TestDefs for running benchmark programs as smoke tests."""
    defs = []
    for p in BENCHMARK_PROGRAMS:
        command = ["{wavm_bin}", "run", *p.wavm_run_args,
                   "{source_dir}/" + p.path, *p.args]
        safe_name = p.name.replace("-", "_")
        if p.expected_binary_output_sha256:
            step = TestStep(command=command,
                            expected_binary_output_sha256=p.expected_binary_output_sha256)
        elif p.expected_output:
            step = TestStep(command=command, expected_output=p.expected_output)
        else:
            step = TestStep(command=command)
        defs.append(
            TestDef(
                name=f"Benchmarks_{safe_name}",
                steps=[step],
            )
        )
    return defs


# =============================================================================
# Benchmark Source Compilation
# =============================================================================


@dataclass
class BenchmarkSource:
    """A compilable benchmark, potentially with multiple source files."""

    name: str
    src_paths: list[str]  # Relative to workspace root
    wasm_path: str  # Relative to WAVM source dir
    include_dirs: list[str] = field(default_factory=list)  # Relative to workspace root
    extra_flags: list[str] = field(default_factory=list)


def _clbg_source(name: str, extra_flags: Optional[list[str]] = None) -> BenchmarkSource:
    """Helper for single-file Computer Language Benchmarks Game sources."""
    return BenchmarkSource(
        name=name,
        src_paths=[f"{_CLBG}/{name}/{name}.c"],
        wasm_path=f"Benchmarks/{name}.wasm",
        extra_flags=extra_flags or [],
    )


BENCHMARK_SOURCES: list[BenchmarkSource] = [
    BenchmarkSource(
        name="coremark",
        src_paths=[
            "ThirdParty/Benchmarks/coremark/core_main.c",
            "ThirdParty/Benchmarks/coremark/core_list_join.c",
            "ThirdParty/Benchmarks/coremark/core_matrix.c",
            "ThirdParty/Benchmarks/coremark/core_state.c",
            "ThirdParty/Benchmarks/coremark/core_util.c",
            "ThirdParty/Benchmarks/coremark/simple/core_portme.c",
        ],
        wasm_path="Benchmarks/coremark.wasm",
        include_dirs=[
            "ThirdParty/Benchmarks/coremark",
            "ThirdParty/Benchmarks/coremark/simple",
        ],
        extra_flags=[
            "-DPERFORMANCE_RUN=1",
            "-DSEED_METHOD=SEED_ARG",
            "-DITERATIONS=0",
            '-DFLAGS_STR="wasi-sdk -O2"',
            "-D_WASI_EMULATED_PROCESS_CLOCKS",
            "-lwasi-emulated-process-clocks",
        ],
    ),
    _clbg_source("fannkuch-redux"),
    _clbg_source("nbody", extra_flags=["-lm"]),
    _clbg_source("spectral-norm", extra_flags=["-lm"]),
    _clbg_source("mandelbrot"),
    _clbg_source("fasta"),
    _clbg_source("binary-trees", extra_flags=["-lm"]),
]


class CompileBenchmarkTask(Task):
    """Compile a benchmark to WASI .wasm (may have multiple source files)."""

    def __init__(
        self,
        bench_src: BenchmarkSource,
        source_dir: Path,
        wasi_sdk_task: DownloadWasiSdkTask,
    ):
        super().__init__(
            name=f"compile_benchmark_{bench_src.name}",
            description=f"Compile benchmark: {bench_src.name}",
            dependencies=[wasi_sdk_task],
        )
        self.bench_src = bench_src
        self.source_dir = source_dir
        self.wasi_sdk_task = wasi_sdk_task

    def run(self) -> TaskResult:
        wasm_file = self.source_dir / self.bench_src.wasm_path
        wasm_file.parent.mkdir(parents=True, exist_ok=True)

        # Resolve source files relative to source_dir
        src_files: list[Path] = []
        for src_rel in self.bench_src.src_paths:
            src_file = self.source_dir / src_rel
            if not src_file.exists():
                return TaskResult(False, f"Source file not found: {src_file}")
            src_files.append(src_file)

        assert self.wasi_sdk_task.wasi_sdk_dir is not None
        wasi_sdk_dir = self.wasi_sdk_task.wasi_sdk_dir
        clang = wasi_sdk_dir / "bin" / f"clang{EXE_EXT}"
        sysroot = wasi_sdk_dir / "share" / "wasi-sysroot"

        cmd = [
            str(clang),
            "--target=wasm32-wasi",
            f"--sysroot={sysroot}",
            "-O2",
            "-o",
            str(wasm_file),
        ]
        for inc_dir in self.bench_src.include_dirs:
            cmd.append(f"-I{self.source_dir / inc_dir}")
        cmd.extend(str(f) for f in src_files)
        cmd.extend(self.bench_src.extra_flags)

        result = run_command(cmd)
        if result.returncode != 0:
            return TaskResult(
                False,
                result.format_failure(f"Failed to compile {self.bench_src.name}"),
            )

        return TaskResult(True)


def setup_benchmark_compile_tasks(
    source_dir: Path,
    work_dir: Path,
    offline: bool = False,
) -> list[Task]:
    """Set up tasks to compile benchmark C sources to .wasm.

    Returns the list of compile tasks.
    """
    wasi_sdk_task = DownloadWasiSdkTask(working_dir=work_dir, offline=offline)

    tasks: list[Task] = []
    for bench_src in BENCHMARK_SOURCES:
        task = CompileBenchmarkTask(
            bench_src=bench_src,
            source_dir=source_dir,
            wasi_sdk_task=wasi_sdk_task,
        )
        tasks.append(task)

    return tasks


# =============================================================================
# Statistics
# =============================================================================


def _mean(values: list[float]) -> float:
    if not values:
        return 0.0
    return sum(values) / len(values)


def _stddev(values: list[float]) -> float:
    if len(values) < 2:
        return 0.0
    m = _mean(values)
    variance = sum((x - m) ** 2 for x in values) / (len(values) - 1)
    return math.sqrt(variance)


# =============================================================================
# Running Benchmarks
# =============================================================================


def _run_file_benchmark_once(
    wavm_bin: Path, source_dir: Path, program: BenchmarkProgram
) -> Optional[dict]:
    """Run a single file benchmark invocation and return parsed results."""
    file_path = source_dir / program.path
    if not file_path.exists():
        output.error(f"Benchmark file not found: {file_path}")
        return None

    cmd = [
        str(wavm_bin),
        "test",
        "benchmark",
        "--json",
        "--enable", "all",
        str(file_path),
        *program.args,
    ]

    start = time.perf_counter()
    result = run_command(cmd, timeout=600)
    wall_clock_ms = (time.perf_counter() - start) * 1000.0

    if result.timed_out:
        output.error(f"Benchmark timed out: {program.name}")
        return None

    if result.returncode != 0:
        output.error(f"Benchmark failed: {program.name}")
        if result.stderr.strip():
            output.error(result.stderr.strip())
        return None

    # The WASI program may output binary data (e.g. mandelbrot PBM) before
    # the JSON result. Decode only the tail of stdout to find the JSON.
    tail = result.stdout_bytes[-4096:].decode("utf-8", errors="replace")
    json_text = _extract_json_from_output(tail)
    if json_text is None:
        output.error(f"No JSON found in output from {program.name}")
        return None

    try:
        data = json.loads(json_text)
    except json.JSONDecodeError:
        output.error(f"Failed to parse JSON output from {program.name}")
        return None

    phases = data.get("phases", {})
    return {
        "load_ms": phases.get("load_ms", 0.0),
        "compile_ms": phases.get("compile_ms", 0.0),
        "instantiate_ms": phases.get("instantiate_ms", 0.0),
        "execute_ms": phases.get("execute_ms", 0.0),
        "wall_clock_ms": wall_clock_ms,
        "peak_memory_bytes": data.get("peak_memory_bytes", 0),
    }


def _extract_json_from_output(text: str) -> Optional[str]:
    """Extract the last JSON object from stdout.

    The benchmark program's own stdout (e.g. from WASI programs) may precede
    the JSON result. Find the last '{' ... '}' block.
    """
    # Find the last closing brace
    end = text.rfind("}")
    if end < 0:
        return None
    # Find the matching opening brace
    depth = 0
    for i in range(end, -1, -1):
        if text[i] == "}":
            depth += 1
        elif text[i] == "{":
            depth -= 1
        if depth == 0:
            return text[i : end + 1]
    return None


NUM_RUNS = 10


# =============================================================================
# Result I/O
# =============================================================================


def _get_results_dir(work_dir: Path) -> Path:
    return work_dir / "benchmark_results"


def save_results(work_dir: Path, name: str, results: dict) -> Path:
    """Save benchmark results to a JSON file."""
    results_dir = _get_results_dir(work_dir)
    results_dir.mkdir(parents=True, exist_ok=True)
    path = results_dir / f"{name}.json"
    path.write_text(json.dumps(results, indent=2))
    output.print(f"Results saved to {path}")
    return path


def load_results(work_dir: Path, name: str) -> Optional[dict]:
    """Load benchmark results from a JSON file."""
    path = _get_results_dir(work_dir) / f"{name}.json"
    if not path.exists():
        output.error(f"Baseline not found: {path}")
        return None
    try:
        return json.loads(path.read_text())
    except (json.JSONDecodeError, IOError) as e:
        output.error(f"Failed to load baseline: {e}")
        return None


def list_saved_results(work_dir: Path) -> list[str]:
    """List saved benchmark result names."""
    results_dir = _get_results_dir(work_dir)
    if not results_dir.exists():
        return []
    return sorted(p.stem for p in results_dir.glob("*.json"))


# =============================================================================
# Display and Comparison
# =============================================================================


def _format_val(mean: float, std: float, unit: str) -> str:
    if mean >= 100:
        return f"{mean:.1f} +/- {std:.1f}{unit}"
    elif mean >= 1:
        return f"{mean:.2f} +/- {std:.2f}{unit}"
    else:
        return f"{mean:.3f} +/- {std:.3f}{unit}"


def _format_delta(delta: float, pct: float, unit: str) -> str:
    sign = "+" if delta >= 0 else ""
    return f"{sign}{delta:.1f}{unit}  {sign}{pct:.1f}%"


def _is_regression(pct: float) -> bool:
    return pct > 2.0


def _is_improvement(pct: float) -> bool:
    return pct < -2.0


def _ansi_red(s: str) -> str:
    return f"\033[31m{s}\033[0m"


def _ansi_green(s: str) -> str:
    return f"\033[32m{s}\033[0m"


def _colorize_delta(delta_str: str, pct: float, interactive: bool) -> str:
    if not interactive:
        return delta_str
    if _is_regression(pct):
        return _ansi_red(delta_str)
    elif _is_improvement(pct):
        return _ansi_green(delta_str)
    return delta_str


METRICS = [
    ("load_ms", "Load (ms)", "ms", 1.0),
    ("compile_ms", "Compile (ms)", "ms", 1.0),
    ("instantiate_ms", "Instantiate (ms)", "ms", 1.0),
    ("execute_ms", "Execute (ms)", "ms", 1.0),
    ("peak_memory_bytes", "Peak RSS (MiB)", "MiB", 1.0 / (1024.0 * 1024.0)),
]


def _geometric_mean(values: list[float]) -> float:
    """Geometric mean, appropriate for aggregating times across different scales."""
    if not values or any(v <= 0 for v in values):
        return 0.0
    log_sum = sum(math.log(v) for v in values)
    return math.exp(log_sum / len(values))


def _per_run_geometric_means(
    benchmarks: dict, phase_key: str, bench_names: list[str]
) -> list[float]:
    """Compute the geometric mean across benchmarks for each run index."""
    num_runs = min(
        len(benchmarks[name].get("iterations", []))
        for name in bench_names
        if name in benchmarks
    )
    geo_means: list[float] = []
    for run_idx in range(num_runs):
        values: list[float] = []
        for name in bench_names:
            iters = benchmarks[name].get("iterations", [])
            if run_idx < len(iters) and phase_key in iters[run_idx]:
                v = iters[run_idx][phase_key]
                if v > 0:
                    values.append(v)
        if len(values) == len(bench_names):
            geo_means.append(_geometric_mean(values))
    return geo_means


def print_results(results: dict) -> None:
    """Print results as one table per metric, with per-run geometric mean aggregate."""
    benchmarks = results.get("benchmarks", {})
    if not benchmarks:
        output.print("No benchmark results.")
        return

    bench_names = sorted(benchmarks.keys())

    for metric_key, metric_label, unit, scale in METRICS:
        rows: list[tuple[str, float, float]] = []
        for name in bench_names:
            iterations = benchmarks[name].get("iterations", [])
            values = [it[metric_key] * scale for it in iterations if metric_key in it]
            if values:
                rows.append((name, _mean(values), _stddev(values)))

        if not rows:
            continue

        output.print(f"\n{metric_label}")
        output.print(f"  {'Benchmark':<25} {'Mean +/- StdDev':<30}")
        output.print(f"  {'-' * 55}")
        for name, m, s in rows:
            output.print(f"  {name:<25} {_format_val(m, s, unit)}")

        geo_means = _per_run_geometric_means(benchmarks, metric_key, bench_names)
        if geo_means:
            scaled = [v * scale for v in geo_means]
            gm = _mean(scaled)
            gs = _stddev(scaled)
            output.print(f"  {'(geometric mean)':<25} {_format_val(gm, gs, unit)}")

    output.print("")


def print_comparison(current: dict, baseline: dict) -> None:
    """Print a comparison table between current and baseline results, one table per metric."""
    interactive = output.interactive

    cur_benchmarks = current.get("benchmarks", {})
    base_benchmarks = baseline.get("benchmarks", {})

    all_names = sorted(set(cur_benchmarks.keys()) | set(base_benchmarks.keys()))
    if not all_names:
        output.print("No benchmarks to compare.")
        return

    for metric_key, metric_label, unit, scale in METRICS:
        rows: list[tuple[str, list[float], list[float]]] = []
        for name in all_names:
            cur_iters = cur_benchmarks.get(name, {}).get("iterations", [])
            base_iters = base_benchmarks.get(name, {}).get("iterations", [])
            cur_values = [it[metric_key] * scale for it in cur_iters if metric_key in it]
            base_values = [it[metric_key] * scale for it in base_iters if metric_key in it]
            if cur_values and base_values:
                rows.append((name, cur_values, base_values))

        if not rows:
            continue

        output.print(f"\n{metric_label}")
        output.print(
            f"  {'Benchmark':<20} {'Current (mean +/- std)':<28} "
            f"{'Baseline (mean +/- std)':<28} {'Delta':<20}"
        )
        output.print(f"  {'-' * 96}")
        for name, cur_values, base_values in rows:
            _print_comparison_row(name, cur_values, base_values, unit, interactive)

        common_names = [name for name, _, _ in rows]
        cur_geos = _per_run_geometric_means(cur_benchmarks, metric_key, common_names)
        base_geos = _per_run_geometric_means(base_benchmarks, metric_key, common_names)
        if cur_geos and base_geos:
            _print_comparison_row(
                "(geometric mean)",
                [v * scale for v in cur_geos],
                [v * scale for v in base_geos],
                unit,
                interactive,
            )

    output.print("")


def _print_comparison_row(
    bench_name: str,
    cur_values: list[float],
    base_values: list[float],
    unit: str,
    interactive: bool,
) -> None:
    cur_mean = _mean(cur_values)
    cur_std = _stddev(cur_values)
    base_mean = _mean(base_values)
    base_std = _stddev(base_values)

    delta = cur_mean - base_mean
    pct = (delta / base_mean * 100.0) if base_mean != 0 else 0.0

    cur_str = _format_val(cur_mean, cur_std, unit)
    base_str = _format_val(base_mean, base_std, unit)
    delta_str = _format_delta(delta, pct, unit)
    delta_str = _colorize_delta(delta_str, pct, interactive)

    output.print(f"  {bench_name:<20} {cur_str:<28} {base_str:<28} {delta_str}")


# =============================================================================
# Running All Benchmarks
# =============================================================================


def _get_wavm_version(source_dir: Path) -> str:
    version_file = source_dir / "VERSION"
    if version_file.exists():
        return version_file.read_text().strip()
    return "unknown"


def run_benchmarks(
    build_dir: Path,
    source_dir: Path,
    config_name: str,
    benchmark_names: Optional[list[str]] = None,
) -> dict:
    """Run all benchmarks serially, with the same number of runs for each.

    Each "run" executes every benchmark once, serially, to avoid CPU contention.
    This ensures per-run geometric means are computed from the same set of
    benchmarks measured under the same conditions.
    """
    wavm_bin = get_wavm_bin_path(build_dir)
    if not wavm_bin.exists():
        raise RuntimeError(f"wavm binary not found: {wavm_bin}")

    programs = list(BENCHMARK_PROGRAMS)
    if benchmark_names:
        name_set = set(benchmark_names)
        programs = [p for p in programs if p.name in name_set]
        if not programs:
            available = [p.name for p in BENCHMARK_PROGRAMS]
            raise RuntimeError(
                f"No matching benchmarks for: {benchmark_names}. "
                f"Available: {available}"
            )

    # Filter out programs whose files don't exist
    valid_programs: list[BenchmarkProgram] = []
    for program in programs:
        file_path = source_dir / program.path
        if not file_path.exists():
            output.error(f"Skipping {program.name}: file not found ({file_path})")
        else:
            valid_programs.append(program)

    if not valid_programs:
        raise RuntimeError("No benchmark files found")

    # Run all benchmarks NUM_RUNS times in lockstep
    all_iterations: dict[str, list[dict]] = {p.name: [] for p in valid_programs}

    for run_index in range(NUM_RUNS):
        for program in valid_programs:
            output.status(f"Run {run_index + 1}/{NUM_RUNS}: {program.name}")
            result = _run_file_benchmark_once(wavm_bin, source_dir, program)
            if result is None:
                output.error(f"  {program.name}: FAILED")
                all_iterations[program.name].append({})
            else:
                all_iterations[program.name].append(result)

    output.clear_status()

    results: dict = {
        "version": 1,
        "num_runs": NUM_RUNS,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "host": {
            "os": platform.system().lower(),
            "arch": get_host_arch(),
        },
        "wavm": {
            "version": _get_wavm_version(source_dir),
            "config": config_name,
        },
        "benchmarks": {},
    }

    for program in valid_programs:
        iterations = [it for it in all_iterations[program.name] if it]
        if iterations:
            results["benchmarks"][program.name] = {"iterations": iterations}

    return results


# =============================================================================
# Command Handlers
# =============================================================================


def cmd_benchmark(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Run benchmarks, save results, and compare against baselines."""
    # List mode: show available benchmarks and saved baselines
    if args.list:
        output.print("Available benchmarks:")
        for p in BENCHMARK_PROGRAMS:
            output.print(f"  {p.name:<20} {p.path}")
        saved = list_saved_results(ctx.work_dir)
        if saved:
            output.print("\nSaved baselines:")
            for name in saved:
                output.print(f"  {name}")
        return 0

    config_name = args.config

    # Build WAVM
    build_dir, err = build_single_config(
        ctx.work_dir, ctx.source_dir, config_name,
        llvm_source=args.llvm_source, offline=args.offline,
        suppress_summary=True,
    )
    if err is not None:
        return err
    assert build_dir is not None

    # Run benchmarks serially
    benchmark_names = args.benchmark if args.benchmark else None
    results = run_benchmarks(
        build_dir=build_dir,
        source_dir=ctx.source_dir,
        config_name=config_name,
        benchmark_names=benchmark_names,
    )
    print_results(results)

    # Save if requested
    if args.save:
        save_results(ctx.work_dir, args.save, results)

    # Compare if requested
    if args.compare:
        baseline = load_results(ctx.work_dir, args.compare)
        if baseline:
            print_comparison(results, baseline)
        else:
            return 1

    return 0


def cmd_benchmark_compile(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Compile benchmark C sources to .wasm using WASI SDK."""
    tasks = setup_benchmark_compile_tasks(
        source_dir=ctx.source_dir,
        work_dir=ctx.work_dir,
        offline=args.offline,
    )

    if not execute(tasks):
        return 1

    output.print("Benchmark .wasm files compiled successfully.")
    for src in BENCHMARK_SOURCES:
        wasm_path = ctx.source_dir / src.wasm_path
        output.print(f"  {wasm_path}")
    return 0


# =============================================================================
# Argparse Registration
# =============================================================================


def register_benchmark_commands(subparsers: argparse._SubParsersAction) -> None:
    """Register benchmark and benchmark-compile subcommands."""
    benchmark_parser = subparsers.add_parser("benchmark", help="Run performance benchmarks")
    benchmark_parser.add_argument(
        "--config",
        default="LTO",
        help="Configuration to build and benchmark (default: LTO)",
    )
    benchmark_parser.add_argument(
        "--save",
        metavar="NAME",
        help="Save results with the given name",
    )
    benchmark_parser.add_argument(
        "--compare",
        metavar="NAME",
        help="Compare results against a saved baseline",
    )
    benchmark_parser.add_argument(
        "--list",
        action="store_true",
        help="List available benchmarks and saved baselines",
    )
    benchmark_parser.add_argument(
        "--benchmark",
        action="append",
        metavar="NAME",
        help="Run only the named benchmark(s) (can be repeated; default: all)",
    )
    benchmark_parser.set_defaults(func=cmd_benchmark)

    subparsers.add_parser(
        "benchmark-compile",
        help="Compile benchmark C sources to .wasm using WASI SDK",
    ).set_defaults(func=cmd_benchmark_compile)
