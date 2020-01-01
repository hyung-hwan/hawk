/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#include <hawk-mtx.h>
#include "hawk-prv.h"

#if (!defined(__unix__) && !defined(__unix)) || defined(HAVE_PTHREAD)

#if defined(_WIN32)
#	include <windows.h>
#	include <process.h>

#elif defined(__OS2__)
#	define INCL_DOSSEMAPHORES
#	define INCL_DOSERRORS
#	include <os2.h>

#elif defined(__DOS__)
	/* implement this */

#else
#	include "syscall.h"
#	if defined(AIX) && defined(__GNUC__)
		typedef int crid_t;
		typedef unsigned int class_id_t;
#	endif
#	include <pthread.h>
#endif

hawk_mtx_t* hawk_mtx_open (hawk_gem_t* gem, hawk_oow_t xtnsize)
{
	hawk_mtx_t* mtx;

	mtx = (hawk_mtx_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_mtx_t) + xtnsize);
	if (mtx)
	{
		if (hawk_mtx_init(mtx, gem) <= -1)
		{
			hawk_gem_freemem (gem, mtx);
			return HAWK_NULL;
		}
		else HAWK_MEMSET (mtx + 1, 0, xtnsize);
	}

	return mtx;
}

void hawk_mtx_close (hawk_mtx_t* mtx)
{
	hawk_mtx_fini (mtx);
	hawk_gem_freemem (mtx->gem, mtx);
}

int hawk_mtx_init (hawk_mtx_t* mtx, hawk_gem_t* gem)
{
	HAWK_MEMSET (mtx, 0, HAWK_SIZEOF(*mtx));
	mtx->gem = gem;

#if defined(_WIN32)
	mtx->hnd = CreateMutex(HAWK_NULL, FALSE, HAWK_NULL);
	if (mtx->hnd == HAWK_NULL) 
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

#elif defined(__OS2__)

	{
		APIRET rc;
		HMTX m;

		rc = DosCreateMutexSem(HAWK_NULL, &m, DC_SEM_SHARED, FALSE);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			return -1;
		}

		mtx->hnd = m;
	}

#elif defined(__DOS__)
#	error not implemented

#else
	/*
	hawk_ensure (pthread_mutexattr_init (&attr) == 0);
	if (pthread_mutexattr_settype (&attr, type) != 0) 
	{
		int num = hawk_geterrno();
		pthread_mutexattr_destroy (&attr);
		if (mtx->__dynamic) hawk_free (mtx);
		hawk_seterrno (num);
		return HAWK_NULL;
	}
	hawk_ensure (pthread_mutex_init (&mtx->hnd, &attr) == 0);
	hawk_ensure (pthread_mutexattr_destroy (&attr) == 0);
	*/
	{
		int n;
		n = pthread_mutex_init((pthread_mutex_t*)&mtx->hnd, HAWK_NULL);
		if (n != 0)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
			return -1;
		}
	}
#endif

	return 0;
}

void hawk_mtx_fini (hawk_mtx_t* mtx)
{
#if defined(_WIN32)
	CloseHandle (mtx->hnd);

#elif defined(__OS2__)
	DosCloseMutexSem (mtx->hnd);

#elif defined(__DOS__)
#	error not implemented

#else
	pthread_mutex_destroy ((pthread_mutex_t*)&mtx->hnd);
#endif
}

