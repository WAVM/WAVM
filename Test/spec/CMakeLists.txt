file(GLOB Sources "*.wast")
add_custom_target(SpecTests SOURCES ${Sources})

set(TEST_BIN ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CONFIGURATION}/Test)

add_test(WAVM_known_failures ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/WAVM_known_failures.wast)

add_test(address ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/address.wast)
add_test(binary ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/binary.wast)
add_test(block ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/block.wast)
add_test(br ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/br.wast)
add_test(break-drop ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/break-drop.wast)
add_test(br_if ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/br_if.wast)
add_test(br_table ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/br_table.wast)
add_test(call ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/call.wast)
add_test(call_indirect ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/call_indirect.wast)
add_test(comments ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/comments.wast)
add_test(conversions ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/conversions.wast)
add_test(custom_section ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/custom_section.wast)
add_test(endianness ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/endianness.wast)
add_test(exports ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/exports.wast)
add_test(f32 ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32.wast)
add_test(f32_bitwise ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32_bitwise.wast)
add_test(f32_cmp ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32_cmp.wast)
add_test(f64 ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64.wast)
add_test(f64_bitwise ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64_bitwise.wast)
add_test(f64_cmp ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64_cmp.wast)
add_test(fac ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/fac.wast)
add_test(float_exprs ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/float_exprs.wast)
add_test(float_literals ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/float_literals.wast)
add_test(float_memory ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/float_memory.wast)
add_test(float_misc ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/float_misc.wast)
add_test(forward ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/forward.wast)
add_test(func ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func.wast)
add_test(func_ptrs ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func_ptrs.wast)
add_test(get_local ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/get_local.wast)
add_test(globals ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/globals.wast)
add_test(i32 ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.wast)
add_test(i64 ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i64.wast)
add_test(if ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if.wast)
add_test(imports ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/imports.wast)
add_test(int_exprs ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/int_exprs.wast)
add_test(int_literals ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/int_literals.wast)
add_test(labels ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/labels.wast)
add_test(left-to-right ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/left-to-right.wast)
add_test(linking ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/linking.wast)
add_test(loop ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/loop.wast)
add_test(memory ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/memory.wast)
add_test(memory_redundancy ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/memory_redundancy.wast)
add_test(memory_trap ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/memory_trap.wast)
add_test(names ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/names.wast)
add_test(nop ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/nop.wast)
add_test(resizing ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/resizing.wast)
add_test(return ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/return.wast)
add_test(select ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/select.wast)
add_test(set_local ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/set_local.wast)
#add_test(skip-stack-guard-page ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/skip-stack-guard-page.wast)
add_test(start ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/start.wast)
add_test(stack ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/stack.wast)
add_test(store_retval ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/store_retval.wast)
add_test(switch ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/switch.wast)
add_test(tee_local ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/tee_local.wast)
add_test(traps ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/traps.wast)
add_test(typecheck ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/typecheck.wast)
add_test(unreachable ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/unreachable.wast)
add_test(unreached-invalid ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/unreached-invalid.wast)
add_test(unwind ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/unwind.wast)
add_test(utf8-custom-section-id ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/utf8-custom-section-id.wast)
add_test(utf8-import-field ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/utf8-import-field.wast)
add_test(utf8-import-module ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/utf8-import-module.wast)

add_test(address-offset-range.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/address-offset-range.fail.wast)
set_tests_properties(address-offset-range.fail PROPERTIES WILL_FAIL TRUE)

add_test(f32.load32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32.load32.fail.wast)
set_tests_properties(f32.load32.fail PROPERTIES WILL_FAIL TRUE)

add_test(f32.load64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32.load64.fail.wast)
set_tests_properties(f32.load64.fail PROPERTIES WILL_FAIL TRUE)

add_test(f32.store32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32.store32.fail.wast)
set_tests_properties(f32.store32.fail PROPERTIES WILL_FAIL TRUE)

add_test(f32.store64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f32.store64.fail.wast)
set_tests_properties(f32.store64.fail PROPERTIES WILL_FAIL TRUE)

