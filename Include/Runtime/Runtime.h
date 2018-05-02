#pragma once

#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "IR/TaggedValue.h"
#include "IR/Types.h"
#include "Platform/Platform.h"

#ifndef RUNTIME_API
	#define RUNTIME_API DLL_IMPORT
#endif

// Declare IR::Module to avoid including the definition.
namespace IR { struct Module; }

namespace Runtime
{
	// Runtime object types. This must be a superset of IR::ObjectKind, with IR::ObjectKind
	// values having the same representation in Runtime::ObjectKind.
	enum class ObjectKind : U8
	{
		// Standard object kinds that may be imported/exported from WebAssembly modules.
		function = 0,
		table = 1,
		memory = 2,
		global = 3,
		exceptionType = 4,

		// Runtime-specific object kinds that are only used by transient runtime objects.
		module = 5,
		context = 6,
		compartment = 7,

		invalid = 0xff,
	};

	// A runtime object of any type.
	struct Object
	{
		const ObjectKind kind;

		Object(ObjectKind inKind): kind(inKind) {}
		virtual ~Object() {}
	};
	
	// Tests whether an object is of the given type.
	RUNTIME_API bool isA(Object* object,const IR::ObjectType& type);
	RUNTIME_API IR::ObjectType getObjectType(Object* object);

	inline Object* asObject(Object* object) { return object; }
	template<typename Type> Type* as(Object* object);
	template<> inline Object* as<Object>(Object* object) { return asObject(object); }

	// Declare the different kinds of objects.
	// They are only declared as incomplete struct types here, and Runtime clients
	// will only handle opaque pointers to them.
	#define DECLARE_OBJECT_TYPE(kindId,kindName,Type) \
		struct Type; \
		inline Type* as##kindName(Object* object) \
		{ \
			wavmAssert(!object || object->kind == kindId); \
			return (Type*)object; \
		} \
		inline Type* as##kindName##Nullable(Object* object) \
		{ \
			return object && object->kind == kindId ? (Type*)object : nullptr; \
		} \
		inline Object* asObject(Type* object) { return (Object*)object; } \
		template<> inline Type* as<Type>(Object* object) { return as##kindName(object); }

	DECLARE_OBJECT_TYPE(ObjectKind::function,Function,FunctionInstance);
	DECLARE_OBJECT_TYPE(ObjectKind::table,Table,TableInstance);
	DECLARE_OBJECT_TYPE(ObjectKind::memory,Memory,MemoryInstance);
	DECLARE_OBJECT_TYPE(ObjectKind::global,Global,GlobalInstance);
	DECLARE_OBJECT_TYPE(ObjectKind::module,Module,ModuleInstance);
	DECLARE_OBJECT_TYPE(ObjectKind::exceptionType,ExceptionType,ExceptionTypeInstance);
	DECLARE_OBJECT_TYPE(ObjectKind::context,Context,Context);
	DECLARE_OBJECT_TYPE(ObjectKind::compartment,Compartment,Compartment);

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
	RUNTIME_API void addGCRoot(Object* object);

	// Decrements the object's counter of root referencers.
	RUNTIME_API void removeGCRoot(Object* object);

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
		RUNTIME_API static const GCPointer<ExceptionTypeInstance> invalidArgumentType;

		GCPointer<ExceptionTypeInstance> type;
		std::vector<UntaggedValue> arguments;
		Platform::CallStack callStack;
	};

	// Creates an exception type instance.
	RUNTIME_API ExceptionTypeInstance* createExceptionTypeInstance(const IR::TupleType* parameters);

	// Returns a string that describes the given exception cause.
	RUNTIME_API std::string describeException(const Exception& exception);

	// Returns a string that describes the given exception type.
	RUNTIME_API std::string describeExceptionType(const ExceptionTypeInstance* type);

	// Returns the parameter types for an exception type instance.
	RUNTIME_API const IR::TupleType* getExceptionTypeParameters(const ExceptionTypeInstance* type);

	// Throws a runtime exception.
	[[noreturn]] RUNTIME_API void throwException(ExceptionTypeInstance* type,std::vector<UntaggedValue>&& arguments = {});

	// Calls a thunk and catches any runtime exceptions that occur within it.
	RUNTIME_API void catchRuntimeExceptions(
		const std::function<void()>& thunk,
		const std::function<void(Exception&&)>& catchThunk
		);

	typedef void (*UnhandledExceptionHandler)(Exception&&);
	RUNTIME_API void setUnhandledExceptionHandler(UnhandledExceptionHandler handler);

	//
	// Functions
	//

	// The calling convention for a FunctionInstance.
	enum class CallingConvention
	{
		wasm,
		intrinsic,
		intrinsicWithContextSwitch,
		intrinsicWithMemAndTable,
		c
	};