int hawk_mtx_lock (hawk_mtx_t* mtx, const hawk_ntime_t* waiting_time)
{
#if defined(_WIN32)
	/* 
	 * MSDN
	 *   WAIT_ABANDONED The specified object is a mutex object that was 
	 *                  not released by the thread that owned the mutex 
	 *                  object before the owning thread terminated. 
	 *                  Ownership of the mutex object is granted to the 
	 *                  calling thread, and the mutex is set to nonsignaled.
	 *   WAIT_OBJECT_0  The state of the specified object is signaled. 
	 *   WAIT_TIMEOUT   The time-out interval elapsed, and the object's 
	 *                  state is nonsignaled. 
	 *   WAIT_FAILED    An error occurred
	 */
	if (waiting_time)
	{
		DWORD msec, ret;
		msec = HAWK_SECNSEC_TO_MSEC(waiting_time->sec, waiting_time->nsec);
		ret = WaitForSingleObject(mtx->hnd, msec);
		if (ret == WAIT_TIMEOUT)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, HAWK_ETMOUT);
			return -1;
		}
		else if (ret == WAIT_ABANDONED)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, HAWK_ESYSERR);
			return -1;
		}
		else if (ret != WAIT_OBJECT_0)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			return -1;
		}
	}
	else
	{
		if (WaitForSingleObject(mtx->hnd, INFINITE) == WAIT_FAILED) 
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			return -1;
		}
	}

#elif defined(__OS2__)
	if (waiting_time)
	{
		ULONG msec;
		APIRET rc;
		msec = HAWK_SECNSEC_TO_MSEC(waiting_time->sec, waiting_time->nsec);
		rc = DosRequestMutexSem(mtx->hnd, msec);
		if (rc != NO_ERROR)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			return -1;
		}
	}
	else
	{
		APIRET rc;
		rc = DosRequestMutexSem(mtx->hnd, SEM_INDEFINITE_WAIT);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			return -1;
		}
	}

#elif defined(__DOS__)

	/* nothing to do */

#else

	/* if pthread_mutex_timedlock() isn't available, don't honor the waiting time. */
	#if defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
	if (waiting_time)
	{
		hawk_ntime_t t;
		struct timespec ts;
		int n;

		hawk_get_time (&t);
		hawk_add_time (&t, waiting_time, &t);

		ts.tv_sec = t.sec;
		ts.tv_nsec = t.nsec;
		n = pthread_mutex_timedlock((pthread_mutex_t*)&mtx->hnd, &ts);
		if (n != 0)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
			return -1;
		}
	}
	else
	{
	#endif
		int n;
		n = pthread_mutex_lock((pthread_mutex_t*)&mtx->hnd);
		if (n != 0)
		{
			hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
			return -1;
		}
	#if defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
	}
	#endif
#endif

	return 0;
}

int hawk_mtx_unlock (hawk_mtx_t* mtx)
{
#if defined(_WIN32)
	if (ReleaseMutex(mtx->hnd) == FALSE) 
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

#elif defined(__OS2__)
	APIRET rc;
	rc = DosReleaseMutexSem(mtx->hnd);
	if (rc != NO_ERROR) 
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

#elif defined(__DOS__)

	/* nothing to do */

#else
	int n;
	n = pthread_mutex_unlock((pthread_mutex_t*)&mtx->hnd);
	if (n != 0) 
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
		return -1;
	}
#endif
	return 0;
}

int hawk_mtx_trylock (hawk_mtx_t* mtx)
{
#if defined(_WIN32)
	if (WaitForSingleObject(mtx->hnd, 0) != WAIT_OBJECT_0) 
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}
#elif defined(__OS2__)
	APIRET rc;
	rc = DosRequestMutexSem(mtx->hnd, 0);
	if (rc != NO_ERROR) 
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

#elif defined(__DOS__)
	/* nothing to do */

#else

	/* -------------------------------------------------- */
	int n;
	#if defined(HAVE_PTHREAD_MUTEX_TRYLOCK)
	n = pthread_mutex_trylock((pthread_mutex_t*)&mtx->hnd);
	#elif defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
	hawk_ntime_t t;
	struct timespec ts;
	hawk_get_time (&t);
	ts.tv_sec = t.sec;
	ts.tv_nsec = t.nsec;
	n = pthread_mutex_timedlock((pthread_mutex_t*)&mtx->hnd, &ts);
	#else
	/* not supported. fallback to normal pthread_mutex_lock(). <--- is this really desirable? */
	n = pthread_mutex_lock((pthread_mutex_t*)&mtx->hnd);
	#endif
	if (n != 0)
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
		return -1;
	}
	/* -------------------------------------------------- */

#endif
	return 0;
}

#endif
