#include "WAVM/IR/Module.h"
#include "WAVM/IR/IR.h"

using namespace WAVM;
using namespace WAVM::IR;

const char* IR::asString(OrderedSectionID id)
{
	switch(id)
	{
	case OrderedSectionID::moduleBeginning: return "module_ beginning";
	case OrderedSectionID::type: return "type";
	case OrderedSectionID::import_: return "import";
	case OrderedSectionID::function: return "func";
	case OrderedSectionID::table: return "table";
	case OrderedSectionID::memory: return "memory";
	case OrderedSectionID::global: return "global";
	case OrderedSectionID::exceptionType: return "exception_type";
	case OrderedSectionID::export_: return "export";
	case OrderedSectionID::start: return "start";
	case OrderedSectionID::elem: return "elem";
	case OrderedSectionID::dataCount: return "data_count";
	case OrderedSectionID::code: return "code";
	case OrderedSectionID::data: return "data";

	default: WAVM_UNREACHABLE();
	};
}

bool IR::findCustomSection(const Module& module_,
						   const char* customSectionName,
						   Uptr& outCustomSectionIndex)
{
	for(Uptr sectionIndex = 0; sectionIndex < module_.customSections.size(); ++sectionIndex)
	{
		if(module_.customSections[sectionIndex].name == customSectionName)
		{
			outCustomSectionIndex = sectionIndex;
			return true;
		}
	}
	return false;
}

void IR::insertCustomSection(Module& module_, CustomSection&& customSection)
{
	auto it = module_.customSections.begin();
	for(; it != module_.customSections.end(); ++it)
	{
		if(it->afterSection > customSection.afterSection) { break; }
	}
	module_.customSections.insert(it, std::move(customSection));
}

OrderedSectionID IR::getMaxPresentSection(const Module& module_, OrderedSectionID maxSection)
{
	switch(maxSection)
	{
	case OrderedSectionID::data:
		if(hasDataSection(module_)) { return OrderedSectionID::data; }
		// fall through
	case OrderedSectionID::code:
		if(hasCodeSection(module_)) { return OrderedSectionID::code; }
		// fall through
	case OrderedSectionID::dataCount:
		if(hasDataCountSection(module_)) { return OrderedSectionID::dataCount; }
		// fall through
	case OrderedSectionID::elem:
		if(hasElemSection(module_)) { return OrderedSectionID::elem; }
		// fall through
	case OrderedSectionID::start:
		if(hasStartSection(module_)) { return OrderedSectionID::start; }
		// fall through
	case OrderedSectionID::export_:
		if(hasExportSection(module_)) { return OrderedSectionID::export_; }
		// fall through
	case OrderedSectionID::exceptionType:
		if(hasExceptionTypeSection(module_)) { return OrderedSectionID::exceptionType; }
		// fall through
	case OrderedSectionID::global:
		if(hasGlobalSection(module_)) { return OrderedSectionID::global; }
		// fall through
	case OrderedSectionID::memory:
		if(hasMemorySection(module_)) { return OrderedSectionID::memory; }
		// fall through
	case OrderedSectionID::table:
		if(hasTableSection(module_)) { return OrderedSectionID::table; }
		// fall through
	case OrderedSectionID::function:
		if(hasFunctionSection(module_)) { return OrderedSectionID::function; }
		// fall through
	case OrderedSectionID::import_:
		if(hasImportSection(module_)) { return OrderedSectionID::import_; }
		// fall through
	case OrderedSectionID::type:
		if(hasTypeSection(module_)) { return OrderedSectionID::type; }
		// fall through
	case OrderedSectionID::moduleBeginning: return OrderedSectionID::moduleBeginning;

	default: WAVM_UNREACHABLE();
	};
}
