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

#include <hawk-mtx.h>
#include "hawk-prv.h"

#if (!defined(__unix__) && !defined(__unix)) || defined(HAVE_PTHREAD)

#if defined(_WIN32)
#	include <windows.h>
#	include <process.h>

#elif defined(__OS2__)
#	define INCL_DOSSEMAPHORES
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>

#elif defined(__DOS__)
	/* implement this */

#else
#	if !defined(_GNU_SOURCE)
#		define _GNU_SOURCE
#	endif
#	include "syscall.h"
#	if defined(AIX) && defined(__GNUC__)
		typedef int crid_t;
		typedef unsigned int class_id_t;
#	endif
#	include <pthread.h>
#	if defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER) || defined(PTHREAD_MUTEX_RECURSIVE)
#		define HAWK_PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE
#	elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP) || defined(PTHREAD_MUTEX_RECURSIVE_NP)
#		define HAWK_PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#	endif
#endif

hawk_mtx_t* hawk_mtx_open (hawk_gem_t* gem, hawk_oow_t xtnsize, int flags)
{
	hawk_mtx_t* mtx;

	mtx = (hawk_mtx_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_mtx_t) + xtnsize);
	if (mtx)
	{
		if (hawk_mtx_init(mtx, gem, flags) <= -1)
		{
			hawk_gem_freemem(gem, mtx);
			return HAWK_NULL;
		}
		else HAWK_MEMSET(mtx + 1, 0, xtnsize);
	}

	return mtx;
}

void hawk_mtx_close (hawk_mtx_t* mtx)
{
	hawk_mtx_fini(mtx);
	hawk_gem_freemem(mtx->gem, mtx);
}

int hawk_mtx_init (hawk_mtx_t* mtx, hawk_gem_t* gem, int flags)
{
	HAWK_MEMSET(mtx, 0, HAWK_SIZEOF(*mtx));
	mtx->gem = gem;
	mtx->flags = flags;

#if defined(_WIN32)
	mtx->hnd = CreateMutex(HAWK_NULL, FALSE, HAWK_NULL);
	if (mtx->hnd == HAWK_NULL)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

#elif defined(__OS2__)

	{
		APIRET rc;
		HMTX m;

		rc = DosCreateMutexSem(HAWK_NULL, &m, DC_SEM_SHARED, FALSE);
		if (rc != NO_ERROR)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			return -1;
		}

		mtx->hnd = m;
	}

#elif defined(__DOS__)
	/* nothing to implement */
#else

	#if (HAWK_SIZEOF_PTHREAD_MUTEX_T <= 0)
		/* nothing to initialize as there is no actual mutex support */
	#else
	{
		int n;
		pthread_mutexattr_t attr;

		pthread_mutexattr_init(&attr);
	#if defined(HAWK_PTHREAD_MUTEX_RECURSIVE)
		if (flags & HAWK_MTX_FLAG_RECURSIVE)
			pthread_mutexattr_settype(&attr, HAWK_PTHREAD_MUTEX_RECURSIVE);
		/* TODO: implement recursive mutex where it's not supported natively */
	#endif
		n = pthread_mutex_init((pthread_mutex_t*)&mtx->hnd, &attr);
		pthread_mutexattr_destroy(&attr);
		if (n != 0)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
			return -1;
		}
	}
	#endif
#endif

	return 0;
}

void hawk_mtx_fini (hawk_mtx_t* mtx)
{
#if defined(_WIN32)
	CloseHandle(mtx->hnd);

#elif defined(__OS2__)
	DosCloseMutexSem(mtx->hnd);

#elif defined(__DOS__)
	/* nothing to destroy as there is no mutex support */

#else
	#if (HAWK_SIZEOF_PTHREAD_MUTEX_T <= 0)
	/* nothing to destroy as there is no actual mutex support */
	#else
	pthread_mutex_destroy((pthread_mutex_t*)&mtx->hnd);
	#endif
#endif
}


