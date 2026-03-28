set(Sources
	echo.wast
	helloworld.wast
	tee.wast
	trap.wast)
add_custom_target(Examples SOURCES ${Sources})
set_target_properties(Examples PROPERTIES FOLDER Examples)