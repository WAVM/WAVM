# Getting Started

## Install a binary build

Binary builds of WAVM are available on [GitHub releases](https://github.com/WAVM/WAVM/releases) for Windows, Linux, and macOS.

### Windows

Download `wavm-windows-x64.zip` and extract it anywhere in your filesystem. Add the `bin` directory to your `PATH` environment variable to use `wavm` from any directory.

### Linux

Linux builds are available as `.deb` packages, `.rpm` packages, or `.tar.gz` archives.

**Debian/Ubuntu (deb):**
```bash
sudo apt install ./wavm-{version}-linux.deb
# To remove: sudo apt remove wavm
```

**Fedora/RHEL/AlmaLinux (rpm):**
```bash
sudo dnf install ./wavm-{version}-linux.rpm
# To remove: sudo dnf remove wavm
```

**Other distributions (tar.gz):**
```bash
mkdir wavm && tar -xzf wavm-{version}-linux.tar.gz -C wavm
```

The Linux binaries are built on a manylinux_2_28 (AlmaLinux 8) base and should work on most modern glibc-based Linux distributions, including:
* Ubuntu 20.04+
* Debian 11+
* Fedora 40+
* AlmaLinux/Rocky Linux 8+
* openSUSE Leap 15.6+

### macOS

Download `wavm-macos-arm64.tar.gz` and extract it:

```bash
mkdir wavm && tar -xzf wavm-macos-arm64.tar.gz -C wavm
```

Note: macOS builds are currently ARM64 (Apple Silicon) only.

## Build from source

Alternatively, you can build WAVM from source. See [Building WAVM from source](Building.md) for instructions.

## WAVM on the command line

The `wavm` executable provides command-line access to WAVM. It has several sub-commands:

* `wavm help` displays the available sub-commands, or detailed information about a specific
  sub-command.

* `wavm run` loads a WebAssembly file (text or binary) and calls `main` (or a specified function).

* `wavm assemble` loads a WebAssembly text file (WAST/WAT), and saves it as a WebAssembly binary
  file (WASM).

* `wavm disassemble` loads a WebAssembly binary file, translates it to the WebAssembly text format,
  and either prints that text to the console or saves it to a file.

* `wavm compile` loads a WebAssembly file, and compiles it to one of several formats: unoptimized or
  optimized LLVM IR, a native object file, or a WebAssembly file with object code embedded in a
  a custom section (`wavm.precompiled_object`).

### Run some example programs

WAVM builds include some simple WebAssembly programs to try. In Windows builds, they will be in
`examples`. In Linux and macOS builds, they will be in `share/wavm/examples`. Try running
these example programs:

```bash
wavm run examples/helloworld.wast
wavm run examples/zlib.wasm
wavm run examples/trap.wast
wavm run examples/echo.wast "Hello, world!"
wavm run examples/helloworld.wast | wavm run examples/tee.wast
wavm run examples/blake2b.wast
```

## Disassemble a WebAssembly module

```bash
wavm disassemble examples/zlib.wasm zlib.wast
```

## Set up an object cache

WAVM supports caching compiled object code for WebAssembly modules. The cache is configured by
setting a `WAVM_OBJECT_CACHE_DIR` environment variable to point to a directory to store the
cache in.

```bash
export WAVM_OBJECT_CACHE_DIR=/path/to/existing/directory
wavm run huge.wasm # Slow
wavm run huge.wasm # Fast
```

The first time `wavm` loads `huge.wasm`, it must spend some time to compile it to object code.
The second time `wavm` loads `huge.wasm`, it just needs to load its object code from the cache.
For small modules, that may not save much time, but for large WebAssembly modules, that can be
much faster than compiling the module from scratch.

## Continue to: [Building WAVM from source](Building.md)
