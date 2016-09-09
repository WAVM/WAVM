#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"

#include <vector>

#include "Types.h"

namespace WebAssembly
{
	struct CodeRef
	{
		uintptr offset;
		size_t numBytes;

		CodeRef(): offset(0), numBytes(0) {}
		CodeRef(uintptr inOffset,size_t inNumBytes): offset(inOffset), numBytes(inNumBytes) {}
	};

	struct Function
	{
		// Note: doesn't include parameters.
		std::vector<ValueType> nonParameterLocalTypes;
		uintptr typeIndex;
		CodeRef code;

		Function(): typeIndex(UINTPTR_MAX) {}
		Function(std::vector<ValueType>&& inNonParameterLocalTypes,uintptr inTypeIndex,CodeRef inCode)
		: nonParameterLocalTypes(inNonParameterLocalTypes), typeIndex(inTypeIndex), code(inCode)
		{}
	};
	
	struct ImportType
	{
		ObjectKind kind;
		union
		{
			uintptr functionTypeIndex;
			TableType table;
			MemoryType memory;
			GlobalType global;
		};
		ImportType()							: kind(ObjectKind::invalid) {}
		ImportType(uintptr inFunctionTypeIndex)	: kind(ObjectKind::function), functionTypeIndex(inFunctionTypeIndex) {}
		ImportType(TableType inTable)			: kind(ObjectKind::table), table(inTable) {}
		ImportType(MemoryType inMemory)		: kind(ObjectKind::memory), memory(inMemory) {}
		ImportType(GlobalType inGlobal)			: kind(ObjectKind::global), global(inGlobal) {}
	};

	struct Import
	{
		std::string module;
		std::string exportName;
		ImportType type;
	};

	struct Export
	{
		ObjectKind kind;
		uintptr index;
		std::string name;
	};
	
	struct InitializerExpression
	{
		enum class Type : uint8
		{
			i32_const = 0x10,
			i64_const = 0x11,
			f32_const = 0x13,
			f64_const = 0x12,
			get_global = 0x1a,
			error = 0xff
		};
		Type type;
		union
		{
			int32 i32;
			int64 i64;
			float32 f32;
			float64 f64;
			uintptr globalIndex;
		};
		InitializerExpression(): type(Type::error) {}
		InitializerExpression(int32 inI32): type(Type::i32_const), i32(inI32) {}
		InitializerExpression(int64 inI64): type(Type::i64_const), i64(inI64) {}
		InitializerExpression(float32 inF32): type(Type::f32_const), f32(inF32) {}
		InitializerExpression(float64 inF64): type(Type::f64_const), f64(inF64) {}
		InitializerExpression(Type inType,uintptr inGlobalIndex): type(inType), globalIndex(inGlobalIndex) { assert(inType == Type::get_global); }
	};

	struct Global
	{
		GlobalType type;
		InitializerExpression initializer;
	};

	struct DataSegment
	{
		InitializerExpression baseOffset;
		std::vector<uint8> data;

		DataSegment() {}
		DataSegment(InitializerExpression inBaseOffset,const std::vector<uint8>& inData)
		: baseOffset(inBaseOffset), data(inData) {}
	};

	struct TableSegment
	{
		InitializerExpression baseOffset;
		std::vector<uintptr> indices;
		
		TableSegment() {}
		TableSegment(InitializerExpression inBaseOffset,const std::vector<uintptr>& inIndices)
		: baseOffset(inBaseOffset), indices(inIndices) {}
	};

	struct FunctionDisassemblyInfo
	{
		std::string name;
		std::vector<std::string> localNames;
	};

	struct ModuleDisassemblyInfo
	{
		std::vector<FunctionDisassemblyInfo> functions;
	};

	struct Module
	{
		std::vector<const FunctionType*> types;
		std::vector<Import> imports;
		std::vector<Function> functionDefs;
		std::vector<TableType> tableDefs;
		std::vector<MemoryType> memoryDefs;
		std::vector<Global> globalDefs;
		std::vector<Export> exports;
		std::vector<uint8> code;
		std::vector<DataSegment> dataSegments;
		std::vector<TableSegment> tableSegments;
		
		ModuleDisassemblyInfo disassemblyInfo;

		uintptr startFunctionIndex;

		Module() : startFunctionIndex(UINTPTR_MAX) {}
	};

	inline ObjectType resolveImportType(const Module& module,const ImportType& type)
	{
		switch(type.kind)
		{
		case ObjectKind::function: return ObjectType(module.types[type.functionTypeIndex]);
		case ObjectKind::table: return ObjectType(type.table);
		case ObjectKind::memory: return ObjectType(type.memory);
		case ObjectKind::global: return ObjectType(type.global);
		default: throw;
		}
	}
}
