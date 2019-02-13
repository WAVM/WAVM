#include <string>
#include <utility>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"

namespace WAVM { namespace Intrinsics {
	struct ModuleImpl
	{
		HashMap<std::string, Intrinsics::Function*> functionMap;
		HashMap<std::string, Intrinsics::Table*> tableMap;
		HashMap<std::string, Intrinsics::Memory*> memoryMap;
		HashMap<std::string, Intrinsics::Global*> globalMap;
	};
}}

using namespace WAVM;
using namespace WAVM::Runtime;

Intrinsics::Module::~Module()
{
	if(impl) { delete impl; }
}

static void initializeModule(Intrinsics::Module& moduleRef)
{
	if(!moduleRef.impl) { moduleRef.impl = new Intrinsics::ModuleImpl; }
}

Intrinsics::Function::Function(Intrinsics::Module& moduleRef,
							   const char* inName,
							   void* inNativeFunction,
							   IR::FunctionType inType,
							   IR::CallingConvention inCallingConvention)
: name(inName)
, type(inType)
, nativeFunction(inNativeFunction)
, callingConvention(inCallingConvention)
{
	initializeModule(moduleRef);

	if(moduleRef.impl->functionMap.contains(name))
	{ Errors::fatalf("Intrinsic function already registered: %s", name); }
	moduleRef.impl->functionMap.set(name, this);
}

Function* Intrinsics::Function::instantiate(Compartment* compartment)
{
	return LLVMJIT::getIntrinsicThunk(nativeFunction, type, callingConvention, name);
}

Intrinsics::Global::Global(Intrinsics::Module& moduleRef,
						   const char* inName,
						   IR::ValueType inType,
						   IR::Value inValue)
: name(inName), type(inType), value(inValue)
{
	initializeModule(moduleRef);

	if(moduleRef.impl->globalMap.contains(name))
	{ Errors::fatalf("Intrinsic global already registered: %s", name); }
	moduleRef.impl->globalMap.set(name, this);
}

Runtime::Global* Intrinsics::Global::instantiate(Compartment* compartment)
{
	Runtime::Global* global = createGlobal(compartment, IR::GlobalType(type, false));
	initializeGlobal(global, value);
	return global;
}

Intrinsics::Table::Table(Intrinsics::Module& moduleRef,
						 const char* inName,
						 const IR::TableType& inType)
: name(inName), type(inType)
{
	initializeModule(moduleRef);

	if(moduleRef.impl->tableMap.contains(name))
	{ Errors::fatalf("Intrinsic table already registered: %s", name); }
	moduleRef.impl->tableMap.set(name, this);
}

Table* Intrinsics::Table::instantiate(Compartment* compartment)
{
	return createTable(compartment, type, name);
}

Intrinsics::Memory::Memory(Intrinsics::Module& moduleRef,
						   const char* inName,
						   const IR::MemoryType& inType)
: name(inName), type(inType)
{
	initializeModule(moduleRef);

	if(moduleRef.impl->memoryMap.contains(name))
	{ Errors::fatalf("Intrinsic memory already registered: %s", name); }
	moduleRef.impl->memoryMap.set(name, this);
}

Memory* Intrinsics::Memory::instantiate(Compartment* compartment)
{
	return createMemory(compartment, type, name);
}

ModuleInstance* Intrinsics::instantiateModule(Compartment* compartment,
											  const Intrinsics::Module& moduleRef,
											  std::string&& debugName,
											  const HashMap<std::string, Object*>& extraExports)
{
	HashMap<std::string, Object*> exportMap = extraExports;
	std::vector<Runtime::Function*> functions;
	std::vector<Runtime::Table*> tables;
	std::vector<Runtime::Memory*> memories;
	std::vector<Runtime::Global*> globals;
	std::vector<Runtime::ExceptionType*> exceptionTypes;
	if(moduleRef.impl)
	{
		for(const auto& pair : moduleRef.impl->functionMap)
		{
			auto function = pair.value->instantiate(compartment);
			functions.push_back(function);
			exportMap.addOrFail(pair.key, asObject(function));
		}

		for(const auto& pair : moduleRef.impl->tableMap)
		{
			auto table = pair.value->instantiate(compartment);
			tables.push_back(table);
			exportMap.addOrFail(pair.key, asObject(table));
		}

		for(const auto& pair : moduleRef.impl->memoryMap)
		{
			auto memory = pair.value->instantiate(compartment);
			memories.push_back(memory);
			exportMap.addOrFail(pair.key, asObject(memory));
		}

		for(const auto& pair : moduleRef.impl->globalMap)
		{
			auto global = pair.value->instantiate(compartment);
			globals.push_back(global);
			exportMap.addOrFail(pair.key, asObject(global));
		}

		for(const auto& pair : extraExports)
		{
			Object* object = pair.value;
			switch(object->kind)
			{
			case ObjectKind::function: functions.push_back(asFunction(object)); break;
			case ObjectKind::table: tables.push_back(asTable(object)); break;
			case ObjectKind::memory: memories.push_back(asMemory(object)); break;
			case ObjectKind::global: globals.push_back(asGlobal(object)); break;
			case ObjectKind::exceptionType:
				exceptionTypes.push_back(asExceptionType(object));
				break;
			default: Errors::unreachable();
			};
		}
	}

	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	const Uptr id = compartment->moduleInstances.add(UINTPTR_MAX, nullptr);
	if(id == UINTPTR_MAX) { throwException(ExceptionTypes::outOfMemory, {}); }
	auto moduleInstance = new ModuleInstance(compartment,
											 id,
											 std::move(exportMap),
											 std::move(functions),
											 std::move(tables),
											 std::move(memories),
											 std::move(globals),
											 std::move(exceptionTypes),
											 nullptr,
											 {},
											 {},
											 nullptr,
											 std::move(debugName));
	compartment->moduleInstances[id] = moduleInstance;
	return moduleInstance;
}

HashMap<std::string, Intrinsics::Function*> Intrinsics::getUninstantiatedFunctions(
	const Intrinsics::Module& moduleRef)
{
	if(moduleRef.impl) { return moduleRef.impl->functionMap; }
	else
	{
		return {};
	}
}
