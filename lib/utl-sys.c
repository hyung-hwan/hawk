/*
    Copyright (c) 2006-2020 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hawk-utl.h>
#include <hawk-gem.h>
#include "err-prv.h"

#if defined(_WIN32)
#	include <windows.h>
#	include <time.h>
#	define EPOCH_DIFF_YEARS (HAWK_EPOCH_YEAR - HAWK_EPOCH_YEAR_WIN)
#	define EPOCH_DIFF_DAYS  ((hawk_intptr_t)EPOCH_DIFF_YEARS * 365 + EPOCH_DIFF_YEARS / 4 - 3)
#	define EPOCH_DIFF_SECS  ((hawk_intptr_t)EPOCH_DIFF_DAYS * 24 * 60 * 60)
#elif defined(__OS2__)
#	define INCL_DOSDATETIME
#	define INCL_DOSERRORS
#	include <os2.h>
#	include <time.h>
#elif defined(__DOS__)
#	include <dos.h>
#	include <time.h>
#else
#	if !defined(_GNU_SOURCE)
#		define _GNU_SOURCE
#	endif
#	include "syscall.h"
#	if defined(HAVE_SYS_TIME_H)
#		include <sys/time.h>
#	endif
#	if defined(HAVE_TIME_H)
#		include <time.h>
#	endif
#endif


/* ----------------------------------------------------------------------- */

int hawk_get_ntime (hawk_ntime_t* t)
{
#if defined(_WIN32)
	SYSTEMTIME st;
	FILETIME ft;
	ULARGE_INTEGER li;

	/*
	 * MSDN: The FILETIME structure is a 64-bit value representing the
	 *       number of 100-nanosecond intervals since January 1, 1601 (UTC).
	 */

	GetSystemTime (&st);
	if (SystemTimeToFileTime (&st, &ft) == FALSE) return -1;

	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;

	/* li.QuadPart is in the 100-nanosecond intervals */
	t->sec = (li.QuadPart / (HAWK_NSECS_PER_SEC / 100)) - EPOCH_DIFF_SECS;
	t->nsec = (li.QuadPart % (HAWK_NSECS_PER_SEC / 100)) * 100;

	return 0;

#elif defined(__OS2__)

	APIRET rc;
	DATETIME dt;
	hawk_btime_t bt;

	/* Can I use DosQuerySysInfo(QSV_TIME_LOW) and
	 * DosQuerySysInfo(QSV_TIME_HIGH) for this instead?
	 * Maybe, resolution too low as it returns values
	 * in seconds. */

	rc = DosGetDateTime (&dt);
	if (rc != NO_ERROR) return -1;

	bt.year = dt.year - HAWK_BTIME_YEAR_BASE;
	bt.mon = dt.month - 1;
	bt.mday = dt.day;
	bt.hour = dt.hours;
	bt.min = dt.minutes;
	bt.sec = dt.seconds;
	/*bt.msec = dt.hundredths * 10;*/
	bt.isdst = -1; /* determine dst for me */

	if (hawk_timelocal(&bt, t) <= -1) return -1;
	t->nsec = HAWK_MSEC_TO_NSEC(dt.hundredths * 10);
	return 0;

#elif defined(__DOS__)

	struct dostime_t dt;
	struct dosdate_t dd;
	hawk_btime_t bt;

	_dos_gettime (&dt);
	_dos_getdate (&dd);

	bt.year = dd.year - HAWK_BTIME_YEAR_BASE;
	bt.mon = dd.month - 1;
	bt.mday = dd.day;
	bt.hour = dt.hour;
	bt.min = dt.minute;
	bt.sec = dt.second;
	/*bt.msec = dt.hsecond * 10; */
	bt.isdst = -1; /* determine dst for me */

	if (hawk_timelocal(&bt, t) <= -1) return -1;
	t->nsec = HAWK_MSEC_TO_NSEC(dt.hsecond * 10);
	return 0;

#elif defined(macintosh)
	unsigned long tv;

	GetDateTime (&tv);

	t->sec = tv;
	tv->nsec = 0;

	return 0;

#elif defined(HAVE_CLOCK_GETTIME)
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) == -1) return -1;

	t->sec = ts.tv_sec;
	t->nsec = ts.tv_nsec;
	return 0;

#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval tv;
	int n;

	n = HAWK_GETTIMEOFDAY(&tv, HAWK_NULL);
	if (n == -1) return -1;

	t->sec = tv.tv_sec;
	t->nsec = HAWK_USEC_TO_NSEC(tv.tv_usec);
	return 0;

#else
	t->sec = time(HAWK_NULL);
	t->nsec = 0;

	return 0;
#endif
}