#if defined(__OS2__)
static HAWK_INLINE hawk_uintptr_t os2_get_current_tid (void)
{
	PTIB ptib;
	PPIB ppib;
	if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR && ptib && ptib->tib_ptib2)
		return (hawk_uintptr_t)ptib->tib_ptib2->tib2_ultid;
	return 0;
}
#endif

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
	hawk_uintptr_t tid;

	tid = GetCurrentThreadId();
	if (!(mtx->flags & HAWK_MTX_FLAG_RECURSIVE) && mtx->owner_tid == tid && mtx->owner_count > 0)
	{
		hawk_gem_seterrbfmt(mtx->gem, HAWK_NULL, HAWK_EBUSY, "multiple lock attempts on non-recursive mutex");
		return -1;
	}

	if (waiting_time)
	{
		DWORD msec, ret;
		msec = HAWK_SECNSEC_TO_MSEC(waiting_time->sec, waiting_time->nsec);
		ret = WaitForSingleObject(mtx->hnd, msec);
		if (ret == WAIT_TIMEOUT)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, HAWK_ETMOUT);
			return -1;
		}
		else if (ret == WAIT_ABANDONED)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, HAWK_ESYSERR);
			return -1;
		}
		else if (ret != WAIT_OBJECT_0)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			return -1;
		}
	}
	else
	{
		if (WaitForSingleObject(mtx->hnd, INFINITE) == WAIT_FAILED)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			return -1;
		}
	}

	if (mtx->owner_tid == tid) { mtx->owner_count++; }
	else { mtx->owner_tid = tid; mtx->owner_count = 1; }

#elif defined(__OS2__)
	hawk_uintptr_t tid;

	tid = os2_get_current_tid();
	if (!(mtx->flags & HAWK_MTX_FLAG_RECURSIVE) && mtx->owner_tid == tid && mtx->owner_count > 0)
	{
		hawk_gem_seterrbfmt(mtx->gem, HAWK_NULL, HAWK_EBUSY, "multiple lock attempts on non-recursive mutex");
		return -1;
	}

	if (waiting_time)
	{
		ULONG msec;
		APIRET rc;
		msec = HAWK_SECNSEC_TO_MSEC(waiting_time->sec, waiting_time->nsec);
		rc = DosRequestMutexSem(mtx->hnd, msec);
		if (rc != NO_ERROR)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			return -1;
		}
	}
	else
	{
		APIRET rc;
		rc = DosRequestMutexSem(mtx->hnd, SEM_INDEFINITE_WAIT);
		if (rc != NO_ERROR)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			return -1;
		}
	}

	if (mtx->owner_tid == tid) { mtx->owner_count++; }
	else { mtx->owner_tid = tid; mtx->owner_count = 1; }

#elif defined(__DOS__)

	/* nothing to do */

#else
	#if (HAWK_SIZEOF_PTHREAD_MUTEX_T <= 0)
		/* nothing to do as there is no actual mutex support */
	#else

	/* if pthread_mutex_timedlock() isn't available, don't honor the waiting time. */
	#if defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
	if (waiting_time)
	{
		hawk_ntime_t t;
		struct timespec ts;
		int n;

		hawk_get_ntime (&t);
		hawk_add_ntime (&t, waiting_time, &t);

		ts.tv_sec = t.sec;
		ts.tv_nsec = t.nsec;
		n = pthread_mutex_timedlock((pthread_mutex_t*)&mtx->hnd, &ts);
		if (n != 0)
		{
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
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
			hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
			return -1;
		}
	#if defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
	}
	#endif

	#endif
#endif

	return 0;
}

