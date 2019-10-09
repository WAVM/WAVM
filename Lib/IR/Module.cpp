#include "WAVM/IR/Module.h"
#include "WAVM/IR/IR.h"

using namespace WAVM;
using namespace WAVM::IR;

const char* IR::asString(OrderedSectionID id)
{
	switch(id)
	{
	case OrderedSectionID::moduleBeginning: return "module beginning";
	case OrderedSectionID::type: return "type";
	case OrderedSectionID::import: return "import";
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

bool IR::findCustomSection(const Module& module,
						   const char* customSectionName,
						   Uptr& outCustomSectionIndex)
{
	for(Uptr sectionIndex = 0; sectionIndex < module.customSections.size(); ++sectionIndex)
	{
		if(module.customSections[sectionIndex].name == customSectionName)
		{
			outCustomSectionIndex = sectionIndex;
			return true;
		}
	}
	return false;
}

void IR::insertCustomSection(Module& module, CustomSection&& customSection)
{
	auto it = module.customSections.begin();
	for(; it != module.customSections.end(); ++it)
	{
		if(it->afterSection > customSection.afterSection) { break; }
	}
	module.customSections.insert(it, std::move(customSection));
}

OrderedSectionID IR::getMaxPresentSection(const Module& module, OrderedSectionID maxSection)
{
	switch(maxSection)
	{
	case OrderedSectionID::data:
		if(hasDataSection(module)) { return OrderedSectionID::data; }
		// fall through
	case OrderedSectionID::code:
		if(hasCodeSection(module)) { return OrderedSectionID::code; }
		// fall through
	case OrderedSectionID::dataCount:
		if(hasDataCountSection(module)) { return OrderedSectionID::dataCount; }
		// fall through
	case OrderedSectionID::elem:
		if(hasDataCountSection(module)) { return OrderedSectionID::elem; }
		// fall through
	case OrderedSectionID::start:
		if(hasDataCountSection(module)) { return OrderedSectionID::start; }
		// fall through
	case OrderedSectionID::export_:
		if(hasDataCountSection(module)) { return OrderedSectionID::export_; }
		// fall through
	case OrderedSectionID::exceptionType:
		if(hasDataCountSection(module)) { return OrderedSectionID::exceptionType; }
		// fall through
	case OrderedSectionID::global:
		if(hasDataCountSection(module)) { return OrderedSectionID::global; }
		// fall through
	case OrderedSectionID::memory:
		if(hasDataCountSection(module)) { return OrderedSectionID::memory; }
		// fall through
	case OrderedSectionID::table:
		if(hasDataCountSection(module)) { return OrderedSectionID::table; }
		// fall through
	case OrderedSectionID::function:
		if(hasDataCountSection(module)) { return OrderedSectionID::function; }
		// fall through
	case OrderedSectionID::import:
		if(hasDataCountSection(module)) { return OrderedSectionID::import; }
		// fall through
	case OrderedSectionID::type:
		if(hasDataCountSection(module)) { return OrderedSectionID::type; }
		// fall through
	case OrderedSectionID::moduleBeginning: return OrderedSectionID::moduleBeginning;

	default: WAVM_UNREACHABLE();
	};
}
