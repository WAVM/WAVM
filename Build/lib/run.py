"""Build-and-run subcommand."""

import argparse
import subprocess

from .build import build_single_config
from .context import CommandContext
from .output import output
from .platform import EXE_EXT


def cmd_run(args: argparse.Namespace, ctx: CommandContext) -> int:
    """Build and run a program from the build output."""
    program_args = args.program_args
    # Strip leading '--' if present
    if program_args and program_args[0] == "--":
        program_args = program_args[1:]

    if not program_args:
        output.error("Error: No program specified. Usage: dev.py run [--config CONFIG] -- PROGRAM [ARGS...]")
        return 1

    program_name = program_args[0]
    build_dir, err = build_single_config(
        ctx.work_dir, ctx.source_dir, args.config,
        llvm_source=args.llvm_source, offline=args.offline,
        ninja_targets=[program_name], suppress_summary=True,
    )
    if err is not None:
        return err
    assert build_dir is not None

    binary = build_dir / "bin" / (program_name + EXE_EXT)
    if not binary.exists():
        output.error(f"Error: Binary not found at {binary}")
        return 1

    result = subprocess.run([str(binary)] + program_args[1:])
    return result.returncode


def register_run(subparsers: argparse._SubParsersAction) -> None:
    p = subparsers.add_parser("run", help="Build and run a program from the build output")
    p.add_argument(
        "--config",
        default="LTO",
        help="Configuration to build (default: LTO)",
    )
    p.add_argument(
        "program_args",
        nargs=argparse.REMAINDER,
        help="Program and arguments (after --)",
    )
    p.set_defaults(func=cmd_run)
