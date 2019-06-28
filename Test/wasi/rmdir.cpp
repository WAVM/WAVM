#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
		return 1;
	}

	if(rmdir(argv[1]))
	{
		fprintf(
			stderr, "Failed to remove directory '%s': %s (%i)\n", argv[1], strerror(errno), errno);
		return 1;
	}

	return 0;
}
