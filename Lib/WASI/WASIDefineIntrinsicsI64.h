#undef WASIAddressIPtr
#undef WASIADDRESSIPTR_MAX
#undef WASIADDRESSIPTR_FORMAT
#undef WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR
#define WASIAddressIPtr uint64_t
#define WASIADDRESSIPTR_MAX UINT64_MAX
#define WASIADDRESSIPTR_FORMAT "%" PRIu64
#undef STRINGIFY
#define STRINGIFY(s) #s
#define WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(module_, nameString, Result, cName, ...) 	\
	WAVM_DEFINE_INTRINSIC_FUNCTION( module_ , nameString "_i64", Result , cName##_i64 , ##__VA_ARGS__)
#undef STRINGIFY