int hawk_set_ntime (const hawk_ntime_t* t)
{
#if defined(_WIN32)
	FILETIME ft;
	SYSTEMTIME st;

	/**((hawk_int64_t*)&ft) = ((t + EPOCH_DIFF_MSECS) * (10 * 1000));*/
	*((hawk_int64_t*)&ft) = (HAWK_SEC_TO_NSEC(t->sec + EPOCH_DIFF_SECS) / 100)  + (t->nsec / 100);
	if (FileTimeToSystemTime(&ft, &st) == FALSE) return -1;
	if (SetSystemTime(&st) == FALSE) return -1;
	return 0;

#elif defined(__OS2__)

	APIRET rc;
	DATETIME dt;
	hawk_btime_t bt;

	if (hawk_localtime (t, &bt) <= -1) return -1;

	HAWK_MEMSET(&dt, 0, HAWK_SIZEOF(dt));
	dt.year = bt.year + HAWK_BTIME_YEAR_BASE;
	dt.month = bt.mon + 1;
	dt.day = bt.mday;
	dt.hours = bt.hour;
	dt.minutes = bt.min;
	dt.seconds = bt.sec;
	dt.hundredths = HAWK_NSEC_TO_MSEC(t->nsec) / 10;

	rc = DosSetDateTime(&dt);
	return (rc != NO_ERROR)? -1: 0;

#elif defined(__DOS__)

	struct dostime_t dt;
	struct dosdate_t dd;
	hawk_btime_t bt;

	if (hawk_localtime(t, &bt) <= -1) return -1;

	dd.year = bt.year + HAWK_BTIME_YEAR_BASE;
	dd.month = bt.mon + 1;
	dd.day = bt.mday;
	dt.hour = bt.hour;
	dt.minute = bt.min;
	dt.second = bt.sec;
	dt.hsecond = HAWK_NSEC_TO_MSEC(t->nsec) / 10;

	if (_dos_settime(&dt) != 0) return -1;
	if (_dos_setdate(&dd) != 0) return -1;

	return 0;

#elif defined(HAVE_SETTIMEOFDAY)
	struct timeval tv;
	int n;

	tv.tv_sec = t->sec;
	tv.tv_usec = HAWK_NSEC_TO_USEC(t->nsec);

/*
#if defined(CLOCK_REALTIME) && defined(HAVE_CLOCK_SETTIME)
	{
		int r = clock_settime(CLOCK_REALTIME, ts);
		if (r == 0 || errno == EPERM) return r;
	}
#elif defined(HAVE_STIME)
	return stime (&ts->tv_sec);
#else
*/
	n = HAWK_SETTIMEOFDAY(&tv, HAWK_NULL);
	if (n == -1) return -1;
	return 0;

#else

	time_t tv;
	tv = t->sec;
	return (HAWK_STIME(&tv) == -1)? -1: 0;
#endif
}

void hawk_add_ntime (hawk_ntime_t* z, const hawk_ntime_t* x, const hawk_ntime_t* y)
{
	hawk_ntime_sec_t xs, ys;
	hawk_ntime_nsec_t ns;

	HAWK_ASSERT (x->nsec >= 0 && x->nsec < HAWK_NSECS_PER_SEC);
	HAWK_ASSERT (y->nsec >= 0 && y->nsec < HAWK_NSECS_PER_SEC);

	ns = x->nsec + y->nsec;
	if (ns >= HAWK_NSECS_PER_SEC)
	{
		ns = ns - HAWK_NSECS_PER_SEC;
		if (x->sec == HAWK_TYPE_MAX(hawk_ntime_sec_t))
		{
			if (y->sec >= 0) goto overflow;
			xs = x->sec;
			ys = y->sec + 1; /* this won't overflow */
		}
		else
		{
			xs = x->sec + 1; /* this won't overflow */
			ys = y->sec;
		}
	}
	else
	{
		xs = x->sec;
		ys = y->sec;
	}

	if ((ys >= 1 && xs > HAWK_TYPE_MAX(hawk_ntime_sec_t) - ys) ||
	    (ys <= -1 && xs < HAWK_TYPE_MIN(hawk_ntime_sec_t) - ys))
	{
		if (xs >= 0)
		{
		overflow:
			xs = HAWK_TYPE_MAX(hawk_ntime_sec_t);
			ns = HAWK_NSECS_PER_SEC - 1;
		}
		else
		{
			xs = HAWK_TYPE_MIN(hawk_ntime_sec_t);
			ns = 0;
		}
	}
	else
	{
		xs = xs + ys;
	}

	z->sec = xs;
	z->nsec = ns;
}

