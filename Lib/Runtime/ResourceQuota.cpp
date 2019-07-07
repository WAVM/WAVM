#include <memory>
#include "RuntimePrivate.h"
#include "WAVM/Inline/Lock.h"
#include "WAVM/Platform/Mutex.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::Runtime;

ResourceQuotaRef Runtime::createResourceQuota() { return std::make_shared<ResourceQuota>(); }

Uptr Runtime::getResourceQuotaMaxTableElems(ResourceQuotaConstRefParam resourceQuota)
{
	Lock<Platform::Mutex> lock(resourceQuota->mutex);
	return resourceQuota->tableElems.max;
}

Uptr Runtime::getResourceQuotaCurrentTableElems(ResourceQuotaConstRefParam resourceQuota)
{
	Lock<Platform::Mutex> lock(resourceQuota->mutex);
	return resourceQuota->tableElems.current;
}

void Runtime::setResourceQuotaMaxTableElems(ResourceQuotaRefParam resourceQuota, Uptr maxTableElems)
{
	Lock<Platform::Mutex> lock(resourceQuota->mutex);
	resourceQuota->tableElems.max = maxTableElems;
}

Uptr Runtime::getResourceQuotaMaxMemoryPages(ResourceQuotaConstRefParam resourceQuota)
{
	Lock<Platform::Mutex> lock(resourceQuota->mutex);
	return resourceQuota->memoryPages.max;
}

Uptr Runtime::getResourceQuotaCurrentMemoryPages(ResourceQuotaConstRefParam resourceQuota)
{
	Lock<Platform::Mutex> lock(resourceQuota->mutex);
	return resourceQuota->memoryPages.current;
}

void Runtime::setResourceQuotaMaxMemoryPages(ResourceQuotaRefParam resourceQuota,
											 Uptr maxMemoryPages)
{
	Lock<Platform::Mutex> lock(resourceQuota->mutex);
	resourceQuota->memoryPages.max = maxMemoryPages;
}
