#include "WAVM/IR/FeatureSpec.h"
#include <string.h>
#include "WAVM/Inline/BasicTypes.h"

using namespace WAVM;

const char* getFeatureListHelpText()
{
	return "  prestd-*               All \"pre-standard\" WebAssembly extensions.\n"
		   "  prestd-simd            WebAssembly SIMD extension.\n"
		   "  prestd-atomics         WebAssembly atomics extension.\n"
		   "  prestd-eh              WebAssembly exception handling extension.\n"
		   "  prestd-extended-names  WebAssembly extended name section extension.\n"
		   "  prestd-multivalue      WebAssembly multi-value extension.\n"
		   "  prestd-multimemory     WebAssembly multi-memory extension.\n"
		   "  prestd-reftypes        WebAssembly reference types extension.\n"
		   "\n"
		   "  legacy-instr-names     Allow legacy instruction names.\n"
		   "  quoted-names           Quoted WAT names extension.\n"
		   "  shared-tables          Shared tables extension.\n"
		   "  wat-custom-sections    Custom sections in WAT format.\n";
}

bool parseAndSetFeature(const char* featureName, IR::FeatureSpec& featureSpec, bool enable)
{
	if(!strcmp(featureName, "prestd-*"))
	{
		featureSpec.setPreStandardizationFeatures(enable);
		return true;
	}
	else if(!strcmp(featureName, "prestd-simd"))
	{
		featureSpec.simd = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-atomics"))
	{
		featureSpec.atomics = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-eh"))
	{
		featureSpec.exceptionHandling = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-extended-names"))
	{
		featureSpec.extendedNamesSection = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-multivalue"))
	{
		featureSpec.multipleResultsAndBlockParams = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-multimemory"))
	{
		featureSpec.multipleMemories = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-reftypes"))
	{
		featureSpec.referenceTypes = enable;
		return true;
	}
	else if(!strcmp(featureName, "legacy-instr-names"))
	{
		featureSpec.allowLegacyInstructionNames = enable;
		return true;
	}
	else if(!strcmp(featureName, "shared-tables"))
	{
		featureSpec.sharedTables = enable;
		return true;
	}
	else if(!strcmp(featureName, "quoted-names"))
	{
		featureSpec.quotedNamesInTextFormat = enable;
		return true;
	}
	else if(!strcmp(featureName, "wat-custom-sections"))
	{
		featureSpec.customSectionsInTextFormat = enable;
		return true;
	}
	else
	{
		return false;
	}
}
