#include "Core/Core.h"
#include "Core/MemoryArena.h"
#include "Core/SExpressions.h"
#include "AST/AST.h"
#include "AST/ASTExpressions.h"
#include "AST/ASTDispatch.h"
#include "WebAssembly.h"
#include "WebAssemblyTextSymbols.h"

#include <map>

using namespace AST;

namespace WebAssemblyText
{
	typedef SExp::Node SNode;
	typedef SExp::NodeOutputStream SNodeOutputStream;

	SNodeOutputStream& operator<<(SNodeOutputStream& stream,Symbol symbol)
	{
		auto symbolNode = new(stream.getArena()) SNode();
		symbolNode->type = SExp::NodeType::Symbol;
		symbolNode->symbol = (uintptr)symbol;
		return stream << symbolNode;
	}
	SNodeOutputStream& operator<<(SNodeOutputStream& stream,TypeId type)
	{
		stream << Symbol((uintptr)Symbol::_typeBase + (uintptr)type);
		return stream;
	}

	// An AST visitor that lowers certain operations that aren't supported by the WebAssembly text format:
	//	- I8 and I16 operations (todo)
	//	- boolean operations (todo)
	struct LoweringVisitor : MapChildrenVisitor<LoweringVisitor&,TypedExpression>
	{
		LoweringVisitor(Memory::Arena& inArena,const Module* inModule,Function* inFunction)
		: MapChildrenVisitor(inArena,inModule,inFunction,*this) {}

		TypedExpression operator()(TypedExpression child)
		{
			return dispatch(*this,child);
		}
	};

	struct PrintContext
	{
		Memory::Arena& arena;

		PrintContext(Memory::Arena& inArena): arena(inArena) {}

		SNodeOutputStream createSubtree()
		{
			SNodeOutputStream result(arena);
			result.enterSubtree();
			return result;
		}
		SNodeOutputStream createAttribute()
		{
			SNodeOutputStream result(arena);
			result.enterAttribute();
			return result;
		}
		SNodeOutputStream createTaggedSubtree(Symbol symbol) { auto subtree = createSubtree(); subtree << symbol; return subtree; }
		SNodeOutputStream createTypedTaggedSubtree(TypeId type,Symbol symbol) { auto subtree = createSubtree(); subtree << getTypedSymbol(type,symbol); return subtree; }
		SNodeOutputStream createBitypedTaggedSubtree(TypeId leftType,Symbol symbol,TypeId rightType) { auto subtree = createSubtree(); subtree << getBitypedSymbol(leftType,symbol,rightType); return subtree; }
	};

	struct ModulePrintContext : PrintContext
	{
		const Module* module;

		std::map<FunctionType,uintptr> functionTypeToSignatureIndexMap;
		std::vector<uint32> functionTableBaseIndices;
		
		ModulePrintContext(Memory::Arena& inArena,const Module* inModule)
		: PrintContext(inArena), module(inModule) {}
		
		std::string getFunctionImportName(uintptr importIndex) const
		{
			assert(module->functionImports[importIndex].name);
			return std::string("$_") + module->functionImports[importIndex].name;
		}
		std::string getFunctionName(uintptr functionIndex) const
		{
			if(module->functions[functionIndex].name) { return std::string("$_") + module->functions[functionIndex].name; }
			else { return "$func" + std::to_string(functionIndex); }
		}
		std::string getFunctionTableName(uintptr functionTableIndex) const
		{
			return std::to_string(functionTableIndex);
		}
		
		uintptr getSignatureIndex(const FunctionType& type)
		{
			auto signatureIt = functionTypeToSignatureIndexMap.find(type);
			uintptr signatureIndex;
			if(signatureIt != functionTypeToSignatureIndexMap.end()) { signatureIndex = signatureIt->second; }
			else
			{
				signatureIndex = functionTypeToSignatureIndexMap.size();
				functionTypeToSignatureIndexMap[type] = signatureIndex;
			}
			return signatureIndex;
		}

