#include <stdio.h>
#include <stdlib.h>

extern char** environ;

int main(int argc, char** argv)
{
	int envCount = 0;
	if(environ)
	{
		for(char** env = environ; *env != NULL; ++env) { ++envCount; }
	}
	printf("environ_count: %d\n", envCount);

	const char* pathValue = getenv("PATH");
	if(pathValue) { printf("PATH: %s\n", pathValue); }
	else
	{
		printf("PATH: not set\n");
	}

	printf("environ: passed\n");
	return 0;
}
