#include "IR/Module.h"
#include "IR/Operators.h"
#include "IR/Types.h"
#include "IR/Validate.h"
#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Hash.h"
#include "Inline/HashMap.h"
#include "Inline/Serialization.h"
#include "Logging/Logging.h"
#include "Runtime/Intrinsics.h"
#include "Runtime/Linker.h"
#include "Runtime/Runtime.h"
#include "WASM/WASM.h"
#include "WASTParse/TestScript.h"
#include "WASTParse/WASTParse.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

using namespace IR;
using namespace Runtime;
using namespace WAST;

// A stream that uses a combination of a PRNG and input data to produce pseudo-random values.
struct RandomStream
{
	RandomStream(const U8* inData, Uptr numBytes)
	: next(inData), end(inData + numBytes), denominator(0), numerator(0), seed(0)
	{
		refill();
	}

	// Returns true if the stream has more entropy from input data to use to generate random values.
	// If it returns false, get() will only be using the PRNG to generate values.
	bool hasMore() const { return next < end || numerator; }

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
		const U32 result = U32((numerator ^ seed) % (U64(maxResult) + 1));
		seed = 6364136223846793005 * seed + 1442695040888963407;
		numerator /= (U64(maxResult) + 1);
		denominator /= (U64(maxResult) + 1);
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

static void generateImm(RandomStream& random, const IR::Module& module, NoImm& outImm) {}
static void generateImm(RandomStream& random, const IR::Module& module, MemoryImm& outImm) {}

static void generateImm(RandomStream& random, const IR::Module& module, LiteralImm<I32>& outImm)
{
	outImm.value = I32(random.get(UINT32_MAX));
}

static void generateImm(RandomStream& random, const IR::Module& module, LiteralImm<I64>& outImm)
{
	outImm.value = I64(random.get(UINT64_MAX));
}

static void generateImm(RandomStream& random, const IR::Module& module, LiteralImm<F32>& outImm)
{
	const U32 u32 = random.get(UINT32_MAX);
	memcpy(&outImm.value, &u32, sizeof(U32));
}

static void generateImm(RandomStream& random, const IR::Module& module, LiteralImm<F64>& outImm)
{
	const U64 u64 = random.get(UINT64_MAX);
	memcpy(&outImm.value, &u64, sizeof(U64));
}

static void generateImm(RandomStream& random, const IR::Module& module, LiteralImm<V128>& outImm)
{
	outImm.value.u64[0] = random.get(UINT64_MAX);
	outImm.value.u64[1] = random.get(UINT64_MAX);
}

template<Uptr naturalAlignmentLog2>
static void generateImm(RandomStream& random,
						const IR::Module& module,
						LoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.alignmentLog2 = random.get<U8>(naturalAlignmentLog2);
	outImm.offset = random.get(UINT32_MAX);
}

template<Uptr naturalAlignmentLog2>
static void generateImm(RandomStream& random,
						const IR::Module& module,
						AtomicLoadOrStoreImm<naturalAlignmentLog2>& outImm)
{
	outImm.alignmentLog2 = naturalAlignmentLog2;
	outImm.offset = random.get(UINT32_MAX);
}

template<Uptr numLanes>
static void generateImm(RandomStream& random,
						const IR::Module& module,
						LaneIndexImm<numLanes>& outImm)
{
	outImm.laneIndex = random.get<U8>(numLanes - 1);
}

template<Uptr numLanes>
static void generateImm(RandomStream& random,
						const IR::Module& module,
						ShuffleImm<numLanes>& outImm)
{
	for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
	{ outImm.laneIndices[laneIndex] = random.get<U8>(numLanes * 2 - 1); }
}

// Build a table with information about non-parametric operators.

typedef CodeValidationProxyStream<OperatorEncoderStream> CodeStream;
typedef void OperatorEmitFunc(RandomStream&, const IR::Module&, CodeStream&);

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
	 [](RandomStream& random, const IR::Module& module, CodeStream& codeStream) {                  \
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

static void generateFunction(RandomStream& random, IR::Module& module)
{
	FunctionDef functionDef;

	// For now just use a ()->() signature.
	functionDef.type.index = 0;

	Serialization::ArrayOutputStream codeByteStream;
	OperatorEncoderStream opEncoder(codeByteStream);
	CodeValidationProxyStream<OperatorEncoderStream> codeStream(module, functionDef, opEncoder);

	std::vector<ValueType> stack;

	std::vector<const OperatorInfo*> validNonParametricOps;
	while(true)
	{
		// Build a list of the non-parametric operators that are valid given the current state of
		// the stack.
		validNonParametricOps.clear();
		for(Uptr opIndex = 0; opIndex < numNonParametricOps; ++opIndex)
		{
			const OperatorInfo* opInfo = &operatorInfos[opIndex];
			const TypeTuple params = opInfo->sig().params();
			const TypeTuple results = opInfo->sig().results();

			// If the random stream has run out of entropy, don't consider operators that might
			// result in a net increase in stack size.
			if(!random.hasMore() && results.size() > params.size()) { continue; }

			// Ensure the stack has enough values for the operator's parameters.
			if(params.size() > stack.size()) { continue; }

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
				validNonParametricOps.push_back(opInfo);
			}
		}

		// Count the number of possible parametric and non-parametric ops that could be emitted.
		Uptr numValidOps = 0;
		numValidOps += 1; // end
		numValidOps += validNonParametricOps.size();

		// Choose a random op to emit.
		Uptr randomOpIndex = random.get(numValidOps - 1);
		if(randomOpIndex == 0)
		{
			// Emit the function end.

			// If anything is left on the stack, pass it to the pre-generated nop function with the
			// right signature.
			while(stack.size())
			{
				ValueType type = stack.back();
				stack.pop_back();
				switch(type)
				{
				case ValueType::i32: codeStream.call({0}); break;
				case ValueType::i64: codeStream.call({1}); break;
				case ValueType::f32: codeStream.call({2}); break;
				case ValueType::f64: codeStream.call({3}); break;
				case ValueType::v128: codeStream.call({4}); break;
				default: Errors::unreachable();
				};
			}

			// Emit the end operator and break out of the operator emitting loop.
			codeStream.end();
			break;
		}
		else
		{
			const OperatorInfo* opInfo = validNonParametricOps[randomOpIndex - 1];

			// Emit the operator.
			opInfo->emit(random, module, codeStream);

			// Remove the operator's parameters from the top of the stack.
			stack.resize(stack.size() - opInfo->sig().params().size());

			// Push the operator's results onto the stack.
			for(ValueType result : opInfo->sig().results()) { stack.push_back(result); }
		}
	};

	codeStream.finishValidation();

	functionDef.code = codeByteStream.getBytes();

	module.functions.defs.push_back(std::move(functionDef));
};

void generateValidModule(IR::Module& module, const U8* inputBytes, Uptr numBytes)
{
	RandomStream random(inputBytes, numBytes);

	// Generate some standard definitions that are the same for all modules.

	module.types.push_back(FunctionType());

	module.types.push_back(FunctionType(TypeTuple{}, TypeTuple{ValueType::i32}));
	module.types.push_back(FunctionType(TypeTuple{}, TypeTuple{ValueType::i64}));
	module.types.push_back(FunctionType(TypeTuple{}, TypeTuple{ValueType::f32}));
	module.types.push_back(FunctionType(TypeTuple{}, TypeTuple{ValueType::f64}));
	module.types.push_back(FunctionType(TypeTuple{}, TypeTuple{ValueType::v128}));

	module.functions.defs.push_back({{1}, {}, {U8(Opcode::end), 0}, {}});
	module.functions.defs.push_back({{2}, {}, {U8(Opcode::end), 0}, {}});
	module.functions.defs.push_back({{3}, {}, {U8(Opcode::end), 0}, {}});
	module.functions.defs.push_back({{4}, {}, {U8(Opcode::end), 0}, {}});
	module.functions.defs.push_back({{5}, {}, {U8(Opcode::end), 0}, {}});

	module.memories.imports.push_back(
		{MemoryType{true, SizeConstraints{1024, IR::maxMemoryPages}}});

	validateDefinitions(module);

	// Generate functions until the random stream runs out of entropy.
	while(random.hasMore()) { generateFunction(random, module); };
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
