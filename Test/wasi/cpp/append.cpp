#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s <output file> <string>\n", argv[0]);
		return 1;
	}

	size_t stringLength = strlen(argv[2]);

	int fd = open(argv[1], O_WRONLY | O_APPEND | O_CREAT);
	if(fd == -1)
	{
		fprintf(stderr, "Failed to open '%s' for writing: %s\n", argv[1], strerror(errno));
		return 1;
	}
	ssize_t writeResult = write(fd, argv[2], stringLength);
	if(writeResult != (ssize_t)stringLength)
	{
		fprintf(stderr,
				"Failed to write string to '%s': write returned %zi, errno = %s.\n",
				argv[1],
				writeResult,
				strerror(errno));
		return 1;
	}

	const char newline = '\n';
	writeResult = write(fd, &newline, 1);
	if(writeResult != 1)
	{
		fprintf(stderr,
				"Failed to write newline to '%s': write returned %zi, errno = %s.\n",
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
