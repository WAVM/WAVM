"""Discover C/C++ source files in the WAVM tree."""

import os
from pathlib import Path


def discover_cpp_files(source_dir: Path) -> list[Path]:
    """Discover C/C++ files to check, excluding ThirdParty and other non-project dirs."""
    files: list[Path] = []
    extensions = {".h", ".cpp", ".c"}

    for dirpath, dirnames, filenames in os.walk(source_dir):
        rel_dir = Path(dirpath).relative_to(source_dir)

        if rel_dir == Path("."):
            for excluded in [".git", ".working", "ThirdParty"]:
                if excluded in dirnames:
                    dirnames.remove(excluded)
        elif rel_dir == Path("Include") / "WAVM" / "Inline":
            if "xxhash" in dirnames:
                dirnames.remove("xxhash")

        for filename in filenames:
            if Path(filename).suffix.lower() in extensions:
                files.append(Path(dirpath) / filename)

    return files