add_test(f64.load32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64.load32.fail.wast)
set_tests_properties(f64.load32.fail PROPERTIES WILL_FAIL TRUE)

add_test(f64.load64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64.load64.fail.wast)
set_tests_properties(f64.load64.fail PROPERTIES WILL_FAIL TRUE)

add_test(f64.store32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64.store32.fail.wast)
set_tests_properties(f64.store32.fail PROPERTIES WILL_FAIL TRUE)

add_test(f64.store64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/f64.store64.fail.wast)
set_tests_properties(f64.store64.fail PROPERTIES WILL_FAIL TRUE)

add_test(func-local-after-body.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func-local-after-body.fail.wast)
set_tests_properties(func-local-after-body.fail PROPERTIES WILL_FAIL TRUE)

add_test(func-local-before-param.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func-local-before-param.fail.wast)
set_tests_properties(func-local-before-param.fail PROPERTIES WILL_FAIL TRUE)

add_test(func-local-before-result.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func-local-before-result.fail.wast)
set_tests_properties(func-local-before-result.fail PROPERTIES WILL_FAIL TRUE)

add_test(func-param-after-body.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func-param-after-body.fail.wast)
set_tests_properties(func-param-after-body.fail PROPERTIES WILL_FAIL TRUE)

add_test(func-result-after-body.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func-result-after-body.fail.wast)
set_tests_properties(func-result-after-body.fail PROPERTIES WILL_FAIL TRUE)

add_test(func-result-before-param.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/func-result-before-param.fail.wast)
set_tests_properties(func-result-before-param.fail PROPERTIES WILL_FAIL TRUE)

add_test(i32.load32_s.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.load32_s.fail.wast)
set_tests_properties(i32.load32_s.fail PROPERTIES WILL_FAIL TRUE)

add_test(i32.load32_u.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.load32_u.fail.wast)
set_tests_properties(i32.load32_u.fail PROPERTIES WILL_FAIL TRUE)

add_test(i32.load64_s.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.load64_s.fail.wast)
set_tests_properties(i32.load64_s.fail PROPERTIES WILL_FAIL TRUE)

add_test(i32.load64_u.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.load64_u.fail.wast)
set_tests_properties(i32.load64_u.fail PROPERTIES WILL_FAIL TRUE)

add_test(i32.store32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.store32.fail.wast)
set_tests_properties(i32.store32.fail PROPERTIES WILL_FAIL TRUE)

add_test(i32.store64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i32.store64.fail.wast)
set_tests_properties(i32.store64.fail PROPERTIES WILL_FAIL TRUE)

add_test(i64.load64_s.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i64.load64_s.fail.wast)
set_tests_properties(i64.load64_s.fail PROPERTIES WILL_FAIL TRUE)

add_test(i64.load64_u.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i64.load64_u.fail.wast)
set_tests_properties(i64.load64_u.fail PROPERTIES WILL_FAIL TRUE)

add_test(i64.store64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/i64.store64.fail.wast)
set_tests_properties(i64.store64.fail PROPERTIES WILL_FAIL TRUE)

add_test(import-after-func.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/import-after-func.fail.wast)
set_tests_properties(import-after-func.fail PROPERTIES WILL_FAIL TRUE)

add_test(import-after-global.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/import-after-global.fail.wast)
set_tests_properties(import-after-global.fail PROPERTIES WILL_FAIL TRUE)

add_test(import-after-memory.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/import-after-memory.fail.wast)
set_tests_properties(import-after-memory.fail PROPERTIES WILL_FAIL TRUE)

add_test(import-after-table.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/import-after-table.fail.wast)
set_tests_properties(import-after-table.fail PROPERTIES WILL_FAIL TRUE)

add_test(of_string-overflow-hex-u32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/of_string-overflow-hex-u32.fail.wast)
set_tests_properties(of_string-overflow-hex-u32.fail PROPERTIES WILL_FAIL TRUE)

