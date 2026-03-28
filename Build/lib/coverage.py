"""Code coverage report generation and LCOV merging utilities."""

import argparse
import shutil
from collections import defaultdict
from pathlib import Path
from typing import Optional, Union

from .context import CommandContext
from .output import output
from .platform import EXE_EXT, WINDOWS, MACOS, run_command
from .task_graph import Task, TaskResult


def find_coverage_tool(tool_name: str, llvm_toolchain_task: Optional[Task]) -> Optional[Path]:
    """Find a coverage tool (llvm-profdata or llvm-cov).

    Checks the LLVM toolchain first, then falls back to the system PATH.
    """
    if llvm_toolchain_task and llvm_toolchain_task.result:
        llvm_install_dir = llvm_toolchain_task.result.artifacts.get("llvm_install_dir")
        if llvm_install_dir:
            tool_path = Path(llvm_install_dir) / "bin" / f"{tool_name}{EXE_EXT}"
            if tool_path.exists():
                return tool_path

    # Fall back to system PATH
    system_tool = shutil.which(f"{tool_name}{EXE_EXT}")
    if system_tool:
        return Path(system_tool)

    return None


def find_instrumented_binaries(build_dir: Path) -> list[Path]:
    """Find instrumented binaries for coverage reporting.

    Returns the wavm binary and (if present) the shared WAVM library.

    In a CMake build directory, shared libraries are in the build root
    (e.g. build_dir/libWAVM.so), except on Windows where DLLs go to bin/
    via CMAKE_RUNTIME_OUTPUT_DIRECTORY.
    """
    binaries = []

    wavm_bin = build_dir / "bin" / f"wavm{EXE_EXT}"
    if wavm_bin.exists():
        binaries.append(wavm_bin)

    # Check for shared library in the build directory.
    # CMake may create versioned names (libWAVM.so.0.0.0) with symlinks.
    if WINDOWS:
        candidates = [build_dir / "bin" / "libWAVM.dll"]
    elif MACOS:
        candidates = [build_dir / "libWAVM.dylib"]
    else:
        candidates = [build_dir / "libWAVM.so"]

    for lib_path in candidates:
        if lib_path.exists():
            binaries.append(lib_path)

    return binaries


class CoverageReportTask(Task):
    """Generate a coverage report from profraw files produced by tests."""

    def __init__(
        self,
        config_name: str,
        build_dir: Path,
        coverage_dir: Path,
        test_names: list[str],
        llvm_toolchain_task: Optional[Task],
        work_dir: Path,
        source_dir: Path,
        test_tasks: list[Task],
    ):
        super().__init__(
            name=f"coverage_report_{config_name}",
            description=f"{config_name}: coverage report",
            dependencies=test_tasks,
        )
        self.config_name = config_name
        self.build_dir = build_dir
        self.coverage_dir = coverage_dir
        self.test_names = test_names
        self.llvm_toolchain_task = llvm_toolchain_task
        self.work_dir = work_dir
        self.source_dir = source_dir

    def run(self) -> TaskResult:
        # Collect all profraw files
        profraw_files = sorted(self.coverage_dir.glob("*.profraw"))
        if not profraw_files:
            return TaskResult(False, "No .profraw files found")

        # Find tools
        profdata_tool = find_coverage_tool("llvm-profdata", self.llvm_toolchain_task)
        if not profdata_tool:
            return TaskResult(False, "llvm-profdata not found")

        cov_tool = find_coverage_tool("llvm-cov", self.llvm_toolchain_task)
        if not cov_tool:
            return TaskResult(False, "llvm-cov not found")

        # Find instrumented binaries
        binaries = find_instrumented_binaries(self.build_dir)
        if not binaries:
            return TaskResult(False, "No instrumented binaries found")

        # Merge profraw files using a response file to avoid command-line
        # length limits on Windows.
        merged_profdata = self.coverage_dir / "merged.profdata"
        response_file = self.coverage_dir / "profraw_files.txt"
        response_file.write_text("\n".join(str(f) for f in profraw_files) + "\n")
        merge_cmd: list[Union[str, Path]] = [
            profdata_tool, "merge", "-sparse",
            f"@{response_file}",
            "-o", merged_profdata,
        ]
        result = run_command(merge_cmd)
        if result.returncode != 0:
            return TaskResult(False, result.format_failure("llvm-profdata merge failed"))

        # Build binary args for llvm-cov (first binary is main, rest are -object args)
        binary_args: list[Union[str, Path]] = [binaries[0]]
        for extra_binary in binaries[1:]:
            binary_args.extend(["-object", extra_binary])

        source_dir_str = str(self.source_dir).replace("\\", "/")

        # Common args for llvm-cov: -compilation-dir resolves relative paths
        # (produced by -ffile-prefix-map at compile time) back to absolute paths
        # for source file lookup.
        compilation_dir_arg = f"-compilation-dir={source_dir_str}"

        # Clean old report directories before generating new ones
        report_dir = self.work_dir / "coverage-report"
        text_report_dir = self.work_dir / "coverage-text"
        for d in [report_dir, text_report_dir]:
            if d.exists():
                shutil.rmtree(d)

        # Generate HTML report
        show_cmd: list[Union[str, Path]] = [
            cov_tool, "show",
            *binary_args,
            f"-instr-profile={merged_profdata}",
            compilation_dir_arg,
            "-format=html",
            f"-output-dir={report_dir}",
            "-show-line-counts-or-regions",
            self.source_dir,
        ]
        result = run_command(show_cmd, timeout=300)
        if result.returncode != 0:
            return TaskResult(False, result.format_failure("llvm-cov show failed"))

        # Generate LCOV file
        lcov_path = self.work_dir / "coverage.lcov"
        export_cmd: list[Union[str, Path]] = [
            cov_tool, "export",
            *binary_args,
            f"-instr-profile={merged_profdata}",
            compilation_dir_arg,
            "-format=lcov",
            self.source_dir,
        ]
        result = run_command(export_cmd, timeout=300)
        if result.returncode != 0:
            return TaskResult(False, result.format_failure("llvm-cov export failed"))

        lcov_path.write_text(result.stdout)

        # Generate text report (annotated source with per-line hit counts)
        text_cmd: list[Union[str, Path]] = [
            cov_tool, "show",
            *binary_args,
            f"-instr-profile={merged_profdata}",
            compilation_dir_arg,
            "-format=text",
            f"-output-dir={text_report_dir}",
            "-show-line-counts-or-regions",
            self.source_dir,
        ]
        result = run_command(text_cmd, timeout=300)
        if result.returncode != 0:
            return TaskResult(False, result.format_failure("llvm-cov show (text) failed"))

        output.print(f"Coverage HTML report: {report_dir}")
        output.print(f"Coverage text report: {text_report_dir}")
        output.print(f"Coverage LCOV file: {lcov_path}")

        return TaskResult(True, artifacts={
            "report_dir": report_dir,
            "text_report_dir": text_report_dir,
            "lcov_path": lcov_path,
        })


