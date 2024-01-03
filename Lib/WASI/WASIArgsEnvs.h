WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasiArgsEnvs,
									"args_sizes_get",
									__wasi_errno_return_t,
									wasi_args_sizes_get,
									WASIAddressIPtr argcAddress,
									WASIAddressIPtr argBufSizeAddress)
{
	TRACE_SYSCALL_IPTR("args_sizes_get",
					   "(" WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
					   argcAddress,
					   argBufSizeAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numArgBufferBytes = 0;
	for(const std::string& arg : process->args) { numArgBufferBytes += arg.size() + 1; }

	if(process->args.size() > WASIADDRESSIPTR_MAX || numArgBufferBytes > WASIADDRESSIPTR_MAX)
	{
		return TRACE_SYSCALL_RETURN(__WASI_EOVERFLOW);
	}
	memoryRef<WASIAddressIPtr>(process->memory, argcAddress)
		= WASIAddressIPtr(process->args.size());
	memoryRef<WASIAddressIPtr>(process->memory, argBufSizeAddress)
		= WASIAddressIPtr(numArgBufferBytes);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasiArgsEnvs,
									"args_get",
									__wasi_errno_return_t,
									wasi_args_get,
									WASIAddressIPtr argvAddress,
									WASIAddressIPtr argBufAddress)
{
	TRACE_SYSCALL_IPTR("args_get",
					   "(" WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
					   argvAddress,
					   argBufAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASIAddressIPtr nextArgBufAddress = argBufAddress;
	for(Uptr argIndex = 0; argIndex < process->args.size(); ++argIndex)
	{
		const std::string& arg = process->args[argIndex];
		const Uptr numArgBytes = arg.size() + 1;

		if(numArgBytes > WASIADDRESSIPTR_MAX
		   || nextArgBufAddress > WASIADDRESSIPTR_MAX - numArgBytes - 1)
		{
			return TRACE_SYSCALL_RETURN(__WASI_EOVERFLOW);
		}

		if(numArgBytes > 0)
		{
			memcpy(memoryArrayPtr<U8>(process->memory, nextArgBufAddress, numArgBytes),
				   (const U8*)arg.c_str(),
				   numArgBytes);
		}
		memoryRef<WASIAddressIPtr>(process->memory,
								   argvAddress + argIndex * sizeof(WASIAddressIPtr))
			= WASIAddressIPtr(nextArgBufAddress);

		nextArgBufAddress += WASIAddressIPtr(numArgBytes);
	}

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasiArgsEnvs,
									"environ_sizes_get",
									__wasi_errno_return_t,
									wasi_environ_sizes_get,
									WASIAddressIPtr envCountAddress,
									WASIAddressIPtr envBufSizeAddress)
{
	TRACE_SYSCALL_IPTR("environ_sizes_get",
					   "(" WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
					   envCountAddress,
					   envBufSizeAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Uptr numEnvBufferBytes = 0;
	for(const std::string& env : process->envs) { numEnvBufferBytes += env.size() + 1; }

	if(process->envs.size() > WASIADDRESSIPTR_MAX || numEnvBufferBytes > WASIADDRESSIPTR_MAX)
	{
		return TRACE_SYSCALL_RETURN(__WASI_EOVERFLOW);
	}
	memoryRef<WASIAddressIPtr>(process->memory, envCountAddress)
		= WASIAddressIPtr(process->envs.size());
	memoryRef<WASIAddressIPtr>(process->memory, envBufSizeAddress)
		= WASIAddressIPtr(numEnvBufferBytes);

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasiArgsEnvs,
									"environ_get",
									__wasi_errno_return_t,
									wasi_environ_get,
									WASIAddressIPtr envvAddress,
									WASIAddressIPtr envBufAddress)
{
	TRACE_SYSCALL_IPTR("environ_get",
					   "(" WASIADDRESSIPTR_FORMAT ", " WASIADDRESSIPTR_FORMAT ")",
					   envvAddress,
					   envBufAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	WASIAddressIPtr nextEnvBufAddress = envBufAddress;
	for(Uptr argIndex = 0; argIndex < process->envs.size(); ++argIndex)
	{
		const std::string& env = process->envs[argIndex];
		const Uptr numEnvBytes = env.size() + 1;

		if(numEnvBytes > WASIADDRESSIPTR_MAX
		   || nextEnvBufAddress > WASIADDRESSIPTR_MAX - numEnvBytes - 1)
		{
			return TRACE_SYSCALL_RETURN(__WASI_EOVERFLOW);
		}

		if(numEnvBytes > 0)
		{
			memcpy(memoryArrayPtr<U8>(process->memory, nextEnvBufAddress, numEnvBytes),
				   (const U8*)env.c_str(),
				   numEnvBytes);
		}
		memoryRef<WASIAddressIPtr>(process->memory,
								   envvAddress + argIndex * sizeof(WASIAddressIPtr))
			= WASIAddressIPtr(nextEnvBufAddress);

		nextEnvBufAddress += WASIAddressIPtr(numEnvBytes);
	}

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS);
}