add_test(of_string-overflow-hex-u64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/of_string-overflow-hex-u64.fail.wast)
set_tests_properties(of_string-overflow-hex-u64.fail PROPERTIES WILL_FAIL TRUE)

add_test(of_string-overflow-s32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/of_string-overflow-s32.fail.wast)
set_tests_properties(of_string-overflow-s32.fail PROPERTIES WILL_FAIL TRUE)

add_test(of_string-overflow-s64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/of_string-overflow-s64.fail.wast)
set_tests_properties(of_string-overflow-s64.fail PROPERTIES WILL_FAIL TRUE)

add_test(of_string-overflow-u32.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/of_string-overflow-u32.fail.wast)
set_tests_properties(of_string-overflow-u32.fail PROPERTIES WILL_FAIL TRUE)

add_test(of_string-overflow-u64.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/of_string-overflow-u64.fail.wast)
set_tests_properties(of_string-overflow-u64.fail PROPERTIES WILL_FAIL TRUE)

add_test(block-end-label-mismatch.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/block-end-label-mismatch.fail.wast)
set_tests_properties(block-end-label-mismatch.fail PROPERTIES WILL_FAIL TRUE)

add_test(block-end-label-superfluous.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/block-end-label-superfluous.fail.wast)
set_tests_properties(block-end-label-superfluous.fail PROPERTIES WILL_FAIL TRUE)

add_test(if-else-end-label-mismatch.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if-else-end-label-mismatch.fail.wast)
set_tests_properties(if-else-end-label-mismatch.fail PROPERTIES WILL_FAIL TRUE)

add_test(if-else-end-label-superfluous.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if-else-end-label-superfluous.fail.wast)
set_tests_properties(if-else-end-label-superfluous.fail PROPERTIES WILL_FAIL TRUE)

add_test(if-else-label-mismatch.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if-else-label-mismatch.fail.wast)
set_tests_properties(if-else-label-mismatch.fail PROPERTIES WILL_FAIL TRUE)

add_test(if-else-label-superfluous.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if-else-label-superfluous.fail.wast)
set_tests_properties(if-else-label-superfluous.fail PROPERTIES WILL_FAIL TRUE)

add_test(if-end-label-mismatch.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if-end-label-mismatch.fail.wast)
set_tests_properties(if-end-label-mismatch.fail PROPERTIES WILL_FAIL TRUE)

add_test(if-end-label-superfluous.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/if-end-label-superfluous.fail.wast)
set_tests_properties(if-end-label-superfluous.fail PROPERTIES WILL_FAIL TRUE)

add_test(load-align-0.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/load-align-0.fail.wast)
set_tests_properties(load-align-0.fail PROPERTIES WILL_FAIL TRUE)

add_test(load-align-odd.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/load-align-odd.fail.wast)
set_tests_properties(load-align-odd.fail PROPERTIES WILL_FAIL TRUE)

add_test(load-align-big.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/load-align-big.fail.wast)
set_tests_properties(load-align-big.fail PROPERTIES WILL_FAIL TRUE)

add_test(loop-end-label-mismatch.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/loop-end-label-mismatch.fail.wast)
set_tests_properties(loop-end-label-mismatch.fail PROPERTIES WILL_FAIL TRUE)

add_test(loop-end-label-superfluous.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/loop-end-label-superfluous.fail.wast)
set_tests_properties(loop-end-label-superfluous.fail PROPERTIES WILL_FAIL TRUE)

add_test(store-align-0.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/store-align-0.fail.wast)
set_tests_properties(store-align-0.fail PROPERTIES WILL_FAIL TRUE)

add_test(store-align-odd.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/store-align-odd.fail.wast)
set_tests_properties(store-align-odd.fail PROPERTIES WILL_FAIL TRUE)

add_test(store-align-big.fail ${TEST_BIN} ${CMAKE_CURRENT_LIST_DIR}/store-align-big.fail.wast)
set_tests_properties(store-align-big.fail PROPERTIES WILL_FAIL TRUE)