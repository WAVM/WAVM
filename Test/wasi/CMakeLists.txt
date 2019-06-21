set(WASTTests
	random_get.wast
)
add_custom_target(WASITests SOURCES ${WASTTests})

if(WAVM_ENABLE_RUNTIME)
	foreach(WAST_PATH ${WASTTests})
		add_test(
			NAME ${WAST_PATH}
			COMMAND $<TARGET_FILE:wavm-run-wasi> ${WAST_PATH})
	endforeach()
endif()
