#include "WAVM/IR/RandomModule.h"
#include <vector>
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Module.h"
#include "WAVM/IR/OperatorSignatures.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/IR/Types.h"
#include "WAVM/IR/Validate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/RandomStream.h"

using namespace WAVM;
using namespace WAVM::IR;

constexpr Uptr softMaxStackDepthInControlContext = 6;
constexpr Uptr softMaxInstructionsInFunction = 15;

struct ModuleState
{
	Module& module_;
	std::vector<Uptr> declaredFunctionIndices;
	std::vector<ElemSegmentAndTableImm> validElemSegmentAndTableImms;
	HashMap<FunctionType, Uptr> functionTypeMap;

	RandomStream& random;

	ModuleState(Module& inModule, RandomStream& inRandom) : module_(inModule), random(inRandom) {}
};

using CodeStream = CodeValidationProxyStream<OperatorEncoderStream>;

struct FunctionState
{
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

	ModuleState& moduleState;
	ModuleValidationState& moduleValidationState;
	const Module& module_;
	const FunctionType functionType;
	FunctionDef& functionDef;

	const Uptr numLocals;
	Uptr numInstructions = 0;
	bool allowStackGrowth = false;

	std::vector<ControlContext> controlStack;
	std::vector<ValueType> stack;

	Serialization::ArrayOutputStream codeByteStream;
	OperatorEncoderStream opEncoder;
	CodeStream codeStream;

	FunctionState(ModuleState& inModuleState,
				  ModuleValidationState& inModuleValidationState,
				  FunctionDef& inFunctionDef)
	: moduleState(inModuleState)
	, moduleValidationState(inModuleValidationState)
	, module_(inModuleState.module_)
	, functionType(inModuleState.module_.types[inFunctionDef.type.index])
	, functionDef(inFunctionDef)
	, numLocals(functionType.params().size() + functionDef.nonParameterLocalTypes.size())
	, opEncoder(codeByteStream)
	, codeStream(inModuleValidationState, inFunctionDef, opEncoder)
	{
	}

	template<typename TypeTupleOrOpTypeTuple>
	bool doesStackMatchParams(const TypeTupleOrOpTypeTuple& params,
							  Uptr offsetFromTopOfStack = 0) const
	{
		// Ensure the stack has enough values for the operator's parameters.
		if(params.size() + offsetFromTopOfStack > stack.size() - controlStack.back().outerStackSize)
		{
			return false;
		}

		// Check that the types of values on top of the stack are the right type for the
		// operator's parameters.
		for(Uptr paramIndex = 0; paramIndex < params.size(); ++paramIndex)
		{
			if(!isSubtype(stack[stack.size() - offsetFromTopOfStack - params.size() + paramIndex],
						  params[paramIndex]))
			{
				return false;
			}
		}
		return true;
	};

	bool isOpSignatureAllowed(const OpSignature& sig) const
	{
		// If the random stream has run out of entropy, only consider operators that result
		// in fewer operands on the stack.
		if(!allowStackGrowth && sig.results.size() >= sig.params.size()) { return false; }

		return doesStackMatchParams(sig.params);
	}

	void applyOpSignature(const OpSignature& sig)
	{
		// Remove the operator's parameters from the top of the stack.
		stack.resize(stack.size() - sig.params.size());

		// Push the operator's results onto the stack.
		for(ValueType result : sig.results) { stack.push_back(result); }
	}

	void emitControlEnd()
	{
		ControlContext& controlContext = controlStack.back();
		if(controlContext.type == ControlContext::Type::ifThen)
		{
			// Emit the else operator.
			codeStream.else_();

			stack.resize(controlContext.outerStackSize);
			for(ValueType elseParam : controlContext.elseParams) { stack.push_back(elseParam); }

			// Change the current control context type to an else clause.
			controlContext.type = ControlContext::Type::ifElse;
		}
		else
		{
			if(controlContext.type == ControlContext::Type::try_
			   || controlContext.type == ControlContext::Type::catch_)
			{
				// TODO: catch
				WAVM_UNREACHABLE();
			}

			codeStream.end();
			stack.resize(controlContext.outerStackSize);
			for(ValueType result : controlContext.results) { stack.push_back(result); }
			controlStack.pop_back();
		}
	}

	void generateFunction(RandomStream& random);
};

template<typename Imm> struct ImmTypeAsValue
{
};

using OperatorEmitFunc = std::function<void(RandomStream&)>;

template<typename Imm> bool isImmValid(const FunctionState&, ImmTypeAsValue<Imm>) { return true; }

static void generateImm(const FunctionState& state, RandomStream& random, NoImm& outImm) {}

static bool isImmValid(const FunctionState& state, ImmTypeAsValue<FunctionRefImm>)
{
	return state.moduleState.declaredFunctionIndices.size();
}
static void generateImm(const FunctionState& state, RandomStream& random, FunctionRefImm& outImm)
{
	outImm.functionIndex = state.moduleState.declaredFunctionIndices[random.get(
		state.moduleState.declaredFunctionIndices.size() - 1)];
}

