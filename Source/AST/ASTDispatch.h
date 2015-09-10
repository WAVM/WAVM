#pragma once

#include "AST.h"
#include "ASTExpressions.h"

namespace AST
{
	// Dispatch a literal to visitLiteral with the correct Literal<Type> cast.
	template<typename Visitor,typename Class>
	static typename Visitor::DispatchResult dispatchLiteral(Visitor& visitor,Expression<Class>* expression,TypeId type)
	{
		switch(type)
		{
		#define AST_TYPE(typeName,className,...) case TypeId::typeName: return visitor.visitLiteral((Literal<typeName##Type>*)expression);
			ENUM_AST_TYPES_NonVoid(AST_TYPE)
		#undef AST_TYPE
		default: throw;
		}
	}

	// Dispatch opcodes that can occur in any type context.
	template<typename Visitor,typename Class>
	static typename Visitor::DispatchResult dispatchAny(Visitor& visitor,Expression<Class>* expression,TypeId type)
	{
		switch((AnyOp)expression->op())
		{
		case AnyOp::error: return visitor.visitError(type,(Error<Class>*)expression);
		case AnyOp::callDirect: return visitor.visitCall(type,(Call*)expression,OpTypes<AnyClass>::callDirect());
		case AnyOp::callImport: return visitor.visitCall(type,(Call*)expression,OpTypes<AnyClass>::callImport());
		case AnyOp::callIndirect: return visitor.visitCallIndirect(type,(CallIndirect*)expression);
		case AnyOp::getLocal: return visitor.visitLoadVariable(type,(LoadVariable*)expression,OpTypes<AnyClass>::getLocal());
		case AnyOp::loadGlobal: return visitor.visitLoadVariable(type,(LoadVariable*)expression,OpTypes<AnyClass>::loadGlobal());
		case AnyOp::sequence: return visitor.visitSequence(type,(Sequence<Class>*)expression);
		case AnyOp::loop: return visitor.visitLoop(type,(Loop<Class>*)expression);
		case AnyOp::switch_: return visitor.visitSwitch(type,(Switch<Class>*)expression);
		case AnyOp::ifElse: return visitor.visitIfElse(type,(IfElse<Class>*)expression);
		case AnyOp::label: return visitor.visitLabel(type,(Label<Class>*)expression);
		case AnyOp::ret: return visitor.visitReturn((Return<Class>*)expression);
		case AnyOp::branch: return visitor.visitBranch((Branch<Class>*)expression);
		default: throw;
		}
	}

