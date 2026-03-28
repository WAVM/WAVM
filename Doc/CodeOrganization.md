# Exploring the WAVM source

## Directory Structure

- **Lib/** - Core library implementations (compiled into libWAVM)
- **Include/WAVM/** - Public header files organized by component
- **Programs/wavm/** - Implementation of the wavm program
- **Test/** - Test suites and benchmarks
- **Examples/** - Example WebAssembly modules and embedder code
- **Build/** - Build system and scripts
- **Doc/** - Documentation
- **ThirdParty/** - External dependencies

## Core Library Components (Lib/)

Each component has a corresponding header directory in `Include/WAVM/`.

| Component | Purpose |
|-----------|---------|
| **IR** | WebAssembly module structure, type system, and validation |
| **LLVMJIT** | Compiles WebAssembly IR to native machine code using LLVM |
| **Runtime** | Execution engine, memory management, module instantiation |
| **WASM** | Binary WebAssembly format (.wasm) parsing and serialization |
| **WASTParse** | Text format (.wast/.wat) parsing |
| **WASTPrint** | IR to text format conversion (disassembly) |
| **Platform** | OS-specific implementations (POSIX/, Windows/) |
| **WASI** | WebAssembly System Interface implementation |
| **VFS** | Virtual filesystem abstraction for WASI |
| **ObjectCache** | JIT code caching using LMDB |
| **Logging** | Logging infrastructure |
| **NFA/RegExp** | Lexer support for WASTParse |
| **wavm-c** | Standard WebAssembly C API bindings |

## "Inline" Header-Only Library Component (Include/WAVM/Inline/)

Header-only library component including hash containers (`HashMap.h`, `HashSet.h`), assertions, error handling, serialization, and 128-bit integer support.

## Test Organization (Test/)

| Directory | Contents |
|-----------|----------|
| **wavm/** | WAVM-specific feature tests |
| **WebAssembly/spec/** | Official spec compliance tests |
| **WebAssembly/memory64/** | Memory64 proposal tests |
| **WebAssembly/threads/** | Threading/atomics tests |
| **WebAssembly/multi-memory/** | Multi-memory proposal tests |
| **wasi/** | WASI interface tests |
| **fuzz/** | Fuzzing targets |
| **benchmark/** | Performance benchmarks |

## Key API References

- **Runtime API:** `Include/WAVM/Runtime/Runtime.h`
- **IR Module:** `Include/WAVM/IR/Module.h`
- **IR Types:** `Include/WAVM/IR/Types.h`
- **IR Operators:** `Include/WAVM/IR/Operators.h`
- **WASI:** `Include/WAVM/WASI/WASI.h`
- **C API:** `Include/WAVM/wavm-c/wavm-c.h`

## Key Implementation References

- **Translation to LLVM IR:** `Lib/LLVMJIT/Emit*.cpp`
- **Module instantiation:** `Lib/Runtime/Instance.cpp`
- **WASM parsing:** `Lib/WASM/WASMSerialization.cpp`
- **WAST parsing:** `Lib/WASTParse/Parse*.cpp`
- **IR validation:** `Lib/IR/Validate.cpp`
- **WASI implementation:** `Lib/WASI/WASI*.cpp`
