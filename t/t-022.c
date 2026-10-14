/* Module registry lookups must coexist with creation/cleanup of other runtimes. */
#include "hawk-prv.h"
#include "tap.h"

#if defined(HAVE_PTHREAD) && (HAWK_SIZEOF_PTHREAD_MUTEX_T > 0)
#include <pthread.h>

#define NUM_WORKERS 8
#define NUM_CALLS 10000
#define NUM_CHURN 128
#define NUM_RUNTIMES 32

#ifndef TEST_MODLIBDIRS
#define TEST_MODLIBDIRS "../lib/.libs:../mod/.libs"
#endif

typedef struct gate_t gate_t;
struct gate_t
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int ready;
};

typedef struct job_t job_t;
struct job_t
{
	hawk_rtx_t* rtx;
	gate_t* gate;
	hawk_int_t bad;
	int failed;
};

static int sample (job_t* job)
{
	hawk_val_t* arg;
	hawk_val_t* result;
	hawk_int_t value;
	int good;

	arg = hawk_rtx_makeintval(job->rtx, job->bad);
	if (!arg) return -1;
	hawk_rtx_refupval(job->rtx, arg);
	result = hawk_rtx_callwithbcstr(job->rtx, "sample", &arg, 1);
	hawk_rtx_refdownval(job->rtx, arg);
	if (!result) return -1;
	good = hawk_rtx_valtoint(job->rtx, result, &value) == 0 && value == job->bad;
	hawk_rtx_refdownval(job->rtx, result);
	return good? 0: -1;
}

static void* run_job (void* ctx)
{
	job_t* job = ctx;
	int i;

	pthread_mutex_lock(&job->gate->mutex);
	while (!job->gate->ready) pthread_cond_wait(&job->gate->cond, &job->gate->mutex);
	pthread_mutex_unlock(&job->gate->mutex);
	for (i = 0; i < NUM_CALLS; i++)
	{
		if (sample(job) <= -1)
		{
			job->failed = 1;
			break;
		}
	}
	return HAWK_NULL;
}

static int check_module (const hawk_bch_t* source)
{
	hawk_t* hawk;
	hawk_parsestd_t input[2];
	job_t jobs[NUM_WORKERS];
	pthread_t threads[NUM_WORKERS];
	hawk_rtx_t* transient[NUM_RUNTIMES];
	gate_t gate;
	int started = 0;
	int good = 0;
	int i, j;

	HAWK_MEMSET(jobs, 0, HAWK_SIZEOF(jobs));
	HAWK_MEMSET(transient, 0, HAWK_SIZEOF(transient));
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	gate.ready = 0;
	if (pthread_mutex_init(&gate.mutex, HAWK_NULL) != 0) return 0;
	if (pthread_cond_init(&gate.cond, HAWK_NULL) != 0)
	{
		pthread_mutex_destroy(&gate.mutex);
		return 0;
	}
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) goto done;
	if (hawk_setopt(hawk, HAWK_OPT_MODLIBDIRS, HAWK_T(TEST_MODLIBDIRS)) <= -1) goto done;
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = hawk_count_bcstr(source);
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1) goto done;
	for (i = 0; i < NUM_WORKERS; i++)
	{
		jobs[i].gate = &gate;
		jobs[i].bad = i % 2;
		jobs[i].rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("module-threads"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
		if (!jobs[i].rtx || sample(&jobs[i]) <= -1) goto done;
	}
	/* Warm up calls before sharing the parsed tree. Each worker owns one
	 * runtime and diagnostic; only the registry is shared with churn below. */
	for (i = 0; i < NUM_WORKERS; i++)
	{
		if (pthread_create(&threads[i], HAWK_NULL, run_job, &jobs[i]) != 0) break;
		started++;
	}
	pthread_mutex_lock(&gate.mutex);
	gate.ready = 1;
	pthread_cond_broadcast(&gate.cond);
	pthread_mutex_unlock(&gate.mutex);
	if (started != NUM_WORKERS) goto done;
	for (i = 0; i < NUM_CHURN; i++)
	{
		/* Keeping a batch live forces insert/delete rotations instead of
		 * repeatedly reusing one tree leaf and one allocation address. */
		for (j = 0; j < NUM_RUNTIMES; j++)
		{
			transient[j] = hawk_rtx_openstd(hawk, 0, HAWK_T("module-churn"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
			if (!transient[j]) goto done;
		}
		for (j = NUM_RUNTIMES; j > 0;)
		{
			--j;
			hawk_rtx_close(transient[j]);
			transient[j] = HAWK_NULL;
		}
	}
	good = 1;

done:
	for (i = 0; i < started; i++)
	{
		if (pthread_join(threads[i], HAWK_NULL) != 0) bail_out("cannot join module worker");
		if (jobs[i].failed) good = 0;
	}
	if (!good && hawk) diag(hawk_geterrbmsg(hawk));
	for (i = 0; i < NUM_RUNTIMES; i++)
		if (transient[i]) hawk_rtx_close(transient[i]);
	for (i = 0; i < NUM_WORKERS; i++)
	{
		if (jobs[i].rtx)
		{
			if (!good) diag(hawk_rtx_geterrbmsg(jobs[i].rtx));
			hawk_rtx_close(jobs[i].rtx);
		}
	}
	if (hawk) hawk_close(hawk);
	pthread_cond_destroy(&gate.cond);
	pthread_mutex_destroy(&gate.mutex);
	return good;
}
#endif

int main (void)
{
#if defined(HAVE_PTHREAD) && (HAWK_SIZEOF_PTHREAD_MUTEX_T > 0)
	plan(4);
#if defined(HAWK_ENABLE_MOD_MYSQL)
	OK(check_module("function sample(bad) { if(bad) { mysql::free_result(-1); mysql::stmt_close(-1); } return length(mysql::errmsg()) > 0; }"),
		"MySQL registry and diagnostics survive concurrent runtime churn");
#else
	skip("MySQL module disabled", 1);
#endif
#if defined(HAWK_ENABLE_MOD_FFI)
	OK(check_module("function sample(bad) { if(bad) ffi::close(-1); return length(ffi::errmsg()) > 0; }"),
		"FFI registry and diagnostics survive concurrent runtime churn");
#else
	skip("FFI module disabled", 1);
#endif
#if defined(HAWK_ENABLE_MOD_MEMC)
	OK(check_module("function sample(bad) { if(bad) memc::close(-1); return length(memc::errmsg()) > 0; }"),
		"Memcached registry and diagnostics survive concurrent runtime churn");
#else
	skip("Memcached module disabled", 1);
#endif
#if defined(HAWK_ENABLE_MOD_UCI)
	OK(check_module("function sample(bad) { if(bad) uci::close(-1); return uci::errno() != 0; }"),
		"UCI registry and diagnostics survive concurrent runtime churn");
#else
	skip("UCI module disabled", 1);
#endif
#else
	skip_all("requires pthreads and native mutex support");
#endif
	return exit_status();
}