int hawk_mtx_unlock (hawk_mtx_t* mtx)
{
#if defined(_WIN32)
	hawk_uintptr_t tid;

	tid = GetCurrentThreadId();
	if (mtx->owner_tid != tid || mtx->owner_count == 0)
	{
		hawk_gem_seterrbfmt(mtx->gem, HAWK_NULL, HAWK_EPERM, "prohibited unlock attempt by non-owner");
		return -1;
	}

	if (ReleaseMutex(mtx->hnd) == FALSE)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

	mtx->owner_count--;
	if (mtx->owner_count == 0) mtx->owner_tid = 0;

#elif defined(__OS2__)
	APIRET rc;
	hawk_uintptr_t tid;

	tid = os2_get_current_tid();
	if (mtx->owner_tid != tid || mtx->owner_count == 0)
	{
		hawk_gem_seterrbfmt(mtx->gem, HAWK_NULL, HAWK_EPERM, "prohibited unlock attempt by non-owner");
		return -1;
	}

	rc = DosReleaseMutexSem(mtx->hnd);
	if (rc != NO_ERROR)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

	mtx->owner_count--;
	if (mtx->owner_count == 0) mtx->owner_tid = 0;

#elif defined(__DOS__)

	/* nothing to do */

#else
	#if (HAWK_SIZEOF_PTHREAD_MUTEX_T <= 0)
		/* nothing to do as there is no actual mutex support */
	#else

	int n;
	n = pthread_mutex_unlock((pthread_mutex_t*)&mtx->hnd);
	if (n != 0)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
		return -1;
	}

	#endif
#endif
	return 0;
}

int hawk_mtx_trylock (hawk_mtx_t* mtx)
{
#if defined(_WIN32)
	hawk_uintptr_t tid;
	DWORD ret;

	tid = GetCurrentThreadId();
	if (!(mtx->flags & HAWK_MTX_FLAG_RECURSIVE) && mtx->owner_tid == tid && mtx->owner_count > 0)
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, HAWK_EBUSY);
		return -1;
	}

	ret = WaitForSingleObject(mtx->hnd, 0);
	if (ret == WAIT_TIMEOUT)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, HAWK_EBUSY);
		return -1;
	}
	else if (ret == WAIT_ABANDONED)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, HAWK_ESYSERR);
		return -1;
	}
	else if (ret != WAIT_OBJECT_0)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

	if (mtx->owner_tid == tid) { mtx->owner_count++; }
	else { mtx->owner_tid = tid; mtx->owner_count = 1; }

#elif defined(__OS2__)
	hawk_uintptr_t tid;
	APIRET rc;

	tid = os2_get_current_tid();
	if (!(mtx->flags & HAWK_MTX_FLAG_RECURSIVE) && mtx->owner_tid == tid && mtx->owner_count > 0)
	{
		hawk_gem_seterrnum (mtx->gem, HAWK_NULL, HAWK_EBUSY);
		return -1;
	}

	rc = DosRequestMutexSem(mtx->hnd, 0);
	if (rc == ERROR_TIMEOUT)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, HAWK_EBUSY);
		return -1;
	}
	else if (rc != NO_ERROR)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

	if (mtx->owner_tid == tid) { mtx->owner_count++; }
	else { mtx->owner_tid = tid; mtx->owner_count = 1; }
#elif defined(__DOS__)
	/* nothing to do */

#else

	#if (HAWK_SIZEOF_PTHREAD_MUTEX_T <= 0)
		/* nothing to do as there is no actual mutex support */
	#else
	/* -------------------------------------------------- */
	int n;
	#if defined(HAVE_PTHREAD_MUTEX_TRYLOCK)
	n = pthread_mutex_trylock((pthread_mutex_t*)&mtx->hnd);
	#elif defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
	hawk_ntime_t t;
	struct timespec ts;
	hawk_get_ntime (&t);
	ts.tv_sec = t.sec;
	ts.tv_nsec = t.nsec;
	n = pthread_mutex_timedlock((pthread_mutex_t*)&mtx->hnd, &ts);
	#else
	/* not supported. fallback to normal pthread_mutex_lock(). <--- is this really desirable? */
	n = pthread_mutex_lock((pthread_mutex_t*)&mtx->hnd);
	#endif
	if (n != 0)
	{
		hawk_gem_seterrnum(mtx->gem, HAWK_NULL, hawk_syserr_to_errnum(n));
		return -1;
	}
	/* -------------------------------------------------- */
	#endif

#endif
	return 0;
}

#endif
