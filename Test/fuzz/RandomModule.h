#pragma once

#include <vector>
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/HashSet.h"

using namespace WAVM;
using namespace WAVM::IR;

// A stream that uses a combination of a PRNG and input data to produce pseudo-random values.
struct RandomStream
{
	RandomStream(const U8* inData, Uptr numBytes)
	: next(inData), end(inData + numBytes), denominator(0), numerator(0), seed(0)
	{
		refill();
	}

	// Returns a pseudo-random value between 0 and maxResult, inclusive.
	template<typename Result> Result get(Result maxResult)
	{
		Result result = Result(get64(maxResult));
		WAVM_ASSERT(result <= maxResult);
		return result;
	}

private:
	const U8* next;
	const U8* end;

	U64 denominator;
	U64 numerator;

	U64 seed;

	void refill()
	{
		while(denominator <= UINT32_MAX)
		{
			if(next < end) { numerator += (denominator + 1) * *next++; }
			denominator += 255 * (denominator + 1);
		};
	}

	U32 get32(U32 maxResult)
	{
		if(maxResult == 0) { return 0; }

		WAVM_ASSERT(denominator >= maxResult);
		seed ^= numerator;
		const U32 result = U32(seed % (U64(maxResult) + 1));
		seed /= (U64(maxResult) + 1);
		numerator /= (U64(maxResult) + 1);
		denominator /= (U64(maxResult) + 1);
		seed = 6364136223846793005 * seed + 1442695040888963407;
		refill();
		return result;
	}

	U64 get64(U64 maxResult)
	{
		U64 result = get32(U32(maxResult));
		result += U64(get32(U32(maxResult >> 32))) << 32;
		WAVM_ASSERT(result <= maxResult);
		return result;
	}
};

struct ModuleState
{
	IR::Module& module;
	std::vector<Uptr> declaredFunctionIndices;
	std::vector<ElemSegmentAndTableImm> validElemSegmentAndTableImms;

	ModuleState(IR::Module& inModule) : module(inModule) {}
};

template<typename Imm> struct ImmTypeAsValue
{
};

template<typename Imm> bool isImmAllowed(const ModuleState&, ImmTypeAsValue<Imm>) { return true; }

static void generateImm(RandomStream& random, const ModuleState& state, NoImm& outImm) {}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<MemoryImm>)
{
	return state.module.memories.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, MemoryImm& outImm)
{
	WAVM_ASSERT(state.module.memories.size());
	outImm.memoryIndex = random.get(state.module.memories.size() - 1);
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<MemoryCopyImm>)
{
	return state.module.memories.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, MemoryCopyImm& outImm)
{
	WAVM_ASSERT(state.module.memories.size());
	outImm.sourceMemoryIndex = random.get(state.module.memories.size() - 1);
	outImm.destMemoryIndex = random.get(state.module.memories.size() - 1);
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<TableImm>)
{
	return state.module.tables.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, TableImm& outImm)
{
	WAVM_ASSERT(state.module.tables.size());
	outImm.tableIndex = random.get(state.module.tables.size() - 1);
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<TableCopyImm>)
{
	return state.module.tables.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, TableCopyImm& outImm)
{
	WAVM_ASSERT(state.module.tables.size());
	outImm.sourceTableIndex = random.get(state.module.tables.size() - 1);

	const ReferenceType sourceElemType
		= state.module.tables.getType(outImm.sourceTableIndex).elementType;

	Uptr* validDestTableIndices = (Uptr*)alloca(state.module.tables.size() * sizeof(Uptr));
	Uptr numValidDestTables = 0;
	for(Uptr tableIndex = 0; tableIndex < state.module.tables.size(); ++tableIndex)
	{
		const ReferenceType destElemType = state.module.tables.getType(tableIndex).elementType;
		if(isSubtype(sourceElemType, destElemType))
		{ validDestTableIndices[numValidDestTables++] = tableIndex; }
	}
	WAVM_ASSERT(numValidDestTables);

	outImm.destTableIndex = validDestTableIndices[random.get(numValidDestTables - 1)];
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<FunctionRefImm>)
{
	return state.declaredFunctionIndices.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, FunctionRefImm& outImm)
{
	outImm.functionIndex
		= state.declaredFunctionIndices[random.get(state.declaredFunctionIndices.size() - 1)];
}

static void generateImm(RandomStream& random, const ModuleState& state, LiteralImm<I32>& outImm)
{
	outImm.value = I32(random.get(UINT32_MAX));
}

static void generateImm(RandomStream& random, const ModuleState& state, LiteralImm<I64>& outImm)
{
	outImm.value = I64(random.get(UINT64_MAX));
}

static void generateImm(RandomStream& random, const ModuleState& state, LiteralImm<F32>& outImm)
{
	const U32 u32 = random.get(UINT32_MAX);
	memcpy(&outImm.value, &u32, sizeof(U32));
}

static void generateImm(RandomStream& random, const ModuleState& state, LiteralImm<F64>& outImm)
{
	const U64 u64 = random.get(UINT64_MAX);
	memcpy(&outImm.value, &u64, sizeof(U64));
}

static void generateImm(RandomStream& random, const ModuleState& state, LiteralImm<V128>& outImm)
{
	outImm.value.u64x2[0] = random.get(UINT64_MAX);
	outImm.value.u64x2[1] = random.get(UINT64_MAX);
}