static void generateImm(const FunctionState& state, RandomStream& random, LiteralImm<I32>& outImm)
{
	outImm.value = I32(random.get(UINT32_MAX));
}

static void generateImm(const FunctionState& state, RandomStream& random, LiteralImm<I64>& outImm)
{
	outImm.value = I64(random.get(UINT64_MAX));
}

static void generateImm(const FunctionState& state, RandomStream& random, LiteralImm<F32>& outImm)
{
	const U32 u32 = random.get(UINT32_MAX);
	memcpy(&outImm.value, &u32, sizeof(U32));
}

static void generateImm(const FunctionState& state, RandomStream& random, LiteralImm<F64>& outImm)
{
	const U64 u64 = random.get(UINT64_MAX);
	memcpy(&outImm.value, &u64, sizeof(U64));
}

static void generateImm(const FunctionState& state, RandomStream& random, LiteralImm<V128>& outImm)
{
	outImm.value.u64x2[0] = random.get(UINT64_MAX);
	outImm.value.u64x2[1] = random.get(UINT64_MAX);
}

static void generateImm(const FunctionState& state, RandomStream& random, AtomicFenceImm& outImm)
{
	outImm.order = MemoryOrder::sequentiallyConsistent;
}

template<Uptr numLanes>
static void generateImm(const FunctionState& state,
						RandomStream& random,
						LaneIndexImm<numLanes>& outImm)
{
	outImm.laneIndex = random.get<U8>(numLanes - 1);
}

template<Uptr numLanes>
static void generateImm(const FunctionState& state,
						RandomStream& random,
						ShuffleImm<numLanes>& outImm)
{
	for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
	{
		outImm.laneIndices[laneIndex] = random.get<U8>(numLanes * 2 - 1);
	}
}

static bool isImmValid(const FunctionState& state, ImmTypeAsValue<DataSegmentImm>)
{
	return state.module_.dataSegments.size();
}
static void generateImm(const FunctionState& state, RandomStream& random, DataSegmentImm& outImm)
{
	WAVM_ASSERT(state.module_.dataSegments.size());
	outImm.dataSegmentIndex = random.get(state.module_.dataSegments.size() - 1);
}

static bool isImmValid(const FunctionState& state, ImmTypeAsValue<ElemSegmentImm>)
{
	return state.module_.elemSegments.size();
}
static void generateImm(const FunctionState& state, RandomStream& random, ElemSegmentImm& outImm)
{
	WAVM_ASSERT(state.module_.elemSegments.size());
	outImm.elemSegmentIndex = random.get(state.module_.elemSegments.size() - 1);
}

template<typename Imm>
void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(Imm),
					  const OpSignature& sig)
{
	if(isImmValid(state, ImmTypeAsValue<Imm>()) && state.isOpSignatureAllowed(sig))
	{
		outValidOpEmitters.push_back([&state, emitOp, &sig](RandomStream& random) {
			Imm imm;
			generateImm(state, random, imm);
			(state.codeStream.*emitOp)(imm);
			state.applyOpSignature(sig);
		});
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(NoImm),
					  OpSignature (*sigFromImm)(const Module&, const NoImm&))
{
	NoImm imm;
	const OpSignature sig = (*sigFromImm)(state.module_, imm);
	if(state.isOpSignatureAllowed(sig))
	{
		outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
			(state.codeStream.*emitOp)(imm);
			state.applyOpSignature(sig);
		});
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(MemoryImm),
					  OpSignature (*sigFromImm)(const Module&, const MemoryImm&))
{
	for(Uptr memoryIndex = 0; memoryIndex < state.module_.memories.size(); ++memoryIndex)
	{
		MemoryImm imm;
		imm.memoryIndex = memoryIndex;
		const OpSignature sig = (*sigFromImm)(state.module_, imm);
		if(state.isOpSignatureAllowed(sig))
		{
			outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
				(state.codeStream.*emitOp)(imm);
				state.applyOpSignature(sig);
			});
		}
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(MemoryCopyImm),
					  OpSignature (*sigFromImm)(const Module&, const MemoryCopyImm&))
{
	for(Uptr destMemoryIndex = 0; destMemoryIndex < state.module_.memories.size();
		++destMemoryIndex)
	{
		for(Uptr sourceMemoryIndex = 0; sourceMemoryIndex < state.module_.memories.size();
			++sourceMemoryIndex)
		{
			MemoryCopyImm imm;
			imm.destMemoryIndex = destMemoryIndex;
			imm.sourceMemoryIndex = sourceMemoryIndex;
			const OpSignature sig = (*sigFromImm)(state.module_, imm);
			if(state.isOpSignatureAllowed(sig))
			{
				outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
					(state.codeStream.*emitOp)(imm);
					state.applyOpSignature(sig);
				});
			}
		}
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(TableImm),
					  OpSignature (*sigFromImm)(const Module&, const TableImm&))
{
	for(Uptr tableIndex = 0; tableIndex < state.module_.tables.size(); ++tableIndex)
	{
		TableImm imm;
		imm.tableIndex = tableIndex;
		const OpSignature sig = (*sigFromImm)(state.module_, imm);
		if(state.isOpSignatureAllowed(sig))
		{
			outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
				(state.codeStream.*emitOp)(imm);
				state.applyOpSignature(sig);
			});
		}
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(TableCopyImm),
					  OpSignature (*sigFromImm)(const Module&, const TableCopyImm&))
{
	for(Uptr destTableIndex = 0; destTableIndex < state.module_.tables.size(); ++destTableIndex)
	{
		for(Uptr sourceTableIndex = 0; sourceTableIndex < state.module_.tables.size();
			++sourceTableIndex)
		{
			const TableType& destTableType = state.module_.tables.getType(destTableIndex);
			const TableType& sourceTableType = state.module_.tables.getType(sourceTableIndex);
			if(isSubtype(sourceTableType.elementType, destTableType.elementType))
			{
				TableCopyImm imm;
				imm.destTableIndex = destTableIndex;
				imm.sourceTableIndex = sourceTableIndex;
				const OpSignature sig = (*sigFromImm)(state.module_, imm);
				if(state.isOpSignatureAllowed(sig))
				{
					outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
						(state.codeStream.*emitOp)(imm);
						state.applyOpSignature(sig);
					});
				}
			}
		}
	}
}

