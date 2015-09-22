#include "Core/Core.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "WebAssembly.h"

namespace WebAssemblyBinary
{
	using namespace AST;

	// =================================================================================================
	// Magic serialization constants

	static const uint32 MagicNumber = 0x6d736177;

	enum class StmtOpEncoding : uint8
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

	enum class StmtOpEncodingWithImm : uint8
	{
		SetLoc,
		SetGlo,
		Reseved1,
		Reseved2,

		Bad
	};

	enum class SwitchCase : uint8
	{
		Case0,
		Case1,
		CaseN,
		Default0,
		Default1,
		DefaultN
	};

	enum class I32OpEncoding : uint8
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

	enum class I32OpEncodingWithImm : uint8
	{
		LitPool,
		LitImm,
		GetLoc,
		Reserved,

		Bad
	};

	enum class F32OpEncoding : uint8
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

	enum class F32OpEncodingWithImm : uint8
	{
		LitPool,
		GetLoc,
		Reserved0,
		Reserved1,

		Bad
	};

	enum class F64OpEncoding : uint8
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

	enum class F64OpEncodingWithImm : uint8
	{
		LitPool,
		GetLoc,
		Reserved0,
		Reserved1,

		Bad
	};

	enum class VoidOpEncoding : uint8
	{
		CallInt,
		CallInd,
		CallImp,

		Bad
	};

	enum class Type : uint8
	{
		I32,
		F32,
		F64
	};

	enum class VaReturnTypes : uint8
	{
		I32 = 0x1,
		F32 = 0x2,
		F64 = 0x4,
	};

	inline VaReturnTypes operator|(VaReturnTypes lhs,VaReturnTypes rhs) { return VaReturnTypes(uint8(lhs) | uint8(rhs)); }
	inline bool operator&(VaReturnTypes lhs,VaReturnTypes rhs) { return bool((uint8(lhs) & uint8(rhs)) != 0); }

	enum class VaReturnTypesWithImm : uint8
	{
		OnlyI32,
		Reserved0,
		Reserved1,
		Reserved2
	};

	enum class ReturnType : uint8
	{
		I32 = uint8(Type::I32),
		F32 = uint8(Type::F32),
		F64 = uint8(Type::F64),
		Void
	};

	static const uint8 HasImmFlag = 0x80;
	static_assert(uint8(I32OpEncoding::Bad) <= HasImmFlag,"MSB reserved to distinguish I32 from I32WithImm");
	static_assert(uint8(F32OpEncoding::Bad) <= HasImmFlag,"MSB reserved to distinguish F32 from F32WithImm");
	static_assert(uint8(F64OpEncoding::Bad) <= HasImmFlag,"MSB reserved to distinguish F64 from F64WithImm");

	static const unsigned OpWithImmBits = 2;
	static const uint32 OpWithImmLimit = 1 << OpWithImmBits;
	static_assert(uint8(I32OpEncodingWithImm::Bad) <= OpWithImmLimit,"I32WithImm op fits");
	static_assert(uint8(F32OpEncodingWithImm::Bad) <= OpWithImmLimit,"F32WithImm op fits");
	static_assert(uint8(F64OpEncodingWithImm::Bad) <= OpWithImmLimit,"F64WithImm op fits");

	static const unsigned ImmBits = 5;
	static const uint32 ImmLimit = 1 << ImmBits;
	static_assert(1 + OpWithImmBits + ImmBits == 8,"Bits of immediate op should add up to a byte");

	template <class TWithImm>
	static inline void UnpackOpWithImm(uint8 byte,TWithImm* op,uint8 *imm)
	{
		assert(byte & HasImmFlag);
		*op = TWithImm((byte >> ImmBits) & (OpWithImmLimit - 1));
		*imm = byte & (ImmLimit - 1);
	}

	enum class ExportFormat : uint8
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
		InputStream(const uint8* inStart,const uint8* inEnd) : cur(inStart), end(inEnd) {}

		uint32 u32()
		{
			uint32 u32 = cur[0] | cur[1] << 8 | cur[2] << 16 | cur[3] << 24;
			cur += 4;
			return u32;
		}
		float32 f32()
		{
			union
			{
				uint8 arr[4];
				float32 f;
			} u ={{cur[0],cur[1],cur[2],cur[3]}};
			cur += 4;
			return u.f;
		}
		float64 f64()
		{
			union
			{
				uint8 arr[8];
				float64 d;
			} u ={{cur[0],cur[1],cur[2],cur[3],cur[4],cur[5],cur[6],cur[7]}};
			cur += 8;
			return u.d;
		}
		StmtOpEncoding stmt() { return u8<StmtOpEncoding>(); }
		SwitchCase switch_case() { return u8<SwitchCase>(); }
		template<class T,class TWithImm> inline bool code(T*,TWithImm*,uint8*);
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
		inline uint32 immU32();
		inline uint32 boundedImmU32(const char* context,size_t maxValue)
		{
			auto result = immU32();
			if(result >= maxValue) { throw new FatalDecodeException(std::string("invalid ") + context); }
			return result;
		}
		inline int32 immS32();
		char single_char() { return *cur++; }
		bool complete() const { return cur == end; }
	private:
		const uint8* cur;
		const uint8* end;

