#include "WAVM/VFS/SandboxFS.h"
#include <string>
#include "WAVM/Inline/I128.h"
#include "WAVM/VFS/VFS.h"

using namespace WAVM;
using namespace WAVM::VFS;

struct SandboxFS : FileSystem
{
	SandboxFS(FileSystem* inInnerFS, const std::string& inRootPath)
	: innerFS(inInnerFS), rootPath(inRootPath)
	{
		if(rootPath.back() != '/' && rootPath.back() != '\\') { rootPath += '/'; }
	}

	virtual Result open(const std::string& absolutePathName,
						FileAccessMode accessMode,
						FileCreateMode createMode,
						VFD*& outFD,
						const VFDFlags& flags) override
	{
		return innerFS->open(getInnerPath(absolutePathName), accessMode, createMode, outFD, flags);
	}

	virtual Result getInfo(const std::string& absolutePathName, FileInfo& outInfo) override
	{
		return innerFS->getInfo(getInnerPath(absolutePathName), outInfo);
	}
	virtual Result setFileTimes(const std::string& absolutePathName,
								bool setLastAccessTime,
								I128 lastAccessTime,
								bool setLastWriteTime,
								I128 lastWriteTime) override
	{
		return innerFS->setFileTimes(getInnerPath(absolutePathName),
									 setLastAccessTime,
									 lastAccessTime,
									 setLastWriteTime,
									 lastWriteTime);
	}

	virtual Result openDir(const std::string& absolutePathName, DirEntStream*& outStream) override
	{
		return innerFS->openDir(getInnerPath(absolutePathName), outStream);
	}

	virtual Result unlinkFile(const std::string& absolutePathName) override
	{
		return innerFS->unlinkFile(getInnerPath(absolutePathName));
	}

	virtual Result removeDir(const std::string& absolutePathName) override
	{
		return innerFS->removeDir(getInnerPath(absolutePathName));
	}

	virtual Result createDir(const std::string& absolutePathName) override
	{
		return innerFS->createDir(getInnerPath(absolutePathName));
	}

private:
	VFS::FileSystem* innerFS;
	std::string rootPath;

	std::string getInnerPath(const std::string& absolutePathName)
	{
		return rootPath + absolutePathName;
	}
};

FileSystem* VFS::makeSandboxFS(FileSystem* innerFS, const std::string& innerRootPath)
{
	return new SandboxFS(innerFS, innerRootPath);
}
