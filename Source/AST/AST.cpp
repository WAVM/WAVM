#include "AST.h"

namespace AST
{
	// Define the TypeClass::id.
	#define AST_TYPECLASS(className) \
		TypeClassId className##Class::id = TypeClassId::className;
	ENUM_AST_TYPECLASSES()
	#undef AST_TYPECLASS

	// Define Type::id, Type::classId, and Type::name.
	#define I8LowerCaseString "i8"
	#define I16LowerCaseString "i16"
	#define I32LowerCaseString "i32"
	#define I64LowerCaseString "i64"
	#define F32LowerCaseString "f32"
	#define F64LowerCaseString "f64"
	#define BoolLowerCaseString "bool"
	#define VoidLowerCaseString "void"
	#define AST_TYPE(typeName,className,...) \
		TypeId typeName##Type::id = TypeId::typeName; \
		TypeClassId typeName##Type::classId = TypeClassId::className; \
		const char* typeName##Type::name = typeName##LowerCaseString;
	ENUM_AST_TYPES(AST_TYPE)
	#undef AST_TYPE

	bool isTypeClass(TypeId type,TypeClassId typeClass)
	{
		if(typeClass == TypeClassId::Any) { return true; }
		else
		{
			switch(type)
			{
			case TypeId::None: return true;
			case TypeId::I8: return typeClass == TypeClassId::Int;
			case TypeId::I16: return typeClass == TypeClassId::Int;
			case TypeId::I32: return typeClass == TypeClassId::Int;
			case TypeId::I64: return typeClass == TypeClassId::Int;
			case TypeId::F32: return typeClass == TypeClassId::Float;
			case TypeId::F64: return typeClass == TypeClassId::Float;
			case TypeId::Bool: return typeClass == TypeClassId::Bool;
			case TypeId::Void: return typeClass == TypeClassId::Void;
			default: throw;
			}
		}
	}

	TypeClassId getPrimaryTypeClass(TypeId type)
	{
		switch(type)
		{
		case TypeId::I8: return TypeClassId::Int;
		case TypeId::I16: return TypeClassId::Int;
		case TypeId::I32: return TypeClassId::Int;
		case TypeId::I64: return TypeClassId::Int;
		case TypeId::F32: return TypeClassId::Float;
		case TypeId::F64: return TypeClassId::Float;
		case TypeId::Bool: return TypeClassId::Bool;
		case TypeId::Void: return TypeClassId::Void;
		default: throw;
		}
	}

	template<typename Type> struct NameVisitor { static const char* visit() { return Type::name; } };
	const char* getTypeName(TypeId type) { return dispatchByType<const char*,NameVisitor>(type,"invalid"); }

	size_t getTypeBitWidth(TypeId type)
	{
		switch(type)
		{
		case TypeId::I8: return 8;
		case TypeId::I16: return 16;
		case TypeId::I32: return 32;
		case TypeId::I64: return 64;
		case TypeId::F32: return 32;
		case TypeId::F64: return 64;
		case TypeId::Bool: return 1;
		case TypeId::Void: return 0;
		default: throw;
		}
	}

	size_t getTypeByteWidth(TypeId type)
	{
		switch(type)
		{
		case TypeId::I8: return 1;
		case TypeId::I16: return 2;
		case TypeId::I32: return 4;
		case TypeId::I64: return 8;
		case TypeId::F32: return 4;
		case TypeId::F64: return 8;
		case TypeId::Bool: return 1;
		case TypeId::Void: return 0;
		default: throw;
		};
	}
	
	uint8 getTypeByteWidthLog2(TypeId type)
	{
		switch(type)
		{
		case TypeId::I8: return 0;
		case TypeId::I16: return 1;
		case TypeId::I32: return 2;
		case TypeId::I64: return 3;
		case TypeId::F32: return 2;
		case TypeId::F64: return 3;
		case TypeId::Bool: return 0;
		case TypeId::Void: return 0;
		default: throw;
		}
	}

	#define AST_OP(op) #op,
	#define AST_TYPECLASS(className) \
		static const char* nameStrings##className##Ops[] = { ENUM_AST_OPS_##className() }; \
		const char* getOpName(className##Op op) \
		{ \
			assert((uintptr)op < (sizeof(nameStrings##className##Ops) / sizeof(const char*))); \
			return nameStrings##className##Ops[(uintptr)op]; \
		}
	ENUM_AST_TYPECLASSES()
	#undef AST_OP
	#undef AST_TYPECLASS
}
