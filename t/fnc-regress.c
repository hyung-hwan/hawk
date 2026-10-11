/* Invoked one case at a time by regress-fnc.sh so a crash doesn't hide other cases. */
#include "hawk-prv.h" /* HAWK_ENABLE_GC is an internal feature switch. */
#include <hawk-utl.h>
#include <hawk-pio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

extern int hawk_test_mod_math (hawk_mod_t* mod, hawk_t* hawk);
extern int hawk_test_mod_math_time (hawk_mod_t* mod, hawk_t* hawk);
extern int hawk_test_math_threads (const hawk_bch_t* source);
extern int hawk_test_mod_sys_fallback (hawk_mod_t* mod, hawk_t* hawk);
extern int hawk_test_mod_sys_fault (hawk_mod_t* mod, hawk_t* hawk);
extern int hawk_test_mod_sys_unavailable (hawk_mod_t* mod, hawk_t* hawk);
extern int hawk_test_sys_probe (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
extern int hawk_test_sys_begin (void);
extern int hawk_test_sys_end (void);
extern int hawk_test_sys_pid32_narrow (void);
extern int hawk_test_sys_offset32_narrow (void);
extern int hawk_test_sys_owner32_narrow (void);
extern int hawk_test_sys_mode32_narrow (void);
extern int hawk_test_sys_tcflag32_narrow (void);
extern int hawk_test_sys_fd_flags_available (void);
extern int hawk_test_sys_native_kill_begin (void);
extern int hawk_test_sys_native_kill_end (void);

typedef struct allocation_t allocation_t;
struct allocation_t
{
	allocation_t* next;
	void* ptr;
};

typedef struct allocator_t allocator_t;
struct allocator_t
{
	allocation_t* allocations;
	hawk_oow_t calls;
	hawk_oow_t fail_at;
	hawk_oow_t fail_after;
	hawk_rtx_t* pair_rtx;
	hawk_val_ref_t* pair_refs[2];
	hawk_val_t* pair_old[2];
	hawk_val_t* pair_new[2];
	hawk_rtx_gbl_t pair_gbl;
	hawk_ooch_t* pair_line;
	hawk_oow_t pair_nflds;
	int pair_observer_error;
	int pair_callbacks;
	int failed;
	int invalid_frees;
	int free_errno;
	hawk_val_t* oversized_mbs;
	int large_io_buffers;
	int invalid_io_allocation;
};

static int fail_allocation (allocator_t* a)
{
	if (a->pair_rtx)
	{
		hawk_rtx_t* rtx = a->pair_rtx;
		if (hawk_rtx_getrefval(rtx, a->pair_refs[0]) != a->pair_old[0] ||
		    hawk_rtx_getrefval(rtx, a->pair_refs[1]) != a->pair_old[1] ||
		    HAWK_MEMCMP(&rtx->gbl, &a->pair_gbl, HAWK_SIZEOF(rtx->gbl)) != 0 ||
		    HAWK_OOECS_PTR(&rtx->inrec.line) != a->pair_line || rtx->inrec.nflds != a->pair_nflds)
			a->pair_observer_error = 1;
	}
	if (++a->calls == a->fail_at)
	{
		a->failed = 1;
		return 1;
	}
	return 0;
}

static void* allocate (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	allocation_t* item;
	void* ptr;

	if (fail_allocation(a)) return HAWK_NULL;
	/* Large-I/O cases use mock CRT calls that touch at most three bytes. */
	if (a->large_io_buffers && size >= HAWK_TYPE_MAX(int))
	{
		if (size != HAWK_TYPE_MAX(int)) a->invalid_io_allocation = 1;
		size = 16;
	}
	ptr = malloc(size);
	if (!ptr) return HAWK_NULL;
	item = malloc(sizeof(*item));
	if (!item) { free(ptr); return HAWK_NULL; }
	item->ptr = ptr;
	item->next = a->allocations;
	a->allocations = item;
	return ptr;
}

static void* reallocate (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	allocation_t* item;
	void* replacement;

	if (!ptr) return allocate(mmgr, size);
	if (fail_allocation(a)) return HAWK_NULL;
	for (item = a->allocations; item && item->ptr != ptr; item = item->next);
	if (!item)
	{
		a->invalid_frees++;
		fprintf(stderr, "invalid realloc of %p\n", ptr);
		return HAWK_NULL;
	}
	if (a->large_io_buffers && size >= HAWK_TYPE_MAX(int))
	{
		if (size != HAWK_TYPE_MAX(int)) a->invalid_io_allocation = 1;
		size = 16;
	}
	replacement = realloc(ptr, size);
	if (replacement) item->ptr = replacement;
	return replacement;
}

static void deallocate (hawk_mmgr_t* mmgr, void* ptr)
{
	allocator_t* a = mmgr->ctx;
	allocation_t** link;
	allocation_t* item;

	if (!ptr) return;
	for (link = &a->allocations; *link && (*link)->ptr != ptr; link = &(*link)->next);
	if (!*link)
	{
		/* Record the error without handing an invalid pointer to the system allocator. */
		a->invalid_frees++;
		fprintf(stderr, "invalid free of %p\n", ptr);
		return;
	}
	item = *link;
	*link = item->next;
	free(item->ptr);
	free(item);
	if (a->free_errno) errno = EACCES;
}

static int clobber_free_errno (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	allocator_t* a = hawk_rtx_getmmgr(rtx)->ctx;
	hawk_int_t enabled;
	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &enabled) <= -1) return -1;
	a->free_errno = enabled != 0;
	return 0;
}

static int arm (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	allocator_t* a = hawk_rtx_getmmgr(rtx)->ctx;

	if (a->fail_after > 0) a->fail_at = a->calls + a->fail_after;
	return 0;
}

static void pair_gblset (hawk_rtx_t* rtx, hawk_oow_t id, hawk_val_t* val, void* ctx)
{
	allocator_t* a = ctx;
	hawk_oow_t i;

	a->pair_callbacks++;
	/* A callback is permitted to allocate now that publication is complete. */
	a->pair_rtx = HAWK_NULL;
	for (i = 0; i < 2; i++)
	{
		/* Ordinary globals retain the exact input value. NF can instead be
		 * changed implicitly by assigning a positional destination. */
		if (a->pair_refs[i]->id == HAWK_VAL_REF_GBL &&
		    (hawk_oow_t)a->pair_refs[i]->adr > HAWK_MAX_GBL_ID)
		{
			hawk_val_t* expected = a->pair_new[i];
			if (i == 0 && a->pair_refs[1]->id == HAWK_VAL_REF_GBL &&
			    a->pair_refs[0]->adr == a->pair_refs[1]->adr) expected = a->pair_new[1];
			if (hawk_rtx_getrefval(rtx, a->pair_refs[i]) != expected) a->pair_observer_error = 1;
		}
	}
}

static int setpair (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	allocator_t* a = hawk_rtx_getmmgr(rtx)->ctx;
	hawk_rtx_ecb_t ecb;
	hawk_oow_t i;
	int n;
	hawk_errnum_t error;

	HAWK_MEMSET(&ecb, 0, HAWK_SIZEOF(ecb));
	a->pair_refs[0] = (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 0);
	a->pair_refs[1] = (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2);
	a->pair_new[0] = hawk_rtx_getarg(rtx, 1);
	a->pair_new[1] = hawk_rtx_getarg(rtx, 3);
	for (i = 0; i < 2; i++)
	{
		a->pair_old[i] = hawk_rtx_getrefval(rtx, a->pair_refs[i]);
		hawk_rtx_refupval(rtx, a->pair_old[i]);
	}
	HAWK_MEMCPY(&a->pair_gbl, &rtx->gbl, HAWK_SIZEOF(rtx->gbl));
	a->pair_line = HAWK_OOECS_PTR(&rtx->inrec.line);
	a->pair_nflds = rtx->inrec.nflds;
	a->pair_callbacks = 0;
	a->pair_rtx = rtx;
	ecb.gblset = pair_gblset;
	ecb.ctx = a;
	hawk_rtx_pushecb(rtx, &ecb);
	n = hawk_rtx_setrefvalpair(rtx, a->pair_refs[0], a->pair_new[0], a->pair_refs[1], a->pair_new[1]);
	error = hawk_rtx_geterrnum(rtx);
	if (n == 0 && a->pair_refs[0]->id == HAWK_VAL_REF_GBL &&
	    a->pair_refs[1]->id == HAWK_VAL_REF_GBL &&
	    (hawk_oow_t)a->pair_refs[0]->adr > HAWK_MAX_GBL_ID &&
	    (hawk_oow_t)a->pair_refs[1]->adr > HAWK_MAX_GBL_ID &&
	    a->pair_old[0] != a->pair_new[0] && a->pair_old[1] != a->pair_new[1] &&
	    a->pair_callbacks != (a->pair_refs[0]->adr == a->pair_refs[1]->adr? 1: 2)) a->pair_observer_error = 1;
	a->pair_rtx = HAWK_NULL;
	hawk_rtx_popecb(rtx);
	if (n < 0 && (a->pair_callbacks != 0 ||
	    hawk_rtx_getrefval(rtx, a->pair_refs[0]) != a->pair_old[0] ||
	    hawk_rtx_getrefval(rtx, a->pair_refs[1]) != a->pair_old[1])) a->pair_observer_error = 1;
	for (i = 0; i < 2; i++) hawk_rtx_refdownval(rtx, a->pair_old[i]);
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, n < 0? -(hawk_int_t)error: 0));
	return 0;
}

void hawk_test_fail_next_allocation (hawk_rtx_t* rtx)
{
	allocator_t* a = hawk_rtx_getmmgr(rtx)->ctx;
	a->fail_at = a->calls + 1;
}

static int piowait (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_gem_t* gem = hawk_rtx_getgem(rtx);
	hawk_pio_t* pio;
	hawk_uintmax_t status;
	int ok;

#if defined(_WIN32)
	const hawk_ooch_t* command = HAWK_T("set /p x= & exit /b 23");
#else
	const hawk_ooch_t* command = HAWK_T("read x; exit 23");
#endif
	pio = hawk_pio_open(gem, 0, command, HAWK_PIO_SHELL | HAWK_PIO_WRITEIN | HAWK_PIO_WAITNOBLOCK, HAWK_NULL, HAWK_NULL);
	if (!pio) return -1;
	/* The child cannot finish until its input pipe receives a line or EOF. */
	status = 999;
	ok = hawk_pio_waitex(pio, &status) == 0 && status == 999;
	if (hawk_pio_wait(pio) != 256) ok = 0;
	if (hawk_pio_writebytes(pio, HAWK_PIO_IN, "go\n", 3) != 3) ok = 0;
	hawk_pio_end(pio, HAWK_PIO_IN);
	pio->flags &= ~HAWK_PIO_WAITNOBLOCK;
	if (hawk_pio_waitex(pio, &status) != 1 || status != 23) ok = 0;
	status = 999;
	if (hawk_pio_waitex(pio, &status) != -1 || status != 999 || hawk_gem_geterrnum(gem) != HAWK_ECHILD) ok = 0;
	hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
	hawk_pio_close(pio);
	if (hawk_gem_geterrnum(gem) != HAWK_ENOERR) ok = 0;

#if defined(_WIN32)
	pio = hawk_pio_open(gem, 0, HAWK_T("exit /b -1"), HAWK_PIO_SHELL, HAWK_NULL, HAWK_NULL);
	if (!pio) return -1;
	if (hawk_pio_waitex(pio, &status) != 1 || status != (hawk_uintmax_t)0xffffffffu) ok = 0;
	hawk_pio_close(pio);
	pio = hawk_pio_open(gem, 0, HAWK_T("exit /b -1"), HAWK_PIO_SHELL, HAWK_NULL, HAWK_NULL);
	if (!pio) return -1;
	if (hawk_pio_wait(pio) != -1 || hawk_gem_geterrnum(gem) != HAWK_EINVAL) ok = 0;
	hawk_pio_close(pio);
	if (hawk_gem_geterrnum(gem) != HAWK_EINVAL) ok = 0;
#endif
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ok? 0: 1));
	return 0;
}

static int bob (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* value;

	value = hawk_rtx_makebobval(rtx, "abc", 3);
	if (!value) return -1;
	hawk_rtx_setretval(rtx, value);
	return 0;
}

static int minint (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* value;

	value = hawk_rtx_makeintval(rtx, -(hawk_int_t)(HAWK_TYPE_MAX(hawk_uint_t) >> 1) - 1);
	if (!value) return -1;
	hawk_rtx_setretval(rtx, value);
	return 0;
}

static int maxint (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* value;

	value = hawk_rtx_makeintval(rtx, (hawk_int_t)(HAWK_TYPE_MAX(hawk_uint_t) >> 1));
	if (!value) return -1;
	hawk_rtx_setretval(rtx, value);
	return 0;
}

static int intsize (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, HAWK_SIZEOF(hawk_int_t)));
	return 0;
}

static int toint (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t n;
	hawk_val_t* value;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &n) <= -1) return -1;
	value = hawk_rtx_makeintval(rtx, n);
	if (!value) return -1;
	hawk_rtx_setretval(rtx, value);
	return 0;
}

static int oversized_mbs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	allocator_t* a = hawk_rtx_getmmgr(rtx)->ctx;
	hawk_val_t* value;
	hawk_int_t mode;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &mode) < 0) return -1;
	value = hawk_rtx_makembsvalwithbchars(rtx, "x", 1);
	if (!value) return -1;
	/* Synthetic metadata exercises rejection before allocation or input access.
	 * Keep a reference and restore the real length before runtime cleanup. */
	hawk_rtx_refupval(rtx, value);
	a->oversized_mbs = value;
	((hawk_val_mbs_t*)value)->val.len = HAWK_TYPE_MAX(hawk_oow_t) / 2 + (mode == 0);
	hawk_rtx_setretval(rtx, value);
	return 0;
}

#define SWEEP 1
#define MB8 2
#define GC 4
#define INT32_NARROW 8
#define THREADS 16
#define QUAD 32
#define SYS 64
#define EPOLL 128
#define STRICTMAP 256
#define SLEEP_OS 512
#define PID32_NARROW 1024
#define FD_FLAGS 2048
#define NATIVE_KILL 4096
#define WIN_KILL 8192
#define OS2_KILL 16384
#define WIN_SYS (NATIVE_KILL | WIN_KILL)
#define OFFSET32_NARROW 32768
#define OWNER32_NARROW 65536
#define LARGE_IO 131072
#define MODE32_NARROW 262144
#define TCFLAG32_NARROW 524288
#define WIDE_CHAR 1048576
#define SEEK_WIDE_RESULT 2097152
#define UNIX_SOCKET 4194304
#define DEEP_SWEEP 8388608

#if defined(_WIN32)
#define SOCKET_TEST WIN_SYS
#else
#define SOCKET_TEST SYS
#endif
#define SOCKET_RANGE_TEST (SOCKET_TEST | INT32_NARROW)

#define SYS_TIME_OUTPUT_TYPE_ERROR(name) \
	"BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-time-stale-errno\");a[1]=7; " \
	"if(sysfault::" name "(a)!=-114 || a[1]!==7 || length(sysfault::errmsg())==0) exit 1; " \
	"ns=99;if(sysfault::" name "(ns)!==3 || ns!==0) exit 1; }"

#define SYS_TIME_OUTPUT_ALLOCATION(name) \
	"BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-time-stale-errno\");OFS=\"old\";arm(); " \
	"n=sysfault::" name "(OFS);if(n<0) {if(n!=sysfault::RC_ENOMEM || OFS!==\"old\" || " \
	"length(sysfault::errmsg())==0) exit 1;}else if(n!==3 || OFS!==0) exit 1; }"

#define SYS_WIN_RECV_RESIDUE \
	"a=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0); " \
	"b=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0); " \
	"if(a<0 || b<0 || sysfault::bind(b,\"127.0.0.1:0\")!=0 || " \
	"sysfault::setsockopt(b,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,1)!=0) exit 1; " \
	"addr=sysprobe(\"socket-address\");if(sysfault::sendto(a,@b\"ABC\\nOLD\",addr)!=7 || " \
	"sysfault::read(b,x,100,@b\"\\n\")!=4 || x!==@b\"ABC\\n\") exit 1; "

typedef struct case_t case_t;
struct case_t
{
	const hawk_bch_t* name;
	const hawk_bch_t* source;
	hawk_errnum_t expected_error;
	int flags;
};

#define SYS_SEEK_SOURCE \
	"function seekfile() { @local p,f,x; p=sys::tempname(\"hawk-seek-\"); " \
	"f=sys::open(p,sys::O_RDWR); if(f<0) { sys::unlink(p); exit 1; } " \
	"if(sys::unlink(p)!=0 || sys::write(f,@b\"A\\nLEFT\\n\")!=7 || sys::fseek(f,0,sys::SEEK_SET)!=0 || " \
	"sys::read(f,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\") exit 1; return f; } "

/* Real file reads seed the wrapper's residue; the terminal fixture controls
 * only the OS attribute/flush result, so no pseudo-terminal is required. */
#define SYS_TERMINAL_RESIDUE \
	"BEGIN {p=sys::tempname(\"hawk-terminal-\");if(p==\"\") exit 1; " \
	"f=sysfault::open(p,sysfault::O_RDWR);if(f<0) {sys::unlink(p);exit 1;} " \
	"if(sys::unlink(p)!=0 || sysfault::write(f,@b\"A\\nOLD\")!=5 || " \
	"sysfault::fseek(f,0,sysfault::SEEK_SET)!=0 || " \
	"sysfault::read(f,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\") exit 1; "

#define SYS_TERMINAL_RESIDUE_CLEARED \
	"if(sysfault::read(f,x,8192)!=0 || x!==@b\"A\\n\" || sysfault::close(f)!=0) exit 1; }"

#define SYS_TERMINAL_RESIDUE_KEPT \
	"if(sysfault::read(f,x,8192)!=3 || x!==@b\"OLD\" || " \
	"sysfault::read(f,x,8192)!=0 || sysfault::close(f)!=0) exit 1; }"

#define SYS_ACCEPT_FLAG_FAILURE(failure) \
	"BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1; " \
	"sysprobe(\"" failure "\");peer=\"old\";n=sysfault::accept(s,sysfault::SOCK_NONBLOCK|sysfault::SOCK_CLOEXEC,peer); " \
	"if(n!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || peer!=\"old\" || sysprobe(\"accepted-closed\")!=1) exit 1; }"

#define SYS_PIPE_FLAG_FAILURE(failure) \
	"BEGIN {sysprobe(\"" failure "\");a=41;b=42;n=sysfault::pipe(a,b,sysfault::O_NONBLOCK|sysfault::O_CLOEXEC); " \
	"if(n!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || a!=41 || b!=42 || sysprobe(\"pipe-closed\")!=1) exit 1; }"

#define SYS_SOCKET_ARG_OVERFLOW(domain,type,proto) \
	"BEGIN {n=sysfault::socket(" domain "," type "," proto "); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"socket-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }"

#define SYS_LOG_ISO8601(mode,stamp) \
	"BEGIN {sysprobe(\"log-mock\");sysprobe(\"" mode "\"); " \
	"if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_ISO8601|sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=0 || " \
	"sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=0 || " \
	"sysprobe(\"log-message\")!==@b\"<14>" stamp " test: hello\" || " \
	"sysprobe(\"log-send-calls\")!=1 || sysfault::closelog()!=0) exit 1; }"

#define SYS_LOG_ISO8601_UTC_FAILURE(mode,code) \
	"BEGIN {sysprobe(\"log-mock\");sysprobe(\"" mode "\"); " \
	"if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_ISO8601,sysfault::LOG_FAC_USER)!=0 || " \
	"sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::" code " || " \
	"index(sysfault::errmsg(),\"unable to get UTC time\")==0 || sysprobe(\"log-send-calls\")!=0) exit 1; " \
	"sysprobe(\"log-tz-zero\");if(sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || " \
	"sysprobe(\"log-message\")!==@b\"<14>2023-12-31T23:45:30+00:00 test: retry\" || " \
	"sysprobe(\"log-send-calls\")!=1 || sysfault::closelog()!=0) exit 1; }"

#define SYS_STRFTIME_NUL(format) \
	"BEGIN {arm();n=sys::strftime(" format ",0,sys::STRFTIME_UTC); " \
	"if(n!==sys::RC_EINVAL || sys::errmsg()!==\"time format contains an embedded NUL character\") exit 1; " \
	"if(sys::strftime(\"%Y-%m-%d\",0,sys::STRFTIME_UTC)!==\"1970-01-01\" || " \
	"sys::strftime(\"\",0,sys::STRFTIME_UTC)!==\"\") exit 1; }"

#define SYS_MKTIME_INVALID(value) \
	"BEGIN {n=sys::mktime(" value ");if(n!=sys::RC_EINVAL || " \
	"index(sys::errmsg(),\"invalid time specification\")==0 || " \
	"index(sys::errmsg(),\"YYYY MM DD HH MM SS [DST]\")==0) exit 1; " \
	"t=sys::mktime(\"2020 01 02 03 04 05 -1\"); " \
	"if(sys::errmsg()!=\"\" || sys::strftime(\"%Y-%m-%d %H:%M:%S\",t)!=\"2020-01-02 03:04:05\") exit 1; }"

#define SYS_SOCKET_OPTION_OVERFLOW(level,value) \
	"BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0) exit 1; " \
	"n=sysfault::setsockopt(s," level ",sysfault::SO_REUSEADDR," value "); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"setsockopt-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }"

#define SYS_SOCKET_TIMEOUT_TOO_SMALL(option,value) \
	"BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0); " \
	"if(s<0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::" option ",0.5)!=0) exit 1; " \
	"if(sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::" option "," value ")!=sysfault::RC_EINVAL || " \
	"sysprobe(\"setsockopt-calls\")!=1 || length(sysfault::errmsg())==0) exit 1; " \
	"if(sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::" option ",0)!=0 || sysprobe(\"setsockopt-calls\")!=2) exit 1; }"

#define SYS_SOCKET_CONTROL_OVERFLOW(fnc,value) \
	"BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1; " \
	"n=sysfault::" fnc "(s," value "); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"" fnc "-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }"

#define SYS_ACCEPT_OVERFLOW(value) \
	"BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;peer=\"old\"; " \
	"n=sysfault::accept(s," value ",peer); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"accept-calls\")!=0 || length(sysfault::errmsg())==0 || peer!==\"old\") exit 1; }"

#define SYS_ACCEPT_UNSUPPORTED_FLAGS(value) \
	"BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;peer=\"old\"; " \
	"n=sysfault::accept(s," value ",peer); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"accept-calls\")!=0 || length(sysfault::errmsg())==0 || peer!==\"old\") exit 1; " \
	"a=sysfault::accept(s,0,peer);if(a<0 || sysprobe(\"accept-calls\")!=1 || peer===\"old\" || " \
	"sysfault::close(a)!=0 || sysfault::close(s)!=0) exit 1; }"

#define SYS_OPEN_FLAGS_OVERFLOW(value) \
	"BEGIN {p=sys::tempname(\"hawk-flags-\");if(p==\"\") exit 1;f=sys::open(p,sys::O_RDWR); " \
	"if(f<0) {sys::unlink(p);exit 1;}if(sys::write(f,@b\"abc\")!=3 || sys::close(f)!=0) {sys::unlink(p);exit 1;} " \
	"n=sys::open(p," value ");ok=n==sys::RC_EINVAL && length(sys::errmsg())>0;if(n>=0) sys::close(n); " \
	"f=sys::open(p,sys::O_RDONLY);if(f>=0) {ok=ok && sys::read(f,x,10)==3 && x===@b\"abc\";sys::close(f);}else ok=0; " \
	"if(sys::unlink(p)!=0 || !ok) exit 1; }"

#define SYS_DUP_FLAGS_OVERFLOW(value) \
	"BEGIN {if(sysfault::pipe(a,b)!=0) exit 1;n=sysfault::dup(a,b," value "); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"dup-calls\")!=0 || length(sysfault::errmsg())==0 || " \
	"sysfault::write(b,@b\"x\")!=1 || sysfault::read(a,x,1)!=1 || x!==@b\"x\") exit 1; }"

#define SYS_DUP_INVALID_FLAGS(module,value) \
	"function f() {} BEGIN {expected=" module "::openfd(" value ");message=" module "::errmsg(); " \
	"if(expected>=0 || length(message)==0 || " module "::pipe(a,b)!=0 || " module "::pipe(c,d)!=0 || " \
	module "::write(d,@b\"A\\nOLD\\n\")!=6 || " module "::read(c,x,8192,\"\\n\")!=2) exit 1; " \
	"if(" module "::dup(a,c," value ")!=expected || " module "::errmsg()!==message || sysprobe(\"dup-calls\")!=0 || " \
	module "::read(c,x,4)!=4 || x!==@b\"OLD\\n\" || " module "::write(d,@b\"x\")!=1 || " \
	module "::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }"

#define SYS_CHMOD_INVALID_MODE(value) \
	"function f() {} BEGIN {p=sys::tempname(\"hawk-chmod-\");if(p==\"\") exit 1; " \
	"if(sys::chmod(p,0600)!=0) {sys::unlink(p);exit 1;}expected=sys::openfd(" value ");message=sys::errmsg(); " \
	"n=sys::chmod(p," value ");ok=expected<0 && length(message)>0 && n==expected && sys::errmsg()===message; " \
	"if(sys::stat(p,s)!=0 || (s[\"mode\"]&07777)!=0600) ok=0;if(sys::unlink(p)!=0 || !ok) exit 1; }"

#define SYS_FCHOWN_OVERFLOW(uid,gid) \
	"BEGIN {if(sysfault::pipe(a,b)!=0) exit 1;n=sysfault::fchown(a," uid "," gid "); " \
	"if(n!=sysfault::RC_EINVAL || sysprobe(\"fchown-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }"

#define SYS_DUP_FILE_OVERFLOW(value) \
	"BEGIN {p=sys::tempname(\"hawk-dup-\");q=sys::tempname(\"hawk-dup-\");if(p==\"\" || q==\"\") exit 1; " \
	"a=sys::open(p,sys::O_RDWR);b=sys::open(q,sys::O_RDWR);if(a<0 || b<0) {sys::unlink(p);sys::unlink(q);exit 1;} " \
	"if(sys::write(b,@b\"abc\")!=3 || sys::fseek(b,0,sys::SEEK_SET)!=0) exit 1; " \
	"n=sys::dup(a,b," value ");ok=n==sys::RC_EINVAL && length(sys::errmsg())>0; " \
	"ok=ok && sys::read(b,x,10)==3 && x===@b\"abc\";sys::close(a);sys::close(b); " \
	"if(sys::unlink(p)!=0 || sys::unlink(q)!=0 || !ok) exit 1; }"

#define SYS_BAD_NUMBER(value) \
	"function bad() {} BEGIN {v=" value ";expected=sys::openfd(v);message=sys::errmsg(); " \
	"if(expected>=0 || length(message)==0) exit 1; "

#define SYS_IO_BAD_NUMBER(call,value) \
	SYS_BAD_NUMBER(value) \
	"p=sys::tempname(\"hawk-io-number-\");if(p==\"\") exit 1;a=sys::open(p,sys::O_RDWR); " \
	"if(a<0 || sys::write(a,@b\"keep\")!=4 || sys::fseek(a,0,sys::SEEK_SET)!=0) {sys::unlink(p);exit 1;} " \
	"x=@b\"old\";n=" call ";ok=n==expected && sys::errmsg()===message && x===@b\"old\"; " \
	"if(sys::fseek(a,0,sys::SEEK_CUR)!=0 || sys::fseek(a,0,sys::SEEK_SET)!=0 || " \
	"sys::read(a,x,10)!=4 || x!==@b\"keep\") ok=0;sys::close(a);if(sys::unlink(p)!=0 || !ok) exit 1; }"

#define SYS_OPEN_BAD_NUMBER(args,value) \
	SYS_BAD_NUMBER(value) \
	"p=sys::tempname(\"hawk-open-number-\");if(p==\"\") exit 1;a=sys::open(p,sys::O_RDWR); " \
	"if(a<0 || sys::write(a,@b\"keep\")!=4 || sys::close(a)!=0) {sys::unlink(p);exit 1;} " \
	"n=sys::open(p," args ");ok=n==expected && sys::errmsg()===message;if(n>=0) sys::close(n); " \
	"a=sys::open(p,sys::O_RDONLY);if(a<0 || sys::read(a,x,10)!=4 || x!==@b\"keep\") ok=0; " \
	"if(a>=0) sys::close(a);if(sys::unlink(p)!=0 || !ok) exit 1; }"

#define SYS_MKDIR_BAD_NUMBER(value) \
	SYS_BAD_NUMBER(value) \
	"p=sys::tempname(\"hawk-mkdir-number-\");if(p==\"\" || sys::unlink(p)!=0) exit 1; " \
	"n=sys::mkdir(p,v);ok=n==expected && sys::errmsg()===message;if(n>=0) sys::rmdir(p); " \
	"if(!ok || sys::opendir(p)!=sys::RC_ENOENT) exit 1; }"

#define SYS_DIR_BAD_NUMBER(value) \
	SYS_BAD_NUMBER(value) \
	"n=sys::opendir(\".\",v);ok=n==expected && sys::errmsg()===message; " \
	"if(n>=0) sys::closedir(n);if(!ok) exit 1; }"

#define SYS_CLOSE_FILE_ERROR(failure,result,clobber) \
	"BEGIN {p=sys::tempname(\"hawk-close-error-\");if(p==\"\") exit 1;f=sysfault::open(p,sysfault::O_RDWR); " \
	"if(f<0) {sys::unlink(p);exit 1;}sysprobe(\"" failure "\");clobber_free_errno(" clobber "); " \
	"n=sysfault::close(f);clobber_free_errno(0);ok=n==sysfault::" result " && " \
	"index(sysfault::errmsg(),\"unable to close file descriptor\")>0 && sysprobe(\"file-close-calls\")==1; " \
	"if(sysfault::close(f)!=sysfault::RC_EINVAL || sysprobe(\"file-close-calls\")!=1) ok=0; " \
	"sysprobe(\"file-close-normal\");if(sys::unlink(p)!=0 || !ok) exit 1; }"

#define SYS_CLOSELOG_ERROR(failure,result,counter,reset,clobber) \
	"BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/old\", " \
	"sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=0) exit 1; " \
	"sysprobe(\"" failure "\");clobber_free_errno(" clobber ");n=sysfault::closelog();clobber_free_errno(0); " \
	"if(n!=sysfault::" result " || index(sysfault::errmsg(),\"unable to close log socket\")==0 || " \
	"sysprobe(\"" counter "\")!=1 || sysfault::closelog()!=0 || sysprobe(\"" counter "\")!=1) exit 1; " \
	"sysprobe(\"" reset "\");if(sysfault::openlog(\"remote://127.0.0.1:9/new\",sysfault::LOG_OPT_NDELAY, " \
	"sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || " \
	"index(sysprobe(\"log-message\"),@b\"new: retry\")==0 || sysprobe(\"socket-calls\")!=2 || " \
	"sysprobe(\"log-send-calls\")!=2 || sysfault::closelog()!=0 || sysprobe(\"" counter "\")!=2) exit 1; }"

