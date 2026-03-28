#if WAVM_PLATFORM_POSIX

#include <stdlib.h>
#include <unwind.h>
#include <unwind_itanium.h>
#include <cinttypes>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <typeinfo>
#include "WAVM/DWARF/Constants.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/LEB128.h"
#include "WAVM/Inline/Serialization.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Unwind.h"

using namespace WAVM;
using namespace WAVM::DWARF;
using namespace WAVM::Serialization;

// The real C++ personality function from libcxxabi.
extern "C" _Unwind_Reason_Code __gxx_personality_v0(int version,
													_Unwind_Action actions,
													U64 exceptionClass,
													_Unwind_Exception* unwind_exception,
													_Unwind_Context* context);

// Read a DWARF-encoded pointer from the stream. For PC-relative encodings,
// lsdaBase must point to the start of the LSDA so the field address can be recovered.
static Uptr readEncodedPointer(MemoryInputStream& stream, const U8* lsdaBase, U8 encoding)
{
	Uptr result = 0;
	if(encoding == DW_EH_PE_omit) return result;

	// Remember the address of this field for PC-relative relocation.
	const U8* fieldAddr = lsdaBase + stream.position();

	switch(encoding & 0x0F)
	{
	case DW_EH_PE_absptr: {
		Uptr v = 0;
		tryRead(stream, v);
		result = v;
		break;
	}
	case DW_EH_PE_uleb128: {
		U64 v = 0;
		trySerializeVarUInt64(stream, v);
		result = Uptr(v);
		break;
	}
	case DW_EH_PE_sleb128: {
		I64 v = 0;
		trySerializeVarSInt64(stream, v);
		result = static_cast<Uptr>(Iptr(v));
		break;
	}
	case DW_EH_PE_udata2: {
		U16 v = 0;
		tryRead(stream, v);
		result = v;
		break;
	}
	case DW_EH_PE_udata4: {
		U32 v = 0;
		tryRead(stream, v);
		result = v;
		break;
	}
	case DW_EH_PE_udata8: {
		U64 v = 0;
		tryRead(stream, v);
		result = Uptr(v);
		break;
	}
	case DW_EH_PE_signed: {
		Iptr v = 0;
		tryRead(stream, v);
		result = static_cast<Uptr>(v);
		break;
	}
	case DW_EH_PE_sdata2: {
		I16 v = 0;
		tryRead(stream, v);
		result = static_cast<Uptr>(Iptr(v));
		break;
	}
	case DW_EH_PE_sdata4: {
		I32 v = 0;
		tryRead(stream, v);
		result = static_cast<Uptr>(Iptr(v));
		break;
	}
	case DW_EH_PE_sdata8: {
		I64 v = 0;
		tryRead(stream, v);
		result = static_cast<Uptr>(Iptr(v));
		break;
	}
	default:
		Log::printf(
			Log::traceUnwind, "  ERROR: Unknown pointer encoding format 0x%x\n", encoding & 0x0F);
		abort();
	}

	switch(encoding & 0x70)
	{
	case DW_EH_PE_absptr: break;
	case DW_EH_PE_pcrel:
		if(result) result += reinterpret_cast<Uptr>(fieldAddr);
		break;
	default:
		Log::printf(Log::traceUnwind,
					"  ERROR: Unknown pointer encoding relocation 0x%x\n",
					encoding & 0x70);
		abort();
	}

	if(result && (encoding & DW_EH_PE_indirect)) result = *reinterpret_cast<Uptr*>(result);

	return result;
}

struct scan_results
{
	I64 ttypeIndex;
	const U8* actionRecord;
	const U8* languageSpecificData;
	Uptr landingPad;
	void* adjustedPtr;
	_Unwind_Reason_Code reason;
};

// Minimal __cxa_exception header structure (layout must match libcxxabi).
// We only define the fields we need to set for exception handling to work.
// The _Unwind_Exception is at the end of this struct, so we can compute
// the header address from the unwind_exception pointer.
struct __cxa_exception
{
#if defined(__LP64__) || defined(_WIN64)
	void* reserve;
	size_t referenceCount;
#endif
	std::type_info* exceptionType;
	void (*exceptionDestructor)(void*);
	void (*unexpectedHandler)();
	std::terminate_handler terminateHandler;
	__cxa_exception* nextException;
	int handlerCount;
	int handlerSwitchValue;
	const U8* actionRecord;
	const U8* languageSpecificData;
	void* catchTemp;
	void* adjustedPtr;
#if !defined(__LP64__) && !defined(_WIN64)
	void* reserve;
	size_t referenceCount;
#endif
	_Unwind_Exception unwindHeader;
};

