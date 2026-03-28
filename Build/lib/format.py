"""C/C++ formatting tasks using clang-format."""

import argparse
import difflib
from pathlib import Path
from typing import Optional

from .context import CommandContext
from .cpp_files import discover_cpp_files
from .llvm import get_llvm_toolchain_task
from .platform import EXE_EXT, run_command
from .task_graph import Task, TaskResult, execute


class ClangFormatTask(Task):
    """Check or fix formatting of a single file."""

    def __init__(
        self,
        file_path: Path,
        source_dir: Path,
        fix: bool,
        tools_task: Task,  # BuildLLVMTask or DownloadLLVMTask
        extra_dependencies: Optional[list[Task]] = None,
    ):
        action = "fix" if fix else "check"
        rel_path = file_path.relative_to(source_dir)
        super().__init__(
            name=f"format_{action}_{rel_path}",
            description=f"Format {action}: {rel_path}",
            dependencies=[tools_task] + (extra_dependencies or []),
        )
        self.file_path = file_path
        self.source_dir = source_dir
        self.fix = fix
        self.tools_task = tools_task

    def run(self) -> TaskResult:
        assert self.tools_task.result is not None
        llvm_install_dir: Path = self.tools_task.result.artifacts["llvm_install_dir"]
        clang_format = llvm_install_dir / "bin" / f"clang-format{EXE_EXT}"

        rel_path = self.file_path.relative_to(self.source_dir)

        if self.fix:
            result = run_command([clang_format, "-i", str(self.file_path)])
            if result.returncode != 0:
                return TaskResult(False, result.format_failure(f"Failed to format: {rel_path}"))
            return TaskResult(True)

        # Check mode: get formatted output and compare
        result = run_command([clang_format, str(self.file_path)])
        if result.returncode != 0:
            return TaskResult(False, result.format_failure(f"clang-format failed on {rel_path}"))

        # Compare raw bytes to avoid encoding/line-ending mismatches
        original_bytes = self.file_path.read_bytes()

        if result.stdout_bytes == original_bytes:
            return TaskResult(True)

        # Generate colorized diff from decoded text
        original = original_bytes.decode("utf-8", errors="replace")
        formatted = result.stdout_bytes.decode("utf-8", errors="replace")
        original_lines = original.splitlines(keepends=True)
        formatted_lines = formatted.splitlines(keepends=True)

        diff = difflib.unified_diff(
            original_lines,
            formatted_lines,
            fromfile=str(rel_path),
            tofile=str(rel_path) + " (formatted)",
        )

        RED = "\033[91m"
        GREEN = "\033[92m"
        CYAN = "\033[96m"
        RESET = "\033[0m"

        colored_lines = []
        for line in diff:
            if line.startswith("---") or line.startswith("+++"):
                colored_lines.append(f"{CYAN}{line}{RESET}")
            elif line.startswith("@@"):
                colored_lines.append(f"{CYAN}{line}{RESET}")
            elif line.startswith("-"):
                colored_lines.append(f"{RED}{line}{RESET}")
            elif line.startswith("+"):
                colored_lines.append(f"{GREEN}{line}{RESET}")
            else:
                colored_lines.append(line)

        diff_output = "".join(colored_lines)
        return TaskResult(
            False,
            f"File needs formatting: {rel_path}\n"
            f"{diff_output}\n"
            f"To fix all: python3 Build/dev.py format --fix",
        )


class DiscoverFormatFilesTask(Task):
    """Discover files to format and create individual format tasks."""

    def __init__(
        self,
        source_dir: Path,
        fix: bool,
        tools_task: Task,
        complete_task: Task,
    ):
        super().__init__(
            name="discover_format_files",
            description="Discover format files",
        )
        self.source_dir = source_dir
        self.fix = fix
        self.tools_task = tools_task
        self.complete_task = complete_task

    def run(self) -> TaskResult:
        files_to_check = discover_cpp_files(self.source_dir)

        new_tasks: list[Task] = []
        for file_path in files_to_check:
            format_task = ClangFormatTask(
                file_path=file_path,
                source_dir=self.source_dir,
                fix=self.fix,
                tools_task=self.tools_task,
            )
            new_tasks.append(format_task)

        self.complete_task.dependencies.extend(new_tasks)

        return TaskResult(
            True,
            artifacts={"file_count": len(files_to_check)},
        )


def create_format_tasks(
    tools_task: Task,
    source_dir: Path,
    fix: bool,
) -> Task:
    """Create clang-format tasks. Returns the completion task."""
    format_complete_task = Task("format_complete", "Completion of all format tasks")

    discover_task = DiscoverFormatFilesTask(
        source_dir=source_dir,
        fix=fix,
        tools_task=tools_task,
        complete_task=format_complete_task,
    )
    format_complete_task.dependencies.append(discover_task)

    return format_complete_task


# =============================================================================
# Command Handler
# =============================================================================


def cmd_format(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Check or fix C/C++ source code formatting."""
    toolchain_task = get_llvm_toolchain_task(
        ctx.work_dir, args.llvm_source, offline=args.offline
    )
    format_task = create_format_tasks(toolchain_task, ctx.source_dir, args.fix)
    return 0 if execute([format_task]) else 1


def register_format(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser(
        "format", help="Check or fix C/C++ source formatting (clang-format)"
    )
    p.add_argument(
        "--fix",
        action="store_true",
        help="Fix formatting issues instead of just checking",
    )
    p.set_defaults(func=cmd_format)