template<Uptr naturalAlignmentLog2>
bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<LoadOrStoreImm<naturalAlignmentLog2>>)
{
	return state.module.memories.size();
}
template<Uptr naturalAlignmentLog2>
static void generateImm(RandomStream& random,
						const ModuleState& state,
						LoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.alignmentLog2 = random.get<U8>(naturalAlignmentLog2);
	outImm.offset = random.get(UINT32_MAX);
	outImm.memoryIndex = random.get(state.module.memories.size() - 1);
}

template<Uptr naturalAlignmentLog2>
bool isImmAllowed(const ModuleState& state,
				  ImmTypeAsValue<AtomicLoadOrStoreImm<naturalAlignmentLog2>>)
{
	return state.module.memories.size() != 0;
}
template<Uptr naturalAlignmentLog2>
static void generateImm(RandomStream& random,
						const ModuleState& state,
						AtomicLoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.alignmentLog2 = naturalAlignmentLog2;
	outImm.offset = random.get(UINT32_MAX);
	outImm.memoryIndex = random.get(state.module.memories.size() - 1);
}

static void generateImm(RandomStream& random, const ModuleState& state, AtomicFenceImm& outImm)
{
	outImm.order = MemoryOrder::sequentiallyConsistent;
}

template<Uptr numLanes>
static void generateImm(RandomStream& random,
						const ModuleState& state,
						LaneIndexImm<numLanes>& outImm)
{
	outImm.laneIndex = random.get<U8>(numLanes - 1);
}

template<Uptr numLanes>
static void generateImm(RandomStream& random,
						const ModuleState& state,
						ShuffleImm<numLanes>& outImm)
{
	for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
	{ outImm.laneIndices[laneIndex] = random.get<U8>(numLanes * 2 - 1); }
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<DataSegmentAndMemImm>)
{
	return state.module.dataSegments.size() && state.module.memories.size();
}
static void generateImm(RandomStream& random,
						const ModuleState& state,
						DataSegmentAndMemImm& outImm)
{
	WAVM_ASSERT(state.module.dataSegments.size());
	WAVM_ASSERT(state.module.memories.size());
	outImm.dataSegmentIndex = random.get(state.module.dataSegments.size() - 1);
	outImm.memoryIndex = random.get(state.module.memories.size() - 1);
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<DataSegmentImm>)
{
	return state.module.dataSegments.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, DataSegmentImm& outImm)
{
	WAVM_ASSERT(state.module.dataSegments.size());
	outImm.dataSegmentIndex = random.get(state.module.dataSegments.size() - 1);
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<ElemSegmentAndTableImm>)
{
	return state.validElemSegmentAndTableImms.size();
}
static void generateImm(RandomStream& random,
						const ModuleState& state,
						ElemSegmentAndTableImm& outImm)
{
	WAVM_ASSERT(state.validElemSegmentAndTableImms.size());
	outImm = state.validElemSegmentAndTableImms[random.get(state.validElemSegmentAndTableImms.size()
														   - 1)];
}

static bool isImmAllowed(const ModuleState& state, ImmTypeAsValue<ElemSegmentImm>)
{
	return state.module.elemSegments.size();
}
static void generateImm(RandomStream& random, const ModuleState& state, ElemSegmentImm& outImm)
{
	WAVM_ASSERT(state.module.elemSegments.size());
	outImm.elemSegmentIndex = random.get(state.module.elemSegments.size() - 1);
}

// Build a table with information about non-parametric operators.

typedef CodeValidationProxyStream<OperatorEncoderStream> CodeStream;
typedef void OperatorEmitFunc(RandomStream&, const ModuleState&, CodeStream&);

struct OperatorInfo
{
	const char* name;
	FunctionType (*sig)();
	bool (*isAllowed)(const ModuleState& state);
	OperatorEmitFunc* emit;
};

#define VISIT_OP(encoding, name, nameString, Imm, SIGNATURE, ...)                                  \
	{nameString,                                                                                   \
	 []() { return IR::getNonParametricOpSigs().name; },                                           \
	 [](const ModuleState& state) { return isImmAllowed(state, ImmTypeAsValue<Imm>{}); },          \
	 [](RandomStream& random, const ModuleState& state, CodeStream& codeStream) {                  \
		 Imm imm;                                                                                  \
		 generateImm(random, state, imm);                                                          \
		 codeStream.name(imm);                                                                     \
	 }},
static OperatorInfo operatorInfos[]{WAVM_ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VISIT_OP)};
#undef VISIT_OP

static constexpr Uptr numNonParametricOps = sizeof(operatorInfos) / sizeof(OperatorInfo);

static ValueType generateValueType(RandomStream& random)
{
	switch(random.get(6))
	{
	case 0: return ValueType::i32;
	case 1: return ValueType::i64;
	case 2: return ValueType::f32;
	case 3: return ValueType::f64;
	case 4: return ValueType::v128;
	case 5: return ValueType::externref;
	case 6: return ValueType::funcref;
	default: WAVM_UNREACHABLE();
	}
}

static FunctionType generateFunctionType(RandomStream& random)
{
	std::vector<ValueType> functionParams;
	const Uptr numParams = random.get(4);
	for(Uptr paramIndex = 0; paramIndex < numParams; ++paramIndex)
	{ functionParams.push_back(generateValueType(random)); };

	std::vector<ValueType> functionResults;
	const Uptr numResults = random.get(2);
	for(Uptr resultIndex = 0; resultIndex < numResults; ++resultIndex)
	{ functionResults.push_back(generateValueType(random)); }

	return FunctionType({functionResults}, {functionParams});
}

static ReferenceType generateRefType(RandomStream& random)
{
	switch(random.get(1))
	{
	case 0: return ReferenceType::externref;
	case 1: return ReferenceType::funcref;
	default: WAVM_UNREACHABLE();
	};
}

