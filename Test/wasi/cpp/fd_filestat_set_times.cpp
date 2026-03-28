#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wasi/api.h>

static void printUsage(char* exe)
{
	printf("Usage: %s <file> <last access time> <last write time>\n", exe);
}

int main(int argc, char** argv)
{
	if(argc != 4)
	{
		printUsage(argv[0]);
		return 1;
	}

	__wasi_fstflags_t flags = 0;
	__wasi_timestamp_t lastAccessTime = 0;
	__wasi_timestamp_t lastWriteTime = 0;

	if(!strcmp(argv[2], "now")) { flags |= __WASI_FSTFLAGS_ATIM_NOW; }
	else if(strcmp(argv[2], "nochange"))
	{
		char* end;
		errno = 0;
		lastAccessTime = strtoull(argv[2], &end, 10);
		if(*end != 0 || errno)
		{
			printUsage(argv[0]);
			return 1;
		}
		flags |= __WASI_FSTFLAGS_ATIM;
	}

	if(!strcmp(argv[3], "now")) { flags |= __WASI_FSTFLAGS_MTIM_NOW; }
	else if(strcmp(argv[3], "nochange"))
	{
		char* end;
		errno = 0;
		lastWriteTime = strtoull(argv[3], &end, 10);
		if(*end != 0 || errno)
		{
			printUsage(argv[0]);
			return 1;
		}
		flags |= __WASI_FSTFLAGS_MTIM;
	}

	int fd = open(argv[1], O_WRONLY);
	if(fd == -1)
	{
		fprintf(stderr, "Couldn't open '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	int setTimesResult = __wasi_fd_filestat_set_times(fd, lastAccessTime, lastWriteTime, flags);
	if(setTimesResult)
	{
		fprintf(stderr, "Couldn't set times on '%s': %s\n", argv[1], strerror(setTimesResult));
		return 1;
	}

	if(close(fd) == -1)
	{
		fprintf(stderr, "Couldn't close '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
