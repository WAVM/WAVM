#include "WAVM.h"
#include "Memory.h"
#include "SExpressions.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "AST/ASTDispatch.h"
#include "WebAssemblyTextSymbols.h"

#include <map>
#include <iostream>

using namespace AST;

namespace WebAssemblyText
{
	typedef SExp::Node SNode;
	typedef SExp::NodeOutputStream SNodeOutputStream;

	SNodeOutputStream& operator<<(SNodeOutputStream& stream,Symbol symbol)
	{
		auto symbolNode = new(stream.getArena()) SNode();
		symbolNode->type = SExp::NodeType::Symbol;
		symbolNode->symbol = (uintptr_t)symbol;
		return stream << symbolNode;
	}
	SNodeOutputStream& operator<<(SNodeOutputStream& stream,TypeId type)
	{
		stream << Symbol((uintptr_t)Symbol::_typeBase + (uintptr_t)type);
		return stream;
	}

	struct ModulePrintContext
	{
		Memory::Arena& arena;
		const Module* module;

		ModulePrintContext(Memory::Arena& inArena,const Module* inModule)
		: arena(inArena), module(inModule) {}
		
		std::string getGlobalName(uintptr_t variableIndex) const
		{
			if(module->globals[variableIndex].name) { return std::string("$_") + module->globals[variableIndex].name; }
			else { return "$global" + std::to_string(variableIndex); }
		}
		std::string getFunctionImportName(uintptr_t importIndex) const
		{
			assert(module->functionImports[importIndex].name);
			return std::string("$_") + module->functionImports[importIndex].name;
		}
		std::string getFunctionName(uintptr_t functionIndex) const
		{
			if(module->functions[functionIndex]->name) { return std::string("$_") + module->functions[functionIndex]->name; }
			else { return "$func" + std::to_string(functionIndex); }
		}
		std::string getFunctionTableName(uintptr_t functionTableIndex) const
		{
			return std::to_string(functionTableIndex);
		}
		
		SNodeOutputStream createSubtree()
		{
			SNodeOutputStream result(arena);
			result.enterSubtree();
			return result;
		}
		SNodeOutputStream createTaggedSubtree(Symbol symbol) { auto subtree = createSubtree(); subtree << symbol; return subtree; }
		SNodeOutputStream createTypedTaggedSubtree(TypeId type,Symbol symbol) { auto subtree = createSubtree(); subtree << getTypedSymbol(type,symbol); return subtree; }
		SNodeOutputStream createBitypedTaggedSubtree(TypeId leftType,Symbol symbol,TypeId rightType) { auto subtree = createSubtree(); subtree << getBitypedSymbol(leftType,symbol,rightType); return subtree; }

		SNodeOutputStream printFunction(uintptr_t functionIndex);
		SNodeOutputStream print();
	};

	struct FunctionPrintContext : ModulePrintContext
	{
		typedef SNodeOutputStream DispatchResult;

		const Function* function;
		std::map<BranchTarget*,std::string> branchTargetIndexMap;

		FunctionPrintContext(Memory::Arena& inArena,const Module* inModule,const Function* inFunction)
		: ModulePrintContext(inArena,inModule), function(inFunction) {}

		std::string getLabelName(BranchTarget* branchTarget)
		{
			if(!branchTarget || !branchTargetIndexMap.count(branchTarget))
			{
				std::string result = "$label" + std::to_string(branchTargetIndexMap.size());
				branchTargetIndexMap[branchTarget] = result;
				return result;
			}
			else { return branchTargetIndexMap[branchTarget]; }
		}
		std::string getLocalName(uintptr_t variableIndex) const\
		{
			if(function->locals[variableIndex].name) { return function->locals[variableIndex].name; }
			else { return "$local" + std::to_string(variableIndex); }
		}

		template<typename Type>
		DispatchResult visitLiteral(const Literal<Type>* literal)
		{
			return createTypedTaggedSubtree(Type::id,Symbol::_const) << literal->value;
		}

		template<typename Class>
		DispatchResult visitError(TypeId type,const Error<Class>* error)
		{
			return createSubtree() << "error" << error->message;
		}
		
