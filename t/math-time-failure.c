/* A failed time query must not expose uninitialized seed data. */
#include "hawk-prv.h"

static int fail_get_ntime (hawk_ntime_t* time)
{
	/* Leave the caller's output untouched, as an OS time-query failure can. */
	return -1;
}

#define hawk_get_ntime fail_get_ntime
#define hawk_mod_math hawk_test_mod_math_time
#include "../lib/mod-math.c"
