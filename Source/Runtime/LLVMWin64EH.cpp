#include "Inline/Assert.h"
#include "IR/Operators.h"
#include "LLVMJIT.h"
#include "Logging/Logging.h"

#ifdef _WIN64

#define PRINT_SEH_TABLES 0

namespace LLVMJIT
{
	enum class UnwindOpcode : U8
	{
		UWOP_PUSH_NONVOL = 0,
		UWOP_ALLOC_LARGE = 1,
		UWOP_ALLOC_SMALL = 2,
		UWOP_SET_FPREG = 3,
		UWOP_SAVE_NONVOL = 4,
		UWOP_SAVE_NONVOL_FAR = 5,
		UWOP_SAVE_XMM128 = 6,
		UWOP_SAVE_XMM128_FAR = 7,
		UWOP_PUSH_MACHFRAME = 8
	};

	PACKED_STRUCT(struct SEHLanguageSpecificDataEntry
	{
		U32 startAddress;
		U32 endAddress;
		U32 filterOrFinallyAddress;
		U32 landingPadAddress;
	});

	PACKED_STRUCT(struct SEHLanguageSpecificData
	{
		U32 numEntries;
		SEHLanguageSpecificDataEntry entries[1];
	});

	PACKED_STRUCT(struct UnwindInfoSuffix
	{
		U32 exceptionHandlerAddress;
		SEHLanguageSpecificData sehLSDA;
	});

	PACKED_STRUCT(struct UnwindCode
	{
		U8 codeOffset;
		UnwindOpcode opcode : 4;
		U8 opInfo : 4;
	});
	
	namespace UnwindInfoFlags
	{
		enum UnwindInfoFlags : U8
		{
			UNW_FLAG_EHANDLER = 0x1,
			UNW_FLAG_UHANDLER = 0x2,
			UNW_FLAG_CHAININFO = 0x4
		};
	};

	PACKED_STRUCT(struct UnwindInfoPrefix
	{
		U8 version:3;
		U8 flags:5;
		U8 sizeOfProlog;
		U8 countOfCodes;
		U8 frameRegister:4;
		U8 frameOffset:4;
		UnwindCode unwindCodes[1];
	});

	PACKED_STRUCT(struct RuntimeFunction
	{
		U32 beginAddress;
		U32 endAddress;
		union
		{
			U32 unwindInfoAddress;
			U32 unwindData;
		};
	});

	static const char* getUnwindRegisterName(U8 registerIndex)
	{
		static const char* names[] = {
			"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
			"r8","r9","r10","r11","r12","r13","r14","r15"
		};
		errorUnless(registerIndex < (sizeof(names) / sizeof(names[0])));
		return names[registerIndex];
	}

	void applyImageRelativeRelocations(
		const llvm::LoadedObjectInfo* loadedObject,
		llvm::object::SectionRef section,
		const U8* sectionCopy,
		Uptr imageBaseAddress,
		Uptr sehTrampolineAddress
		)
	{
		U8* sectionData = reinterpret_cast<U8*>(Uptr(loadedObject->getSectionLoadAddress(section)));
		for(auto relocIt : section.relocations())
		{
			// Only handle type 3 (IMAGE_REL_AMD64_ADDR32NB).
			if(relocIt.getType() == 3)
			{
				const auto symbol = relocIt.getSymbol();

				U64 symbolAddress;
				if(symbol->getFlags() & llvm::object::SymbolRef::SF_Undefined)
				{
					// Resolve __C_specific_handler to the trampoline previously created in the image's 32-bit address space.
					const std::string symbolName = cantFail(symbol->getName()).str();
					if(symbolName == "__C_specific_handler") { symbolAddress = sehTrampolineAddress; }
					else { symbolAddress = U64(cantFail(NullResolver::singleton->findSymbol(cantFail(symbol->getName()).str()).getAddress())); }
				}
				else
				{
					const llvm::object::section_iterator symbolSection = cantFail(symbol->getSection());
					symbolAddress =
						(cantFail(symbol->getAddress()) - symbolSection->getAddress())
						+ loadedObject->getSectionLoadAddress(*symbolSection);
				}
				
				U32* valueToRelocate = (U32*)(sectionData + relocIt.getOffset());
				const U32* originalValue = (U32*)(sectionCopy + relocIt.getOffset());
				const U64 relocatedValue64 =
					symbolAddress
					+ *originalValue
					- imageBaseAddress;
				errorUnless(relocatedValue64 <= UINT32_MAX);
				*valueToRelocate = (U32)relocatedValue64;
			}
		}
	}