		std::string getSignatureName(uintptr signatureIndex)
		{
			return "$sig" + std::to_string(signatureIndex);
		}

		SNodeOutputStream printFunction(uintptr functionIndex);
		SNodeOutputStream printFunctionType(const FunctionType& functionType);
		SNodeOutputStream print();
	};

	struct FunctionPrintContext : PrintContext
	{
		typedef SNodeOutputStream DispatchResult;

		ModulePrintContext& moduleContext;
		const Function* function;
		std::map<BranchTarget*,std::string> branchTargetIndexMap;

		FunctionPrintContext(ModulePrintContext& inModuleContext,const Function* inFunction)
		: PrintContext(inModuleContext.arena), moduleContext(inModuleContext), function(inFunction) {}

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
		std::string getLocalName(uintptr variableIndex) const\
		{
			if(function->locals[variableIndex].name) { return std::string("$_") + function->locals[variableIndex].name; }
			else { return "$local" + std::to_string(variableIndex); }
		}

		template<typename Type>
		DispatchResult visitLiteral(const Literal<Type>* literal)
		{
			return createTypedTaggedSubtree(Type::id,Symbol::_const) << literal->value;
		}
		
		DispatchResult visitLiteral(const Literal<F32Type>* literal)
		{
			return createTypedTaggedSubtree(TypeId::F32,Symbol::_const) << literal->value;
		}
		
		DispatchResult visitLiteral(const Literal<F64Type>* literal)
		{
			return createTypedTaggedSubtree(TypeId::F64,Symbol::_const) << literal->value;
		}

		DispatchResult visitError(TypeId type,const Error* error)
		{
			return createSubtree() << "error" << error->message;
		}
		
		DispatchResult visitGetLocal(TypeId type,const GetLocal* getVariable)
		{
			return createTaggedSubtree(getAnyOpSymbol<AnyClass>(getVariable->op()))
				<< getLocalName(getVariable->variableIndex);
		}
		DispatchResult visitSetLocal(const SetLocal* setVariable)
		{
			return createTaggedSubtree(Symbol::_set_local)
				<< getLocalName(setVariable->variableIndex)
				<< dispatch(*this,setVariable->value,function->locals[setVariable->variableIndex].type);
		}

		DispatchResult createOffsetAttribute(uint64 offset)
		{
			if(offset == 0) { return SNodeOutputStream(arena); }
			else
			{
				auto attributeStream = createAttribute();
				attributeStream << Symbol::_offset;
				attributeStream << offset;
				return attributeStream;
			}
		}
		
		DispatchResult createAlignmentAttribute(uint8 alignmentLog2)
		{
			if(alignmentLog2 == 0) { return SNodeOutputStream(arena); }
			else
			{
				auto attributeStream = createAttribute();
				attributeStream << Symbol::_align;
				attributeStream << uint64(1ull << alignmentLog2);
				return attributeStream;
			}
		}

