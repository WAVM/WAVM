#pragma once

#include <stdint.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/IR/Types.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"

namespace WAVM { namespace IR {
	enum class Opcode : U16;

	// An initializer expression: serialized like any other code, but only supports a few specific
	// instructions.
	template<typename Ref> struct InitializerExpressionBase
	{
		enum class Type : U16
		{
			i32_const = 0x0041,
			i64_const = 0x0042,
			f32_const = 0x0043,
			f64_const = 0x0044,
			v128_const = 0xfd02,
			global_get = 0x0023,
			ref_null = 0x00d0,
			ref_func = 0x00d2,
			error = 0xffff
		};
		union
		{
			Type type;
			Opcode typeOpcode;
		};
		union
		{
			I32 i32;
			I64 i64;
			F32 f32;
			F64 f64;
			V128 v128;
			Ref ref;
		};
		InitializerExpressionBase() : type(Type::error) {}
		InitializerExpressionBase(I32 inI32) : type(Type::i32_const), i32(inI32) {}
		InitializerExpressionBase(I64 inI64) : type(Type::i64_const), i64(inI64) {}
		InitializerExpressionBase(F32 inF32) : type(Type::f32_const), f32(inF32) {}
		InitializerExpressionBase(F64 inF64) : type(Type::f64_const), f64(inF64) {}
		InitializerExpressionBase(V128 inV128) : type(Type::v128_const), v128(inV128) {}
		InitializerExpressionBase(Type inType, Ref inRef) : type(inType), ref(inRef)
		{
			wavmAssert(type == Type::global_get || type == Type::ref_func);
		}
		InitializerExpressionBase(std::nullptr_t) : type(Type::ref_null) {}

		friend bool operator==(const InitializerExpressionBase& a,
							   const InitializerExpressionBase& b)
		{
			if(a.type != b.type) { return false; }
			switch(a.type)
			{
			case Type::i32_const: return a.i32 == b.i32;
			case Type::i64_const: return a.i64 == b.i64;
			// For FP constants, use integer comparison to test for bitwise equality. Using FP
			// comparison can't distinguish when NaNs are identical.
			case Type::f32_const: return a.i32 == b.i32;
			case Type::f64_const: return a.i64 == b.i64;
			case Type::v128_const:
				return a.v128.u64[0] == b.v128.u64[0] && a.v128.u64[1] == b.v128.u64[1];
			case Type::global_get: return a.ref == b.ref;
			case Type::ref_null: return true;
			case Type::ref_func: return a.ref == b.ref;
			case Type::error: return true;
			default: Errors::unreachable();
			};
		}

		friend bool operator!=(const InitializerExpressionBase& a,
							   const InitializerExpressionBase& b)
		{
			return !(a == b);
		}
	};

	typedef InitializerExpressionBase<Uptr> InitializerExpression;

	// A function definition
	struct FunctionDef
	{
		IndexedFunctionType type;
		std::vector<ValueType> nonParameterLocalTypes;
		std::vector<U8> code;
		std::vector<std::vector<Uptr>> branchTables;
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

	// A tagged tuple type definition
	struct ExceptionTypeDef
	{
		ExceptionType type;
	};

	// Describes an object imported into a module or a specific type
	template<typename Type> struct Import
	{
		Type type;
		std::string moduleName;
		std::string exportName;
	};

	typedef Import<IndexedFunctionType> FunctionImport;
	typedef Import<TableType> TableImport;
	typedef Import<MemoryType> MemoryImport;
	typedef Import<GlobalType> GlobalImport;
	typedef Import<ExceptionType> ExceptionTypeImport;

	// Describes an export from a module. The interpretation of index depends on kind
	struct Export
	{
		std::string name;
		ExternKind kind;
		Uptr index;
	};

	// A data segment: a literal sequence of bytes that is copied into a Runtime::Memory when
	// instantiating a module
	struct DataSegment
	{
		bool isActive;
		Uptr memoryIndex;
		InitializerExpression baseOffset;
		std::vector<U8> data;
	};

	// An elem: a literal reference used to initialize a table element.
	struct Elem
	{
		enum class Type
		{
			// These must match the corresponding Opcode members.
			ref_null = 0xd0,
			ref_func = 0xd2
		};
		union
		{
			Type type;
			Opcode typeOpcode;
		};
		Uptr index;

