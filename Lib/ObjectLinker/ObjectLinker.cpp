// Object linker: layout, memory protection, relocation application, stubs, and GOT.
// Format-specific parsers (COFF.cpp, ELF.cpp, MachO.cpp) call into this shared code.

#include "WAVM/ObjectLinker/ObjectLinker.h"
#include <string>
#include <vector>
#include "ObjectLinkerPrivate.h"
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/HashMap.h"
#include "WAVM/Inline/IntCasts.h"
#include "WAVM/Platform/Diagnostics.h"
#include "WAVM/Platform/Memory.h"

using namespace WAVM;
using namespace WAVM::ObjectLinker;

void ObjectLinker::linkObject(U8* objectBytes,
							  Uptr objectNumBytes,
							  const HashMap<std::string, Uptr>& importedSymbolMap,
							  LinkResult& outResult,
							  PageAllocator allocatePages)
{
	if(objectNumBytes < 4) { Errors::fatalf("Object file too small (%zu bytes)", objectNumBytes); }

	// Detect format from magic bytes.
	if(objectNumBytes >= 4 && objectBytes[0] == 0x7f && objectBytes[1] == 'E'
	   && objectBytes[2] == 'L' && objectBytes[3] == 'F')
	{
		linkELF(objectBytes, objectNumBytes, importedSymbolMap, outResult, allocatePages);
	}
	else if(objectNumBytes >= 4
			&& ((objectBytes[0] == 0xCF && objectBytes[1] == 0xFA && objectBytes[2] == 0xED
				 && objectBytes[3] == 0xFE)
				|| (objectBytes[0] == 0xFE && objectBytes[1] == 0xED && objectBytes[2] == 0xFA
					&& objectBytes[3] == 0xCF)))
	{
		linkMachO(objectBytes, objectNumBytes, importedSymbolMap, outResult, allocatePages);
	}
	else
	{
		// Assume COFF (no single magic for COFF .obj files).
		linkCOFF(objectBytes, objectNumBytes, importedSymbolMap, outResult, allocatePages);
	}
}

