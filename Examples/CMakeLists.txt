set(Sources
	Benchmark/Benchmark.cpp
	Benchmark/Benchmark.wast
	blake2b.wast
	echo.wast
	helloworld.wast
	tee.wast
	trap.wast
	zlib.wast)
add_custom_target(Examples SOURCES ${Sources})
set_target_properties(Examples PROPERTIES FOLDER Examples)
