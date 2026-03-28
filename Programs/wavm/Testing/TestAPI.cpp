#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "TestUtils.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Value.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/LLVMJIT/LLVMJIT.h"
#include "WAVM/Runtime/Runtime.h"
#include "WAVM/WASTParse/WASTParse.h"
#include "wavm-test.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;
using namespace WAVM::Testing;

static bool loadTextModule(const char* wat, Uptr watLength, ModuleRef& outModule)
{
	IR::Module irModule;
	std::vector<WAST::Error> parseErrors;
	if(!WAST::parseModule(wat, watLength, irModule, parseErrors))
	{
		WAST::reportParseErrors("loadTextModule", wat, parseErrors);
		return false;
	}
	outModule = compileModule(irModule);
	return true;
}

namespace WAVM { namespace IR {
	inline std::string testToString(ValueType type)
	{
		switch(type)
		{
		case ValueType::none: return "none";
		case ValueType::any: return "any";
		case ValueType::i32: return "i32";
		case ValueType::i64: return "i64";
		case ValueType::f32: return "f32";
		case ValueType::f64: return "f64";
		case ValueType::v128: return "v128";
		case ValueType::externref: return "externref";
		case ValueType::funcref: return "funcref";
		default: return "unknown(" + std::to_string(U8(type)) + ")";
		}
	}
}}

namespace WAVM { namespace Runtime {
	inline std::string testToString(GrowResult result)
	{
		switch(result)
		{
		case GrowResult::success: return "success";
		case GrowResult::outOfMemory: return "outOfMemory";
		case GrowResult::outOfQuota: return "outOfQuota";
		case GrowResult::outOfMaxSize: return "outOfMaxSize";
		default: return "unknown";
		}
	}

	inline std::string testToString(InstructionSource::Type type)
	{
		switch(type)
		{
		case InstructionSource::Type::unknown: return "unknown";
		case InstructionSource::Type::native: return "native";
		case InstructionSource::Type::wasm: return "wasm";
		default: return "InstructionSource::Type(" + std::to_string(int(type)) + ")";
		}
	}

	inline std::string testToString(Function* function)
	{
		return function ? getDebugName(function) : "<null>";
	}
}}

