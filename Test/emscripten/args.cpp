#include <stdio.h>

int main(int argc, char** argv)
{
	printf("argc=%i\n", argc);
	for(int argIndex = 0; argIndex <= argc; ++argIndex)
	{ printf("argv[%i]: %s\n", argIndex, argv[argIndex] == NULL ? "<null>" : argv[argIndex]); }

	return 0;
}
