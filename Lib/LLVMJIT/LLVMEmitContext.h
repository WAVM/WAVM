#pragma once

#include "IR/Module.h"
#include "IR/Types.h"
#include "LLVMJITPrivate.h"

#include "LLVMPreInclude.h"

#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Value.h"

#include "LLVMPostInclude.h"

namespace LLVMJIT
{
	// Code and state that is used for generating IR for both thunks and WASM functions.
	struct EmitContext
	{
		llvm::IRBuilder<> irBuilder;

		llvm::Value* contextPointerVariable;
		llvm::Value* memoryBasePointerVariable;
		llvm::Value* tableBasePointerVariable;

		EmitContext(llvm::Constant* inDefaultMemoryOffset, llvm::Constant* inDefaultTableOffset)
		: irBuilder(*llvmContext)
		, contextPointerVariable(nullptr)
		, memoryBasePointerVariable(nullptr)
		, tableBasePointerVariable(nullptr)
		, defaultMemoryOffset(inDefaultMemoryOffset)
		, defaultTableOffset(inDefaultTableOffset)
		{
		}

		llvm::Value* loadFromUntypedPointer(llvm::Value* pointer, llvm::Type* valueType)
		{
			return irBuilder.CreateLoad(
				irBuilder.CreatePointerCast(pointer, valueType->getPointerTo()));
		}

		void storeToUntypedPointer(llvm::Value* value, llvm::Value* pointer)
		{
			irBuilder.CreateStore(
				value, irBuilder.CreatePointerCast(pointer, value->getType()->getPointerTo()));
		}

		void reloadMemoryAndTableBase()
		{
			// Derive the compartment runtime data from the context address by masking off the lower
			// 32 bits.
			llvm::Value* compartmentAddress = irBuilder.CreateIntToPtr(
				irBuilder.CreateAnd(irBuilder.CreatePtrToInt(
										irBuilder.CreateLoad(contextPointerVariable), llvmI64Type),
									emitLiteral(~((U64(1) << 32) - 1))),
				llvmI8PtrType);

			// Load the defaultMemoryBase and defaultTableBase values from the runtime data for this
			// module instance.

			if(defaultMemoryOffset)
			{
				irBuilder.CreateStore(
					loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(compartmentAddress, {defaultMemoryOffset}),
						llvmI8PtrType),
					memoryBasePointerVariable);
			}

			if(defaultTableOffset)
			{
				irBuilder.CreateStore(
					loadFromUntypedPointer(
						irBuilder.CreateInBoundsGEP(compartmentAddress, {defaultTableOffset}),
						llvmI8PtrType),
					tableBasePointerVariable);
			}
		}

		void initContextVariables(llvm::Value* initialContextPointer)
		{
			memoryBasePointerVariable
				= irBuilder.CreateAlloca(llvmI8PtrType, nullptr, "memoryBase");
			tableBasePointerVariable = irBuilder.CreateAlloca(llvmI8PtrType, nullptr, "tableBase");
			contextPointerVariable = irBuilder.CreateAlloca(llvmI8PtrType, nullptr, "context");
			irBuilder.CreateStore(initialContextPointer, contextPointerVariable);
			reloadMemoryAndTableBase();
		}

