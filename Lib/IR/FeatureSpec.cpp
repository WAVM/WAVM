#include "WAVM/IR/FeatureSpec.h"
#include <string.h>
#include "WAVM/Inline/BasicTypes.h"

using namespace WAVM;

const char* IR::getFeatureListHelpText()
{
	return "  prestd-*               All \"pre-standard\" WebAssembly extensions.\n"
		   "  prestd-simd            WebAssembly SIMD extension.\n"
		   "  prestd-atomics         WebAssembly atomics extension.\n"
		   "  prestd-eh              WebAssembly exception handling extension.\n"
		   "  prestd-multivalue      WebAssembly multi-value extension.\n"
		   "  prestd-bulkmemoryops   WebAssembly bulk memory ops extension.\n"
		   "  prestd-reftypes        WebAssembly reference types extension.\n";
}

bool IR::parseAndSetFeature(const char* featureName, IR::FeatureSpec& featureSpec, bool enable)
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
	else if(!strcmp(featureName, "prestd-multivalue"))
	{
		featureSpec.multipleResultsAndBlockParams = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-bulkmemoryops"))
	{
		featureSpec.bulkMemoryOperations = enable;
		return true;
	}
	else if(!strcmp(featureName, "prestd-reftypes"))
	{
		featureSpec.referenceTypes = enable;
		return true;
	}
	else
	{
		return false;
	}
}
