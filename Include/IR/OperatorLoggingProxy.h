#pragma once

#include "Core/Core.h"
#include "Inline/Serialization.h"
#include "IR.h"
#include "Module.h"
#include "Operators.h"

namespace IR
{
	template<typename InnerVisitor>
	struct OperatorLoggingProxy
	{
		OperatorLoggingProxy(const Module& inModule,InnerVisitor& inInnerVisitor): module(inModule), innerVisitor(inInnerVisitor) {}
		#define VISIT_OPCODE(encoding,name,nameString,Imm) \
			void name(Imm imm = {}) \
			{ \
				innerVisitor.logOperator(nameString + describeImm(imm)); \
				innerVisitor.name(imm); \
			}
		ENUM_OPERATORS(VISIT_OPCODE)
		VISIT_OPCODE(_,unknown,"unknown",Opcode)
		#undef VISIT_OPCODE
	private:
		const Module& module;
		InnerVisitor& innerVisitor;

		std::string describeImm(Opcode opcode) { return std::to_string((uintp)opcode); }
		std::string describeImm(NoImm) { return ""; }
		std::string describeImm(ControlStructureImm imm) { return std::string(" : ") + asString(imm.resultType); }
		std::string describeImm(BranchImm imm) { return " " + std::to_string(imm.targetDepth); }
		std::string describeImm(BranchTableImm imm)
		{
			std::string result = " " + std::to_string(imm.defaultTargetDepth);
			const char* prefix = " [";
			for(auto depth : imm.targetDepths) { result += prefix + std::to_string(depth); prefix = ","; }
			result += "]";
			return result;
		}
		template<typename NativeValue>
		std::string describeImm(LiteralImm<NativeValue> imm) { return " " + std::to_string(imm.value); }
		std::string describeImm(GetOrSetVariableImm imm) { return " " + std::to_string(imm.variableIndex); }
		std::string describeImm(CallImm imm)
		{
			const std::string typeString = imm.functionIndex >= module.functions.size()
				? "<invalid function index>"
				: asString(module.types[module.functions.getType(imm.functionIndex).index]);
			return " " + std::to_string(imm.functionIndex) + " " + typeString;
		}
		std::string describeImm(CallIndirectImm imm)
		{
			const std::string typeString = imm.type.index >= module.types.size()
				? "<invalid type index>"
				: asString(module.types[imm.type.index]);
			return " " + typeString;
		}
		std::string describeImm(LoadOrStoreImm imm) { return " align=" + std::to_string(1<<imm.alignmentLog2) + " offset=" + std::to_string(imm.offset); }
		std::string describeImm(MemoryImm) { return ""; }
		std::string describeImm(ErrorImm imm) { return " " + imm.message; }
	};
}