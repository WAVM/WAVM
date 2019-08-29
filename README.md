[![Travis Build Status](https://travis-ci.com/WAVM/WAVM.svg?branch=master)](https://travis-ci.com/WAVM/WAVM) [![Azure Build Status](https://dev.azure.com/WAVM/WAVM/_apis/build/status/WAVM.WAVM)](https://dev.azure.com/WAVM/WAVM/_build/latest?definitionId=1)

[License](LICENSE) | [Primary repo](https://github.com/WAVM/WAVM) | [Discord](https://discordapp.com/invite/fchkxFM)

[Getting Started](Docs/GettingStarted.md) | [Building WAVM from Source](Docs/Building.md) | [Exploring the WAVM source](CodeOrganization.md)

# WAVM

##### WAVM is a WebAssembly virtual machine, designed for use in non-web applications.

### Fast

WAVM uses [LLVM](https://llvm.org/) to compile WebAssembly code to machine code with close to
native performance. It can even beat native performance in some cases, thanks to the ability to
generate machine code tuned for the exact CPU that is running the code.

WAVM also leverages virtual memory and signal handlers to execute WebAssembly's bounds-checked
memory accesses at the same cost as a native, unchecked memory access.

### Safe

WAVM prevents WebAssembly code from accessing state outside of WebAssembly virtual machine*, or
calling native code that you do not explicitly link with the WebAssembly module.

*&nbsp;WAVM <i>is</i> vulnerable to some side-channel attacks, such as Spectre variant 2. WAVM may
add further mitigations for specific side-channel attacks, but it's impractical to guard against
all such attacks. You should use another form of isolation, such as OS processes, to protect
sensitive data from untrusted WebAssembly code.

### WebAssembly 1.0+

WAVM fully supports WebAssembly 1.0, plus many proposed extensions to it:
* [WASI](https://github.com/WebAssembly/WASI)
* [128-bit SIMD](https://github.com/WebAssembly/simd)
* [Threads](https://github.com/WebAssembly/threads)
* [Reference types](https://github.com/WebAssembly/reference-types)
* [Multiple results and block parameters](https://github.com/WebAssembly/multi-value)
* [Bulk memory operations](https://github.com/webassembly/bulk-memory-operations)
* [Non-trapping float-to-int conversions](https://github.com/WebAssembly/nontrapping-float-to-int-conversions)
* [Sign-extension instructions](https://github.com/WebAssembly/sign-extension-ops)
* [Exception handling](https://github.com/WebAssembly/exception-handling)
* [Extended name section](https://github.com/WebAssembly/extended-name-section)

### Portable

WAVM is written in portable C/C++, with a small amount of architecture-specific assembly and LLVM
IR generation code.

WAVM is tested on and fully supports X86-64 Windows, MacOS, and Linux. It is designed to run on any
POSIX-compatible system, but is not routinely tested on other systems.

Support for AArch64 is a [work-in-progress](#76).
WAVM mostly works on AArch64 Linux, but with some known bugs with handling WebAssembly stack
overflow and partially out-of-bounds stores.

WAVM's runtime requires a 64-bit virtual address space, and so is not portable to 32-bit hosts.
However, WAVM's assembler and disassembler work on 32-bit hosts.

[Portability Matrix](Docs/PortabilityMatrix.md)

