#pragma once

#include "WAVM/IR/Validate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Logging/Logging.h"

namespace WAVM { namespace IR {
	struct Module;
}}
namespace WAVM { namespace Serialization {
	struct InputStream;
	struct OutputStream;
}}

namespace WAVM { namespace WASM {
	// Serializes a module to or from a stream. May throw FatalSerializationException or
	// ValidationException.
	WASM_API void serialize(Serialization::InputStream& stream, IR::Module& module);
	WASM_API void serialize(Serialization::OutputStream& stream, const IR::Module& module);

	// Loads a binary module, catching any exceptions that might be
	WASM_API bool loadBinaryModule(const void* wasmBytes,
								   Uptr numBytes,
								   IR::Module& outModule,
								   Log::Category errorCategory = Log::error);
}}
