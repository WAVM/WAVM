#pragma once

#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace IR {
	struct FeatureSpec
	{
		// A feature flag for the MVP, just so the MVP operators can reference it as the required
		// feature flag.
		const bool mvp = true;

		// Proposed standard extensions that are likely to be standardized without further changes.
		bool importExportMutableGlobals = true;
		bool nonTrappingFloatToInt = true;
		bool extendedSignExtension = true;
		bool bulkMemoryOperations = true;

		// Proposed standard extensions
		bool simd = true;
		bool atomics = true;
		bool exceptionHandling = true;
		bool multipleResultsAndBlockParams = true;
		bool referenceTypes = true;
		bool extendedNamesSection = true;
		bool multipleMemories = true;

		// WAVM-specific extensions
		bool sharedTables = false;
		bool requireSharedFlagForAtomicOperators = false; // (true is standard)
		bool allowLegacyInstructionNames = false;
		bool allowAnyExternKindElemSegments = false;
		bool quotedNamesInTextFormat = false;
		bool customSectionsInTextFormat = false;

		Uptr maxLocals = 65536;
		Uptr maxLabelsPerFunction = UINTPTR_MAX;
		Uptr maxDataSegments = UINTPTR_MAX;
		Uptr maxSyntaxRecursion = 500;

		FeatureSpec(bool enablePreStandardizationFeatures = false)
		{
			setPreStandardizationFeatures(enablePreStandardizationFeatures);
		}

		void setPreStandardizationFeatures(bool enablePreStandardizationFeatures)
		{
			simd = enablePreStandardizationFeatures;
			atomics = enablePreStandardizationFeatures;
			exceptionHandling = enablePreStandardizationFeatures;
			multipleResultsAndBlockParams = enablePreStandardizationFeatures;
			referenceTypes = enablePreStandardizationFeatures;
			extendedNamesSection = enablePreStandardizationFeatures;
			multipleMemories = enablePreStandardizationFeatures;
		}

		void setWAVMFeatures(bool enableWAVMFeatures)
		{
			sharedTables = enableWAVMFeatures;
			quotedNamesInTextFormat = enableWAVMFeatures;
			extendedNamesSection = enableWAVMFeatures;
			customSectionsInTextFormat = enableWAVMFeatures;
		}
	};
}}
