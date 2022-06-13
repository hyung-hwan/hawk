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
 * Do NOT edit utl-str.c. Edit utl-str.c.m4 instead.
 * 
 * Generate utl-str.c with m4
 *   $ m4 utl-str.c.m4 > utl-str.c  
 */

#include "hawk-prv.h"
#include <hawk-chr.h>
dnl
dnl ---------------------------------------------------------------------------
include(`utl-str.m4')dnl
dnl ---------------------------------------------------------------------------
dnl --
fn_comp_chars(hawk_comp_uchars, hawk_uch_t, hawk_uchu_t, hawk_to_uch_lower)
fn_comp_chars(hawk_comp_bchars, hawk_bch_t, hawk_bchu_t, hawk_to_bch_lower)
dnl --
fn_comp_cstr(hawk_comp_ucstr, hawk_uch_t, hawk_uchu_t, hawk_to_uch_lower)
fn_comp_cstr(hawk_comp_bcstr, hawk_bch_t, hawk_bchu_t, hawk_to_bch_lower)
dnl --
fn_comp_cstr_limited(hawk_comp_ucstr_limited, hawk_uch_t, hawk_uchu_t, hawk_to_uch_lower)
fn_comp_cstr_limited(hawk_comp_bcstr_limited, hawk_bch_t, hawk_bchu_t, hawk_to_bch_lower)
dnl --
fn_comp_chars_cstr(hawk_comp_uchars_ucstr, hawk_uch_t, hawk_uchu_t, hawk_to_uch_lower)
fn_comp_chars_cstr(hawk_comp_bchars_bcstr, hawk_bch_t, hawk_bchu_t, hawk_to_bch_lower)
dnl --
fn_concat_chars_to_cstr(hawk_concat_uchars_to_ucstr, hawk_uch_t, hawk_count_ucstr)
fn_concat_chars_to_cstr(hawk_concat_bchars_to_bcstr, hawk_bch_t, hawk_count_bcstr)
dnl --
fn_concat_cstr(hawk_concat_ucstr, hawk_uch_t, hawk_count_ucstr)
fn_concat_cstr(hawk_concat_bcstr, hawk_bch_t, hawk_count_bcstr)
dnl --
fn_copy_chars(hawk_copy_uchars, hawk_uch_t)
fn_copy_chars(hawk_copy_bchars, hawk_bch_t)
dnl --
fn_copy_chars_to_cstr(hawk_copy_uchars_to_ucstr, hawk_uch_t)
fn_copy_chars_to_cstr(hawk_copy_bchars_to_bcstr, hawk_bch_t)
dnl --
fn_copy_chars_to_cstr_unlimited(hawk_copy_uchars_to_ucstr_unlimited, hawk_uch_t)
fn_copy_chars_to_cstr_unlimited(hawk_copy_bchars_to_bcstr_unlimited, hawk_bch_t)
dnl --
fn_copy_cstr_to_chars(hawk_copy_ucstr_to_uchars, hawk_uch_t)
fn_copy_cstr_to_chars(hawk_copy_bcstr_to_bchars, hawk_bch_t)
dnl --
fn_copy_cstr(hawk_copy_ucstr, hawk_uch_t)
fn_copy_cstr(hawk_copy_bcstr, hawk_bch_t)
dnl --
fn_copy_cstr_unlimited(hawk_copy_ucstr_unlimited, hawk_uch_t)
fn_copy_cstr_unlimited(hawk_copy_bcstr_unlimited, hawk_bch_t)
dnl --
fn_copy_fmt_cstrs_to_cstr(hawk_copy_fmt_ucstrs_to_ucstr, hawk_uch_t)
fn_copy_fmt_cstrs_to_cstr(hawk_copy_fmt_bcstrs_to_bcstr, hawk_bch_t)
dnl --
fn_copy_fmt_cses_to_cstr(hawk_copy_fmt_ucses_to_ucstr, hawk_uch_t, hawk_ucs_t)
fn_copy_fmt_cses_to_cstr(hawk_copy_fmt_bcses_to_bcstr, hawk_bch_t, hawk_bcs_t)
dnl --
fn_count_cstr(hawk_count_ucstr, hawk_uch_t)
fn_count_cstr(hawk_count_bcstr, hawk_bch_t)
dnl --
fn_count_cstr_limited(hawk_count_ucstr_limited, hawk_uch_t)
fn_count_cstr_limited(hawk_count_bcstr_limited, hawk_bch_t)
dnl --
fn_equal_chars(hawk_equal_uchars, hawk_uch_t)
fn_equal_chars(hawk_equal_bchars, hawk_bch_t)
dnl --
fn_fill_chars(hawk_fill_uchars, hawk_uch_t)
fn_fill_chars(hawk_fill_bchars, hawk_bch_t)
dnl --
fn_find_char_in_chars(hawk_find_uchar_in_uchars, hawk_uch_t)
fn_find_char_in_chars(hawk_find_bchar_in_bchars, hawk_bch_t)
dnl --
fn_rfind_char_in_chars(hawk_rfind_uchar_in_uchars, hawk_uch_t)
fn_rfind_char_in_chars(hawk_rfind_bchar_in_bchars, hawk_bch_t)
dnl --
fn_find_char_in_cstr(hawk_find_uchar_in_ucstr, hawk_uch_t)
fn_find_char_in_cstr(hawk_find_bchar_in_bcstr, hawk_bch_t)
dnl --
fn_rfind_char_in_cstr(hawk_rfind_uchar_in_ucstr, hawk_uch_t)
fn_rfind_char_in_cstr(hawk_rfind_bchar_in_bcstr, hawk_bch_t)
dnl --
fn_find_chars_in_chars(hawk_find_uchars_in_uchars, hawk_uch_t, hawk_to_uch_lower)
fn_find_chars_in_chars(hawk_find_bchars_in_bchars, hawk_bch_t, hawk_to_bch_lower)
dnl --
fn_rfind_chars_in_chars(hawk_rfind_uchars_in_uchars, hawk_uch_t, hawk_to_uch_lower)
fn_rfind_chars_in_chars(hawk_rfind_bchars_in_bchars, hawk_bch_t, hawk_to_bch_lower)
dnl --
fn_compact_chars(hawk_compact_uchars, hawk_uch_t, hawk_is_uch_space)
fn_compact_chars(hawk_compact_bchars, hawk_bch_t, hawk_is_bch_space)
dnl --
fn_rotate_chars(hawk_rotate_uchars, hawk_uch_t)
fn_rotate_chars(hawk_rotate_bchars, hawk_bch_t)
dnl --
fn_trim_chars(hawk_trim_uchars, hawk_uch_t, hawk_is_uch_space, HAWK_TRIM_UCHARS)
fn_trim_chars(hawk_trim_bchars, hawk_bch_t, hawk_is_bch_space, HAWK_TRIM_BCHARS)
dnl --
fn_split_cstr(hawk_split_ucstr, hawk_uch_t, hawk_is_uch_space, hawk_copy_ucstr_unlimited)
fn_split_cstr(hawk_split_bcstr, hawk_bch_t, hawk_is_bch_space, hawk_copy_bcstr_unlimited)
dnl --
fn_tokenize_chars(hawk_tokenize_uchars, hawk_uch_t, hawk_ucs_t, hawk_is_uch_space, hawk_to_uch_lower)
fn_tokenize_chars(hawk_tokenize_bchars, hawk_bch_t, hawk_bcs_t, hawk_is_bch_space, hawk_to_bch_lower)
dnl --
fn_byte_to_cstr(hawk_byte_to_ucstr, hawk_uch_t, HAWK_BYTE_TO_UCSTR)
fn_byte_to_cstr(hawk_byte_to_bcstr, hawk_bch_t, HAWK_BYTE_TO_BCSTR)
dnl --
fn_int_to_cstr(hawk_int_to_ucstr, hawk_uch_t, hawk_int_t, hawk_count_ucstr)
fn_int_to_cstr(hawk_int_to_bcstr, hawk_bch_t, hawk_int_t, hawk_count_bcstr)
fn_int_to_cstr(hawk_uint_to_ucstr, hawk_uch_t, hawk_uint_t, hawk_count_ucstr)
fn_int_to_cstr(hawk_uint_to_bcstr, hawk_bch_t, hawk_uint_t, hawk_count_bcstr)
dnl --
fn_chars_to_int(hawk_uchars_to_int, hawk_uch_t, hawk_int_t, hawk_is_uch_space, HAWK_UCHARS_TO_INTMAX)
fn_chars_to_int(hawk_bchars_to_int, hawk_bch_t, hawk_int_t, hawk_is_bch_space, HAWK_BCHARS_TO_INTMAX)
dnl --
fn_chars_to_uint(hawk_uchars_to_uint, hawk_uch_t, hawk_uint_t, hawk_is_uch_space, HAWK_UCHARS_TO_UINTMAX)
fn_chars_to_uint(hawk_bchars_to_uint, hawk_bch_t, hawk_uint_t, hawk_is_bch_space, HAWK_BCHARS_TO_UINTMAX)
