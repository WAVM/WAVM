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
		std::string name;
		ObjectKind kind;
		uintptr index;
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
		uintptr memoryIndex;
		InitializerExpression baseOffset;
		std::vector<uint8> data;

		DataSegment() {}
		DataSegment(uintptr inMemoryIndex,InitializerExpression inBaseOffset,const std::vector<uint8>& inData)
		: memoryIndex(inMemoryIndex), baseOffset(inBaseOffset), data(inData) {}
	};

	struct TableSegment
	{
		uintptr tableIndex;
		InitializerExpression baseOffset;
		std::vector<uintptr> indices;
		
		TableSegment() {}
		TableSegment(uintptr inTableIndex,InitializerExpression inBaseOffset,const std::vector<uintptr>& inIndices)
		: tableIndex(inTableIndex), baseOffset(inBaseOffset), indices(inIndices) {}
	};

	struct UserSection
	{
		std::string name;
		std::vector<uint8> data;
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
		std::vector<UserSection> userSections;

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
		default: Core::unreachable();
		}
	}
	
	inline bool findUserSection(const Module& module,const char* userSectionName,uintptr& outUserSectionIndex)
	{
		for(uintptr sectionIndex = 0;sectionIndex < module.userSections.size();++sectionIndex)
		{
			if(module.userSections[sectionIndex].name == userSectionName)
			{
				outUserSectionIndex = sectionIndex;
				return true;
			}
		}
		return false;
	}

	struct DisassemblyNames
	{
		struct FunctionDef
		{
			std::vector<std::string> locals;
		};

		std::vector<std::string> types;
		std::vector<std::string> functions;
		std::vector<std::string> tables;
		std::vector<std::string> memories;
		std::vector<std::string> globals;
		std::vector<FunctionDef> functionDefs;
	};

	// Looks for a name section in a module. If it exists, deserialize it into outNames.
	// If it doesn't exist, fill outNames with sensible defaults.
	WEBASSEMBLY_API void getDisassemblyNames(const Module& module,DisassemblyNames& outNames);

	// Serializes a DisassemblyNames structure and adds it to the module as a name section.
	WEBASSEMBLY_API void setDisassemblyNames(Module& module,const DisassemblyNames& names);
}
