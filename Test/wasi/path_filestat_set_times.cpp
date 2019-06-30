#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void printUsage(char* exe)
{
	printf("Usage: %s <file> <last access time> <last write time>\n", exe);
}

bool parseTimeSpec(const char* string, struct timespec& outTimeSpec)
{
	if(!strcmp(string, "now")) { outTimeSpec.tv_nsec = UTIME_NOW; }
	else if(!strcmp(string, "nochange"))
	{
		outTimeSpec.tv_nsec = UTIME_OMIT;
	}
	else
	{
		char* end;
		errno = 0;
		unsigned long long lastAccessTime = strtoull(string, &end, 10);
		if(*end != 0 || errno) { return false; }
		outTimeSpec.tv_sec = time_t(lastAccessTime / 1000000000);
		outTimeSpec.tv_nsec = long(lastAccessTime % 1000000000);
	}
	return true;
}

int main(int argc, char** argv)
{
	if(argc != 4)
	{
		printUsage(argv[0]);
		return 1;
	}

	struct timespec timespecs[2];
	if(!parseTimeSpec(argv[2], timespecs[0]))
	{
		printUsage(argv[0]);
		return 1;
	}
	if(!parseTimeSpec(argv[3], timespecs[1]))
	{
		printUsage(argv[0]);
		return 1;
	}

	int rootFD = open("/", O_RDONLY);
	if(rootFD == -1)
	{
		fprintf(stderr, "Couldn't open root directory: %s\n", strerror(errno));
		return 1;
	}

	if(utimensat(rootFD, argv[1], timespecs, 0) == -1)
	{
		fprintf(stderr, "Couldn't set times on '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
