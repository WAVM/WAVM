#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "TaggedValue.h"
#include "WebAssembly/Types.h"

#ifndef RUNTIME_API
	#define RUNTIME_API DLL_IMPORT
#endif

// Declare WebAssembly::Module to avoid including the definition.
namespace WebAssembly { struct Module; }

namespace Runtime
{
	// Initializes the runtime. Should only be called once per process.
	RUNTIME_API void init();

	// Information about a runtime exception.
	struct Exception
	{
		enum class Cause : uint8
		{
			unknown,
			accessViolation,
			stackOverflow,
			integerDivideByZeroOrIntegerOverflow,
			invalidFloatOperation,
			invokeSignatureMismatch,
			reachedUnreachable,
			indirectCallSignatureMismatch,
			undefinedTableElement,
			calledAbort,
			calledUnimplementedIntrinsic
		};

		Cause cause;
		std::vector<std::string> callStack;		
	};
	
	// Returns a string that describes the given exception cause.
	inline const char* describeExceptionCause(Exception::Cause cause)
	{
		switch(cause)
		{
		case Exception::Cause::accessViolation: return "access violation";
		case Exception::Cause::stackOverflow: return "stack overflow";
		case Exception::Cause::integerDivideByZeroOrIntegerOverflow: return "integer divide by zero or signed integer overflow";
		case Exception::Cause::invalidFloatOperation: return "invalid floating point operation";
		case Exception::Cause::invokeSignatureMismatch: return "invoke signature mismatch";
		case Exception::Cause::reachedUnreachable: return "reached unreachable code";
		case Exception::Cause::indirectCallSignatureMismatch: return "call_indirect to function with wrong signature";
		case Exception::Cause::undefinedTableElement: return "undefined function table element";
		case Exception::Cause::calledAbort: return "called abort";
		case Exception::Cause::calledUnimplementedIntrinsic: return "called unimplemented intrinsic";
		default: return "unknown";
		}
	}

	// Causes a runtime exception.
	[[noreturn]] RUNTIME_API void causeException(Exception::Cause cause);

	// An exception that may be thrown during module instantiation.
	struct InstantiationException
	{
		enum Cause
		{
			outOfMemory,
			invalidSegmentOffset
		};
		const Cause cause;
		InstantiationException(Cause inCause): cause(inCause) {}
	};

	// These are subclasses of Object, but are only defined within Runtime, so other modules must
	// use these forward declarations as opaque pointers.
	struct FunctionInstance;
	struct Table;
	struct Memory;
	struct GlobalInstance;
	struct ModuleInstance;

	// A runtime object of any type.
	struct Object
	{
		const WebAssembly::ObjectKind kind;

		Object(WebAssembly::ObjectKind inKind): kind(inKind) {}
		virtual ~Object() {}
	};
	
	// Tests whether an object is of the given type.
	RUNTIME_API bool isA(Object* object,const WebAssembly::ObjectType& type);

	// Casts from object to subclasses, and vice versa.
	inline FunctionInstance* asFunction(Object* object)	{ assert(object && object->kind == WebAssembly::ObjectKind::function); return (FunctionInstance*)object; }
	inline Table* asTable(Object* object)				{ assert(object && object->kind == WebAssembly::ObjectKind::table); return (Table*)object; }
	inline Memory* asMemory(Object* object)			{ assert(object && object->kind == WebAssembly::ObjectKind::memory); return (Memory*)object; }
	inline GlobalInstance* asGlobal(Object* object)		{ assert(object && object->kind == WebAssembly::ObjectKind::global); return (GlobalInstance*)object; }
	inline ModuleInstance* asModule(Object* object)		{ assert(object && object->kind == WebAssembly::ObjectKind::module); return (ModuleInstance*)object; }

	inline Object* asObject(FunctionInstance* function) { return (Object*)function; }
	inline Object* asObject(Table* table) { return (Object*)table; }
	inline Object* asObject(Memory* memory) { return (Object*)memory; }
	inline Object* asObject(GlobalInstance* global) { return (Object*)global; }
	inline Object* asObject(ModuleInstance* module) { return (Object*)module; }

	// Casts from object to subclass that checks that the object is the right kind and returns null if not.
	inline FunctionInstance* asFunctionNullable(Object* object)	{ return object && object->kind == WebAssembly::ObjectKind::function ? (FunctionInstance*)object : nullptr; }
	inline Table* asTableNullable(Object* object)			{ return object && object->kind == WebAssembly::ObjectKind::table ? (Table*)object : nullptr; }
	inline Memory* asMemoryNullable(Object* object)		{ return object && object->kind == WebAssembly::ObjectKind::memory ? (Memory*)object : nullptr; }
	inline GlobalInstance* asGlobalNullable(Object* object)		{ return object && object->kind == WebAssembly::ObjectKind::global ? (GlobalInstance*)object : nullptr; }
	inline ModuleInstance* asModuleNullable(Object* object)	{ return object && object->kind == WebAssembly::ObjectKind::module ? (ModuleInstance*)object : nullptr; }
	
