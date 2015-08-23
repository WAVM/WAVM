#include "WAVM.h"

#include "llvm/IR/Verifier.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/raw_os_ostream.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

using namespace std;
using namespace WASM;

namespace Decode
{
	// Used to represent the types that may be loaded from or stored to memory.
	enum class MemoryType : uint8_t
	{
		I32 = uint8_t(Type::I32),
		F32 = uint8_t(Type::F32),
		F64 = uint8_t(Type::F64),
		I8,
		I16
	};

	// The binary operations that may be performed on integers.
	enum class IntBinaryOp
	{
		Add,
		Sub,
		Mul,
		SDiv,
		SRem,
		UDiv,
		URem,
		Or,
		And,
		Xor,
		Shl,
		LShr,
		AShr,
	};

	// The binary operations that may be performed on floats.
	enum class FloatBinaryOp
	{
		Add,
		Sub,
		Mul,
		Div,
		Rem,
		ATan2,
		Pow,
	};

	// The unary operations that may be performed on integers.
	enum class IntUnaryOp
	{
		Neg,
		BitwiseNot,
		LogicalNot,
		Abs,
		Clz
	};

	// The unary operations that may be performed on floats.
	enum class FloatUnaryOp
	{
		Neg,
		Abs,
		Ceil,
		Floor,
		Sqrt,
		Cos,
		Sin,
		Tan,
		ACos,
		ASin,
		ATan,
		Exp,
		Log,
	};

	// The predicates that may be used in a comparison.
	enum class CmpPredicate
	{
		Equal,
		NotEqual,
		Less,
		LessOrEqual,
		Greater,
		GreaterOrEqual,
	};

	template <class T>
	T inline unreachable()
	{
		assert(false && "unreachable");
		return *(T*)nullptr;
	}

	template <>
	void inline unreachable<void>()
	{
		assert(false && "unreachable");
	}

	class In
	{
		const uint8_t* cur;

		template <class T> T u8()
		{
			// todo: bounds check
			return T(*cur++);
		}

	public:
		In(const uint8_t* beg) : cur(beg) {}

		uint32_t u32()
		{
			uint32_t u32 = cur[0] | cur[1] << 8 | cur[2] << 16 | cur[3] << 24;
			cur += 4;
			return u32;
		}
		float f32()
		{
			union
			{
				uint8_t arr[4];
				float f;
			} u ={{cur[0],cur[1],cur[2],cur[3]}};
			cur += 4;
			return u.f;
		}
		double f64()
		{
			union
			{
				uint8_t arr[8];
				double d;
			} u ={{cur[0],cur[1],cur[2],cur[3],cur[4],cur[5],cur[6],cur[7]}};
			cur += 8;
			return u.d;
		}
		Stmt decodeStatement() { return u8<Stmt>(); }
		SwitchCase switch_case() { return u8<SwitchCase>(); }
		template<class T,class TWithImm> inline bool code(T*,TWithImm*,uint8_t*);
		Void void_decodeExpression() { return u8<Void>(); }
		ExportFormat export_format() { return u8<ExportFormat>(); }
		Type type() { return u8<Type>(); }
		ReturnType returnType() { return u8<ReturnType>(); }
		inline uint32_t immU32();
		inline int32_t immS32();
		char single_char() { return *cur++; }
	};

	template <class T,class TWithImm>
	bool In::code(T* t,TWithImm* tWithImm,uint8_t* imm)
	{
		uint8_t byte = *cur++;
		if(!(byte & HasImmFlag))
		{
			*t = T(byte);
			return true;
		}

		UnpackOpWithImm(byte,tWithImm,imm);
		return false;
	}

	uint32_t inline In::immU32()
	{
		uint32_t u32 = *cur++;
		if(u32 < 0x80)
			return u32;

		u32 &= 0x7f;

		for(unsigned shift = 7; true; shift += 7)
		{
			uint32_t b = *cur++;
			if(b < 0x80)
				return u32 | (b << shift);
			u32 |= (b & 0x7f) << shift;
		}
	}

	int32_t inline In::immS32()
	{
		uint32_t u32 = *cur++;
		if(u32 < 0x80)
			return int32_t(u32) << (32-7) >> (32-7);

		u32 &= 0x7f;

		for(unsigned shift = 7; true; shift += 7)
		{
			uint32_t b = *cur++;
			if(b < 0x80)
			{
				u32 |= b << shift;
				int sign_extend = (32-7) - shift;
				if(sign_extend > 0)
					return int32_t(u32) << sign_extend >> sign_extend;
				return int32_t(u32);
			}
			u32 |= (b & 0x7f) << shift;
		}
	}

	struct Label
	{
		llvm::BasicBlock* breakToBlock;
		llvm::BasicBlock* continueToBlock;
	};

	struct State
	{
		In read;

		// Information about the mobule being decoded.
		WAVM::Module& module;

		vector<uint32_t> i32Constants;
		vector<float> f32Constants;
		vector<double> f64Constants;

		vector<WAVM::FunctionType> functionTypes;
		vector<WAVM::FunctionType> functionDeclarations;
		vector<llvm::Function*> llvmFunctions;
		vector<llvm::Constant*> llvmFunctionPointerTables;
		vector<uint32_t> functionPointerTableSizes;
		vector<WAVM::FunctionType> functionPointerTableTypes;
		
		vector<Type> globalVariableTypes;
		vector<llvm::Value*> globalVariableAddresses;

		// Information about the current operation being decoded.
		vector<Label> explicitLabels;
		vector<Label> implicitLabels;
		llvm::Function* llvmCurrentFunction;
		ReturnType currentReturnType;
		vector<Type> localVariableTypes;
		vector<llvm::Value*> localVariableAddresses;

		// Cached LLVM values.
		llvm::LLVMContext& llvmContext;
		llvm::IRBuilder<> llvmIRBuilder;

		llvm::Type* llvmVoidType;
		llvm::Type* llvmI1Type;
		llvm::Type* llvmI8Type;
		llvm::Type* llvmI16Type;
		llvm::Type* llvmI32Type;
		llvm::Type* llvmI64Type;
		llvm::Type* llvmF32Type;
		llvm::Type* llvmF64Type;

		llvm::Value* llvmCtlz32Intrinsic;

		llvm::Value* llvmFAbsF32Intrinsic;
		llvm::Value* llvmFAbsF64Intrinsic;
		llvm::Value* llvmCeilF32Intrinsic;
		llvm::Value* llvmCeilF64Intrinsic;
		llvm::Value* llvmFloorF32Intrinsic;
		llvm::Value* llvmFloorF64Intrinsic;
		llvm::Value* llvmSqrtF32Intrinsic;
		llvm::Value* llvmSqrtF64Intrinsic;
		llvm::Value* llvmCosF32Intrinsic;
		llvm::Value* llvmCosF64Intrinsic;
		llvm::Value* llvmSinF32Intrinsic;
		llvm::Value* llvmSinF64Intrinsic;
		llvm::Value* llvmExpF32Intrinsic;
		llvm::Value* llvmExpF64Intrinsic;
		llvm::Value* llvmLogF32Intrinsic;
		llvm::Value* llvmLogF64Intrinsic;

		llvm::Value* llvmMinnumF64Intrinsic;
		llvm::Value* llvmMaxnumF64Intrinsic;

