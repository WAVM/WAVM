#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wasi/api.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
	if(fd == -1)
	{
		fprintf(stderr, "Failed to open '%s' for writing: %s\n", argv[1], strerror(errno));
		return 1;
	}

	int result = __wasi_fd_renumber(fd, 1);
	if(result)
	{
		fprintf(stderr, "__wasi_fd_renumber failed: %s\n", strerror(result));
		return 1;
	}

	printf("Hello stdout!\n");

	const char string[] = "Hello file!\n";
	size_t stringLength = sizeof(string) - 1;
	if(write(fd, string, stringLength) == stringLength) { return 1; }
	else
	{
		fprintf(stderr, "Failed to write to '%s': %s\n", argv[1], strerror(errno));
		fprintf(stderr, "*** THIS FAILURE WAS EXPECTED ***\n");
	}

	return 0;
}
