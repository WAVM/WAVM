"""VS Code workspace file generation for clangd integration."""

import argparse
import json
from pathlib import Path
from typing import Any, Optional

from .build import get_lint_config, create_wavm_configure_task
from .context import CommandContext
from .llvm import get_llvm_toolchain_task
from .output import output
from .platform import EXE_EXT
from .task_graph import execute


def _add_unique(items: list[str], value: str) -> None:
    """Add a value to a list if it's not already present."""
    if value not in items:
        items.append(value)


def generate_workspace_file(
    workspace_path: Path,
    source_dir: Path,
    build_dir: Path,
    llvm_install_dir: Optional[Path] = None,
) -> None:
    """Generate or update a VS Code .code-workspace file configured for clangd.

    If the file already exists, merges clangd settings into it. Otherwise creates
    a new workspace file pointing at the WAVM source tree.
    """
    if workspace_path.exists():
        workspace: dict[str, Any] = json.loads(workspace_path.read_text())
    else:
        workspace = {}

    # Ensure folders list exists; add WAVM source folder if not already present
    folders: list[dict[str, str]] = workspace.setdefault("folders", [])
    source_str = str(source_dir)
    if not any(f.get("path") == source_str for f in folders):
        folders.append({"path": source_str, "name": "WAVM"})

    # Set clangd settings
    settings: dict[str, Any] = workspace.setdefault("settings", {})
    settings["clangd.arguments"] = [
        f"--compile-commands-dir={build_dir}",
        "--background-index",
    ]
    if llvm_install_dir:
        settings["clangd.path"] = str(llvm_install_dir / "bin" / f"clangd{EXE_EXT}")
    settings["C_Cpp.intelliSenseEngine"] = "disabled"
    settings["[cpp]"] = {"editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"}
    settings["[c]"] = {"editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"}

    # Set extension recommendations
    extensions: dict[str, list[str]] = workspace.setdefault("extensions", {})
    recommendations: list[str] = extensions.setdefault("recommendations", [])
    _add_unique(recommendations, "llvm-vs-code-extensions.vscode-clangd")
    unwanted: list[str] = extensions.setdefault("unwantedRecommendations", [])
    _add_unique(unwanted, "ms-vscode.cpptools")

    workspace_path.parent.mkdir(parents=True, exist_ok=True)
    workspace_path.write_text(json.dumps(workspace, indent=4) + "\n")


# =============================================================================
# Command Handler
# =============================================================================


def cmd_setup_workspace(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Generate a VS Code workspace file with clangd configuration."""
    wavm_config = get_lint_config()

    configure_task = create_wavm_configure_task(
        wavm_config, ctx.work_dir, ctx.source_dir,
        build_llvm_from_source=args.llvm_source,
        offline=args.offline,
    )

    if not execute([configure_task]):
        return 1

    # Extract LLVM install dir for clangd path
    toolchain_task = get_llvm_toolchain_task(
        ctx.work_dir, args.llvm_source, offline=args.offline
    )
    llvm_install_dir: Optional[Path] = None
    if toolchain_task.result:
        llvm_install_dir = toolchain_task.result.artifacts.get("llvm_install_dir")

    # Generate or update workspace file
    workspace_path = Path(args.workspace_file).resolve()
    generate_workspace_file(
        workspace_path, ctx.source_dir, configure_task.build_dir, llvm_install_dir
    )
    output.print(f"Workspace file: {workspace_path}")

    return 0


def register_setup_workspace(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser(
        "setup-workspace",
        help="Generate or update a VS Code workspace file with clangd configuration",
    )
    p.add_argument(
        "workspace_file",
        help="Path to the .code-workspace file to create or update",
    )
    p.set_defaults(func=cmd_setup_workspace)
