#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if(fd == -1)
	{
		printf("Failed to open '%s' for reading: %s\n", argv[1], strerror(errno));
		return 1;
	}

	struct stat fileStat;
	if(fstat(fd, &fileStat))
	{
		printf("Failed to get file status for '%s': %s", argv[1], strerror(errno));
		return 1;
	}

	const size_t numFileBytes = fileStat.st_size;

	void* mappedFile = mmap(nullptr, numFileBytes, PROT_READ, MAP_PRIVATE, fd, 0);
	if(mappedFile == MAP_FAILED)
	{
		printf("Failed to memory map '%s': %s", argv[1], strerror(errno));
		return 1;
	}

	const int closeResult = close(fd);
	if(closeResult)
	{
		printf("Failed to close '%s': fclose returned %i.\n", argv[1], closeResult);
		return 1;
	}

	const ssize_t writeResult = write(1, mappedFile, numFileBytes);
	if(writeResult == -1)
	{
		printf("Failed to write to stdout: %s.\n", strerror(errno));
		return 1;
	}

	if(munmap(mappedFile, numFileBytes))
	{
		printf("Failed to unmap memory: %s", strerror(errno));
		return 1;
	}

	return 0;
}
