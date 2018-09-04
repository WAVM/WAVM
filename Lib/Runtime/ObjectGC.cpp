#include <inttypes.h>
#include <atomic>
#include <utility>
#include <vector>

#include "Inline/BasicTypes.h"
#include "Inline/Errors.h"
#include "Inline/Hash.h"
#include "Inline/HashSet.h"
#include "Inline/Lock.h"
#include "Inline/Timing.h"
#include "Logging/Logging.h"
#include "Platform/Platform.h"
#include "Runtime/Runtime.h"
#include "RuntimePrivate.h"

using namespace Runtime;

// Keep a global list of all objects.
struct GCGlobals
{
	Platform::Mutex mutex;
	HashSet<ObjectImpl*> allObjects;

	static GCGlobals& get()
	{
		static GCGlobals globals;
		return globals;
	}

private:
	GCGlobals() {}
};

Runtime::ObjectImpl::ObjectImpl(ObjectKind inKind) : Object(inKind), numRootReferences(0)
{
	// Add the object to the global array.
	Lock<Platform::Mutex> lock(GCGlobals::get().mutex);
	GCGlobals::get().allObjects.addOrFail(this);
}

void Runtime::addGCRoot(Object* object)
{
	ObjectImpl* gcObject = (ObjectImpl*)object;
	++gcObject->numRootReferences;
}

void Runtime::removeGCRoot(Object* object)
{
	ObjectImpl* gcObject = (ObjectImpl*)object;
	--gcObject->numRootReferences;
}

void Runtime::collectGarbage()
{
	GCGlobals& gcGlobals = GCGlobals::get();
	Lock<Platform::Mutex> lock(gcGlobals.mutex);
	Timing::Timer timer;

	HashSet<ObjectImpl*> referencedObjects;
	std::vector<Object*> pendingScanObjects;

	// Initialize the referencedObjects set from the rooted object set.
	Uptr numRoots = 0;
	for(ObjectImpl* object : gcGlobals.allObjects)
	{
		if(object && object->numRootReferences > 0)
		{
			referencedObjects.addOrFail(object);
			pendingScanObjects.push_back(object);
			++numRoots;
		}
	}

	// Scan the objects added to the referenced set so far: gather their child references and
	// recurse.
	while(pendingScanObjects.size())
	{
		Object* scanObject = pendingScanObjects.back();
		pendingScanObjects.pop_back();

		// Gather the child references for this object based on its kind.
		std::vector<Object*> childReferences;
		switch(scanObject->kind)
		{
		case ObjectKind::function:
		{
			FunctionInstance* function = asFunction(scanObject);
			childReferences.push_back(function->moduleInstance);
			break;
		}
		case ObjectKind::table:
		{
			TableInstance* table = asTable(scanObject);
			childReferences.push_back(table->compartment);
			childReferences.insert(
				childReferences.end(), table->elements.begin(), table->elements.end());
			break;
		}
		case ObjectKind::memory:
		{
			MemoryInstance* memory = asMemory(scanObject);
			childReferences.push_back(memory->compartment);
			break;
		}
		case ObjectKind::global:
		{
			GlobalInstance* global = asGlobal(scanObject);
			childReferences.push_back(global->compartment);
			break;
		}
		case ObjectKind::moduleInstance:
		{
			ModuleInstance* moduleInstance = asModuleInstance(scanObject);
			childReferences.push_back(moduleInstance->compartment);
			childReferences.insert(childReferences.begin(),
								   moduleInstance->functionDefs.begin(),
								   moduleInstance->functionDefs.end());
			childReferences.insert(childReferences.begin(),
								   moduleInstance->functions.begin(),
								   moduleInstance->functions.end());
			childReferences.insert(childReferences.begin(),
								   moduleInstance->tables.begin(),
								   moduleInstance->tables.end());
			childReferences.insert(childReferences.begin(),
								   moduleInstance->memories.begin(),
								   moduleInstance->memories.end());
			childReferences.insert(childReferences.begin(),
								   moduleInstance->globals.begin(),
								   moduleInstance->globals.end());
			childReferences.insert(childReferences.begin(),
								   moduleInstance->exceptionTypes.begin(),
								   moduleInstance->exceptionTypes.end());
			childReferences.push_back(moduleInstance->defaultMemory);
			childReferences.push_back(moduleInstance->defaultTable);
			break;
		}
		case ObjectKind::context:
		{
			Context* context = asContext(scanObject);
			childReferences.push_back(context->compartment);
			break;
		}
		case ObjectKind::compartment:
		{
			Compartment* compartment = asCompartment(scanObject);
			childReferences.push_back(compartment->wavmIntrinsics);
			break;
		}

		case ObjectKind::module:
		case ObjectKind::exceptionTypeInstance: break;

		default: Errors::unreachable();
		};

		// Add the object's child references to the referenced set, and enqueue them for
		// scanning.
		for(Object* reference : childReferences)
		{
			if(reference && referencedObjects.add((ObjectImpl*)reference))
			{ pendingScanObjects.push_back(reference); }
		}
	};

	// Find the objects that weren't reached, and call finalize on each of them.
	std::vector<ObjectImpl*> finalizedObjects;
	for(ObjectImpl* object : gcGlobals.allObjects)
	{
		if(!referencedObjects.contains(object))
		{
			object->finalize();
			finalizedObjects.push_back(object);
		}
	}
	gcGlobals.allObjects = std::move(referencedObjects);

	// Delete all the finalized objects.
	for(ObjectImpl* object : finalizedObjects) { delete object; }

	Log::printf(Log::metrics,
				"Collected garbage in %.2fms: %" PRIuPTR " roots, %" PRIuPTR " objects, %" PRIuPTR
				" garbage\n",
				timer.getMilliseconds(),
				numRoots,
				Uptr(gcGlobals.allObjects.size() + finalizedObjects.size()),
				Uptr(finalizedObjects.size()));
}
