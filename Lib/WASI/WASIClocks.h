WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasiClocks,
									"clock_res_get",
									__wasi_errno_return_t,
									__wasi_clock_res_get,
									__wasi_clockid_t clockId,
									WASIAddressIPtr resolutionAddress)
{
	TRACE_SYSCALL_IPTR(
		"clock_res_get", "(%u, " WASIADDRESSIPTR_FORMAT ")", clockId, resolutionAddress);

	Platform::Clock platformClock;
	if(!getPlatformClock(clockId, platformClock)) { return TRACE_SYSCALL_RETURN(__WASI_EINVAL); }

	const Time clockResolution = Platform::getClockResolution(platformClock);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	__wasi_timestamp_t wasiClockResolution = __wasi_timestamp_t(clockResolution.ns);
	memoryRef<__wasi_timestamp_t>(process->memory, resolutionAddress) = wasiClockResolution;

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS, "(%" PRIu64 ")", wasiClockResolution);
}

WAVM_DEFINE_INTRINSIC_FUNCTION_IPTR(wasiClocks,
									"clock_time_get",
									__wasi_errno_return_t,
									__wasi_clock_time_get,
									__wasi_clockid_t clockId,
									__wasi_timestamp_t precision,
									WASIAddressIPtr timeAddress)
{
	TRACE_SYSCALL_IPTR("clock_time_get",
					   "(%u, %" PRIu64 ", " WASIADDRESSIPTR_FORMAT ")",
					   clockId,
					   precision,
					   timeAddress);

	Process* process = getProcessFromContextRuntimeData(contextRuntimeData);

	Platform::Clock platformClock;
	if(!getPlatformClock(clockId, platformClock)) { return TRACE_SYSCALL_RETURN(__WASI_EINVAL); }

	Time clockTime = Platform::getClockTime(platformClock);

	if(platformClock == Platform::Clock::processCPUTime)
	{
		clockTime.ns -= process->processClockOrigin.ns;
	}

	__wasi_timestamp_t wasiClockTime = __wasi_timestamp_t(clockTime.ns);
	memoryRef<__wasi_timestamp_t>(process->memory, timeAddress) = wasiClockTime;

	return TRACE_SYSCALL_RETURN(__WASI_ESUCCESS, "(%" PRIu64 ")", wasiClockTime);
}
