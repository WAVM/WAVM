#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
		return 1;
	}

	DIR* dir = opendir(argv[1]);
	if(!dir)
	{
		fprintf(stderr, "Couldn't open directory '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	while(true)
	{
		struct dirent* dirent = readdir(dir);
		if(dirent)
		{
			const char* typeString;
			switch(dirent->d_type)
			{
			case DT_BLK: typeString = "DT_BLK"; break;
			case DT_CHR: typeString = "DT_CHR"; break;
			case DT_DIR: typeString = "DT_DIR"; break;
			case DT_FIFO: typeString = "DT_FIFO"; break;
			case DT_LNK: typeString = "DT_LNK"; break;
			case DT_REG: typeString = "DT_REG"; break;
			case DT_UNKNOWN: typeString = "DT_UNKNOWN"; break;
			default: typeString = "?"; break;
			};

			printf("%s : %s -> %" PRIu64 "\n", dirent->d_name, typeString, dirent->d_ino);
		}
		else if(errno)
		{
			fprintf(stderr, "Couldn't read directory '%s': %s\n", argv[1], strerror(errno));
			return 1;
		}
		else
		{
			break;
		}
	}

	printf("\n");

	if(closedir(dir))
	{
		fprintf(stderr, "Couldn't close directory '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
