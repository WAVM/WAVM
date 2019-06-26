#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
		return 1;
	}

	FILE* file = fopen(argv[1], "rb");
	if(!file)
	{
		printf("Failed to open '%s' for reading: %s\n", argv[1], strerror(errno));
		return 1;
	}

	while(true)
	{
		char buffer[4096];
		const size_t numBytesRead = fread(buffer, 1, sizeof(buffer), file);
		if(numBytesRead != sizeof(buffer) && ferror(file))
		{
			printf("Failed to read from '%s': %s.\n", argv[1], strerror(errno));
			return 1;
		}

		size_t numWrittenBytes = 0;
		while(numWrittenBytes < numBytesRead)
		{
			const ssize_t writeResult
				= write(1, buffer + numWrittenBytes, numBytesRead - numWrittenBytes);
			if(writeResult == -1)
			{
				printf("Failed to write to stdout: %s.\n", strerror(errno));
				return 1;
			}
			else
			{
				numWrittenBytes += writeResult;
			}
		};

		if(numBytesRead < sizeof(buffer)) { break; }
	};

	const int closeResult = fclose(file);
	if(closeResult != 0)
	{
		printf("Failed to close '%s': fclose returned %i.\n", argv[1], closeResult);
		return 1;
	}

	return 0;
}
