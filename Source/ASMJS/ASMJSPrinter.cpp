#include "Core/Core.h"
#include "Core/Memory.h"
#include "Core/SExpressions.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "AST/ASTDispatch.h"

#include <map>
#include <sstream>

using namespace AST;

namespace ASMJS
{
	// An expression that has been lowered into ASM.JS-like statements and an expression for the final value.
	struct LoweredExpression
	{
		VoidExpression* statements;
		TypedExpression value;

		LoweredExpression(): statements(nullptr) {}
		LoweredExpression(const TypedExpression& inValue): statements(nullptr), value(inValue) {}
		LoweredExpression(VoidExpression* inStatements,const TypedExpression& inValue = TypedExpression())
		: statements(inStatements), value(inValue) {}
	};
	
	// Concatenates 0-2 statements into 0-1 statements. If both statements are null, the result will be null.
	VoidExpression* concatStatements(Memory::Arena& arena,VoidExpression* first,VoidExpression* second)
	{
		if(!first && !second) { return nullptr; }
		else if(first && !second) { return first; }
		else if(!first && second) { return second; }
		else
		{
			return new(arena) Sequence<VoidClass>(first,second);
		}
	}

	// Concatenates the lowered expression's statements with a SetVariable statement that assigns the expression's value to a local variable.
	VoidExpression* setValueToLocal(Memory::Arena& arena,const LoweredExpression& expression,uintptr_t localIndex)
	{
		if(!expression.value) { return expression.statements; }
		else
		{
			auto valueStatement = expression.value.type == TypeId::Void
				? as<VoidClass>(expression.value)
				: as<VoidClass>(new(arena) SetVariable(AnyOp::setLocal,TypeClassId::Void,expression.value.expression,localIndex));
			return concatStatements(arena,expression.statements,valueStatement);
		}
	}

	// Sequences the value of a lowered expression with its statements to yield a single statement.
	VoidExpression* sequenceValue(Memory::Arena& arena,const LoweredExpression& expression)
	{
		VoidExpression* voidValue;
		if(!expression.value) { voidValue = nullptr; }
		else if(expression.value.type == TypeId::Void) { voidValue = as<VoidClass>(expression.value); }
		else { voidValue = new(arena) DiscardResult(expression.value); }
		return concatStatements(arena,expression.statements,voidValue);
	}

	// An AST visitor that lowers certain operations that aren't supported by ASM.JS:
	//	- turns ifs, loops, blocks, labels, switches, branches, returns, etc into statements
	//	- I8, I16, and I64 operations (todo)
	//	- boolean operations (todo)
	//	- unaligned memory operations (todo)
	struct LoweringVisitor : MapChildrenVisitor<LoweringVisitor&,LoweredExpression>
	{
		LoweringVisitor(Memory::Arena& inArena,const Module* inModule,Function* inFunction)
		: MapChildrenVisitor(inArena,inModule,inFunction,*this) {}

		struct LoweredBranchTarget
		{
			BranchTarget* target;
			uintptr_t valueLocalIndex;
		};
		std::map<BranchTarget*,LoweredBranchTarget> branchTargetRemap;

		LoweredExpression visitChild(const TypedExpression& child)
		{
			return dispatch(*this,child);
		}
		
		LoweredExpression operator()(const TypedExpression& child)
		{
			return dispatch(*this,child);
		}

		// Creates a local variable of a specific type.
		uintptr_t createLocalVariable(TypeId type)
		{
			auto index = function->locals.size();
			function->locals.push_back({type,nullptr});
			return index;
		}

		template<typename OpAsType>
		LoweredExpression visitSetVariable(const SetVariable* setVariable,OpAsType)
		{
			TypeId variableType;
			switch(setVariable->op())
			{
			case AnyOp::setLocal: variableType = function->locals[setVariable->variableIndex].type; break;
			case AnyOp::setGlobal: variableType = module->globals[setVariable->variableIndex].type; break;
			default: throw;
			}
			auto value = dispatch(*this,setVariable->value,variableType);
			return LoweredExpression(
				value.statements,
				TypedExpression(new(arena) SetVariable(setVariable->op(),getPrimaryTypeClass(variableType),value.value.expression,setVariable->variableIndex),value.value.type)
				);
		}
		template<typename Class,typename OpAsType>
		LoweredExpression visitLoad(TypeId type,const Load<Class>* load,OpAsType)
		{
			auto address = dispatch(*this,load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32);
			return LoweredExpression(
				address.statements,
				TypedExpression(new(arena) Load<Class>(load->op(),load->isFarAddress,load->isAligned,as<IntClass>(address.value),load->memoryType),type)
				);
		}
		template<typename Class>
		LoweredExpression visitStore(const Store<Class>* store)
		{
			auto address = dispatch(*this,store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32);
			auto value = dispatch(*this,store->value);
			return LoweredExpression(
				concatStatements(arena,address.statements,value.statements),
				TypedExpression(new(arena) Store<Class>(store->isFarAddress,store->isAligned,as<IntClass>(address.value),value.value,store->memoryType),value.value.type)
				);
		}

