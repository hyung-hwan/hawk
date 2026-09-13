/* Native stack exhaustion must return an error, unwind, and permit reuse.
 * Parse/open on the main thread, then execute the same rtx on smaller stacks. */
#include <hawk.h>
#include <hawk-utl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tap.h"

#if defined(_WIN32)
#define TEST_CSTACK
#elif defined(__HAIKU__) && defined(HAVE_FIND_THREAD) && defined(HAVE_GET_THREAD_INFO)
#define TEST_CSTACK
#elif defined(__APPLE__) && defined(HAVE_PTHREAD) && defined(HAVE_PTHREAD_GET_STACKADDR_NP) && defined(HAVE_PTHREAD_GET_STACKSIZE_NP)
#define TEST_CSTACK
#elif defined(__OpenBSD__) && defined(HAVE_PTHREAD) && defined(HAVE_PTHREAD_STACKSEG_NP)
#define TEST_CSTACK
#elif defined(__linux__) && defined(HAVE_PTHREAD) && defined(HAVE_PTHREAD_GETATTR_NP) && defined(HAVE_PTHREAD_ATTR_GETSTACK) && defined(HAVE_PTHREAD_ATTR_GETGUARDSIZE)
#define TEST_CSTACK
#elif (defined(__FreeBSD__) || defined(__NetBSD__)) && defined(HAVE_PTHREAD) && defined(HAVE_PTHREAD_ATTR_GET_NP) && defined(HAVE_PTHREAD_ATTR_GETSTACK) && defined(HAVE_PTHREAD_ATTR_GETGUARDSIZE)
#define TEST_CSTACK
#endif

#if defined(TEST_CSTACK)
#	if defined(_WIN32)
#		include <windows.h>
#	else
#		include <pthread.h>
#		include <sys/mman.h>
#		include <unistd.h>
#		if !defined(MAP_ANONYMOUS)
#			define MAP_ANONYMOUS MAP_ANON
#		endif
#	endif
#endif

#if defined(TEST_CSTACK)
static int reenter (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* v;
	(void)fi;
	v = hawk_rtx_callwithbcstr(rtx, "callback", HAWK_NULL, 0);
	if (!v) return -1;
	hawk_rtx_setretval(rtx, v);
	hawk_rtx_refdownval(rtx, v);
	return 0;
}

struct job_t
{
	hawk_rtx_t* rtx;
	const char* name; /* NULL exercises hawk_rtx_loop() */
	int passed;
};

static void* exhaust (void* data)
{
	struct job_t* job = (struct job_t*)data;
	hawk_val_t* v;
	v = job->name? hawk_rtx_callwithbcstr(job->rtx, job->name, HAWK_NULL, 0): hawk_rtx_loop(job->rtx);
	job->passed = !v && hawk_rtx_geterrnum(job->rtx) == HAWK_ESTACK &&
		strstr(hawk_rtx_geterrbmsg(job->rtx), "native C stack limit reached") != HAWK_NULL;
	if (!job->passed) printf("# unexpected result: %s\n", hawk_rtx_geterrbmsg(job->rtx));
	if (v) hawk_rtx_refdownval(job->rtx, v);
	return HAWK_NULL;
}

#if defined(_WIN32)
static DWORD WINAPI thread_main (LPVOID data)
{
	exhaust(data);
	return 0;
}
#endif