template<Uptr naturalAlignmentLog2>
void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(LoadOrStoreImm<naturalAlignmentLog2>),
					  OpSignature (*sigFromImm)(const Module&, const BaseLoadOrStoreImm&))
{
	for(Uptr memoryIndex = 0; memoryIndex < state.module_.memories.size(); ++memoryIndex)
	{
		LoadOrStoreImm<naturalAlignmentLog2> sigImm;
		sigImm.memoryIndex = memoryIndex;
		sigImm.alignmentLog2 = 0;
		sigImm.offset = 0;
		const OpSignature sig = (*sigFromImm)(state.module_, sigImm);
		if(state.isOpSignatureAllowed(sig))
		{
			outValidOpEmitters.push_back([&state, emitOp, sig, memoryIndex](RandomStream& random) {
				LoadOrStoreImm<naturalAlignmentLog2> imm;
				imm.memoryIndex = memoryIndex;
				imm.alignmentLog2 = random.get<U8>(naturalAlignmentLog2);
				imm.offset = random.get(state.module_.memories.getType(imm.memoryIndex).indexType
												== IndexType::i32
											? UINT32_MAX
											: UINT64_MAX);
				(state.codeStream.*emitOp)(imm);
				state.applyOpSignature(sig);
			});
		}
	}
}

template<Uptr naturalAlignmentLog2, Uptr numLanes>
void getValidEmitters(
	FunctionState& state,
	std::vector<OperatorEmitFunc>& outValidOpEmitters,
	void (CodeStream::*emitOp)(LoadOrStoreLaneImm<naturalAlignmentLog2, numLanes>),
	OpSignature (*sigFromImm)(const Module&,
							  const LoadOrStoreLaneImm<naturalAlignmentLog2, numLanes>&))
{
	for(Uptr memoryIndex = 0; memoryIndex < state.module_.memories.size(); ++memoryIndex)
	{
		LoadOrStoreLaneImm<naturalAlignmentLog2, numLanes> sigImm;
		sigImm.memoryIndex = memoryIndex;
		sigImm.alignmentLog2 = 0;
		sigImm.offset = 0;
		sigImm.laneIndex = 0;
		const OpSignature sig = (*sigFromImm)(state.module_, sigImm);
		if(state.isOpSignatureAllowed(sig))
		{
			outValidOpEmitters.push_back([&state, emitOp, sig, memoryIndex](RandomStream& random) {
				LoadOrStoreLaneImm<naturalAlignmentLog2, numLanes> imm;
				imm.memoryIndex = memoryIndex;
				imm.alignmentLog2 = random.get<U8>(naturalAlignmentLog2);
				imm.offset = random.get(state.module_.memories.getType(imm.memoryIndex).indexType
												== IndexType::i32
											? UINT32_MAX
											: UINT64_MAX);
				imm.laneIndex = random.get<U8>(numLanes - 1);
				(state.codeStream.*emitOp)(imm);
				state.applyOpSignature(sig);
			});
		}
	}
}

