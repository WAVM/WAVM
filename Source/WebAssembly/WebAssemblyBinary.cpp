#include "WAVM.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>

using namespace AST;

namespace WebAssemblyBinary
{
	// =================================================================================================
	// Magic serialization constants

	static const uint32_t MagicNumber = 0x6d736177;

	enum class StmtOpEncoding : uint8_t
	{
		SetLoc,
		SetGlo,
		I32Store8,
		I32StoreOff8,
		I32Store16,
		I32StoreOff16,
		I32Store32,
		I32StoreOff32,
		F32Store,
		F32StoreOff,
		F64Store,
		F64StoreOff,
		CallInt,
		CallInd,
		CallImp,
		Ret,
		Block,
		IfThen,
		IfElse,
		While,
		Do,
		Label,
		Break,
		BreakLabel,
		Continue,
		ContinueLabel,
		Switch,

		Bad
	};

	enum class StmtOpEncodingWithImm : uint8_t
	{
		SetLoc,
		SetGlo,
		Reseved1,
		Reseved2,

		Bad
	};

	enum class SwitchCase : uint8_t
	{
		Case0,
		Case1,
		CaseN,
		Default0,
		Default1,
		DefaultN
	};

	enum class I32OpEncoding : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		GetGlo,
		SetLoc,
		SetGlo,
		SLoad8,
		SLoadOff8,
		ULoad8,
		ULoadOff8,
		SLoad16,
		SLoadOff16,
		ULoad16,
		ULoadOff16,
		Load32,
		LoadOff32,
		Store8,
		StoreOff8,
		Store16,
		StoreOff16,
		Store32,
		StoreOff32,
		CallInt,
		CallInd,
		CallImp,
		Cond,
		Comma,
		FromF32,
		FromF64,
		Neg,
		Add,
		Sub,
		Mul,
		SDiv,
		UDiv,
		SMod,
		UMod,
		BitNot,
		BitOr,
		BitAnd,
		BitXor,
		Lsh,
		ArithRsh,
		LogicRsh,
		Clz,
		LogicNot,
		EqI32,
		EqF32,
		EqF64,
		NEqI32,
		NEqF32,
		NEqF64,
		SLeThI32,
		ULeThI32,
		LeThF32,
		LeThF64,
		SLeEqI32,
		ULeEqI32,
		LeEqF32,
		LeEqF64,
		SGrThI32,
		UGrThI32,
		GrThF32,
		GrThF64,
		SGrEqI32,
		UGrEqI32,
		GrEqF32,
		GrEqF64,
		SMin,
		UMin,
		SMax,
		UMax,
		Abs,

