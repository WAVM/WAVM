#ifdef __APPLE__
	#define C_NAME(name) _##name
	#define C_NAME_PLT(name) _##name
	#define TYPE(name,typeSpec) 
	#define SIZE(name)
#else
	#define C_NAME(name) name
	#define C_NAME_PLT(name) name@PLT
	#define TYPE(name,typeSpec) .type name, typeSpec;
	#define SIZE(name) .size name, .-name;
#endif

#define CFI_REG_RBX    3
#define CFI_REG_RDI    5
#define CFI_REG_RBP    6
#define CFI_REG_RSP    7
#define CFI_REG_R12   12
#define CFI_REG_R13   13
#define CFI_REG_R14   14
#define CFI_REG_R15   15
#define CFI_REG_RIP   16

#define DW_OP_deref .cfi_escape 0x06;
#define DW_OP_reg_plus_offset(reg,offset) .cfi_escape 0x70+reg, offset;

#define CFI_def_cfa_expression(length)     .cfi_escape 0x0f, length;
#define CFI_cfa_val_expression(reg,length) .cfi_escape 0x16, reg, length;

#define CFI_save_reg_at_reg_offset(savedReg,baseReg,offset) \
	CFI_cfa_val_expression(savedReg,3) \
		DW_OP_reg_plus_offset(baseReg,offset) \
		DW_OP_deref

#define BEGIN_FUNC(name,...) \
	.globl C_NAME(name); \
	TYPE(C_NAME(name),@function) \
	.align 16; \
	C_NAME(name):; \
	.cfi_startproc __VA_ARGS__;

#define END_FUNC(name) \
	.cfi_endproc; \
	SIZE(C_NAME(name))

/* If this flag isn't present, the linker will assume this object needs an executable stack. */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack, "", %progbits
#endif

.text

#define EXECUTION_CONTEXT_RBX 0
#define EXECUTION_CONTEXT_RSP 8
#define EXECUTION_CONTEXT_RBP 16
#define EXECUTION_CONTEXT_R12 24
#define EXECUTION_CONTEXT_R13 32
#define EXECUTION_CONTEXT_R14 40
#define EXECUTION_CONTEXT_R15 48
#define EXECUTION_CONTEXT_RIP 56
#define EXECUTION_CONTEXT_NUMBYTES 64

BEGIN_FUNC(saveExecutionStateImpl)
	/* Save the non-volatile registers and a RIP/RSP passed in RDX/RCX.  */
	movq %rbx, (EXECUTION_CONTEXT_RBX)(%rdi)
	movq %rcx, (EXECUTION_CONTEXT_RSP)(%rdi)
	movq %rbp, (EXECUTION_CONTEXT_RBP)(%rdi)
	movq %r12, (EXECUTION_CONTEXT_R12)(%rdi)
	movq %r13, (EXECUTION_CONTEXT_R13)(%rdi)
	movq %r14, (EXECUTION_CONTEXT_R14)(%rdi)
	movq %r15, (EXECUTION_CONTEXT_R15)(%rdi)
	movq %rdx, (EXECUTION_CONTEXT_RIP)(%rdi)
	movq %rsi, %rax
	ret
END_FUNC(saveExecutionStateImpl)

BEGIN_FUNC(saveExecutionState)
	/* Save the non-volatile registers and the RIP/RSP of the caller.  */
	movq (%rsp), %rdx
	lea 8(%rsp), %rcx
	jmp C_NAME_PLT(saveExecutionStateImpl)
END_FUNC(saveExecutionState)

BEGIN_FUNC(loadExecutionState)
	movq (EXECUTION_CONTEXT_RBX)(%rdi),%rbx
	movq (EXECUTION_CONTEXT_RSP)(%rdi),%rsp
	movq (EXECUTION_CONTEXT_RBP)(%rdi),%rbp
	movq (EXECUTION_CONTEXT_R12)(%rdi),%r12
	movq (EXECUTION_CONTEXT_R13)(%rdi),%r13
	movq (EXECUTION_CONTEXT_R14)(%rdi),%r14
	movq (EXECUTION_CONTEXT_R15)(%rdi),%r15
	movq (EXECUTION_CONTEXT_RIP)(%rdi),%rdx
	movq %rsi, %rax
	jmpq *%rdx
END_FUNC(loadExecutionState)

/*	This is never called, but is "returned to" at the bottom of a forked stack.
	The CFI definitions allow stack walking to find the pointer back to the
	forked thread's primordial stack (with the forkedThreadEntry frame). */
