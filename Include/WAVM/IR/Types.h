#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <initializer_list>
#include <string>
#include <vector>

#include "WAVM/IR/IR.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"

namespace WAVM { namespace Runtime {
	struct Object;
	struct Function;
}}

namespace WAVM { namespace IR {
	// The type of a WebAssembly operand
	enum class ValueType : U8
	{
		none = 0,
		any = 1,
		i32 = 2,
		i64 = 3,
		f32 = 4,
		f64 = 5,
		v128 = 6,
		anyref = 7,
		funcref = 8,
		nullref = 9,

		num,
		max = num - 1
	};

	// The reference types subset of ValueType.
	enum class ReferenceType : U8
	{
		invalid = 0,

		anyref = 7,
		funcref = 8,
	};

	static_assert(Uptr(ValueType::anyref) == Uptr(ReferenceType::anyref),
				  "ReferenceType and ValueType must match");
	static_assert(Uptr(ValueType::funcref) == Uptr(ReferenceType::funcref),
				  "ReferenceType and ValueType must match");

	inline ValueType asValueType(ReferenceType type) { return ValueType(type); }

	inline bool isReferenceType(ValueType type)
	{
		return type == ValueType::anyref || type == ValueType::funcref
			   || type == ValueType::nullref;
	}

	inline bool isSubtype(ValueType subtype, ValueType supertype)
	{
		if(subtype == supertype) { return true; }
		else
		{
			switch(supertype)
			{
			case ValueType::any: return true;
			case ValueType::anyref:
				return subtype == ValueType::funcref || subtype == ValueType::nullref;
			case ValueType::funcref: return subtype == ValueType::nullref;
			default: return false;
			}
		}
	}

	// Returns the type that includes all values that are an instance of a OR b.
	inline ValueType join(ValueType a, ValueType b)
	{
		if(a == b) { return a; }
		else if(isReferenceType(a) && isReferenceType(b))
		{
			// a \ b    anyref  funcref  nullref
			// anyref   anyref  anyref   anyref
			// funcref  anyref  funcref  funcref
			// nullref  anyref  funcref  nullref
			if(a == ValueType::nullref) { return b; }
			else if(b == ValueType::nullref)
			{
				return a;
			}
			else
			{
				// Because we know a != b, and neither a or b are nullref, we can infer that one is
				// anyref, and one is funcref.
				return ValueType::anyref;
			}
		}
		else
		{
			return ValueType::any;
		}
	}

	// Returns the type that includes all values that are an instance of both a AND b.
	inline ValueType meet(ValueType a, ValueType b)
	{
		if(a == b) { return a; }
		else if(isReferenceType(a) && isReferenceType(b))
		{
			// a \ b    anyref   funcref  nullref
			// anyref   anyref   funcref  nullref
			// funcref  funcref  funcref  nullref
			// nullref  nullref  nullref  nullref
			if(a == ValueType::nullref || b == ValueType::nullref) { return ValueType::nullref; }
			else if(a == ValueType::anyref)
			{
				return b;
			}
			else
			{
				wavmAssert(b == ValueType::anyref);
				return a;
			}
		}
		else
		{
			return ValueType::none;
		}
	}

	inline std::string asString(I32 value) { return std::to_string(value); }
	inline std::string asString(I64 value) { return std::to_string(value); }
	IR_API std::string asString(F32 value);
	IR_API std::string asString(F64 value);

	inline std::string asString(const V128& v128)
	{
		// buffer needs 50 characters:
		// i32x4 0xHHHHHHHH 0xHHHHHHHH 0xHHHHHHHH 0xHHHHHHHH\0
		char buffer[50];
		snprintf(buffer,
				 sizeof(buffer),
				 "i32x4 0x%.8x 0x%.8x 0x%.8x 0x%.8x",
				 v128.u32[0],
				 v128.u32[1],
				 v128.u32[2],
				 v128.u32[3]);
		return std::string(buffer);
	}

	inline U8 getTypeByteWidth(ValueType type)
	{
		switch(type)
		{
		case ValueType::i32: return 4;
		case ValueType::i64: return 8;
		case ValueType::f32: return 4;
		case ValueType::f64: return 8;
		case ValueType::v128: return 16;
		case ValueType::anyref:
		case ValueType::funcref:
		case ValueType::nullref: return 8;
		default: Errors::unreachable();
		};
	}

	inline U8 getTypeBitWidth(ValueType type) { return getTypeByteWidth(type) * 8; }

