"""Python linting tasks using ruff and ty."""

import argparse

from pathlib import Path

from .context import CommandContext
from .output import output
from .platform import run_command
from .toolchain import is_command_available
from .task_graph import Task, TaskResult, execute


class PythonLintTask(Task):
    """Run a Python linter via uvx on the Build directory."""

    def __init__(self, source_dir: Path, tool: str, args: list[str]):
        super().__init__(
            name=f"python_lint_{tool}",
            description=f"Python lint: {tool}",
        )
        self.source_dir = source_dir
        self.tool = tool
        self.args = args

    def run(self) -> TaskResult:
        cmd = ["uvx", self.tool, *self.args]
        result = run_command(cmd, cwd=self.source_dir / "Build")

        if result.returncode != 0:
            return TaskResult(False, result.format_failure(f"{self.tool} failed:"))

        return TaskResult(True)


def create_lint_tasks(
    source_dir: Path,
    fix: bool = False,
) -> Task:
    """Create Python lint tasks (ruff + ty). Returns the completion task."""
    lint_complete_task = Task("lint_complete", "Completion of all lint tasks")

    if is_command_available("uvx"):
        ty_task = PythonLintTask(source_dir, "ty", ["check"])
        lint_complete_task.dependencies.append(ty_task)

        ruff_args = ["check", "--fix"] if fix else ["check"]
        ruff_task = PythonLintTask(source_dir, "ruff", ruff_args)
        lint_complete_task.dependencies.append(ruff_task)
    else:
        output.error(
            "Warning: uvx not found, skipping Python linting (ty, ruff)."
        )

    return lint_complete_task


# =============================================================================
# Command Handler
# =============================================================================


def cmd_lint(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Run Python linters (ruff + ty)."""
    lint_task = create_lint_tasks(ctx.source_dir, fix=args.fix)
    return 0 if execute([lint_task]) else 1


def register_lint(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser(
        "lint", help="Run Python linters (ruff + ty)"
    )
    p.add_argument(
        "--fix",
        action="store_true",
        help="Auto-fix ruff issues (ty does not support auto-fix)",
    )
    p.set_defaults(func=cmd_lint)