BEGIN_FUNC(forkedStackTrampoline,simple)
	CFI_def_cfa_expression(3)
		DW_OP_reg_plus_offset(CFI_REG_RSP,EXECUTION_CONTEXT_RSP)
		DW_OP_deref
	CFI_save_reg_at_reg_offset(CFI_REG_RBX,CFI_REG_RSP,EXECUTION_CONTEXT_RBX)
	CFI_save_reg_at_reg_offset(CFI_REG_RBP,CFI_REG_RSP,EXECUTION_CONTEXT_RBP)
	CFI_save_reg_at_reg_offset(CFI_REG_R12,CFI_REG_RSP,EXECUTION_CONTEXT_R12)
	CFI_save_reg_at_reg_offset(CFI_REG_R13,CFI_REG_RSP,EXECUTION_CONTEXT_R13)
	CFI_save_reg_at_reg_offset(CFI_REG_R14,CFI_REG_RSP,EXECUTION_CONTEXT_R14)
	CFI_save_reg_at_reg_offset(CFI_REG_R15,CFI_REG_RSP,EXECUTION_CONTEXT_R15)
	CFI_save_reg_at_reg_offset(CFI_REG_RIP,CFI_REG_RSP,EXECUTION_CONTEXT_RIP)
	/* DWARF-based unwinding subtracts 1 from the return address before looking up the FDE,
	   so there needs the be a phony prologue preceding the "return address" that's patched
	   into the stack. Additionally, libunwind seems to search for DWARF function infos with
	   start<pc, so a second nop is necessary for MacOS. */
	nop
	nop
forkedStackTrampolineExit:
	/* Upon return to the trampoline, rsp points to an ExecutionContext that
	   was set up in switchToForkedStackContext to return to that function's
	   caller. Load it, passing through the value returned to this trampoline
	   as the return value. */
	movq %rsp, %rdi
	movq %rax, %rsi
	jmp C_NAME_PLT(loadExecutionState)
END_FUNC(forkedStackTrampoline)

// extern "C" I64 switchToForkedStackContext(ExecutionContext* forkedContext,U8* trampolineFramePointer) noexcept(false);
BEGIN_FUNC(switchToForkedStackContext)
	sub $8, %rsp
	.cfi_adjust_cfa_offset 8

	/* Spill the forkedContext pointer to the stack. */
	movq %rdi, (%rsp)

	/* Replace the trampoline frame return address with forkedStackTrampolineExit. */
	lea forkedStackTrampolineExit(%rip), %rdx
	movq %rdx, -8(%rsi)

	/* Save the non-volatile registers and the RIP/RSP of the caller. */
	movq %rsi, %rdi
	movq 8(%rsp), %rdx
	lea 16(%rsp), %rcx
	call C_NAME_PLT(saveExecutionStateImpl)

	/* Load the forked context. */
	movq (%rsp), %rdi
	movq $1, %rsi
	jmp C_NAME_PLT(loadExecutionState)

END_FUNC(switchToForkedStackContext)

BEGIN_FUNC(getStackPointer)
	lea 8(%rsp), %rax
	ret
END_FUNC(getStackPointer)

BEGIN_FUNC(wavm_probe_stack)
	/* On entry, the number of bytes of stack being allocated is in %rax.
	   The allocation is relative to the caller's %rsp (i.e. %rsp+8).
	   The function is not allowed to clobber any registers, with the exception
	   of r11 as it's a scratch register in all calling conventions.
	   https://github.com/llvm/llvm-project/blob/7770f83d614c7216e4c1e83026955fbf73298f34/llvm/lib/Target/X86/X86FrameLowering.cpp#L784
	   */

	/* Negate the allocation size, initialize the probe offset to zero, and
	   jump to the first loop step. */
	neg %rax
	xor %r11, %r11
	jmp step

loop:
	/* The request to probe the stack is relative to %rsp in the caller, so
	   probe relative to %rsp+8 to cancel out the return address pushed by
	   calling wavm_probe_stack.
	   Use test to load from the address without clobbering a register with
	   the loaded value. */
	test %rsp, 8(%rsp,%r11)

step:
	/* Decrement the probe offset by 4096 until it is less than the negative
	   allocation size. */
	sub $4096, %r11

	cmp %rax, %r11
	jg loop         /* Note the reversed GAS argument order on the cmp, so this
	                   is actually jumping if %r11 >= %rax. */

	/* After probing to the greatest multiple of 4096 that is less than the
	   allocation size, do a final probe at the allocation size. */
	test %rsp, 8(%rsp, %rax)

	neg %rax
	ret

END_FUNC(wavm_probe_stack)
