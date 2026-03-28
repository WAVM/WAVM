"""Clang-tidy tasks."""

import argparse
import json
from pathlib import Path
from typing import Union

from .build import ConfigureWAVMTask, get_lint_config, create_wavm_configure_task
from .context import CommandContext
from .llvm import get_llvm_toolchain_task
from .platform import EXE_EXT, run_command
from .cpp_files import discover_cpp_files
from .task_graph import Task, TaskResult, execute


class ClangTidyTask(Task):
    """Run clang-tidy on a single source file."""

    def __init__(
        self,
        file_path: Path,
        source_dir: Path,
        fix: bool,
        tools_task: Task,
        configure_task: ConfigureWAVMTask,
    ):
        action = " fix" if fix else ""
        rel_path = file_path.relative_to(source_dir)
        super().__init__(
            name=f"tidy_{action}_{rel_path}",
            description=f"Tidy{action}: {rel_path}",
            dependencies=[tools_task, configure_task],
        )
        self.file_path = file_path
        self.source_dir = source_dir
        self.fix = fix
        self.tools_task = tools_task
        self.configure_task = configure_task

    def run(self) -> TaskResult:
        assert self.tools_task.result is not None
        llvm_install_dir: Path = self.tools_task.result.artifacts["llvm_install_dir"]
        clang_tidy = llvm_install_dir / "bin" / f"clang-tidy{EXE_EXT}"

        build_dir = self.configure_task.build_dir

        rel_path = self.file_path.relative_to(self.source_dir)

        cmd: list[Union[str, Path]] = [
            clang_tidy, "-p", build_dir,
        ]
        if not self.fix:
            cmd.append("--warnings-as-errors=*")
        if self.fix:
            cmd.append("--fix")
        cmd.append(self.file_path)

        result = run_command(cmd)

        if result.returncode != 0:
            hint = None if self.fix else "To fix all: python3 Build/dev.py tidy --fix"
            return TaskResult(
                False,
                result.format_failure(f"clang-tidy failed: {rel_path}", hint=hint),
            )

        return TaskResult(True)


class DiscoverTidyFilesTask(Task):
    """Discover files to tidy and create individual clang-tidy tasks."""

    def __init__(
        self,
        source_dir: Path,
        fix: bool,
        tools_task: Task,
        complete_task: Task,
        configure_task: ConfigureWAVMTask,
    ):
        super().__init__(
            name="discover_tidy_files",
            description="Discover tidy files",
            dependencies=[configure_task],
        )
        self.source_dir = source_dir
        self.fix = fix
        self.tools_task = tools_task
        self.complete_task = complete_task
        self.configure_task = configure_task

    def run(self) -> TaskResult:
        files_to_check = discover_cpp_files(self.source_dir)

        # Build set of files in the compilation database for clang-tidy.
        # This naturally excludes wrong-platform files (e.g. Windows sources on
        # macOS) that would fail to compile under clang-tidy.
        build_dir = self.configure_task.build_dir
        compdb_path = build_dir / "compile_commands.json"
        compdb_files: set[Path] = set()
        if compdb_path.exists():
            with open(compdb_path) as f:
                for entry in json.load(f):
                    compdb_files.add(Path(entry["file"]).resolve())

        new_tasks: list[Task] = []
        for file_path in files_to_check:
            if file_path.resolve() in compdb_files:
                tidy_task = ClangTidyTask(
                    file_path=file_path,
                    source_dir=self.source_dir,
                    fix=self.fix,
                    tools_task=self.tools_task,
                    configure_task=self.configure_task,
                )
                new_tasks.append(tidy_task)

        self.complete_task.dependencies.extend(new_tasks)

        return TaskResult(
            True,
            artifacts={"file_count": len(new_tasks)},
        )


def create_tidy_tasks(
    tools_task: Task,
    source_dir: Path,
    fix: bool,
    configure_task: ConfigureWAVMTask,
) -> Task:
    """Create clang-tidy tasks. Returns the completion task."""
    tidy_complete_task = Task("tidy_complete", "Completion of all tidy tasks")

    discover_task = DiscoverTidyFilesTask(
        source_dir=source_dir,
        fix=fix,
        tools_task=tools_task,
        complete_task=tidy_complete_task,
        configure_task=configure_task,
    )
    tidy_complete_task.dependencies.append(discover_task)

    return tidy_complete_task


# =============================================================================
# Command Handler
# =============================================================================


def cmd_tidy(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Run clang-tidy."""
    lint_config = get_lint_config()

    configure_task = create_wavm_configure_task(
        lint_config, ctx.work_dir, ctx.source_dir,
        build_llvm_from_source=args.llvm_source,
        offline=args.offline,
    )
    toolchain_task = get_llvm_toolchain_task(
        ctx.work_dir, args.llvm_source, offline=args.offline
    )
    tidy_task = create_tidy_tasks(
        toolchain_task, ctx.source_dir, args.fix,
        configure_task=configure_task,
    )
    return 0 if execute([tidy_task]) else 1


def register_tidy(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser("tidy", help="Run clang-tidy")
    p.add_argument(
        "--fix",
        action="store_true",
        help="Fix clang-tidy issues instead of just checking",
    )
    p.set_defaults(func=cmd_tidy)