		template<typename Class,typename OpAsType>
		DispatchResult visitLoadVariable(TypeId type,const LoadVariable<Class>* loadVariable,OpAsType)
		{
			return createTaggedSubtree(getOpSymbol(loadVariable->op()))
				<< (loadVariable->op() == Class::Op::getLocal ? getLocalName(loadVariable->variableIndex) : getGlobalName(loadVariable->variableIndex));
		}
		template<typename Class>
		DispatchResult visitLoadMemory(TypeId type,const LoadMemory<Class>* loadMemory)
		{
			return createBitypedTaggedSubtree(type,isTypeClass(type,TypeClassId::Int) ? Symbol::_load_u : Symbol::_load,type)
				<< dispatch(*this,loadMemory->address,loadMemory->isFarAddress ? TypeId::I64 : TypeId::I32);
		}

		template<typename Class,typename OpAsType>
		DispatchResult visitUnary(TypeId type,const Unary<Class>* unary,OpAsType)
		{
			return createTypedTaggedSubtree(type,getOpSymbol(unary->op())) << dispatch(*this,unary->operand,type);
		}
		template<typename Class,typename OpAsType>
		DispatchResult visitBinary(TypeId type,const Binary<Class>* binary,OpAsType)
		{
			return createTypedTaggedSubtree(type,getOpSymbol(binary->op())) << dispatch(*this,binary->left,type) << dispatch(*this,binary->right,type);
		}
		
		template<typename Class,typename OpAsType>
		DispatchResult visitCast(TypeId type,const Cast<Class>* cast,OpAsType)
		{
			return createBitypedTaggedSubtree(type,getOpSymbol(cast->op()),cast->source.type) << dispatch(*this,cast->source);
		}
		
		template<typename Class,typename OpAsType>
		DispatchResult visitCall(TypeId type,const Call<Class>* call,OpAsType)
		{
			auto subtreeStream = createTaggedSubtree(getOpSymbol(call->op()));
			auto functionName = call->op() == Class::Op::callDirect ? getFunctionName(call->functionIndex) : getFunctionImportName(call->functionIndex);
			auto functionType = call->op() == Class::Op::callDirect ? module->functions[call->functionIndex]->type : module->functionImports[call->functionIndex].type;
			subtreeStream << functionName;
			for(uintptr_t parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				subtreeStream << dispatch(*this,call->parameters[parameterIndex],functionType.parameters[parameterIndex]);
			}
			return subtreeStream;
		}
		template<typename Class>
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect<Class>* callIndirect)
		{
			auto subtreeStream = createTaggedSubtree(getOpSymbol(callIndirect->op()))
				<< getFunctionTableName(callIndirect->tableIndex)
				<< dispatch(*this,callIndirect->functionIndex,TypeId::I32);
			auto functionType = module->functionTables[callIndirect->tableIndex].type;
			for(uintptr_t parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				subtreeStream << dispatch(*this,callIndirect->parameters[parameterIndex],functionType.parameters[parameterIndex]);
			}
			return subtreeStream;
		}
		
