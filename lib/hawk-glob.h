/*
    Copyright (c) 2006-2020 Chung, Hyung-Hwan. All rights reserved.
    are met:
    1. Redistributions of source code must retain the above copyright

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
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

#ifndef _HAWK_GLOB_H_
#define _HAWK_GLOB_H_

#include <hawk-gem.h>

/** \file
 * This file provides functions, types, macros for wildcard expansion
 * in a path name.
 */

typedef int (*hawk_gem_bglob_cb_t) (
	const hawk_bcs_t*  path,
	void*              cbctx
);

typedef int (*hawk_gem_uglob_cb_t) (
	const hawk_ucs_t*  path,
	void*              cbctx
);

enum hawk_glob_flag_t
{
	/** Don't use the backslash as an escape charcter.
	 *  This option is on in Win32/OS2/DOS. */
	HAWK_GLOB_NOESCAPE   = (1 << 0),

	/** Match a leading period explicitly by a literal period in the pattern */
	HAWK_GLOB_PERIOD     = (1 << 1),

	/** Perform case-insensitive matching.
	 *  This option is always on in Win32/OS2/DOS. */
	HAWK_GLOB_IGNORECASE = (1 << 2),

	/** Make the function to be more fault-resistent */
	HAWK_GLOB_TOLERANT   = (1 << 3),

	/** Exclude special entries from matching.
	  * Special entries include . and .. */
	HAWK_GLOB_SKIPSPCDIR  = (1 << 4),

	/**
	  * bitsise-ORed of all valid enumerators
	  */
	HAWK_GLOB_ALL = (HAWK_GLOB_NOESCAPE | HAWK_GLOB_PERIOD |
	                 HAWK_GLOB_IGNORECASE | HAWK_GLOB_TOLERANT |
	                 HAWK_GLOB_SKIPSPCDIR)
};
typedef enum hawk_glob_flag_t hawk_glob_flag_t;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_gem_bglob() function finds path names matchin the \a pattern.
 * It calls the call-back function \a cbimpl for each path name found.
 *
 * \return -1 on failure, 0 on no match, 1 if matches are found.
 */
HAWK_EXPORT int hawk_gem_bglob (
	hawk_gem_t*               gem,
	const hawk_bch_t*         pattern,
	hawk_gem_bglob_cb_t       cbimpl,
	void*                     cbctx,
	int                       flags
);

HAWK_EXPORT int hawk_gem_uglob (
	hawk_gem_t*               gem,
	const hawk_uch_t*         pattern,
	hawk_gem_uglob_cb_t       cbimpl,
	void*                     cbctx,
	int                       flags
);

#if defined(HAWK_OOCH_IS_UCH)
	typedef hawk_gem_uglob_cb_t hawk_gem_glob_cb_t;
#	define hawk_gem_glob hawk_gem_uglob
#else
	typedef hawk_gem_bglob_cb_t hawk_gem_glob_cb_t;
#	define hawk_gem_glob hawk_gem_bglob

#endif

#if defined(__cplusplus)
}
#endif

#endif

