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
		#define VISIT_OPCODE(encoding,name,Immediates) \
			void name(Immediates immediates) \
			{ \
				innerVisitor.logOperator(#name + describeImmediates(immediates)); \
				innerVisitor.name(immediates); \
			}
		ENUM_OPCODES(VISIT_OPCODE)
		VISIT_OPCODE(_,unknown,Opcode)
		#undef VISIT_OPCODE
	private:
		const Module& module;
		InnerVisitor& innerVisitor;

		std::string describeImmediates(Opcode opcode) { return std::to_string((uintptr)opcode); }
		std::string describeImmediates(NoImmediates) { return ""; }
		std::string describeImmediates(ControlStructureImmediates imm) { return std::string(" : ") + getTypeName(imm.resultType); }
		std::string describeImmediates(BranchImmediates imm) { return " " + std::to_string(imm.targetDepth); }
		std::string describeImmediates(BranchTableImmediates imm)
		{
			std::string result = " " + std::to_string(imm.defaultTargetDepth);
			const char* prefix = " [";
			for(auto depth : imm.targetDepths) { result += prefix + std::to_string(depth); prefix = ","; }
			result += "]";
			return result;
		}
		template<typename NativeValue>
		std::string describeImmediates(LiteralImmediates<NativeValue> imm) { return " " + std::to_string(imm.value); }
		std::string describeImmediates(GetOrSetVariableImmediates imm) { return " " + std::to_string(imm.variableIndex); }
		std::string describeImmediates(CallImmediates imm) { return " " + std::to_string(imm.functionIndex); }
		std::string describeImmediates(CallIndirectImmediates imm) { return " " + getTypeName(module.types[imm.typeIndex]); }
		std::string describeImmediates(LoadOrStoreImmediates imm) { return " align=" + std::to_string(1<<imm.alignmentLog2) + " offset=" + std::to_string(imm.offset); }
		std::string describeImmediates(ErrorImmediates imm) { return " " + imm.message; }
	};
}