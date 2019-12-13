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

#ifndef _HAWK_FMT_PRV_H_
#define _HAWK_FMT_PRV_H_

#include <hawk-fmt.h>
#include <stdarg.h>

typedef int (*hawk_mfmtout_put_t) (
	hawk_bch_t c,
	void*       ctx
);

/* convert a wide string to a multi-byte string */
typedef int (*hawk_mfmtout_conv_t) (
	const hawk_uch_t* wcs,
	hawk_oow_t*        wcslen,
	hawk_bch_t*       mbs,
	hawk_oow_t*        mbslen,
	void*              ctx
);

struct hawk_mfmtout_t
{
	hawk_oow_t         count;     /* out */
	hawk_oow_t         limit;     /* in */
	void*              ctx;       /* in */
	hawk_mfmtout_put_t  put;       /* in */
	hawk_mfmtout_conv_t conv;      /* in */
};

typedef struct hawk_mfmtout_t hawk_mfmtout_t;

typedef int (*hawk_wfmtout_put_t) (
	hawk_uch_t c,
	void*       ctx
);

/* convert a multi-byte string to a wide string */
typedef int (*hawk_wfmtout_conv_t) (
	const hawk_bch_t* mbs,
	hawk_oow_t*        mbslen,
	hawk_uch_t*       wcs,
	hawk_oow_t*        wcslen,
	void*              ctx
);

struct hawk_wfmtout_t
{
	hawk_oow_t         count;     /* out */
	hawk_oow_t         limit;     /* in */
	void*              ctx;       /* in */
	hawk_wfmtout_put_t  put;       /* in */
	hawk_wfmtout_conv_t conv;      /* in */
};


typedef struct hawk_wfmtout_t hawk_wfmtout_t;

#if defined(__cplusplus)
extern "C" {
#endif

/* HAWK_EXPORTed, but keep in it the private header for used by other libraries in HAWK */
HAWK_EXPORT int hawk_mfmtout (
	const hawk_bch_t* fmt,
	hawk_mfmtout_t*     data,
	va_list            ap
);

HAWK_EXPORT int hawk_wfmtout (
	const hawk_uch_t* fmt,
	hawk_wfmtout_t*     data,
	va_list            ap
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_fmtout_t hawk_mfmtout_t
#	define hawk_fmtout(fmt,fo,ap) hawk_mfmtout(fmt,fo,ap)
#else
#	define hawk_fmtout_t hawk_wfmtout_t
#	define hawk_fmtout(fmt,fo,ap) hawk_wfmtout(fmt,fo,ap)
#endif

#if defined(__cplusplus)
}
#endif

#endif
