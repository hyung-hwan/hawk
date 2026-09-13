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

#if !defined(_GNU_SOURCE)
/* to expose non-portable functions like pthread_getattr_np() */
#define _GNU_SOURCE
#endif

#include "hawk-prv.h"

#if defined(HAWK_HAVE_NATIVE_CSTACK_BOUNDS)


#if defined(_WIN32)
#	include <windows.h>
#elif defined(__HAIKU__)
#	include <OS.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#	include <pthread.h>
#	if defined(__FreeBSD__)
#		include <pthread_np.h>
#	elif defined(__OpenBSD__)
#		include <signal.h>
#		include <pthread_np.h>
#	endif
#endif

int hawk_rtx_entercstack (hawk_rtx_t* rtx)
{
	hawk_oow_t low = 0, high = HAWK_TYPE_MAX(hawk_oow_t);
	hawk_oow_t pos = HAWK_CSTACK_POSITION(low);

#if defined(_WIN32)
	MEMORY_BASIC_INFORMATION mbi;
	typedef BOOL (WINAPI *get_guarantee_t)(ULONG*);
	get_guarantee_t get_guarantee;
	HMODULE kernel;
	ULONG guarantee = 0;

	if (!VirtualQuery((const void*)pos, &mbi, HAWK_SIZEOF(mbi))) goto unavailable;

	/* AllocationBase includes the reserved portion, unlike the TIB's
	 * StackLimit which only describes currently committed stack pages. */
	low = (hawk_oow_t)mbi.AllocationBase;
	high = (hawk_oow_t)mbi.BaseAddress + mbi.RegionSize;

	/* A host may reserve additional exception-handling stack space. A zero
	 * request queries it without changing it. Resolve dynamically for old
	 * Windows versions which do not implement SetThreadStackGuarantee. */
	kernel = GetModuleHandleA("kernel32.dll");
	if (HAWK_UNLIKELY(!kernel)) goto unavailable;

	get_guarantee = (get_guarantee_t)GetProcAddress(kernel, "SetThreadStackGuarantee");
	if (get_guarantee && !get_guarantee(&guarantee)) goto unavailable;

	if (low >= high || guarantee >= high - low)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_ESTACK, "native C stack limit reached");
		return -1;
	}
	low += guarantee;

#elif defined(__HAIKU__)

	thread_info ti;
	if (get_thread_info(find_thread(HAWK_NULL), &ti) != B_OK) goto unavailable;
	/* The Kernel Kit documentation has this:
	 * https://www.haiku-os.org/legacy-docs/bebook/TheKernelKit_ThreadsAndTeams.html#thread_info
	 *   Warning
	 *   The two stack pointers are currently inverted such that stack_base is less than stack_end.
	 *   (In a stack-grows-down world, the base should be greater than the end.) */
	low = (hawk_oow_t)ti.stack_base;
	high = (hawk_oow_t)ti.stack_end;

#elif defined(__APPLE__)

	size_t size;
	high = (hawk_oow_t)pthread_get_stackaddr_np(pthread_self());
	size = pthread_get_stacksize_np(pthread_self());
	if (size > high) goto unavailable;
	low = high - size;

#elif defined(__OpenBSD__)

	stack_t seg;
	if (pthread_stackseg_np(pthread_self(), &seg) != 0) goto unavailable;
	high = (hawk_oow_t)seg.ss_sp; /* this API returns the top, not the base */
	if (seg.ss_size > high) goto unavailable;
	low = high - seg.ss_size;

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__)

	pthread_attr_t attr;
	void* addr;
	size_t size, guard;
	int n;

	#if defined(__FreeBSD__) || defined(__NetBSD__)
	if (pthread_attr_init(&attr) != 0) goto unavailable;
	n = pthread_attr_get_np(pthread_self(), &attr);
	if (n != 0)
	{
		pthread_attr_destroy(&attr);
		goto unavailable;
	}
	#else
	if (pthread_getattr_np(pthread_self(), &attr) != 0) goto unavailable;
	#endif
	n = pthread_attr_getstack(&attr, &addr, &size);

	guard = 0;
	if (n == 0) n = pthread_attr_getguardsize(&attr, &guard);

	pthread_attr_destroy(&attr);
	if (n != 0 || guard >= size) goto unavailable;

	low = (hawk_oow_t)addr;
	if (size > HAWK_TYPE_MAX(hawk_oow_t) - low) goto unavailable;

	high = low + size;

	/* some pthread implementations include the guard in the reported
	 * interval, others exclude it. exclude it conservatively in either case. */
	#if defined(HAWK_CSTACK_GROWS_UPWARDS)
	high -= guard;
	#else
	low += guard;
	#endif

#endif

	if (low >= high || pos < low || pos >= high) goto unavailable;
	if (high - low <= HAWK_CSTACK_HEADROOM) goto cstack_full;

	#if defined(HAWK_CSTACK_GROWS_UPWARDS)
	high -= HAWK_CSTACK_HEADROOM;
	#else
	low += HAWK_CSTACK_HEADROOM;
	#endif
	if (pos < low || pos >= high) goto cstack_full;

#if defined(HAWK_CSTACK_GROWS_UPWARDS)
	rtx->cstack_limit = high;
#else
	rtx->cstack_limit = low;
#endif
	return 0;

#if defined(HAWK_HAVE_NATIVE_CSTACK_BOUNDS)
unavailable:
	hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_ESYSERR, "unable to determine native C stack bounds");
	return -1;

cstack_full:
	hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_ESTACK, "native C stack limit reached");
	return -1;
#endif
}

#endif
