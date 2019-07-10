set(SIMDSpecTestSources
    simd_address.wast
    simd_align.wast
    simd_load.wast
    simd_store.wast
    simd_const.wast
    simd_splat.wast)
add_custom_target(SIMDSpecTests SOURCES ${SIMDSpecTestSources})
set_target_properties(SIMDSpecTests PROPERTIES FOLDER Testing)

if(WAVM_ENABLE_RUNTIME)
    ADD_WAST_TESTS("${SIMDSpecTestSources}")
endif()
