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


#ifndef _HAWK_BTX_H_
#define _HAWK_BTX_H_

#include <hawk-cmn.h>

typedef struct hawk_mtx_t hawk_mtx_t;

#if defined(_WIN32)
	/* <winnt.h> => typedef PVOID HANDLE; */
	typedef void* hawk_mtx_hnd_t;

#elif defined(__OS2__)

	/* typdef unsigned long ULONG;
	 * typedef ULONG HMTX; */
	typedef unsigned long hawk_mtx_hnd_t;

#elif defined(__DOS__)
	/* not implemented */
#	error not implemented

#elif defined(__BEOS__)
	/* typedef int32 sem_id; 
	 * typedef sem_id hawk_mtx_hnd_t; */
	typdef hawk_int32_t hawk_mtx_hnd_t;

#else

#	if (HAWK_SIZEOF_PTHREAD_MUTEX_T == 0)
#		error unsupported

#	elif (HAWK_SIZEOF_PTHREAD_MUTEX_T == HAWK_SIZEOF_INT)
#		if defined(HAWK_PTHREAD_MUTEX_T_IS_SIGNED)
			typedef int hawk_mtx_hnd_t;
#		else
			typedef unsigned int hawk_mtx_hnd_t;
#		endif
#	elif (HAWK_SIZEOF_PTHREAD_MUTEX_T == HAWK_SIZEOF_LONG)
#		if defined(HAWK_PTHREAD_MUTEX_T_IS_SIGNED)
			typedef long hawk_mtx_hnd_t;
#		else
			typedef unsigned long hawk_mtx_hnd_t;
#		endif
#	else
#		include <hawk-pac1.h>
		struct hawk_mtx_hnd_t
		{
			hawk_uint8_t b[HAWK_SIZEOF_PTHREAD_MUTEX_T];
		};
		typedef struct hawk_mtx_hnd_t hawk_mtx_hnd_t;
#		include <hawk-upac.h>
#	endif

#endif

struct hawk_mtx_t
{
	hawk_gem_t* gem;
	hawk_mtx_hnd_t hnd;
};

#ifdef __cplusplus
extern "C" {
#endif

HAWK_EXPORT hawk_mtx_t* hawk_mtx_open (
	hawk_gem_t*  gem,
	hawk_oow_t   xtnsize
);

HAWK_EXPORT void hawk_mtx_close (
	hawk_mtx_t*  mtx
);

HAWK_EXPORT int hawk_mtx_init (
	hawk_mtx_t*  mtx,
	hawk_gem_t*  gem
);

HAWK_EXPORT void hawk_mtx_fini (
	hawk_mtx_t*  mtx
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_mtx_getxtn (hawk_mtx_t* mtx) { return (void*)(mtx + 1); }
#else
#define hawk_mtx_getxtn(mtx) ((void*)((hawk_mtx_t*)(mtx) + 1))
#endif

HAWK_EXPORT int hawk_mtx_lock (
	hawk_mtx_t*         mtx,
	const hawk_ntime_t* waiting_time
);

HAWK_EXPORT int hawk_mtx_unlock (
	hawk_mtx_t*   mtx
);

HAWK_EXPORT int hawk_mtx_trylock (
	hawk_mtx_t* mtx
);

#ifdef __cplusplus
}
#endif

#endif