	inline const char* asString(ValueType type)
	{
		switch(type)
		{
		case ValueType::any: return "any";
		case ValueType::i32: return "i32";
		case ValueType::i64: return "i64";
		case ValueType::f32: return "f32";
		case ValueType::f64: return "f64";
		case ValueType::v128: return "v128";
		case ValueType::anyref: return "anyref";
		case ValueType::funcref: return "funcref";
		case ValueType::nullref: return "nullref";
		default: Errors::unreachable();
		};
	}

	// The tuple of value types.
	struct TypeTuple
	{
		TypeTuple() : impl(getUniqueImpl(0, nullptr)) {}
		IR_API TypeTuple(ValueType inElem);
		IR_API TypeTuple(const std::initializer_list<ValueType>& inElems);
		IR_API TypeTuple(const std::vector<ValueType>& inElems);
		IR_API TypeTuple(const ValueType* inElems, Uptr numElems);

		const ValueType* begin() const { return impl->elems; }
		const ValueType* end() const { return impl->elems + impl->numElems; }
		const ValueType* data() const { return impl->elems; }

		ValueType operator[](Uptr index) const
		{
			wavmAssert(index < impl->numElems);
			return impl->elems[index];
		}

		Uptr getHash() const { return impl->hash; }
		Uptr size() const { return impl->numElems; }

		friend bool operator==(const TypeTuple& left, const TypeTuple& right)
		{
			return left.impl == right.impl;
		}
		friend bool operator!=(const TypeTuple& left, const TypeTuple& right)
		{
			return left.impl != right.impl;
		}

	private:
		struct Impl
		{
			Uptr hash;
			Uptr numElems;
			ValueType elems[1];

			Impl(Uptr inNumElems, const ValueType* inElems);
			Impl(const Impl& inCopy);

			static Uptr calcNumBytes(Uptr numElems)
			{
				return offsetof(Impl, elems) + numElems * sizeof(ValueType);
			}
		};

		const Impl* impl;

		TypeTuple(const Impl* inImpl) : impl(inImpl) {}

		IR_API static const Impl* getUniqueImpl(Uptr numElems, const ValueType* inElems);
	};

	inline std::string asString(TypeTuple typeTuple)
	{
		if(typeTuple.size() == 1) { return asString(typeTuple[0]); }
		else
		{
			std::string result = "(";
			for(Uptr elementIndex = 0; elementIndex < typeTuple.size(); ++elementIndex)
			{
				if(elementIndex != 0) { result += ", "; }
				result += asString(typeTuple[elementIndex]);
			}
			result += ")";
			return result;
		}
	}

	inline bool isSubtype(TypeTuple subtype, TypeTuple supertype)
	{
		if(subtype.size() != supertype.size()) { return false; }
		else
		{
			for(Uptr elementIndex = 0; elementIndex < subtype.size(); ++elementIndex)
			{
				if(!isSubtype(subtype[elementIndex], supertype[elementIndex])) { return false; }
			}
			return true;
		}
	}

	// Infer value and result types from a C type.

	template<typename> constexpr ValueType inferValueType();
	template<> constexpr ValueType inferValueType<I8>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<U8>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<I16>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<U16>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<I32>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<U32>() { return ValueType::i32; }
	template<> constexpr ValueType inferValueType<I64>() { return ValueType::i64; }
	template<> constexpr ValueType inferValueType<U64>() { return ValueType::i64; }
	template<> constexpr ValueType inferValueType<F32>() { return ValueType::f32; }
	template<> constexpr ValueType inferValueType<F64>() { return ValueType::f64; }
	template<> constexpr ValueType inferValueType<Runtime::Object*>() { return ValueType::anyref; }
	template<> constexpr ValueType inferValueType<Runtime::Function*>()
	{
		return ValueType::funcref;
	}
	template<> constexpr ValueType inferValueType<const Runtime::Object*>()
	{
		return ValueType::anyref;
	}
	template<> constexpr ValueType inferValueType<const Runtime::Function*>()
	{
		return ValueType::funcref;
	}

	template<typename T> inline TypeTuple inferResultType()
	{
		return TypeTuple(inferValueType<T>());
	}
	template<> inline TypeTuple inferResultType<void>() { return TypeTuple(); }

	// The type of a WebAssembly function
	struct FunctionType
	{
		// Used to represent a function type as an abstract pointer-sized value in the runtime.
		struct Encoding
		{
			Uptr impl;
		};

		FunctionType(TypeTuple inResults = TypeTuple(), TypeTuple inParams = TypeTuple())
		: impl(getUniqueImpl(inResults, inParams))
		{
		}

		FunctionType(Encoding encoding) : impl(reinterpret_cast<const Impl*>(encoding.impl)) {}

