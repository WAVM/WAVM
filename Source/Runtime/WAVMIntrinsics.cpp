#include "Inline/Assert.h"
#include "Inline/BasicTypes.h"
#include "Inline/Floats.h"
#include "Logging/Logging.h"
#include "Intrinsics.h"
#include "RuntimePrivate.h"

#include <math.h>

namespace Runtime
{
	DEFINE_INTRINSIC_MODULE(wavmIntrinsics)

	template<typename Float>
	Float quietNaN(Float value)
	{
		Floats::FloatComponents<Float> components;
		components.value = value;
		components.bits.significand |= typename Floats::FloatComponents<Float>::Bits(1) << (Floats::FloatComponents<Float>::numSignificandBits - 1);
		return components.value;
	}

	template<typename Float>
	Float floatMin(Float left,Float right)
	{
		// If either operand is a NaN, convert it to a quiet NaN and return it.
		if(left != left) { return quietNaN(left); }
		else if(right != right) { return quietNaN(right); }
		// If either operand is less than the other, return it.
		else if(left < right) { return left; }
		else if(right < left) { return right; }
		else
		{
			// Finally, if the operands are apparently equal, compare their integer values to distinguish -0.0 from +0.0
			Floats::FloatComponents<Float> leftComponents;
			leftComponents.value = left;
			Floats::FloatComponents<Float> rightComponents;
			rightComponents.value = right;
			return leftComponents.bitcastInt < rightComponents.bitcastInt ? right : left;
		}
	}
	
	template<typename Float>
	Float floatMax(Float left,Float right)
	{
		// If either operand is a NaN, convert it to a quiet NaN and return it.
		if(left != left) { return quietNaN(left); }
		else if(right != right) { return quietNaN(right); }
		// If either operand is less than the other, return it.
		else if(left > right) { return left; }
		else if(right > left) { return right; }
		else
		{
			// Finally, if the operands are apparently equal, compare their integer values to distinguish -0.0 from +0.0
			Floats::FloatComponents<Float> leftComponents;
			leftComponents.value = left;
			Floats::FloatComponents<Float> rightComponents;
			rightComponents.value = right;
			return leftComponents.bitcastInt > rightComponents.bitcastInt ? right : left;
		}
	}

	template<typename Float>
	Float floatCeil(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return ceil(value); }
	}
	
	template<typename Float>
	Float floatFloor(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return floor(value); }
	}
	
	template<typename Float>
	Float floatTrunc(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return trunc(value); }
	}
	
	template<typename Float>
	Float floatNearest(Float value)
	{
		if(value != value) { return quietNaN(value); }
		else { return nearbyint(value); }
	}

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f32.min",F32,f32Min,F32 left,F32 right) { return floatMin(left,right); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f64.min",F64,f64Min,F64 left,F64 right) { return floatMin(left,right); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f32.max",F32,f32Max,F32 left,F32 right) { return floatMax(left,right); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f64.max",F64,f64Max,F64 left,F64 right) { return floatMax(left,right); }

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f32.ceil",F32,f32Ceil,F32 value) { return floatCeil(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f64.ceil",F64,f64Ceil,F64 value) { return floatCeil(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f32.floor",F32,f32Floor,F32 value) { return floatFloor(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f64.floor",F64,f64Floor,F64 value) { return floatFloor(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f32.trunc",F32,f32Trunc,F32 value) { return floatTrunc(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f64.trunc",F64,f64Trunc,F64 value) { return floatTrunc(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f32.nearest",F32,f32Nearest,F32 value) { return floatNearest(value); }
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"f64.nearest",F64,f64Nearest,F64 value) { return floatNearest(value); }

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"divideByZeroOrIntegerOverflowTrap",void,divideByZeroOrIntegerOverflowTrap)
	{
		throwException(Exception::integerDivideByZeroOrIntegerOverflowType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"unreachableTrap",void,unreachableTrap)
	{
		throwException(Exception::reachedUnreachableType);
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"accessViolationTrap",void,accessViolationTrap)
	{
		throwException(Exception::accessViolationType);
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"invalidFloatOperationTrap",void,invalidFloatOperationTrap)
	{
		throwException(Exception::invalidFloatOperationType);
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"indirectCallSignatureMismatch",void,indirectCallSignatureMismatch,
		I32 index, I64 expectedSignatureBits, I64 tableId)
	{
		TableInstance* table = getTableFromRuntimeData(contextRuntimeData,tableId);
		wavmAssert(table);
		void* elementValue = table->baseAddress[index].value;
		const FunctionType* actualSignature = table->baseAddress[index].type;
		const FunctionType* expectedSignature = reinterpret_cast<const FunctionType*>((Uptr)expectedSignatureBits);
		std::string ipDescription = "<unknown>";
		LLVMJIT::describeInstructionPointer(reinterpret_cast<Uptr>(elementValue),ipDescription);
		Log::printf(Log::Category::debug,"call_indirect signature mismatch: expected %s at index %u but got %s (%s)\n",
			asString(expectedSignature).c_str(),
			index,
			actualSignature ? asString(actualSignature).c_str() : "nullptr",
			ipDescription.c_str()
			);
		throwException(elementValue == nullptr ? Exception::undefinedTableElementType : Exception::indirectCallSignatureMismatchType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"indirectCallIndexOutOfBounds",void,indirectCallIndexOutOfBounds)
	{
		throwException(Exception::undefinedTableElementType);
	}

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"growMemory",I32,_growMemory,I32 deltaPages,I64 memoryId)
	{
		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,memoryId);
		wavmAssert(memory);
		const Iptr numPreviousMemoryPages = growMemory(memory,(Uptr)deltaPages);
		if(numPreviousMemoryPages + (Uptr)deltaPages > IR::maxMemoryPages) { return -1; }
		else { return (I32)numPreviousMemoryPages; }
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"currentMemory",I32,_currentMemory,I64 memoryId)
	{
		MemoryInstance* memory = getMemoryFromRuntimeData(contextRuntimeData,memoryId);
		wavmAssert(memory);
		Uptr numMemoryPages = getMemoryNumPages(memory);
		if(numMemoryPages > UINT32_MAX) { numMemoryPages = UINT32_MAX; }
		return (U32)numMemoryPages;
	}

	thread_local Uptr indentLevel = 0;

	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"debugEnterFunction",void,debugEnterFunction,I64 functionInstanceBits)
	{
		FunctionInstance* function = reinterpret_cast<FunctionInstance*>(functionInstanceBits);
		Log::printf(Log::Category::debug,"ENTER: %s\n",function->debugName.c_str());
		++indentLevel;
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"debugExitFunction",void,debugExitFunction,I64 functionInstanceBits)
	{
		FunctionInstance* function = reinterpret_cast<FunctionInstance*>(functionInstanceBits);
		--indentLevel;
		Log::printf(Log::Category::debug,"EXIT:  %s\n",function->debugName.c_str());
	}
	
	DEFINE_INTRINSIC_FUNCTION(wavmIntrinsics,"debugBreak",void,debugBreak)
	{
		Log::printf(Log::Category::debug,"================== wavmIntrinsics.debugBreak\n");
	}

	extern void dummyReferenceAtomics();

	Runtime::ModuleInstance* instantiateWAVMIntrinsics(Compartment* compartment)
	{
		dummyReferenceAtomics();
		return Intrinsics::instantiateModule(compartment,INTRINSIC_MODULE_REF(wavmIntrinsics));
	}
}
