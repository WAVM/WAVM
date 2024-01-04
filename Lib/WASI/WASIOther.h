WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"poll_oneoff",
									__wasi_errno_return_t,
									wasi_poll_oneoff,
									WASIAddressIPtr inAddress,
									WASIAddressIPtr outAddress,
									WASIAddressIPtr numSubscriptions,
									WASIAddressIPtr outNumEventsAddress)
{
	UNIMPLEMENTED_SYSCALL_IPTR("poll_oneoff",
							   "(" WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT
							   ", " WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
							   inAddress,
							   outAddress,
							   numSubscriptions,
							   outNumEventsAddress);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"proc_exit",
									void,
									wasi_proc_exit,
									__wasi_exitcode_t exitCode)
{
	TRACE_SYSCALL_IPTR("proc_exit", "(%u)", exitCode);
	throw ExitException{exitCode};
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"proc_raise",
									__wasi_errno_return_t,
									wasi_proc_raise,
									__wasi_signal_t sig)
{
	// proc_raise will possibly be removed: https://github.com/WebAssembly/WASI/issues/7
	UNIMPLEMENTED_SYSCALL_IPTR("proc_raise", "(%u)", sig);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"random_get",
									__wasi_errno_return_t,
									wasi_random_get,
									WASIAddressIPtr bufferAddress,
									WASIAddressIPtr numBufferBytes)
{
	TRACE_SYSCALL_IPTR("random_get",
					   "(" WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
					   bufferAddress,
					   numBufferBytes);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_errno_t result = __WASI_ESUCCESS;
	Runtime::catchRuntimeExceptions(
		[&] {
			U8* buffer = memoryArrayPtr<U8>(process->memory, bufferAddress, numBufferBytes);
			Platform::getCryptographicRNG(buffer, numBufferBytes);
		},
		[&](Runtime::Exception* exception) {
			WAVM_ASSERT(getExceptionType(exception) == ExceptionTypes::outOfBoundsMemoryAccess);
			result = __WASI_EFAULT;
		});

	return TRACE_SYSCALL_RETURN(result);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"sock_recv",
									__wasi_errno_return_t,
									wasi_sock_recv,
									__wasi_fd_t sock,
									WASIAddressIPtr ri_data,
									WASIAddressIPtr ri_data_len,
									__wasi_riflags_t ri_flags,
									WASIAddressIPtr ro_datalen,
									WASIAddressIPtr ro_flags)
{
	UNIMPLEMENTED_SYSCALL_IPTR("sock_recv",
							   "(%u, " WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT
							   ", 0x%04x, " WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
							   sock,
							   ri_data,
							   ri_data_len,
							   ri_flags,
							   ro_datalen,
							   ro_flags);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"sock_send",
									__wasi_errno_return_t,
									wasi_sock_send,
									__wasi_fd_t sock,
									WASIAddressIPtr si_data,
									WASIAddressIPtr si_data_len,
									__wasi_siflags_t si_flags,
									WASIAddressIPtr so_datalen)
{
	UNIMPLEMENTED_SYSCALL_IPTR("sock_send",
							   "(%u, " WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT
							   ", 0x%04x, " WASIADDRESSIPTR_FORMAT ")",
							   sock,
							   si_data,
							   si_data_len,
							   si_flags,
							   so_datalen);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi,
									"sock_shutdown",
									__wasi_errno_return_t,
									wasi_sock_shutdown,
									__wasi_fd_t sock,
									__wasi_sdflags_t how)
{
	UNIMPLEMENTED_SYSCALL_IPTR("sock_shutdown", "(%u, 0x%02x)", sock, how);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasi, "sched_yield", __wasi_errno_return_t, wasi_sched_yield)
{
	TRACE_SYSCALL_IPTR("sched_yield", "()");
	Platform::yieldToAnotherThread();
	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}