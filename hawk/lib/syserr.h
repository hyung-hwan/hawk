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

/* 
 * this file is intended for internal use only.
 * you can include this file and use IMPLEMENT_SYSERR_TO_ERRNUM.
 *
 *   #include "syserr.h"
 *   IMPLEMENT_SYSERR_TO_ERRNUM (pio, PIO)
 *
 * header files defining system error codes must be included
 * before this file.
 */

#if defined(__cplusplus)
#	define __SYSERRTYPE__(obj) obj
#	define __SYSERRNUM__(obj,num) (obj E_ ## num)
#else
/*#	define __SYSERRTYPE__(obj) hawk_ ## obj ## _errnum_t
#	define __SYSERRNUM__(obj,num) (HAWK_ ## obj ## _ ## num)*/
#	define __SYSERRTYPE__(obj) obj ## _errnum_t
#	define __SYSERRNUM__(obj,num) (obj ## _ ## num)
#endif

#if defined(_WIN32)

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (DWORD e) \
	{ \
		switch (e) \
		{ \
			case ERROR_NOT_ENOUGH_MEMORY: \
			case ERROR_OUTOFMEMORY: \
				return __SYSERRNUM__ (obj2, ENOMEM); \
			case ERROR_INVALID_PARAMETER: \
			case ERROR_INVALID_HANDLE: \
			case ERROR_INVALID_NAME: \
				return __SYSERRNUM__ (obj2, EINVAL); \
			case ERROR_ACCESS_DENIED: \
			case ERROR_SHARING_VIOLATION: \
				return __SYSERRNUM__ (obj2, EACCES); \
			case ERROR_FILE_NOT_FOUND: \
			case ERROR_PATH_NOT_FOUND: \
				return __SYSERRNUM__ (obj2, ENOENT); \
			case ERROR_ALREADY_EXISTS: \
			case ERROR_FILE_EXISTS: \
				return __SYSERRNUM__ (obj2, EEXIST); \
			case ERROR_BROKEN_PIPE: \
				return __SYSERRNUM__ (obj2, EPIPE); \
			default: \
				return __SYSERRNUM__ (obj2, ESYSERR); \
		} \
	}

#elif defined(__OS2__)

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (APIRET e) \
	{ \
		switch (e) \
		{ \
			case ERROR_NOT_ENOUGH_MEMORY: \
				return __SYSERRNUM__ (obj2, ENOMEM); \
			case ERROR_INVALID_PARAMETER: \
			case ERROR_INVALID_HANDLE: \
			case ERROR_INVALID_NAME: \
				return __SYSERRNUM__ (obj2, EINVAL); \
			case ERROR_ACCESS_DENIED: \
			case ERROR_SHARING_VIOLATION:  \
				return __SYSERRNUM__ (obj2, EACCES); \
			case ERROR_FILE_NOT_FOUND: \
			case ERROR_PATH_NOT_FOUND: \
				return __SYSERRNUM__ (obj2, ENOENT); \
			case ERROR_ALREADY_EXISTS: \
				return __SYSERRNUM__ (obj2, EEXIST); \
			case ERROR_BROKEN_PIPE: \
				return __SYSERRNUM__ (obj2, EPIPE); \
			default: \
				return __SYSERRNUM__ (obj2, ESYSERR); \
		} \
	}

/*
#elif defined(__DOS__) 

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (int e) \
	{ \
		switch (e) \
		{ \
			case ENOMEM: return __SYSERRNUM__ (obj2, ENOMEM); \
			case EINVAL: return __SYSERRNUM__ (obj2, EINVAL); \
			case EACCES: return __SYSERRNUM__ (obj2, EACCES); \
			case ENOENT: return __SYSERRNUM__ (obj2, ENOENT); \
			case EEXIST: return __SYSERRNUM__ (obj2, EEXIST); \
			default:     return __SYSERRNUM__ (obj2, ESYSERR); \
		} \
	}
*/

#elif defined(vms) || defined(__vms)

	/* TODO: */
	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (unsigned long e) \
	{ \
		switch (e) \
		{ \
			case RMS$_NORMAL: return __SYSERRNUM__ (obj2, ENOERR); \
			default:          return __SYSERRNUM__ (obj2, ESYSERR); \
		} \
	}