template<Uptr naturalAlignmentLog2>
void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(AtomicLoadOrStoreImm<naturalAlignmentLog2>),
					  OpSignature (*sigFromImm)(const Module&, const BaseLoadOrStoreImm&))
{
	for(Uptr memoryIndex = 0; memoryIndex < state.module_.memories.size(); ++memoryIndex)
	{
		AtomicLoadOrStoreImm<naturalAlignmentLog2> sigImm;
		sigImm.memoryIndex = memoryIndex;
		sigImm.alignmentLog2 = 0;
		sigImm.offset = 0;
		const OpSignature sig = (*sigFromImm)(state.module_, sigImm);
		if(state.isOpSignatureAllowed(sig))
		{
			outValidOpEmitters.push_back([&state, emitOp, sig, memoryIndex](RandomStream& random) {
				AtomicLoadOrStoreImm<naturalAlignmentLog2> imm;
				imm.memoryIndex = memoryIndex;
				imm.alignmentLog2 = naturalAlignmentLog2;
				imm.offset = random.get(state.module_.memories.getType(imm.memoryIndex).indexType
												== IndexType::i32
											? UINT32_MAX
											: UINT64_MAX);
				(state.codeStream.*emitOp)(imm);
				state.applyOpSignature(sig);
			});
		}
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(DataSegmentAndMemImm),
					  OpSignature (*sigFromImm)(const Module&, const DataSegmentAndMemImm&))
{
	for(Uptr segmentIndex = 0; segmentIndex < state.module_.dataSegments.size(); ++segmentIndex)
	{
		for(Uptr memoryIndex = 0; memoryIndex < state.module_.memories.size(); ++memoryIndex)
		{
			DataSegmentAndMemImm imm;
			imm.dataSegmentIndex = segmentIndex;
			imm.memoryIndex = memoryIndex;
			const OpSignature sig = (*sigFromImm)(state.module_, imm);
			if(state.isOpSignatureAllowed(sig))
			{
				outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
					(state.codeStream.*emitOp)(imm);
					state.applyOpSignature(sig);
				});
			}
		}
	}
}

void getValidEmitters(FunctionState& state,
					  std::vector<OperatorEmitFunc>& outValidOpEmitters,
					  void (CodeStream::*emitOp)(ElemSegmentAndTableImm),
					  OpSignature (*sigFromImm)(const Module&, const ElemSegmentAndTableImm&))
{
	for(const ElemSegmentAndTableImm& imm : state.moduleState.validElemSegmentAndTableImms)
	{
		const OpSignature sig = (*sigFromImm)(state.module_, imm);
		if(state.isOpSignatureAllowed(sig))
		{
			outValidOpEmitters.push_back([&state, emitOp, sig, imm](RandomStream& random) {
				(state.codeStream.*emitOp)(imm);
				state.applyOpSignature(sig);
			});
		}
	}
}

// Build a table with information about non-parametric operators.
struct OperatorInfo
{
	const char* name;
	void (*getValidEmitters)(FunctionState& state,
							 std::vector<OperatorEmitFunc>& outValidOpEmitters);
};

static const OperatorInfo operatorInfos[]{
#define VISIT_OP(encoding, name, nameString, Imm, Signature, ...)                                  \
	{nameString, [](FunctionState& state, std::vector<OperatorEmitFunc>& outValidOpEmitters) {     \
		 getValidEmitters(state, outValidOpEmitters, &CodeStream::name, OpSignatures::Signature);  \
	 }},
	WAVM_ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VISIT_OP)
		WAVM_ENUM_INDEX_POLYMORPHIC_OPERATORS(VISIT_OP)
#undef VISIT_OP
};
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
	{
		functionParams.push_back(generateValueType(random));
	};

	std::vector<ValueType> functionResults;
	const Uptr numResults = random.get(2);
	for(Uptr resultIndex = 0; resultIndex < numResults; ++resultIndex)
	{
		functionResults.push_back(generateValueType(random));
	}

	return FunctionType({functionResults}, {functionParams});
}

static IndexType generateIndexType(RandomStream& random)
{
	switch(random.get(1))
	{
	case 0: return IndexType::i32;
	case 1: return IndexType::i64;
	default: WAVM_UNREACHABLE();
	};
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
	{
		results[resultIndex] = generateValueType(random);
	}

	return FunctionType(TypeTuple(results, numResults), params);
}

IndexedBlockType getIndexedBlockType(ModuleState& moduleState, const FunctionType sig)
{
	if(sig.params().size() || sig.results().size() > 1)
	{
		IndexedBlockType result;
		result.format = IndexedBlockType::functionType;
		result.index = moduleState.functionTypeMap.getOrAdd(sig, moduleState.module_.types.size());
		if(result.index == moduleState.module_.types.size())
		{
			moduleState.module_.types.push_back(sig);
		}
		return result;
	}
	else
	{
		return sig.results().size() == 1
				   ? IndexedBlockType{IndexedBlockType::Format::oneResult, {sig.results()[0]}}
				   : IndexedBlockType{IndexedBlockType::Format::noParametersOrResult, {}};
	}
}

