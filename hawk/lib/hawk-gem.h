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

#ifndef _HAWK_GEM_H_
#define _HAWK_GEM_H_

#include <hawk-cmn.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_gem_allocmem_noerr (hawk_gem_t* gem, hawk_oow_t size) { return HAWK_MMGR_ALLOC(gem->mmgr, size); }
static HAWK_INLINE void* hawk_gem_reallocmem_noerr (hawk_gem_t* gem, void* ptr, hawk_oow_t size) { return HAWK_MMGR_REALLOC(gem->mmgr, ptr, size); }
#else
#define hawk_gem_allocmem_noerr(gem, size) (HAWK_MMGR_ALLOC((gem)->mmgr, size))
#define hawk_gem_reallocmem_noerr(gem, ptr, size) (HAWK_MMGR_REALLOC((gem)->mmgr, ptr, size))
#endif

void* hawk_gem_allocmem (
	hawk_gem_t*       gem,
	hawk_oow_t        size
);

void* hawk_gem_callocmem (
	hawk_gem_t*       gem,
	hawk_oow_t        size
);

void* hawk_gem_reallocmem (
	hawk_gem_t*       gem,
	void*             ptr,
	hawk_oow_t        size
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_gem_freemem (hawk_gem_t* gem, void* ptr) { HAWK_MMGR_FREE (gem->mmgr, ptr); }
#else
#define hawk_gem_freemem(gem, ptr) HAWK_MMGR_FREE((gem)->mmgr, ptr);
#endif


void* hawk_gem_callocmem_noerr (
	hawk_gem_t*       gem,
	hawk_oow_t        size
);

/* ----------------------------------------------------------------------- */

HAWK_EXPORT hawk_uch_t* hawk_gem_dupucstr (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t*       _ucslen
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbcstr (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t*       _bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupuchars (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t        ucslen
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbchars (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupucs (
	hawk_gem_t*       gem,
	const hawk_ucs_t* ucs
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbcs (
	hawk_gem_t*       gem,
	const hawk_bcs_t* bcs
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupucstrarr (
	hawk_gem_t*       gem,
	const hawk_uch_t* str[],
	hawk_oow_t*       len
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupbcstrarr (
	hawk_gem_t*       gem,
	const hawk_bch_t* str[],
	hawk_oow_t* len
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_gem_dupoocstr    hawk_gem_dupucstr
#	define hawk_gem_dupoochars   hawk_gem_dupuchars
#	define hawk_gem_dupoocs      hawk_gem_dupucs
#	define hawk_gem_dupoocstrarr hawk_gem_dupucstrarr
#else
#	define hawk_gem_dupoocstr    hawk_gem_dupbcstr
#	define hawk_gem_dupoochars   hawk_gem_dupbchars
#	define hawk_gem_dupoocs      hawk_gem_dupbcs
#	define hawk_gem_dupoocstrarr hawk_gem_dupbcstrarr
#endif

/* ----------------------------------------------------------------------- */

HAWK_EXPORT int hawk_gem_convbtouchars (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	int               all
);

HAWK_EXPORT int hawk_gem_convutobchars (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t*       ucslen,
	hawk_bch_t*       bcs,
	hawk_oow_t*       bcslen
);

HAWK_EXPORT int hawk_gem_convbtoucstr (
	hawk_gem_t*       gem,
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	int               all
);

HAWK_EXPORT int hawk_gem_convutobcstr (
	hawk_gem_t*       gem,
	const hawk_uch_t* ucs,
	hawk_oow_t*       ucslen,
	hawk_bch_t*       bcs,
	hawk_oow_t*       bcslen
);

/* ----------------------------------------------------------------------- */


HAWK_EXPORT hawk_uch_t* hawk_gem_dupbtouchars (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs,
	hawk_oow_t         bcslen,
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_duputobchars (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs,
	hawk_oow_t         ucslen,
	hawk_oow_t*        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupb2touchars (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs1,
	hawk_oow_t         bcslen1,
	const hawk_bch_t*  bcs2,
	hawk_oow_t         bcslen2,
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupu2tobchars (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs1,
	hawk_oow_t         ucslen1,
	const hawk_uch_t*  ucs2,
	hawk_oow_t         ucslen2,
	hawk_oow_t*        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupbtoucstr (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs,
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_duputobcstr (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs,
	hawk_oow_t*        bcslen
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupbtoucharswithcmgr (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs,
	hawk_oow_t         _bcslen,
	hawk_oow_t*        _ucslen,
	hawk_cmgr_t*       cmgr,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_duputobcharswithcmgr (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs,
	hawk_oow_t         _ucslen,
	hawk_oow_t*        _bcslen,
	hawk_cmgr_t*       cmgr
);

HAWK_EXPORT hawk_uch_t* hawk_gem_dupbcstrarrtoucstr (
	hawk_gem_t*        gem,
	const hawk_bch_t*  bcs[],
	hawk_oow_t*        ucslen,
	int                all
);

HAWK_EXPORT hawk_bch_t* hawk_gem_dupucstrarrtobcstr (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ucs[],
	hawk_oow_t*        bcslen
);


/* ----------------------------------------------------------------------- */

hawk_oow_t hawk_gem_vfmttoucstr (
	hawk_gem_t*       gem,
	hawk_uch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_uch_t* fmt,
	va_list           ap
);

hawk_oow_t hawk_gem_fmttoucstr (
	hawk_gem_t*       gem,
	hawk_uch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_uch_t* fmt,
	...
);

hawk_oow_t hawk_gem_vfmttobcstr (
	hawk_gem_t*       gem,
	hawk_bch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_bch_t* fmt,
	va_list           ap
);

hawk_oow_t hawk_gem_fmttobcstr (
	hawk_gem_t*       gem,
	hawk_bch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_bch_t* fmt,
	...
);

/* ----------------------------------------------------------------------- */

int hawk_gem_oocharstoskad (
	hawk_gem_t*        gem,
	const hawk_ooch_t* str,
	hawk_oow_t         len,
	hawk_skad_t*       skad
);

/* ----------------------------------------------------------------------- */

int hawk_gem_bcstrtoifindex (
	hawk_gem_t*        gem,
	const hawk_bch_t*  ptr,
	unsigned int*      index
);

int hawk_gem_bcharstoifindex (
	hawk_gem_t*        gem,
	const hawk_bch_t*  ptr,
	hawk_oow_t         len,
	unsigned int*      index
);

int hawk_gem_ucstrtoifindex (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ptr,
	unsigned int*      index
);

int hawk_gem_ucharstoifindex (
	hawk_gem_t*        gem,
	const hawk_uch_t*  ptr,
	hawk_oow_t         len,
	unsigned int*      index
);

int hawk_gem_ifindextobcstr (
	hawk_gem_t*        gem,
	unsigned int       index,
	hawk_bch_t*        buf,
	hawk_oow_t         len
);

int hawk_gem_ifindextoucstr (
	hawk_gem_t*        gem,
	unsigned int       index,
	hawk_uch_t*        buf,
	hawk_oow_t         len
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_gem_oocstrtoifindex hawk_gem_ucstrtoifindex
#	define hawk_gem_oocharstoifindex hawk_gem_ucharstoifindex
#	define hawk_gem_ifindextooocstr hawk_gem_ifindextoucstr
#else
#	define hawk_gem_oocstrtoifindex hawk_gem_bcstrtoifindex
#	define hawk_gem_oocharstoifindex hawk_gem_bcharstoifindex
#	define hawk_gem_ifindextooocstr hawk_gem_ifindextobcstr
#endif

#if defined(__cplusplus)
}
#endif

#endif
