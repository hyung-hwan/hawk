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

#include <hawk-cmn.h>

#if defined(_WIN32)
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(vms) || defined(__vms)
#	define __NEW_STARLET 1
#	include <starlet.h>
#	include <rms.h>
#elif defined(__DOS__)
#	include <errno.h>
#else
#	include <errno.h>
#endif


#if defined(_WIN32)

hawk_errnum_t hawk_syserr_to_errnum (hawk_intptr_t e) /* actually DWORD */
{
	switch (e)
	{
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			return HAWK_ENOMEM;
		case ERROR_INVALID_PARAMETER:
		case ERROR_INVALID_HANDLE:
		case ERROR_INVALID_NAME:
			return HAWK_EINVAL;
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
			return HAWK_EACCES;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return HAWK_ENOENT;
		case ERROR_ALREADY_EXISTS:
		case ERROR_FILE_EXISTS:
			return HAWK_EEXIST;
		case ERROR_BROKEN_PIPE:
			return HAWK_EPIPE;
		default:
			return HAWK_ESYSERR;
	}
}

#elif defined(__OS2__)

hawk_errnum_t hawk_syserr_to_errnum (hawk_intptr_t e) /* actually APIRET */
{
	switch (e)
	{
		case ERROR_NOT_ENOUGH_MEMORY:
			return HAWK_ENOMEM;
		case ERROR_INVALID_PARAMETER:
		case ERROR_INVALID_HANDLE:
		case ERROR_INVALID_NAME:
			return HAWK_EINVAL;
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION: 
			return HAWK_EACCES;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return HAWK_ENOENT;
		case ERROR_ALREADY_EXISTS:
			return HAWK_EEXIST;
		case ERROR_BROKEN_PIPE:
			return HAWK_EPIPE;
		default:
			return HAWK_ESYSERR;
	}
}

#elif defined(__DOS__) 

hawk_errnum_t hawk_syserr_to_errnum (hawk_intptr_t e)
{
	switch (e)
	{
		case ENOMEM: return HAWK_ENOMEM;
		case EINVAL: return HAWK_EINVAL;
		case EACCES: return HAWK_EACCES;
		case ENOENT: return HAWK_ENOENT;
		case EEXIST: return HAWK_EEXIST;
		default:     return HAWK_ESYSERR;
	} 
}

#elif defined(vms) || defined(__vms)

	/* TODO: */
hawk_errnum_t hawk_syserr_to_errnum (hawk_intptr_t e) /* actually unsigned long */
{
	switch (e)
	{
		case RMS$_NORMAL: return HAWK_ENOERR;
		default:          return HAWK_ESYSERR;
	}
}

#else

hawk_errnum_t hawk_syserr_to_errnum (hawk_intptr_t e)
{
	switch (e)
	{
	#if defined(ENOMEM)
		case ENOMEM: return HAWK_ENOMEM; 
	#endif
	#if defined(EINVAL)
		case EINVAL: return HAWK_EINVAL; 
	#endif
	#if defined(EBUSY)
		case EBUSY: return HAWK_EBUSY; 
	#endif
	#if defined(EACCES)
		case EACCES: return HAWK_EACCES; 
	#endif
	#if defined(EPERM)
		case EPERM: return HAWK_EPERM; 
	#endif
	#if defined(EISDIR)
		case EISDIR: return HAWK_EISDIR; 
	#endif
	#if defined(ENOTDIR)
		case ENOTDIR: return HAWK_ENOTDIR; 
	#endif
	#if defined(ENXIO)
		case ENXIO: return HAWK_ENOENT; /* ENODEV mapped to ENOENT */
	#endif
	#if defined(ENODEV)
		case ENODEV: return HAWK_ENOENT; /* ENODEV mapped to ENOENT */
	#endif
	#if defined(ENOENT)
		case ENOENT: return HAWK_ENOENT; 
	#endif
	#if defined(EEXIST)
		case EEXIST: return HAWK_EEXIST; 
	#endif
	#if defined(EINTR)
		case EINTR:  return HAWK_EINTR; 
	#endif
	#if defined(EPIPE)
		case EPIPE:  return HAWK_EPIPE; 
	#endif
	#if defined(ECHILD)
		case ECHILD:  return HAWK_ECHILD; 
	#endif
	#if defined(ETIMEDOUT)
		case ETIMEDOUT: return HAWK_ETMOUT; 
	#endif
	#if defined(EINPROGRESS)
		case EINPROGRESS: return HAWK_EINPROG; 
	#endif

	#if defined(EWOULDBLOCK) && defined(EAGAIN) && (EWOULDBLOCK == EAGAIN)
		case EAGAIN: return HAWK_EAGAIN; 
	#else
		#if defined(EWOULDBLOCK)
		case EWOULDBLOCK: return HAWK_EAGAIN; 
		#endif
		#if defined(EAGAIN)
		case EAGAIN: return HAWK_EAGAIN; 
		#endif
	#endif
		default:     return HAWK_ESYSERR; 
	}
}


#endif
