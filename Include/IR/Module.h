#pragma once

#include "Inline/BasicTypes.h"
#include "IR.h"
#include "Types.h"

#include <vector>

namespace IR
{
	// An initializer expression: serialized like any other code, but may only be a constant or immutable global
	struct InitializerExpression
	{
		enum class Type : uint8
		{
			i32_const = 0x41,
			i64_const = 0x42,
			f32_const = 0x43,
			f64_const = 0x44,
			get_global = 0x23,
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

	// A function definition
	struct FunctionDef
	{
		IndexedFunctionType type;
		std::vector<ValueType> nonParameterLocalTypes;
		std::vector<uint8> code;
		std::vector<std::vector<uint32>> branchTables;
	};

	// A table definition
	struct TableDef
	{
		TableType type;
	};
	
	// A memory definition
	struct MemoryDef
	{
		MemoryType type;
	};

	// A global definition
	struct GlobalDef
	{
		GlobalType type;
		InitializerExpression initializer;
	};

	// Describes an object imported into a module or a specific type
	template<typename Type>
	struct Import
	{
		Type type;
		std::string moduleName;
		std::string exportName;
	};

	typedef Import<uintp> FunctionImport;
	typedef Import<TableType> TableImport;
	typedef Import<MemoryType> MemoryImport;
	typedef Import<GlobalType> GlobalImport;

	// Describes an export from a module. The interpretation of index depends on kind
	struct Export
	{
		std::string name;
		ObjectKind kind;
		uintp index;
	};
	
	// A data segment: a literal sequence of bytes that is copied into a Runtime::Memory when instantiating a module
	struct DataSegment
	{
		uintp memoryIndex;
		InitializerExpression baseOffset;
		std::vector<uint8> data;
	};

	// A table segment: a literal sequence of function indices that is copied into a Runtime::Table when instantiating a module
	struct TableSegment
	{
		uintp tableIndex;
		InitializerExpression baseOffset;
		std::vector<uintp> indices;
	};

	// A user-defined module section as an array of bytes
	struct UserSection
	{
		std::string name;
		std::vector<uint8> data;
	};

	// An index-space for imports and definitions of a specific kind.
	template<typename Definition,typename Type>
	struct IndexSpace
	{
		std::vector<Import<Type>> imports;
		std::vector<Definition> defs;

		size_t size() const { return imports.size() + defs.size(); }
		Type getType(uintp index) const
		{
			if(index < imports.size()) { return imports[index].type; }
			else { return defs[index - imports.size()].type; }
		}
	};

	// A WebAssembly module definition
	struct Module
	{
		std::vector<const FunctionType*> types;

		IndexSpace<FunctionDef,IndexedFunctionType> functions;
		IndexSpace<TableDef,TableType> tables;
		IndexSpace<MemoryDef,MemoryType> memories;
		IndexSpace<GlobalDef,GlobalType> globals;

		std::vector<Export> exports;
		std::vector<DataSegment> dataSegments;
		std::vector<TableSegment> tableSegments;
		std::vector<UserSection> userSections;

		uintp startFunctionIndex;

		Module() : startFunctionIndex(UINTPTR_MAX) {}
	};
	
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
		struct Function
		{
			std::string name;
			std::vector<std::string> locals;
		};

		std::vector<std::string> types;
		std::vector<Function> functions;
		std::vector<std::string> tables;
		std::vector<std::string> memories;
		std::vector<std::string> globals;
	};
	
	// Looks for a name section in a module. If it exists, deserialize it into outNames.
	// If it doesn't exist, fill outNames with sensible defaults.
	IR_API void getDisassemblyNames(const Module& module,DisassemblyNames& outNames);

	// Serializes a DisassemblyNames structure and adds it to the module as a name section.
	IR_API void setDisassemblyNames(Module& module,const DisassemblyNames& names);
}
