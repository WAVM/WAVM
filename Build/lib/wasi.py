"""WASI SDK download, compilation tasks, and WASI test definitions."""

import platform as platform_mod
import shutil
from pathlib import Path
from typing import Optional

from .platform import LINUX, MACOS, WINDOWS, EXE_EXT, download_and_extract, run_command
from .task_graph import Task, TaskResult
from .test_def import TestDef, TestStep


# =============================================================================
# WASI SDK
# =============================================================================

WASI_SDK_VERSION = "29"


def get_wasi_sdk_platform() -> str:
    """Get the platform suffix for WASI SDK downloads."""
    machine = platform_mod.machine().lower()
    if machine in ("x86_64", "amd64"):
        arch = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch = "arm64"
    else:
        raise RuntimeError(f"Unsupported architecture for WASI SDK: {machine}")

    if LINUX:
        return f"{arch}-linux"
    elif MACOS:
        return f"{arch}-macos"
    elif WINDOWS:
        return f"{arch}-windows"
    else:
        raise RuntimeError("Unsupported platform for WASI SDK")


class DownloadWasiSdkTask(Task):
    """Download the WASI SDK."""

    def __init__(self, working_dir: Path, offline: bool = False):
        super().__init__(
            name="download_wasi_sdk",
            description="WASI SDK: download",
        )
        self.working_dir = working_dir
        self.wasi_sdk_dir: Optional[Path] = None
        self.offline = offline

    def run(self) -> TaskResult:
        platform_name = get_wasi_sdk_platform()
        dir_name = f"wasi-sdk-{WASI_SDK_VERSION}"
        self.wasi_sdk_dir = self.working_dir / dir_name

        # Check stamp file to skip re-downloading
        stamp_file = self.wasi_sdk_dir / ".download_stamp"
        if stamp_file.exists() and stamp_file.read_text().strip() == WASI_SDK_VERSION:
            clang = self.wasi_sdk_dir / "bin" / f"clang{EXE_EXT}"
            if clang.exists():
                return TaskResult(True, artifacts={"wasi_sdk_dir": self.wasi_sdk_dir})

        if self.offline:
            return TaskResult(
                False,
                f"WASI SDK not found at {self.wasi_sdk_dir}"
                " (--offline requires a prior download)",
            )

        # Clean and download
        if self.wasi_sdk_dir.exists():
            shutil.rmtree(self.wasi_sdk_dir)

        url = (
            f"https://github.com/WebAssembly/wasi-sdk/releases/download/"
            f"wasi-sdk-{WASI_SDK_VERSION}/{dir_name}.0-{platform_name}.tar.gz"
        )

        try:
            download_and_extract(url, self.wasi_sdk_dir)
        except Exception as e:
            return TaskResult(False, f"Failed to download WASI SDK from {url}: {e}")

        # The tarball extracts with a nested directory; move contents up if needed
        nested = self.wasi_sdk_dir / f"{dir_name}.0-{platform_name}"
        if nested.exists() and nested.is_dir():
            for item in list(nested.iterdir()):
                shutil.move(str(item), str(self.wasi_sdk_dir / item.name))
            nested.rmdir()

        stamp_file.write_text(WASI_SDK_VERSION)
        return TaskResult(True, artifacts={"wasi_sdk_dir": self.wasi_sdk_dir})