	// Dispatch opcodes that can occur in type contexts expecting an integer result.
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,Expression<IntClass>* expression,TypeId type)
	{
		switch(expression->op())
		{
		#define AST_OP(op) case IntOp::op: return visitor.visitUnary(type,(Unary<IntClass>*)expression,OpTypes<IntClass>::op());
		ENUM_AST_UNARY_OPS_Int()
		#undef AST_OP

		#define AST_OP(op) case IntOp::op: return visitor.visitBinary(type,(Binary<IntClass>*)expression,OpTypes<IntClass>::op());
		ENUM_AST_BINARY_OPS_Int()
		#undef AST_OP

		#define AST_OP(op) case IntOp::op: return visitor.visitCast(type,(Cast<IntClass>*)expression,OpTypes<IntClass>::op());
		ENUM_AST_CAST_OPS_Int()
		#undef AST_OP
		
		case IntOp::lit: return dispatchLiteral(visitor,expression,type);
		case IntOp::loadMemory: return visitor.visitLoadMemory(type,(LoadMemory<IntClass>*)expression);
		default: return dispatchAny(visitor,expression,type);
		}
	}

	// Dispatch opcodes that can occur in type contexts expecting a floating point result.
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,Expression<FloatClass>* expression,TypeId type)
	{
		switch(expression->op())
		{
		#define AST_OP(op) case FloatOp::op: return visitor.visitUnary(type,(Unary<FloatClass>*)expression,OpTypes<FloatClass>::op());
		ENUM_AST_UNARY_OPS_Float()
		#undef AST_OP

		#define AST_OP(op) case FloatOp::op: return visitor.visitBinary(type,(Binary<FloatClass>*)expression,OpTypes<FloatClass>::op());
		ENUM_AST_BINARY_OPS_Float()
		#undef AST_OP

		#define AST_OP(op) case FloatOp::op: return visitor.visitCast(type,(Cast<FloatClass>*)expression,OpTypes<FloatClass>::op());
		ENUM_AST_CAST_OPS_Float()
		#undef AST_OP
		
		case FloatOp::lit: return dispatchLiteral(visitor,expression,type);
		case FloatOp::loadMemory: return visitor.visitLoadMemory(type,(LoadMemory<FloatClass>*)expression);
		default: return dispatchAny(visitor,expression,type);
		}
	}

	// Dispatch opcodes that can occur in type contexts expecting a boolean result.
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,Expression<BoolClass>* expression,TypeId type = TypeId::Bool)
	{
		switch(expression->op())
		{
		#define AST_OP(op) case BoolOp::op: return visitor.visitUnary(type,(Unary<BoolClass>*)expression,OpTypes<BoolClass>::op());
		ENUM_AST_UNARY_OPS_Bool()
		#undef AST_OP

		#define AST_OP(op) case BoolOp::op: return visitor.visitBinary(type,(Binary<BoolClass>*)expression,OpTypes<BoolClass>::op());
		ENUM_AST_BINARY_OPS_Bool()
		#undef AST_OP

		#define AST_OP(op) case BoolOp::op: return visitor.visitComparison((Comparison*)expression,OpTypes<BoolClass>::op());
		ENUM_AST_COMPARISON_OPS()
		#undef AST_OP

		default: return dispatchAny(visitor,expression,type);
		}
	}

	// Dispatch opcodes that can occur in type contexts expecting a void result.
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,Expression<VoidClass>* expression,TypeId type = TypeId::Void)
	{
		switch(expression->op())
		{
		case VoidOp::setLocal: return visitor.visitStoreVariable((StoreVariable*)expression,OpTypes<VoidClass>::setLocal());
		case VoidOp::storeGlobal: return visitor.visitStoreVariable((StoreVariable*)expression,OpTypes<VoidClass>::storeGlobal());
		case VoidOp::storeMemory: return visitor.visitStoreMemory((StoreMemory*)expression);
		case VoidOp::nop: return visitor.visitNop((Nop*)expression);
		case VoidOp::discardResult: return visitor.visitDiscardResult((DiscardResult*)expression);
		default: return dispatchAny(visitor,expression,type);
		}
	}

	// Dispatch opcodes that can occur in contexts with the specified type.
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,UntypedExpression* expression,TypeId type)
	{
		switch(type)
		{
		#define AST_TYPE(typeName,className,...) case TypeId::typeName: return dispatch(visitor,as<className##Class>(expression),type);
			ENUM_AST_TYPES(AST_TYPE)
		#undef AST_TYPE
		default: throw;
		};
	}
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,const TypedExpression& expression)
	{
		return dispatch(visitor,expression.expression,expression.type);
	}

	// A visitor that recursively visits each child of a node with the provided child visitor,
	// and recreates the node using the results on the given arena.
	template<typename VisitChild>
	struct MapChildrenVisitor
	{
		typedef TypedExpression DispatchResult;
		
		const Module* module;
		Function* function;
		Memory::Arena& arena;
		VisitChild visitChild;

		std::map<BranchTarget*,BranchTarget*> branchTargetRemap;

		MapChildrenVisitor(Memory::Arena& inArena,const Module* inModule,Function* inFunction,VisitChild inVisitChild)
		: arena(inArena), module(inModule), function(inFunction), visitChild(inVisitChild) {}

		template<typename Type>
		DispatchResult visitLiteral(const Literal<Type>* literal)
		{
			return TypedExpression(new(arena) Literal<Type>(literal->value),Type::id);
		}

		template<typename Class>
		DispatchResult visitError(TypeId type,const Error<Class>* error)
		{
			std::string message = error->message;
			return TypedExpression(new(arena) Error<Class>(std::move(message)),type);
		}
		
		template<typename OpAsType>
		DispatchResult visitLoadVariable(TypeId type,const LoadVariable* loadVariable,OpAsType)
		{
			return TypedExpression(new(arena) LoadVariable(loadVariable->op(),getPrimaryTypeClass(type),loadVariable->variableIndex),type);
		}
		template<typename Class>
		DispatchResult visitLoadMemory(TypeId type,const LoadMemory<Class>* loadMemory)
		{
			auto address = as<IntClass>(visitChild(TypedExpression(loadMemory->address,loadMemory->isFarAddress ? TypeId::I64 : TypeId::I32)));
			return TypedExpression(new(arena) LoadMemory<Class>(loadMemory->isFarAddress,loadMemory->isAligned,address),type);
		}

		template<typename Class,typename OpAsType>
		DispatchResult visitUnary(TypeId type,const Unary<Class>* unary,OpAsType)
		{
			auto operand = as<Class>(visitChild(TypedExpression(unary->operand,type)));
			return TypedExpression(new(arena) Unary<Class>(unary->op(),operand),type);
		}
		template<typename Class,typename OpAsType>
		DispatchResult visitBinary(TypeId type,const Binary<Class>* binary,OpAsType)
		{
			auto left = as<Class>(visitChild(TypedExpression(binary->left,type)));
			auto right = as<Class>(visitChild(TypedExpression(binary->right,type)));
			return TypedExpression(new(arena) Binary<Class>(binary->op(),left,right),type);
		}
		
		template<typename Class,typename OpAsType>
		DispatchResult visitCast(TypeId type,const Cast<Class>* cast,OpAsType)
		{
			auto source = visitChild(cast->source);
			return TypedExpression(new(arena) Cast<Class>(cast->op(),source),type);
		}
		
		template<typename OpAsType>
		DispatchResult visitCall(TypeId type,const Call* call,OpAsType)
		{
			const FunctionType* functionType;
			switch(call->op())
			{
			case AnyOp::callDirect: functionType = &module->functions[call->functionIndex]->type; break;
			case AnyOp::callImport: functionType = &module->functionImports[call->functionIndex].type; break;
			default: throw;
			}

			auto parameters = new(arena) UntypedExpression*[functionType->parameters.size()];
			for(uintptr_t parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
			{
				auto parameterType = functionType->parameters[parameterIndex];
				parameters[parameterIndex] = visitChild(TypedExpression(call->parameters[parameterIndex],parameterType)).expression;
			}

			return TypedExpression(new(arena) Call(call->op(),getPrimaryTypeClass(type),call->functionIndex,parameters),type);
		}
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			const FunctionType& functionType = module->functionTables[callIndirect->tableIndex].type;

			auto parameters = new(arena) UntypedExpression*[functionType.parameters.size()];
			for(uintptr_t parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				auto parameterType = functionType.parameters[parameterIndex];
				parameters[parameterIndex] = visitChild(TypedExpression(callIndirect->parameters[parameterIndex],parameterType)).expression;
			}

			auto functionIndex = as<IntClass>(visitChild(TypedExpression(callIndirect->functionIndex,TypeId::I32)));
			return TypedExpression(new(arena) CallIndirect(getPrimaryTypeClass(type),callIndirect->tableIndex,functionIndex,parameters),type);
		}
		template<typename Class>
		DispatchResult visitSwitch(TypeId type,const Switch<Class>* switch_)
		{
			auto endTarget = new(arena) BranchTarget(type);
			branchTargetRemap[switch_->endTarget] = endTarget;

			auto key = visitChild(switch_->key);
			auto switchArms = new(arena) SwitchArm[switch_->numArms];
			for(uintptr_t armIndex = 0;armIndex < switch_->numArms;++armIndex)
			{
				auto armType = armIndex + 1 == switch_->numArms ? type : TypeId::Void;
				switchArms[armIndex].key = switch_->arms[armIndex].key;
				switchArms[armIndex].value = visitChild(TypedExpression(switch_->arms[armIndex].value,armType)).expression;
			}
			return TypedExpression(new(arena) Switch<Class>(key,switch_->numArms - 1,switch_->numArms,switchArms,endTarget),type);
		}
		template<typename Class>
		DispatchResult visitIfElse(TypeId type,const IfElse<Class>* ifElse)
		{
			auto condition = as<BoolClass>(visitChild(TypedExpression(ifElse->condition,TypeId::Bool)));
			auto thenExpression = as<Class>(visitChild(TypedExpression(ifElse->thenExpression,type)));
			auto elseExpression = as<Class>(visitChild(TypedExpression(ifElse->elseExpression,type)));
			return TypedExpression(new(arena) IfElse<Class>(condition,thenExpression,elseExpression),type);
		}
		template<typename Class>
		DispatchResult visitLabel(TypeId type,const Label<Class>* label)
		{
			auto endTarget = new(arena) BranchTarget(label->endTarget->type);
			branchTargetRemap[label->endTarget] = endTarget;

			auto expression = as<Class>(visitChild(TypedExpression(label->expression,type)));
			return TypedExpression(new(arena) Label<Class>(endTarget,expression),type);
		}
		template<typename Class>
		DispatchResult visitSequence(TypeId type,const Sequence<Class>* seq)
		{
			auto voidExpression = as<VoidClass>(visitChild(TypedExpression(seq->voidExpression,TypeId::Void)));
			auto resultExpression = visitChild(TypedExpression(seq->resultExpression,type));
			return TypedExpression(new(arena) Sequence<Class>(voidExpression,as<Class>(resultExpression)),resultExpression.type);
		}
		template<typename Class>
		DispatchResult visitReturn(const Return<Class>* ret)
		{
			auto value = function->type.returnType == TypeId::Void ? nullptr
				: visitChild(TypedExpression(ret->value,function->type.returnType)).expression;
			return TypedExpression(new(arena) Return<Class>(value),TypeId::None);
		}
		template<typename Class>
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			auto breakTarget = new(arena) BranchTarget(loop->breakTarget->type);
			auto continueTarget = new(arena) BranchTarget(loop->continueTarget->type);
			branchTargetRemap[loop->breakTarget] = breakTarget;
			branchTargetRemap[loop->continueTarget] = continueTarget;

			auto expression = as<VoidClass>(visitChild(TypedExpression(loop->expression,TypeId::Void)));
			return TypedExpression(new(arena) Loop<Class>(expression,breakTarget,continueTarget),type);
		}
		template<typename Class>
		DispatchResult visitBranch(const Branch<Class>* branch)
		{
			auto branchTarget = branchTargetRemap[branch->branchTarget];
			auto value = branch->branchTarget->type == TypeId::Void ? nullptr
				: visitChild(TypedExpression(branch->value,branch->branchTarget->type)).expression;
			return TypedExpression(new(arena) Branch<Class>(branchTarget,value),TypeId::None);
		}

		template<typename OpAsType>
		DispatchResult visitComparison(const Comparison* compare,OpAsType)
		{
			auto left = visitChild(TypedExpression(compare->left,compare->operandType)).expression;
			auto right = visitChild(TypedExpression(compare->right,compare->operandType)).expression;
			return TypedExpression(new(arena) Comparison(compare->op(),compare->operandType,left,right),TypeId::Bool);
		}
		DispatchResult visitNop(const Nop* nop)
		{
			return TypedExpression(new(arena) Nop(),TypeId::Void);
		}
		DispatchResult visitDiscardResult(const DiscardResult* discardResult)
		{
			auto expression = visitChild(discardResult->expression);
			return TypedExpression(new(arena) DiscardResult(expression),TypeId::Void);
		}
		
		template<typename OpAsType>
		DispatchResult visitStoreVariable(const StoreVariable* storeVariable,OpAsType)
		{
			TypeId variableType;
			switch(storeVariable->op())
			{
			case VoidOp::setLocal: variableType = function->locals[storeVariable->variableIndex].type; break;
			case VoidOp::storeGlobal: variableType = module->globals[storeVariable->variableIndex].type; break;
			default: throw;
			}
			auto value = visitChild(TypedExpression(storeVariable->value,variableType)).expression;
			return TypedExpression(new(arena) StoreVariable(storeVariable->op(),value,storeVariable->variableIndex),TypeId::Void);
		}
		DispatchResult visitStoreMemory(const StoreMemory* store)
		{
			auto address = as<IntClass>(visitChild(TypedExpression(store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32)));
			auto value = visitChild(store->value);
			return TypedExpression(new(arena) StoreMemory(store->isFarAddress,store->isAligned,address,value),TypeId::Void);
		}
	};
}