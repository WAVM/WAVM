#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "WAVM/IR/Value.h"

namespace WAVM { namespace IR {
	struct Module;
}}

namespace WAVM { namespace VFS {
	struct VFD;
}}

namespace WAVM { namespace Runtime {
	struct Compartment;
	struct Context;
	struct ModuleInstance;
	struct Resolver;
}}

namespace WAVM { namespace Emscripten {

	struct Instance;

	WAVM_API std::shared_ptr<Instance> instantiate(Runtime::Compartment* compartment,
												   VFS::VFD* stdIn = nullptr,
												   VFS::VFD* stdOut = nullptr,
												   VFS::VFD* stdErr = nullptr);
	WAVM_API bool initializeModuleInstance(const std::shared_ptr<Instance>& instance,
										   Runtime::Context* context,
										   const IR::Module& module,
										   Runtime::ModuleInstance* moduleInstance);
	WAVM_API std::vector<IR::Value> injectCommandArgs(const std::shared_ptr<Instance>& instance,
													  Runtime::Context* context,
													  const std::vector<std::string>& argStrings);

	WAVM_API Runtime::Resolver& getInstanceResolver(const std::shared_ptr<Instance>& instance);

	WAVM_API void joinAllThreads(Instance* instance);

	WAVM_API I32 catchExit(std::function<I32()>&& thunk);
}}