#define SYS_OPENLOG_CLOSE_ERROR(failure,result,counter,reset,clobber,allocation) \
	"BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/old\", " \
	"sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=0) exit 1; " \
	"sysprobe(\"" failure "\");clobber_free_errno(" clobber ");" allocation \
	"n=sysfault::openlog(\"remote://127.0.0.1:9/new\",0,sysfault::LOG_FAC_LOCAL0);clobber_free_errno(0); " \
	"if(n==sysfault::RC_ENOMEM) {if(sysprobe(\"" counter "\")!=0 || length(sysfault::errmsg())==0) exit 1;} " \
	"else if(n!=sysfault::" result " || index(sysfault::errmsg(),\"unable to close previous log socket\")==0 || " \
	"sysprobe(\"" counter "\")!=1) exit 1; " \
	"sysprobe(\"" reset "\");if(sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || " \
	"index(sysprobe(\"log-message\"),@b\"<14>\")!=1 || index(sysprobe(\"log-message\"),@b\"old: retry\")==0 || " \
	"sysprobe(\"socket-calls\")!=(n==sysfault::RC_ENOMEM?1:2) || sysprobe(\"log-send-calls\")!=2 || " \
	"sysfault::closelog()!=0 || sysprobe(\"" counter "\")!=(n==sysfault::RC_ENOMEM?1:2)) exit 1; }"

#define SYS_WAIT_STATUS_INVALID(name,value) \
	"function bad() {} BEGIN {v=" value ";expected=sys::settime(v);message=sys::errmsg(); " \
	"if(expected>=0 || message==\"\") exit 1;sys::mktime(\"invalid\"); " \
	"if(sys::" name "(v)!=expected || sys::errmsg()!==message) exit 1; }"

#define SYS_WAIT_STATUS_UNSUPPORTED(name) \
	"BEGIN {if(sysunavailable::" name "(0)!=sysunavailable::RC_ENOIMPL || sysunavailable::errmsg()==\"\") exit 1; " \
	"message=sysunavailable::errmsg();sysunavailable::openfd(-1); " \
	"if(sysunavailable::" name "(0)!=sysunavailable::RC_ENOIMPL || sysunavailable::errmsg()!==message) exit 1; }"

#define SYS_MUX_UNSUPPORTED(call) \
	"BEGIN {id=41;ev=42;if(" call "!=sysunavailable::RC_ENOIMPL || sysunavailable::errmsg()==\"\" || id!=41 || ev!=42) exit 1; " \
	"message=sysunavailable::errmsg();sysunavailable::openfd(-1); " \
	"if(" call "!=sysunavailable::RC_ENOIMPL || sysunavailable::errmsg()!==message || id!=41 || ev!=42) exit 1; }"

#define SYS_DIR_ENCODING_FLAGS(path,value) \
	"BEGIN {for(i=0;i<4;i++) {flags=" value " | (i%2?sys::DIR_SORT:0) | (i>=2?sys::DIR_SKIPSPCDIR:0); " \
	"d=sys::opendir(" path ",flags);ok=d==sys::RC_EINVAL && index(sys::errmsg(),\"path encoding flags\")>0; " \
	"if(d>=0) sys::closedir(d);if(!ok) exit 1;}d=sys::opendir(\".\");if(d<0 || sys::closedir(d)!=0) exit 1; }"

#define SYS_DIR_OPTIONS(path,flags) \
	"BEGIN {p=sys::tempname(\"hawk-directory-\");if(p==\"\" || sys::unlink(p)!=0 || sys::mkdir(p,0700)!=0) exit 1; " \
	"names[1]=\"zeta\";names[2]=\"alpha\";names[3]=\"middle\";ok=1; " \
	"for(i=1;i<=3;i++) {f=sys::open(p \"/\" names[i],sys::O_WRONLY|sys::O_CREAT|sys::O_EXCL,0600); " \
	"if(f<0) ok=0;else if(sys::close(f)!=0) ok=0;}flags=" flags ";d=sys::opendir(" path ",flags); " \
	"count=0;previous=\"\";if(d<0) ok=0;else {while((n=sys::readdir(d,name))>0) { " \
	"if(name==\".\" || name==\"..\") {if(flags & sys::DIR_SKIPSPCDIR) ok=0;continue;} " \
	"if(name!=\"alpha\" && name!=\"middle\" && name!=\"zeta\") ok=0; " \
	"if((flags & sys::DIR_SORT) && previous>name) ok=0;previous=name;seen[name]++;count++;} " \
	"if(n!=0 || sys::closedir(d)!=0) ok=0;} " \
	"if(count!=3 || seen[\"alpha\"]!=1 || seen[\"middle\"]!=1 || seen[\"zeta\"]!=1) ok=0; " \
	"for(i=1;i<=3;i++) if(sys::unlink(p \"/\" names[i])!=0) ok=0; " \
	"if(sys::rmdir(p)!=0 || !ok) exit 1; }"

#define SYS_PIPE_BAD_NUMBER(module,value) \
	SYS_BAD_NUMBER(value) \
	"a=41;b=42;n=" module "::pipe(a,b,v);ok=n==expected && " module "::errmsg()===message && " \
	"a==41 && b==42 && sysprobe(\"pipe-calls\")==0;if(n==0) {" module "::close(a);" module "::close(b);}if(!ok) exit 1; }"

#define SYS_SOCKET_BAD_NUMBER(args,value) \
	SYS_BAD_NUMBER(value) \
	"n=sysfault::socket(" args ");ok=n==expected && sysfault::errmsg()===message && " \
	"sysprobe(\"socket-calls\")==0;if(n>=0) sysfault::close(n);if(!ok) exit 1; }"

#define SYS_SOCKET_CONTROL_BAD_NUMBER(call,probe,value) \
	SYS_BAD_NUMBER(value) \
	"s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;peer=\"old\"; " \
	"n=" call ";if(n!=expected || sysfault::errmsg()!==message || sysprobe(\"" probe "\")!=0 || peer!==\"old\") exit 1; }"

#define SYS_RECVFROM_BAD_NUMBER(value) \
	SYS_BAD_NUMBER(value) \
	"sysprobe(\"recv-no-address-data\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0); " \
	"if(s<0) exit 1;x=@b\"old\";peer=\"old\";n=sysfault::recvfrom(s,x,v,peer); " \
	"if(n!=expected || sysfault::errmsg()!==message || x!==@b\"old\" || peer!==\"old\" || " \
	"sysfault::recvfrom(s,x,1,peer)!=1 || x!==@b\"Z\") exit 1; }"

#define SYS_WIN_RECVFROM_BAD_NUMBER(value) \
	SYS_BAD_NUMBER(value) \
	"s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0); " \
	"if(s<0 || sysfault::bind(s,\"127.0.0.1:0\")!=0 || " \
	"sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,1)!=0) exit 1; " \
	"addr=sysprobe(\"socket-address\");c=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0); " \
	"if(c<0 || sysfault::sendto(c,@b\"ABC\",addr)!=3) exit 1;x=@b\"old\";peer=\"old\"; " \
	"n=sysfault::recvfrom(s,x,v,peer);ok=n==expected && sysfault::errmsg()===message && x===@b\"old\" && peer===\"old\"; " \
	"if(sysfault::recvfrom(s,x,3,peer)!=3 || x!==@b\"ABC\" || !ok) exit 1; }"

#define SYS_TCFLUSH_BAD_NUMBER(value) \
	SYS_BAD_NUMBER(value) \
	"a=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(a<0) exit 1;n=sysfault::tcflush(a,v); " \
	"if(n!=expected || sysfault::errmsg()!==message || sysprobe(\"tcflush-calls\")!=0) exit 1; }"