// Get the __cxa_exception header from an _Unwind_Exception pointer.
// The _Unwind_Exception is the last member of __cxa_exception.
static __cxa_exception* getExceptionHeader(_Unwind_Exception* unwind_exception)
{
	return reinterpret_cast<__cxa_exception*>(unwind_exception + 1) - 1;
}

static void scan_eh_tab(scan_results& results,
						_Unwind_Action actions,
						bool native_exception,
						_Unwind_Exception* unwind_exception,
						_Unwind_Context* context)
{
	results.ttypeIndex = 0;
	results.actionRecord = 0;
	results.languageSpecificData = 0;
	results.landingPad = 0;
	results.adjustedPtr = 0;
	results.reason = _URC_FATAL_PHASE1_ERROR;

	const U8* lsda = (const U8*)_Unwind_GetLanguageSpecificData(context);
	if(lsda == 0)
	{
		Log::printf(Log::traceUnwind, "  scan_eh_tab: No LSDA, continuing unwind\n");
		results.reason = _URC_CONTINUE_UNWIND;
		return;
	}
	Log::printf(Log::traceUnwind,
				"  scan_eh_tab: lsda=%p first bytes: %02x %02x %02x %02x\n",
				(void*)lsda,
				lsda[0],
				lsda[1],
				lsda[2],
				lsda[3]);
	results.languageSpecificData = lsda;

	MemoryInputStream stream = MemoryInputStream(lsda);

	Uptr ip = _Unwind_GetIP(context) - 1;
	Uptr funcStart = _Unwind_GetRegionStart(context);
	Uptr ipOffset = ip - funcStart;

	Log::printf(Log::traceUnwind,
				"  scan_eh_tab: ip=0x%" WAVM_PRIxPTR " funcStart=0x%" WAVM_PRIxPTR
				" ipOffset=0x%" WAVM_PRIxPTR "\n",
				ip,
				funcStart,
				ipOffset);

	// Parse LSDA header
	U8 lpStartEncoding = 0;
	tryRead(stream, lpStartEncoding);
	const U8* lpStart = (lpStartEncoding == DW_EH_PE_omit)
							? (const U8*)funcStart
							: (const U8*)readEncodedPointer(stream, lsda, lpStartEncoding);

	U8 ttypeEncoding = 0;
	tryRead(stream, ttypeEncoding);
	const U8* classInfo = NULL;
	if(ttypeEncoding != DW_EH_PE_omit)
	{
		U64 classInfoOffset = 0;
		trySerializeVarUInt64(stream, classInfoOffset);
		classInfo = lsda + stream.position() + Uptr(classInfoOffset);
	}

	U8 callSiteEncoding = 0;
	tryRead(stream, callSiteEncoding);
	U64 callSiteTableLength = 0;
	trySerializeVarUInt64(stream, callSiteTableLength);
	Uptr callSiteTableEndPos = stream.position() + Uptr(callSiteTableLength);
	Uptr actionTableStartPos = callSiteTableEndPos;

	Log::printf(Log::traceUnwind,
				"  LSDA: lpStartEnc=0x%x lpStart=%p ttypeEnc=0x%x classInfo=%p callSiteEnc=0x%x\n",
				lpStartEncoding,
				(void*)lpStart,
				ttypeEncoding,
				(void*)classInfo,
				callSiteEncoding);

	while(stream.position() < callSiteTableEndPos)
	{
		Uptr start = readEncodedPointer(stream, lsda, callSiteEncoding);
		Uptr length = readEncodedPointer(stream, lsda, callSiteEncoding);
		Uptr landingPad = readEncodedPointer(stream, lsda, callSiteEncoding);
		U64 actionEntry = 0;
		trySerializeVarUInt64(stream, actionEntry);

		Log::printf(Log::traceUnwind,
					"  CallSite: start=0x%" WAVM_PRIxPTR " len=0x%" WAVM_PRIxPTR
					" lp=0x%" WAVM_PRIxPTR " action=%" PRIu64 "\n",
					start,
					length,
					landingPad,
					actionEntry);

		if((start <= ipOffset) && (ipOffset < (start + length)))
		{
			Log::printf(Log::traceUnwind, "  Found matching call site!\n");

			if(landingPad == 0)
			{
				results.reason = _URC_CONTINUE_UNWIND;
				return;
			}
			landingPad = reinterpret_cast<Uptr>(lpStart) + landingPad;
			results.landingPad = landingPad;
			Log::printf(
				Log::traceUnwind, "  Computed landingPad: 0x%" WAVM_PRIxPTR "\n", landingPad);

			if(actionEntry == 0)
			{
				// Cleanup only
				results.reason
					= (actions & _UA_SEARCH_PHASE) ? _URC_CONTINUE_UNWIND : _URC_HANDLER_FOUND;
				return;
			}

			// Process action table. Action offsets are relative to the action record
			// start, so we track position in the LSDA via seek().
			Uptr actionPos = actionTableStartPos + Uptr(actionEntry - 1);
			while(true)
			{
				Uptr actionRecordPos = actionPos;
				stream.seek(actionPos);

				I64 ttypeIndex = 0;
				trySerializeVarSInt64(stream, ttypeIndex);
				Log::printf(Log::traceUnwind, "  Action: ttypeIndex=%" PRId64 "\n", ttypeIndex);

				if(ttypeIndex > 0)
				{
					void** thrownObjectPtr = reinterpret_cast<void**>(unwind_exception + 1);
					results.adjustedPtr = *thrownObjectPtr;
					results.ttypeIndex = ttypeIndex;
					results.actionRecord = lsda + actionRecordPos;
					results.reason = _URC_HANDLER_FOUND;
					Log::printf(Log::traceUnwind,
								"  Handler found! ttypeIndex=%" PRId64 " adjustedPtr=%p\n",
								ttypeIndex,
								results.adjustedPtr);
					return;
				}
				else if(ttypeIndex == 0)
				{
					// Cleanup
					if(!(actions & _UA_SEARCH_PHASE))
					{
						results.reason = _URC_HANDLER_FOUND;
						return;
					}
				}

				I64 actionOffset = 0;
				trySerializeVarSInt64(stream, actionOffset);
				if(actionOffset == 0)
				{
					results.reason = _URC_CONTINUE_UNWIND;
					return;
				}
				// Offset is relative to the action record start.
				actionPos = Uptr(Iptr(actionRecordPos) + actionOffset);
			}
		}
		else if(ipOffset < start)
		{
			Log::printf(Log::traceUnwind, "  ERROR: No call site for IP!\n");
			abort();
		}
	}

	Log::printf(Log::traceUnwind, "  ERROR: Fell through call site table!\n");
	abort();
}

