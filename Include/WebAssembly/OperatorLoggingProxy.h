#pragma once

#include "WebAssembly.h"
#include "Core/Serialization.h"
#include "Module.h"
#include "Operations.h"

namespace WebAssembly
{
	template<typename InnerVisitor>
	struct OperatorLoggingProxy
	{
		OperatorLoggingProxy(const Module& inModule,InnerVisitor& inInnerVisitor): module(inModule), innerVisitor(inInnerVisitor) {}
		#define VISIT_OPCODE(encoding,name,Imm) \
			void name(Imm imm) \
			{ \
				innerVisitor.logOperator(#name + describeImm(imm)); \
				innerVisitor.name(imm); \
			}
		ENUM_OPS(VISIT_OPCODE)
		VISIT_OPCODE(_,unknown,Opcode)
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
		std::string describeImm(CallImm imm) { return " " + std::to_string(imm.functionIndex); }
		std::string describeImm(CallIndirectImm imm) { return " " + asString(module.types[imm.typeIndex]); }
		std::string describeImm(LoadOrStoreImm imm) { return " align=" + std::to_string(1<<imm.alignmentLog2) + " offset=" + std::to_string(imm.offset); }
		std::string describeImm(MemoryImm) { return ""; }
		std::string describeImm(ErrorImm imm) { return " " + imm.message; }
	};
}