void hawk_sub_ntime (hawk_ntime_t* z, const hawk_ntime_t* x, const hawk_ntime_t* y)
{
	hawk_ntime_sec_t xs, ys;
	hawk_ntime_nsec_t ns;

	HAWK_ASSERT (x->nsec >= 0 && x->nsec < HAWK_NSECS_PER_SEC);
	HAWK_ASSERT (y->nsec >= 0 && y->nsec < HAWK_NSECS_PER_SEC);

	ns = x->nsec - y->nsec;
	if (ns < 0)
	{
		ns = ns + HAWK_NSECS_PER_SEC;
		if (x->sec == HAWK_TYPE_MIN(hawk_ntime_sec_t))
		{
			if (y->sec <= 0) goto underflow;
			xs = x->sec;
			ys = y->sec - 1; /* this won't underflow */
		}
		else
		{
			xs = x->sec - 1; /* this won't underflow */
			ys = y->sec;
		}
	}
	else
	{
		xs = x->sec;
		ys = y->sec;
	}

	if ((ys >= 1 && xs < HAWK_TYPE_MIN(hawk_ntime_sec_t) + ys) ||
	    (ys <= -1 && xs > HAWK_TYPE_MAX(hawk_ntime_sec_t) + ys))
	{
		if (xs >= 0)
		{
			xs = HAWK_TYPE_MAX(hawk_ntime_sec_t);
			ns = HAWK_NSECS_PER_SEC - 1;
		}
		else
		{
		underflow:
			xs = HAWK_TYPE_MIN(hawk_ntime_sec_t);
			ns = 0;
		}
	}
	else
	{
		xs = xs - ys;
	}

	z->sec = xs;
	z->nsec = ns;
}

/* ----------------------------------------------------------------------- */

static const hawk_bch_t* get_temp_dir_from_env (hawk_temp_dir_getenv_t getenv_cb, void* ctx)
{
	static const hawk_bch_t* names[] = { "TMPDIR", "TEMP", "TMP" };
	const hawk_bch_t* dir;
	hawk_oow_t i;

	for (i = 0; getenv_cb && i < HAWK_COUNTOF(names); i++)
	{
		dir = getenv_cb(names[i], ctx);
		if (dir && dir[0]) return dir;
	}

#if defined(_WIN32)
	return HAWK_NULL;
#elif defined(__OS2__) || defined(__DOS__)
	return ".";
#elif defined(vms) || defined(__vms)
	return "SYS$SCRATCH:";
#elif defined(P_tmpdir)
	return P_tmpdir;
#else
	return "/tmp";
#endif
}

#if defined(_WIN32)
static void* get_native_temp_dir (hawk_gem_t* gem, int wide)
{
	DWORD capa = MAX_PATH + 1, n, e;
	hawk_oow_t unitsize = wide? HAWK_SIZEOF(WCHAR): HAWK_SIZEOF(hawk_bch_t);
	void* dir;

	while (1)
	{
		if ((hawk_oow_t)capa > HAWK_TYPE_MAX(hawk_oow_t) / unitsize) goto too_long;

		dir = hawk_gem_allocmem(gem, (hawk_oow_t)capa * unitsize);
		if (HAWK_UNLIKELY(!dir)) return HAWK_NULL;

		n = wide? GetTempPathW(capa, (WCHAR*)dir): GetTempPathA(capa, (hawk_bch_t*)dir);
		if (n == 0)
		{
			e = GetLastError();
			hawk_gem_freemem(gem, dir);
			hawk_gem_seterrbfmt(gem, HAWK_NULL, hawk_syserr_to_errnum(e), "unable to get the temporary directory (OS error %lu)", (unsigned long)e);
			return HAWK_NULL;
		}
		if (n < capa) return dir;

		hawk_gem_freemem(gem, dir);
		if (n == HAWK_TYPE_MAX(DWORD)) goto too_long;
		capa = n + 1;
	}

too_long:
	hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_EINVAL, "temporary directory path too long");
	return HAWK_NULL;
}
#endif

hawk_bch_t* hawk_get_temp_dir_bcstr (hawk_gem_t* gem, hawk_temp_dir_getenv_t getenv_cb, void* ctx)
{
	const hawk_bch_t* dir;

	dir = get_temp_dir_from_env(getenv_cb, ctx);
#if defined(_WIN32)
	if (!dir) return (hawk_bch_t*)get_native_temp_dir(gem, 0);
#endif
	return hawk_gem_dupbcstr(gem, dir, HAWK_NULL);
}

hawk_uch_t* hawk_get_temp_dir_ucstr (hawk_gem_t* gem, hawk_temp_dir_getenv_t getenv_cb, void* ctx)
{
	const hawk_bch_t* dir;

	dir = get_temp_dir_from_env(getenv_cb, ctx);
#if defined(_WIN32)
	if (!dir)
	{
		WCHAR* native;

		native = get_native_temp_dir(gem, 1);
		if (!native) return HAWK_NULL;

	#if HAWK_SIZEOF_UCH_T == 2
		return (hawk_uch_t*)native;
	#else
		{
			hawk_oow_t len = 0;
			hawk_uch_t* result;

			while (native[len]) len++;
			result = hawk_gem_dupbtoucharswithcmgr(gem, (const hawk_bch_t*)native, len * HAWK_SIZEOF(*native), HAWK_NULL, hawk_get_utf16_cmgr(), 0);
			hawk_gem_freemem(gem, native);
			return result;
		}
	#endif
	}
#endif
	return hawk_gem_dupbtoucstr(gem, dir, HAWK_NULL, 0);
}
