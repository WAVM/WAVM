ADD_WAST_TESTS(
    SOURCES ltz_mask.wast
            memory_copy_benchmark.wast
            interleaved_load_store_benchmark.wast
    WAVM_ARGS "--trace-assembly"
    RUN_SERIAL
)