set(Headers
	Assert.h
	BasicTypes.h
	CLI.h
	DenseStaticIntSet.h
	Errors.h
	FloatComponents.h
	Hash.h
	HashMap.h
	HashSet.h
	HashTable.h
	I128.h
	IndexMap.h
	InlineArray.h
	IntrusiveSharedPtr.h
	IsNameChar.h
	Impl/HashMapImpl.h
	Impl/HashSetImpl.h
	Impl/HashTableImpl.h
	Impl/I128Impl.h
	Impl/OptionalStorage.h
	RandomStream.h
	Serialization.h
	Time.h
	Timing.h
	Unicode.h
	LEB128.h)

set(NonCompiledSources
	Config.h.in
	Impl/HashMap.natvis
	Impl/HashSet.natvis
	Impl/HashTable.natvis
	Impl/I128Impl.LICENSE
	Impl/OptionalStorage.natvis)

WAVM_ADD_LIB_COMPONENT(Inline
	HEADERS ${Headers}
	NONCOMPILED_SOURCES ${NonCompiledSources}
	PUBLIC_SYSTEM_INCLUDE_DIRECTORIES
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/xxhash>
		$<INSTALL_INTERFACE:include/WAVM/Inline/xxhash>)

add_subdirectory(xxhash)
