#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

const char* formatTime(time_t time)
{
	static char buffer[256];
	struct tm* utc = gmtime(&time);
	sprintf(buffer,
			"%u/%u/%u %u:%u:%u UTC",
			1900 + utc->tm_year,
			1 + utc->tm_mon,
			utc->tm_mday,
			utc->tm_hour,
			utc->tm_min,
			utc->tm_sec);
	return buffer;
}

static void printStat(struct stat& fileStat)
{
	printf("  st_dev: %" PRIu64 "\n", fileStat.st_dev);
	printf("  st_ino: %" PRIu64 "\n", fileStat.st_ino);
	printf("  st_mode: %u\n", fileStat.st_mode);
	printf("  st_nlink: %" PRIu64 "\n", fileStat.st_nlink);
	printf("  st_size: %" PRIu64 "\n", fileStat.st_size);
	printf("  st_atim: %s\n", formatTime(fileStat.st_atime));
	printf("  st_mtim: %s\n", formatTime(fileStat.st_mtime));
	printf("  st_ctim: %s\n", formatTime(fileStat.st_ctime));
}

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: stat <file>\n");
		return 1;
	}

	struct stat fileStat;
	if(stat(argv[1], &fileStat))
	{
		fprintf(stderr, "stat(\"%s\") failed: %s\n", argv[1], strerror(errno));
		return 1;
	}

	printf("stat for '%s':\n", argv[1]);
	printStat(fileStat);

	struct stat stdinStat;
	if(fstat(1, &stdinStat))
	{
		fprintf(stderr, "fstat(stdin) failed: %s\n", strerror(errno));
		return 1;
	}

	printf("stat for stdin:\n");
	printStat(stdinStat);

	return 0;
}
