#if WAVM_PLATFORM_POSIX

#include <sys/ucontext.h> // IWYU pragma: keep
#include <optional>
#include "POSIXPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Platform/CPU.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Unwind.h"

#if defined(__APPLE__)
#include <dlfcn.h>
#include <cstddef>
#include <map>
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Memory.h"
#endif

#define UNW_LOCAL_ONLY
#ifdef __APPLE__
#include <libunwind.h>
#else
#include "libunwind.h"
#endif

// libunwind eh_frame section registration/deregistration.
extern "C" void __unw_add_dynamic_eh_frame_section(const void* eh_frame_start);
extern "C" void __unw_remove_dynamic_eh_frame_section(const void* eh_frame_start);

using namespace WAVM;
using namespace WAVM::Platform;

// On macOS, we use the dynamic unwind sections API (__unw_add_find_dynamic_unwind_sections)
// instead of __register_frame. The __register_frame approach uses DwarfFDECache which is only
// checked as a fallback after static unwind sections. On arm64 with pointer authentication,
// setInfoBasedOnIPRegister fails to find dynamically registered FDEs because it checks static
// sections first. The dynamic unwind sections API integrates directly into libunwind's
// findUnwindSections path, ensuring JIT unwind info is found.
#if defined(__APPLE__)

// This struct's layout must match libunwind's unw_dynamic_unwind_sections struct.
struct UnwindSections
{
	Uptr dso_base;
	Uptr dwarf_section;
	Uptr dwarf_section_length;
	Uptr compact_unwind_section;
	Uptr compact_unwind_section_length;
};

// Manages dynamic unwind sections registration for JIT code on macOS.
//
// Both DWARF eh_frame and compact unwind data are provided via the
// __unw_add_find_dynamic_unwind_sections callback. The callback sets dso_base to the imageBase
// (codeStart), so libunwind caches DWARF FDEs with mh=imageBase as the cache key.
//
// On deregistration, we call __unw_remove_dynamic_eh_frame_section(imageBase) to flush any
// DwarfFDECache entries keyed by imageBase. This prevents stale FDE pointers from causing
// crashes when the virtual address range is reused by a new module. The function just calls
// DwarfFDECache::removeAllIn(), which doesn't validate its argument.
class DynamicUnwindRegistration
{
public:
	static DynamicUnwindRegistration& getInstance()
	{
		static DynamicUnwindRegistration instance;
		return instance;
	}

	void registerSections(Uptr codeStart,
						  Uptr codeEnd,
						  const U8* ehFrames,
						  Uptr ehFramesNumBytes,
						  const U8* compactUnwind,
						  Uptr compactUnwindNumBytes)
	{
		Platform::Mutex::Lock lock(mutex);

		RegisteredUnwindSections entry;
		entry.sections.dso_base = codeStart;
		entry.sections.dwarf_section = reinterpret_cast<Uptr>(ehFrames);
		entry.sections.dwarf_section_length = ehFramesNumBytes;
		entry.sections.compact_unwind_section = reinterpret_cast<Uptr>(compactUnwind);
		entry.sections.compact_unwind_section_length = compactUnwindNumBytes;
		entry.codeEnd = codeEnd;
		unwindSections[codeStart] = entry;

		Log::printf(Log::debug,
					"Registered dynamic unwind sections: codeStart=0x%" WAVM_PRIxPTR
					" codeEnd=0x%" WAVM_PRIxPTR " ehFrames=0x%" WAVM_PRIxPTR "/%" WAVM_PRIuPTR
					" compactUnwind=0x%" WAVM_PRIxPTR "/%" WAVM_PRIuPTR "\n",
					codeStart,
					codeEnd,
					entry.sections.dwarf_section,
					ehFramesNumBytes,
					entry.sections.compact_unwind_section,
					compactUnwindNumBytes);
	}

