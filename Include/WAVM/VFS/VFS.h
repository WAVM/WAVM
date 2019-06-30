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

	struct FDFlags
	{
		// If true, writes will always occur at the end of the file.
		bool append{false};

		// If true, reads and writes will fail if they can't immediately complete.
		bool nonBlocking{false};

		// The amount of synchronization implied for reads and writes.
		FDImplicitSync implicitSync{FDImplicitSync::none};
	};

	struct FDInfo
	{
		FileType type;
		FDFlags flags;
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

	struct IOReadBuffer
	{
		void* data;
		Uptr numBytes;
	};

	struct IOWriteBuffer
	{
		const void* data;
		Uptr numBytes;
	};

	// Error codes
	// clang-format off

	#define ENUM_VFS_RESULTS(v) \
		v(success, "Success") \
		/* Asynchronous I/O statuses */ \
		v(ioPending, "IO pending") \
		/* Hardware errors */ \
		v(ioDeviceError, "IO device error") \
		/* Transient errors */ \
		v(interruptedBySignal, "Interrupted by signal") \
		v(interruptedByCancellation, "Interrupted by cancellation") \
		v(wouldBlock, "Operation on non-blocking file descriptor would block") \
		/* Invalid argument errors */ \
		v(inaccessibleBuffer, "A provided buffer is in memory that is not accessible") \
		v(invalidOffset, "Invalid offset") \
		/* Capability errors */ \
		v(notSeekable, "File descriptor is not seekable") \
		v(notPermitted, "Not permitted") \
		v(notAccessible, "Not accessible") \
		v(notSynchronizable,"File descriptor is not synchronizable") \
		/* Argument constraints */ \
		v(tooManyBufferBytes, "Too many bytes") \
		v(notEnoughBufferBytes, "Not enough bytes") \
		v(tooManyBuffers, "Too many buffers") \
		v(notEnoughBits, "Not enough bits") \
		v(exceededFileSizeLimit, "File is too large") \
		/* Resource exhaustion errors */ \
		v(outOfSystemFDs, "Out of system file descriptors") \
		v(outOfProcessFDs, "Out of process file descriptors") \
		V(outOfMemory, "Out of memory") \
		v(outOfQuota, "Out of quota") \
		v(outOfFreeSpace, "Out of free space") \
		v(outOfLinksToParentDir, "Out of links to parent directory") \
		/* Path errors */ \
		v(invalidNameCharacter, "Invalid filename character") \
		v(nameTooLong, "Filename is too long") \
		v(tooManyLinksInPath, "Path follows too many links") \
		/* File state errors */ \
		v(alreadyExists, "Already exists") \
		v(doesNotExist, "Doesn't exist") \
		v(isDirectory, "Is a directory") \
		v(isNotDirectory, "Isn't a directory") \
		v(isNotEmpty, "Directory isn't empty") \
		v(brokenPipe, "Pipe is broken") \
		v(missingDevice, "Device is missing") \
		v(busy, "Device or resource busy")

	enum class Result : I32
	{
		#define V(name, description) name,
		ENUM_VFS_RESULTS(V)
		#undef V
	};

	// clang-format on

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
		virtual Result close() = 0;

		virtual Result seek(I64 offset, SeekOrigin origin, U64* outAbsoluteOffset = nullptr) = 0;

		virtual Result readv(const IOReadBuffer* buffers,
							 Uptr numBuffers,
							 Uptr* outNumBytesRead = nullptr)
			= 0;
		virtual Result writev(const IOWriteBuffer* buffers,
							  Uptr numBuffers,
							  Uptr* outNumBytesWritten = nullptr)
			= 0;

		virtual Result preadv(const IOReadBuffer* buffers,
							  Uptr numBuffers,
							  U64 offset,
							  Uptr* outNumBytesRead = nullptr)
			= 0;
		virtual Result pwritev(const IOWriteBuffer* buffers,
							   Uptr numBuffers,
							   U64 offset,
							   Uptr* outNumBytesWritten = nullptr)
			= 0;

		virtual Result sync(SyncType type) = 0;

		virtual Result getFDInfo(FDInfo& outInfo) = 0;
		virtual Result getFileInfo(FileInfo& outInfo) = 0;
		virtual Result setFDFlags(const FDFlags& flags) = 0;
		virtual Result setFileSize(U64 numBytes) = 0;

		virtual Result openDir(DirEntStream*& outStream) = 0;

		Result read(void* outData, Uptr numBytes, Uptr* outNumBytesRead = nullptr)
		{
			IOReadBuffer buffer{outData, numBytes};
			return readv(&buffer, 1, outNumBytesRead);
		}
		Result write(const void* data, Uptr numBytes, Uptr* outNumBytesWritten = nullptr)
		{
			IOWriteBuffer buffer{data, numBytes};
			return writev(&buffer, 1, outNumBytesWritten);
		}

	protected:
		virtual ~FD() {}
	};

	struct FileSystem
	{
		virtual ~FileSystem() {}

		virtual Result open(const std::string& absolutePathName,
							FileAccessMode accessMode,
							FileCreateMode createMode,
							FD*& outFD,
							const FDFlags& flags = FDFlags{})
			= 0;

		virtual Result getInfo(const std::string& absolutePathName, FileInfo& outInfo) = 0;

		virtual Result openDir(const std::string& absolutePathName, DirEntStream*& outStream) = 0;

		virtual Result unlinkFile(const std::string& absolutePathName) = 0;
		virtual Result removeDir(const std::string& absolutePathName) = 0;
		virtual Result createDir(const std::string& absolutePathName) = 0;
	};

	VFS_API const char* describeResult(Result result);
}}