		template<typename Class,typename OpAsType>
		DispatchResult visitLoad(TypeId type,const Load<Class>* load,OpAsType)
		{
			assert(load->memoryType == type);
			return createTypedTaggedSubtree(type,getOpSymbol(load->op()))
				<< createOffsetAttribute(load->offset)
				<< createAlignmentAttribute(load->alignmentLog2)
				<< dispatch(*this,load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32);
		}
		template<typename OpAsType>
		DispatchResult visitLoad(TypeId type,const Load<IntClass>* load,OpAsType)
		{
			Symbol symbol = Symbol::_load;
			switch(load->memoryType)
			{
			case TypeId::I8:  symbol = load->op() == IntOp::loadSExt ? Symbol::_load8_s  : Symbol::_load8_u; break;
			case TypeId::I16: symbol = load->op() == IntOp::loadSExt ? Symbol::_load16_s : Symbol::_load16_u; break;
			default:;
			}
			return createTypedTaggedSubtree(type,symbol)
				<< createOffsetAttribute(load->offset)
				<< createAlignmentAttribute(load->alignmentLog2)
				<< dispatch(*this,load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32);
		}
		template<typename Class>
		DispatchResult visitStore(const Store<Class>* store)
		{
			assert(store->memoryType == store->value.type);
			return createTypedTaggedSubtree(store->value.type,getOpSymbol(store->op()))
				<< createOffsetAttribute(store->offset)
				<< createAlignmentAttribute(store->alignmentLog2)
				<< dispatch(*this,store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32)
				<< dispatch(*this,store->value);
		}
		DispatchResult visitStore(const Store<IntClass>* store)
		{
			Symbol symbol = Symbol::_store;
			switch(store->memoryType)
			{
			case TypeId::I8: symbol = Symbol::_store8; break;
			case TypeId::I16: symbol = Symbol::_store16; break;
			default:;
			}
			return createTypedTaggedSubtree(store->value.type,symbol)
				<< createOffsetAttribute(store->offset)
				<< createAlignmentAttribute(store->alignmentLog2)
				<< dispatch(*this,store->address,store->isFarAddress ? TypeId::I64 : TypeId::I32)
				<< dispatch(*this,store->value);
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
		
		template<typename OpAsType>
		DispatchResult visitCall(TypeId type,const Call* call,OpAsType)
		{
			auto subtreeStream = createTaggedSubtree(getAnyOpSymbol<AnyClass>(call->op()));
			auto functionName = call->op() == AnyOp::callDirect
				? moduleContext.getFunctionName(call->functionIndex)
				: moduleContext.getFunctionImportName(call->functionIndex);
			auto functionType = call->op() == AnyOp::callDirect
				? moduleContext.module->functions[call->functionIndex].type
				: moduleContext.module->functionImports[call->functionIndex].type;
			subtreeStream << functionName;
			for(uintptr parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{ subtreeStream << dispatch(*this,call->parameters[parameterIndex],functionType.parameters[parameterIndex]); }
			return subtreeStream;
		}
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_call_indirect)
				<< moduleContext.getSignatureName(moduleContext.getSignatureIndex(callIndirect->functionType))
				<< dispatch(*this,callIndirect->functionIndex,TypeId::I32);
			auto functionType = callIndirect->functionType;
			for(uintptr parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{ subtreeStream << dispatch(*this,callIndirect->parameters[parameterIndex],functionType.parameters[parameterIndex]); }
			return subtreeStream;
		}
		
		template<typename Class,typename OpAsType>
		DispatchResult visitConditional(TypeId type,const Conditional<Class>* conditional,OpAsType)
		{
			return createTaggedSubtree(getOpSymbol(conditional->op()))
				<< dispatch(*this,conditional->condition,TypeId::I32)
				<< dispatch(*this,conditional->trueValue,type)
				<< dispatch(*this,conditional->falseValue,type);
		}
		DispatchResult visitConditional(TypeId type,const Conditional<VoidClass>* ifElse,OpTypes<VoidClass>::ifElse)
		{
			// Recognize IfElse(...,nop) and omit the else branch.
			// Also recognize IfElse(branch,nop) and translate it into (br_if).
			if(ifElse->falseValue->op() == VoidOp::nop)
			{
				return createTaggedSubtree(Symbol::_if)
					<< dispatch(*this,ifElse->condition,TypeId::I32)
					<< dispatch(*this,ifElse->trueValue,type);
			}
			else
			{
				return createTaggedSubtree(Symbol::_if)
					<< dispatch(*this,ifElse->condition,TypeId::I32)
					<< dispatch(*this,ifElse->trueValue,type)
					<< dispatch(*this,ifElse->falseValue,type);
			}
		}
		template<typename Class>
		DispatchResult visitLabel(TypeId type,const Label<Class>* label)
		{
			auto blockStream = createTaggedSubtree(Symbol::_block) << getLabelName(label->endTarget);
			if(label->expression->op() == Class::Op::sequence) { visitSequenceRecursive(blockStream,type,(Sequence<Class>*)label->expression); }
			else { blockStream << dispatch(*this,label->expression,type); }
			return blockStream;
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
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			// Simulate the loop break/continue labels with two label nodes.
			auto loopStream = createTaggedSubtree(Symbol::_loop);
			loopStream << getLabelName(loop->breakTarget) << getLabelName(loop->continueTarget);
			auto bodyStream = dispatch(*this,loop->expression,type);
			loopStream << bodyStream;
			return loopStream;
		}
		DispatchResult visitBranch(TypeId type,const Branch* branch)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_br) << getLabelName(branch->branchTarget);
			if(branch->branchTarget->type != TypeId::Void) { subtreeStream << dispatch(*this,branch->value,branch->branchTarget->type); }
			return subtreeStream;
		}
		DispatchResult visitBranchIf(const BranchIf* branchIf)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_br_if)
				<< getLabelName(branchIf->branchTarget);
			if(branchIf->branchTarget->type != TypeId::Void) { subtreeStream << dispatch(*this,branchIf->value,branchIf->branchTarget->type); }
			subtreeStream << dispatch(*this,branchIf->condition,TypeId::I32);
			return subtreeStream;
		}
		DispatchResult visitBranchTable(TypeId type,const BranchTable* branchTable)
		{
			auto brTableStream = createTaggedSubtree(Symbol::_br_table);
			for(uintptr tableIndex = 0;tableIndex < branchTable->numTableTargets;++tableIndex)
			{
				brTableStream << getLabelName(branchTable->tableTargets[tableIndex]);
			}
			brTableStream << getLabelName(branchTable->defaultTarget);
			if(branchTable->value) { brTableStream << dispatch(*this,branchTable->value); }
			brTableStream << dispatch(*this,branchTable->index);
			return brTableStream;
		}
		DispatchResult visitReturn(TypeId type,const Return* ret)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_return);
			if(function->type.returnType != TypeId::Void) { subtreeStream << dispatch(*this,ret->value,function->type.returnType); }
			return subtreeStream;
		}
		DispatchResult visitUnreachable(TypeId type,const Unreachable* unreachable)
		{
			return createTaggedSubtree(Symbol::_unreachable);
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
	};

	SNodeOutputStream ModulePrintContext::printFunction(uintptr functionIndex)
	{
		// Before printing, lower the function's expressions into those supported by WAST.
		Function loweredFunction = module->functions[functionIndex];
		LoweringVisitor loweringVisitor(arena,module,&loweredFunction);
		loweredFunction.expression = loweringVisitor(TypedExpression(loweredFunction.expression,loweredFunction.type.returnType)).expression;
		
		FunctionPrintContext functionContext(*this,&loweredFunction);
		auto functionStream = createTaggedSubtree(Symbol::_func);

		// Print the function name.
		functionStream << getFunctionName(functionIndex);

		// Print the function parameters.
		for(auto parameterLocalIndex : loweredFunction.parameterLocalIndices)
		{
			auto parameter = loweredFunction.locals[parameterLocalIndex];
			auto paramStream = createTaggedSubtree(Symbol::_param);
			paramStream << functionContext.getLocalName(parameterLocalIndex);
			paramStream << parameter.type;
			functionStream << paramStream;
		}

		// Print the function return type.
		if(loweredFunction.type.returnType != TypeId::Void)
		{
			auto resultStream = createTaggedSubtree(Symbol::_result);
			resultStream << loweredFunction.type.returnType;
			functionStream << resultStream;
		}

		// Print the function's locals.
		for(uintptr localIndex = 0;localIndex < loweredFunction.locals.size();++localIndex)
		{
			bool isParameter = false;
			for(auto parameterLocalIndex : loweredFunction.parameterLocalIndices)
			{
				if(parameterLocalIndex == localIndex) { isParameter = true; break; }
			}

			if(!isParameter)
			{
				auto localStream = createTaggedSubtree(Symbol::_local);
				localStream << functionContext.getLocalName(localIndex);
				localStream << loweredFunction.locals[localIndex].type;
				functionStream << localStream;
			}
		}

		// Print the function's expression.
		functionStream << dispatch(functionContext,loweredFunction.expression,loweredFunction.type.returnType);

		return functionStream;
	}
	
	SNodeOutputStream ModulePrintContext::printFunctionType(const FunctionType& functionType)
	{
		auto funcStream = createTaggedSubtree(Symbol::_func);

		auto paramStream = createTaggedSubtree(Symbol::_param);
		for(auto parameterType : functionType.parameters) { paramStream << parameterType; }
		funcStream << paramStream;

		if(functionType.returnType != TypeId::Void)
		{
			auto resultStream = createTaggedSubtree(Symbol::_result);
			resultStream << functionType.returnType;
			funcStream << resultStream;
		}
		
		return funcStream;
	}

	SNodeOutputStream ModulePrintContext::print()
	{
		auto moduleStream = createTaggedSubtree(Symbol::_module);
		
		// Print the module memory declarations and data segments.
		auto memoryStream = createTaggedSubtree(Symbol::_memory);
		memoryStream << module->initialNumPagesMemory << module->maxNumPagesMemory;
		for(auto dataSegment : module->dataSegments)
		{
			// Split the data segments up into 32 byte chunks so the S-expression pretty printer uses one line/segment.
			enum { maxDataSegmentSize = 32 };
			for(uintptr offset = 0;offset < dataSegment.numBytes;offset += maxDataSegmentSize)
			{
				auto segmentStream = createTaggedSubtree(Symbol::_segment);
				segmentStream << (dataSegment.baseAddress + offset);
				segmentStream << SNodeOutputStream::StringAtom(
					(const char*)dataSegment.data + offset,
					std::min<uintptr>(maxDataSegmentSize,dataSegment.numBytes - offset)
					);
				memoryStream << segmentStream;
			}
		}
		moduleStream << memoryStream;

		// Print the module imports.
		for(uintptr importFunctionIndex = 0;importFunctionIndex < module->functionImports.size();++importFunctionIndex)
		{
			auto import = module->functionImports[importFunctionIndex];
			auto importStream = createTaggedSubtree(Symbol::_import);
			importStream << getFunctionImportName(importFunctionIndex);
			
			importStream << SNodeOutputStream::StringAtom(import.module,strlen(import.module));
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

		// Print the indirect call signatures.
		for(uintptr elementIndex = 0;elementIndex < module->functionTable.numFunctions;++elementIndex)
		{
			getSignatureIndex(module->functions[module->functionTable.functionIndices[elementIndex]].type);
		}
		for(auto functionTypeIndex : functionTypeToSignatureIndexMap)
		{
			moduleStream << (createTaggedSubtree(Symbol::_type)
				<< getSignatureName(functionTypeIndex.second)
				<< printFunctionType(functionTypeIndex.first));
		}

		// Print the module function table.
		if (module->functionTable.numFunctions != 0)
		{
			auto tableStream = createTaggedSubtree(Symbol::_table);
			for(uintptr elementIndex = 0;elementIndex < module->functionTable.numFunctions;++elementIndex)
			{ tableStream << getFunctionName(module->functionTable.functionIndices[elementIndex]); }
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

		// Print the module functions.
		const uintptr initialNumReferencedSignatures = functionTypeToSignatureIndexMap.size();
		for(uintptr functionIndex = 0;functionIndex < module->functions.size();++functionIndex)
		{
			moduleStream << printFunction(functionIndex);
		}

		// Ensure that there weren't any references to function types first discovered while visiting the function expressions,
		// which occurs after the function types have already been printed.
		if(initialNumReferencedSignatures != functionTypeToSignatureIndexMap.size()) { throw; }

		return moduleStream;
	}

	std::string print(const AST::Module* module)
	{
		Memory::ScopedArena scopedArena;
		return SExp::print((SExp::Node*)ModulePrintContext(scopedArena,module).print().getRoot(),wastSymbols);
	}
}
