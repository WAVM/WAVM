#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"

#include <vector>

#include "Types.h"

namespace WebAssembly
{
	// A reference to a function's code within the owning module's code array
	struct CodeRef
	{
		uintp offset;
		size_t numBytes;

		CodeRef(): offset(0), numBytes(0) {}
		CodeRef(uintp inOffset,size_t inNumBytes): offset(inOffset), numBytes(inNumBytes) {}
	};

	// A function definition
	struct Function
	{
		std::vector<ValueType> nonParameterLocalTypes;
		uintp typeIndex;
		CodeRef code;

		Function(): typeIndex(UINTPTR_MAX) {}
		Function(std::vector<ValueType>&& inNonParameterLocalTypes,uintp inTypeIndex,CodeRef inCode)
		: nonParameterLocalTypes(inNonParameterLocalTypes), typeIndex(inTypeIndex), code(inCode)
		{}
	};
	
	// The type of an imported object.
	// This is very similar to ObjectType, but refers to a function type by index into the module's type table instead of pointer.
	struct ImportType
	{
		ObjectKind kind;
		union
		{
			uintp functionTypeIndex;
			TableType table;
			MemoryType memory;
			GlobalType global;
		};
		ImportType()						: kind(ObjectKind::invalid) {}
		ImportType(uintp inFunctionTypeIndex)	: kind(ObjectKind::function), functionTypeIndex(inFunctionTypeIndex) {}
		ImportType(TableType inTable)		: kind(ObjectKind::table), table(inTable) {}
		ImportType(MemoryType inMemory)	: kind(ObjectKind::memory), memory(inMemory) {}
		ImportType(GlobalType inGlobal)		: kind(ObjectKind::global), global(inGlobal) {}
	};

	// Describes an object imported into a module.
	struct Import
	{
		std::string module;
		std::string exportName;
		ImportType type;
	};

	// Describes an export from a module. The interpretation of index depends on kind
	struct Export
	{
		std::string name;
		ObjectKind kind;
		uintp index;
	};
	
	// An initializer expression: serialized like any other code, but may only be a constant or immutable global
	struct InitializerExpression
	{
		enum class Type : uint8
		{
			i32_const = 0x10,
			i64_const = 0x11,
			f32_const = 0x13,
			f64_const = 0x12,
			get_global = 0xbb,
			error = 0xff
		};
		Type type;
		union
		{
			int32 i32;
			int64 i64;
			float32 f32;
			float64 f64;
			uintp globalIndex;
		};
		InitializerExpression(): type(Type::error) {}
		InitializerExpression(int32 inI32): type(Type::i32_const), i32(inI32) {}
		InitializerExpression(int64 inI64): type(Type::i64_const), i64(inI64) {}
		InitializerExpression(float32 inF32): type(Type::f32_const), f32(inF32) {}
		InitializerExpression(float64 inF64): type(Type::f64_const), f64(inF64) {}
		InitializerExpression(Type inType,uintp inGlobalIndex): type(inType), globalIndex(inGlobalIndex) { assert(inType == Type::get_global); }
	};

	// A global definition
	struct Global
	{
		GlobalType type;
		InitializerExpression initializer;
	};

	// A data segment: a literal sequence of bytes that is copied into a Runtime::Memory when instantiating a module
	struct DataSegment
	{
		uintp memoryIndex;
		InitializerExpression baseOffset;
		std::vector<uint8> data;

		DataSegment() {}
		DataSegment(uintp inMemoryIndex,InitializerExpression inBaseOffset,const std::vector<uint8>& inData)
		: memoryIndex(inMemoryIndex), baseOffset(inBaseOffset), data(inData) {}
	};

	// A table segment: a literal sequence of function indices that is copied into a Runtime::Table when instantiating a module
	struct TableSegment
	{
		uintp tableIndex;
		InitializerExpression baseOffset;
		std::vector<uintp> indices;
		
		TableSegment() {}
		TableSegment(uintp inTableIndex,InitializerExpression inBaseOffset,const std::vector<uintp>& inIndices)
		: tableIndex(inTableIndex), baseOffset(inBaseOffset), indices(inIndices) {}
	};

	// A user-defined module section as an array of bytes
	struct UserSection
	{
		std::string name;
		std::vector<uint8> data;
	};

	// A WebAssembly module definition
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

		uintp startFunctionIndex;

		Module() : startFunctionIndex(UINTPTR_MAX) {}
	};

	// Converts an ImportType, which is only meaningful in the context of a module, to an ObjectType.
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
	
	// Finds a named user section in a module.
	inline bool findUserSection(const Module& module,const char* userSectionName,uintp& outUserSectionIndex)
	{
		for(uintp sectionIndex = 0;sectionIndex < module.userSections.size();++sectionIndex)
		{
			if(module.userSections[sectionIndex].name == userSectionName)
			{
				outUserSectionIndex = sectionIndex;
				return true;
			}
		}
		return false;
	}

	// Maps declarations in a module to names to use in disassembly.
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
