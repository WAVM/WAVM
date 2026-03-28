#pragma once

#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/Unwind.h"

namespace WAVM { namespace Platform {
	struct Signal
	{
		enum class Type
		{
			invalid = 0,
			accessViolation,
			stackOverflow,
			intDivideByZeroOrOverflow
		};

		Type type = Type::invalid;

		union
		{
			struct
			{
				Uptr address;
			} accessViolation;
		};
	};

	WAVM_API bool catchSignals(void (*thunk)(void*),
							   bool (*filter)(void*, Signal, UnwindState&&),
							   void* argument);
}}