class CompileWasiTestTask(Task):
    """Compile a single WASI test .cpp file to .wasm using WASI SDK."""

    def __init__(self, cpp_file: Path, output_dir: Path, wasi_sdk_task: DownloadWasiSdkTask):
        self.cpp_file = cpp_file
        self.wasm_file = output_dir / f"{cpp_file.stem}.wasm"
        super().__init__(
            name=f"compile_wasi_{cpp_file.stem}",
            description=f"WASI: compile {cpp_file.name}",
            dependencies=[wasi_sdk_task],
        )
        self.wasi_sdk_task = wasi_sdk_task

    def run(self) -> TaskResult:
        # Skip if .wasm is up-to-date
        if self.wasm_file.exists():
            if self.wasm_file.stat().st_mtime >= self.cpp_file.stat().st_mtime:
                return TaskResult(True)

        self.wasm_file.parent.mkdir(parents=True, exist_ok=True)
        assert self.wasi_sdk_task.wasi_sdk_dir is not None
        wasi_sdk_dir = self.wasi_sdk_task.wasi_sdk_dir

        clang = wasi_sdk_dir / "bin" / f"clang{EXE_EXT}"
        sysroot = wasi_sdk_dir / "share" / "wasi-sysroot"

        cmd = [
            str(clang),
            "-D_WASI_EMULATED_MMAN",
            "-lwasi-emulated-mman",
            "-O3",
            "-g3",
            f"--sysroot={sysroot}",
            "-o",
            str(self.wasm_file),
            str(self.cpp_file),
        ]
        result = run_command(cmd)
        if result.returncode != 0:
            return TaskResult(False, result.format_failure(f"Failed to compile {self.cpp_file.name}"))

        return TaskResult(True)


# =============================================================================
# WASI Test Definitions
# =============================================================================

# Common command prefix for running WASI test modules
WASI_RUN = ["{wavm_bin}", "run", "--abi=wasi", "--enable", "extended-name-section"]
WASI_RUN_MOUNTED = [*WASI_RUN, "--mount-root", "{temp_dir}"]

