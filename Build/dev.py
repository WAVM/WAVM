#!/usr/bin/env python3
"""Build, test, and maintain WAVM across multiple configurations."""

import argparse
import os
import sys
from pathlib import Path

from lib.output import output
from lib.context import CommandContext
import lib.platform as platform
from lib.benchmark import register_benchmark_commands
from lib.build import register_list_configs, register_package
from lib.format import register_format
from lib.lint import register_lint
from lib.coverage import register_merge_coverage
from lib.fuzz import register_fuzz_commands
from lib.run import register_run
from lib.test import register_test, register_test_install
from lib.tidy import register_tidy
from lib.workspace import register_setup_workspace

WAVM_SOURCE_DIR = Path(__file__).resolve().parent.parent


def get_default_work_dir() -> Path:
    env_dir = os.environ.get("WAVM_BUILD_TOOL_WORK_DIR")
    if env_dir:
        return Path(env_dir)
    return WAVM_SOURCE_DIR / ".working"


def init_work_dir(work_dir: Path) -> Path:
    """Resolve and create the working directory."""
    work_dir = work_dir.resolve()
    work_dir.mkdir(parents=True, exist_ok=True)
    return work_dir


def main() -> int:
    # Don't let the host's object cache leak into test runs.
    os.environ.pop("WAVM_OBJECT_CACHE_DIR", None)
    os.environ.pop("WAVM_OBJECT_CACHE_MAX_MB", None)

    # Create main parser
    parser = argparse.ArgumentParser(
        description="Build and test WAVM",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    # Global arguments
    parser.add_argument(
        "--work-dir",
        default=get_default_work_dir(),
        help="Working directory for downloads and builds (default: $WAVM_BUILD_TOOL_WORK_DIR or .working/)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print verbose output",
    )
    parser.add_argument(
        "--llvm-source",
        action="store_true",
        help="Build LLVM from source instead of downloading pre-built binaries",
    )
    parser.add_argument(
        "--offline",
        action="store_true",
        help="Skip all network operations; use previously downloaded/cloned data",
    )
    parser.add_argument(
        "--target-arch",
        metavar="ARCH",
        choices=["x64", "arm64"],
        help="Target architecture (x64 or arm64). Defaults to host architecture. "
             "Used for cross-compilation on macOS (builds x86-64 on ARM64 via Rosetta).",
    )

    # Each register_* function lives next to its command handler.
    subparsers = parser.add_subparsers(dest="command", required=True)
    register_test(subparsers)
    register_test_install(subparsers)
    register_format(subparsers)
    register_lint(subparsers)
    register_tidy(subparsers)
    register_package(subparsers)
    register_merge_coverage(subparsers)
    register_setup_workspace(subparsers)
    register_list_configs(subparsers)
    register_fuzz_commands(subparsers)
    register_benchmark_commands(subparsers)
    register_run(subparsers)

    args = parser.parse_args()

    # Set output options
    output.verbose_enabled = args.verbose

    # Set target architecture if specified (must be before init_env)
    if args.target_arch:
        platform.set_target_arch(args.target_arch)

    # Initialize the environment (merges MSVC vars on Windows).
    platform.init_env()

    # Resolve working directory once for all commands.
    work_dir = init_work_dir(Path(args.work_dir))

    ctx = CommandContext(work_dir=work_dir, source_dir=WAVM_SOURCE_DIR)
    try:
        return args.func(args, ctx)
    except RuntimeError as e:
        output.error(f"Fatal error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
