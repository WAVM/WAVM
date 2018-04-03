.code

EXECUTION_CONTEXT_RIP       EQU 0
EXECUTION_CONTEXT_CS        EQU 8
EXECUTION_CONTEXT_RFLAGS    EQU 16
EXECUTION_CONTEXT_RSP       EQU 24
EXECUTION_CONTEXT_SS        EQU 32
EXECUTION_CONTEXT_R12       EQU 40
EXECUTION_CONTEXT_R13       EQU 48
EXECUTION_CONTEXT_R14       EQU 56
EXECUTION_CONTEXT_R15       EQU 64
EXECUTION_CONTEXT_RDI       EQU 72
EXECUTION_CONTEXT_RSI       EQU 80
EXECUTION_CONTEXT_RBX       EQU 88
EXECUTION_CONTEXT_RBP       EQU 96
EXECUTION_CONTEXT_XMM6      EQU 112
EXECUTION_CONTEXT_XMM7      EQU 128
EXECUTION_CONTEXT_XMM8      EQU 144
EXECUTION_CONTEXT_XMM9      EQU 160
EXECUTION_CONTEXT_XMM10     EQU 176
EXECUTION_CONTEXT_XMM11     EQU 192
EXECUTION_CONTEXT_XMM12     EQU 208
EXECUTION_CONTEXT_XMM13     EQU 224
EXECUTION_CONTEXT_XMM14     EQU 240
EXECUTION_CONTEXT_XMM15     EQU 256

saveExecutionStateImpl PROC
	pushfq
	pop r10
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RIP],   r8
	mov       WORD PTR [rcx + EXECUTION_CONTEXT_CS],    cs
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RFLAGS],r10
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RSP],   r9
	mov       WORD PTR [rcx + EXECUTION_CONTEXT_SS],    ss
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_R12],   r12
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_R13],   r13
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_R14],   r14
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_R15],   r15
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RDI],   rdi
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RSI],   rsi
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RBX],   rbx
	mov      QWORD PTR [rcx + EXECUTION_CONTEXT_RBP],   rbp
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM6],  xmm6
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM7],  xmm7
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM8],  xmm8
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM9],  xmm9
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM10], xmm10
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM11], xmm11
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM12], xmm12
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM13], xmm13
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM14], xmm14
	movdqa XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM15], xmm15
	mov rax, rdx
	ret
saveExecutionStateImpl ENDP

; extern "C" I64 saveExecutionState(ExecutionContext* outContext,I64 returnCode);
saveExecutionState PROC
	mov r8, QWORD PTR [rsp]
	lea r9, [rsp+8]
	jmp saveExecutionStateImpl
saveExecutionState ENDP

; extern "C" [[noreturn]] void loadExecutionState(ExecutionContext* context,I64 returnCode);
loadExecutionState PROC
	; We save the registers for the benefits of the .PUSHFRAME in forkedStackTrampoline, but can't load them.
	;mov    cs,       WORD PTR [rcx + EXECUTION_CONTEXT_CS]
	;mov    ss,       WORD PTR [rcx + EXECUTION_CONTEXT_SS]
	mov    r10,     QWORD PTR [rcx + EXECUTION_CONTEXT_RIP]
	mov    r11,     QWORD PTR [rcx + EXECUTION_CONTEXT_RFLAGS]
	mov    rsp,     QWORD PTR [rcx + EXECUTION_CONTEXT_RSP]
	mov    r12,     QWORD PTR [rcx + EXECUTION_CONTEXT_R12]
	mov    r13,     QWORD PTR [rcx + EXECUTION_CONTEXT_R13]
	mov    r14,     QWORD PTR [rcx + EXECUTION_CONTEXT_R14]
	mov    r15,     QWORD PTR [rcx + EXECUTION_CONTEXT_R15]
	mov    rdi,     QWORD PTR [rcx + EXECUTION_CONTEXT_RDI]
	mov    rsi,     QWORD PTR [rcx + EXECUTION_CONTEXT_RSI]
	mov    rbx,     QWORD PTR [rcx + EXECUTION_CONTEXT_RBX]
	mov    rbp,     QWORD PTR [rcx + EXECUTION_CONTEXT_RBP]
	movdqa xmm6,  XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM6]
	movdqa xmm7,  XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM7]
	movdqa xmm8,  XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM8]
	movdqa xmm9,  XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM9]
	movdqa xmm10, XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM10]
	movdqa xmm11, XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM11]
	movdqa xmm12, XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM12]
	movdqa xmm13, XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM13]
	movdqa xmm14, XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM14]
	movdqa xmm15, XMMWORD PTR [rcx + EXECUTION_CONTEXT_XMM15]
	push r11
	popfq
	mov rax, rdx
	jmp r10
loadExecutionState ENDP

forkedStackTrampoline PROC FRAME
	.PUSHFRAME
	.SAVEREG    r12,   EXECUTION_CONTEXT_R12
	.SAVEREG    r13,   EXECUTION_CONTEXT_R13
	.SAVEREG    r14,   EXECUTION_CONTEXT_R14
	.SAVEREG    r15,   EXECUTION_CONTEXT_R15
	.SAVEREG    rdi,   EXECUTION_CONTEXT_RDI
	.SAVEREG    rsi,   EXECUTION_CONTEXT_RSI
	.SAVEREG    rbx,   EXECUTION_CONTEXT_RBX
	.SAVEREG    rbp,   EXECUTION_CONTEXT_RBP
	.SAVEXMM128 xmm6,  EXECUTION_CONTEXT_XMM6
	.SAVEXMM128 xmm7,  EXECUTION_CONTEXT_XMM7
	.SAVEXMM128 xmm8,  EXECUTION_CONTEXT_XMM8
	.SAVEXMM128 xmm9,  EXECUTION_CONTEXT_XMM9
	.SAVEXMM128 xmm10, EXECUTION_CONTEXT_XMM10
	.SAVEXMM128 xmm11, EXECUTION_CONTEXT_XMM11
	.SAVEXMM128 xmm12, EXECUTION_CONTEXT_XMM12
	.SAVEXMM128 xmm13, EXECUTION_CONTEXT_XMM13
	.SAVEXMM128 xmm14, EXECUTION_CONTEXT_XMM14
	.SAVEXMM128 xmm15, EXECUTION_CONTEXT_XMM15
	.ENDPROLOG
forkedStackTrampolineEntry::
	mov rcx, rsp
	mov rdx, rax
	jmp loadExecutionState
forkedStackTrampoline ENDP

; extern "C" I64 switchToForkedStackContext(ExecutionContext* forkedContext,U8* trampolineFramePointer) noexcept(false);
switchToForkedStackContext PROC FRAME
	sub rsp, 8
	.ALLOCSTACK 8
	.ENDPROLOG

	mov r10, forkedStackTrampolineEntry
	mov QWORD PTR [rdx-8], r10
	
	mov [rsp+16], rcx
	mov rcx, rdx
	mov r8, return
	mov r9, rsp
	call saveExecutionStateImpl

	mov rcx, [rsp+16]
	mov rdx, 1
	jmp loadExecutionState

return:
	add rsp, 8
	ret
switchToForkedStackContext ENDP

getStackPointer PROC
	lea rax, [rsp+8]
	ret
getStackPointer ENDP

End
