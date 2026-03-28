# Building WAVM from source

## Quick Start (Recommended)

The easiest way to build and test WAVM is using the `dev.py` script, which automatically downloads a compatible LLVM toolchain and builds WAVM:

```bash
# List available configurations for your platform
python3 Build/dev.py list-configs

# Build and test a specific configuration
python3 Build/dev.py test --config RelWithDebInfo-clang

# Build and test all configurations (takes a long time)
python3 Build/dev.py test

# Build and create packages (.tar.gz, .deb, .rpm on Linux)
python3 Build/dev.py package --config LTO-clang /path/to/output
```

The script stores downloaded LLVM toolchains and build artifacts in `.working/` by default. You can override this with `--work-dir` or the `WAVM_BUILD_TOOL_WORK_DIR` environment variable.

### Other commands

```bash
# Check C/C++ source formatting (clang-format)
python3 Build/dev.py format

# Auto-fix C/C++ formatting
python3 Build/dev.py format --fix

# Run Python linters (ruff + ty)
python3 Build/dev.py lint

# Run clang-tidy
python3 Build/dev.py tidy

# Auto-fix clang-tidy issues
python3 Build/dev.py tidy --fix
```

## Manual Build

If you prefer to configure the build manually, you'll need CMake and LLVM.

### Prerequisites

* **CMake 3.16 or higher** - On Linux, it is probably available via your
package manager. For example, you can install it on Ubuntu with `sudo apt install cmake`.
Otherwise, you can download it from the [CMake website](https://cmake.org/download/).

* **Ninja** (recommended) - Available via package managers or from the [Ninja website](https://ninja-build.org/).

* **LLVM 20.0+** - LLVM 20 is the minimum supported version, but we recommend using [WAVM-LLVM](https://github.com/WAVM/WAVM-LLVM) (LLVM 21.x with WAVM-specific patches). This is especially important on macOS, where upstream LLVM builds may have compatibility issues.
  * **Recommended**: Download pre-built WAVM-LLVM from the [WAVM-LLVM releases](https://github.com/WAVM/WAVM-LLVM/releases).
  * On Ubuntu/Debian, upstream LLVM builds are available from the LLVM [apt repo](https://apt.llvm.org/).
  * On other systems, you can download upstream LLVM from the [LLVM download page](https://llvm.org/releases/download.html).
  * To build LLVM from source, see [Getting Started with the LLVM System](http://llvm.org/docs/GettingStarted.html)
    and [Building LLVM with CMake](http://llvm.org/docs/CMake.html).

#### Windows

You can use Visual Studio 2022+ to compile WAVM. If you don't have Visual Studio, you can use the
freely available Visual Studio C++ Build Tools or Visual Studio Community, both
available from the Visual Studio [download page](https://visualstudio.microsoft.com/downloads/).
Visual Studio 2019 may also work, but has not been recently tested.

#### Linux

You'll need a C/C++ compiler. `gcc` and `clang` are known to compile WAVM correctly.

#### macOS

You'll need to install Xcode from the App Store.

### Configure and build

```bash
# Create a build directory
mkdir build && cd build

# Configure with CMake (using Ninja)
cmake <path to WAVM source> -G Ninja -DLLVM_DIR=<path to LLVM>/lib/cmake/llvm

# Build
ninja

# Run tests
./bin/wavm test script ../Test/wavm/simd.wast
```

If CMake can't find your LLVM directory, set the `LLVM_DIR` CMake variable to point to your LLVM installation's `lib/cmake/llvm` directory.

### CMake options

| Option | Description |
|--------|-------------|
| `-DWAVM_ENABLE_STATIC_LINKING=ON` | Static linking |
| `-DWAVM_ENABLE_LTO=ON` or `THIN` | Link-time optimization |
| `-DWAVM_ENABLE_RELEASE_ASSERTS=ON` | Assertions in release builds |
| `-DWAVM_ENABLE_RUNTIME=OFF` | Disable runtime (parse/validate only) |

## Continue to: [Exploring the WAVM source](CodeOrganization.md)
