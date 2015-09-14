#pragma once

#include "Core/Core.h"
#include "AST/AST.h"

#include <string>

namespace ASMJS
{
	void print(std::ostream& outputStream,const AST::Module* module);
}