def merge_lcov_files(lcov_paths: list[Path], output_path: Path) -> None:
    """Merge multiple LCOV files into one, summing hit counts for shared lines.

    LCOV format:
        SF:<source file path>
        DA:<line number>,<hit count>
        end_of_record
    """
    # file_path -> {line_number -> total_hit_count}
    file_data: dict[str, dict[int, int]] = defaultdict(lambda: defaultdict(int))

    for lcov_path in lcov_paths:
        current_file: Optional[str] = None
        for line in lcov_path.read_text().splitlines():
            if line.startswith("SF:"):
                current_file = line[3:]
            elif line.startswith("DA:") and current_file is not None:
                parts = line[3:].split(",", 1)
                if len(parts) == 2:
                    try:
                        line_num = int(parts[0])
                        hit_count = int(parts[1])
                        file_data[current_file][line_num] += hit_count
                    except ValueError:
                        pass
            elif line == "end_of_record":
                current_file = None

    # Write merged output
    lines = []
    for source_file in sorted(file_data.keys()):
        lines.append(f"SF:{source_file}")
        for line_num in sorted(file_data[source_file].keys()):
            lines.append(f"DA:{line_num},{file_data[source_file][line_num]}")
        lines.append("end_of_record")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n")


# =============================================================================
# Command Handler
# =============================================================================


def cmd_merge_coverage(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Merge multiple LCOV coverage files into one."""
    lcov_paths = [Path(p).resolve() for p in args.lcov_files]
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    # Validate inputs
    for p in lcov_paths:
        if not p.exists():
            output.error(f"Error: LCOV file not found: {p}")
            return 1

    combined_lcov = output_dir / "combined-coverage.lcov"
    merge_lcov_files(lcov_paths, combined_lcov)
    output.print(f"Merged LCOV: {combined_lcov}")

    # Generate HTML report if genhtml is available
    if shutil.which("genhtml"):
        report_dir = output_dir / "combined-coverage-report"
        result = run_command(
            ["genhtml", str(combined_lcov), "--output-directory", str(report_dir)],
            timeout=300,
        )
        if result.returncode == 0:
            output.print(f"Combined HTML report: {report_dir}")
        else:
            output.error(f"genhtml failed: {result.stderr}")
    else:
        output.print("genhtml not found, skipping HTML report generation")

    return 0


def register_merge_coverage(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser("merge-coverage", help="Merge multiple LCOV coverage files")
    p.add_argument(
        "--output-dir",
        required=True,
        help="Directory for merged output",
    )
    p.add_argument(
        "lcov_files",
        nargs="+",
        help="LCOV files to merge",
    )
    p.set_defaults(func=cmd_merge_coverage)
