#pragma once

#ifndef WEBASSEMBLY_API
	#define WEBASSEMBLY_API DLL_IMPORT
#endif

#include "Core/Core.h"
#include "Core/Serialization.h"

namespace WebAssembly
{
	enum { maxMemoryPages = (uintptr)65535 };
	enum { numBytesPerPage = (uintptr)65536 };
	enum { numBytesPerPageLog2 = (uintptr)16 };
	
	struct Module;

	struct ValidationException
	{
		std::string message;
		ValidationException(std::string&& inMessage): message(inMessage) {}
	};

	WEBASSEMBLY_API void serialize(Serialization::InputStream& stream,Module& module);
	WEBASSEMBLY_API void serialize(Serialization::OutputStream& stream,const Module& module);
	WEBASSEMBLY_API void validate(const Module& module);
}
