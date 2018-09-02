#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <utility>
#include <vector>

#include "IR/IR.h"
#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Types.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "Runtime/Runtime.h"

using namespace IR;
using namespace Runtime;

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
		wavmAssert(result <= maxResult);
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

		wavmAssert(denominator >= maxResult);
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
		wavmAssert(result <= maxResult);
		return result;
	}
};

static void generateImm(RandomStream& random, IR::Module& module, NoImm& outImm) {}
static void generateImm(RandomStream& random, IR::Module& module, MemoryImm& outImm) {}

static void generateImm(RandomStream& random, IR::Module& module, LiteralImm<I32>& outImm)
{
	outImm.value = I32(random.get(UINT32_MAX));
}

static void generateImm(RandomStream& random, IR::Module& module, LiteralImm<I64>& outImm)
{
	outImm.value = I64(random.get(UINT64_MAX));
}

static void generateImm(RandomStream& random, IR::Module& module, LiteralImm<F32>& outImm)
{
	const U32 u32 = random.get(UINT32_MAX);
	memcpy(&outImm.value, &u32, sizeof(U32));
}

static void generateImm(RandomStream& random, IR::Module& module, LiteralImm<F64>& outImm)
{
	const U64 u64 = random.get(UINT64_MAX);
	memcpy(&outImm.value, &u64, sizeof(U64));
}

static void generateImm(RandomStream& random, IR::Module& module, LiteralImm<V128>& outImm)
{
	outImm.value.u64[0] = random.get(UINT64_MAX);
	outImm.value.u64[1] = random.get(UINT64_MAX);
}

template<Uptr naturalAlignmentLog2>
static void generateImm(RandomStream& random,
						IR::Module& module,
						LoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.alignmentLog2 = random.get<U8>(naturalAlignmentLog2);
	outImm.offset = random.get(UINT32_MAX);
}

template<Uptr naturalAlignmentLog2>
static void generateImm(RandomStream& random,
						IR::Module& module,
						AtomicLoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.alignmentLog2 = naturalAlignmentLog2;
	outImm.offset = random.get(UINT32_MAX);
}

template<Uptr numLanes>
static void generateImm(RandomStream& random, IR::Module& module, LaneIndexImm<numLanes>& outImm)
{
	outImm.laneIndex = random.get<U8>(numLanes - 1);
}

template<Uptr numLanes>
static void generateImm(RandomStream& random, IR::Module& module, ShuffleImm<numLanes>& outImm)
{
	for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
	{ outImm.laneIndices[laneIndex] = random.get<U8>(numLanes * 2 - 1); }
}

// Build a table with information about non-parametric operators.

typedef CodeValidationProxyStream<OperatorEncoderStream> CodeStream;
typedef void OperatorEmitFunc(RandomStream&, IR::Module&, CodeStream&);

struct OperatorInfo
{
	const char* name;
	FunctionType (*sig)();
	OperatorEmitFunc* emit;
};

#define VISIT_OP(encoding, name, nameString, Imm, SIGNATURE, ...)                                  \
	{nameString,                                                                                   \
	 []() {                                                                                        \
		 static FunctionType sig = SIGNATURE;                                                      \
		 return sig;                                                                               \
	 },                                                                                            \
	 [](RandomStream& random, IR::Module& module, CodeStream& codeStream) {                        \
		 Imm imm;                                                                                  \
		 generateImm(random, module, imm);                                                         \
		 codeStream.name(imm);                                                                     \
	 }},
static OperatorInfo operatorInfos[]{ENUM_NONCONTROL_NONPARAMETRIC_OPERATORS(VISIT_OP)};
#undef VISIT_OP

enum
{
	numNonParametricOps = sizeof(operatorInfos) / sizeof(OperatorInfo)
};

FunctionType generateBlockSig(RandomStream& random)
{
	// const ValueType resultType = ValueType(random.get(U32(ValueType::max)));
	// return resultType == ValueType::any ? FunctionType() : FunctionType(TypeTuple{resultType});
	return FunctionType();
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
				   : IndexedBlockType{IndexedBlockType::Format::noParametersOrResult};
	}
}

