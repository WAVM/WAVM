#include <inttypes.h>
#include <atomic>
#include <utility>
#include <vector>

#include "RuntimePrivate.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Hash.h"
#include "WAVM/Inline/HashSet.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Logging/Logging.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::Runtime;

Runtime::GCObject::GCObject(ObjectKind inKind, Compartment* inCompartment)
: Object{inKind}, compartment(inCompartment)
{
}

void Runtime::addGCRoot(Object* object)
{
	if(object->kind == ObjectKind::function)
	{
		Function* function = (Function*)object;
		wavmAssert(function->mutableData);
		++function->mutableData->numRootReferences;
	}
	else
	{
		GCObject* gcObject = (GCObject*)object;
		++gcObject->numRootReferences;
	}
}

void Runtime::removeGCRoot(Object* object)
{
	if(object->kind == ObjectKind::function)
	{
		Function* function = (Function*)object;
		wavmAssert(function->mutableData);
		--function->mutableData->numRootReferences;
	}
	else
	{
		GCObject* gcObject = (GCObject*)object;
		--gcObject->numRootReferences;
	}
}

struct GCState
{
	Compartment* compartment;
	HashSet<GCObject*> unreferencedObjects;
	std::vector<GCObject*> pendingScanObjects;

	GCState(Compartment* inCompartment) : compartment(inCompartment) {}

	void visitReference(Object* object)
	{
		if(object)
		{
			if(object->kind == ObjectKind::function)
			{
				Function* function = asFunction(object);
				if(function->moduleInstanceId != UINTPTR_MAX)
				{
					wavmAssert(compartment->moduleInstances.contains(function->moduleInstanceId));
					ModuleInstance* moduleInstance
						= compartment->moduleInstances[function->moduleInstanceId];
					visitReference(moduleInstance);
				}
			}
			else if(unreferencedObjects.remove((GCObject*)object))
			{
				pendingScanObjects.push_back((GCObject*)object);
			}
		}
	}

	template<typename Array> void visitReferenceArray(const Array& array)
	{
		for(auto reference : array) { visitReference(asObject(reference)); }
	}

	void initGCObject(GCObject* object, bool forceRoot = false)
	{
		if(forceRoot || object->numRootReferences > 0) { pendingScanObjects.push_back(object); }
		else
		{
			unreferencedObjects.add(object);
		}
	}

	void scanObject(GCObject* object)
	{
		wavmAssert(!object->compartment || object->compartment == compartment);
		visitReference(object->compartment);

		// Gather the child references for this object based on its kind.
		switch(object->kind)
		{
		case ObjectKind::table:
		{
			Table* table = asTable(object);

			Lock<Platform::Mutex> resizingLock(table->resizingMutex);
			const Uptr numElements = getTableNumElements(table);
			for(Uptr elementIndex = 0; elementIndex < numElements; ++elementIndex)
			{ visitReference(getTableElement(table, elementIndex)); }
			break;
		}
		case ObjectKind::global:
		{
			Global* global = asGlobal(object);
			if(isReferenceType(global->type.valueType))
			{
				if(global->type.isMutable)
				{
					visitReference(
						compartment->initialContextMutableGlobals[global->mutableGlobalIndex]
							.object);
					for(Context* context : compartment->contexts)
					{
						visitReference(
							context->runtimeData->mutableGlobals[global->mutableGlobalIndex]
								.object);
					}
				}
				visitReference(global->initialValue.object);
			}
			break;
		}
		case ObjectKind::moduleInstance:
		{
			ModuleInstance* moduleInstance = asModuleInstance(object);
			visitReferenceArray(moduleInstance->functions);
			visitReferenceArray(moduleInstance->tables);
			visitReferenceArray(moduleInstance->memories);
			visitReferenceArray(moduleInstance->globals);
			visitReferenceArray(moduleInstance->exceptionTypes);

			{
				Lock<Platform::Mutex> passiveElemSegmentLock(
					moduleInstance->passiveElemSegmentsMutex);
				for(const auto& passiveElemSegmentPair : moduleInstance->passiveElemSegments)
				{ visitReferenceArray(*passiveElemSegmentPair.value); }
			}

			break;
		}
		case ObjectKind::compartment:
		{
			wavmAssert(object == compartment);
			break;
		}

		case ObjectKind::memory:
		case ObjectKind::exceptionType:
		case ObjectKind::context: break;

		case ObjectKind::function:
		default: Errors::unreachable();
		};
	}
};

static bool collectGarbageImpl(Compartment* compartment)
{
	Lock<Platform::Mutex> compartmentLock(compartment->mutex);
	Timing::Timer timer;

	GCState state(compartment);

	// Initialize the GC state from the compartment's various sets of objects.
	state.initGCObject(compartment);
	for(ModuleInstance* moduleInstance : compartment->moduleInstances)
	{
		// Transfer root markings from functions to their module instance.
		bool hasRootFunction = false;
		for(Function* function : moduleInstance->functions)
		{
			if(function->mutableData->numRootReferences)
			{
				hasRootFunction = true;
				break;
			}
		}

		state.initGCObject(moduleInstance, hasRootFunction);
	}
	for(Memory* memory : compartment->memories) { state.initGCObject(memory); }
	for(Table* table : compartment->tables) { state.initGCObject(table); }
	for(ExceptionType* exceptionType : compartment->exceptionTypes)
	{ state.initGCObject(exceptionType); }
	for(Global* global : compartment->globals) { state.initGCObject(global); }
	for(Context* context : compartment->contexts) { state.initGCObject(context); }

	// Scan the objects added to the referenced set so far: gather their child references and
	// recurse.
	const Uptr numInitialObjects
		= state.pendingScanObjects.size() + state.unreferencedObjects.size();
	const Uptr numRoots = state.pendingScanObjects.size();
	while(state.pendingScanObjects.size())
	{
		GCObject* object = state.pendingScanObjects.back();
		state.pendingScanObjects.pop_back();
		state.scanObject(object);
	};

	// Delete each unreferenced object that isn't the compartment.
	bool wasCompartmentUnreferenced = false;
	for(GCObject* object : state.unreferencedObjects)
	{
		if(object == compartment) { wasCompartmentUnreferenced = true; }
		else
		{
			delete object;
		}
	}

	// Delete the compartment last, if it wasn't referenced.
	compartmentLock.unlock();
	if(wasCompartmentUnreferenced) { delete compartment; }

	Log::printf(Log::metrics,
				"Collected garbage in %.2fms: %" PRIuPTR " roots, %" PRIuPTR " objects, %" PRIuPTR
				" garbage\n",
				timer.getMilliseconds(),
				numRoots,
				numInitialObjects,
				Uptr(state.unreferencedObjects.size()));

	return wasCompartmentUnreferenced;
}

void Runtime::collectCompartmentGarbage(Compartment* compartment)
{
	collectGarbageImpl(compartment);
}

bool Runtime::tryCollectCompartment(GCPointer<Compartment>&& compartmentRootRef)
{
	Compartment* compartment = &*compartmentRootRef;
	compartmentRootRef = nullptr;
	return collectGarbageImpl(compartment);
}
