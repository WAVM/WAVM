#pragma once

#include <string>
#include "WAVM/IR/IR.h"
#include "WAVM/IR/Operators.h"

namespace WAVM { namespace IR {
	struct FunctionDef;
	struct Module;

	struct ValidationException
	{
		std::string message;
		ValidationException(std::string&& inMessage) : message(inMessage) {}
	};

	struct CodeValidationStreamImpl;

	struct CodeValidationStream
	{
		WAVM_API CodeValidationStream(const Module& module, const FunctionDef& function);
		WAVM_API ~CodeValidationStream();

		WAVM_API void finish();

#define VISIT_OPCODE(_, name, nameString, Imm, ...) WAVM_API void name(Imm imm = {});
		WAVM_ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE

	private:
		CodeValidationStreamImpl* impl;
	};

	template<typename InnerStream> struct CodeValidationProxyStream
	{
		CodeValidationProxyStream(const Module& module,
								  const FunctionDef& function,
								  InnerStream& inInnerStream)
		: codeValidationStream(module, function), innerStream(inInnerStream)
		{
		}

		void finishValidation() { codeValidationStream.finish(); }

#define VISIT_OPCODE(_, name, nameString, Imm, ...)                                                \
	void name(Imm imm = {})                                                                        \
	{                                                                                              \
		codeValidationStream.name(imm);                                                            \
		innerStream.name(imm);                                                                     \
	}
		WAVM_ENUM_OPERATORS(VISIT_OPCODE)
#undef VISIT_OPCODE

	private:
		CodeValidationStream codeValidationStream;
		InnerStream& innerStream;
	};

	WAVM_API void validateTypes(const IR::Module& module);
	WAVM_API void validateImports(const IR::Module& module);
	WAVM_API void validateFunctionDeclarations(const IR::Module& module);
	WAVM_API void validateTableDefs(const IR::Module& module);
	WAVM_API void validateMemoryDefs(const IR::Module& module);
	WAVM_API void validateGlobalDefs(const IR::Module& module);
	WAVM_API void validateExceptionTypeDefs(const IR::Module& module);
	WAVM_API void validateExports(const IR::Module& module);
	WAVM_API void validateStartFunction(const IR::Module& module);
	WAVM_API void validateElemSegments(const IR::Module& module);
	WAVM_API void validateDataSegments(const IR::Module& module);

	inline void validatePreCodeSections(const IR::Module& module)
	{
		validateTypes(module);
		validateImports(module);
		validateFunctionDeclarations(module);
		validateTableDefs(module);
		validateMemoryDefs(module);
		validateGlobalDefs(module);
		validateExceptionTypeDefs(module);
		validateExports(module);
		validateStartFunction(module);
		validateElemSegments(module);
	}

	inline void validatePostCodeSections(const IR::Module& module) { validateDataSegments(module); }
}}
