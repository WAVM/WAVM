#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	const char testData[] = "Hello, seek and tell!";
	const size_t testDataLen = sizeof(testData) - 1;

	int fd = open("seek_test.txt", O_RDWR | O_CREAT | O_TRUNC);
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

	off_t pos = lseek(fd, 0, SEEK_CUR);
	if(pos != (off_t)testDataLen)
	{
		fprintf(
			stderr, "Position after write: expected %zu, got %lld\n", testDataLen, (long long)pos);
		return 1;
	}
	printf("Position after write: %lld\n", (long long)pos);

	pos = lseek(fd, 0, SEEK_SET);
	if(pos != 0)
	{
		fprintf(stderr, "Position after seek to beginning: expected 0, got %lld\n", (long long)pos);
		return 1;
	}
	printf("Position after seek to beginning: %lld\n", (long long)pos);

	char readBuffer[64];
	memset(readBuffer, 0, sizeof(readBuffer));
	ssize_t bytesRead = read(fd, readBuffer, testDataLen);
	if(bytesRead != (ssize_t)testDataLen)
	{
		fprintf(stderr, "Failed to read: %s\n", strerror(errno));
		return 1;
	}

	if(memcmp(readBuffer, testData, testDataLen) != 0)
	{
		fprintf(stderr, "Data mismatch: expected '%s', got '%s'\n", testData, readBuffer);
		return 1;
	}
	printf("Read back: %s\n", readBuffer);

	pos = lseek(fd, -5, SEEK_END);
	if(pos != (off_t)(testDataLen - 5))
	{
		fprintf(stderr,
				"Position after SEEK_END -5: expected %zu, got %lld\n",
				testDataLen - 5,
				(long long)pos);
		return 1;
	}
	printf("Position after SEEK_END -5: %lld\n", (long long)pos);

	FILE* file = fopen("seek_test2.txt", "w+");
	if(!file)
	{
		fprintf(stderr, "Failed to fopen: %s\n", strerror(errno));
		return 1;
	}

	if(fwrite(testData, 1, testDataLen, file) != testDataLen)
	{
		fprintf(stderr, "Failed to fwrite: %s\n", strerror(errno));
		return 1;
	}

	long tellPos = ftell(file);
	if(tellPos != (long)testDataLen)
	{
		fprintf(stderr, "ftell after write: expected %zu, got %ld\n", testDataLen, tellPos);
		return 1;
	}
	printf("ftell after write: %ld\n", tellPos);

	if(fseek(file, 0, SEEK_SET) != 0)
	{
		fprintf(stderr, "fseek failed: %s\n", strerror(errno));
		return 1;
	}

	tellPos = ftell(file);
	if(tellPos != 0)
	{
		fprintf(stderr, "ftell after seek to beginning: expected 0, got %ld\n", tellPos);
		return 1;
	}
	printf("ftell after seek to beginning: %ld\n", tellPos);

	fclose(file);

	if(close(fd) == -1)
	{
		fprintf(stderr, "Failed to close fd: %s\n", strerror(errno));
		return 1;
	}

	printf("fd_seek_tell: passed\n");
	return 0;
}