static void testCompartmentCloning(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("testCloning");
	WAVM_ERROR_UNLESS(compartment);

	// Create objects in the compartment.
	Memory* memory = createMemory(
		compartment, MemoryType(false, IndexType::i32, SizeConstraints{1, 10}), "memory");
	WAVM_ERROR_UNLESS(memory);

	Table* table = createTable(compartment,
							   TableType(ReferenceType::funcref, false, IndexType::i32, {1, 10}),
							   nullptr,
							   "table");
	WAVM_ERROR_UNLESS(table);

	Global* global = createGlobal(compartment, GlobalType(ValueType::i32, true), "global");
	WAVM_ERROR_UNLESS(global);
	initializeGlobal(global, Value(I32(42)));

	// Verify the objects are in this compartment.
	CHECK_TRUE(isInCompartment(asObject(memory), compartment));
	CHECK_TRUE(isInCompartment(asObject(table), compartment));
	CHECK_TRUE(isInCompartment(asObject(global), compartment));

	// Clone the compartment.
	GCPointer<Compartment> clonedCompartment = cloneCompartment(compartment, "clonedCompartment");
	WAVM_ERROR_UNLESS(clonedCompartment);

	// Remap objects to the cloned compartment.
	Memory* clonedMemory = remapToClonedCompartment(memory, clonedCompartment);
	Table* clonedTable = remapToClonedCompartment(table, clonedCompartment);
	Global* clonedGlobal = remapToClonedCompartment(global, clonedCompartment);

	CHECK_NOT_NULL(clonedMemory);
	CHECK_NOT_NULL(clonedTable);
	CHECK_NOT_NULL(clonedGlobal);

	// Verify the cloned objects are in the cloned compartment.
	CHECK_TRUE(isInCompartment(asObject(clonedMemory), clonedCompartment));
	CHECK_TRUE(isInCompartment(asObject(clonedTable), clonedCompartment));
	CHECK_TRUE(isInCompartment(asObject(clonedGlobal), clonedCompartment));

	// Verify the cloned objects are NOT in the original compartment.
	CHECK_FALSE(isInCompartment(asObject(clonedMemory), compartment));
	CHECK_FALSE(isInCompartment(asObject(clonedTable), compartment));
	CHECK_FALSE(isInCompartment(asObject(clonedGlobal), compartment));

	// Verify the cloned global has the same value.
	Context* clonedContext = createContext(clonedCompartment, "clonedContext");
	WAVM_ERROR_UNLESS(clonedContext);
	Value clonedGlobalValue = getGlobalValue(clonedContext, clonedGlobal);
	CHECK_EQ(clonedGlobalValue.type, ValueType::i32);
	CHECK_EQ(clonedGlobalValue.i32, I32(42));

	// Clean up.
	CHECK_TRUE(tryCollectCompartment(std::move(clonedCompartment)));
	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testResourceQuotas(TEST_STATE_PARAM)
{
	ResourceQuotaRef quota = createResourceQuota();

	// Verify default quotas (UINT64_MAX represents unlimited).
	CHECK_EQ(getResourceQuotaMaxMemoryPages(quota), Uptr(UINT64_MAX));
	CHECK_EQ(getResourceQuotaMaxTableElems(quota), Uptr(UINT64_MAX));

	// Set and verify memory page quota.
	setResourceQuotaMaxMemoryPages(quota, 100);
	CHECK_EQ(getResourceQuotaMaxMemoryPages(quota), Uptr(100));

	// Set and verify table element quota.
	setResourceQuotaMaxTableElems(quota, 200);
	CHECK_EQ(getResourceQuotaMaxTableElems(quota), Uptr(200));

	// Create a compartment and memory within the quota.
	GCPointer<Compartment> compartment = createCompartment("quotaTest");
	WAVM_ERROR_UNLESS(compartment);

	Memory* memory = createMemory(compartment,
								  MemoryType(false, IndexType::i32, SizeConstraints{1, 1000}),
								  "quotaMemory",
								  quota);
	WAVM_ERROR_UNLESS(memory);

	// Growing within quota should succeed.
	Uptr oldNumPages = 0;
	GrowResult growResult = growMemory(memory, 1, &oldNumPages);
	CHECK_EQ(growResult, GrowResult::success);
	CHECK_EQ(oldNumPages, Uptr(1));

	// Set a very restrictive quota and try to exceed it.
	setResourceQuotaMaxMemoryPages(quota, 3);
	growResult = growMemory(memory, 10, nullptr);
	CHECK_EQ(growResult, GrowResult::outOfQuota);

	// Verify table element quota enforcement.
	Table* table = createTable(compartment,
							   TableType(ReferenceType::funcref, false, IndexType::i32, {1, 1000}),
							   nullptr,
							   "quotaTable",
							   quota);
	WAVM_ERROR_UNLESS(table);

	setResourceQuotaMaxTableElems(quota, 3);
	GrowResult tableGrowResult = growTable(table, 10, nullptr);
	CHECK_EQ(tableGrowResult, GrowResult::outOfQuota);

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testMemoryOperations(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("memoryTest");
	WAVM_ERROR_UNLESS(compartment);

	// Create a memory with initial size 1 page and max 10 pages.
	MemoryType memType(false, IndexType::i32, SizeConstraints{1, 10});
	Memory* memory = createMemory(compartment, memType, "testMemory");
	WAVM_ERROR_UNLESS(memory);

	// Verify initial state.
	CHECK_EQ(getMemoryNumPages(memory), Uptr(1));

	MemoryType retrievedType = getMemoryType(memory);
	CHECK_EQ(retrievedType.size.min, U64(1));
	CHECK_EQ(retrievedType.size.max, U64(10));
	CHECK_FALSE(retrievedType.isShared);

	// Verify base address is non-null.
	U8* baseAddress = getMemoryBaseAddress(memory);
	CHECK_NOT_NULL(baseAddress);

	// Write to and read from memory.
	WAVM_ERROR_UNLESS(baseAddress);
	baseAddress[0] = 0xAB;
	baseAddress[1] = 0xCD;
	CHECK_EQ(baseAddress[0], U8(0xAB));
	CHECK_EQ(baseAddress[1], U8(0xCD));

	// Grow the memory.
	Uptr oldNumPages = 0;
	GrowResult result = growMemory(memory, 2, &oldNumPages);
	CHECK_EQ(result, GrowResult::success);
	CHECK_EQ(oldNumPages, Uptr(1));
	CHECK_EQ(getMemoryNumPages(memory), Uptr(3));

	// Verify that the original data is still there after growing.
	baseAddress = getMemoryBaseAddress(memory);
	WAVM_ERROR_UNLESS(baseAddress);
	CHECK_EQ(baseAddress[0], U8(0xAB));
	CHECK_EQ(baseAddress[1], U8(0xCD));

	// Try to grow beyond the maximum.
	result = growMemory(memory, 100, nullptr);
	CHECK_EQ(result, GrowResult::outOfMaxSize);
	CHECK_EQ(getMemoryNumPages(memory), Uptr(3));

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testTableOperations(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("tableTest");
	WAVM_ERROR_UNLESS(compartment);

	// Create a table with initial size 5 and max 20.
	TableType tableType(ReferenceType::funcref, false, IndexType::i32, {5, 20});
	Table* table = createTable(compartment, tableType, nullptr, "testTable");
	WAVM_ERROR_UNLESS(table);

	// Verify initial state.
	CHECK_EQ(getTableNumElements(table), Uptr(5));

	TableType retrievedType = getTableType(table);
	CHECK_EQ(retrievedType.size.min, U64(5));
	CHECK_EQ(retrievedType.size.max, U64(20));
	CHECK_FALSE(retrievedType.isShared);

	// All elements should initially be null.
	for(Uptr i = 0; i < 5; ++i) { CHECK_NULL(getTableElement(table, i)); }

	// Grow the table.
	Uptr oldNumElems = 0;
	GrowResult result = growTable(table, 3, &oldNumElems);
	CHECK_EQ(result, GrowResult::success);
	CHECK_EQ(oldNumElems, Uptr(5));
	CHECK_EQ(getTableNumElements(table), Uptr(8));

	// Try to grow beyond the maximum.
	result = growTable(table, 100, nullptr);
	CHECK_EQ(result, GrowResult::outOfMaxSize);
	CHECK_EQ(getTableNumElements(table), Uptr(8));

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testGlobalOperations(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("globalTest");
	WAVM_ERROR_UNLESS(compartment);

	Context* context = createContext(compartment, "globalContext");
	WAVM_ERROR_UNLESS(context);

	// Test a mutable i32 global.
	Global* mutableI32 = createGlobal(compartment, GlobalType(ValueType::i32, true), "mutableI32");
	WAVM_ERROR_UNLESS(mutableI32);
	initializeGlobal(mutableI32, Value(I32(100)));

	GlobalType mutableI32Type = getGlobalType(mutableI32);
	CHECK_EQ(mutableI32Type.valueType, ValueType::i32);
	CHECK_TRUE(mutableI32Type.isMutable);

	Value val = getGlobalValue(context, mutableI32);
	CHECK_EQ(val.type, ValueType::i32);
	CHECK_EQ(val.i32, I32(100));

	// Set a new value and read it back.
	setGlobalValue(context, mutableI32, Value(I32(200)));
	val = getGlobalValue(context, mutableI32);
	CHECK_EQ(val.i32, I32(200));

	// Test a mutable i64 global.
	Global* mutableI64 = createGlobal(compartment, GlobalType(ValueType::i64, true), "mutableI64");
	WAVM_ERROR_UNLESS(mutableI64);
	initializeGlobal(mutableI64, Value(I64(9999999999LL)));

	val = getGlobalValue(context, mutableI64);
	CHECK_EQ(val.type, ValueType::i64);
	CHECK_EQ(val.i64, I64(9999999999LL));

	// Test a mutable f32 global.
	Global* mutableF32 = createGlobal(compartment, GlobalType(ValueType::f32, true), "mutableF32");
	WAVM_ERROR_UNLESS(mutableF32);
	initializeGlobal(mutableF32, Value(F32(3.14f)));

	val = getGlobalValue(context, mutableF32);
	CHECK_EQ(val.type, ValueType::f32);

	// Test a mutable f64 global.
	Global* mutableF64 = createGlobal(compartment, GlobalType(ValueType::f64, true), "mutableF64");
	WAVM_ERROR_UNLESS(mutableF64);
	initializeGlobal(mutableF64, Value(F64(2.718281828)));

	val = getGlobalValue(context, mutableF64);
	CHECK_EQ(val.type, ValueType::f64);

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testExceptionTypes(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("exceptionTest");
	WAVM_ERROR_UNLESS(compartment);

	// Create an exception type with no parameters.
	Runtime::ExceptionType* emptyExType
		= createExceptionType(compartment, IR::ExceptionType{{}}, "emptyException");
	WAVM_ERROR_UNLESS(emptyExType);

	TypeTuple emptyParams = getExceptionTypeParameters(emptyExType);
	CHECK_EQ(emptyParams.size(), Uptr(0));

	// Create an exception type with i32 and i64 parameters.
	Runtime::ExceptionType* paramExType
		= createExceptionType(compartment,
							  IR::ExceptionType{TypeTuple{ValueType::i32, ValueType::i64}},
							  "paramException");
	WAVM_ERROR_UNLESS(paramExType);

	TypeTuple params = getExceptionTypeParameters(paramExType);
	CHECK_EQ(params.size(), Uptr(2));
	CHECK_EQ(params[0], ValueType::i32);
	CHECK_EQ(params[1], ValueType::i64);

	// Verify describeExceptionType returns a non-empty string.
	std::string description = describeExceptionType(paramExType);
	CHECK_TRUE(description.size() > 0);

	// Create and destroy an exception instance.
	UntaggedValue args[2];
	args[0].i32 = 42;
	args[1].i64 = 99;
	CallStack callStack;
	Exception* exception = createException(paramExType, args, 2, std::move(callStack));
	WAVM_ERROR_UNLESS(exception);

	CHECK_EQ(getExceptionType(exception), paramExType);

	UntaggedValue arg0 = getExceptionArgument(exception, 0);
	UntaggedValue arg1 = getExceptionArgument(exception, 1);
	CHECK_EQ(arg0.i32, I32(42));
	CHECK_EQ(arg1.i64, I64(99));

	std::string exDescription = describeException(exception);
	CHECK_TRUE(exDescription.size() > 0);

	destroyException(exception);

	// Test throwing and catching an exception.
	bool caughtException = false;
	Runtime::ExceptionType* caughtType = nullptr;
	catchRuntimeExceptions(
		[&]() { throwException(createException(emptyExType, nullptr, 0, CallStack())); },
		[&](Exception* caught) {
			caughtException = true;
			caughtType = getExceptionType(caught);
			destroyException(caught);
		});
	CHECK_TRUE(caughtException);
	CHECK_EQ(caughtType, emptyExType);

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testModuleCompileAndIntrospect(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("moduleTest");
	WAVM_ERROR_UNLESS(compartment);

	// A module with a single identity function: (i32) -> (i32).
	static const char wat[]
		= "(module"
		  "  (func (export \"identity\") (param i32) (result i32) (local.get 0))"
		  ")";

	// Parse and compile the module from WAT text.
	ModuleRef compiledModule;
	bool loaded = loadTextModule(wat, sizeof(wat), compiledModule);
	CHECK_TRUE(loaded);
	WAVM_ERROR_UNLESS(loaded && compiledModule);

	// Verify the IR is accessible.
	const IR::Module& retrievedIR = getModuleIR(compiledModule);
	CHECK_EQ(retrievedIR.types.size(), Uptr(1));
	CHECK_EQ(retrievedIR.functions.defs.size(), Uptr(1));
	CHECK_EQ(retrievedIR.exports.size(), Uptr(1));

	// Verify object code can be extracted.
	std::vector<U8> objectCode = getObjectCode(compiledModule);
	CHECK_TRUE(objectCode.size() > 0);

	// Instantiate the module.
	ImportBindings imports;
	Instance* instance
		= instantiateModule(compartment, compiledModule, std::move(imports), "testInstance");
	WAVM_ERROR_UNLESS(instance);

	// Look up the exported function.
	Object* exportedObj = getInstanceExport(instance, "identity");
	CHECK_NOT_NULL(exportedObj);
	WAVM_ERROR_UNLESS(exportedObj);

	Function* identityFunc = asFunctionNullable(exportedObj);
	CHECK_NOT_NULL(identityFunc);
	WAVM_ERROR_UNLESS(identityFunc);

	FunctionType funcType = getFunctionType(identityFunc);
	CHECK_EQ(funcType.params().size(), Uptr(1));
	CHECK_EQ(funcType.results().size(), Uptr(1));
	CHECK_EQ(funcType.params()[0], ValueType::i32);
	CHECK_EQ(funcType.results()[0], ValueType::i32);

	// Invoke the identity function.
	Context* context = createContext(compartment, "moduleContext");
	WAVM_ERROR_UNLESS(context);

	UntaggedValue invokeArgs[1];
	invokeArgs[0].i32 = 123;
	UntaggedValue invokeResults[1];
	invokeFunction(context, identityFunc, funcType, invokeArgs, invokeResults);
	CHECK_EQ(invokeResults[0].i32, I32(123));

	// Test that a non-existent export returns null.
	CHECK_NULL(getInstanceExport(instance, "nonExistent"));

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testForeignObjects(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("foreignTest");
	WAVM_ERROR_UNLESS(compartment);

	// Create a foreign object with user data.
	int userData = 42;
	Foreign* foreign = createForeign(compartment, &userData, nullptr, "testForeign");
	WAVM_ERROR_UNLESS(foreign);

	// Verify the user data is retrievable.
	void* retrieved = getUserData(foreign);
	CHECK_EQ(retrieved, static_cast<void*>(&userData));

	// Create a foreign object with a finalizer that sets a flag in the heap object.
	struct FinalizerData
	{
		int value;
		bool finalized;
	};
	FinalizerData* heapData = new FinalizerData{99, false};
	Foreign* foreignWithFinalizer = createForeign(
		compartment,
		heapData,
		[](void* ptr) { static_cast<FinalizerData*>(ptr)->finalized = true; },
		"foreignWithFinalizer");
	WAVM_ERROR_UNLESS(foreignWithFinalizer);

	CHECK_EQ(getUserData(foreignWithFinalizer), static_cast<void*>(heapData));

	// Update user data on an object.
	int newUserData = 77;
	setUserData(foreign, &newUserData);
	CHECK_EQ(getUserData(foreign), static_cast<void*>(&newUserData));

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));

	// Verify the finalizer was called during compartment collection.
	CHECK_TRUE(heapData->finalized);
	delete heapData;
}

static void testTrapInstructionIndex(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("trapIndexTest");
	WAVM_ERROR_UNLESS(compartment);

	// A module with a single exported function "trap" containing:
	// nop, nop, unreachable, end
	// The unreachable instruction is at op index 2.
	static const char wat[]
		= "(module"
		  "  (func (export \"trap\") nop nop unreachable)"
		  ")";

	ModuleRef compiledModule;
	bool loaded = loadTextModule(wat, sizeof(wat), compiledModule);
	CHECK_TRUE(loaded);
	WAVM_ERROR_UNLESS(loaded && compiledModule);

	ImportBindings imports;
	Instance* instance
		= instantiateModule(compartment, compiledModule, std::move(imports), "trapInstance");
	WAVM_ERROR_UNLESS(instance);

	Object* exportedObj = getInstanceExport(instance, "trap");
	CHECK_NOT_NULL(exportedObj);
	WAVM_ERROR_UNLESS(exportedObj);

	Function* trapFunc = asFunctionNullable(exportedObj);
	CHECK_NOT_NULL(trapFunc);
	WAVM_ERROR_UNLESS(trapFunc);

	Context* context = createContext(compartment, "trapContext");
	WAVM_ERROR_UNLESS(context);

	// Invoke the trap function and catch the runtime exception.
	bool caughtException = false;
	catchRuntimeExceptions(
		[&]() { invokeFunction(context, trapFunc, getFunctionType(trapFunc)); },
		[&](Exception* caught) {
			caughtException = true;

			const CallStack& callStack = getExceptionCallStack(caught);
			bool foundTrapFrame = false;
			for(const auto& frame : callStack.frames)
			{
				InstructionSource sources[Runtime::maxInlineSourceFrames];
				Uptr numSources = getInstructionSourceByAddress(
					frame.ip, sources, Runtime::maxInlineSourceFrames);
				if(numSources != 1 || sources[0].type != InstructionSource::Type::wasm)
				{
					continue;
				}
				foundTrapFrame = true;
				CHECK_EQ(sources[0].wasm.function, trapFunc);
				CHECK_EQ(sources[0].wasm.instructionIndex, Uptr(2));
				break;
			}
			CHECK_TRUE(foundTrapFrame);

			destroyException(caught);
		});
	CHECK_TRUE(caughtException);

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testTrapInstructionIndexWithInlining(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("inliningTest");
	WAVM_ERROR_UNLESS(compartment);

	// A module with two functions:
	// - Function 0 (inner): nop, unreachable, end  (trap at instruction index 1)
	// - Function 1 (outer): nop, call 0, end       (call at instruction index 1)
	// Both are exported. LLVM may inline function 0 into function 1.
	static const char wat[]
		= "(module"
		  "  (func (export \"inner\") nop unreachable)"
		  "  (func (export \"outer\") nop (call 0))"
		  ")";

	ModuleRef compiledModule;
	bool loaded = loadTextModule(wat, sizeof(wat), compiledModule);
	CHECK_TRUE(loaded);
	WAVM_ERROR_UNLESS(loaded && compiledModule);

	ImportBindings imports;
	Instance* instance
		= instantiateModule(compartment, compiledModule, std::move(imports), "inliningInstance");
	WAVM_ERROR_UNLESS(instance);

	Object* innerExport = getInstanceExport(instance, "inner");
	CHECK_NOT_NULL(innerExport);
	WAVM_ERROR_UNLESS(innerExport);
	Function* innerFunc = asFunctionNullable(innerExport);
	CHECK_NOT_NULL(innerFunc);
	WAVM_ERROR_UNLESS(innerFunc);

	Object* outerExport = getInstanceExport(instance, "outer");
	CHECK_NOT_NULL(outerExport);
	WAVM_ERROR_UNLESS(outerExport);
	Function* outerFunc = asFunctionNullable(outerExport);
	CHECK_NOT_NULL(outerFunc);
	WAVM_ERROR_UNLESS(outerFunc);

	Context* context = createContext(compartment, "inliningContext");
	WAVM_ERROR_UNLESS(context);

	bool caughtException = false;
	catchRuntimeExceptions(
		[&]() { invokeFunction(context, outerFunc, getFunctionType(outerFunc)); },
		[&](Exception* caught) {
			caughtException = true;

			const CallStack& callStack = getExceptionCallStack(caught);

			// Collect all instruction sources including inline frames, skipping unknown
			// frames and any native frames at the top of the stack (runtime internals).
			std::vector<InstructionSource> framesFromFirstWasm;
			for(const auto& frame : callStack.frames)
			{
				InstructionSource inlineSources[Runtime::maxInlineSourceFrames];
				Uptr numInline = getInstructionSourceByAddress(
					frame.ip, inlineSources, Runtime::maxInlineSourceFrames);
				for(Uptr si = 0; si < numInline; ++si)
				{
					if(inlineSources[si].type == InstructionSource::Type::wasm
					   || !framesFromFirstWasm.empty())
					{
						framesFromFirstWasm.push_back(inlineSources[si]);
					}
				}
			}

			if(CHECK_GE(framesFromFirstWasm.size(), Uptr(2)))
			{
				if(CHECK_EQ(framesFromFirstWasm[0].type, InstructionSource::Type::wasm))
				{
					CHECK_EQ(framesFromFirstWasm[0].wasm.function, innerFunc);
					CHECK_EQ(framesFromFirstWasm[0].wasm.instructionIndex, Uptr(1));
				}

				if(CHECK_EQ(framesFromFirstWasm[1].type, InstructionSource::Type::wasm))
				{
					CHECK_EQ(framesFromFirstWasm[1].wasm.function, outerFunc);
					CHECK_EQ(framesFromFirstWasm[1].wasm.instructionIndex, Uptr(1));
				}
			}

			destroyException(caught);
		});
	CHECK_TRUE(caughtException);

	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

static void testGarbageCollection(TEST_STATE_PARAM)
{
	GCPointer<Compartment> compartment = createCompartment("gcTest");
	WAVM_ERROR_UNLESS(compartment);

	// Create several objects.
	Memory* memory = createMemory(
		compartment, MemoryType(false, IndexType::i32, SizeConstraints{1, 1}), "gcMemory");
	WAVM_ERROR_UNLESS(memory);

	Table* table = createTable(compartment,
							   TableType(ReferenceType::funcref, false, IndexType::i32, {1, 1}),
							   nullptr,
							   "gcTable");
	WAVM_ERROR_UNLESS(table);

	// Add a GC root for the memory, so it persists across garbage collection.
	addGCRoot(memory);

	// Collect garbage; table has no root so it may be freed.
	collectCompartmentGarbage(compartment);

	// The memory should still be accessible since it has a root.
	CHECK_EQ(getMemoryNumPages(memory), Uptr(1));

	removeGCRoot(memory);

	// Test tryCollectCompartment.
	CHECK_TRUE(tryCollectCompartment(std::move(compartment)));
}

int execAPITest(int argc, char** argv)
{
	TEST_STATE_LOCAL;
	testCompartmentCloning(testState);
	testResourceQuotas(testState);
	testMemoryOperations(testState);
	testTableOperations(testState);
	testGlobalOperations(testState);
	testExceptionTypes(testState);
	testModuleCompileAndIntrospect(testState);
	testTrapInstructionIndex(testState);
	testTrapInstructionIndexWithInlining(testState);
	testForeignObjects(testState);
	testGarbageCollection(testState);
	return testState.exitCode();
}
