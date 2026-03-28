#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	const char testData[] = "File stat test data.";
	const size_t testDataLen = sizeof(testData) - 1;

	int fd = open("fdstat_test.txt", O_RDWR | O_CREAT | O_TRUNC);
	if(fd == -1)
	{
		fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
		return 1;
	}

	ssize_t written = write(fd, testData, testDataLen);
	if(written != (ssize_t)testDataLen)
	{
		fprintf(stderr, "Failed to write: %s\n", strerror(errno));
		return 1;
	}

	struct stat fileStat;
	if(fstat(fd, &fileStat) != 0)
	{
		fprintf(stderr, "fstat failed: %s\n", strerror(errno));
		return 1;
	}

	printf("st_size: %" PRIu64 "\n", (uint64_t)fileStat.st_size);
	if((uint64_t)fileStat.st_size != testDataLen)
	{
		fprintf(stderr,
				"Unexpected file size: expected %zu, got %" PRIu64 "\n",
				testDataLen,
				(uint64_t)fileStat.st_size);
		return 1;
	}

	if(!S_ISREG(fileStat.st_mode))
	{
		fprintf(stderr, "File type is not regular file, st_mode: %u\n", fileStat.st_mode);
		return 1;
	}
	printf("File type: regular file\n");

	printf("st_nlink: %" PRIu64 "\n", (uint64_t)fileStat.st_nlink);
	printf("st_ino: %" PRIu64 "\n", (uint64_t)fileStat.st_ino);

	if(close(fd) == -1)
	{
		fprintf(stderr, "Failed to close: %s\n", strerror(errno));
		return 1;
	}

	printf("fd_fdstat: passed\n");
	return 0;
}
