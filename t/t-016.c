/* Standalone mux: exercise every compiled backend, including the fallbacks. */
#include <hawk.h>
#include <hawk-mux.h>
#include <stdlib.h>
#include <string.h>
#include "tap.h"

#if defined(_WIN32)
#include <winsock2.h>
#else
#if !defined(__OS2__) && !defined(__DOS__)
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#endif
#endif

#if !defined(__OS2__) && !defined(__DOS__)
typedef struct memory_t memory_t;
struct memory_t
{
	int fail_after;
	int live;
};

static void* alloc_mem (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	memory_t* m = mmgr->ctx;
	void* ptr;

	if (m->fail_after == 0) return HAWK_NULL;
	if (m->fail_after > 0) m->fail_after--;
	ptr = malloc(size);
	if (ptr) m->live++;
	return ptr;
}

static void* realloc_mem (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	memory_t* m = mmgr->ctx;
	void* tmp;
	int fresh = ptr == HAWK_NULL;

	if (m->fail_after == 0) return HAWK_NULL;
	if (m->fail_after > 0) m->fail_after--;
	tmp = realloc(ptr, size);
	if (tmp && fresh) m->live++;
	return tmp;
}

static void free_mem (hawk_mmgr_t* mmgr, void* ptr)
{
	memory_t* m = mmgr->ctx;

	if (ptr) m->live--;
	free(ptr);
}

static int make_pair (hawk_mux_hnd_t* hnd)
{
#if defined(_WIN32)
	SOCKET listener, a, b;
	struct sockaddr_in addr;
	int len = sizeof(addr);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET) return -1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(listener, (struct sockaddr*)&addr, len) == SOCKET_ERROR ||
	    listen(listener, 1) == SOCKET_ERROR ||
	    getsockname(listener, (struct sockaddr*)&addr, &len) == SOCKET_ERROR)
	{
		closesocket(listener);
		return -1;
	}
	a = socket(AF_INET, SOCK_STREAM, 0);
	if (a == INVALID_SOCKET || connect(a, (struct sockaddr*)&addr, len) == SOCKET_ERROR)
	{
		if (a != INVALID_SOCKET) closesocket(a);
		closesocket(listener);
		return -1;
	}
	b = accept(listener, HAWK_NULL, HAWK_NULL);
	closesocket(listener);
	if (b == INVALID_SOCKET)
	{
		closesocket(a);
		return -1;
	}
	hnd[0] = a;
	hnd[1] = b;
	return 0;
#else
	return socketpair(AF_UNIX, SOCK_STREAM, 0, hnd);
#endif
}

static void close_handle (hawk_mux_hnd_t hnd)
{
#if defined(_WIN32)
	closesocket(hnd);
#else
	close(hnd);
#endif
}

static int get_clock (hawk_ntime_t* time)
{
#if defined(_WIN32)
	LARGE_INTEGER counter, frequency;

	/* Hawk's wall clock uses SYSTEMTIME, whose resolution is too coarse for
	 * this timeout test. QPC measures elapsed time independently of UTC. */
	if (!QueryPerformanceCounter(&counter) || !QueryPerformanceFrequency(&frequency)) return -1;
	time->sec = counter.QuadPart / frequency.QuadPart;
	time->nsec = (counter.QuadPart % frequency.QuadPart) * HAWK_NSECS_PER_SEC / frequency.QuadPart;
	return 0;
#else
	return hawk_get_ntime(time);
#endif
}

