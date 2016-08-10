[![Linux/OSX Build Status](https://travis-ci.org/AndrewScheidecker/WAVM.svg?branch=master)](https://travis-ci.org/AndrewScheidecker/WAVM)

# Overview

This is a prototype of a simple standalone VM for WebAssembly. It can load two forms of WebAssembly code:
* Most of the text format defined by the [WebAssembly reference interpreter](https://github.com/WebAssembly/spec/tree/master/ml-proto). [The tests](Tests/WAST) are so far just copied from the reference interpreter, and are covered by its [license](spec.LICENSE). What I don't support are multiple return values and unaligned loads/stores, but AFAIK everything else does.
* The binary format defined by the [polyfill prototype](https://github.com/WebAssembly/polyfill-prototype-1). The code was evolved from that project, so should be considered to be covered by its [license](polyfill.LICENSE).

It does not yet handle loading the official binary format for WebAssembly, though I plan to support it once it's closer to done.

# Building and running it

To build it, you'll need CMake and [LLVM 3.8](http://llvm.org/releases/download.html#3.8.0). If CMake can't find your LLVM directory, you can manually give it the location in the LLVM_DIR CMake configuration variable. Note that on Windows, you must compile LLVM from source, and manually point the LLVM_DIR configuration variable at `<LLVM build directory>/share/llvm/cmake`.

I've tested it on Windows with Visual C++ 2013 and 2015, Linux with GCC and Clang, and MacOS with Xcode/Clang. Travis CI is testing Linux/GCC, Linux/clang, and OSX/clang.

```
Usage: wavm [switches] [programfile] [--] [arguments]
  --text in.wast                Specify text program file (.wast)
  --binary in.wasm in.js.mem    Specify binary program file (.wasm) and memory file (.js.mem)
  -f|--function name            Specify function name to run in module rather than main
  -c|--check                    Exit after checking that the program is valid
  --                            Stop parsing arguments

PrintWAST -binary in.wasm in.js.mem out.wast
PrintWAST -text in.wast out.wast
```

wavm will load a WebAssembly file and call main (or a specified function).  Example programs to try without changing any code include those found in the Test/wast and Test/spec directory such as the following:

```
wavm ../Test/wast/helloworld.wast
wavm --text ../Test/spec/fac.wast --function fac-iter 5
```

WebAssembly programs that export a main function with the standard parameters will be passed in the command line arguments.  If the same main function returns a i32 type it will become the exit code.  WAVM supports Emscripten's defined I/O functions so programs can read from stdin and write to stdout and stderr.  See [echo.wast](Test/wast/echo.wast) for an example of a program that echos the command line arguments back out through stdout.

# Design

Parsing the WebAssembly text format goes through a [generic S-expression parser](Source/Core/SExpressions.cpp) that creates a tree of nodes, symbols, integers, etc. The symbols are statically defined strings, and are represented in the tree by an index. After creating that tree, it is transformed into a WebAssembly-like AST by [WebAssemblyTextParse.cpp](Source/WebAssembly/WebAssemblyTextParse.cpp).

Decoding the polyfill binary format also produces the same AST, so while it sticks pretty closely to the syntax of the text format, there are a few differences to accomodate the polyfill format:
* WebAssembly only supports I32 and I64 integer value types, with loads and stores supporting explicitly converting to and from I8s or I16s in memory. The WAVM AST just supports general I8 and I16 values.

The AST also has a few concepts that the text format doesn't. For example, it uses type classes to represent the idea of a set of types an operation can be defined on. Every AST opcode is specific to a type class, and so there is a different opcode enum for each type class. The type classes defined are Int, Float, Bool, and Void. This means that you need to know what type a subexpresion is to interpret its opcode, which is usually trivial, but occasionally requires a *TypedExpression* to wrap up the subexpression with an explicit type. Types are otherwise implicit.

Both the polyfill binary decoder and the WebAssembly text parser are expected to validate that the program is well typed. The AST uses Error nodes to isolate most parse or type errors to a subtree of the AST. Given an AST without Error nodes, you should assume it is well typed.

After it has constructed the AST, it will convert it to LLVM IR, and feed that to LLVM's MCJIT to generate executable machine code, and call it!

The generated code should be unable to access any memory outside of the addresses allocated to it. The VM reserves 4TB of addresses at startup for the VM, and masks addresses to be within those 4TBs.

# License

Copyright (c) 2016, Andrew Scheidecker
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of WAVM nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.