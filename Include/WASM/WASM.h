#pragma once

#ifndef WASM_API
#define WASM_API DLL_IMPORT
#endif

#include "Inline/BasicTypes.h"

namespace IR
{
	struct Module;
}
namespace Serialization
{
	struct InputStream;
	struct OutputStream;
}

namespace WASM
{
	WASM_API void serialize(Serialization::InputStream& stream, IR::Module& module);
	WASM_API void serialize(Serialization::OutputStream& stream, const IR::Module& module);
}
