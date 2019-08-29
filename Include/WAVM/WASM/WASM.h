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
	// Saves a binary module.
	WASM_API void saveBinaryModule(Serialization::OutputStream& stream, const IR::Module& module);

	// Loads a binary module, returning either an error or a module.
	// If true is returned, the load succeeded, and outModule contains the loaded module.
	// If false is returned, the load failed. If outError != nullptr, *outError will contain the
	// error that caused the load to fail.
	struct LoadError
	{
		enum class Type
		{
			malformed,
			invalid
		};
		Type type;
		std::string message;
	};
	WASM_API bool loadBinaryModule(Serialization::InputStream& stream,
								   IR::Module& outModule,
								   LoadError* outError = nullptr);
}}
