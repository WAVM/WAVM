#pragma once

#include "Inline/BasicTypes.h"
#include "TaggedValue.h"
#include "IR/Types.h"

#ifndef RUNTIME_API
	#define RUNTIME_API DLL_IMPORT
#endif

// Declare IR::Module to avoid including the definition.
namespace IR { struct Module; }

namespace Runtime
{
	// Initializes the runtime. Should only be called once per process.
	RUNTIME_API void init();

	// These are subclasses of Object, but are only defined within Runtime, so other modules must
	// use these forward declarations as opaque pointers.
	struct FunctionInstance;
	struct TableInstance;
	struct MemoryInstance;
	struct GlobalInstance;
	struct ModuleInstance;
	struct ExceptionTypeInstance;

	// A runtime object of any type.
	struct ObjectInstance
	{
		const IR::ObjectKind kind;

		ObjectInstance(IR::ObjectKind inKind): kind(inKind) {}
		virtual ~ObjectInstance() {}
	};
	
	// Tests whether an object is of the given type.
	RUNTIME_API bool isA(ObjectInstance* object,const IR::ObjectType& type);

	// Casts from object to subclasses, and vice versa.
	inline FunctionInstance* asFunction(ObjectInstance* object)				{ assert(object && object->kind == IR::ObjectKind::function); return (FunctionInstance*)object; }
	inline TableInstance* asTable(ObjectInstance* object)					{ assert(object && object->kind == IR::ObjectKind::table); return (TableInstance*)object; }
	inline MemoryInstance* asMemory(ObjectInstance* object)				{ assert(object && object->kind == IR::ObjectKind::memory); return (MemoryInstance*)object; }
	inline GlobalInstance* asGlobal(ObjectInstance* object)				{ assert(object && object->kind == IR::ObjectKind::global); return (GlobalInstance*)object; }
	inline ModuleInstance* asModule(ObjectInstance* object)				{ assert(object && object->kind == IR::ObjectKind::module); return (ModuleInstance*)object; }
	inline ExceptionTypeInstance* asExceptionType(ObjectInstance* object)	{ assert(object && object->kind == IR::ObjectKind::exceptionType); return (ExceptionTypeInstance*)object; }
	
	inline ObjectInstance* asObject(ObjectInstance* object) { return object; }
	inline ObjectInstance* asObject(FunctionInstance* function) { return (ObjectInstance*)function; }
	inline ObjectInstance* asObject(TableInstance* table) { return (ObjectInstance*)table; }
	inline ObjectInstance* asObject(MemoryInstance* memory) { return (ObjectInstance*)memory; }
	inline ObjectInstance* asObject(GlobalInstance* global) { return (ObjectInstance*)global; }
	inline ObjectInstance* asObject(ModuleInstance* module) { return (ObjectInstance*)module; }
	inline ObjectInstance* asObject(ExceptionTypeInstance* exceptionType) { return (ObjectInstance*)exceptionType; }

	template<typename Instance> Instance* as(ObjectInstance* object);
	template<> inline ObjectInstance* as<ObjectInstance>(ObjectInstance* object) { return asObject(object); }
	template<> inline FunctionInstance* as<FunctionInstance>(ObjectInstance* object) { return asFunction(object); }
	template<> inline TableInstance* as<TableInstance>(ObjectInstance* object) { return asTable(object); }
	template<> inline MemoryInstance* as<MemoryInstance>(ObjectInstance* object) { return asMemory(object); }
	template<> inline GlobalInstance* as<GlobalInstance>(ObjectInstance* object) { return asGlobal(object); }
	template<> inline ModuleInstance* as<ModuleInstance>(ObjectInstance* object) { return asModule(object); }
	template<> inline ExceptionTypeInstance* as<ExceptionTypeInstance>(ObjectInstance* object) { return asExceptionType(object); }

