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
		case AnyOp::callDirect: return visitor.visitCall(type,(Call<Class>*)expression,OpTypes<Class>::callDirect());
		case AnyOp::callImport: return visitor.visitCall(type,(Call<Class>*)expression,OpTypes<Class>::callImport());
		case AnyOp::callIndirect: return visitor.visitCallIndirect(type,(CallIndirect<Class>*)expression);
		case AnyOp::getLocal: return visitor.visitLoadVariable(type,(LoadVariable<Class>*)expression,OpTypes<Class>::getLocal());
		case AnyOp::loadGlobal: return visitor.visitLoadVariable(type,(LoadVariable<Class>*)expression,OpTypes<Class>::loadGlobal());
		case AnyOp::block: return visitor.visitBlock(type,(Block<Class>*)expression);
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
}