ImageLayout ObjectLinker::layoutImage(const std::vector<SectionLayoutInfo>& sections,
									  Uptr numStubBytes,
									  Uptr numGotBytes,
									  Uptr numExtraReadOnlyBytes,
									  PageAllocator allocatePages)
{
	const Uptr pageSize = Platform::getBytesPerPage();
	ImageLayout layout;
	layout.numSections = sections.size();

	// Separate sections by access type and track order.
	std::vector<Uptr> codeSections, roSections, rwSections;
	for(Uptr i = 0; i < sections.size(); ++i)
	{
		switch(sections[i].access)
		{
		case SectionAccess::readExecute: codeSections.push_back(i); break;
		case SectionAccess::readOnly: roSections.push_back(i); break;
		case SectionAccess::readWrite: rwSections.push_back(i); break;
		default: Errors::fatalf("Unknown section access type"); break;
		}
	}

	// Calculate sizes per segment with alignment.
	auto alignUp = [](Uptr value, Uptr alignment) -> Uptr {
		return alignment ? (value + alignment - 1) & ~(alignment - 1) : value;
	};

	Uptr codeSize = 0;
	for(Uptr i : codeSections)
	{
		codeSize = alignUp(codeSize, sections[i].alignment) + sections[i].dataSize;
	}

	// Add stub space to code segment.
	if(numStubBytes > 0) { codeSize = alignUp(codeSize, 16) + numStubBytes; }

	Uptr roSize = 0;
	for(Uptr i : roSections)
	{
		roSize = alignUp(roSize, sections[i].alignment) + sections[i].dataSize;
	}

	// Add GOT space to read-only segment.
	if(numGotBytes > 0) { roSize = alignUp(roSize, 8) + numGotBytes; }

	// Add extra read-only space (e.g. for synthesized __unwind_info).
	if(numExtraReadOnlyBytes > 0) { roSize = alignUp(roSize, 4) + numExtraReadOnlyBytes; }

	Uptr rwSize = 0;
	for(Uptr i : rwSections)
	{
		rwSize = alignUp(rwSize, sections[i].alignment) + sections[i].dataSize;
	}

	// Page-align each segment.
	Uptr codePages = (codeSize + pageSize - 1) / pageSize;
	Uptr roPages = (roSize + pageSize - 1) / pageSize;
	Uptr rwPages = (rwSize + pageSize - 1) / pageSize;
	Uptr totalPages = codePages + roPages + rwPages;

	if(totalPages == 0)
	{
		layout.imageBase = nullptr;
		layout.numPages = 0;
		layout.codeSize = 0;
		layout.roSize = 0;
		layout.rwSize = 0;
		layout.sectionLoadAddresses.resize(sections.size(), 0);
		layout.stubBase = layout.stubEnd = layout.stubCurrent = nullptr;
		layout.gotBase = layout.gotEnd = layout.gotCurrent = nullptr;
		return layout;
	}

	// Allocate and commit pages.
	if(!allocatePages)
	{
		allocatePages = [](Uptr numPages) -> U8* {
			U8* base = Platform::allocateVirtualPages(numPages);
			if(!base || !Platform::commitVirtualPages(base, numPages))
			{
				Errors::fatalf("Failed to allocate %" WAVM_PRIuPTR " pages for JIT image",
							   numPages);
			}
			Platform::registerVirtualAllocation(numPages << Platform::getBytesPerPageLog2());
			return base;
		};
	}
	U8* imageBase = allocatePages(totalPages);
	if(!imageBase)
	{
		Errors::fatalf("Page allocator returned null for %" WAVM_PRIuPTR " pages", totalPages);
	}

	layout.imageBase = imageBase;
	layout.numPages = totalPages;
	layout.codeSize = codePages * pageSize;
	layout.roSize = roPages * pageSize;
	layout.rwSize = rwPages * pageSize;
	layout.sectionLoadAddresses.resize(sections.size(), 0);

	// Assign addresses within each segment.
	U8* codeBase = imageBase;
	U8* roBase = imageBase + codePages * pageSize;
	U8* rwBase = roBase + roPages * pageSize;

	Uptr codeOffset = 0;
	for(Uptr i : codeSections)
	{
		codeOffset = alignUp(codeOffset, sections[i].alignment);
		layout.sectionLoadAddresses[i] = reinterpret_cast<Uptr>(codeBase + codeOffset);
		codeOffset += sections[i].dataSize;
	}

	// Place stubs after code sections.
	if(numStubBytes > 0)
	{
		codeOffset = alignUp(codeOffset, 16);
		layout.stubBase = codeBase + codeOffset;
		layout.stubEnd = layout.stubBase + numStubBytes;
		layout.stubCurrent = layout.stubBase;
	}
	else
	{
		layout.stubBase = layout.stubEnd = layout.stubCurrent = nullptr;
	}

	Uptr roOffset = 0;
	for(Uptr i : roSections)
	{
		roOffset = alignUp(roOffset, sections[i].alignment);
		layout.sectionLoadAddresses[i] = reinterpret_cast<Uptr>(roBase + roOffset);
		roOffset += sections[i].dataSize;
	}

	// Place GOT after read-only sections.
	if(numGotBytes > 0)
	{
		roOffset = alignUp(roOffset, 8);
		layout.gotBase = roBase + roOffset;
		layout.gotEnd = layout.gotBase + numGotBytes;
		layout.gotCurrent = layout.gotBase;
		roOffset += numGotBytes;
	}
	else
	{
		layout.gotBase = layout.gotEnd = layout.gotCurrent = nullptr;
	}

	// Place extra read-only space after GOT.
	if(numExtraReadOnlyBytes > 0)
	{
		roOffset = alignUp(roOffset, 4);
		layout.extraReadOnly = roBase + roOffset;
	}

	Uptr rwOffset = 0;
	for(Uptr i : rwSections)
	{
		rwOffset = alignUp(rwOffset, sections[i].alignment);
		layout.sectionLoadAddresses[i] = reinterpret_cast<Uptr>(rwBase + rwOffset);
		rwOffset += sections[i].dataSize;
	}

	return layout;
}

