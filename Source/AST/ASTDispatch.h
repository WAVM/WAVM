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
		case AnyOp::error: return visitor.visitError(type,(Error*)expression);
		case AnyOp::callDirect: return visitor.visitCall(type,(Call*)expression,OpTypes<AnyClass>::callDirect());
		case AnyOp::callImport: return visitor.visitCall(type,(Call*)expression,OpTypes<AnyClass>::callImport());
		case AnyOp::callIndirect: return visitor.visitCallIndirect(type,(CallIndirect*)expression);
		case AnyOp::getLocal: return visitor.visitGetLocal(type,(GetLocal*)expression);
		case AnyOp::setLocal: return visitor.visitSetLocal((SetLocal*)expression);
		case AnyOp::load: return visitor.visitLoad(type,(Load<Class>*)expression,OpTypes<AnyClass>::load());
		case AnyOp::store: return visitor.visitStore((Store<Class>*)expression);
		case AnyOp::sequence: return visitor.visitSequence(type,(Sequence<Class>*)expression);
		case AnyOp::loop: return visitor.visitLoop(type,(Loop<Class>*)expression);
		case AnyOp::ifElse: return visitor.visitConditional(type,(Conditional<Class>*)expression,typename OpTypes<Class>::ifElse());
		case AnyOp::select: return visitor.visitConditional(type,(Conditional<Class>*)expression,typename OpTypes<Class>::select());
		case AnyOp::label: return visitor.visitLabel(type,(Label<Class>*)expression);
		case AnyOp::ret: return visitor.visitReturn(type,(Return*)expression);
		case AnyOp::branch: return visitor.visitBranch(type,(Branch*)expression);
		case AnyOp::branchTable: return visitor.visitBranchTable(type,(BranchTable*)expression);
		case AnyOp::unreachable: return visitor.visitUnreachable(type,(Unreachable*)expression);
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
		
		#define AST_OP(op) case IntOp::op: return visitor.visitComparison((Comparison*)expression,OpTypes<IntClass>::op());
		ENUM_AST_COMPARISON_OPS()
		#undef AST_OP

		case IntOp::lit: return dispatchLiteral(visitor,expression,type);
		case IntOp::loadZExt: return visitor.visitLoad(type,(Load<IntClass>*)expression,OpTypes<IntClass>::loadZExt());
		case IntOp::loadSExt: return visitor.visitLoad(type,(Load<IntClass>*)expression,OpTypes<IntClass>::loadSExt());
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
		default: return dispatchAny(visitor,expression,type);
		}
	}

	// Dispatch opcodes that can occur in type contexts expecting a void result.
	template<typename Visitor>
	static typename Visitor::DispatchResult dispatch(Visitor& visitor,Expression<VoidClass>* expression,TypeId type = TypeId::Void)
	{
		switch(expression->op())
		{
		case VoidOp::nop: return visitor.visitNop((Nop*)expression);
		case VoidOp::discardResult: return visitor.visitDiscardResult((DiscardResult*)expression);
		case VoidOp::branchIf: return visitor.visitBranchIf((BranchIf*)expression);
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
	template<typename VisitChild,typename InDispatchResult>
	struct MapChildrenVisitor
	{
		typedef InDispatchResult DispatchResult;

		const Module* module;
		Function* function;
		Memory::Arena& arena;
		VisitChild visitChild;

		std::map<BranchTarget*,BranchTarget*> branchTargetRemap;

		MapChildrenVisitor(Memory::Arena& inArena,const Module* inModule,Function* inFunction,VisitChild inVisitChild)
		: module(inModule), function(inFunction), arena(inArena), visitChild(inVisitChild) {}

		template<typename Type>
		DispatchResult visitLiteral(const Literal<Type>* literal)
		{
			return TypedExpression(new(arena) Literal<Type>(literal->value),Type::id);
		}

		DispatchResult visitError(TypeId type,const Error* error)
		{
			std::string message = error->message;
			return TypedExpression(new(arena) Error(std::move(message)),type);
		}
		
		DispatchResult visitGetLocal(TypeId type,const GetLocal* getVariable)
		{
			return TypedExpression(new(arena) GetLocal(getPrimaryTypeClass(type),getVariable->variableIndex),type);
		}
		DispatchResult visitSetLocal(const SetLocal* setVariable)
		{
			auto variableType = function->locals[setVariable->variableIndex].type;
			auto value = visitChild(TypedExpression(setVariable->value,variableType));
			return TypedExpression(new(arena) SetLocal(getPrimaryTypeClass(variableType),value.expression,setVariable->variableIndex),value.type);
		}
		template<typename Class,typename OpAsType>
		DispatchResult visitLoad(TypeId type,const Load<Class>* load,OpAsType)
		{
			auto address = as<IntClass>(visitChild(TypedExpression(load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32)));
			return TypedExpression(new(arena) Load<Class>(load->op(),load->isFarAddress,load->alignmentLog2,load->offset,address,load->memoryType),type);
		}
		template<typename Class>
		DispatchResult visitStore(const Store<Class>* store)
		{
			auto address = as<IntClass>(visitChild(TypedExpression(store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32)));
			auto value = visitChild(store->value);
			return TypedExpression(new(arena) Store<Class>(store->isFarAddress,store->alignmentLog2,store->offset,address,value,store->memoryType),value.type);
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
			for(uintptr parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
			{
				auto parameterType = functionType->parameters[parameterIndex];
				parameters[parameterIndex] = visitChild(TypedExpression(call->parameters[parameterIndex],parameterType)).expression;
			}

			return TypedExpression(new(arena) Call(call->op(),getPrimaryTypeClass(type),call->functionIndex,parameters),type);
		}
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			const FunctionType& functionType = callIndirect->functionType;

			auto parameters = new(arena) UntypedExpression*[functionType.parameters.size()];
			for(uintptr parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				auto parameterType = functionType.parameters[parameterIndex];
				parameters[parameterIndex] = visitChild(TypedExpression(callIndirect->parameters[parameterIndex],parameterType)).expression;
			}

			auto functionIndex = as<IntClass>(visitChild(TypedExpression(callIndirect->functionIndex,TypeId::I32)));
			return TypedExpression(new(arena) CallIndirect(getPrimaryTypeClass(type),callIndirect->functionType,functionIndex,parameters),type);
		}
		template<typename Class,typename OpAsType>
		DispatchResult visitConditional(TypeId type,const Conditional<Class>* conditional,OpAsType)
		{
			auto condition = as<IntClass>(visitChild(TypedExpression(conditional->condition,TypeId::I32)));
			auto trueValue = as<Class>(visitChild(TypedExpression(conditional->trueValue,type)));
			auto falseValue = as<Class>(visitChild(TypedExpression(conditional->falseValue,type)));
			return TypedExpression(new(arena) Conditional<Class>(conditional->op(),condition,trueValue,falseValue),type);
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
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			auto breakTarget = new(arena) BranchTarget(loop->breakTarget->type);
			auto continueTarget = new(arena) BranchTarget(loop->continueTarget->type);
			branchTargetRemap[loop->breakTarget] = breakTarget;
			branchTargetRemap[loop->continueTarget] = continueTarget;

			auto expression = as<Class>(visitChild(TypedExpression(loop->expression,type)));
			return TypedExpression(new(arena) Loop<Class>(expression,breakTarget,continueTarget),type);
		}
		DispatchResult visitBranch(TypeId type,const Branch* branch)
		{
			auto branchTarget = branchTargetRemap[branch->branchTarget];
			auto value = branch->branchTarget->type == TypeId::Void ? nullptr
				: visitChild(TypedExpression(branch->value,branch->branchTarget->type)).expression;
			return TypedExpression(new(arena) Branch(branchTarget,value),TypeId::None);
		}
		DispatchResult visitBranchIf(const BranchIf* branchIf)
		{
			auto branchTarget = branchTargetRemap[branchIf->branchTarget];
			auto condition = dispatch(*this,branchIf->condition,TypeId::I32);
			auto value = branchIf->branchTarget->type == TypeId::Void ? nullptr
				: visitChild(TypedExpression(branchIf->value,branchIf->branchTarget->type)).expression;
			return TypedExpression(new(arena) BranchIf(branchTarget,as<IntClass>(condition),value),TypeId::Void);
		}
		DispatchResult visitBranchTable(TypeId type,const BranchTable* branchTable)
		{
			auto value = branchTable->defaultTarget->type == TypeId::Void ? nullptr
				: visitChild(TypedExpression(branchTable->value,branchTable->defaultTarget->type)).expression;
			auto index = visitChild(branchTable->index);
			auto defaultTarget = branchTargetRemap[branchTable->defaultTarget];
			auto tableTargets = new(arena) BranchTarget*[branchTable->numTableTargets];
			for(uintptr tableIndex = 0;tableIndex < branchTable->numTableTargets;++tableIndex)
			{ tableTargets[tableIndex] = branchTargetRemap[branchTable->tableTargets[tableIndex]]; }
			return TypedExpression(new(arena) BranchTable(value,index,defaultTarget,tableTargets,branchTable->numTableTargets),TypeId::None);
		}
		DispatchResult visitReturn(TypeId type,const Return* ret)
		{
			auto value = function->type.returnType == TypeId::Void ? nullptr
				: visitChild(TypedExpression(ret->value,function->type.returnType)).expression;
			return TypedExpression(new(arena) Return(value),TypeId::None);
		}
		DispatchResult visitUnreachable(TypeId type,const Unreachable* unreachable)
		{
			return TypedExpression(Unreachable::get(),TypeId::None);
		}

		template<typename OpAsType>
		DispatchResult visitComparison(const Comparison* compare,OpAsType)
		{
			auto left = visitChild(TypedExpression(compare->left,compare->operandType)).expression;
			auto right = visitChild(TypedExpression(compare->right,compare->operandType)).expression;
			return TypedExpression(new(arena) Comparison(compare->op(),compare->operandType,left,right),TypeId::I32);
		}
		DispatchResult visitNop(const Nop* nop)
		{
			return TypedExpression(Nop::get(),TypeId::Void);
		}
		DispatchResult visitDiscardResult(const DiscardResult* discardResult)
		{
			auto expression = visitChild(discardResult->expression);
			return TypedExpression(new(arena) DiscardResult(expression),TypeId::Void);
		}
	};
}
