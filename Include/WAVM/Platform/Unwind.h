#pragma once

#include <optional>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Config.h"
#include "WAVM/Platform/CPU.h"

namespace WAVM {
	struct StringBuilderBase;
}

namespace WAVM { namespace Platform {

	// Unwind info for registering JIT code with the system unwinder.
	// On Windows, `data` points to the RUNTIME_FUNCTION table (.pdata).
	// On POSIX, `data` points to the DWARF .eh_frame section.
	// On macOS, `compactUnwind` additionally points to synthesized __unwind_info.
	struct UnwindInfo
	{
		const U8* data = nullptr;
		Uptr dataNumBytes = 0;
		const U8* compactUnwind = nullptr;
		Uptr compactUnwindNumBytes = 0;
	};

	// Opaque handle returned by registerUnwindData, defined in platform implementations.
	struct UnwindRegistration;

	WAVM_API UnwindRegistration* registerUnwindData(const U8* imageBase,
													Uptr imageNumPages,
													const UnwindInfo& unwindInfo);
	WAVM_API void deregisterUnwindData(UnwindRegistration* registration);

	//
	// Platform-independent unwind API
	//

	// Result of stepping the unwind cursor
	enum class UnwindStepResult
	{
		success, // Successfully stepped to next frame
		end,     // Reached end of stack (no more frames)
		error    // Error during unwinding
	};

	struct UnwindOp;

	// Procedure/function info from unwind data
	struct UnwindProcInfo
	{
		Uptr startIP = 0;
		Uptr endIP = 0;
		Uptr lsda = 0;
		Uptr handler = 0;

		// Decode unwind ops from this procedure's unwind data.
		WAVM_API Uptr decodeUnwindOps(UnwindOp* outOps, Uptr maxOps) const;

	private:
		friend struct UnwindState;
		const U8* unwindData = nullptr;
		Uptr unwindDataSize = 0;
		U32 compactEncoding = 0;
	};

	// UnwindState holds platform-specific unwind context and cursor.
	// The storage is sized at configure time to fit the platform's structures.
	// Copy/move operations are implemented in platform-specific code.
	struct UnwindState
	{
		WAVM_API UnwindState(const UnwindState& other);
		WAVM_API UnwindState(UnwindState&& other) noexcept;
		WAVM_API UnwindState& operator=(const UnwindState& other);
		WAVM_API UnwindState& operator=(UnwindState&& other) noexcept;

		// Capture unwind state from current execution context.
		// The returned state starts at the caller of capture() (capture itself is skipped).
		// numFramesToSkip additional frames are skipped beyond the caller.
		// Returns an invalid state on failure (e.g. on platforms where unwinding is unsupported).
		WAVM_API static UnwindState capture(Uptr numFramesToSkip = 0);

		// Step to the next frame.
		WAVM_API UnwindStepResult step();

		// Get procedure info for current unwind position.
		WAVM_API bool getProcInfo(UnwindProcInfo& outInfo) const;

		// Get a CPU register value at the current unwind position.
		WAVM_API std::optional<Uptr> getRegister(HostCPURegister reg) const;

	private:
		friend UnwindState makeUnwindStateFromSignalContext(void*);
		UnwindState() = default;
		alignas(WAVM_PLATFORM_UNWIND_STATE_ALIGN) U8 storage[WAVM_PLATFORM_UNWIND_STATE_SIZE];
	};

	//
	// Unwind op iteration (for CFI/unwind code disassembly)
	//

	struct UnwindOp
	{
		Uptr codeOffset = 0;

		// Format as human-readable string into the given string builder.
		WAVM_API void format(StringBuilderBase& sb) const;

	private:
		friend struct UnwindProcInfo;
		U8 opcode = 0;
		U32 reg = 0;
		I64 operand = 0;
		[[maybe_unused]] U32 compactEncoding = 0;
	};

}}

#ifndef _WIN32
// Forward declarations for Itanium C++ ABI unwind types.
struct _Unwind_Exception;
struct _Unwind_Context;

namespace WAVM { namespace Platform {
	// C++ personality function that logs unwind activity when the traceUnwind
	// log category is enabled, otherwise delegates to __gxx_personality_v0.
	WAVM_API int DebugPersonalityFunction(int version,
										  int actions,
										  U64 exceptionClass,
										  _Unwind_Exception* unwind_exception,
										  _Unwind_Context* context);
}}
#endif
