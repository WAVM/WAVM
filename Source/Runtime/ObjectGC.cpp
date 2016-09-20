#include "Core/Core.h"
#include "Runtime.h"
#include "RuntimePrivate.h"
#include "Intrinsics.h"

#include <set>
#include <vector>

namespace Runtime
{
	struct GCGlobals
	{
		std::set<GCObject*> allObjects;

		static GCGlobals& get()
		{
			static GCGlobals globals;
			return globals;
		}
		
	private:
		GCGlobals() {}
	};

	GCObject::GCObject(ObjectKind inKind): Object(inKind)
	{
		GCGlobals::get().allObjects.insert(this);
	}

	GCObject::~GCObject()
	{
		GCGlobals::get().allObjects.erase(this);
	}

	void freeUnreferencedObjects(const std::vector<Object*>& rootObjectReferences)
	{
		std::set<Object*> referencedObjects;
		std::vector<Object*> pendingScanObjects;

		// Initialize the referencedObjects set from the rootObjectReferences and intrinsic objects.
		for(auto object : rootObjectReferences)
		{
			if(object && !referencedObjects.count(object))
			{
				referencedObjects.insert(object);
				pendingScanObjects.push_back(object);
			}
		}

		const std::vector<Object*> intrinsicObjects = Intrinsics::getAllIntrinsicObjects();
		for(auto object : intrinsicObjects)
		{
			if(object && !referencedObjects.count(object))
			{
				referencedObjects.insert(object);
				pendingScanObjects.push_back(object);
			}
		}

		// Scan the objects added to the referenced set so far: gather their child references and recurse.
		while(pendingScanObjects.size())
		{
			Object* scanObject = pendingScanObjects.back();
			pendingScanObjects.pop_back();

			std::vector<Object*> childReferences;
			switch(scanObject->kind)
			{
			case ObjectKind::function:
			{
				FunctionInstance* function = asFunction(scanObject);
				childReferences.push_back(function->moduleInstance);
				break;
			}
			case ObjectKind::module:
			{
				ModuleInstance* moduleInstance = asModule(scanObject);
				childReferences.insert(childReferences.begin(),moduleInstance->imports.begin(),moduleInstance->imports.end());
				childReferences.insert(childReferences.begin(),moduleInstance->functionDefs.begin(),moduleInstance->functionDefs.end());
				childReferences.insert(childReferences.begin(),moduleInstance->functions.begin(),moduleInstance->functions.end());
				childReferences.insert(childReferences.begin(),moduleInstance->tables.begin(),moduleInstance->tables.end());
				childReferences.insert(childReferences.begin(),moduleInstance->memories.begin(),moduleInstance->memories.end());
				childReferences.insert(childReferences.begin(),moduleInstance->globals.begin(),moduleInstance->globals.end());
				childReferences.push_back(moduleInstance->defaultMemory);
				childReferences.push_back(moduleInstance->defaultTable);
				break;
			}
			case ObjectKind::table:
			{
				Table* table = asTable(scanObject);
				childReferences.insert(childReferences.end(),table->elements.begin(),table->elements.end());
				break;
			}
			case ObjectKind::memory:
			case ObjectKind::global: break;
			default: Core::unreachable();
			};

			for(auto reference : childReferences)
			{
				if(reference && !referencedObjects.count(reference))
				{
					referencedObjects.insert(reference);
					pendingScanObjects.push_back(reference);
				}
			}
		};

		GCGlobals& gcGlobals = GCGlobals::get();
		auto objectIt = gcGlobals.allObjects.begin();
		while(objectIt != gcGlobals.allObjects.end())
		{
			if(referencedObjects.count(*objectIt)) { ++objectIt; }
			else
			{
				Object* object = *objectIt;
				objectIt = gcGlobals.allObjects.erase(objectIt);
				delete object;
			}
		}
	}
}