	void deregisterSections(Uptr codeStart)
	{
		Platform::Mutex::Lock lock(mutex);

		unwindSections.erase(codeStart);

		// Flush stale DwarfFDECache entries keyed by this imageBase. The callback path caches
		// DWARF FDEs with mh=sects.dso_base=codeStart. Without this flush, reusing the same
		// imageBase for a new module would find stale cache entries pointing to freed memory.
		__unw_remove_dynamic_eh_frame_section(reinterpret_cast<const void*>(codeStart));

		Log::printf(Log::debug,
					"Deregistered dynamic unwind sections: codeStart=0x%" WAVM_PRIxPTR "\n",
					codeStart);
	}

private:
	using AddFn = int (*)(int (*)(Uptr, UnwindSections*));
	using RemoveFn = int (*)(int (*)(Uptr, UnwindSections*));

	struct RegisteredUnwindSections
	{
		UnwindSections sections;
		Uptr codeEnd;
	};

	Platform::Mutex mutex;
	std::map<Uptr, RegisteredUnwindSections> unwindSections;
	RemoveFn removeFn = nullptr;

	DynamicUnwindRegistration()
	{
		auto addFn = reinterpret_cast<AddFn>(
			dlsym(RTLD_DEFAULT, "__unw_add_find_dynamic_unwind_sections"));
		removeFn = reinterpret_cast<RemoveFn>(
			dlsym(RTLD_DEFAULT, "__unw_remove_find_dynamic_unwind_sections"));

		WAVM_ERROR_UNLESS(addFn && removeFn);

		int result = addFn(findSectionsCallback);
		WAVM_ERROR_UNLESS(result == 0);

		Log::printf(Log::debug, "Dynamic unwind sections API enabled\n");
	}

	~DynamicUnwindRegistration()
	{
		if(removeFn)
		{
			int result = removeFn(findSectionsCallback);
			if(result != 0)
			{
				Log::printf(Log::debug,
							"Failed to unregister dynamic unwind sections callback: %d\n",
							result);
			}
		}
	}

	// Callback function that libunwind calls to find unwind sections for a given address
	static int findSectionsCallback(Uptr addr, UnwindSections* info)
	{
		return getInstance().findSections(addr, info);
	}

	int findSections(Uptr addr, UnwindSections* info)
	{
		Platform::Mutex::Lock lock(mutex);

		// Find the entry with the largest start address <= addr
		auto it = unwindSections.upper_bound(addr);
		if(it == unwindSections.begin()) { return 0; }
		--it;

		// Check that addr is within this module's address range.
		if(addr >= it->second.codeEnd) { return 0; }

		*info = it->second.sections;

		Log::printf(Log::debug,
					"DynamicUnwindRegistration::findSections: addr=0x%" WAVM_PRIxPTR
					" -> dso_base=0x%" WAVM_PRIxPTR " ehFrames=0x%" WAVM_PRIxPTR "/%" WAVM_PRIuPTR
					" compact=0x%" WAVM_PRIxPTR "/%" WAVM_PRIuPTR "\n",
					addr,
					info->dso_base,
					info->dwarf_section,
					info->dwarf_section_length,
					info->compact_unwind_section,
					info->compact_unwind_section_length);

		return 1;
	}
};

UnwindRegistration* Platform::registerUnwindData(const U8* imageBase,
												 Uptr imageNumPages,
												 const UnwindInfo& unwindInfo)
{
	if(!unwindInfo.data && !unwindInfo.compactUnwind) { return nullptr; }

	Uptr codeStart = reinterpret_cast<Uptr>(imageBase);
	Uptr codeEnd = codeStart + imageNumPages * Platform::getBytesPerPage();
	DynamicUnwindRegistration::getInstance().registerSections(codeStart,
															  codeEnd,
															  unwindInfo.data,
															  unwindInfo.dataNumBytes,
															  unwindInfo.compactUnwind,
															  unwindInfo.compactUnwindNumBytes);
	return reinterpret_cast<UnwindRegistration*>(codeStart);
}

void Platform::deregisterUnwindData(UnwindRegistration* registration)
{
	DynamicUnwindRegistration::getInstance().deregisterSections(
		reinterpret_cast<Uptr>(registration));
}

#else // !__APPLE__