	void processSEHTables(
		Uptr imageBaseAddress,
		const llvm::LoadedObjectInfo* loadedObject,
		const llvm::object::SectionRef& pdataSection,const U8* pdataCopy,Uptr pdataNumBytes,
		const llvm::object::SectionRef& xdataSection,const U8* xdataCopy,
		Uptr sehTrampolineAddress
		)
	{
		if(pdataCopy)
		{
			wavmAssert(xdataCopy);
			applyImageRelativeRelocations(loadedObject,pdataSection,pdataCopy,imageBaseAddress,sehTrampolineAddress);
			applyImageRelativeRelocations(loadedObject,xdataSection,xdataCopy,imageBaseAddress,sehTrampolineAddress);
			
			const RuntimeFunction* functionTable
				= reinterpret_cast<const RuntimeFunction*>(Uptr(loadedObject->getSectionLoadAddress(pdataSection)));

			Platform::registerSEHUnwindInfo(imageBaseAddress,reinterpret_cast<Uptr>(functionTable),pdataNumBytes);
			
			if(PRINT_SEH_TABLES)
			{
				const Uptr numFunctions = pdataNumBytes / sizeof(RuntimeFunction);

				Log::printf(Log::Category::debug,"Win64 SEH function table:\n");
				for(Uptr functionIndex = 0;functionIndex < numFunctions;++functionIndex)
				{
					const RuntimeFunction& runtimeFunction = functionTable[functionIndex];
					U8* unwindInfo = reinterpret_cast<U8*>(imageBaseAddress + runtimeFunction.unwindInfoAddress);
					UnwindInfoPrefix* unwindInfoPrefix = (UnwindInfoPrefix*)unwindInfo;
						
					Log::printf(Log::Category::debug," Function %u\n",functionIndex);
					Log::printf(Log::Category::debug,"  address: 0x%04x-0x%04x\n",runtimeFunction.beginAddress,runtimeFunction.endAddress);
					Log::printf(Log::Category::debug,"  unwind info:\n");
					Log::printf(Log::Category::debug,"   version: %u\n",unwindInfoPrefix->version);
					Log::printf(Log::Category::debug,"   flags: %x\n",unwindInfoPrefix->flags);
					Log::printf(Log::Category::debug,"   prolog bytes: %u\n",unwindInfoPrefix->sizeOfProlog);
					Log::printf(Log::Category::debug,"   frame register: %s\n",getUnwindRegisterName(unwindInfoPrefix->frameRegister));
					Log::printf(Log::Category::debug,"   frame offset: 0x%x\n",unwindInfoPrefix->frameOffset * 16);

					UnwindCode* const unwindCodes = unwindInfoPrefix->unwindCodes;
					UnwindCode* unwindCode = unwindCodes;
					if(unwindInfoPrefix->countOfCodes)
					{
						Log::printf(Log::Category::debug,"   prolog unwind codes:\n");
						while(unwindCode < unwindCodes + unwindInfoPrefix->countOfCodes)
						{
							switch(unwindCode->opcode)
							{
							case UnwindOpcode::UWOP_PUSH_NONVOL:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_PUSH_NONVOL %s\n",
									unwindCode->codeOffset,
									getUnwindRegisterName(unwindCode->opInfo)
									);
								++unwindCode;
								break;
							case UnwindOpcode::UWOP_ALLOC_LARGE:
								if(unwindCode->opInfo == 0)
								{
									Log::printf(Log::Category::debug,"    0x%02x UWOP_ALLOC_LARGE 0x%x\n",
										unwindCode->codeOffset,
										*(U16*)&unwindCode[1] * 8
										);
									unwindCode += 2;
								}
								else
								{
									errorUnless(unwindCode->opInfo == 1);
									Log::printf(Log::Category::debug,"    0x%02x UWOP_ALLOC_LARGE 0x%x\n",
										unwindCode->codeOffset,
										*(U32*)&unwindCode[1]
										);
									unwindCode += 3;
								}
								break;
							case UnwindOpcode::UWOP_ALLOC_SMALL:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_ALLOC_SMALL 0x%x\n",
									unwindCode->codeOffset,
									unwindCode->opInfo * 8 + 8
									);
								++unwindCode;
								break;
							case UnwindOpcode::UWOP_SET_FPREG:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_SET_FPREG\n",
									unwindCode->codeOffset
									);
								++unwindCode;
								break;
							case UnwindOpcode::UWOP_SAVE_NONVOL:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_SAVE_NONVOL %s 0x%x\n",
									unwindCode->codeOffset,
									getUnwindRegisterName(unwindCode->opInfo),
									*(U16*)&unwindCode[1] * 8
									);
									unwindCode += 2;
								break;
							case UnwindOpcode::UWOP_SAVE_NONVOL_FAR:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_SAVE_NONVOL_FAR %s 0x%x\n",
									unwindCode->codeOffset,
									getUnwindRegisterName(unwindCode->opInfo),
									*(U32*)&unwindCode[1]
									);
									unwindCode += 3;
								break;
							case UnwindOpcode::UWOP_SAVE_XMM128:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_SAVE_XMM128 xmm%u 0x%x\n",
									unwindCode->codeOffset,
									unwindCode->opInfo,
									*(U16*)&unwindCode[1] * 8
									);
								unwindCode += 2;
								break;
							case UnwindOpcode::UWOP_SAVE_XMM128_FAR:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_SAVE_XMM128_FAR xmm%u 0x%x\n",
									unwindCode->codeOffset,
									unwindCode->opInfo,
									*(U32*)&unwindCode[1]
									);
								unwindCode += 3;
								break;
							case UnwindOpcode::UWOP_PUSH_MACHFRAME:
								Log::printf(Log::Category::debug,"    0x%02x UWOP_PUSH_MACHFRAME %u\n",
									unwindCode->codeOffset,
									unwindCode->opInfo
									);
								++unwindCode;
								break;
							}
						}
					}

					if(unwindInfoPrefix->countOfCodes & 1)
					{
						++unwindCode;
					}

					if(	(unwindInfoPrefix->flags & UnwindInfoFlags::UNW_FLAG_EHANDLER)
					||	(unwindInfoPrefix->flags & UnwindInfoFlags::UNW_FLAG_UHANDLER))
					{
						UnwindInfoSuffix* suffix = (UnwindInfoSuffix*)unwindCode;
						Log::printf(Log::Category::debug,"   exception handler address: 0x%x\n",suffix->exceptionHandlerAddress);
						for(Uptr entryIndex = 0;entryIndex < suffix->sehLSDA.numEntries;++entryIndex)
						{
							const SEHLanguageSpecificDataEntry& entry = suffix->sehLSDA.entries[entryIndex];
							Log::printf(Log::Category::debug,"   LSDA entry %u:\n",entryIndex);
							Log::printf(Log::Category::debug,"    address: 0x%x-0x%x\n",entry.startAddress,entry.endAddress);
							Log::printf(Log::Category::debug,"    filterOrFinally address: 0x%x\n",entry.filterOrFinallyAddress);
							Log::printf(Log::Category::debug,"    landing pad address: 0x%x\n",entry.landingPadAddress);
						}
					}
				}
			}
		}
	}
}

#endif