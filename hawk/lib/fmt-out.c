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


#include <hawk-fmt.h>
#include <hawk-prv.h>
#include <stdarg.h>
#include "fmt-prv.h"

#include <stdio.h> /* for snrintf(). used for floating-point number formatting */
#if defined(_MSC_VER) || defined(__BORLANDC__) || (defined(__WATCOMC__) && (__WATCOMC__ < 1200))
#	define snprintf _snprintf 
#	if !defined(HAVE_SNPRINTF)
#		define HAVE_SNPRINTF
#	endif
#endif
#if defined(HAVE_QUADMATH_H)
#	include <quadmath.h> /* for quadmath_snprintf() */
#endif
/* TODO: remove stdio.h and quadmath.h once snprintf gets replaced by own 
floting-point conversion implementation*/

/* number of bits in a byte */
#define NBBY    8               

/* Max number conversion buffer length: 
 * hawk_intmax_t in base 2, plus NUL byte. */
#define MAXNBUF (HAWK_SIZEOF(hawk_intmax_t) * NBBY + 1)

enum
{
	/* integer */
	LF_C = (1 << 0),
	LF_H = (1 << 1),
	LF_J = (1 << 2),
	LF_L = (1 << 3),
	LF_Q = (1 << 4),
	LF_T = (1 << 5),
	LF_Z = (1 << 6),

	LF_I8  = (1 << 7),
	LF_I16 = (1 << 8),
	LF_I32 = (1 << 9),
	LF_I64 = (1 << 10),
	LF_I128 = (1 << 11),

	/* long double */
	LF_LD = (1 << 12),
	/* __float128 */
	LF_QD = (1 << 13)
};

static struct
{
	hawk_uint8_t flag; /* for single occurrence */
	hawk_uint8_t dflag; /* for double occurrence */
} lm_tab[26] = 
{
	{ 0,    0 }, /* a */
	{ 0,    0 }, /* b */
	{ 0,    0 }, /* c */
	{ 0,    0 }, /* d */
	{ 0,    0 }, /* e */
	{ 0,    0 }, /* f */
	{ 0,    0 }, /* g */
	{ LF_H, LF_C }, /* h */
	{ 0,    0 }, /* i */
	{ LF_J, 0 }, /* j */
	{ 0,    0 }, /* k */
	{ LF_L, LF_Q }, /* l */
	{ 0,    0 }, /* m */
	{ 0,    0 }, /* n */
	{ 0,    0 }, /* o */
	{ 0,    0 }, /* p */
	{ LF_Q, 0 }, /* q */
	{ 0,    0 }, /* r */
	{ 0,    0 }, /* s */
	{ LF_T, 0 }, /* t */
	{ 0,    0 }, /* u */
	{ 0,    0 }, /* v */
	{ 0,    0 }, /* w */
	{ 0,    0 }, /* z */
	{ 0,    0 }, /* y */
	{ LF_Z, 0 }, /* z */
};


enum 
{
	FLAGC_DOT       = (1 << 0),
	FLAGC_SPACE     = (1 << 1),
	FLAGC_SHARP     = (1 << 2),
	FLAGC_SIGN      = (1 << 3),
	FLAGC_LEFTADJ   = (1 << 4),
	FLAGC_ZEROPAD   = (1 << 5),
	FLAGC_WIDTH     = (1 << 6),
	FLAGC_PRECISION = (1 << 7),
	FLAGC_STAR1     = (1 << 8),
	FLAGC_STAR2     = (1 << 9),
	FLAGC_LENMOD    = (1 << 10) /* length modifier */
};

/* ------------------------------------------------------------------ */

static const hawk_bch_t* m_hex2ascii =
	HAWK_BT("0123456789abcdefghijklmnopqrstuvwxyz");
static const hawk_uch_t* w_hex2ascii =
	HAWK_UT("0123456789abcdefghijklmnopqrstuvwxyz");

/* ------------------------------------------------------------------ */

#undef char_t
#undef uchar_t
#undef ochar_t
#undef T
#undef OT
#undef CONV_MAX
#undef toupper
#undef hex2ascii
#undef sprintn
#undef fmtout_t
#undef fmtout

#define char_t hawk_bch_t
#define uchar_t hawk_bch_t
#define ochar_t hawk_uch_t
#define T(x) HAWK_BT(x)
#define OT(x) HAWK_UT(x)
#define CONV_MAX HAWK_MBLEN_MAX
#define toupper HAWK_TOUPPER
#define sprintn m_sprintn
#define fmtout_t hawk_mfmtout_t
#define fmtout hawk_mfmtout

#define hex2ascii(hex)  (m_hex2ascii[hex])

#include "fmt-out.h"

/* ------------------------------------------------------------------ */

#undef char_t
#undef uchar_t
#undef ochar_t
#undef T
#undef OT
#undef CONV_MAX
#undef toupper
#undef hex2ascii
#undef sprintn
#undef fmtout_t
#undef fmtout

#define char_t hawk_uch_t
#define uchar_t hawk_uch_t
#define ochar_t hawk_bch_t
#define T(x) HAWK_UT(x)
#define OT(x) HAWK_BT(x)
#define CONV_MAX 1
#define toupper HAWK_TOWUPPER
#define sprintn w_sprintn
#define fmtout_t hawk_wfmtout_t
#define fmtout hawk_wfmtout

#define hex2ascii(hex)  (w_hex2ascii[hex])

#include "fmt-out.h"

