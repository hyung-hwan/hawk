/* Exercise the production normalization with the generator's maximum value.
 * A separate module name leaves the real math module available to other tests. */
#if !defined(_GNU_SOURCE)
#	define _GNU_SOURCE
#endif
#include "hawk-prv.h"
#include <stdlib.h>

#if defined(HAVE_INITSTATE_R) && defined(HAVE_SRANDOM_R) && defined(HAVE_RANDOM_R)
static int max_random_r (struct random_data* state, hawk_int32_t* value)
{
	*value = 2147483647;
	return 0;
}
#	define random_r max_random_r
#elif defined(HAVE_RANDOM)
static long max_random (void)
{
	return 2147483647;
}
#	define random max_random
#else
static int max_rand (void)
{
	return RAND_MAX;
}
#	define rand max_rand
#endif

#define hawk_mod_math hawk_test_mod_math
#include "../lib/mod-math.c"