		// Creates either a call or an invoke if the call occurs inside a try.
		ValueVector emitCallOrInvoke(llvm::Value* callee,
									 llvm::ArrayRef<llvm::Value*> args,
									 IR::FunctionType calleeType,
									 IR::CallingConvention callingConvention,
									 llvm::BasicBlock* unwindToBlock = nullptr)
		{
			llvm::ArrayRef<llvm::Value*> augmentedArgs = args;

			if(callingConvention == IR::CallingConvention::intrinsicWithMemAndTable)
			{
				// Augment the argument list with the context pointer, and the default memory and
				// table IDs.
				auto augmentedArgsAlloca
					= (llvm::Value**)alloca(sizeof(llvm::Value*) * (args.size() + 3));
				augmentedArgs = llvm::ArrayRef<llvm::Value*>(augmentedArgsAlloca, args.size() + 3);
				augmentedArgsAlloca[0] = irBuilder.CreateLoad(contextPointerVariable);
				augmentedArgsAlloca[1] = defaultMemoryOffset
											 ? getMemoryIdFromOffset(defaultMemoryOffset)
											 : emitLiteral(I64(-1));
				augmentedArgsAlloca[2] = defaultTableOffset
											 ? getTableIdFromOffset(defaultTableOffset)
											 : emitLiteral(I64(-1));
				for(Uptr argIndex = 0; argIndex < args.size(); ++argIndex)
				{ augmentedArgsAlloca[3 + argIndex] = args[argIndex]; }
			}
			else if(callingConvention != IR::CallingConvention::c)
			{
				// Augment the argument list with the context pointer.
				auto augmentedArgsAlloca
					= (llvm::Value**)alloca(sizeof(llvm::Value*) * (args.size() + 1));
				augmentedArgs = llvm::ArrayRef<llvm::Value*>(augmentedArgsAlloca, args.size() + 1);
				augmentedArgsAlloca[0] = irBuilder.CreateLoad(contextPointerVariable);
				for(Uptr argIndex = 0; argIndex < args.size(); ++argIndex)
				{ augmentedArgsAlloca[1 + argIndex] = args[argIndex]; }
			}

			// Call or invoke the callee.
			llvm::Value* returnValue;
			if(!unwindToBlock)
			{
				auto call = irBuilder.CreateCall(callee, augmentedArgs);
				call->setCallingConv(asLLVMCallingConv(callingConvention));
				returnValue = call;
			}
			else
			{
				auto returnBlock = llvm::BasicBlock::Create(
					*llvmContext, "invokeReturn", irBuilder.GetInsertBlock()->getParent());
				auto invoke
					= irBuilder.CreateInvoke(callee, returnBlock, unwindToBlock, augmentedArgs);
				invoke->setCallingConv(asLLVMCallingConv(callingConvention));
				irBuilder.SetInsertPoint(returnBlock);
				returnValue = invoke;
			}

			ValueVector results;
			switch(callingConvention)
			{
			case IR::CallingConvention::wasm:
			{
				// Update the context variable.
				auto newContextPointer = irBuilder.CreateExtractValue(returnValue, {0});
				irBuilder.CreateStore(newContextPointer, contextPointerVariable);

				// Reload the memory/table base pointers.
				reloadMemoryAndTableBase();

				if(areResultsReturnedDirectly(calleeType.results()))
				{
					// If the results are returned directly, extract them from the returned struct.
					for(Uptr resultIndex = 0; resultIndex < calleeType.results().size();
						++resultIndex)
					{
						results.push_back(
							irBuilder.CreateExtractValue(returnValue, {U32(1), U32(resultIndex)}));
					}
				}
				else
				{
					// Otherwise, load them from the context.
					Uptr resultOffset = 0;
					for(IR::ValueType resultType : calleeType.results())
					{
						const U8 resultNumBytes = getTypeByteWidth(resultType);

						resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
						wavmAssert(resultOffset < Runtime::maxThunkArgAndReturnBytes);

						results.push_back(irBuilder.CreateLoad(irBuilder.CreatePointerCast(
							irBuilder.CreateInBoundsGEP(newContextPointer,
														{emitLiteral(resultOffset)}),
							asLLVMType(resultType)->getPointerTo())));

						resultOffset += resultNumBytes;
					}
				}

				break;
			}
			case IR::CallingConvention::intrinsicWithContextSwitch:
			{
				auto newContextPointer = returnValue;

				// Update the context variable.
				irBuilder.CreateStore(newContextPointer, contextPointerVariable);

				// Reload the memory/table base pointers.
				reloadMemoryAndTableBase();

				// Load the call result from the returned context.
				wavmAssert(calleeType.results().size() <= 1);
				if(calleeType.results().size() == 1)
				{
					llvm::Type* llvmResultType = asLLVMType(calleeType.results()[0]);
					results.push_back(loadFromUntypedPointer(newContextPointer, llvmResultType));
				}

				break;
			}
			case IR::CallingConvention::intrinsicWithMemAndTable:
			case IR::CallingConvention::intrinsic:
			case IR::CallingConvention::c:
			{
				wavmAssert(calleeType.results().size() <= 1);
				if(calleeType.results().size() == 1) { results.push_back(returnValue); }
				break;
			}
			default: Errors::unreachable();
			};

			return results;
		}

		void emitReturn(IR::TypeTuple resultTypes, const llvm::ArrayRef<llvm::Value*>& results)
		{
			llvm::Value* returnStruct = getZeroedLLVMReturnStruct(resultTypes);
			returnStruct = irBuilder.CreateInsertValue(
				returnStruct, irBuilder.CreateLoad(contextPointerVariable), {U32(0)});

			wavmAssert(resultTypes.size() == results.size());
			if(areResultsReturnedDirectly(resultTypes))
			{
				// If the results are returned directly, insert them into the return struct.
				for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
				{
					llvm::Value* result = results[resultIndex];
					returnStruct = irBuilder.CreateInsertValue(
						returnStruct, result, {U32(1), U32(resultIndex)});
				}
			}
			else
			{
				// Otherwise, store them in the context.
				Uptr resultOffset = 0;
				for(Uptr resultIndex = 0; resultIndex < results.size(); ++resultIndex)
				{
					const IR::ValueType resultType = resultTypes[resultIndex];
					const U8 resultNumBytes = IR::getTypeByteWidth(resultType);

					resultOffset = (resultOffset + resultNumBytes - 1) & -I8(resultNumBytes);
					wavmAssert(resultOffset < Runtime::maxThunkArgAndReturnBytes);

					irBuilder.CreateStore(results[resultIndex],
										  irBuilder.CreatePointerCast(
											  irBuilder.CreateInBoundsGEP(
												  irBuilder.CreateLoad(contextPointerVariable),
												  {emitLiteral(resultOffset)}),
											  asLLVMType(resultType)->getPointerTo()));

					resultOffset += resultNumBytes;
				}
			}

			irBuilder.CreateRet(returnStruct);
		}

	private:
		llvm::Constant* defaultMemoryOffset;
		llvm::Constant* defaultTableOffset;
	};
}
