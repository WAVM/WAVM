#include "WAVM/ObjectCache/ObjectCache.h"
#include <errno.h>
#include <functional>
#include <memory>
#include <vector>
#include "WAVM/Inline/Assert.h"
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/Errors.h"
#include "WAVM/Inline/Time.h"
#include "WAVM/Inline/Timing.h"
#include "WAVM/Platform/Clock.h"
#include "WAVM/Platform/Defines.h"
#include "WAVM/Runtime/Runtime.h"
#include "lmdb.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4804)
#endif

#include "blake2.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define CURRENT_DB_VERSION 1

using namespace WAVM;
using namespace WAVM::ObjectCache;

#define ERROR_UNLESS_MDB_SUCCESS(expr)                                                             \
	{                                                                                              \
		int error = (expr);                                                                        \
		if(error) { Errors::fatalf("%s failed: %s", #expr, mdb_strerror(error)); }                 \
	}

// A database key to identify a cached module by a hash that identifies the code generation version,
// and a hash of the module's WASM serialization.
WAVM_PACKED_STRUCT(struct ModuleKey {
	U8 codeKeyBytes[8];

	union
	{
		U8 moduleHashBytes[16];
		U64 moduleHashU64s[2];
	};

	ModuleKey() = default;
	ModuleKey(U64 codeKey, U8 inModuleHashBytes[16])
	{
		memcpy(codeKeyBytes, &codeKey, sizeof(codeKeyBytes));
		memcpy(moduleHashBytes, inModuleHashBytes, sizeof(moduleHashBytes));
	}

	U64 getCodeKey() const
	{
		U64 result = 0;
		memcpy(&result, codeKeyBytes, sizeof(U64));
		return result;
	}
});

// A database key that orders Time values in ascending order. LMDB orders the keys lexically as a
// string of bytes, so the Time's I128 needs to be encoded in big-endian order to place the
// most-signifigant bits at the beginning of that string.
WAVM_PACKED_STRUCT(struct TimeKey {
	U8 bytes[16];

	TimeKey() = default;
	TimeKey(const Time& time)
	{
		WAVM_ASSERT(time.ns >= 0);
		for(Uptr byteIndex = 0; byteIndex < 16; ++byteIndex)
		{ bytes[15 - byteIndex] = U8((time.ns >> (byteIndex * 8)) & 0xff); }
	}

	Time getTime() const
	{
		Time result;
		result.ns = 0;
		for(Uptr byteIndex = 0; byteIndex < 16; ++byteIndex)
		{ result.ns |= I128(bytes[15 - byteIndex]) << (byteIndex * 8); }
		return result;
	}
});

WAVM_PACKED_STRUCT(struct Metadata { TimeKey lastAccessTimeKey; });

//
// Helper functions to reinterpret C++ types to and from MDB_vals.
//

template<typename Value> MDB_val asMDBVal(const Value& value)
{
	MDB_val result;
	result.mv_data = const_cast<Value*>(&value);
	result.mv_size = sizeof(Value);
	return result;
}
template<typename Value> void fromMDBVal(const MDB_val& mdbVal, Value& outValue)
{
	WAVM_ASSERT(mdbVal.mv_size == sizeof(Value));
	memcpy(&outValue, mdbVal.mv_data, sizeof(Value));
}

template<> MDB_val asMDBVal<std::vector<U8>>(const std::vector<U8>& bytes)
{
	MDB_val result;
	result.mv_data = const_cast<U8*>(bytes.data());
	result.mv_size = bytes.size();
	return result;
}
template<> void fromMDBVal<std::vector<U8>>(const MDB_val& mdbVal, std::vector<U8>& outBytes)
{
	outBytes
		= std::vector<U8>((const U8*)mdbVal.mv_data, (const U8*)mdbVal.mv_data + mdbVal.mv_size);
}

template<> MDB_val asMDBVal<MDB_val>(const MDB_val& val) { return val; }
template<> void fromMDBVal<MDB_val>(const MDB_val& mdbVal, MDB_val& outMDBVal)
{
	outMDBVal = mdbVal;
}

static bool testSoftFailure()
{
#if 0
	const U64 clockHash
		= Hash<U64>()(U64(Platform::getClockTime(Platform::Clock::realtime).ns & UINT64_MAX), 0);
	return (clockHash & 15) == 0;
#else
	return false;
#endif
}

// A thin wrapper around a LMDB database that encapsulates error handling and translating
// keys/values to MDB_val.
struct Database
{
	// An exception that may be thrown by beginTxn, getKeyValue, putKeyValue, and deleteKey.
	struct Exception
	{
		enum class Type
		{
			keyNotFound,
			diskIsFull,
			mapIsFull,
			tooManyReaders,
		};
		const Type type;
		Exception(Type inType) : type(inType) {}

		static const char* getMessage(Type type)
		{
			switch(type)
			{
			case Type::keyNotFound: return "key not found";
			case Type::diskIsFull: return "disk is full";
			case Type::mapIsFull: return "database is full";
			case Type::tooManyReaders: return "too many readers";
			default: WAVM_UNREACHABLE();
			};
		}
	};

	Database(MDB_env* inEnv) : env(inEnv) {}
	~Database() { mdb_env_close(env); }

	// Begins a transaction. Throws a Database::Exception if there are too many concurrent readers.
	// All other errors are fatal.
	MDB_txn* beginTxn(unsigned int flags = 0, MDB_txn* parentTxn = nullptr)
	{
		if(testSoftFailure()) { throw Exception(Exception::Type::tooManyReaders); }

		MDB_txn* txn = nullptr;
		int beginResult = mdb_txn_begin(env, parentTxn, flags, &txn);
		if(beginResult == MDB_MAP_RESIZED)
		{
			// "If the mapsize is increased by another process, and data has grown beyond the range
			// of the current mapsize, mdb_txn_begin() will return MDB_MAP_RESIZED. This function
			// may be called with a size of zero to adopt the new size."
			ERROR_UNLESS_MDB_SUCCESS(mdb_env_set_mapsize(env, 0));

			beginResult = mdb_txn_begin(env, parentTxn, flags, &txn);
		}
		if(beginResult == MDB_READERS_FULL) { throw Exception(Exception::Type::tooManyReaders); }
		ERROR_UNLESS_MDB_SUCCESS(beginResult);
		return txn;
	}

	// Gets a handle for a table. All errors are fatal.
	MDB_dbi openTable(MDB_txn* txn, const char* name, unsigned int flags)
	{
		MDB_dbi dbi{0};
		ERROR_UNLESS_MDB_SUCCESS(mdb_dbi_open(txn, name, flags, &dbi));
		return dbi;
	}

	// Looks up a key in a table.
	// asMDBVal(const Key&) and fromMDBVal(MDB_val, Value&) must be defined.
	// If the table contains the key, it writes the to outValue and returns true.
	// If the table does not contain the key, it returns false.
	template<typename Key, typename Value>
	static bool tryGetKeyValue(MDB_txn* txn, MDB_dbi dbi, const Key& key, Value& outValue)
	{
		MDB_val keyVal = asMDBVal(key);
		MDB_val dataVal;
		int getResult = mdb_get(txn, dbi, &keyVal, &dataVal);
		if(getResult == MDB_NOTFOUND) { return false; }
		if(getResult) { Errors::fatalf("mdb_get failed: %s", mdb_strerror(getResult)); }
		fromMDBVal(dataVal, outValue);
		return true;
	}

	// Looks up a key in a table.
	// asMDBVal(const Key&) and fromMDBVal(MDB_val, Value&) must be defined.
	// If the table contains the key, it writes the to outValue.
	// If the table does not contain the key, it throws a Database::Exception.
	template<typename Key, typename Value>
	static void getKeyValue(MDB_txn* txn, MDB_dbi dbi, const Key& key, Value& outValue)
	{
		if(!tryGetKeyValue(txn, dbi, key, outValue) || testSoftFailure())
		{ throw Exception(Exception::Type::keyNotFound); }
	}

	// Writes a key+value pair to a table.
	// asMDBVal(const Key&) and asMDBVal(const Value&) must be defined.
	// If the operation succeeded, it returns true.
	// If the database is full, it returns false.
	// All other errors are fatal.
	template<typename Key, typename Value>
	static bool tryPutKeyValue(MDB_txn* txn, MDB_dbi dbi, const Key& key, const Value& value)
	{
		MDB_val keyVal = asMDBVal(key);
		MDB_val dataVal = asMDBVal(value);
		int putResult = mdb_put(txn, dbi, &keyVal, &dataVal, 0);
		if(putResult == MDB_MAP_FULL) { return false; }
		if(putResult) { Errors::fatalf("mdb_put failed: %s", mdb_strerror(putResult)); }
		return true;
	}

	// Writes a key+value pair to a table.
	// asMDBVal(const Key&) and asMDBVal(const Value&) must be defined.
	// If the database is full, it throws a Database::Exception.
	// All other errors are fatal.
	template<typename Key, typename Value>
	static void putKeyValue(MDB_txn* txn, MDB_dbi dbi, const Key& key, const Value& value)
	{
		if(!tryPutKeyValue(txn, dbi, key, value) || testSoftFailure())
		{ throw Exception(Exception::Type::mapIsFull); }
	}

	// Deletes a key from a table.
	// asMDBVal(const Key&) must be defined.
	// If the table contains the key, it deletes it and returns true.
	// If the table does not contain the key, it returns false.
	// All other errors are fatal.
	template<typename Key> static bool tryDeleteKey(MDB_txn* txn, MDB_dbi dbi, const Key& key)
	{
		MDB_val keyVal = asMDBVal(key);
		MDB_val dataVal;
		int delResult = mdb_del(txn, dbi, &keyVal, &dataVal);
		if(delResult == MDB_NOTFOUND) { return false; }
		if(delResult) { Errors::fatalf("mdb_del failed: %s", mdb_strerror(delResult)); }
		return true;
	}

	// Deletes a key from a table.
	// asMDBVal(const Key&) must be defined.
	// If the table contains the key, it deletes it.
	// If the table does not contain the key, it throws a Database::Exception.
	// All other errors are fatal.
	template<typename Key> static void deleteKey(MDB_txn* txn, MDB_dbi dbi, const Key& key)
	{
		if(!tryDeleteKey(txn, dbi, key) || testSoftFailure())
		{ throw Exception(Exception::Type::keyNotFound); }
	}

	// Opens a cursor for the given table. All errors are fatal.
	static MDB_cursor* openCursor(MDB_txn* txn, MDB_dbi dbi)
	{
		MDB_cursor* cursor = nullptr;
		ERROR_UNLESS_MDB_SUCCESS(mdb_cursor_open(txn, dbi, &cursor));
		return cursor;
	}

	// Reads the key+value pair from the given cursor after executing a cursor operation.
	// fromMDBVal(MDB_val, Key&) and fromMDBVal(MDB_val, Value&) must be defined.
	// If the cursor doesn't point to a valid key, returns false.
	// Otherwise, writes the key to outKey, the value to outValue, and returns true.
	// All other errors are fatal.
	template<typename Key, typename Value>
	static bool tryGetCursor(MDB_cursor* cursor, Key& outKey, Value& outValue, MDB_cursor_op op)
	{
		MDB_val mdbKey;
		MDB_val mdbValue;
		int getResult = mdb_cursor_get(cursor, &mdbKey, &mdbValue, op);
		if(getResult == MDB_NOTFOUND) { return false; }
		if(getResult) { Errors::fatalf("mdb_get failed: %s", mdb_strerror(getResult)); }

		fromMDBVal(mdbKey, outKey);
		fromMDBVal(mdbValue, outValue);
		return true;
	}

	// Closes a cursor. May not fail.
	static void closeCursor(MDB_cursor* cursor) { mdb_cursor_close(cursor); }

	// Removes all entries from a table. All errors are fatal.
	static void dropDB(MDB_txn* txn, MDB_dbi dbi)
	{
		ERROR_UNLESS_MDB_SUCCESS(mdb_drop(txn, dbi, 0));
	}

private:
	MDB_env* env;
};

// Ensures that a lexically scoped transaction is committed or aborted when going out of scope.
struct ScopedTxn
{
	ScopedTxn(MDB_txn* inTxn) : txn(inTxn) {}

	~ScopedTxn()
	{
		if(txn) { abort(); }
	}

	operator MDB_txn*() { return txn; }

	void abort()
	{
		WAVM_ASSERT(txn);
		mdb_txn_abort(txn);
		txn = nullptr;
	}

	void commit()
	{
		WAVM_ASSERT(txn);

		int commitResult = mdb_txn_commit(txn);
		txn = nullptr;

		if(commitResult == ENOSPC || testSoftFailure())
		{ throw Database::Exception(Database::Exception::Type::diskIsFull); }
		if(commitResult)
		{ Errors::fatalf("mdb_txn_commit failed: %s", mdb_strerror(commitResult)); }
	}

private:
	MDB_txn* txn;
};

// Encapsulates the global state of the object cache.
struct LMDBObjectCache : Runtime::ObjectCacheInterface
{
	OpenResult init(const char* path, Uptr maxBytes, U64 inCodeKey)
	{
		codeKey = inCodeKey;

		// Open the LMDB database.
		MDB_env* env = nullptr;
		ERROR_UNLESS_MDB_SUCCESS(mdb_env_create(&env));
		ERROR_UNLESS_MDB_SUCCESS(mdb_env_set_mapsize(env, maxBytes));
		ERROR_UNLESS_MDB_SUCCESS(mdb_env_set_maxdbs(env, 5));
		const int openError = mdb_env_open(env, path, MDB_NOMETASYNC, 0666);
		if(openError)
		{
			mdb_env_close(env);
			switch(openError)
			{
			case ENOENT: return OpenResult::doesNotExist;
			case ENOTDIR: return OpenResult::notDirectory;
			case EACCES: return OpenResult::notAccessible;
			case MDB_INVALID: return OpenResult::invalidDatabase;

			default:
				Errors::fatalf(
					"mdb_env_open(env, \"%s\", 0, 0666) failed: %s", path, mdb_strerror(openError));
			};
		}

		// Create the database wrapper.
		database.reset(new Database(env));

		try
		{
			ScopedTxn txn(database->beginTxn());

			// Open or create the database tables used by the object cache.
			objectTable = database->openTable(txn, "objects", MDB_CREATE);
			metaTable = database->openTable(txn, "meta", MDB_CREATE);
			lruTable = database->openTable(txn, "lru", MDB_CREATE);
			versionTable = database->openTable(txn, "version", MDB_CREATE);

			// Check the object cache version stored in the database.
			const char versionString[] = "version";
			U64 dbVersion = UINT64_MAX;
			bool writeVersion = false;
			if(!Database::tryGetKeyValue(txn, versionTable, versionString, dbVersion))
			{ writeVersion = true; }
			else if(dbVersion != CURRENT_DB_VERSION)
			{
				writeVersion = true;

				// If the object cache version doesn't match the current version, clear all the
				// tables.
				Log::printf(Log::debug,
							"Clearing contents of outdated object cache database '%s'.\n",
							path);
				Database::dropDB(txn, objectTable);
				Database::dropDB(txn, metaTable);
				Database::dropDB(txn, lruTable);
				Database::dropDB(txn, versionTable);
			}

			if(writeVersion)
			{
				// Write the current object cache version to the database.
				dbVersion = CURRENT_DB_VERSION;
				Database::putKeyValue(txn, versionTable, versionString, dbVersion);
			}

			// Commit the initialization transaction.
			txn.commit();

			return OpenResult::success;
		}
		catch(Database::Exception const& exception)
		{
			// If one of the database operations threw an exception, close the database and return
			// the appropriate error code.
			database.reset();

			switch(exception.type)
			{
			case Database::Exception::Type::tooManyReaders: return OpenResult::tooManyReaders;
			case Database::Exception::Type::keyNotFound:
			case Database::Exception::Type::diskIsFull:
			case Database::Exception::Type::mapIsFull: return OpenResult::invalidDatabase;
			default: WAVM_UNREACHABLE();
			};
		}
	}

	bool tryGetCachedObject(U8 moduleHash[16],
							const U8* wasmBytes,
							Uptr numWASMBytes,
							std::vector<U8>& outObjectCode)
	{
		Timing::Timer readTimer;

		ScopedTxn txn(database->beginTxn());

		// Check for a cached module with this hash key.
		bool hadCachedObject = false;
		ModuleKey moduleKey(codeKey, moduleHash);
		if(Database::tryGetKeyValue(txn, objectTable, moduleKey, outObjectCode))
		{
			// Update the last-used time for the cached module.
			Metadata metadata;
			Database::getKeyValue(txn, metaTable, moduleKey, metadata);
			Database::deleteKey(txn, lruTable, metadata.lastAccessTimeKey);
			metadata.lastAccessTimeKey = Platform::getClockTime(Platform::Clock::realtime);
			Database::putKeyValue(txn, metaTable, moduleKey, metadata);
			Database::putKeyValue(txn, lruTable, metadata.lastAccessTimeKey, moduleKey);

			hadCachedObject = true;
		}

		// Commit the read transaction.
		txn.commit();

		Timing::logTimer("Probed for cached object", readTimer);

		return hadCachedObject;
	}

	void addCachedObject(U8 moduleHash[16],
						 const U8* wasmBytes,
						 Uptr numWASMBytes,
						 const std::vector<U8>& objectBytes)
	{
		Timing::Timer writeTimer;

		Time now = Platform::getClockTime(Platform::Clock::realtime);
		ModuleKey moduleKey(codeKey, moduleHash);

		// Try to add the module to the database.
		bool firstTry = true;
		while(true)
		{
			// If a previous try failed due to the database being full, try to evict the least
			// recently used cached object.
			if(!firstTry)
			{
				if(!evictLRU())
				{
					// If there are no cached objects to evict, the object must just not fit in the
					// database. Log a warning and return without adding it to the database.
					Log::printf(Log::debug, "Failed to add object to cache: too large.\n");
					break;
				}
			}
			firstTry = false;

			ScopedTxn txn(database->beginTxn());

			// If there was already a metadata entry for the module, remove its LRU entry to ensure
			// the LRU table doesn't end up with multiple entries for this module. This keeps the
			// database consistent if a cached object is being redundantly added for some reason.
			Metadata metadata;
			if(Database::tryGetKeyValue(txn, metaTable, moduleKey, metadata))
			{ Database::deleteKey(txn, lruTable, metadata.lastAccessTimeKey); }
			metadata.lastAccessTimeKey = now;

			// Add the module to the object, metadata, and LRU tables.
			if(Database::tryPutKeyValue(txn, metaTable, moduleKey, metadata)
			   && Database::tryPutKeyValue(txn, lruTable, metadata.lastAccessTimeKey, moduleKey)
			   && Database::tryPutKeyValue(txn, objectTable, moduleKey, objectBytes))
			{
				txn.commit();
				break;
			}
		};

		Timing::logTimer("Add object to cache", writeTimer);
	}

	void dump()
	{
		const Time now = Platform::getClockTime(Platform::Clock::realtime);

		ScopedTxn txn(database->beginTxn(MDB_RDONLY));

		// Dump the contents of the object table.
		Log::printf(Log::debug, "Object table:\n");
		{
			MDB_cursor* cursor = Database::openCursor(txn, objectTable);

			ModuleKey moduleKey;
			MDB_val objectBytesVal;
			bool getResult = Database::tryGetCursor(cursor, moduleKey, objectBytesVal, MDB_FIRST);
			while(getResult)
			{
				Log::printf(Log::debug,
							"  %16" PRIx64 "|%16" PRIx64 "%16" PRIx64 " %zu bytes\n",
							moduleKey.getCodeKey(),
							moduleKey.moduleHashU64s[0],
							moduleKey.moduleHashU64s[1],
							objectBytesVal.mv_size);

				getResult = Database::tryGetCursor(cursor, moduleKey, objectBytesVal, MDB_NEXT);
			};
			Database::closeCursor(cursor);
		}

		// Dump the contents of the metadata table.
		Log::printf(Log::debug, "Meta table:\n");
		{
			MDB_cursor* cursor = Database::openCursor(txn, metaTable);

			ModuleKey moduleKey;
			Metadata metadata;
			bool getResult = Database::tryGetCursor(cursor, moduleKey, metadata, MDB_FIRST);
			while(getResult)
			{
				const F64 lastAccessTimeAge
					= F64((now.ns - metadata.lastAccessTimeKey.getTime().ns) / 1000000000);

				Log::printf(Log::debug,
							"  %16" PRIx64 "|%16" PRIx64 "%16" PRIx64
							" last access: %.1f seconds ago\n",
							moduleKey.getCodeKey(),
							moduleKey.moduleHashU64s[0],
							moduleKey.moduleHashU64s[1],
							lastAccessTimeAge);

				getResult = Database::tryGetCursor(cursor, moduleKey, metadata, MDB_NEXT);
			};
			Database::closeCursor(cursor);
		}

		// Dump the contents of the LRU table.
		Log::printf(Log::debug, "LRU table:\n");
		{
			MDB_cursor* cursor = Database::openCursor(txn, lruTable);

			TimeKey lastAccessTimeKey;
			ModuleKey moduleKey;
			bool getResult
				= Database::tryGetCursor(cursor, lastAccessTimeKey, moduleKey, MDB_FIRST);
			while(getResult)
			{
				const F64 ageSeconds = F64((now.ns - lastAccessTimeKey.getTime().ns) / 1000000000);

				Log::printf(Log::debug,
							"  %16" PRIx64 "|%16" PRIx64 "%16" PRIx64 " %.1f seconds ago\n",
							moduleKey.getCodeKey(),
							moduleKey.moduleHashU64s[0],
							moduleKey.moduleHashU64s[1],
							ageSeconds);

				getResult = Database::tryGetCursor(cursor, lastAccessTimeKey, moduleKey, MDB_NEXT);
			};
			Database::closeCursor(cursor);
		}
	}

	virtual std::vector<U8> getCachedObject(
		const U8* wasmBytes,
		Uptr numWASMBytes,
		std::function<std::vector<U8>()>&& compileThunk) override
	{
		// Compute a hash of the serialized WASM module.
		Timing::Timer hashTimer;

		U8 moduleHashBytes[16];
		if(blake2b(moduleHashBytes, sizeof(moduleHashBytes), wasmBytes, numWASMBytes, nullptr, 0))
		{ Errors::fatal("blake2b error"); }

		Timing::logRatePerSecond(
			"Hashed module key", hashTimer, numWASMBytes / 1024.0 / 1024.0, "MiB");

		// Try to find the module's object code in the cache.
		std::vector<U8> objectCode;
		try
		{
			if(tryGetCachedObject(moduleHashBytes, wasmBytes, numWASMBytes, objectCode))
			{ return objectCode; }
		}
		catch(Database::Exception const& exception)
		{
			Log::printf(Log::error,
						"Failed to lookup module in object cache: %s\n",
						Database::Exception::getMessage(exception.type));
		}

		// If there wasn't a matching cached module+object code, compile the module.
		objectCode = compileThunk();

		// Add the cached module+object code to the database.
		try
		{
			addCachedObject(moduleHashBytes, wasmBytes, numWASMBytes, objectCode);
		}
		catch(Database::Exception const& exception)
		{
			Log::printf(Log::error,
						"Failed to add module to object cache: %s\n",
						Database::Exception::getMessage(exception.type));
		}

		return objectCode;
	}

private:
	std::unique_ptr<Database> database;
	MDB_dbi objectTable;
	MDB_dbi metaTable;
	MDB_dbi lruTable;
	MDB_dbi versionTable;
	U64 codeKey{0};

	bool evictLRU()
	{
		ScopedTxn txn(database->beginTxn());

		// Try to read the oldest entry in the LRU table.
		TimeKey lastAccessTimeKey;
		ModuleKey moduleKey;
		MDB_cursor* cursor = Database::openCursor(txn, lruTable);
		if(!Database::tryGetCursor(cursor, lastAccessTimeKey, moduleKey, MDB_FIRST))
		{ return false; }
		Database::closeCursor(cursor);

		// Delete the cached object identified by the oldest entry in the LRU table from all tables.
		Database::deleteKey(txn, objectTable, moduleKey);
		Database::deleteKey(txn, metaTable, moduleKey);
		Database::deleteKey(txn, lruTable, lastAccessTimeKey);

		// Commit the delete.
		txn.commit();

		Log::printf(Log::debug,
					"Evicted %16" PRIx64 "%16" PRIx64 " from the object cache.\n",
					moduleKey.moduleHashU64s[0],
					moduleKey.moduleHashU64s[1]);

		return true;
	}
};

OpenResult ObjectCache::open(const char* path,
							 Uptr maxBytes,
							 U64 codeKey,
							 std::shared_ptr<Runtime::ObjectCacheInterface>& outObjectCache)
{
	LMDBObjectCache lmdbObjectCache;
	OpenResult result = lmdbObjectCache.init(path, maxBytes, codeKey);
	if(result == OpenResult::success)
	{ outObjectCache = std::make_shared<LMDBObjectCache>(std::move(lmdbObjectCache)); }
	return result;
}