		Bad
	};

	enum class I32OpEncodingWithImm : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		Reserved,

		Bad
	};

	enum class F32OpEncoding : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		GetGlo,
		SetLoc,
		SetGlo,
		Load,
		LoadOff,
		Store,
		StoreOff,
		CallInt,
		CallInd,
		Cond,
		Comma,
		FromS32,
		FromU32,
		FromF64,
		Neg,
		Add,
		Sub,
		Mul,
		Div,
		Abs,
		Ceil,
		Floor,
		Sqrt,

		Bad
	};

	enum class F32OpEncodingWithImm : uint8_t
	{
		LitPool,
		GetLoc,
		Reserved0,
		Reserved1,

		Bad
	};

	enum class F64OpEncoding : uint8_t
	{
		LitPool,
		LitImm,
		GetLoc,
		GetGlo,
		SetLoc,
		SetGlo,
		Load,
		LoadOff,
		Store,
		StoreOff,
		CallInt,
		CallInd,
		CallImp,
		Cond,
		Comma,
		FromS32,
		FromU32,
		FromF32,
		Neg,
		Add,
		Sub,
		Mul,
		Div,
		Mod,
		Min,
		Max,
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
		ATan2,
		Exp,
		Ln,
		Pow,

		Bad
	};

	enum class F64OpEncodingWithImm : uint8_t
	{
		LitPool,
		GetLoc,
		Reserved0,
		Reserved1,

		Bad
	};

	enum class VoidOpEncoding : uint8_t
	{
		CallInt,
		CallInd,
		CallImp,

		Bad
	};

	enum class Type : uint8_t
	{
		I32,
		F32,
		F64
	};

	enum class VaReturnTypes : uint8_t
	{
		I32 = 0x1,
		F32 = 0x2,
		F64 = 0x4,
	};

	inline VaReturnTypes operator|(VaReturnTypes lhs,VaReturnTypes rhs) { return VaReturnTypes(uint8_t(lhs) | uint8_t(rhs)); }
	inline bool operator&(VaReturnTypes lhs,VaReturnTypes rhs) { return bool((uint8_t(lhs) & uint8_t(rhs)) != 0); }

	enum class VaReturnTypesWithImm : uint8_t
	{
		OnlyI32,
		Reserved0,
		Reserved1,
		Reserved2
	};

	enum class ReturnType : uint8_t
	{
		I32 = uint8_t(Type::I32),
		F32 = uint8_t(Type::F32),
		F64 = uint8_t(Type::F64),
		Void
	};

	static const uint8_t HasImmFlag = 0x80;
	static_assert(uint8_t(I32OpEncoding::Bad) <= HasImmFlag,"MSB reserved to distinguish I32 from I32WithImm");
	static_assert(uint8_t(F32OpEncoding::Bad) <= HasImmFlag,"MSB reserved to distinguish F32 from F32WithImm");
	static_assert(uint8_t(F64OpEncoding::Bad) <= HasImmFlag,"MSB reserved to distinguish F64 from F64WithImm");

	static const unsigned OpWithImmBits = 2;
	static const uint32_t OpWithImmLimit = 1 << OpWithImmBits;
	static_assert(uint8_t(I32OpEncodingWithImm::Bad) <= OpWithImmLimit,"I32WithImm op fits");
	static_assert(uint8_t(F32OpEncodingWithImm::Bad) <= OpWithImmLimit,"F32WithImm op fits");
	static_assert(uint8_t(F64OpEncodingWithImm::Bad) <= OpWithImmLimit,"F64WithImm op fits");

	static const unsigned ImmBits = 5;
	static const uint32_t ImmLimit = 1 << ImmBits;
	static_assert(1 + OpWithImmBits + ImmBits == 8,"Bits of immediate op should add up to a byte");

	static inline uint8_t PackOpWithImm(uint8_t op,uint8_t imm)
	{
		assert(op < OpWithImmLimit);
		assert(imm < ImmLimit);
		return HasImmFlag | (uint8_t(op) << ImmBits) | imm;
	}

	template <class TWithImm>
	static inline void UnpackOpWithImm(uint8_t byte,TWithImm* op,uint8_t *imm)
	{
		assert(byte & HasImmFlag);
		*op = TWithImm((byte >> ImmBits) & (OpWithImmLimit - 1));
		*imm = byte & (ImmLimit - 1);
	}

	enum class ExportFormat : uint8_t
	{
		Default,
		Record
	};

	struct FatalDecodeException : public ErrorRecord
	{
		FatalDecodeException(std::string&& inMessage) : ErrorRecord(std::move(inMessage)) {}
	};

	struct InputStream
	{
		InputStream(const uint8_t* inStart,const uint8_t* inEnd) : cur(inStart), end(inEnd) {}

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
		StmtOpEncoding stmt() { return u8<StmtOpEncoding>(); }
		SwitchCase switch_case() { return u8<SwitchCase>(); }
		template<class T,class TWithImm> inline bool code(T*,TWithImm*,uint8_t*);
		VoidOpEncoding void_expr() { return u8<VoidOpEncoding>(); }
		ExportFormat export_format() { return u8<ExportFormat>(); }
		TypeId type()
		{
			switch(u8<Type>())
			{
			case Type::I32: return TypeId::I32;
			case Type::F32: return TypeId::F32;
			case Type::F64: return TypeId::F64;
			default: throw new FatalDecodeException("invalid type");
			}
		}

		TypeId returnType()
		{
			switch(u8<ReturnType>())
			{
			case ReturnType::I32: return TypeId::I32;
			case ReturnType::F32: return TypeId::F32;
			case ReturnType::F64: return TypeId::F64;
			case ReturnType::Void: return TypeId::Void;
			default: throw new FatalDecodeException("invalid return type");
			}
		}
		inline uint32_t immU32();
		inline uint32_t boundedImmU32(const char* context,size_t maxValue)
		{
			auto result = immU32();
			if(result >= maxValue) { throw new FatalDecodeException(std::string("invalid ") + context); }
			return result;
		}
		inline int32_t immS32();
		char single_char() { return *cur++; }
		bool complete() const { return cur == end; }
	private:
		const uint8_t* cur;
		const uint8_t* end;

		template <class T> T u8()
		{
			if(cur >= end) { throw new FatalDecodeException("expected more input than available"); }
			return T(*cur++);
		}
	};

	template <class T,class TWithImm>
	bool InputStream::code(T* t,TWithImm* tWithImm,uint8_t* imm)
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

	uint32_t inline InputStream::immU32()
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

	int32_t inline InputStream::immS32()
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

	struct DecodeContext
	{
		InputStream& in;

		// Information about the mobule being decoded.
		Module& module;
		std::vector<ErrorRecord*>& outErrors;
		Memory::Arena& arena;

		std::vector<uint32_t> i32Constants;
		std::vector<float> f32Constants;
		std::vector<double> f64Constants;
		std::vector<FunctionType> functionTypes;

		// Information about the current operation being decoded.
		std::vector<BranchTarget*> explicitBreakTargets;
		std::vector<BranchTarget*> implicitBreakTargets;
		std::vector<BranchTarget*> explicitContinueTargets;
		std::vector<BranchTarget*> implicitContinueTargets;
		Function* currentFunction;
		intptr_t i32TempLocalIndex;
		intptr_t f32TempLocalIndex;
		intptr_t f64TempLocalIndex;

		DecodeContext(InputStream& inIn,Module& inModule,std::vector<ErrorRecord*>& inOutErrors)
		:	in(inIn)
		,	module(inModule)
		,	outErrors(inOutErrors)
		,	arena(inModule.arena)
		,	i32TempLocalIndex(-1)
		,	f32TempLocalIndex(-1)
		,	f64TempLocalIndex(-1)
		{}

		template<typename Class>
		Error<Class>* recordError(const char* message)
		{
			auto error = new(arena) Error<Class>(message);
			outErrors.push_back(error);
			return error;
		}

		template<typename Type>
		intptr_t getTempLocalIndex();

		template<>
		intptr_t getTempLocalIndex<I32Type>()
		{
			if(i32TempLocalIndex == -1) { i32TempLocalIndex = currentFunction->locals.size(); currentFunction->locals.push_back({TypeId::I32,nullptr}); }
			return i32TempLocalIndex;
		}
		
		template<>
		intptr_t getTempLocalIndex<F32Type>()
		{
			if(f32TempLocalIndex == -1) { f32TempLocalIndex = currentFunction->locals.size(); currentFunction->locals.push_back({TypeId::F32,nullptr}); }
			return f32TempLocalIndex;
		}
		
		template<>
		intptr_t getTempLocalIndex<F64Type>()
		{
			if(f64TempLocalIndex == -1) { f64TempLocalIndex = currentFunction->locals.size(); currentFunction->locals.push_back({TypeId::F64,nullptr}); }
			return f64TempLocalIndex;
		}

		// Decodes an expression based on the type of its result.
		template<typename Type> typename Type::Expression* decodeExpression();
		template<> typename I32Type::Expression* decodeExpression<I32Type>() { return decodeExpressionI32(); }
		template<> typename F32Type::Expression* decodeExpression<F32Type>() { return decodeExpressionF32(); }
		template<> typename F64Type::Expression* decodeExpression<F64Type>() { return decodeExpressionF64(); }
		template<> typename VoidType::Expression* decodeExpression<VoidType>() { return decodeExpressionVoid(); }

		UntypedExpression* decodeExpression(TypeId type)
		{
			switch(type)
			{
			case TypeId::I32: return decodeExpressionI32();
			case TypeId::F32: return decodeExpressionF32();
			case TypeId::F64: return decodeExpressionF64();
			case TypeId::Void: return decodeExpressionVoid();
			default: throw;
			}
		}
		
		// Loads a value from a local variable.
		template<typename Type>
		typename Type::Expression* getLocal(uint32_t localIndex)
		{
			if(localIndex >= currentFunction->locals.size()) { return recordError<Type::Class>("getlocal: invalid local index"); }
			if(currentFunction->locals[localIndex].type != Type::id) { return recordError<Type::Class>("getlocal: incorrect type"); }
			return new(arena) LoadVariable<Type::Class>(Type::Op::getLocal,localIndex);
		}

		// Stores a value to a local variable.
		template<typename Type>
		typename Type::Expression* setLocalExpression(uint32_t localIndex)
		{
			auto value = decodeExpression<Type>();
			if(localIndex >= currentFunction->locals.size()) { return recordError<Type::Class>("setlocal: invalid local index"); }
			if(currentFunction->locals[localIndex].type != Type::id) { return recordError<Type::Class>("setlocal: incorrect type"); }
			auto store = new(arena) StoreVariable(VoidOp::setLocal,TypedExpression(value,Type::id),localIndex);
			auto load = new(arena) LoadVariable<Type::Class>(Type::Op::getLocal,localIndex);
			auto voidExpressions = new(arena) Expression<VoidClass>*[1] {store};
			return new(arena) Block<typename Type::Class>(voidExpressions,1,load);
		}
		VoidExpression* setLocal(uint32_t localIndex)
		{
			if(localIndex >= currentFunction->locals.size()) { throw new FatalDecodeException("setlocal: invalid local index"); }
			auto type = currentFunction->locals[localIndex].type;
			auto value = decodeExpression(type);
			return new(arena) StoreVariable(VoidOp::setLocal,TypedExpression(value,type),localIndex);
		}
		
		// Loads a value from a global variable.
		template<typename Type>
		typename Type::Expression* getGlobal(uint32_t globalIndex)
		{
			if(globalIndex >= module.globals.size()) { return recordError<Type::Class>("getglobal: invalid global index"); }
			if(module.globals[globalIndex].type != Type::id) { return recordError<Type::Class>("getglobal: incorrect type"); }
			return new(arena) LoadVariable<Type::Class>(Type::Op::loadGlobal,globalIndex);
		}

		// Stores a value to a global variable.
		template<typename Type>
		typename Type::Expression* setGlobalExpression(uint32_t globalIndex)
		{
			auto value = decodeExpression<Type>();
			if(globalIndex >= module.globals.size()) { return recordError<Type::Class>("setglobal: invalid global index"); }
			if(module.globals[globalIndex].type != Type::id) { return recordError<Type::Class>("setglobal: incorrect type"); }
			auto store = new(arena) StoreVariable(VoidOp::storeGlobal,TypedExpression(value,Type::id),globalIndex);
			auto load = new(arena) LoadVariable<Type::Class>(Type::Op::loadGlobal,globalIndex);
			auto voidExpressions = new(arena) Expression<VoidClass>*[1] {store};
			return new(arena) Block<typename Type::Class>(voidExpressions,1,load);
		}
		VoidExpression* setGlobal(uint32_t globalIndex)
		{
			if(globalIndex >= module.globals.size()) { throw new FatalDecodeException("setglobal: invalid global index"); }
			auto type = module.globals[globalIndex].type;
			auto value = decodeExpression(type);
			return new(arena) StoreVariable(VoidOp::storeGlobal,TypedExpression(value,type),globalIndex);
		}

		// Decodes an address in the form of an offset into the module's linear memory.
		IntExpression* decodeAddress(uint32_t offset)
		{
			IntExpression* byteIndex = decodeExpression<I32Type>();

			// Add the offset provided by the operation.
			if(offset != 0)
			{ byteIndex = new(arena) Binary<IntClass>(IntOp::add,byteIndex,new(arena) Literal<I32Type>(offset)); }

			return byteIndex;
		}

		// Loads an I8, I16, or I32 into an I32 intermediate. I8 and I16 is either zero or sign extended to 32-bit depending on isSigned.
		template<typename Type>
		typename Type::Expression* loadMemory(TypeId memoryType,typename Type::Op castOp,IntExpression* address)
		{
			auto memoryValue = new(arena) LoadMemory<Type::Class>(false,true,address);
			return memoryType == Type::id ? as<Type::Class>(memoryValue)
				: new(arena) Cast<Type::Class>(castOp,TypedExpression(memoryValue,memoryType));
		}

		// Stores a value to memory.
		template<typename Type>
		typename Type::Expression* storeMemoryExpression(TypeId memoryType,typename Type::Op castOp,IntExpression* address)
		{
			auto value = decodeExpression<Type>();
			auto tempLocalIndex = getTempLocalIndex<Type>();
			auto tempStore = new(arena) StoreVariable(VoidOp::setLocal,TypedExpression(value,Type::id),tempLocalIndex);
			auto tempLoad1 = new(arena) LoadVariable<Type::Class>(Type::Op::getLocal,tempLocalIndex);
			auto castedValue = memoryType == Type::id ? as<Type::Class>(tempLoad1)
				: new(arena) Cast<Type::Class>(castOp,TypedExpression(tempLoad1,Type::id));
			auto store = new(arena) StoreMemory(false,true,address,TypedExpression(castedValue,memoryType));
			auto voidExpressions = new(arena) Expression<VoidClass>*[2] {tempStore,store};
			auto tempLoad2 = new(arena) LoadVariable<Type::Class>(Type::Op::getLocal,tempLocalIndex);
			return new(arena) Block<typename Type::Class>(voidExpressions,2,tempLoad2);
		}
		template<typename Type>
		VoidExpression* storeMemory(TypeId memoryType,typename Type::Op castOp,IntExpression* address)
		{
			auto value = decodeExpression<Type>();
			auto castedValue = memoryType == Type::id ? value
				: new(arena) Cast<Type::Class>(castOp,TypedExpression(value,Type::id));
			return new(arena) StoreMemory(false,true,address,TypedExpression(castedValue,memoryType));
		}

		// Converts a signed or unsigned 32-bit integer to a float.
		FloatExpression* castI32ToFloat(FloatOp op)
		{
			return new(arena) Cast<FloatClass>(op,TypedExpression(decodeExpression<I32Type>(),TypeId::I32));
		}

		// Applies an unary operation to an integer operand.
		template<typename Type>
		typename Type::Expression* decodeUnary(typename Type::Op op)
		{
			auto operand = decodeExpression<Type>();
			return new(arena) Unary<typename Type::Class>(op,operand);
		}

		// Applies a binary operation to an integer operand.
		template<typename Type>
		typename Type::Expression* decodeBinary(typename Type::Op op)
		{
			auto leftOperand = decodeExpression<Type>();
			auto rightOperand = decodeExpression<Type>();
			return new(arena) Binary<typename Type::Class>(op,leftOperand,rightOperand);
		}

		// Compares two integers and returns an I32 value.
		template<typename OperandType>
		IntExpression* decodeCompare(BoolOp op)
		{
			auto leftOperand = decodeExpression<OperandType>();
			auto rightOperand = decodeExpression<OperandType>();
			return new(arena) Cast<IntClass>(IntOp::reinterpretBool,TypedExpression(new(arena) Comparison(op,OperandType::id,leftOperand,rightOperand),TypeId::Bool));
		}

		// Evaluates two expressions, and returns only the result of the second.
		template<typename Type>
		typename Type::Expression* comma()
		{
			auto voidExpression = new(arena) Expression<VoidClass>*[1];
			switch(in.returnType())
			{
			case TypeId::I32: *voidExpression = new(arena) DiscardResult(TypedExpression(decodeExpression<I32Type>(),TypeId::I32)); break;
			case TypeId::F32: *voidExpression = new(arena) DiscardResult(TypedExpression(decodeExpression<F32Type>(),TypeId::F32)); break;
			case TypeId::F64: *voidExpression = new(arena) DiscardResult(TypedExpression(decodeExpression<F64Type>(),TypeId::F64)); break;
			case TypeId::Void: *voidExpression = decodeExpression<VoidType>(); break;
			default: throw;
			}
			auto value = decodeExpression<Type>();
			return new(arena) Block<typename Type::Class>(voidExpression,1,value);
		}

		BoolExpression* decodeExpressionI32AsBool()
		{
			auto i32Value = decodeExpression<I32Type>();
			if(i32Value->op() == IntOp::reinterpretBool && ((Cast<IntClass>*)i32Value)->source.type == TypeId::Bool)
				{ return as<BoolClass>(((Cast<IntClass>*)i32Value)->source); }
			else { return new(arena) Comparison(BoolOp::neq,TypeId::I32,i32Value,new(arena) Literal<I32Type>(0)); }
		}

		// Chooses between two values based on a predicate value.
		template<typename Type>
		typename Type::Expression* cond()
		{
			auto condition = decodeExpressionI32AsBool();
			auto trueValue = decodeExpression<Type>();
			auto falseValue = decodeExpression<Type>();
			return new(arena) IfElse<typename Type::Class>(condition,trueValue,falseValue);
		}

		// Decodes the parameters for a function.
		UntypedExpression** decodeParameters(const std::vector<TypeId>& parameterTypes)
		{
			auto parameters = new(arena) UntypedExpression*[parameterTypes.size()];
			for(uintptr_t parameterIndex = 0;parameterIndex < parameterTypes.size();++parameterIndex)
			{ parameters[parameterIndex] = decodeExpression(parameterTypes[parameterIndex]); }
			return parameters;
		}

		// Calls a function by a direct pointer.
		template<typename Class>
		typename Class::Expression* callInternal(TypeId returnType,uint32_t functionIndex)
		{
			if(functionIndex >= module.functions.size()) { throw new FatalDecodeException("callinternal: invalid function index"); }
			auto function = module.functions[functionIndex];
			auto parameters = decodeParameters(function->type.parameters);
			return function->type.returnType == returnType
				? as<Class>(new(arena) Call<Class>(Class::Op::callDirect,functionIndex,parameters))
				: recordError<Class>("callinternal: incorrect type");
		}
		VoidExpression* callInternalStatement(uint32_t functionIndex)
		{
			if(functionIndex >= module.functions.size()) { throw new FatalDecodeException("callinternal: invalid function index"); }
			auto function = module.functions[functionIndex];
			auto returnType = function->type.returnType;
			switch(function->type.returnType)
			{
			case TypeId::I32: return new(arena) DiscardResult(TypedExpression(callInternal<IntClass>(TypeId::I32,functionIndex),returnType));
			case TypeId::F32: return new(arena) DiscardResult(TypedExpression(callInternal<FloatClass>(TypeId::F32,functionIndex),returnType));
			case TypeId::F64: return new(arena) DiscardResult(TypedExpression(callInternal<FloatClass>(TypeId::F64,functionIndex),returnType));
			case TypeId::Void: return callInternal<VoidClass>(TypeId::Void,functionIndex);
			default: throw;
			};
		}

		// Calls a function by index into a function pointer table.
		template<typename Class>
		typename Class::Expression* callIndirect(TypeId returnType,uint32_t tableIndex)
		{
			if(tableIndex >= module.functions.size()) { throw new FatalDecodeException("callindirect: invalid table index"); }
			const FunctionTable& functionTable = module.functionTables[tableIndex];
			auto functionIndex = decodeExpression<I32Type>();
			auto parameters = decodeParameters(functionTable.type.parameters);
			return functionTable.type.returnType == returnType
				? as<Class>(new(arena) CallIndirect<Class>(Class::Op::callIndirect,tableIndex,functionIndex,parameters))
				: recordError<Class>("callindirect: incorrect type");
		}
		VoidExpression* callIndirectStatement(uint32_t tableIndex)
		{
			if(tableIndex >= module.functions.size()) { throw new FatalDecodeException("callindirect: invalid table index"); }
			const FunctionTable& functionTable = module.functionTables[tableIndex];
			auto returnType = functionTable.type.returnType;
			switch(functionTable.type.returnType)
			{
			case TypeId::I32: return new(arena) DiscardResult(TypedExpression(callIndirect<IntClass>(TypeId::I32,tableIndex),returnType));
			case TypeId::F32: return new(arena) DiscardResult(TypedExpression(callIndirect<FloatClass>(TypeId::F32,tableIndex),returnType));
			case TypeId::F64: return new(arena) DiscardResult(TypedExpression(callIndirect<FloatClass>(TypeId::F64,tableIndex),returnType));
			case TypeId::Void: return callIndirect<VoidClass>(TypeId::Void,tableIndex);
			default: throw;
			};
		}

		// Calls a function by index into the module's import table.
		template<typename Class>
		typename Class::Expression* callImport(TypeId returnType,uint32_t functionImportIndex)
		{
			if(functionImportIndex >= module.functionImports.size()) { throw new FatalDecodeException("callimport: invalid import index"); }
			const FunctionImport& functionImport = module.functionImports[functionImportIndex];
			UntypedExpression** parameters = decodeParameters(functionImport.type.parameters);
			return functionImport.type.returnType == returnType
				? as<Class>(new(arena) Call<Class>(Class::Op::callImport,functionImportIndex,parameters))
				: recordError<Class>("callimport: incorrect type");
		}
		VoidExpression* callImportStatement(uint32_t functionImportIndex)
		{
			if(functionImportIndex >= module.functionImports.size()) { throw new FatalDecodeException("callimport: invalid import index"); }
			const FunctionImport& functionImport = module.functionImports[functionImportIndex];
			auto returnType = functionImport.type.returnType;
			switch(functionImport.type.returnType)
			{
			case TypeId::I32: return new(arena) DiscardResult(TypedExpression(callImport<IntClass>(TypeId::I32,functionImportIndex),returnType));
			case TypeId::F32: return new(arena) DiscardResult(TypedExpression(callImport<FloatClass>(TypeId::F32,functionImportIndex),returnType));
			case TypeId::F64: return new(arena) DiscardResult(TypedExpression(callImport<FloatClass>(TypeId::F64,functionImportIndex),returnType));
			case TypeId::Void: return callImport<VoidClass>(TypeId::Void,functionImportIndex);
			default: throw;
			};
		}

		// Computes the minimum or maximum of a set of F64s.
		template<typename Type>
		typename Type::Expression* minMax(typename Type::Op op)
		{
			auto numParameters = in.immU32();
			if(numParameters == 0) { return recordError<Type::Class>("minmax: must receive >0 parameters"); }

			Memory::ScopedArena scopedArena;
			Type::Expression** parameters = new(scopedArena) Type::Expression*[numParameters];
			for(uint32_t parameterIndex = 0;parameterIndex < numParameters;++parameterIndex)
			{ parameters[parameterIndex] = decodeExpression<Type>(); }

			typename Type::Expression* result = parameters[0];
			for(uint32_t parameterIndex = 1;parameterIndex < numParameters;++parameterIndex)
			{ result = new(arena) Binary<Type::Class>(op,result,parameters[parameterIndex]); }
			return result;
		}

		VoidExpression* decodeBlock()
		{
			return decodeStatementList();
		}

		VoidExpression* decodeIf()
		{
			auto condition = decodeExpressionI32AsBool();
			auto thenExpression = decodeStatement();
			return new(arena) IfElse<VoidClass>(condition,thenExpression,new(arena) Nop());
		}

		VoidExpression* decodeIfElse()
		{
			auto condition = decodeExpressionI32AsBool();
			auto thenExpression = decodeStatement();
			auto elseExpression = decodeStatement();
			return new(arena) IfElse<VoidClass>(condition,thenExpression,elseExpression);
		}

		VoidExpression* decodeWhile(bool isEnclosedByLabel)
		{
			auto condition = decodeExpressionI32AsBool();
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			auto continueBranchTarget = new(arena) BranchTarget(TypeId::Void);
			implicitBreakTargets.push_back(breakBranchTarget);
			implicitContinueTargets.push_back(continueBranchTarget);
			// If the loop is immediately inside a label node, then it needs to create an explicit continue target.
			if(isEnclosedByLabel) { explicitContinueTargets.push_back(continueBranchTarget); }
			auto loopExpression = decodeStatement();
			implicitBreakTargets.pop_back();
			implicitContinueTargets.pop_back();
			if(isEnclosedByLabel) { explicitContinueTargets.pop_back(); }
			auto loopBody = new(arena) IfElse<VoidClass>(condition,loopExpression,new(arena) Branch<VoidClass>(breakBranchTarget,nullptr));
			return new(arena) Loop<VoidClass>(loopBody,breakBranchTarget,continueBranchTarget);
		}

		VoidExpression* decodeDo(bool isEnclosedByLabel)
		{
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			auto continueBranchTarget = new(arena) BranchTarget(TypeId::Void);
			implicitBreakTargets.push_back(breakBranchTarget);
			implicitContinueTargets.push_back(continueBranchTarget);
			// If the loop is immediately inside a label node, then it needs to create an explicit continue target.
			if(isEnclosedByLabel) { explicitContinueTargets.push_back(continueBranchTarget); }
			auto loopExpression = decodeStatement();
			auto condition = decodeExpressionI32AsBool();
			implicitBreakTargets.pop_back();
			implicitContinueTargets.pop_back();
			if(isEnclosedByLabel) { explicitContinueTargets.pop_back(); }
			auto loopBody = new(arena) Block<VoidClass>(
				new(arena) Expression<VoidClass>*[1] {loopExpression},
				1,
				new(arena) IfElse<VoidClass>(
					new(arena) Unary<BoolClass>(BoolOp::not,condition),
					new(arena) Branch<VoidClass>(breakBranchTarget,nullptr),
					new(arena) Nop()
					)
				);
			return new(arena) Loop<VoidClass>(loopBody,breakBranchTarget,continueBranchTarget);
		}

		VoidExpression* decodeLabel()
		{
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			explicitBreakTargets.push_back(breakBranchTarget);
			auto expression = decodeStatement(true);
			explicitBreakTargets.pop_back();
			return new(arena) Label<VoidClass>(breakBranchTarget,expression);
		}

		VoidExpression* decodeBreak()
		{
			if(!implicitBreakTargets.size()) { return recordError<VoidClass>("break: no implicit break label"); }
			return new(arena) Branch<VoidClass>(implicitBreakTargets.back(),nullptr);
		}

		VoidExpression* decodeContinue()
		{
			if(!implicitContinueTargets.size()) { return recordError<VoidClass>("continue: no implicit continue label"); }
			return new(arena) Branch<VoidClass>(implicitContinueTargets[implicitContinueTargets.size() - 1],nullptr);
		}

		VoidExpression* decodeBreakLabel(uint32_t labelIndex)
		{
			if(labelIndex >= explicitBreakTargets.size()) { return recordError<VoidClass>("break: invalid label index"); }
			return new(arena) Branch<VoidClass>(explicitBreakTargets[labelIndex],nullptr);
		}

		VoidExpression* decodeContinueLabel(uint32_t labelIndex)
		{
			if(labelIndex >= explicitContinueTargets.size()) { return recordError<VoidClass>("continue: invalid label index"); }
			return new(arena) Branch<VoidClass>(explicitContinueTargets[labelIndex],nullptr);
		}

		VoidExpression* decodeSwitch()
		{
			uint32_t numCases = in.immU32();
			auto value = decodeExpression<I32Type>();
			
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			implicitBreakTargets.push_back(breakBranchTarget);

			SwitchArm* arms = new(arena) SwitchArm[numCases];
			uint32_t defaultCaseIndex = numCases;
			size_t numDefaultCases = 0;
			for(uint32_t caseIndex = 0;caseIndex < numCases;++caseIndex)
			{
				auto caseType = in.switch_case();
				switch(caseType)
				{
				case SwitchCase::Case0:
				case SwitchCase::Case1:
				case SwitchCase::CaseN:
				{
					arms[caseIndex].key = in.immS32();
					break;
				}
				case SwitchCase::Default0:
				case SwitchCase::Default1:
				case SwitchCase::DefaultN:
					defaultCaseIndex = caseIndex;
					++numDefaultCases;
					arms[caseIndex].key = 0;
					break;
				default: throw new FatalDecodeException("invalid switch case type");
				};

				Expression<VoidClass>* caseValue;
				switch(caseType)
				{
				case SwitchCase::Case0:
				case SwitchCase::Default0:
					caseValue = nullptr;
					break;
				case SwitchCase::Case1:
				case SwitchCase::Default1:
					caseValue = decodeStatement();
					break;
				case SwitchCase::CaseN:
				case SwitchCase::DefaultN:
					caseValue = decodeStatementList();
					break;
				default: throw;
				};
				arms[caseIndex].value = caseValue;
			}
			
			implicitBreakTargets.pop_back();
			
			if(numDefaultCases > 1) { return recordError<VoidClass>("switch: must not have more than 1 default case"); }
			else if(!numDefaultCases)
			{
				arms = arena.reallocate(arms,numCases,numCases + 1);
				arms[numCases].key = 0;
				arms[numCases].value = new(arena) Nop();
				defaultCaseIndex = numCases++;
			}

			return new(arena) Switch<VoidClass>(TypedExpression(value,TypeId::I32),defaultCaseIndex,numCases,arms,breakBranchTarget);
		}
		
		// Decodes a return based on the current function's return type.
		VoidExpression* decodeReturn()
		{
			switch(currentFunction->type.returnType)
			{
			case TypeId::I32: return new(arena) Return<VoidClass>(decodeExpression<I32Type>());
			case TypeId::F32: return new(arena) Return<VoidClass>(decodeExpression<F32Type>());
			case TypeId::F64: return new(arena) Return<VoidClass>(decodeExpression<F64Type>());
			case TypeId::Void: return new(arena) Return<VoidClass>(nullptr);
			default: throw;
			}
		}
		
		// Decodes expression opcodes that return an I32.
		IntExpression* decodeExpressionI32()
		{
			I32OpEncoding i32;
			I32OpEncodingWithImm i32WithImm;
			uint8_t imm;
			if(in.code(&i32,&i32WithImm,&imm))
			{
				switch(i32)
				{
				case I32OpEncoding::LitImm:    return new(arena) Literal<I32Type>(in.immU32());
				case I32OpEncoding::LitPool:    return new(arena) Literal<I32Type>(i32Constants[in.boundedImmU32("I32 constant index",i32Constants.size())]);
				case I32OpEncoding::GetLoc:     return getLocal<I32Type>(in.immU32());
				case I32OpEncoding::GetGlo:     return getGlobal<I32Type>(in.immU32());
				case I32OpEncoding::SetLoc:     return setLocalExpression<I32Type>(in.immU32());
				case I32OpEncoding::SetGlo:     return setGlobalExpression<I32Type>(in.immU32());
				case I32OpEncoding::SLoad8:     return loadMemory<I32Type>(TypeId::I8,IntOp::sext,decodeAddress(0));
				case I32OpEncoding::SLoadOff8:  return loadMemory<I32Type>(TypeId::I8,IntOp::sext,decodeAddress(in.immU32()));
				case I32OpEncoding::ULoad8:     return loadMemory<I32Type>(TypeId::I8,IntOp::zext,decodeAddress(0));
				case I32OpEncoding::ULoadOff8:  return loadMemory<I32Type>(TypeId::I8,IntOp::zext,decodeAddress(in.immU32()));
				case I32OpEncoding::SLoad16:    return loadMemory<I32Type>(TypeId::I16,IntOp::sext,decodeAddress(0));
				case I32OpEncoding::SLoadOff16: return loadMemory<I32Type>(TypeId::I16,IntOp::sext,decodeAddress(in.immU32()));
				case I32OpEncoding::ULoad16:    return loadMemory<I32Type>(TypeId::I16,IntOp::zext,decodeAddress(0));
				case I32OpEncoding::ULoadOff16: return loadMemory<I32Type>(TypeId::I16,IntOp::zext,decodeAddress(in.immU32()));
				case I32OpEncoding::Load32:     return loadMemory<I32Type>(TypeId::I32,IntOp::zext,decodeAddress(0));
				case I32OpEncoding::LoadOff32:  return loadMemory<I32Type>(TypeId::I32,IntOp::zext,decodeAddress(in.immU32()));
				case I32OpEncoding::Store8:     return storeMemoryExpression<I32Type>(TypeId::I8,IntOp::wrap,decodeAddress(0));
				case I32OpEncoding::StoreOff8:  return storeMemoryExpression<I32Type>(TypeId::I8,IntOp::wrap,decodeAddress(in.immU32()));
				case I32OpEncoding::Store16:    return storeMemoryExpression<I32Type>(TypeId::I16,IntOp::wrap,decodeAddress(0));
				case I32OpEncoding::StoreOff16: return storeMemoryExpression<I32Type>(TypeId::I16,IntOp::wrap,decodeAddress(in.immU32()));
				case I32OpEncoding::Store32:    return storeMemoryExpression<I32Type>(TypeId::I32,IntOp::wrap,decodeAddress(0));
				case I32OpEncoding::StoreOff32: return storeMemoryExpression<I32Type>(TypeId::I32,IntOp::wrap,decodeAddress(in.immU32()));
				case I32OpEncoding::CallInt:    return callInternal<IntClass>(TypeId::I32,in.immU32());
				case I32OpEncoding::CallInd:    return callIndirect<IntClass>(TypeId::I32,in.immU32());
				case I32OpEncoding::CallImp:    return callImport<IntClass>(TypeId::I32,in.immU32());
				case I32OpEncoding::Cond:       return cond<I32Type>();
				case I32OpEncoding::Comma:      return comma<I32Type>();
				case I32OpEncoding::FromF32:    return new(arena) Cast<IntClass>(IntOp::truncSignedFloat,TypedExpression(decodeExpression<F32Type>(),TypeId::F32));
				case I32OpEncoding::FromF64:    return new(arena) Cast<IntClass>(IntOp::truncSignedFloat,TypedExpression(decodeExpression<F64Type>(),TypeId::F64));
				case I32OpEncoding::Neg:        return decodeUnary<I32Type>(IntOp::neg);
				case I32OpEncoding::Add:        return decodeBinary<I32Type>(IntOp::add);
				case I32OpEncoding::Sub:        return decodeBinary<I32Type>(IntOp::sub);
				case I32OpEncoding::Mul:        return decodeBinary<I32Type>(IntOp::mul);
				case I32OpEncoding::SDiv:       return decodeBinary<I32Type>(IntOp::divs);
				case I32OpEncoding::UDiv:       return decodeBinary<I32Type>(IntOp::divu);
				case I32OpEncoding::SMod:       return decodeBinary<I32Type>(IntOp::rems);
				case I32OpEncoding::UMod:       return decodeBinary<I32Type>(IntOp::remu);
				case I32OpEncoding::BitNot:     return decodeUnary<I32Type>(IntOp::not);
				case I32OpEncoding::BitOr:      return decodeBinary<I32Type>(IntOp::or);
				case I32OpEncoding::BitAnd:     return decodeBinary<I32Type>(IntOp::and);
				case I32OpEncoding::BitXor:     return decodeBinary<I32Type>(IntOp::xor);
				case I32OpEncoding::Lsh:        return decodeBinary<I32Type>(IntOp::shl);
				case I32OpEncoding::ArithRsh:   return decodeBinary<I32Type>(IntOp::sar);
				case I32OpEncoding::LogicRsh:   return decodeBinary<I32Type>(IntOp::shr);
				case I32OpEncoding::Clz:        return decodeUnary<I32Type>(IntOp::clz);
				case I32OpEncoding::LogicNot: 
				{
					auto comparison = new(arena) Comparison(BoolOp::eq,TypeId::I32,decodeExpression<I32Type>(),new(arena) Literal<I32Type>(0));
					return new(arena) Cast<IntClass>(IntOp::reinterpretBool,TypedExpression(comparison,TypeId::Bool));
				}
				case I32OpEncoding::EqI32:      return decodeCompare<I32Type>(BoolOp::eq);
				case I32OpEncoding::EqF32:      return decodeCompare<F32Type>(BoolOp::eq);
				case I32OpEncoding::EqF64:      return decodeCompare<F64Type>(BoolOp::eq);
				case I32OpEncoding::NEqI32:     return decodeCompare<I32Type>(BoolOp::neq);
				case I32OpEncoding::NEqF32:     return decodeCompare<F32Type>(BoolOp::neq);
				case I32OpEncoding::NEqF64:     return decodeCompare<F64Type>(BoolOp::neq);
				case I32OpEncoding::SLeThI32:   return decodeCompare<I32Type>(BoolOp::lts);
				case I32OpEncoding::ULeThI32:   return decodeCompare<I32Type>(BoolOp::ltu);
				case I32OpEncoding::LeThF32:    return decodeCompare<F32Type>(BoolOp::lt);
				case I32OpEncoding::LeThF64:    return decodeCompare<F64Type>(BoolOp::lt);
				case I32OpEncoding::SLeEqI32:   return decodeCompare<I32Type>(BoolOp::les);
				case I32OpEncoding::ULeEqI32:   return decodeCompare<I32Type>(BoolOp::leu);
				case I32OpEncoding::LeEqF32:    return decodeCompare<F32Type>(BoolOp::le);
				case I32OpEncoding::LeEqF64:    return decodeCompare<F64Type>(BoolOp::le);
				case I32OpEncoding::SGrThI32:   return decodeCompare<I32Type>(BoolOp::gts);
				case I32OpEncoding::UGrThI32:   return decodeCompare<I32Type>(BoolOp::gtu);
				case I32OpEncoding::GrThF32:    return decodeCompare<F32Type>(BoolOp::gt);
				case I32OpEncoding::GrThF64:    return decodeCompare<F64Type>(BoolOp::gt);
				case I32OpEncoding::SGrEqI32:   return decodeCompare<I32Type>(BoolOp::ges);
				case I32OpEncoding::UGrEqI32:   return decodeCompare<I32Type>(BoolOp::geu);
				case I32OpEncoding::GrEqF32:    return decodeCompare<F32Type>(BoolOp::ge);
				case I32OpEncoding::GrEqF64:    return decodeCompare<F64Type>(BoolOp::ge);
				case I32OpEncoding::SMin:       return minMax<I32Type>(IntOp::mins);
				case I32OpEncoding::UMin:       return minMax<I32Type>(IntOp::minu);
				case I32OpEncoding::SMax:       return minMax<I32Type>(IntOp::maxs);
				case I32OpEncoding::UMax:       return minMax<I32Type>(IntOp::maxu);
				case I32OpEncoding::Abs:        return decodeUnary<I32Type>(IntOp::abs);
				default: throw new FatalDecodeException("invalid I32 opcode");
				}
			}
			else
			{
				switch(i32WithImm)
				{
				case I32OpEncodingWithImm::LitImm:
					if(imm >= i32Constants.size()) { throw new FatalDecodeException("invalid I32 constant index"); }
					return new(arena) Literal<I32Type>(imm);
				case I32OpEncodingWithImm::LitPool: return new(arena) Literal<I32Type>(i32Constants[imm]);
				case I32OpEncodingWithImm::GetLoc: return getLocal<I32Type>(imm);
				default: throw new FatalDecodeException("invalid I32 opcode");
				}
			}
		}

		// Decodes an expression opcodes that return a F32.
		FloatExpression* decodeExpressionF32()
		{
			F32OpEncoding f32;
			F32OpEncodingWithImm f32WithImm;
			uint8_t imm;
			if(in.code(&f32,&f32WithImm,&imm))
			{
				switch(f32)
				{
				case F32OpEncoding::LitImm:   return new(arena) Literal<F32Type>(in.f32());
				case F32OpEncoding::LitPool:  return new(arena) Literal<F32Type>(f32Constants[in.boundedImmU32("F32 constant index",f32Constants.size())]);
				case F32OpEncoding::GetLoc:   return getLocal<F32Type>(in.immU32());
				case F32OpEncoding::GetGlo:   return getGlobal<F32Type>(in.immU32());
				case F32OpEncoding::SetLoc:   return setLocalExpression<F32Type>(in.immU32());
				case F32OpEncoding::SetGlo:   return setGlobalExpression<F32Type>(in.immU32());
				case F32OpEncoding::Load:     return loadMemory<F32Type>(TypeId::F32,FloatOp::demote,decodeAddress(0));
				case F32OpEncoding::LoadOff:  return loadMemory<F32Type>(TypeId::F32,FloatOp::demote,decodeAddress(in.immU32()));
				case F32OpEncoding::Store:    return storeMemoryExpression<F32Type>(TypeId::F32,FloatOp::demote,decodeAddress(0));
				case F32OpEncoding::StoreOff: return storeMemoryExpression<F32Type>(TypeId::F32,FloatOp::demote,decodeAddress(in.immU32()));
				case F32OpEncoding::CallInt:  return callInternal<FloatClass>(TypeId::F32,in.immU32());
				case F32OpEncoding::CallInd:  return callIndirect<FloatClass>(TypeId::F32,in.immU32());
				case F32OpEncoding::Cond:     return cond<F32Type>();
				case F32OpEncoding::Comma:    return comma<F32Type>();
				case F32OpEncoding::FromS32:  return castI32ToFloat(FloatOp::convertSignedInt);
				case F32OpEncoding::FromU32:  return castI32ToFloat(FloatOp::convertUnsignedInt);
				case F32OpEncoding::FromF64:  return new(arena) Cast<FloatClass>(FloatOp::demote,TypedExpression(decodeExpression<F64Type>(),TypeId::F64));
				case F32OpEncoding::Neg:      return decodeUnary<F32Type>(FloatOp::neg);
				case F32OpEncoding::Add:      return decodeBinary<F32Type>(FloatOp::add);
				case F32OpEncoding::Sub:      return decodeBinary<F32Type>(FloatOp::sub);
				case F32OpEncoding::Mul:      return decodeBinary<F32Type>(FloatOp::mul);
				case F32OpEncoding::Div:      return decodeBinary<F32Type>(FloatOp::div);
				case F32OpEncoding::Abs:      return decodeUnary<F32Type>(FloatOp::abs);
				case F32OpEncoding::Ceil:     return decodeUnary<F32Type>(FloatOp::ceil);
				case F32OpEncoding::Floor:    return decodeUnary<F32Type>(FloatOp::floor);
				case F32OpEncoding::Sqrt:     return decodeUnary<F32Type>(FloatOp::sqrt);
				default: throw new FatalDecodeException("invalid F32 opcode");
				}
			}
			else
			{
				switch(f32WithImm)
				{
				case F32OpEncodingWithImm::LitPool:
					if(imm >= f32Constants.size()) { throw new FatalDecodeException("invalid F32 constant index"); }
					return new(arena) Literal<F32Type>(f32Constants[imm]);
				case F32OpEncodingWithImm::GetLoc:  return getLocal<F32Type>(imm);
				default: throw new FatalDecodeException("invalid F32 opcode");
				}
			}
		}
		
		// Decodes an expression opcodes that return a F64.
		FloatExpression* decodeExpressionF64()
		{
			F64OpEncoding f64;
			F64OpEncodingWithImm f64WithImm;
			uint8_t imm;
			if(in.code(&f64,&f64WithImm,&imm))
			{
				switch(f64)
				{
				case F64OpEncoding::LitImm:   return new(arena) Literal<F64Type>(in.f64());
				case F64OpEncoding::LitPool:  return new(arena) Literal<F64Type>(f64Constants[in.boundedImmU32("F64 constant index",f64Constants.size())]);
				case F64OpEncoding::GetLoc:   return getLocal<F64Type>(in.immU32());
				case F64OpEncoding::GetGlo:   return getGlobal<F64Type>(in.immU32());
				case F64OpEncoding::SetLoc:   return setLocalExpression<F64Type>(in.immU32());
				case F64OpEncoding::SetGlo:   return setGlobalExpression<F64Type>(in.immU32());
				case F64OpEncoding::Load:     return loadMemory<F64Type>(TypeId::F64,FloatOp::demote,decodeAddress(0));
				case F64OpEncoding::LoadOff:  return loadMemory<F64Type>(TypeId::F64,FloatOp::demote,decodeAddress(in.immU32()));
				case F64OpEncoding::Store:    return storeMemoryExpression<F64Type>(TypeId::F64,FloatOp::demote,decodeAddress(0));
				case F64OpEncoding::StoreOff: return storeMemoryExpression<F64Type>(TypeId::F64,FloatOp::demote,decodeAddress(in.immU32()));
				case F64OpEncoding::CallInt:  return callInternal<FloatClass>(TypeId::F64,in.immU32());
				case F64OpEncoding::CallInd:  return callIndirect<FloatClass>(TypeId::F64,in.immU32());
				case F64OpEncoding::CallImp:  return callImport<FloatClass>(TypeId::F64,in.immU32());
				case F64OpEncoding::Cond:     return cond<F64Type>();
				case F64OpEncoding::Comma:    return comma<F64Type>();
				case F64OpEncoding::FromS32:  return castI32ToFloat(FloatOp::convertSignedInt);
				case F64OpEncoding::FromU32:  return castI32ToFloat(FloatOp::convertUnsignedInt);
				case F64OpEncoding::FromF32:  return new(arena) Cast<FloatClass>(FloatOp::promote,TypedExpression(decodeExpression<F32Type>(),TypeId::F32));
				case F64OpEncoding::Neg:      return decodeUnary<F64Type>(FloatOp::neg);
				case F64OpEncoding::Add:      return decodeBinary<F64Type>(FloatOp::add);
				case F64OpEncoding::Sub:      return decodeBinary<F64Type>(FloatOp::sub);
				case F64OpEncoding::Mul:      return decodeBinary<F64Type>(FloatOp::mul);
				case F64OpEncoding::Div:      return decodeBinary<F64Type>(FloatOp::div);
				case F64OpEncoding::Mod:      return decodeBinary<F64Type>(FloatOp::rem);
				case F64OpEncoding::Min:      return minMax<F64Type>(FloatOp::min);
				case F64OpEncoding::Max:      return minMax<F64Type>(FloatOp::max);
				case F64OpEncoding::Abs:      return decodeUnary<F64Type>(FloatOp::abs);
				case F64OpEncoding::Ceil:     return decodeUnary<F64Type>(FloatOp::ceil);
				case F64OpEncoding::Floor:    return decodeUnary<F64Type>(FloatOp::floor);
				case F64OpEncoding::Sqrt:     return decodeUnary<F64Type>(FloatOp::sqrt);
				case F64OpEncoding::Cos:     return decodeUnary<F64Type>(FloatOp::cos);
				case F64OpEncoding::Sin:      return decodeUnary<F64Type>(FloatOp::sin);
				case F64OpEncoding::Tan:      decodeExpression<F64Type>(); return recordError<FloatClass>("unsupported opcode tan");
				case F64OpEncoding::ACos:     decodeExpression<F64Type>(); return recordError<FloatClass>("unsupported opcode acos");
				case F64OpEncoding::ASin:     decodeExpression<F64Type>(); return recordError<FloatClass>("unsupported opcode asin");
				case F64OpEncoding::ATan:     decodeExpression<F64Type>(); return recordError<FloatClass>("unsupported opcode atan");
				case F64OpEncoding::ATan2:    decodeExpression<F64Type>(); decodeExpression<F64Type>(); return recordError<FloatClass>("unsupported opcode atan2");
				case F64OpEncoding::Exp:      return decodeUnary<F64Type>(FloatOp::exp);
				case F64OpEncoding::Ln:       return decodeUnary<F64Type>(FloatOp::log);
				case F64OpEncoding::Pow:      return decodeBinary<F64Type>(FloatOp::pow);
				default: throw new FatalDecodeException("invalid F64 opcode");
				}
			}
			else
			{
				switch(f64WithImm)
				{
				case F64OpEncodingWithImm::LitPool:
					if(imm >= f64Constants.size()) { throw new FatalDecodeException("invalid F64 constant index"); }
					return new(arena) Literal<F64Type>(f64Constants[imm]);
				case F64OpEncodingWithImm::GetLoc: return getLocal<F64Type>(imm);
				default: throw new FatalDecodeException("invalid F64 opcode");
				}
			}
		}

		// Decodes expression opcodes that return no value.
		VoidExpression* decodeExpressionVoid()
		{
			auto op = in.void_expr();
			switch(op)
			{
			case VoidOpEncoding::CallInt: return callInternalStatement(in.immU32());
			case VoidOpEncoding::CallInd: return callIndirectStatement(in.immU32());
			case VoidOpEncoding::CallImp: return callImportStatement(in.immU32());
			default: throw new FatalDecodeException("invalid void opcode");
			}
		}
		
		// Decodes statement opcodes.
		VoidExpression* decodeStatement(bool isEnclosedByLabel = false)
		{
			StmtOpEncoding statement;
			StmtOpEncodingWithImm statementWithImm;
			uint8_t imm;
			if(in.code(&statement,&statementWithImm,&imm))
			{
				switch(statement)
				{
				case StmtOpEncoding::SetLoc: return setLocal(in.immU32());
				case StmtOpEncoding::SetGlo: return setGlobal(in.immU32());
				case StmtOpEncoding::I32Store8: return storeMemory<I32Type>(TypeId::I8,IntOp::wrap,decodeAddress(0));
				case StmtOpEncoding::I32StoreOff8: return storeMemory<I32Type>(TypeId::I8,IntOp::wrap,decodeAddress(in.immU32()));
				case StmtOpEncoding::I32Store16: return storeMemory<I32Type>(TypeId::I16,IntOp::wrap,decodeAddress(0));
				case StmtOpEncoding::I32StoreOff16: return storeMemory<I32Type>(TypeId::I16,IntOp::wrap,decodeAddress(in.immU32()));
				case StmtOpEncoding::I32Store32: return storeMemory<I32Type>(TypeId::I32,IntOp::wrap,decodeAddress(0));
				case StmtOpEncoding::I32StoreOff32: return storeMemory<I32Type>(TypeId::I32,IntOp::wrap,decodeAddress(in.immU32()));
				case StmtOpEncoding::F32Store: return storeMemory<F32Type>(TypeId::F32,FloatOp::demote,decodeAddress(0));
				case StmtOpEncoding::F32StoreOff: return storeMemory<F32Type>(TypeId::F32,FloatOp::demote,decodeAddress(in.immU32()));
				case StmtOpEncoding::F64Store: return storeMemory<F64Type>(TypeId::F64,FloatOp::demote,decodeAddress(0));
				case StmtOpEncoding::F64StoreOff: return storeMemory<F64Type>(TypeId::F64,FloatOp::demote,decodeAddress(in.immU32()));
				case StmtOpEncoding::CallInt: return callInternalStatement(in.immU32());
				case StmtOpEncoding::CallInd: return callIndirectStatement(in.immU32());
				case StmtOpEncoding::CallImp: return callImportStatement(in.immU32());
				case StmtOpEncoding::Ret: return decodeReturn();
				case StmtOpEncoding::Block: return decodeBlock();
				case StmtOpEncoding::IfThen: return decodeIf();
				case StmtOpEncoding::IfElse: return decodeIfElse();
				case StmtOpEncoding::While: return decodeWhile(isEnclosedByLabel);
				case StmtOpEncoding::Do: return decodeDo(isEnclosedByLabel);
				case StmtOpEncoding::Label: return decodeLabel();
				case StmtOpEncoding::Break: return decodeBreak();
				case StmtOpEncoding::Continue: return decodeContinue();
				case StmtOpEncoding::BreakLabel: return decodeBreakLabel(in.immU32());
				case StmtOpEncoding::ContinueLabel: return decodeContinueLabel(in.immU32());
				case StmtOpEncoding::Switch: return decodeSwitch();
				default: throw new FatalDecodeException("invalid statement opcode");
				}
			}
			else
			{
				switch(statementWithImm)
				{
				case StmtOpEncodingWithImm::SetLoc: return setLocal(imm);
				case StmtOpEncodingWithImm::SetGlo: return setGlobal(imm);
				default: throw new FatalDecodeException("invalid statement opcode");
				}
			}
		}

		VoidExpression* decodeStatementList()
		{
			uint32_t numStatements = in.immU32();
			if(numStatements == 0) { return new(arena) Nop(); }
			else
			{
				auto voidExpressions = new(arena) VoidExpression*[numStatements - 1];
				for(uint32_t statementIndex = 0;statementIndex < numStatements - 1;++statementIndex)
				{ voidExpressions[statementIndex] = decodeStatement(); }
				VoidExpression* resultExpression = decodeStatement();
				return new(arena) Block<VoidClass>(voidExpressions,numStatements-1,resultExpression);
			}
		}

		void decodeFunctions()
		{
			for(size_t functionIndex = 0; functionIndex < module.functions.size(); functionIndex++)
			{
				currentFunction = module.functions[functionIndex];

				// Decode the number of local variables used by the function.
				uint32_t numLocalI32s = 0;
				uint32_t numLocalF32s = 0;
				uint32_t numLocalF64s = 0;
				VaReturnTypes varTypes;
				VaReturnTypesWithImm varTypesWithImm;
				uint8_t imm;
				if(in.code(&varTypes,&varTypesWithImm,&imm))
				{
					if(varTypes & VaReturnTypes::I32) numLocalI32s = in.immU32();
					if(varTypes & VaReturnTypes::F32) numLocalF32s = in.immU32();
					if(varTypes & VaReturnTypes::F64) numLocalF64s = in.immU32();
				}
				else numLocalI32s = imm;
				
				currentFunction->locals.resize(currentFunction->type.parameters.size() + numLocalI32s + numLocalF32s + numLocalF64s);
				uintptr_t localIndex = 0;
				i32TempLocalIndex = -1;
				f32TempLocalIndex = -1;
				f64TempLocalIndex = -1;
				
				// Create locals for the function's parameters.
				currentFunction->parameterLocalIndices.resize(currentFunction->type.parameters.size());
				for(uintptr_t parameterIndex = 0;parameterIndex < currentFunction->type.parameters.size();++parameterIndex)
				{
					currentFunction->parameterLocalIndices[parameterIndex] = localIndex;
					currentFunction->locals[localIndex++] = {currentFunction->type.parameters[parameterIndex],nullptr};
				}
				
				// Create the function local variables.
				for(size_t variableIndex = 0;variableIndex < numLocalI32s;++variableIndex)
				{ currentFunction->locals[localIndex++] = {TypeId::I32,nullptr}; }
				for(size_t variableIndex = 0;variableIndex < numLocalF32s;++variableIndex)
				{ currentFunction->locals[localIndex++] = {TypeId::F32,nullptr}; }
				for(size_t variableIndex = 0;variableIndex < numLocalF64s;++variableIndex)
				{ currentFunction->locals[localIndex++] = {TypeId::F64,nullptr}; }

				// Decode the function's statements.
				auto voidExpression = decodeStatementList();
				switch(currentFunction->type.returnType)
				{
				case TypeId::I32: currentFunction->expression = new(arena) Block<IntClass>(new(arena) VoidExpression*[1]{voidExpression},1,new(arena) Literal<I32Type>(0)); break;
				case TypeId::F32: currentFunction->expression = new(arena) Block<FloatClass>(new(arena) VoidExpression*[1]{voidExpression},1,new(arena) Literal<F32Type>(0.0f)); break;
				case TypeId::F64: currentFunction->expression = new(arena) Block<FloatClass>(new(arena) VoidExpression*[1]{voidExpression},1,new(arena) Literal<F64Type>(0.0)); break;
				case TypeId::Void: currentFunction->expression = voidExpression; break;
				default: throw;
				}
			}
		}

		void decodeExports()
		{
			switch(in.export_format())
			{
			case ExportFormat::Default:
			{
				// The module has a single export. Call it "default"
				auto functionIndex = in.boundedImmU32("export function index",module.functions.size());
				module.exportNameToFunctionIndexMap["default"] = functionIndex;
				break;
			}
			case ExportFormat::Record:
			{
				// The module has multiple exports. Each associates an externally exported name with a function index in the module.
				uint32_t numExports = in.immU32();
				for(uint32_t exportIndex = 0;exportIndex < numExports;++exportIndex)
				{
					Memory::ArenaString exportName;
					while(char c = in.single_char()) { exportName.append(arena,c); };

					auto functionIndex = in.boundedImmU32("export function index",module.functions.size());
					module.exportNameToFunctionIndexMap[exportName.c_str()] = functionIndex;

					// Also set the export name on the function.
					module.functions[functionIndex]->name = exportName.c_str();
				}
				break;
			}
			default: throw new FatalDecodeException("invalid export record format");
			}
		}

		// Decode the table of function types used in the module.
		void decodeFunctionTypes()
		{
			uint32_t numTypes = in.immU32();
			functionTypes.resize(numTypes);
			for(uint32_t typeIndex = 0;typeIndex < numTypes;++typeIndex)
			{
				functionTypes[typeIndex].returnType = in.returnType();
				uint32_t numParameters = in.immU32();
				functionTypes[typeIndex].parameters.resize(numParameters);
				for(uint32_t parameterIndex = 0; parameterIndex < numParameters; parameterIndex++)
				{ functionTypes[typeIndex].parameters[parameterIndex] = in.type(); }
			}
		}

		// Decode the table of function imports used in the module.
		void decodeFunctionImports()
		{
			uint32_t numFunctionImports = in.immU32();
			uint32_t numFunctionImportTypes = in.immU32();

			module.functionImports.resize(0);
			module.functionImports.reserve(numFunctionImportTypes);
			for(uint32_t functionImportIndex = 0; functionImportIndex < numFunctionImports; functionImportIndex++)
			{
				Memory::ArenaString importName;
				while(char c = in.single_char()) { importName.append(arena,c); };

				auto numTypes = in.immU32();
				for(uint32_t typeIndex = 0;typeIndex < numTypes;++typeIndex)
				{ module.functionImports.push_back({functionTypes[in.boundedImmU32("function type index",functionTypes.size())],importName.c_str()}); }
			}
		}

		// Decodes the module's global variables.
		void decodeGlobals()
		{
			uint32_t numGlobalsI32 = in.immU32();
			uint32_t numGlobalsF32 = in.immU32();
			uint32_t numGlobalsF64 = in.immU32();
			uint32_t numImportsI32 = in.immU32();
			uint32_t numImportsF32 = in.immU32();
			uint32_t numImportsF64 = in.immU32();

			module.globals.reserve(numGlobalsI32 + numGlobalsF32 + numGlobalsF64 + numImportsI32 + numImportsF32 + numImportsF64);

			for(uint32_t variableIndex = 0;variableIndex < numGlobalsI32;++variableIndex) { module.globals.push_back({TypeId::I32,nullptr}); }
			for(uint32_t variableIndex = 0;variableIndex < numGlobalsF32;++variableIndex) { module.globals.push_back({TypeId::F32,nullptr}); }
			for(uint32_t variableIndex = 0;variableIndex < numGlobalsF64;++variableIndex) { module.globals.push_back({TypeId::F64,nullptr}); }

			for(uint32_t variableIndex = 0;variableIndex < numImportsI32;++variableIndex)
			{
				Memory::ArenaString importName;
				while(char c = in.single_char()) { importName.append(arena,c); };
				module.variableImports.push_back({TypeId::I32,importName.c_str(),module.globals.size()});
				module.globals.push_back({TypeId::I32,importName.c_str()});
			}
			for(uint32_t i = 0; i < numImportsF32; ++i)
			{
				Memory::ArenaString importName;
				while(char c = in.single_char()) { importName.append(arena,c); };
				module.variableImports.push_back({TypeId::F32,importName.c_str(),module.globals.size()});
				module.globals.push_back({TypeId::F32,importName.c_str()});
			}
			for(uint32_t i = 0; i < numImportsF64; ++i)
			{
				Memory::ArenaString importName;
				while(char c = in.single_char()) { importName.append(arena,c); };
				module.variableImports.push_back({TypeId::F64,importName.c_str(),module.globals.size()});
				module.globals.push_back({TypeId::F64,importName.c_str()});
			}
		}

		// Decodes the constant table: constants that may be used by index throughout the module.
		void decodeConstantPool()
		{
			uint32_t numI32s = in.immU32();
			uint32_t numF32s = in.immU32();
			uint32_t numF64s = in.immU32();

			i32Constants.resize(numI32s);
			f32Constants.resize(numF32s);
			f64Constants.resize(numF64s);

			for(uint32_t i = 0; i < numI32s; ++i) { i32Constants[i] = in.immU32(); }
			for(uint32_t i = 0; i < numF32s; ++i) { f32Constants[i] = in.f32(); }
			for(uint32_t i = 0; i < numF64s; ++i) { f64Constants[i] = in.f64(); }
		}

		// Decodes the types of the functions defined in the module.
		void decodeFunctionDeclarations()
		{
			const uint32_t numFunctions = in.immU32();
			module.functions.resize(numFunctions);
			for(uint32_t functionIndex = 0;functionIndex < numFunctions;++functionIndex)
			{
				auto function = new(arena) Function;
				module.functions[functionIndex] = function;
				function->type = functionTypes[in.boundedImmU32("function type index",functionTypes.size())];
			}
		}

		// Decode the function pointer tables: groups of functions that may be called indirectly, grouped by type.
		void decodeFunctionPointerTables()
		{
			const uint32_t numFunctionPointerTables = in.immU32();
			module.functionTables.resize(numFunctionPointerTables);
			for(uint32_t tableIndex = 0;tableIndex < numFunctionPointerTables;++tableIndex)
			{
				auto& table = module.functionTables[tableIndex];
				table.type = functionTypes[in.boundedImmU32("function type index",functionTypes.size())];

				// Decode the function table elements.
				table.numFunctions = in.immU32();
				table.functionIndices = new(arena) uintptr_t[table.numFunctions];
				for(uint32_t functionIndex = 0;functionIndex < table.numFunctions;++functionIndex)
				{
					table.functionIndices[functionIndex] = in.boundedImmU32("function index",module.functions.size());
					if(table.functionIndices[functionIndex] >= module.functions.size()) { throw new FatalDecodeException("invalid function index"); }
				}

				// Verify that the number of elements is a non-zero power of two, so we can use bitwise and to prevent out-of-bounds accesses.
				if((table.numFunctions & (table.numFunctions-1)) != 0) { throw new FatalDecodeException("function table have non-zero power of two number of entries"); }
			}
		}

		bool decode()
		{
			// Check for the WASM magic number.
			if(in.u32() != MagicNumber) { throw new FatalDecodeException("expected magic number"); }
			// ?
			in.u32();

			decodeConstantPool();
			decodeFunctionTypes();
			decodeFunctionImports();
			decodeGlobals();
			decodeFunctionDeclarations();
			decodeFunctionPointerTables();
			decodeFunctions();
			decodeExports();

			return in.complete() && !outErrors.size();
		}
	};

	bool decode(const uint8_t* packed,size_t numBytes,Module*& outModule,std::vector<AST::ErrorRecord*>& outErrors)
	{
		outModule = new Module;
		try
		{
			InputStream in(packed,packed + numBytes);
			return DecodeContext(in,*outModule,outErrors).decode();
		}
		catch(FatalDecodeException* exception)
		{
			outErrors.push_back(exception);
			return false;
		}
	}
}
