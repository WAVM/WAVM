#pragma once

#include <string>
#include "WAVM/Inline/BasicTypes.h"

namespace WAVM { namespace VFS {

	enum class FileAccessMode
	{
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

	enum class StdDevice
	{
		in,
		out,
		err,
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

	enum class FDType
	{
		unknown,
		blockDevice,
		characterDevice,
		directory,
		file,
		datagramSocket,
		streamSocket,
		symbolicLink
	};

	enum class FDImplicitSync
	{
		none,
		syncContentsAfterWrite,
		syncContentsAndMetadataAfterWrite,
		syncContentsAfterWriteAndBeforeRead,
		syncContentsAndMetadataAfterWriteAndBeforeRead
	};

	struct FDFlags
	{
		// If true, data written to the FD is always appended to the file's end.
		bool append;

		// If true, reads and writes will fail if they can't immediately complete.
		bool nonBlocking;

		// The amount of synchronization implied for reads and writes.
		FDImplicitSync implicitSync;
	};

	struct FDInfo
	{
		FDType type;
		FDFlags flags;
	};

	// Result enumerations

	enum class SeekResult
	{
		success,
		invalidOffset,
		unseekable
	};

	enum class CloseResult
	{
		success,
		interrupted,
		ioError
	};

	enum class ReadResult
	{
		success,
		ioError,
		interrupted,
		invalidArgument,
		notPermitted,
		isDirectory,
		outOfMemory
	};

	enum class WriteResult
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

	enum class SyncResult
	{
		success,
		ioError,
		interrupted,
		notSupported
	};

	enum class GetFDInfoResult
	{
		success,
		ioError,
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
		virtual GetFDInfoResult getInfo(FDInfo& outInfo) = 0;

	protected:
		virtual ~FD() {}
	};

	struct FileSystem
	{
		virtual FD* openFile(const std::string& absolutePathName,
							 FileAccessMode accessMode,
							 FileCreateMode createMode)
			= 0;
	};
}}