		State(const uint8_t* in,WAVM::Module& inModule)
		:	read(in)
		,	llvmContext(llvm::getGlobalContext())
		,	llvmIRBuilder(llvm::getGlobalContext())
		,	module(inModule)
		{
			// Check for the WASM magic number.
			if(read.u32() != MagicNumber)
				abort();
			// ?
			read.u32();

			// Create the LLVM module.
			module.llvmModule = new llvm::Module("WebAssemblyModule",llvmContext);

			// Create a global variable that is used as the base address for memory accesses from this module.
			module.llvmVirtualAddressBase = new llvm::GlobalVariable(*module.llvmModule,llvm::Type::getInt8PtrTy(llvmContext),true,llvm::GlobalValue::PrivateLinkage,nullptr,"virtualAddressBase");

			// Cache LLVM types that the IR generation uses frequently.
			llvmVoidType = llvm::Type::getVoidTy(llvmContext);
			llvmI1Type = llvm::Type::getInt1Ty(llvmContext);
			llvmI8Type = llvm::Type::getInt8Ty(llvmContext);
			llvmI16Type = llvm::Type::getInt16Ty(llvmContext);
			llvmI32Type = llvm::Type::getInt32Ty(llvmContext);
			llvmI64Type = llvm::Type::getInt64Ty(llvmContext);
			llvmF32Type = llvm::Type::getFloatTy(llvmContext);
			llvmF64Type = llvm::Type::getDoubleTy(llvmContext);

			// Cache the LLVM intrinsics that the IR generation uses frequently.
			llvmCtlz32Intrinsic = getLLVMIntrinsic(ReturnType::I32,{llvmI32Type},llvm::Intrinsic::ctlz);

			llvmFAbsF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::fabs);
			llvmFAbsF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::fabs);
			llvmCeilF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::ceil);
			llvmCeilF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::ceil);
			llvmFloorF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::floor);
			llvmFloorF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::floor);
			llvmSqrtF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::sqrt);
			llvmSqrtF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::sqrt);
			llvmCosF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::cos);
			llvmCosF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::cos);
			llvmSinF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::sin);
			llvmSinF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::sin);
			llvmExpF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::exp);
			llvmExpF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::exp);
			llvmLogF32Intrinsic = getLLVMIntrinsic(ReturnType::F32,{llvmF32Type},llvm::Intrinsic::log);
			llvmLogF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::log);

			llvmMinnumF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::minnum);
			llvmMaxnumF64Intrinsic = getLLVMIntrinsic(ReturnType::F64,{llvmF64Type},llvm::Intrinsic::maxnum);
		}

		// Functions for creating LLVM types.

		llvm::Type* toLLVMType(const Type type)
		{
			switch(type)
			{
			case Type::I32: return llvmI32Type;
			case Type::F32: return llvmF32Type;
			case Type::F64: return llvmF64Type;
			default: return unreachable<llvm::Type*>();
			}
		}
		llvm::Type* toLLVMType(const ReturnType type)
		{
			switch(type)
			{
			case ReturnType::I32: return llvmI32Type;
			case ReturnType::F32: return llvmF32Type;
			case ReturnType::F64: return llvmF64Type;
			case ReturnType::Void: return llvmVoidType;
			default: return unreachable<llvm::Type*>();
			}
		}
		llvm::Type* toLLVMType(const MemoryType type)
		{
			switch(type)
			{
			case MemoryType::I32: return llvmI32Type;
			case MemoryType::F32: return llvmF32Type;
			case MemoryType::F64: return llvmF64Type;
			case MemoryType::I8: return llvmI8Type;
			case MemoryType::I16: return llvmI16Type;
			default: return unreachable<llvm::Type*>();
			}
		}
		llvm::FunctionType* makeLLVMFunctionType(const ReturnType returnType,const Type* argTypes,size_t numArgs)
		{
			vector<llvm::Type*> llvmArgTypes;
			for(size_t i = 0;i < numArgs;++i)
			{
				llvmArgTypes.push_back(toLLVMType(argTypes[i]));
			}

			auto llvmReturnType = toLLVMType(returnType);
			return llvm::FunctionType::get(llvmReturnType,llvmArgTypes,false);
		}
		template<size_t numArgs>
		llvm::FunctionType* makeLLVMFunctionType(const ReturnType returnType,const Type (&argTypes)[numArgs])
		{
			return makeLLVMFunctionType(returnType,argTypes,numArgs);
		}
		llvm::FunctionType* toLLVMType(const WAVM::FunctionType& functionType)
		{
			return makeLLVMFunctionType(functionType.ret,functionType.args.data(),functionType.args.size());
		}
		llvm::Value* getLLVMIntrinsic(const ReturnType returnType,const std::initializer_list<llvm::Type*>& argTypes,llvm::Intrinsic::ID id)
		{
			return llvm::Intrinsic::getDeclaration(module.llvmModule,id,std::vector<llvm::Type*>(argTypes));
		}

		// Returns a LLVM constant for the given 32-bit int.
		llvm::ConstantInt* getLLVMConstantInt32(uint32_t Value)
		{
			return (llvm::ConstantInt*)llvm::ConstantInt::get(llvmI32Type,llvm::APInt(32,(uint64_t)Value,false));
		}

		// Decodes an expression based on its ReturnType.
		template <ReturnType T> llvm::Value* decodeExpression();
		template <> llvm::Value* decodeExpression<ReturnType::I32>() { return decodeExpressionI32(); }
		template <> llvm::Value* decodeExpression<ReturnType::F32>() { return decodeExpressionF32(); }
		template <> llvm::Value* decodeExpression<ReturnType::F64>() { return decodeExpressionF64(); }
		template <> llvm::Value* decodeExpression<ReturnType::Void>() { return decodeExpressionVoid(); }

		// Reads the value of a local variable.
		llvm::Value* getLocal(Type expectedType,uint32_t localIndex)
		{
			assert(localVariableTypes[localIndex] == expectedType);
			return llvmIRBuilder.CreateLoad(localVariableAddresses[localIndex]);
		}

		// Stores a value to a local variable.
		template <Type T>
		llvm::Value* setLocal(uint32_t localIndex)
		{
			assert(localVariableTypes[localIndex] == T);
			auto value = decodeExpression<ReturnType(T)>();
			llvmIRBuilder.CreateStore(value,localVariableAddresses[localIndex]);
			return value;
		}
		llvm::Value* setLocal(uint32_t localIndex)
		{
			switch(localVariableTypes[localIndex])
			{
			case Type::I32: return setLocal<Type::I32>(localIndex); break;
			case Type::F32: return setLocal<Type::F32>(localIndex); break;
			case Type::F64: return setLocal<Type::F64>(localIndex); break;
			default: return unreachable<llvm::Value*>();
			}
		}

		// Reads the value of a global variable.
		llvm::Value* getGlobal(Type expectedType,uint32_t globalIndex)
		{
			assert(globalVariableTypes[globalIndex] == expectedType);
			return llvmIRBuilder.CreateLoad(globalVariableAddresses[globalIndex]);
		}

		// Stores a value to a global variable.
		template <Type T>
		llvm::Value* setGlobal(uint32_t globalIndex)
		{
			assert(globalVariableTypes[globalIndex] == T);
			auto value = decodeExpression<ReturnType(T)>();
			llvmIRBuilder.CreateStore(value,globalVariableAddresses[globalIndex]);
			return value;
		}
		llvm::Value* setGlobal(uint32_t globalIndex)
		{
			switch(globalVariableTypes[globalIndex])
			{
			case Type::I32: return setGlobal<Type::I32>(globalIndex);
			case Type::F32: return setGlobal<Type::F32>(globalIndex);
			case Type::F64: return setGlobal<Type::F64>(globalIndex);
			default: return unreachable<llvm::Value*>();
			}
		}

		// Casts a pointer to a different MemoryType.
		llvm::Value* castPointerType(MemoryType type,llvm::Value* Pointer)
		{
			llvm::Type* pointerType;
			switch(type)
			{
			case MemoryType::I8: pointerType = llvm::Type::getInt8PtrTy(llvmContext); break;
			case MemoryType::I16: pointerType = llvm::Type::getInt16PtrTy(llvmContext); break;
			case MemoryType::I32: pointerType = llvm::Type::getInt32PtrTy(llvmContext); break;
			case MemoryType::F32: pointerType = llvm::Type::getFloatPtrTy(llvmContext); break;
			case MemoryType::F64: pointerType = llvm::Type::getDoublePtrTy(llvmContext); break;
			default: return unreachable<llvm::Value*>();
			}
			return llvmIRBuilder.CreatePointerCast(Pointer,pointerType);
		}

		// Extends an I8 or I16 to an I32.
		llvm::Value* extendToI32(bool isSigned,llvm::Value* Value)
		{
			return isSigned ? llvmIRBuilder.CreateSExt(Value,llvmI32Type) : llvmIRBuilder.CreateZExt(Value,llvmI32Type);
		}

		// Reinterprets an I32 as a true or false value.
		llvm::Value* castI32ToBool(llvm::Value* Value)
		{
			return llvmIRBuilder.CreateICmp(llvm::CmpInst::Predicate::ICMP_NE,Value,getLLVMConstantInt32(0));
		}

		// Decodes an address in the form of an offset into the module's linear memory.
		llvm::Value* decodeAddress(uint32_t offset)
		{
			llvm::Value* byteIndex = decodeExpression<ReturnType::I32>();

			// Add the offset provided by the operation.
			if(offset != 0)
			{
				byteIndex = llvmIRBuilder.CreateAdd(byteIndex,getLLVMConstantInt32(offset));
			}

			// This zext is crucial for security, as LLVM will otherwise sext it to 64-bits. That would allow negative offsets, and access
			// to memory outside the VM's address space.
			byteIndex = llvmIRBuilder.CreateZExt(byteIndex,llvmI64Type);

			return llvmIRBuilder.CreateGEP(llvmIRBuilder.CreateLoad(module.llvmVirtualAddressBase),byteIndex);
		}

		// Loads an I8, I16, or I32 into an I32 intermediate. I8 and I16 is either zero or sign extended to 32-bit depending on isSigned.
		llvm::Value* loadInt(MemoryType memoryType,bool isSigned,uint32_t offset)
		{
			auto address = decodeAddress(offset);
			auto memoryValue = llvmIRBuilder.CreateLoad(castPointerType(memoryType,address));
			return extendToI32(isSigned,memoryValue);
		}

		// Loads a float from memory.
		llvm::Value* loadFloat(ReturnType type,uint32_t offset)
		{
			auto address = decodeAddress(offset);
			return llvmIRBuilder.CreateLoad(castPointerType((MemoryType)type,address));
		}

		// Stores a value to memory.
		template <ReturnType T>
		llvm::Value* store(MemoryType memoryType,uint32_t offset)
		{
			auto address = decodeAddress(offset);
			auto value = decodeExpression<T>();
			llvm::Value* memoryValue = value;
			if(T == ReturnType::I32)
			{
				switch(memoryType)
				{
				case MemoryType::I8: memoryValue = llvmIRBuilder.CreateTrunc(value,llvmI8Type); break;
				case MemoryType::I16: memoryValue = llvmIRBuilder.CreateTrunc(value,llvmI16Type); break;
				case MemoryType::I32: break;
				default: return unreachable<llvm::Value*>();
				}
			}
			llvmIRBuilder.CreateStore(memoryValue,castPointerType(memoryType,address));
			return value;
		}

		// Converts a signed or unsigned 32-bit integer to a float.
		llvm::Value* castI32ToFloat(llvm::Type* destType,bool isSigned)
		{
			return isSigned
				? llvmIRBuilder.CreateUIToFP(decodeExpression<ReturnType::I32>(),destType)
				: llvmIRBuilder.CreateSIToFP(decodeExpression<ReturnType::I32>(),destType);
		}

		// Converts a float to a 32-bit signed integer.
		template<ReturnType floatType>
		llvm::Value* castFloatToI32()
		{
			return llvmIRBuilder.CreateFPToSI(decodeExpression<floatType>(),llvmI32Type);
		}

		// Converts a 64-bit float to a 32-bit float.
		llvm::Value* castF64ToF32()
		{
			return llvmIRBuilder.CreateFPCast(decodeExpression<ReturnType::F64>(),llvmF32Type);
		}

		// Converts a 32-bit float to a 64-bit float.
		llvm::Value* castF32ToF64()
		{
			return llvmIRBuilder.CreateFPCast(decodeExpression<ReturnType::F32>(),llvmF64Type);
		}

		// Computes the absolute value of an integer operand.
		llvm::Value* absInt(ReturnType operandType,llvm::Value* operand)
		{
			auto mask = llvmIRBuilder.CreateAShr(operand,getLLVMConstantInt32(31));
			return llvmIRBuilder.CreateXor(llvmIRBuilder.CreateAdd(mask,operand),mask);
		}

		// Applies an unary operation to an integer operand.
		template<ReturnType operandType>
		llvm::Value* unaryOpInt(IntUnaryOp op)
		{
			auto operand = decodeExpression<operandType>();
			switch(op)
			{
			case IntUnaryOp::Neg: return llvmIRBuilder.CreateNeg(operand);
			case IntUnaryOp::BitwiseNot: return llvmIRBuilder.CreateNot(operand);
			case IntUnaryOp::LogicalNot: return extendToI32(false,llvmIRBuilder.CreateICmp(llvm::CmpInst::Predicate::ICMP_EQ,operand,getLLVMConstantInt32(0)));
			case IntUnaryOp::Clz: return llvmIRBuilder.CreateCall2(llvmCtlz32Intrinsic,operand,llvm::Constant::getIntegerValue(llvmI1Type,llvm::APInt(1,0)));
			case IntUnaryOp::Abs: return absInt(operandType,operand);
			default: return unreachable<llvm::Value*>();
			}
		}

		// Applies an unary operation to a floating-point operand.
		template<ReturnType operandType>
		llvm::Value* unaryOpFloat(FloatUnaryOp op)
		{
			auto operand = decodeExpression<operandType>();
			switch(op)
			{
			case FloatUnaryOp::Neg: return llvmIRBuilder.CreateFNeg(operand);
			case FloatUnaryOp::Abs: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmFAbsF32Intrinsic : llvmFAbsF64Intrinsic,operand);
			case FloatUnaryOp::Ceil: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmCeilF32Intrinsic : llvmCeilF64Intrinsic,operand);
			case FloatUnaryOp::Floor: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmFloorF32Intrinsic : llvmFloorF64Intrinsic,operand);
			case FloatUnaryOp::Sqrt: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmSqrtF32Intrinsic : llvmSqrtF64Intrinsic,operand);
			case FloatUnaryOp::Cos: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmCosF32Intrinsic : llvmCosF64Intrinsic,operand);
			case FloatUnaryOp::Sin: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmSinF32Intrinsic : llvmSinF64Intrinsic,operand);
			case FloatUnaryOp::Tan: /*HACK*/return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmLogF32Intrinsic : llvmLogF64Intrinsic,operand);
			case FloatUnaryOp::Exp: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmExpF32Intrinsic : llvmExpF64Intrinsic,operand);
			case FloatUnaryOp::Log: return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmLogF32Intrinsic : llvmLogF64Intrinsic,operand);
			case FloatUnaryOp::ACos: /*HACK*/return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmLogF32Intrinsic : llvmLogF64Intrinsic,operand);
			case FloatUnaryOp::ASin: /*HACK*/return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmLogF32Intrinsic : llvmLogF64Intrinsic,operand);
			case FloatUnaryOp::ATan: /*HACK*/return llvmIRBuilder.CreateCall(operandType == ReturnType::F32 ? llvmLogF32Intrinsic : llvmLogF64Intrinsic,operand);
			default: return unreachable<llvm::Value*>();
			}
		}

		// Applies a binary operation to an integer operand.
		template<ReturnType operandType>
		llvm::Value* binaryOpInt(IntBinaryOp op)
		{
			llvm::Value* leftOperand = decodeExpression<operandType>();
			llvm::Value* rightOperand = decodeExpression<operandType>();
			switch(op)
			{
			case IntBinaryOp::Add: return llvmIRBuilder.CreateAdd(leftOperand,rightOperand);
			case IntBinaryOp::Sub: return llvmIRBuilder.CreateSub(leftOperand,rightOperand);
			case IntBinaryOp::Mul: return llvmIRBuilder.CreateMul(leftOperand,rightOperand);
			case IntBinaryOp::SDiv: return llvmIRBuilder.CreateSDiv(leftOperand,rightOperand);
			case IntBinaryOp::SRem: return llvmIRBuilder.CreateSRem(leftOperand,rightOperand);
			case IntBinaryOp::UDiv: return llvmIRBuilder.CreateUDiv(leftOperand,rightOperand);
			case IntBinaryOp::URem: return llvmIRBuilder.CreateURem(leftOperand,rightOperand);
			case IntBinaryOp::Or: return llvmIRBuilder.CreateOr(leftOperand,rightOperand);
			case IntBinaryOp::And: return llvmIRBuilder.CreateAnd(leftOperand,rightOperand);
			case IntBinaryOp::Xor: return llvmIRBuilder.CreateXor(leftOperand,rightOperand);
			case IntBinaryOp::Shl: return llvmIRBuilder.CreateShl(leftOperand,rightOperand);
			case IntBinaryOp::LShr: return llvmIRBuilder.CreateLShr(leftOperand,rightOperand);
			case IntBinaryOp::AShr: return llvmIRBuilder.CreateAShr(leftOperand,rightOperand);
			default: return unreachable<llvm::Value*>();
			}
		}

		// Applies a binary operation to a floating-point operand.
		template<ReturnType operandType>
		llvm::Value* binaryOpFloat(FloatBinaryOp op)
		{
			llvm::Value* leftOperand = decodeExpression<operandType>();
			llvm::Value* rightOperand = decodeExpression<operandType>();
			switch(op)
			{
			case FloatBinaryOp::Add: return llvmIRBuilder.CreateFAdd(leftOperand,rightOperand);
			case FloatBinaryOp::Sub: return llvmIRBuilder.CreateFSub(leftOperand,rightOperand);
			case FloatBinaryOp::Mul: return llvmIRBuilder.CreateFMul(leftOperand,rightOperand);
			case FloatBinaryOp::Div: return llvmIRBuilder.CreateFDiv(leftOperand,rightOperand);
			case FloatBinaryOp::Rem: return llvmIRBuilder.CreateFRem(leftOperand,rightOperand);
			case FloatBinaryOp::ATan2: /*HACK*/return llvmIRBuilder.CreateFRem(leftOperand,rightOperand);
			case FloatBinaryOp::Pow: /*HACK*/return llvmIRBuilder.CreateFRem(leftOperand,rightOperand);
			default: return unreachable<llvm::Value*>();
			}
		}

		// Compares two integers and returns an I32 value.
		template<ReturnType operandType>
		llvm::Value* compareInt(bool isSigned,CmpPredicate predicate)
		{
			llvm::Value* leftOperand = decodeExpression<operandType>();
			llvm::Value* rightOperand = decodeExpression<operandType>();
			switch(predicate)
			{
			case CmpPredicate::Equal: return extendToI32(false,llvmIRBuilder.CreateICmpEQ(leftOperand,rightOperand));
			case CmpPredicate::NotEqual: return extendToI32(false,llvmIRBuilder.CreateICmpNE(leftOperand,rightOperand));
			case CmpPredicate::Less: return extendToI32(false,isSigned ? llvmIRBuilder.CreateICmpSLT(leftOperand,rightOperand) : llvmIRBuilder.CreateICmpULT(leftOperand,rightOperand));
			case CmpPredicate::LessOrEqual: return extendToI32(false,isSigned ? llvmIRBuilder.CreateICmpSLE(leftOperand,rightOperand) : llvmIRBuilder.CreateICmpULE(leftOperand,rightOperand));
			case CmpPredicate::Greater: return extendToI32(false,isSigned ? llvmIRBuilder.CreateICmpSGT(leftOperand,rightOperand) : llvmIRBuilder.CreateICmpUGT(leftOperand,rightOperand));
			case CmpPredicate::GreaterOrEqual: return extendToI32(false,isSigned ? llvmIRBuilder.CreateICmpSGE(leftOperand,rightOperand) : llvmIRBuilder.CreateICmpUGE(leftOperand,rightOperand));
			default: return unreachable<llvm::Value*>();
			}
		}

		// Compares two floats and returns an I32 value.
		template<ReturnType operandType>
		llvm::Value* compareFloat(CmpPredicate predicate)
		{
			llvm::Value* leftOperand = decodeExpression<operandType>();
			llvm::Value* rightOperand = decodeExpression<operandType>();
			switch(predicate)
			{
			case CmpPredicate::Equal: return extendToI32(false,llvmIRBuilder.CreateFCmpUEQ(leftOperand,rightOperand));
			case CmpPredicate::NotEqual: return extendToI32(false,llvmIRBuilder.CreateFCmpUNE(leftOperand,rightOperand));
			case CmpPredicate::Less: return extendToI32(false,llvmIRBuilder.CreateFCmpULT(leftOperand,rightOperand));
			case CmpPredicate::LessOrEqual: return extendToI32(false,llvmIRBuilder.CreateFCmpULE(leftOperand,rightOperand));
			case CmpPredicate::Greater: return extendToI32(false,llvmIRBuilder.CreateFCmpUGT(leftOperand,rightOperand));
			case CmpPredicate::GreaterOrEqual: return extendToI32(false,llvmIRBuilder.CreateFCmpUGE(leftOperand,rightOperand));
			default: return unreachable<llvm::Value*>();
			}
		}

		// Evaluates two expressions, and returns only the result of the second.
		template <ReturnType T>
		llvm::Value* comma()
		{
			switch(read.returnType())
			{
			case ReturnType::I32: decodeExpression<ReturnType::I32>(); break;
			case ReturnType::F32: decodeExpression<ReturnType::F32>(); break;
			case ReturnType::F64: decodeExpression<ReturnType::F64>(); break;
			case ReturnType::Void: decodeExpression<ReturnType::Void>(); break;
			default: return unreachable<llvm::Value*>();
			}
			return decodeExpression<T>();
		}

		// Chooses between two values based on a predicate value.
		template <ReturnType T>
		llvm::Value* cond()
		{
			auto condition = decodeExpression<ReturnType::I32>();

			auto trueBlock = llvm::BasicBlock::Create(llvmContext,"condTrue",llvmCurrentFunction);
			auto falseBlock = llvm::BasicBlock::Create(llvmContext,"condFalse",llvmCurrentFunction);
			auto successorBlock = llvm::BasicBlock::Create(llvmContext,"condSucc",llvmCurrentFunction);

			if(llvmIRBuilder.GetInsertBlock())
			{
				llvmIRBuilder.CreateCondBr(castI32ToBool(condition),trueBlock,falseBlock);
			}

			llvmIRBuilder.SetInsertPoint(trueBlock);
			auto trueValue = decodeExpression<T>();
			auto trueExitBlock = llvmIRBuilder.GetInsertBlock();
			createBranch(successorBlock);

			llvmIRBuilder.SetInsertPoint(falseBlock);
			auto falseValue = decodeExpression<T>();
			auto falseExitBlock = llvmIRBuilder.GetInsertBlock();
			createBranch(successorBlock);

			llvmIRBuilder.SetInsertPoint(successorBlock);
			auto phi = llvmIRBuilder.CreatePHI(trueValue->getType(),2);
			phi->addIncoming(trueValue,trueExitBlock);
			phi->addIncoming(falseValue,falseExitBlock);			

			return phi;
		}

		// Calls a function.
		llvm::Value* call(llvm::Value* function,const vector<Type>& args)
		{
			vector<llvm::Value*> llvmArgs;
			for(size_t i = 0;i < args.size();++i)
			{
				llvm::Value* llvmArg;
				switch(args[i])
				{
				case Type::I32: llvmArg = decodeExpression<ReturnType::I32>(); break;
				case Type::F32: llvmArg = decodeExpression<ReturnType::F32>(); break;
				case Type::F64: llvmArg = decodeExpression<ReturnType::F64>(); break;
				default: return unreachable<llvm::Value*>();
				}
				llvmArgs.push_back(llvmArg);
			}
			return llvmIRBuilder.CreateCall(function,llvmArgs);
		}

		// Calls a function by a direct pointer.
		llvm::Value* callInternal(uint32_t moduleFunctionIndex)
		{
			return call(llvmFunctions[moduleFunctionIndex],functionDeclarations[moduleFunctionIndex].args);
		}
		llvm::Value* callInternal(ReturnType returnType,uint32_t moduleFunctionIndex)
		{
			assert(functionDeclarations[moduleFunctionIndex].ret == returnType);
			return callInternal(moduleFunctionIndex);
		}

		// Calls a function by index into a function pointer table.
		llvm::Value* callIndirect(uint32_t tableIndex)
		{
			auto functionPointerTable = llvmFunctionPointerTables[tableIndex];
			auto tableSize = functionPointerTableSizes[tableIndex];
			auto functionType = functionPointerTableTypes[tableIndex];
			auto elementIndex = decodeExpression<ReturnType::I32>();
			auto maskedElementIndex = llvmIRBuilder.CreateAnd(elementIndex,getLLVMConstantInt32(tableSize-1));
			auto function = llvmIRBuilder.CreateLoad(llvmIRBuilder.CreateGEP(functionPointerTable,std::vector<llvm::Value*>({getLLVMConstantInt32(0),maskedElementIndex})));
			return call(function,functionType.args);
		}
		llvm::Value* callIndirect(ReturnType returnType,uint32_t tableIndex)
		{
			assert(functionPointerTableTypes[tableIndex].ret == returnType);
			return callIndirect(tableIndex);
		}

		// Calls a function by index into the module's import table.
		llvm::Value* callImport(uint32_t functionImportIndex)
		{
			WAVM::FunctionImport& functionImport = module.functionImports[functionImportIndex];
			functionImport.isReferenced = true;
			return call(llvmIRBuilder.CreateLoad(functionImport.llvmVariable),functionImport.type.args);
		}
		llvm::Value* callImport(ReturnType returnType,uint32_t functionImportIndex)
		{
			const WAVM::FunctionImport& functionImportType = module.functionImports[functionImportIndex];
			assert(functionImportType.type.ret == returnType);
			return callImport(functionImportIndex);
		}

		// Computes the minimum or maximum of a set of I32s.
		llvm::Value* minMaxI32(bool isSigned,bool isMin)
		{
			uint32_t numArgs = read.immU32();
			vector<llvm::Value*> args;
			for(uint32_t i = 0;i < numArgs;++i)
			{
				args.push_back(decodeExpression<ReturnType::I32>());
			}
			llvm::Value* result = args[0];
			for(auto argIt = args.begin() + 1;argIt != args.end();++argIt)
			{
				result = llvmIRBuilder.CreateSelect(
					isSigned ? llvmIRBuilder.CreateICmpSLT(result,*argIt) : llvmIRBuilder.CreateICmpULT(result,*argIt),
					isMin ? result : *argIt,
					isMin ? *argIt : result
					);
			}
			return result;
		}

		// Computes the minimum or maximum of a set of F64s.
		llvm::Value* minMaxF64(bool isMin)
		{
			uint32_t numArgs = read.immU32();
			assert(numArgs > 0);
			vector<llvm::Value*> args(numArgs);
			for(uint32_t i = 0;i < numArgs;++i)
			{
				args[i] = decodeExpression<ReturnType::F64>();
			}
			llvm::Value* result = args[0];
			for(auto argIt = args.begin() + 1;argIt != args.end();++argIt)
			{
				result = llvmIRBuilder.CreateCall2(isMin ? llvmMinnumF64Intrinsic : llvmMaxnumF64Intrinsic,result,*argIt);
			}
			return result;
		}

		void decodeBlock()
		{
			decodeStatementList();
		}

		void createBranch(llvm::BasicBlock* Dest)
		{
			if(llvmIRBuilder.GetInsertBlock())
			{
				llvmIRBuilder.CreateBr(Dest);
			}
		}

		void decodeIf()
		{
			auto condition = decodeExpression<ReturnType::I32>();

			auto trueBlock = llvm::BasicBlock::Create(llvmContext,"ifTrue",llvmCurrentFunction);
			auto successorBlock = llvm::BasicBlock::Create(llvmContext,"ifSucc",llvmCurrentFunction);

			if(llvmIRBuilder.GetInsertBlock())
			{
				llvmIRBuilder.CreateCondBr(castI32ToBool(condition),trueBlock,successorBlock);
			}

			llvmIRBuilder.SetInsertPoint(trueBlock);
			decodeStatement();
			createBranch(successorBlock);

			llvmIRBuilder.SetInsertPoint(successorBlock);
		}


		void decodeIfElse()
		{
			auto condition = decodeExpression<ReturnType::I32>();

			auto trueBlock = llvm::BasicBlock::Create(llvmContext,"ifElseTrue",llvmCurrentFunction);
			auto falseBlock = llvm::BasicBlock::Create(llvmContext,"ifElseFalse",llvmCurrentFunction);
			auto successorBlock = llvm::BasicBlock::Create(llvmContext,"ifElseSucc",llvmCurrentFunction);

			if(llvmIRBuilder.GetInsertBlock())
			{
				llvmIRBuilder.CreateCondBr(castI32ToBool(condition),trueBlock,falseBlock);
			}

			llvmIRBuilder.SetInsertPoint(trueBlock);
			decodeStatement();
			createBranch(successorBlock);

			llvmIRBuilder.SetInsertPoint(falseBlock);
			decodeStatement();
			createBranch(successorBlock);

			llvmIRBuilder.SetInsertPoint(successorBlock);
		}

		void decodeWhile()
		{
			auto conditionBlock = llvm::BasicBlock::Create(llvmContext,"whileCondition",llvmCurrentFunction);
			auto loopBlock = llvm::BasicBlock::Create(llvmContext,"whileLoop",llvmCurrentFunction);
			auto successorBlock = llvm::BasicBlock::Create(llvmContext,"whileSucc",llvmCurrentFunction);

			createBranch(conditionBlock);
			llvmIRBuilder.SetInsertPoint(conditionBlock);
			auto condition = decodeExpression<ReturnType::I32>();
			llvmIRBuilder.CreateCondBr(castI32ToBool(condition),loopBlock,successorBlock);
			implicitLabels.push_back({successorBlock,conditionBlock});

			llvmIRBuilder.SetInsertPoint(loopBlock);
			decodeStatement();
			createBranch(conditionBlock);

			implicitLabels.pop_back();
			llvmIRBuilder.SetInsertPoint(successorBlock);
		}

		void decodeDo()
		{
			auto loopBlock = llvm::BasicBlock::Create(llvmContext,"doLoop",llvmCurrentFunction);
			auto successorBlock = llvm::BasicBlock::Create(llvmContext,"doSucc",llvmCurrentFunction);

			createBranch(loopBlock);
			implicitLabels.push_back({successorBlock,loopBlock});

			llvmIRBuilder.SetInsertPoint(loopBlock);
			decodeStatement();
			auto condition = decodeExpression<ReturnType::I32>();
			if(llvmIRBuilder.GetInsertBlock())
			{
				llvmIRBuilder.CreateCondBr(castI32ToBool(condition),loopBlock,successorBlock);
			}

			implicitLabels.pop_back();
			llvmIRBuilder.SetInsertPoint(successorBlock);
		}

		void decodeLabel()
		{
			auto continueBlock = llvm::BasicBlock::Create(llvmContext,"labelContinue",llvmCurrentFunction);
			auto breakBlock = llvm::BasicBlock::Create(llvmContext,"labelBreak",llvmCurrentFunction);
			
			createBranch(continueBlock);
			llvmIRBuilder.SetInsertPoint(continueBlock);

			explicitLabels.push_back({breakBlock,continueBlock});
			decodeStatement();
			explicitLabels.pop_back();

			llvmIRBuilder.CreateBr(breakBlock);
			llvmIRBuilder.SetInsertPoint(breakBlock);
		}

		void decodeBreak()
		{
			assert(implicitLabels.size());
			const uint32_t labelIndex = implicitLabels.size() - 1;
			createBranch(implicitLabels[labelIndex].breakToBlock);

			// Clear the insert point for the unreachable code that may follow the break.
			llvmIRBuilder.ClearInsertionPoint();
		}

		void decodeContinue()
		{
			assert(implicitLabels.size());
			const uint32_t labelIndex = implicitLabels.size() - 1;
			createBranch(implicitLabels[labelIndex].continueToBlock);

			// Clear the insert point for the unreachable code that may follow the continue.
			llvmIRBuilder.ClearInsertionPoint();
		}

		void decodeBreakLabel(uint32_t labelIndex)
		{
			assert(labelIndex < explicitLabels.size());
			createBranch(explicitLabels[labelIndex].breakToBlock);

			// Clear the insert point for the unreachable code that may follow the break.
			llvmIRBuilder.ClearInsertionPoint();
		}

		void decodeContinueLabel(uint32_t labelIndex)
		{
			assert(labelIndex < explicitLabels.size());
			createBranch(explicitLabels[labelIndex].continueToBlock);

			// Clear the insert point for the unreachable code that may follow the continue.
			llvmIRBuilder.ClearInsertionPoint();
		}

		void decodeSwitch()
		{
			const uint32_t numCases = read.immU32();
			auto value = decodeExpression<ReturnType::I32>();
			auto defaultBlock = llvm::BasicBlock::Create(llvmContext,"switchDefault",llvmCurrentFunction);
			auto successorBlock = llvm::BasicBlock::Create(llvmContext,"switchSucc",llvmCurrentFunction);
			
			implicitLabels.push_back({successorBlock,nullptr});

			auto switchInstruction = llvmIRBuilder.CreateSwitch(value,defaultBlock,numCases);
			vector<llvm::BasicBlock*> caseEntryBasicBlocks(numCases + 1);
			vector<llvm::BasicBlock*> caseExitBasicBlocks(numCases);
			uint32_t defaultCaseIndex = numCases;
			for(uint32_t caseIndex = 0;caseIndex < numCases;++caseIndex)
			{
				auto caseType = read.switch_case();
				switch(caseType)
				{
				case SwitchCase::Case0:
				case SwitchCase::Case1:
				case SwitchCase::CaseN:
				{
					const int32_t caseValue = read.immS32();
					caseEntryBasicBlocks[caseIndex] = llvm::BasicBlock::Create(llvmContext,"switchCase",llvmCurrentFunction);
					switchInstruction->addCase(getLLVMConstantInt32(caseValue),caseEntryBasicBlocks[caseIndex]);
					break;
				}
				case SwitchCase::Default0:
				case SwitchCase::Default1:
				case SwitchCase::DefaultN:
					caseEntryBasicBlocks[caseIndex] = defaultBlock;
					defaultCaseIndex = caseIndex;
					break;
				default: unreachable<void>();
				};

				switch(caseType)
				{
				case SwitchCase::Case0:
				case SwitchCase::Default0:
					caseExitBasicBlocks[caseIndex] = caseEntryBasicBlocks[caseIndex];
					break;
				case SwitchCase::Case1:
				case SwitchCase::Default1:
					llvmIRBuilder.SetInsertPoint(caseEntryBasicBlocks[caseIndex]);
					decodeStatement();
					caseExitBasicBlocks[caseIndex] = llvmIRBuilder.GetInsertBlock();
					break;
				case SwitchCase::CaseN:
				case SwitchCase::DefaultN:
					llvmIRBuilder.SetInsertPoint(caseEntryBasicBlocks[caseIndex]);
					decodeStatementList();
					caseExitBasicBlocks[caseIndex] = llvmIRBuilder.GetInsertBlock();
					break;
				};
			}
			caseEntryBasicBlocks[numCases] = successorBlock;

			implicitLabels.pop_back();

			for(uint32_t caseIndex = 0;caseIndex < numCases;++caseIndex)
			{
				if(caseExitBasicBlocks[caseIndex])
				{
					llvmIRBuilder.SetInsertPoint(caseExitBasicBlocks[caseIndex]);
					createBranch(caseEntryBasicBlocks[caseIndex + 1]);
				}
			}

			if(defaultCaseIndex == numCases)
			{
				llvmIRBuilder.SetInsertPoint(defaultBlock);
				createBranch(successorBlock);
			}

			llvmIRBuilder.SetInsertPoint(successorBlock);
		}
		
		// Decodes a return based on the current function's return type.
		void decodeReturn()
		{
			if(currentReturnType == ReturnType::Void)
			{
				if(llvmIRBuilder.GetInsertBlock())
				{
					llvmIRBuilder.CreateRetVoid();
				}
			}
			else
			{
				llvm::Value* result = nullptr;
				switch(currentReturnType)
				{
				case ReturnType::I32: result = decodeExpression<ReturnType::I32>(); break;
				case ReturnType::F32: result = decodeExpression<ReturnType::F32>(); break;
				case ReturnType::F64: result = decodeExpression<ReturnType::F64>(); break;
				default: unreachable<void>();
				}
				if(llvmIRBuilder.GetInsertBlock())
				{
					llvmIRBuilder.CreateRet(result);
				}
			}

			// Clear the insert point for the unreachable code that may follow the return.
			llvmIRBuilder.ClearInsertionPoint();
		}
		
		// Decodes expression opcodes that return an I32.
		llvm::Value* decodeExpressionI32()
		{
			assert(llvmIRBuilder.GetInsertBlock());

			I32 i32;
			I32WithImm i32WithImm;
			uint8_t imm;
			if(read.code(&i32,&i32WithImm,&imm))
			{
				switch(i32)
				{
				case I32::LitImm:    return getLLVMConstantInt32(read.immU32());
				case I32::LitPool:    return getLLVMConstantInt32(i32Constants[read.immU32()]);
				case I32::GetLoc:     return getLocal(Type::I32,read.immU32());
				case I32::GetGlo:     return getGlobal(Type::I32,read.immU32());
				case I32::SetLoc:     return setLocal<Type::I32>(read.immU32());
				case I32::SetGlo:     return setGlobal<Type::I32>(read.immU32());
				case I32::SLoad8:     return loadInt(MemoryType::I8,true,0);
				case I32::SLoadOff8:  return loadInt(MemoryType::I8,true,read.immU32());
				case I32::ULoad8:     return loadInt(MemoryType::I8,false,0);
				case I32::ULoadOff8:  return loadInt(MemoryType::I8,false,read.immU32());
				case I32::SLoad16:    return loadInt(MemoryType::I16,true,0);
				case I32::SLoadOff16: return loadInt(MemoryType::I16,true,read.immU32());
				case I32::ULoad16:    return loadInt(MemoryType::I16,false,0);
				case I32::ULoadOff16: return loadInt(MemoryType::I16,false,read.immU32());
				case I32::Load32:     return loadInt(MemoryType::I32,false,0);
				case I32::LoadOff32:  return loadInt(MemoryType::I32,false,read.immU32());
				case I32::Store8:     return store<ReturnType::I32>(MemoryType::I8,0);
				case I32::StoreOff8:  return store<ReturnType::I32>(MemoryType::I8,read.immU32());
				case I32::Store16:    return store<ReturnType::I32>(MemoryType::I16,0);
				case I32::StoreOff16: return store<ReturnType::I32>(MemoryType::I16,read.immU32());
				case I32::Store32:    return store<ReturnType::I32>(MemoryType::I32,0);
				case I32::StoreOff32: return store<ReturnType::I32>(MemoryType::I32,read.immU32());
				case I32::CallInt:    return callInternal(ReturnType::I32,read.immU32());
				case I32::CallInd:    return callIndirect(ReturnType::I32,read.immU32());
				case I32::CallImp:    return callImport(ReturnType::I32,read.immU32());
				case I32::Cond:       return cond<ReturnType::I32>();
				case I32::Comma:      return comma<ReturnType::I32>();
				case I32::FromF32:    return castFloatToI32<ReturnType::F32>();
				case I32::FromF64:    return castFloatToI32<ReturnType::F64>();
				case I32::Neg:        return unaryOpInt<ReturnType::I32>(IntUnaryOp::Neg);
				case I32::Add:        return binaryOpInt<ReturnType::I32>(IntBinaryOp::Add);
				case I32::Sub:        return binaryOpInt<ReturnType::I32>(IntBinaryOp::Sub);
				case I32::Mul:        return binaryOpInt<ReturnType::I32>(IntBinaryOp::Mul);
				case I32::SDiv:       return binaryOpInt<ReturnType::I32>(IntBinaryOp::SDiv);
				case I32::UDiv:       return binaryOpInt<ReturnType::I32>(IntBinaryOp::UDiv);
				case I32::SMod:       return binaryOpInt<ReturnType::I32>(IntBinaryOp::SRem);
				case I32::UMod:       return binaryOpInt<ReturnType::I32>(IntBinaryOp::URem);
				case I32::BitNot:     return unaryOpInt<ReturnType::I32>(IntUnaryOp::BitwiseNot);
				case I32::BitOr:      return binaryOpInt<ReturnType::I32>(IntBinaryOp::Or);
				case I32::BitAnd:     return binaryOpInt<ReturnType::I32>(IntBinaryOp::And);
				case I32::BitXor:     return binaryOpInt<ReturnType::I32>(IntBinaryOp::Xor);
				case I32::Lsh:        return binaryOpInt<ReturnType::I32>(IntBinaryOp::Shl);
				case I32::ArithRsh:   return binaryOpInt<ReturnType::I32>(IntBinaryOp::AShr);
				case I32::LogicRsh:   return binaryOpInt<ReturnType::I32>(IntBinaryOp::LShr);
				case I32::Clz:        return unaryOpInt<ReturnType::I32>(IntUnaryOp::Clz);
				case I32::LogicNot:   return unaryOpInt<ReturnType::I32>(IntUnaryOp::LogicalNot);
				case I32::EqI32:      return compareInt<ReturnType::I32>(true,CmpPredicate::Equal);
				case I32::EqF32:      return compareFloat<ReturnType::F32>(CmpPredicate::Equal);
				case I32::EqF64:      return compareFloat<ReturnType::F64>(CmpPredicate::Equal);
				case I32::NEqI32:     return compareInt<ReturnType::I32>(true,CmpPredicate::NotEqual);
				case I32::NEqF32:     return compareFloat<ReturnType::F32>(CmpPredicate::NotEqual);
				case I32::NEqF64:     return compareFloat<ReturnType::F64>(CmpPredicate::NotEqual);
				case I32::SLeThI32:   return compareInt<ReturnType::I32>(true,CmpPredicate::Less);
				case I32::ULeThI32:   return compareInt<ReturnType::I32>(false,CmpPredicate::Less);
				case I32::LeThF32:    return compareFloat<ReturnType::F32>(CmpPredicate::Less);
				case I32::LeThF64:    return compareFloat<ReturnType::F64>(CmpPredicate::Less);
				case I32::SLeEqI32:   return compareInt<ReturnType::I32>(true,CmpPredicate::LessOrEqual);
				case I32::ULeEqI32:   return compareInt<ReturnType::I32>(false,CmpPredicate::LessOrEqual);
				case I32::LeEqF32:    return compareFloat<ReturnType::F32>(CmpPredicate::LessOrEqual);
				case I32::LeEqF64:    return compareFloat<ReturnType::F64>(CmpPredicate::LessOrEqual);
				case I32::SGrThI32:   return compareInt<ReturnType::I32>(true,CmpPredicate::Greater);
				case I32::UGrThI32:   return compareInt<ReturnType::I32>(false,CmpPredicate::Greater);
				case I32::GrThF32:    return compareFloat<ReturnType::F32>(CmpPredicate::Greater);
				case I32::GrThF64:    return compareFloat<ReturnType::F64>(CmpPredicate::Greater);
				case I32::SGrEqI32:   return compareInt<ReturnType::I32>(true,CmpPredicate::GreaterOrEqual);
				case I32::UGrEqI32:   return compareInt<ReturnType::I32>(false,CmpPredicate::GreaterOrEqual);
				case I32::GrEqF32:    return compareFloat<ReturnType::F32>(CmpPredicate::GreaterOrEqual);
				case I32::GrEqF64:    return compareFloat<ReturnType::F64>(CmpPredicate::GreaterOrEqual);
				case I32::SMin:       return minMaxI32(true,true);
				case I32::UMin:       return minMaxI32(false,true);
				case I32::SMax:       return minMaxI32(true,false);
				case I32::UMax:       return minMaxI32(false,false);
				case I32::Abs:        return unaryOpInt<ReturnType::I32>(IntUnaryOp::Abs);
				default: return unreachable<llvm::Value*>();
				}
			}
			else
			{
				switch(i32WithImm)
				{
				case I32WithImm::LitImm: return getLLVMConstantInt32(imm);
				case I32WithImm::LitPool: return getLLVMConstantInt32(i32Constants[imm]);
				case I32WithImm::GetLoc: return getLocal(Type::I32,imm);
				default: return unreachable<llvm::Value*>();
				}
			}
		}

		// Decodes an expression opcodes that return a F32.
		llvm::Value* decodeExpressionF32()
		{
			assert(llvmIRBuilder.GetInsertBlock());

			F32 f32;
			F32WithImm f32WithImm;
			uint8_t imm;
			if(read.code(&f32,&f32WithImm,&imm))
			{
				switch(f32)
				{
				case F32::LitImm:   return llvm::ConstantFP::get(llvmContext,llvm::APFloat(read.f32()));
				case F32::LitPool:  return llvm::ConstantFP::get(llvmContext,llvm::APFloat(f32Constants[read.immU32()]));
				case F32::GetLoc:   return getLocal(Type::F32,read.immU32());
				case F32::GetGlo:   return getGlobal(Type::F32,read.immU32());
				case F32::SetLoc:   return setLocal<Type::F32>(read.immU32());
				case F32::SetGlo:   return setGlobal<Type::F32>(read.immU32());
				case F32::Load:     return loadFloat(ReturnType::F32,0);
				case F32::LoadOff:  return loadFloat(ReturnType::F32,read.immU32());
				case F32::Store:    return store<ReturnType::F32>(MemoryType::F32,0);
				case F32::StoreOff: return store<ReturnType::F32>(MemoryType::F32,read.immU32());
				case F32::CallInt:  return callInternal(ReturnType::F32,read.immU32());
				case F32::CallInd:  return callIndirect(ReturnType::F32,read.immU32());
				case F32::Cond:     return cond<ReturnType::F32>();
				case F32::Comma:    return comma<ReturnType::F32>();
				case F32::FromS32:  return castI32ToFloat(llvmF32Type,true);
				case F32::FromU32:  return castI32ToFloat(llvmF32Type,false);
				case F32::FromF64:  return castF64ToF32();
				case F32::Neg:      return unaryOpFloat<ReturnType::F32>(FloatUnaryOp::Neg);
				case F32::Add:      return binaryOpFloat<ReturnType::F32>(FloatBinaryOp::Add);
				case F32::Sub:      return binaryOpFloat<ReturnType::F32>(FloatBinaryOp::Sub);
				case F32::Mul:      return binaryOpFloat<ReturnType::F32>(FloatBinaryOp::Mul);
				case F32::Div:      return binaryOpFloat<ReturnType::F32>(FloatBinaryOp::Div);
				case F32::Abs:      return unaryOpFloat<ReturnType::F32>(FloatUnaryOp::Abs);
				case F32::Ceil:     return unaryOpFloat<ReturnType::F32>(FloatUnaryOp::Ceil);
				case F32::Floor:    return unaryOpFloat<ReturnType::F32>(FloatUnaryOp::Floor);
				case F32::Sqrt:     return unaryOpFloat<ReturnType::F32>(FloatUnaryOp::Sqrt);
				default: return unreachable<llvm::Value*>();
				}
			}
			else
			{
				switch(f32WithImm)
				{
				case F32WithImm::LitPool: return llvm::ConstantFP::get(llvmContext,llvm::APFloat(f32Constants[imm]));
				case F32WithImm::GetLoc:  return getLocal(Type::F32,imm);
				default: return unreachable<llvm::Value*>();
				}
			}
		}
		
		// Decodes an expression opcodes that return a F64.
		llvm::Value* decodeExpressionF64()
		{
			assert(llvmIRBuilder.GetInsertBlock());

			F64 f64;
			F64WithImm f64WithImm;
			uint8_t imm;
			if(read.code(&f64,&f64WithImm,&imm))
			{
				switch(f64)
				{
				case F64::LitImm:   return llvm::ConstantFP::get(llvmContext,llvm::APFloat(read.f64()));
				case F64::LitPool:  return llvm::ConstantFP::get(llvmContext,llvm::APFloat(f64Constants[read.immU32()]));
				case F64::GetLoc:   return getLocal(Type::F64,read.immU32());
				case F64::GetGlo:   return getGlobal(Type::F64,read.immU32());
				case F64::SetLoc:   return setLocal<Type::F64>(read.immU32());
				case F64::SetGlo:   return setGlobal<Type::F64>(read.immU32());
				case F64::Load:     return loadFloat(ReturnType::F64,0);
				case F64::LoadOff:  return loadFloat(ReturnType::F64,read.immU32());
				case F64::Store:    return store<ReturnType::F64>(MemoryType::F64,0);
				case F64::StoreOff: return store<ReturnType::F64>(MemoryType::F64,read.immU32());
				case F64::CallInt:  return callInternal(ReturnType::F64,read.immU32());
				case F64::CallInd:  return callIndirect(ReturnType::F64,read.immU32());
				case F64::CallImp:  return callImport(ReturnType::F64,read.immU32());
				case F64::Cond:     return cond<ReturnType::F64>();
				case F64::Comma:    return comma<ReturnType::F64>();
				case F64::FromS32:  return castI32ToFloat(llvmF64Type,true);
				case F64::FromU32:  return castI32ToFloat(llvmF64Type,false);
				case F64::FromF32:  return castF32ToF64();
				case F64::Neg:      return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Neg);
				case F64::Add:      return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::Add);
				case F64::Sub:      return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::Sub);
				case F64::Mul:      return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::Mul);
				case F64::Div:      return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::Div);
				case F64::Mod:      return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::Rem);
				case F64::Min:      return minMaxF64(true);
				case F64::Max:      return minMaxF64(false);
				case F64::Abs:      return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Abs);
				case F64::Ceil:     return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Ceil);
				case F64::Floor:    return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Floor);
				case F64::Sqrt:     return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Sqrt);
				case F64::Cos:     return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Cos);
				case F64::Sin:      return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Sin);
				case F64::Tan:      return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Tan);
				case F64::ACos:     return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::ACos);
				case F64::ASin:     return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::ASin);
				case F64::ATan:     return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::ATan);
				case F64::ATan2:    return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::ATan2);
				case F64::Exp:      return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Exp);
				case F64::Ln:       return unaryOpFloat<ReturnType::F64>(FloatUnaryOp::Log);
				case F64::Pow:      return binaryOpFloat<ReturnType::F64>(FloatBinaryOp::Pow);
				default: return unreachable<llvm::Value*>();
				}
			}
			else
			{
				switch(f64WithImm)
				{
				case F64WithImm::LitPool: return llvm::ConstantFP::get(llvmContext,llvm::APFloat(f64Constants[imm]));
				case F64WithImm::GetLoc: return getLocal(Type::F64,imm);
				default: return unreachable<llvm::Value*>();
				}
			}
		}

		// Decodes expression opcodes that return no value.
		llvm::Value* decodeExpressionVoid()
		{
			switch(read.void_decodeExpression())
			{
			case Void::CallInt: return callInternal(ReturnType::Void,read.immU32());
			case Void::CallInd: return callIndirect(ReturnType::Void,read.immU32());
			case Void::CallImp: return callImport(ReturnType::Void,read.immU32());
			default: return unreachable<llvm::Value*>();
			}
		}
		
		// Decodes statement opcodes.
		void decodeStatement()
		{
			assert(llvmIRBuilder.GetInsertBlock());

			Stmt statement;
			StmtWithImm statementWithImm;
			uint8_t imm;
			if(read.code(&statement,&statementWithImm,&imm))
			{
				switch(statement)
				{
				case Stmt::SetLoc: setLocal(read.immU32()); break;
				case Stmt::SetGlo: setGlobal(read.immU32()); break;
				case Stmt::I32Store8: store<ReturnType::I32>(MemoryType::I8,0); break;
				case Stmt::I32StoreOff8: store<ReturnType::I32>(MemoryType::I8,read.immU32()); break;
				case Stmt::I32Store16: store<ReturnType::I32>(MemoryType::I16,0); break;
				case Stmt::I32StoreOff16: store<ReturnType::I32>(MemoryType::I16,read.immU32()); break;
				case Stmt::I32Store32: store<ReturnType::I32>(MemoryType::I32,0); break;
				case Stmt::I32StoreOff32: store<ReturnType::I32>(MemoryType::I32,read.immU32()); break;
				case Stmt::F32Store: store<ReturnType::F32>(MemoryType::F32,0); break;
				case Stmt::F32StoreOff: store<ReturnType::F32>(MemoryType::F32,read.immU32()); break;
				case Stmt::F64Store: store<ReturnType::F64>(MemoryType::F64,0); break;
				case Stmt::F64StoreOff: store<ReturnType::F64>(MemoryType::F64,read.immU32()); break;
				case Stmt::CallInt: callInternal(read.immU32()); break;
				case Stmt::CallInd: callIndirect(read.immU32()); break;
				case Stmt::CallImp: callImport(read.immU32()); break;
				case Stmt::Ret: decodeReturn(); break;
				case Stmt::Block: decodeBlock(); break;
				case Stmt::IfThen: decodeIf(); break;
				case Stmt::IfElse: decodeIfElse(); break;
				case Stmt::While: decodeWhile(); break;
				case Stmt::Do: decodeDo(); break;
				case Stmt::Label: decodeLabel(); break;
				case Stmt::Break: decodeBreak(); break;
				case Stmt::Continue: decodeContinue(); break;
				case Stmt::BreakLabel: decodeBreakLabel(read.immU32()); break;
				case Stmt::ContinueLabel: decodeContinueLabel(read.immU32()); break;
				case Stmt::Switch: decodeSwitch(); break;
				default: unreachable<void>();
				}
			}
			else
			{
				switch(statementWithImm)
				{
				case StmtWithImm::SetLoc: setLocal(imm); break;
				case StmtWithImm::SetGlo: setGlobal(imm); break;
				default: unreachable<void>();
				}
			}
		}

		void decodeStatementList()
		{
			uint32_t numStatements = read.immU32();
			for(uint32_t i = 0; i < numStatements; ++i)
				decodeStatement();
		}

		void decodeFunctions()
		{
			for(size_t functionIndex = 0; functionIndex < functionDeclarations.size(); functionIndex++)
			{
				const WAVM::FunctionType& functionType = functionDeclarations[functionIndex];
				llvm::FunctionType* llvmFunctionType = toLLVMType(functionType);

				llvm::Function* llvmFunction = llvmFunctions[functionIndex];
				auto basicBlock = llvm::BasicBlock::Create(llvmContext,"entry",llvmFunction);
				llvmIRBuilder.SetInsertPoint(basicBlock);

				currentReturnType = functionType.ret;
				llvmCurrentFunction = llvmFunction;
				localVariableTypes.clear();
				localVariableAddresses.clear();

				// Move the function arguments into allocas so they may be treated the same as other locals.
				size_t argIndex = 0;
				for(auto argIt = llvmFunction->arg_begin();argIt != llvmFunction->arg_end();++argIt,++argIndex)
				{
					Type type = functionType.args[argIndex];
					auto Allocation = llvmIRBuilder.CreateAlloca(toLLVMType(type),nullptr);
					llvmIRBuilder.CreateStore(argIt,Allocation);
					localVariableAddresses.push_back(Allocation);
					localVariableTypes.push_back(type);
				}

				// Decode the number of local variables used by the function.
				uint32_t numLocalI32s = 0;
				uint32_t numLocalF32s = 0;
				uint32_t numLocalF64s = 0;
				VaReturnTypes varTypes;
				VaReturnTypesWithImm varTypesWithImm;
				uint8_t imm;
				if(read.code(&varTypes,&varTypesWithImm,&imm))
				{
					if(varTypes & VaReturnTypes::I32) numLocalI32s = read.immU32();
					if(varTypes & VaReturnTypes::F32) numLocalF32s = read.immU32();
					if(varTypes & VaReturnTypes::F64) numLocalF64s = read.immU32();
				}
				else numLocalI32s = imm;
				
				// Allocate the function local variables.
				for(size_t i = 0; i < numLocalI32s; ++i)
				{
					localVariableAddresses.push_back(llvmIRBuilder.CreateAlloca(llvmI32Type,nullptr));
					localVariableTypes.push_back(Type::I32);
				}
				for(size_t i = 0; i < numLocalF32s; ++i)
				{
					localVariableAddresses.push_back(llvmIRBuilder.CreateAlloca(llvmF32Type,nullptr));
					localVariableTypes.push_back(Type::F32);
				}
				for(size_t i = 0; i < numLocalF64s; ++i)
				{
					localVariableAddresses.push_back(llvmIRBuilder.CreateAlloca(llvmF64Type,nullptr));
					localVariableTypes.push_back(Type::F64);
				}

				// Decode the function's statements.
				decodeStatementList();

				if(llvmIRBuilder.GetInsertBlock())
				{
					// Terminate the final basic block with a return void. This should only be hit for functions typed to return void.
					assert(currentReturnType == ReturnType::Void);
					llvmIRBuilder.CreateRetVoid();
				}

				#ifdef _DEBUG
					llvm::raw_os_ostream llvmErrStream(std::cout);
					bool result = llvm::verifyFunction(*llvmCurrentFunction,&llvmErrStream);
					llvmErrStream.flush();
					assert(!result);
				#endif
			}
		}

		void decodeExports()
		{
			switch(read.export_format())
			{
			case ExportFormat::Default:
			{
				// The module has a single export. Call it "default"
				auto functionIndex = read.immU32();
				module.exports["default"] = WAVM::FunctionExport({functionDeclarations[functionIndex],llvmFunctions[functionIndex]});
				break;
			}
			case ExportFormat::Record:
			{
				// The module has multiple exports. Each associates an externally exported name with a function index in the module.
				uint32_t numExports = read.immU32();
				for(uint32_t exportIndex = 0;exportIndex < numExports;++exportIndex)
				{
					std::string exportName;
					while(char c = read.single_char()) { exportName += c; };

					auto functionIndex = read.immU32();
					module.exports[exportName] = WAVM::FunctionExport({functionDeclarations[functionIndex],llvmFunctions[functionIndex]});
				}
				break;
			}
			default: unreachable<void>();
			}

			// Mark all the exported functions as having external linkage to ensure they aren't optimized away.
			for(auto exportIt = module.exports.begin();exportIt != module.exports.end();++exportIt)
			{
				exportIt->second.llvmFunction->setName(exportIt->first);
				exportIt->second.llvmFunction->setLinkage(llvm::GlobalValue::ExternalLinkage);
			}
		}

		// Decode the table of function types used in the module.
		void decodeFunctionTypes()
		{
			uint32_t numTypes = read.immU32();
			functionTypes.resize(numTypes);
			for(uint32_t typeIndex = 0; typeIndex != numTypes; typeIndex++)
			{
				ReturnType ret = read.returnType();
				uint32_t numArgs = read.immU32();
				WAVM::FunctionType type(ret);
				for(uint32_t argIndex = 0; argIndex < numArgs; argIndex++)
					type.args.push_back(read.type());
				functionTypes[typeIndex] = move(type);
			}
		}

		// Decode the table of function imports used in the module.
		void decodeFunctionImports()
		{
			uint32_t numFunctionImports = read.immU32();
			uint32_t numFunctionImportTypes = read.immU32();

			module.functionImports.resize(0);
			module.functionImports.reserve(numFunctionImportTypes);
			for(uint32_t functionImportIndex = 0; functionImportIndex < numFunctionImports; functionImportIndex++)
			{
				std::string importName;
				while(char c = read.single_char()) { importName += c; };

				uint32_t numTypes = read.immU32();
				for(uint32_t i = 0; i < numTypes; i++)
				{
					auto functionType = functionTypes[read.immU32()];
					// Create a global variable that can be filled in with the address of the imported function.
					module.functionImports.push_back({
						functionType,
						importName,
						new llvm::GlobalVariable(*module.llvmModule,toLLVMType(functionType)->getPointerTo(),true,llvm::GlobalValue::PrivateLinkage,nullptr,importName),
						false
						});
				}
			}
		}

		// Decodes the module's global variables.
		void decodeGlobals()
		{
			uint32_t numGlobalsI32 = read.immU32();
			uint32_t numGlobalsF32 = read.immU32();
			uint32_t numGlobalsF64 = read.immU32();
			uint32_t numImportsI32 = read.immU32();
			uint32_t numImportsF32 = read.immU32();
			uint32_t numImportsF64 = read.immU32();

			globalVariableTypes.resize(0);

			llvm::Constant* ZeroI32 = getLLVMConstantInt32(0);
			for(uint32_t i = 0; i < numGlobalsI32; ++i)
			{
				globalVariableTypes.push_back(Type::I32);
				globalVariableAddresses.push_back(new llvm::GlobalVariable(*module.llvmModule,llvmI32Type,false,llvm::GlobalValue::PrivateLinkage,ZeroI32));
			}

			llvm::Constant* ZeroF32 = llvm::ConstantFP::get(llvmContext,llvm::APFloat(0.0f));
			for(uint32_t i = 0; i < numGlobalsF32; ++i)
			{
				globalVariableTypes.push_back(Type::F32);
				globalVariableAddresses.push_back(new llvm::GlobalVariable(*module.llvmModule,llvmF32Type,false,llvm::GlobalValue::PrivateLinkage,ZeroF32));
			}

			llvm::Constant* ZeroF64 = llvm::ConstantFP::get(llvmContext,llvm::APFloat(0.0));
			for(uint32_t i = 0; i < numGlobalsF64; ++i)
			{
				globalVariableTypes.push_back(Type::F64);
				globalVariableAddresses.push_back(new llvm::GlobalVariable(*module.llvmModule,llvmF64Type,false,llvm::GlobalValue::PrivateLinkage,ZeroF64));
			}
			for(uint32_t i = 0; i < numImportsI32; ++i)
			{
				std::string importName;
				while(char c = read.single_char()) { importName += c; };

				auto llvmVariable = new llvm::GlobalVariable(*module.llvmModule,llvmI32Type,false,llvm::GlobalValue::PrivateLinkage,nullptr,importName);
				globalVariableTypes.push_back(Type::I32);
				globalVariableAddresses.push_back(llvmVariable);
				module.valueImports.push_back({Type::I32,importName,llvmVariable});
			}
			for(uint32_t i = 0; i < numImportsF32; ++i)
			{
				std::string importName;
				while(char c = read.single_char()) { importName += c; };

				auto llvmVariable = new llvm::GlobalVariable(*module.llvmModule,llvmF32Type,false,llvm::GlobalValue::PrivateLinkage,nullptr,importName);
				globalVariableTypes.push_back(Type::F32);
				globalVariableAddresses.push_back(llvmVariable);
				module.valueImports.push_back({Type::F32,importName,llvmVariable});
			}
			for(uint32_t i = 0; i < numImportsF64; ++i)
			{
				std::string importName;
				while(char c = read.single_char()) { importName += c; };

				auto llvmVariable = new llvm::GlobalVariable(*module.llvmModule,llvmF64Type,false,llvm::GlobalValue::PrivateLinkage,nullptr,importName);
				globalVariableTypes.push_back(Type::F64);
				globalVariableAddresses.push_back(llvmVariable);
				module.valueImports.push_back({Type::F64,importName,llvmVariable});
			}
		}

		// Decodes the constant table: constants that may be used by index throughout the module.
		void decodeConstantPool()
		{
			uint32_t numI32s = read.immU32();
			uint32_t numF32s = read.immU32();
			uint32_t numF64s = read.immU32();

			i32Constants.resize(numI32s);
			f32Constants.resize(numF32s);
			f64Constants.resize(numF64s);

			for(uint32_t i = 0; i < numI32s; ++i) { i32Constants[i] = read.immU32(); }
			for(uint32_t i = 0; i < numF32s; ++i) { f32Constants[i] = read.f32(); }
			for(uint32_t i = 0; i < numF64s; ++i) { f64Constants[i] = read.f64(); }
		}

		// Decodes the types of the functions defined in the module.
		void decodeFunctionDeclarations()
		{
			const uint32_t numFunctions = read.immU32();
			functionDeclarations.resize(numFunctions);
			llvmFunctions.resize(numFunctions);
			for(uint32_t functionIndex = 0;functionIndex < numFunctions;++functionIndex)
			{
				functionDeclarations[functionIndex] = functionTypes[read.immU32()];
				llvmFunctions[functionIndex] = llvm::Function::Create(toLLVMType(functionDeclarations[functionIndex]), llvm::Function::PrivateLinkage, "", module.llvmModule);
				llvmFunctions[functionIndex]->setCallingConv(llvm::CallingConv::C);
			}
		}

		// Decode the function pointer tables: groups of functions that may be called indirectly, grouped by type.
		void decodeFunctionPointerTables()
		{
			llvmFunctionPointerTables.resize(0);
			functionPointerTableSizes.resize(0);
			functionPointerTableTypes.resize(0);

			const uint32_t numFunctionPointerTables = read.immU32();
			for(uint32_t tableIndex = 0; tableIndex != numFunctionPointerTables; ++tableIndex)
			{
				auto functionType = functionTypes[read.immU32()];

				// Decode the function table elements.
				const uint32_t numElements = read.immU32();
				vector<llvm::Constant*> llvmFunctionTableElements(numElements);
				for(uint32_t elementIndex = 0;elementIndex < numElements;++elementIndex)
				{
					const uint32_t functionIndex = read.immU32();
					llvmFunctionTableElements[elementIndex] = llvmFunctions[functionIndex];
				}
				// Verify that the number of elements is a power of two, so we can use bitwise and to prevent out-of-bounds accesses.
				assert((numElements & (numElements-1)) == 0);

				// Create a LLVM global variable that holds the array of function pointers.
				auto llvmFunctionTablePointerType = llvm::ArrayType::get(toLLVMType(functionType)->getPointerTo(),llvmFunctionTableElements.size());
				auto llvmFunctionPointerTableValue = llvm::ConstantArray::get(llvmFunctionTablePointerType,llvmFunctionTableElements);
				auto llvmFunctionPointerTableGlobal = new llvm::GlobalVariable(
					*module.llvmModule,
					llvmFunctionTablePointerType,
					true,
					llvm::GlobalValue::PrivateLinkage,
					llvmFunctionPointerTableValue
					);
				llvmFunctionPointerTables.push_back(llvmFunctionPointerTableGlobal);
				functionPointerTableSizes.push_back(numElements);
				functionPointerTableTypes.push_back(functionType);
			}
		}

		bool decode()
		{
			decodeConstantPool();
			decodeFunctionTypes();
			decodeFunctionImports();
			decodeGlobals();
			decodeFunctionDeclarations();
			decodeFunctionPointerTables();
			decodeFunctions();
			decodeExports();
			return true;
		}
	};
}

namespace WAVM
{
	bool decodeWASM(const uint8_t* packed,WAVM::Module& outModule)
	{
		return Decode::State(packed,outModule).decode();
	}
}