	// Casts from object to subclass that checks that the object is the right kind and returns null if not.
	inline FunctionInstance* asFunctionNullable(ObjectInstance* object)			{ return object && object->kind == IR::ObjectKind::function ? (FunctionInstance*)object : nullptr; }
	inline TableInstance* asTableNullable(ObjectInstance* object)				{ return object && object->kind == IR::ObjectKind::table ? (TableInstance*)object : nullptr; }
	inline MemoryInstance* asMemoryNullable(ObjectInstance* object)			{ return object && object->kind == IR::ObjectKind::memory ? (MemoryInstance*)object : nullptr; }
	inline GlobalInstance* asGlobalNullable(ObjectInstance* object)				{ return object && object->kind == IR::ObjectKind::global ? (GlobalInstance*)object : nullptr; }
	inline ModuleInstance* asModuleNullable(ObjectInstance* object)			{ return object && object->kind == IR::ObjectKind::module ? (ModuleInstance*)object : nullptr; }
	inline ExceptionTypeInstance* asExceptionTypeNullable(ObjectInstance* object)	{ return object && object->kind == IR::ObjectKind::exceptionType ? (ExceptionTypeInstance*)object : nullptr; }
	
	//
	// Garbage collection
	//

	// A GC root pointer.
	template<typename ObjectType>
	struct GCPointer
	{
		GCPointer(): value(nullptr) {}
		GCPointer(ObjectType* inValue)
		{
			value = inValue;
			if(value) { addGCRoot(asObject(value)); }
		}
		GCPointer(const GCPointer<ObjectType>& inCopy)
		{
			value = inCopy.value;
			if(value) { addGCRoot(asObject(value)); }
		}
		GCPointer(GCPointer<ObjectType>&& inMove)
		{
			value = inMove.value;
			inMove.value = nullptr;
		}
		~GCPointer()
		{
			if(value) { removeGCRoot(asObject(value)); }
		}

		void operator=(ObjectType* inValue)
		{
			if(value) { removeGCRoot(asObject(value)); }
			value = inValue;
			if(value) { addGCRoot(asObject(value)); }
		}
		void operator=(const GCPointer<ObjectType>& inCopy)
		{
			if(value) { removeGCRoot(asObject(value)); }
			value = inCopy.value;
			if(value) { addGCRoot(asObject(value)); }
		}
		void operator=(GCPointer<ObjectType>&& inMove)
		{
			if(value) { removeGCRoot(asObject(value)); }
			value = inMove.value;
			inMove.value = nullptr;
		}

		operator ObjectType*() const { return value; }
		ObjectType& operator*() const { return *value; }
		ObjectType* operator->() const { return value; }

	private:
		ObjectType* value;
	};

	// Increments the object's counter of root references.
	RUNTIME_API void addGCRoot(ObjectInstance* object);

	// Decrements the object's counter of root referencers.
	RUNTIME_API void removeGCRoot(ObjectInstance* object);

	// Frees objects that are unreachable from root object references.
	RUNTIME_API void collectGarbage();

	//
	// Exceptions
	//

	// Information about a runtime exception.
	struct Exception
	{
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> accessViolationType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> stackOverflowType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> integerDivideByZeroOrIntegerOverflowType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> invalidFloatOperationType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> invokeSignatureMismatchType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> reachedUnreachableType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> indirectCallSignatureMismatchType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> undefinedTableElementType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> calledAbortType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> calledUnimplementedIntrinsicType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> outOfMemoryType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> invalidSegmentOffsetType;
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> misalignedAtomicMemoryAccessType;

		GCPointer<ExceptionTypeInstance> type;
		std::vector<UntaggedValue> arguments;
		Platform::CallStack callStack;
	};

	// Creates an exception type instance.
	RUNTIME_API ExceptionTypeInstance* createExceptionTypeInstance(const IR::TupleType& parameters);

	// Returns a string that describes the given exception cause.
	RUNTIME_API std::string describeException(const Exception& exception);

	// Returns a string that describes the given exception type.
	RUNTIME_API std::string describeExceptionType(const ExceptionTypeInstance* type);

	// Returns the parameter types for an exception type instance.
	RUNTIME_API IR::TupleType getExceptionTypeParameters(const ExceptionTypeInstance* type);

	// Throws a runtime exception.
	[[noreturn]] RUNTIME_API void throwException(ExceptionTypeInstance* type,std::vector<UntaggedValue>&& arguments = {});

	// Calls a thunk and catches any runtime exceptions that occur within it.
	RUNTIME_API void catchRuntimeExceptions(
		const std::function<void()>& thunk,
		const std::function<void(Exception&&)>& catchThunk
		);

	//
	// Functions
	//

