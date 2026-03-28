#pragma once

#include <string>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/FixedString.h"
#include "WAVM/Inline/StringBuilder.h"

namespace WAVM { namespace Platform {
	// Describes the source of an instruction in a native module.
	// Uses fixed-capacity strings for signal safety (no heap allocation).
	struct InstructionSource
	{
		FixedString<256> module;
		FixedString<256> function;
		Uptr instructionOffset;

		InstructionSource() : instructionOffset(0) {}

		friend std::string asString(const InstructionSource& source)
		{
			std::string result = source.module.c_str();
			if(!source.function.empty())
			{
				result += '!';
				result += source.function.c_str();
			}
			result += '+';
			result += std::to_string(source.instructionOffset);
			return result;
		}

		// Signal-safe formatting into a StringBuilderBase.
		friend void appendToString(StringBuilderBase& stringBuilder,
								   const InstructionSource& source)
		{
			if(!source.function.empty())
			{
				stringBuilder.appendf("%s!%s+%" WAVM_PRIuPTR,
									  source.module.c_str(),
									  source.function.c_str(),
									  source.instructionOffset);
			}
			else
			{
				stringBuilder.appendf(
					"%s+%" WAVM_PRIuPTR, source.module.c_str(), source.instructionOffset);
			}
		}
	};

	// Looks up the source of an instruction from a native module.
	WAVM_API bool getInstructionSourceByAddress(Uptr ip, InstructionSource& outSource);

	// Prints a profile of heap and virtual memory allocations.
	WAVM_API void printMemoryProfile();

	WAVM_API void registerVirtualAllocation(Uptr numBytes);
	WAVM_API void deregisterVirtualAllocation(Uptr numBytes);
}}
