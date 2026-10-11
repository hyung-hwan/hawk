/* Separate runtimes share the parsed program and its math module. */
#include "hawk-prv.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(HAVE_PTHREAD) && (HAWK_SIZEOF_PTHREAD_MUTEX_T > 0)
#include <pthread.h>

#define NUM_THREADS 4
#define NUM_CALLS 1000

typedef struct job_t job_t;
struct job_t
{
	hawk_rtx_t* rtx;
	hawk_int_t seed;
	hawk_int_t previous[NUM_CALLS];
	int failed;
};

static int sample (hawk_rtx_t* rtx, hawk_int_t seed, hawk_int_t* previous)
{
	hawk_val_t* arg;
	hawk_val_t* result;
	int n;

	arg = hawk_rtx_makeintval(rtx, seed);
	if (!arg) return -1;
	hawk_rtx_refupval(rtx, arg);
	result = hawk_rtx_callwithbcstr(rtx, "sample", &arg, 1);
	hawk_rtx_refdownval(rtx, arg);
	if (!result) return -1;
	n = hawk_rtx_valtoint(rtx, result, previous);
	hawk_rtx_refdownval(rtx, result);
	return n;
}

static void* run_job (void* ctx)
{
	job_t* job = ctx;
	int i;

	for (i = 0; i < NUM_CALLS; i++)
	{
		if (sample(job->rtx, job->seed + i, &job->previous[i]) <= -1)
		{
			job->failed = 1;
			break;
		}
	}
	return HAWK_NULL;
}

int hawk_test_math_threads (const hawk_bch_t* source)
{
	hawk_t* hawk;
	hawk_parsestd_t input[2] = { { 0 } };
	job_t jobs[NUM_THREADS] = { { 0 } };
	pthread_t threads[NUM_THREADS];
	hawk_uint8_t seen[NUM_THREADS * NUM_CALLS + 1] = { 0 };
	hawk_int_t previous;
	int started = 0;
	int ok = 0;
	int i, j;

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) return 1;
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = hawk_count_bcstr(source);
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1) goto done;

	for (i = 0; i < NUM_THREADS; i++)
	{
		jobs[i].rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("math-threads"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
		if (!jobs[i].rtx) goto done;
		jobs[i].seed = i * NUM_CALLS + 1;
		/* Resolve shared call nodes before starting the worker threads. */
		if (sample(jobs[i].rtx, 0, &previous) <= -1) goto done;
	}

	for (i = 0; i < NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], HAWK_NULL, run_job, &jobs[i]) != 0) break;
		started++;
	}
	for (i = 0; i < started; i++)
	{
		if (pthread_join(threads[i], HAWK_NULL) != 0)
		{
			/* Do not close a runtime while its worker might still be using it. */
			fprintf(stderr, "unable to join random-generator worker\n");
			exit(1);
		}
	}
	if (started != NUM_THREADS) goto done;

	/* Returned previous seeds plus the final seed must contain each submitted
	 * seed exactly once. This catches a read/update outside the RNG mutex. */
	if (sample(jobs[0].rtx, 0, &previous) <= -1) goto done;
	if (previous < 0 || previous > NUM_THREADS * NUM_CALLS) goto done;
	seen[previous] = 1;
	for (i = 0; i < NUM_THREADS; i++)
	{
		if (jobs[i].failed) goto done;
		for (j = 0; j < NUM_CALLS; j++)
		{
			previous = jobs[i].previous[j];
			if (previous < 0 || previous > NUM_THREADS * NUM_CALLS || seen[previous]) goto done;
			seen[previous] = 1;
		}
	}
	ok = 1;

done:
	if (!ok)
	{
		fprintf(stderr, "concurrent random generation or previous-seed tracking failed\n");
		fprintf(stderr, "interpreter: %s\n", hawk_geterrbmsg(hawk));
		for (i = 0; i < NUM_THREADS; i++)
			if (jobs[i].rtx) fprintf(stderr, "runtime %d: %s\n", i, hawk_rtx_geterrbmsg(jobs[i].rtx));
	}
	for (i = 0; i < NUM_THREADS; i++)
		if (jobs[i].rtx) hawk_rtx_close(jobs[i].rtx);
	hawk_close(hawk);
	return ok? 0: 1;
}

#else
int hawk_test_math_threads (const hawk_bch_t* source)
{
	puts("requires pthreads and native mutex support");
	return 77;
}
#endif
