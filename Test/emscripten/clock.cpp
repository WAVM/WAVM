#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char* formatTime(time_t time)
{
	static char buffer[256];
	struct tm* utc = gmtime(&time);
	sprintf(buffer,
			"%u/%u/%u %u:%u:%u UTC",
			1900 + utc->tm_year,
			1 + utc->tm_mon,
			utc->tm_mday,
			utc->tm_hour,
			utc->tm_min,
			utc->tm_sec);
	return buffer;
}

static void printClock(clockid_t clockId, const char* name)
{
	struct timespec ts;
	if(clock_gettime(clockId, &ts))
	{
		fprintf(stderr, "clock_gettime(%s) failed: %s\n", name, strerror(errno));
		exit(1);
	}

	if(clockId == CLOCK_REALTIME)
	{ printf("%s: %s + %lu ns\n", name, formatTime(ts.tv_sec), ts.tv_nsec); }
	else
	{
		printf("%s: %" PRIu64 " s + %lu ns\n", name, (uint64_t)ts.tv_sec, ts.tv_nsec);
	}
}

int main(int argc, char** argv)
{
	printClock(CLOCK_REALTIME, "CLOCK_REALTIME");
	printClock(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
	printClock(CLOCK_PROCESS_CPUTIME_ID, "CLOCK_PROCESS_CPUTIME_ID");
	printClock(CLOCK_THREAD_CPUTIME_ID, "CLOCK_THREAD_CPUTIME_ID");
}