#else

	#if defined(EWOULDBLOCK) && defined(EAGAIN) && (EWOULDBLOCK != EAGAIN)

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (int e) \
	{ \
		switch (e) \
		{ \
			case ENOMEM: return __SYSERRNUM__(obj2, ENOMEM); \
			case EINVAL: return __SYSERRNUM__(obj2, EINVAL); \
			case EBUSY: return _SYSERRNUM__(obj2, EBUSY); \
			case EACCES: return __SYSERRNUM__(obj2, EACCES); \
			case EPERM: return __SYSERRNUM__(obj2, EPERM); \
			case ENOTDIR: return __SYSERRNUM__(obj2, ENOTDIR); \
			case ENOENT: return __SYSERRNUM__(obj2, ENOENT); \
			case EEXIST: return __SYSERRNUM__(obj2, EEXIST); \
			case EINTR:  return __SYSERRNUM__(obj2, EINTR); \
			case EPIPE:  return __SYSERRNUM__(obj2, EPIPE); \
			case ECHILD:  return __SYSERRNUM__(obj2, ECHILD); \
			case ETIMEDOUT: return __SYSERRNUM__(obj2, ETMOUT); \
			case EINPROGRESS: return __SYSERRNUM__(obj2, EINPROG); \
			case EWOULDBLOCK: \
			case EAGAIN: return __SYSERRNUM__(obj2, EAGAIN); \
			default:     return __SYSERRNUM__(obj2, ESYSERR); \
		} \
	}

	#elif defined(EAGAIN)

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (int e) \
	{ \
		switch (e) \
		{ \
			case ENOMEM: return __SYSERRNUM__(obj2, ENOMEM); \
			case EINVAL: return __SYSERRNUM__(obj2, EINVAL); \
			case EBUSY: return __SYSERRNUM__(obj2, EBUSY); \
			case EACCES: return __SYSERRNUM__(obj2, EACCES); \
			case EPERM: return __SYSERRNUM__(obj2, EPERM); \
			case ENOTDIR: return __SYSERRNUM__(obj2, ENOTDIR); \
			case ENOENT: return __SYSERRNUM__(obj2, ENOENT); \
			case EEXIST: return __SYSERRNUM__(obj2, EEXIST); \
			case EINTR:  return __SYSERRNUM__(obj2, EINTR); \
			case EPIPE:  return __SYSERRNUM__(obj2, EPIPE); \
			case ECHILD:  return __SYSERRNUM__(obj2, ECHILD); \
			case ETIMEDOUT: return __SYSERRNUM__(obj2, ETMOUT); \
			case EINPROGRESS: return __SYSERRNUM__(obj2, EINPROG); \
			case EAGAIN: return __SYSERRNUM__(obj2, EAGAIN); \
			default:     return __SYSERRNUM__(obj2, ESYSERR); \
		} \
	}

	#elif defined(EWOULDBLOCK)

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (int e) \
	{ \
		switch (e) \
		{ \
			case ENOMEM: return __SYSERRNUM__(obj2, ENOMEM); \
			case EINVAL: return __SYSERRNUM__(obj2, EINVAL); \
			case EBUSY: return __SYSERRNUM__(obj2, EBUSY); \
			case EACCES: return __SYSERRNUM__(obj2, EACCES); \
			case EPERM: return __SYSERRNUM__(obj2, EPERM); \
			case ENOTDIR: return __SYSERRNUM__(obj2, ENOTDIR); \
			case ENOENT: return __SYSERRNUM__(obj2, ENOENT); \
			case EEXIST: return __SYSERRNUM__(obj2, EEXIST); \
			case EINTR:  return __SYSERRNUM__(obj2, EINTR); \
			case EPIPE:  return __SYSERRNUM__(obj2, EPIPE); \
			case ECHILD: return __SYSERRNUM__(obj2, ECHILD); \
			case ETIMEDOUT: return __SYSERRNUM__(obj2, ETMOUT); \
			case EINPROGRESS: return __SYSERRNUM__(obj2, EINPROG); \
			case EWOULDBLOCK: return __SYSERRNUM__(obj2, EAGAIN); \
			default:     return __SYSERRNUM__(obj2, ESYSERR); \
		} \
	}

	#else

	#define IMPLEMENT_SYSERR_TO_ERRNUM(obj1,obj2) \
	static __SYSERRTYPE__(obj1) syserr_to_errnum (int e) \
	{ \
		switch (e) \
		{ \
			case ENOMEM: return __SYSERRNUM__(obj2, ENOMEM); \
			case EINVAL: return __SYSERRNUM__(obj2, EINVAL); \
			case EBUSY: return __SYSERRNUM__(obj2, EBUSY); \
			case EACCES: return __SYSERRNUM__(obj2, EACCES); \
			case EPERM: return __SYSERRNUM__(obj2, EPERM); \
			case ENOTDIR: return __SYSERRNUM__(obj2, ENOTDIR); \
			case ENOENT: return __SYSERRNUM__(obj2, ENOENT); \
			case EEXIST: return __SYSERRNUM__(obj2, EEXIST); \
			case EINTR:  return __SYSERRNUM__(obj2, EINTR); \
			case EPIPE:  return __SYSERRNUM__(obj2, EPIPE); \
			case ECHILD: return __SYSERRNUM__(obj2, ECHILD); \
			case ETIMEDOUT: return __SYSERRNUM__(obj2, ETMOUT); \
			case EINPROGRESS: return __SYSERRNUM__(obj2, EINPROG); \
			default:     return __SYSERRNUM__(obj2, ESYSERR); \
		} \
	}

	#endif

#endif
