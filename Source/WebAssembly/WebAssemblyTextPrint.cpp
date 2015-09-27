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
	//	- Switches with default case in the middle.
	struct LoweringVisitor : MapChildrenVisitor<LoweringVisitor&,TypedExpression>
	{
		LoweringVisitor(Memory::Arena& inArena,const Module* inModule,Function* inFunction)
		: MapChildrenVisitor(inArena,inModule,inFunction,*this) {}

		TypedExpression operator()(TypedExpression child)
		{
			return dispatch(*this,child);
		}

		template<typename Class>
		DispatchResult visitSwitch(TypeId type,const Switch<Class>* switch_)
		{
			// Convert switches so the default arm is always last.
			if(switch_->defaultArmIndex == switch_->numArms - 1) { return MapChildrenVisitor::visitSwitch(type,switch_); }
			else
			{
				auto key = visitChild(switch_->key);

				// Create a switch that just maps each case of the original switch to the arm index that handles it.
				Switch<IntClass>* armIndexSwitch;
				{
					auto switchEndTarget = new(arena) BranchTarget(TypeId::I64);
					auto switchArms = new(arena) SwitchArm[switch_->numArms];
					SwitchArm* switchArm = switchArms;
					for(uintptr armIndex = 0;armIndex < switch_->numArms;++armIndex)
					{
						if(armIndex != switch_->defaultArmIndex)
						{
							switchArm->key = switch_->arms[armIndex].key;
							switchArm->value = new(arena) Branch<IntClass>(switchEndTarget,new(arena) Literal<I64Type>(armIndex));
							++switchArm;
						}
					}
					switchArm->key = 0;
					switchArm->value = new(arena) Literal<I64Type>(switch_->defaultArmIndex);
					armIndexSwitch = new(arena) Switch<IntClass>(key,switch_->numArms - 1,switch_->numArms,switchArms,switchEndTarget);
				}

				// Create a switch that maps the arm index to the appropriate expression from the original switch.
				auto cfgEndTarget = new(arena) BranchTarget(type);
				branchTargetRemap[switch_->endTarget] = cfgEndTarget;

				auto cfgArms = new(arena) SwitchArm[switch_->numArms];
				for(uintptr armIndex = 0;armIndex < switch_->numArms;++armIndex)
				{
					auto armType = armIndex + 1 == switch_->numArms ? type : TypeId::Void;
					cfgArms[armIndex].key = armIndex;
					cfgArms[armIndex].value = visitChild(TypedExpression(switch_->arms[armIndex].value,armType)).expression;
				}
				return TypedExpression(new(arena) Switch<Class>(
					TypedExpression(armIndexSwitch,TypeId::I64),
					switch_->numArms - 1,
					switch_->numArms,
					cfgArms,
					cfgEndTarget
					),type);
			}
		}
	};

	struct ModulePrintContext
	{
		Memory::Arena& arena;
		const Module* module;

		ModulePrintContext(Memory::Arena& inArena,const Module* inModule)
		: arena(inArena), module(inModule) {}
		
		std::string getFunctionImportName(uintptr importIndex) const
		{
			assert(module->functionImports[importIndex].name);
			return std::string("$_") + module->functionImports[importIndex].name;
		}
		std::string getFunctionName(uintptr functionIndex) const
		{
			if(module->functions[functionIndex]->name) { return std::string("$_") + module->functions[functionIndex]->name; }
			else { return "$func" + std::to_string(functionIndex); }
		}
		std::string getFunctionTableName(uintptr functionTableIndex) const
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
		SNodeOutputStream createAlignedTypedTaggedSubtree(TypeId type,Symbol symbol,uint8 alignmentLog2,TypeId memoryType) { auto subtree = createSubtree(); subtree << getAlignedTypedSymbol(type,symbol,alignmentLog2,memoryType); return subtree; }
		SNodeOutputStream createBitypedTaggedSubtree(TypeId leftType,Symbol symbol,TypeId rightType) { auto subtree = createSubtree(); subtree << getBitypedSymbol(leftType,symbol,rightType); return subtree; }

		SNodeOutputStream printFunction(uintptr functionIndex);
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
			// Just output floats as reinterpreted ints for now.
			//if(std::isnan(literal->value) || std::isinf(literal->value))
			{
				union { float32 floatLiteral; uint32 reinterpretedAsInt; };
				floatLiteral = literal->value;
				SNodeOutputStream bitsStream = createTypedTaggedSubtree(TypeId::I32,Symbol::_const) << reinterpretedAsInt;
				return createBitypedTaggedSubtree(TypeId::F32,Symbol::_reinterpret,TypeId::I32) << bitsStream;
			}
			//else { return createTypedTaggedSubtree(TypeId::F32,Symbol::_const) << literal->value; }
		}
		
		DispatchResult visitLiteral(const Literal<F64Type>* literal)
		{
			// Just output floats as reinterpreted ints for now.
			//if(std::isnan(literal->value) || std::isinf(literal->value))
			{
				union { float64 floatLiteral; uint64 reinterpretedAsInt; };
				floatLiteral = literal->value;
				SNodeOutputStream bitsStream = createTypedTaggedSubtree(TypeId::I64,Symbol::_const) << reinterpretedAsInt;
				return createBitypedTaggedSubtree(TypeId::F64,Symbol::_reinterpret,TypeId::I64) << bitsStream;
			}
			//else { return createTypedTaggedSubtree(TypeId::F64,Symbol::_const) << literal->value; }
		}

		template<typename Class>
		DispatchResult visitError(TypeId type,const Error<Class>* error)
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

		template<typename Class,typename OpAsType>
		DispatchResult visitLoad(TypeId type,const Load<Class>* load,OpAsType)
		{
			assert(load->memoryType == type);
			return createAlignedTypedTaggedSubtree(type,getOpSymbol(load->op()),load->alignmentLog2,load->memoryType)
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
			return createAlignedTypedTaggedSubtree(type,symbol,load->alignmentLog2,load->memoryType)
				<< dispatch(*this,load->address,load->isFarAddress ? TypeId::I64 : TypeId::I32);
		}
		template<typename Class>
		DispatchResult visitStore(const Store<Class>* store)
		{
			assert(store->memoryType == store->value.type);
			return createAlignedTypedTaggedSubtree(store->value.type,getOpSymbol(store->op()),store->alignmentLog2,store->memoryType)
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
			return createAlignedTypedTaggedSubtree(store->value.type,symbol,store->alignmentLog2,store->memoryType)
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
			auto functionName = call->op() == AnyOp::callDirect ? getFunctionName(call->functionIndex) : getFunctionImportName(call->functionIndex);
			auto functionType = call->op() == AnyOp::callDirect ? module->functions[call->functionIndex]->type : module->functionImports[call->functionIndex].type;
			subtreeStream << functionName;
			for(uintptr parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				subtreeStream << dispatch(*this,call->parameters[parameterIndex],functionType.parameters[parameterIndex]);
			}
			return subtreeStream;
		}
		DispatchResult visitCallIndirect(TypeId type,const CallIndirect* callIndirect)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_call_indirect)
				<< getFunctionTableName(callIndirect->tableIndex)
				<< dispatch(*this,callIndirect->functionIndex,TypeId::I32);
			auto functionType = module->functionTables[callIndirect->tableIndex].type;
			for(uintptr parameterIndex = 0;parameterIndex < functionType.parameters.size();++parameterIndex)
			{
				subtreeStream << dispatch(*this,callIndirect->parameters[parameterIndex],functionType.parameters[parameterIndex]);
			}
			return subtreeStream;
		}
		
		template<typename Class>
		DispatchResult visitSwitch(TypeId type,const Switch<Class>* switch_)
		{
			// The lowering pass should only give us switches with the default arm as the final arm.
			assert(switch_->defaultArmIndex == switch_->numArms - 1);
			auto switchStream = createTypedTaggedSubtree(switch_->key.type,Symbol::_switch);
			switchStream << getLabelName(switch_->endTarget);
			switchStream << dispatch(*this,switch_->key);
			for(uintptr armIndex = 0;armIndex < switch_->numArms - 1;++armIndex)
			{
				auto caseSubstream = createTaggedSubtree(Symbol::_case) << switch_->arms[armIndex].key;
				caseSubstream << dispatch(*this,switch_->arms[armIndex].value,TypeId::Void);
				caseSubstream << Symbol::_fallthrough;
				switchStream << caseSubstream;
			}
			switchStream << dispatch(*this,switch_->arms[switch_->numArms - 1].value,type);
			
			return switchStream;
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
		DispatchResult visitReturn(TypeId type,const Return<Class>* ret)
		{
			auto subtreeStream = createTaggedSubtree(Symbol::_return);
			if(function->type.returnType != TypeId::Void) { subtreeStream << dispatch(*this,ret->value,function->type.returnType); }
			return subtreeStream;
		}
		template<typename Class>
		DispatchResult visitLoop(TypeId type,const Loop<Class>* loop)
		{
			// Simulate the loop break/continue labels with two label nodes.
			auto loopStream = createTaggedSubtree(Symbol::_loop);
			loopStream << getLabelName(loop->breakTarget) << getLabelName(loop->continueTarget);
			auto bodyStream = dispatch(*this,loop->expression);
			loopStream << bodyStream;
			return loopStream;
		}
		template<typename Class>
		DispatchResult visitBranch(TypeId type,const Branch<Class>* branch)
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
	};

	SNodeOutputStream ModulePrintContext::printFunction(uintptr functionIndex)
	{
		// Before printing, lower the function's expressions into those supported by WAST.
		Function loweredFunction = *module->functions[functionIndex];
		LoweringVisitor loweringVisitor(arena,module,&loweredFunction);
		loweredFunction.expression = loweringVisitor(TypedExpression(loweredFunction.expression,loweredFunction.type.returnType)).expression;
		
		FunctionPrintContext functionContext(arena,module,&loweredFunction);
		auto functionStream = createTaggedSubtree(Symbol::_func);

		// Print the function name.
		functionStream << functionContext.getFunctionName(functionIndex);

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

	SNodeOutputStream ModulePrintContext::print()
	{
		auto moduleStream = createTaggedSubtree(Symbol::_module);
		
		// Print the module memory declarations and data segments.
		auto memoryStream = createTaggedSubtree(Symbol::_memory);
		memoryStream << module->initialNumBytesMemory << module->maxNumBytesMemory;
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

		// Print the module function tables.
		for(uintptr functionTableIndex = 0;functionTableIndex < module->functionTables.size();++functionTableIndex)
		{
			auto functionTable = module->functionTables[functionTableIndex];
			auto tableStream = createTaggedSubtree(Symbol::_table);
			for(uintptr elementIndex = 0;elementIndex < functionTable.numFunctions;++elementIndex)
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

		// Print the module functions.
		for(uintptr functionIndex = 0;functionIndex < module->functions.size();++functionIndex)
		{
			moduleStream << printFunction(functionIndex);
		}

		return moduleStream;
	}

	std::string print(const AST::Module* module)
	{
		Memory::ScopedArena scopedArena;
		return SExp::print((SExp::Node*)ModulePrintContext(scopedArena,module).print().getRoot(),wastSymbols);
	}
}
