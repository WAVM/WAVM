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

	struct Error : public NoneExpression, public ErrorRecord
	{
		Error(std::string&& inMessage)
		: NoneExpression(NoneOp::error), ErrorRecord(std::move(inMessage))
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
		uint64 offset;
		Expression<IntClass>* address;
		TypeId memoryType;

		Load(typename Class::Op op,bool inIsFarAddress,uint8 inAlignmentLog2,uint64 inOffset,Expression<IntClass>* inAddress,TypeId inMemoryType)
		: Expression<Class>(op), isFarAddress(inIsFarAddress), alignmentLog2(inAlignmentLog2), offset(inOffset), address(inAddress), memoryType(inMemoryType) {}
	};
	
	template<typename Class>
	struct Store : public Expression<Class>
	{
		bool isFarAddress;
		uint8 alignmentLog2;
		uint64 offset;
		Expression<IntClass>* address;
		TypedExpression value;
		TypeId memoryType;

		Store(bool inIsFarAddress,uint8 inAlignmentLog2,uint64 inOffset,Expression<IntClass>* inAddress,TypedExpression inValue,TypeId inMemoryType)
		: Expression<Class>(Class::Op::store), isFarAddress(inIsFarAddress), alignmentLog2(inAlignmentLog2), offset(inOffset), address(inAddress), value(inValue), memoryType(inMemoryType) {}
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
		FunctionType functionType;
		Expression<IntClass>* functionIndex; // must be I32
		UntypedExpression** parameters;
		CallIndirect(TypeClassId inTypeClass,uintptr inTableIndex,FunctionType inFunctionType,Expression<IntClass>* inFunctionIndex,UntypedExpression** inParameters)
		: Expression(AnyOp::callIndirect,inTypeClass), tableIndex(inTableIndex), functionType(inFunctionType), functionIndex(inFunctionIndex), parameters(inParameters) {}
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
	struct Select : public Expression<Class>
	{
		Expression<BoolClass>* condition;
		Expression<Class>* trueValue;
		Expression<Class>* falseValue;

		Select(Expression<BoolClass>* inCondition,Expression<Class>* inTrueValue,Expression<Class>* inFalseValue)
		:	Expression<Class>(Class::Op::select)
		,	condition(inCondition)
		,	trueValue(inTrueValue)
		,	falseValue(inFalseValue)
		{}
	};
	
	// Each unique branch target has a BranchTarget allocated in the module's arena, so you can identify them by pointer.
	struct BranchTarget
	{
		TypeId type;
		BranchTarget(TypeId inType = TypeId::Void): type(inType) {}
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
		Expression<Class>* expression;
		
		BranchTarget* breakTarget;
		BranchTarget* continueTarget;

		Loop(Expression<Class>* inExpression,BranchTarget* inBreakTarget,BranchTarget* inContinueTarget)
		: Expression<Class>(Class::Op::loop), expression(inExpression), breakTarget(inBreakTarget), continueTarget(inContinueTarget) {}
	};

	struct Branch : public NoneExpression
	{
		BranchTarget* branchTarget;
		UntypedExpression* value; // The type is determined by the branch target. If the type is void, it will be null.

		Branch(BranchTarget* inBranchTarget,UntypedExpression* inValue)
		: NoneExpression(NoneOp::branch), branchTarget(inBranchTarget), value(inValue) {}
	};
	
	struct BranchTable : public NoneExpression
	{
		TypedExpression index;
		BranchTarget* defaultTarget;
		BranchTarget** tableTargets;
		size_t numTableTargets;

		BranchTable(TypedExpression inIndex,BranchTarget* inDefaultTarget,BranchTarget** inTableTargets,size_t inNumTableTargets)
		: NoneExpression(NoneOp::branchTable)
		, index(inIndex)
		, defaultTarget(inDefaultTarget)
		, tableTargets(inTableTargets)
		, numTableTargets(inNumTableTargets) {}
	};

	struct Return : public NoneExpression
	{
		// The value to return. The type is implied by the return type of the function.
		// If the return type is Void, it will be null.
		UntypedExpression* value;

		Return(UntypedExpression* inValue): NoneExpression(NoneOp::ret), value(inValue) {}
	};
	
	struct Unreachable : public NoneExpression
	{
		static Unreachable* get()
		{
			static Unreachable globalInstance;
			return &globalInstance;
		}
	private:
		Unreachable(): NoneExpression(NoneOp::unreachable) {}
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
	
	struct HasFeature : public IntExpression
	{
		const char* featureName;
		size_t featureNameLength;

		HasFeature(const char* inFeatureName,size_t inFeatureNameLength)
		: IntExpression(IntOp::hasFeature)
		, featureName(inFeatureName)
		, featureNameLength(inFeatureNameLength)
		{}
	};
}