		template<typename Class,typename OpAsType>
		LoweredExpression visitUnary(TypeId type,const Unary<Class>* unary,OpAsType)
		{
			auto operand = dispatch(*this,unary->operand,type);
			return LoweredExpression(
				operand.statements,
				TypedExpression(new(arena) Unary<Class>(unary->op(),as<Class>(operand.value)),type)
				);
		}
		template<typename Class,typename OpAsType>
		LoweredExpression visitBinary(TypeId type,const Binary<Class>* binary,OpAsType)
		{
			auto left = dispatch(*this,binary->left,type);
			auto right = dispatch(*this,binary->right,type);
			return LoweredExpression(
				concatStatements(arena,left.statements,right.statements),
				TypedExpression(new(arena) Binary<Class>(binary->op(),as<Class>(left.value.expression),as<Class>(right.value.expression)),type)
				);
		}
		
		template<typename Class,typename OpAsType>
		LoweredExpression visitCast(TypeId type,const Cast<Class>* cast,OpAsType)
		{
			auto source = dispatch(*this,cast->source);
			return LoweredExpression(
				source.statements,
				TypedExpression(new(arena) Cast<Class>(cast->op(),source.value),type)
				);
		}
		
		template<typename OpAsType>
		LoweredExpression visitCall(TypeId type,const Call* call,OpAsType)
		{
			// Determine the function type.
			const FunctionType* functionType;
			switch(call->op())
			{
			case AnyOp::callDirect: functionType = &module->functions[call->functionIndex]->type; break;
			case AnyOp::callImport: functionType = &module->functionImports[call->functionIndex].type; break;
			default: throw;
			}

			// Lower the call's parameters.
			auto parameters = new(arena) UntypedExpression*[functionType->parameters.size()];
			VoidExpression* statements = nullptr;
			for(uintptr_t parameterIndex = 0;parameterIndex < functionType->parameters.size();++parameterIndex)
			{
				auto parameterType = functionType->parameters[parameterIndex];
				auto parameter = dispatch(*this,call->parameters[parameterIndex],parameterType);
				parameters[parameterIndex] = parameter.value.expression;
				statements = concatStatements(arena,statements,parameter.statements);
			}

			// Create a new Call node.
			return LoweredExpression(
				statements,
				TypedExpression(new(arena) Call(call->op(),getPrimaryTypeClass(type),call->functionIndex,parameters),type)
				);
		}
		LoweredExpression visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			const FunctionType& functionType = module->functionTables[callIndirect->tableIndex].type;
			
			// Lower the function index.
			auto functionIndex = dispatch(*this,callIndirect->functionIndex,TypeId::I32);
			VoidExpression* statements = functionIndex.statements;

			// Lower the call parameters.
			auto parameters = new(arena) UntypedExpression*[functionType.parameters.size()];
			for(uintptr_t parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				auto parameterType = functionType.parameters[parameterIndex];
				auto parameter = dispatch(*this,callIndirect->parameters[parameterIndex],parameterType);
				parameters[parameterIndex] = parameter.value.expression;
				statements = concatStatements(arena,statements,parameter.statements);
			}

