#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wasi/api.h>

static const char string500[] = "Hello 500!";
static const char string5000[] = "Hello 5000!";

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

	int setSizeResult = __wasi_fd_filestat_set_size(fd, 10000);
	if(setSizeResult)
	{
		fprintf(stderr, "Couldn't resize '%s': %s\n", argv[1], strerror(setSizeResult));
		return 1;
	}

	const size_t stringLength5000 = sizeof(string5000) - 1;
	const ssize_t writeResult1 = pwrite(fd, string5000, stringLength5000, 5000);
	if(writeResult1 != stringLength5000)
	{
		fprintf(stderr,
				"Couldn't write to '%s': pwrite(5000) returned %zi, errno: %s\n",
				argv[1],
				writeResult1,
				strerror(errno));
		return 1;
	}

	const size_t stringLength500 = sizeof(string500) - 1;
	const ssize_t writeResult2 = pwrite(fd, string500, stringLength500, 500);
	if(writeResult2 != stringLength500)
	{
		fprintf(stderr,
				"Couldn't write to '%s': pwrite(500) returned %zi, errno: %s\n",
				argv[1],
				writeResult2,
				strerror(errno));
		return 1;
	}

	char readBuffer5000[sizeof(string5000) + 1];
	const ssize_t readResult1 = pread(fd, readBuffer5000, sizeof(string5000), 5000);
	if(readResult1 != sizeof(string5000))
	{
		fprintf(stderr,
				"Couldn't read from '%s': pread(5000) returned %zi, errno: %s\n",
				argv[1],
				readResult1,
				strerror(errno));
		return 1;
	}
	readBuffer5000[sizeof(string5000)] = 0;
	printf("pread(5000): %s\n", readBuffer5000);

	char readBuffer500[sizeof(string500) + 1];
	const ssize_t readResult2 = pread(fd, readBuffer500, sizeof(string500), 500);
	if(readResult2 != sizeof(string500))
	{
		fprintf(stderr,
				"Couldn't read from '%s': pread(500) returned %zi, errno: %s\n",
				argv[1],
				readResult2,
				strerror(errno));
		return 1;
	}
	readBuffer500[sizeof(string500)] = 0;
	printf("pread(500): %s\n", readBuffer500);

	if(close(fd) == -1)
	{
		fprintf(stderr, "Couldn't close '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