static void exercise_backend (hawk_mux_backend_t backend)
{
	memory_t memory;
	hawk_mmgr_t mmgr;
	hawk_mux_t* mux;
	hawk_errinf_t errinf;
	hawk_mux_evt_t ev[8], saved;
	hawk_mux_hnd_t pair[2], other[2];
	hawk_uint64_t token = ((hawk_uint64_t)0xfedcba98 << 32) | 0x76543210;
	hawk_ntime_t zero = { 0, 0 };
	hawk_ntime_t invalid = { 0, HAWK_NSECS_PER_SEC };
	hawk_bch_t ch;
	hawk_ooi_t n;
	int i, found;

	memory.fail_after = -1;
	memory.live = 0;
	mmgr.alloc = alloc_mem;
	mmgr.realloc = realloc_mem;
	mmgr.free = free_mem;
	mmgr.ctx = &memory;
	mux = hawk_mux_open(&mmgr, 16, hawk_get_utf8_cmgr(), backend, &errinf);
	if (!mux && errinf.num == HAWK_ENOIMPL)
	{
		skip("backend is not available", 1);
		OK(memory.live == 0, "unavailable backend releases the object");
		return;
	}
	OK(mux != HAWK_NULL, "open requested mux backend");
	if (!mux) return;
	printf("# backend %d\n", (int)hawk_mux_getbackend(mux));
	OK(hawk_mux_getbackend(mux) == backend, "explicit backend is honored");
	found = 1;
	for (i = 0; i < 16; i++)
	{
		if (((hawk_uint8_t*)hawk_mux_getxtn(mux))[i] != 0) found = 0;
	}
	OK(found, "extension is zero-initialized");
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 0, "empty nonblocking mux");
	OK(hawk_mux_wait(mux, ev, 0, &zero) <= -1, "reject zero event capacity");
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &invalid) <= -1, "reject invalid nanoseconds");
	invalid.sec = -1;
	invalid.nsec = 0;
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &invalid) <= -1, "reject negative timeout");
	if (make_pair(pair) <= -1 || make_pair(other) <= -1)
	{
		FAIL("create socket pairs");
		hawk_mux_close(mux, HAWK_NULL);
		return;
	}
	OK(hawk_mux_ctl(mux, HAWK_MUX_MOD, pair[0], HAWK_MUX_IN, token) <= -1 &&
	   hawk_gem_geterrnum(hawk_mux_getgem(mux)) == HAWK_ENOENT, "modify requires a registration");
	OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], 0x100000, token) <= -1, "unsupported event bits are rejected");
	/* Fail the first ADD allocation, then retry without a partial watch. */
	memory.fail_after = 0;
	OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], HAWK_MUX_IN, token) <= -1 &&
	   hawk_gem_geterrnum(hawk_mux_getgem(mux)) == HAWK_ENOMEM, "allocation failure leaves ADD uncommitted");
	memory.fail_after = -1;
	OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], HAWK_MUX_IN, token) == 0, "retry ADD after allocation failure");
	OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], HAWK_MUX_IN, token + 1) <= -1 &&
	   hawk_gem_geterrnum(hawk_mux_getgem(mux)) == HAWK_EEXIST, "duplicate ADD leaves original registration");
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 0, "idle read watch has no events");
	n = send(pair[1], "A", 1, 0);
	OK(n == 1, "make read watch ready");
	if (n != 1) goto done;
	n = hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), HAWK_NULL);
	OK(n == 1 && ev[0].hnd == pair[0] && ev[0].token == token &&
	   (ev[0].mask & HAWK_MUX_IN), "infinite wait returns handle, mask and full 64-bit token");
	saved = ev[0];
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 1, "level-triggered readiness persists");
	OK(recv(pair[0], &ch, 1, 0) == 1 && ch == 'A', "consume input");
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 0, "consumed readiness disappears");
	OK(hawk_mux_ctl(mux, HAWK_MUX_MOD, pair[0], HAWK_MUX_IN | HAWK_MUX_OUT | HAWK_MUX_ONESHOT, token + 1) == 0,
	   "modify mask and token and arm a one-shot");
	n = send(pair[1], "B", 1, 0);
	OK(n == 1, "make both directions ready");
	if (n != 1) goto done;
	n = hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero);
	OK(n == 1 && ev[0].token == token + 1 && (ev[0].mask & (HAWK_MUX_IN | HAWK_MUX_OUT)) ==
	   (HAWK_MUX_IN | HAWK_MUX_OUT), "merge read/write readiness into one registration event");
	OK(hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 0, "one-shot disables the whole watch");
	OK(hawk_mux_ctl(mux, HAWK_MUX_MOD, pair[0], HAWK_MUX_IN | HAWK_MUX_ONESHOT, token + 2) == 0 &&
	   hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 1 && ev[0].token == token + 2,
	   "MOD rearms consumed one-shot with new token");
	OK(hawk_mux_ctl(mux, HAWK_MUX_DEL, pair[0], 0, 0) == 0, "delete consumed one-shot");
	OK(hawk_mux_ctl(mux, HAWK_MUX_DEL, pair[0], 0, 0) <= -1, "delete missing registration fails");
	OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], HAWK_MUX_IN, token + 3) == 0 &&
	   hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 1 && ev[0].token == token + 3,
	   "re-register descriptor without reviving old token");
	OK(saved.hnd == pair[0] && saved.token == token && (saved.mask & HAWK_MUX_IN),
	   "copied event survives deletion and re-registration");
	i = hawk_mux_ctl(mux, HAWK_MUX_ADD, other[0], HAWK_MUX_IN, token + 4);
	n = send(other[1], "C", 1, 0);
	OK(i == 0 && n == 1, "add a second ready registration");
	if (n != 1) goto done;
	OK(hawk_mux_wait(mux, ev, 1, &zero) == 1, "respect caller event capacity");
	OK(hawk_mux_ctl(mux, HAWK_MUX_DEL, pair[0], 0, 0) == 0, "delete first dense entry");
	n = hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero);
	OK(n == 1 && ev[0].hnd == other[0] && ev[0].token == token + 4,
	   "remaining watch survives dense-array compaction");
	OK(hawk_mux_ctl(mux, HAWK_MUX_MOD, other[0], HAWK_MUX_IN, token + 5) == 0 &&
	   hawk_mux_wait(mux, ev, HAWK_COUNTOF(ev), &zero) == 1 && ev[0].token == token + 5,
	   "modify remaining entry after compaction");
#if !defined(_WIN32)
	if (backend == HAWK_MUX_BACKEND_SELECT)
	{
		OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, FD_SETSIZE, HAWK_MUX_IN, 1) <= -1 &&
		   hawk_gem_geterrnum(hawk_mux_getgem(mux)) == HAWK_EINVAL, "select guards FD_SETSIZE before FD_SET");
	}