			// Create a new CallIndirect node.
			return LoweredExpression(
				statements,
				TypedExpression(new(arena) CallIndirect(getPrimaryTypeClass(type),callIndirect->tableIndex,as<IntClass>(functionIndex.value),parameters),type)
				);
		}
		template<typename Class>
		LoweredExpression visitSwitch(TypeId type,const Switch<Class>* switch_)
		{
			auto endTarget = new(arena) BranchTarget(TypeId::Void);
			TypedExpression value;
			uintptr_t resultLocalIndex = 0;
			if(type != TypeId::Void)
			{
				// If the switch is used as a value, create a local variable and use the variable's value as the value of the switch.
				resultLocalIndex = createLocalVariable(type);
				value = TypedExpression(new(arena) GetVariable(AnyOp::getLocal,Class::id,resultLocalIndex),type);
			}
			// Store the branch target and the switch's result variable in branchTargetRemap so branching to the end of the switch will set the variable.
			branchTargetRemap[switch_->endTarget] = {endTarget,resultLocalIndex};

			// Lower the switch arms.
			auto switchArms = new(arena) SwitchArm[switch_->numArms];
			for(uintptr_t armIndex = 0;armIndex < switch_->numArms;++armIndex)
			{
				auto armType = armIndex + 1 == switch_->numArms ? type : TypeId::Void;
				auto armValue = dispatch(*this,switch_->arms[armIndex].value,armType);
				switchArms[armIndex].key = switch_->arms[armIndex].key;

				// If the final arm of the switch is an expression, assign it to the switch's value variable.
				switchArms[armIndex].value = armType == TypeId::Void ? sequenceValue(arena,armValue)
					: setValueToLocal(arena,armValue,resultLocalIndex);
			}

			// Create a new Switch node as a statement.
			auto key = dispatch(*this,switch_->key);
			VoidExpression* statements = key.statements;
			statements = concatStatements(arena,statements,
				new(arena) Switch<VoidClass>(key.value,switch_->numArms - 1,switch_->numArms,switchArms,endTarget));
			return LoweredExpression(statements,value);
		}
		template<typename Class>
		LoweredExpression visitIfElse(TypeId type,const IfElse<Class>* ifElse)
		{
			auto condition = dispatch(*this,ifElse->condition,TypeId::Bool);
			auto thenExpression = dispatch(*this,ifElse->thenExpression,type);
			auto elseExpression = dispatch(*this,ifElse->elseExpression,type);
			
			// If the IfElse is used as a value, create a local variable, and in each branch assign their value to it.
			TypedExpression value;
			VoidExpression* thenStatements = nullptr;
			VoidExpression* elseStatements = nullptr;
			if(type == TypeId::Void)
			{
				thenStatements = sequenceValue(arena,thenExpression);
				elseStatements = sequenceValue(arena,elseExpression);
			}
			else
			{
				auto resultLocalIndex = createLocalVariable(type);
				thenStatements = setValueToLocal(arena,thenExpression,resultLocalIndex);
				elseStatements = setValueToLocal(arena,elseExpression,resultLocalIndex);
				value = TypedExpression(new(arena) GetVariable(AnyOp::getLocal,Class::id,resultLocalIndex),type);
			}

			// Create a new IfElse node.
			VoidExpression* statements = concatStatements(arena,condition.statements,
				new(arena) IfElse<VoidClass>(as<BoolClass>(condition.value),thenStatements,elseStatements));
			return LoweredExpression(statements,value);
		}
		template<typename Class>
		LoweredExpression visitLabel(TypeId type,const Label<Class>* label)
		{
			auto endTarget = new(arena) BranchTarget(label->endTarget->type);
			TypedExpression value;
			uintptr_t resultLocalIndex = 0;
			if(type == TypeId::Void) { branchTargetRemap[label->endTarget] = {endTarget,0}; }
			else
			{
				// If the label is used as a value, create a local variable and use the variable's value as the value of the label.
				resultLocalIndex = createLocalVariable(type);
				value = TypedExpression(new(arena) GetVariable(AnyOp::getLocal,Class::id,resultLocalIndex),type);
				
				// Store the branch target and the label's result variable in branchTargetRemap so branching to the end of the label will set the variable.
				branchTargetRemap[label->endTarget] = {endTarget,resultLocalIndex};
			}

			// Lower the label's body. If it is an expression, assign it to the label's value variable.
			auto expression = dispatch(*this,label->expression,type);
			VoidExpression* statements = type == TypeId::Void ? sequenceValue(arena,expression)
				: setValueToLocal(arena,expression,resultLocalIndex);

			// Create a new Label node.
			return LoweredExpression(new(arena) Label<VoidClass>(endTarget,statements),value);
		}
		template<typename Class>
		LoweredExpression visitSequence(TypeId type,const Sequence<Class>* seq)
		{
			auto voidExpression = dispatch(*this,seq->voidExpression,TypeId::Void);
			auto resultExpression = dispatch(*this,seq->resultExpression,type);
			return LoweredExpression(
				concatStatements(arena,sequenceValue(arena,voidExpression),resultExpression.statements),
				resultExpression.value
				);
		}
		template<typename Class>
		LoweredExpression visitReturn(const Return<Class>* ret)
		{
			auto value = function->type.returnType == TypeId::Void ? LoweredExpression()
				: dispatch(*this,ret->value,function->type.returnType);
			return LoweredExpression(concatStatements(arena,value.statements,new(arena) Return<VoidClass>(value.value.expression)));
		}
		template<typename Class>
		LoweredExpression visitLoop(TypeId type,const Loop<Class>* loop)
		{
			auto continueTarget = new(arena) BranchTarget(TypeId::Void);
			branchTargetRemap[loop->continueTarget] = {continueTarget,0};
			
			uintptr_t resultLocalIndex = 0;
			TypedExpression value;
			auto breakTarget = new(arena) BranchTarget(TypeId::Void);
			if(type == TypeId::Void) { branchTargetRemap[loop->breakTarget] = {breakTarget,0}; }
			else
			{
				// If the loop is used as a value, create a local variable and use the variable's value as the value of the loop.
				resultLocalIndex = createLocalVariable(type);
				value = TypedExpression(new(arena) GetVariable(AnyOp::getLocal,Class::id,resultLocalIndex),type);

				// Store the branch target and the loop's result variable in branchTargetRemap so breaking out of the loop will set the variable.
				branchTargetRemap[loop->breakTarget] = {breakTarget,resultLocalIndex};
			}

			// Create a new Loop node.
			auto expression = dispatch(*this,loop->expression,TypeId::Void);
			auto loopStatement = new(arena) Loop<VoidClass>(sequenceValue(arena,expression),breakTarget,continueTarget);
			return LoweredExpression(loopStatement,value);
		}
		template<typename Class>
		LoweredExpression visitBranch(const Branch<Class>* branch)
		{
			auto loweredBranchTarget = branchTargetRemap[branch->branchTarget];
			VoidExpression* statements = nullptr;
			if(branch->branchTarget->type != TypeId::Void)
			{
				// If the branch target is expecting a value, the branchTargetRemap entry for it will have a local variable to put the value in.
				// Assign the value passed to the branch target to the variable before breaking.
				auto value = dispatch(*this,branch->value,branch->branchTarget->type);
				statements = setValueToLocal(arena,value,loweredBranchTarget.valueLocalIndex);
			}
			statements = concatStatements(arena,statements,new(arena) Branch<VoidClass>(loweredBranchTarget.target,nullptr));
			return LoweredExpression(statements);
		}

