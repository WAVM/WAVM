#include <inttypes.h>
#include <stdio.h>
#include "WAVM/IR/Types.h"
#include "WAVM/Runtime/Intrinsics.h"
#include "WAVM/Runtime/Runtime.h"

using namespace WAVM;
using namespace WAVM::IR;
using namespace WAVM::Runtime;

WAVM_DEFINE_INTRINSIC_MODULE(embedder)
WAVM_DEFINE_INTRINSIC_FUNCTION(embedder, "hello", void, hello) { printf("Hello world!\n"); }

int main(int argc, char** argv)
{
	U8 helloWASM[]
		= {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x84, 0x80, 0x80, 0x80, 0x00, 0x01,
		   0x60, 0x00, 0x00, 0x02, 0x8a, 0x80, 0x80, 0x80, 0x00, 0x01, 0x00, 0x05, 0x68, 0x65, 0x6c,
		   0x6c, 0x6f, 0x00, 0x00, 0x03, 0x82, 0x80, 0x80, 0x80, 0x00, 0x01, 0x00, 0x07, 0x87, 0x80,
		   0x80, 0x80, 0x00, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01, 0x0a, 0x8a, 0x80, 0x80, 0x80,
		   0x00, 0x01, 0x84, 0x80, 0x80, 0x80, 0x00, 0x00, 0x10, 0x00, 0x0b};

	ModuleRef module;
	if(!loadBinaryModule(helloWASM, sizeof(helloWASM), module)) { return EXIT_FAILURE; }

	GCPointer<Compartment> compartment = createCompartment();
	Context* context = createContext(compartment);

	Instance* intrinsicsInstance = WAVM::Intrinsics::instantiateModule(
		compartment, {WAVM_INTRINSIC_MODULE_REF(embedder)}, "embedder");
	Function* intrinsicFunction
		= getTypedInstanceExport(intrinsicsInstance, "hello", FunctionType());

	Instance* instance
		= instantiateModule(compartment, module, {asObject(intrinsicFunction)}, "hello");
	Function* runFunction = getTypedInstanceExport(instance, "run", FunctionType());

	invokeFunction(context, runFunction, FunctionType());

	tryCollectCompartment(std::move(compartment));

	return 0;
}