static ExternKind generateExternKind(RandomStream& random)
{
	switch(random.get(4))
	{
	case 0: return ExternKind::function;
	case 1: return ExternKind::table;
	case 2: return ExternKind::memory;
	case 3: return ExternKind::global;
	case 4: return ExternKind::exceptionType;
	default: WAVM_UNREACHABLE();
	};
}

FunctionType generateBlockSig(RandomStream& random, TypeTuple params)
{
	const Uptr maxResults = 4;
	ValueType results[maxResults];
	const Uptr numResults = random.get(4);
	for(Uptr resultIndex = 0; resultIndex < numResults; ++resultIndex)
	{ results[resultIndex] = generateValueType(random); }

	return FunctionType(TypeTuple(results, numResults), params);
}

IndexedBlockType getIndexedBlockType(IR::Module& module,
									 HashMap<FunctionType, Uptr>& functionTypeMap,
									 const FunctionType sig)
{
	if(sig.params().size() || sig.results().size() > 1)
	{
		IndexedBlockType result;
		result.format = IndexedBlockType::functionType;
		result.index = functionTypeMap.getOrAdd(sig, module.types.size());
		if(result.index == module.types.size()) { module.types.push_back(sig); }
		return result;
	}
	else
	{
		return sig.results().size() == 1
				   ? IndexedBlockType{IndexedBlockType::Format::oneResult, {sig.results()[0]}}
				   : IndexedBlockType{IndexedBlockType::Format::noParametersOrResult, {}};
	}
}

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

	TypeTuple elseParams;
};

static void emitControlEnd(std::vector<ControlContext>& controlStack,
						   std::vector<ValueType>& stack,
						   IR::Module& module,
						   CodeStream& codeStream)
{
	if(controlStack.back().type == ControlContext::Type::ifThen)
	{
		// Emit the else operator.
		codeStream.else_();

		stack.resize(controlStack.back().outerStackSize);
		for(ValueType elseParam : controlStack.back().elseParams) { stack.push_back(elseParam); }

		// Change the current control context type to an else clause.
		controlStack.back().type = ControlContext::Type::ifElse;
	}
	else
	{
		if(controlStack.back().type == ControlContext::Type::try_
		   || controlStack.back().type == ControlContext::Type::catch_)
		{
			// TODO: catch
			WAVM_UNREACHABLE();
		}

		codeStream.end();
		stack.resize(controlStack.back().outerStackSize);
		for(ValueType result : controlStack.back().results) { stack.push_back(result); }
		controlStack.pop_back();
	}
}