static const case_t cases[] =
{
	{ "refpair-nf-type-error", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";b[1]=1;n=setpair(NF,1,b,2);if(n!=-114 || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\") exit 1;}", HAWK_ENOERR, STRICTMAP },
	{ "refpair-record-type-error", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";FS=\":\";b[1]=1;n=setpair($0,\"replacement\",b,2);if(n!=-114 || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\") exit 1;}", HAWK_ENOERR, STRICTMAP },
	{ "refpair-field-type-error", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";b[1]=1;n=setpair($2,\"replacement\",b,2);if(n!=-114 || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\") exit 1;}", HAWK_ENOERR, STRICTMAP },
	{ "refpair-nf-allocation", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";OFS=\":\";arm();n=setpair(NF,1,OFS,\"new\");if(n<0) {if(n!=sys::RC_ENOMEM || OFS!==\":\" || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\") exit 1;}else {if(NF!=1 || $0!==\"one\" || OFS!==\"new\") exit 1;}}", HAWK_ENOERR, DEEP_SWEEP },
	{ "refpair-record-allocation", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";arm();n=setpair($0,\"red green blue yellow\",OFS,\":\");if(n<0) {if(n!=sys::RC_ENOMEM || OFS!==\" \" || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\") exit 1;}else if(NF!=4 || $0!==\"red green blue yellow\" || $4!==\"yellow\" || OFS!==\":\") exit 1;}", HAWK_ENOERR, DEEP_SWEEP },
	{ "refpair-fs-before-record", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";arm();n=setpair(FS,\":+\",$0,\"red::green:blue\");if(n<0) {if(n!=sys::RC_ENOMEM || FS!==\" \" || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\") exit 1;}else if(FS!==\":+\" || NF!=3 || $1!==\"red\" || $2!==\"green\" || $3!==\"blue\") exit 1;}", HAWK_ENOERR, DEEP_SWEEP },
	{ "refpair-ofs-before-nf", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";if(setpair(OFS,\":\",NF,2)!=0 || $0!==\"one:two\" || NF!=2 || $1!==\"one\" || $2!==\"two\") exit 1;NF=1;if($0!==\"one\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-nf-before-field", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";if(setpair(NF,1,$2,\"new\")!=0 || NF!=2 || $0!==\"one new\" || $2!==\"new\") exit 1;NF=1;if($0!==\"one\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-field-before-nf", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";if(setpair($2,\"new\",NF,2)!=0 || NF!=2 || $0!==\"one new\" || $2!==\"new\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-record-same-destination", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";if(setpair($0,\"first\",$0,\"second third\")!=0 || NF!=2 || $0!==\"second third\" || $1!==\"second\" || $2!==\"third\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-nf-same-destination", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";if(setpair(NF,1,NF,2)!=0 || NF!=2 || $0!==\"one \" || $2!==\"\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-cached-same-destination", "BEGIN {OFS=\":\";arm();n=setpair(OFS,\"long first separator\",OFS,\"/\");if(n<0) {if(n!=sys::RC_ENOMEM || OFS!==\":\") exit 1;}else {FS=\" \";$0=\"a b\";NF=NF;if(OFS!==\"/\" || $0!==\"a/b\") exit 1;}}", HAWK_ENOERR, DEEP_SWEEP },
	{ "refpair-regex-same-destination", "BEGIN {FS=\" +\";arm();n=setpair(FS,\":+\",FS,\"/+\");if(n<0) {if(n!=sys::RC_ENOMEM || FS!==\" +\") exit 1;$0=\"a  b\";}else $0=\"a//b\";if(NF!=2 || $1!==\"a\" || $2!==\"b\") exit 1;}", HAWK_ENOERR, DEEP_SWEEP },
	{ "refpair-invalid-second-regex", "BEGIN {OFS=\":\";FS=\" +\";n=setpair(OFS,\"new\",FS,\"[[\");if(n>=0 || OFS!==\":\" || FS!==\" +\") exit 1;$0=\"a  b\";if(NF!=2 || $2!==\"b\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-rs-regex-allocation", "BEGIN {RS=\"a+\";OFS=\":\";arm();n=setpair(RS,\"b+\",OFS,\"new\");if(n<0) {if(n!=sys::RC_ENOMEM || RS!==\"a+\" || OFS!==\":\") exit 1;}else if(RS!==\"b+\" || OFS!==\"new\") exit 1;}", HAWK_ENOERR, DEEP_SWEEP },
	{ "refpair-convfmt-before-value", "BEGIN {CONVFMT=\"%.6g\";OFS=\":\";if(setpair(CONVFMT,\"%.2f\",OFS,1.2345)!=0 || OFS!=1.2345) exit 1;FS=\" \";$0=\"a b\";NF=NF;if($0!==\"a1.23b\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-ignorecase-before-record", "BEGIN {FS=\"X+\";IGNORECASE=0;if(setpair(IGNORECASE,1,$0,\"axb\")!=0 || NF!=2 || $1!==\"a\" || $2!==\"b\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-striprecspc-before-record", "BEGIN {FS=\" +\";STRIPRECSPC=0;if(setpair(STRIPRECSPC,1,$0,\" a b \")!=0 || NF!=2 || $1!==\"a\" || $2!==\"b\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-stripstrspc-before-number", "BEGIN {STRIPSTRSPC=0;NR=10;if(setpair(STRIPSTRSPC,1,NR,\" 23 \")!=0 || NR!==\" 23 \") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-global-observers", "@global g1,g2;BEGIN {g1=1;g2=2;if(setpair(g1,10,g2,20)!=0 || g1!=10 || g2!=20) exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-global-same-destination", "@global g;BEGIN {g=1;if(setpair(g,10,g,20)!=0 || g!=20) exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-value-swap", "BEGIN {a=\"first\";b=\"second\";if(setpair(a,b,b,a)!=0 || a!==\"second\" || b!==\"first\") exit 1;}", HAWK_ENOERR, 0 },
	{ "refpair-quoted-record", "BEGIN {FS=\"?,'''\";$0=\"'one,two',three,four\";b[1]=1;if(setpair(NF,1,b,2)>=0 || NF!=3 || $1!==\"one,two\" || $2!==\"three\" || $3!==\"four\") exit 1;if(setpair(NF,2,$2,\"new\")!=0 || NF!=2 || $0!==\"one,two new\") exit 1;}", HAWK_ENOERR, STRICTMAP },
	{ "refpair-ordinary-destinations", "@global g;function f(x) {@local l;l=1;if(setpair(x,20,l,30)!=0 || x!=20 || l!=30) exit 1;}BEGIN {a=1;g=2;m[\"out\"]=3;if(setpair(a,10,g,20)!=0 || a!=10 || g!=20 || setpair(m[\"out\"],30,m[\"out\"],40)!=0 || m[\"out\"]!=40) exit 1;f(1);}", HAWK_ENOERR, 0 },
	{ "refpair-strict-alias-validation", "BEGIN {a[1]=1;if(setpair(b,a,b,10)>=0 || hawk::typename(b)!=\"nil\") exit 1;}", HAWK_ENOERR, STRICTMAP },
	{ "refpair-parent-and-element", "BEGIN {m[\"out\"]=\"old\";if(setpair(m,20,m[\"out\"],30)!=0 || m!=20) exit 1;}", HAWK_ENOERR, GC },
	{ "sys-pipe-nf-output-atomic", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";b[1]=1;if(sysfault::pipe(NF,b)!=-114 || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\" || sysprobe(\"pipe-closed\")!=1) exit 1;}", HAWK_ENOERR, SYS | STRICTMAP },
	{ "sys-pipe-record-output-atomic", "BEGIN {FS=\" \";OFS=\" \";$0=\"one two three\";FS=\":\";b[1]=1;if(sysfault::pipe($0,b)!=-114 || NF!=3 || $0!==\"one two three\" || $1!==\"one\" || $2!==\"two\" || $3!==\"three\" || sysprobe(\"pipe-closed\")!=1) exit 1;}", HAWK_ENOERR, SYS | STRICTMAP },
	{ "sys-pipe-cache-type-error", "BEGIN {OFS=\":\";b[1]=1;if(sysfault::pipe(OFS,b)!=-114 || OFS!==\":\" || sysprobe(\"pipe-closed\")!=1) exit 1;}", HAWK_ENOERR, SYS | STRICTMAP },
	{ "sys-recvfrom-output-type-error", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);sysprobe(\"recv-no-address-data\");b=@b\"old\";peer[1]=1;if(s<0 || sysfault::recvfrom(s,b,16,peer)!=-114 || b!==@b\"old\" || peer[1]!=1 || length(sysfault::errmsg())==0) exit 1;}", HAWK_ENOERR, SYS | STRICTMAP },
	{ "sys-recvfrom-output-allocation", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);sysprobe(\"recv-no-address-data\");b=@b\"old\";OFS=\":\";arm();n=sysfault::recvfrom(s,b,16,OFS);if(n<0) {if(n!=sysfault::RC_ENOMEM || b!==@b\"old\" || OFS!==\":\" || length(sysfault::errmsg())==0) exit 1;}else if(n!=1 || b!==@b\"Z\" || OFS!==\"\") exit 1;}", HAWK_ENOERR, SYS | DEEP_SWEEP },
	{ "sys-recvfrom-output-same-destination", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);sysprobe(\"recv-no-address-data\");b=\"old\";if(s<0 || sysfault::recvfrom(s,b,16,b)!=1 || b!==\"\") exit 1;}", HAWK_ENOERR, SYS },
	{ "sys-pipe-output-atomic-named", "BEGIN {a=41;FS=\" \";arm();n=sysfault::pipe(a,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || a!==41 || FS!==\" \" || length(sysfault::errmsg())==0 || sysprobe(\"pipe-closed\")!=1) exit 1;f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::close(f)!=0) exit 1;}else {if(sysfault::close(a)!=0 || sysfault::close(FS)!=0) exit 1;} }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-mux-output-atomic-named", "BEGIN {a=41;FS=\" \";m=sysfault::openmux();if(m<0 || sysfault::pipe(a0,b0)!=0 || sysfault::addtomux(m,a0,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b0,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;arm();n=sysfault::getmuxevt(m,0,a,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || a!==41 || FS!==\" \" || length(sysfault::errmsg())==0) exit 1;if(sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a0 || !(ev & sysfault::MUX_EVT_IN)) exit 1;}else if(a!=a0 || FS!=sysfault::MUX_EVT_IN) exit 1;}", HAWK_ENOERR, SYS | EPOLL | SWEEP },
	{ "sys-pipe-output-atomic-local", "BEGIN {@local a; a=41;FS=\" \";arm();n=sysfault::pipe(a,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || a!==41 || FS!==\" \" || length(sysfault::errmsg())==0 || sysprobe(\"pipe-closed\")!=1) exit 1;f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::close(f)!=0) exit 1;}else {if(sysfault::close(a)!=0 || sysfault::close(FS)!=0) exit 1;} }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-mux-output-atomic-local", "BEGIN {@local a; a=41;FS=\" \";m=sysfault::openmux();if(m<0 || sysfault::pipe(a0,b0)!=0 || sysfault::addtomux(m,a0,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b0,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;arm();n=sysfault::getmuxevt(m,0,a,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || a!==41 || FS!==\" \" || length(sysfault::errmsg())==0) exit 1;if(sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a0 || !(ev & sysfault::MUX_EVT_IN)) exit 1;}else if(a!=a0 || FS!=sysfault::MUX_EVT_IN) exit 1;}", HAWK_ENOERR, SYS | EPOLL | SWEEP },
	{ "sys-pipe-output-atomic-global", "@global g; BEGIN {g=41;FS=\" \";arm();n=sysfault::pipe(g,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || g!==41 || FS!==\" \" || length(sysfault::errmsg())==0 || sysprobe(\"pipe-closed\")!=1) exit 1;f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::close(f)!=0) exit 1;}else {if(sysfault::close(g)!=0 || sysfault::close(FS)!=0) exit 1;} }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-mux-output-atomic-global", "@global g; BEGIN {g=41;FS=\" \";m=sysfault::openmux();if(m<0 || sysfault::pipe(a0,b0)!=0 || sysfault::addtomux(m,a0,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b0,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;arm();n=sysfault::getmuxevt(m,0,g,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || g!==41 || FS!==\" \" || length(sysfault::errmsg())==0) exit 1;if(sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a0 || !(ev & sysfault::MUX_EVT_IN)) exit 1;}else if(g!=a0 || FS!=sysfault::MUX_EVT_IN) exit 1;}", HAWK_ENOERR, SYS | EPOLL | SWEEP },
	{ "sys-pipe-output-atomic-indexed", "BEGIN {a[\"out\"]=\"original\";FS=\" \";arm();n=sysfault::pipe(a[\"out\"],FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || a[\"out\"]!==\"original\" || FS!==\" \" || length(sysfault::errmsg())==0 || sysprobe(\"pipe-closed\")!=1) exit 1;f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::close(f)!=0) exit 1;}else {if(sysfault::close(a[\"out\"])!=0 || sysfault::close(FS)!=0) exit 1;} }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-mux-output-atomic-indexed", "BEGIN {a[\"out\"]=\"original\";FS=\" \";m=sysfault::openmux();if(m<0 || sysfault::pipe(a0,b0)!=0 || sysfault::addtomux(m,a0,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b0,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;arm();n=sysfault::getmuxevt(m,0,a[\"out\"],FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || a[\"out\"]!==\"original\" || FS!==\" \" || length(sysfault::errmsg())==0) exit 1;if(sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a0 || !(ev & sysfault::MUX_EVT_IN)) exit 1;}else if(a[\"out\"]!=a0 || FS!=sysfault::MUX_EVT_IN) exit 1;}", HAWK_ENOERR, SYS | EPOLL | SWEEP },
	{ "sys-pipe-output-atomic-cached-global", "BEGIN {OFS=\":\";FS=\" \";arm();n=sysfault::pipe(OFS,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || OFS!==\":\" || FS!==\" \" || length(sysfault::errmsg())==0 || sysprobe(\"pipe-closed\")!=1) exit 1;f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::close(f)!=0) exit 1;}else {if(sysfault::close(OFS)!=0 || sysfault::close(FS)!=0) exit 1;} }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-mux-output-atomic-cached-global", "BEGIN {OFS=\":\";FS=\" \";m=sysfault::openmux();if(m<0 || sysfault::pipe(a0,b0)!=0 || sysfault::addtomux(m,a0,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b0,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;arm();n=sysfault::getmuxevt(m,0,OFS,FS);if(n<0) {if(n!=sysfault::RC_ENOMEM || OFS!==\":\" || FS!==\" \" || length(sysfault::errmsg())==0) exit 1;if(sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a0 || !(ev & sysfault::MUX_EVT_IN)) exit 1;}else if(OFS!=a0 || FS!=sysfault::MUX_EVT_IN) exit 1;}", HAWK_ENOERR, SYS | EPOLL | SWEEP },
	{ "sys-pipe-output-assignment-error", "BEGIN {a=\"original\";b[1]=1;if(sysfault::pipe(a,b)>=0 || a!==\"original\" || b[1]!=1 || length(sysfault::errmsg())==0 || sysprobe(\"pipe-closed\")!=1) exit 1;}", HAWK_ENOERR, SYS | STRICTMAP },
	{ "sys-mux-output-assignment-error", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;id=\"original\";ev[1]=1;if(sysfault::getmuxevt(m,0,id,ev)>=0 || id!==\"original\" || ev[1]!=1 || length(sysfault::errmsg())==0) exit 1;}", HAWK_ENOERR, SYS | EPOLL | STRICTMAP },
	{ "sys-mux-output-same-destination", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::write(b,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,x,x)!=0 || !(x & sysfault::MUX_EVT_IN)) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-dirname-basic", "BEGIN {if(sys::dirname(\"\")!==\"\" || sys::dirname(\"file\")!==\"\" || sys::dirname(\"/\")!==\"/\" || sys::dirname(\"/file\")!==\"/\" || sys::dirname(\"dir/file\")!==\"dir\" || sys::dirname(\"dir/\")!==\"dir\" || sys::dirname(\"dir/a\\0b\")!==\"dir\") exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-dirname-windows-drive-root", "BEGIN {if(sys::dirname(\"C:/file\")!==\"C:/\" || sys::dirname(\"C:/\")!==\"C:/\" || sys::dirname(\"z:/file\")!==\"z:/\" || sys::dirname(\"C:\\\\file\")!==\"C:\\\\\" || sys::dirname(\"C:\\\\\")!==\"C:\\\\\") exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-dirname-windows-subdirectory", "BEGIN {if(sys::dirname(\"C:/dir/file\")!==\"C:/dir\" || sys::dirname(\"C:\\\\dir\\\\file\")!==\"C:\\\\dir\" || sys::dirname(\"C:file\")!==\"\" || sys::dirname(\"1:/file\")!==\"1:\") exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-getppid-windows-success", "BEGIN {if(sys::errmsg()!==\"\") exit 1;p=sys::getppid();if(p<0 || sys::errmsg()!==\"\") exit 1;p=sys::getppid();if(p<0 || sys::errmsg()!==\"\") exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-mux-explicit-delete-error", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-fail-delete\");if(sysfault::delfrommux(m,a)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"mux-delete-calls\")!=1 || sysfault::delfrommux(m,a)!=sysfault::RC_EPERM) exit 1;sysprobe(\"mux-clear-failure\");if(sysfault::close(a)!=0 || sysfault::close(b)!=0 || sysfault::closemux(m)!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-explicit-delete-noautodel-error", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0) exit 1;sysprobe(\"mux-fail-delete\");if(sysfault::delfrommux(m,a)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;sysprobe(\"mux-clear-failure\");}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-explicit-delete-error-recovery", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0) exit 1;k=sysfault::dup(a);if(k<0) exit 1;sysprobe(\"mux-fail-delete\");if(sysfault::delfrommux(m,a)!=sysfault::RC_EACCES) exit 1;sysprobe(\"mux-clear-failure\");if(sysfault::write(b,@b\"old\")!=3 || sysfault::write(d,@b\"new\")!=3 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c || sysfault::read(c,x,3)!=3 || x!==@b\"new\" || sysfault::waitonmux(m,0)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-accept-allocation-preserves-peer", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;peer=\"unchanged\";arm();a=sysfault::accept(s,0,peer);if(a<0) {if(a!=sysfault::RC_ENOMEM || peer!==\"unchanged\" || length(sysfault::errmsg())==0 || sysprobe(\"accepted-closed\")!=1) exit 1;}else {if(peer===\"unchanged\" || sysfault::close(a)!=0) exit 1;}sysfault::close(s);}", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-windows-accept-allocation-preserves-peer", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;peer=\"unchanged\";arm();a=sysfault::accept(s,0,peer);if(a<0) {if(a!=sysfault::RC_ENOMEM || peer!==\"unchanged\" || length(sysfault::errmsg())==0 || sysprobe(\"socket-closed\")!=1) exit 1;}else {if(peer===\"unchanged\" || sysfault::close(a)!=0) exit 1;}sysfault::close(s);}", HAWK_ENOERR, WIN_SYS | SWEEP },
	{ "value-toint-nan", "BEGIN {toint(math::sqrt(-1));exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-positive-infinity", "BEGIN {toint(-math::log(0));exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-negative-infinity", "BEGIN {toint(math::log(0));exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-positive-overflow", "BEGIN {toint(1e100);exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-negative-overflow", "BEGIN {toint(-1e100);exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-string-overflow", "BEGIN {toint(\"1e100\");exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-byte-string-overflow", "BEGIN {toint(@b\"-1e100\");exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-upper-boundary", "BEGIN {limit=-(minint()*1.0);toint(limit);exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-below-lower-boundary", "BEGIN {limit=-(minint()*1.0);toint(-limit-limit/1024);exit 1;}", HAWK_EINVAL, 0 },
	{ "value-toint-valid-boundaries", "BEGIN {limit=-(minint()*1.0);if(toint(-limit)!==minint() || toint(limit-limit/1024)!==maxint()-(maxint()>>10) || toint(minint())!==minint() || toint(maxint())!==maxint()) exit 1;}", HAWK_ENOERR, 0 },
	{ "value-toint-fractional", "BEGIN {if(toint(42.9)!==42 || toint(-42.9)!==-42 || toint(0.9)!==0 || toint(-0.9)!==0 || toint(-0.0)!==0 || toint(\"42.9\")!==42 || toint(@b\"-42.9\")!==-42) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-closelog-io-error", SYS_CLOSELOG_ERROR("file-close-eio", "RC_EIOERR", "file-close-calls", "file-close-normal", "0"), HAWK_ENOERR, SYS },
	{ "sys-openlog-close-io-error", SYS_OPENLOG_CLOSE_ERROR("file-close-eio", "RC_EIOERR", "file-close-calls", "file-close-normal", "0", ""), HAWK_ENOERR, SYS },
	{ "sys-openlog-close-interrupted", SYS_OPENLOG_CLOSE_ERROR("file-close-eintr", "RC_EINTR", "file-close-calls", "file-close-normal", "0", ""), HAWK_ENOERR, SYS },
	{ "sys-openlog-close-before-cleanup", SYS_OPENLOG_CLOSE_ERROR("file-close-eio", "RC_EIOERR", "file-close-calls", "file-close-normal", "1", ""), HAWK_ENOERR, SYS },
	{ "sys-openlog-close-allocation", SYS_OPENLOG_CLOSE_ERROR("file-close-eio", "RC_EIOERR", "file-close-calls", "file-close-normal", "1", "arm();"), HAWK_ENOERR, SYS | SWEEP },
	{ "sys-openlog-close-windows-error", SYS_OPENLOG_CLOSE_ERROR("socket-close-error", "RC_EACCES", "socket-closed", "socket-close-normal", "0", ""), HAWK_ENOERR, WIN_SYS },
	{ "sys-openlog-close-windows-before-cleanup", SYS_OPENLOG_CLOSE_ERROR("socket-close-error", "RC_EACCES", "socket-closed", "socket-close-normal", "1", ""), HAWK_ENOERR, WIN_SYS },
	{ "sys-wait-status-wifexited-function", SYS_WAIT_STATUS_INVALID("WIFEXITED", "bad"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifexited-overflow", SYS_WAIT_STATUS_INVALID("WIFEXITED", "1e100"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifexited-nan", SYS_WAIT_STATUS_INVALID("WIFEXITED", "math::sqrt(-1)"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifexited-unsupported", SYS_WAIT_STATUS_UNSUPPORTED("WIFEXITED"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wexitstatus-function", SYS_WAIT_STATUS_INVALID("WEXITSTATUS", "bad"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wexitstatus-overflow", SYS_WAIT_STATUS_INVALID("WEXITSTATUS", "1e100"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wexitstatus-nan", SYS_WAIT_STATUS_INVALID("WEXITSTATUS", "math::sqrt(-1)"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wexitstatus-unsupported", SYS_WAIT_STATUS_UNSUPPORTED("WEXITSTATUS"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifsignaled-function", SYS_WAIT_STATUS_INVALID("WIFSIGNALED", "bad"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifsignaled-overflow", SYS_WAIT_STATUS_INVALID("WIFSIGNALED", "1e100"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifsignaled-nan", SYS_WAIT_STATUS_INVALID("WIFSIGNALED", "math::sqrt(-1)"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wifsignaled-unsupported", SYS_WAIT_STATUS_UNSUPPORTED("WIFSIGNALED"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wtermsig-function", SYS_WAIT_STATUS_INVALID("WTERMSIG", "bad"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wtermsig-overflow", SYS_WAIT_STATUS_INVALID("WTERMSIG", "1e100"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wtermsig-nan", SYS_WAIT_STATUS_INVALID("WTERMSIG", "math::sqrt(-1)"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wtermsig-unsupported", SYS_WAIT_STATUS_UNSUPPORTED("WTERMSIG"), HAWK_ENOERR, 0 },
#if defined(WCOREDUMP)
	{ "sys-wait-status-wcoredump-function", SYS_WAIT_STATUS_INVALID("WCOREDUMP", "bad"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wcoredump-overflow", SYS_WAIT_STATUS_INVALID("WCOREDUMP", "1e100"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wcoredump-nan", SYS_WAIT_STATUS_INVALID("WCOREDUMP", "math::sqrt(-1)"), HAWK_ENOERR, 0 },
	{ "sys-wait-status-wcoredump-valid", "BEGIN {if(sys::WCOREDUMP(0)!=0 || sys::WCOREDUMP(134)!=1) exit 1;}", HAWK_ENOERR, 0 },
#endif
	{ "sys-wait-status-valid", "BEGIN {n=sys::WIFEXITED(0);if(n==sys::RC_ENOIMPL) {if(sys::errmsg()==\"\" || sys::WEXITSTATUS(0)!=sys::RC_ENOIMPL || sys::WIFSIGNALED(0)!=sys::RC_ENOIMPL || sys::WTERMSIG(0)!=sys::RC_ENOIMPL) exit 1;}else if(n!=1 || sys::WIFEXITED(5888)!=1 || sys::WEXITSTATUS(5888)!=23 || sys::WIFSIGNALED(5888)!=0 || sys::WIFEXITED(15)!=0 || sys::WIFSIGNALED(15)!=1 || sys::WTERMSIG(15)!=15) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-mux-unsupported-add", SYS_MUX_UNSUPPORTED("sysunavailable::addtomux(0,0,0)"), HAWK_ENOERR, 0 },
	{ "sys-mux-unsupported-delete", SYS_MUX_UNSUPPORTED("sysunavailable::delfrommux(0,0)"), HAWK_ENOERR, 0 },
	{ "sys-mux-unsupported-modify", SYS_MUX_UNSUPPORTED("sysunavailable::modinmux(0,0,0)"), HAWK_ENOERR, 0 },
	{ "sys-mux-unsupported-wait", SYS_MUX_UNSUPPORTED("sysunavailable::waitonmux(0,0)"), HAWK_ENOERR, 0 },
	{ "sys-mux-unsupported-get-event", SYS_MUX_UNSUPPORTED("sysunavailable::getmuxevt(0,0,id,ev)"), HAWK_ENOERR, 0 },
	{ "sys-closelog-interrupted", SYS_CLOSELOG_ERROR("file-close-eintr", "RC_EINTR", "file-close-calls", "file-close-normal", "0"), HAWK_ENOERR, SYS },
	{ "sys-closelog-error-before-cleanup", SYS_CLOSELOG_ERROR("file-close-eio", "RC_EIOERR", "file-close-calls", "file-close-normal", "1"), HAWK_ENOERR, SYS },
	{ "sys-closelog-windows-socket-error", SYS_CLOSELOG_ERROR("socket-close-error", "RC_EACCES", "socket-closed", "socket-close-normal", "0"), HAWK_ENOERR, WIN_SYS },
	{ "sys-closelog-windows-error-before-cleanup", SYS_CLOSELOG_ERROR("socket-close-error", "RC_EACCES", "socket-closed", "socket-close-normal", "1"), HAWK_ENOERR, WIN_SYS },
	{ "sys-closelog-unused-remote", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::closelog()!=0 || sysfault::closelog()!=0 || sysprobe(\"socket-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-east", SYS_LOG_ISO8601("log-tz-east", "2024-01-01T05:15:30+05:30"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-west", SYS_LOG_ISO8601("log-tz-west", "2023-12-31T20:45:30-03:30"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-zero", SYS_LOG_ISO8601("log-tz-zero", "2023-12-31T23:45:30+00:00"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-dst", SYS_LOG_ISO8601("log-tz-dst", "2024-01-01T01:45:30+02:00"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-leap-year", SYS_LOG_ISO8601("log-tz-leap-year", "2025-01-01T05:30:30+05:45"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-utc-error", SYS_LOG_ISO8601_UTC_FAILURE("log-tz-utc-error", "RC_EIOERR"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-utc-no-errno", SYS_LOG_ISO8601_UTC_FAILURE("log-tz-utc-no-errno", "RC_EINVAL"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-subminute-offset", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-tz-subminute\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_ISO8601,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_EINVAL || index(sysfault::errmsg(),\"offset\")==0 || sysprobe(\"log-send-calls\")!=0) exit 1;sysfault::closelog();}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-reopen-default", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-tz-east\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_ISO8601,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"first\")!=0 || sysprobe(\"log-message\")!==@b\"<14>2024-01-01T05:15:30+05:30 test: first\" || sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"second\")!=0 || sysprobe(\"log-message\")!==@b\"<14>Jan 01 05:15:30 test: second\" || sysfault::closelog()!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-pid", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-tz-east\");if(sysfault::openlog(\"sock://127.0.0.1:9/test\",sysfault::LOG_OPT_ISO8601|sysfault::LOG_OPT_PID,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=0) exit 1;x=str::tombs(\"<14>2024-01-01T05:15:30+05:30 test[\" sysfault::getpid() \"]: hello\");if(sysprobe(\"log-message\")!==x || sysfault::closelog()!=0) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-iso8601-allocation", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-tz-east\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_ISO8601,sysfault::LOG_FAC_USER)!=0) exit 1;arm();n=sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\");if(n<0 && n!=sysfault::RC_ENOMEM) exit 1;if(n==0 && (sysprobe(\"log-message\")!==@b\"<14>2024-01-01T05:15:30+05:30 test: hello\" || sysprobe(\"log-send-calls\")!=1)) exit 1;sysfault::closelog(); }", HAWK_ENOERR, SOCKET_TEST | SWEEP },
	{ "sys-log-iso8601-local-options", "BEGIN {if(sysfault::openlog(\"local://test\",sysfault::LOG_OPT_ISO8601|sysfault::LOG_OPT_PID,sysfault::LOG_FAC_USER)!=0 || sysprobe(\"native-log-calls\")!=1 || sysprobe(\"native-log-options\")!==sysfault::LOG_OPT_PID || sysfault::closelog()!=0) exit 1;}", HAWK_ENOERR, SYS },
	{ "sys-mux-noautodel-api", "BEGIN {if(sys::MUX_FLAG_NOAUTODEL!=1) exit 1;m=sys::openmux();if(m==sys::RC_ENOIMPL) {if(sys::addtomux(-1,-1,sys::MUX_EVT_IN,sys::MUX_FLAG_NOAUTODEL)!=sys::RC_ENOIMPL) exit 1;}else if(m<0 || sys::closemux(m)!=0) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-mux-noautodel-close", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0 || sysfault::write(b,@b\"X\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;k=sysfault::dup(a);sysprobe(\"mux-save-event\");if(k<0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=0) exit 1;id=41;ev=42;if(sysfault::getmuxevt(m,0,id,ev)!=sysfault::RC_ENOENT || id!=41 || ev!=42) exit 1;sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::waitonmux(m,0)!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-keepfd", "BEGIN {r=sysprobe(\"rawfd\");a=sysfault::openfd(r);w=sysfault::openfd(sysprobe(\"rawfd-writer\"));m=sysfault::openmux();if(a<0 || w<0 || m<0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0 || sysfault::write(w,@b\"X\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::close(a,sysfault::C_KEEPFD)!=0 || sysprobe(\"rawfd-open\")!=1 || sysprobe(\"mux-delete-calls\")!=0) exit 1;id=41;ev=42;if(sysfault::getmuxevt(m,0,id,ev)!=sysfault::RC_ENOENT || id!=41 || ev!=42 || sysfault::closemux(m)!=0 || sysprobe(\"mux-delete-calls\")!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-explicit-delete-reset", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0 || sysfault::delfrommux(m,a)!=0 || sysprobe(\"mux-delete-calls\")!=1 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=2) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-mod-preserves", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0 || sysfault::modinmux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-closemux-mixed", "BEGIN {m=sysfault::openmux();n=sysfault::openmux();if(m<0 || n<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN,0)!=0 || sysfault::closemux(m)!=0 || sysprobe(\"mux-delete-calls\")!=1 || sysfault::addtomux(n,a,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(n,c,sysfault::MUX_EVT_IN)!=0 || sysfault::closemux(n)!=0 || sysprobe(\"mux-delete-calls\")!=3) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-alias-close", "BEGIN {if(sysfault::pipe(a,b)!=0) exit 1;e=sysfault::openfd(sysprobe(\"pipe-reader\"));m=sysfault::openmux();k=sysfault::dup(a);if(e<0 || m<0 || k<0 || sysfault::addtomux(m,e,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0 || sysfault::write(b,@b\"X\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=0) exit 1;id=41;ev=42;if(sysfault::getmuxevt(m,0,id,ev)!=sysfault::RC_ENOENT || id!=41 || ev!=42 || sysfault::close(e)!=0 || sysfault::waitonmux(m,0)!=0 || sysfault::waitonmux(m,0)!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-runtime-cleanup", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=0) exit 1;sysprobe(\"mux-expect-no-cleanup-delete\");}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-noautodel-failed-add-preserves", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0) exit 1;sysprobe(\"mux-fail-add\");if(sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,sysfault::MUX_FLAG_NOAUTODEL)!=sysfault::RC_EACCES) exit 1;sysprobe(\"mux-clear-failure\");if(sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-addtomux-invalid-unknown-flags", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,2)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysprobe(\"mux-add-calls\")!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-addtomux-invalid-negative-flags", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,-1)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysprobe(\"mux-add-calls\")!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-addtomux-invalid-wide-flags", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,4294967297)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysprobe(\"mux-add-calls\")!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL | INT32_NARROW },
	{ "sys-addtomux-invalid-function-flags", "function f() {} BEGIN {bad=f;expected=sysfault::openfd(bad);message=sysfault::errmsg();m=sysfault::openmux();if(expected>=0 || m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,bad)!=expected || sysfault::errmsg()!==message || sysprobe(\"mux-add-calls\")!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-addtomux-invalid-bob-flags", "function f() {} BEGIN {bad=bob();expected=sysfault::openfd(bad);message=sysfault::errmsg();m=sysfault::openmux();if(expected>=0 || m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN,bad)!=expected || sysfault::errmsg()!==message || sysprobe(\"mux-add-calls\")!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysprobe(\"mux-delete-calls\")!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-noautodel-child-cleanup", "BEGIN {if(sys::pipe(a,b)!=0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN,sys::MUX_FLAG_NOAUTODEL)!=0) exit 1;pid=sys::fork();if(pid<0) exit 1;if(pid==0) exit 0;if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=1 || sys::getmuxevt(m,0,id,ev)!=0 || id!=a || sys::read(a,x,1)!=1 || x!==@b\"X\") exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-noautodel-closemux", "BEGIN {if(sys::pipe(a,b)!=0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN,sys::MUX_FLAG_NOAUTODEL)!=0) exit 1;pid=sys::fork();if(pid<0) exit 1;if(pid==0) {if(sys::closemux(m)!=0) exit 1;exit 0;}if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=1 || sys::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-explicit-delete", "BEGIN {if(sys::pipe(a,b)!=0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN,sys::MUX_FLAG_NOAUTODEL)!=0) exit 1;pid=sys::fork();if(pid<0) exit 1;if(pid==0) {if(sys::delfrommux(m,a)!=0) exit 1;exit 0;}if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-shared-cached-events", "BEGIN {if(sys::pipe(a,b)!=0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN,sys::MUX_FLAG_NOAUTODEL)!=0 || sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=1) exit 1;pid=sys::fork();if(pid<0) exit 1;if(pid==0) {id=41;ev=42;if(sys::getmuxevt(m,0,id,ev)!=0 || id!=a || sys::waitonmux(m,0)!=1 || sys::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1;exit 0;}if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::getmuxevt(m,0,id,ev)!=0 || id!=a || sys::waitonmux(m,0)!=1) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-shared-oneshot", "BEGIN {flags=sys::MUX_EVT_IN|sysprobe(\"mux-oneshot\");if(sys::pipe(a,b)!=0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,flags,sys::MUX_FLAG_NOAUTODEL)!=0 || sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=1 || sys::read(a,x,1)!=1 || sys::write(b,@b\"Y\")!=1) exit 1;pid=sys::fork();if(pid<0) exit 1;if(pid==0) {if(sys::waitonmux(m,0)!=0 || sys::modinmux(m,a,flags)!=0 || sys::waitonmux(m,0)!=1) exit 1;exit 0;}if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::waitonmux(m,0)!=0 || sys::modinmux(m,a,flags)!=0 || sys::waitonmux(m,0)!=1 || sys::read(a,x,1)!=1 || x!==@b\"Y\") exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-separate-muxes", "BEGIN {if(sys::pipe(a,b)!=0) exit 1;pid=sys::fork();if(pid<0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)!=0) exit 1;if(pid==0) {if(sys::waitonmux(m,0)!=0) exit 1;exit 0;}if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=1 || sys::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-fork-default-child-cleanup", "BEGIN {if(sys::pipe(a,b)!=0) exit 1;m=sys::openmux();if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN,0)!=0) exit 1;pid=sys::fork();if(pid<0) exit 1;if(pid==0) exit 0;if(sys::wait(pid,s)!=pid || !sys::WIFEXITED(s) || sys::WEXITSTATUS(s)!=0) exit 1;if(sys::write(b,@b\"X\")!=1 || sys::waitonmux(m,0)!=0) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-token-rewatched-node", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-save-event\");if(sysfault::close(a)!=0 || sysfault::pipe(c,d)!=0 || c!=a || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::write(d,@b\"X\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-token-zero", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-save-zero-token\");sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::write(b,@b\"X\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-token-invalid-id", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-save-invalid-id\");sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::write(b,@b\"X\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1;}", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-seek-wide-result", "BEGIN {p=sys::tempname(\"hawk-wide-seek-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;}half=(maxint() >> 1)+1;ok=sys::fseek(f,half,sys::SEEK_SET)==half;sys::openfd(-1);n=sys::fseek(f,half,sys::SEEK_CUR);ok=ok && n==sys::RC_EINVAL && sys::errmsg()===\"seek position out of range\" && sys::fseek(f,-1,sys::SEEK_CUR)==maxint();sys::close(f);sys::unlink(p);if(!ok) exit 1;}", HAWK_ENOERR, SEEK_WIDE_RESULT },
	{ "sys-seek-wide-result-clears-buffer", "BEGIN {p=sys::tempname(\"hawk-wide-seek-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;}ok=sys::write(f,@b\"A:LEFT:\")==7 && sys::fseek(f,0,sys::SEEK_SET)==0 && sys::read(f,x,8192,\":\")==2;n=sys::fseek(f,maxint(),sys::SEEK_CUR);ok=ok && n==sys::RC_EINVAL && sys::read(f,x,5)==0 && sys::fseek(f,-3,sys::SEEK_CUR)==maxint()-1;sys::close(f);sys::unlink(p);if(!ok) exit 1;}", HAWK_ENOERR, SEEK_WIDE_RESULT },
	{ "sys-unpack-integer-boundaries", "BEGIN {for(i=1;i<=2;i++) {fmt=(i==1?\"<\":\">\");if(sys::pack(b,fmt \"qqQ\",minint(),maxint(),maxint())!=0 || sys::unpack(b,fmt \"qqQ\",a,c,d)!=0 || a!=minint() || c!=maxint() || d!=maxint()) exit 1;}}", HAWK_ENOERR, 0 },
	{ "sys-unpack-signed-wide-overflow", "BEGIN {sys::pack(b,\"<q\",0);if(length(b)>intsize()) {b=substr(b,1,intsize()) @b\"\\x01\" substr(b,intsize()+2);v=777;if(sys::unpack(b,\"<q\",v)!=sys::RC_EINVAL || v!=777 || length(sys::errmsg())==0) exit 1;}}", HAWK_ENOERR, 0 },
	{ "sys-unpack-signed-wide-underflow", "BEGIN {sys::pack(b,\"<Q\",-1);if(length(b)>intsize()) {b=substr(b,1,intsize()-1) @b\"\\x7f\" substr(b,intsize()+1);v=777;if(sys::unpack(b,\"<q\",v)!=sys::RC_EINVAL || v!=777 || length(sys::errmsg())==0) exit 1;}}", HAWK_ENOERR, 0 },
	{ "sys-unpack-unsigned-wide-overflow", "BEGIN {for(i=1;i<=2;i++) {fmt=(i==1?\"<Q\":\">Q\");sys::pack(b,fmt,-1);v=777;if(sys::unpack(b,fmt,v)!=sys::RC_EINVAL || v!=777 || length(sys::errmsg())==0) exit 1;}}", HAWK_ENOERR, 0 },
	{ "sys-unpack-unsigned-native-overflow", "BEGIN {sys::pack(b,\"<N\",-1);v=777;r=sys::unpack(b,\"<N\",v);if(intsize()<=length(b)) {if(r!=sys::RC_EINVAL || v!=777) exit 1;} else if(r!=0 || v<=0) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-unpack-unsigned32-range", "BEGIN {v=777;r=sys::unpack(@b\"\\xff\\xff\\xff\\xff\",\">I\",v);if(intsize()<=4) {if(r!=sys::RC_EINVAL || v!=777) exit 1;} else if(r!=0 || v!=4294967295) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-unpack-signed64-range", "BEGIN {v=777;r=sys::unpack(@b\"\\x00\\x00\\x00\\x00\\x01\\x00\\x00\\x00\",\"<l\",v);if(intsize()<=4) {if(r!=sys::RC_EINVAL || v!=777) exit 1;} else if(r!=0 || v!=4294967296) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-unpack-unsigned64-range", "BEGIN {v=777;r=sys::unpack(@b\"\\xff\\xff\\xff\\xff\\xff\\xff\\xff\\xff\",\">L\",v);if(intsize()<=8) {if(r!=sys::RC_EINVAL || v!=777) exit 1;} else if(r!=0 || v<=0) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-unpack-overflow-partial-outputs", "BEGIN {sys::pack(b,\"BQ\",65,-1);a=11;c=22;if(sys::unpack(b,\"BQ\",a,c)!=sys::RC_EINVAL || a!=65 || c!=22) exit 1;}", HAWK_ENOERR, 0 },
	{ "sys-mux-external-close-freed-node", "BEGIN {m=sysfault::openmux();r=sysprobe(\"rawfd\");a=sysfault::openfd(r);w=sysfault::openfd(sysprobe(\"rawfd-writer\"));k=sysfault::dup(a);e=sysfault::openfd(r);if(m<0 || r<0 || a<0 || w<0 || k<0 || e<0 || sysfault::addtomux(m,e,sysfault::MUX_EVT_IN)!=0 || sysfault::write(w,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;sysprobe(\"mux-save-event\");if(sysprobe(\"rawfd-release-reader\")!=r || sysfault::close(e,sysfault::C_KEEPFD)!=0 || sysfault::close(a,sysfault::C_KEEPFD)!=0) exit 1;c=sysfault::open(\"/dev/null\",sysfault::O_RDONLY);d=sysfault::open(\"/dev/null\",sysfault::O_RDONLY);if(c!=a || d!=e || sysfault::waitonmux(m,0)!=0) exit 1;sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-win-dup-alias-buffer-replacement", "BEGIN {p=sysfault::tempname(\"hawk-dup-alias-\");q=sysfault::tempname(\"hawk-dup-alias-\");if(p==\"\" || q==\"\") exit 1;a=sysfault::open(p,sysfault::O_RDWR);b=sysfault::open(q,sysfault::O_RDWR);if(a<0 || b<0 || sysfault::write(a,@b\"NEW!\")!=4 || sysfault::fseek(a,0,sysfault::SEEK_SET)!=0 || sysfault::write(b,@b\"A:OLD!\")!=6) exit 1;e=sysfault::openfd(sysprobe(\"file-fd\"));if(e<0 || sysfault::fseek(b,0,sysfault::SEEK_SET)!=0 || sysfault::read(e,x,8192,\":\")!=2 || x!==@b\"A:\" || sysfault::dup(a,b)!=b || sysfault::read(e,x,4)!=4 || x!==@b\"NEW!\" || sysfault::fseek(b,0,sysfault::SEEK_SET)!=0 || sysfault::read(b,x,4)!=4 || x!==@b\"NEW!\") exit 1;if(sysfault::close(a)!=0 || sysfault::close(b)!=0 || sysfault::close(e)!=0 || sysfault::unlink(p)!=0 || sysfault::unlink(q)!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-dup-alias-buffer-replacement", "BEGIN {if(sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0) exit 1;e=sysfault::openfd(sysprobe(\"pipe-reader\"));if(e<0 || sysfault::write(d,@b\"A\\nOLD\\n\")!=6 || sysfault::read(e,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\" || sysfault::dup(a,c)!=c || sysfault::write(b,@b\"NEW\\n\")!=4 || sysfault::read(e,x,4)!=4 || x!==@b\"NEW\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-dup-alias-failure-preserves-buffer-watch", "BEGIN {if(sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0) exit 1;e=sysfault::openfd(sysprobe(\"pipe-reader\"));m=sysfault::openmux();if(e<0 || m<0 || sysfault::addtomux(m,e,sysfault::MUX_EVT_IN)!=0 || sysfault::write(d,@b\"A\\nOLD\\n\")!=6 || sysfault::waitonmux(m,0)!=1 || sysfault::read(e,x,8192,\"\\n\")!=2 || sysfault::dup(a,c,1)!=sysfault::RC_EINVAL || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=e || sysfault::read(e,x,4)!=4 || x!==@b\"OLD\\n\" || sysfault::write(d,@b\"Z\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=e) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-dup-alias-watches-replacement", "BEGIN {if(sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0) exit 1;e=sysfault::openfd(sysprobe(\"pipe-reader\"));k=sysfault::dup(c);m=sysfault::openmux();n=sysfault::openmux();if(e<0 || k<0 || m<0 || n<0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(n,e,sysfault::MUX_EVT_IN)!=0 || sysfault::write(d,@b\"old\")!=3 || sysfault::waitonmux(m,0)!=1 || sysfault::waitonmux(n,0)!=1 || sysfault::dup(a,c)!=c) exit 1;id=41;ev=42;if(sysfault::getmuxevt(m,0,id,ev)!=sysfault::RC_ENOENT || sysfault::getmuxevt(n,0,id,ev)!=sysfault::RC_ENOENT || id!=41 || ev!=42 || sysfault::close(e)!=0 || sysfault::waitonmux(m,0)!=0 || sysfault::waitonmux(n,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-dup-alias-socket-type", "BEGIN {if(sysfault::pipe(a,b)!=0) exit 1;e=sysfault::openfd(sysprobe(\"pipe-reader\"));s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(e<0 || s<0 || sysfault::dup(s,a)!=a || sysfault::shutdown(e,0)!=0 || sysprobe(\"shutdown-calls\")!=1) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-dup-alias-fallback-flag-failure", "BEGIN {r=sysprobe(\"rawfd\");a=sysfallback::openfd(r);e=sysfallback::openfd(r);w=sysfallback::openfd(sysprobe(\"rawfd-writer\"));if(r<0 || a<0 || e<0 || w<0 || sysfallback::pipe(c,d)!=0 || sysfallback::write(w,@b\"A\\nOLD\\n\")!=6 || sysfallback::read(e,x,8192,\"\\n\")!=2) exit 1;sysprobe(\"fail-setfd\");n=sysfallback::dup(c,a,sysfallback::O_CLOEXEC);sysprobe(\"clear-fcntl-failure\");if(n!=sysfallback::RC_EINVAL || sysfallback::write(d,@b\"NEW\\n\")!=4 || sysfallback::read(e,x,4)!=4 || x!==@b\"NEW\\n\") exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-mux-external-close-rebuild", "BEGIN {m=sysfault::openmux();r=sysprobe(\"rawfd\");a=sysfault::openfd(r);w=sysfault::openfd(sysprobe(\"rawfd-writer\"));k=sysfault::dup(a);if(m<0 || r<0 || a<0 || w<0 || k<0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0 || sysfault::write(w,@b\"old\")!=3 || sysfault::waitonmux(m,0)!=1 || sysprobe(\"rawfd-release-reader\")!=r || sysfault::close(a,sysfault::C_KEEPFD)!=0) exit 1;id=41;ev=42;if(sysfault::getmuxevt(m,0,id,ev)!=sysfault::RC_ENOENT || id!=41 || ev!=42) exit 1;e=sysfault::open(\"/dev/null\",sysfault::O_RDONLY);if(e!=a || sysfault::write(d,@b\"new\")!=3 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c || sysfault::read(c,x,3)!=3 || x!==@b\"new\") exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-external-close-delete", "BEGIN {m=sysfault::openmux();r=sysprobe(\"rawfd\");a=sysfault::openfd(r);w=sysfault::openfd(sysprobe(\"rawfd-writer\"));k=sysfault::dup(a);if(m<0 || r<0 || a<0 || w<0 || k<0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysprobe(\"rawfd-release-reader\")!=r || sysfault::delfrommux(m,a)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysfault::write(w,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-token-node-reuse", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-save-event\");if(sysfault::close(a)!=0) exit 1;c=sysfault::open(\"/dev/null\",sysfault::O_RDONLY);if(c!=a) exit 1;sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-token-registration-reuse", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-save-event\");if(sysfault::delfrommux(m,a)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(m,0)!=0 || sysfault::write(b,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-token-wrong-mux", "BEGIN {m=sysfault::openmux();n=sysfault::openmux();if(m<0 || n<0 || sysfault::pipe(a,b)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0) exit 1;sysprobe(\"mux-save-event\");sysprobe(\"mux-inject-event\");if(sysfault::waitonmux(n,0)!=0 || sysfault::write(b,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-rebuild-create-failure", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0) exit 1;k=sysfault::dup(a);sysprobe(\"mux-fail-delete\");if(k<0 || sysfault::close(a)!=0 || sysfault::write(b,@b\"old\")!=3 || sysfault::write(d,@b\"new\")!=3) exit 1;sysprobe(\"mux-fail-create\");if(sysfault::waitonmux(m,0)!=sysfault::RC_EACCES || sysprobe(\"mux-wait-calls\")!=0 || length(sysfault::errmsg())==0 || sysfault::modinmux(m,c,sysfault::MUX_EVT_IN)!=sysfault::RC_EACCES) exit 1;sysprobe(\"mux-clear-failure\");if(sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-rebuild-add-failure", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0) exit 1;k=sysfault::dup(a);sysprobe(\"mux-fail-delete\");if(k<0 || sysfault::close(a)!=0 || sysfault::write(b,@b\"old\")!=3 || sysfault::write(d,@b\"new\")!=3) exit 1;sysprobe(\"mux-fail-add\");if(sysfault::waitonmux(m,0)!=sysfault::RC_EACCES || sysprobe(\"mux-wait-calls\")!=0 || length(sysfault::errmsg())==0) exit 1;sysprobe(\"mux-clear-failure\");if(sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-rebuild-oneshot", "BEGIN {m=sysfault::openmux();flags=sysfault::MUX_EVT_IN|sysprobe(\"mux-oneshot\");if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::addtomux(m,c,flags)!=0 || sysfault::write(d,@b\"first\")!=5 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c || sysfault::read(c,x,5)!=5 || sysfault::write(d,@b\"second\")!=6) exit 1;sysprobe(\"mux-fail-delete\");if(sysfault::close(a)!=0) exit 1;sysprobe(\"mux-clear-failure\");if(sysfault::waitonmux(m,0)!=0 || sysfault::modinmux(m,c,flags)!=0 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c || sysfault::read(c,x,6)!=6 || x!==@b\"second\") exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-io-negative-defaults", "BEGIN {p=sys::tempname(\"hawk-defaults-\");if(p==\"\") exit 1;a=sys::open(p,sys::O_RDWR,-1);if(a<0) {sys::unlink(p);exit 1;}ok=sys::write(a,@b\"ABC\",-1)==3 && sys::fseek(a,0,sys::SEEK_SET)==0 && sys::read(a,x,0)==3 && x===@b\"ABC\";if(sys::fseek(a,0,sys::SEEK_SET)!=0 || sys::write(a,@b\"X\",1,-1)!=0 || sys::fseek(a,0,sys::SEEK_CUR)!=0 || sys::read(a,x,-1)!=3 || x!==@b\"ABC\" || sys::close(a,-1)!=0) ok=0;a=sys::open(p,-1,-1);if(a<0 || sys::read(a,x,3)!=3 || x!==@b\"ABC\") ok=0;if(a>=0) sys::close(a);if(sys::unlink(p)!=0 || !ok) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mkdir-negative-default", "BEGIN {p=sys::tempname(\"hawk-defaults-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;n=sys::mkdir(p,-1);if(n!=0) exit 1;if(sys::rmdir(p)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-dir-strftime-negative-defaults", "BEGIN {a=sys::opendir(\".\",-1);if(a<0 || sys::closedir(a)!=0 || sys::strftime(\"%Y-%m-%d\",0,-1)!==sys::strftime(\"%Y-%m-%d\",0,0)) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-close-flags-invalid-function", SYS_IO_BAD_NUMBER("sys::close(a,v)","bad"), HAWK_ENOERR, 0 },
	{ "sys-open-flags-invalid-function", SYS_OPEN_BAD_NUMBER("v","bad"), HAWK_ENOERR, 0 },
	{ "sys-open-mode-invalid-function", SYS_OPEN_BAD_NUMBER("sys::O_RDWR|sys::O_TRUNC,v","bad"), HAWK_ENOERR, 0 },
	{ "sys-read-limit-invalid-function", SYS_IO_BAD_NUMBER("sys::read(a,x,v)","bad"), HAWK_ENOERR, 0 },
	{ "sys-write-start-invalid-function", SYS_IO_BAD_NUMBER("sys::write(a,@b\"BAD\",v)","bad"), HAWK_ENOERR, 0 },
	{ "sys-write-limit-invalid-function", SYS_IO_BAD_NUMBER("sys::write(a,@b\"BAD\",1,v)","bad"), HAWK_ENOERR, 0 },
	{ "sys-mkdir-mode-invalid-function", SYS_MKDIR_BAD_NUMBER("bad"), HAWK_ENOERR, 0 },
	{ "sys-opendir-flags-invalid-function", SYS_DIR_BAD_NUMBER("bad"), HAWK_ENOERR, 0 },
	{ "sys-pipe-flags-invalid-function", SYS_PIPE_BAD_NUMBER("sys","bad"), HAWK_ENOERR, SYS },
	{ "sys-pipe-fallback-flags-invalid-function", SYS_PIPE_BAD_NUMBER("sysfault","bad"), HAWK_ENOERR, SYS },
	{ "sys-tcflush-selector-invalid-function", SYS_TCFLUSH_BAD_NUMBER("bad"), HAWK_ENOERR, SYS },
	{ "sys-socket-domain-invalid-function", SYS_SOCKET_BAD_NUMBER("v,sysfault::SOCK_STREAM,0","bad"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-type-invalid-function", SYS_SOCKET_BAD_NUMBER("sysfault::AF_INET,v,0","bad"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-protocol-invalid-function", SYS_SOCKET_BAD_NUMBER("sysfault::AF_INET,sysfault::SOCK_STREAM,v","bad"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-shutdown-how-invalid-function", SYS_SOCKET_CONTROL_BAD_NUMBER("sysfault::shutdown(s,v)","shutdown-calls","bad"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-listen-backlog-invalid-function", SYS_SOCKET_CONTROL_BAD_NUMBER("sysfault::listen(s,v)","listen-calls","bad"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-accept-flags-invalid-function", SYS_SOCKET_CONTROL_BAD_NUMBER("sysfault::accept(s,v,peer)","accept-calls","bad"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-recvfrom-limit-invalid-function", SYS_RECVFROM_BAD_NUMBER("bad"), HAWK_ENOERR, SYS },
	{ "sys-win-recvfrom-limit-invalid-function", SYS_WIN_RECVFROM_BAD_NUMBER("bad"), HAWK_ENOERR, WIN_SYS },
	{ "sys-strftime-seconds-invalid-function", "function bad() {} BEGIN { sys::strftime(\"%Y\",bad); }", HAWK_EVALTONUM, 0 },
	{ "sys-strftime-flags-invalid-function", "function bad() {} BEGIN { sys::strftime(\"%Y\",0,bad); }", HAWK_EVALTONUM, 0 },
	{ "sys-close-flags-invalid-bob", SYS_IO_BAD_NUMBER("sys::close(a,v)","bob()"), HAWK_ENOERR, 0 },
	{ "sys-open-flags-invalid-bob", SYS_OPEN_BAD_NUMBER("v","bob()"), HAWK_ENOERR, 0 },
	{ "sys-open-mode-invalid-bob", SYS_OPEN_BAD_NUMBER("sys::O_RDWR|sys::O_TRUNC,v","bob()"), HAWK_ENOERR, 0 },
	{ "sys-read-limit-invalid-bob", SYS_IO_BAD_NUMBER("sys::read(a,x,v)","bob()"), HAWK_ENOERR, 0 },
	{ "sys-write-start-invalid-bob", SYS_IO_BAD_NUMBER("sys::write(a,@b\"BAD\",v)","bob()"), HAWK_ENOERR, 0 },
	{ "sys-write-limit-invalid-bob", SYS_IO_BAD_NUMBER("sys::write(a,@b\"BAD\",1,v)","bob()"), HAWK_ENOERR, 0 },
	{ "sys-mkdir-mode-invalid-bob", SYS_MKDIR_BAD_NUMBER("bob()"), HAWK_ENOERR, 0 },
	{ "sys-opendir-flags-invalid-bob", SYS_DIR_BAD_NUMBER("bob()"), HAWK_ENOERR, 0 },
	{ "sys-pipe-flags-invalid-bob", SYS_PIPE_BAD_NUMBER("sys","bob()"), HAWK_ENOERR, SYS },
	{ "sys-pipe-fallback-flags-invalid-bob", SYS_PIPE_BAD_NUMBER("sysfault","bob()"), HAWK_ENOERR, SYS },
	{ "sys-tcflush-selector-invalid-bob", SYS_TCFLUSH_BAD_NUMBER("bob()"), HAWK_ENOERR, SYS },
	{ "sys-socket-domain-invalid-bob", SYS_SOCKET_BAD_NUMBER("v,sysfault::SOCK_STREAM,0","bob()"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-type-invalid-bob", SYS_SOCKET_BAD_NUMBER("sysfault::AF_INET,v,0","bob()"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-protocol-invalid-bob", SYS_SOCKET_BAD_NUMBER("sysfault::AF_INET,sysfault::SOCK_STREAM,v","bob()"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-shutdown-how-invalid-bob", SYS_SOCKET_CONTROL_BAD_NUMBER("sysfault::shutdown(s,v)","shutdown-calls","bob()"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-listen-backlog-invalid-bob", SYS_SOCKET_CONTROL_BAD_NUMBER("sysfault::listen(s,v)","listen-calls","bob()"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-accept-flags-invalid-bob", SYS_SOCKET_CONTROL_BAD_NUMBER("sysfault::accept(s,v,peer)","accept-calls","bob()"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-recvfrom-limit-invalid-bob", SYS_RECVFROM_BAD_NUMBER("bob()"), HAWK_ENOERR, SYS },
	{ "sys-win-recvfrom-limit-invalid-bob", SYS_WIN_RECVFROM_BAD_NUMBER("bob()"), HAWK_ENOERR, WIN_SYS },
	{ "sys-strftime-seconds-invalid-bob", "function bad() {} BEGIN { sys::strftime(\"%Y\",bob()); }", HAWK_EVALTONUM, 0 },
	{ "sys-strftime-flags-invalid-bob", "function bad() {} BEGIN { sys::strftime(\"%Y\",0,bob()); }", HAWK_EVALTONUM, 0 },
	{ "close-to-preserves-input", "BEGIN { x=\"to\"; y=x; close(\"unused\",x); if(x!=\"to\" || y!=\"to\") exit 1; }", HAWK_ENOERR, 0 },
	{ "close-from-preserves-input", "BEGIN { x=\"from\"; y=x; close(\"unused\",x); if(x!=\"from\" || y!=\"from\") exit 1; }", HAWK_ENOERR, 0 },
	{ "byte-toupper", "BEGIN { if(toupper(@b'a')!==@b'A' || toupper(@b'z')!==@b'Z' || tolower(@b'A')!==@b'a') exit 1; }", HAWK_ENOERR, 0 },
	{ "byte-sub-escaped-ampersand", "BEGIN { x=@b\"a\"; if(sub(/a/,@b\"\\\\&Z\",x)!=1 || x!==@b\"&Z\") exit 1; }", HAWK_ENOERR, 0 },
	{ "byte-gsub-escaped-backslash", "BEGIN { x=@b\"aa\"; if(gsub(/a/,@b\"\\\\\\\\Z\",x)!=2 || x!==@b\"\\\\Z\\\\Z\") exit 1; }", HAWK_ENOERR, 0 },
	{ "byte-gensub-escaped-ampersand", "BEGIN { if(gensub(/a/,\"\\\\&Z\",\"g\",@b\"aa\")!==@b\"&Z&Z\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-large-comparator", "function cmp(a,b) { return (a>b)? 4294967296: (a<b)? -4294967296: 0; } BEGIN { @local a,b; a=@[2,1,3]; asort(a,b,cmp); if(b[1]!=1 || b[2]!=2 || b[3]!=3) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-comparator-string-lifetime", "function cmp(a,b) { return sprintf(\"%d\",a-b); } BEGIN { @local a,b; a=@[2,1,3]; if(asort(a,b,cmp)!=3 || b[1]!=1 || b[2]!=2 || b[3]!=3) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-comparator-error-lifetime", "function cmp(a,b) { return bob(); } BEGIN { @local a,b; a=@[2,1,3]; asort(a,b,cmp); }", HAWK_EVALTONUM, 0 },
	{ "sort-implicit-map", "BEGIN { a[1]=2; a[2]=1; if(asort(a,b)!=2 || b[1]!=1 || b[2]!=2) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-implicit-array", "BEGIN { a=@[2,1]; if(asort(a,b)!=2 || b[1]!=1 || b[2]!=2) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-implicit-keys", "BEGIN { a[\"b\"]=2; a[\"a\"]=1; if(asorti(a,b)!=2 || b[1]!=\"a\" || b[2]!=\"b\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-empty-map-clears-destination", "BEGIN { @local a,b; a[1]=1; delete a; b[1]=99; if(asort(a,b)!=0 || length(b)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-empty-array-clears-destination", "BEGIN { @local a,b; a=@[]; b=@[99]; if(asort(a,b)!=0 || length(b)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-nil-clears-destination", "BEGIN { @local a,b; b[1]=99; if(asort(a,b)!=0 || length(b)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sort-empty-keys-clear-destination", "BEGIN { @local a,b; a=@[]; b=@[99]; if(asorti(a,b)!=0 || length(b)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "substr-minimum-index", "BEGIN { if(substr(\"abc\",minint(),1)!==\"a\" || substr(@b\"abc\",minint(),1)!==@b\"a\") exit 1; }", HAWK_ENOERR, 0 },
	{ "gensub-invalid-mode", "function f() {} BEGIN { gensub(/a/,\"x\",f,\"a\"); }", HAWK_EVALTONUM, 0 },
	{ "index-empty-input", "BEGIN { if(index(\"\",\"\")!=1 || index(\"\",\"\",0)!=1 || index(\"\",\"\",1)!=1 || index(\"\",\"\",2)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "index-byte-empty-input", "BEGIN { if(index(@b\"\",@b\"\")!=1 || index(@b\"\",@b\"\",0)!=1 || index(@b\"\",@b\"\",1)!=1 || index(@b\"\",@b\"\",2)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "index-empty-input-nonempty-needle", "BEGIN { if(index(\"\",\"x\")!=0 || index(\"\",\"x\",1)!=0 || index(@b\"\",@b\"x\")!=0 || index(@b\"\",@b\"x\",1)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "index-empty-needle-boundaries", "BEGIN { if(index(\"abc\",\"\")!=1 || index(\"abc\",\"\",2)!=2 || index(\"abc\",\"\",-1)!=3 || index(@b\"abc\",@b\"\")!=1 || index(@b\"abc\",@b\"\",2)!=2 || index(@b\"abc\",@b\"\",-1)!=3) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-no-match-clears-captures", "BEGIN { c[1]=\"stale\"; if(match(\"random-string\",/^(.+:.+:.+:.+:.+:.+)$/,c)!=0 || length(c)!=0 || RSTART!=0 || RLENGTH!=-1) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-empty-input", "BEGIN { if(match(\"\",/^$/,c)!=1 || c[0]!==\"\" || c[0,\"start\"]!=1 || c[0,\"length\"]!=0 || RLENGTH!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-input-capture-alias", "BEGIN { x=\"abc123\"; if(match(x,/([a-z]+)([0-9]+)/,x)!=1 || x[0]!=\"abc123\" || x[1]!=\"abc\" || x[2]!=\"123\") exit 1; }", HAWK_ENOERR, 0 },
	{ "match-converted-input-lifetime", "BEGIN { if(match(12345,/(23)/,c)!=2 || c[0]!=\"23\" || c[1]!=\"23\" || c[1,\"start\"]!=2) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-unmatched-leading-group", "BEGIN { if(match(\"b\",/(a)?(b)/,c)!=1 || length(c)!=6 || c[0]!==\"b\" || c[2]!==\"b\" || c[2,\"start\"]!=1 || c[2,\"length\"]!=1 || (1 in c) || ((1,\"start\") in c) || ((1,\"length\") in c)) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-unmatched-middle-group", "BEGIN { if(match(\"ab\",/(a)(x)?(b)/,c)!=1 || length(c)!=9 || c[1]!==\"a\" || c[3]!==\"b\" || c[3,\"start\"]!=2 || c[3,\"length\"]!=1 || (2 in c) || ((2,\"start\") in c) || ((2,\"length\") in c)) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-byte-unmatched-leading-group", "BEGIN { if(match(@b\"b\",/(a)?(b)/,c)!=1 || length(c)!=6 || c[0]!==@b\"b\" || c[2]!==@b\"b\" || c[2,\"start\"]!=1 || c[2,\"length\"]!=1 || (1 in c) || ((1,\"start\") in c) || ((1,\"length\") in c)) exit 1; }", HAWK_ENOERR, 0 },
	{ "match-byte-unmatched-middle-group", "BEGIN { if(match(@b\"ab\",/(a)(x)?(b)/,c)!=1 || length(c)!=9 || c[1]!==@b\"a\" || c[3]!==@b\"b\" || c[3,\"start\"]!=2 || c[3,\"length\"]!=1 || (2 in c) || ((2,\"start\") in c) || ((2,\"length\") in c)) exit 1; }", HAWK_ENOERR, 0 },
	{ "split-final-token", "BEGIN { if(split(\"one,two\",a,\",\")!=2 || a[1]!=\"one\" || a[2]!=\"two\") exit 1; if(split(@b\"one,two\",a,\",\")!=2 || a[2]!==@b\"two\") exit 1; }", HAWK_ENOERR, 0 },
	{ "split-unrepresentable-separator", "BEGIN { split(@b\"abc\",a,\"\\uAC00\"); }", HAWK_EECERR, MB8 },
	{ "split-byte-input-allocation", "BEGIN { @local s,a; s=bob(); arm(); if(split(s,a,\",\")!=1 || a[1]!==@b\"abc\") exit 1; }", HAWK_ENOERR, SWEEP },
	{ "split-byte-separator-allocation", "BEGIN { @local s,a; s=@b\"abc\"; arm(); if(split(s,a,\",\")!=1 || a[1]!==@b\"abc\") exit 1; }", HAWK_ENOERR, SWEEP },
	{ "split-regex-allocation", "BEGIN { @local s,a; s=\"abc,,def\"; arm(); if(split(s,a,/,+/)!=2 || a[1]!=\"abc\" || a[2]!=\"def\") exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sort-keys-allocation", "BEGIN { @local a,b; a=@[3,1,2]; arm(); if(asorti(a,b)!=3 || b[1]!=1 || b[2]!=2 || b[3]!=3) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "str-tocharcode-minimum-index", "BEGIN { @local m; m=minint(); if(str::tocharcode(\"abc\",m)!==@nil || str::tocharcode(@b\"abc\",m)!==@nil || str::tocharcode('a',m)!==@nil || str::tocharcode(@b'a',m)!==@nil || str::tocharcode(bob(),m)!==@nil) exit 1; }", HAWK_ENOERR, 0 },
	{ "str-subchar-minimum-index", "BEGIN { @local m; m=minint(); if(str::subchar(\"abc\",m)!==@nil || str::subchar(@b\"abc\",m)!==@nil || str::subchar('a',m)!==@nil || str::subchar(@b'a',m)!==@nil || str::subchar(bob(),m)!==@nil) exit 1; }", HAWK_ENOERR, 0 },
	{ "str-character-index-boundaries", "BEGIN { @local i; for(i=-1;i<=0;i++) if(str::tocharcode(\"abc\",i)!==@nil || str::subchar(@b\"abc\",i)!==@nil) exit 1; if(str::tocharcode(\"abc\")!=97 || str::tocharcode(@b\"abc\",3)!=99 || str::subchar(\"abc\",1)!=='a' || str::subchar(@b\"abc\",3)!==@b'c' || str::tocharcode(\"abc\",maxint())!==@nil || str::subchar(@b\"abc\",maxint())!==@nil) exit 1; }", HAWK_ENOERR, 0 },
	{ "str-tonum-valid-bases", "BEGIN { @local b; for(b=2;b<=36;b++) if(str::tonum(\"10\",b)!=b || str::tonum(@b\"10\",b)!=b || str::tonum('1',b)!=1 || str::tonum(@b'1',b)!=1) exit 1; if(str::tonum(\"0x10\",0)!=16 || str::tonum(@b\"0b10\",0)!=2 || str::tonum('7',0)!=7 || str::tonum(@b'7',0)!=7 || str::tonum(bob(),16)!=2748 || str::tonum(7,-1)!=7) exit 1; }", HAWK_ENOERR, 0 },
	{ "str-tonum-negative-string-base", "BEGIN { str::tonum(\"10\",-1); }", HAWK_EINVAL, 0 },
	{ "str-tonum-negative-byte-string-base", "BEGIN { str::tonum(@b\"10\",-1); }", HAWK_EINVAL, 0 },
	{ "str-tonum-negative-character-base", "BEGIN { str::tonum('1',-1); }", HAWK_EINVAL, 0 },
	{ "str-tonum-negative-byte-character-base", "BEGIN { str::tonum(@b'1',-1); }", HAWK_EINVAL, 0 },
	{ "str-tonum-negative-bob-base", "BEGIN { str::tonum(bob(),-1); }", HAWK_EINVAL, 0 },
	{ "str-tonum-base-one", "BEGIN { str::tonum(\"10\",1); }", HAWK_EINVAL, 0 },
	{ "str-tonum-base-thirty-seven", "BEGIN { str::tonum(@b\"10\",37); }", HAWK_EINVAL, 0 },
	{ "str-tonum-truncated-base", "BEGIN { str::tonum(\"10\",16777218); }", HAWK_EINVAL, 0 },
	{ "str-tonum-maximum-base", "BEGIN { str::tonum(\"10\",maxint()); }", HAWK_EINVAL, 0 },
	{ "str-tonum-minimum-base", "BEGIN { str::tonum(@b\"10\",minint()); }", HAWK_EINVAL, 0 },
	{ "str-tonum-invalid-base-type", "function f() {} BEGIN { str::tonum(\"10\",f); }", HAWK_EVALTONUM, 0 },
	{ "str-tohex-size-overflow", "BEGIN { str::tohex(oversized_mbs(0)); }", HAWK_ENOMEM, 0 },
	{ "str-tohex-allocation-size-overflow", "BEGIN { str::tohex(oversized_mbs(1)); }", HAWK_ENOMEM, 0 },
	{ "str-tohex-allocation", "BEGIN { @local s; s=bob(); arm(); if(str::tohex(s)!==@b\"616263\" || str::tohex(@b\"\")!==@b\"\") exit 1; }", HAWK_ENOERR, SWEEP },
	{ "hawk-hash-allocation", "BEGIN { arm(); if(hawk::hash(\"abc\")<0) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "hawk-hash-byte-allocation", "BEGIN { arm(); if(hawk::hash(@b\"abc\")<0) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "hawk-call-variadic-name", "function f(first, ...) { return @argc*100+first+@argv[1]+@argv[2]; } BEGIN { if(f(10,20,30)!=360 || hawk::call(\"f\",10,20,30)!=360) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-call-variadic-value", "function f(first, ...) { return @argc*100+first+@argv[1]+@argv[2]; } BEGIN { if(hawk::call(f,10,20,30)!=360) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-call-variadic-empty", "function f(...) { return @argc; } BEGIN { if(hawk::call(\"f\")!=0 || hawk::call(f)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-call-fixed-arguments", "function f(x) { return x; } BEGIN { hawk::call(\"f\",1,2); }", HAWK_EARGTM, 0 },
	{ "hawk-function-exists-functions", "function f() {} BEGIN { if(hawk::function_exists(\"f\")!=1 || hawk::function_exists(\"length\")!=1 || hawk::function_exists(\"hawk::type\")!=1 || hawk::function_exists(\"hawk::missing_function\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-function-exists-constant", "BEGIN { if(hawk::function_exists(\"hawk::VAL_INT\")!=0 || hawk::func_exists(\"hawk::GC_NUM_GENS\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-cmgr-exists-names", "BEGIN { if(hawk::cmgr_exists(\"utf8\")!=1 || hawk::cmgr_exists(@b\"utf8\")!=1 || hawk::cmgr_exists(\"missing_cmgr\")!=0 || hawk::cmgr_exists(\"\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-cmgr-exists-nul", "BEGIN { if(hawk::cmgr_exists(\"utf8\\0garbage\")!=0 || hawk::cmgr_exists(\"utf8\\0\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-cmgr-exists-byte-nul", "BEGIN { if(hawk::cmgr_exists(@b\"utf8\\0garbage\")!=0 || hawk::cmgr_exists(@b\"utf8\\0\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "hawk-gc-generation-boundaries", "BEGIN { @local g; for(g=0;g<hawk::GC_NUM_GENS;g++) if(hawk::gc(g)!=g) exit 1; if(hawk::gc(999)!=hawk::GC_NUM_GENS-1) exit 1; g=hawk::gc(-1); if(g<0 || g>=hawk::GC_NUM_GENS) exit 1; }", HAWK_ENOERR, GC },
	{ "hawk-gc-generation-wraps-zero", "BEGIN { if(hawk::gc(4294967296)!=hawk::GC_NUM_GENS-1) exit 1; }", HAWK_ENOERR, GC | INT32_NARROW },
	{ "hawk-gc-generation-wraps-negative", "BEGIN { if(hawk::gc(2147483648)!=hawk::GC_NUM_GENS-1) exit 1; }", HAWK_ENOERR, GC | INT32_NARROW },
	{ "math-srand-previous-seed", "BEGIN { math::srand(123); if(math::srand(456)!=123 || math::srand(789)!=456 || math::srand()!=789) exit 1; }", HAWK_ENOERR, 0 },
	{ "math-srand-zero-seed", "BEGIN { math::srand(0); if(math::srand(1)!=0 || math::srand(0)!=1 || math::srand(2)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "math-srand-effective-seed", "BEGIN { math::srand(4294967297); if(math::srand(-1)!=1 || math::srand(0)!=4294967295) exit 1; }", HAWK_ENOERR, INT32_NARROW },
	{ "math-srand-repeatable-sequence", "BEGIN { math::srand(123); for(i=1;i<=20;i++) a[i]=math::rand(); math::srand(123); for(i=1;i<=20;i++) if(math::rand()!=a[i]) exit 1; }", HAWK_ENOERR, 0 },
	{ "math-srand-invalid-type", "function f() {} BEGIN { math::srand(f); }", HAWK_EVALTONUM, 0 },
	{ "math-rand-range", "BEGIN { math::srand(123); for(i=0;i<1000;i++) { x=math::rand(); if(x<0 || x>=1) exit 1; } }", HAWK_ENOERR, 0 },
	{ "math-rand-maximum", "BEGIN { x=randtest::rand(); if(x<0 || x>=1 || x<0.99) exit 1; }", HAWK_ENOERR, 0 },
	{ "math-result-allocation", "BEGIN { arm(); if(math::sqrt(4)!=2 || math::atan2(0,1)!=0 || math::rand()<0) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "math-shared-runtime-state", "function sample(seed) { @local x; x=math::rand(); if(x<0 || x>=1) return -1; return math::srand(seed); }", HAWK_ENOERR, THREADS },
	{ "math-time-failure", "BEGIN { if(timetest::srand(123)!=0 || timetest::srand()!=123 || timetest::srand(456)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "math-unary-invalid-type", "function f() {} BEGIN { math::sqrt(f); }", HAWK_EVALTONUM, 0 },
	{ "math-binary-first-invalid-type", "function f() {} BEGIN { math::atan2(f,1); }", HAWK_EVALTONUM, 0 },
	{ "math-binary-second-invalid-type", "function f() {} BEGIN { math::atan2(1,f); }", HAWK_EVALTONUM, 0 },
	{ "math-known-values", "BEGIN { if(math::ceil(-1.5)!=-1 || math::floor(-1.5)!=-2 || math::round(-1.5)!=-2 || math::round(1.5)!=2 || math::sqrt(4)!=2 || (math::log2(8)<2.999999999 || math::log2(8)>3.000000001) || math::sin(0)!=0 || math::cos(0)!=1 || math::atan2(0,1)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "math-log2-quad-precision", "BEGIN { @local d,x,y; d=2.0**-100; x=1.0+d; y=math::log2(x); if(x<=1 || !(y>d && y<2*d)) exit 1; }", HAWK_ENOERR, QUAD },
	{ "sys-symlink-string-byte-cleanup", "BEGIN { a=\"target\";b=@b\"unused\\0suffix\";arm();r=sys::symlink(a,b);if((r!=sys::RC_EINVAL && r!=sys::RC_ENOMEM) || a!==\"target\" || b!==@b\"unused\\0suffix\") exit 1; }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-symlink-byte-string-cleanup", "BEGIN { a=@b\"target\";b=\"unused\\0suffix\";arm();r=sys::symlink(a,b);if((r!=sys::RC_EINVAL && r!=sys::RC_ENOMEM) || a!==@b\"target\" || b!==\"unused\\0suffix\") exit 1; }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-symlink-string-bchar-cleanup", "BEGIN { if(sys::symlink(\"target\\0suffix\",@b'x')!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-symlink-byte-byte-cleanup", "BEGIN { a=@b\"target\";b=@b\"unused\\0suffix\";if(sys::symlink(a,b)!=sys::RC_EINVAL || a!==@b\"target\" || b!==@b\"unused\\0suffix\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-symlink-string-byte-success", "BEGIN { p=sys::tempname(\"hawk-symlink-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;a=\"target\";b=str::tombs(p);r=sys::symlink(a,b);u=sys::unlink(p);if(r!=0 || u!=0 || a!==\"target\" || b!==str::tombs(p)) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-symlink-byte-string-success", "BEGIN { p=sys::tempname(\"hawk-symlink-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;a=@b\"target\";r=sys::symlink(a,p);u=sys::unlink(p);if(r!=0 || u!=0 || a!==@b\"target\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-fcntl-get-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-fcntl-einval\");if(sysfault::fcntl(f,sysfault::F_GETFD)!=sysfault::RC_EINVAL || sysfault::fcntl(f,sysfault::F_GETFL)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-fcntl-set-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-fcntl-einval\");if(sysfault::fcntl(f,sysfault::F_SETFD,0)!=sysfault::RC_EINVAL || sysfault::fcntl(f,sysfault::F_SETFL,0)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-flock-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-fcntl-einval\");pid=42;if(sysfault::flock(f,sysfault::FLOCK_READ,0,0,sysfault::SEEK_SET)!=sysfault::RC_EINVAL || sysfault::flock(f,sysfault::FLOCK_WRITE|sysfault::FLOCK_GET,0,0,sysfault::SEEK_SET,pid)!=sysfault::RC_EINVAL || pid!=42 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcgetattr-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-tcgetattr\");t=42;if(sysfault::tcgetattr(f,t)!=sysfault::RC_EINVAL || t!=42 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcsetattr-get-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;t[\"lflag\"]=0;sysprobe(\"fail-tcgetattr\");if(sysfault::tcsetattr(f,0,t)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcsetattr-set-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;t[\"lflag\"]=0;sysprobe(\"fail-tcsetattr\");if(sysfault::tcsetattr(f,0,t)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcsetraw-get-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-tcgetattr\");if(sysfault::tcsetraw(f)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcsetraw-set-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-tcsetattr\");if(sysfault::tcsetraw(f)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcflush-errno", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0) exit 1;sysprobe(\"fail-tcflush\");if(sysfault::tcflush(f,0)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-tcflush-input-discards-residue", SYS_TERMINAL_RESIDUE "if(sysfault::tcflush(f,sysfault::TC_IFLUSH)!=0) exit 1; " SYS_TERMINAL_RESIDUE_CLEARED, HAWK_ENOERR, SYS },
	{ "sys-tcflush-both-discards-residue", SYS_TERMINAL_RESIDUE "if(sysfault::tcflush(f,sysfault::TC_IOFLUSH)!=0) exit 1; " SYS_TERMINAL_RESIDUE_CLEARED, HAWK_ENOERR, SYS },
	{ "sys-tcflush-output-keeps-residue", SYS_TERMINAL_RESIDUE "if(sysfault::tcflush(f,sysfault::TC_OFLUSH)!=0) exit 1; " SYS_TERMINAL_RESIDUE_KEPT, HAWK_ENOERR, SYS },
	{ "sys-tcflush-failure-keeps-residue", SYS_TERMINAL_RESIDUE "sysprobe(\"fail-tcflush\");if(sysfault::tcflush(f,sysfault::TC_IFLUSH)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; " SYS_TERMINAL_RESIDUE_KEPT, HAWK_ENOERR, SYS },
	{ "sys-tcsetattr-flush-discards-residue", SYS_TERMINAL_RESIDUE "t[\"lflag\"]=0;if(sysfault::tcsetattr(f,sysfault::TC_SAFLUSH,t)!=0) exit 1; " SYS_TERMINAL_RESIDUE_CLEARED, HAWK_ENOERR, SYS },
	{ "sys-tcsetattr-now-keeps-residue", SYS_TERMINAL_RESIDUE "t[\"lflag\"]=0;if(sysfault::tcsetattr(f,sysfault::TC_SANOW,t)!=0) exit 1; " SYS_TERMINAL_RESIDUE_KEPT, HAWK_ENOERR, SYS },
	{ "sys-tcsetattr-drain-keeps-residue", SYS_TERMINAL_RESIDUE "t[\"lflag\"]=0;if(sysfault::tcsetattr(f,sysfault::TC_SADRAIN,t)!=0) exit 1; " SYS_TERMINAL_RESIDUE_KEPT, HAWK_ENOERR, SYS },
	{ "sys-tcsetattr-flush-failure-keeps-residue", SYS_TERMINAL_RESIDUE "t[\"lflag\"]=0;sysprobe(\"fail-tcsetattr\");if(sysfault::tcsetattr(f,sysfault::TC_SAFLUSH,t)!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; " SYS_TERMINAL_RESIDUE_KEPT, HAWK_ENOERR, SYS },
	{ "sys-unpack-padding-bounds", "BEGIN { x=42; if(sys::unpack(@b\"A\",\"2x\")!=sys::RC_EINVAL || sys::unpack(@b\"A\",\"64xB\",x)!=sys::RC_EINVAL || x!=42) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-unpack-padding-exact", "BEGIN { if(sys::unpack(@b\"AB\",\"2x\")!=0 || sys::unpack(@b\"AB\",\"1xB\",x)!=0 || x!=66 || sys::unpack(@b\"\",\"0x\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-pack-repeat-overflow", "BEGIN { x=@b\"keep\"; if(sys::pack(x,\"18446744073709551616x\")!=sys::RC_EINVAL || x!==@b\"keep\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-pack-alignment-overflow", "BEGIN { x=@b\"keep\"; if(sys::pack(x,\"18446744073709551360x\")!=sys::RC_EINVAL || x!==@b\"keep\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-unpack-repeat-overflow", "BEGIN { if(sys::unpack(@b\"A\",\"18446744073709551616x\")!=sys::RC_EINVAL || sys::unpack(@b\"A\",\"18446744073709551615s\",x)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-pack-unpack-roundtrip", "BEGIN { if(sys::pack(b,\"<bBhHiIlLfd3s2x\",-1,255,-123,65535,-123456,4294967295,-123456789,123456789,1.5,2.5,@b\"abc\")!=0) exit 1; if(sys::unpack(b,\"<bBhHiIlLfd3s2x\",a,c,d,e,f,g,h,i,j,k,l)!=0 || a!=-1 || c!=255 || d!=-123 || e!=65535 || f!=-123456 || g!=4294967295 || h!=-123456789 || i!=123456789 || j!=1.5 || k!=2.5 || l!==@b\"abc\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-unpack-assignment-failure", "BEGIN { if(sys::unpack(@b\"((\",\"2s\",FS)>=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-pack-allocation", "BEGIN { arm(); if(sys::pack(b,\"300x\")<0) exit 0; if(length(b)!=300) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-unpack-allocation", "BEGIN { @local b; b=@b\"abc\"; arm(); if(sys::unpack(b,\"3s\",x)<0) exit 0; if(x!==b) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-read-descriptor-isolation", "BEGIN { sys::pipe(a,b);sys::pipe(c,d);sys::write(b,@b\"A\\nLEFT\\n\");sys::write(d,@b\"B\\n\");if(sys::read(a,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\") exit 1;if(sys::read(c,x,8192,\"\\n\")!=2 || x!==@b\"B\\n\") exit 1;if(sys::read(a,x,8192,\"\\n\")!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-delimiter-ff", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\xffB\\xff\")!=4 || sys::read(a,x,8192,@b\"\\xff\")!=2 || x!==@b\"A\\xff\" || sys::read(a,x,8192,@b\"\\xff\")!=2 || x!==@b\"B\\xff\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-delimiter-80", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\x80B\\x80\")!=4 || sys::read(a,x,8192,@b\"\\x80\")!=2 || x!==@b\"A\\x80\" || sys::read(a,x,8192,@b\"\\x80\")!=2 || x!==@b\"B\\x80\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-delimiter-nul", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\0B\\0\")!=4 || sys::read(a,x,8192,@b\"\\0\")!=2 || x!==@b\"A\\0\" || sys::read(a,x,8192,@b\"\\0\")!=2 || x!==@b\"B\\0\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-blocking-buffered", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\nLEFT\\n\")!=7 || sys::read(a,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\" || sys::read(a,x)!=5 || x!==@b\"LEFT\\n\" || sys::write(b,@b\"NEW\")!=3) exit 1;sys::close(b);if(sys::read(a,x)!=3 || x!==@b\"NEW\" || sys::read(a,x)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-nonblocking-buffered", "BEGIN {if(sys::pipe(a,b,sys::O_NONBLOCK)<0 || sys::write(b,@b\"A\\nLEFT\\n\")!=7 || sys::read(a,x,8192,\"\\n\")!=2 || sys::read(a,x)!=5 || x!==@b\"LEFT\\n\" || sys::read(a,x)!=sys::RC_EAGAIN) exit 1;sys::close(b);if(sys::read(a,x)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-buffered-empty-delimiter", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\nLEFT\\n\")!=7 || sys::read(a,x,8192,\"\\n\")!=2 || sys::read(a,x,8192,@b\"\")!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-buffered-partial", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\nLEFT\\n\")!=7 || sys::read(a,x,8192,\"\\n\")!=2 || sys::read(a,x,3)!=3 || x!==@b\"LEF\" || sys::read(a,x,8192)!=2 || x!==@b\"T\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-buffered-allocation", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\nLEFT\\n\")!=7 || sys::read(a,x,8192,\"\\n\")!=2) exit 1;arm();n=sys::read(a,x);if(n==sys::RC_ENOMEM) n=sys::read(a,x);if(n!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-read-assignment-failure-partial", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\n((TAIL\")!=8 || sys::close(b)!=0 || sys::read(a,x,8192,\"\\n\")!=2) exit 1;if(sys::read(a,FS,2)>=0 || length(sys::errmsg())==0 || sys::read(a,x,8192)!=6 || x!==@b\"((TAIL\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-assignment-failure-full", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\n((TAIL\")!=8 || sys::close(b)!=0 || sys::read(a,x,8192,\"\\n\")!=2) exit 1;if(sys::read(a,FS,8192)>=0 || sys::read(a,x,8192)!=6 || x!==@b\"((TAIL\" || sys::read(a,x,8192)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-assignment-failure-delimiter", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\n((\\nTAIL\")!=9 || sys::close(b)!=0 || sys::read(a,x,8192,\"\\n\")!=2) exit 1;if(sys::read(a,FS,8192,\"\\n\")>=0 || sys::read(a,x,8192)!=7 || x!==@b\"((\\nTAIL\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-eof-residue", "BEGIN { sys::pipe(a,b);sys::write(b,@b\"A\\nTAIL\");sys::close(b);if(sys::read(a,x,8192,\"\\n\")!=2 || sys::read(a,x,8192,\"\\n\")!=4 || x!==@b\"TAIL\" || sys::read(a,x,8192,\"\\n\")!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-residue-limit", "BEGIN { sys::pipe(a,b);sys::write(b,@b\"A\\nLONGTAIL\\n\");sys::close(b);sys::read(a,x,8192,\"\\n\");if(sys::read(a,x,3,\"\\n\")!=3 || x!==@b\"LON\" || sys::read(a,x,3,\"\\n\")!=3 || x!==@b\"GTA\" || sys::read(a,x,3,\"\\n\")!=3 || x!==@b\"IL\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-closed-handle-residue", "BEGIN { sys::pipe(a,b);sys::pipe(c,d);sys::write(b,@b\"A\\nOLD\\n\");sys::write(d,@b\"NEW\\n\");sys::read(a,x,8192,\"\\n\");sys::close(a);if(sys::read(c,x,8192,\"\\n\")!=4 || x!==@b\"NEW\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-read-allocation", "BEGIN { sys::pipe(a,b);sys::write(b,@b\"abc\");sys::close(b);arm();n=sys::read(a,x,3);if(n<0) exit 0;if(n!=3 || x!==@b\"abc\") exit 1; }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-write-offset-length", "BEGIN { sys::pipe(a,b);if(sys::write(b,@b\"abcdef\",3,2)!=2 || sys::write(b,@b\"abcdef\",99,2)!=0 || sys::write(b,@b\"abcdef\",1,0)!=0) exit 1;sys::close(b);if(sys::read(a,x)!=2 || x!==@b\"cd\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-pipe-teardown", "BEGIN { if(sys::pipe(a,b)<0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-directory-teardown", "BEGIN { if(sys::opendir(\".\")<0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-teardown-handle-holes", "BEGIN {sys::pipe(a,b);sys::pipe(c,d);sys::close(a);sys::close(d); }", HAWK_ENOERR, SYS },
	{ "sys-resetdir-failure-lifetime", "BEGIN { d=sys::opendir(\".\");if(d<0 || sys::resetdir(d,@b\"/nonexistent-hawk-regression-dir\")>=0) exit 1;sys::closedir(d); }", HAWK_ENOERR, SYS },
	{ "sys-nul-paths", "BEGIN { if(sys::open(\".\\0suffix\",sys::O_RDONLY)!=sys::RC_EINVAL || sys::opendir(@b\".\\0suffix\")!=sys::RC_EINVAL) exit 1;d=sys::opendir(\".\");if(d<0 || sys::resetdir(d,\".\\0suffix\")!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mux-keepfd", "BEGIN { m=sys::openmux();r=sysprobe(\"rawfd\");a=sys::openfd(r);if(m<0 || r<0 || a<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)<0) exit 1;sys::close(a,sys::C_KEEPFD);if(sys::closemux(m)<0 || sysprobe(\"rawfd-open\")!=1) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-reuse", "BEGIN {m=sys::openmux();sys::pipe(a,b);sys::pipe(c,d);if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)<0 || sys::addtomux(m,c,sys::MUX_EVT_IN)<0) exit 1;sys::close(a);n=sys::openmux();if(n<0 || sys::closemux(n)<0 || sys::closemux(m)<0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-teardown", "BEGIN {m=sys::openmux();sys::pipe(a,b);if(m<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)<0) exit 1;sys::write(b,@b\"x\");if(sys::waitonmux(m,0)!=1) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-large-timeout", "BEGIN {m=sys::openmux();if(m<0 || sys::waitonmux(m,1e100)!=sys::RC_EINVAL || sys::waitonmux(m,2147483.649)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-empty", "BEGIN {m=sys::openmux();if(m<0 || sys::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-sleep-invalid-time", "BEGIN { if(sys::sleep(1e100)!=sys::RC_EINVAL || sys::sleep(-0.1)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-sleep-negative-subnanosecond", "BEGIN {if(sys::sleep(-1e-10)!=sys::RC_EINVAL || length(sys::errmsg())==0 || sys::sleep(-1e-100)!=sys::RC_EINVAL || sys::sleep(0)!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-sleep-negative-subnanosecond-string", "BEGIN {if(sys::sleep(\"-0.0000000001\")!=sys::RC_EINVAL || length(sys::errmsg())==0 || sys::sleep(\"-1e-100\")!=sys::RC_EINVAL || sys::sleep(\"0\")!=0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-settime-invalid-function", "function bad() {} BEGIN {expected=sys::openfd(bad);message=sys::errmsg();if(expected>=0 || length(message)==0 || sys::settime(bad)!=expected || sys::errmsg()!==message) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-settime-invalid-bob", "BEGIN {v=bob();expected=sys::openfd(v);message=sys::errmsg();if(expected>=0 || length(message)==0 || sys::settime(v)!=expected || sys::errmsg()!==message) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-settime-conversion-before-os", "function bad() {} BEGIN {expected=sysfault::openfd(bad);message=sysfault::errmsg();if(expected>=0 || length(message)==0 || sysfault::settime(bad)!=expected || sysfault::errmsg()!==message || sysprobe(\"settime-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-settime-os-error", "BEGIN {sysfault::close(-1);old=sysfault::errmsg();if(sysfault::settime(0)!=sysfault::RC_EACCES || sysprobe(\"settime-calls\")!=1 || length(sysfault::errmsg())==0 || sysfault::errmsg()===old || sysprobe(\"settime-seconds\")!=0 || sysprobe(\"settime-nanoseconds\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-settime-os-success", "BEGIN {sysprobe(\"settime-success\");if(sysfault::settime(123)!=0 || sysprobe(\"settime-calls\")!=1 || sysprobe(\"settime-seconds\")!=123 || sysprobe(\"settime-nanoseconds\")!=0 || sysfault::settime(-1)!=0 || sysprobe(\"settime-calls\")!=2 || sysprobe(\"settime-seconds\")!=-1 || sysprobe(\"settime-nanoseconds\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-sleep-interrupted-errno", "BEGIN {sysprobe(\"sleep-eintr\");n=sysfault::sleep(0.5);if(sysprobe(\"sleep-calls\")!=1 || n!=sysfault::RC_EINTR) exit 1; }", HAWK_ENOERR, SYS | SLEEP_OS },
	{ "sys-sleep-interrupted-message", "BEGIN {sysfault::close(-1);old=sysfault::errmsg();sysprobe(\"sleep-eintr\");sysfault::sleep(0.5);if(sysprobe(\"sleep-calls\")!=1 || length(sysfault::errmsg())==0 || sysfault::errmsg()==old) exit 1; }", HAWK_ENOERR, SYS | SLEEP_OS },
	{ "sys-sleep-os-einval", "BEGIN {sysprobe(\"sleep-einval\");n=sysfault::sleep(0.5);if(sysprobe(\"sleep-calls\")!=1 || n!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | SLEEP_OS },
	{ "sys-sleep-os-success", "BEGIN {if(sysfault::sleep(0)!=0 || sysfault::sleep(0.5)!=0 || sysprobe(\"sleep-calls\")!=2) exit 1; }", HAWK_ENOERR, SYS | SLEEP_OS },
	{ "sys-signal-positive-overflow", "function handler(s) {} BEGIN {if(sys::signal(4294967297,handler)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, INT32_NARROW },
	{ "sys-signal-overflow-preserves-handler", "function original(s) { good++; } function replacement(s) { bad++; } BEGIN {good=bad=0;if(sys::signal(1,original)!=0) exit 1;n=sys::signal(4294967297,replacement);if(n!=sys::RC_EINVAL) exit 1;n=sys::raise(1);if(n!=0 || good!=1 || bad!=0) exit 1;sys::signal(1,@nil); }", HAWK_ENOERR, INT32_NARROW },
	{ "sys-signal-overflow-unregister", "function handler(s) { calls++; } BEGIN {calls=0;if(sys::signal(1,handler)!=0 || sys::signal(4294967297,@nil)!=sys::RC_EINVAL) exit 1;n=sys::raise(1);if(n!=0 || calls!=1) exit 1;sys::signal(1,@nil); }", HAWK_ENOERR, INT32_NARROW },
	{ "sys-raise-positive-overflow", "function handler(s) { calls++; } BEGIN {calls=0;if(sys::signal(1,handler)!=0) exit 1;n=sys::raise(4294967297);if(n!=sys::RC_EINVAL || calls!=0 || length(sys::errmsg())==0) exit 1;sys::signal(1,@nil); }", HAWK_ENOERR, INT32_NARROW },
	{ "sys-raise-negative-overflow", "function handler(s) { calls++; } BEGIN {calls=0;if(sys::signal(1,handler)!=0) exit 1;n=sys::raise(-4294967295);if(n!=sys::RC_EINVAL || calls!=0 || length(sys::errmsg())==0) exit 1;sys::signal(1,@nil); }", HAWK_ENOERR, INT32_NARROW },
	{ "sys-signal-valid", "function handler(s) { calls++; if(s!=1) exit 1; } BEGIN {calls=0;if(sys::signal(1,handler)!=0) exit 1;n=sys::raise(1);if(n!=0 || calls!=1 || sys::signal(1,@nil)!=0 || sys::raise(1)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-kill-pid-positive-overflow", "BEGIN {if(sysfault::kill(4294967297,0)!=sysfault::RC_EINVAL || sysprobe(\"kill-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | PID32_NARROW },
	{ "sys-kill-pid-negative-overflow", "BEGIN {if(sysfault::kill(-4294967295,0)!=sysfault::RC_EINVAL || sysprobe(\"kill-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | PID32_NARROW },
	{ "sys-kill-signal-positive-overflow", "BEGIN {if(sysfault::kill(123,4294967296)!=sysfault::RC_EINVAL || sysprobe(\"kill-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-kill-signal-negative-overflow", "BEGIN {if(sysfault::kill(123,-4294967295)!=sysfault::RC_EINVAL || sysprobe(\"kill-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-kill-negative-signal", "BEGIN {if(sysfault::kill(123,-1)!=sysfault::RC_EINVAL || sysprobe(\"kill-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-kill-native-invalid-pid", "BEGIN {if(sys::kill(0,sys::SIGKILL)!=sys::RC_EINVAL || sys::kill(-1,sys::SIGKILL)!=sys::RC_EINVAL || sys::kill(minint(),sys::SIGKILL)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL },
	{ "sys-kill-native-invalid-signal", "BEGIN {if(sys::kill(sys::getpid(),-1)!=sys::RC_EINVAL || sys::kill(sys::getpid(),minint())!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL },
	{ "sys-kill-native-unsupported-signal", "BEGIN {if(sys::kill(sys::getpid(),sys::SIGTERM)!=sys::RC_ENOSUP || sys::kill(sys::getpid(),sys::SIGINT)!=sys::RC_ENOSUP || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL },
	{ "sys-kill-native-pid-overflow", "BEGIN {if(sys::kill(4294967297,sys::SIGKILL)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | INT32_NARROW },
	{ "sys-kill-native-signal-overflow", "BEGIN {if(sys::kill(sys::getpid(),4294967296)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | INT32_NARROW },
	{ "sys-kill-os2-zero-unsupported", "BEGIN {if(sys::kill(sys::getpid(),0)!=sys::RC_ENOSUP || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | OS2_KILL },
	{ "sys-kill-windows-self-probe", "BEGIN {for(i=0;i<100;i++) if(sysfault::kill(sysfault::getpid(),0)!=0 || sysprobe(\"kill-handles\")!=0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | WIN_KILL },
	{ "sys-kill-windows-missing-process", "BEGIN {sysprobe(\"kill-missing\");if(sysfault::kill(123,0)!=sysfault::RC_ENOENT || length(sysfault::errmsg())==0 || sysprobe(\"kill-handles\")!=0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | WIN_KILL },
	{ "sys-kill-windows-access-denied", "BEGIN {sysprobe(\"kill-denied\");if(sysfault::kill(123,sysfault::SIGKILL)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"kill-handles\")!=0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | WIN_KILL },
	{ "sys-kill-windows-wait-failure", "BEGIN {sysprobe(\"kill-wait-failure\");if(sysfault::kill(sysfault::getpid(),0)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"kill-handles\")!=0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | WIN_KILL },
	{ "sys-kill-windows-terminate-failure", "BEGIN {p=sysprobe(\"kill-child\");if(p<=0) exit 1;sysprobe(\"kill-terminate-failure\");if(sysfault::kill(p,sysfault::SIGKILL)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"kill-handles\")!=0 || sys::kill(p,0)!=0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | WIN_KILL },
	{ "sys-kill-windows-child", "BEGIN {p=sysprobe(\"kill-child\");if(p<=0 || sys::kill(p,0)!=0 || sys::kill(p,sys::SIGKILL)!=0 || sysprobe(\"kill-child-exited\")!=1 || sys::kill(p,0)!=sys::RC_ENOENT || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, NATIVE_KILL | WIN_KILL },
	{ "sys-kill-valid-selectors", "BEGIN {if(sysfault::kill(-1,0)!=0 || sysprobe(\"kill-pid\")!=-1 || sysprobe(\"kill-signal\")!=0 || sysfault::kill(0,0)!=0 || sysprobe(\"kill-pid\")!=0 || sysfault::kill(-2,15)!=0 || sysprobe(\"kill-pid\")!=-2 || sysprobe(\"kill-signal\")!=15 || sysfault::kill(123,0)!=0 || sysprobe(\"kill-pid\")!=123 || sysprobe(\"kill-calls\")!=4) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mktime-overflow", SYS_MKTIME_INVALID("\"2147483648 1 1 0 0 0\""), HAWK_ENOERR, 0 },
	{ "sys-mktime-year-adjustment-overflow", SYS_MKTIME_INVALID("\"-2147483648 1 1 0 0 0\""), HAWK_ENOERR, 0 },
	{ "sys-mktime-month-adjustment-overflow", SYS_MKTIME_INVALID("\"2020 -2147483648 1 0 0 0\""), HAWK_ENOERR, 0 },
	{ "sys-mktime-missing-field", SYS_MKTIME_INVALID("\"2020 1 1\""), HAWK_ENOERR, 0 },
	{ "sys-mktime-invalid-field", SYS_MKTIME_INVALID("\"2020 1 bad 0 0 0\""), HAWK_ENOERR, 0 },
	{ "sys-mktime-valid", "BEGIN {t=sys::mktime(\"2020 01 02 03 04 05 -1\"); if(sys::strftime(\"%Y-%m-%d %H:%M:%S\",t)!=\"2020-01-02 03:04:05\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-native-normalization-overflow", "BEGIN {n=sys::mktime(\"2147483647 2147483647 2147483647 2147483647 2147483647 2147483647\");if(n>=0 || (index(sys::errmsg(),\"unable to convert local time\")==0 && sys::errmsg()!==\"local time is outside the supported range\")) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-native-error", "BEGIN {sysprobe(\"mktime-failure\");if(sysfault::mktime(\"2020 1 2 3 4 5\")!=sysfault::RC_EIOERR || index(sysfault::errmsg(),\"unable to convert local time\")==0) exit 1;sysprobe(\"mktime-normal\");t=sysfault::mktime(\"2020 1 2 3 4 5 -1\");if(sys::strftime(\"%Y-%m-%d %H:%M:%S\",t)!=\"2020-01-02 03:04:05\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-valid-minus-one", "BEGIN {sysprobe(\"mktime-minus-one\");if(sysfault::mktime(\"1969 12 31 23 59 59\")!==-1 || sysfault::errmsg()!=\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mktime-stale-errno", "BEGIN {sysprobe(\"mktime-minus-one\");sysprobe(\"time-stale-errno\");if(sysfault::mktime(\"1969 12 31 23 59 59\")!==-1 || sysfault::errmsg()!=\"\") exit 1;sysprobe(\"mktime-stale-errno\");if(sysfault::mktime(\"2020 1 1 0 0 0\")!==123 || sysfault::errmsg()!=\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mktime-minus-one-incidental-errno", "BEGIN {sysprobe(\"mktime-minus-one-errno\");if(sysfault::mktime(\"1969 12 31 23 59 59\")!==-1 || sysfault::errmsg()!=\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mktime-success-incidental-errno", "BEGIN {sysprobe(\"mktime-stale-errno\");if(sysfault::mktime(\"2020 1 1 0 0 0\")!==123 || sysfault::errmsg()!=\"\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-failure-no-errno", "BEGIN {sysprobe(\"mktime-no-errno\");if(sysfault::mktime(\"2040 1 1 0 0 0\")!=sysfault::RC_EINVAL || index(sysfault::errmsg(),\"outside the supported range\")==0) exit 1;sysprobe(\"mktime-stale-errno\");if(sysfault::mktime(\"2020 1 1 0 0 0\")!==123) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-no-errno-allocation", "BEGIN {sysprobe(\"mktime-no-errno\");arm();n=sysfault::mktime(\"2040 1 1 0 0 0\");if(n!=sysfault::RC_EINVAL || index(sysfault::errmsg(),\"outside the supported range\")==0) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-mktime-native-minus-one", "BEGIN {s=sys::strftime(\"%Y %m %d %H %M %S\",-1);if(s==\"\" || sys::mktime(s \" -1\")!==-1 || sys::errmsg()!=\"\") exit 1;for(n=-3;n<=-1;n++) {s=sys::strftime(\"%Y %m %d %H %M %S\",n);if(sys::mktime(s \" -1\")!==n) exit 1;} }", HAWK_ENOERR, SYS },
	{ "sys-mktime-windows-minus-one-failure", "BEGIN {sysprobe(\"mktime-minus-one\");if(sysfault::mktime(\"1969 12 31 23 59 59\")!=sysfault::RC_EINVAL || index(sysfault::errmsg(),\"outside the supported range\")==0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-mktime-windows-native-2040", "BEGIN {t=sys::mktime(\"2040 01 01 00 00 00 -1\");if(sysprobe(\"mktime-time-size\")==4) {if(t>=0 || length(sys::errmsg())==0) exit 1;}else if(t<0 || sys::strftime(\"%Y-%m-%d\",t)!=\"2040-01-01\") exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-mktime-error-allocation", "BEGIN {sysprobe(\"mktime-failure\");arm();n=sysfault::mktime(\"2020 1 1 0 0 0\");if(n!=sysfault::RC_EIOERR || index(sysfault::errmsg(),\"unable to convert local time\")==0) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-strftime-literal", "BEGIN { if(length(sys::strftime(\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",0))!=80 || sys::strftime(\"\",0)!=\"\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-strftime-format-nul-middle", SYS_STRFTIME_NUL("\"left\\0right\""), HAWK_ENOERR, 0 },
	{ "sys-strftime-format-nul-leading", SYS_STRFTIME_NUL("\"\\0right\""), HAWK_ENOERR, 0 },
	{ "sys-strftime-format-nul-trailing", SYS_STRFTIME_NUL("\"left\\0\""), HAWK_ENOERR, 0 },
	{ "sys-strftime-format-nul-only", SYS_STRFTIME_NUL("\"\\0\""), HAWK_ENOERR, 0 },
	{ "sys-strftime-format-nul-byte", SYS_STRFTIME_NUL("@b\"left\\0right\""), HAWK_ENOERR, 0 },
	{ "sys-strftime-format-nul-allocation", SYS_STRFTIME_NUL("\"left\\0right\""), HAWK_ENOERR, SWEEP },
	{ "sys-strftime-growing", "BEGIN {f=\"\";for(i=0;i<40;i++) f=f \"%Y\";if(length(sys::strftime(f,0,sys::STRFTIME_UTC))!=160) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-strftime-allocation", "BEGIN { arm(); if(length(sys::strftime(\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",0))!=80) exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-strftime-static-tm", "BEGIN { if(sysfault::strftime(\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA%Y\",0,sysfault::STRFTIME_UTC)!=\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA1970\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-strftime-conversion-utc-error", "BEGIN {sysfault::strftime(\"%Y\",1,sysfault::STRFTIME_UTC); }", HAWK_EIOERR, 0 },
	{ "sys-strftime-conversion-local-error", "BEGIN {sysfault::strftime(\"%Y\",1); }", HAWK_EIOERR, 0 },
	{ "sys-strftime-conversion-utc-no-errno", "BEGIN {sysfault::strftime(\"%Y\",2,sysfault::STRFTIME_UTC); }", HAWK_EINVAL, 0 },
	{ "sys-strftime-conversion-local-no-errno", "BEGIN {sysfault::strftime(\"%Y\",2); }", HAWK_EINVAL, 0 },
	{ "sys-strftime-conversion-utc-allocation", "BEGIN {arm();sysfault::strftime(\"%Y\",1,sysfault::STRFTIME_UTC); }", HAWK_EIOERR, SWEEP },
	{ "sys-strftime-conversion-local-allocation", "BEGIN {arm();sysfault::strftime(\"%Y\",2); }", HAWK_EINVAL, SWEEP },
	{ "sys-strftime-conversion-stale-errno", "BEGIN {sysprobe(\"time-stale-errno\");if(sysfault::strftime(\"%S\",3,sysfault::STRFTIME_UTC)!==\"03\") exit 1;sysprobe(\"time-stale-errno\");if(length(sysfault::strftime(\"%Y\",3))==0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-strftime-windows-post2038", "BEGIN {if(sys::strftime(\"%Y-%m-%d %H:%M:%S\",2147483648,sys::STRFTIME_UTC)!==\"2038-01-19 03:14:08\") exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW },
	{ "sys-strftime-windows-time64", "BEGIN {if(sys::strftime(\"%Y-%m-%d %H:%M:%S\",4294967296,sys::STRFTIME_UTC)!==\"2106-02-07 06:28:16\") exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW },
	{ "sys-strftime-windows-time64-string", "BEGIN {if(sys::strftime(\"%Y-%m-%d %H:%M:%S\",\"4294967296\",sys::STRFTIME_UTC)!==\"2106-02-07 06:28:16\") exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW },
	{ "sys-strftime-windows-time64-local", "BEGIN {if(sys::strftime(\"%Y\",4294967296)!==\"2106\") exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW },
	{ "sys-strftime-invalid-format", "BEGIN { sysfault::strftime(\"invalid-format\",0,sysfault::STRFTIME_UTC); }", HAWK_EINVAL, 0 },
	{ "sys-strftime-format-failure", "BEGIN { sysfault::strftime(\"failed-format\",0,sysfault::STRFTIME_UTC); }", HAWK_EIOERR, 0 },
	{ "sys-strftime-range-retry", "BEGIN { if(sysfault::strftime(\"range-format\",0,sysfault::STRFTIME_UTC)!=\"range-format\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-strftime-stale-errno", "BEGIN { if(sysfault::strftime(\"stale-errno\",0,sysfault::STRFTIME_UTC)!=\"stale-errno\" || sysfault::strftime(\"range-format\",0,sysfault::STRFTIME_UTC)!=\"range-format\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-strftime-invalid-format-allocation", "BEGIN { arm(); sysfault::strftime(\"invalid-format\",0,sysfault::STRFTIME_UTC); }", HAWK_EINVAL, SWEEP },
	{ "sys-socket-domain-overflow", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET+4294967296", "sysfault::SOCK_DGRAM", "0"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-type-overflow", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET", "sysfault::SOCK_DGRAM+4294967296", "0"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-protocol-overflow", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET", "sysfault::SOCK_DGRAM", "4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-domain-int-boundary", SYS_SOCKET_ARG_OVERFLOW("2147483648", "sysfault::SOCK_DGRAM", "0"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-domain-negative-overflow", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET-4294967296", "sysfault::SOCK_DGRAM", "0"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-type-negative-overflow", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET", "sysfault::SOCK_DGRAM-4294967296", "0"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-protocol-negative-overflow", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET", "sysfault::SOCK_DGRAM", "-4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-type-int-boundary", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET", "2147483648", "0"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-protocol-int-boundary", SYS_SOCKET_ARG_OVERFLOW("sysfault::AF_INET", "sysfault::SOCK_DGRAM", "2147483648"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-option-level-positive-overflow", SYS_SOCKET_OPTION_OVERFLOW("sysfault::SOL_SOCKET+4294967296", "1"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-option-level-negative-overflow", SYS_SOCKET_OPTION_OVERFLOW("sysfault::SOL_SOCKET-4294967296", "1"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-option-value-positive-overflow", SYS_SOCKET_OPTION_OVERFLOW("sysfault::SOL_SOCKET", "4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-option-value-negative-overflow", SYS_SOCKET_OPTION_OVERFLOW("sysfault::SOL_SOCKET", "-4294967295"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-option-value-int-boundary", SYS_SOCKET_OPTION_OVERFLOW("sysfault::SOL_SOCKET", "2147483648"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-option-valid-values", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysprobe(\"socket-calls\")!=1 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_REUSEADDR,1)!=0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_REUSEADDR,0)!=0 || sysprobe(\"setsockopt-calls\")!=2 || sysfault::close(s)!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-shutdown-positive-overflow", SYS_SOCKET_CONTROL_OVERFLOW("shutdown", "4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-shutdown-negative-overflow", SYS_SOCKET_CONTROL_OVERFLOW("shutdown", "-4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-listen-positive-overflow", SYS_SOCKET_CONTROL_OVERFLOW("listen", "4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-listen-negative-overflow", SYS_SOCKET_CONTROL_OVERFLOW("listen", "-4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-listen-int-boundary", SYS_SOCKET_CONTROL_OVERFLOW("listen", "2147483648"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-accept-positive-overflow", SYS_ACCEPT_OVERFLOW("4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-accept-negative-overflow", SYS_ACCEPT_OVERFLOW("-4294967296"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-accept-int-boundary", SYS_ACCEPT_OVERFLOW("2147483648"), HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-socket-control-valid-values", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0 || sysfault::shutdown(s,2)!=0 || sysprobe(\"shutdown-how\")!=2 || sysfault::shutdown(s,-1)!=0 || sysprobe(\"shutdown-how\")!=0 || sysprobe(\"shutdown-calls\")!=2 || sysfault::listen(s,2147483647)!=0 || sysprobe(\"listen-backlog\")!=2147483647 || sysfault::listen(s,-1)!=0 || sysprobe(\"listen-backlog\")!=0 || sysprobe(\"listen-calls\")!=2) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-accept-negative-default", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;a=sysfault::accept(s,-1);if(a<0 || sysprobe(\"accept-calls\")!=1 || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-open-flags-positive-overflow", SYS_OPEN_FLAGS_OVERFLOW("sys::O_RDWR+sys::O_TRUNC+4294967296"), HAWK_ENOERR, INT32_NARROW },
	{ "sys-open-flags-negative-overflow", SYS_OPEN_FLAGS_OVERFLOW("-4294967296"), HAWK_ENOERR, INT32_NARROW },
	{ "sys-open-flags-int-boundary", SYS_OPEN_FLAGS_OVERFLOW("2147483648"), HAWK_ENOERR, INT32_NARROW },
	{ "sys-open-flags-negative-default", "BEGIN {p=sys::tempname(\"hawk-flags-\");if(p==\"\") exit 1;f=sys::open(p,-1);ok=f>=0;if(f>=0) sys::close(f);if(sys::unlink(p)!=0 || !ok) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-file-binary-roundtrip", "BEGIN {p=sys::tempname(\"hawk-bytes-\");if(p==\"\") exit 1;v=@b\"A\\nB\\r\\nC\\x1aD\\0E\";f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;}ok=sys::write(f,v)==length(v) && sys::fseek(f,0,sys::SEEK_END)==length(v) && sys::fseek(f,0,sys::SEEK_SET)==0 && sys::read(f,x,8192)==length(v) && x===v;sys::close(f);if(sys::unlink(p)!=0 || !ok) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-file-binary-buffered-seek", "BEGIN {p=sys::tempname(\"hawk-bytes-\");if(p==\"\") exit 1;f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;}ok=sys::write(f,@b\"A\\nLEFT\\n\")==7 && sys::fseek(f,0,sys::SEEK_SET)==0 && sys::read(f,x,8192,\"\\n\")==2 && x===@b\"A\\n\" && sys::fseek(f,0,sys::SEEK_CUR)==2 && sys::read(f,x,8192,\"\\n\")==5 && x===@b\"LEFT\\n\" && sys::fseek(f,0,sys::SEEK_CUR)==7;sys::close(f);if(sys::unlink(p)!=0 || !ok) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-file-binary-reopen-append", "BEGIN {p=sys::tempname(\"hawk-bytes-\");if(p==\"\") exit 1;f=sys::open(p,sys::O_WRONLY);if(f<0) {sys::unlink(p);exit 1;}ok=sys::write(f,@b\"A\\n\")==2;sys::close(f);f=sys::open(p,sys::O_WRONLY|sys::O_APPEND);if(f>=0) {ok=ok && sys::write(f,@b\"\\x1aB\\n\")==3;sys::close(f);}else ok=0;f=sys::open(p,sys::O_RDONLY);if(f>=0) {ok=ok && sys::read(f,x,8192)==5 && x===@b\"A\\n\\x1aB\\n\";sys::close(f);}else ok=0;if(sys::unlink(p)!=0 || !ok) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-dup-flags-positive-overflow", SYS_DUP_FLAGS_OVERFLOW("4294967296"), HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-dup-flags-negative-overflow", SYS_DUP_FLAGS_OVERFLOW("-4294967296"), HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-dup-flags-int-boundary", SYS_DUP_FLAGS_OVERFLOW("2147483648"), HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-dup-invalid-function-flags", SYS_DUP_INVALID_FLAGS("sysfault", "f"), HAWK_ENOERR, SYS },
	{ "sys-dup-invalid-bob-flags", SYS_DUP_INVALID_FLAGS("sysfault", "bob()"), HAWK_ENOERR, SYS },
	{ "sys-dup-fallback-invalid-function-flags", SYS_DUP_INVALID_FLAGS("sysfallback", "f"), HAWK_ENOERR, SYS },
	{ "sys-dup-invalid-function-flags-mux", "function f() {} BEGIN {expected=sysfault::openfd(f);message=sysfault::errmsg();m=sysfault::openmux();if(expected>=0 || m<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,c,sysfault::MUX_EVT_IN)!=0 || sysfault::write(d,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1) exit 1;if(sysfault::dup(a,c,f)!=expected || sysfault::errmsg()!==message || sysprobe(\"dup-calls\")!=0 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=c || sysfault::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-dup-file-invalid-function-flags", "function f() {} BEGIN {p=sys::tempname(\"hawk-dup-\");q=sys::tempname(\"hawk-dup-\");if(p==\"\" || q==\"\") exit 1;a=sys::open(p,sys::O_RDWR);b=sys::open(q,sys::O_RDWR);if(a<0 || b<0 || sys::write(b,@b\"abc\")!=3 || sys::fseek(b,0,sys::SEEK_SET)!=0) {sys::unlink(p);sys::unlink(q);exit 1;}expected=sys::openfd(f);message=sys::errmsg();n=sys::dup(a,b,f);ok=expected<0 && length(message)>0 && n==expected && sys::errmsg()===message;r=sys::read(b,x,10);ok=ok && r==3 && x===@b\"abc\";sys::close(a);sys::close(b);if(sys::unlink(p)!=0 || sys::unlink(q)!=0 || !ok) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-dup-file-positive-overflow", SYS_DUP_FILE_OVERFLOW("4294967296"), HAWK_ENOERR, INT32_NARROW },
	{ "sys-dup-file-negative-overflow", SYS_DUP_FILE_OVERFLOW("-4294967296"), HAWK_ENOERR, INT32_NARROW },
	{ "sys-dup-file-int-boundary", SYS_DUP_FILE_OVERFLOW("2147483648"), HAWK_ENOERR, INT32_NARROW },
	{ "sys-dup-negative-default", "BEGIN {if(sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::dup(a,c,-1)!=c || sysprobe(\"dup-calls\")!=1 || sysfault::write(b,@b\"x\")!=1 || sysfault::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-dup-overflow-preserves-mux", "BEGIN {m=sysfault::openmux();if(m<0 || sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::dup(c,a,4294967296)!=sysfault::RC_EINVAL || sysprobe(\"dup-calls\")!=0 || sysfault::write(b,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1; }", HAWK_ENOERR, SYS | EPOLL | INT32_NARROW },
	{ "sys-fchown-uid-positive-overflow", SYS_FCHOWN_OVERFLOW("4294967296", "-1"), HAWK_ENOERR, SYS | OWNER32_NARROW },
	{ "sys-fchown-gid-positive-overflow", SYS_FCHOWN_OVERFLOW("-1", "4294967296"), HAWK_ENOERR, SYS | OWNER32_NARROW },
	{ "sys-fchown-uid-negative-overflow", SYS_FCHOWN_OVERFLOW("-4294967296", "-1"), HAWK_ENOERR, SYS | OWNER32_NARROW },
	{ "sys-fchown-gid-negative-overflow", SYS_FCHOWN_OVERFLOW("-1", "-4294967296"), HAWK_ENOERR, SYS | OWNER32_NARROW },
	{ "sys-fchown-valid-selectors", "BEGIN {if(sysfault::pipe(a,b)!=0 || sysfault::fchown(a,-1,-1)!=0 || sysprobe(\"owner-uid\")!=4294967295 || sysprobe(\"owner-gid\")!=4294967295 || sysfault::fchown(a,2147483648,4294967294)!=0 || sysprobe(\"owner-uid\")!=2147483648 || sysprobe(\"owner-gid\")!=4294967294 || sysprobe(\"fchown-calls\")!=2) exit 1; }", HAWK_ENOERR, SYS | OWNER32_NARROW },
	{ "sys-pipe-flags-overflow", "BEGIN {a=41;b=42;if(sys::pipe(a,b,4294967296)!=sys::RC_EINVAL || a!=41 || b!=42 || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-pipe-fallback-flags-overflow", "BEGIN {a=41;b=42;if(sysfault::pipe(a,b,4294967296)!=sysfault::RC_EINVAL || a!=41 || b!=42 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-pipe-flags-negative-overflow", "BEGIN {a=41;b=42;if(sys::pipe(a,b,-4294967296)!=sys::RC_EINVAL || a!=41 || b!=42 || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-pipe-fallback-flags-negative-overflow", "BEGIN {a=41;b=42;if(sysfault::pipe(a,b,-4294967296)!=sysfault::RC_EINVAL || a!=41 || b!=42 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
#if defined(HAWK_OOCH_IS_UCH)
	{ "sys-open-errno-after-path-free", "BEGIN {p=sys::tempname(\"hawk-errno-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;clobber_free_errno(1);n=sys::open(p,sys::O_RDONLY);clobber_free_errno(0);if(n!=sys::RC_ENOENT || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, 0 },
#endif
	{ "sys-open-byte-path-errno-control", "BEGIN {p=sys::tempname(\"hawk-errno-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;p=str::tombs(p);clobber_free_errno(1);n=sys::open(p,sys::O_RDONLY);clobber_free_errno(0);if(n!=sys::RC_ENOENT || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-open-unicode-redirection", "BEGIN {p=sys::tempname(\"hawk-unicode-\");if(p==\"\") exit 1;q=p \"-\\u00E9-\\uAC00\";printf \"payload\" >q;close(q);f=sys::open(q,sys::O_RDONLY);ok=f>=0;if(f>=0) {ok=sys::read(f,x,7)==7 && x===@b\"payload\";sys::close(f);}u=sys::unlink(q);v=sys::unlink(p);if(!ok || u!=0 || v!=0) exit 1; }", HAWK_ENOERR, WIDE_CHAR },
	{ "sys-open-unicode-byte-path", "BEGIN {p=sys::tempname(\"hawk-unicode-\");if(p==\"\") exit 1;q=p \"-\\u00E9-\\uAC00\";printf \"payload\" >q;close(q);f=sys::open(str::tombs(q),sys::O_RDONLY);ok=f>=0;if(f>=0) {ok=sys::read(f,x,7)==7 && x===@b\"payload\";sys::close(f);}u=sys::unlink(q);v=sys::unlink(p);if(!ok || u!=0 || v!=0) exit 1; }", HAWK_ENOERR, WIDE_CHAR },
	{ "sys-open-unicode-create", "BEGIN {p=sys::tempname(\"hawk-unicode-\");if(p==\"\") exit 1;q=p \"-\\u00E9-\\uAC00\";f=sys::open(q,sys::O_RDWR|sys::O_CREAT|sys::O_EXCL,0600);if(f<0) {sys::unlink(p);exit 1;}ok=sys::write(f,@b\"payload\")==7;sys::close(f);n=(getline x <q);close(q);u=sys::unlink(q);v=sys::unlink(p);if(!ok || n!=1 || x!==\"payload\" || u!=0 || v!=0) exit 1; }", HAWK_ENOERR, WIDE_CHAR },
	{ "sys-tempname-unicode-prefix", "BEGIN {prefix=\"hawk-\\u00E9-\\uAC00-\";p=sys::tempname(prefix);if(p==\"\") exit 1;n=(getline x <p);close(p);f=sys::open(p,sys::O_RDONLY);ok=f>=0;if(f>=0) {ok=sys::read(f,x,1)==0;sys::close(f);}u=sys::unlink(p);if(index(sys::basename(p),prefix)!=1 || n!=0 || !ok || u!=0) exit 1; }", HAWK_ENOERR, WIDE_CHAR },
	{ "sys-tempname-unicode-byte-prefix", "BEGIN {prefix=\"hawk-\\u00E9-\\uAC00-\";p=sys::tempname(str::tombs(prefix));if(p==\"\") exit 1;n=(getline x <p);close(p);f=sys::open(p,sys::O_RDONLY);ok=f>=0;if(f>=0) sys::close(f);u=sys::unlink(p);if(index(sys::basename(p),prefix)!=1 || n!=0 || !ok || u!=0) exit 1; }", HAWK_ENOERR, WIDE_CHAR },
	{ "sys-tempname-nul-prefix", "BEGIN {if(sys::tempname(\"hawk\\0hidden\")!==\"\" || length(sys::errmsg())==0 || sys::tempname(@b\"hawk\\0hidden\")!==\"\" || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, 0 },
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#if defined(HAWK_OOCH_IS_UCH)
	{ "sys-stat-errno-after-path-free", "BEGIN {p=sys::tempname(\"hawk-stat-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;s=\"old\";clobber_free_errno(1);n=sys::stat(p,s);clobber_free_errno(0);if(n!=sys::RC_ENOENT || length(sys::errmsg())==0 || s!==\"old\") exit 1; }", HAWK_ENOERR, 0 },
#endif
	{ "sys-stat-byte-path-errno-control", "BEGIN {p=sys::tempname(\"hawk-stat-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;p=str::tombs(p);s=\"old\";clobber_free_errno(1);n=sys::stat(p,s);clobber_free_errno(0);if(n!=sys::RC_ENOENT || length(sys::errmsg())==0 || s!==\"old\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-stat-success-after-path-free", "BEGIN {p=sys::tempname(\"hawk-stat-\");if(p==\"\") exit 1;f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;}if(sys::write(f,@b\"abc\")!=3 || sys::close(f)!=0) {sys::unlink(p);exit 1;}clobber_free_errno(1);n=sys::stat(p,s);clobber_free_errno(0);if(sys::unlink(p)!=0 || n!=0 || s[\"size\"]!=3) exit 1; }", HAWK_ENOERR, 0 },
#endif
	{ "sys-basename-basic", "BEGIN {if(sys::basename(\"\")!==\"\" || sys::basename(\"abc\")!==\"abc\" || sys::basename(\"dir/abc\")!==\"abc\" || sys::basename(\"dir/\")!==\"\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-basename-nul-unindexed", "BEGIN {p=\"a\\0b\";if(sys::basename(p)!==p || length(sys::basename(p))!=3 || p!==\"a\\0b\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-basename-nul-path", "BEGIN {p=\"dir/a\\0b\";if(sys::basename(p)!==\"a\\0b\" || length(sys::basename(p))!=3 || p!==\"dir/a\\0b\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-basename-nul-only", "BEGIN {if(sys::basename(\"dir/\\0\")!==\"\\0\" || length(sys::basename(\"dir/\\0\"))!=1) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-basename-nul-byte-input", "BEGIN {p=@b\"dir/a\\0b\";if(sys::basename(p)!==\"a\\0b\" || p!==@b\"dir/a\\0b\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-basename-nul-allocation", "BEGIN {p=@b\"dir/a\\0b\";arm();if(sys::basename(p)!==\"a\\0b\" || p!==@b\"dir/a\\0b\") exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-unlink-reserved-tempfile", "BEGIN {p=sys::tempname(\"hawk-unlink-\");if(p==\"\") exit 1;if(sys::unlink(p)!=0 || sys::unlink(p)!=sys::RC_ENOENT || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, 0 },
#if defined(_WIN32)
	{ "sys-strftime-windows-unknown-format", "BEGIN { sys::strftime(\"%Q\",0,sys::STRFTIME_UTC); }", HAWK_EINVAL, 0 },
	{ "sys-strftime-windows-incomplete-format", "BEGIN { sys::strftime(\"%\",0,sys::STRFTIME_UTC); }", HAWK_EINVAL, 0 },
	{ "sys-strftime-windows-incomplete-modifier", "BEGIN { sys::strftime(\"%#\",0,sys::STRFTIME_UTC); }", HAWK_EINVAL, 0 },
#endif
	{ "sys-time-failure", "BEGIN {ns=999;if(sysfault::gettime(ns)!=sysfault::RC_ESYSERR || ns!=999 || sysfault::errmsg()!==\"unable to get current time\" || sysfault::mktime()!=sysfault::RC_ESYSERR || sysfault::errmsg()!==\"unable to get current time\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-gettime-no-output-failure", "BEGIN {if(sysfault::gettime()!=sysfault::RC_ESYSERR || sysfault::errmsg()!==\"unable to get current time\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-systime-failure", "BEGIN {ns=777;if(sysfault::systime(ns)!=sysfault::RC_ESYSERR || ns!=777 || sysfault::errmsg()!==\"unable to get current time\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-gettime-recovery", "BEGIN {ns=999;if(sysfault::gettime(ns)!=sysfault::RC_ESYSERR || ns!=999) exit 1;sysprobe(\"log-mock\");sysprobe(\"log-time-stale-errno\");if(sysfault::gettime(ns)!==3 || ns!==0 || sysfault::gettime()!==3 || sysfault::systime(ns)!==3 || ns!==0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-gettime-output-type-error", SYS_TIME_OUTPUT_TYPE_ERROR("gettime"), HAWK_ENOERR, STRICTMAP },
	{ "sys-systime-output-type-error", SYS_TIME_OUTPUT_TYPE_ERROR("systime"), HAWK_ENOERR, STRICTMAP },
	{ "sys-gettime-output-allocation", SYS_TIME_OUTPUT_ALLOCATION("gettime"), HAWK_ENOERR, SWEEP },
	{ "sys-systime-output-allocation", SYS_TIME_OUTPUT_ALLOCATION("systime"), HAWK_ENOERR, SWEEP },
	{ "sys-mktime-clock-recovery", "BEGIN {if(sysfault::mktime()!=sysfault::RC_ESYSERR || sysfault::errmsg()!==\"unable to get current time\") exit 1;sysprobe(\"log-mock\");sysprobe(\"log-time-stale-errno\");if(sysfault::mktime()!==3 || sysfault::errmsg()!==\"\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-input-type-error", "BEGIN {a[1]=7;if(sysfault::mktime(a)!=-117 || a[1]!==7 || length(sysfault::errmsg())==0) exit 1;sysprobe(\"mktime-stale-errno\");if(sysfault::mktime(\"2020 1 1 0 0 0\")!==123 || sysfault::errmsg()!==\"\") exit 1; }", HAWK_ENOERR, STRICTMAP },
	{ "sys-mktime-input-conversion-allocation", "BEGIN {sysprobe(\"mktime-stale-errno\");year=2020;arm();n=sysfault::mktime(year);if((n!=sysfault::RC_EINVAL && n!=sysfault::RC_ENOMEM) || year!==2020 || length(sysfault::errmsg())==0) exit 1;if(sysfault::mktime(\"2020 1 1 0 0 0\")!==123 || sysfault::errmsg()!==\"\") exit 1; }", HAWK_ENOERR, SWEEP },
	{ "sys-mktime-positive-clears-error", "BEGIN {sysprobe(\"mktime-failure\");if(sysfault::mktime(\"2020 1 1 0 0 0\")!=sysfault::RC_EIOERR || sysfault::errmsg()==\"\") exit 1;sysprobe(\"mktime-stale-errno\");if(sysfault::mktime(\"2020 1 1 0 0 0\")!==123 || sysfault::errmsg()!==\"\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-mktime-negative-clears-error", "BEGIN {if(sysfault::mktime(\"invalid\")!=sysfault::RC_EINVAL || sysfault::errmsg()==\"\") exit 1;sysprobe(\"mktime-minus-one-errno\");if(sysfault::mktime(\"1969 12 31 23 59 59\")!==-1 || sysfault::errmsg()!==\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-wait-pid-positive-overflow", "BEGIN {s=999;if(sysfault::wait(4294967297,s,sysfault::WNOHANG)!=sysfault::RC_EINVAL || s!=999 || sysprobe(\"wait-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | PID32_NARROW },
	{ "sys-wait-pid-negative-overflow", "BEGIN {s=999;if(sysfault::wait(-4294967295,s,sysfault::WNOHANG)!=sysfault::RC_EINVAL || s!=999 || sysprobe(\"wait-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | PID32_NARROW },
	{ "sys-wait-options-overflow", "BEGIN {s=999;if(sysfault::wait(123,s,4294967296)!=sysfault::RC_EINVAL || s!=999 || sysprobe(\"wait-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-wait-valid-selectors", "BEGIN {s=999;for(i=-2;i<=1;i++) if(sysfault::wait(i,s,sysfault::WNOHANG)!=0 || sysprobe(\"wait-pid\")!=i || sysprobe(\"wait-options\")!=sysfault::WNOHANG || s!=999) exit 1;if(sysprobe(\"wait-calls\")!=4) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-fcntl-value-overflow", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(sysfault::fcntl(f,sysfault::F_SETFL,sysfault::O_NONBLOCK)!=0) exit 1;v=sysfault::fcntl(f,sysfault::F_GETFL);if(f<0 || sysfault::fcntl(f,sysfault::F_SETFL,4294967296)!=sysfault::RC_EINVAL || sysfault::fcntl(f,sysfault::F_GETFL)!=v || sysfault::fcntl(f,sysfault::F_SETFD,-4294967296)!=sysfault::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-flock-whence-overflow", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);pid=999;if(f<0 || sysfault::flock(f,sysfault::FLOCK_READ|sysfault::FLOCK_GET,0,0,65536,pid)!=sysfault::RC_EINVAL || pid!=999 || sysprobe(\"flock-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-flock-type-overflow", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::flock(f,sysfault::FLOCK_WRITE+65536,0,0,sysfault::SEEK_SET)!=sysfault::RC_EINVAL || sysprobe(\"flock-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-windows-socket-close", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::close(s)!=0 || sysprobe(\"socket-closed\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-teardown", "BEGIN {if(sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0)<0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-allocation", "BEGIN {arm();s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 && s!=sysfault::RC_ENOMEM) exit 1; }", HAWK_ENOERR, WIN_SYS | SWEEP },
	{ "sys-windows-socket-io", "BEGIN {sysprobe(\"socket-mock-io\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::write(s,@b\"ABC\")!=3 || sysfault::read(s,x,3)!=3 || x!==@b\"ABC\" || sysprobe(\"socket-io\")!=2) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-wide-handle", "BEGIN {sysprobe(\"socket-mock-io\");sysprobe(\"socket-high\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::write(s,@b\"ABC\")!=3 || sysfault::read(s,x,3)!=3 || x!==@b\"ABC\" || sysfault::close(s)!=0 || sysprobe(\"socket-closed\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-dup", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);d=sysfault::dup(s);if(s<0 || d<0 || d==s || sysfault::close(s)!=0 || sysfault::setsockopt(d,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,0.5)!=0 || sysfault::close(d)!=0 || sysprobe(\"socket-closed\")!=2) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-accept-allocation", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;arm();a=sysfault::accept(s,0,peer);if(a<0 && a!=sysfault::RC_ENOMEM) exit 1; }", HAWK_ENOERR, WIN_SYS | SWEEP },
	{ "sys-windows-sleep-infinite", "BEGIN {if(sysfault::sleep(4294967.2955)!=sysfault::RC_EINVAL || sysprobe(\"sleep-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-sleep-finite-limit", "BEGIN {if(sysfault::sleep(4294967.2945)!=0 || sysprobe(\"sleep-calls\")!=1 || sysprobe(\"sleep-ms\")!=4294967294) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-timeout", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,2)!=0 || sysprobe(\"socket-timeout\")!=2000 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,0.5)!=0 || sysprobe(\"socket-timeout\")!=500 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,4294967.296)!=sysfault::RC_EINVAL || sysprobe(\"socket-timeout\")!=500 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,0)!=0 || sysprobe(\"socket-timeout\")!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-crt-errno", "BEGIN {p=sys::tempname(\"hawk-errno-\");if(p==\"\") exit 1;n=sysfault::open(p,sysfault::O_WRONLY|sysfault::O_CREAT|sysfault::O_EXCL);sys::unlink(p);if(n!=sysfault::RC_EEXIST || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-errno", "BEGIN {sysprobe(\"socket-failure\");if(sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-io-errno", "BEGIN {sysprobe(\"socket-mock-io\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0) exit 1;sysprobe(\"socket-failure\");x=@b\"old\";if(sysfault::read(s,x,3)!=sysfault::RC_EAGAIN || x!==@b\"old\" || sysfault::write(s,@b\"ABC\")!=sysfault::RC_EAGAIN || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-flock-type-wide-overflow", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::flock(f,sysfault::FLOCK_WRITE+4294967296,0,0,sysfault::SEEK_SET)!=sysfault::RC_EINVAL || sysprobe(\"flock-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-windows-socket-dup-allocation", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0) exit 1;arm();d=sysfault::dup(s);if(d<0 && d!=sysfault::RC_ENOMEM) exit 1; }", HAWK_ENOERR, WIN_SYS | SWEEP },
	{ "sys-windows-socket-dup-target", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::dup(s,s)!=sysfault::RC_ENOSUP || sysfault::close(s)!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-socket-loopback", "BEGIN {a=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);b=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(a<0 || b<0 || sysfault::bind(b,\"127.0.0.1:0\")!=0 || sysfault::setsockopt(b,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,1)!=0) exit 1;addr=sysprobe(\"socket-address\");if(sysfault::connect(a,addr)!=0 || sysfault::write(a,@b\"ABC\")!=3 || sysfault::read(b,x,3)!=3 || x!==@b\"ABC\" || sysfault::sendto(a,@b\"DEF\",addr)!=3 || sysfault::recvfrom(b,x,3)!=3 || x!==@b\"DEF\") exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-recvfrom-error-discards-residue", "BEGIN {" SYS_WIN_RECV_RESIDUE "y=@b\"old\";peer=\"old\";if(sysfault::sendto(a,@b\"XYZUVW\",addr)!=6 || sysfault::recvfrom(b,y,2,peer)>=0 || y!==@b\"old\" || peer!==\"old\" || length(sysfault::errmsg())==0) exit 1;if(sysfault::sendto(a,@b\"FRESH\",addr)!=5 || sysfault::read(b,z,100)!=5 || z!==@b\"FRESH\" || sysfault::close(a)!=0 || sysfault::close(b)!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-recvfrom-invalid-limit-keeps-residue", "function bad() {} BEGIN {" SYS_WIN_RECV_RESIDUE "y=@b\"old\";peer=\"old\";if(sysfault::recvfrom(b,y,bad,peer)>=0 || y!==@b\"old\" || peer!==\"old\" || length(sysfault::errmsg())==0 || sysfault::read(b,z,100)!=3 || z!==@b\"OLD\" || sysfault::close(a)!=0 || sysfault::close(b)!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-flock-start-positive-overflow", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::flock(f,sysfault::FLOCK_WRITE,4294967296,0,sysfault::SEEK_SET)!=sysfault::RC_EINVAL || sysprobe(\"flock-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | OFFSET32_NARROW },
	{ "sys-flock-length-negative-overflow", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::flock(f,sysfault::FLOCK_WRITE,0,-4294967296,sysfault::SEEK_SET)!=sysfault::RC_EINVAL || sysprobe(\"flock-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | OFFSET32_NARROW },
	{ "sys-flock-valid-origins", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::flock(f,sysfault::FLOCK_READ|sysfault::FLOCK_WAIT,0,0,sysfault::SEEK_SET)!=0 || sysfault::flock(f,sysfault::FLOCK_WRITE,1,-1,sysfault::SEEK_CUR)!=0 || sysfault::flock(f,sysfault::FLOCK_UNLOCK,-1,1,sysfault::SEEK_END)!=0 || sysprobe(\"flock-calls\")!=3) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-windows-crt-missing-file", "BEGIN {p=sys::tempname(\"hawk-errno-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;if(sysfault::open(p,sysfault::O_RDONLY)!=sysfault::RC_ENOENT || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-wait-no-status", "BEGIN {s=999;if(sysfault::wait(123,s,sysfault::WNOHANG)!=0 || s!=999) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-socket-timeout", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,0.5)!=0 || sysprobe(\"timeout\")!=1 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,1e100)!=sysfault::RC_EINVAL || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,-0.1)!=sysfault::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-socket-receive-timeout-submicrosecond", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_RCVTIMEO", "0.0000005"), HAWK_ENOERR, SYS },
	{ "sys-socket-send-timeout-submicrosecond", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_SNDTIMEO", "0.0000005"), HAWK_ENOERR, SYS },
	{ "sys-windows-receive-timeout-submillisecond", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_RCVTIMEO", "0.0005"), HAWK_ENOERR, WIN_SYS },
	{ "sys-windows-send-timeout-submillisecond", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_SNDTIMEO", "0.0005"), HAWK_ENOERR, WIN_SYS },
	{ "sys-socket-receive-timeout-subnanosecond", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_RCVTIMEO", "1e-12"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-send-timeout-subnanosecond-string", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_SNDTIMEO", "\"0.000000000001\""), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-receive-timeout-negative-subnanosecond", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_RCVTIMEO", "-1e-10"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-send-timeout-negative-subnanosecond-string", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_SNDTIMEO", "\"-0.0000000001\""), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-receive-timeout-negative-tiny", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_RCVTIMEO", "-1e-100"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-send-timeout-negative-tiny-string", SYS_SOCKET_TIMEOUT_TOO_SMALL("SO_SNDTIMEO", "\"-1e-100\""), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-socket-timeout-resolution-boundary", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0) exit 1;for(i=0;i<2;i++){opt=i?sysfault::SO_SNDTIMEO:sysfault::SO_RCVTIMEO;if(sysfault::setsockopt(s,sysfault::SOL_SOCKET,opt,0.000001)!=0 || sysprobe(\"timeout-sec\")!=0 || sysprobe(\"timeout-usec\")!=1 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,opt,0)!=0 || sysprobe(\"timeout-sec\")!=0 || sysprobe(\"timeout-usec\")!=0) exit 1;} }", HAWK_ENOERR, SYS },
	{ "sys-windows-timeout-resolution-boundary", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,0.001)!=0 || sysprobe(\"socket-timeout\")!=1 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,0)!=0 || sysprobe(\"socket-timeout\")!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-socket-invalid-option", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,999999,1)!=sysfault::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-accept-allocation", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;arm();a=sysfault::accept(s,0,peer);if(a>=0) sysfault::close(a);sysfault::close(s); }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-log-fcntl-failure", "BEGIN {sysprobe(\"fail-fcntl\");sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER);if(sysprobe(\"log-closed\")!=1) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-log-priority-negative", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(-1,\"hello\")!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysprobe(\"socket-calls\")!=0 || sysprobe(\"log-send-calls\")!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || sysprobe(\"log-send-calls\")!=1) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-priority-invalid-facility", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(192,\"hello\")!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysprobe(\"socket-calls\")!=0 || sysprobe(\"log-send-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-invalid-facility-preserves-log", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/old\",0,sysfault::LOG_FAC_USER)!=0) exit 1;bad[1]=-1;bad[2]=1;bad[3]=sysfault::LOG_FAC_USER|sysfault::LOG_PRI_INFO;bad[4]=192;for(i=1;i<=4;i++) if(sysfault::openlog(\"remote://127.0.0.1:9/new\",sysfault::LOG_OPT_NDELAY,bad[i])!=sysfault::RC_EINVAL || length(sysfault::errmsg())==0 || sysprobe(\"socket-calls\")!=0) exit 1;if(sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || index(sysprobe(\"log-message\"),@b\"<14>\")!=1 || index(sysprobe(\"log-message\"),@b\"old: retry\")==0 || sysprobe(\"log-send-calls\")!=1) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-valid-facilities-and-priorities", "BEGIN {sysprobe(\"log-mock\");for(f=0;f<24;f++){if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,f*8)!=0) exit 1;for(p=0;p<8;p++){if(sysfault::writelog(p,\"hello\")!=0 || index(sysprobe(\"log-message\"),str::tombs(\"<\" (f*8+p) \">\"))!=1) exit 1;}}if(sysprobe(\"log-send-calls\")!=192) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-priority-facility-override", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_FAC_LOCAL1|sysfault::LOG_PRI_ERR,\"hello\")!=0 || index(sysprobe(\"log-message\"),@b\"<139>\")!=1) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-remote-delayed", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_LOCAL0)!=0 || sysprobe(\"socket-calls\")!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"first\")!=0 || sysprobe(\"socket-calls\")!=1 || sysprobe(\"log-send-calls\")!=1) exit 1;x=sysprobe(\"log-message\");if(index(x,@b\"<134>\")!=1 || index(x,@b\"test: first\")==0 || sysfault::writelog(sysfault::LOG_PRI_WARNING,\"second\")!=0 || sysprobe(\"log-send-calls\")!=2) exit 1;x=sysprobe(\"log-message\");if(index(x,@b\"<132>\")!=1 || index(x,@b\"test: second\")==0 || index(x,@b\"first\")!=0 || sysfault::closelog()!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-remote-eager-pid", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"sock://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY|sysfault::LOG_OPT_PID,sysfault::LOG_FAC_USER)!=0 || sysprobe(\"socket-calls\")!=1 || sysfault::writelog(sysfault::LOG_PRI_DEBUG,\"hello\")!=0) exit 1;x=sysprobe(\"log-message\");tag=str::tombs(\"test[\" sysfault::getpid() \"]: hello\");if(index(x,@b\"<15>\")!=1 || index(x,tag)==0 || sysfault::closelog()!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-remote-reopen", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/old\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"old-message\")!=0 || sysfault::openlog(\"remote://127.0.0.1:9/new\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_LOCAL1)!=0 || sysfault::writelog(sysfault::LOG_PRI_ERR,\"new-message\")!=0) exit 1;x=sysprobe(\"log-message\");if(index(x,@b\"<139>\")!=1 || index(x,@b\"new: new-message\")==0 || index(x,@b\"old-message\")!=0 || sysprobe(\"socket-calls\")!=2 || sysfault::closelog()!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-remote-send-error", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-send-failure\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_EAGAIN || length(sysfault::errmsg())==0 || sysprobe(\"log-send-calls\")!=1 || sysfault::closelog()!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-time-errno", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-time-errno\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_EIOERR || index(sysfault::errmsg(),\"unable to get local time\")==0 || sysprobe(\"log-send-calls\")!=0) exit 1;sysprobe(\"log-time-normal\");if(sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || sysprobe(\"log-send-calls\")!=1) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-time-no-errno", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-time-no-errno\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_EINVAL || index(sysfault::errmsg(),\"unable to get local time\")==0 || index(sysfault::errmsg(),\"Success\")!=0 || sysprobe(\"log-send-calls\")!=0) exit 1;sysprobe(\"log-time-normal\");if(sysfault::writelog(sysfault::LOG_PRI_INFO,\"retry\")!=0 || sysprobe(\"log-send-calls\")!=1) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-time-stale-errno", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-time-stale-errno\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=0 || sysprobe(\"log-send-calls\")!=1) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-log-time-error-allocation", "BEGIN {sysprobe(\"log-mock\");sysprobe(\"log-time-no-errno\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0) exit 1;arm();n=sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\");if((n!=sysfault::RC_EINVAL && n!=sysfault::RC_ENOMEM) || sysprobe(\"log-send-calls\")!=0) exit 1;sysfault::closelog();}", HAWK_ENOERR, SOCKET_TEST | SWEEP },
	{ "sys-system-windows-large-status", "BEGIN {if(sys::system(\"exit /b 1000000000\")!=1000000000) exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-pio-wait-contract", "BEGIN {if(piowait()!=0) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-system-normal-status", "BEGIN {if(sys::system(\"exit 0\")!=0 || sys::system(\"exit 23\")!=23 || sys::system(\"exit 255\")!=255) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-system-signal-status", "BEGIN {if(sys::system(\"kill -TERM $$\")!=256+sys::SIGTERM) exit 1;}", HAWK_ENOERR, SYS },
	{ "sys-system-max-status", "BEGIN {sysprobe(\"system-max-status\");if(sysfault::system(\"exit 0\")!=maxint()) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-system-result-allocation-failure", "BEGIN {sysprobe(\"system-result-oom\");sysfault::system(\"exit 0\");exit 1;}", HAWK_ENOMEM, SOCKET_TEST },
	{ "sys-system-wait-error", "BEGIN {sysprobe(\"system-wait-error\");if(sysfault::system(\"exit 0\")!=sysfault::RC_EIOERR || length(sysfault::errmsg())==0) exit 1;}", HAWK_ENOERR, SOCKET_TEST },
	{ "sys-system-windows-high-bit", "BEGIN {n=sys::system(\"exit /b -2147483648\");if(intsize()<=4) {if(n!=sys::RC_EINVAL || index(sys::errmsg(),\"exit status out of range\")==0) exit 1;} else if(n!=2147483648) exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-system-windows-max-status", "BEGIN {n=sys::system(\"exit /b -1\");if(intsize()<=4) {if(n!=sys::RC_EINVAL || index(sys::errmsg(),\"exit status out of range\")==0) exit 1;} else if(n!=4294967295) exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-system-windows-stale-error", "BEGIN {if(sys::system(\"exit /b 7\")!=7 || sys::openfd(-1)>=0) exit 1;n=sys::system(\"exit /b -1\");if(intsize()<=4) {if(n!=sys::RC_EINVAL || index(sys::errmsg(),\"exit status out of range\")==0) exit 1;} else if(n!=4294967295) exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-system-windows-reserved-status", "BEGIN {if(sys::system(\"exit /b 256\")!=256 || sys::system(\"exit /b 259\")!=259) exit 1;}", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-remote-allocation", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0) exit 1;arm();n=sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\");if(n<0 && n!=sysfault::RC_ENOMEM) exit 1;if(n==0 && sysprobe(\"log-send-calls\")!=1) exit 1;sysfault::closelog(); }", HAWK_ENOERR, SOCKET_TEST | SWEEP },
	{ "sys-log-remote-nul-message", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"a\\0b\")!=sysfault::RC_EINVAL || sysprobe(\"socket-calls\")!=0 || sysprobe(\"log-send-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
#if defined(HAWK_OOCH_IS_UCH)
	{ "sys-log-remote-utf8", "BEGIN {sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/caf\\xE9\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"caf\\xE9\")!=0 || index(sysprobe(\"log-message\"),@b\"caf\\xC3\\xA9: caf\\xC3\\xA9\")==0) exit 1; }", HAWK_ENOERR, SOCKET_TEST },
#endif
	{ "sys-log-options-overflow", "BEGIN {if(sysfault::openlog(\"remote://127.0.0.1:9/test\",4294967296+sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=sysfault::RC_EINVAL || sysprobe(\"socket-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-log-facility-overflow", "BEGIN {if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY,4294967296+sysfault::LOG_FAC_USER)!=sysfault::RC_EINVAL || sysprobe(\"socket-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-log-priority-overflow", "BEGIN {if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(4294967296+sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_EINVAL || sysprobe(\"socket-calls\")!=0) exit 1; }", HAWK_ENOERR, SOCKET_RANGE_TEST },
	{ "sys-log-windows-socket-error", "BEGIN {sysprobe(\"socket-failure\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"socket-calls\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-windows-delayed-socket-error", "BEGIN {sysprobe(\"socket-failure\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"socket-calls\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-windows-nonblock-error", "BEGIN {sysprobe(\"log-fail-nonblock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"socket-closed\")!=1 || sysprobe(\"log-config-calls\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-windows-inheritance-error", "BEGIN {sysprobe(\"log-fail-inheritance\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=sysfault::RC_EACCES || length(sysfault::errmsg())==0 || sysprobe(\"socket-closed\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-windows-socket-flags", "BEGIN {if(sysfault::openlog(\"remote://127.0.0.1:9/test\",sysfault::LOG_OPT_NDELAY,sysfault::LOG_FAC_USER)!=0 || sysprobe(\"log-nonblock\")!=1 || sysprobe(\"log-inheritance\")!=1 || sysfault::closelog()!=0 || sysprobe(\"socket-closed\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-windows-local-unsupported", "BEGIN {if(sysfault::openlog(\"local://test\",0,sysfault::LOG_FAC_USER)!=sysfault::RC_ENOSUP || sysfault::writelog(sysfault::LOG_PRI_INFO,\"hello\")!=sysfault::RC_ENOSUP || length(sysfault::errmsg())==0) exit 1;sysprobe(\"log-mock\");if(sysfault::openlog(\"remote://127.0.0.1:9/test\",0,sysfault::LOG_FAC_USER)!=0 || sysfault::openlog(\"test\",0,sysfault::LOG_FAC_USER)!=sysfault::RC_ENOSUP || sysfault::writelog(sysfault::LOG_PRI_INFO,\"remote still works\")!=0 || sysprobe(\"log-send-calls\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-log-windows-loopback", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::bind(s,\"127.0.0.1:0\")!=0 || sysfault::setsockopt(s,sysfault::SOL_SOCKET,sysfault::SO_RCVTIMEO,2)!=0) exit 1;addr=sysprobe(\"socket-address\");if(sys::openlog(\"remote://\" addr \"/hawk-log\",sys::LOG_OPT_ISO8601|sys::LOG_OPT_NDELAY|sys::LOG_OPT_PID,sys::LOG_FAC_LOCAL0)!=0 || sys::writelog(sys::LOG_PRI_INFO,\"actual datagram\")!=0 || sysfault::read(s,x,2048)<=0) exit 1;tag=str::tombs(\"hawk-log[\" sys::getpid() \"]: actual datagram\");if(x !~ /^<134>[0-9]+-[0-9]+-[0-9]+T[0-9]+:[0-9]+:[0-9]+[+-][0-9]+:[0-9]+ / || index(x,tag)==0 || sys::closelog()!=0 || sysfault::close(s)!=0) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-accept-no-address", "BEGIN {sysprobe(\"accept-no-address\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);peer=\"old\";a=sysfault::accept(s,0,peer);if(s<0 || a<0 || peer!==\"\" || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-accept-unix-unnamed", "BEGIN {sysprobe(\"accept-unix-unnamed\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);peer=\"old\";a=sysfault::accept(s,0,peer);if(s<0 || a<0 || peer!==\"\" || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-accept-unix-short-address", "BEGIN {sysprobe(\"accept-unix-short\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);a=sysfault::accept(s,0,peer);if(s<0 || a<0 || peer!==\"peer\" || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-accept-unix-full-address", "BEGIN {sysprobe(\"accept-unix-full\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);a=sysfault::accept(s,0,peer);if(s<0 || a<0 || length(peer)!=sysprobe(\"unix-path-size\") || peer !~ /^p+$/ || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-bind-unix-nul-address", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0 || sysfault::bind(s,\"/hawk-review\\0extra\")!=sysfault::RC_EINVAL || sysprobe(\"bind-calls\")!=0 || index(sysfault::errmsg(),\"socket address\")==0 || sysfault::bind(s,\"@hawk-review\\0extra\")!=sysfault::RC_EINVAL || sysprobe(\"bind-calls\")!=0 || sysfault::bind(s,\"@hawk-review\")!=0 || sysprobe(\"bind-calls\")!=1) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-connect-unix-nul-address", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0 || sysfault::connect(s,\"/hawk-review\\0extra\")!=sysfault::RC_EINVAL || sysprobe(\"connect-calls\")!=0 || index(sysfault::errmsg(),\"socket address\")==0 || sysfault::connect(s,\"@hawk-review\")!=0 || sysprobe(\"connect-calls\")!=1) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-sendto-unix-nul-address", "BEGIN {sysprobe(\"log-mock\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);if(s<0 || sysfault::sendto(s,@b\"x\",\"/hawk-review\\0extra\")!=sysfault::RC_EINVAL || sysprobe(\"log-send-calls\")!=0 || index(sysfault::errmsg(),\"socket address\")==0 || sysfault::sendto(s,@b\"x\",\"@hawk-review\")!=1 || sysprobe(\"log-send-calls\")!=1) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-sockaddrdom-nul-address", "BEGIN {if(sys::sockaddrdom(\"/hawk-review\\0extra\")!=sys::RC_EINVAL || index(sys::errmsg(),\"socket address\")==0) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-unix-socket-native-addresses", "BEGIN {p=sys::tempname(\"hawk-socket-\");if(p==\"\" || sys::unlink(p)!=0) exit 1;d=sys::sockaddrdom(p);s=sys::socket(d,sys::SOCK_STREAM,0);c=sys::socket(d,sys::SOCK_STREAM,0);bad=0;if(s<0 || c<0 || sys::bind(s,p \"\\0extra\")!=sys::RC_EINVAL || sys::stat(p,st)>=0 || sys::bind(s,p)!=0 || sys::listen(s,1)!=0 || sys::connect(c,p \"\\0extra\")!=sys::RC_EINVAL || sys::connect(c,p)!=0) bad=1;else {peer=\"old\";a=sys::accept(s,0,peer);if(a<0 || peer!==\"\") bad=1;else sys::close(a);}if(s>=0) sys::close(s);if(c>=0) sys::close(c);sys::unlink(p);if(bad) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-accept-assignment-failure", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);x[1]=1;if(s<0 || sysfault::accept(s,0,x)>=0) exit 1; }", HAWK_ENOERR, SYS | STRICTMAP },
	{ "sys-accept-fallback-unknown-flags", SYS_ACCEPT_UNSUPPORTED_FLAGS("1"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-accept-fallback-mixed-cloexec-flags", SYS_ACCEPT_UNSUPPORTED_FLAGS("sysfault::SOCK_CLOEXEC|1"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-accept-fallback-mixed-nonblock-flags", SYS_ACCEPT_UNSUPPORTED_FLAGS("sysfault::SOCK_NONBLOCK|1"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-accept-fallback-getfd-failure", SYS_ACCEPT_FLAG_FAILURE("fail-getfd"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-accept-fallback-setfd-failure", SYS_ACCEPT_FLAG_FAILURE("fail-setfd"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-accept-fallback-getfl-failure", SYS_ACCEPT_FLAG_FAILURE("fail-getfl"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-accept-fallback-setfl-failure", SYS_ACCEPT_FLAG_FAILURE("fail-setfl"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-accept-fallback-flags-success", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;a=sysfault::accept(s,sysfault::SOCK_NONBLOCK|sysfault::SOCK_CLOEXEC,peer);if(a<0 || peer!=\"0.0.0.0\" || sysprobe(\"accepted-flags\")!=1 || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-pipe-fallback-getfd-failure", SYS_PIPE_FLAG_FAILURE("fail-getfd"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-pipe-fallback-setfd-failure", SYS_PIPE_FLAG_FAILURE("fail-setfd"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-pipe-fallback-getfl-failure", SYS_PIPE_FLAG_FAILURE("fail-getfl"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-pipe-fallback-setfl-failure", SYS_PIPE_FLAG_FAILURE("fail-setfl"), HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-pipe-fallback-flags-success", "BEGIN {if(sysfault::pipe(a,b,sysfault::O_NONBLOCK|sysfault::O_CLOEXEC)!=0 || sysprobe(\"pipe-flags\")!=1 || sysfault::close(a)!=0 || sysfault::close(b)!=0) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-pipe-fallback-unknown-flags", "BEGIN {a=41;b=42;if(sysfault::pipe(a,b,1)!=sysfault::RC_EINVAL || a!=41 || b!=42 || sysprobe(\"pipe-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-pipe-fallback-mixed-flags", "BEGIN {a=41;b=42;if(sysfault::pipe(a,b,sysfault::O_CLOEXEC|sysfault::O_NONBLOCK|1)!=sysfault::RC_EINVAL || a!=41 || b!=42 || sysprobe(\"pipe-calls\")!=0 || length(sysfault::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-empty-datagram", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);b=@b\"old\";peer=\"old\";if(s<0 || sysfault::recvfrom(s,b,16,peer)!=0 || b!==@b\"\" || peer==\"old\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-recv-unix-unnamed", "BEGIN {sysprobe(\"recv-unix-unnamed\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);b=@b\"old\";peer=\"old\";if(s<0 || sysfault::recvfrom(s,b,16,peer)!=1 || b!==@b\"Z\" || peer!==\"\" || sysfault::close(s)!=0) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-recv-unix-short-address", "BEGIN {sysprobe(\"recv-unix-short\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);b=@b\"old\";peer=\"old\";if(s<0 || sysfault::recvfrom(s,b,16,peer)!=1 || b!==@b\"Z\" || peer!==\"peer\" || sysfault::close(s)!=0) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-recv-unix-full-address", "BEGIN {sysprobe(\"recv-unix-full\");s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_DGRAM,0);b=@b\"old\";peer=\"old\";if(s<0 || sysfault::recvfrom(s,b,16,peer)!=1 || b!==@b\"Z\" || length(peer)!=sysprobe(\"unix-path-size\") || peer !~ /^p+$/ || sysfault::close(s)!=0) exit 1; }", HAWK_ENOERR, SYS | UNIX_SOCKET },
	{ "sys-recv-no-address-data", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;sysprobe(\"recv-no-address-data\");b=@b\"old\";peer=\"old\";if(sysfault::recvfrom(s,b,16,peer)!=1 || b!==@b\"Z\" || peer!==\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-recv-no-address-eof", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;sysprobe(\"recv-no-address-eof\");b=@b\"old\";peer=\"old\";if(sysfault::recvfrom(s,b,16,peer)!=0 || b!==@b\"\" || peer!==\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-recv-no-address-without-output", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;sysprobe(\"recv-no-address-data\");if(sysfault::recvfrom(s,b,16)!=1 || b!==@b\"Z\") exit 1;sysprobe(\"recv-no-address-eof\");if(sysfault::recvfrom(s,b,16)!=0 || b!==@b\"\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-pack-unpack-big-endian", "BEGIN { if(sys::pack(b,\">hIldc3p\",-123,16777215,-123456789,2.5,@b\"A\",@b\"abc\")!=0 || sys::unpack(b,\">hIldc3p\",a,c,d,e,f,g)!=0 || a!=-123 || c!=16777215 || d!=-123456789 || e!=2.5 || f!==@b'A' || g!==@b\"abc\") exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-pack-unpack-native-width", "BEGIN { if(sys::pack(b,\"=qQnN\",-123,123,-456,456)!=0 || sys::unpack(b,\"=qQnN\",a,c,d,e)!=0 || a!=-123 || c!=123 || d!=-456 || e!=456) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-dup-buffer-residue", "BEGIN {sys::pipe(a,b);sys::pipe(c,d);sys::write(b,@b\"A\\nOLD\\n\");sys::write(d,@b\"NEW\\n\");sys::close(d);sys::read(a,x,8192,\"\\n\");if(sys::dup(c,a)!=a || sys::read(a,x,8192,\"\\n\")!=4 || x!==@b\"NEW\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-dup-mux-alias", "BEGIN {m=sys::openmux();sys::pipe(a,b);k=sys::dup(a);s=sys::open(\"/dev/null\",sys::O_RDONLY);if(m<0 || k<0 || s<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)<0 || sys::dup(s,a)!=a) exit 1;sys::write(b,@b\"x\");if(sys::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-dup-failure-mux", "BEGIN {m=sysfault::openmux();sysfault::pipe(a,b);s=sysfault::open(\"/dev/null\",sysfault::O_RDONLY);if(m<0 || s<0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)<0) exit 1;if(sysfault::dup(s,a,1)>=0) exit 1;sysfault::write(b,@b\"x\");if(sysfault::waitonmux(m,0)!=1 || sysfault::getmuxevt(m,0,id,ev)!=0 || id!=a) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-flock-get-optional-pid", "BEGIN {p=sys::tempname(\"hawk-flock-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;} if(sys::unlink(p)!=0 || sys::flock(f,sys::FLOCK_READ|sys::FLOCK_GET,0,0,sys::SEEK_SET)!=sys::FLOCK_UNLOCK || sys::flock(f,sys::FLOCK_WRITE|sys::FLOCK_GET,0,0,sys::SEEK_SET)!=sys::FLOCK_UNLOCK || sys::flock(f,sys::FLOCK_READ|sys::FLOCK_GET,0,0,sys::SEEK_SET,pid)!=sys::FLOCK_UNLOCK || sys::flock(f,sys::FLOCK_WRITE|sys::FLOCK_GET,0,0,sys::SEEK_SET,pid)!=sys::FLOCK_UNLOCK || sys::close(f)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-set-residue", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,0,sys::SEEK_SET)!=0 || sys::read(f,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-current-position", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,0,sys::SEEK_CUR)!=2 || sys::read(f,x,8192,\"\\n\")!=5 || x!==@b\"LEFT\\n\" || sys::fseek(f,0,sys::SEEK_CUR)!=7) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-current-forward", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,2,sys::SEEK_CUR)!=4 || sys::read(f,x,8192,\"\\n\")!=3 || x!==@b\"FT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-current-backward", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,-2,sys::SEEK_CUR)!=0 || sys::read(f,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-end-residue", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,-3,sys::SEEK_END)!=4 || sys::read(f,x,8192,\"\\n\")!=3 || x!==@b\"FT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-failure-preserves-residue", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,-1,sys::SEEK_SET)>=0 || sys::read(f,x,8192,\"\\n\")!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-current-underflow", SYS_SEEK_SOURCE "BEGIN {f=seekfile(); if(sys::fseek(f,minint(),sys::SEEK_CUR)!=sys::RC_EINVAL || sys::read(f,x,8192,\"\\n\")!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-seek-whence-positive-overflow", SYS_SEEK_SOURCE "BEGIN {f=seekfile();n=sys::fseek(f,0,4294967297);if(n!=sys::RC_EINVAL || length(sys::errmsg())==0 || sys::read(f,x,8192)!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-seek-whence-negative-overflow", SYS_SEEK_SOURCE "BEGIN {f=seekfile();n=sys::fseek(f,0,-4294967295);if(n!=sys::RC_EINVAL || length(sys::errmsg())==0 || sys::read(f,x,8192)!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-seek-pipe-preserves-residue", "BEGIN {if(sys::pipe(a,b)<0 || sys::write(b,@b\"A\\nLEFT\\n\")!=7) exit 1;sys::close(b);if(sys::read(a,x,8192,\"\\n\")!=2 || sys::fseek(a,0,sys::SEEK_CUR)>=0 || sys::read(a,x,8192,\"\\n\")!=5 || x!==@b\"LEFT\\n\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mux-delete-wrong-owner", "BEGIN {m=sys::openmux();n=sys::openmux();if(m<0 || n<0 || sys::pipe(a,b)<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)!=0 || sys::delfrommux(n,a)!=sys::RC_EPERM || sys::modinmux(m,a,sys::MUX_EVT_IN)!=0) exit 1;sys::write(b,@b\"x\");if(sys::waitonmux(m,0)!=1 || sys::getmuxevt(m,0,id,ev)!=0 || id!=a || sys::delfrommux(m,a)!=0 || sys::closemux(n)!=0 || sys::closemux(m)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-delete-shared-descriptor", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);b=sys::openfd(r);m=sys::openmux();n=sys::openmux();if(r<0 || a<0 || b<0 || m<0 || n<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)!=0 || sys::addtomux(n,b,sys::MUX_EVT_IN)!=0 || sys::delfrommux(n,a)!=sys::RC_EPERM || sys::modinmux(m,a,sys::MUX_EVT_IN)!=0 || sys::modinmux(n,b,sys::MUX_EVT_IN)!=0 || sys::delfrommux(m,b)!=sys::RC_EPERM || sys::delfrommux(m,a)!=0 || sys::delfrommux(n,b)!=0) exit 1;if(sys::close(a,sys::C_KEEPFD)!=0 || sys::close(b,sys::C_KEEPFD)!=0 || sys::closemux(n)!=0 || sys::closemux(m)!=0 || sysprobe(\"rawfd-open\")!=1) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-close-shared-descriptor", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);w=sys::openfd(sysprobe(\"rawfd-writer\"));k=sys::dup(a);m=sys::openmux();b=sys::openfd(r);if(r<0 || a<0 || w<0 || k<0 || m<0 || b<0 || sys::addtomux(m,b,sys::MUX_EVT_IN)!=0) exit 1;if(sys::close(a)!=0 || sys::close(b)!=0 || sys::write(w,@b\"x\")!=1 || sys::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-close-shared-slot-reuse", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);b=sys::openfd(r);w=sys::openfd(sysprobe(\"rawfd-writer\"));k=sys::dup(a);m=sys::openmux();if(r<0 || a<0 || b<0 || w<0 || k<0 || m<0 || sys::addtomux(m,b,sys::MUX_EVT_IN)!=0 || sys::close(a)!=0 || sys::close(b)!=0) exit 1;c=sys::open(\"/dev/null\",sys::O_RDONLY);d=sys::open(\"/dev/null\",sys::O_RDONLY);if(c!=a || d!=b || sys::write(w,@b\"x\")!=1 || sys::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-close-shared-cached-events", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);b=sys::openfd(r);w=sys::openfd(sysprobe(\"rawfd-writer\"));k=sys::dup(a);m=sys::openmux();n=sys::openmux();if(r<0 || a<0 || b<0 || w<0 || k<0 || m<0 || n<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)!=0 || sys::addtomux(n,b,sys::MUX_EVT_IN)!=0 || sys::write(w,@b\"x\")!=1 || sys::waitonmux(m,0)!=1 || sys::waitonmux(n,0)!=1 || sys::close(a)!=0) exit 1;id=41;ev=42;if(sys::getmuxevt(m,0,id,ev)!=sys::RC_ENOENT || sys::getmuxevt(n,0,id,ev)!=sys::RC_ENOENT || id!=41 || ev!=42 || sys::close(b)!=0 || sys::waitonmux(m,0)!=0 || sys::waitonmux(n,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-keepfd-shared-descriptor", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);b=sys::openfd(r);w=sys::openfd(sysprobe(\"rawfd-writer\"));m=sys::openmux();n=sys::openmux();if(r<0 || a<0 || b<0 || w<0 || m<0 || n<0 || sys::addtomux(m,a,sys::MUX_EVT_IN)!=0 || sys::addtomux(n,b,sys::MUX_EVT_IN)!=0 || sys::close(a,sys::C_KEEPFD)!=0 || sysprobe(\"rawfd-open\")!=1 || sys::write(w,@b\"x\")!=1 || sys::waitonmux(m,0)!=0 || sys::waitonmux(n,0)!=1 || sys::getmuxevt(n,0,id,ev)!=0 || id!=b || sys::read(b,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-close-invalid-descriptor", "BEGIN {f=sys::openfd(2147483647);if(f<0 || sys::close(f)!=sys::RC_EINVAL || index(sys::errmsg(),\"unable to close file descriptor\")==0 || sys::close(f)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-close-invalid-shared-descriptor", "BEGIN {a=sys::openfd(2147483647);b=sys::openfd(2147483647);if(a<0 || b<0 || sys::close(a)!=sys::RC_EINVAL || index(sys::errmsg(),\"unable to close file descriptor\")==0 || sys::close(b)!=0 || sys::close(a)!=sys::RC_EINVAL || sys::close(b)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-close-invalid-descriptor-keepfd", "BEGIN {f=sys::openfd(2147483647);if(f<0 || sys::close(f,sys::C_KEEPFD)!=0 || sys::close(f)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, 0 },
	{ "sys-close-file-io-error", SYS_CLOSE_FILE_ERROR("file-close-eio", "RC_EIOERR", "0"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-close-file-interrupted", SYS_CLOSE_FILE_ERROR("file-close-eintr", "RC_EINTR", "0"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-close-file-error-before-cleanup", SYS_CLOSE_FILE_ERROR("file-close-eio", "RC_EIOERR", "1"), HAWK_ENOERR, SOCKET_TEST },
	{ "sys-close-cleanup-preserves-error", "BEGIN {sysprobe(\"file-close-eio\");arm();n=sysfault::pipe(a,b);if(n<0 && (n!=sysfault::RC_ENOMEM || length(sysfault::errmsg())==0 || index(sysfault::errmsg(),\"close\")>0)) exit 1;sysprobe(\"file-close-normal\");if(n==0 && (sysfault::close(a)!=0 || sysfault::close(b)!=0)) exit 1; }", HAWK_ENOERR, SYS | SWEEP },
	{ "sys-close-shared-io-error", "BEGIN {if(sysfault::pipe(a,w)!=0) exit 1;b=sysfault::openfd(sysprobe(\"pipe-reader\"));m=sysfault::openmux();if(b<0 || m<0 || sysfault::addtomux(m,b,sysfault::MUX_EVT_IN)!=0 || sysfault::write(w,@b\"A\\nLEFT\\n\")!=7 || sysfault::waitonmux(m,0)!=1 || sysfault::read(b,x,8192,\"\\n\")!=2) exit 1;sysprobe(\"file-close-eio\");n=sysfault::close(a);ok=n==sysfault::RC_EIOERR && index(sysfault::errmsg(),\"unable to close file descriptor\")>0 && sysprobe(\"file-close-calls\")==1;x=@b\"keep\";id=41;ev=42;if(sysfault::read(b,x,8192)!=sysfault::RC_EINVAL || x!==@b\"keep\" || sysfault::getmuxevt(m,0,id,ev)!=sysfault::RC_ENOENT || id!=41 || ev!=42 || sysfault::close(b)!=0 || sysprobe(\"file-close-calls\")!=1) ok=0;sysprobe(\"file-close-normal\");if(sysfault::close(w)!=0 || sysfault::closemux(m)!=0 || !ok) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-close-keepfd-skips-os-close", "BEGIN {if(sysfault::pipe(a,w)!=0) exit 1;b=sysfault::openfd(sysprobe(\"pipe-reader\"));if(b<0) exit 1;sysprobe(\"file-close-eio\");if(sysfault::close(b,sysfault::C_KEEPFD)!=0 || sysprobe(\"file-close-calls\")!=0 || sysfault::write(w,@b\"x\")!=1 || sysfault::read(a,x,1)!=1 || x!==@b\"x\") exit 1;sysprobe(\"file-close-normal\");if(sysfault::close(a)!=0 || sysfault::close(w)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-closemux-io-error", "BEGIN {m=sysfault::openmux();if(m<0) exit 1;sysprobe(\"file-close-eio\");if(sysfault::closemux(m)!=sysfault::RC_EIOERR || index(sysfault::errmsg(),\"unable to close multiplexer\")==0 || sysprobe(\"file-close-calls\")!=1 || sysfault::closemux(m)!=sysfault::RC_EINVAL || sysprobe(\"file-close-calls\")!=1) exit 1;sysprobe(\"file-close-normal\"); }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-close-socket-os-error", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;sysprobe(\"socket-close-error\");clobber_free_errno(1);n=sysfault::close(s);clobber_free_errno(0);if(n!=sysfault::RC_EACCES || index(sysfault::errmsg(),\"unable to close socket\")==0 || sysprobe(\"socket-closed\")!=1 || sysprobe(\"file-close-calls\")!=0 || sysfault::close(s)!=sysfault::RC_EINVAL || sysprobe(\"socket-closed\")!=1) exit 1; }", HAWK_ENOERR, WIN_SYS },
	{ "sys-close-socket-teardown-error", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(s<0) exit 1;sysprobe(\"socket-close-error\"); }", HAWK_ENOERR, WIN_SYS },
	{ "sys-close-shared-descriptor-reuse", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);b=sys::openfd(r);w=sys::openfd(sysprobe(\"rawfd-writer\"));k=sys::dup(a);if(r<0 || a<0 || b<0 || w<0 || k<0 || sys::close(a)!=0 || sysprobe(\"rawfd-reuse\")!=r || sys::close(b)!=0 || sysprobe(\"rawfd-open\")!=1) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-close-shared-buffered-residue", "BEGIN {r=sysprobe(\"rawfd\");a=sys::openfd(r);b=sys::openfd(r);w=sys::openfd(sysprobe(\"rawfd-writer\"));k=sys::dup(a);if(r<0 || a<0 || b<0 || w<0 || k<0 || sys::write(w,@b\"A\\nLEFT\\n\")!=7 || sys::read(b,x,8192,\"\\n\")!=2 || x!==@b\"A\\n\" || sys::close(a)!=0) exit 1;x=@b\"keep\";if(sys::read(b,x,8192)>=0 || x!==@b\"keep\" || sys::close(b)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-mux-close-ordinary-with-import", "BEGIN {if(sysfault::pipe(a,w)<0) exit 1;b=sysfault::openfd(sysprobe(\"pipe-reader\"));k=sysfault::dup(a);m=sysfault::openmux();if(b<0 || k<0 || m<0 || sysfault::addtomux(m,b,sysfault::MUX_EVT_IN)!=0 || sysfault::close(a)!=0 || sysfault::close(b)!=0 || sysfault::write(w,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-mux-close-import-with-ordinary", "BEGIN {if(sysfault::pipe(a,w)<0) exit 1;b=sysfault::openfd(sysprobe(\"pipe-reader\"));k=sysfault::dup(a);m=sysfault::openmux();if(b<0 || k<0 || m<0 || sysfault::addtomux(m,a,sysfault::MUX_EVT_IN)!=0 || sysfault::close(b)!=0 || sysfault::close(a)!=0 || sysfault::write(w,@b\"x\")!=1 || sysfault::waitonmux(m,0)!=0) exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-close-import-before-dup", "BEGIN {if(sysprobe(\"rawfd\")<0) exit 1;w=sysfault::openfd(sysprobe(\"rawfd-writer\"));r=sysprobe(\"rawfd-release-reader\");a=sysfault::openfd(r);k=sysfault::dup(w);if(r<0 || w<0 || a<0 || k<0 || sysprobe(\"rawfd-open\")!=1 || sysfault::close(k)!=0 || sysprobe(\"rawfd-open\")!=0 || sysprobe(\"rawfd-reuse\")!=r || sysfault::close(a)!=0 || sysprobe(\"rawfd-open\")!=1) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-close-import-before-socket", "BEGIN {if(sysprobe(\"rawfd\")<0) exit 1;r=sysprobe(\"socket-at-rawfd\");a=sysfault::openfd(r);s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);if(r<0 || a<0 || s<0 || sysprobe(\"socket-fd\")!=r || sysfault::close(a)!=0 || sysprobe(\"rawfd-open\")!=0 || sysprobe(\"rawfd-reuse\")!=r || sysfault::close(s)!=0 || sysprobe(\"rawfd-open\")!=1) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-close-socket-with-import", "BEGIN {s=sysfault::socket(sysfault::AF_INET,sysfault::SOCK_STREAM,0);a=sysfault::openfd(sysprobe(\"socket-fd\"));if(s<0 || a<0 || sysfault::close(s)!=0 || sysfault::fcntl(a,sysfault::F_GETFD)>=0 || sysfault::close(a)!=0 || sysprobe(\"log-closed\")!=1) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-close-real-dup-independent", "BEGIN {if(sysfault::pipe(a,w)<0) exit 1;b=sysfault::openfd(sysprobe(\"pipe-reader\"));k=sysfault::dup(a);if(b<0 || k<0 || sysfault::close(k)!=0 || sysfault::write(w,@b\"x\")!=1 || sysfault::read(b,x,1)!=1 || x!==@b\"x\" || sysfault::close(b)!=0 || sysfault::fcntl(a,sysfault::F_GETFD)>=0 || sysfault::close(a)!=0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-close-import-keepfd-node-reuse", "BEGIN {for(i=0;i<100;i++) {if(sysfault::pipe(a,w)<0) exit 1;b=sysfault::openfd(sysprobe(\"pipe-reader\"));if(b<0 || sysfault::close(b,sysfault::C_KEEPFD)!=0 || sysfault::write(w,@b\"x\")!=1 || sysfault::read(a,x,1)!=1 || x!==@b\"x\" || sysfault::close(a)!=0) exit 1;c=sysfault::open(\"/dev/null\",sysfault::O_RDONLY);if(c!=a || sysfault::close(c)!=0 || sysfault::close(w)!=0) exit 1;} }", HAWK_ENOERR, SYS },
	{ "sys-dup-fallback-fail-getfd", "BEGIN { a=sysfallback::open(\"/dev/null\",sysfallback::O_RDWR);b=sysfallback::open(\"/dev/null\",sysfallback::O_RDWR);if(a<0 || b<0) exit 1;sysprobe(\"fail-getfd\");if(sysfallback::dup(a,b,sysfallback::O_CLOEXEC)!=sysfallback::RC_EINVAL || length(sysfallback::errmsg())==0) exit 1;sysprobe(\"clear-fcntl-failure\");if(sysfallback::fcntl(b,sysfallback::F_GETFD)<0 || sysfallback::close(a)!=0 || sysfallback::close(b)!=0) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-dup-fallback-fail-setfd", "BEGIN { a=sysfallback::open(\"/dev/null\",sysfallback::O_RDWR);b=sysfallback::open(\"/dev/null\",sysfallback::O_RDWR);if(a<0 || b<0) exit 1;sysprobe(\"fail-setfd\");if(sysfallback::dup(a,b,sysfallback::O_CLOEXEC)!=sysfallback::RC_EINVAL || length(sysfallback::errmsg())==0) exit 1;sysprobe(\"clear-fcntl-failure\");if(sysfallback::fcntl(b,sysfallback::F_GETFD)<0 || sysfallback::close(a)!=0 || sysfallback::close(b)!=0) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-dup-fallback-cloexec", "BEGIN { a=sysfallback::open(\"/dev/null\",sysfallback::O_RDWR);b=sysfallback::open(\"/dev/null\",sysfallback::O_RDWR);if(a<0 || b<0 || sysfallback::dup(a,b,sysfallback::O_CLOEXEC)!=b || !(sysfallback::fcntl(b,sysfallback::F_GETFD)&sysfallback::FD_CLOEXEC)) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-dup-fallback-unknown-flags-residue", "BEGIN {if(sysfallback::pipe(a,b)!=0 || sysfallback::pipe(c,d)!=0 || sysfallback::write(d,@b\"A\\nOLD\\n\")!=6 || sysfallback::read(c,x,8192,\"\\n\")!=2) exit 1;if(sysfallback::dup(a,c,1)!=sysfallback::RC_EINVAL || length(sysfallback::errmsg())==0 || sysfallback::read(c,x,4)!=4 || x!==@b\"OLD\\n\" || sysfallback::write(d,@b\"x\")!=1 || sysfallback::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-dup-fallback-mixed-flags", "BEGIN {if(sysfallback::pipe(a,b)!=0 || sysfallback::pipe(c,d)!=0 || sysfallback::dup(a,c,sysfallback::O_CLOEXEC|1)!=sysfallback::RC_EINVAL || sysfallback::write(d,@b\"x\")!=1 || sysfallback::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-dup-fallback-nonblock", "BEGIN {if(sysfallback::pipe(a,b)!=0 || sysfallback::pipe(c,d)!=0 || sysfallback::dup(a,c,sysfallback::O_NONBLOCK)!=sysfallback::RC_EINVAL || sysfallback::write(d,@b\"x\")!=1 || sysfallback::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-dup-fallback-unknown-flags-mux", "BEGIN {m=sysfallback::openmux();if(m<0 || sysfallback::pipe(a,b)!=0 || sysfallback::pipe(c,d)!=0 || sysfallback::addtomux(m,c,sysfallback::MUX_EVT_IN)!=0 || sysfallback::write(d,@b\"x\")!=1 || sysfallback::waitonmux(m,0)!=1) exit 1;if(sysfallback::dup(a,c,1)!=sysfallback::RC_EINVAL || sysfallback::getmuxevt(m,0,id,ev)!=0 || id!=c || sysfallback::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS | EPOLL },
	{ "sys-dup-native-forwards-flags", "BEGIN {if(sysfault::pipe(a,b)!=0 || sysfault::pipe(c,d)!=0 || sysfault::dup(a,c,1)!=sysfault::RC_EINVAL || sysprobe(\"dup-calls\")!=1 || sysfault::write(d,@b\"x\")!=1 || sysfault::read(c,x,1)!=1 || x!==@b\"x\") exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-ifcfg-socket-error", "BEGIN { x=\"keep\";if(sysfallback::getifcfg(\"anything\",sysfallback::IFCFG_IN4,x)!=sysfallback::RC_EACCES || x!==\"keep\" || length(sysfallback::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-ifcfg-invalid-family", "BEGIN { x=\"keep\";if(sys::getifcfg(\"anything\",12345,x)!=sys::RC_EINVAL || x!==\"keep\" || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-ifcfg-name-nul", "BEGIN { x=\"keep\";if(sys::getifcfg(\"lo\\0other\",sys::IFCFG_IN4,x)!=sys::RC_EINVAL || x!==\"keep\" || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-ifcfg-name-nul-only", "BEGIN { x=\"keep\";if(sys::getifcfg(\"\\0\",sys::IFCFG_IN4,x)!=sys::RC_EINVAL || x!==\"keep\" || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-ifcfg-family-overflow-positive", "BEGIN { x=\"keep\";if(sys::getifcfg(\"anything\",4294967296+sys::IFCFG_IN4,x)!=sys::RC_EINVAL || x!==\"keep\") exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-ifcfg-family-overflow-negative", "BEGIN { x=\"keep\";if(sys::getifcfg(\"anything\",-4294967296+sys::IFCFG_IN4,x)!=sys::RC_EINVAL || x!==\"keep\") exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-win-file-read-wrap-zero", "BEGIN { sysprobe(\"file-mock\");f=sysfault::open(\"NUL\",sysfault::O_RDONLY);if(f<0 || sysfault::read(f,x,4294967296)!=3 || x!==@b\"ABC\" || sysprobe(\"file-count\")!=2147483647) exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW | LARGE_IO },
	{ "sys-win-file-read-int-boundary", "BEGIN { sysprobe(\"file-mock\");f=sysfault::open(\"NUL\",sysfault::O_RDONLY);if(f<0 || sysfault::read(f,x,2147483648)!=3 || x!==@b\"ABC\" || sysprobe(\"file-count\")!=2147483647) exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW | LARGE_IO },
	{ "sys-win-file-write-count", "BEGIN { sysprobe(\"file-mock\");f=sysfault::open(\"NUL\",sysfault::O_WRONLY);if(f<0 || sysfault::write(f,oversized_mbs(0))!=1 || sysprobe(\"file-count\")!=2147483647) exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW },
	{ "sys-open-mode-overflow", "BEGIN { p=sys::tempname(\"hawk-mode-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;} if(sys::write(f,@b\"keep\")!=4) exit 1;n=sys::open(p,sys::O_RDWR|sys::O_TRUNC,4294967296+0600);sys::fseek(f,0,sys::SEEK_SET);r=sys::read(f,x,4);sys::close(f);sys::unlink(p);if(r!=4 || x!==@b\"keep\" || n!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | MODE32_NARROW },
	{ "sys-chmod-mode-overflow", "BEGIN { p=sys::tempname(\"hawk-mode-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;} n=sys::chmod(p,4294967296+0600);sys::close(f);sys::unlink(p);if(n!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | MODE32_NARROW },
	{ "sys-chmod-invalid-function-mode", SYS_CHMOD_INVALID_MODE("f"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-bob-mode", SYS_CHMOD_INVALID_MODE("bob()"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-nan-mode", SYS_CHMOD_INVALID_MODE("math::sqrt(-1)"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-positive-infinity-mode", SYS_CHMOD_INVALID_MODE("-math::log(0)"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-negative-infinity-mode", SYS_CHMOD_INVALID_MODE("math::log(0)"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-positive-float-mode", SYS_CHMOD_INVALID_MODE("1e100"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-negative-float-mode", SYS_CHMOD_INVALID_MODE("-1e100"), HAWK_ENOERR, SYS },
	{ "sys-chmod-invalid-float-string-mode", SYS_CHMOD_INVALID_MODE("\"1e100\""), HAWK_ENOERR, SYS },
	{ "sys-chmod-negative-mode-default", "BEGIN {p=sys::tempname(\"hawk-chmod-\");if(p==\"\") exit 1;ok=sys::chmod(p,-1)==0 && sys::stat(p,s)==0 && (s[\"mode\"]&07777)==0777;if(sys::chmod(p,0600)!=0 || sys::stat(p,s)!=0 || (s[\"mode\"]&07777)!=0600) ok=0;if(sys::unlink(p)!=0 || !ok) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-fchmod-mode-overflow", "BEGIN { p=sys::tempname(\"hawk-mode-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;} n=sys::fchmod(f,4294967296+0600);sys::close(f);sys::unlink(p);if(n!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | MODE32_NARROW },
	{ "sys-mkdir-mode-overflow", "BEGIN { p=sys::tempname(\"hawk-mode-\");f=sys::open(p,sys::O_RDWR);if(f<0) {sys::unlink(p);exit 1;} sys::unlink(p);n=sys::mkdir(p,4294967296+0700);sys::close(f);if(n>=0) sys::rmdir(p);if(n!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | MODE32_NARROW },
	{ "sys-addtomux-event-overflow", "BEGIN { m=sys::openmux();if(m<0 || sys::pipe(a,b)<0) exit 1;if(sys::addtomux(m,a,4294967296+sys::MUX_EVT_IN)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS | EPOLL | INT32_NARROW },
	{ "sys-modinmux-event-overflow", "BEGIN { m=sys::openmux();if(m<0 || sys::pipe(a,b)<0) exit 1;if(sys::addtomux(m,a,sys::MUX_EVT_IN)!=0) exit 1;if(sys::modinmux(m,a,4294967296+sys::MUX_EVT_IN)!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS | EPOLL | INT32_NARROW },
	{ "sys-tcsetattr-action-overflow", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);t[\"lflag\"]=0;if(f<0 || sysfault::tcsetattr(f,4294967296,t)!=sysfault::RC_EINVAL || sysprobe(\"tcsetattr-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-tcsetattr-iflag-overflow", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);t[\"iflag\"]=4294967296;if(f<0 || sysfault::tcsetattr(f,0,t)!=sysfault::RC_EINVAL || sysprobe(\"tcsetattr-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | TCFLAG32_NARROW },
	{ "sys-tcsetattr-oflag-overflow", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);t[\"oflag\"]=4294967296;if(f<0 || sysfault::tcsetattr(f,0,t)!=sysfault::RC_EINVAL || sysprobe(\"tcsetattr-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | TCFLAG32_NARROW },
	{ "sys-tcsetattr-cflag-overflow", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);t[\"cflag\"]=4294967296;if(f<0 || sysfault::tcsetattr(f,0,t)!=sysfault::RC_EINVAL || sysprobe(\"tcsetattr-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | TCFLAG32_NARROW },
	{ "sys-tcsetattr-lflag-overflow", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);t[\"lflag\"]=4294967296;if(f<0 || sysfault::tcsetattr(f,0,t)!=sysfault::RC_EINVAL || sysprobe(\"tcsetattr-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | TCFLAG32_NARROW },
	{ "sys-tcflush-queue-overflow", "BEGIN { f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);if(f<0 || sysfault::tcflush(f,4294967296)!=sysfault::RC_EINVAL || sysprobe(\"tcflush-calls\")!=0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-dup-fallback-failure-residue", "BEGIN {if(sysfallback::pipe(a,b)<0 || sysfallback::pipe(c,d)<0 || sysfallback::write(b,@b\"A\\nOLD\\n\")!=6 || sysfallback::read(a,x,8192,\"\\n\")!=2) exit 1;sysfallback::close(d);sysprobe(\"fail-setfd\");if(sysfallback::dup(c,a,sysfallback::O_CLOEXEC)!=sysfallback::RC_EINVAL) exit 1;sysprobe(\"clear-fcntl-failure\");if(sysfallback::read(a,x,8192)!=0) exit 1; }", HAWK_ENOERR, SYS | FD_FLAGS },
	{ "sys-opendir-flags-overflow-positive", "BEGIN {if(sys::opendir(\"anything\",4294967296)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-opendir-flags-overflow-negative", "BEGIN {if(sys::opendir(\"anything\",-4294967296)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, SYS | INT32_NARROW },
	{ "sys-opendir-byte-encoding-flag", SYS_DIR_ENCODING_FLAGS("\".\"", "1"), HAWK_ENOERR, 0 },
	{ "sys-opendir-wide-encoding-flag", SYS_DIR_ENCODING_FLAGS("\".\"", "2"), HAWK_ENOERR, 0 },
	{ "sys-opendir-both-encoding-flags", SYS_DIR_ENCODING_FLAGS("\".\"", "3"), HAWK_ENOERR, 0 },
	{ "sys-opendir-byte-input-encoding-flag", SYS_DIR_ENCODING_FLAGS("@b\".\"", "1"), HAWK_ENOERR, 0 },
	{ "sys-opendir-encoding-flag-before-path", SYS_DIR_ENCODING_FLAGS("\"/hawk-review-directory-that-does-not-exist\"", "1"), HAWK_ENOERR, 0 },
	{ "sys-opendir-default-entries", SYS_DIR_OPTIONS("p", "0"), HAWK_ENOERR, 0 },
	{ "sys-opendir-sorted-entries", SYS_DIR_OPTIONS("p", "sys::DIR_SORT"), HAWK_ENOERR, 0 },
	{ "sys-opendir-skip-special-entries", SYS_DIR_OPTIONS("p", "sys::DIR_SKIPSPCDIR"), HAWK_ENOERR, 0 },
	{ "sys-opendir-sorted-skip-special-entries", SYS_DIR_OPTIONS("p", "sys::DIR_SORT|sys::DIR_SKIPSPCDIR"), HAWK_ENOERR, 0 },
	{ "sys-opendir-byte-input-entries", SYS_DIR_OPTIONS("str::tombs(p)", "sys::DIR_SORT|sys::DIR_SKIPSPCDIR"), HAWK_ENOERR, 0 },
	{ "sys-tcsetattr-flags-upper-bound", "BEGIN {f=sysfault::open(\"/dev/null\",sysfault::O_RDWR);t[\"iflag\"]=4294967295;t[\"oflag\"]=4294967295;t[\"cflag\"]=4294967295;t[\"lflag\"]=4294967295;if(f<0 || sysfault::tcsetattr(f,0,t)!=0 || sysprobe(\"tcsetattr-calls\")!=1) exit 1; }", HAWK_ENOERR, SYS | TCFLAG32_NARROW },
	{ "sys-fchmod-negative-mode", "BEGIN {p=sys::tempname(\"hawk-mode-\");f=sys::open(p,sys::O_RDWR);n=sys::fchmod(f,-1);sys::close(f);sys::unlink(p);if(n!=sys::RC_EINVAL) exit 1; }", HAWK_ENOERR, SYS },
	{ "sys-win-open-mode-overflow", "BEGIN { if(sys::open(\"NUL\",sys::O_WRONLY,4294967296+0600)!=sys::RC_EINVAL || length(sys::errmsg())==0) exit 1; }", HAWK_ENOERR, WIN_SYS | INT32_NARROW }
};

/* Let the CRT return EINVAL for invalid formats rather than terminating this
 * test process in its default invalid-parameter handler. */
#if defined(_WIN32) && (defined(__MINGW32__) || (defined(_MSC_VER) && _MSC_VER >= 1400))
static void __cdecl invalid_parameter (const wchar_t* expr, const wchar_t* func, const wchar_t* file, unsigned int line, uintptr_t reserved)
{
}
#endif

static int run_case (const case_t* test, hawk_oow_t fail_after)
{
	allocator_t a = { 0 };
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &a };
	hawk_t* hawk = HAWK_NULL;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_parsestd_t input[2] = { { 0 } };
	hawk_fnc_bspec_t spec = { { 0, 0, HAWK_NULL }, arm, 0 };
	hawk_val_t* result;
	hawk_bitmask_t trait;
	hawk_int_t status;
	hawk_errnum_t errnum;
	allocation_t* item;
	hawk_oow_t leaked = 0;
	int ok = 0;

	if ((test->flags & SYS) && hawk_test_sys_begin() != 0)
	{
		puts("requires POSIX descriptor failure fixtures");
		return 77;
	}
	if ((test->flags & NATIVE_KILL) && hawk_test_sys_native_kill_begin() != 0) return 77;
	a.fail_after = fail_after;
	a.large_io_buffers = (test->flags & LARGE_IO) != 0;
	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) goto done;
	if (test->flags & STRICTMAP)
	{
		hawk_getopt(hawk, HAWK_OPT_TRAIT, &trait);
		trait &= ~HAWK_FLEXMAP;
		hawk_setopt(hawk, HAWK_OPT_TRAIT, &trait);
	}
	if (hawk_addstaticmodwithbcstr(hawk, "randtest", hawk_test_mod_math) <= -1) goto done;
	if (hawk_addstaticmodwithbcstr(hawk, "timetest", hawk_test_mod_math_time) <= -1) goto done;
	if (hawk_addstaticmodwithbcstr(hawk, "sysfallback", hawk_test_mod_sys_fallback) <= -1) goto done;
	if (hawk_addstaticmodwithbcstr(hawk, "sysfault", hawk_test_mod_sys_fault) <= -1) goto done;
	if (hawk_addstaticmodwithbcstr(hawk, "sysunavailable", hawk_test_mod_sys_unavailable) <= -1) goto done;
	if (!hawk_addfncwithbcstr(hawk, "arm", &spec)) goto done;
	spec.impl = bob;
	if (!hawk_addfncwithbcstr(hawk, "bob", &spec)) goto done;
	spec.impl = minint;
	if (!hawk_addfncwithbcstr(hawk, "minint", &spec)) goto done;
	spec.impl = maxint;
	if (!hawk_addfncwithbcstr(hawk, "maxint", &spec)) goto done;
	spec.impl = intsize;
	if (!hawk_addfncwithbcstr(hawk, "intsize", &spec)) goto done;
	spec.impl = piowait;
	if (!hawk_addfncwithbcstr(hawk, "piowait", &spec)) goto done;
	spec.arg.min = spec.arg.max = 1;
	spec.impl = hawk_test_sys_probe;
	if (!hawk_addfncwithbcstr(hawk, "sysprobe", &spec)) goto done;
	spec.impl = clobber_free_errno;
	if (!hawk_addfncwithbcstr(hawk, "clobber_free_errno", &spec)) goto done;
	spec.impl = oversized_mbs;
	if (!hawk_addfncwithbcstr(hawk, "oversized_mbs", &spec)) goto done;
	spec.impl = toint;
	if (!hawk_addfncwithbcstr(hawk, "toint", &spec)) goto done;
	spec.arg.min = spec.arg.max = 4;
	spec.arg.spec = "rvrv";
	spec.impl = setpair;
	if (!hawk_addfncwithbcstr(hawk, "setpair", &spec)) goto done;
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)test->source;
	input[0].u.bcs.len = hawk_count_bcstr(test->source);
	if (hawk_parsestd(hawk, input, HAWK_NULL) < 0)
	{
		fprintf(stderr, "parse: %s\n", hawk_geterrbmsg(hawk));
		goto done;
	}
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("fnc-regress"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) goto done;
	if (test->flags & MB8) hawk_rtx_setcmgr(rtx, hawk_get_cmgr_by_id(HAWK_CMGR_MB8));
	result = hawk_rtx_loop(rtx);
	errnum = hawk_rtx_geterrnum(rtx);
	if (result)
	{
		ok = test->expected_error == HAWK_ENOERR && hawk_rtx_valtoint(rtx, result, &status) == 0 && status == 0;
		/* A recovered allocation failure or nonfatal close error can leave errnum set. */
		if (!ok) fprintf(stderr, "unexpected success or wrong result: %s\n", hawk_rtx_geterrbmsg(rtx));
		hawk_rtx_refdownval(rtx, result);
	}
	else
	{
		ok = (test->expected_error != HAWK_ENOERR && errnum == test->expected_error) ||
		     (a.failed && errnum == HAWK_ENOMEM);
		if (!ok) fprintf(stderr, "unexpected runtime error %d: %s\n", (int)errnum, hawk_rtx_geterrbmsg(rtx));
	}

done:
	a.pair_rtx = HAWK_NULL;
	if (a.oversized_mbs)
	{
		((hawk_val_mbs_t*)a.oversized_mbs)->val.len = 1;
		hawk_rtx_refdownval(rtx, a.oversized_mbs);
	}
	if (rtx) hawk_rtx_close(rtx);
	if (hawk) hawk_close(hawk);
	if ((test->flags & SYS) && hawk_test_sys_end() <= -1) ok = 0;
	if ((test->flags & NATIVE_KILL) && hawk_test_sys_native_kill_end() <= -1) ok = 0;
	if (a.invalid_frees || a.invalid_io_allocation || a.pair_observer_error) ok = 0;
	/* Live allocations after close expose leaked comparator return values. */
	while (a.allocations)
	{
		item = a.allocations;
		a.allocations = item->next;
		leaked++;
		free(item->ptr);
		free(item);
	}
	if (leaked)
	{
		fprintf(stderr, "%lu allocations leaked after runtime/interpreter close\n", (unsigned long)leaked);
		ok = 0;
	}
	if (!ok) fprintf(stderr, "case %s, failure offset %lu\nsource: %s\n", test->name, (unsigned long)fail_after, test->source);
	return ok? 0: 1;
}

int main (int argc, char** argv)
{
	hawk_oow_t i, offset;
	int status;

#if defined(_WIN32) && (defined(__MINGW32__) || (defined(_MSC_VER) && _MSC_VER >= 1400))
	_set_invalid_parameter_handler(invalid_parameter);
#endif
#if defined(_WIN32)
	if (argc == 2 && hawk_comp_bcstr(argv[1], "--kill-child", 0) == 0)
	{
		Sleep(INFINITE);
		return 1;
	}
#endif

	if (argc == 2 && hawk_comp_bcstr(argv[1], "--list", 0) == 0)
	{
		for (i = 0; i < HAWK_COUNTOF(cases); i++) puts(cases[i].name);
		return 0;
	}
	if (argc != 2 && argc != 3)
	{
		fprintf(stderr, "usage: %s --list | CASE [failure-offset]\n", argv[0]);
		return 2;
	}
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		if (hawk_comp_bcstr(argv[1], cases[i].name, 0) != 0) continue;
#if !defined(_WIN32) && !defined(__OS2__)
		if (cases[i].flags & NATIVE_KILL) { puts("requires Windows or OS/2"); return 77; }
#endif
#if !defined(_WIN32)
		if (cases[i].flags & WIN_KILL) { puts("requires Windows"); return 77; }
#endif
#if !defined(__OS2__)
		if (cases[i].flags & OS2_KILL) { puts("requires OS/2"); return 77; }
#endif
#if defined(HAWK_OOCH_IS_BCH)
		if (cases[i].flags & MB8) { puts("requires a wide-character build"); return 77; }
		if (cases[i].flags & WIDE_CHAR) { puts("requires a wide-character build"); return 77; }
#endif
#if !defined(HAWK_ENABLE_GC)
		if (cases[i].flags & GC) { puts("requires garbage collection"); return 77; }
#endif
#if !defined(HAWK_USE_FLTMAX) || !defined(HAWK_FLTMAX_REQUIRE_QUADMATH) || !defined(HAVE_LOG2Q)
		if (cases[i].flags & QUAD) { puts("requires quad precision and log2q()"); return 77; }
#endif
		if ((cases[i].flags & OFFSET32_NARROW) && !hawk_test_sys_offset32_narrow())
		{
			puts("requires 32-bit off_t and a wider Hawk integer");
			return 77;
		}
		if ((cases[i].flags & SEEK_WIDE_RESULT) &&
		#if defined(_WIN32)
		    HAWK_SIZEOF(hawk_int_t) >= 8)
		#else
		    HAWK_SIZEOF(hawk_int_t) >= HAWK_SIZEOF_OFF_T)
		#endif
		{
			puts("requires a native seek result wider than Hawk's integer");
			return 77;
		}
		if ((cases[i].flags & OWNER32_NARROW) && !hawk_test_sys_owner32_narrow())
		{
			puts("requires unsigned 32-bit uid_t/gid_t and a wider Hawk integer");
			return 77;
		}
		if ((cases[i].flags & MODE32_NARROW) && !hawk_test_sys_mode32_narrow())
		{
			puts("requires a file mode of at most 32 bits and a wider Hawk integer");
			return 77;
		}
		if ((cases[i].flags & TCFLAG32_NARROW) && !hawk_test_sys_tcflag32_narrow())
		{
			puts("requires 32-bit terminal flags and a wider Hawk integer");
			return 77;
		}
		if ((cases[i].flags & INT32_NARROW) && (HAWK_SIZEOF(int) != 4 || HAWK_SIZEOF(hawk_int_t) <= 4))
		{
			puts("requires 32-bit C int and a wider Hawk integer");
			return 77;
		}
		if ((cases[i].flags & PID32_NARROW) && !hawk_test_sys_pid32_narrow())
		{
			puts("requires 32-bit pid_t and a wider Hawk integer");
			return 77;
		}
		if ((cases[i].flags & FD_FLAGS) && !hawk_test_sys_fd_flags_available())
		{
			puts("requires nonblocking and close-on-exec descriptor/socket flags");
			return 77;
		}
#if !defined(HAVE_SYS_EPOLL_H) || !defined(HAVE_EPOLL_CREATE)
		if (cases[i].flags & EPOLL) { puts("requires epoll"); return 77; }
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_UN <= 0)
		if (cases[i].flags & UNIX_SOCKET) { puts("requires Unix-domain sockets"); return 77; }
#endif
#if !defined(HAVE_NANOSLEEP) && !defined(HAVE_SELECT)
		if (cases[i].flags & SLEEP_OS) { puts("requires nanosleep() or select()"); return 77; }
#endif
		if (cases[i].flags & THREADS) return hawk_test_math_threads(cases[i].source);
		if (argc == 3) return run_case(&cases[i], (hawk_oow_t)strtoul(argv[2], HAWK_NULL, 10));
		for (offset = 0; offset <= ((cases[i].flags & DEEP_SWEEP)? 128: (cases[i].flags & SWEEP)? 32: 0); offset++)
		{
			status = run_case(&cases[i], offset);
			if (status != 0) return status;
		}
		return 0;
	}
	fprintf(stderr, "unknown case: %s\n", argv[1]);
	return 2;
}