void FunctionState::generateFunction(RandomStream& random)
{
	controlStack.push_back({ControlContext::Type::function,
							0,
							functionType.results(),
							functionType.results(),
							TypeTuple()});

	std::vector<OperatorEmitFunc> validOpEmitters;
	while(controlStack.size())
	{
		const ControlContext& controlContext = controlStack.back();
		allowStackGrowth
			= stack.size() - controlContext.outerStackSize <= softMaxStackDepthInControlContext
			  && numInstructions < softMaxInstructionsInFunction;

		validOpEmitters.clear();

		if(stack.size() < controlContext.outerStackSize + controlContext.results.size())
		{
			// If there aren't enough params on the stack to end the current control context, allow
			// instructions that grow the stack even if we've exceeded the
			allowStackGrowth = true;
		}
		else if(stack.size() == controlContext.outerStackSize + controlContext.results.size())
		{
			// Check whether the current state has valid results to end the current control context.
			if(doesStackMatchParams(controlContext.results))
			{
				if(controlContext.type == ControlContext::Type::ifThen)
				{
					// Enter an if-else clause.
					validOpEmitters.push_back([this](RandomStream& random) {
						// Emit the else operator.
						codeStream.else_();

						stack.resize(controlStack.back().outerStackSize);
						for(ValueType elseParam : controlStack.back().elseParams)
						{
							stack.push_back(elseParam);
						}

						// Change the current control context type to an else clause.
						controlStack.back().type = ControlContext::Type::ifElse;
					});
				}

				if(controlContext.type != ControlContext::Type::try_
				   && (controlContext.type != ControlContext::Type::ifThen
					   || controlContext.elseParams == controlContext.results))
				{
					// End the current control structure.
					validOpEmitters.push_back([this](RandomStream& random) {
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

		// Build a list of the non-parametric operators that are valid given the module_ and the
		// current state of the stack.
		for(Uptr opIndex = 0; opIndex < numNonParametricOps; ++opIndex)
		{
			const OperatorInfo& opInfo = operatorInfos[opIndex];

			opInfo.getValidEmitters(*this, validOpEmitters);
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

			if(stack.size() > controlContext.outerStackSize && isSubtype(stack.back(), localType))
			{
				// local.set
				validOpEmitters.push_back([this, localIndex](RandomStream& random) {
					codeStream.local_set({localIndex});
					stack.pop_back();
				});

				// local.tee
				if(allowStackGrowth)
				{
					validOpEmitters.push_back([this, localIndex](RandomStream& random) {
						codeStream.local_tee({localIndex});
					});
				}
			}

			// local.get
			if(allowStackGrowth)
			{
				validOpEmitters.push_back([this, localIndex, localType](RandomStream& random) {
					codeStream.local_get({localIndex});
					stack.push_back(localType);
				});
			}
		}

		for(Uptr globalIndex = 0; globalIndex < module_.globals.size(); ++globalIndex)
		{
			const GlobalType globalType = module_.globals.getType(globalIndex);

			if(stack.size() > controlStack.back().outerStackSize
			   && isSubtype(stack.back(), globalType.valueType) && globalType.isMutable)
			{
				// global.set
				validOpEmitters.push_back([this, globalIndex](RandomStream& random) {
					codeStream.global_set({globalIndex});
					stack.pop_back();
				});
			}

			if(allowStackGrowth)
			{
				// global.get
				validOpEmitters.push_back([this, globalIndex, globalType](RandomStream& random) {
					codeStream.global_get({globalIndex});
					stack.push_back(globalType.valueType);
				});
			}
		}

		for(Uptr tableIndex = 0; tableIndex < module_.tables.size(); ++tableIndex)
		{
			const TableType& tableType = module_.tables.getType(tableIndex);

			// TODO: table.grow and table.fill

			if(stack.size() - controlStack.back().outerStackSize >= 2
			   && stack[stack.size() - 2] == asValueType(tableType.indexType)
			   && isSubtype(stack.back(), asValueType(tableType.elementType)))
			{
				// table.set
				validOpEmitters.push_back([this, tableIndex](RandomStream& random) {
					codeStream.table_set({tableIndex});
					stack.resize(stack.size() - 2);
				});
			}

			if(stack.size() > controlStack.back().outerStackSize
			   && stack.back() == asValueType(tableType.indexType))
			{
				// table.get
				validOpEmitters.push_back([this, tableIndex, tableType](RandomStream& random) {
					codeStream.table_get({tableIndex});
					stack.pop_back();
					stack.push_back(asValueType(tableType.elementType));
				});

				if(tableType.elementType == ReferenceType::funcref)
				{
					// call_indirect
					for(Uptr typeIndex = 0; typeIndex < module_.types.size(); ++typeIndex)
					{
						const FunctionType calleeType = module_.types[typeIndex];
						const TypeTuple params = calleeType.params();
						const TypeTuple results = calleeType.results();

						// If the random stream has run out of entropy, only consider operators
						// that result in fewer operands on the stack.
						if(!allowStackGrowth && results.size() >= params.size() + 1) { continue; }

						// Ensure the stack has enough values for the operator's parameters.
						if(params.size() + 1 > stack.size() - controlStack.back().outerStackSize)
						{
							continue;
						}

						// Check whether the top of the stack is compatible with function's
						// parameters.
						if(doesStackMatchParams(params, /*offsetFromTopOfStack*/ 1))
						{
							validOpEmitters.push_back(
								[this, calleeType, typeIndex, tableIndex](RandomStream& random) {
									codeStream.call_indirect({{typeIndex}, tableIndex});

									// Remove the function's parameters and the table index from
									// the top of the stack.
									stack.resize(stack.size() - calleeType.params().size() - 1);

									// Push the function's results onto the stack.
									for(ValueType result : calleeType.results())
									{
										stack.push_back(result);
									}
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
				validOpEmitters.push_back([this, arity](RandomStream& random) {
					const FunctionType blockSig = generateBlockSig(
						random, TypeTuple(stack.data() + stack.size() - arity, arity));
					stack.resize(stack.size() - arity);
					stack.insert(stack.end(), blockSig.params().begin(), blockSig.params().end());
					codeStream.block({getIndexedBlockType(moduleState, blockSig)});
					controlStack.push_back({ControlContext::Type::block,
											stack.size() - arity,
											blockSig.results(),
											blockSig.results(),
											TypeTuple()});
				});

				// Enter a loop control structure.
				validOpEmitters.push_back([this, arity](RandomStream& random) {
					const FunctionType loopSig = generateBlockSig(
						random, TypeTuple(stack.data() + stack.size() - arity, arity));
					stack.resize(stack.size() - arity);
					stack.insert(stack.end(), loopSig.params().begin(), loopSig.params().end());
					codeStream.loop({getIndexedBlockType(moduleState, loopSig)});
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
				validOpEmitters.push_back([this, arity](RandomStream& random) {
					const FunctionType ifSig = generateBlockSig(
						random, TypeTuple(stack.data() + stack.size() - arity - 1, arity));
					stack.resize(stack.size() - arity - 1);
					stack.insert(stack.end(), ifSig.params().begin(), ifSig.params().end());
					codeStream.if_({getIndexedBlockType(moduleState, ifSig)});
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

			// Check whether the top of the stack is compatible with branch target's parameters.
			if(doesStackMatchParams(params))
			{
				// br
				validOpEmitters.push_back([this, branchTargetDepth](RandomStream& random) {
					codeStream.br({U32(branchTargetDepth)});
					emitControlEnd();
				});

				if(branchTargetDepth == controlStack.size() - 1)
				{
					// return
					validOpEmitters.push_back([this](RandomStream& random) {
						codeStream.return_();
						emitControlEnd();
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
				{
					continue;
				}

				// Check whether the top of the stack is compatible with branch target's parameters.
				if(doesStackMatchParams(params, /*offsetFromTopOfStack*/ 1))
				{
					validOpEmitters.push_back([this, branchTargetDepth](RandomStream& random) {
						stack.pop_back();
						codeStream.br_if({U32(branchTargetDepth)});
					});
				}
			}
		}

		// unreachable
		validOpEmitters.push_back([this](RandomStream& random) {
			codeStream.unreachable();
			emitControlEnd();
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
					validOpEmitters.push_back([this, joinType](RandomStream& random) {
						stack.resize(stack.size() - 3);
						stack.push_back(joinType);
						codeStream.select({joinType});
					});
				}
				else
				{
					// Non-typed select
					validOpEmitters.push_back([this, joinType](RandomStream& random) {
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
			validOpEmitters.push_back([this](RandomStream& random) {
				codeStream.drop();
				stack.pop_back();
			});
		}

		// call
		for(Uptr functionIndex = 0; functionIndex < module_.functions.size(); ++functionIndex)
		{
			const FunctionType calleeType
				= module_.types[module_.functions.getType(functionIndex).index];
			const TypeTuple params = calleeType.params();
			const TypeTuple results = calleeType.results();

			// If the random stream has run out of entropy, only consider operators that result
			// in fewer operands on the stack.
			if(!allowStackGrowth && results.size() >= params.size()) { continue; }

			// Ensure the stack has enough values for the operator's parameters.
			if(params.size() > stack.size() - controlStack.back().outerStackSize) { continue; }

			// Check whether the top of the stack is compatible with function's parameters.
			if(doesStackMatchParams(params))
			{
				validOpEmitters.push_back([this, functionIndex](RandomStream& random) {
					const FunctionType calleeType
						= moduleState.module_
							  .types[moduleState.module_.functions.getType(functionIndex).index];

					codeStream.call({functionIndex});

					// Remove the function's parameters from the top of the stack.
					stack.resize(stack.size() - calleeType.params().size());

					// Push the function's results onto the stack.
					for(ValueType result : calleeType.results()) { stack.push_back(result); }
				});
			}
		}

		// ref.null
		validOpEmitters.push_back([this](RandomStream& random) {
			const ReferenceType nullReferenceType = generateRefType(random);
			codeStream.ref_null({nullReferenceType});
			stack.push_back(asValueType(nullReferenceType));
		});

		// ref.is_null
		if(stack.size() > controlStack.back().outerStackSize && isReferenceType(stack.back()))
		{
			validOpEmitters.push_back([this](RandomStream& random) {
				codeStream.ref_is_null();
				stack.back() = ValueType::i32;
			});
		}

		// Emit a random operator.
		WAVM_ASSERT(validOpEmitters.size());
		const Uptr randomOpIndex = random.get(validOpEmitters.size() - 1);
		validOpEmitters[randomOpIndex](random);
		++numInstructions;
	};

	codeStream.finishValidation();

	functionDef.code = codeByteStream.getBytes();
};

static InitializerExpression generateInitializerExpression(Module& module_,
														   RandomStream& random,
														   ValueType type)
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
		const Uptr functionIndex = random.get(module_.functions.size());
		return functionIndex == module_.functions.size()
				   ? InitializerExpression(ReferenceType::funcref)
				   : InitializerExpression(InitializerExpression::Type::ref_func, functionIndex);
	}

	case ValueType::none:
	case ValueType::any:
	default: WAVM_UNREACHABLE();
	}
}

void IR::generateValidModule(Module& module_, RandomStream& random)
{
	ModuleState moduleState(module_, random);

	WAVM_ASSERT(module_.featureSpec.simd);
	WAVM_ASSERT(module_.featureSpec.atomics);
	WAVM_ASSERT(module_.featureSpec.exceptionHandling);
	WAVM_ASSERT(module_.featureSpec.multipleResultsAndBlockParams);
	WAVM_ASSERT(module_.featureSpec.bulkMemoryOperations);
	WAVM_ASSERT(module_.featureSpec.referenceTypes);
	WAVM_ASSERT(module_.featureSpec.sharedTables);

	// Generate some memories.
	const Uptr numMemories = random.get(3);
	for(Uptr memoryIndex = 0; memoryIndex < numMemories; ++memoryIndex)
	{
		MemoryType type;
		type.isShared = !!random.get(1);
		type.indexType = generateIndexType(random);
		type.size.min = random.get<U64>(100);
		type.size.max = type.size.min + random.get<U64>(IR::maxMemory32Pages - type.size.min);

		if(random.get(1)) { module_.memories.defs.push_back({type}); }
		else
		{
			module_.imports.push_back({ExternKind::memory, module_.memories.imports.size()});
			module_.memories.imports.push_back({type, "env", "memory"});
		}
	}

	// Generate some tables.
	const Uptr numTables = random.get(3);
	for(Uptr tableIndex = 0; tableIndex < numTables; ++tableIndex)
	{
		TableType type;
		type.elementType = generateRefType(random);
		type.isShared = !!random.get(1);
		type.indexType = generateIndexType(random);
		type.size.min = random.get<U64>(100);
		type.size.max = IR::maxTable32Elems;

		if(random.get(1)) { module_.tables.defs.push_back({type}); }
		else
		{
			module_.imports.push_back({ExternKind::table, module_.tables.imports.size()});
			module_.tables.imports.push_back({type, "env", "table"});
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
			module_.imports.push_back({ExternKind::global, module_.globals.imports.size()});
			module_.globals.imports.push_back(
				{globalType, "env", "global" + std::to_string(globalIndex)});
		}
		else
		{
			InitializerExpression initializer
				= generateInitializerExpression(module_, random, globalValueType);
			module_.globals.defs.push_back({globalType, initializer});
		}
	};

	// Generate some data segments.
	Uptr numDataSegments = random.get(2);
	for(Uptr segmentIndex = 0; segmentIndex < numDataSegments; ++segmentIndex)
	{
		const Uptr numSegmentBytes = random.get(100);
		std::vector<U8> bytes;
		for(Uptr byteIndex = 0; byteIndex < numSegmentBytes; ++byteIndex)
		{
			bytes.push_back(random.get<U8>(255));
		}
		if(!module_.memories.size() || random.get(1))
		{
			module_.dataSegments.push_back(
				{false, UINTPTR_MAX, {}, std::make_shared<std::vector<U8>>(std::move(bytes))});
		}
		else
		{
			const Uptr memoryIndex = random.get(module_.memories.size() - 1);
			const MemoryType& memoryType = module_.memories.getType(memoryIndex);
			module_.dataSegments.push_back(
				{true,
				 memoryIndex,
				 generateInitializerExpression(module_, random, asValueType(memoryType.indexType)),
				 std::make_shared<std::vector<U8>>(std::move(bytes))});
		}
	};

	// Create some function imports/defs
	const Uptr numFunctions = 1 + random.get(4);
	while(module_.functions.size() < numFunctions)
	{
		// Generate a signature.
		FunctionType functionType = generateFunctionType(random);
		const Uptr functionTypeIndex
			= moduleState.functionTypeMap.getOrAdd(functionType, module_.types.size());
		if(functionTypeIndex == module_.types.size()) { module_.types.push_back(functionType); }

		if(random.get(1))
		{
			// Generate a function import.
			module_.imports.push_back({ExternKind::function, module_.functions.imports.size()});
			module_.functions.imports.push_back(
				{{functionTypeIndex},
				 "env",
				 "func" + std::to_string(module_.functions.imports.size())});
		}
		else
		{
			// Generate a FunctionDef, but don't generate its code until we have generated
			// all declarations.
			FunctionDef functionDef;
			functionDef.type.index = functionTypeIndex;

			// Generate locals.
			const Uptr numNonParameterLocals = random.get(4);
			for(Uptr localIndex = 0; localIndex < numNonParameterLocals; ++localIndex)
			{
				functionDef.nonParameterLocalTypes.push_back(generateValueType(random));
			}

			module_.functions.defs.push_back(std::move(functionDef));
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
					const Uptr functionIndex = random.get(module_.functions.size());
					if(functionIndex == module_.functions.size())
					{
						contents->elemExprs.push_back(ElemExpr(ReferenceType::funcref));
					}
					else
					{
						contents->elemExprs.push_back(
							ElemExpr(ElemExpr::Type::ref_func, functionIndex));
						if(declaredFunctionIndexSet.add(functionIndex))
						{
							moduleState.declaredFunctionIndices.push_back(functionIndex);
						}
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
					if(module_.functions.size())
					{
						const Uptr functionIndex = random.get(module_.functions.size() - 1);
						contents->elemIndices.push_back(functionIndex);
						if(declaredFunctionIndexSet.add(functionIndex))
						{
							moduleState.declaredFunctionIndices.push_back(functionIndex);
						}
					}
					break;
				case ExternKind::table:
					if(module_.tables.size())
					{
						contents->elemIndices.push_back(random.get(module_.tables.size() - 1));
					}
					break;
				case ExternKind::memory:
					if(module_.memories.size())
					{
						contents->elemIndices.push_back(random.get(module_.memories.size() - 1));
					}
					break;
				case ExternKind::global:
					if(module_.globals.size())
					{
						contents->elemIndices.push_back(random.get(module_.globals.size() - 1));
					}
					break;
				case ExternKind::exceptionType:
					if(module_.exceptionTypes.size())
					{
						contents->elemIndices.push_back(
							random.get(module_.exceptionTypes.size() - 1));
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
		for(Uptr tableIndex = 0; tableIndex < module_.tables.size(); ++tableIndex)
		{
			const ReferenceType tableElemType = module_.tables.getType(tableIndex).elementType;
			if(isSubtype(segmentElemType, tableElemType))
			{
				validTableIndices.push_back(tableIndex);
			}
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
			module_.elemSegments.push_back({ElemSegment::Type::passive,
											UINTPTR_MAX,
											InitializerExpression(),
											std::move(contents)});
			break;
		}
		case ElemSegment::Type::active: {
			const Uptr validTableIndex = random.get(validTableIndices.size() - 1);
			const TableType& tableType = module_.tables.getType(validTableIndices[validTableIndex]);
			module_.elemSegments.push_back(
				{ElemSegment::Type::active,
				 validTableIndices[validTableIndex],
				 generateInitializerExpression(module_, random, asValueType(tableType.indexType)),
				 std::move(contents)});
			break;
		}
		case ElemSegment::Type::declared: {
			module_.elemSegments.push_back({ElemSegment::Type::declared,
											UINTPTR_MAX,
											InitializerExpression(),
											std::move(contents)});
			break;
		}
		default: WAVM_UNREACHABLE();
		};

		// Precalculate a list of element-table pairs that are valid for a table.init
		for(Uptr tableIndex = 0; tableIndex < module_.tables.size(); ++tableIndex)
		{
			if(isSubtype(segmentElemType, module_.tables.getType(tableIndex).elementType))
			{
				moduleState.validElemSegmentAndTableImms.push_back(
					ElemSegmentAndTableImm{segmentIndex, tableIndex});
			}
		}
	};

	std::shared_ptr<ModuleValidationState> moduleValidationState
		= createModuleValidationState(module_);

	validatePreCodeSections(*moduleValidationState);

	// Generate a few functions.
	for(FunctionDef& functionDef : module_.functions.defs)
	{
		FunctionState functionState(moduleState, *moduleValidationState, functionDef);
		functionState.generateFunction(random);
	}

	// Generating functions might have added some block types, so revalidate the type section.
	validateTypes(*moduleValidationState);

	validatePostCodeSections(*moduleValidationState);
}
