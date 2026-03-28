#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void testClockRes(clockid_t clockId, const char* name)
{
	struct timespec res;
	if(clock_getres(clockId, &res))
	{
		fprintf(stderr, "clock_getres(%s) failed: %s\n", name, strerror(errno));
		exit(1);
	}

	printf("%s resolution: %ld s + %ld ns\n", name, (long)res.tv_sec, res.tv_nsec);

	if(res.tv_sec == 0 && res.tv_nsec == 0)
	{
		fprintf(stderr, "%s: resolution is zero\n", name);
		exit(1);
	}
}

int main(int argc, char** argv)
{
	testClockRes(CLOCK_REALTIME, "CLOCK_REALTIME");
	testClockRes(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");

	printf("clock_res: passed\n");
	return 0;
}