static int on_stack (struct job_t* job, size_t size)
{
#if defined(_WIN32)
	HANDLE thread = CreateThread(HAWK_NULL, size, thread_main, job, STACK_SIZE_PARAM_IS_A_RESERVATION, HAWK_NULL);
	DWORD n;
	if (!thread) return -1;
	n = WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	return n == WAIT_OBJECT_0? 0: -1;
#else
	pthread_attr_t attr;
	pthread_t thread;
	void* stack;
	long page = sysconf(_SC_PAGESIZE);
	int n, started = 0;
	if (page <= 0) return -1;
	/* Supply the exact allocation: pthreads may otherwise reuse a cached,
	 * larger stack and turn the small-stack regression into a false pass. */
	stack = mmap(HAWK_NULL, size + 2 * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (stack == MAP_FAILED) return -1;
	if (mprotect((char*)stack + page, size, PROT_READ | PROT_WRITE) != 0)
	{
		munmap(stack, size + 2 * page);
		return -1;
	}
	n = pthread_attr_init(&attr);
	if (n == 0)
	{
		n = pthread_attr_setstack(&attr, (char*)stack + page, size);
		if (n == 0)
		{
			n = pthread_create(&thread, &attr, exhaust, job);
			started = (n == 0);
		}
		pthread_attr_destroy(&attr);
		if (n == 0) n = pthread_join(thread, HAWK_NULL);
	}
	/* A failed join cannot prove that the worker stopped using this stack. */
	if (!started || n == 0) munmap(stack, size + 2 * page);
	return n == 0? 0: -1;
#endif
}

static void check_reuse (hawk_rtx_t* rtx)
{
	hawk_val_t* v;
	hawk_int_t n = 0;
	v = hawk_rtx_callwithbcstr(rtx, "answer", HAWK_NULL, 0);
	OK(v && hawk_rtx_valtoint(rtx, v, &n) == 0 && n == 42, "rtx reusable on main thread after exhaustion");
	if (v) hawk_rtx_refdownval(rtx, v);
}
#endif

int main (void)
{
#if defined(TEST_CSTACK)
	static const char prefix[] =
		"@pragma stack_limit 1048576\n"
		"function direct() { @local s; s = sprintf(\"value-%d\", 1); return 1 + direct(); }\n"
		"function mutual() { return other(); }\n"
		"function other() { return mutual(); }\n"
		"function callback() { return host_reenter(); }\n"
		"function answer() { return 42; }\n"
		"BEGIN { direct(); }\n"
		"function expression() { @local n; n = 1; return n";
	static const char* names[] = { "direct", "mutual", "callback", "expression", HAWK_NULL };
	static const size_t sizes[] = { 128 * 1024, 256 * 1024, 512 * 1024 };
	hawk_t* hawk;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_parsestd_t in[2];
	hawk_fnc_bspec_t spec;
	hawk_oow_t unlimited = 0;
	struct job_t job;
	char* src;
	size_t i, j, len;
	int n;

	no_plan();
	hawk = hawk_openstd(0, HAWK_NULL);
	OK(hawk != HAWK_NULL, "open interpreter");
	if (!hawk) return exit_status();
	/* Make depth/value-stack limits incapable of masking native exhaustion. */
	hawk_setopt(hawk, HAWK_OPT_DEPTH_RECURS_RUN, &unlimited);
	memset(&spec, 0, sizeof(spec));
	spec.impl = reenter;
	OK(hawk_addfncwithbcstr(hawk, "host_reenter", &spec) != HAWK_NULL, "register host callback");
	src = (char*)malloc(sizeof(prefix) + 12000);
	OK(src != HAWK_NULL, "allocate expression source");
	if (!src) goto done;
	strcpy(src, prefix);
	len = strlen(src);
	/* Not constant-foldable; evaluates recursively without function calls. */
	for (i = 0; i < 5000; i++) { src[len++] = '+'; src[len++] = 'n'; }
	strcpy(src + len, "; }\n");
	memset(in, 0, sizeof(in));
	in[0].type = HAWK_PARSESTD_BCS;
	in[0].u.bcs.ptr = src;
	in[0].u.bcs.len = strlen(src);
	in[1].type = HAWK_PARSESTD_NULL;
	n = hawk_parsestd(hawk, in, HAWK_NULL);
	OK(n >= 0, "parse recursive functions and long expression");
	free(src);
	if (n < 0) { printf("# %s\n", hawk_geterrbmsg(hawk)); goto done; }
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("t-011"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	OK(rtx != HAWK_NULL, "open runtime on main thread");
	if (!rtx) goto done;
	job.rtx = rtx;
	job.name = "direct";
	job.passed = 0;
	exhaust(&job);
	OK(job.passed, "native exhaustion on main thread");
	check_reuse(rtx);
	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	{
		for (j = 0; j < sizeof(names) / sizeof(names[0]); j++)
		{
			job.name = names[j];
			job.passed = 0;
			printf("# stack=%lu entry=%s\n", (unsigned long)sizes[i], job.name? job.name: "BEGIN");
			n = on_stack(&job, sizes[i]);
			OK(n == 0, "execute on bounded worker stack");
			OK(n == 0 && job.passed, "native stack exhausted gracefully");
			check_reuse(rtx);
		}
	}
	/* An entry with less than the reserve must refuse even trivial work. */
	job.name = "answer";
	job.passed = 0;
	OK(on_stack(&job, 64 * 1024) == 0 && job.passed, "reject entry without stack headroom");
	check_reuse(rtx);
	done:
	if (rtx) hawk_rtx_close(rtx);
	hawk_close(hawk);
	return exit_status();
#else
	static char src[] =
		"function direct() { return 1 + direct(); }\n"
		"function answer() { return 42; }\n";
	hawk_t* hawk;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_parsestd_t in[2];
	hawk_val_t* v;
	hawk_oow_t limit = 32;
	hawk_int_t answer = 0;
	int n;

	no_plan();
	hawk = hawk_openstd(0, HAWK_NULL);
	OK(hawk != HAWK_NULL, "open interpreter for logical-depth fallback");
	if (!hawk) return exit_status();
	OK(hawk_setopt(hawk, HAWK_OPT_DEPTH_RECURS_RUN, &limit) == 0, "set logical recursion limit");
	memset(in, 0, sizeof(in));
	in[0].type = HAWK_PARSESTD_BCS;
	in[0].u.bcs.ptr = src;
	in[0].u.bcs.len = sizeof(src) - 1;
	in[1].type = HAWK_PARSESTD_NULL;
	n = hawk_parsestd(hawk, in, HAWK_NULL);
	OK(n >= 0, "parse fallback test functions");
	if (n < 0) goto fallback_done;
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("t-011"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	OK(rtx != HAWK_NULL, "open runtime for logical-depth fallback");
	if (!rtx) goto fallback_done;
	v = hawk_rtx_callwithbcstr(rtx, "direct", HAWK_NULL, 0);
	OK(!v && hawk_rtx_geterrnum(rtx) == HAWK_EBLKNST, "logical recursion limit stops recursive evaluation");
	if (v) hawk_rtx_refdownval(rtx, v);
	v = hawk_rtx_callwithbcstr(rtx, "answer", HAWK_NULL, 0);
	OK(v && hawk_rtx_valtoint(rtx, v, &answer) == 0 && answer == 42, "runtime reusable after logical-depth exhaustion");
	if (v) hawk_rtx_refdownval(rtx, v);

fallback_done:
	if (rtx) hawk_rtx_close(rtx);
	hawk_close(hawk);
	return exit_status();
#endif
}
