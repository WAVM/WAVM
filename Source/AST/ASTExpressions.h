#pragma once

#include "AST.h"

namespace AST
{
	template<typename Type>
	struct Literal : public Type::TypeExpression
	{
		typedef typename Type::NativeType NativeType;
		NativeType value;
		Literal(NativeType inValue) : Type::TypeExpression(Type::Op::lit), value(inValue) {}
	};

	template<typename Class>
	struct Error : public Expression<Class>, public ErrorRecord
	{
		Error(std::string&& inMessage)
		: Expression<Class>(Class::Op::error), ErrorRecord(std::move(inMessage))
		{}
	};

	struct GetLocal : public Expression<AnyClass>
	{
		uintptr variableIndex;
		GetLocal(TypeClassId inTypeClass,uintptr inVariableIndex) : Expression(AnyOp::getLocal,inTypeClass), variableIndex(inVariableIndex) {}
	};
	
	struct SetLocal : public Expression<AnyClass>
	{
		UntypedExpression* value;
		uintptr variableIndex;
		SetLocal(TypeClassId inTypeClass,UntypedExpression* inValue,uintptr inVariableIndex)
		: Expression(AnyOp::setLocal,inTypeClass), value(inValue), variableIndex(inVariableIndex) {}
	};

	template<typename Class>
	struct Load : public Expression<Class>
	{
		bool isFarAddress;
		uint8 alignmentLog2;
		Expression<IntClass>* address;
		TypeId memoryType;

		Load(typename Class::Op op,bool inIsFarAddress,uint8 inAlignmentLog2,Expression<IntClass>* inAddress,TypeId inMemoryType)
		: Expression<Class>(op), isFarAddress(inIsFarAddress), alignmentLog2(inAlignmentLog2), address(inAddress), memoryType(inMemoryType) {}
	};
	
	template<typename Class>
	struct Store : public Expression<Class>
	{
		bool isFarAddress;
		uint8 alignmentLog2;
		Expression<IntClass>* address;
		TypedExpression value;
		TypeId memoryType;

		Store(bool inIsFarAddress,uint8 inAlignmentLog2,Expression<IntClass>* inAddress,TypedExpression inValue,TypeId inMemoryType)
		: Expression<Class>(Class::Op::store), isFarAddress(inIsFarAddress), alignmentLog2(inAlignmentLog2), address(inAddress), value(inValue), memoryType(inMemoryType) {}
	};
	
	template<typename Class>
	struct Unary : public Expression<Class>
	{
		Expression<Class>* operand;
		Unary(typename Class::Op op,Expression<Class>* inOperand): Expression<Class>(op), operand(inOperand) {}
	};
	
	template<typename Class>
	struct Binary : public Expression<Class>
	{
		Expression<Class>* left;
		Expression<Class>* right;

		Binary(typename Class::Op op,Expression<Class>* inLeft,Expression<Class>* inRight)
		: Expression<Class>(op), left(inLeft), right(inRight)
		{}
	};

	struct Comparison : public Expression<BoolClass>
	{
		TypeId operandType;
		UntypedExpression* left;
		UntypedExpression* right;

		Comparison(Op op,TypeId inOperandType,UntypedExpression* inLeft,UntypedExpression* inRight)
		: Expression(op), operandType(inOperandType), left(inLeft), right(inRight) {}
	};

	template<typename Class>
	struct Cast : public Expression<Class>
	{
		typedef typename Class::Op Op;

		TypedExpression source;

		Cast(Op op,TypedExpression inSource)
		: Expression<Class>(op), source(inSource) {}
	};
	
	struct Call : public Expression<AnyClass>
	{
		uintptr functionIndex;
		UntypedExpression** parameters;
		Call(AnyOp op,TypeClassId inTypeClass,uintptr inFunctionIndex,UntypedExpression** inParameters)
		: Expression(op,inTypeClass), functionIndex(inFunctionIndex), parameters(inParameters) {}
	};