	// Invokes a FunctionInstance with the given arguments, and returns the result. The result is returned as a pointer
	// to an untagged value that is stored in the Context that will be overwritten by subsequent calls to
	// invokeFunctionUnchecked. This allows using this function in a call stack that will be forked, since returning
	// the result as a value will be lowered to passing in a pointer to stack memory for most calling conventions.
	RUNTIME_API UntaggedValue* invokeFunctionUnchecked(Context* context,FunctionInstance* function,const UntaggedValue* arguments);

	// Like invokeFunctionUnchecked, but returns a result tagged with its type, and takes arguments as tagged values.
	// If the wrong number or types or arguments are provided, a runtime exception is thrown.
	RUNTIME_API Result invokeFunctionChecked(
		Context* context,
		FunctionInstance* function,
		const std::vector<Value>& arguments);

	// Returns the type of a FunctionInstance.
	RUNTIME_API const IR::FunctionType* getFunctionType(FunctionInstance* function);

	//
	// Tables
	//

	// Creates a Table. May return null if the memory allocation fails.
	RUNTIME_API TableInstance* createTable(Compartment* compartment,IR::TableType type);

	RUNTIME_API TableInstance* cloneTable(TableInstance* table,Compartment* newCompartment);

	// Reads an element from the table. Assumes that index is in bounds.
	RUNTIME_API Object* getTableElement(TableInstance* table,Uptr index);

	// Writes an element to the table. Assumes that index is in bounds, and returns a pointer to the previous value of the element.
	RUNTIME_API Object* setTableElement(TableInstance* table,Uptr index,Object* newValue);

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
	RUNTIME_API MemoryInstance* createMemory(Compartment* compartment,IR::MemoryType type);

	RUNTIME_API MemoryInstance* cloneMemory(MemoryInstance* memory,Compartment* newCompartment);

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
	template<typename Value> Value& memoryRef(MemoryInstance* memory,Uptr offset)
	{ return *(Value*)getValidatedMemoryOffsetRange(memory,offset,sizeof(Value)); }

	// Validates an access to multiple elements of memory at the given offset, and returns a pointer to it.
	template<typename Value> Value* memoryArrayPtr(MemoryInstance* memory,Uptr offset,Uptr numElements)
	{ return (Value*)getValidatedMemoryOffsetRange(memory,offset,numElements * sizeof(Value)); }

	//
	// Globals
	//

	// Creates a GlobalInstance with the specified type and initial value.
	RUNTIME_API GlobalInstance* createGlobal(Compartment* compartment,IR::GlobalType type,Value initialValue);

	RUNTIME_API GlobalInstance* cloneGlobal(GlobalInstance* global,Compartment* newCompartment);

	// Reads the current value of a global.
	RUNTIME_API Value getGlobalValue(Context* context,GlobalInstance* global);

	// Writes a new value to a global, and returns the previous value.
	RUNTIME_API Value setGlobalValue(Context* context,GlobalInstance* global,Value newValue);

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
	RUNTIME_API ModuleInstance* instantiateModule(Compartment* compartment,const IR::Module& module,ImportBindings&& imports);

	// Gets the start function of a ModuleInstance.
	RUNTIME_API FunctionInstance* getStartFunction(ModuleInstance* moduleInstance);

	// Gets the default table/memory for a ModuleInstance.
	RUNTIME_API MemoryInstance* getDefaultMemory(ModuleInstance* moduleInstance);
	RUNTIME_API TableInstance* getDefaultTable(ModuleInstance* moduleInstance);

	// Gets an object exported by a ModuleInstance by name.
	RUNTIME_API Object* getInstanceExport(ModuleInstance* moduleInstance,const std::string& name);

	//
	// Compartments
	//

	RUNTIME_API Compartment* createCompartment();

	RUNTIME_API Compartment* cloneCompartment(Compartment* compartment);

	//
	// Contexts
	//

	RUNTIME_API Context* createContext(Compartment* compartment);
	RUNTIME_API Compartment* getCompartmentFromContext(Context* context);

	// Creates a new context, initializing its mutable global state from the given context.
	RUNTIME_API Context* cloneContext(Context* context,Compartment* newCompartment);

	RUNTIME_API Context* getContextFromRuntimeData(struct ContextRuntimeData* contextRuntimeData);
	RUNTIME_API struct ContextRuntimeData* getContextRuntimeData(Context* context);
	RUNTIME_API TableInstance* getTableFromRuntimeData(struct ContextRuntimeData* contextRuntimeData,Uptr tableId);
	RUNTIME_API MemoryInstance* getMemoryFromRuntimeData(struct ContextRuntimeData* contextRuntimeData,Uptr memoryId);
}
