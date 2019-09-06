#pragma once

#include <atomic>
#include <vector>
#include "EmscriptenABI.h"
#include "WAVM/Emscripten/Emscripten.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/IndexMap.h"
#include "WAVM/Inline/IntrusiveSharedPtr.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Platform/Thread.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Linker.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace IR {
	struct Module;
}}

namespace WAVM { namespace VFS {
	struct VFD;
}}

namespace WAVM { namespace Emscripten {

	struct Instance;

	// Keeps track of the entry function used by a running WebAssembly-spawned thread.
	// Used to find garbage collection roots.
	struct Thread
	{
		Emscripten::Instance* instance;
		emabi::pthread_t id = 0;
		std::atomic<Uptr> numRefs{0};

		Platform::Thread* platformThread = nullptr;
		Runtime::GCPointer<Runtime::Context> context;
		Runtime::GCPointer<Runtime::Function> threadFunc;

		emabi::Address stackAddress;
		emabi::Address numStackBytes;

		std::atomic<U32> exitCode{0};

		I32 argument;

		HashMap<emabi::pthread_key_t, emabi::Address> pthreadSpecific;

		Thread(Emscripten::Instance* inInstance,
			   Runtime::Context* inContext,
			   Runtime::Function* inEntryFunction,
			   I32 inArgument)
		: instance(inInstance)
		, context(inContext)
		, threadFunc(inEntryFunction)
		, argument(inArgument)
		{
		}

		void addRef(Uptr delta = 1) { numRefs += delta; }
		void removeRef()
		{
			if(--numRefs == 0) { delete this; }
		}
	};

	struct Instance : Runtime::Resolver
	{
		Runtime::GCPointer<Runtime::Compartment> compartment;
		Runtime::GCPointer<Runtime::ModuleInstance> moduleInstance;

		Runtime::GCPointer<Runtime::ModuleInstance> env;
		Runtime::GCPointer<Runtime::ModuleInstance> asm2wasm;
		Runtime::GCPointer<Runtime::ModuleInstance> global;

		Runtime::GCPointer<Runtime::Memory> memory;
		Runtime::GCPointer<Runtime::Table> table;

		IntrusiveSharedPtr<Thread> mainThread;

		// A global list of running threads created by WebAssembly code.
		Platform::Mutex threadsMutex;
		IndexMap<emabi::pthread_t, IntrusiveSharedPtr<Thread>> threads{1, UINT32_MAX};

		std::atomic<emabi::pthread_key_t> pthreadSpecificNextKey{0};

		emabi::Address errnoAddress{0};

		std::atomic<emabi::Address> currentLocale;

		WAVM::VFS::VFD* stdIn{nullptr};
		WAVM::VFS::VFD* stdOut{nullptr};
		WAVM::VFS::VFD* stdErr{nullptr};

		~Instance();

		bool resolve(const std::string& moduleName,
					 const std::string& exportName,
					 IR::ExternType type,
					 Runtime::Object*& outObject) override;
	};

	struct ExitException
	{
		U32 exitCode;
	};

	struct ExitThreadException
	{
		U32 exitCode;
	};

	inline emabi::Address coerce32bitAddress(Runtime::Memory* memory, Uptr address)
	{
		if(address >= emabi::addressMax)
		{
			Runtime::throwException(Runtime::ExceptionTypes::outOfBoundsMemoryAccess,
									{Runtime::asObject(memory), U64(address)});
		}
		return (U32)address;
	}

	inline emabi::Result coerce32bitAddressResult(Runtime::Memory* memory, Iptr address)
	{
		if(address >= emabi::resultMax)
		{
			Runtime::throwException(Runtime::ExceptionTypes::outOfBoundsMemoryAccess,
									{Runtime::asObject(memory), U64(address)});
		}
		return (emabi::Result)address;
	}

	inline Emscripten::Instance* getEmscriptenInstance(
		Runtime::ContextRuntimeData* contextRuntimeData)
	{
		auto instance = (Emscripten::Instance*)getUserData(
			getCompartmentFromContextRuntimeData(contextRuntimeData));
		WAVM_ASSERT(instance);
		WAVM_ASSERT(instance->memory);
		return instance;
	}

	inline Emscripten::Thread* getEmscriptenThread(Runtime::ContextRuntimeData* contextRuntimeData)
	{
		auto thread
			= (Emscripten::Thread*)getUserData(getContextFromRuntimeData(contextRuntimeData));
		WAVM_ASSERT(thread);
		WAVM_ASSERT(thread->instance);
		return thread;
	}

	bool resizeHeap(Emscripten::Instance* instance, U32 desiredNumBytes);
	emabi::Address dynamicAlloc(Emscripten::Instance* instance, emabi::Size numBytes);
	void initThreadLocals(Thread* thread);

	WAVM_DECLARE_INTRINSIC_MODULE(envThreads)
}}
