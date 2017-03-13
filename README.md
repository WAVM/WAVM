[![Linux/OSX Build Status](https://travis-ci.org/AndrewScheidecker/WAVM.svg?branch=master)](https://travis-ci.org/AndrewScheidecker/WAVM)

# Overview

This is a prototype of a simple standalone VM for WebAssembly. It can load two forms of WebAssembly code:
* The text format defined by the [WebAssembly reference interpreter](https://github.com/WebAssembly/spec/tree/master/ml-proto). [The tests](Tests/WAST) are just copied from the tests used by the reference interpreter, and are covered by its [license](spec.LICENSE). WAVM passes most of these spec tests; you can look at [WAVM_known_failures.wast](Test/spec/WAVM_known_failures.wast) to see the tests it DOESN'T pass.
* The official binary format: this is a work in progress, both in WAVM and in the other tools that use it, so don't be surprised if there are incompatibilities.

# Building and running it

To build it, you'll need CMake and [LLVM 3.8](http://llvm.org/releases/download.html#3.8.0). If CMake can't find your LLVM directory, you can manually give it the location in the LLVM_DIR CMake configuration variable. Note that on Windows, you must compile LLVM from source, and manually point the LLVM_DIR configuration variable at `<LLVM build directory>/share/llvm/cmake`.

I've tested it on Windows with Visual C++ 2015, Linux with GCC and Clang, and MacOS with Xcode/Clang. Travis CI is testing Linux/GCC, Linux/clang, and OSX/clang.

The primary executable is `wavm`:
```
Usage: wavm [switches] [programfile] [--] [arguments]
  in.wast|in.wasm		Specify program file (.wast/.wasm)
  -f|--function name		Specify function name to run in module rather than main
  -c|--check			Exit after checking that the program is valid
  -d|--debug			Write additional debug information to stdout
  --				Stop parsing arguments
```

`wavm` will load a WebAssembly file and call `main` (or a specified function).  Example programs to try without changing any code include those found in the Test/wast and Test/spec directory such as the following:

```
wavm ../Test/wast/helloworld.wast
wavm ../Test/zlib/zlib.wast
wavm ../Test/spec/fac.wast --function fac-iter 5
```

WebAssembly programs that export a main function with the standard parameters will be passed in the command line arguments.  If the same main function returns a i32 type it will become the exit code.  WAVM supports Emscripten's defined I/O functions so programs can read from stdin and write to stdout and stderr.  See [echo.wast](Test/wast/echo.wast) for an example of a program that echos the command line arguments back out through stdout.

There are a few additional executables that can be used to assemble the WAST file into a binary, disassemble a binary into a WAST file, and to execute a test script defined by a WAST file (see the [Test/spec directory](Test/spec) for examples of the syntax).

```
Assemble in.wast out.wasm
Disassemble in.wasm out.wast
Test in.wast
```

# Design

Parsing the WebAssembly text format goes through a [generic S-expression parser](Source/Core/SExpressions.cpp) that creates a tree of nodes, symbols, integers, etc. The symbols are statically defined strings, and are represented in the tree by an index. After creating that tree, it is parsed into a WebAssembly module by [WASTParse.cpp](Source/WAST/WASTParse.cpp). The parsed module encodes the WAST expressions as a stack machine byte code which can be directly serialized to and from disk.

Loading a binary WebAssembly module deserializes the module-scoped definitions into [C++ structures](Include/WebAssembly/Module.h), but leaves the function code as the same byte codes saved to disk. However, it validates the byte code to reject all forms of undefined code before any other consumer may encounter it.

The [Runtime](Source/Runtime/) is the primary consumer of the byte code. It provides an [API](Include/Runtime/Runtime.h) for instantiating WebAssembly modules and calling functions exported from them. To instantiate a module, it [initializes the module's runtime environment](Source/Runtime/ModuleInstance.cpp) (globals, memory objects, and table objects), [translates the byte code into LLVM IR](Source/Runtime/LLVMEmitIR.cpp), and [uses LLVM to generate machine code](Source/Runtime/LLVMJIT.cpp) for the module's functions.

# License

Copyright (c) 2016, Andrew Scheidecker
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of WAVM nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.