static void generateFunction(RandomStream& random,
							 IR::Module& module,
							 HashMap<FunctionType, Uptr>& functionTypeMap)
{
	FunctionDef functionDef;

	// Generate a signature.
	std::vector<ValueType> functionParams;
	while(true)
	{
		const ValueType paramType = ValueType(random.get(U32(ValueType::max)));
		if(paramType == ValueType::any) { break; }

		functionParams.push_back(paramType);
	};

	// const ValueType resultType = ValueType(random.get(U32(ValueType::max)));
	// FunctionType functionType(resultType == ValueType::any ? TypeTuple() : TypeTuple{resultType},
	//						  TypeTuple(functionParams));
	FunctionType functionType({}, TypeTuple(functionParams));
	functionDef.type.index = functionTypeMap.getOrAdd(functionType, module.types.size());
	if(functionDef.type.index == module.types.size()) { module.types.push_back(functionType); }

	// Generate locals.
	while(true)
	{
		const ValueType localType = ValueType(random.get(U32(ValueType::max)));
		if(localType == ValueType::any) { break; }

		functionDef.nonParameterLocalTypes.push_back(localType);
	};
	const Uptr numLocals = functionType.params().size() + functionDef.nonParameterLocalTypes.size();

	Serialization::ArrayOutputStream codeByteStream;
	OperatorEncoderStream opEncoder(codeByteStream);
	CodeValidationProxyStream<OperatorEncoderStream> codeStream(module, functionDef, opEncoder);

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
			= stack.size() - controlStack.back().outerStackSize <= 6 && numInstructions++ < 30;

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
				else if(stack[controlContext.outerStackSize + resultIndex]
						!= controlContext.results[resultIndex])
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
					validOpEmitters.push_back([&stack, &controlStack](RandomStream& random,
																	  IR::Module& module,
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
					validOpEmitters.push_back([&stack, &controlStack](RandomStream& random,
																	  IR::Module& module,
																	  CodeStream& codeStream) {
						// Emit the end operator.
						codeStream.end();

						// Push the control context's results on the stack.
						for(ValueType result : controlStack.back().results)
						{ stack.push_back(result); }

						// Pop the control stack.
						controlStack.pop_back();
					});
				}
			}
		}

		// Build a list of the non-parametric operators that are valid given the current state of
		// the stack.
		for(Uptr opIndex = 0; opIndex < numNonParametricOps; ++opIndex)
		{
			const OperatorInfo& opInfo = operatorInfos[opIndex];
			const TypeTuple params = opInfo.sig().params();
			const TypeTuple results = opInfo.sig().results();

			// If the random stream has run out of entropy, only consider operators that result in
			// fewer operands on the stack.
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
				// Add the operator to the list of valid operators.
				validOpEmitters.push_back([&stack, opInfo](RandomStream& random,
														   IR::Module& module,
														   CodeStream& codeStream) {
					opInfo.emit(random, module, codeStream);

					// Remove the operator's parameters from the top of the stack.
					stack.resize(stack.size() - opInfo.sig().params().size());

					// Push the operator's results onto the stack.
					for(ValueType result : opInfo.sig().results()) { stack.push_back(result); }
				});
			}
		}

		// Build a list of the parametric operators that are valid given the current state of the
		// stack.

		for(Uptr localIndex = 0; localIndex < numLocals; ++localIndex)
		{
			const ValueType localType
				= localIndex < functionType.params().size()
					  ? functionType.params()[localIndex]
					  : functionDef
							.nonParameterLocalTypes[localIndex - functionType.params().size()];

			if(stack.size() > controlStack.back().outerStackSize && localType == stack.back())
			{
				// set_local
				validOpEmitters.push_back([&stack, localIndex](RandomStream& random,
															   IR::Module& module,
															   CodeStream& codeStream) {
					codeStream.set_local({U32(localIndex)});
					stack.pop_back();
				});

				// tee_local
				if(allowStackGrowth)
				{
					validOpEmitters.push_back([localIndex](RandomStream& random,
														   IR::Module& module,
														   CodeStream& codeStream) {
						codeStream.tee_local({U32(localIndex)});
					});
				}
			}

			// get_local
			if(allowStackGrowth)
			{
				validOpEmitters.push_back([&stack, localIndex, localType](RandomStream& random,
																		  IR::Module& module,
																		  CodeStream& codeStream) {
					codeStream.get_local({U32(localIndex)});
					stack.push_back(localType);
				});
			}
		}

		for(Uptr globalIndex = 0; globalIndex < module.globals.size(); ++globalIndex)
		{
			const GlobalType globalType = module.globals.getType(globalIndex);

			if(stack.size() > controlStack.back().outerStackSize
			   && globalType.valueType == stack.back() && globalType.isMutable)
			{
				// set_global
				validOpEmitters.push_back([&stack, globalIndex](RandomStream& random,
																IR::Module& module,
																CodeStream& codeStream) {
					codeStream.set_global({U32(globalIndex)});
					stack.pop_back();
				});
			}

			// get_global
			if(allowStackGrowth)
			{
				validOpEmitters.push_back(
					[&stack, globalIndex, globalType](
						RandomStream& random, IR::Module& module, CodeStream& codeStream) {
						codeStream.get_global({U32(globalIndex)});
						stack.push_back(globalType.valueType);
					});
			}
		}

		if(allowStackGrowth)
		{
			// Enter a block control structure.
			validOpEmitters.push_back(
				[&stack, &controlStack, &functionTypeMap](
					RandomStream& random, IR::Module& module, CodeStream& codeStream) {
					const FunctionType blockSig = generateBlockSig(random);
					codeStream.block({getIndexedBlockType(module, functionTypeMap, blockSig)});
					controlStack.push_back({ControlContext::Type::block,
											stack.size(),
											blockSig.results(),
											blockSig.results(),
											TypeTuple()});
				});

			// Enter a loop control structure.
			validOpEmitters.push_back(
				[&stack, &controlStack, &functionTypeMap](
					RandomStream& random, IR::Module& module, CodeStream& codeStream) {
					const FunctionType loopSig = generateBlockSig(random);
					codeStream.loop({getIndexedBlockType(module, functionTypeMap, loopSig)});
					controlStack.push_back({ControlContext::Type::loop,
											stack.size(),
											loopSig.params(),
											loopSig.results(),
											TypeTuple()});
				});
		}

		// Enter an if control structure.
		if(allowStackGrowth && stack.size() > controlStack.back().outerStackSize
		   && stack.back() == ValueType::i32)
		{
			validOpEmitters.push_back(
				[&stack, &controlStack, &functionTypeMap](
					RandomStream& random, IR::Module& module, CodeStream& codeStream) {
					stack.pop_back();
					const FunctionType ifSig = generateBlockSig(random);
					codeStream.if_({getIndexedBlockType(module, functionTypeMap, ifSig)});
					controlStack.push_back({ControlContext::Type::ifThen,
											stack.size(),
											ifSig.results(),
											ifSig.results(),
											ifSig.params()});
				});
		}

		// TODO: try/catch/catch_all

		// br
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
				if(stack[stack.size() - params.size() + paramIndex] != params[paramIndex])
				{
					sigMatch = false;
					break;
				}
			}
			if(sigMatch)
			{
				validOpEmitters.push_back(
					[&controlStack, &stack, branchTargetDepth](
						RandomStream& random, IR::Module& module, CodeStream& codeStream) {
						codeStream.br({U32(branchTargetDepth)});
						if(controlStack.back().type == ControlContext::Type::ifThen)
						{
							// Emit the else operator.
							codeStream.else_();

							stack.resize(controlStack.back().outerStackSize);
							for(ValueType elseParam : controlStack.back().elseParams)
							{ stack.push_back(elseParam); }

							// Change the current control context type to an else clause.
							controlStack.back().type = ControlContext::Type::ifElse;
						}
						else
						{
							codeStream.end();
							stack.resize(controlStack.back().outerStackSize);
							for(ValueType result : controlStack.back().results)
							{ stack.push_back(result); }
							controlStack.pop_back();
						}
					});
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
						[&stack, branchTargetDepth, params](
							RandomStream& random, IR::Module& module, CodeStream& codeStream) {
							stack.resize(stack.size() - params.size() - 1);
							codeStream.br_if({U32(branchTargetDepth)});
						});
				}
			}
		}

		// TODO: br_table, unreachable, return

		// TODO: select

		if(stack.size() > controlStack.back().outerStackSize)
		{
			// drop
			validOpEmitters.push_back(
				[&stack](RandomStream& random, IR::Module& module, CodeStream& codeStream) {
					codeStream.drop();
					stack.pop_back();
				});
		}

		// call
		std::vector<Uptr> validFunctionIndices;
		for(Uptr functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex)
		{
			const FunctionType calleeType
				= module.types[module.functions.getType(functionIndex).index];
			const TypeTuple params = calleeType.params();
			const TypeTuple results = calleeType.results();

			// If the random stream has run out of entropy, only consider operators that result in
			// fewer operands on the stack.
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
			if(sigMatch) { validFunctionIndices.push_back(functionIndex); }
		}

		if(validFunctionIndices.size())
		{
			validOpEmitters.push_back([&stack, &validFunctionIndices](RandomStream& random,
																	  IR::Module& module,
																	  CodeStream& codeStream) {
				const Uptr randomFunctionIndex = random.get(validFunctionIndices.size() - 1);
				const Uptr functionIndex = validFunctionIndices[randomFunctionIndex];
				const FunctionType calleeType
					= module.types[module.functions.getType(functionIndex).index];

				codeStream.call({U32(functionIndex)});

				// Remove the functions's parameters from the top of the stack.
				stack.resize(stack.size() - calleeType.params().size());

				// Push the function's results onto the stack.
				for(ValueType result : calleeType.results()) { stack.push_back(result); }
			});
		}

		// TODO: call_indirect

		// Emit a random operator.
		wavmAssert(validOpEmitters.size());
		const Uptr randomOpIndex = random.get(validOpEmitters.size() - 1);
		validOpEmitters[randomOpIndex](random, module, codeStream);
		validOpEmitters.clear();
	};

	codeStream.finishValidation();

	functionDef.code = codeByteStream.getBytes();

	module.functions.defs.push_back(std::move(functionDef));
};

