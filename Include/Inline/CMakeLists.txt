set(PublicHeaders
	Assert.h
	BasicTypes.h
	ConcurrentHashMap.h
	DenseStaticIntSet.h
	Errors.h
	Floats.h
	Hash.h
	HashMap.h HashMapImpl.h HashMap.natvis
	HashSet.h HashSetImpl.h HashSet.natvis
	HashTable.h HashTableImpl.h HashTable.natvis
	IndexAllocator.h
	IntrusiveSharedPtr.h
	OptionalStorage.h
	Serialization.h
	Timing.h
	Unicode.h)
add_custom_target(Inline SOURCES ${PublicHeaders})