#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s <source> <dest>\n", argv[0]);
		return 1;
	}

	if(rename(argv[1], argv[2]))
	{ fprintf(stderr, "Failed to rename '%s' to '%s': %s\n", argv[1], argv[2], strerror(errno)); }

	return 0;
}
