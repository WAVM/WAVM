# Hardware trap handling

WAVM uses hardware trap handling to implement WebAssembly's semantics efficiently. The primary 
use is for bounds checking of the address operands of WebAssembly's memory instructions. WAVM
reserves enough virtual address space that any virtual address accessed by a WebAssembly memory
instruction will be within the reserved address space. This allows the WebAssembly memory
instructions to be translated directly to hardware memory accesses without explicitly checking
that the effective address is within the bounds of the WebAssembly memory. Because WebAssembly's
effective addresses are generated from a 32-bit address and a 32-bit offset, this means WAVM must
reserve a little more than 8GB of virtual address space for each WebAssembly linear memory object.
The portion of that reserved address space that corresponds to effective addresses outside the
bounds of the WebAssembly memory will be mapped as protected, so any access to it will trigger a
hardware trap.

WAVM also uses hardware trap handling to handle stack overflow, as well as some boundary conditions
for floating-point conversion operations.

# Portability

WAVM supports POSIX-compliant systems and Windows as host OSes, which provide different mechanisms
for how a user-mode program may handle hardware traps.

## POSIX

On POSIX-compliant systems, a process may register a function as a handler for various signals,
some of which correspond to different kinds of hardware traps.

When a hardware trap occurs, the kernel mode interrupt handler executes. It sets up a signal
handler frame on top of the user-mode stack: this includes the program execution state at the point
where it was interrupted. It also pushes a phony return address that points to the "signal
trampoline" function, and resumes the user mode process at the start of the signal handler
function. When the signal handler function returns, it returns to the signal trampoline function.
The signal handler function reads the registers and other state saved before calling the signal
handler from the signal frame, and restores it, returning to the code that was executing before the
signal handler was called.

## Windows

Windows uses Structured Exception Handling (SEH) to handle a hardware trap. SEH is similar in
concept to C++ exception handling, but provides lower-level functionality that can be used to
implement both C++ exception handling and hardware trap handling.

SEH is supported through some C++ extensions: `__try` and `__except` equivalents to the C++ `try`
and `catch`. However, unlike the C++ `catch`, which declares an C++ exception type to catch, the
SEH `__except` lets you write "filter" code to determine whether to execute the handler:
```
LONG filter(EXCEPTION_POINTERS*);

__try
{
	<code that raises an exception>
}
__except(filter(GetExceptionInformation()))
{
	// This handler will execute if filter(GetExceptionInformation()) returns 1.
}
```
You can simply write a boolean expression in place of calling a filter function, but the SEH filter
function is a useful idiom for clarity and debugging.

Windows SEH also provides a `__finally` clause that can be used to declare a handler that
unconditionally executes when unwinding the stack.

When a trap occurs, Windows dispatches it to a function that is called on top of the user-mode
stack, similar how a POSIX system would call a signal handler. That function walks the stack from
top to bottom, and for each function that has a SEH `__except`, it evaluates the corresponding
filter code. The filter code looks at the properties of the exception, and returns a code that
indicates whether to continue searching for a handler, to execute the handler corresponding to this
filter function, or to continue executing the code that caused the exception.

If it returns the code that indicates to execute the handler, then the dispatcher will unwind the
stack to the function containing the handler, and "return" to the handler. To unwind the stack, it
executes all the `__finally` handlers that occur above the target handler on the stack, as well as
destructors for C++ locals (which the compiler turns into a different kind of SEH handler).

## Stack overflow

Another difference between Windows SEH and POSIX signals is relevant to WAVM: how stack overflow is
handled. Both signals and SEH require space on the user-mode stack, so some special support for
handling stack overflow is necessary.

On Windows, WAVM uses a function called `SetThreadStackGuarantee` to reserve some extra space on
top of the stack for dispatching SEH. The function simply protects some space at the top of the
stack, allowing a stack overflow exception to be triggered before the stack is completely
exhaused. If such a "soft" stack overflow occurs, the protected space is made unprotected, and
the exception is dispatched using the newly expanded stack space. After handling the stack
overflow exception, `_resetstkoflw` is called to reprotect that space.

On POSIX, a function called `sigaltstack` is used to designate an alternative stack to use when
executing a signal handler. Signal handlers that are registered with the `SA_ONSTACK` flag will be
called on this alternative stack, so when the main stack overflows, the handler will still work.
When the handler returns, the signal trampoline switches back to the main stack before resuming
execution of the code that was interrupted by the signal handler.

## C++ exceptions

Unlike POSIX signals and Windows SEH, C++ exceptions are completely portable, but provide different
functionality. They can only be thrown by explicit `throw` statements, and there isn't an
opportunity to handle the exception without unwinding the stack, as there is with signal handlers
and SEH filters. They cannot be used to handle hardware traps without a Windows-specific C++
extension, but it is useful to understand how they interact with destructors and SEH/signals.

Linux and MacOS use a two-phase dispatch to implement C++ exceptions that is similar to SEH
unwinding: a "personality function" is called for each function on the stack, from top to bottom,
until the personality function returns a value that indicates the function handles the exception.
This first phase is similar to the phase where SEH evaluates filters to find a handler.

In the second phase, the personality functions are then called again for each function on the
stack, from the top down to the function that will handle the exception, but with different
parameters that indicate the personality function should unwind the function's stack frame
(i.e. call destructors for local variables).

On Windows, C++ exceptions are dispatched as a specific kind of SEH exception. `catch` statements
translate to a handler with a filter that rejects any non-C++ SEH exceptions, and any C++ SEH
exceptions that have a different C++ type from the exception type declared by the `catch`.

C++ exception handling has an effect on the generated code for functions even when they don't seem
to `throw` or `catch` an exception. Since C++ classes may have destructors, a local variable that
has a destructor will implicitly require a limited form of exception handling to call the variable's
destructor if any function that may throw an exception is called while the variable is alive.

On Windows, local destructors are called when unwinding C++ exceptions, but not other kinds of SEH
exceptions.

## SEH vs signals vs C++ exceptions

SEH lets you write code to act on an exception in two phases: the filter function, before the stack
has been unwound, and the handler, after the stack has been unwound. Raising a non-C++ SEH
exception doesn't call C++ destructors for local variables between the top of the stack and the
handler function.

C++ exceptions let you write code to act on an exception in the equivalent of the SEH second phase,
after the stack above the handler has been unwound.

And because signal handling does not include any stack unwinding functionality, it only allows you
to write code that executes on the top of the stack, equivalent to the first phase of SEH.

# Runtime exceptions

WebAssembly is specified to trap in various scenarios: e.g. accessing memory with an out-of-bounds
address, or performing an invalid numeric conversion. Trapping is
[specified](https://webassembly.github.io/spec/core/exec/runtime.html#syntax-trap) to abort
execution of the WebAssembly program, with the details of how it may be handled left up to the
WebAssembly embedder.

WAVM exposes these WebAssembly-specified traps as C++ exceptions of the `Runtime::Exception` type.
If you call `Runtime::invokeFunction`, and a trap occurs in the invoked function, it will throw a
`Runtime::Exception` that you can catch.

# Accessing WebAssembly memory from C/C++ code

Accessing WebAssembly memory from C/C++ code is nearly as simple as any other memory access, but
with some caveats for out-of-bounds accesses.

It isn't practical to do explicit bounds checking of those accesses, since the memory size or page
protections may be changed by another thread in between the bounds check and the memory access.
To avoid a race condition, you should only check that the address is within the reserved virtual
address space for the WebAssembly memory, and let the access trap to detect when the address was
out-of-bounds.