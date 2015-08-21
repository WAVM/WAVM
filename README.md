# Overview

This is a simple VM for WebAssembly as it was defined by the [polyfill prototype](https://github.com/WebAssembly/polyfill-prototype-1). The code was evolved from that project, so should be considered to be covered by its license ([polyfill.LICENSE](polyfill.LICENSE) and [polyfill.AUTHORS](polyfill.AUTHORS)).

The VM uses LLVM's MCJIT to generate native code, though it's used as more of a very tardy AOT compiler than a JIT at the point.

To build it, you'll need [LLVM 3.6](https://github.com/llvm-mirror/llvm/tree/release_36). The VC project expects it in WAVM/External/llvm and WAVM/External/LLVM.build, but it's probably easiest to point it to wherever your copy is.

My next task is to update to running the newer version of the WebAssembly standard as defined by the new [reference implementation](https://github.com/WebAssembly/spec) (using SExpressions rather than a binary format)

# License

Copyright (c) 2015, Andrew Scheidecker
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of WAVM nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.