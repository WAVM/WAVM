set(Sources
	blake2b.wast
	echo.wast
	helloworld.wast
	tee.wast
	trap.wast
	zlib.wasm)
add_custom_target(Examples SOURCES ${Sources})
set_target_properties(Examples PROPERTIES FOLDER Examples)
