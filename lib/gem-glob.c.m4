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

/* 
 * Do NOT edit glob.c. Edit glob.c.m4 instead.
 * 
 * Generate gem-glob.c with m4
 *   $ m4 gem-glob.c.m4 > gem-glob.c  
 */

#include "hawk-prv.h"
#include <hawk-chr.h>
#include <hawk-ecs.h>
#include <hawk-dir.h>
#include <hawk-glob.h>

#include "syscall.h"

#define NO_RECURSION 1

enum segment_type_t
{
        NONE,
        ROOT,
        NORMAL
};
typedef enum segment_type_t segment_type_t;


#define IS_ESC(c) HAWK_FNMAT_IS_ESC(c)
#define IS_SEP(c) HAWK_FNMAT_IS_SEP(c)
#define IS_NIL(c) ((c) == '\0')
#define IS_SEP_OR_NIL(c) (IS_SEP(c) || IS_NIL(c))

/* this macro only checks for top-level wild-cards among these.
 *  *, ?, [], !, -
 * see str-fnmat.c for more wild-card letters
 */
#define IS_WILD(c) ((c) == '*' || (c) == '?' || (c) == '[')


static hawk_bch_t* wcs_to_mbuf (hawk_gem_t* g, const hawk_uch_t* wcs, hawk_becs_t* mbuf)
{
	hawk_oow_t ml, wl;

	if (hawk_gem_convutobcstr(g, wcs, &wl, HAWK_NULL, &ml) <= -1 ||
	    hawk_becs_setlen(mbuf, ml) == (hawk_oow_t)-1) return HAWK_NULL;

	hawk_gem_convutobcstr (g, wcs, &wl, HAWK_BECS_PTR(mbuf), &ml);
	return HAWK_BECS_PTR(mbuf);
}

#if defined(_WIN32) && !defined(INVALID_FILE_ATTRIBUTES)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

static int upath_exists (hawk_gem_t* g, const hawk_uch_t* name, hawk_becs_t* mbuf)
{
#if defined(_WIN32)
	return (GetFileAttributesW(name) != INVALID_FILE_ATTRIBUTES)? 1: 0;

#elif defined(__OS2__)
	FILESTATUS3 fs;
	APIRET rc;
	const hawk_bch_t* mptr;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	rc = DosQueryPathInfo(mptr, FIL_STANDARD, &fs, HAWK_SIZEOF(fs));
	return (rc == NO_ERROR)? 1: ((rc == ERROR_PATH_NOT_FOUND)? 0: -1);

#elif defined(__DOS__)
	unsigned int x, attr;
	const hawk_bch_t* mptr;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	x = _dos_getfileattr (mptr, &attr);
	return (x == 0)? 1: ((errno == ENOENT)? 0: -1);

#elif defined(macintosh)
	HFileInfo fpb;
	const hawk_bch_t* mptr;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	HAWK_MEMSET (&fpb, 0, HAWK_SIZEOF(fpb));
	fpb.ioNamePtr = (unsigned char*)mptr;

	return (PBGetCatInfoSync ((CInfoPBRec*)&fpb) == noErr)? 1: 0;
#else
	struct stat st;
	const hawk_bch_t* mptr;
	int t;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	#if defined(HAVE_LSTAT)
	t = HAWK_LSTAT(mptr, &st);
	#else
	t = HAWK_STAT(mptr, &st);/* use stat() if no lstat() is available. */
	#endif
	return (t == 0);
#endif
}

static int bpath_exists (hawk_gem_t* g, const hawk_bch_t* name, hawk_becs_t* mbuf)
{
#if defined(_WIN32)
	return (GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES)? 1: 0;
#elif defined(__OS2__)

	FILESTATUS3 fs;
	APIRET rc;
	rc = DosQueryPathInfo(name, FIL_STANDARD, &fs, HAWK_SIZEOF(fs));
	return (rc == NO_ERROR)? 1: ((rc == ERROR_PATH_NOT_FOUND)? 0: -1);

#elif defined(__DOS__)
	unsigned int x, attr;
	x = _dos_getfileattr(name, &attr);
	return (x == 0)? 1: ((errno == ENOENT)? 0: -1);

#elif defined(macintosh)
	HFileInfo fpb;
	HAWK_MEMSET (&fpb, 0, HAWK_SIZEOF(fpb));
	fpb.ioNamePtr = (unsigned char*)name;
	return (PBGetCatInfoSync((CInfoPBRec*)&fpb) == noErr)? 1: 0;
#else
	struct stat st;
	int t;
	#if defined(HAVE_LSTAT)
	t = HAWK_LSTAT(name, &st);
	#else
	t = HAWK_STAT(name, &st); /* use stat() if no lstat() is available. */
	#endif
	return (t == 0);
#endif
}

dnl
dnl ---------------------------------------------------------------------------
include(`gem-glob.m4')dnl
dnl ---------------------------------------------------------------------------
fn_glob(hawk_gem_uglob, hawk_uch_t, hawk_ucs_t, hawk_uecs, HAWK_UECS, hawk_gem_uglob_cb_t, upath_exists, hawk_fnmat_ucstr_uchars, __u)
fn_glob(hawk_gem_bglob, hawk_bch_t, hawk_bcs_t, hawk_becs, HAWK_BECS, hawk_gem_bglob_cb_t, bpath_exists, hawk_fnmat_bcstr_bchars, __b)
