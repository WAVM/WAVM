#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	const char testData[] = "Data to sync to disk.";
	const size_t testDataLen = sizeof(testData) - 1;

	int fd = open("sync_test.txt", O_RDWR | O_CREAT | O_TRUNC);
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
	printf("Wrote %zd bytes\n", written);

	if(fsync(fd) != 0)
	{
		fprintf(stderr, "fsync failed: %s\n", strerror(errno));
		return 1;
	}
	printf("fsync: success\n");

	written = write(fd, testData, testDataLen);
	if(written != (ssize_t)testDataLen)
	{
		fprintf(stderr, "Failed to write again: %s\n", strerror(errno));
		return 1;
	}
	printf("Wrote %zd more bytes\n", written);

	if(fdatasync(fd) != 0)
	{
		fprintf(stderr, "fdatasync failed: %s\n", strerror(errno));
		return 1;
	}
	printf("fdatasync: success\n");

	if(close(fd) == -1)
	{
		fprintf(stderr, "Failed to close: %s\n", strerror(errno));
		return 1;
	}

	printf("fd_sync: passed\n");
	return 0;
}