UnwindRegistration* Platform::registerUnwindData(const U8* imageBase,
												 Uptr imageNumPages,
												 const UnwindInfo& unwindInfo)
{
	if(!unwindInfo.data) { return nullptr; }

	__unw_add_dynamic_eh_frame_section(unwindInfo.data);
	return reinterpret_cast<UnwindRegistration*>(const_cast<U8*>(unwindInfo.data));
}

void Platform::deregisterUnwindData(UnwindRegistration* registration)
{
	__unw_remove_dynamic_eh_frame_section(reinterpret_cast<const void*>(registration));
}

#endif

//
// Platform-independent unwind API implementation
//

struct UnwindStateImpl
{
	unw_context_t context;
	unw_cursor_t cursor;
	Iptr ipAdjustment;
	bool valid;
};

static_assert(sizeof(UnwindStateImpl) <= WAVM_PLATFORM_UNWIND_STATE_SIZE,
			  "WAVM_PLATFORM_UNWIND_STATE_SIZE is too small");
static_assert(alignof(UnwindStateImpl) <= WAVM_PLATFORM_UNWIND_STATE_ALIGN,
			  "WAVM_PLATFORM_UNWIND_STATE_ALIGN is too small");

static UnwindStateImpl& getImpl(U8* storage)
{
	return *reinterpret_cast<UnwindStateImpl*>(storage);
}

static const UnwindStateImpl& getImpl(const U8* storage)
{
	return *reinterpret_cast<const UnwindStateImpl*>(storage);
}

UnwindState::UnwindState(const UnwindState& other) { getImpl(storage) = getImpl(other.storage); }

UnwindState::UnwindState(UnwindState&& other) noexcept
{
	getImpl(storage) = getImpl(other.storage);
}

UnwindState& UnwindState::operator=(const UnwindState& other)
{
	getImpl(storage) = getImpl(other.storage);
	return *this;
}

UnwindState& UnwindState::operator=(UnwindState&& other) noexcept
{
	getImpl(storage) = getImpl(other.storage);
	return *this;
}

// Must not be inlined: the step() call below skips capture() itself, so if capture() were inlined
// into the caller, that step would skip the caller's frame instead.
WAVM_FORCENOINLINE UnwindState UnwindState::capture(Uptr numFramesToSkip)
{
	UnwindState state;
	auto& impl = getImpl(state.storage);
	if(unw_getcontext(&impl.context) || unw_init_local(&impl.cursor, &impl.context))
	{
		impl.valid = false;
		return state;
	}
	impl.ipAdjustment = 0;
	impl.valid = true;

	// Step past capture() itself so frame 0 is the caller.
	if(state.step() != UnwindStepResult::success)
	{
		impl.valid = false;
		return state;
	}

	// Skip additional frames as requested.
	for(Uptr i = 0; i < numFramesToSkip; ++i)
	{
		if(state.step() != UnwindStepResult::success) { break; }
	}

	return state;
}

UnwindStepResult UnwindState::step()
{
	auto& impl = getImpl(storage);
	WAVM_ASSERT(impl.valid);
	int result = unw_step(&impl.cursor);
	if(result > 0)
	{
		// After stepping, all subsequent frames are return addresses that need -1 to get the
		// call-site address.
		impl.ipAdjustment = 1;
		return UnwindStepResult::success;
	}
	if(result == 0) { return UnwindStepResult::end; }
	return UnwindStepResult::error;
}

// Helper to get a libunwind register value directly
static bool getLibunwindReg(const U8* storage, int libunwindReg, Uptr& outValue)
{
	const auto& impl = getImpl(storage);
	unw_word_t value;
	if(unw_get_reg(const_cast<unw_cursor_t*>(&impl.cursor), libunwindReg, &value) != 0)
	{
		return false;
	}
	outValue = static_cast<Uptr>(value);
	return true;
}