#endif
	done:
	OK(hawk_mux_close(mux, &errinf) == 0, "close mux");
	OK(memory.live == 0, "free all mux allocations");
	if (n == 1)
		OK(recv(other[0], &ch, 1, 0) == 1 && ch == 'C', "closing mux preserves watched handles");
	close_handle(pair[0]);
	close_handle(pair[1]);
	close_handle(other[0]);
	close_handle(other[1]);
}

static void exercise_failures (hawk_mux_backend_t backend)
{
	memory_t memory;
	hawk_mmgr_t mmgr;
	hawk_mux_t* mux;
	hawk_errinf_t errinf;
	hawk_mux_hnd_t pair[2];
	hawk_mux_evt_t events[4];
	hawk_ntime_t timeout = { 0, 1000 };
	hawk_ntime_t start, finish;
	hawk_ooi_t n;
	int rc;

	memory.fail_after = 0;
	memory.live = 0;
	mmgr.alloc = alloc_mem;
	mmgr.realloc = realloc_mem;
	mmgr.free = free_mem;
	mmgr.ctx = &memory;
	mux = hawk_mux_open(&mmgr, 0, hawk_get_utf8_cmgr(), backend, &errinf);
	OK(!mux && errinf.num == HAWK_ENOMEM && memory.live == 0, "open reports allocator failure without leaking");
	memory.fail_after = -1;
	mux = hawk_mux_open(&mmgr, 0, hawk_get_utf8_cmgr(), backend, &errinf);
	if (!mux)
	{
		skip("backend is not available", 1);
		return;
	}
	if (make_pair(pair) <= -1)
	{
		FAIL("create socket pair for failure tests");
		hawk_mux_close(mux, HAWK_NULL);
		return;
	}
#if defined(_WIN32)
	memory.fail_after = 0;
#else
	memory.fail_after = 1; /* map succeeds, watch allocation fails */
#endif
	rc = hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], HAWK_MUX_IN, 42);
	OK(rc <= -1 && hawk_gem_geterrnum(hawk_mux_getgem(mux)) == HAWK_ENOMEM,
	   "failed second allocation preserves the grown descriptor map");
	memory.fail_after = -1;
	OK(hawk_mux_ctl(mux, HAWK_MUX_ADD, pair[0], HAWK_MUX_IN, 42) == 0, "retry after partial allocation growth");
	rc = send(pair[1], "D", 1, 0);
	OK(rc == 1, "make failure-test watch ready");
	if (rc != 1) goto done;
	memory.fail_after = 0;
	n = hawk_mux_wait(mux, events, HAWK_COUNTOF(events), &timeout);
	if (backend == HAWK_MUX_BACKEND_SELECT) OK(n == 1, "select waits without allocating");
	else OK(n <= -1 && hawk_gem_geterrnum(hawk_mux_getgem(mux)) == HAWK_ENOMEM,
	   "failed event allocation preserves the registration");
	memory.fail_after = -1;
	OK(hawk_mux_wait(mux, events, HAWK_COUNTOF(events), &timeout) == 1 && events[0].token == 42,
	   "retry wait after allocation failure");
	OK(hawk_mux_ctl(mux, HAWK_MUX_DEL, pair[0], 0, 0) == 0, "remove ready watch");
	/* Use a sub-millisecond duration and verify it was not truncated to a
	 * nonblocking wait. Scheduling may lengthen it; it must not shorten it. */
	timeout.nsec = 500000;
	rc = get_clock(&start);
	n = hawk_mux_wait(mux, events, HAWK_COUNTOF(events), &timeout);
	rc |= get_clock(&finish);
	OK(n == 0 && rc == 0 && (finish.sec > start.sec || finish.nsec - start.nsec >= timeout.nsec),
	   "positive fractional timeout does not become a nonblocking poll");

done:
	hawk_mux_close(mux, HAWK_NULL);
	OK(memory.live == 0, "release buffers after allocation failures");
	close_handle(pair[0]);
	close_handle(pair[1]);
}
#endif

int main (void)
{
#if defined(_WIN32)
	WSADATA data;
#endif

	no_plan();
#if defined(__OS2__) || defined(__DOS__)
	skip("no mux backend on this platform", 1);
#else
#if defined(_WIN32)
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
	{
		FAIL("initialize Winsock");
		return exit_status();
	}
#endif
	exercise_backend(HAWK_MUX_BACKEND_EPOLL);
	exercise_backend(HAWK_MUX_BACKEND_KQUEUE);
	exercise_backend(HAWK_MUX_BACKEND_POLL);
	exercise_backend(HAWK_MUX_BACKEND_SELECT);
	exercise_failures(HAWK_MUX_BACKEND_EPOLL);
	exercise_failures(HAWK_MUX_BACKEND_KQUEUE);
	exercise_failures(HAWK_MUX_BACKEND_POLL);
	exercise_failures(HAWK_MUX_BACKEND_SELECT);
#if defined(_WIN32)
	WSACleanup();
#endif
#endif
	return exit_status();
}
