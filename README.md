# Overview

This is a prototype VM for WebAssembly. It can load two forms of WebAssembly code:
* Most of the text format defined by the [WebAssembly reference interpreter](https://github.com/WebAssembly/spec/tree/master/ml-proto). [The tests](Tests/WAST) are so far just copied from the reference interpreter, and are covered by its [license](spec.LICENSE). What I don't support are multiple return values and unaligned loads/stores, but AFAIK everything else does.
* The binary format defined by the [polyfill prototype](https://github.com/WebAssembly/polyfill-prototype-1). The code was evolved from that project, so should be considered to be covered by its [license](polyfill.LICENSE).

# Building and running it

To build it, you'll need [LLVM 3.7 built from source](http://llvm.org/releases/download.html#3.7.0). The VC project expects it in WAVM/External/llvm and WAVM/External/LLVM.build, but it's probably easiest to point it to wherever your copy is.

I've only tried building it with VS2013 so far, and there are a few Windows-specific bits of code related to virtual memory management. However, it should all be in Windows.cpp, and easily ported to Linux or MacOS. There are also no makefiles yet. TODO...

The command-line usage is:
```
WAVM -binary in.wasm in.js.mem functionname
WAVM -text in.wast functionname
```

That will load a text or binary WebAssembly file, and call the named exported function. The type of the function must be I64->I64 at the moment, though that can be easily changed in the source code. A good command-line to try without changing any code:

```WAVM -text ../Test/WAST/fac.wast fac-iter```

# Design

Parsing the WebAssembly text format goes through a [generic S-expression parser](Source/SExpressions.cpp) that creates a tree of nodes, symbols, integers, etc. The symbols are statically defined strings, and are represented in the tree by an index. After creating that tree, it is transformed into a WebAssembly-like AST by [WebAssemblyText.cpp](Source/WebAssembly/WebAssemblyText.cpp).

Decoding the polyfill binary format also produces the same AST, so while it sticks pretty closely to the syntax of the text format, there are a few differences to accomodate the polyfill format:
* WebAssembly only supports I32 and I64 integer value types, with loads and stores supporting explicitly converting to and from I8s or I16s in memory. The WAVM AST just supports general I8 and I16 values.
* WebAssembly's switch statement is more restrictive than the polyfill prototype's switch statements, so I use a more general switch construct as described [here](https://github.com/WebAssembly/design/issues/322).
* WebAssembly's branching is also a little more constrained than the polyfill prototype. It only allows breaking out of a label node, which is sufficient to encode any control flow possible in JavaScript, but requires extra label nodes to do so. Instead of creating more label nodes when decoding the polyfill prototype format, I give each loop node a label that represents continuing out of it, as well as a label that represents breaking out of it. A branch node can then reference the appropriate in-scope label to break or continue out of the loop.
* WebAssembly doesn't yet have explicit boolean types, but I like them, and I think there's a concensus to add them, so I have boolean AST types.

The AST also has a few concepts that the text format doesn't. For example, it uses type classes to represent the idea of a set of types an operation can be defined on. Every AST opcode is specific to a type class, and so there is a different opcode enum for each type class. The type classes defined are Int, Float, Bool, and Void. This means that you need to know what type a subexpresion is to interpret its opcode, which is usually trivial, but occasionally requires a *TypedExpression* to wrap up the subexpression with an explicit type. Types are otherwise implicit.

Both the polyfill binary decoder and the WebAssembly text parser are expected to validate that the program is well typed. The AST uses Error nodes to isolate most parse or type errors to a subtree of the AST. Given an AST without Error nodes, you should assume it is well typed.

After it has constructed the AST, it will convert it to LLVM IR, and feed that to LLVM's MCJIT to generate executable machine code, and call it!

The generated code should be unable to access any memory outside of the addresses allocated to it. The VM reserves 4GB of addresses at startup for the VM, and only allows access to 32-bit addresses within that 4GB. WebAssembly will soon support 64-bit addresses, and I expect to support that by reserving most of the process's virtual address space for VM memory, and generating code to mask VM addresses to this subset of the virtual address space.

# License

Copyright (c) 2015, Andrew Scheidecker
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of WAVM nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.