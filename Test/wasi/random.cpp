#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <wasi/api.h>

int main(int argc, char** argv)
{
	unsigned char buffer[1024];

	int result = __wasi_random_get(buffer, sizeof(buffer));
	if(result)
	{
		fprintf(stderr, "random_get failed: %s\n", strerror(result));
		return 1;
	}

	for(ssize_t index = 0; index < sizeof(buffer); ++index) { printf("%02x", buffer[index]); }

	printf("\n");

	return 0;
}