static void generateFunction(RandomStream& random,
							 const ModuleState& moduleState,
							 IR::ModuleValidationState& moduleValidationState,
							 FunctionDef& functionDef,
							 HashMap<FunctionType, Uptr>& functionTypeMap)
{
	const IR::Module& module = moduleState.module;
	const FunctionType functionType = module.types[functionDef.type.index];

	// Generate locals.
	const Uptr numNonParameterLocals = random.get(4);
	for(Uptr localIndex = 0; localIndex < numNonParameterLocals; ++localIndex)
	{ functionDef.nonParameterLocalTypes.push_back(generateValueType(random)); }
	const Uptr numLocals = functionType.params().size() + numNonParameterLocals;

	Serialization::ArrayOutputStream codeByteStream;
	OperatorEncoderStream opEncoder(codeByteStream);
	CodeValidationProxyStream<OperatorEncoderStream> codeStream(
		moduleValidationState, functionDef, opEncoder);

	std::vector<ControlContext> controlStack;
	controlStack.push_back({ControlContext::Type::function,
							0,
							functionType.results(),
							functionType.results(),
							TypeTuple()});

	std::vector<ValueType> stack;

	Uptr numInstructions = 0;

	std::vector<std::function<OperatorEmitFunc>> validOpEmitters;
	while(controlStack.size())
	{
		bool allowStackGrowth
			= stack.size() - controlStack.back().outerStackSize <= 6 && numInstructions++ < 15;

		if(stack.size() <= controlStack.back().outerStackSize + controlStack.back().results.size())
		{
			const ControlContext& controlContext = controlStack.back();

			bool sigMatches = true;
			for(Uptr resultIndex = 0; resultIndex < controlContext.results.size(); ++resultIndex)
			{
				if(controlContext.outerStackSize + resultIndex >= stack.size())
				{
					sigMatches = false;
					allowStackGrowth = true;
				}
				else if(!isSubtype(stack[controlContext.outerStackSize + resultIndex],
								   controlContext.results[resultIndex]))
				{
					sigMatches = false;
					break;
				}
			}

			if(sigMatches)
			{
				if(controlContext.type == ControlContext::Type::ifThen)
				{
					// Enter an if-else clause.
					validOpEmitters.push_back(
						[&stack, &controlStack](RandomStream& random,
												const ModuleState& moduleState,
												CodeStream& codeStream) {
							// Emit the else operator.
							codeStream.else_();

							stack.resize(controlStack.back().outerStackSize);
							for(ValueType elseParam : controlStack.back().elseParams)
							{ stack.push_back(elseParam); }

							// Change the current control context type to an else clause.
							controlStack.back().type = ControlContext::Type::ifElse;
						});
				}

				if(controlContext.type != ControlContext::Type::try_
				   && (controlContext.type != ControlContext::Type::ifThen
					   || controlContext.elseParams == controlStack.back().results))
				{
					// End the current control structure.
					validOpEmitters.push_back(
						[&stack, &controlStack](RandomStream& random,
												const ModuleState& moduleState,
												CodeStream& codeStream) {
							// Emit the end operator.
							codeStream.end();

							// Push the control context's results on the stack.
							stack.resize(controlStack.back().outerStackSize);
							const TypeTuple& results = controlStack.back().results;
							stack.insert(stack.end(), results.begin(), results.end());

							// Pop the control stack.
							controlStack.pop_back();
						});
				}
			}
		}

		// Build a list of the non-parametric operators that are valid given the module and the
		// current state of the stack.
		for(Uptr opIndex = 0; opIndex < numNonParametricOps; ++opIndex)
		{
			const OperatorInfo& opInfo = operatorInfos[opIndex];

			if(!opInfo.isAllowed(moduleState)) { continue; }

			const TypeTuple params = opInfo.sig().params();
			const TypeTuple results = opInfo.sig().results();

			// If the random stream has run out of entropy, only consider operators that result
			// in fewer operands on the stack.
			if(!allowStackGrowth && results.size() >= params.size()) { continue; }

			// Ensure the stack has enough values for the operator's parameters.
			if(params.size() > stack.size() - controlStack.back().outerStackSize) { continue; }

			// Check that the types of values on top of the stack are the right type for the
			// operator's parameters.
			bool sigMatch = true;
			for(Uptr paramIndex = 0; paramIndex < params.size(); ++paramIndex)
			{
				if(!isSubtype(stack[stack.size() - params.size() + paramIndex], params[paramIndex]))
				{
					sigMatch = false;
					break;
				}
			}
			if(sigMatch)
			{
				// Add the operator to the list of valid operators.
				validOpEmitters.push_back([&stack, opInfo](RandomStream& random,
														   const ModuleState& moduleState,
														   CodeStream& codeStream) {
					opInfo.emit(random, moduleState.module, codeStream);

					// Remove the operator's parameters from the top of the stack.
					stack.resize(stack.size() - opInfo.sig().params().size());

					// Push the operator's results onto the stack.
					for(ValueType result : opInfo.sig().results()) { stack.push_back(result); }
				});
			}
		}

		// Build a list of the parametric operators that are valid given the current state of
		// the stack.

		for(Uptr localIndex = 0; localIndex < numLocals; ++localIndex)
		{
			const ValueType localType
				= localIndex < functionType.params().size()
					  ? functionType.params()[localIndex]
					  : functionDef
							.nonParameterLocalTypes[localIndex - functionType.params().size()];

			if(stack.size() > controlStack.back().outerStackSize
			   && isSubtype(stack.back(), localType))
			{
				// local.set
				validOpEmitters.push_back([&stack, localIndex](RandomStream& random,
															   const ModuleState& moduleState,
															   CodeStream& codeStream) {
					codeStream.local_set({localIndex});
					stack.pop_back();
				});

				// local.tee
				if(allowStackGrowth)
				{
					validOpEmitters.push_back([localIndex](RandomStream& random,
														   const ModuleState& moduleState,
														   CodeStream& codeStream) {
						codeStream.local_tee({localIndex});
					});
				}
			}

			// local.get
			if(allowStackGrowth)
			{
				validOpEmitters.push_back(
					[&stack, localIndex, localType](RandomStream& random,
													const ModuleState& moduleState,
													CodeStream& codeStream) {
						codeStream.local_get({localIndex});
						stack.push_back(localType);
					});
			}
		}

		for(Uptr globalIndex = 0; globalIndex < module.globals.size(); ++globalIndex)
		{
			const GlobalType globalType = module.globals.getType(globalIndex);

			if(stack.size() > controlStack.back().outerStackSize
			   && isSubtype(stack.back(), globalType.valueType) && globalType.isMutable)
			{
				// global.set
				validOpEmitters.push_back([&stack, globalIndex](RandomStream& random,
																const ModuleState& moduleState,
																CodeStream& codeStream) {
					codeStream.global_set({globalIndex});
					stack.pop_back();
				});
			}

			if(allowStackGrowth)
			{
				// global.get
				validOpEmitters.push_back(
					[&stack, globalIndex, globalType](RandomStream& random,
													  const ModuleState& moduleState,
													  CodeStream& codeStream) {
						codeStream.global_get({globalIndex});
						stack.push_back(globalType.valueType);
					});
			}
		}

		for(Uptr tableIndex = 0; tableIndex < module.tables.size(); ++tableIndex)
		{
			const TableType tableType = module.tables.getType(tableIndex);

			// TODO: table.grow and table.fill

			if(stack.size() - controlStack.back().outerStackSize >= 2
			   && stack[stack.size() - 2] == ValueType::i32
			   && isSubtype(stack.back(), asValueType(tableType.elementType)))
			{
				// table.set
				validOpEmitters.push_back([&stack, tableIndex](RandomStream& random,
															   const ModuleState& moduleState,
															   CodeStream& codeStream) {
					codeStream.table_set({U32(tableIndex)});
					stack.resize(stack.size() - 2);
				});
			}

			if(stack.size() > controlStack.back().outerStackSize && stack.back() == ValueType::i32)
			{
				// table.get
				validOpEmitters.push_back(
					[&stack, tableIndex, tableType](RandomStream& random,
													const ModuleState& moduleState,
													CodeStream& codeStream) {
						codeStream.table_get({tableIndex});
						stack.pop_back();
						stack.push_back(asValueType(tableType.elementType));
					});

				if(tableType.elementType == ReferenceType::funcref)
				{
					// call_indirect
					for(Uptr typeIndex = 0; typeIndex < module.types.size(); ++typeIndex)
					{
						const FunctionType calleeType = module.types[typeIndex];
						const TypeTuple params = calleeType.params();
						const TypeTuple results = calleeType.results();

						// If the random stream has run out of entropy, only consider operators
						// that result in fewer operands on the stack.
						if(!allowStackGrowth && results.size() >= params.size() + 1) { continue; }

						// Ensure the stack has enough values for the operator's parameters.
						if(params.size() + 1 > stack.size() - controlStack.back().outerStackSize)
						{ continue; }

						// Check that the types of values on top of the stack are the right type
						// for the operator's parameters.
						bool sigMatch = true;
						for(Uptr paramIndex = 0; paramIndex < params.size(); ++paramIndex)
						{
							if(stack[stack.size() - params.size() + paramIndex - 1]
							   != params[paramIndex])
							{
								sigMatch = false;
								break;
							}
						}
						if(sigMatch)
						{
							validOpEmitters.push_back([&stack, calleeType, typeIndex, tableIndex](
														  RandomStream& random,
														  const ModuleState& moduleState,
														  CodeStream& codeStream) {
								codeStream.call_indirect({{typeIndex}, tableIndex});

								// Remove the function's parameters and the table index from
								// the top of the stack.
								stack.resize(stack.size() - calleeType.params().size() - 1);

								// Push the function's results onto the stack.
								for(ValueType result : calleeType.results())
								{ stack.push_back(result); }
							});
						}
					}
				}
			}
		}

		if(allowStackGrowth)
		{
			const Uptr maxArity = stack.size() - controlStack.back().outerStackSize;
			for(Uptr arity = 0; arity < maxArity; ++arity)
			{
				// Enter a block control structure.
				validOpEmitters.push_back([&stack, &controlStack, &functionTypeMap, arity](
											  RandomStream& random,
											  const ModuleState& moduleState,
											  CodeStream& codeStream) {
					const FunctionType blockSig = generateBlockSig(
						random, TypeTuple(stack.data() + stack.size() - arity, arity));
					stack.resize(stack.size() - arity);
					stack.insert(stack.end(), blockSig.params().begin(), blockSig.params().end());
					codeStream.block(
						{getIndexedBlockType(moduleState.module, functionTypeMap, blockSig)});
					controlStack.push_back({ControlContext::Type::block,
											stack.size() - arity,
											blockSig.results(),
											blockSig.results(),
											TypeTuple()});
				});

				// Enter a loop control structure.
				validOpEmitters.push_back(
					[&stack, &controlStack, &functionTypeMap, arity](RandomStream& random,
																	 const ModuleState& moduleState,
																	 CodeStream& codeStream) {
						const FunctionType loopSig = generateBlockSig(
							random, TypeTuple(stack.data() + stack.size() - arity, arity));
						stack.resize(stack.size() - arity);
						stack.insert(stack.end(), loopSig.params().begin(), loopSig.params().end());
						codeStream.loop(
							{getIndexedBlockType(moduleState.module, functionTypeMap, loopSig)});
						controlStack.push_back({ControlContext::Type::loop,
												stack.size() - arity,
												loopSig.params(),
												loopSig.results(),
												TypeTuple()});
					});
			}
		}

		// Enter an if control structure.
		if(allowStackGrowth && stack.size() > controlStack.back().outerStackSize
		   && stack.back() == ValueType::i32)
		{
			const Uptr maxArity = stack.size() - controlStack.back().outerStackSize - 1;
			for(Uptr arity = 0; arity < maxArity; ++arity)
			{
				validOpEmitters.push_back(
					[&stack, &controlStack, &functionTypeMap, arity](RandomStream& random,
																	 const ModuleState& moduleState,
																	 CodeStream& codeStream) {
						const FunctionType ifSig = generateBlockSig(
							random, TypeTuple(stack.data() + stack.size() - arity - 1, arity));
						stack.resize(stack.size() - arity - 1);
						stack.insert(stack.end(), ifSig.params().begin(), ifSig.params().end());
						codeStream.if_(
							{getIndexedBlockType(moduleState.module, functionTypeMap, ifSig)});
						controlStack.push_back({ControlContext::Type::ifThen,
												stack.size() - arity,
												ifSig.results(),
												ifSig.results(),
												ifSig.params()});
					});
			}
		}

		// TODO: try/catch/catch_all/throw/rethrow

		for(Uptr branchTargetDepth = 0; branchTargetDepth < controlStack.size();
			++branchTargetDepth)
		{
			const ControlContext& targetContext
				= controlStack[controlStack.size() - branchTargetDepth - 1];
			const TypeTuple params = targetContext.params;

			if(params.size() > stack.size() - controlStack.back().outerStackSize) { continue; }

			// Check that the types of values on top of the stack are the right type for the
			// operator's parameters.
			bool sigMatch = true;
			for(Uptr paramIndex = 0; paramIndex < params.size(); ++paramIndex)
			{
				if(!isSubtype(stack[stack.size() - params.size() + paramIndex], params[paramIndex]))
				{
					sigMatch = false;
					break;
				}
			}
			if(sigMatch)
			{
				// br
				validOpEmitters.push_back(
					[&controlStack, &stack, branchTargetDepth](RandomStream& random,
															   const ModuleState& moduleState,
															   CodeStream& codeStream) {
						codeStream.br({U32(branchTargetDepth)});
						emitControlEnd(controlStack, stack, moduleState.module, codeStream);
					});

				if(branchTargetDepth == controlStack.size() - 1)
				{
					// return
					validOpEmitters.push_back(
						[&controlStack, &stack](RandomStream& random,
												const ModuleState& moduleState,
												CodeStream& codeStream) {
							codeStream.return_();
							emitControlEnd(controlStack, stack, moduleState.module, codeStream);
						});
				}
			}
		}

		// br_if
		if(stack.size() > controlStack.back().outerStackSize && stack.back() == ValueType::i32)
		{
			for(Uptr branchTargetDepth = 0; branchTargetDepth < controlStack.size();
				++branchTargetDepth)
			{
				const ControlContext& targetContext
					= controlStack[controlStack.size() - branchTargetDepth - 1];
				const TypeTuple params = targetContext.params;

				if(params.size() + 1 > stack.size() - controlStack.back().outerStackSize)
				{ continue; }

				// Check that the types of values on top of the stack are the right type for the
				// operator's parameters.
				bool sigMatch = true;
				for(Uptr paramIndex = 0; paramIndex < params.size(); ++paramIndex)
				{
					if(stack[stack.size() - params.size() - 1 + paramIndex] != params[paramIndex])
					{
						sigMatch = false;
						break;
					}
				}
				if(sigMatch)
				{
					validOpEmitters.push_back(
						[&stack, branchTargetDepth](RandomStream& random,
													const ModuleState& moduleState,
													CodeStream& codeStream) {
							stack.pop_back();
							codeStream.br_if({U32(branchTargetDepth)});
						});
				}
			}
		}

		// unreachable
		validOpEmitters.push_back([&controlStack, &stack](RandomStream& random,
														  const ModuleState& moduleState,
														  CodeStream& codeStream) {
			codeStream.unreachable();
			emitControlEnd(controlStack, stack, moduleState.module, codeStream);
		});

		// TODO: br_table

		if(stack.size() - controlStack.back().outerStackSize >= 3 && stack.back() == ValueType::i32)
		{
			const ValueType trueValueType = stack[stack.size() - 3];
			const ValueType falseValueType = stack[stack.size() - 2];
			if(trueValueType == falseValueType)
			{
				const ValueType joinType = trueValueType;
				if(isReferenceType(joinType))
				{
					validOpEmitters.push_back([&stack, joinType](RandomStream& random,
																 const ModuleState& moduleState,
																 CodeStream& codeStream) {
						stack.resize(stack.size() - 3);
						stack.push_back(joinType);
						codeStream.select({joinType});
					});
				}
				else
				{
					// Non-typed select
					validOpEmitters.push_back([&stack, joinType](RandomStream& random,
																 const ModuleState& moduleState,
																 CodeStream& codeStream) {
						stack.resize(stack.size() - 3);
						stack.push_back(joinType);
						codeStream.select({ValueType::any});
					});
				}
			}
		}

		if(stack.size() > controlStack.back().outerStackSize)
		{
			// drop
			validOpEmitters.push_back([&stack](RandomStream& random,
											   const ModuleState& moduleState,
											   CodeStream& codeStream) {
				codeStream.drop();
				stack.pop_back();
			});
		}

		// call
		for(Uptr functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex)
		{
			const FunctionType calleeType
				= module.types[module.functions.getType(functionIndex).index];
			const TypeTuple params = calleeType.params();
			const TypeTuple results = calleeType.results();

			// If the random stream has run out of entropy, only consider operators that result
			// in fewer operands on the stack.
			if(!allowStackGrowth && results.size() >= params.size()) { continue; }

			// Ensure the stack has enough values for the operator's parameters.
			if(params.size() > stack.size() - controlStack.back().outerStackSize) { continue; }

			// Check that the types of values on top of the stack are the right type for the
			// operator's parameters.
			bool sigMatch = true;
			for(Uptr paramIndex = 0; paramIndex < params.size(); ++paramIndex)
			{
				if(stack[stack.size() - params.size() + paramIndex] != params[paramIndex])
				{
					sigMatch = false;
					break;
				}
			}
			if(sigMatch)
			{
				validOpEmitters.push_back([&stack, functionIndex](RandomStream& random,
																  const ModuleState& moduleState,
																  CodeStream& codeStream) {
					const FunctionType calleeType
						= moduleState.module
							  .types[moduleState.module.functions.getType(functionIndex).index];

					codeStream.call({functionIndex});

					// Remove the function's parameters from the top of the stack.
					stack.resize(stack.size() - calleeType.params().size());

					// Push the function's results onto the stack.
					for(ValueType result : calleeType.results()) { stack.push_back(result); }
				});
			}
		}

		// ref.null
		validOpEmitters.push_back(
			[&stack](RandomStream& random, const ModuleState& moduleState, CodeStream& codeStream) {
				const ReferenceType nullReferenceType = generateRefType(random);
				codeStream.ref_null({nullReferenceType});
				stack.push_back(asValueType(nullReferenceType));
			});

		// ref.is_null
		if(stack.size() > controlStack.back().outerStackSize && isReferenceType(stack.back()))
		{
			validOpEmitters.push_back([&stack](RandomStream& random,
											   const ModuleState& moduleState,
											   CodeStream& codeStream) {
				codeStream.ref_is_null();
				stack.back() = ValueType::i32;
			});
		}

		// Emit a random operator.
		WAVM_ASSERT(validOpEmitters.size());
		const Uptr randomOpIndex = random.get(validOpEmitters.size() - 1);
		validOpEmitters[randomOpIndex](random, moduleState, codeStream);
		validOpEmitters.clear();
	};

	codeStream.finishValidation();

	functionDef.code = codeByteStream.getBytes();
};

