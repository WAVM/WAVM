#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <output file>\n", argv[0]);
		return 1;
	}

	const char string[] = "Hello World!\n";
	size_t stringLength = sizeof(string) - 1;

	FILE* file = fopen(argv[1], "wb");
	if(!file)
	{
		printf("Failed to open '%s' for writing. errno=%s\n", argv[1], strerror(errno));
		return 1;
	}
	size_t writeResult = fwrite(string, 1, stringLength, file);
	if(writeResult != stringLength)
	{
		printf("Failed to write string to '%s': fwrite returned %zu, errno=%s.\n",
			   argv[1],
			   writeResult,
			   strerror(errno));
		return 1;
	}
	int closeResult = fclose(file);
	if(closeResult != 0)
	{
		printf("Failed to close '%s': fclose returned %i.\n", argv[1], closeResult);
		return 1;
	}

	return 0;
}
