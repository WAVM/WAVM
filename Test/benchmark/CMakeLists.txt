ADD_WAST_TESTS(
	NAME_PREFIX benchmark/
    SOURCES bitmask.wast
            memory_copy_benchmark.wast
            interleaved_load_store_benchmark.wast
    WAVM_ARGS --trace-assembly --enable all
    RUN_SERIAL
)