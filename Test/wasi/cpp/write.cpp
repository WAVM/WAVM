#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s <output file> <string>\n", argv[0]);
		return 1;
	}

	FILE* file = fopen(argv[1], "wb");
	if(!file)
	{
		fprintf(stderr, "Failed to open '%s' for writing: %s\n", argv[1], strerror(errno));
		return 1;
	}

	size_t stringLength = strlen(argv[2]);
	size_t writeResult = fwrite(argv[2], 1, stringLength, file);
	if(writeResult != stringLength)
	{
		fprintf(stderr,
				"Failed to write string to '%s': fwrite returned %zu, errno = %s.\n",
				argv[1],
				writeResult,
				strerror(errno));
		return 1;
	}

	const char newline[] = "\n";
	writeResult = fwrite(newline, 1, 1, file);
	if(writeResult != 1)
	{
		fprintf(stderr,
				"Failed to write newline to '%s': fwrite returned %zu, errno = %s.\n",
				argv[1],
				writeResult,
				strerror(errno));
		return 1;
	}

	int closeResult = fclose(file);
	if(closeResult != 0)
	{
		fprintf(stderr, "Failed to close '%s': fclose returned %i.\n", argv[1], closeResult);
		return 1;
	}

	return 0;
}
