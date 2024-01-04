#undef WASIAddressIPtr
#undef WASIADDRESSIPTR_MAX
#undef WASIADDRESSIPTR_FORMAT
#undef WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR
#undef UNIMPLEMENTED_SYSCALL_IPTR
#undef TRACE_SYSCALL_IPTR
#undef WAVM_WASM_WASI32
#undef WAVM_WASM_WASI64
#undef wasi_iovec_iptr
#undef wasi_ciovec_iptr
#undef wasi_prestat_iptr
#define WASIAddressIPtr uint32_t
#define WASIADDRESSIPTR_MAX UINT32_MAX
#define WASIADDRESSIPTR_FORMAT "0x%08" PRIx32
#define WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR WAVM_DEFINE_INTRINSIC_FUNCTION
#define UNIMPLEMENTED_SYSCALL_IPTR UNIMPLEMENTED_SYSCALL
#define TRACE_SYSCALL_IPTR TRACE_SYSCALL
#define wasi_iovec_iptr __wasi_iovec_t
#define wasi_ciovec_iptr __wasi_ciovec_t
#define wasi_prestat_iptr __wasi_prestat_t