	// Frees unreferenced Objects, using the provided array of Objects as the root set.
	RUNTIME_API void freeUnreferencedObjects(const std::vector<Object*>& rootObjectReferences);

	//
	// Functions
	//

	// Invokes a FunctionInstance with the given parameters, and returns the result.
	// Throws a Runtime::Exception if a trap occurs.
	RUNTIME_API Result invokeFunction(FunctionInstance* function,const std::vector<Value>& parameters);

	// Returns the type of a FunctionInstance.
	RUNTIME_API const WebAssembly::FunctionType* getFunctionType(FunctionInstance* function);

	//
	// Tables
	//

	// Creates a Table. May return null if the memory allocation fails.
	RUNTIME_API Table* createTable(WebAssembly::TableType type);

	// Reads an element from the table. Assumes that index is in bounds.
	RUNTIME_API Object* getTableElement(Table* table,uintp index);

	// Writes an element to the table. Assumes that index is in bounds, and returns a pointer to the previous value of the element.
	RUNTIME_API Object* setTableElement(Table* table,uintp index,Object* newValue);

	// Gets the current or maximum size of the table.
	RUNTIME_API size_t getTableNumElements(Table* table);
	RUNTIME_API size_t getTableMaxElements(Table* table);

	// Grows or shrinks the size of a table by numElements. Returns the previous size of the table.
	RUNTIME_API intp growTable(Table* table,size_t numElements);
	RUNTIME_API intp shrinkTable(Table* table,size_t numElements);

	//
	// Memories
	//

	// Creates a Memory. May return null if the memory allocation fails.
	RUNTIME_API Memory* createMemory(WebAssembly::MemoryType type);

	// Gets the base address of the memory's data.
	RUNTIME_API uint8* getMemoryBaseAddress(Memory* memory);

	// Gets the current or maximum size of the memory in pages.
	RUNTIME_API size_t getMemoryNumPages(Memory* memory);
	RUNTIME_API size_t getMemoryMaxPages(Memory* memory);

	// Grows or shrinks the size of a memory by numPages. Returns the previous size of the memory.
	RUNTIME_API intp growMemory(Memory* memory,size_t numPages);
	RUNTIME_API intp shrinkMemory(Memory* memory,size_t numPages);

	// Validates that an offset range is wholly inside a Memory's virtual address range.
	RUNTIME_API uint8* getValidatedMemoryOffsetRange(Memory* memory,uintp offset,size_t numBytes);
	
	// Validates an access to a single element of memory at the given offset, and returns a reference to it.
	template<typename Value> Value& memoryRef(Memory* memory,uint32 offset)
	{ return *(Value*)getValidatedMemoryOffsetRange(memory,offset,sizeof(Value)); }

	// Validates an access to multiple elements of memory at the given offset, and returns a pointer to it.
	template<typename Value> Value* memoryArrayPtr(Memory* memory,uint32 offset,uint32 numElements)
	{ return (Value*)getValidatedMemoryOffsetRange(memory,offset,numElements * sizeof(Value)); }

	//
	// Globals
	//

	// Creates a GlobalInstance with the specified type and initial value.
	RUNTIME_API GlobalInstance* createGlobal(WebAssembly::GlobalType type,Value initialValue);

	// Reads the current value of a global.
	RUNTIME_API Value getGlobalValue(GlobalInstance* global);

	// Writes a new value to a global, and returns the previous value.
	RUNTIME_API Value setGlobalValue(GlobalInstance* global,Value newValue);

	//
	// Modules
	//

	// Instantiates a module, bindings its imports to the specified objects. May throw InstantiationException.
	RUNTIME_API ModuleInstance* instantiateModule(const WebAssembly::Module& module,std::vector<Object*>&& imports);

	// Gets the default table/memory for a ModuleInstance.
	RUNTIME_API Memory* getDefaultMemory(ModuleInstance* moduleInstance);
	RUNTIME_API Table* getDefaultTable(ModuleInstance* moduleInstance);

	// Gets an object exported by a ModuleInstance by name.
	RUNTIME_API Object* getInstanceExport(ModuleInstance* moduleInstance,const char* exportName);
}
