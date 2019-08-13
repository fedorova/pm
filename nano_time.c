#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "nano_time.h"

#define USE_RDTSC
#define NS_IN_SECOND 1000000000

#ifdef __MACH__
#include <mach/mach_time.h>
static uint64_t mach_gethrtime() {
	static bool scale = true;
	static double scaling_factor = 1.0;
	static mach_timebase_info_data_t timebase = {0, 0};
	uint64_t time;

	/* This function call is very expensive so it must
	 * be done only once. That being said, this implementation
	 * is not thread safe. We don't really care, because any
	 * number of calls to this function will produce the same
	 * result, so we don't care if one thread overwrites another.
	 */
	if (timebase.denom == 0) {
		mach_timebase_info(&timebase);
		scaling_factor = ((double)timebase.numer)/
			((double)timebase.denom);
		if (scaling_factor == 1.0 || scaling_factor == 0)
			scale = false;
	}

	time = mach_absolute_time();
	if (scale)
		return ((double)time) * scaling_factor;
	else
		return time;
}
#endif

#ifdef __i386
extern __inline__ uint64_t rdtsc(void) {
	uint64_t x;
	__asm__ volatile ("rdtsc" : "=A" (x));
	return x;
}
#elif defined __amd64
extern __inline__ uint64_t rdtsc(void) {
	uint64_t a, d;
	__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
	return (d<<32) | a;
}
#endif


uint64_t
nano_time(void) {

#ifdef USE_RDTSC
	return rdtsc();
#else
#ifdef __linux__
	struct timespec ts;
	if( clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		return ts.tv_sec * NS_IN_SECOND + ts.tv_nsec;
	else
		return 0;
#elif defined __MACH__
	return mach_gethrtime();
#endif
#endif
}
