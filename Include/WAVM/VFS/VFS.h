#pragma once

#include <string>
#include "WAVM/Inline/BasicTypes.h"
#include "WAVM/Inline/I128.h"

namespace WAVM { namespace VFS {

	enum class FileAccessMode
	{
		none,
		readOnly,
		writeOnly,
		readWrite,
	};

	enum class FileCreateMode
	{
		createAlways,
		createNew,
		openAlways,
		openExisting,
		truncateExisting,
	};

	enum class SeekOrigin
	{
		begin,
		cur,
		end
	};

	enum class SyncType
	{
		contents,
		contentsAndMetadata
	};

	enum class FileType
	{
		unknown,
		blockDevice,
		characterDevice,
		directory,
		file,
		datagramSocket,
		streamSocket,
		symbolicLink,
		pipe
	};

	enum class FDImplicitSync
	{
		none,
		syncContentsAfterWrite,
		syncContentsAndMetadataAfterWrite,
		syncContentsAfterWriteAndBeforeRead,
		syncContentsAndMetadataAfterWriteAndBeforeRead
	};

	struct FDInfo
	{
		FileType type;

		// If true, data written to the FD is always appended to the file's end.
		bool append;

		// If true, reads and writes will fail if they can't immediately complete.
		bool nonBlocking;

		// The amount of synchronization implied for reads and writes.
		FDImplicitSync implicitSync;
	};

	struct FileInfo
	{
		U64 deviceNumber;
		U64 fileNumber;

		FileType type;
		U32 numLinks;
		U64 numBytes;

		// Time values correspond to the "real-time" clock defined by Platform::getRealtimeClock.
		I128 lastAccessTime;
		I128 lastWriteTime;
		I128 creationTime;
	};

	// Result enumerations

	enum class SeekResult : I32
	{
		success,

		invalidOffset,
		unseekable,
	};

	enum class CloseResult : I32
	{
		success,

		interrupted,
		ioError
	};

	enum class ReadResult : I32
	{
		success,

		ioError,
		interrupted,
		invalidArgument,
		notPermitted,
		isDirectory,
		outOfMemory
	};

	enum class WriteResult : I32
	{
		success,
		ioError,
		interrupted,
		invalidArgument,
		outOfMemory,
		outOfQuota,
		outOfFreeSpace,
		exceededFileSizeLimit,
		notPermitted
	};

	enum class SyncResult : I32
	{
		success,
		ioError,
		interrupted,
		notSupported
	};

	enum class GetInfoResult : I32
	{
		success,
		ioError,
	};

	enum class OpenResult : I32
	{
		success,

		alreadyExists,
		doesNotExist,
		isDirectory,
		cantSynchronize,
		invalidNameCharacter,
		nameTooLong,
		pathUsesFileAsDirectory,

		notPermitted,
		ioError,
		interrupted,

		outOfMemory,
		outOfQuota,
		outOfFreeSpace,
	};

	enum GetInfoByPathResult : I32
	{
		success,

		doesNotExist,
		invalidNameCharacter,
		nameTooLong,
		pathUsesFileAsDirectory,

		notPermitted,
		ioError
	};

	struct FD
	{
		// Closes the FD. If CloseResult::success is returned, also deletes this FD.
		virtual CloseResult close() = 0;

		virtual SeekResult seek(I64 offset, SeekOrigin origin, U64* outAbsoluteOffset = nullptr)
			= 0;
		virtual ReadResult read(void* outData, Uptr numBytes, Uptr* outNumBytesRead = nullptr) = 0;
		virtual WriteResult write(const void* data,
								  Uptr numBytes,
								  Uptr* outNumBytesWritten = nullptr)
			= 0;
		virtual SyncResult sync(SyncType type) = 0;
		virtual GetInfoResult getFDInfo(FDInfo& outInfo) = 0;
		virtual GetInfoResult getFileInfo(FileInfo& outInfo) = 0;

	protected:
		virtual ~FD() {}
	};

	struct FileSystem
	{
		virtual OpenResult open(const std::string& absolutePathName,
								FileAccessMode accessMode,
								FileCreateMode createMode,
								FD*& outFD,
								VFS::FDImplicitSync implicitSync = VFS::FDImplicitSync::none)
			= 0;

		virtual GetInfoByPathResult getInfo(const std::string& absolutePathName, FileInfo& outInfo)
			= 0;
	};
}}
