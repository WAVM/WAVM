#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wasi/api.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		printf("Usage: %s <file>", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC);
	if(fd == -1)
	{
		fprintf(stderr, "Couldn't open '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	int setSizeResult = __wasi_fd_filestat_set_size(fd, 4201);
	if(setSizeResult)
	{
		fprintf(stderr, "Couldn't resize '%s': %s\n", argv[1], strerror(setSizeResult));
		return 1;
	}

	if(close(fd) == -1)
	{
		fprintf(stderr, "Couldn't close '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