WASI_TESTS: list[TestDef] = [
    TestDef(
        "wasi_args",
        test_wasi_cpp_sources=["args"],
        steps=[
            TestStep(
                command=[*WASI_RUN, "{wasi_wasm_dir}/args.wasm", "arg1", "arg2"],
                expected_output=r"argc=3\nargv\[0\]: .+\nargv\[1\]: arg1\nargv\[2\]: arg2",
            )
        ],
    ),
    TestDef(
        "wasi_clock",
        test_wasi_cpp_sources=["clock"],
        steps=[TestStep(command=[*WASI_RUN, "{wasi_wasm_dir}/clock.wasm"])],
    ),
    TestDef(
        "wasi_exit",
        test_wasi_cpp_sources=["exit"],
        steps=[TestStep(
            command=[*WASI_RUN, "{wasi_wasm_dir}/exit.wasm"],
            # WASI exit relies on being able to unwind an exception through the WASM code.
            # Enable debug+trace-unwind log channels to get test coverage of the code that
            # only runs when those log channels are enabled.
            env={"WAVM_OUTPUT": "debug,trace-unwind"}
            )],
    ),
    TestDef(
        "wasi_random",
        test_wasi_cpp_sources=["random"],
        steps=[TestStep(command=[*WASI_RUN, "{wasi_wasm_dir}/random.wasm"])],
    ),
    TestDef(
        "wasi_stdout",
        test_wasi_cpp_sources=["stdout"],
        steps=[
            TestStep(
                command=[*WASI_RUN, "{wasi_wasm_dir}/stdout.wasm"],
                expected_output=r"Hello world!",
            )
        ],
    ),
    TestDef(
        "wasi_stdout_detected_abi",
        test_wasi_cpp_sources=["stdout"],
        steps=[
            TestStep(
                command=[
                    "{wavm_bin}",
                    "run",
                    "--enable",
                    "extended-name-section",
                    "{wasi_wasm_dir}/stdout.wasm",
                ],
                expected_output=r"Hello world!",
            )
        ],
    ),
    # WASI filesystem tests
    TestDef(
        "wasi_write_cat",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "cat"],
        steps=[
            TestStep(
                name="write",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/write.wasm",
                    "file.txt",
                    "write_cat_test",
                ],
            ),
            TestStep(
                name="cat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/cat.wasm", "file.txt"],
                expected_output=r"write_cat_test",
            ),
        ],
    ),
    TestDef(
        "wasi_append",
        create_temp_dir=True,
        test_wasi_cpp_sources=["append", "cat"],
        steps=[
            TestStep(
                name="append1",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/append.wasm",
                    "file.txt",
                    "first line",
                ],
            ),
            TestStep(
                name="append2",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/append.wasm",
                    "file.txt",
                    "second line",
                ],
            ),
            TestStep(
                name="cat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/cat.wasm", "file.txt"],
                expected_output=r"first line\nsecond line",
            ),
        ],
    ),
    TestDef(
        "wasi_mmap",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "mmap"],
        steps=[
            TestStep(
                name="write",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/write.wasm", "file.txt", "mmap_test"],
            ),
            TestStep(
                name="mmap",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/mmap.wasm", "file.txt"],
                expected_output=r"mmap_test",
            ),
        ],
    ),
    TestDef(
        "wasi_mkdir_ls_rmdir",
        create_temp_dir=True,
        test_wasi_cpp_sources=["mkdir", "ls", "rmdir"],
        steps=[
            TestStep(
                name="mkdir", command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/mkdir.wasm", "subdir"]
            ),
            TestStep(
                name="ls",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/ls.wasm", "."],
                expected_output=r"subdir",
            ),
            TestStep(
                name="rmdir", command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/rmdir.wasm", "subdir"]
            ),
        ],
    ),
    TestDef(
        "wasi_mv",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "ls", "mv", "cat"],
        steps=[
            TestStep(
                name="write",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/write.wasm", "src.txt", "mv_test"],
            ),
            TestStep(
                name="ls_before",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/ls.wasm", "."],
                expected_output=r"src\.txt",
            ),
            TestStep(
                name="mv",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/mv.wasm", "src.txt", "dst.txt"],
            ),
            TestStep(
                name="ls_after",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/ls.wasm", "."],
                expected_output=r"dst\.txt",
                unexpected_output=r"src\.txt",
            ),
            TestStep(
                name="cat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/cat.wasm", "dst.txt"],
                expected_output=r"mv_test",
            ),
        ],
    ),
    TestDef(
        "wasi_rm",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "ls", "rm"],
        steps=[
            TestStep(
                name="write",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/write.wasm", "file.txt", "rm_test"],
            ),
            TestStep(
                name="ls_before",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/ls.wasm", "."],
                expected_output=r"file\.txt",
            ),
            TestStep(name="rm", command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/rm.wasm", "file.txt"]),
            TestStep(
                name="ls_after",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/ls.wasm", "."],
                unexpected_output=r"file\.txt",
            ),
        ],
    ),
    TestDef(
        "wasi_stat",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "stat"],
        steps=[
            TestStep(
                name="write",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/write.wasm", "file.txt", "stat_test"],
            ),
            TestStep(
                name="stat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/stat.wasm", "file.txt"],
                expected_output=r"st_size: 10",
            ),
        ],
    ),
    TestDef(
        "wasi_preadwrite",
        create_temp_dir=True,
        test_wasi_cpp_sources=["preadwrite"],
        steps=[
            TestStep(
                name="preadwrite",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/preadwrite.wasm",
                    "file.txt",
                ],
                expected_output=r"pread\(5000\): Hello 5000!\npread\(500\): Hello 500!",
            ),
        ],
    ),
    TestDef(
        "wasi_fd_filestat_set_size",
        create_temp_dir=True,
        test_wasi_cpp_sources=["fd_filestat_set_size", "stat"],
        steps=[
            TestStep(
                name="truncate",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/fd_filestat_set_size.wasm",
                    "file.txt",
                ],
            ),
            TestStep(
                name="stat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/stat.wasm", "file.txt"],
                expected_output=r"st_size: 4201",
            ),
        ],
    ),
    TestDef(
        "wasi_fd_renumber",
        create_temp_dir=True,
        test_wasi_cpp_sources=["fd_renumber", "cat"],
        steps=[
            TestStep(
                name="renumber",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/fd_renumber.wasm", "output.txt"],
            ),
            TestStep(
                name="cat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/cat.wasm", "output.txt"],
                expected_output=r"Hello stdout!",
            ),
        ],
    ),
    TestDef(
        "wasi_fd_filestat_set_times",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "fd_filestat_set_times", "stat"],
        steps=[
            TestStep(
                name="write",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/write.wasm", "file.txt", "times_test"],
            ),
            TestStep(
                name="set_times",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/fd_filestat_set_times.wasm",
                    "file.txt",
                    "946684800000000000",
                    "946684800000000000",
                ],
            ),
            TestStep(
                name="stat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/stat.wasm", "file.txt"],
                expected_output=r"st_atim: 2000/1/1 0:0:0 UTC",
            ),
        ],
    ),
    TestDef(
        "wasi_path_filestat_set_times",
        create_temp_dir=True,
        test_wasi_cpp_sources=["write", "path_filestat_set_times", "stat"],
        steps=[
            TestStep(
                name="write",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/write.wasm", "file.txt", "times_test"],
            ),
            TestStep(
                name="set_times",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/path_filestat_set_times.wasm",
                    "file.txt",
                    "946684800000000000",
                    "946684800000000000",
                ],
            ),
            TestStep(
                name="stat",
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/stat.wasm", "file.txt"],
                expected_output=r"st_atim: 2000/1/1 0:0:0 UTC",
            ),
        ],
    ),
    TestDef(
        "wasi_largefile",
        create_temp_dir=True,
        test_wasi_cpp_sources=["largefile"],
        steps=[
            TestStep(
                name="largefile",
                command=[
                    *WASI_RUN_MOUNTED,
                    "{wasi_wasm_dir}/largefile.wasm",
                    "file.txt",
                ],
                expected_output=r"pread\(3GB\): Hello 3GB!\npread\(6GB\): Hello 6GB!\npread\(9GB\): Hello 9GB!",
            ),
        ],
    ),
    # WASI API tests
    TestDef(
        "wasi_fd_seek_tell",
        create_temp_dir=True,
        test_wasi_cpp_sources=["fd_seek_tell"],
        steps=[
            TestStep(
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/fd_seek_tell.wasm"],
            )
        ],
    ),
    TestDef(
        "wasi_fd_sync",
        create_temp_dir=True,
        test_wasi_cpp_sources=["fd_sync"],
        steps=[
            TestStep(
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/fd_sync.wasm"],
            )
        ],
    ),
    TestDef(
        "wasi_fd_fdstat",
        create_temp_dir=True,
        test_wasi_cpp_sources=["fd_fdstat"],
        steps=[
            TestStep(
                command=[*WASI_RUN_MOUNTED, "{wasi_wasm_dir}/fd_fdstat.wasm"],
            )
        ],
    ),
    TestDef(
        "wasi_clock_res",
        test_wasi_cpp_sources=["clock_res"],
        steps=[
            TestStep(command=[*WASI_RUN, "{wasi_wasm_dir}/clock_res.wasm"]),
        ],
    ),
    TestDef(
        "wasi_clock_res_detected_abi",
        test_wasi_cpp_sources=["clock_res"],
        steps=[
            TestStep(
                command=[
                    "{wavm_bin}",
                    "run",
                    "{wasi_wasm_dir}/clock_res.wasm",
                ],
            ),
        ],
    ),
    TestDef(
        "wasi_environ",
        test_wasi_cpp_sources=["environ"],
        steps=[
            TestStep(
                command=[*WASI_RUN, "{wasi_wasm_dir}/environ.wasm"],
                expected_output=r"environ_count: \d+",
            )
        ],
    ),
    # WASI OOB pointer tests (raw .wast, no compilation needed)
    TestDef(
        "wasi_random_get_oob",
        steps=[
            TestStep(
                command=[*WASI_RUN, "{source_dir}/Test/wasi/wast/random_get_oob.wast"],
            )
        ],
    ),
    TestDef(
        "wasi_fd_write_oob",
        steps=[
            TestStep(
                command=[*WASI_RUN, "{source_dir}/Test/wasi/wast/fd_write_oob.wast"],
            )
        ],
    ),
    TestDef(
        "wasi_fd_read_oob",
        steps=[
            TestStep(
                command=[*WASI_RUN, "{source_dir}/Test/wasi/wast/fd_read_oob.wast"],
            )
        ],
    ),
]
