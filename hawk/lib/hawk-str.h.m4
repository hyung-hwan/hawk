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
 * Do NOT edit hawk-str.h. Edit hawk-str.h.m4 instead.
 *
 * Generate hawk-str.h.m4 with m4
 *   $ m4 hawk-str.h.m4 > hawk-str.h
 */

#ifndef _HAWK_STR_H_
#define _HAWK_STR_H_

#include <hawk-cmn.h>

dnl ---------------------------------------------------------------------------
dnl include utl-str.m4 for c++ template functions far below
include(`utl-str.m4')
dnl ---------------------------------------------------------------------------

/* ========================================================================= 
 * STRING
 * ========================================================================= */

enum hawk_trim_oochars_flag_t
{
        HAWK_TRIM_OOCHARS_LEFT  = (1 << 0), /**< trim leading spaces */
#define HAWK_TRIM_OOCHARS_LEFT HAWK_TRIM_OOCHARS_LEFT
#define HAWK_TRIM_UCHARS_LEFT HAWK_TRIM_OOCHARS_LEFT
#define HAWK_TRIM_BCHARS_LEFT HAWK_TRIM_OOCHARS_LEFT
        HAWK_TRIM_OOCHARS_RIGHT = (1 << 1)  /**< trim trailing spaces */
#define HAWK_TRIM_OOCHARS_RIGHT HAWK_TRIM_OOCHARS_RIGHT
#define HAWK_TRIM_UCHARS_RIGHT HAWK_TRIM_OOCHARS_RIGHT
#define HAWK_TRIM_BCHARS_RIGHT HAWK_TRIM_OOCHARS_RIGHT
};

#if defined(__cplusplus)
extern "C" {
#endif


/* ------------------------------------ */

HAWK_EXPORT int hawk_comp_uchars (
	const hawk_uch_t* str1,
	hawk_oow_t        len1,
	const hawk_uch_t* str2,
	hawk_oow_t        len2,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_bchars (
	const hawk_bch_t* str1,
	hawk_oow_t        len1,
	const hawk_bch_t* str2,
	hawk_oow_t        len2,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_ucstr (
	const hawk_uch_t* str1,
	const hawk_uch_t* str2,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_bcstr (
	const hawk_bch_t* str1,
	const hawk_bch_t* str2,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_ucstr_limited (
	const hawk_uch_t* str1,
	const hawk_uch_t* str2,
	hawk_oow_t        maxlen,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_bcstr_limited (
	const hawk_bch_t* str1,
	const hawk_bch_t* str2,
	hawk_oow_t        maxlen,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_uchars_ucstr (
	const hawk_uch_t* str1,
	hawk_oow_t        len,
	const hawk_uch_t* str2,
	int              ignorecase
);

HAWK_EXPORT int hawk_comp_bchars_bcstr (
	const hawk_bch_t* str1,
	hawk_oow_t        len,
	const hawk_bch_t* str2,
	int              ignorecase
);

/* ------------------------------------ */

HAWK_EXPORT hawk_oow_t hawk_concat_uchars_to_ucstr (
	hawk_uch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_uch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_concat_bchars_to_bcstr (
	hawk_bch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_bch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_concat_ucstr (
	hawk_uch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_uch_t* src
);

HAWK_EXPORT hawk_oow_t hawk_concat_bcstr (
	hawk_bch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_bch_t* src
);

/* ------------------------------------ */

HAWK_EXPORT void hawk_copy_uchars (
	hawk_uch_t*       dst,
	const hawk_uch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT void hawk_copy_bchars (
	hawk_bch_t*       dst,
	const hawk_bch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT void hawk_copy_bchars_to_uchars (
	hawk_uch_t*       dst,
	const hawk_bch_t* src,
	hawk_oow_t        len
);
HAWK_EXPORT void hawk_copy_uchars_to_bchars (
	hawk_bch_t*       dst,
	const hawk_uch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_copy_uchars_to_ucstr (
	hawk_uch_t*       dst,
	hawk_oow_t        dlen,
	const hawk_uch_t* src,
	hawk_oow_t        slen
);

HAWK_EXPORT hawk_oow_t hawk_copy_bchars_to_bcstr (
	hawk_bch_t*       dst,
	hawk_oow_t        dlen,
	const hawk_bch_t* src,
	hawk_oow_t        slen
);

HAWK_EXPORT hawk_oow_t hawk_copy_uchars_to_ucstr_unlimited (
	hawk_uch_t*       dst,
	const hawk_uch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_copy_bchars_to_bcstr_unlimited (
	hawk_bch_t*       dst,
	const hawk_bch_t* src,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_copy_ucstr_to_uchars (
	hawk_uch_t*        dst,
	hawk_oow_t         dlen,
	const hawk_uch_t*  src
);

HAWK_EXPORT hawk_oow_t hawk_copy_bcstr_to_bchars (
	hawk_bch_t*        dst,
	hawk_oow_t         dlen,
	const hawk_bch_t*  src
);

HAWK_EXPORT hawk_oow_t hawk_copy_ucstr (
	hawk_uch_t*       dst,
	hawk_oow_t        len,
	const hawk_uch_t* src
);

HAWK_EXPORT hawk_oow_t hawk_copy_bcstr (
	hawk_bch_t*       dst,
	hawk_oow_t        len,
	const hawk_bch_t* src
);

HAWK_EXPORT hawk_oow_t hawk_copy_ucstr_unlimited (
	hawk_uch_t*       dst,
	const hawk_uch_t* src
);

HAWK_EXPORT hawk_oow_t hawk_copy_bcstr_unlimited (
	hawk_bch_t*       dst,
	const hawk_bch_t* src
);

HAWK_EXPORT hawk_oow_t hawk_copy_fmt_ucstrs_to_ucstr (
	hawk_uch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_uch_t* fmt,
	const hawk_uch_t* str[]
);

HAWK_EXPORT hawk_oow_t hawk_copy_fmt_bcstrs_to_bcstr (
	hawk_bch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_bch_t* fmt,
	const hawk_bch_t* str[]
);

HAWK_EXPORT hawk_oow_t hawk_copy_fmt_ucses_to_ucstr (
	hawk_uch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_uch_t* fmt,
	const hawk_ucs_t  str[]
);

HAWK_EXPORT hawk_oow_t hawk_copy_fmt_bcses_to_bcstr (
	hawk_bch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_bch_t* fmt,
	const hawk_bcs_t  str[]
);

/* ------------------------------------ */

HAWK_EXPORT hawk_oow_t hawk_count_ucstr (
	const hawk_uch_t* str
);

HAWK_EXPORT hawk_oow_t hawk_count_bcstr (
	const hawk_bch_t* str
);

HAWK_EXPORT hawk_oow_t hawk_count_ucstr_limited (
	const hawk_uch_t* str,
	hawk_oow_t        maxlen
);

HAWK_EXPORT hawk_oow_t hawk_count_bcstr_limited (
	const hawk_bch_t* str,
	hawk_oow_t        maxlen
);

/* ------------------------------------ */

/**
 * The hawk_equal_uchars() function determines equality of two strings
 * of the same length \a len.
 */
HAWK_EXPORT int hawk_equal_uchars (
	const hawk_uch_t* str1,
	const hawk_uch_t* str2,
	hawk_oow_t        len
);

HAWK_EXPORT int hawk_equal_bchars (
	const hawk_bch_t* str1,
	const hawk_bch_t* str2,
	hawk_oow_t        len
);

/* ------------------------------------ */


HAWK_EXPORT void hawk_fill_uchars (
	hawk_uch_t*       dst,
	const hawk_uch_t  ch,
	hawk_oow_t        len
);

HAWK_EXPORT void hawk_fill_bchars (
	hawk_bch_t*       dst,
	const hawk_bch_t  ch,
	hawk_oow_t        len
);

/* ------------------------------------ */

HAWK_EXPORT const hawk_bch_t* hawk_find_bcstr_word_in_bcstr (
	const hawk_bch_t* str,
	const hawk_bch_t* word,
	hawk_bch_t        extra_delim,
	int              ignorecase
);

HAWK_EXPORT const hawk_uch_t* hawk_find_ucstr_word_in_ucstr (
	const hawk_uch_t* str,
	const hawk_uch_t* word,
	hawk_uch_t        extra_delim,
	int              ignorecase
);

HAWK_EXPORT hawk_uch_t* hawk_find_uchar_in_uchars (
	const hawk_uch_t* ptr,
	hawk_oow_t        len,
	hawk_uch_t        c
);

HAWK_EXPORT hawk_bch_t* hawk_find_bchar_in_bchars (
	const hawk_bch_t* ptr,
	hawk_oow_t        len,
	hawk_bch_t        c
);

HAWK_EXPORT hawk_uch_t* hawk_rfind_uchar_in_uchars (
	const hawk_uch_t* ptr,
	hawk_oow_t        len,
	hawk_uch_t        c
);

HAWK_EXPORT hawk_bch_t* hawk_rfind_bchar_in_bchars (
	const hawk_bch_t* ptr,
	hawk_oow_t        len,
	hawk_bch_t        c
);

HAWK_EXPORT hawk_uch_t* hawk_find_uchar_in_ucstr (
	const hawk_uch_t* ptr,
	hawk_uch_t        c
);

HAWK_EXPORT hawk_bch_t* hawk_find_bchar_in_bcstr (
	const hawk_bch_t* ptr,
	hawk_bch_t        c
);

HAWK_EXPORT hawk_uch_t* hawk_rfind_uchar_in_ucstr (
	const hawk_uch_t* ptr,
	hawk_uch_t        c
);

HAWK_EXPORT hawk_bch_t* hawk_rfind_bchar_in_bcstr (
	const hawk_bch_t* ptr,
	hawk_bch_t        c
);

HAWK_EXPORT hawk_uch_t* hawk_find_uchars_in_uchars (
	const hawk_uch_t* str,
	hawk_oow_t        strsz,
	const hawk_uch_t* sub,
	hawk_oow_t        subsz,
	int              inorecase
);

HAWK_EXPORT hawk_bch_t* hawk_find_bchars_in_bchars (
	const hawk_bch_t* str,
	hawk_oow_t        strsz,
	const hawk_bch_t* sub,
	hawk_oow_t        subsz,
	int              inorecase
);

HAWK_EXPORT hawk_uch_t* hawk_rfind_uchars_in_uchars (
	const hawk_uch_t* str,
	hawk_oow_t        strsz,
	const hawk_uch_t* sub,
	hawk_oow_t        subsz,
	int              inorecase
);

HAWK_EXPORT hawk_bch_t* hawk_rfind_bchars_in_bchars (
	const hawk_bch_t* str,
	hawk_oow_t        strsz,
	const hawk_bch_t* sub,
	hawk_oow_t        subsz,
	int              inorecase
);

/* ------------------------------------ */

HAWK_EXPORT hawk_oow_t hawk_compact_uchars (
	hawk_uch_t*       str,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_compact_bchars (
	hawk_bch_t*       str,
	hawk_oow_t        len
);


HAWK_EXPORT hawk_oow_t hawk_rotate_uchars (
	hawk_uch_t*       str,
	hawk_oow_t        len,
	int               dir,
	hawk_oow_t        n
);

HAWK_EXPORT hawk_oow_t hawk_rotate_bchars (
	hawk_bch_t*       str,
	hawk_oow_t        len,
	int               dir,
	hawk_oow_t        n
);

HAWK_EXPORT hawk_uch_t* hawk_tokenize_uchars (
	const hawk_uch_t* s,
	hawk_oow_t        len,
	const hawk_uch_t* delim,
	hawk_oow_t        delim_len,
	hawk_ucs_t*       tok,
	int               ignorecase
);

HAWK_EXPORT hawk_bch_t* hawk_tokenize_bchars (
	const hawk_bch_t* s,
	hawk_oow_t        len,
	const hawk_bch_t* delim,
	hawk_oow_t        delim_len,
	hawk_bcs_t*       tok,
	int               ignorecase
);

HAWK_EXPORT hawk_uch_t* hawk_trim_uchars (
	const hawk_uch_t* str,
	hawk_oow_t*       len,
	int               flags
);

HAWK_EXPORT hawk_bch_t* hawk_trim_bchars (
	const hawk_bch_t* str,
	hawk_oow_t*       len,
	int               flags
);

HAWK_EXPORT int hawk_split_ucstr (
	hawk_uch_t*       s,
	const hawk_uch_t* delim,
	hawk_uch_t        lquote,
	hawk_uch_t        rquote,
	hawk_uch_t        escape
);

HAWK_EXPORT int hawk_split_bcstr (
	hawk_bch_t*       s,
	const hawk_bch_t* delim,
	hawk_bch_t        lquote,
	hawk_bch_t        rquote,
	hawk_bch_t        escape
);


#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_count_oocstr hawk_count_ucstr
#	define hawk_count_oocstr_limited hawk_count_ucstr_limited

#	define hawk_equal_oochars hawk_equal_uchars
#	define hawk_comp_oochars hawk_comp_uchars
#	define hawk_comp_oocstr_bcstr hawk_comp_ucstr_bcstr
#	define hawk_comp_oochars_bcstr hawk_comp_uchars_bcstr
#	define hawk_comp_oochars_ucstr hawk_comp_uchars_ucstr
#	define hawk_comp_oochars_oocstr hawk_comp_uchars_ucstr
#	define hawk_comp_oocstr hawk_comp_ucstr
#	define hawk_comp_oocstr_limited hawk_comp_ucstr_limited

#	define hawk_copy_oochars hawk_copy_uchars
#	define hawk_copy_bchars_to_oochars hawk_copy_bchars_to_uchars
#	define hawk_copy_oochars_to_bchars hawk_copy_uchars_to_bchars
#	define hawk_copy_uchars_to_oochars hawk_copy_uchars
#	define hawk_copy_oochars_to_uchars hawk_copy_uchars

#	define hawk_copy_oochars_to_oocstr hawk_copy_uchars_to_ucstr
#	define hawk_copy_oochars_to_oocstr_unlimited hawk_copy_uchars_to_ucstr_unlimited
#	define hawk_copy_oocstr hawk_copy_ucstr
#	define hawk_copy_oocstr_unlimited hawk_copy_ucstr_unlimited
#	define hawk_copy_fmt_oocses_to_oocstr hawk_copy_fmt_ucses_to_ucstr
#	define hawk_copy_fmt_oocstr_to_oocstr hawk_copy_fmt_ucstr_to_ucstr

#	define hawk_concat_oochars_to_ucstr hawk_concat_uchars_to_ucstr
#	define hawk_concat_oocstr hawk_concat_ucstr

#	define hawk_fill_oochars hawk_fill_uchars
#	define hawk_find_oocstr_word_in_oocstr hawk_find_ucstr_word_in_ucstr
#	define hawk_find_oochar_in_oochars hawk_find_uchar_in_uchars
#	define hawk_rfind_oochar_in_oochars hawk_rfind_uchar_in_uchars
#	define hawk_find_oochar_in_oocstr hawk_find_uchar_in_ucstr
#	define hawk_find_oochars_in_oochars hawk_find_uchars_in_uchars
#	define hawk_rfind_oochars_in_oochars hawk_rfind_uchars_in_uchars

#	define hawk_compact_oochars hawk_compact_uchars
#	define hawk_rotate_oochars hawk_rotate_uchars
#	define hawk_tokenize_oochars hawk_tokenize_uchars
#	define hawk_trim_oochars hawk_trim_uchars
#	define hawk_split_oocstr hawk_split_ucstr
#else
#	define hawk_count_oocstr hawk_count_bcstr
#	define hawk_count_oocstr_limited hawk_count_bcstr_limited

#	define hawk_equal_oochars hawk_equal_bchars
#	define hawk_comp_oochars hawk_comp_bchars
#	define hawk_comp_oocstr_bcstr hawk_comp_bcstr
#	define hawk_comp_oochars_bcstr hawk_comp_bchars_bcstr
#	define hawk_comp_oochars_ucstr hawk_comp_bchars_ucstr
#	define hawk_comp_oochars_oocstr hawk_comp_bchars_bcstr
#	define hawk_comp_oocstr hawk_comp_bcstr
#	define hawk_comp_oocstr_limited hawk_comp_bcstr_limited

#	define hawk_copy_oochars hawk_copy_bchars
#	define hawk_copy_bchars_to_oochars hawk_copy_bchars
#	define hawk_copy_oochars_to_bchars hawk_copy_bchars
#	define hawk_copy_uchars_to_oochars hawk_copy_uchars_to_bchars
#	define hawk_copy_oochars_to_uchars hawk_copy_bchars_to_uchars

#	define hawk_copy_oochars_to_oocstr hawk_copy_bchars_to_bcstr
#	define hawk_copy_oochars_to_oocstr_unlimited hawk_copy_bchars_to_bcstr_unlimited
#	define hawk_copy_oocstr hawk_copy_bcstr
#	define hawk_copy_oocstr_unlimited hawk_copy_bcstr_unlimited
#	define hawk_copy_fmt_oocses_to_oocstr hawk_copy_fmt_bcses_to_bcstr
#	define hawk_copy_fmt_oocstr_to_oocstr hawk_copy_fmt_bcstr_to_bcstr

#	define hawk_concat_oochars_to_bcstr hawk_concat_bchars_to_bcstr
#	define hawk_concat_oocstr hawk_concat_bcstr

#	define hawk_fill_oochars hawk_fill_bchars
#	define hawk_find_oocstr_word_in_oocstr hawk_find_bcstr_word_in_bcstr
#	define hawk_find_oochar_in_oochars hawk_find_bchar_in_bchars
#	define hawk_rfind_oochar_in_oochars hawk_rfind_bchar_in_bchars
#	define hawk_find_oochar_in_oocstr hawk_find_bchar_in_bcstr
#	define hawk_find_oochars_in_oochars hawk_find_bchars_in_bchars
#	define hawk_rfind_oochars_in_oochars hawk_rfind_bchars_in_bchars

#	define hawk_compact_oochars hawk_compact_uchars
#	define hawk_rotate_oochars hawk_rotate_uchars
#	define hawk_tokenize_oochars hawk_tokenize_bchars
#	define hawk_trim_oochars hawk_trim_bchars
#	define hawk_split_oocstr hawk_split_bcstr
#endif

/* ------------------------------------------------------------------------- */

#define HAWK_BYTE_TO_OOCSTR_RADIXMASK (0xFF)
#define HAWK_BYTE_TO_OOCSTR_LOWERCASE (1 << 8)

#define HAWK_BYTE_TO_UCSTR_RADIXMASK HAWK_BYTE_TO_OOCSTR_RADIXMASK
#define HAWK_BYTE_TO_UCSTR_LOWERCASE HAWK_BYTE_TO_OOCSTR_LOWERCASE

#define HAWK_BYTE_TO_BCSTR_RADIXMASK HAWK_BYTE_TO_OOCSTR_RADIXMASK
#define HAWK_BYTE_TO_BCSTR_LOWERCASE HAWK_BYTE_TO_OOCSTR_LOWERCASE

HAWK_EXPORT hawk_oow_t hawk_byte_to_bcstr (
	hawk_uint8_t   byte,  
	hawk_bch_t*    buf,
	hawk_oow_t     size,
	int           flagged_radix,
	hawk_bch_t     fill
);

HAWK_EXPORT hawk_oow_t hawk_byte_to_ucstr (
	hawk_uint8_t   byte,  
	hawk_uch_t*    buf,
	hawk_oow_t     size,
	int           flagged_radix,
	hawk_uch_t     fill
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_byte_to_oocstr hawk_byte_to_ucstr
#else
#	define hawk_byte_to_oocstr hawk_byte_to_bcstr
#endif

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_int_to_ucstr (
	hawk_int_t     value, 
	int              radix, 
	const hawk_uch_t* prefix,
	hawk_uch_t*       buf,
	hawk_oow_t        size
);

HAWK_EXPORT hawk_oow_t hawk_int_to_bcstr (
	hawk_int_t     value, 
	int              radix, 
	const hawk_bch_t* prefix,
	hawk_bch_t*       buf,
	hawk_oow_t        size
);

HAWK_EXPORT hawk_oow_t hawk_uint_to_ucstr (
	hawk_uint_t     value, 
	int              radix, 
	const hawk_uch_t* prefix,
	hawk_uch_t*       buf,
	hawk_oow_t        size
);

HAWK_EXPORT hawk_oow_t hawk_uint_to_bcstr (
	hawk_uint_t     value, 
	int              radix, 
	const hawk_bch_t* prefix,
	hawk_bch_t*       buf,
	hawk_oow_t        size
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_int_to_oocstr hawk_int_to_ucstr
#	define hawk_uint_to_oocstr hawk_uint_to_ucstr
#else
#	define hawk_int_to_oocstr hawk_int_to_bcstr
#	define hawk_uint_to_oocstr hawk_uint_to_bcstr
#endif


/* ------------------------------------------------------------------------- */

#define HAWK_CHARS_TO_INT_MAKE_OPTION(e,ltrim,rtrim,base) (((!!(e)) << 0) | ((!!(ltrim)) << 2) | ((!!(rtrim)) << 3) | ((base) << 8))
#define HAWK_CHARS_TO_INT_GET_OPTION_E(option) ((option) & 1)
#define HAWK_CHARS_TO_INT_GET_OPTION_LTRIM(option) ((option) & 4)
#define HAWK_CHARS_TO_INT_GET_OPTION_RTRIM(option) ((option) & 8)
#define HAWK_CHARS_TO_INT_GET_OPTION_BASE(option) ((option) >> 8)

#define HAWK_CHARS_TO_UINT_MAKE_OPTION(e,ltrim,rtrim,base) (((!!(e)) << 0) | ((!!(ltrim)) << 2) | ((!!(rtrim)) << 3) | ((base) << 8))
#define HAWK_CHARS_TO_UINT_GET_OPTION_E(option) ((option) & 1)
#define HAWK_CHARS_TO_UINT_GET_OPTION_LTRIM(option) ((option) & 4)
#define HAWK_CHARS_TO_UINT_GET_OPTION_RTRIM(option) ((option) & 8)
#define HAWK_CHARS_TO_UINT_GET_OPTION_BASE(option) ((option) >> 8)

#define HAWK_OOCHARS_TO_INTMAX_MAKE_OPTION(e,ltrim,rtrim,base)  HAWK_CHARS_TO_INT_MAKE_OPTION(e,ltrim,rtrim,base)
#define HAWK_OOCHARS_TO_INTMAX_GET_OPTION_E(option)             HAWK_CHARS_TO_INT_GET_OPTION_E(option)
#define HAWK_OOCHARS_TO_INTMAX_GET_OPTION_LTRIM(option)         HAWK_CHARS_TO_INT_GET_OPTION_LTRIM(option)
#define HAWK_OOCHARS_TO_INTMAX_GET_OPTION_RTRIM(option)         HAWK_CHARS_TO_INT_GET_OPTION_RTRIM(option)
#define HAWK_OOCHARS_TO_INTMAX_GET_OPTION_BASE(option)          HAWK_CHARS_TO_INT_GET_OPTION_BASE(option)

#define HAWK_OOCHARS_TO_UINTMAX_MAKE_OPTION(e,ltrim,rtrim,base) HAWK_CHARS_TO_UINT_MAKE_OPTION(e,ltrim,rtrim,base)
#define HAWK_OOCHARS_TO_UINTMAX_GET_OPTION_E(option)            HAWK_CHARS_TO_UINT_GET_OPTION_E(option)
#define HAWK_OOCHARS_TO_UINTMAX_GET_OPTION_LTRIM(option)        HAWK_CHARS_TO_UINT_GET_OPTION_LTRIM(option)
#define HAWK_OOCHARS_TO_UINTMAX_GET_OPTION_RTRIM(option)        HAWK_CHARS_TO_UINT_GET_OPTION_RTRIM(option)
#define HAWK_OOCHARS_TO_UINTMAX_GET_OPTION_BASE(option)         HAWK_CHARS_TO_UINT_GET_OPTION_BASE(option)

#define HAWK_UCHARS_TO_INTMAX_MAKE_OPTION(e,ltrim,rtrim,base)   HAWK_CHARS_TO_INT_MAKE_OPTION(e,ltrim,rtrim,base)
#define HAWK_UCHARS_TO_INTMAX_GET_OPTION_E(option)              HAWK_CHARS_TO_INT_GET_OPTION_E(option)
#define HAWK_UCHARS_TO_INTMAX_GET_OPTION_LTRIM(option)          HAWK_CHARS_TO_INT_GET_OPTION_LTRIM(option)
#define HAWK_UCHARS_TO_INTMAX_GET_OPTION_RTRIM(option)          HAWK_CHARS_TO_INT_GET_OPTION_RTRIM(option)
#define HAWK_UCHARS_TO_INTMAX_GET_OPTION_BASE(option)           HAWK_CHARS_TO_INT_GET_OPTION_BASE(option)

#define HAWK_BCHARS_TO_INTMAX_MAKE_OPTION(e,ltrim,rtrim,base)   HAWK_CHARS_TO_INT_MAKE_OPTION(e,ltrim,rtrim,base)
#define HAWK_BCHARS_TO_INTMAX_GET_OPTION_E(option)              HAWK_CHARS_TO_INT_GET_OPTION_E(option)
#define HAWK_BCHARS_TO_INTMAX_GET_OPTION_LTRIM(option)          HAWK_CHARS_TO_INT_GET_OPTION_LTRIM(option)
#define HAWK_BCHARS_TO_INTMAX_GET_OPTION_RTRIM(option)          HAWK_CHARS_TO_INT_GET_OPTION_RTRIM(option)
#define HAWK_BCHARS_TO_INTMAX_GET_OPTION_BASE(option)           HAWK_CHARS_TO_INT_GET_OPTION_BASE(option)

#define HAWK_UCHARS_TO_UINTMAX_MAKE_OPTION(e,ltrim,rtrim,base)  HAWK_CHARS_TO_UINT_MAKE_OPTION(e,ltrim,rtrim,base)
#define HAWK_UCHARS_TO_UINTMAX_GET_OPTION_E(option)             HAWK_CHARS_TO_UINT_GET_OPTION_E(option)
#define HAWK_UCHARS_TO_UINTMAX_GET_OPTION_LTRIM(option)         HAWK_CHARS_TO_UINT_GET_OPTION_LTRIM(option)
#define HAWK_UCHARS_TO_UINTMAX_GET_OPTION_RTRIM(option)         HAWK_CHARS_TO_UINT_GET_OPTION_RTRIM(option)
#define HAWK_UCHARS_TO_UINTMAX_GET_OPTION_BASE(option)          HAWK_CHARS_TO_UINT_GET_OPTION_BASE(option)

#define HAWK_BCHARS_TO_UINTMAX_MAKE_OPTION(e,ltrim,rtrim,base)  HAWK_CHARS_TO_UINT_MAKE_OPTION(e,ltrim,rtrim,base)
#define HAWK_BCHARS_TO_UINTMAX_GET_OPTION_E(option)             HAWK_CHARS_TO_UINT_GET_OPTION_E(option)
#define HAWK_BCHARS_TO_UINTMAX_GET_OPTION_LTRIM(option)         HAWK_CHARS_TO_UINT_GET_OPTION_LTRIM(option)
#define HAWK_BCHARS_TO_UINTMAX_GET_OPTION_RTRIM(option)         HAWK_CHARS_TO_UINT_GET_OPTION_RTRIM(option)
#define HAWK_BCHARS_TO_UINTMAX_GET_OPTION_BASE(option)          HAWK_CHARS_TO_UINT_GET_OPTION_BASE(option)

HAWK_EXPORT hawk_int_t hawk_uchars_to_int (
	const hawk_uch_t*  str,
	hawk_oow_t         len,
	int               option,
	const hawk_uch_t** endptr,
	int*              is_sober
);

HAWK_EXPORT hawk_int_t hawk_bchars_to_int (
	const hawk_bch_t*  str,
	hawk_oow_t         len,
	int               option,
	const hawk_bch_t** endptr,
	int*              is_sober
);

HAWK_EXPORT hawk_uint_t hawk_uchars_to_uint (
	const hawk_uch_t*  str,
	hawk_oow_t         len,
	int               option,
	const hawk_uch_t** endptr,
	int*              is_sober
);

HAWK_EXPORT hawk_uint_t hawk_bchars_to_uint (
	const hawk_bch_t*  str,
	hawk_oow_t         len,
	int               option,
	const hawk_bch_t** endptr,
	int*              is_sober
);
#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_oochars_to_int hawk_uchars_to_int
#	define hawk_oochars_to_uint hawk_uchars_to_uint
#else
#	define hawk_oochars_to_int hawk_bchars_to_int
#	define hawk_oochars_to_uint hawk_bchars_to_uint
#endif

#if defined(__cplusplus)
}
#endif

/* Some C++ utilities below */
#if defined(__cplusplus)

/*
static inline bool is_space (char c) { return isspace(c); }
static inline bool is_wspace (wchar_t c) { return iswspace(c); }
unsigned x = hawk_chars_to_uint<char,unsigned,is_space>("0x12345", 7, 0, NULL, NULL);
unsigned y = hawk_chars_to_uint<wchar_t,unsigned,is_wspace>(L"0x12345", 7, 0, NULL, NULL);
int a = hawk_chars_to_int<char,int,is_space>("-0x12345", 8, 0, NULL, NULL);
int b = hawk_chars_to_int<wchar_t,int,is_wspace>(L"-0x12345", 8, 0, NULL, NULL);
*/
template<typename CHAR_TYPE, typename INT_TYPE, bool(*IS_SPACE)(CHAR_TYPE)>fn_chars_to_int(hawk_chars_to_int, CHAR_TYPE, INT_TYPE, IS_SPACE, HAWK_CHARS_TO_INT)
template<typename CHAR_TYPE, typename UINT_TYPE, bool(*IS_SPACE)(CHAR_TYPE)>fn_chars_to_uint(hawk_chars_to_uint, CHAR_TYPE, UINT_TYPE, IS_SPACE, HAWK_CHARS_TO_UINT)

#endif

#endif