bool UnwindState::getProcInfo(UnwindProcInfo& outInfo) const
{
	const auto& impl = getImpl(storage);
	WAVM_ASSERT(impl.valid);

	unw_proc_info_t procInfo;
	if(unw_get_proc_info(const_cast<unw_cursor_t*>(&impl.cursor), &procInfo) != 0) { return false; }

	outInfo.startIP = static_cast<Uptr>(procInfo.start_ip);
	outInfo.endIP = static_cast<Uptr>(procInfo.end_ip);
	outInfo.lsda = static_cast<Uptr>(procInfo.lsda);
	outInfo.handler = static_cast<Uptr>(procInfo.handler);
	outInfo.unwindData = reinterpret_cast<const U8*>(procInfo.unwind_info);
	outInfo.unwindDataSize = static_cast<Uptr>(procInfo.unwind_info_size);
#ifdef __APPLE__
	outInfo.compactEncoding = static_cast<U32>(procInfo.format);
#endif

	return true;
}

std::optional<Uptr> UnwindState::getRegister(HostCPURegister reg) const
{
	WAVM_ASSERT(getImpl(storage).valid);
	int libunwindReg;
#if defined(__x86_64__)
	switch(reg)
	{
	case X86CPURegister::RAX: libunwindReg = UNW_X86_64_RAX; break;
	case X86CPURegister::RBX: libunwindReg = UNW_X86_64_RBX; break;
	case X86CPURegister::RCX: libunwindReg = UNW_X86_64_RCX; break;
	case X86CPURegister::RDX: libunwindReg = UNW_X86_64_RDX; break;
	case X86CPURegister::RSI: libunwindReg = UNW_X86_64_RSI; break;
	case X86CPURegister::RDI: libunwindReg = UNW_X86_64_RDI; break;
	case X86CPURegister::RBP: libunwindReg = UNW_X86_64_RBP; break;
	case X86CPURegister::RSP: libunwindReg = UNW_REG_SP; break;
	case X86CPURegister::R8: libunwindReg = UNW_X86_64_R8; break;
	case X86CPURegister::R9: libunwindReg = UNW_X86_64_R9; break;
	case X86CPURegister::R10: libunwindReg = UNW_X86_64_R10; break;
	case X86CPURegister::R11: libunwindReg = UNW_X86_64_R11; break;
	case X86CPURegister::R12: libunwindReg = UNW_X86_64_R12; break;
	case X86CPURegister::R13: libunwindReg = UNW_X86_64_R13; break;
	case X86CPURegister::R14: libunwindReg = UNW_X86_64_R14; break;
	case X86CPURegister::R15: libunwindReg = UNW_X86_64_R15; break;
	case X86CPURegister::RIP: libunwindReg = UNW_REG_IP; break;
	default: return std::nullopt;
	}
#elif defined(__aarch64__)
	switch(reg)
	{
	case AArch64CPURegister::X0: libunwindReg = UNW_AARCH64_X0; break;
	case AArch64CPURegister::X1: libunwindReg = UNW_AARCH64_X0 + 1; break;
	case AArch64CPURegister::X2: libunwindReg = UNW_AARCH64_X0 + 2; break;
	case AArch64CPURegister::X3: libunwindReg = UNW_AARCH64_X0 + 3; break;
	case AArch64CPURegister::X4: libunwindReg = UNW_AARCH64_X0 + 4; break;
	case AArch64CPURegister::X5: libunwindReg = UNW_AARCH64_X0 + 5; break;
	case AArch64CPURegister::X6: libunwindReg = UNW_AARCH64_X0 + 6; break;
	case AArch64CPURegister::X7: libunwindReg = UNW_AARCH64_X0 + 7; break;
	case AArch64CPURegister::X8: libunwindReg = UNW_AARCH64_X0 + 8; break;
	case AArch64CPURegister::X9: libunwindReg = UNW_AARCH64_X0 + 9; break;
	case AArch64CPURegister::X10: libunwindReg = UNW_AARCH64_X0 + 10; break;
	case AArch64CPURegister::X11: libunwindReg = UNW_AARCH64_X0 + 11; break;
	case AArch64CPURegister::X12: libunwindReg = UNW_AARCH64_X0 + 12; break;
	case AArch64CPURegister::X13: libunwindReg = UNW_AARCH64_X0 + 13; break;
	case AArch64CPURegister::X14: libunwindReg = UNW_AARCH64_X0 + 14; break;
	case AArch64CPURegister::X15: libunwindReg = UNW_AARCH64_X0 + 15; break;
	case AArch64CPURegister::X16: libunwindReg = UNW_AARCH64_X0 + 16; break;
	case AArch64CPURegister::X17: libunwindReg = UNW_AARCH64_X0 + 17; break;
	case AArch64CPURegister::X18: libunwindReg = UNW_AARCH64_X0 + 18; break;
	case AArch64CPURegister::X19: libunwindReg = UNW_AARCH64_X0 + 19; break;
	case AArch64CPURegister::X20: libunwindReg = UNW_AARCH64_X0 + 20; break;
	case AArch64CPURegister::X21: libunwindReg = UNW_AARCH64_X0 + 21; break;
	case AArch64CPURegister::X22: libunwindReg = UNW_AARCH64_X0 + 22; break;
	case AArch64CPURegister::X23: libunwindReg = UNW_AARCH64_X0 + 23; break;
	case AArch64CPURegister::X24: libunwindReg = UNW_AARCH64_X0 + 24; break;
	case AArch64CPURegister::X25: libunwindReg = UNW_AARCH64_X0 + 25; break;
	case AArch64CPURegister::X26: libunwindReg = UNW_AARCH64_X0 + 26; break;
	case AArch64CPURegister::X27: libunwindReg = UNW_AARCH64_X0 + 27; break;
	case AArch64CPURegister::X28: libunwindReg = UNW_AARCH64_X0 + 28; break;
	case AArch64CPURegister::FP: libunwindReg = UNW_AARCH64_FP; break;
	case AArch64CPURegister::LR: libunwindReg = UNW_AARCH64_LR; break;
	case AArch64CPURegister::SP: libunwindReg = UNW_REG_SP; break;
	case AArch64CPURegister::PC: libunwindReg = UNW_REG_IP; break;
	default: return std::nullopt;
	}
#endif
	Uptr value;
	if(!getLibunwindReg(storage, libunwindReg, value)) { return std::nullopt; }
	// If reading the IP register, subtract the ipAdjustment to convert from the +1 biased value
	// (needed for libunwind FDE matching) back to the actual instruction address.
	if(libunwindReg == UNW_REG_IP) { value -= getImpl(storage).ipAdjustment; }
	return value;
}