	// Invokes a FunctionInstance with the given parameters, and returns the result.
	RUNTIME_API Result invokeFunction(FunctionInstance* function,const std::vector<Value>& parameters);

	// Returns the type of a FunctionInstance.
	RUNTIME_API const IR::FunctionType* getFunctionType(FunctionInstance* function);

	//
	// Tables
	//

	// Creates a Table. May return null if the memory allocation fails.
	RUNTIME_API TableInstance* createTable(IR::TableType type);

	// Reads an element from the table. Assumes that index is in bounds.
	RUNTIME_API ObjectInstance* getTableElement(TableInstance* table,Uptr index);

	// Writes an element to the table. Assumes that index is in bounds, and returns a pointer to the previous value of the element.
	RUNTIME_API ObjectInstance* setTableElement(TableInstance* table,Uptr index,ObjectInstance* newValue);

	// Gets the current or maximum size of the table.
	RUNTIME_API Uptr getTableNumElements(TableInstance* table);
	RUNTIME_API Uptr getTableMaxElements(TableInstance* table);

	// Grows or shrinks the size of a table by numElements. Returns the previous size of the table.
	RUNTIME_API Iptr growTable(TableInstance* table,Uptr numElements);
	RUNTIME_API Iptr shrinkTable(TableInstance* table,Uptr numElements);

	//
	// Memories
	//

	// Creates a Memory. May return null if the memory allocation fails.
	RUNTIME_API MemoryInstance* createMemory(IR::MemoryType type);

	// Gets the base address of the memory's data.
	RUNTIME_API U8* getMemoryBaseAddress(MemoryInstance* memory);

	// Gets the current or maximum size of the memory in pages.
	RUNTIME_API Uptr getMemoryNumPages(MemoryInstance* memory);
	RUNTIME_API Uptr getMemoryMaxPages(MemoryInstance* memory);

	// Grows or shrinks the size of a memory by numPages. Returns the previous size of the memory.
	RUNTIME_API Iptr growMemory(MemoryInstance* memory,Uptr numPages);
	RUNTIME_API Iptr shrinkMemory(MemoryInstance* memory,Uptr numPages);

	// Validates that an offset range is wholly inside a Memory's virtual address range.
	RUNTIME_API U8* getValidatedMemoryOffsetRange(MemoryInstance* memory,Uptr offset,Uptr numBytes);
	
	// Validates an access to a single element of memory at the given offset, and returns a reference to it.
	template<typename Value> Value& memoryRef(MemoryInstance* memory,U32 offset)
	{ return *(Value*)getValidatedMemoryOffsetRange(memory,offset,sizeof(Value)); }

	// Validates an access to multiple elements of memory at the given offset, and returns a pointer to it.
	template<typename Value> Value* memoryArrayPtr(MemoryInstance* memory,U32 offset,U32 numElements)
	{ return (Value*)getValidatedMemoryOffsetRange(memory,offset,numElements * sizeof(Value)); }

	//
	// Globals
	//

	// Creates a GlobalInstance with the specified type and initial value.
	RUNTIME_API GlobalInstance* createGlobal(IR::GlobalType type,Value initialValue);

	// Reads the current value of a global.
	RUNTIME_API Value getGlobalValue(GlobalInstance* global);

	// Writes a new value to a global, and returns the previous value.
	RUNTIME_API Value setGlobalValue(GlobalInstance* global,Value newValue);

	//
	// Modules
	//

	struct ImportBindings
	{
		std::vector<FunctionInstance*> functions;
		std::vector<TableInstance*> tables;
		std::vector<MemoryInstance*> memories;
		std::vector<GlobalInstance*> globals;
		std::vector<ExceptionTypeInstance*> exceptionTypes;
	};

	// Instantiates a module, bindings its imports to the specified objects. May throw InstantiationException.
	RUNTIME_API ModuleInstance* instantiateModule(const IR::Module& module,ImportBindings&& imports);

	// Gets the default table/memory for a ModuleInstance.
	RUNTIME_API MemoryInstance* getDefaultMemory(ModuleInstance* moduleInstance);
	RUNTIME_API TableInstance* getDefaultTable(ModuleInstance* moduleInstance);

	// Gets an object exported by a ModuleInstance by name.
	RUNTIME_API ObjectInstance* getInstanceExport(ModuleInstance* moduleInstance,const std::string& name);
}