void generateValidModule(IR::Module& module, const U8* inputBytes, Uptr numBytes)
{
	RandomStream random(inputBytes, numBytes);

	HashMap<FunctionType, Uptr> functionTypeMap;

	// Generate some standard definitions that are the same for all modules.

	module.memories.defs.push_back({{true, {1024, IR::maxMemoryPages}}});
	module.tables.defs.push_back({{TableElementType::anyfunc, true, {1024, IR::maxTableElems}}});

	while(true)
	{
		const ValueType globalValueType = ValueType(random.get(U32(ValueType::max)));
		if(globalValueType == ValueType::any) { break; }

		const bool isMutable = random.get(1);
		const GlobalType globalType{globalValueType, isMutable};
		if(random.get(1)) { module.globals.imports.push_back({globalType}); }
		else
		{
			InitializerExpression initializer;
			switch(globalValueType)
			{
			case ValueType::i32:
				initializer.type = InitializerExpression::Type::i32_const;
				initializer.i32 = I32(random.get(UINT32_MAX));
				break;
			case ValueType::i64:
				initializer.type = InitializerExpression::Type::i64_const;
				initializer.i64 = I64(random.get(UINT64_MAX));
				break;
			case ValueType::f32:
				initializer.type = InitializerExpression::Type::f32_const;
				initializer.f32 = F32(random.get(UINT32_MAX));
				break;
			case ValueType::f64:
				initializer.type = InitializerExpression::Type::f64_const;
				initializer.f64 = F64(random.get(UINT64_MAX));
				break;
			case ValueType::v128:
				initializer.type = InitializerExpression::Type::v128_const;
				initializer.v128.u64[0] = random.get(UINT64_MAX);
				initializer.v128.u64[1] = random.get(UINT64_MAX);
				break;
			default: Errors::unreachable();
			}
			module.globals.defs.push_back({globalType, initializer});
		}
	};

	validateDefinitions(module);

	// Generate a few functions.
	while(module.functions.defs.size() < 3) { generateFunction(random, module, functionTypeMap); };
}

extern "C" I32 LLVMFuzzerTestOneInput(const U8* data, Uptr numBytes)
{
	IR::Module module;
	generateValidModule(module, data, numBytes);
	compileModule(module);
	collectGarbage();

	return 0;
}

#if !ENABLE_LIBFUZZER

#include "Inline/CLI.h"

I32 main(int argc, char** argv)
{
	if(argc != 2)
	{
		Log::printf(Log::error, "Usage: FuzzCompileModel in.wasm\n");
		return EXIT_FAILURE;
	}
	const char* inputFilename = argv[1];

	std::vector<U8> inputBytes;
	if(!loadFile(inputFilename, inputBytes)) { return EXIT_FAILURE; }

	LLVMFuzzerTestOneInput(inputBytes.data(), inputBytes.size());
	return EXIT_SUCCESS;
}
#endif
