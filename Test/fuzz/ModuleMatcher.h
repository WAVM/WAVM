#pragma once

#include "WAVM/IR/Module.h"
#include "WAVM/IR/Operators.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM {
	using namespace WAVM::IR;
	struct ModuleMatcher
	{
		ModuleMatcher(const Module& inA, const Module& inB) : aModule(inA), bModule(inB) {}

		void verify()
		{
			verifyMatches(aModule.imports, bModule.imports);
			verifyMatches(aModule.exports, bModule.exports);

			verifyMatches(aModule.functions, bModule.functions);
			verifyMatches(aModule.tables, bModule.tables);
			verifyMatches(aModule.memories, bModule.memories);
			verifyMatches(aModule.globals, bModule.globals);
			verifyMatches(aModule.exceptionTypes, bModule.exceptionTypes);

			if(aModule.startFunctionIndex != bModule.startFunctionIndex) { failVerification(); }

			if(aModule.dataSegments.size() != bModule.dataSegments.size()) { failVerification(); }
			for(Uptr segmentIndex = 0; segmentIndex < aModule.dataSegments.size(); ++segmentIndex)
			{
				const DataSegment& segment = aModule.dataSegments[segmentIndex];
				const DataSegment& wastSegment = bModule.dataSegments[segmentIndex];
				if(segment.isActive != wastSegment.isActive) { failVerification(); }
				if(segment.isActive
				   && (segment.memoryIndex != wastSegment.memoryIndex
					   || segment.baseOffset != wastSegment.baseOffset
					   || *segment.data != *wastSegment.data))
				{ failVerification(); }
			}

			if(aModule.elemSegments.size() != bModule.elemSegments.size()) { failVerification(); }
			for(Uptr segmentIndex = 0; segmentIndex < aModule.elemSegments.size(); ++segmentIndex)
			{
				const ElemSegment& aSegment = aModule.elemSegments[segmentIndex];
				const ElemSegment& bSegment = bModule.elemSegments[segmentIndex];
				if(aSegment.type != bSegment.type
				   || aSegment.contents->encoding != bSegment.contents->encoding)
				{ failVerification(); }
				if(aSegment.type == ElemSegment::Type::active
				   && (aSegment.tableIndex != bSegment.tableIndex
					   || aSegment.baseOffset != bSegment.baseOffset))
				{ failVerification(); }
				switch(aSegment.contents->encoding)
				{
				case ElemSegment::Encoding::expr:
					if(aSegment.contents->elemType != bSegment.contents->elemType
					   || aSegment.contents->elemExprs != bSegment.contents->elemExprs)
					{ failVerification(); }
					break;

				case ElemSegment::Encoding::index:
					if(aSegment.contents->externKind != bSegment.contents->externKind
					   || aSegment.contents->elemIndices != bSegment.contents->elemIndices)
					{ failVerification(); }
					break;

				default: WAVM_UNREACHABLE();
				};
			}

			if(aModule.customSections.size() != bModule.customSections.size())
			{ failVerification(); }
			for(Uptr customSectionIndex = 0; customSectionIndex < aModule.customSections.size();
				++customSectionIndex)
			{
				const CustomSection& aSection = aModule.customSections[customSectionIndex];
				const CustomSection& bSection = bModule.customSections[customSectionIndex];
				if(aSection.afterSection != bSection.afterSection || aSection.name != bSection.name
				   || aSection.data != bSection.data)
				{ failVerification(); }
			}
		}

	private:
		const Module& aModule;
		const Module& bModule;

		const FunctionDef* aFunction = nullptr;
		const FunctionDef* bFunction = nullptr;

		[[noreturn]] void failVerification()
		{
			Errors::fatal("Disassembled module doesn't match the input");
		}

		void verifyMatches(const IndexedFunctionType& a, const IndexedFunctionType& b)
		{
			if(aModule.types[a.index] != bModule.types[b.index]) { failVerification(); }
		}

		void verifyMatches(const KindAndIndex& a, const KindAndIndex& b)
		{
			if(a.kind != b.kind || a.index != b.index) { failVerification(); }
		}

		void verifyMatches(const Export& a, const Export& b)
		{
			if(a.name != b.name || a.kind != b.kind || a.index != b.index) { failVerification(); }
		}

		template<typename Type> void verifyMatches(const Import<Type>& a, const Import<Type>& b)
		{
			verifyMatches(a.type, b.type);
			if(a.moduleName != b.moduleName || a.exportName != b.exportName) { failVerification(); }
		}

		void verifyMatches(NoImm, NoImm) {}
		void verifyMatches(MemoryImm a, MemoryImm b)
		{
			if(a.memoryIndex != b.memoryIndex) { failVerification(); }
		}
		void verifyMatches(MemoryCopyImm a, MemoryCopyImm b)
		{
			if(a.sourceMemoryIndex != b.sourceMemoryIndex || a.destMemoryIndex != b.destMemoryIndex)
			{ failVerification(); }
		}
		void verifyMatches(TableImm a, TableImm b)
		{
			if(a.tableIndex != b.tableIndex) { failVerification(); }
		}
		void verifyMatches(TableCopyImm a, TableCopyImm b)
		{
			if(a.sourceTableIndex != b.sourceTableIndex || a.destTableIndex != b.destTableIndex)
			{ failVerification(); }
		}

		void verifyMatches(ControlStructureImm a, ControlStructureImm b)
		{
			if(resolveBlockType(aModule, a.type) != resolveBlockType(bModule, b.type))
			{ failVerification(); }
		}

		void verifyMatches(SelectImm a, SelectImm b)
		{
			if(a.type != b.type) { failVerification(); }
		}

		void verifyMatches(BranchImm a, BranchImm b)
		{
			if(a.targetDepth != b.targetDepth) { failVerification(); }
		}

		void verifyMatches(BranchTableImm a, BranchTableImm b)
		{
			if(a.defaultTargetDepth != b.defaultTargetDepth
			   || aFunction->branchTables[a.branchTableIndex]
					  != bFunction->branchTables[b.branchTableIndex])
			{ failVerification(); }
		}

		template<typename Value> void verifyMatches(LiteralImm<Value> a, LiteralImm<Value> b)
		{
			if(memcmp(&a.value, &b.value, sizeof(Value))) { failVerification(); }
		}

		template<bool isGlobal>
		void verifyMatches(GetOrSetVariableImm<isGlobal> a, GetOrSetVariableImm<isGlobal> b)
		{
			if(a.variableIndex != b.variableIndex) { failVerification(); }
		}

		void verifyMatches(FunctionImm a, FunctionImm b)
		{
			if(a.functionIndex != b.functionIndex) { failVerification(); }
		}

		void verifyMatches(FunctionRefImm a, FunctionRefImm b)
		{
			if(a.functionIndex != b.functionIndex) { failVerification(); }
		}

		void verifyMatches(CallIndirectImm a, CallIndirectImm b)
		{
			if(aModule.types[a.type.index] != bModule.types[b.type.index]) { failVerification(); }
		}

		void verifyMatches(BaseLoadOrStoreImm a, BaseLoadOrStoreImm b)
		{
			if(a.alignmentLog2 != b.alignmentLog2 || a.offset != b.offset
			   || a.memoryIndex != b.memoryIndex)
			{ failVerification(); }
		}

		template<Uptr numLanes>
		void verifyMatches(LaneIndexImm<numLanes> a, LaneIndexImm<numLanes> b)
		{
			if(a.laneIndex != b.laneIndex) { failVerification(); }
		}

		template<Uptr numLanes> void verifyMatches(ShuffleImm<numLanes> a, ShuffleImm<numLanes> b)
		{
			for(Uptr laneIndex = 0; laneIndex < numLanes; ++laneIndex)
			{
				if(a.laneIndices[laneIndex] != b.laneIndices[laneIndex]) { failVerification(); }
			}
		}

		void verifyMatches(AtomicFenceImm a, AtomicFenceImm b)
		{
			if(a.order != b.order) { failVerification(); }
		}

		void verifyMatches(ExceptionTypeImm a, ExceptionTypeImm b)
		{
			if(a.exceptionTypeIndex != b.exceptionTypeIndex) { failVerification(); }
		}

		void verifyMatches(RethrowImm a, RethrowImm b)
		{
			if(a.catchDepth != b.catchDepth) { failVerification(); }
		}

		void verifyMatches(DataSegmentAndMemImm a, DataSegmentAndMemImm b)
		{
			if(a.dataSegmentIndex != b.dataSegmentIndex) { failVerification(); }
			if(a.memoryIndex != b.memoryIndex) { failVerification(); }
		}

		void verifyMatches(DataSegmentImm a, DataSegmentImm b)
		{
			if(a.dataSegmentIndex != b.dataSegmentIndex) { failVerification(); }
		}

		void verifyMatches(ElemSegmentAndTableImm a, ElemSegmentAndTableImm b)
		{
			if(a.elemSegmentIndex != b.elemSegmentIndex) { failVerification(); }
			if(a.tableIndex != b.tableIndex) { failVerification(); }
		}

		void verifyMatches(ElemSegmentImm a, ElemSegmentImm b)
		{
			if(a.elemSegmentIndex != b.elemSegmentIndex) { failVerification(); }
		}

		void verifyMatches(ReferenceTypeImm a, ReferenceTypeImm b)
		{
			if(a.referenceType != b.referenceType) { failVerification(); }
		}

		void verifyMatches(const FunctionDef& a, const FunctionDef& b)
		{
			aFunction = &a;
			bFunction = &b;

			verifyMatches(a.type, b.type);
			if(a.nonParameterLocalTypes != b.nonParameterLocalTypes) { failVerification(); }

			if(a.code.size() != b.code.size()) { failVerification(); }

			const U8* aNextByte = a.code.data();
			const U8* bNextByte = b.code.data();
			const U8* aEnd = a.code.data() + a.code.size();
			const U8* bEnd = b.code.data() + b.code.size();

			while(aNextByte < aEnd && bNextByte < bEnd)
			{
				WAVM_ASSERT(aNextByte + sizeof(Opcode) <= aEnd);
				WAVM_ASSERT(bNextByte + sizeof(Opcode) <= bEnd);

				Opcode aOpcode;
				Opcode bOpcode;
				memcpy(&aOpcode, aNextByte, sizeof(Opcode));
				memcpy(&bOpcode, bNextByte, sizeof(Opcode));
				if(aOpcode != bOpcode) { failVerification(); }

				switch(aOpcode)
				{
#define VISIT_OPCODE(opcode, name, nameString, Imm, ...)                                           \
	case Opcode::name: {                                                                           \
		WAVM_ASSERT(aNextByte + sizeof(OpcodeAndImm<Imm>) <= aEnd);                                \
		WAVM_ASSERT(bNextByte + sizeof(OpcodeAndImm<Imm>) <= bEnd);                                \
		OpcodeAndImm<Imm> aEncodedOperator;                                                        \
		OpcodeAndImm<Imm> bEncodedOperator;                                                        \
		memcpy(&aEncodedOperator, aNextByte, sizeof(OpcodeAndImm<Imm>));                           \
		memcpy(&bEncodedOperator, bNextByte, sizeof(OpcodeAndImm<Imm>));                           \
		aNextByte += sizeof(OpcodeAndImm<Imm>);                                                    \
		bNextByte += sizeof(OpcodeAndImm<Imm>);                                                    \
		verifyMatches(aEncodedOperator.imm, bEncodedOperator.imm);                                 \
		break;                                                                                     \
	}
					WAVM_ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
				default: WAVM_UNREACHABLE();
				}
			}
			WAVM_ASSERT(aNextByte == aEnd);
			WAVM_ASSERT(bNextByte == bEnd);

			aFunction = nullptr;
			bFunction = nullptr;
		}

		void verifyMatches(const TableType& a, const TableType& b)
		{
			if(a != b) { failVerification(); }
		}

		void verifyMatches(const MemoryType& a, const MemoryType& b)
		{
			if(a != b) { failVerification(); }
		}

		void verifyMatches(const MemoryDef& a, const MemoryDef& b)
		{
			verifyMatches(a.type, b.type);
		}

		void verifyMatches(const GlobalType& a, const GlobalType& b)
		{
			if(a != b) { failVerification(); }
		}

		void verifyMatches(const GlobalDef& a, const GlobalDef& b)
		{
			verifyMatches(a.type, b.type);
			if(a.initializer != b.initializer) { failVerification(); }
		}

		void verifyMatches(const ExceptionType& a, const ExceptionType& b)
		{
			if(a != b) { failVerification(); }
		}

		void verifyMatches(const ExceptionTypeDef& a, const ExceptionTypeDef& b)
		{
			verifyMatches(a.type, b.type);
		}

		void verifyMatches(const TableDef& a, const TableDef& b) { verifyMatches(a.type, b.type); }

		template<typename Element>
		void verifyMatches(const std::vector<Element>& a, const std::vector<Element>& b)
		{
			if(a.size() != b.size()) { failVerification(); }
			for(Uptr index = 0; index < a.size(); ++index) { verifyMatches(a[index], b[index]); }
		}

		template<typename Definition, typename Type>
		void verifyMatches(const IndexSpace<Definition, Type>& a,
						   const IndexSpace<Definition, Type>& b)
		{
			verifyMatches(a.imports, b.imports);
			verifyMatches(a.defs, b.defs);
		}
	};
}
