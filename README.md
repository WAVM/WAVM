[![Linux/OSX Build Status](https://travis-ci.com/WAVM/WAVM.svg?branch=master)](https://travis-ci.com/WAVM/WAVM)

[![Windows Build Status](https://dev.azure.com/WAVM/WAVM/_apis/build/status/WAVM.WAVM)](https://dev.azure.com/WAVM/WAVM/_build/latest?definitionId=1)

Primary repo: https://github.com/WAVM/WAVM

Discord (chat room): https://discordapp.com/invite/fchkxFM

# Overview

This is a standalone VM for WebAssembly. It can load both the standard binary format, and the text format defined by the [WebAssembly reference interpreter](https://github.com/WebAssembly/spec/tree/master/interpreter). For the text format, it can load both the standard stack machine syntax and the old-fashioned AST syntax used by the reference interpreter, and all of the testing commands.

# Building and running it

To build it, you'll need CMake and [LLVM 6.0](http://llvm.org/releases/download.html#6.0.0). If CMake can't find your LLVM directory, you can manually give it the location in the LLVM_DIR CMake configuration variable. Note that on Windows, you must compile LLVM from source, and manually point the LLVM_DIR configuration variable at `<LLVM build directory>\lib\cmake\llvm`.

### Building WAVM on Windows 

**1.) Install the [Visual Studio C++ Build Tools for Visual Studio 2015 or 2017](http://landinghub.visualstudio.com/visual-cpp-build-tools)**

Take note of which version you have installed:

- If using Visual Studio 2015, use `-G"Visual Studio 14 Win64"` for the `<VS Generator Directive>` placeholder below
- If using Visual Studio 2017, use `-G"Visual Studio 15 Win64"` for the `<VS Generator Directive>` placeholder below

**2.) Build LLVM x64 on Windows with Visual Studio**

Create an llvm_build directory, navigate to that directory and run:

    cmake -Thost=x64 <VS Generator Directive> -DCMAKE_INSTALL_PREFIX=<desired install path for LLVM> <path to LLVM source>

Open the generated LLVM.sln locateed within the 'llvm_build' directory in Visual Studio and build the "INSTALL" Project

The output binaries should be located in `<desired install path for LLVM>`

**3.) Build WAVM x64 on Windows with Visual Studio against LLVM x64**

Create a wavm_build directory, navigate to that directory and run:

    cmake -Thost=x64 <VS Generator Directive> -DLLVM_DIR=<LLVM build directory>\lib\cmake\llvm <path to WAVM source>

Open the generated WAVM.sln located within the 'wavm_build' directory in Visual Studio and build the "ALL_BUILD" Project

The output binaries should be located in `wavm_build\bin`

# Usage

I've tested it on Windows with Visual C++ 2015/2017, Linux with GCC and clang, and MacOS with Xcode/clang. Travis CI is testing Linux/GCC, Linux/clang, and OSX/clang.

The primary executable is `wavm-run`:
```
Usage: wavm-run [switches] [programfile] [--] [arguments]
  in.wast|in.wasm		Specify program file (.wast/.wasm)
  -f|--function name		Specify function name to run in module rather than main
  -c|--check			Exit after checking that the program is valid
  -d|--debug			Write additional debug information to stdout
  --				Stop parsing arguments
```

`wavm-run` will load a WebAssembly file and call `main` (or a specified function).  Example programs to try without changing any code include those found in the Examples and Test/spec directory such as the following:

```
wavm-run Examples/helloworld.wast
wavm-run Examples/Benchmark/Benchmark.wast
wavm-run Examples/zlib.wast
```

WebAssembly programs that export a main function with the standard parameters will be passed in the command line arguments.  If the same main function returns a i32 type it will become the exit code.  WAVM supports Emscripten's defined I/O functions so programs can read from stdin and write to stdout and stderr.  See [echo.wast](Examples/echo.wast) for an example of a program that echos the command line arguments back out through stdout.

There are a few additional executables that can be used to assemble the WAST file into a binary:

```
wavm-as in.wast out.wasm
```

wavm-disas a binary into a WAST file:

```
wavm-disas in.wasm out.wast
```

and to execute a test script defined by a WAST file (see the [Test/spec directory](Test/spec) for examples of the syntax):

```
Test in.wast
```

# Architecture

## IR

The [IR](Include/WAVM/IR) (Intermediate Representation) is the glue that the WAST parser, the WASM serialization, and the Runtime communicate through. It closely mirrors the semantics of the WebAssembly binary format, but is easier to work with in memory.

## Parsing

Parsing the WebAssembly text format uses a table-driven deterministic finite automaton to scan the input string for tokens. The tables are generated from a set of tokens that match literal strings, and a set of tokens that match regular expressions. The parser is a standard recursive descent parser.

## Runtime

The [Runtime](Lib/Runtime/) is the primary consumer of the byte code. It provides an [API](Include/WAVM/Runtime/Runtime.h) for instantiating WebAssembly modules and calling functions exported from them. To instantiate a module,
1) Translates the byte code into LLVM IR([1](Lib/LLVMJIT/EmitModule.cpp), [2](Lib/LLVMJIT/EmitFunction.cpp), and the various `Emit*.cpp` in [Lib/LLVMJIT](Lib/LLVMJIT))
2) [Uses LLVM to compile the LLVM IR to object code](Lib/LLVMJIT/LLVMCompile.cpp) for the module's functions, with symbols in place of literal values for things like the module's imports.
3) [Instantiates the module's runtime environment](Lib/Runtime/Module.cpp) (globals, memory objects, and table objects).
4) [Loads the object code](Lib/LLVMJIT/LLVMModule.cpp) created by step 2, replacing symbols with the appropriate values from the module's environment.

# License

WAVM is provided under the terms of [LICENSE.md](LICENSE.md).
