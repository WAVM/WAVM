#pragma once

namespace WAVM { namespace Runtime {
	struct Compartment;
	struct ModuleInstance;
}}

namespace WAVM { namespace ThreadTest {
	WAVM_API Runtime::ModuleInstance* instantiate(Runtime::Compartment* compartment);
}}
