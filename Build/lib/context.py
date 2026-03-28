"""Shared command context passed to all subcommand handlers."""

from dataclasses import dataclass
from pathlib import Path


@dataclass
class CommandContext:
    work_dir: Path
    source_dir: Path
