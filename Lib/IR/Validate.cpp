#include "WAVM/IR/Validate.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/OperatorPrinter.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Logging/Logging.h"

#define ENABLE_LOGGING 0

using namespace WAVM;
using namespace WAVM::IR;

#define VALIDATE_UNLESS(reason, comparison)                                                        \
	if(comparison) { throw ValidationException(reason #comparison); }

#define VALIDATE_INDEX(index, arraySize)                                                           \
	if(index >= arraySize)                                                                         \
	{                                                                                              \
		throw ValidationException(                                                                 \
			std::string("invalid index: " #index " must be less than " #arraySize " (" #index "=") \
			+ std::to_string(index) + (", " #arraySize "=") + std::to_string(arraySize) + ')');    \
	}

#define VALIDATE_FEATURE(context, feature)                                                         \
	if(!module.featureSpec.feature)                                                                \
	{ throw ValidationException(context " requires " #feature " feature"); }

static void validate(const IR::FeatureSpec& featureSpec, IR::ValueType valueType)
{
	bool isValid;
	switch(valueType)
	{
	case ValueType::i32:
	case ValueType::i64:
	case ValueType::f32:
	case ValueType::f64: isValid = featureSpec.mvp; break;
	case ValueType::v128: isValid = featureSpec.simd; break;
	case ValueType::anyref:
	case ValueType::anyfunc: isValid = featureSpec.referenceTypes; break;
	default: isValid = false;
	};

	if(!isValid)
	{ throw ValidationException("invalid value type (" + std::to_string((Uptr)valueType) + ")"); }
}

static void validate(SizeConstraints size, U64 maxMax)
{
	U64 max = size.max == UINT64_MAX ? maxMax : size.max;
	VALIDATE_UNLESS("disjoint size bounds: ", size.min > max);
	VALIDATE_UNLESS("maximum size exceeds limit: ", max > maxMax);
}

static void validate(const IR::FeatureSpec& featureSpec, ReferenceType type)
{
	bool isValid;
	switch(type)
	{
	case ReferenceType::anyfunc: isValid = featureSpec.mvp; break;
	case ReferenceType::anyref: isValid = featureSpec.referenceTypes; break;
	default: isValid = false;
	}

	if(!isValid)
	{ throw ValidationException("invalid reference type (" + std::to_string((Uptr)type) + ")"); }
}

static void validate(const Module& module, TableType type)
{
	validate(module.featureSpec, type.elementType);
	validate(type.size, IR::maxTableElems);
	if(type.isShared)
	{
		VALIDATE_FEATURE("shared table", sharedTables);
		VALIDATE_UNLESS("shared tables must have a maximum size: ", type.size.max == UINT64_MAX);
	}
}

static void validate(const Module& module, MemoryType type)
{
	validate(type.size, IR::maxMemoryPages);
	if(type.isShared)
	{
		VALIDATE_FEATURE("shared memory", atomics);
		VALIDATE_UNLESS("shared memories must have a maximum size: ", type.size.max == UINT64_MAX);
	}
}

static void validate(const IR::FeatureSpec& featureSpec, GlobalType type)
{
	validate(featureSpec, type.valueType);
}

static void validate(const IR::FeatureSpec& featureSpec, TypeTuple typeTuple)
{
	for(ValueType valueType : typeTuple) { validate(featureSpec, valueType); }
}

template<typename Type> void validateType(Type expectedType, Type actualType, const char* context)
{
	if(!isSubtype(actualType, expectedType))
	{
		throw ValidationException(std::string("type mismatch: expected ") + asString(expectedType)
								  + " but got " + asString(actualType) + " in " + context);
	}
}

static ValueType validateGlobalIndex(const Module& module,
									 Uptr globalIndex,
									 bool mustBeMutable,
									 bool mustBeImmutable,
									 bool mustBeImport,
									 const char* context)
{
	VALIDATE_INDEX(globalIndex, module.globals.size());
	const GlobalType& globalType = module.globals.getType(globalIndex);
	if(mustBeMutable && !globalType.isMutable)
	{ throw ValidationException("attempting to mutate immutable global"); }
	else if(mustBeImport && globalIndex >= module.globals.imports.size())
	{
		throw ValidationException(
			"global variable initializer expression may only access imported globals");
	}
	else if(mustBeImmutable && globalType.isMutable)
	{
		throw ValidationException(
			"global variable initializer expression may only access immutable globals");
	}
	return globalType.valueType;
}

static FunctionType validateFunctionIndex(const Module& module, Uptr functionIndex)
{
	VALIDATE_INDEX(functionIndex, module.functions.size());
	return module.types[module.functions.getType(functionIndex).index];
}

static FunctionType validateBlockType(const Module& module, const IndexedBlockType& type)
{
	switch(type.format)
	{
	case IndexedBlockType::noParametersOrResult: return FunctionType();
	case IndexedBlockType::oneResult:
		validate(module.featureSpec, type.resultType);
		return FunctionType(TypeTuple(type.resultType));
	case IndexedBlockType::functionType:
	{
		VALIDATE_INDEX(type.index, module.types.size());
		FunctionType functionType = module.types[type.index];
		if(functionType.params().size() > 0 && !module.featureSpec.multipleResultsAndBlockParams)
		{
			throw ValidationException("block has params, but \"multivalue\" extension is disabled");
		}
		else if(functionType.results().size() > 1
				&& !module.featureSpec.multipleResultsAndBlockParams)
		{
			throw ValidationException(
				"block has multiple results, but \"multivalue\" extension is disabled");
		}
		return functionType;
	}
	default: Errors::unreachable();
	}
}

static FunctionType validateFunctionType(const Module& module, const IndexedFunctionType& type)
{
	VALIDATE_INDEX(type.index, module.types.size());
	const FunctionType functionType = module.types[type.index];
	if(functionType.results().size() > IR::maxReturnValues)
	{ throw ValidationException("function has more return values than WAVM can support"); }
	return functionType;
}

static void validateInitializer(const Module& module,
								const InitializerExpression& expression,
								ValueType expectedType,
								const char* context)
{
	switch(expression.type)
	{
	case InitializerExpression::Type::i32_const:
		validateType(expectedType, ValueType::i32, context);
		break;
	case InitializerExpression::Type::i64_const:
		validateType(expectedType, ValueType::i64, context);
		break;
	case InitializerExpression::Type::f32_const:
		validateType(expectedType, ValueType::f32, context);
		break;
	case InitializerExpression::Type::f64_const:
		validateType(expectedType, ValueType::f64, context);
		break;
	case InitializerExpression::Type::v128_const:
		validateType(expectedType, ValueType::v128, context);
		break;
	case InitializerExpression::Type::get_global:
	{
		const ValueType globalValueType = validateGlobalIndex(
			module, expression.globalRef, false, true, true, "initializer expression global index");
		validateType(expectedType, globalValueType, context);
		break;
	}
	case InitializerExpression::Type::ref_null:
		validateType(expectedType, ValueType::nullref, context);
		break;
	default: throw ValidationException("invalid initializer expression");
	};
}

struct FunctionValidationContext
{
	FunctionValidationContext(const Module& inModule,
							  const FunctionDef& inFunctionDef,
							  DeferredCodeValidationState& inDeferredCodeValidationState)
	: module(inModule)
	, functionDef(inFunctionDef)
	, deferredCodeValidationState(inDeferredCodeValidationState)
	, functionType(inModule.types[inFunctionDef.type.index])
	{
		// Validate the function's local types.
		for(auto localType : functionDef.nonParameterLocalTypes)
		{ validate(module.featureSpec, localType); }

		// Initialize the local types.
		locals.reserve(functionType.params().size() + functionDef.nonParameterLocalTypes.size());
		locals.insert(locals.end(), functionType.params().begin(), functionType.params().end());
		locals.insert(locals.end(),
					  functionDef.nonParameterLocalTypes.begin(),
					  functionDef.nonParameterLocalTypes.end());

		// Log the start of the function and its signature+locals.
		if(ENABLE_LOGGING)
		{
			logOperator("func");
			for(auto param : functionType.params())
			{ logOperator(std::string("param ") + asString(param)); }
			for(auto result : functionType.results())
			{ logOperator(std::string("result ") + asString(result)); }
			for(auto local : functionDef.nonParameterLocalTypes)
			{ logOperator(std::string("local ") + asString(local)); }
		}

		// Push the function context onto the control stack.
		pushControlStack(
			ControlContext::Type::function, functionType.results(), functionType.results());
	}

	Uptr getControlStackSize() { return controlStack.size(); }

	void validateNonEmptyControlStack(const char* context)
	{
		if(controlStack.size() == 0)
		{
			throw ValidationException(std::string("Expected non-empty control stack in ")
									  + context);
		}
	}

	void logOperator(const std::string& operatorDescription)
	{
		if(ENABLE_LOGGING)
		{
			std::string controlStackString;
			for(Uptr stackIndex = 0; stackIndex < controlStack.size(); ++stackIndex)
			{
				if(!controlStack[stackIndex].isReachable) { controlStackString += "("; }
				switch(controlStack[stackIndex].type)
				{
				case ControlContext::Type::function: controlStackString += "F"; break;
				case ControlContext::Type::block: controlStackString += "B"; break;
				case ControlContext::Type::ifThen: controlStackString += "T"; break;
				case ControlContext::Type::ifElse: controlStackString += "E"; break;
				case ControlContext::Type::loop: controlStackString += "L"; break;
				case ControlContext::Type::try_: controlStackString += "R"; break;
				case ControlContext::Type::catch_: controlStackString += "C"; break;
				default: Errors::unreachable();
				};
				if(!controlStack[stackIndex].isReachable) { controlStackString += ")"; }
			}

			std::string stackString;
			const Uptr stackBase
				= controlStack.size() == 0 ? 0 : controlStack.back().outerStackSize;
			for(Uptr stackIndex = 0; stackIndex < stack.size(); ++stackIndex)
			{
				if(stackIndex == stackBase) { stackString += "| "; }
				stackString += asString(stack[stackIndex]);
				stackString += " ";
			}
			if(stack.size() == stackBase) { stackString += "|"; }

			Log::printf(Log::debug,
						"%-50s %-50s %-50s\n",
						controlStackString.c_str(),
						operatorDescription.c_str(),
						stackString.c_str());
		}
	}

	// Operation dispatch methods.
	void unknown(Opcode opcode)
	{
		throw ValidationException("Unknown opcode: " + std::to_string((Uptr)opcode));
	}
	void block(ControlStructureImm imm)
	{
		const FunctionType type = validateBlockType(module, imm.type);
		popAndValidateTypeTuple("block arguments", type.params());
		pushControlStack(ControlContext::Type::block, type.results(), type.results());

		pushOperandTuple(type.params());
	}
	void loop(ControlStructureImm imm)
	{
		const FunctionType type = validateBlockType(module, imm.type);
		popAndValidateTypeTuple("loop arguments", type.params());
		pushControlStack(ControlContext::Type::loop, type.params(), type.results());
		pushOperandTuple(type.params());
	}
	void if_(ControlStructureImm imm)
	{
		const FunctionType type = validateBlockType(module, imm.type);
		popAndValidateOperand("if condition", ValueType::i32);
		popAndValidateTypeTuple("if arguments", type.params());
		pushControlStack(
			ControlContext::Type::ifThen, type.results(), type.results(), type.params());
		pushOperandTuple(type.params());
	}
	void else_(NoImm imm)
	{
		wavmAssert(controlStack.size());

		if(controlStack.back().type != ControlContext::Type::ifThen)
		{ throw ValidationException("else only allowed in if context"); }

		popAndValidateTypeTuple("if result", controlStack.back().results);
		validateStackEmptyAtEndOfControlStructure();

		controlStack.back().type = ControlContext::Type::ifElse;
		controlStack.back().isReachable = true;

		pushOperandTuple(controlStack.back().elseParams);
	}
	void end(NoImm)
	{
		wavmAssert(controlStack.size());

		if(controlStack.back().type == ControlContext::Type::try_)
		{ throw ValidationException("end may not occur in try context"); }

		TypeTuple results = controlStack.back().results;
		if(controlStack.back().type == ControlContext::Type::ifThen
		   && results != controlStack.back().elseParams)
		{
			throw ValidationException("else-less if must have identity signature");
		}

		popAndValidateTypeTuple("end result", controlStack.back().results);
		validateStackEmptyAtEndOfControlStructure();
				
		controlStack.pop_back();
		if(controlStack.size()) { pushOperandTuple(results); }
	}
	void try_(ControlStructureImm imm)
	{
		const FunctionType type = validateBlockType(module, imm.type);
		VALIDATE_FEATURE("try", exceptionHandling);
		popAndValidateTypeTuple("try arguments", type.params());
		pushControlStack(ControlContext::Type::try_, type.results(), type.results());
		pushOperandTuple(type.params());
	}
	void validateCatch()
	{
		wavmAssert(controlStack.size());

		popAndValidateTypeTuple("try result", controlStack.back().results);
		validateStackEmptyAtEndOfControlStructure();

		if(controlStack.back().type == ControlContext::Type::try_
		   || controlStack.back().type == ControlContext::Type::catch_)
		{
			controlStack.back().type = ControlContext::Type::catch_;
			controlStack.back().isReachable = true;
		}
		else
		{
			throw ValidationException("catch only allowed in try/catch context");
		}
	}
	void catch_(ExceptionTypeImm imm)
	{
		VALIDATE_FEATURE("catch", exceptionHandling);
		VALIDATE_INDEX(imm.exceptionTypeIndex, module.exceptionTypes.size());
		ExceptionType type = module.exceptionTypes.getType(imm.exceptionTypeIndex);
		validateCatch();
		for(auto param : type.params) { pushOperand(param); }
	}
	void catch_all(NoImm)
	{
		VALIDATE_FEATURE("catch_all", exceptionHandling);
		validateCatch();
	}

	void return_(NoImm)
	{
		popAndValidateTypeTuple("ret", functionType.results());
		enterUnreachable();
	}

	void br(BranchImm imm)
	{
		popAndValidateTypeTuple("br argument", getBranchTargetByDepth(imm.targetDepth).params);
		enterUnreachable();
	}
	void br_table(BranchTableImm imm)
	{
		popAndValidateOperand("br_table index", ValueType::i32);

		const TypeTuple defaultTargetParams = getBranchTargetByDepth(imm.defaultTargetDepth).params;

		// Meet (intersect) the parameters of all the branch targets to get the most general type
		// that matches any of the targets.
		const Uptr numTargetParams = defaultTargetParams.size();
		ValueType* targetParamMeets = new(alloca(sizeof(ValueType) * defaultTargetParams.size()))
			ValueType[numTargetParams];
		for(Uptr elementIndex = 0; elementIndex < numTargetParams; ++elementIndex)
		{ targetParamMeets[elementIndex] = defaultTargetParams[elementIndex]; }

		wavmAssert(imm.branchTableIndex < functionDef.branchTables.size());
		const std::vector<Uptr>& targetDepths = functionDef.branchTables[imm.branchTableIndex];
		for(Uptr targetIndex = 0; targetIndex < targetDepths.size(); ++targetIndex)
		{
			const TypeTuple targetParams = getBranchTargetByDepth(targetDepths[targetIndex]).params;
			if(targetParams.size() != numTargetParams)
			{
				throw ValidationException(
					"br_table targets must all take the same number of parameters");
			}
			else
			{
				for(Uptr elementIndex = 0; elementIndex < numTargetParams; ++elementIndex)
				{
					targetParamMeets[elementIndex]
						= meet(targetParamMeets[elementIndex], targetParams[elementIndex]);
				}
			}
		}

		// If any target's parameters are disjoint from any other target's parameters, the result of
		// the meet will be empty; i.e. there are no possible arguments that will match any of the
		// branch targets.
		for(Uptr elementIndex = 0; elementIndex < numTargetParams; ++elementIndex)
		{
			if(targetParamMeets[elementIndex] == ValueType::none)
			{ throw ValidationException("br_table targets have incompatible parameter types"); }
		}

		// Validate that the br_table's arguments match the meet of the target's parameters.
		popAndValidateOperands(
			"br_table argument", (const ValueType*)targetParamMeets, numTargetParams);

		enterUnreachable();
	}
	void br_if(BranchImm imm)
	{
		const TypeTuple targetParams = getBranchTargetByDepth(imm.targetDepth).params;
		popAndValidateOperand("br_if condition", ValueType::i32);
		popAndValidateTypeTuple("br_if argument", targetParams);
		pushOperandTuple(targetParams);
	}

	void unreachable(NoImm) { enterUnreachable(); }
	void drop(NoImm) { popAndValidateOperand("drop", ValueType::any); }

	void select(NoImm)
	{
		popAndValidateOperand("select condition", ValueType::i32);
		const ValueType falseType = popAndValidateOperand("select false value", ValueType::any);
		const ValueType trueType = popAndValidateOperand("select true value", ValueType::any);
		if(falseType == ValueType::any) { pushOperand(trueType); }
		else if(trueType == ValueType::any)
		{
			pushOperand(falseType);
		}
		else
		{
			const ValueType resultType = join(falseType, trueType);
			if(resultType == ValueType::any)
			{ throw ValidationException("select operands must have a common supertype"); }
			pushOperand(resultType);
		}
	}

	void get_local(GetOrSetVariableImm<false> imm)
	{
		pushOperand(validateLocalIndex(imm.variableIndex));
	}
	void set_local(GetOrSetVariableImm<false> imm)
	{
		popAndValidateOperand("set_local", validateLocalIndex(imm.variableIndex));
	}
	void tee_local(GetOrSetVariableImm<false> imm)
	{
		const ValueType localType = validateLocalIndex(imm.variableIndex);
		const ValueType operandType = popAndValidateOperand("tee_local", localType);
		pushOperand(operandType);
	}

	void get_global(GetOrSetVariableImm<true> imm)
	{
		pushOperand(
			validateGlobalIndex(module, imm.variableIndex, false, false, false, "get_global"));
	}
	void set_global(GetOrSetVariableImm<true> imm)
	{
		popAndValidateOperand(
			"set_global",
			validateGlobalIndex(module, imm.variableIndex, true, false, false, "set_global"));
	}

	void table_get(TableImm imm)
	{
		VALIDATE_INDEX(imm.tableIndex, module.tables.size());
		const TableType& tableType = module.tables.getType(imm.tableIndex);
		popAndValidateOperand("table.get", ValueType::i32);
		pushOperand(asValueType(tableType.elementType));
	}
	void table_set(TableImm imm)
	{
		VALIDATE_INDEX(imm.tableIndex, module.tables.size());
		const TableType& tableType = module.tables.getType(imm.tableIndex);
		popAndValidateOperands("table.get", ValueType::i32, asValueType(tableType.elementType));
	}

	void throw_(ExceptionTypeImm imm)
	{
		VALIDATE_FEATURE("throw", exceptionHandling);
		VALIDATE_INDEX(imm.exceptionTypeIndex, module.exceptionTypes.size());
		ExceptionType exceptionType = module.exceptionTypes.getType(imm.exceptionTypeIndex);
		popAndValidateTypeTuple("exception arguments", exceptionType.params);
		enterUnreachable();
	}

	void rethrow(RethrowImm imm)
	{
		VALIDATE_FEATURE("rethrow", exceptionHandling);
		VALIDATE_UNLESS(
			"rethrow must target a catch: ",
			getBranchTargetByDepth(imm.catchDepth).type != ControlContext::Type::catch_);
		enterUnreachable();
	}

	void call(FunctionImm imm)
	{
		FunctionType calleeType = validateFunctionIndex(module, imm.functionIndex);
		popAndValidateTypeTuple("call arguments", calleeType.params());
		pushOperandTuple(calleeType.results());
	}
	void call_indirect(CallIndirectImm imm)
	{
		VALIDATE_INDEX(imm.tableIndex, module.tables.size());
		VALIDATE_UNLESS(
			"call_indirect requires a table element type of anyfunc: ",
			module.tables.getType(imm.tableIndex).elementType != ReferenceType::anyfunc);
		FunctionType calleeType = validateFunctionType(module, imm.type);
		popAndValidateOperand("call_indirect function index", ValueType::i32);
		popAndValidateTypeTuple("call_indirect arguments", calleeType.params());
		pushOperandTuple(calleeType.results());
	}

	void validateImm(NoImm) {}

	template<typename nativeType> void validateImm(LiteralImm<nativeType> imm) {}

	template<Uptr naturalAlignmentLog2> void validateImm(LoadOrStoreImm<naturalAlignmentLog2> imm)
	{
		VALIDATE_UNLESS("load or store alignment greater than natural alignment: ",
						imm.alignmentLog2 > naturalAlignmentLog2);
		VALIDATE_UNLESS("load or store in module without default memory: ",
						module.memories.size() == 0);
	}

	void validateImm(MemoryImm imm) { VALIDATE_INDEX(imm.memoryIndex, module.memories.size()); }

	void validateImm(TableImm imm) { VALIDATE_INDEX(imm.tableIndex, module.tables.size()); }

	void validateImm(FunctionImm imm) { validateFunctionIndex(module, imm.functionIndex); }

	template<Uptr numLanes> void validateImm(LaneIndexImm<numLanes> imm)
	{
		VALIDATE_UNLESS("swizzle invalid lane index: ", imm.laneIndex >= numLanes);
	}

	template<Uptr numLanes> void validateImm(ShuffleImm<numLanes> imm)
	{
		for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
		{
			VALIDATE_UNLESS("shuffle invalid lane index: ",
							imm.laneIndices[laneIndex] >= numLanes * 2);
		}
	}

	template<Uptr naturalAlignmentLog2>
	void validateImm(AtomicLoadOrStoreImm<naturalAlignmentLog2> imm)
	{
		VALIDATE_UNLESS("atomic memory operator in module without default memory: ",
						module.memories.size() == 0);
		if(module.featureSpec.requireSharedFlagForAtomicOperators)
		{
			VALIDATE_UNLESS("atomic memory operators require a memory with the shared flag: ",
							!module.memories.getType(0).isShared);
		}
		VALIDATE_UNLESS("atomic memory operators must have natural alignment: ",
						imm.alignmentLog2 != naturalAlignmentLog2);
	}

	void validateImm(DataSegmentAndMemImm imm)
	{
		VALIDATE_INDEX(imm.memoryIndex, module.memories.size());

		// Defer the validation of the data segment index until the data section is deserialized.
		deferredCodeValidationState.requiredNumDataSegments = std::max(
			deferredCodeValidationState.requiredNumDataSegments, imm.dataSegmentIndex + 1);
	}

	void validateImm(DataSegmentImm imm)
	{
		// Defer the validation of the data segment index until the data section is deserialized.
		deferredCodeValidationState.requiredNumDataSegments = std::max(
			deferredCodeValidationState.requiredNumDataSegments, imm.dataSegmentIndex + 1);
	}

	void validateImm(ElemSegmentAndTableImm imm)
	{
		VALIDATE_INDEX(imm.elemSegmentIndex, module.elemSegments.size());
		VALIDATE_INDEX(imm.tableIndex, module.tables.size());
	}

	void validateImm(ElemSegmentImm imm)
	{
		VALIDATE_INDEX(imm.elemSegmentIndex, module.elemSegments.size());
	}

#define VALIDATE_OP(opcode, name, nameString, Imm, signatureInitializer, requiredFeature)          \
	void name(Imm imm)                                                                             \
	{                                                                                              \
		VALIDATE_FEATURE(nameString, requiredFeature);                                             \
		const char* operatorName = nameString;                                                     \
		SUPPRESS_UNUSED(operatorName);                                                             \
		validateImm(imm);                                                                          \
		popAndValidateTypeTuple("call arguments", IR::getNonParametricOpSigs().name.params());     \
		pushOperandTuple(IR::getNonParametricOpSigs().name.results());                             \
	}
	ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VALIDATE_OP)
#undef VALIDATE_OP

private:
	struct ControlContext
	{
		enum class Type : U8
		{
			function,
			block,
			ifThen,
			ifElse,
			loop,
			try_,
			catch_
		};

		Type type;
		Uptr outerStackSize;

		TypeTuple params;
		TypeTuple results;
		bool isReachable;

		TypeTuple elseParams;
	};

	const Module& module;
	const FunctionDef& functionDef;
	DeferredCodeValidationState& deferredCodeValidationState;
	FunctionType functionType;

	std::vector<ValueType> locals;
	std::vector<ControlContext> controlStack;
	std::vector<ValueType> stack;

	void pushControlStack(ControlContext::Type type,
						  TypeTuple params,
						  TypeTuple results,
						  TypeTuple elseParams = TypeTuple())
	{
		controlStack.push_back({type, stack.size(), params, results, true, elseParams});
	}

	void validateStackEmptyAtEndOfControlStructure()
	{
		wavmAssert(controlStack.size());

		if(stack.size() != controlStack.back().outerStackSize)
		{
			std::string message = "stack was not empty at end of control structure: ";
			for(Uptr stackIndex = controlStack.back().outerStackSize; stackIndex < stack.size();
				++stackIndex)
			{
				if(stackIndex != controlStack.back().outerStackSize) { message += ", "; }
				message += asString(stack[stackIndex]);
			}
			throw ValidationException(std::move(message));
		}
	}

	void enterUnreachable()
	{
		wavmAssert(controlStack.size());

		stack.resize(controlStack.back().outerStackSize);
		controlStack.back().isReachable = false;
	}

	void validateBranchDepth(Uptr depth) const
	{
		VALIDATE_INDEX(depth, controlStack.size());
		if(depth >= controlStack.size()) { throw ValidationException("invalid branch depth"); }
	}

	const ControlContext& getBranchTargetByDepth(Uptr depth) const
	{
		validateBranchDepth(depth);
		return controlStack[controlStack.size() - depth - 1];
	}

	ValueType validateLocalIndex(Uptr localIndex)
	{
		VALIDATE_INDEX(localIndex, locals.size());
		return locals[localIndex];
	}

	void popAndValidateOperands(const char* context, const ValueType* expectedTypes, Uptr num)
	{
		for(Uptr operandIndexFromEnd = 0; operandIndexFromEnd < num; ++operandIndexFromEnd)
		{
			const Uptr operandIndex = num - operandIndexFromEnd - 1;
			popAndValidateOperand(context, expectedTypes[operandIndex]);
		}
	}

	template<Uptr num>
	void popAndValidateOperands(const char* context, const ValueType (&expectedTypes)[num])
	{
		popAndValidateOperands(context, expectedTypes, num);
	}

	template<typename... OperandTypes>
	void popAndValidateOperands(const char* context, OperandTypes... operands)
	{
		ValueType operandTypes[] = {operands...};
		popAndValidateOperands(context, operandTypes);
	}

	ValueType popAndValidateOperand(const char* context, const ValueType expectedType)
	{
		wavmAssert(controlStack.size());

		ValueType actualType;
		if(stack.size() > controlStack.back().outerStackSize)
		{
			actualType = stack.back();
			stack.pop_back();
		}
		else if(!controlStack.back().isReachable)
		{
			// If the current instruction is unreachable, then pop a polymorphic type that will
			// match any expected type.
			actualType = ValueType::any;
		}
		else
		{
			// If the current instruction is reachable, but the operand stack is empty, then throw a
			// validation exception.
			throw ValidationException(std::string("type mismatch: expected ")
									  + asString(expectedType) + " but stack was empty" + " in "
									  + context + " operand");
		}

		if(actualType != ValueType::any && !isSubtype(actualType, expectedType))
		{
			throw ValidationException(std::string("type mismatch: expected ")
									  + asString(expectedType) + " but got " + asString(actualType)
									  + " in " + context + " operand");
		}

		return actualType;
	}

	void popAndValidateTypeTuple(const char* context, TypeTuple expectedType)
	{
		popAndValidateOperands(context, expectedType.data(), expectedType.size());
	}

	void pushOperand(ValueType type) { stack.push_back(type); }
	void pushOperandTuple(TypeTuple typeTuple)
	{
		for(ValueType type : typeTuple) { pushOperand(type); }
	}
};

void IR::validateTypes(const Module& module)
{
	for(Uptr typeIndex = 0; typeIndex < module.types.size(); ++typeIndex)
	{
		FunctionType functionType = module.types[typeIndex];

		// Validate the function type parameters and results here, but don't check the limit on
		// number of return values here, since they don't apply to block types that are also stored
		// here. Instead, uses of a function type from the types array must call
		// validateFunctionType to validate its use as a function type.
		validate(module.featureSpec, functionType.params());
		validate(module.featureSpec, functionType.results());

		if(functionType.results().size() > 1 && !module.featureSpec.multipleResultsAndBlockParams)
		{
			throw ValidationException(
				"function/block has multiple return values, but \"multivalue\" extension is "
				"disabled");
		}
	}
}

void IR::validateImports(const Module& module)
{
	for(auto& functionImport : module.functions.imports)
	{ validateFunctionType(module, functionImport.type); }
	for(auto& tableImport : module.tables.imports) { validate(module, tableImport.type); }
	for(auto& memoryImport : module.memories.imports) { validate(module, memoryImport.type); }
	for(auto& globalImport : module.globals.imports)
	{
		validate(module.featureSpec, globalImport.type);
		if(!module.featureSpec.importExportMutableGlobals)
		{ VALIDATE_UNLESS("mutable globals cannot be imported: ", globalImport.type.isMutable); }
	}
	for(auto& exceptionTypeImport : module.exceptionTypes.imports)
	{ validate(module.featureSpec, exceptionTypeImport.type.params); }

	VALIDATE_UNLESS("too many tables: ",
					!module.featureSpec.referenceTypes && module.tables.size() > 1);
	VALIDATE_UNLESS("too many memories: ", module.memories.size() > 1);
}

void IR::validateFunctionDeclarations(const Module& module)
{
	for(Uptr functionDefIndex = 0; functionDefIndex < module.functions.defs.size();
		++functionDefIndex)
	{
		const FunctionDef& functionDef = module.functions.defs[functionDefIndex];
		validateFunctionType(module, functionDef.type);
	}
}

void IR::validateGlobalDefs(const Module& module)
{
	for(auto& globalDef : module.globals.defs)
	{
		validate(module.featureSpec, globalDef.type);
		validateInitializer(module,
							globalDef.initializer,
							globalDef.type.valueType,
							"global initializer expression");
	}
}

void IR::validateExceptionTypeDefs(const Module& module)
{
	for(auto& exceptionTypeDef : module.exceptionTypes.defs)
	{ validate(module.featureSpec, exceptionTypeDef.type.params); }
}

void IR::validateTableDefs(const Module& module)
{
	for(auto& tableDef : module.tables.defs) { validate(module, tableDef.type); }
	VALIDATE_UNLESS("too many tables: ",
					!module.featureSpec.referenceTypes && module.tables.size() > 1);
}

void IR::validateMemoryDefs(const Module& module)
{
	for(auto& memoryDef : module.memories.defs) { validate(module, memoryDef.type); }
	VALIDATE_UNLESS("too many memories: ", module.memories.size() > 1);
}

void IR::validateExports(const Module& module)
{
	HashSet<std::string> exportNameSet;
	for(auto& exportIt : module.exports)
	{
		switch(exportIt.kind)
		{
		case ExternKind::function: VALIDATE_INDEX(exportIt.index, module.functions.size()); break;
		case ExternKind::table: VALIDATE_INDEX(exportIt.index, module.tables.size()); break;
		case ExternKind::memory: VALIDATE_INDEX(exportIt.index, module.memories.size()); break;
		case ExternKind::global:
			validateGlobalIndex(module,
								exportIt.index,
								false,
								!module.featureSpec.importExportMutableGlobals,
								false,
								"exported global index");
			break;
		case ExternKind::exceptionType:
			VALIDATE_INDEX(exportIt.index, module.exceptionTypes.size());
			break;
		default: throw ValidationException("unknown export kind");
		};

		VALIDATE_UNLESS("duplicate export: ", exportNameSet.contains(exportIt.name));
		exportNameSet.addOrFail(exportIt.name);
	}
}

void IR::validateStartFunction(const Module& module)
{
	if(module.startFunctionIndex != UINTPTR_MAX)
	{
		VALIDATE_INDEX(module.startFunctionIndex, module.functions.size());
		FunctionType startFunctionType
			= module.types[module.functions.getType(module.startFunctionIndex).index];
		VALIDATE_UNLESS("start function must not have any parameters or results: ",
						startFunctionType != FunctionType());
	}
}

void IR::validateElemSegments(const Module& module)
{
	for(auto& elemSegment : module.elemSegments)
	{
		if(elemSegment.isActive)
		{
			VALIDATE_INDEX(elemSegment.tableIndex, module.tables.size());
			const TableType& tableType = module.tables.getType(elemSegment.tableIndex);
			VALIDATE_UNLESS("active elem segments must be in anyfunc tables: ",
							tableType.elementType != ReferenceType::anyfunc);
			validateInitializer(
				module, elemSegment.baseOffset, ValueType::i32, "elem segment base initializer");
		}
		for(auto functionIndex : elemSegment.indices)
		{ VALIDATE_INDEX(functionIndex, module.functions.size()); }
	}
}

void IR::validateDataSegments(const Module& module,
							  const DeferredCodeValidationState& deferredCodeValidationState)
{
	if(deferredCodeValidationState.requiredNumDataSegments > module.dataSegments.size())
	{ throw ValidationException("invalid data segment index in operator immediate"); }

	for(auto& dataSegment : module.dataSegments)
	{
		if(dataSegment.isActive)
		{
			VALIDATE_INDEX(dataSegment.memoryIndex, module.memories.size());
			validateInitializer(
				module, dataSegment.baseOffset, ValueType::i32, "data segment base initializer");
		}
	}
}

namespace WAVM { namespace IR {
	struct CodeValidationStreamImpl
	{
		FunctionValidationContext functionContext;
		OperatorPrinter operatorPrinter;

		CodeValidationStreamImpl(const Module& module,
								 const FunctionDef& functionDef,
								 DeferredCodeValidationState& deferredCodeValidationState)
		: functionContext(module, functionDef, deferredCodeValidationState)
		, operatorPrinter(module, functionDef)
		{
		}
	};
}}

IR::CodeValidationStream::CodeValidationStream(
	const Module& module,
	const FunctionDef& functionDef,
	DeferredCodeValidationState& deferredCodeValidationState)
{
	impl = new CodeValidationStreamImpl(module, functionDef, deferredCodeValidationState);
}

IR::CodeValidationStream::~CodeValidationStream()
{
	delete impl;
	impl = nullptr;
}

void IR::CodeValidationStream::finish()
{
	if(impl->functionContext.getControlStackSize())
	{ throw ValidationException("end of code reached before end of function"); }
}

#define VISIT_OPCODE(_, name, nameString, Imm, ...)                                                \
	void IR::CodeValidationStream::name(Imm imm)                                                   \
	{                                                                                              \
		if(ENABLE_LOGGING) { impl->functionContext.logOperator(impl->operatorPrinter.name(imm)); } \
		impl->functionContext.validateNonEmptyControlStack(nameString);                            \
		impl->functionContext.name(imm);                                                           \
	}
ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