void ObjectLinker::protectImage(const ImageLayout& layout)
{
	if(!layout.imageBase) { return; }

	const Uptr pageSize = Platform::getBytesPerPage();
	U8* codeBase = layout.imageBase;
	Uptr codePages = layout.codeSize / pageSize;
	U8* roBase = codeBase + layout.codeSize;
	Uptr roPages = layout.roSize / pageSize;
	U8* rwBase = roBase + layout.roSize;
	Uptr rwPages = layout.rwSize / pageSize;

	if(codePages > 0)
	{
		if(!Platform::setVirtualPageAccess(
			   codeBase, codePages, Platform::MemoryAccess::readExecute))
		{
			Errors::fatalf("Failed to set code pages to readExecute");
		}
	}

	if(roPages > 0)
	{
		if(!Platform::setVirtualPageAccess(roBase, roPages, Platform::MemoryAccess::readOnly))
		{
			Errors::fatalf("Failed to set read-only pages");
		}
	}

	if(rwPages > 0)
	{
		if(!Platform::setVirtualPageAccess(rwBase, rwPages, Platform::MemoryAccess::readWrite))
		{
			Errors::fatalf("Failed to set read-write pages");
		}
	}
}

static void writeStub(Arch arch, U8* stub, Uptr targetAddr)
{
	switch(arch)
	{
	case Arch::x86_64:
		// jmp [rip+0]; .quad addr
		// FF 25 00 00 00 00 <8-byte addr> (14 bytes, padded to 16)
		stub[0] = 0xFF;
		stub[1] = 0x25;
		writeLE<U32>(stub + 2, 0);
		writeLE<U64>(stub + 6, wrap<U64>(targetAddr));
		stub[14] = 0xCC; // INT3 padding
		stub[15] = 0xCC;
		break;
	case Arch::aarch64:
		// LDR X16, [PC, #8]; BR X16; .quad addr
		writeLE<U32>(stub + 0, 0x58000050); // LDR X16, #8
		writeLE<U32>(stub + 4, 0xD61F0200); // BR X16
		writeLE<U64>(stub + 8, wrap<U64>(targetAddr));
		break;
	default: WAVM_UNREACHABLE();
	}
}

template<Arch arch> Uptr ObjectLinker::getOrCreateStub(ImageLayout& layout, Uptr targetAddr)
{
	Uptr* cached = layout.stubCache.get(targetAddr);
	if(cached) { return *cached; }

	if(!layout.stubCurrent || layout.stubCurrent + stubSlotSize > layout.stubEnd)
	{
		Errors::fatalf("Ran out of stub space");
	}
	U8* stub = layout.stubCurrent;
	layout.stubCurrent += stubSlotSize;

	writeStub(arch, stub, targetAddr);

	Uptr stubAddr = reinterpret_cast<Uptr>(stub);
	layout.stubCache.addOrFail(targetAddr, stubAddr);
	return stubAddr;
}

template Uptr ObjectLinker::getOrCreateStub<Arch::x86_64>(ImageLayout&, Uptr);
template Uptr ObjectLinker::getOrCreateStub<Arch::aarch64>(ImageLayout&, Uptr);

Uptr ObjectLinker::getOrCreateGotEntry(ImageLayout& layout, Uptr symbolAddr)
{
	Uptr* existing = layout.gotCache.get(symbolAddr);
	if(existing) { return *existing; }

	if(!layout.gotCurrent) { Errors::fatalf("GOT space not allocated but GOT entry requested"); }
	if(layout.gotCurrent + 8 > layout.gotEnd) { Errors::fatalf("Ran out of GOT space"); }

	U8* entry = layout.gotCurrent;
	layout.gotCurrent += 8;
	writeLE<U64>(entry, wrap<U64>(symbolAddr));
	Uptr entryAddr = reinterpret_cast<Uptr>(entry);
	layout.gotCache.addOrFail(symbolAddr, entryAddr);
	return entryAddr;
}
