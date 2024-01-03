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
#define WASIAddressIPtr uint64_t
#define WASIADDRESSIPTR_MAX UINT64_MAX
#define WASIADDRESSIPTR_FORMAT "0x%016" PRIx64
#define WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(module_, nameString, Result, cName, ...)               \
	WAVM_DEFINE_INTRINSIC_FUNCTION(                                                                \
		module_, nameString "_wasm64", Result, cName##_wasm64, ##__VA_ARGS__)
#define UNIMPLEMENTED_SYSCALL_IPTR(syscallName, argFormat, ...)                                    \
	UNIMPLEMENTED_SYSCALL(syscallName "_wasm64", argFormat, ##__VA_ARGS__)
#define TRACE_SYSCALL_IPTR(syscallName, argFormat, ...)                                            \
	TRACE_SYSCALL(syscallName "_wasm64", argFormat, ##__VA_ARGS__)
#define wasi_iovec_iptr __wasi_iovec_i64
#define wasi_ciovec_iptr __wasi_ciovec_i64
#define wasi_prestat_iptr __wasi_prestat_i64
