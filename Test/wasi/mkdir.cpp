#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
		return 1;
	}

	if(mkdir(argv[1], 0666))
	{
		fprintf(
			stderr, "Failed to create directory '%s': %s (%i)\n", argv[1], strerror(errno), errno);
		return 1;
	}

	return 0;
}