static void set_registers(_Unwind_Exception* unwind_exception,
						  _Unwind_Context* context,
						  const scan_results& results)
{
	Log::printf(Log::traceUnwind,
				"  set_registers: context=%p landingPad=0x%" WAVM_PRIxPTR " ttypeIndex=%" PRId64
				"\n",
				(void*)context,
				results.landingPad,
				results.ttypeIndex);

	// Get current IP and region info before making changes
	Uptr currentIP = _Unwind_GetIP(context);
	Uptr regionStart = _Unwind_GetRegionStart(context);
	Log::printf(Log::traceUnwind,
				"  Current state: IP=0x%" WAVM_PRIxPTR " RegionStart=0x%" WAVM_PRIxPTR "\n",
				currentIP,
				regionStart);
	Log::printf(
		Log::traceUnwind, "  Landing pad to set: 0x%" WAVM_PRIxPTR "\n", results.landingPad);
	Log::printf(Log::traceUnwind,
				"  Landing pad offset from region start: 0x%" WAVM_PRIxPTR "\n",
				results.landingPad - regionStart);

	Log::printf(Log::traceUnwind, "  Calling _Unwind_SetGR for exception pointer...\n");
	_Unwind_SetGR(
		context, __builtin_eh_return_data_regno(0), reinterpret_cast<Uptr>(unwind_exception));

	Log::printf(Log::traceUnwind, "  Calling _Unwind_SetGR for ttypeIndex...\n");
	_Unwind_SetGR(
		context, __builtin_eh_return_data_regno(1), static_cast<Uptr>(results.ttypeIndex));

	Log::printf(Log::traceUnwind,
				"  Calling _Unwind_SetIP to 0x%" WAVM_PRIxPTR "...\n",
				results.landingPad);

	_Unwind_SetIP(context, results.landingPad);

	Log::printf(Log::traceUnwind, "  set_registers completed successfully\n");
}

// Save scan results to the exception header (called during search phase).
static void saveResultsToExceptionHeader(_Unwind_Exception* unwind_exception,
										 const scan_results& results)
{
	__cxa_exception* header = getExceptionHeader(unwind_exception);
	header->handlerSwitchValue = static_cast<int>(results.ttypeIndex);
	header->actionRecord = results.actionRecord;
	header->languageSpecificData = results.languageSpecificData;
	header->catchTemp = reinterpret_cast<void*>(results.landingPad);
	header->adjustedPtr = results.adjustedPtr;

	Log::printf(Log::traceUnwind,
				"  Saved to exception header: ttypeIndex=%d adjustedPtr=%p landingPad=%p\n",
				header->handlerSwitchValue,
				header->adjustedPtr,
				header->catchTemp);
}

