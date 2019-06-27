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

	struct DirEnt
	{
		U64 fileNumber;
		std::string name;
		FileType type;
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

		ioError
	};

	enum class ReadResult : I32
	{
		success,

		ioError,
		interrupted,
		tooManyBytes,
		notPermitted,
		isDirectory,
		outOfMemory
	};

	enum class WriteResult : I32
	{
		success,
		ioError,
		interrupted,
		tooManyBytes,
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
		tooManyLinks,

		notPermitted,
		ioError,
		interrupted,

		outOfMemory,
		outOfQuota,
		outOfFreeSpace,
	};

	enum class GetInfoByPathResult : I32
	{
		success,

		doesNotExist,
		invalidNameCharacter,
		nameTooLong,
		pathUsesFileAsDirectory,
		tooManyLinks,

		notPermitted,
		ioError
	};

	enum class OpenDirResult : I32
	{
		success,

		notADirectory,
		outOfMemory,
		notPermitted,
	};

	enum class OpenDirByPathResult : I32
	{
		success,

		invalidNameCharacter,
		nameTooLong,
		pathUsesFileAsDirectory,
		tooManyLinks,

		notADirectory,
		outOfMemory,
		notPermitted,
	};

	struct DirEntStream
	{
		virtual ~DirEntStream() {}

		virtual void close() = 0;

		virtual bool getNext(DirEnt& outEntry) = 0;

		virtual void restart() = 0;
		virtual U64 tell() = 0;
		virtual bool seek(U64 offset) = 0;
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

		virtual OpenDirResult openDir(DirEntStream*& outStream) = 0;

	protected:
		virtual ~FD() {}
	};

	struct FileSystem
	{
		virtual ~FileSystem() {}

		virtual OpenResult open(const std::string& absolutePathName,
								FileAccessMode accessMode,
								FileCreateMode createMode,
								FD*& outFD,
								VFS::FDImplicitSync implicitSync = VFS::FDImplicitSync::none)
			= 0;

		virtual GetInfoByPathResult getInfo(const std::string& absolutePathName, FileInfo& outInfo)
			= 0;

		virtual OpenDirByPathResult openDir(const std::string& absolutePathName,
											DirEntStream*& outStream)
			= 0;
	};
}}