		TypeTuple results() const { return impl->results; }
		TypeTuple params() const { return impl->params; }
		Uptr getHash() const { return impl->hash; }
		Encoding getEncoding() const { return Encoding{reinterpret_cast<Uptr>(impl)}; }

		friend bool operator==(const FunctionType& left, const FunctionType& right)
		{
			return left.impl == right.impl;
		}

		friend bool operator!=(const FunctionType& left, const FunctionType& right)
		{
			return left.impl != right.impl;
		}

	private:
		struct Impl
		{
			Uptr hash;
			TypeTuple results;
			TypeTuple params;

			Impl(TypeTuple inResults, TypeTuple inParams);
		};

		const Impl* impl;

		FunctionType(const Impl* inImpl) : impl(inImpl) {}

		IR_API static const Impl* getUniqueImpl(TypeTuple results, TypeTuple params);
	};

	struct IndexedFunctionType
	{
		Uptr index;
	};

	struct IndexedBlockType
	{
		enum Format
		{
			noParametersOrResult,
			oneResult,
			functionType
		};
		Format format;
		union
		{
			ValueType resultType;
			Uptr index;
		};
	};

	inline std::string asString(const FunctionType& functionType)
	{
		return asString(functionType.params()) + "->" + asString(functionType.results());
	}

	// A size constraint: a range of expected sizes for some size-constrained type.
	// If max==UINT64_MAX, the maximum size is unbounded.
	struct SizeConstraints
	{
		U64 min;
		U64 max;

		friend bool operator==(const SizeConstraints& left, const SizeConstraints& right)
		{
			return left.min == right.min && left.max == right.max;
		}
		friend bool operator!=(const SizeConstraints& left, const SizeConstraints& right)
		{
			return left.min != right.min || left.max != right.max;
		}
		friend bool isSubset(const SizeConstraints& super, const SizeConstraints& sub)
		{
			return sub.min >= super.min && sub.max <= super.max;
		}
	};

	inline std::string asString(const SizeConstraints& sizeConstraints)
	{
		return std::to_string(sizeConstraints.min)
			   + (sizeConstraints.max == UINT64_MAX ? ".."
													: ".." + std::to_string(sizeConstraints.max));
	}

	// The type of a table
	struct TableType
	{
		ReferenceType elementType;
		bool isShared;
		SizeConstraints size;

		TableType() : elementType(ReferenceType::invalid) {}
		TableType(ReferenceType inElementType, bool inIsShared, SizeConstraints inSize)
		: elementType(inElementType), isShared(inIsShared), size(inSize)
		{
		}

		friend bool operator==(const TableType& left, const TableType& right)
		{
			return left.elementType == right.elementType && left.isShared == right.isShared
				   && left.size == right.size;
		}
		friend bool operator!=(const TableType& left, const TableType& right)
		{
			return left.elementType != right.elementType || left.isShared != right.isShared
				   || left.size != right.size;
		}
		friend bool isSubtype(const TableType& sub, const TableType& super)
		{
			return super.elementType == sub.elementType && super.isShared == sub.isShared
				   && isSubset(super.size, sub.size);
		}
	};

	inline std::string asString(const TableType& tableType)
	{
		return asString(tableType.size) + (tableType.isShared ? " shared funcref" : " funcref");
	}

	// The type of a memory
	struct MemoryType
	{
		bool isShared;
		SizeConstraints size;

		MemoryType() : isShared(false), size({0, UINT64_MAX}) {}
		MemoryType(bool inIsShared, const SizeConstraints& inSize)
		: isShared(inIsShared), size(inSize)
		{
		}

		friend bool operator==(const MemoryType& left, const MemoryType& right)
		{
			return left.isShared == right.isShared && left.size == right.size;
		}
		friend bool operator!=(const MemoryType& left, const MemoryType& right)
		{
			return left.isShared != right.isShared || left.size != right.size;
		}
		friend bool isSubtype(const MemoryType& sub, const MemoryType& super)
		{
			return super.isShared == sub.isShared && isSubset(super.size, sub.size);
		}
	};

	inline std::string asString(const MemoryType& memoryType)
	{
		return asString(memoryType.size) + (memoryType.isShared ? " shared" : "");
	}

	// The type of a global
	struct GlobalType
	{
		ValueType valueType;
		bool isMutable;

		GlobalType() : valueType(ValueType::any), isMutable(false) {}
		GlobalType(ValueType inValueType, bool inIsMutable)
		: valueType(inValueType), isMutable(inIsMutable)
		{
		}