// Load scan results from the exception header (called during cleanup phase).
static void loadResultsFromExceptionHeader(_Unwind_Exception* unwind_exception,
										   scan_results& results)
{
	__cxa_exception* header = getExceptionHeader(unwind_exception);
	results.ttypeIndex = header->handlerSwitchValue;
	results.actionRecord = header->actionRecord;
	results.languageSpecificData = header->languageSpecificData;
	results.landingPad = reinterpret_cast<Uptr>(header->catchTemp);
	results.adjustedPtr = header->adjustedPtr;
	results.reason = _URC_HANDLER_FOUND;

	Log::printf(Log::traceUnwind,
				"  Loaded from exception header: ttypeIndex=%" PRId64
				" adjustedPtr=%p landingPad=0x%" WAVM_PRIxPTR "\n",
				results.ttypeIndex,
				results.adjustedPtr,
				results.landingPad);
}

int Platform::DebugPersonalityFunction(int version,
									   int actions,
									   U64 exceptionClass,
									   _Unwind_Exception* unwind_exception,
									   _Unwind_Context* context)
{
	// If trace-unwind logging is not enabled, delegate to the real personality function.
	if(!Log::isCategoryEnabled(Log::traceUnwind))
	{
		return __gxx_personality_v0(version,
									static_cast<_Unwind_Action>(actions),
									exceptionClass,
									unwind_exception,
									context);
	}

	Log::printf(Log::traceUnwind, "=== DebugPersonalityFunction called ===\n");
	Log::printf(Log::traceUnwind, "  version: %d\n", version);
	Log::printf(Log::traceUnwind, "  actions: 0x%x", actions);
	if(actions & _UA_SEARCH_PHASE) Log::printf(Log::traceUnwind, " SEARCH_PHASE");
	if(actions & _UA_CLEANUP_PHASE) Log::printf(Log::traceUnwind, " CLEANUP_PHASE");
	if(actions & _UA_HANDLER_FRAME) Log::printf(Log::traceUnwind, " HANDLER_FRAME");
	if(actions & _UA_FORCE_UNWIND) Log::printf(Log::traceUnwind, " FORCE_UNWIND");
	Log::printf(Log::traceUnwind, "\n");
	Log::printf(Log::traceUnwind, "  exceptionClass: 0x%llx\n", (unsigned long long)exceptionClass);
	Log::printf(Log::traceUnwind, "  unwind_exception: %p\n", (void*)unwind_exception);
	Log::printf(Log::traceUnwind, "  context: %p\n", (void*)context);

	if(version != 1 || unwind_exception == 0 || context == 0) return _URC_FATAL_PHASE1_ERROR;

	Uptr ip = _Unwind_GetIP(context);
	Uptr regionStart = _Unwind_GetRegionStart(context);
	Log::printf(Log::traceUnwind,
				"  IP: 0x%" WAVM_PRIxPTR " RegionStart: 0x%" WAVM_PRIxPTR "\n",
				ip,
				regionStart);

	// Check if this is a native C++ exception (GNU or Clang)
	static const U64 kGnuExceptionClass = 0x474E5543432B2B00;   // "GNUCC++\0"
	static const U64 kClangExceptionClass = 0x434C4E47432B2B00; // "CLNGC++\0"
	bool native_exception
		= (exceptionClass == kGnuExceptionClass || exceptionClass == kClangExceptionClass);
	Log::printf(Log::traceUnwind, "  native_exception: %s\n", native_exception ? "true" : "false");

	scan_results results;

	if(actions == (_UA_CLEANUP_PHASE | _UA_HANDLER_FRAME) && native_exception)
	{
		// Phase 2 handler frame for native exception - load cached results from exception
		// header
		Log::printf(Log::traceUnwind, "  Phase 2 handler frame - loading cached results\n");
		loadResultsFromExceptionHeader(unwind_exception, results);
		set_registers(unwind_exception, context, results);
		return _URC_INSTALL_CONTEXT;
	}

	scan_eh_tab(
		results, static_cast<_Unwind_Action>(actions), native_exception, unwind_exception, context);
	Log::printf(Log::traceUnwind,
				"  scan_eh_tab result: reason=%d landingPad=0x%" WAVM_PRIxPTR "\n",
				(int)results.reason,
				results.landingPad);

	if(results.reason == _URC_CONTINUE_UNWIND || results.reason == _URC_FATAL_PHASE1_ERROR)
		return results.reason;

	if(actions & _UA_SEARCH_PHASE)
	{
		// Search phase found a handler - save results to exception header for phase 2
		Log::printf(Log::traceUnwind, "  Search phase found handler - saving results\n");
		if(native_exception) { saveResultsToExceptionHeader(unwind_exception, results); }
		return _URC_HANDLER_FOUND;
	}

	// Cleanup phase (not handler frame)
	Log::printf(Log::traceUnwind, "  Cleanup phase - installing context\n");
	set_registers(unwind_exception, context, results);
	return _URC_INSTALL_CONTEXT;
}

#endif // WAVM_PLATFORM_POSIX
