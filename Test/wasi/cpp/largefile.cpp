#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wasi/api.h>

static const char string3[] = "Hello 3GB!";
static const char string6[] = "Hello 6GB!";
static const char string9[] = "Hello 9GB!";

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

	int setSizeResult = __wasi_fd_filestat_set_size(fd, int64_t(10) * 1024 * 1024 * 1024);
	if(setSizeResult)
	{
		fprintf(stderr, "Couldn't resize '%s': %s\n", argv[1], strerror(setSizeResult));
		return 1;
	}

	const off_t seekResult = lseek(fd, int64_t(9) * 1024 * 1024 * 1024, SEEK_SET);
	if(seekResult != int64_t(9) * 1024 * 1024 * 1024)
	{
		fprintf(stderr, "Couldn't seek to 9GB: %s\n", strerror(seekResult));
		return 1;
	}

	{
		const size_t stringLength9 = sizeof(string9);
		const ssize_t writeResult1 = write(fd, string9, stringLength9);
		if(writeResult1 != stringLength9)
		{
			fprintf(stderr,
					"Couldn't write to '%s': write returned %zi, errno: %s\n",
					argv[1],
					writeResult1,
					strerror(errno));
			return 1;
		}
	}

	{
		const size_t stringLength3 = sizeof(string3) - 1;
		const ssize_t writeResult2
			= pwrite(fd, string3, stringLength3, int64_t(3) * 1024 * 1024 * 1024);
		if(writeResult2 != stringLength3)
		{
			fprintf(stderr,
					"Couldn't write to '%s': pwrite(3GB) returned %zi, errno: %s\n",
					argv[1],
					writeResult2,
					strerror(errno));
			return 1;
		}
	}

	{
		const size_t stringLength6 = sizeof(string6) - 1;
		const ssize_t writeResult3
			= pwrite(fd, string6, stringLength6, int64_t(6) * 1024 * 1024 * 1024);
		if(writeResult3 != stringLength6)
		{
			fprintf(stderr,
					"Couldn't write to '%s': pwrite(6GB) returned %zi, errno: %s\n",
					argv[1],
					writeResult3,
					strerror(errno));
			return 1;
		}
	}

	{
		char readBuffer3[sizeof(string3) + 1];
		const ssize_t readResult1
			= pread(fd, readBuffer3, sizeof(string3), int64_t(3) * 1024 * 1024 * 1024);
		if(readResult1 != sizeof(string3))
		{
			fprintf(stderr,
					"Couldn't read from '%s': pread(3GB) returned %zi, errno: %s\n",
					argv[1],
					readResult1,
					strerror(errno));
			return 1;
		}
		readBuffer3[sizeof(string3)] = 0;
		printf("pread(3GB): %s\n", readBuffer3);
	}

	{
		char readBuffer6[sizeof(string6) + 1];
		const ssize_t readResult2
			= pread(fd, readBuffer6, sizeof(string6), int64_t(6) * 1024 * 1024 * 1024);
		if(readResult2 != sizeof(string6))
		{
			fprintf(stderr,
					"Couldn't read from '%s': pread(6GB) returned %zi, errno: %s\n",
					argv[1],
					readResult2,
					strerror(errno));
			return 1;
		}
		readBuffer6[sizeof(string6)] = 0;
		printf("pread(6GB): %s\n", readBuffer6);
	}

	{
		char readBuffer9[sizeof(string9) + 1];
		const ssize_t readResult3
			= pread(fd, readBuffer9, sizeof(string9), int64_t(9) * 1024 * 1024 * 1024);
		if(readResult3 != sizeof(string9))
		{
			fprintf(stderr,
					"Couldn't read from '%s': pread(9GB) returned %zi, errno: %s\n",
					argv[1],
					readResult3,
					strerror(errno));
			return 1;
		}
		readBuffer9[sizeof(string9)] = 0;
		printf("pread(9GB): %s\n", readBuffer9);
	}

	if(close(fd) == -1)
	{
		fprintf(stderr, "Couldn't close '%s': %s\n", argv[1], strerror(errno));
		return 1;
	}

	return 0;
}
