#include "Core/MemoryArena.h"
#include "WebAssembly.h"
#include "Module.h"
#include "Operations.h"
#include "OperatorLoggingProxy.h"

#define ALLOW_MUTABLE_GLOBALS 1

#define ENABLE_LOGGING 0

#if ENABLE_LOGGING
	#include <cstdio>
#endif

namespace WebAssembly
{
	#define VALIDATE_UNLESS(reason,comparison) \
		if(comparison) \
		{ \
			throw ValidationException(reason #comparison); \
		}

	#define VALIDATE_INDEX(index,arraySize) VALIDATE_UNLESS("invalid index: ",index>=arraySize)

	void validate(ValueType valueType)
	{
		if(valueType == ValueType::invalid || valueType > ValueType::max)
		{
			throw ValidationException("invalid value type (" + std::to_string((uintptr)valueType) + ")");
		}
	}

	void validate(ReturnType returnType)
	{
		if(returnType > ReturnType::max)
		{
			throw ValidationException("invalid return type (" + std::to_string((uintptr)returnType) + ")");
		}
	}

	void validate(ObjectKind kind)
	{
		if(kind > ObjectKind::max)
		{
			throw ValidationException("invalid external kind (" + std::to_string((uintptr)kind) + ")");
		}
	}

	void validate(SizeConstraints size,size_t maxMax)
	{
		size_t max = size.max == UINT64_MAX ? maxMax : size.max;
		VALIDATE_UNLESS("disjoint size bounds: ",size.min>max);
		VALIDATE_UNLESS("maximum size exceeds limit: ",max>maxMax);
	}

	void validate(TableElementType type)
	{
		if(type != TableElementType::anyfunc) { throw ValidationException("invalid table element type (" + std::to_string((uintptr)type) + ")"); }
	}

	void validate(GlobalType type)
	{
		validate(type.valueType);
	}
	
	void validateImportKind(ObjectType importType,ObjectKind expectedKind)
	{
		if(importType.kind != expectedKind)
		{
			throw ValidationException("incorrect kind");
		}
	}

	template<typename Type>
	void validateType(Type expectedType,Type type,const char* context)
	{
		if(expectedType != type)
		{
			throw ValidationException(
				std::string("type mismatch: expected ") + getTypeName(expectedType)
				+ " but got " + getTypeName(type)
				+ " in " + context
				);
		}
	}

	struct ModuleValidationContext
	{
		uintptr numTables;
		uintptr numMemories;

		ModuleValidationContext(const Module& inModule);

		ValueType validateGlobalIndex(uintptr globalIndex,bool mustBeMutable,bool mustBeImmutable,bool mustBeImport,const char* context)
		{
			assert(numImportedGlobals != UINTPTR_MAX);
			VALIDATE_INDEX(globalIndex,globals.size());
			const GlobalType& globalType = globals[globalIndex];
			if(mustBeMutable && !globalType.isMutable) { throw ValidationException("attempting to mutate immutable global"); }
			else if(mustBeImport && globalIndex >= numImportedGlobals) { throw ValidationException("global variable initializer expression may only access imported globals"); }
			else if(mustBeImmutable && globalType.isMutable) { throw ValidationException("global variable initializer expression may only access immutable globals"); }
			return globalType.valueType;
		}

		const FunctionType* validateFunctionIndex(uintptr functionIndex)
		{
			VALIDATE_INDEX(functionIndex,functions.size());
			return functions[functionIndex];
		}

	private:
			
		const Module& module;
		std::vector<GlobalType> globals;
		uintptr numImportedGlobals;
		std::vector<const FunctionType*> functions;
		
		void validateInitializer(const InitializerExpression& expression,ValueType expectedType,const char* context)
		{
			switch(expression.type)
			{
			case InitializerExpression::Type::i32_const:
			case InitializerExpression::Type::i64_const:
			case InitializerExpression::Type::f32_const:
			case InitializerExpression::Type::f64_const:
				break;
			case InitializerExpression::Type::get_global:
			{
				const ValueType globalValueType = validateGlobalIndex(expression.globalIndex,false,true,true,"initializer expression global index");
				validateType(expectedType,globalValueType,context);
				break;
			}
			default:
				throw ValidationException("invalid initializer expression");
			};
		}
	};

	void validate(const Module& module)
	{
		ModuleValidationContext context(module);
	}

	struct FunctionCodeValidator
	{
		FunctionCodeValidator(ModuleValidationContext& inModuleContext,const Module& inModule,const Function& inFunction)
		: moduleContext(inModuleContext), module(inModule), function(inFunction), functionType(inModule.types[inFunction.typeIndex])
		{
			// Initialize the local types.
			locals.reserve(functionType->parameters.size() + function.nonParameterLocalTypes.size());
			locals = functionType->parameters;
			locals.insert(locals.end(),function.nonParameterLocalTypes.begin(),function.nonParameterLocalTypes.end());

			// Push the function context onto the control stack.
			pushControlStack(ControlContext::Type::function,functionType->ret,functionType->ret);

			Serialization::InputStream codeStream(module.code.data() + function.code.offset,function.code.numBytes);
			OperationDecoder decoder(codeStream);
			OperatorLoggingProxy<FunctionCodeValidator,ENABLE_LOGGING> loggingProxy(module,*this);
			logOperator("---- function start ----");
			while(decoder && controlStack.size()) { decoder.decodeOp(loggingProxy); };

			if(decoder) { throw ValidationException("function end reached before end of code"); }
			if(controlStack.size()) { throw ValidationException("end of code reached before end of function"); }
		}
		
		void logOperator(const std::string& operatorDescription)
		{
			#if ENABLE_LOGGING
				std::string controlStackString;
				for(uintptr stackIndex = 0;stackIndex < controlStack.size();++stackIndex)
				{
					switch(controlStack[stackIndex].type)
					{
					case ControlContext::Type::function: controlStackString += "F"; break;
					case ControlContext::Type::block: controlStackString += "B"; break;
					case ControlContext::Type::ifWithoutElse: controlStackString += "I"; break;
					case ControlContext::Type::ifThen: controlStackString += "T"; break;
					case ControlContext::Type::ifElse: controlStackString += "E"; break;
					case ControlContext::Type::loop: controlStackString += "L"; break;
					default: throw;
					};
				}

				std::string stackString;
				const uintptr stackBase = controlStack.size() == 0 ? 0 : controlStack.back().outerStackSize;
				for(uintptr stackIndex = 0;stackIndex < stack.size();++stackIndex)
				{
					if(stackIndex == stackBase) { stackString += "| "; }
					stackString +=  getTypeName(stack[stackIndex]);
					stackString +=  " ";
				}
				if(stack.size() == stackBase) { stackString += "|"; }
			
				std::printf("%-50s %-50s %-50s\n",controlStackString.c_str(),operatorDescription.c_str(),stackString.c_str());
				std::fflush(stdout);
			#endif
		}

		// Operation dispatch methods.
		void beginBlock(ControlStructureImmediates immediates)
		{
			VALIDATE_INDEX(immediates.signatureTypeIndex,module.types.size());
			const FunctionType* signature = module.types[immediates.signatureTypeIndex];
			VALIDATE_UNLESS("block signature may not take parameters: ",signature->parameters.size() > 0);
			pushControlStack(ControlContext::Type::block,signature->ret,signature->ret);
		}
		void beginLoop(ControlStructureImmediates immediates)
		{
			VALIDATE_INDEX(immediates.signatureTypeIndex,module.types.size());
			const FunctionType* signature = module.types[immediates.signatureTypeIndex];
			VALIDATE_UNLESS("loop signature may not take parameters: ",signature->parameters.size() > 0);
			pushControlStack(ControlContext::Type::loop,ReturnType::unit,signature->ret);
		}
		void beginIf(NoImmediates)
		{
			popAndValidateOperand(ValueType::i32);
			pushControlStack(ControlContext::Type::ifWithoutElse,ReturnType::unit,ReturnType::unit);
		}
		void beginIfElse(ControlStructureImmediates immediates)
		{
			VALIDATE_INDEX(immediates.signatureTypeIndex,module.types.size());
			const FunctionType* signature = module.types[immediates.signatureTypeIndex];
			VALIDATE_UNLESS("if_else signature may not take parameters: ",signature->parameters.size() > 0);
			popAndValidateOperand(ValueType::i32);
			pushControlStack(ControlContext::Type::ifThen,signature->ret,signature->ret);
		}
		void end(NoImmediates)
		{
			popAndValidateReturnType(controlStack.back().resultType);
			popControlStack();
		}
		
		void ret(NoImmediates)
		{
			popAndValidateReturnType(functionType->ret);
			popControlStack();
		}

		void br(BranchImmediates immediates)
		{
			popAndValidateReturnType(getBranchTargetByDepth(immediates.targetDepth).branchArgumentType);
			popControlStack();
		}
		void br_table(BranchTableImmediates immediates)
		{
			popAndValidateOperand(ValueType::i32);
			const ReturnType defaultTargetArgumentType = getBranchTargetByDepth(immediates.defaultTargetDepth).branchArgumentType;
			popAndValidateReturnType(defaultTargetArgumentType);

			for(uintptr targetIndex = 0;targetIndex < immediates.targetDepths.size();++targetIndex)
			{
				const ReturnType targetArgumentType = getBranchTargetByDepth(immediates.targetDepths[targetIndex]).branchArgumentType;
				VALIDATE_UNLESS("br_table target argument must match default target argument: ",targetArgumentType != defaultTargetArgumentType);
			}

			popControlStack();
		}
		void br_if(BranchImmediates immediates)
		{
			popAndValidateOperand(ValueType::i32);
			popAndValidateReturnType(getBranchTargetByDepth(immediates.targetDepth).branchArgumentType);
		}

		void nop(NoImmediates) {}
		void unreachable(NoImmediates) { popControlStack(); }
		void drop(NoImmediates) { validateStackAccess(1); stack.pop_back(); }

		void select(NoImmediates)
		{
			validateStackAccess(3);
			ValueType operandType = stack[stack.size() - 3];
			popAndValidateOperands(operandType,ValueType::i32);
		}

		void get_local(GetOrSetVariableImmediates immediates)
		{
			push(validateLocalIndex(immediates.variableIndex));
		}
		void set_local(GetOrSetVariableImmediates immediates)
		{
			popAndValidateOperand(validateLocalIndex(immediates.variableIndex));
		}
		void tee_local(GetOrSetVariableImmediates immediates)
		{
			const ValueType localType = validateLocalIndex(immediates.variableIndex);
			popAndValidateOperand(localType);
			push(localType);
		}
		
		void get_global(GetOrSetVariableImmediates immediates)
		{
			push(moduleContext.validateGlobalIndex(immediates.variableIndex,false,false,false,"get_global"));
		}
		void set_global(GetOrSetVariableImmediates immediates)
		{
			popAndValidateOperand(moduleContext.validateGlobalIndex(immediates.variableIndex,true,false,false,"set_global"));
		}

		void call(CallImmediates immediates)
		{
			const FunctionType* calleeType = moduleContext.validateFunctionIndex(immediates.functionIndex);
			popAndValidateOperands(calleeType->parameters.data(),calleeType->parameters.size());
			push(calleeType->ret);
		}
		void call_indirect(CallIndirectImmediates immediates)
		{
			VALIDATE_INDEX(immediates.typeIndex,module.types.size());
			VALIDATE_UNLESS("call_indirect in module without default function table: ",moduleContext.numTables==0);
			const FunctionType* calleeType = module.types[immediates.typeIndex];
			popAndValidateOperand(ValueType::i32);
			popAndValidateOperands(calleeType->parameters.data(),calleeType->parameters.size());
			push(calleeType->ret);
		}

		void grow_memory(NoImmediates) { popAndValidateOperand(ValueType::i32); push(ValueType::i32); }
		void current_memory(NoImmediates) { push(ValueType::i32); }

		void error(ErrorImmediates immediates) { throw ValidationException("error opcode"); }

		#define VALIDATE_CONST(typeId,nativeType) \
			void typeId##_const(LiteralImmediates<nativeType> immediates) { push(ValueType::typeId); }
		VALIDATE_CONST(i32,int32); VALIDATE_CONST(i64,int64);
		VALIDATE_CONST(f32,float32); VALIDATE_CONST(f64,float64);

		#define VALIDATE_LOAD_OPCODE(name,naturalAlignmentLog2,resultType) void name(LoadOrStoreImmediates immediates) \
			{ \
				popAndValidateOperand(ValueType::i32); \
				VALIDATE_UNLESS("load alignment greater than natural alignment: ",immediates.alignmentLog2>naturalAlignmentLog2); \
				VALIDATE_UNLESS(#name " in module without default memory: ",moduleContext.numMemories==0); \
				push(ValueType::resultType); \
			}

		VALIDATE_LOAD_OPCODE(i32_load8_s,1,i32)  VALIDATE_LOAD_OPCODE(i32_load8_u,1,i32)
		VALIDATE_LOAD_OPCODE(i32_load16_s,2,i32) VALIDATE_LOAD_OPCODE(i32_load16_u,2,i32)
		VALIDATE_LOAD_OPCODE(i64_load8_s,1,i64)  VALIDATE_LOAD_OPCODE(i64_load8_u,1,i64)
		VALIDATE_LOAD_OPCODE(i64_load16_s,2,i64)  VALIDATE_LOAD_OPCODE(i64_load16_u,2,i64)
		VALIDATE_LOAD_OPCODE(i64_load32_s,4,i64)  VALIDATE_LOAD_OPCODE(i64_load32_u,4,i64)

		VALIDATE_LOAD_OPCODE(i32_load,4,i32) VALIDATE_LOAD_OPCODE(i64_load,8,i64)
		VALIDATE_LOAD_OPCODE(f32_load,4,f32) VALIDATE_LOAD_OPCODE(f64_load,8,f64)
			
		#define VALIDATE_STORE_OPCODE(name,naturalAlignmentLog2,valueTypeId) void name(LoadOrStoreImmediates immediates) \
			{ \
				popAndValidateOperands(ValueType::i32,ValueType::valueTypeId); \
				VALIDATE_UNLESS("store alignment greater than natural alignment: ",immediates.alignmentLog2>naturalAlignmentLog2); \
				VALIDATE_UNLESS(#name " in module without default memory: ",moduleContext.numMemories==0); \
			}

		VALIDATE_STORE_OPCODE(i32_store8,1,i32) VALIDATE_STORE_OPCODE(i32_store16,2,i32) VALIDATE_STORE_OPCODE(i32_store,4,i32)
		VALIDATE_STORE_OPCODE(i64_store8,1,i64) VALIDATE_STORE_OPCODE(i64_store16,2,i64) VALIDATE_STORE_OPCODE(i64_store32,4,i64) VALIDATE_STORE_OPCODE(i64_store,8,i64)
		VALIDATE_STORE_OPCODE(f32_store,4,f32) VALIDATE_STORE_OPCODE(f64_store,8,f64)

		#define VALIDATE_BINARY_OPCODE(name,operandTypeId,resultTypeId) void name(NoImmediates) \
			{ \
				popAndValidateOperands(ValueType::operandTypeId,ValueType::operandTypeId); \
				push(ValueType::resultTypeId); \
			}
		#define VALIDATE_UNARY_OPCODE(name,operandTypeId,resultTypeId) void name(NoImmediates) \
			{ \
				popAndValidateOperand(ValueType::operandTypeId); \
				push(ValueType::resultTypeId); \
			}

		VALIDATE_BINARY_OPCODE(i32_add,i32,i32) VALIDATE_BINARY_OPCODE(i64_add,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_sub,i32,i32) VALIDATE_BINARY_OPCODE(i64_sub,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_mul,i32,i32) VALIDATE_BINARY_OPCODE(i64_mul,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_div_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_div_s,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_div_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_div_u,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_rem_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_rem_s,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_rem_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_rem_u,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_and,i32,i32) VALIDATE_BINARY_OPCODE(i64_and,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_or,i32,i32) VALIDATE_BINARY_OPCODE(i64_or,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_xor,i32,i32) VALIDATE_BINARY_OPCODE(i64_xor,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_shl,i32,i32) VALIDATE_BINARY_OPCODE(i64_shl,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_shr_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_shr_u,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_shr_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_shr_s,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_rotr,i32,i32) VALIDATE_BINARY_OPCODE(i64_rotr,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_rotl,i32,i32) VALIDATE_BINARY_OPCODE(i64_rotl,i64,i64)
		VALIDATE_BINARY_OPCODE(i32_eq,i32,i32) VALIDATE_BINARY_OPCODE(i64_eq,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_ne,i32,i32) VALIDATE_BINARY_OPCODE(i64_ne,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_lt_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_lt_s,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_le_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_le_s,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_lt_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_lt_u,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_le_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_le_u,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_gt_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_gt_s,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_ge_s,i32,i32) VALIDATE_BINARY_OPCODE(i64_ge_s,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_gt_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_gt_u,i64,i32)
		VALIDATE_BINARY_OPCODE(i32_ge_u,i32,i32) VALIDATE_BINARY_OPCODE(i64_ge_u,i64,i32)
		VALIDATE_UNARY_OPCODE(i32_clz,i32,i32) VALIDATE_UNARY_OPCODE(i64_clz,i64,i64)
		VALIDATE_UNARY_OPCODE(i32_ctz,i32,i32) VALIDATE_UNARY_OPCODE(i64_ctz,i64,i64)
		VALIDATE_UNARY_OPCODE(i32_popcnt,i32,i32) VALIDATE_UNARY_OPCODE(i64_popcnt,i64,i64)
		VALIDATE_UNARY_OPCODE(i32_eqz,i32,i32) VALIDATE_UNARY_OPCODE(i64_eqz,i64,i32)

		VALIDATE_BINARY_OPCODE(f32_add,f32,f32) VALIDATE_BINARY_OPCODE(f64_add,f64,f64)
		VALIDATE_BINARY_OPCODE(f32_sub,f32,f32) VALIDATE_BINARY_OPCODE(f64_sub,f64,f64)
		VALIDATE_BINARY_OPCODE(f32_mul,f32,f32) VALIDATE_BINARY_OPCODE(f64_mul,f64,f64)
		VALIDATE_BINARY_OPCODE(f32_div,f32,f32) VALIDATE_BINARY_OPCODE(f64_div,f64,f64)
		VALIDATE_BINARY_OPCODE(f32_min,f32,f32) VALIDATE_BINARY_OPCODE(f64_min,f64,f64)
		VALIDATE_BINARY_OPCODE(f32_max,f32,f32) VALIDATE_BINARY_OPCODE(f64_max,f64,f64)
		VALIDATE_BINARY_OPCODE(f32_copysign,f32,f32) VALIDATE_BINARY_OPCODE(f64_copysign,f64,f64)

		VALIDATE_BINARY_OPCODE(f32_eq,f32,i32) VALIDATE_BINARY_OPCODE(f64_eq,f64,i32)
		VALIDATE_BINARY_OPCODE(f32_ne,f32,i32) VALIDATE_BINARY_OPCODE(f64_ne,f64,i32)
		VALIDATE_BINARY_OPCODE(f32_lt,f32,i32) VALIDATE_BINARY_OPCODE(f64_lt,f64,i32)
		VALIDATE_BINARY_OPCODE(f32_le,f32,i32) VALIDATE_BINARY_OPCODE(f64_le,f64,i32)
		VALIDATE_BINARY_OPCODE(f32_gt,f32,i32) VALIDATE_BINARY_OPCODE(f64_gt,f64,i32)
		VALIDATE_BINARY_OPCODE(f32_ge,f32,i32) VALIDATE_BINARY_OPCODE(f64_ge,f64,i32)

		VALIDATE_UNARY_OPCODE(f32_abs,f32,f32) VALIDATE_UNARY_OPCODE(f64_abs,f64,f64)
		VALIDATE_UNARY_OPCODE(f32_neg,f32,f32) VALIDATE_UNARY_OPCODE(f64_neg,f64,f64)
		VALIDATE_UNARY_OPCODE(f32_ceil,f32,f32) VALIDATE_UNARY_OPCODE(f64_ceil,f64,f64)
		VALIDATE_UNARY_OPCODE(f32_floor,f32,f32) VALIDATE_UNARY_OPCODE(f64_floor,f64,f64)
		VALIDATE_UNARY_OPCODE(f32_trunc,f32,f32) VALIDATE_UNARY_OPCODE(f64_trunc,f64,f64)
		VALIDATE_UNARY_OPCODE(f32_nearest,f32,f32) VALIDATE_UNARY_OPCODE(f64_nearest,f64,f64)
		VALIDATE_UNARY_OPCODE(f32_sqrt,f32,f32) VALIDATE_UNARY_OPCODE(f64_sqrt,f64,f64)

		VALIDATE_UNARY_OPCODE(i32_trunc_s_f32,f32,i32)
		VALIDATE_UNARY_OPCODE(i32_trunc_s_f64,f64,i32)
		VALIDATE_UNARY_OPCODE(i32_trunc_u_f32,f32,i32)
		VALIDATE_UNARY_OPCODE(i32_trunc_u_f64,f64,i32)
		VALIDATE_UNARY_OPCODE(i32_wrap_i64,i64,i32)
		VALIDATE_UNARY_OPCODE(i64_trunc_s_f32,f32,i64)
		VALIDATE_UNARY_OPCODE(i64_trunc_s_f64,f64,i64)
		VALIDATE_UNARY_OPCODE(i64_trunc_u_f32,f32,i64)
		VALIDATE_UNARY_OPCODE(i64_trunc_u_f64,f64,i64)
		VALIDATE_UNARY_OPCODE(i64_extend_s_i32,i32,i64)
		VALIDATE_UNARY_OPCODE(i64_extend_u_i32,i32,i64)
		VALIDATE_UNARY_OPCODE(f32_convert_s_i32,i32,f32)
		VALIDATE_UNARY_OPCODE(f32_convert_u_i32,i32,f32)
		VALIDATE_UNARY_OPCODE(f32_convert_s_i64,i64,f32)
		VALIDATE_UNARY_OPCODE(f32_convert_u_i64,i64,f32)
		VALIDATE_UNARY_OPCODE(f32_demote_f64,f64,f32)
		VALIDATE_UNARY_OPCODE(f32_reinterpret_i32,i32,f32)
		VALIDATE_UNARY_OPCODE(f64_convert_s_i32,i32,f64)
		VALIDATE_UNARY_OPCODE(f64_convert_u_i32,i32,f64)
		VALIDATE_UNARY_OPCODE(f64_convert_s_i64,i64,f64)
		VALIDATE_UNARY_OPCODE(f64_convert_u_i64,i64,f64)
		VALIDATE_UNARY_OPCODE(f64_promote_f32,f32,f64)
		VALIDATE_UNARY_OPCODE(f64_reinterpret_i64,i64,f64)
		VALIDATE_UNARY_OPCODE(i32_reinterpret_f32,f32,i32)
		VALIDATE_UNARY_OPCODE(i64_reinterpret_f64,f64,i64)

	private:
		
		struct ControlContext
		{
			enum class Type : uint8
			{
				function,
				block,
				ifWithoutElse,
				ifThen,
				ifElse,
				loop
			};

			Type type;
			uintptr outerStackSize;
			
			ReturnType branchArgumentType;
			ReturnType resultType;
		};

		ModuleValidationContext& moduleContext;
		const Module& module;
		const Function& function;
		const FunctionType* functionType;

		std::vector<ValueType> locals;
		std::vector<ControlContext> controlStack;
		std::vector<ValueType> stack;

		void pushControlStack(ControlContext::Type type,ReturnType branchArgumentType,ReturnType resultType)
		{
			controlStack.push_back({type,stack.size(),branchArgumentType,resultType});
		}

		void popControlStack()
		{
			stack.resize(controlStack.back().outerStackSize);
			if(controlStack.back().type == ControlContext::Type::ifThen)
			{
				controlStack.back().type = ControlContext::Type::ifElse;
			}
			else
			{
				push(controlStack.back().resultType);
				controlStack.pop_back();
			}
		}

		void validateStackAccess(size_t num)
		{
			const uintptr stackBase = controlStack.back().outerStackSize;
			if(stack.size() < stackBase + num) { throw ValidationException("invalid stack access"); }
		}

		void validateBranchDepth(uintptr depth) const
		{
			VALIDATE_INDEX(depth,controlStack.size());
			if(depth >= controlStack.size()) { throw ValidationException("invalid branch depth"); }
		}

		const ControlContext& getBranchTargetByDepth(uintptr depth) const
		{
			validateBranchDepth(depth);
			return controlStack[controlStack.size() - depth - 1];
		}

		ValueType validateLocalIndex(uintptr localIndex)
		{
			VALIDATE_INDEX(localIndex,locals.size());
			return locals[localIndex];
		}

		void popAndValidateOperands(const ValueType* expectedTypes,size_t num)
		{
			validateStackAccess(num);
			for(uintptr operandIndex = 0;operandIndex < num;++operandIndex)
			{ validateType(stack[stack.size()-num+operandIndex],expectedTypes[operandIndex],"operand"); }
			stack.resize(stack.size() - num);
		}

		template<size_t num>
		void popAndValidateOperands(const ValueType (&expectedTypes)[num]) { popAndValidateOperands(expectedTypes,num); }
		
		template<typename... OperandTypes>
		void popAndValidateOperands(OperandTypes... operands)
		{
			ValueType operandTypes[] = {operands...};
			popAndValidateOperands(operandTypes);
		}

		void popAndValidateOperand(const ValueType expectedType)
		{
			validateStackAccess(1);
			validateType(stack.back(),expectedType,"operand");
			stack.pop_back();
		}

		void popAndValidateReturnType(ReturnType expectedType)
		{
			if(expectedType != ReturnType::unit) { popAndValidateOperands(asValueType(expectedType)); }
		}

		void push(ValueType type) { stack.push_back(type); }
		void push(ReturnType type) { if(type != ReturnType::unit) { push(asValueType(type)); } }
	};

	ModuleValidationContext::ModuleValidationContext(const Module& inModule)
	: numTables(0), numMemories(0), module(inModule), numImportedGlobals(UINTPTR_MAX)
	{
		{
			std::map<const FunctionType*,uintptr> functionTypeMap;
			for(uintptr typeIndex = 0;typeIndex < module.types.size();++typeIndex)
			{
				const FunctionType* functionType = module.types[typeIndex];
				for(auto parameterType : functionType->parameters) { validate(parameterType); }
				validate(functionType->ret);
				if(functionTypeMap.count(functionType)) { throw ValidationException("duplicate type table entry"); }
				functionTypeMap[functionType] = typeIndex;
			}
		}

		for(auto& import : module.imports)
		{
			validate(import.type.kind);
			switch(import.type.kind)
			{
			case ObjectKind::function:
				VALIDATE_INDEX(import.type.functionTypeIndex,module.types.size());
				functions.push_back(module.types[import.type.functionTypeIndex]);
				break;
			case ObjectKind::table:
				validate(import.type.table.elementType);
				validate(import.type.table.size,UINTPTR_MAX);
				++numTables;
				break;
			case ObjectKind::memory:
				validate(import.type.memory.size,WebAssembly::maxMemoryPages);
				++numMemories;
				break;
			case ObjectKind::global:
				validate(import.type.global);
				#if !ALLOW_MUTABLE_GLOBALS
					VALIDATE_UNLESS("mutable globals cannot be imported: ",import.type.global.isMutable);
				#endif
				globals.push_back(import.type.global);
				break;
			default: throw;
			};
		}

		numImportedGlobals = globals.size();
		for(auto& global : module.globalDefs)
		{
			validate(global.type);
			validateInitializer(global.initializer,global.type.valueType,"global initializer expression");
			globals.push_back(global.type);
		}
		
		for(auto& table : module.tableDefs) { validate(table.size,UINT32_MAX); ++numTables; }
		VALIDATE_UNLESS("too many tables: ",numTables>1);

		for(auto& memory : module.memoryDefs) { validate(memory.size,65535); ++numMemories; }
		VALIDATE_UNLESS("too many memories: ",numMemories>1);

		for(uintptr functionIndex = 0;functionIndex < module.functionDefs.size();++functionIndex)
		{
			const Function& function = module.functionDefs[functionIndex];
			for(auto localType : function.nonParameterLocalTypes) { validate(localType); }
			VALIDATE_INDEX(function.typeIndex,module.types.size());
			functions.push_back(module.types[function.typeIndex]);
		}

		for(auto& exportIt : module.exports)
		{
			switch(exportIt.kind)
			{
			case ObjectKind::function:
				VALIDATE_INDEX(exportIt.index,functions.size());
				break;
			case ObjectKind::table:
				VALIDATE_INDEX(exportIt.index,numTables);
				break;
			case ObjectKind::memory:
				VALIDATE_INDEX(exportIt.index,numMemories);
				break;
			case ObjectKind::global:
				validateGlobalIndex(exportIt.index,false,false,false,"exported global index");
				break;
			default: throw;
			};
		}

		if(module.startFunctionIndex != UINTPTR_MAX)
		{ VALIDATE_INDEX(module.startFunctionIndex,functions.size()); }
			
		for(uintptr functionIndex = 0;functionIndex < module.functionDefs.size();++functionIndex)
		{ FunctionCodeValidator(*this,module,module.functionDefs[functionIndex]); }
			
		for(auto& dataSegment : module.dataSegments) { validateInitializer(dataSegment.baseOffset,ValueType::i32,"data segment base initializer"); }

		for(auto& tableSegment : module.tableSegments) { validateInitializer(tableSegment.baseOffset,ValueType::i32,"table segment base initializer"); }
			
		if(module.disassemblyInfo.functions.size())
		{
			VALIDATE_UNLESS("too many function disassembly infos: ",module.disassemblyInfo.functions.size()>module.functionDefs.size());
			for(uintptr functionIndex = 0;functionIndex < module.functionDefs.size();++functionIndex)
			{
				const Function& function = module.functionDefs[functionIndex];
				const FunctionDisassemblyInfo& functionDisassemblyInfo = module.disassemblyInfo.functions[functionIndex];
				const uintptr numLocals = module.types[function.typeIndex]->parameters.size() + function.nonParameterLocalTypes.size();
				VALIDATE_UNLESS("too many local disassembly infos: ",functionDisassemblyInfo.localNames.size()>numLocals);
			}
		}
	}
}