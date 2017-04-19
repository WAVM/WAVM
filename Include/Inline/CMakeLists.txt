set(PublicHeaders
	DenseStaticIntSet.h
	Floats.h
	Serialization.h)
add_custom_target(Inline SOURCES ${PublicHeaders})