		template <class T> T u8()
		{
			if(cur >= end) { throw new FatalDecodeException("expected more input than available"); }
			return T(*cur++);
		}
	};

	template <class T,class TWithImm>
	bool InputStream::code(T* t,TWithImm* tWithImm,uint8* imm)
	{
		uint8 byte = *cur++;
		if(!(byte & HasImmFlag))
		{
			*t = T(byte);
			return true;
		}

		UnpackOpWithImm(byte,tWithImm,imm);
		return false;
	}

	uint32 inline InputStream::immU32()
	{
		uint32 u32 = *cur++;
		if(u32 < 0x80)
			return u32;

		u32 &= 0x7f;

		for(unsigned shift = 7; true; shift += 7)
		{
			uint32 b = *cur++;
			if(b < 0x80)
				return u32 | (b << shift);
			u32 |= (b & 0x7f) << shift;
		}
	}

	int32 inline InputStream::immS32()
	{
		uint32 u32 = *cur++;
		if(u32 < 0x80)
			return int32(u32) << (32-7) >> (32-7);

		u32 &= 0x7f;

		for(unsigned shift = 7; true; shift += 7)
		{
			uint32 b = *cur++;
			if(b < 0x80)
			{
				u32 |= b << shift;
				int sign_extend = (32-7) - shift;
				if(sign_extend > 0)
					return int32(u32) << sign_extend >> sign_extend;
				return int32(u32);
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

		std::vector<uint32> i32Constants;
		std::vector<float32> f32Constants;
		std::vector<float64> f64Constants;
		std::vector<FunctionType> functionTypes;
		std::map<std::string,uintptr> intrinsicNameToFunctionImportIndex;

		struct Global
		{
			TypeId type;
			uint64 address;
		};
		std::vector<Global> globals;

		struct VariableImport
		{
			TypeId type;
			uintptr getterFunctionImportIndex;
			uintptr setterFunctionImportIndex;
		};
		std::vector<VariableImport> variableImports;

		// Information about the current operation being decoded.
		std::vector<BranchTarget*> explicitBreakTargets;
		std::vector<BranchTarget*> implicitBreakTargets;
		std::vector<BranchTarget*> explicitContinueTargets;
		std::vector<BranchTarget*> implicitContinueTargets;
		Function* currentFunction;

		DecodeContext(InputStream& inIn,Module& inModule,std::vector<ErrorRecord*>& inOutErrors)
		:	in(inIn)
		,	module(inModule)
		,	outErrors(inOutErrors)
		,	arena(inModule.arena)
		{}

		template<typename Class>
		Error<Class>* recordError(const char* message)
		{
			auto error = new(arena) Error<Class>(message);
			outErrors.push_back(error);
			return error;
		}

		// Decodes an expression based on the type of its result.
		IntExpression* decodeExpression(I32Type) { return decodeExpressionI32(); }
		FloatExpression* decodeExpression(F32Type) { return decodeExpressionF32(); }
		FloatExpression* decodeExpression(F64Type) { return decodeExpressionF64(); }
		VoidExpression* decodeExpression(VoidType) { return decodeExpressionVoid(); }

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
		typename Type::TypeExpression* getLocal(uint32 localIndex)
		{
			if(localIndex >= currentFunction->locals.size()) { return recordError<typename Type::Class>("getlocal: invalid local index"); }
			if(currentFunction->locals[localIndex].type != Type::id) { return recordError<typename Type::Class>("getlocal: incorrect type"); }
			return as<typename Type::Class>(new(arena) GetLocal(Type::Class::id,localIndex));
		}

		// Stores a value to a local variable.
		TypedExpression setLocal(uint32 localIndex)
		{
			if(localIndex >= currentFunction->locals.size()) { throw new FatalDecodeException("setlocal: invalid local index"); }
			auto type = currentFunction->locals[localIndex].type;
			auto value = decodeExpression(type);
			return TypedExpression(new(arena) SetLocal(getPrimaryTypeClass(type),value,localIndex),type);
		}
		template<typename Type>
		typename Type::TypeExpression* setLocalExpression(uint32 localIndex)
		{
			return as<typename Type::Class>(setLocal(localIndex));
		}
		
		// Loads a value from a global variable.
		template<typename Type>
		typename Type::TypeExpression* getGlobal(uint32 globalIndex)
		{
			if(globalIndex < globals.size())
			{
				if(globals[globalIndex].type != Type::id) { return recordError<typename Type::Class>("getglobal: incorrect type"); }
				auto address = globals[globalIndex].address;
				bool isFarAddress = address >= (1ull << 32);
				IntExpression* addressLiteral = isFarAddress ? as<IntClass>(new(arena) Literal<I64Type>(address))
					: new(arena) Literal<I32Type>((uint32)address);
				
				return new(arena) Load<typename Type::Class>(Type::Op::load,isFarAddress,getTypeByteWidthLog2(Type::id),addressLiteral,Type::id);
			}
			else if(globalIndex < globals.size() + variableImports.size())
			{
				auto importIndex = globalIndex - globals.size();
				if(variableImports[importIndex].type != Type::id) { return recordError<typename Type::Class>("getglobal: incorrect type"); }
				return as<typename Type::Class>(new(arena) Call(AnyOp::callImport,getPrimaryTypeClass(Type::id),variableImports[importIndex].getterFunctionImportIndex,nullptr));
			}
			else { return recordError<typename Type::Class>("getglobal: invalid global index"); }
		}

		// Stores a value to a global variable.
		TypedExpression setGlobal(uint32 globalIndex)
		{
			if(globalIndex < globals.size())
			{
				auto type = globals[globalIndex].type;
				auto value = decodeExpression(type);
				auto address = globals[globalIndex].address;
				bool isFarAddress = address >= (1ull << 32);
				IntExpression* addressLiteral = isFarAddress ? as<IntClass>(new(arena) Literal<I64Type>(address))
					: new(arena) Literal<I32Type>((uint32)address);
				UntypedExpression* store;
				switch(type)
				{
				case TypeId::I32: store = new(arena) Store<IntClass>(isFarAddress,getTypeByteWidthLog2(type),addressLiteral,TypedExpression(value,type),type); break;
				case TypeId::F32:
				case TypeId::F64: store = new(arena) Store<FloatClass>(isFarAddress,getTypeByteWidthLog2(type),addressLiteral,TypedExpression(value,type),type); break;
				default: throw;
				}
				return TypedExpression(store,type);
			}
			else if(globalIndex < globals.size() + variableImports.size())
			{
				auto import = variableImports[globalIndex - globals.size()];
				auto type = import.type;
				auto value = decodeExpression(type);
				auto parameters = new(arena) UntypedExpression*[1] {value};
				auto call = new(arena) Call(AnyOp::callImport,getPrimaryTypeClass(type),import.setterFunctionImportIndex,parameters);
				return TypedExpression(call,type);
			}
			else { throw new FatalDecodeException("setglobal: invalid global index"); }
		}
		template<typename Type>
		typename Type::TypeExpression* setGlobalExpression(uint32 globalIndex)
		{
			return as<typename Type::Class>(setGlobal(globalIndex));
		}

		// Decodes an address in the form of an offset into the module's linear memory.
		IntExpression* decodeAddress(uint32 offset)
		{
			IntExpression* byteIndex = decodeExpression(I32Type());

			// Add the offset provided by the operation.
			if(offset != 0)
			{ byteIndex = new(arena) Binary<IntClass>(IntOp::add,byteIndex,new(arena) Literal<I32Type>(offset)); }

			return byteIndex;
		}

		// Loads an I8, I16, or I32 into an I32 intermediate. I8 and I16 is either zero or sign extended to 32-bit depending on the loadOp.
		template<typename Type>
		typename Type::TypeExpression* load(TypeId memoryType,typename Type::Op loadOp,IntExpression* address)
		{
			return new(arena) Load<typename Type::Class>(loadOp,false,getTypeByteWidthLog2(memoryType),address,memoryType);
		}

		// Stores a value to memory.
		template<typename Type>
		typename Type::TypeExpression* store(TypeId memoryType,IntExpression* address)
		{
			auto value = decodeExpression(Type());
			return new(arena) Store<typename Type::Class>(false,getTypeByteWidthLog2(memoryType),address,TypedExpression(value,Type::id),memoryType);
		}

		// Converts a signed or unsigned 32-bit integer to a float32.
		FloatExpression* castI32ToFloat(FloatOp op)
		{
			return new(arena) Cast<FloatClass>(op,TypedExpression(decodeExpression(I32Type()),TypeId::I32));
		}

		// Applies an unary operation to an integer operand.
		template<typename Type>
		typename Type::TypeExpression* decodeUnary(typename Type::Op op)
		{
			auto operand = decodeExpression(Type());
			return new(arena) Unary<typename Type::Class>(op,operand);
		}

		// Applies a binary operation to an integer operand.
		template<typename Type>
		typename Type::TypeExpression* decodeBinary(typename Type::Op op)
		{
			auto leftOperand = decodeExpression(Type());
			auto rightOperand = decodeExpression(Type());
			return new(arena) Binary<typename Type::Class>(op,leftOperand,rightOperand);
		}

		// Compares two integers and returns an I32 value.
		template<typename OperandType>
		IntExpression* decodeCompare(BoolOp op)
		{
			auto leftOperand = decodeExpression(OperandType());
			auto rightOperand = decodeExpression(OperandType());
			return new(arena) Cast<IntClass>(IntOp::reinterpretBool,TypedExpression(new(arena) Comparison(op,OperandType::id,leftOperand,rightOperand),TypeId::Bool));
		}

		// Evaluates two expressions, and returns only the result of the second.
		template<typename Type>
		typename Type::TypeExpression* comma()
		{
			VoidExpression* voidExpression;
			switch(in.returnType())
			{
			case TypeId::I32: voidExpression = new(arena) DiscardResult(TypedExpression(decodeExpression(I32Type()),TypeId::I32)); break;
			case TypeId::F32: voidExpression = new(arena) DiscardResult(TypedExpression(decodeExpression(F32Type()),TypeId::F32)); break;
			case TypeId::F64: voidExpression = new(arena) DiscardResult(TypedExpression(decodeExpression(F64Type()),TypeId::F64)); break;
			case TypeId::Void: voidExpression = decodeExpression(VoidType()); break;
			default: throw;
			}
			auto value = decodeExpression(Type());
			return new(arena) Sequence<typename Type::Class>(voidExpression,value);
		}

		BoolExpression* decodeExpressionI32AsBool()
		{
			auto i32Value = decodeExpression(I32Type());
			if(i32Value->op() == IntOp::reinterpretBool && ((Cast<IntClass>*)i32Value)->source.type == TypeId::Bool)
				{ return as<BoolClass>(((Cast<IntClass>*)i32Value)->source); }
			else { return new(arena) Comparison(BoolOp::ne,TypeId::I32,i32Value,new(arena) Literal<I32Type>(0)); }
		}

		// Chooses between two values based on a predicate value.
		template<typename Type>
		typename Type::TypeExpression* cond()
		{
			auto condition = decodeExpressionI32AsBool();
			auto trueValue = decodeExpression(Type());
			auto falseValue = decodeExpression(Type());
			return new(arena) IfElse<typename Type::Class>(condition,trueValue,falseValue);
		}

		// Decodes the parameters for a function.
		UntypedExpression** decodeParameters(const std::vector<TypeId>& parameterTypes)
		{
			auto parameters = new(arena) UntypedExpression*[parameterTypes.size()];
			for(uintptr parameterIndex = 0;parameterIndex < parameterTypes.size();++parameterIndex)
			{ parameters[parameterIndex] = decodeExpression(parameterTypes[parameterIndex]); }
			return parameters;
		}

		// Calls a function by a direct pointer.
		template<typename Class>
		typename Class::ClassExpression* callInternal(TypeId returnType,uint32 functionIndex)
		{
			if(functionIndex >= module.functions.size()) { throw new FatalDecodeException("callinternal: invalid function index"); }
			auto function = module.functions[functionIndex];
			auto parameters = decodeParameters(function->type.parameters);
			return function->type.returnType == returnType
				? as<Class>(new(arena) Call(AnyOp::callDirect,Class::id,functionIndex,parameters))
				: recordError<Class>("callinternal: incorrect type");
		}
		VoidExpression* callInternalStatement(uint32 functionIndex)
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
		typename Class::ClassExpression* callIndirect(TypeId returnType,uint32 tableIndex)
		{
			if(tableIndex >= module.functions.size()) { throw new FatalDecodeException("callindirect: invalid table index"); }
			const FunctionTable& functionTable = module.functionTables[tableIndex];
			auto functionIndex = decodeExpression(I32Type());
			auto parameters = decodeParameters(functionTable.type.parameters);
			return functionTable.type.returnType == returnType
				? as<Class>(new(arena) CallIndirect(Class::id,tableIndex,functionIndex,parameters))
				: recordError<Class>("callindirect: incorrect type");
		}
		VoidExpression* callIndirectStatement(uint32 tableIndex)
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
		typename Class::ClassExpression* callImport(TypeId returnType,uintptr functionImportIndex)
		{
			if(functionImportIndex >= module.functionImports.size()) { throw new FatalDecodeException("callimport: invalid import index"); }
			const FunctionImport& functionImport = module.functionImports[functionImportIndex];
			UntypedExpression** parameters = decodeParameters(functionImport.type.parameters);
			return functionImport.type.returnType == returnType
				? as<Class>(new(arena) Call(AnyOp::callImport,Class::id,functionImportIndex,parameters))
				: recordError<Class>("callimport: incorrect type");
		}
		VoidExpression* callImportStatement(uintptr functionImportIndex)
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
		uintptr getUniqueFunctionImport(const FunctionType& intrinsicType,const char* intrinsicName)
		{
			// Add one import for every unique intrinsic name used.
			auto intrinsicIt = intrinsicNameToFunctionImportIndex.find(intrinsicName);
			if(intrinsicIt != intrinsicNameToFunctionImportIndex.end())
			{
				assert(module.functionImports[intrinsicIt->second].type == intrinsicType);
				return intrinsicIt->second;
			}
			else
			{
				auto functionImportIndex = module.functionImports.size();
				module.functionImports.push_back({intrinsicType,"emscripten",intrinsicName});
				intrinsicNameToFunctionImportIndex[intrinsicName] = functionImportIndex;
				return functionImportIndex;
			}
		}
		template<typename Class>
		typename Class::ClassExpression* decodeIntrinsic(const FunctionType& intrinsicType,const char* intrinsicName)
		{
			return callImport<Class>(intrinsicType.returnType,getUniqueFunctionImport(intrinsicType,intrinsicName));
		}

		// Computes the minimum or maximum of a set.
		template<typename Type>
		typename Type::TypeExpression* minMax(typename Type::Op op)
		{
			auto numParameters = in.immU32();
			if(numParameters == 0) { return recordError<typename Type::Class>("minmax: must receive >0 parameters"); }
			typename Type::TypeExpression* result = decodeExpression(Type());
			for(uint32 parameterIndex = 1;parameterIndex < numParameters;++parameterIndex)
			{ result = new(arena) Binary<typename Type::Class>(op,result,decodeExpression(Type())); }
			return result;
		}
		template<typename Type>
		typename Type::TypeExpression* minMaxIntrinsic(const char* intrinsicName)
		{
			auto numParameters = in.immU32();
			if(numParameters == 0) { return recordError<typename Type::Class>("minmax: must receive >0 parameters"); }
			auto intrinsicImportIndex = getUniqueFunctionImport(FunctionType(Type::id,{Type::id,Type::id}),intrinsicName);
			typename Type::TypeExpression* result = decodeExpression(Type());
			for(uint32 parameterIndex = 1;parameterIndex < numParameters;++parameterIndex)
			{
				auto nextParameter = decodeExpression(Type());
				auto parameterPair = new(arena) UntypedExpression*[2] {result,nextParameter};
				result = as<typename Type::Class>(new(arena) Call(AnyOp::callImport,Type::Class::id,intrinsicImportIndex,parameterPair));
			}
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
			return new(arena) IfElse<VoidClass>(condition,thenExpression,Nop::get());
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
			// If the loop is immediately inside a label node, then it needs to create explicit break/continue targets.
			if(isEnclosedByLabel)
			{
				// The label already pushed an explicit break target, so just replace it with the loop's break target.
				explicitBreakTargets.back() = breakBranchTarget;
				explicitContinueTargets.back() = continueBranchTarget;
			}
			auto loopExpression = decodeStatement();
			implicitBreakTargets.pop_back();
			implicitContinueTargets.pop_back();
			auto loopBody = new(arena) IfElse<VoidClass>(condition,loopExpression,new(arena) Branch<VoidClass>(breakBranchTarget,nullptr));
			return new(arena) Loop<VoidClass>(loopBody,breakBranchTarget,continueBranchTarget);
		}

		VoidExpression* decodeDo(bool isEnclosedByLabel)
		{
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			auto continueBranchTarget = new(arena) BranchTarget(TypeId::Void);
			implicitBreakTargets.push_back(breakBranchTarget);
			implicitContinueTargets.push_back(continueBranchTarget);
			// If the loop is immediately inside a label node, then it needs to create explicit break/continue targets.
			if(isEnclosedByLabel)
			{
				// The label already pushed an explicit break target, so just replace it with the loop's break target.
				explicitBreakTargets.back() = breakBranchTarget;
				explicitContinueTargets.back() = continueBranchTarget;
			}
			auto loopExpression = decodeStatement();
			auto condition = decodeExpressionI32AsBool();
			implicitBreakTargets.pop_back();
			implicitContinueTargets.pop_back();
			auto loopBody = new(arena) Sequence<VoidClass>(
				loopExpression,
				new(arena) IfElse<VoidClass>(
					condition,
					Nop::get(),
					new(arena) Branch<VoidClass>(breakBranchTarget,nullptr)
					)
				);
			return new(arena) Loop<VoidClass>(loopBody,breakBranchTarget,continueBranchTarget);
		}

		VoidExpression* decodeLabel()
		{
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			explicitBreakTargets.push_back(breakBranchTarget);
			explicitContinueTargets.push_back(nullptr);

			auto expression = decodeStatement(true);

			// If the explicit break target was replaced by the inner expression, then omit the label.
			auto innerExplicitBreakTarget = explicitBreakTargets.back();
			explicitBreakTargets.pop_back();
			explicitContinueTargets.pop_back();
			if(innerExplicitBreakTarget == breakBranchTarget) { return new(arena) Label<VoidClass>(breakBranchTarget,expression); }
			else { return expression; }
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

		VoidExpression* decodeBreakLabel(uint32 labelIndex)
		{
			if(labelIndex >= explicitBreakTargets.size()) { return recordError<VoidClass>("break: invalid label index"); }
			return new(arena) Branch<VoidClass>(explicitBreakTargets[labelIndex],nullptr);
		}

		VoidExpression* decodeContinueLabel(uint32 labelIndex)
		{
			if(labelIndex >= explicitContinueTargets.size() || !explicitContinueTargets[labelIndex]) { return recordError<VoidClass>("continue: invalid label index"); }
			return new(arena) Branch<VoidClass>(explicitContinueTargets[labelIndex],nullptr);
		}

		VoidExpression* decodeSwitch(bool isEnclosedByLabel)
		{
			uint32 numCases = in.immU32();
			auto value = decodeExpression(I32Type());
			
			auto breakBranchTarget = new(arena) BranchTarget(TypeId::Void);
			implicitBreakTargets.push_back(breakBranchTarget);

			// If the switch is immediately inside a label node, replace the label's explicit break target with our own.
			if(isEnclosedByLabel) { explicitBreakTargets.back() = breakBranchTarget; }

			SwitchArm* arms = new(arena) SwitchArm[numCases];
			uint32 defaultCaseIndex = numCases;
			size_t numDefaultCases = 0;
			for(uint32 caseIndex = 0;caseIndex < numCases;++caseIndex)
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
					caseValue = Nop::get();
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
				arms[numCases].value = Nop::get();
				defaultCaseIndex = numCases++;
			}

			return new(arena) Switch<VoidClass>(TypedExpression(value,TypeId::I32),defaultCaseIndex,numCases,arms,breakBranchTarget);
		}
		
		// Decodes a return based on the current function's return type.
		VoidExpression* decodeReturn()
		{
			switch(currentFunction->type.returnType)
			{
			case TypeId::I32: return new(arena) Return<VoidClass>(decodeExpression(I32Type()));
			case TypeId::F32: return new(arena) Return<VoidClass>(decodeExpression(F32Type()));
			case TypeId::F64: return new(arena) Return<VoidClass>(decodeExpression(F64Type()));
			case TypeId::Void: return new(arena) Return<VoidClass>(nullptr);
			default: throw;
			}
		}
		
		// Decodes expression opcodes that return an I32.
		IntExpression* decodeExpressionI32()
		{
			I32OpEncoding i32;
			I32OpEncodingWithImm i32WithImm;
			uint8 imm;
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
				case I32OpEncoding::SLoad8:     return load<I32Type>(TypeId::I8,IntOp::loadSExt,decodeAddress(0));
				case I32OpEncoding::SLoadOff8:  return load<I32Type>(TypeId::I8,IntOp::loadSExt,decodeAddress(in.immU32()));
				case I32OpEncoding::ULoad8:     return load<I32Type>(TypeId::I8,IntOp::loadZExt,decodeAddress(0));
				case I32OpEncoding::ULoadOff8:  return load<I32Type>(TypeId::I8,IntOp::loadZExt,decodeAddress(in.immU32()));
				case I32OpEncoding::SLoad16:    return load<I32Type>(TypeId::I16,IntOp::loadSExt,decodeAddress(0));
				case I32OpEncoding::SLoadOff16: return load<I32Type>(TypeId::I16,IntOp::loadSExt,decodeAddress(in.immU32()));
				case I32OpEncoding::ULoad16:    return load<I32Type>(TypeId::I16,IntOp::loadZExt,decodeAddress(0));
				case I32OpEncoding::ULoadOff16: return load<I32Type>(TypeId::I16,IntOp::loadZExt,decodeAddress(in.immU32()));
				case I32OpEncoding::Load32:     return load<I32Type>(TypeId::I32,IntOp::load,decodeAddress(0));
				case I32OpEncoding::LoadOff32:  return load<I32Type>(TypeId::I32,IntOp::load,decodeAddress(in.immU32()));
				case I32OpEncoding::Store8:     return store<I32Type>(TypeId::I8,decodeAddress(0));
				case I32OpEncoding::StoreOff8:  return store<I32Type>(TypeId::I8,decodeAddress(in.immU32()));
				case I32OpEncoding::Store16:    return store<I32Type>(TypeId::I16,decodeAddress(0));
				case I32OpEncoding::StoreOff16: return store<I32Type>(TypeId::I16,decodeAddress(in.immU32()));
				case I32OpEncoding::Store32:    return store<I32Type>(TypeId::I32,decodeAddress(0));
				case I32OpEncoding::StoreOff32: return store<I32Type>(TypeId::I32,decodeAddress(in.immU32()));
				case I32OpEncoding::CallInt:    return callInternal<IntClass>(TypeId::I32,in.immU32());
				case I32OpEncoding::CallInd:    return callIndirect<IntClass>(TypeId::I32,in.immU32());
				case I32OpEncoding::CallImp:    return callImport<IntClass>(TypeId::I32,in.immU32());
				case I32OpEncoding::Cond:       return cond<I32Type>();
				case I32OpEncoding::Comma:      return comma<I32Type>();
				case I32OpEncoding::FromF32:    return new(arena) Cast<IntClass>(IntOp::truncSignedFloat,TypedExpression(decodeExpression(F32Type()),TypeId::F32));
				case I32OpEncoding::FromF64:    return new(arena) Cast<IntClass>(IntOp::truncSignedFloat,TypedExpression(decodeExpression(F64Type()),TypeId::F64));
				case I32OpEncoding::Neg:        return decodeUnary<I32Type>(IntOp::neg);
				case I32OpEncoding::Add:        return decodeBinary<I32Type>(IntOp::add);
				case I32OpEncoding::Sub:        return decodeBinary<I32Type>(IntOp::sub);
				case I32OpEncoding::Mul:        return decodeBinary<I32Type>(IntOp::mul);
				case I32OpEncoding::SDiv:       return decodeBinary<I32Type>(IntOp::divs);
				case I32OpEncoding::UDiv:       return decodeBinary<I32Type>(IntOp::divu);
				case I32OpEncoding::SMod:       return decodeBinary<I32Type>(IntOp::rems);
				case I32OpEncoding::UMod:       return decodeBinary<I32Type>(IntOp::remu);
				case I32OpEncoding::BitNot:     return decodeUnary<I32Type>(IntOp::bitwiseNot);
				case I32OpEncoding::BitOr:      return decodeBinary<I32Type>(IntOp::bitwiseOr);
				case I32OpEncoding::BitAnd:     return decodeBinary<I32Type>(IntOp::bitwiseAnd);
				case I32OpEncoding::BitXor:     return decodeBinary<I32Type>(IntOp::bitwiseXor);
				case I32OpEncoding::Lsh:        return decodeBinary<I32Type>(IntOp::shl);
				case I32OpEncoding::ArithRsh:   return decodeBinary<I32Type>(IntOp::shrSExt);
				case I32OpEncoding::LogicRsh:   return decodeBinary<I32Type>(IntOp::shrZExt);
				case I32OpEncoding::Clz:        return decodeUnary<I32Type>(IntOp::clz);
				case I32OpEncoding::LogicNot: 
				{
					auto comparison = new(arena) Comparison(BoolOp::eq,TypeId::I32,decodeExpression(I32Type()),new(arena) Literal<I32Type>(0));
					return new(arena) Cast<IntClass>(IntOp::reinterpretBool,TypedExpression(comparison,TypeId::Bool));
				}
				case I32OpEncoding::EqI32:      return decodeCompare<I32Type>(BoolOp::eq);
				case I32OpEncoding::EqF32:      return decodeCompare<F32Type>(BoolOp::eq);
				case I32OpEncoding::EqF64:      return decodeCompare<F64Type>(BoolOp::eq);
				case I32OpEncoding::NEqI32:     return decodeCompare<I32Type>(BoolOp::ne);
				case I32OpEncoding::NEqF32:     return decodeCompare<F32Type>(BoolOp::ne);
				case I32OpEncoding::NEqF64:     return decodeCompare<F64Type>(BoolOp::ne);
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
				case I32OpEncoding::SMin:       return minMaxIntrinsic<I32Type>("min_s");
				case I32OpEncoding::UMin:       return minMaxIntrinsic<I32Type>("max_u");
				case I32OpEncoding::SMax:       return minMaxIntrinsic<I32Type>("max_s");
				case I32OpEncoding::UMax:       return minMaxIntrinsic<I32Type>("max_u");
				case I32OpEncoding::Abs:        return decodeUnary<I32Type>(IntOp::abs);
				default: throw new FatalDecodeException("invalid I32 opcode");
				}
			}
			else
			{
				switch(i32WithImm)
				{
				case I32OpEncodingWithImm::LitImm: return new(arena) Literal<I32Type>(imm);
				case I32OpEncodingWithImm::LitPool:
					if(imm >= i32Constants.size()) { throw new FatalDecodeException("invalid I32 constant index"); }
					return new(arena) Literal<I32Type>(i32Constants[imm]);
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
			uint8 imm;
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
				case F32OpEncoding::Load:     return load<F32Type>(TypeId::F32,FloatOp::load,decodeAddress(0));
				case F32OpEncoding::LoadOff:  return load<F32Type>(TypeId::F32,FloatOp::load,decodeAddress(in.immU32()));
				case F32OpEncoding::Store:    return store<F32Type>(TypeId::F32,decodeAddress(0));
				case F32OpEncoding::StoreOff: return store<F32Type>(TypeId::F32,decodeAddress(in.immU32()));
				case F32OpEncoding::CallInt:  return callInternal<FloatClass>(TypeId::F32,in.immU32());
				case F32OpEncoding::CallInd:  return callIndirect<FloatClass>(TypeId::F32,in.immU32());
				case F32OpEncoding::Cond:     return cond<F32Type>();
				case F32OpEncoding::Comma:    return comma<F32Type>();
				case F32OpEncoding::FromS32:  return castI32ToFloat(FloatOp::convertSignedInt);
				case F32OpEncoding::FromU32:  return castI32ToFloat(FloatOp::convertUnsignedInt);
				case F32OpEncoding::FromF64:  return new(arena) Cast<FloatClass>(FloatOp::demote,TypedExpression(decodeExpression(F64Type()),TypeId::F64));
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
			uint8 imm;
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
				case F64OpEncoding::Load:     return load<F64Type>(TypeId::F64,FloatOp::load,decodeAddress(0));
				case F64OpEncoding::LoadOff:  return load<F64Type>(TypeId::F64,FloatOp::load,decodeAddress(in.immU32()));
				case F64OpEncoding::Store:    return store<F64Type>(TypeId::F64,decodeAddress(0));
				case F64OpEncoding::StoreOff: return store<F64Type>(TypeId::F64,decodeAddress(in.immU32()));
				case F64OpEncoding::CallInt:  return callInternal<FloatClass>(TypeId::F64,in.immU32());
				case F64OpEncoding::CallInd:  return callIndirect<FloatClass>(TypeId::F64,in.immU32());
				case F64OpEncoding::CallImp:  return callImport<FloatClass>(TypeId::F64,in.immU32());
				case F64OpEncoding::Cond:     return cond<F64Type>();
				case F64OpEncoding::Comma:    return comma<F64Type>();
				case F64OpEncoding::FromS32:  return castI32ToFloat(FloatOp::convertSignedInt);
				case F64OpEncoding::FromU32:  return castI32ToFloat(FloatOp::convertUnsignedInt);
				case F64OpEncoding::FromF32:  return new(arena) Cast<FloatClass>(FloatOp::promote,TypedExpression(decodeExpression(F32Type()),TypeId::F32));
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
				case F64OpEncoding::Cos:     return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"cos");
				case F64OpEncoding::Sin:      return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"sin");
				case F64OpEncoding::Tan:      return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"tan");
				case F64OpEncoding::ACos:     return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"acos");
				case F64OpEncoding::ASin:     return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"asin");
				case F64OpEncoding::ATan:     return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"atan");
				case F64OpEncoding::ATan2:    return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64,TypeId::F64}),"atan2");
				case F64OpEncoding::Exp:      return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"exp");
				case F64OpEncoding::Ln:       return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64}),"log");
				case F64OpEncoding::Pow:      return decodeIntrinsic<FloatClass>(FunctionType(TypeId::F64,{TypeId::F64,TypeId::F64}),"pow");
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
			uint8 imm;
			if(in.code(&statement,&statementWithImm,&imm))
			{
				switch(statement)
				{
				case StmtOpEncoding::SetLoc: return new(arena) DiscardResult(setLocal(in.immU32()));
				case StmtOpEncoding::SetGlo: return new(arena) DiscardResult(setGlobal(in.immU32()));
				case StmtOpEncoding::I32Store8:		return new(arena) DiscardResult(TypedExpression(store<I32Type>(TypeId::I8,decodeAddress(0)),TypeId::I32));
				case StmtOpEncoding::I32StoreOff8:	return new(arena) DiscardResult(TypedExpression(store<I32Type>(TypeId::I8,decodeAddress(in.immU32())),TypeId::I32));
				case StmtOpEncoding::I32Store16:		return new(arena) DiscardResult(TypedExpression(store<I32Type>(TypeId::I16,decodeAddress(0)),TypeId::I32));
				case StmtOpEncoding::I32StoreOff16:	return new(arena) DiscardResult(TypedExpression(store<I32Type>(TypeId::I16,decodeAddress(in.immU32())),TypeId::I32));
				case StmtOpEncoding::I32Store32:		return new(arena) DiscardResult(TypedExpression(store<I32Type>(TypeId::I32,decodeAddress(0)),TypeId::I32));
				case StmtOpEncoding::I32StoreOff32:	return new(arena) DiscardResult(TypedExpression(store<I32Type>(TypeId::I32,decodeAddress(in.immU32())),TypeId::I32));
				case StmtOpEncoding::F32Store:		return new(arena) DiscardResult(TypedExpression(store<F32Type>(TypeId::F32,decodeAddress(0)),TypeId::F32));
				case StmtOpEncoding::F32StoreOff:	return new(arena) DiscardResult(TypedExpression(store<F32Type>(TypeId::F32,decodeAddress(in.immU32())),TypeId::F32));
				case StmtOpEncoding::F64Store:		return new(arena) DiscardResult(TypedExpression(store<F64Type>(TypeId::F64,decodeAddress(0)),TypeId::F64));
				case StmtOpEncoding::F64StoreOff:	return new(arena) DiscardResult(TypedExpression(store<F64Type>(TypeId::F64,decodeAddress(in.immU32())),TypeId::F64));
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
				case StmtOpEncoding::Switch: return decodeSwitch(isEnclosedByLabel);
				default: throw new FatalDecodeException("invalid statement opcode");
				}
			}
			else
			{
				switch(statementWithImm)
				{
				case StmtOpEncodingWithImm::SetLoc: return new(arena) DiscardResult(setLocal(imm));
				case StmtOpEncodingWithImm::SetGlo: return new(arena) DiscardResult(setGlobal(imm));
				default: throw new FatalDecodeException("invalid statement opcode");
				}
			}
		}

		VoidExpression* decodeStatementList()
		{
			uint32 numStatements = in.immU32();
			if(numStatements == 0) { return Nop::get(); }
			else
			{
				VoidExpression* result = decodeStatement();
				for(uint32 statementIndex = 1;statementIndex < numStatements;++statementIndex)
				{
					result = new(arena) Sequence<VoidClass>(result,decodeStatement());
				}
				return result;
			}
		}

		void decodeFunctions()
		{
			for(size_t functionIndex = 0; functionIndex < module.functions.size(); functionIndex++)
			{
				currentFunction = module.functions[functionIndex];

				// Decode the number of local variables used by the function.
				uint32 numLocalI32s = 0;
				uint32 numLocalF32s = 0;
				uint32 numLocalF64s = 0;
				VaReturnTypes varTypes;
				VaReturnTypesWithImm varTypesWithImm;
				uint8 imm;
				if(in.code(&varTypes,&varTypesWithImm,&imm))
				{
					if(varTypes & VaReturnTypes::I32) numLocalI32s = in.immU32();
					if(varTypes & VaReturnTypes::F32) numLocalF32s = in.immU32();
					if(varTypes & VaReturnTypes::F64) numLocalF64s = in.immU32();
				}
				else numLocalI32s = imm;
				
				currentFunction->locals.resize(currentFunction->type.parameters.size() + numLocalI32s + numLocalF32s + numLocalF64s);
				uintptr localIndex = 0;
				
				// Create locals for the function's parameters.
				currentFunction->parameterLocalIndices.resize(currentFunction->type.parameters.size());
				for(uintptr parameterIndex = 0;parameterIndex < currentFunction->type.parameters.size();++parameterIndex)
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
				case TypeId::I32: currentFunction->expression = new(arena) Sequence<IntClass>(voidExpression,new(arena) Literal<I32Type>(0)); break;
				case TypeId::F32: currentFunction->expression = new(arena) Sequence<FloatClass>(voidExpression,new(arena) Literal<F32Type>(0.0f)); break;
				case TypeId::F64: currentFunction->expression = new(arena) Sequence<FloatClass>(voidExpression,new(arena) Literal<F64Type>(0.0)); break;
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
				uint32 numExports = in.immU32();
				for(uint32 exportIndex = 0;exportIndex < numExports;++exportIndex)
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
			uint32 numTypes = in.immU32();
			functionTypes.resize(numTypes);
			for(uint32 typeIndex = 0;typeIndex < numTypes;++typeIndex)
			{
				functionTypes[typeIndex].returnType = in.returnType();
				uint32 numParameters = in.immU32();
				functionTypes[typeIndex].parameters.resize(numParameters);
				for(uint32 parameterIndex = 0; parameterIndex < numParameters; parameterIndex++)
				{ functionTypes[typeIndex].parameters[parameterIndex] = in.type(); }
			}
		}

		// Decode the table of function imports used in the module.
		void decodeFunctionImports()
		{
			uint32 numFunctionImports = in.immU32();
			uint32 numFunctionImportTypes = in.immU32();

			module.functionImports.resize(0);
			module.functionImports.reserve(numFunctionImportTypes);
			for(uint32 functionImportIndex = 0; functionImportIndex < numFunctionImports; functionImportIndex++)
			{
				Memory::ArenaString importName;
				while(char c = in.single_char()) { importName.append(arena,c); };

				auto numTypes = in.immU32();
				for(uint32 typeIndex = 0;typeIndex < numTypes;++typeIndex)
				{ module.functionImports.push_back({functionTypes[in.boundedImmU32("function type index",functionTypes.size())],"emscripten",importName.c_str()}); }
			}
		}

		void allocateGlobal(TypeId type)
		{
			auto numBytes = getTypeByteWidth(type);
			auto data = new(arena) uint8[numBytes];
			memset(data,0,numBytes);
			module.dataSegments.push_back({module.initialNumBytesMemory,numBytes,data});
			globals.push_back({type,module.initialNumBytesMemory});
			module.initialNumBytesMemory += numBytes;
		}

		void addVariableImport(TypeId type,const char* name)
		{
			auto numNameChars = strlen(name);

			Memory::ArenaString getterName;
			getterName.append(arena,"get_");
			getterName.append(arena,name,numNameChars);
			getterName.shrink(arena);
			auto getterFunctionIndex = getUniqueFunctionImport(FunctionType(type,{}),getterName.c_str());
			
			Memory::ArenaString setterName;
			setterName.append(arena,"set_");
			setterName.append(arena,name,numNameChars);
			setterName.shrink(arena);
			auto setterFunctionIndex = getUniqueFunctionImport(FunctionType(type,{type}),setterName.c_str());

			variableImports.push_back({type,getterFunctionIndex,setterFunctionIndex});
		}

		// Decodes the module's global variables.
		void decodeGlobals()
		{
			uint32 numGlobalsI32 = in.immU32();
			uint32 numGlobalsF32 = in.immU32();
			uint32 numGlobalsF64 = in.immU32();
			uint32 numImportsI32 = in.immU32();
			uint32 numImportsF32 = in.immU32();
			uint32 numImportsF64 = in.immU32();

			globals.reserve(numGlobalsI32 + numGlobalsF32 + numGlobalsF64);
			variableImports.reserve(numImportsI32 + numImportsF32 + numImportsF64);

			for(uint32 variableIndex = 0;variableIndex < numGlobalsI32;++variableIndex) { allocateGlobal(TypeId::I32); }
			for(uint32 variableIndex = 0;variableIndex < numGlobalsF32;++variableIndex) { allocateGlobal(TypeId::F32); }
			for(uint32 variableIndex = 0;variableIndex < numGlobalsF64;++variableIndex) { allocateGlobal(TypeId::F64); }
			
			Memory::ScopedArena scopedArena;
			Memory::ArenaString importName;
			for(uint32 variableIndex = 0;variableIndex < numImportsI32;++variableIndex)
			{
				while(char c = in.single_char()) { importName.append(scopedArena,c); };
				addVariableImport(TypeId::I32,importName.c_str());
				importName.reset(scopedArena);
			}
			for(uint32 i = 0; i < numImportsF32; ++i)
			{
				while(char c = in.single_char()) { importName.append(scopedArena,c); };
				addVariableImport(TypeId::F32,importName.c_str());
				importName.reset(scopedArena);
			}
			for(uint32 i = 0; i < numImportsF64; ++i)
			{
				while(char c = in.single_char()) { importName.append(scopedArena,c); };
				addVariableImport(TypeId::F64,importName.c_str());
				importName.reset(scopedArena);
			}
		}

		// Decodes the constant table: constants that may be used by index throughout the module.
		void decodeConstantPool()
		{
			uint32 numI32s = in.immU32();
			uint32 numF32s = in.immU32();
			uint32 numF64s = in.immU32();

			i32Constants.resize(numI32s);
			f32Constants.resize(numF32s);
			f64Constants.resize(numF64s);

			for(uint32 i = 0; i < numI32s; ++i) { i32Constants[i] = in.immU32(); }
			for(uint32 i = 0; i < numF32s; ++i) { f32Constants[i] = in.f32(); }
			for(uint32 i = 0; i < numF64s; ++i) { f64Constants[i] = in.f64(); }
		}

		// Decodes the types of the functions defined in the module.
		void decodeFunctionDeclarations()
		{
			const uint32 numFunctions = in.immU32();
			module.functions.resize(numFunctions);
			for(uint32 functionIndex = 0;functionIndex < numFunctions;++functionIndex)
			{
				auto function = new(arena) Function;
				module.functions[functionIndex] = function;
				function->type = functionTypes[in.boundedImmU32("function type index",functionTypes.size())];
			}
		}

		// Decode the function pointer tables: groups of functions that may be called indirectly, grouped by type.
		void decodeFunctionPointerTables()
		{
			const uint32 numFunctionPointerTables = in.immU32();
			module.functionTables.resize(numFunctionPointerTables);
			for(uint32 tableIndex = 0;tableIndex < numFunctionPointerTables;++tableIndex)
			{
				auto& table = module.functionTables[tableIndex];
				table.type = functionTypes[in.boundedImmU32("function type index",functionTypes.size())];

				// Decode the function table elements.
				table.numFunctions = in.immU32();
				table.functionIndices = new(arena) uintptr[table.numFunctions];
				for(uint32 functionIndex = 0;functionIndex < table.numFunctions;++functionIndex)
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

	bool decode(const uint8* code,size_t numCodeBytes,const uint8* data,size_t numDataBytes,AST::Module*& outModule,std::vector<AST::ErrorRecord*>& outErrors)
	{
		outModule = new Module;

		// Create a segment for the static data.
		auto segmentArenaData = outModule->arena.copyToArena(data,numDataBytes);
		outModule->dataSegments.push_back({8,numDataBytes,segmentArenaData});
		outModule->initialNumBytesMemory = numDataBytes + 8;
		outModule->maxNumBytesMemory = 1ull << 32;

		try
		{
			InputStream in(code,code + numCodeBytes);
			return DecodeContext(in,*outModule,outErrors).decode();
		}
		catch(FatalDecodeException* exception)
		{
			outErrors.push_back(exception);
			return false;
		}
	}
}
