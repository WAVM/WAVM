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
					   || segment.data != wastSegment.data))
				{ failVerification(); }
			}

			if(aModule.elemSegments.size() != bModule.elemSegments.size()) { failVerification(); }
			for(Uptr segmentIndex = 0; segmentIndex < aModule.elemSegments.size(); ++segmentIndex)
			{
				const ElemSegment& segment = aModule.elemSegments[segmentIndex];
				const ElemSegment& wastSegment = bModule.elemSegments[segmentIndex];
				if(segment.isActive != wastSegment.isActive) { failVerification(); }
				if(segment.isActive
				   && (segment.tableIndex != wastSegment.tableIndex
					   || segment.baseOffset != wastSegment.baseOffset
					   || segment.elems != wastSegment.elems))
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

		void verifyMatches(CallIndirectImm a, CallIndirectImm b)
		{
			if(aModule.types[a.type.index] != bModule.types[b.type.index]) { failVerification(); }
		}

		template<Uptr naturalAlignmentLog2>
		void verifyMatches(LoadOrStoreImm<naturalAlignmentLog2> a,
						   LoadOrStoreImm<naturalAlignmentLog2> b)
		{
			if(a.alignmentLog2 != b.alignmentLog2 || a.offset != b.offset) { failVerification(); }
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

		template<Uptr naturalAlignmentLog2>
		void verifyMatches(AtomicLoadOrStoreImm<naturalAlignmentLog2> a,
						   AtomicLoadOrStoreImm<naturalAlignmentLog2> b)
		{
			if(a.alignmentLog2 != b.alignmentLog2 || a.offset != b.offset) { failVerification(); }
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
				wavmAssert(aNextByte + sizeof(Opcode) <= aEnd);
				wavmAssert(bNextByte + sizeof(Opcode) <= bEnd);

				Opcode aOpcode;
				Opcode bOpcode;
				memcpy(&aOpcode, aNextByte, sizeof(Opcode));
				memcpy(&bOpcode, bNextByte, sizeof(Opcode));
				if(aOpcode != bOpcode) { failVerification(); }

				switch(aOpcode)
				{
#define VISIT_OPCODE(opcode, name, nameString, Imm, ...)                                           \
	case Opcode::name:                                                                             \
	{                                                                                              \
		wavmAssert(aNextByte + sizeof(OpcodeAndImm<Imm>) <= aEnd);                                 \
		wavmAssert(bNextByte + sizeof(OpcodeAndImm<Imm>) <= bEnd);                                 \
		OpcodeAndImm<Imm> aEncodedOperator;                                                        \
		OpcodeAndImm<Imm> bEncodedOperator;                                                        \
		memcpy(&aEncodedOperator, aNextByte, sizeof(OpcodeAndImm<Imm>));                           \
		memcpy(&bEncodedOperator, bNextByte, sizeof(OpcodeAndImm<Imm>));                           \
		aNextByte += sizeof(OpcodeAndImm<Imm>);                                                    \
		bNextByte += sizeof(OpcodeAndImm<Imm>);                                                    \
		verifyMatches(aEncodedOperator.imm, bEncodedOperator.imm);                                 \
		break;                                                                                     \
	}
					ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE
				default: Errors::unreachable();
				}
			}
			wavmAssert(aNextByte == aEnd);
			wavmAssert(bNextByte == bEnd);

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

		template<typename Definition, typename Type>
		void verifyMatches(const IndexSpace<Definition, Type>& a,
						   const IndexSpace<Definition, Type>& b)
		{
			if(a.imports.size() != b.imports.size()) { failVerification(); }
			for(Uptr importIndex = 0; importIndex < a.imports.size(); ++importIndex)
			{ verifyMatches(a.imports[importIndex], b.imports[importIndex]); }

			if(a.defs.size() != b.defs.size()) { failVerification(); }
			for(Uptr defIndex = 0; defIndex < a.defs.size(); ++defIndex)
			{ verifyMatches(a.defs[defIndex], b.defs[defIndex]); }
		}
	};
}
