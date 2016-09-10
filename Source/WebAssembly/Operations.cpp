#include "WebAssembly.h"
#include "Operations.h"

namespace WebAssembly
{
	const char* getOpcodeName(Opcode opcode)
	{
		switch(opcode)
		{
		#define VISIT_OPCODE(encoding,name,Immediates) case Opcode::name: return #name;
		ENUM_OPCODES(VISIT_OPCODE)
		#undef VISIT_OPCODE
		default: return "unknown";
		};
	}
}