		friend bool operator==(const GlobalType& left, const GlobalType& right)
		{
			return left.valueType == right.valueType && left.isMutable == right.isMutable;
		}
		friend bool operator!=(const GlobalType& left, const GlobalType& right)
		{
			return left.valueType != right.valueType || left.isMutable != right.isMutable;
		}
		friend bool isSubtype(const GlobalType& sub, const GlobalType& super)
		{
			if(super.isMutable != sub.isMutable) { return false; }
			else if(super.isMutable)
			{
				return super.valueType == sub.valueType;
			}
			else
			{
				return isSubtype(sub.valueType, super.valueType);
			}
		}
	};

	inline std::string asString(const GlobalType& globalType)
	{
		if(globalType.isMutable) { return std::string("global ") + asString(globalType.valueType); }
		else
		{
			return std::string("immutable ") + asString(globalType.valueType);
		}
	}

	struct ExceptionType
	{
		TypeTuple params;

		friend bool operator==(const ExceptionType& left, const ExceptionType& right)
		{
			return left.params == right.params;
		}
		friend bool operator!=(const ExceptionType& left, const ExceptionType& right)
		{
			return left.params != right.params;
		}
	};

	inline std::string asString(const ExceptionType& exceptionType)
	{
		return asString(exceptionType.params);
	}

	// The type of an external object: something that can be imported or exported from a module.
	enum class ExternKind : U8
	{
		// Standard object kinds that may be imported/exported from WebAssembly modules.
		function = 0,
		table = 1,
		memory = 2,
		global = 3,
		exceptionType = 4,
		max = 4,
		invalid = 0xff,
	};
	struct ExternType
	{
		const ExternKind kind;

		ExternType() : kind(ExternKind::invalid) {}
		ExternType(FunctionType inFunction) : kind(ExternKind::function), function(inFunction) {}
		ExternType(TableType inTable) : kind(ExternKind::table), table(inTable) {}
		ExternType(MemoryType inMemory) : kind(ExternKind::memory), memory(inMemory) {}
		ExternType(GlobalType inGlobal) : kind(ExternKind::global), global(inGlobal) {}
		ExternType(ExceptionType inExceptionType)
		: kind(ExternKind::exceptionType), exceptionType(inExceptionType)
		{
		}
		ExternType(ExternKind inKind) : kind(inKind) {}

		friend FunctionType asFunctionType(const ExternType& objectType)
		{
			wavmAssert(objectType.kind == ExternKind::function);
			return objectType.function;
		}
		friend TableType asTableType(const ExternType& objectType)
		{
			wavmAssert(objectType.kind == ExternKind::table);
			return objectType.table;
		}
		friend MemoryType asMemoryType(const ExternType& objectType)
		{
			wavmAssert(objectType.kind == ExternKind::memory);
			return objectType.memory;
		}
		friend GlobalType asGlobalType(const ExternType& objectType)
		{
			wavmAssert(objectType.kind == ExternKind::global);
			return objectType.global;
		}
		friend ExceptionType asExceptionType(const ExternType& objectType)
		{
			wavmAssert(objectType.kind == ExternKind::exceptionType);
			return objectType.exceptionType;
		}

	private:
		union
		{
			FunctionType function;
			TableType table;
			MemoryType memory;
			GlobalType global;
			ExceptionType exceptionType;
		};
	};

	inline std::string asString(const ExternType& objectType)
	{
		switch(objectType.kind)
		{
		case ExternKind::function: return "func " + asString(asFunctionType(objectType));
		case ExternKind::table: return "table " + asString(asTableType(objectType));
		case ExternKind::memory: return "memory " + asString(asMemoryType(objectType));
		case ExternKind::global: return asString(asGlobalType(objectType));
		case ExternKind::exceptionType:
			return "exception_type " + asString(asExceptionType(objectType));
		default: Errors::unreachable();
		};
	}

	// The calling convention for a function.
	enum class CallingConvention
	{
		wasm,
		intrinsic,
		intrinsicWithContextSwitch,
		c
	};
}}

template<> struct WAVM::Hash<WAVM::IR::TypeTuple>
{
	Uptr operator()(WAVM::IR::TypeTuple typeTuple, Uptr seed = 0) const
	{
		return WAVM::Hash<Uptr>()(typeTuple.getHash(), seed);
	}
};

template<> struct WAVM::Hash<WAVM::IR::FunctionType>
{
	Uptr operator()(WAVM::IR::FunctionType functionType, Uptr seed = 0) const
	{
		return WAVM::Hash<Uptr>()(functionType.getHash(), seed);
	}
};