		template<typename Class>
		DispatchResult visitSwitch(TypeId type,const Switch<Class>* switch_)
		{
			auto switchStream = createTypedTaggedSubtree(switch_->key.type,Symbol::_switch) << dispatch(*this,switch_->key);
			for(uintptr_t armIndex = 0;armIndex < switch_->numArms;++armIndex)
			{
				auto caseSubstream = createTaggedSubtree(Symbol::_case) << switch_->arms[armIndex].key;
				bool isLastArm = armIndex + 1 == switch_->numArms;
				if(switch_->arms[armIndex].value)
				{
					caseSubstream << dispatch(*this,switch_->arms[armIndex].value,isLastArm ? type : TypeId::Void);
				}
				if(!isLastArm) { caseSubstream << Symbol::_fallthrough; }
				switchStream << caseSubstream;
			}
			
			// Wrap the switch in a label for the switch end branch target.
			auto labelStream = createTaggedSubtree(Symbol::_label) << getLabelName(switch_->endTarget);
			labelStream << switchStream;

			return labelStream;
		}
		template<typename Class>
		DispatchResult visitIfElse(TypeId type,const IfElse<Class>* ifElse)
		{
			return createTaggedSubtree(Symbol::_if)
				<< dispatch(*this,ifElse->condition)
				<< dispatch(*this,ifElse->thenExpression,type)
				<< dispatch(*this,ifElse->elseExpression,type);
		}
		template<typename Class>
		DispatchResult visitLabel(TypeId type,const Label<Class>* label)
		{
			return createTaggedSubtree(Symbol::_label) << getLabelName(label->endTarget) << dispatch(*this,label->expression,type);
		}
		template<typename Class>
		void visitSequenceRecursive(SNodeOutputStream& outputStream,TypeId type,const Sequence<Class>* seq)
		{
			if(seq->voidExpression->op() != VoidOp::sequence) { outputStream << dispatch(*this,seq->voidExpression); }
			else
			{
				// If the void expression is another sequence, recurse on it.
				visitSequenceRecursive<VoidClass>(outputStream,TypeId::Void,(Sequence<VoidClass>*)seq->voidExpression);
			}
			outputStream << dispatch(*this,seq->resultExpression,type);
		}
		template<typename Class>
		DispatchResult visitSequence(TypeId type,const Sequence<Class>* seq)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_block);
			visitSequenceRecursive(subtreeStream,type,seq);
			return subtreeStream;
		}
		template<typename Class>
		DispatchResult visitReturn(const Return<Class>* ret)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_return);
			if(function->type.returnType != TypeId::Void) { subtreeStream << dispatch(*this,ret->value,function->type.returnType); }
			return subtreeStream;
		}
		template<typename Class>
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			// Simulate the loop break/continue labels with two label nodes.
			auto innerLabelStream = createTaggedSubtree(Symbol::_label) << getLabelName(loop->continueTarget);
			auto outerLabelStream = createTaggedSubtree(Symbol::_label) << getLabelName(loop->breakTarget);
			auto loopStream = createTaggedSubtree(Symbol::_loop);
			innerLabelStream << dispatch(*this,loop->expression);
			loopStream << innerLabelStream;
			outerLabelStream << loopStream;
			return outerLabelStream;
		}
		template<typename Class>
		DispatchResult visitBranch(const Branch<Class>* branch)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_break) << getLabelName(branch->branchTarget);
			if(branch->branchTarget->type != TypeId::Void) { subtreeStream << dispatch(*this,branch->value,branch->branchTarget->type); }
			return subtreeStream;
		}

		template<typename OpAsType>
		DispatchResult visitComparison(const Comparison* compare,OpAsType)
		{
			return createTypedTaggedSubtree(compare->operandType,getOpSymbol(compare->op()))
				<< dispatch(*this,compare->left,compare->operandType)
				<< dispatch(*this,compare->right,compare->operandType);
		}
		DispatchResult visitNop(const Nop*)
		{
			return createTaggedSubtree(Symbol::_nop);
		}
		DispatchResult visitDiscardResult(const DiscardResult* discardResult)
		{
			return dispatch(*this,discardResult->expression);
		}
		
		DispatchResult visitStoreVariable(const StoreVariable* storeVariable,OpTypes<VoidClass>::setLocal)
		{
			return createTaggedSubtree(Symbol::_set_local)
				<< getLocalName(storeVariable->variableIndex)
				<< dispatch(*this,storeVariable->value,function->locals[storeVariable->variableIndex].type);
		}
		DispatchResult visitStoreVariable(const StoreVariable* storeVariable,OpTypes<VoidClass>::storeGlobal)
		{
			return createTaggedSubtree(Symbol::_store_global)
				<< getGlobalName(storeVariable->variableIndex)
				<< dispatch(*this,storeVariable->value,module->globals[storeVariable->variableIndex].type);
		}
		DispatchResult visitStoreMemory(const StoreMemory* store)
		{
			return createBitypedTaggedSubtree(store->value.type,isTypeClass(store->value.type,TypeClassId::Int) ? Symbol::_store_u : Symbol::_store,store->value.type)
				<< dispatch(*this,store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32)
				<< dispatch(*this,store->value);
		}
	};

	SNodeOutputStream ModulePrintContext::printFunction(uintptr_t functionIndex)
	{
		auto function = module->functions[functionIndex];
		FunctionPrintContext functionContext(arena,module,function);
			
		auto functionStream = createTaggedSubtree(Symbol::_func);

		// Print the function name.
		functionStream << functionContext.getFunctionName(functionIndex);

		// Print the function parameters.
		for(auto parameterLocalIndex : function->parameterLocalIndices)
		{
			auto parameter = function->locals[parameterLocalIndex];
			auto paramStream = createTaggedSubtree(Symbol::_param);
			paramStream << functionContext.getLocalName(parameterLocalIndex);
			paramStream << parameter.type;
			functionStream << paramStream;
		}

		// Print the function return type.
		if(function->type.returnType != TypeId::Void)
		{
			auto resultStream = createTaggedSubtree(Symbol::_result);
			resultStream << function->type.returnType;
			functionStream << resultStream;
		}

		// Print the function's locals.
		for(uintptr_t localIndex = 0;localIndex < function->locals.size();++localIndex)
		{
			bool isParameter = false;
			for(auto parameterLocalIndex : function->parameterLocalIndices)
			{
				if(parameterLocalIndex == localIndex) { isParameter = true; break; }
			}

			if(!isParameter)
			{
				auto localStream = createTaggedSubtree(Symbol::_local);
				localStream << functionContext.getLocalName(localIndex);
				localStream << function->locals[localIndex].type;
				functionStream << localStream;
			}
		}

		// Print the function's expression.
		functionStream << dispatch(functionContext,function->expression,function->type.returnType);

		return functionStream;
	}

	SNodeOutputStream ModulePrintContext::print()
	{
		auto moduleStream = createTaggedSubtree(Symbol::_module);
		
		// Print the module memory declarations and data segments.
		auto memoryStream = createTaggedSubtree(Symbol::_memory);
		memoryStream << module->initialNumBytesMemory << module->maxNumBytesMemory;
		for(auto dataSegment : module->dataSegments)
		{
			auto segmentStream = createTaggedSubtree(Symbol::_segment);
			segmentStream << dataSegment.baseAddress;
			segmentStream << SNodeOutputStream::StringAtom((const char*)dataSegment.data,dataSegment.numBytes);
			memoryStream << segmentStream;
		}
		moduleStream << memoryStream;

		// Print the module imports.
		for(uintptr_t importFunctionIndex = 0;importFunctionIndex < module->functionImports.size();++importFunctionIndex)
		{
			auto import = module->functionImports[importFunctionIndex];
			auto importStream = createTaggedSubtree(Symbol::_import);
			importStream << getFunctionImportName(importFunctionIndex);
			
			importStream << SNodeOutputStream::StringAtom(import.name,strlen(import.name));

			auto paramStream = createTaggedSubtree(Symbol::_param);
			for(auto parameterType : import.type.parameters) { paramStream << parameterType; }
			importStream << paramStream;

			if(import.type.returnType != TypeId::Void)
			{
				auto resultStream = createTaggedSubtree(Symbol::_result);
				resultStream << import.type.returnType;
				importStream << resultStream;
			}
			moduleStream << importStream;
		}
		for(uintptr_t importVariableIndex = 0;importVariableIndex < module->variableImports.size();++importVariableIndex)
		{
			// TODO
		}

		// Print the module function tables.
		for(uintptr_t functionTableIndex = 0;functionTableIndex < module->functionTables.size();++functionTableIndex)
		{
			auto functionTable = module->functionTables[functionTableIndex];
			auto tableStream = createTaggedSubtree(Symbol::_table);
			for(uintptr_t elementIndex = 0;elementIndex < functionTable.numFunctions;++elementIndex)
			{
				tableStream << getFunctionName(functionTable.functionIndices[elementIndex]);
			}
			moduleStream << tableStream;
		}

		// Print the module exports.
		for(auto exportIt : module->exportNameToFunctionIndexMap)
		{
			auto exportName = exportIt.first;
			auto exportFunctionIndex = exportIt.second;
			auto exportStream = createTaggedSubtree(Symbol::_export);
			exportStream << SNodeOutputStream::StringAtom(exportName,strlen(exportName));
			exportStream << getFunctionName(exportFunctionIndex);
			moduleStream << exportStream;
		}

		// Print the module globals.
		for(uintptr_t globalIndex = 0;globalIndex < module->globals.size();++globalIndex)
		{
			auto global = module->globals[globalIndex];
			auto globalStream = createTaggedSubtree(Symbol::_global);
			globalStream << getGlobalName(globalIndex);
			globalStream << global.type;
			moduleStream << globalStream;
		}

		// Print the module functions.
		for(uintptr_t functionIndex = 0;functionIndex < module->functions.size();++functionIndex)
		{
			moduleStream << printFunction(functionIndex);
		}

		return moduleStream;
	}

	std::string print(const Module* module)
	{
		Memory::ScopedArena scopedArena;
		return SExp::print((SExp::Node*)ModulePrintContext(scopedArena,module).print().getRoot(),wastSymbols);
	}
}