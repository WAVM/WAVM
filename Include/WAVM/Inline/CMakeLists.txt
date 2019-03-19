set(PublicHeaders
	Assert.h
	BasicTypes.h
	Config.h.in
	CLI.h
	ConcurrentHashMap.h
	DenseStaticIntSet.h
	Errors.h
	FloatComponents.h
	Hash.h
	HashMap.h HashMapImpl.h HashMap.natvis
	HashSet.h HashSetImpl.h HashSet.natvis
	HashTable.h HashTableImpl.h HashTable.natvis
	IndexMap.h
	IntrusiveSharedPtr.h
	IsNameChar.h
	Lock.h
	OptionalStorage.h
	Serialization.h
	Timing.h
	Unicode.h)

WAVM_ADD_LIB_COMPONENT(Inline
	NONCOMPILED_SOURCES ${PublicHeaders})

add_subdirectory(xxhash)