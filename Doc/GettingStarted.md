# Getting Started

## Install a binary build

A good place to start is by installing a binary build of WAVM, and trying out the command-line
tools. Nightly and release builds are available on [GitHub releases](https://github.com/WAVM/WAVM/releases)
for Windows, MacOS, and Linux.

### Windows

Windows builds are available as either an executable installer or a zip archive. The installer
provides the option to add the wavm binaries directory to the `PATH` environment variable, and the
instructions below assume that you have done so.

### Linux

Linux builds are available as `deb` or `rpm` packages, or `.tar.gz` archives. If your distribution
uses `apt` for package management, you can install the `deb` package with the command:

  ```
  sudo apt install ./wavm-1.0.0-linux.deb
  ```

Or if your distribution uses `yum` for package management, you can install the `rpm package with the
command:

  ```
  sudo yum install ./wavm-1.0.0-linux.rpm
  ```

Once it is installed, you may remove it with the command:

  ```
  sudo apt remove wavm
  ```

Or:

  ```
  sudo yum remove wavm
  ```

You may extract a `.tar.gz` build anywhere in your filesystem.

The Linux binaries in the `.tar.gz` package are built on Ubuntu 16.04, but should work on any
up-to-date glibc-based Linux distribution. They are tested on:
* CentOS 7.0, 7.7, and 8.0
* Ubuntu 16.04, 18.04, and 19.04
* Debian 8, 9, and 10

### MacOS

MacOS builds are only available as `.tar.gz` archives.

Similar to Linux, you may extract a `.tar.gz` build anywhere in your filesystem, but a few things
are smoother if you extract the archive to `/usr/local`.

The MacOS binaries are built on MacOS 10.14. 

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
  `WAVM/examples`. In Linux and MacOS builds, they will be in `share/wavm/examples`. Try running
  these example programs from that directory:

  ```
  wavm run examples/helloworld.wast
  wavm run examples/zlib.wasm
  wavm run examples/trap.wast
  wavm run examples/echo.wast "Hello, world!"
  wavm run examples/helloworld.wast | wavm run examples/tee.wast
  wavm run examples/blake2b.wast
  ```

## Disassemble a WebAssembly module

  ```
  wavm disassemble examples/zlib.wasm zlib.wast
  ```

## Set up an object cache

  WAVM supports caching compiled object code for WebAssembly modules. The cache is configured by
  setting a `WAVM_OBJECT_CACHE_DIR` environment variable to point to a directory to store the
  cache in.

  On Linux or MacOS:
  ```
  export WAVM_OBJECT_CACHE_DIR=/path/to/existing/directory
  wavm run huge.wasm # Slow
  wavm run huge.wasm # Fast
  ```

  The first time `wavm` loads `huge.wasm`, it must spend some time to compile it to object code.
  The second time `wavm` loads `huge.wasm`, it just needs to load its object code from the cache.
  For small modules, that may not save much time, but for large WebAssembly modules, that can be
  much faster than compiling the module from scratch.

## Continue to: [Building WAVM from source](Building.md)
