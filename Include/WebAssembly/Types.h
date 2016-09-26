#pragma once

#include "Core/Core.h"
#include "WebAssembly.h"

namespace WebAssembly
{
	// The type of a WebAssembly operand
	enum class ValueType : uint8
	{
		invalid = 0,
		i32 = 1,
		i64 = 2,
		f32 = 3,
		f64 = 4,
		
		num,
		max = num-1
	};

	template<ValueType type> struct ValueTypeInfo;
	template<> struct ValueTypeInfo<ValueType::i32> { typedef int32 Value; };
	template<> struct ValueTypeInfo<ValueType::i64> { typedef int64 Value; };
	template<> struct ValueTypeInfo<ValueType::f32> { typedef float32 Value; };
	template<> struct ValueTypeInfo<ValueType::f64> { typedef float64 Value; };
	
	inline uint8 getTypeBitWidth(ValueType type)
	{
		switch(type)
		{
		case ValueType::i32: return 32;
		case ValueType::i64: return 64;
		case ValueType::f32: return 32;
		case ValueType::f64: return 64;
		default: Core::unreachable();
		};
	}
	
	inline const char* asString(ValueType type)
	{
		switch(type)
		{
		case ValueType::invalid: return "invalid";
		case ValueType::i32: return "i32";
		case ValueType::i64: return "i64";
		case ValueType::f32: return "f32";
		case ValueType::f64: return "f64";
		default: Core::unreachable();
		};
	}

	// The type of a WebAssembly operator result. Mostly the same as ValueType, but allows operators with no result (none).
	enum class ResultType : uint8
	{
		none = 0,
		i32 = (uint8)ValueType::i32,
		i64 = (uint8)ValueType::i64,
		f32 = (uint8)ValueType::f32,
		f64 = (uint8)ValueType::f64,
		num,
		max = num-1,
	};
	
	inline size_t getArity(ResultType returnType) { return returnType == ResultType::none ? 0 : 1; }
	
	inline const char* asString(ResultType type)
	{
		switch(type)
		{
		case ResultType::i32: return "i32";
		case ResultType::i64: return "i64";
		case ResultType::f32: return "f32";
		case ResultType::f64: return "f64";
		case ResultType::none: return "()";
		default: Core::unreachable();
		};
	}

	// Conversion between ValueType and ResultType.
	inline ValueType asValueType(ResultType type)
	{
		assert(type != ResultType::none);
		return (ValueType)type;
	}

	inline ResultType asResultType(ValueType type)
	{
		assert(type != ValueType::invalid);
		return (ResultType)type;
	}

	// The type of a WebAssembly function
	struct FunctionType
	{
		ResultType ret;
		std::vector<ValueType> parameters;

		WEBASSEMBLY_API static const FunctionType* get(ResultType ret,const std::initializer_list<ValueType>& parameters);
		WEBASSEMBLY_API static const FunctionType* get(ResultType ret,const std::vector<ValueType>& parameters);
		WEBASSEMBLY_API static const FunctionType* get(ResultType ret = ResultType::none);

	private:

		FunctionType(ResultType inRet,const std::vector<ValueType>& inParameters)
		: ret(inRet), parameters(inParameters) {}
	};
	
	inline std::string asString(const std::vector<ValueType>& typeTuple)
	{
		std::string result = "(";
		for(uintp parameterIndex = 0;parameterIndex < typeTuple.size();++parameterIndex)
		{
			if(parameterIndex != 0) { result += ','; }
			result += asString(typeTuple[parameterIndex]);
		}
		result += ")";
		return result;
	}

	inline std::string asString(const FunctionType* functionType)
	{
		return asString(functionType->parameters) + "->" + asString(functionType->ret);
	}

	// A size constraint: a range of expected sizes for some size-constrained type.
	// If max==UINT64_MAX, the maximum size is unbounded.
	struct SizeConstraints
	{
		uint64 min;
		uint64 max;

		friend bool operator==(const SizeConstraints& left,const SizeConstraints& right) { return left.min == right.min && left.max == right.max; }
		friend bool operator!=(const SizeConstraints& left,const SizeConstraints& right) { return left.min != right.min || left.max != right.max; }
		friend bool isSubset(const SizeConstraints& super,const SizeConstraints& sub)
		{
			return sub.min >= super.min && sub.max <= super.max;
		}
	};
	
	// The type of element a table contains: for now can only be anyfunc, meaning any function type.
	enum class TableElementType : uint8
	{
		anyfunc = 0x20
	};

	// The type of a table
	struct TableType
	{
		TableElementType elementType;
		SizeConstraints size;

		TableType(): elementType(TableElementType::anyfunc), size({0,UINT64_MAX}) {}
		TableType(TableElementType inElementType,SizeConstraints inSize): elementType(inElementType), size(inSize) {}

		friend bool operator==(const TableType& left,const TableType& right) { return left.elementType == right.elementType && left.size == right.size; }
		friend bool operator!=(const TableType& left,const TableType& right) { return left.elementType != right.elementType || left.size != right.size; }
	};

	// The type of a memory
	struct MemoryType
	{
		SizeConstraints size;
		
		MemoryType(): size({0,UINT64_MAX}) {}
		MemoryType(const SizeConstraints& inSize): size(inSize) {}

		friend bool operator==(const MemoryType& left,const MemoryType& right) { return left.size == right.size; }
		friend bool operator!=(const MemoryType& left,const MemoryType& right) { return left.size != right.size; }
	};

	// The type of a global
	struct GlobalType
	{
		ValueType valueType;
		bool isMutable;
		
		friend bool operator==(const GlobalType& left,const GlobalType& right) { return left.valueType == right.valueType && left.isMutable == right.isMutable; }
		friend bool operator!=(const GlobalType& left,const GlobalType& right) { return left.valueType != right.valueType || left.isMutable != right.isMutable; }
	};
	
	inline std::string asString(const GlobalType& globalType)
	{
		if(globalType.isMutable) { return std::string("global") + asString(globalType.valueType); }
		else { return std::string("immutable") + asString(globalType.valueType); }
	}

	// The type of an object
	enum class ObjectKind : uint8
	{
		function = 0,
		table = 1,
		memory = 2,
		global = 3,
		module = 4,
		max = 4,
		invalid = 0xff
	};
	struct ObjectType
	{
		ObjectKind kind;
		union
		{
			const FunctionType* function;
			TableType table;
			MemoryType memory;
			GlobalType global;
		};
		ObjectType()							: kind(ObjectKind::invalid) {}
		ObjectType(const FunctionType* inFunction)	: kind(ObjectKind::function), function(inFunction) {}
		ObjectType(TableType inTable)			: kind(ObjectKind::table), table(inTable) {}
		ObjectType(MemoryType inMemory)		: kind(ObjectKind::memory), memory(inMemory) {}
		ObjectType(GlobalType inGlobal)			: kind(ObjectKind::global), global(inGlobal) {}
	};

	inline std::string asString(const ObjectType& objectType)
	{
		switch(objectType.kind)
		{
		case ObjectKind::function: return "func " + asString(objectType.function);
		case ObjectKind::table: return "table";
		case ObjectKind::memory: return "memory";
		case ObjectKind::global: return asString(objectType.global);
		default: Core::unreachable();
		};
	}
}