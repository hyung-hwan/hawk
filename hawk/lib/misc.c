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


#undef char_t
#undef xcs_t
#undef is_xch_space
#undef match_rex_with_xcs
#undef split_xchars_to_fields
#undef tokenize_xchars
#undef tokenize_xchars_by_rex

#define char_t hawk_bch_t
#define xcs_t hawk_bcs_t
#define is_xch_space hawk_is_bch_space
#define match_rex_with_xcs hawk_rtx_matchrexwithbcs

#define split_xchars_to_fields hawk_rtx_fldbchars
#define tokenize_xchars hawk_rtx_tokbcharswithbchars
#define tokenize_xchars_by_rex hawk_rtx_tokbcharsbyrex

#include "misc-imp.h"

#undef char_t
#undef xcs_t
#undef is_xch_space
#undef match_rex_with_xcs
#undef split_xchars_to_fields
#undef tokenize_xchars
#undef tokenize_xchars_by_rex

#define char_t hawk_uch_t
#define xcs_t hawk_ucs_t
#define is_xch_space hawk_is_uch_space
#define match_rex_with_xcs hawk_rtx_matchrexwithucs

#define split_xchars_to_fields hawk_rtx_flduchars
#define tokenize_xchars hawk_rtx_tokucharswithuchars
#define tokenize_xchars_by_rex hawk_rtx_tokucharsbyrex

#include "misc-imp.h"


static int matchtre_ucs (hawk_tre_t* tre, int opt, const hawk_ucs_t* str, hawk_ucs_t* mat, hawk_ucs_t submat[9], hawk_gem_t* errgem)
{
	int n;
	/*hawk_tre_match_t match[10] = { { 0, 0 }, };*/
	hawk_tre_match_t match[10];

	HAWK_MEMSET (match, 0, HAWK_SIZEOF(match));
	n = hawk_tre_execuchars(tre, str->ptr, str->len, match, HAWK_COUNTOF(match), opt, errgem);
	if (n <= -1)
	{
		if (hawk_gem_geterrnum(errgem) == HAWK_EREXNOMAT) return 0;
		return -1;
	}

	HAWK_ASSERT (match[0].rm_so != -1);
	if (mat)
	{
		mat->ptr = &str->ptr[match[0].rm_so];
		mat->len = match[0].rm_eo - match[0].rm_so;
	}

	if (submat)
	{
		int i;

		/* you must intialize submat before you pass into this 
		 * function because it can abort filling */
		for (i = 1; i < HAWK_COUNTOF(match); i++)
		{
			if (match[i].rm_so != -1) 
			{
				submat[i-1].ptr = &str->ptr[match[i].rm_so];
				submat[i-1].len = match[i].rm_eo - match[i].rm_so;
			}
		}
	}
	return 1;
}

static int matchtre_bcs (hawk_tre_t* tre, int opt, const hawk_bcs_t* str, hawk_bcs_t* mat, hawk_bcs_t submat[9], hawk_gem_t* errgem)
{
	int n;
	/*hawk_tre_match_t match[10] = { { 0, 0 }, };*/
	hawk_tre_match_t match[10];

	HAWK_MEMSET (match, 0, HAWK_SIZEOF(match));
	n = hawk_tre_execbchars(tre, str->ptr, str->len, match, HAWK_COUNTOF(match), opt, errgem);
	if (n <= -1)
	{
		if (hawk_gem_geterrnum(errgem) == HAWK_EREXNOMAT) return 0;
		return -1;
	}

	HAWK_ASSERT (match[0].rm_so != -1);
	if (mat)
	{
		mat->ptr = &str->ptr[match[0].rm_so];
		mat->len = match[0].rm_eo - match[0].rm_so;
	}

	if (submat)
	{
		int i;

		/* you must intialize submat before you pass into this 
		 * function because it can abort filling */
		for (i = 1; i < HAWK_COUNTOF(match); i++)
		{
			if (match[i].rm_so != -1) 
			{
				submat[i-1].ptr = &str->ptr[match[i].rm_so];
				submat[i-1].len = match[i].rm_eo - match[i].rm_so;
			}
		}
	}
	return 1;
}