	struct CallIndirect : public Expression<AnyClass>
	{
		uintptr tableIndex;
		Expression<IntClass>* functionIndex; // must be I32
		UntypedExpression** parameters;
		CallIndirect(TypeClassId inTypeClass,uintptr inTableIndex,Expression<IntClass>* inFunctionIndex,UntypedExpression** inParameters)
		: Expression(AnyOp::callIndirect,inTypeClass), tableIndex(inTableIndex), functionIndex(inFunctionIndex), parameters(inParameters) {}
	};

	// Used to coerce an expression result to void.
	struct DiscardResult : public Expression<VoidClass>
	{
		TypedExpression expression;

		DiscardResult(TypedExpression inExpression): Expression(Op::discardResult), expression(inExpression) {}
	};

	struct Nop : public Expression<VoidClass>
	{
		static Nop* get()
		{
			static Nop globalInstance;
			return &globalInstance;
		}

	private:
		
		Nop(): Expression(Op::nop) {}
	};
	
	// Each unique branch target has a BranchTarget allocated in the module's arena, so you can identify them by pointer.
	struct BranchTarget
	{
		TypeId type;
		BranchTarget(TypeId inType): type(inType) {}
	};

	struct SwitchArm
	{
		uint64 key;
		UntypedExpression* value; // The type of the switch for the final arm, void for all others.
	};

	template<typename Class>
	struct Switch : public Expression<Class>
	{
		TypedExpression key;
		uintptr defaultArmIndex;
		size_t numArms;
		SwitchArm* arms;
		BranchTarget* endTarget;

		Switch(TypedExpression inKey,uintptr inDefaultArmIndex,size_t inNumArms,SwitchArm* inArms,BranchTarget* inEndTarget)
		: Expression<Class>(Class::Op::switch_), key(inKey), defaultArmIndex(inDefaultArmIndex), numArms(inNumArms), arms(inArms), endTarget(inEndTarget) {}
	};
	
	template<typename Class>
	struct IfElse : public Expression<Class>
	{
		Expression<BoolClass>* condition;
		Expression<Class>* thenExpression;
		Expression<Class>* elseExpression;

		IfElse(Expression<BoolClass>* inCondition,Expression<Class>* inThenExpression,Expression<Class>* inElseExpression)
		:	Expression<Class>(Class::Op::ifElse)
		,	condition(inCondition)
		,	thenExpression(inThenExpression)
		,	elseExpression(inElseExpression)
		{}
	};

	template<typename Class>
	struct Label : public Expression<Class>
	{
		BranchTarget* endTarget;
		Expression<Class>* expression;

		Label(BranchTarget* inEndTarget,Expression<Class>* inExpression)
		: Expression<Class>(Class::Op::label), endTarget(inEndTarget), expression(inExpression) {}
	};

	template<typename Class>
	struct Loop : public Expression<Class>
	{
		Expression<VoidClass>* expression;
		
		BranchTarget* breakTarget;
		BranchTarget* continueTarget;

		Loop(Expression<VoidClass>* inExpression,BranchTarget* inBreakTarget,BranchTarget* inContinueTarget)
		: Expression<Class>(Class::Op::loop), expression(inExpression), breakTarget(inBreakTarget), continueTarget(inContinueTarget) {}
	};

	template<typename Class>
	struct Branch : public Expression<Class>
	{
		BranchTarget* branchTarget;
		UntypedExpression* value; // The type is determined by the branch target. If the type is void, it will be null.

		Branch(BranchTarget* inBranchTarget,UntypedExpression* inValue)
		: Expression<Class>(Class::Op::branch), branchTarget(inBranchTarget), value(inValue) {}
	};

	template<typename Class>
	struct Return : public Expression<Class>
	{
		// The value to return. The type is implied by the return type of the function.
		// If the return type is Void, it will be null.
		UntypedExpression* value;

		Return(UntypedExpression* inValue): Expression<Class>(Class::Op::ret), value(inValue) {}
	};

	template<typename Class>
	struct Sequence : public Expression<Class>
	{
		Expression<VoidClass>* voidExpression;
		Expression<Class>* resultExpression;

		Sequence(Expression<VoidClass>* inVoidExpression,Expression<Class>* inResultExpression)
		:	Expression<Class>(Class::Op::sequence)
		,	voidExpression(inVoidExpression)
		,	resultExpression(inResultExpression)
		{}
	};
}
