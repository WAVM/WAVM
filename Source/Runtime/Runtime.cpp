#include "Core/Core.h"
#include "Core/Platform.h"
#include "AST/AST.h"
#include "Runtime.h"
#include "RuntimePrivate.h"

#include <iostream>

namespace AST { struct Module; }

namespace Runtime
{
	bool init()
	{
		RuntimePlatform::initGlobalExceptionHandlers();
		LLVMJIT::init();
		return initInstanceMemory();
	}
	
	void handleException(Exception::Cause cause,const ExecutionContext& context)
	{
		std::vector<std::string> frameDescriptions;
		for(auto stackFrame : context.stackFrames)
		{
			frameDescriptions.push_back(describeStackFrame(stackFrame));
		}
		throw new Exception {cause,frameDescriptions};
	}

	std::string describeStackFrame(const StackFrame& frame)
	{
		std::string frameDescription;
		const bool hasDescripton = 
			LLVMJIT::describeInstructionPointer(frame.ip,frameDescription)
		||	RuntimePlatform::describeInstructionPointer(frame.ip,frameDescription);
		if(hasDescripton) { return frameDescription; }
		else { return "<unknown function>"; }
	}

	bool loadModule(const AST::Module* module)
	{
		// Free any existing memory.
		vmSbrk(-(int32)vmSbrk(0));

		// Initialize the module's requested initial memory.
		if(vmSbrk((int32)module->initialNumBytesMemory) != 0)
		{
			std::cerr << "Failed to commit the requested initial memory for module instance (" << module->initialNumBytesMemory/1024 << "KB requested)" << std::endl;
			return false;
		}

		// Copy the module's data segments into VM memory.
		if(module->initialNumBytesMemory >= (1ull<<32)) { throw; }
		for(auto dataSegment : module->dataSegments)
		{
			if(dataSegment.baseAddress + dataSegment.numBytes > module->initialNumBytesMemory)
			{
				std::cerr << "Module data segment exceeds initial memory allocation" << std::endl;
				return false;
			}
			memcpy(instanceMemoryBase + dataSegment.baseAddress,dataSegment.data,dataSegment.numBytes);
		}
		
		// Initialize the intrinsics.
		initEmscriptenIntrinsics();
		initWebAssemblyIntrinsics();

		// Generate machine code for the module.
		return LLVMJIT::compileModule(module);
	}
	
	void* getFunctionPointer(const AST::Module* module,uintptr functionIndex)
	{
		return LLVMJIT::getFunctionPointer(module,functionIndex);
	}
}