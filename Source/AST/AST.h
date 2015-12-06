#pragma once

#include "Core/Core.h"
#include "Core/Platform.h"
#include "Core/MemoryArena.h"

#include <cstdint>
#include <vector>

#ifndef AST_API
	#define AST_API DLL_IMPORT
#endif

#include "ASTTypes.h"
#include "ASTOpcodes.h"

namespace AST
{
	// The superclass of all expression classes.
	struct UntypedExpression
	{
		#ifdef _DEBUG
			#define AST_TYPECLASS(className) \
				UntypedExpression(className##Op inOp) : typeClass(TypeClassId::className), op##className(inOp) {}
		#else
			#define AST_TYPECLASS(className) \
				UntypedExpression(className##Op inOp) : op##className(inOp) {}
		#endif
		ENUM_AST_TYPECLASSES()
		#undef AST_TYPECLASS
		UntypedExpression(AnyOp inOp,TypeClassId inTypeClass): opAny(inOp)
		#ifdef _DEBUG
			, typeClass(inTypeClass)
		#endif
		{}
			
		template<typename Class>
        friend typename Class::ClassExpression* as(const UntypedExpression*);

		AnyOp op() const { return opAny; }

	protected:
		
		#ifdef _DEBUG
			TypeClassId typeClass;
		#endif
		union
		{
			#define AST_TYPECLASS(className) className##Op op##className;
			ENUM_AST_TYPECLASSES()
			#undef AST_TYPECLASS
		};
	};
    
    template<typename Class>
    typename Class::ClassExpression* as(const UntypedExpression* expression)
    {
#ifdef _DEBUG
        assert(isSubclass(Class::id,expression->typeClass));
#endif
        return (Expression<Class>*)expression;
    }

	// Define Expression<...Class> classes which represent expressions that yield a specific type class.
	#define AST_TYPECLASS(className) \
		template<> struct Expression<className##Class> : public UntypedExpression \
		{ \
			typedef className##Class::Op Op; \
			Expression(Op inOp): UntypedExpression(inOp) {} \
			Op op() const { return op##className; } \
		}; \
		typedef Expression<className##Class> className##Expression;
	ENUM_AST_TYPECLASSES_WITHOUT_ANY()
	#undef AST_TYPECLASS

	template<> struct Expression<AnyClass> : public UntypedExpression
	{
		typedef AnyOp Op;
		Expression(AnyOp inOp,TypeClassId inTypeClass): UntypedExpression(inOp,inTypeClass) {}
		Op op() const { return opAny; }
	};
	typedef Expression<AnyClass> AnyExpression;

	// A reference to an expression with an explicit type. Used only when the type isn't defined by the context.
	struct TypedExpression
	{
		UntypedExpression* expression;
		TypeId type;

		TypedExpression(UntypedExpression* inExpression,TypeId inType): expression(inExpression), type(inType) {}
		TypedExpression(): expression(nullptr), type(TypeId::None) {}
		operator bool() const { return expression != nullptr; }
	};
	
	template<typename AsClass> typename AsClass::ClassExpression* as(const TypedExpression& expression)
	{
		assert(expression.expression && isTypeClass(expression.type,AsClass::id));
		return (typename AsClass::ClassExpression*)expression.expression;
	}

	struct Variable
	{
		TypeId type;
		const char* name;
	};

	struct FunctionType
	{
		std::vector<TypeId> parameters;
		TypeId returnType;

		FunctionType(TypeId inReturnType = TypeId::Void,const std::vector<TypeId>& inParameters = {})
		: parameters(inParameters), returnType(inReturnType) {}
		
		friend bool operator==(const FunctionType& left,const FunctionType& right)
		{
			return left.returnType == right.returnType && left.parameters == right.parameters;
		}
		friend bool operator!=(const FunctionType& left,const FunctionType& right)
		{
			return left.returnType != right.returnType || left.parameters != right.parameters;
		}
		friend bool operator<(const FunctionType& left,const FunctionType& right)
		{
			return left.returnType < right.returnType
				|| (left.returnType == right.returnType && left.parameters < right.parameters);
		}
	};

	struct Function
	{
		const char* name;
		std::vector<Variable> locals;
		std::vector<uintptr> parameterLocalIndices;
		FunctionType type;
		UntypedExpression* expression;

		Function(): name(nullptr), expression(nullptr) {}
	};

	struct FunctionTable
	{
		uintptr* functionIndices;
		size_t numFunctions;
		FunctionTable() : functionIndices(nullptr), numFunctions(0) {}
	};

	struct FunctionImport
	{
		FunctionType type;
		const char* module;
		const char* name;
	};

	struct DataSegment
	{
		uint64 baseAddress;
		uint64 numBytes;
		const uint8* data;
	};
	
	struct ErrorRecord
	{
		std::string message;
		ErrorRecord(std::string&& inMessage) : message(std::move(inMessage)) {}
	};

	struct CaseInsensitiveStringCompareFunctor { bool operator()(const char* left,const char* right) const { return strcmp(left,right) < 0; } };
	typedef std::map<const char*,uintptr,CaseInsensitiveStringCompareFunctor> ExportNameToFunctionIndexMap;

	struct Module
	{
		Memory::Arena arena;

		std::vector<Function*> functions;
		ExportNameToFunctionIndexMap exportNameToFunctionIndexMap;
		FunctionTable functionTable;
		std::vector<FunctionImport> functionImports;
		std::vector<DataSegment> dataSegments;

		uint64 initialNumBytesMemory;
		uint64 maxNumBytesMemory;

		Module() : initialNumBytesMemory(0), maxNumBytesMemory(0) {}

		// When copying a module, copy everything but the arena!
		// This means the new module will have its own arena, but will reference expressions and other data stored in the old module's arena.
		Module(const Module& inCopy)
		: functions(inCopy.functions)
		, exportNameToFunctionIndexMap(inCopy.exportNameToFunctionIndexMap)
		, functionTable(inCopy.functionTable)
		, functionImports(inCopy.functionImports)
		, dataSegments(inCopy.dataSegments)
		, initialNumBytesMemory(inCopy.initialNumBytesMemory)
		, maxNumBytesMemory(inCopy.maxNumBytesMemory)
		{}
	};
}
