#pragma once

#include "Core/Core.h"
#include "AST/AST.h"

#include <string>

#ifndef ASMJS_API
	#define ASMJS_API DLL_IMPORT
#endif

namespace ASMJS
{
	ASMJS_API void print(std::ostream& outputStream,const AST::Module* module);
}