int hawk_rtx_matchvalwithucs (hawk_rtx_t* rtx, hawk_val_t* val, const hawk_ucs_t* str, const hawk_ucs_t* substr, hawk_ucs_t* match, hawk_ucs_t submat[9])
{
	int ignorecase, x;
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING ??? */
	hawk_tre_t* code;
	hawk_val_type_t v_type;

	ignorecase = rtx->gbl.ignorecase;

	v_type = HAWK_RTX_GETVALTYPE(rtx, val);
	if (v_type == HAWK_VAL_REX)
	{
		code = ((hawk_val_rex_t*)val)->code[ignorecase];
	}
	else 
	{
		/* convert to a string and build a regular expression */
		hawk_oocs_t tmp;

		tmp.ptr = hawk_rtx_getvaloocstr(rtx, val, &tmp.len);
		if (tmp.ptr == HAWK_NULL) return -1;

		x = ignorecase? hawk_rtx_buildrex(rtx, tmp.ptr, tmp.len, HAWK_NULL, &code):
		                hawk_rtx_buildrex(rtx, tmp.ptr, tmp.len, &code, HAWK_NULL);
		hawk_rtx_freevaloocstr (rtx, val, tmp.ptr);
		if (x <= -1) return -1;
	}


	x = matchtre_ucs(
		code, ((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, hawk_rtx_getgem(rtx)
	);

	if (v_type == HAWK_VAL_REX) 
	{
		/* nothing to free */
	}
	else
	{
		hawk_tre_close (code);
	}

	return x;
}


int hawk_rtx_matchvalwithbcs (hawk_rtx_t* rtx, hawk_val_t* val, const hawk_bcs_t* str, const hawk_bcs_t* substr, hawk_bcs_t* match, hawk_bcs_t submat[9])
{
	int ignorecase, x;
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING ??? */
	hawk_tre_t* code;
	hawk_val_type_t v_type;

	ignorecase = rtx->gbl.ignorecase;

	v_type = HAWK_RTX_GETVALTYPE(rtx, val);
	if (v_type == HAWK_VAL_REX)
	{
		code = ((hawk_val_rex_t*)val)->code[ignorecase];
	}
	else 
	{
		/* convert to a string and build a regular expression */
		hawk_oocs_t tmp;

		tmp.ptr = hawk_rtx_getvaloocstr(rtx, val, &tmp.len);
		if (HAWK_UNLIKELY(!tmp.ptr)) return -1;

		x = ignorecase? hawk_rtx_buildrex(rtx, tmp.ptr, tmp.len, HAWK_NULL, &code):
		                hawk_rtx_buildrex(rtx, tmp.ptr, tmp.len, &code, HAWK_NULL);
		hawk_rtx_freevaloocstr (rtx, val, tmp.ptr);
		if (x <= -1) return -1;
	}

	x = matchtre_bcs(
		code, ((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, hawk_rtx_getgem(rtx)
	);

	if (v_type == HAWK_VAL_REX) 
	{
		/* nothing to free */
	}
	else
	{
		hawk_tre_close (code);
	}

	return x;
}


int hawk_rtx_matchrexwithucs (hawk_rtx_t* rtx, hawk_tre_t* code, const hawk_ucs_t* str, const hawk_ucs_t* substr, hawk_ucs_t* match, hawk_ucs_t submat[9])
{
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING or others??? */
	return matchtre_ucs(
		code, ((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, hawk_rtx_getgem(rtx)
	);
}

int hawk_rtx_matchrexwithbcs (hawk_rtx_t* rtx, hawk_tre_t* code, const hawk_bcs_t* str, const hawk_bcs_t* substr, hawk_bcs_t* match, hawk_bcs_t submat[9])
{
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING or others??? */
	return matchtre_bcs(
		code, ((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, hawk_rtx_getgem(rtx)
	);
}