		friend bool operator==(const Elem& a, const Elem& b)
		{
			if(a.type != b.type) { return false; }
			switch(a.type)
			{
			case Elem::Type::ref_func: return a.index == b.index;
			default: return true;
			}
		}

		friend bool operator!=(const Elem& a, const Elem& b)
		{
			if(a.type != b.type) { return true; }
			switch(a.type)
			{
			case Elem::Type::ref_func: return a.index != b.index;
			default: return false;
			}
		}
	};

	// An elem segment: a literal sequence of elems that is copied into a Runtime::Table when
	// instantiating a module
	struct ElemSegment
	{
		bool isActive;
		Uptr tableIndex;
		InitializerExpression baseOffset;
		ReferenceType elemType;
		std::vector<Elem> elems;
	};

	// A user-defined module section as an array of bytes
	struct UserSection
	{
		std::string name;
		std::vector<U8> data;
	};

	// An index-space for imports and definitions of a specific kind.
	template<typename Definition, typename Type> struct IndexSpace
	{
		std::vector<Import<Type>> imports;
		std::vector<Definition> defs;

		Uptr size() const { return imports.size() + defs.size(); }
		Type getType(Uptr index) const
		{
			if(index < imports.size()) { return imports[index].type; }
			else
			{
				return defs[index - imports.size()].type;
			}
		}
		bool isImport(Uptr index) const
		{
			wavmAssert(index < size());
			return index < imports.size();
		}
		bool isDef(Uptr index) const
		{
			wavmAssert(index < size());
			return index >= imports.size();
		}
		const Definition& getDef(Uptr index) const
		{
			wavmAssert(isDef(index));
			return defs[index - imports.size()];
		}
	};

	// A WebAssembly module definition
	struct Module
	{
		FeatureSpec featureSpec;

		std::vector<FunctionType> types;

		IndexSpace<FunctionDef, IndexedFunctionType> functions;
		IndexSpace<TableDef, TableType> tables;
		IndexSpace<MemoryDef, MemoryType> memories;
		IndexSpace<GlobalDef, GlobalType> globals;
		IndexSpace<ExceptionTypeDef, ExceptionType> exceptionTypes;

		std::vector<Export> exports;
		std::vector<DataSegment> dataSegments;
		std::vector<ElemSegment> elemSegments;
		std::vector<UserSection> userSections;

		Uptr startFunctionIndex;

		Module() : startFunctionIndex(UINTPTR_MAX) {}

		Module(const FeatureSpec& inFeatureSpec)
		: featureSpec(inFeatureSpec), startFunctionIndex(UINTPTR_MAX)
		{
		}
	};

	// Finds a named user section in a module.
	inline bool findUserSection(const Module& module,
								const char* userSectionName,
								Uptr& outUserSectionIndex)
	{
		for(Uptr sectionIndex = 0; sectionIndex < module.userSections.size(); ++sectionIndex)
		{
			if(module.userSections[sectionIndex].name == userSectionName)
			{
				outUserSectionIndex = sectionIndex;
				return true;
			}
		}
		return false;
	}

	// Resolve an indexed block type to a FunctionType.
	inline FunctionType resolveBlockType(const Module& module, const IndexedBlockType& indexedType)
	{
		switch(indexedType.format)
		{
		case IndexedBlockType::noParametersOrResult: return FunctionType();
		case IndexedBlockType::oneResult: return FunctionType(TypeTuple(indexedType.resultType));
		case IndexedBlockType::functionType: return module.types[indexedType.index];
		default: Errors::unreachable();
		};
	}

	// Maps declarations in a module to names to use in disassembly.
	struct DisassemblyNames
	{
		struct Function
		{
			std::string name;
			std::vector<std::string> locals;
			std::vector<std::string> labels;
		};

		std::string moduleName;
		std::vector<std::string> types;
		std::vector<Function> functions;
		std::vector<std::string> tables;
		std::vector<std::string> memories;
		std::vector<std::string> globals;
		std::vector<std::string> elemSegments;
		std::vector<std::string> dataSegments;
		std::vector<std::string> exceptionTypes;
	};

	// Looks for a name section in a module. If it exists, deserialize it into outNames.
	// If it doesn't exist, fill outNames with sensible defaults.
	IR_API void getDisassemblyNames(const Module& module, DisassemblyNames& outNames);

	// Serializes a DisassemblyNames structure and adds it to the module as a name section.
	IR_API void setDisassemblyNames(Module& module, const DisassemblyNames& names);
}}