// Set all registers in an unw_cursor_t from a ucontext_t's saved register state.
// ipAdjustment is added to the IP when setting it in the cursor.
static void setUnwindCursorFromUContext(unw_cursor_t& cursor, ucontext_t* uc, Iptr ipAdjustment)
{
#if defined(__APPLE__) && defined(__aarch64__)
	const auto& ss = uc->uc_mcontext->__ss;
	for(int i = 0; i < 29; ++i) { unw_set_reg(&cursor, UNW_AARCH64_X0 + i, ss.__x[i]); }
	unw_set_reg(&cursor, UNW_AARCH64_FP, ss.__fp);
	unw_set_reg(&cursor, UNW_AARCH64_LR, ss.__lr);
	unw_set_reg(&cursor, UNW_REG_SP, ss.__sp);
	// Set IP last via UNW_REG_IP (not UNW_AARCH64_PC) so that libunwind re-looks up the
	// unwind info for the new IP. All other registers must be set first since the re-lookup
	// may depend on them.
	unw_set_reg(&cursor, UNW_REG_IP, ss.__pc + ipAdjustment);

#elif defined(__APPLE__) && defined(__x86_64__)
	const auto& ss = uc->uc_mcontext->__ss;
	unw_set_reg(&cursor, UNW_X86_64_RAX, ss.__rax);
	unw_set_reg(&cursor, UNW_X86_64_RDX, ss.__rdx);
	unw_set_reg(&cursor, UNW_X86_64_RCX, ss.__rcx);
	unw_set_reg(&cursor, UNW_X86_64_RBX, ss.__rbx);
	unw_set_reg(&cursor, UNW_X86_64_RSI, ss.__rsi);
	unw_set_reg(&cursor, UNW_X86_64_RDI, ss.__rdi);
	unw_set_reg(&cursor, UNW_X86_64_RBP, ss.__rbp);
	unw_set_reg(&cursor, UNW_REG_SP, ss.__rsp);
	unw_set_reg(&cursor, UNW_X86_64_R8, ss.__r8);
	unw_set_reg(&cursor, UNW_X86_64_R9, ss.__r9);
	unw_set_reg(&cursor, UNW_X86_64_R10, ss.__r10);
	unw_set_reg(&cursor, UNW_X86_64_R11, ss.__r11);
	unw_set_reg(&cursor, UNW_X86_64_R12, ss.__r12);
	unw_set_reg(&cursor, UNW_X86_64_R13, ss.__r13);
	unw_set_reg(&cursor, UNW_X86_64_R14, ss.__r14);
	unw_set_reg(&cursor, UNW_X86_64_R15, ss.__r15);
	// Must be last: triggers unwind info re-lookup (see UNW_REG_IP comment above).
	unw_set_reg(&cursor, UNW_REG_IP, ss.__rip + ipAdjustment);

#elif defined(__linux__) && defined(__aarch64__)
	const auto& mc = uc->uc_mcontext;
	for(int i = 0; i < 31; ++i) { unw_set_reg(&cursor, UNW_AARCH64_X0 + i, mc.regs[i]); }
	unw_set_reg(&cursor, UNW_REG_SP, mc.sp);
	// Must be last: triggers unwind info re-lookup (see UNW_REG_IP comment above).
	unw_set_reg(&cursor, UNW_REG_IP, mc.pc + ipAdjustment);

#elif defined(__linux__) && defined(__x86_64__)
	const auto& gregs = uc->uc_mcontext.gregs;
	unw_set_reg(&cursor, UNW_X86_64_RAX, gregs[REG_RAX]);
	unw_set_reg(&cursor, UNW_X86_64_RDX, gregs[REG_RDX]);
	unw_set_reg(&cursor, UNW_X86_64_RCX, gregs[REG_RCX]);
	unw_set_reg(&cursor, UNW_X86_64_RBX, gregs[REG_RBX]);
	unw_set_reg(&cursor, UNW_X86_64_RSI, gregs[REG_RSI]);
	unw_set_reg(&cursor, UNW_X86_64_RDI, gregs[REG_RDI]);
	unw_set_reg(&cursor, UNW_X86_64_RBP, gregs[REG_RBP]);
	unw_set_reg(&cursor, UNW_REG_SP, gregs[REG_RSP]);
	unw_set_reg(&cursor, UNW_X86_64_R8, gregs[REG_R8]);
	unw_set_reg(&cursor, UNW_X86_64_R9, gregs[REG_R9]);
	unw_set_reg(&cursor, UNW_X86_64_R10, gregs[REG_R10]);
	unw_set_reg(&cursor, UNW_X86_64_R11, gregs[REG_R11]);
	unw_set_reg(&cursor, UNW_X86_64_R12, gregs[REG_R12]);
	unw_set_reg(&cursor, UNW_X86_64_R13, gregs[REG_R13]);
	unw_set_reg(&cursor, UNW_X86_64_R14, gregs[REG_R14]);
	unw_set_reg(&cursor, UNW_X86_64_R15, gregs[REG_R15]);
	// Must be last: triggers unwind info re-lookup (see UNW_REG_IP comment above).
	unw_set_reg(&cursor, UNW_REG_IP, gregs[REG_RIP] + ipAdjustment);

#else
#error "Unsupported platform for setUnwindCursorFromUContext"
#endif
}

UnwindState Platform::makeUnwindStateFromSignalContext(void* contextPtr)
{
	UnwindState state;
	auto* signalContext = static_cast<ucontext_t*>(contextPtr);
	state = UnwindState::capture();
	auto& impl = getImpl(state.storage);
	if(!impl.valid) { return state; }

	// An IP adjustment of +1 is necessary because libunwind's DWARF FDE matching uses the
	// condition (pcStart < pc), i.e. the lower bound is exclusive. A signal can interrupt at the
	// very first instruction of a function where pc == pcStart, which would fail the match.
	// The ipAdjustment field ensures getRegister(IP) undoes this +1 for the caller.
	impl.ipAdjustment = 1;
	setUnwindCursorFromUContext(impl.cursor, signalContext, 1);
	return state;
}

#endif // WAVM_PLATFORM_POSIX