		template<typename OpAsType>
		LoweredExpression visitComparison(const Comparison* compare,OpAsType)
		{
			auto left = dispatch(*this,compare->left,compare->operandType);
			auto right = dispatch(*this,compare->right,compare->operandType);
			return LoweredExpression(
				concatStatements(arena,left.statements,right.statements),
				TypedExpression(new(arena) Comparison(compare->op(),compare->operandType,left.value.expression,right.value.expression),TypeId::Bool)
				);
		}
		LoweredExpression visitNop(const Nop* nop)
		{
			return LoweredExpression({new(arena) Nop()});
		}
		LoweredExpression visitDiscardResult(const DiscardResult* discardResult)
		{
			auto expression = dispatch(*this,discardResult->expression);
			return LoweredExpression(concatStatements(arena,expression.statements,new(arena) DiscardResult(expression.value)));
		}
	};
	
	// Lowers a function into the subset of the AST's semantics that ASM.JS supports.
	Function lowerFunction(Memory::Arena& arena,const AST::Module* module,const Function& inFunction)
	{
		Function loweredFunction = inFunction;
		LoweringVisitor loweringVisitor(arena,module,&loweredFunction);
		auto returnType = loweredFunction.type.returnType;
		auto loweredExpression = loweringVisitor(TypedExpression(loweredFunction.expression,returnType));
		VoidExpression* loweredStatements = nullptr;
		if(returnType == TypeId::Void) { loweredStatements = sequenceValue(arena,loweredExpression); }
		else if(!loweredExpression.value) { loweredStatements = loweredExpression.statements; }
		else { loweredStatements = concatStatements(arena,loweredExpression.statements,as<VoidClass>(new(arena) Return<VoidClass>(loweredExpression.value.expression))); }
		loweredFunction.expression = loweredStatements;
		return loweredFunction;
	}

	void printCoercePrefix(std::ostream& out,TypeId type)
	{
		switch(type)
		{
		case TypeId::Bool: 
		case TypeId::I32: out << '('; break;
		case TypeId::F32: out << "f32("; break;
		case TypeId::F64: out << '+';
		case TypeId::Void: break;
		default: throw;
		}
	}
	void printCoerceSuffix(std::ostream& out,TypeId type)
	{
		switch(type)
		{
		case TypeId::Bool:
		case TypeId::I32: out << "|0)"; break;
		case TypeId::F32: out << ')'; break;
		case TypeId::F64: break;
		case TypeId::Void: break;
		default: throw;
		}
	}
	std::string getZero(TypeId type)
	{
		switch(type)
		{
		case TypeId::I32: return "0";
		case TypeId::F32: return "f32(0)";
		case TypeId::F64: return "0.0";
		default: throw;
		}
	}

	const char* getHeapName(TypeId type,bool isSigned)
	{
		switch(type)
		{
		case TypeId::I8: return isSigned ? "s8Mem" : "u8Mem";
		case TypeId::I16: return isSigned ? "s16Mem" : "u16Mem";
		case TypeId::I32: return isSigned ? "s32Mem" : "u32Mem";
		case TypeId::F32: return "f32Mem";
		case TypeId::F64: return "f64Mem";
		default: throw;
		};
	}
	uint32 getAddressShift(TypeId type)
	{
		switch(getTypeBitWidth(type))
		{
		case 8: return 0;
		case 16: return 1;
		case 32: return 2;
		case 64: return 3;
		default: throw;
		}
	}
	
	const char* getOperatorPrefix(IntClass::Op op)
	{
		switch(op)
		{
		case IntOp::neg: return "-";
		case IntOp::abs: return "abs";
		case IntOp::not: return "~";
		case IntOp::clz: return "clz";
		case IntOp::ctz: return "ctz";
		case IntOp::popcnt: return "popcnt";
		case IntOp::mul: return "i32Mul";
		default: return "";
		}
	}
	const char* getOperatorSeparator(IntClass::Op op)
	{
		switch(op)
		{
		case IntOp::add: return "+";
		case IntOp::sub: return "-";
		case IntOp::divs: return "/";
		case IntOp::divu: return "/";
		case IntOp::rems: return "%";
		case IntOp::remu: return "%";
		case IntOp::and: return "&";
		case IntOp::or: return "|";
		case IntOp::xor: return "^";
		case IntOp::shl: return "<<";
		case IntOp::shrSExt: return ">>";
		case IntOp::shrZExt: return ">>>";
		default: return ",";
		}
	}
	const char* getOperatorPrefix(FloatOp op)
	{
		switch(op)
		{
		case FloatOp::neg: return "-";
		case FloatOp::abs: return "abs";
		case FloatOp::ceil: return "ceil";
		case FloatOp::floor: return "floor";
		case FloatOp::trunc: return "trunc";
		case FloatOp::nearestInt: return "nearest";
		case FloatOp::sqrt: return "sqrt";
		case FloatOp::min: return "min";
		case FloatOp::max: return "max";
		case FloatOp::copySign: return "copySign";
		default: return "";
		}
	}
	const char* getOperatorSeparator(FloatOp op)
	{
		switch(op)
		{
		case FloatOp::add: return "+";
		case FloatOp::sub: return "-";
		case FloatOp::mul: return "*";
		case FloatOp::div: return "/";
		case FloatOp::rem: return "%";
		default: return ",";
		}
	}
	const char* getOperatorPrefix(BoolOp op)
	{
		switch(op)
		{
		case BoolOp::not: return "!";
		default: return "";
		}
	}
	const char* getOperatorSeparator(BoolOp op)
	{
		switch(op)
		{
		case BoolOp::and: return "&";
		case BoolOp::or: return "|";
		case BoolOp::eq: return "==";
		case BoolOp::neq: return "!=";
		case BoolOp::lts: case BoolOp::ltu: case BoolOp::lt: return "<";
		case BoolOp::les: case BoolOp::leu: case BoolOp::le: return "<=";
		case BoolOp::gts: case BoolOp::gtu: case BoolOp::gt: return ">";
		case BoolOp::ges: case BoolOp::geu: case BoolOp::ge: return "<";
		default: return ",";
		}
	}
	
	struct ModulePrintContext
	{
		const Module* module;
		std::ostream& out;

		ModulePrintContext(const Module* inModule,std::ostream& inOutputStream)
		: module(inModule), out(inOutputStream) {}
		
		std::ostream& printGlobalName(uintptr_t variableIndex) const
		{
			if(module->globals[variableIndex].name) { return out << '_' << module->globals[variableIndex].name; }
			else { return out << "global" << variableIndex; }
		}
		std::ostream& printFunctionImportName(uintptr_t importIndex) const
		{
			assert(module->functionImports[importIndex].name);
			return out << '_' << module->functionImports[importIndex].name;
		}
		std::ostream& printFunctionName(uintptr_t functionIndex) const
		{
			if(module->functions[functionIndex]->name) { return out << '_' << module->functions[functionIndex]->name; }
			else { return out << "func" << functionIndex; }
		}
		std::ostream& printFunctionTableName(uintptr_t functionTableIndex) const
		{
			return out << "table" << functionTableIndex;
		}

		std::ostream& printFunction(uintptr_t functionIndex);
		std::ostream& print();
	};

	struct FunctionPrintContext
	{
		typedef std::ostream& DispatchResult;
		
		const Module* module;
		ModulePrintContext& moduleContext;
		std::ostream& out;
		const Function* function;
		std::map<BranchTarget*,std::string> branchTargetStatementMap;
		size_t numExtraLocals;

		FunctionPrintContext(ModulePrintContext& inModuleContext,const Function* inFunction)
		: module(inModuleContext.module), moduleContext(inModuleContext), out(inModuleContext.out), function(inFunction), numExtraLocals(0) {}

		std::string addLabel(BranchTarget* breakTarget,BranchTarget* continueTarget = nullptr)
		{
			auto labelName = "label" + std::to_string(branchTargetStatementMap.size());
			branchTargetStatementMap[breakTarget] = "break " + labelName;
			if(continueTarget) { branchTargetStatementMap[continueTarget] = "continue " + labelName; }
			return labelName;
		}

		DispatchResult printLocalName(uintptr_t variableIndex) const
		{
			if(function->locals[variableIndex].name) { return out << function->locals[variableIndex].name; }
			else { return out << "local" << variableIndex; }
		}

		template<typename Type>
		DispatchResult visitLiteral(const Literal<Type>* literal)
		{
			return out << literal->value;
		}

		template<typename Class>
		DispatchResult visitError(TypeId type,const Error<Class>* error)
		{
			return out << "error(\"" << error->message << "\')";
		}
		
		DispatchResult visitGetVariable(TypeId type,const GetVariable* getVariable,OpTypes<AnyClass>::getLocal)
		{
			return printLocalName(getVariable->variableIndex);
		}
		DispatchResult visitGetVariable(TypeId type,const GetVariable* getVariable,OpTypes<AnyClass>::getGlobal)
		{
			return moduleContext.printGlobalName(getVariable->variableIndex);
		}
		
		DispatchResult visitSetVariable(const SetVariable* setVariable,OpTypes<AnyClass>::setLocal)
		{
			auto type = function->locals[setVariable->variableIndex].type;
			printLocalName(setVariable->variableIndex);
			out << '=';
			dispatch(*this,setVariable->value,type);
			return out;
		}
		DispatchResult visitSetVariable(const SetVariable* setVariable,OpTypes<AnyClass>::setGlobal)
		{
			auto type = module->globals[setVariable->variableIndex].type;
			moduleContext.printGlobalName(setVariable->variableIndex);
			out << '=';
			dispatch(*this,setVariable->value,type);
			return out;
		}
		template<typename Class,typename OpAsType>
		DispatchResult visitLoad(TypeId type,const Load<Class>* load,OpAsType)
		{
			printCoercePrefix(out,type);
			out << getHeapName(load->memoryType,false) << "[";
			dispatch(*this,load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32);
			out << ">>" << getAddressShift(load->memoryType) << ']';
			printCoerceSuffix(out,type);
			return out;
		}
		DispatchResult visitLoad(TypeId type,const Load<IntClass>* load,OpTypes<IntClass>::loadSExt)
		{
			printCoercePrefix(out,type);
			out << getHeapName(load->memoryType,true) << "[";
			dispatch(*this,load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32);
			out << ">>" << getAddressShift(load->memoryType) << ']';
			printCoerceSuffix(out,type);
			return out;
		}
		template<typename Class>
		DispatchResult visitStore(const Store<Class>* store)
		{
			out << getHeapName(store->memoryType,false) << "[";
			dispatch(*this,store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32);
			out << ">>" << std::to_string(getAddressShift(store->memoryType)) << "]=";
			dispatch(*this,store->value);
			return out;
		}

		template<typename Class,typename OpAsType>
		DispatchResult visitUnary(TypeId type,const Unary<Class>* unary,OpAsType)
		{
			out << getOperatorPrefix(unary->op()) << '(';
			dispatch(*this,unary->operand,type);
			return out << ')';
		}
		template<typename Class,typename OpAsType>
		DispatchResult visitBinary(TypeId type,const Binary<Class>* binary,OpAsType)
		{
			out << getOperatorPrefix(binary->op()) << '(';
			dispatch(*this,binary->left,type);
			out << getOperatorSeparator(binary->op());
			dispatch(*this,binary->right,type);
			return out << ')';
		}
		
		template<typename Class,typename OpAsType>
		DispatchResult visitCast(TypeId type,const Cast<Class>* cast,OpAsType)
		{
			printCoercePrefix(out,type);
			dispatch(*this,cast->source);
			printCoerceSuffix(out,type);
			return out;
		}
		DispatchResult visitCast(TypeId type,const Cast<IntClass>* cast,OpTypes<IntClass>::truncSignedFloat)
		{
			out << "~~";
			dispatch(*this,cast->source);
			return out;
		}
		
		void compileCallParameters(const std::vector<TypeId>& parameterTypes,UntypedExpression** parameters)
		{
			out << '(';
			for(uintptr_t parameterIndex = 0;parameterIndex < parameterTypes.size();++parameterIndex)
			{
				if(parameterIndex != 0) { out << ','; }
				dispatch(*this,parameters[parameterIndex],parameterTypes[parameterIndex]);
			}
			out << ')';
		}
		template<typename OpAsType>
		DispatchResult visitCall(TypeId type,const Call* call,OpAsType)
		{
			const FunctionType* functionType;
			if(call->op() == AnyOp::callDirect)
			{
				functionType = &module->functions[call->functionIndex]->type;
				printCoercePrefix(out,functionType->returnType);
				moduleContext.printFunctionName(call->functionIndex);
			}
			else
			{
				functionType = &module->functionImports[call->functionIndex].type;
				printCoercePrefix(out,functionType->returnType);
				moduleContext.printFunctionImportName(call->functionIndex);
			}
			compileCallParameters(functionType->parameters,call->parameters);
			printCoerceSuffix(out,functionType->returnType);
			return out;
		}
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			auto& functionType = module->functionTables[callIndirect->tableIndex].type;
			printCoercePrefix(out,functionType.returnType);
			moduleContext.printFunctionTableName(callIndirect->tableIndex);
			out << '[';
			printCoercePrefix(out,TypeId::I32);
			dispatch(*this,callIndirect->functionIndex,TypeId::I32);
			out << ']';
			compileCallParameters(functionType.parameters,callIndirect->parameters);
			printCoerceSuffix(out,functionType.returnType);
			return out;
		}
		
		template<typename Class>
		DispatchResult visitSwitch(TypeId type,const Switch<Class>* switch_)
		{
			auto labelName = addLabel(switch_->endTarget);
			out << labelName << ":switch(";
			dispatch(*this,switch_->key);
			out << ")\n{\n";
			for(uintptr_t armIndex = 0;armIndex < switch_->numArms;++armIndex)
			{
				if(armIndex != 0) { out << ";\n"; }
				if(armIndex == switch_->defaultArmIndex) { out << "default:"; }
				else { out << "case " << switch_->arms[armIndex].key << ':'; }
				dispatch(*this,switch_->arms[armIndex].value,TypeId::Void);
			}
			return out << ";\n}";
		}
		template<typename Class>
		DispatchResult visitIfElse(TypeId type,const IfElse<Class>* ifElse)
		{
			out << "if(";
			dispatch(*this,ifElse->condition);
			out << ")\n{\n";
			dispatch(*this,ifElse->thenExpression,type);
			if(as<VoidClass>(ifElse->elseExpression)->op() != VoidOp::nop)
			{
				out << ";\n}\nelse\n{\n";
				dispatch(*this,ifElse->elseExpression,type);
			}
			return out << ";\n}";
		}
		template<typename Class>
		DispatchResult visitLabel(TypeId type,const Label<Class>* label)
		{
			auto labelName = addLabel(label->endTarget);
			out << labelName << ":{\n";
			dispatch(*this,label->expression,type);
			return out << ";\n}";
		}
		template<typename Class>
		DispatchResult visitSequence(TypeId type,const Sequence<Class>* seq)
		{
			dispatch(*this,seq->voidExpression);
			out << ";\n";
			dispatch(*this,seq->resultExpression,type);
			return out;
		}
		template<typename Class>
		DispatchResult visitReturn(const Return<Class>* ret)
		{
			out << "return";
			if(function->type.returnType != TypeId::Void) { out << " "; dispatch(*this,ret->value,function->type.returnType); };
			return out;
		}
		template<typename Class>
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			auto labelName = addLabel(loop->breakTarget,loop->continueTarget);
			out << labelName << ":do\n{\n";
			dispatch(*this,loop->expression);
			return out << ";\n}\nwhile(1)";
		}
		template<typename Class>
		DispatchResult visitBranch(const Branch<Class>* branch)
		{
			assert(!branch->value);
			assert(branch->branchTarget->type == TypeId::Void);
			out << branchTargetStatementMap[branch->branchTarget];
			return out;
		}

		template<typename OpAsType>
		DispatchResult visitComparison(const Comparison* compare,OpAsType)
		{
			out << getOperatorPrefix(compare->op()) << '(';
			dispatch(*this,compare->left,compare->operandType);
			out << getOperatorSeparator(compare->op());
			dispatch(*this,compare->right,compare->operandType);
			return out << ')';
		}
		DispatchResult visitNop(const Nop*)
		{
			return out;
		}
		DispatchResult visitDiscardResult(const DiscardResult* discardResult)
		{
			return dispatch(*this,discardResult->expression);
		}
	};

	std::ostream& ModulePrintContext::printFunction(uintptr_t functionIndex)
	{
		// Before printing, lower the function's expressions into those supported by ASM.JS.
		Memory::ScopedArena loweredFunctionArena;
		Function loweredFunction = lowerFunction(loweredFunctionArena,module,*module->functions[functionIndex]);
		
		// Print the function declaration.
		out << "function ";
		printFunctionName(functionIndex);
		out << '(';

		// Print the function parameters.
		FunctionPrintContext functionContext(*this,&loweredFunction);
		for(uintptr_t parameterIndex = 0;parameterIndex < loweredFunction.parameterLocalIndices.size();++parameterIndex)
		{
			auto parameterLocalIndex = loweredFunction.parameterLocalIndices[parameterIndex];
			if(parameterIndex != 0) { out << ','; }
			functionContext.printLocalName(parameterLocalIndex);
		}
		out << ")\n{\n";
		for(uintptr_t parameterIndex = 0;parameterIndex < loweredFunction.parameterLocalIndices.size();++parameterIndex)
		{
			auto parameterLocalIndex = loweredFunction.parameterLocalIndices[parameterIndex];
			auto parameterType = loweredFunction.type.parameters[parameterIndex];
			functionContext.printLocalName(parameterLocalIndex);
			out << '=';
			printCoercePrefix(out,parameterType);
			functionContext.printLocalName(parameterLocalIndex);
			printCoerceSuffix(out,parameterType);
			out << ";\n";
		}

		// Print the function return type.
		if(loweredFunction.type.returnType != TypeId::Void)
		{
			out << "if(0){return " << getZero(loweredFunction.type.returnType) << ";}\n";
		}

		// Print the function's locals.
		for(uintptr_t localIndex = 0;localIndex < loweredFunction.locals.size();++localIndex)
		{
			bool isParameter = false;
			for(auto parameterLocalIndex : loweredFunction.parameterLocalIndices)
			{
				if(parameterLocalIndex == localIndex) { isParameter = true; break; }
			}
			if(!isParameter)
			{
				out << "var ";
				functionContext.printLocalName(localIndex);
				out << '=' << getZero(loweredFunction.locals[localIndex].type) << ";\n";
			}
		}

		// Print the function's expression. Note that it has been lowered to a void expression, regardless of return type.
		dispatch(functionContext,loweredFunction.expression,TypeId::Void);

		return out << ";\n}\n";
	}

	std::ostream& ModulePrintContext::print()
	{
		out << "(function(global,env,buffer){'use asm';\n";

		// Declare the different views of the heap.
		out << "var " << getHeapName(TypeId::I8,false) << "=new global.Uint8Array(buffer);\n";
		out << "var " << getHeapName(TypeId::I16,false) << "=new global.Uint16Array(buffer);\n";
		out << "var " << getHeapName(TypeId::I32,false) << "=new global.Uint32Array(buffer);\n";
		out << "var " << getHeapName(TypeId::I8,true) << "=new global.Int8Array(buffer);\n";
		out << "var " << getHeapName(TypeId::I16,true) << "=new global.Int16Array(buffer);\n";
		out << "var " << getHeapName(TypeId::I32,true) << "=new global.Int32Array(buffer);\n";
		out << "var " << getHeapName(TypeId::F32,false) << "=new global.Float32Array(buffer);\n";
		out << "var " << getHeapName(TypeId::F64,false) << "=new global.Float64Array(buffer);\n";
		
		// Print the module imports.
		out << "var f32=global.Math.fround;\n";
		out << "var i32Mul=global.Math.imul;\n";
		for(uintptr_t importFunctionIndex = 0;importFunctionIndex < module->functionImports.size();++importFunctionIndex)
		{
			auto import = module->functionImports[importFunctionIndex];
			out << "var ";
			printFunctionImportName(importFunctionIndex);
			out << "=env." << import.name << ";\n";
		}
		std::map<uintptr_t,uintptr_t> globalIndexToImportMap;
		for(uintptr_t importVariableIndex = 0;importVariableIndex < module->variableImports.size();++importVariableIndex)
		{
			auto import = module->variableImports[importVariableIndex];
			out << "var ";
			printGlobalName(import.globalIndex);
			out << '=';
			printCoercePrefix(out,import.type);
			out << "env." << import.name;
			printCoerceSuffix(out,import.type);
			out << ";\n";
			globalIndexToImportMap[import.globalIndex] = importVariableIndex;
		}

		// Print the module globals.
		for(uintptr_t globalIndex = 0;globalIndex < module->globals.size();++globalIndex)
		{
			// Don't export imported globals.
			if(!globalIndexToImportMap.count(globalIndex))
			{
				auto global = module->globals[globalIndex];
				out << "var ";
				printGlobalName(globalIndex);
				out << '=' << getZero(global.type) << ";\n";
			}
		}

		// Print the module functions.
		for(uintptr_t functionIndex = 0;functionIndex < module->functions.size();++functionIndex)
		{
			printFunction(functionIndex);
		}

		// Print the module function tables.
		for(uintptr_t functionTableIndex = 0;functionTableIndex < module->functionTables.size();++functionTableIndex)
		{
			auto functionTable = module->functionTables[functionTableIndex];
			out << "var ";
			printFunctionTableName(functionTableIndex);
			out << "=[";
			for(uintptr_t elementIndex = 0;elementIndex < functionTable.numFunctions;++elementIndex)
			{
				if(elementIndex != 0) { out << ','; }
				printFunctionName(functionTable.functionIndices[elementIndex]);
			}
			out << "];\n";
		}

		// Print the module exports.
		out << "return {";
		bool needsComma = false;
		for(auto exportIt : module->exportNameToFunctionIndexMap)
		{
			if(needsComma) { out << ','; }
			out << exportIt.first << ':';
			printFunctionName(exportIt.second);
			needsComma = true;
		}
		return out << "};\n})";
	}

	void print(std::ostream& outputStream,const Module* module)
	{
		ModulePrintContext(module,outputStream).print();
	}
}