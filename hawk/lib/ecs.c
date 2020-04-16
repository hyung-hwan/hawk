/*
 * $Id$
 *
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

#include "hawk-prv.h"

#define _FN(type,verb) hawk_ ## type  ## _ ## verb

/* ------------------------------------------------------------------------- */

#undef FN
#undef T
#undef str_t
#undef char_t
#undef cstr_t
#undef count_chars
#define FN(verb) _FN(becs,verb)
#define T(x) HAWK_BT(x)
#define str_t hawk_becs_t
#define char_t hawk_bch_t
#define cstr_t hawk_bcs_t
#define count_chars(x) hawk_count_bcstr(x)
#define BUILD_BECS
#include "ecs-imp.h"

/* ------------------------------------------------------------------------- */

#undef FN
#undef T
#undef str_t
#undef char_t
#undef cstr_t
#undef count_chars
#define FN(verb) _FN(uecs,verb)
#define T(x) HAWK_UT(x)
#define str_t hawk_uecs_t
#define char_t hawk_uch_t
#define cstr_t hawk_ucs_t
#define count_chars(x) hawk_count_ucstr(x)
#define BUILD_UECS
#include "ecs-imp.h"

/* ------------------------------------------------------------------------- */


hawk_oow_t hawk_becs_ncatuchars (hawk_becs_t* str, const hawk_uch_t* s, hawk_oow_t len, hawk_cmgr_t* cmgr)
{
	hawk_oow_t bcslen, ucslen;

	ucslen = len;
	if (hawk_conv_uchars_to_bchars_with_cmgr(s, &ucslen, HAWK_NULL, &bcslen, cmgr) <= -1) return (hawk_oow_t)-1;

	if (hawk_becs_resize_for_ncat(str, bcslen) <= 0) return -1;

	ucslen = len;
	bcslen = str->capa - str->val.len;
	hawk_conv_uchars_to_bchars_with_cmgr (s, &ucslen, &str->val.ptr[str->val.len], &bcslen, cmgr);
	str->val.len += bcslen;
	str->val.ptr[str->val.len] = '\0';

	return str->val.len;
}

hawk_oow_t hawk_uecs_ncatbchars (hawk_uecs_t* str, const hawk_bch_t* s, hawk_oow_t len, hawk_cmgr_t* cmgr, int all)
{
	hawk_oow_t bcslen, ucslen;

	bcslen = len;
	if (hawk_conv_bchars_to_uchars_with_cmgr(s, &bcslen, HAWK_NULL, &ucslen, cmgr, all) <= -1) return (hawk_oow_t)-1;

	if (hawk_uecs_resize_for_ncat(str, ucslen) <= 0) return -1;

	bcslen = len;
	ucslen = str->capa - str->val.len;
	hawk_conv_bchars_to_uchars_with_cmgr (s, &bcslen, &str->val.ptr[str->val.len], &ucslen, cmgr, all);
	str->val.len += ucslen;
	str->val.ptr[str->val.len] = '\0';

	return str->val.len;
}