static InitializerExpression generateInitializerExpression(IR::Module& module,
														   RandomStream& random,
														   IR::ValueType type)
{
	switch(type)
	{
	case ValueType::i32: return InitializerExpression(I32(random.get(UINT32_MAX)));
	case ValueType::i64: return InitializerExpression(I64(random.get(UINT64_MAX)));
	case ValueType::f32: return InitializerExpression(F32(random.get(UINT32_MAX)));
	case ValueType::f64: return InitializerExpression(F64(random.get(UINT64_MAX)));
	case ValueType::v128: {
		V128 v128;
		v128.u64x2[0] = random.get(UINT64_MAX);
		v128.u64x2[1] = random.get(UINT64_MAX);
		return InitializerExpression(v128);
	}
	case ValueType::externref: {
		return InitializerExpression(ReferenceType::externref);
	}
	case ValueType::funcref: {
		const Uptr functionIndex = random.get(module.functions.size());
		return functionIndex == module.functions.size()
				   ? InitializerExpression(ReferenceType::funcref)
				   : InitializerExpression(InitializerExpression::Type::ref_func, functionIndex);
	}

	case ValueType::none:
	case ValueType::any:
	default: WAVM_UNREACHABLE();
	}
}

void generateValidModule(IR::Module& module, RandomStream& random)
{
	ModuleState moduleState(module);

	WAVM_ASSERT(module.featureSpec.simd);
	WAVM_ASSERT(module.featureSpec.atomics);
	WAVM_ASSERT(module.featureSpec.exceptionHandling);
	WAVM_ASSERT(module.featureSpec.multipleResultsAndBlockParams);
	WAVM_ASSERT(module.featureSpec.bulkMemoryOperations);
	WAVM_ASSERT(module.featureSpec.referenceTypes);
	WAVM_ASSERT(module.featureSpec.sharedTables);

	HashMap<FunctionType, Uptr> functionTypeMap;

	// Generate some memories.
	const Uptr numMemories = random.get(3);
	for(Uptr memoryIndex = 0; memoryIndex < numMemories; ++memoryIndex)
	{
		MemoryType type;
		type.isShared = !!random.get(1);
		type.size.min = random.get<U64>(100);
		type.size.max = type.size.min + random.get<U64>(IR::maxMemoryPages - type.size.min);

		if(random.get(1)) { module.memories.defs.push_back({type}); }
		else
		{
			module.imports.push_back({ExternKind::memory, module.memories.imports.size()});
			module.memories.imports.push_back({type, "env", "memory"});
		}
	}

	// Generate some tables.
	const Uptr numTables = random.get(3);
	for(Uptr tableIndex = 0; tableIndex < numTables; ++tableIndex)
	{
		TableType type;
		type.elementType = generateRefType(random);
		type.isShared = !!random.get(1);
		type.size.min = random.get<U64>(100);
		type.size.max = IR::maxTableElems;

		if(random.get(1)) { module.tables.defs.push_back({type}); }
		else
		{
			module.imports.push_back({ExternKind::table, module.tables.imports.size()});
			module.tables.imports.push_back({type, "env", "table"});
		}
	}

	// Generate some globals.
	const Uptr numGlobals = random.get(10);
	for(Uptr globalIndex = 0; globalIndex < numGlobals; ++globalIndex)
	{
		const ValueType globalValueType = generateValueType(random);

		const bool isMutable = random.get(1);
		const GlobalType globalType{globalValueType, isMutable};
		if(random.get(1))
		{
			module.imports.push_back({ExternKind::global, module.globals.imports.size()});
			module.globals.imports.push_back(
				{globalType, "env", "global" + std::to_string(globalIndex)});
		}
		else
		{
			InitializerExpression initializer
				= generateInitializerExpression(module, random, globalValueType);
			module.globals.defs.push_back({globalType, initializer});
		}
	};

	// Generate some data segments.
	Uptr numDataSegments = random.get(2);
	for(Uptr segmentIndex = 0; segmentIndex < numDataSegments; ++segmentIndex)
	{
		const Uptr numSegmentBytes = random.get(100);
		std::vector<U8> bytes;
		for(Uptr byteIndex = 0; byteIndex < numSegmentBytes; ++byteIndex)
		{ bytes.push_back(random.get<U8>(255)); }
		if(!module.memories.size() || random.get(1))
		{
			module.dataSegments.push_back(
				{false, UINTPTR_MAX, {}, std::make_shared<std::vector<U8>>(std::move(bytes))});
		}
		else
		{
			module.dataSegments.push_back(
				{true,
				 random.get(module.memories.size() - 1),
				 generateInitializerExpression(module, random, ValueType::i32),
				 std::make_shared<std::vector<U8>>(std::move(bytes))});
		}
	};

	// Create some function imports/defs
	const Uptr numFunctions = 1 + random.get(4);
	while(module.functions.size() < numFunctions)
	{
		// Generate a signature.
		FunctionType functionType = generateFunctionType(random);
		const Uptr functionTypeIndex = functionTypeMap.getOrAdd(functionType, module.types.size());
		if(functionTypeIndex == module.types.size()) { module.types.push_back(functionType); }

		if(random.get(1))
		{
			// Generate a function import.
			module.imports.push_back({ExternKind::function, module.functions.imports.size()});
			module.functions.imports.push_back(
				{{functionTypeIndex},
				 "env",
				 "func" + std::to_string(module.functions.imports.size())});
		}
		else
		{
			// Generate a FunctionDef, but don't generate its code until we have generated
			// all declarations.
			FunctionDef functionDef;
			functionDef.type.index = functionTypeIndex;
			module.functions.defs.push_back(std::move(functionDef));
		}
	};

	// Generate some elem segments.
	HashSet<Uptr> declaredFunctionIndexSet;
	Uptr numElemSegments = random.get(2);
	for(Uptr segmentIndex = 0; segmentIndex < numElemSegments; ++segmentIndex)
	{
		auto contents = std::make_shared<ElemSegment::Contents>();
		contents->encoding
			= random.get(1) ? ElemSegment::Encoding::expr : ElemSegment::Encoding::index;

		ReferenceType segmentElemType;
		const Uptr numSegmentElements = random.get(100);
		switch(contents->encoding)
		{
		case ElemSegment::Encoding::expr: {
			segmentElemType = contents->elemType = generateRefType(random);
			for(Uptr index = 0; index < numSegmentElements; ++index)
			{
				switch(contents->elemType)
				{
				case ReferenceType::externref: {
					contents->elemExprs.push_back(ElemExpr(ReferenceType::externref));
					break;
				}
				case ReferenceType::funcref: {
					const Uptr functionIndex = random.get(module.functions.size());
					if(functionIndex == module.functions.size())
					{ contents->elemExprs.push_back(ElemExpr(ReferenceType::funcref)); }
					else
					{
						contents->elemExprs.push_back(
							ElemExpr(ElemExpr::Type::ref_func, functionIndex));
						if(declaredFunctionIndexSet.add(functionIndex))
						{ moduleState.declaredFunctionIndices.push_back(functionIndex); }
					}
					break;
				}

				case ReferenceType::none:
				default: WAVM_UNREACHABLE();
				};
			}
			break;
		}
		case ElemSegment::Encoding::index: {
			contents->externKind = generateExternKind(random);
			segmentElemType = asReferenceType(contents->externKind);
			for(Uptr index = 0; index < numSegmentElements; ++index)
			{
				switch(contents->externKind)
				{
				case ExternKind::function:
					if(module.functions.size())
					{
						const Uptr functionIndex = random.get(module.functions.size() - 1);
						contents->elemIndices.push_back(functionIndex);
						if(declaredFunctionIndexSet.add(functionIndex))
						{ moduleState.declaredFunctionIndices.push_back(functionIndex); }
					}
					break;
				case ExternKind::table:
					if(module.tables.size())
					{ contents->elemIndices.push_back(random.get(module.tables.size() - 1)); }
					break;
				case ExternKind::memory:
					if(module.memories.size())
					{ contents->elemIndices.push_back(random.get(module.memories.size() - 1)); }
					break;
				case ExternKind::global:
					if(module.globals.size())
					{ contents->elemIndices.push_back(random.get(module.globals.size() - 1)); }
					break;
				case ExternKind::exceptionType:
					if(module.exceptionTypes.size())
					{
						contents->elemIndices.push_back(
							random.get(module.exceptionTypes.size() - 1));
					}
					break;

				case ExternKind::invalid:
				default: WAVM_UNREACHABLE();
				};
			}
			break;
		}
		default: WAVM_UNREACHABLE();
		};

		std::vector<Uptr> validTableIndices;
		for(Uptr tableIndex = 0; tableIndex < module.tables.size(); ++tableIndex)
		{
			const ReferenceType tableElemType = module.tables.getType(tableIndex).elementType;
			if(isSubtype(segmentElemType, tableElemType))
			{ validTableIndices.push_back(tableIndex); }
		}

		ElemSegment::Type elemSegmentType = ElemSegment::Type::passive;
		if(!validTableIndices.size())
		{
			elemSegmentType
				= random.get(1) ? ElemSegment::Type::passive : ElemSegment::Type::declared;
		}
		else
		{
			switch(random.get(2))
			{
			case 0: elemSegmentType = ElemSegment::Type::passive; break;
			case 1: elemSegmentType = ElemSegment::Type::active; break;
			case 2: elemSegmentType = ElemSegment::Type::declared; break;
			default: WAVM_UNREACHABLE();
			};
		}

		switch(elemSegmentType)
		{
		case ElemSegment::Type::passive: {
			module.elemSegments.push_back({ElemSegment::Type::passive,
										   UINTPTR_MAX,
										   InitializerExpression(),
										   std::move(contents)});
			break;
		}
		case ElemSegment::Type::active: {
			const Uptr validTableIndex = random.get(validTableIndices.size() - 1);
			module.elemSegments.push_back(
				{ElemSegment::Type::active,
				 validTableIndices[validTableIndex],
				 generateInitializerExpression(module, random, ValueType::i32),
				 std::move(contents)});
			break;
		}
		case ElemSegment::Type::declared: {
			module.elemSegments.push_back({ElemSegment::Type::declared,
										   UINTPTR_MAX,
										   InitializerExpression(),
										   std::move(contents)});
			break;
		}
		default: WAVM_UNREACHABLE();
		};

		// Precalculate a list of element-table pairs that are valid for a table.init
		for(Uptr tableIndex = 0; tableIndex < module.tables.size(); ++tableIndex)
		{
			if(isSubtype(segmentElemType, module.tables.getType(tableIndex).elementType))
			{
				moduleState.validElemSegmentAndTableImms.push_back(
					ElemSegmentAndTableImm{segmentIndex, tableIndex});
			}
		}
	};

	std::shared_ptr<IR::ModuleValidationState> moduleValidationState
		= IR::createModuleValidationState(module);

	validatePreCodeSections(*moduleValidationState);

	// Generate a few functions.
	for(FunctionDef& functionDef : module.functions.defs)
	{ generateFunction(random, module, *moduleValidationState, functionDef, functionTypeMap); };

	// Generating functions might have added some block types, so revalidate the type section.
	validateTypes(*moduleValidationState);

	validatePostCodeSections(*moduleValidationState);
}
