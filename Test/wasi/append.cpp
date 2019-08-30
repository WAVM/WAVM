#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char string[] = "Hello World!\n";

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
		return 1;
	}

	size_t stringLength = sizeof(string) - 1;

	int fd = open(argv[1], O_WRONLY | O_APPEND | O_CREAT);
	if(fd == -1)
	{
		fprintf(stderr, "Failed to open '%s' for writing: %s\n", argv[1], strerror(errno));
		return 1;
	}
	ssize_t writeResult = write(fd, string, stringLength);
	if(writeResult != stringLength)
	{
		fprintf(stderr,
				"Failed to write string to '%s': write returned %zi, errno = %s.\n",
				argv[1],
				writeResult,
				strerror(errno));
		return 1;
	}
	int closeResult = close(fd);
	if(closeResult != 0)
	{
		fprintf(stderr, "Failed to close '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
