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

#include "hawk-prv.h"

/*#define USE_REX */

#if defined(USE_REX)
#	include <hawk-rex.h>
#else
#	include <hawk-tre.h>
#endif

/* ========================================================================= */
#undef awk_strxtoint
#undef awk_strtoflt
#undef awk_strxtoflt
#undef char_t
#undef cint_t
#undef AWK_IS_SPACE
#undef AWK_IS_DIGIT
#undef _T

#define awk_strxtoint hawk_bcharstoint
#define awk_strtoflt hawk_mbstoflt
#define awk_strxtoflt hawk_bcharstoflt
#define char_t hawk_bch_t
#define cint_t hawk_bci_t
#define AWK_IS_SPACE hawk_is_bch_space
#define AWK_IS_DIGIT hawk_is_bch_digit
#define _T HAWK_BT
#include "misc-imp.h"

/* ------------------------------------------------------------------------- */
#undef awk_strxtoint
#undef awk_strtoflt
#undef awk_strxtoflt
#undef char_t
#undef cint_t
#undef AWK_IS_SPACE
#undef AWK_IS_DIGIT
#undef _T
/* ------------------------------------------------------------------------- */

#define awk_strxtoint hawk_ucharstoint
#define awk_strtoflt hawk_wcstoflt
#define awk_strxtoflt hawk_ucharstoflt
#define char_t hawk_uch_t
#define cint_t hawk_uci_t
#define AWK_IS_SPACE hawk_is_uch_space
#define AWK_IS_DIGIT hawk_is_uch_digit
#define _T HAWK_UT
#include "misc-imp.h"

#undef awk_strxtoint
#undef awk_strtoflt
#undef awk_strxtoflt
#undef char_t
#undef cint_t
#undef AWK_ISSPACE
#undef AWK_ISDIGIT
#undef _T
/* ========================================================================= */


hawk_ooch_t* hawk_rtx_strtok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, 
	const hawk_ooch_t* delim, hawk_oocs_t* tok)
{
	return hawk_rtx_strxntok(rtx, s, hawk_count_oocstr(s), delim, hawk_count_oocstr(delim), tok);
}

hawk_ooch_t* hawk_rtx_strxtok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, hawk_oow_t len,
	const hawk_ooch_t* delim, hawk_oocs_t* tok)
{
	return hawk_rtx_strxntok(rtx, s, len, delim, hawk_count_oocstr(delim), tok);
}

hawk_ooch_t* hawk_rtx_strntok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, 
	const hawk_ooch_t* delim, hawk_oow_t delim_len,
	hawk_oocs_t* tok)
{
	return hawk_rtx_strxntok(rtx, s, hawk_count_oocstr(s), delim, delim_len, tok);
}

hawk_ooch_t* hawk_rtx_strxntok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, hawk_oow_t len,
	const hawk_ooch_t* delim, hawk_oow_t delim_len, hawk_oocs_t* tok)
{
	const hawk_ooch_t* p = s, *d;
	const hawk_ooch_t* end = s + len;	
	const hawk_ooch_t* sp = HAWK_NULL, * ep = HAWK_NULL;
	const hawk_ooch_t* delim_end = delim + delim_len;
	hawk_ooch_t c; 
	int delim_mode;

#define __DELIM_NULL      0
#define __DELIM_EMPTY     1
#define __DELIM_SPACES    2
#define __DELIM_NOSPACES  3
#define __DELIM_COMPOSITE 4
	if (delim == HAWK_NULL) delim_mode = __DELIM_NULL;
	else 
	{
		delim_mode = __DELIM_EMPTY;

		for (d = delim; d < delim_end; d++) 
		{
			if (hawk_is_ooch_space(*d)) 
			{
				if (delim_mode == __DELIM_EMPTY)
					delim_mode = __DELIM_SPACES;
				else if (delim_mode == __DELIM_NOSPACES)
				{
					delim_mode = __DELIM_COMPOSITE;
					break;
				}
			}
			else
			{
				if (delim_mode == __DELIM_EMPTY)
					delim_mode = __DELIM_NOSPACES;
				else if (delim_mode == __DELIM_SPACES)
				{
					delim_mode = __DELIM_COMPOSITE;
					break;
				}
			}
		}

		/* TODO: verify the following statement... */
		if (delim_mode == __DELIM_SPACES && 
		    delim_len == 1 && 
		    delim[0] != HAWK_T(' ')) delim_mode = __DELIM_NOSPACES;
	}		
	
	if (delim_mode == __DELIM_NULL) 
	{ 
		/* when HAWK_NULL is given as "delim", it trims off the 
		 * leading and trailing spaces characters off the source
		 * string "s" eventually. */

		while (p < end && hawk_is_ooch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;

			if (!hawk_is_ooch_space(c)) 
			{
				if (sp == HAWK_NULL) sp = p;
				ep = p;
			}
			p++;
		}
	}
	else if (delim_mode == __DELIM_EMPTY)
	{
		/* each character in the source string "s" becomes a token. */
		if (p < end)
		{
			c = *p;
			sp = p;
			ep = p++;
		}
	}
	else if (delim_mode == __DELIM_SPACES) 
	{
		/* each token is delimited by space characters. all leading
		 * and trailing spaces are removed. */

		while (p < end && hawk_is_ooch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;
			if (hawk_is_ooch_space(c)) break;
			if (sp == HAWK_NULL) sp = p;
			ep = p++;
		}
		while (p < end && hawk_is_ooch_space(*p)) p++;
	}
	else if (delim_mode == __DELIM_NOSPACES)
	{
		/* each token is delimited by one of charaters 
		 * in the delimeter set "delim". */
		if (rtx->gbl.ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_ooch_upper(*p);
				for (d = delim; d < delim_end; d++) 
				{
					if (c == hawk_to_ooch_upper(*d)) goto exit_loop;
				}

				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
		else
		{
			while (p < end) 
			{
				c = *p;
				for (d = delim; d < delim_end; d++) 
				{
					if (c == *d) goto exit_loop;
				}

				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
	}
	else /* if (delim_mode == __DELIM_COMPOSITE) */ 
	{
		/* each token is delimited by one of non-space charaters
		 * in the delimeter set "delim". however, all space characters
		 * surrounding the token are removed */
		while (p < end && hawk_is_ooch_space(*p)) p++;
		if (rtx->gbl.ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_ooch_upper(*p);
				if (hawk_is_ooch_space(c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == hawk_to_ooch_upper(*d))
						goto exit_loop;
				}
				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
		else
		{
			while (p < end) 
			{
				c = *p;
				if (hawk_is_ooch_space(c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == *d) goto exit_loop;
				}
				if (sp == HAWK_NULL) sp = p;
				ep = p++;
			}
		}
	}

exit_loop:
	if (sp == HAWK_NULL) 
	{
		tok->ptr = HAWK_NULL;
		tok->len = (hawk_oow_t)0;
	}
	else 
	{
		tok->ptr = (hawk_ooch_t*)sp;
		tok->len = ep - sp + 1;
	}

	/* if HAWK_NULL is returned, this function should not be called again */
	if (p >= end) return HAWK_NULL;
	if (delim_mode == __DELIM_EMPTY || 
	    delim_mode == __DELIM_SPACES) return (hawk_ooch_t*)p;
	return (hawk_ooch_t*)++p;
}

hawk_ooch_t* hawk_rtx_strxntokbyrex (
	hawk_rtx_t* rtx, 
	const hawk_ooch_t* str, hawk_oow_t len,
	const hawk_ooch_t* substr, hawk_oow_t sublen,
	void* rex, hawk_oocs_t* tok,
	hawk_errnum_t* errnum)
{
	int n;
	hawk_oow_t i;
	hawk_oocs_t match, s, cursub, realsub;

	s.ptr = (hawk_ooch_t*)str;
	s.len = len;

	cursub.ptr = (hawk_ooch_t*)substr;
	cursub.len = sublen;

	realsub.ptr = (hawk_ooch_t*)substr;
	realsub.len = sublen;

	while (cursub.len > 0)
	{
		n = hawk_matchrex (
			rtx->awk, rex, rtx->gbl.ignorecase,
			&s, &cursub, &match, HAWK_NULL, errnum);
		if (n == -1) return HAWK_NULL;
		if (n == 0)
		{
			/* no match has been found. 
			 * return the entire string as a token */
			tok->ptr = realsub.ptr;
			tok->len = realsub.len;
			*errnum = HAWK_ENOERR;
			return HAWK_NULL; 
		}

		HAWK_ASSERT (hawk_rtx_getawk(rtx), n == 1);

		if (match.len == 0)
		{
			/* the match length is zero. */
			cursub.ptr++;
			cursub.len--;
		}
		else if (rtx->gbl.striprecspc > 0 || (rtx->gbl.striprecspc < 0 && (rtx->awk->opt.trait & HAWK_STRIPRECSPC)))
		{
			/* match at the beginning of the input string */
			if (match.ptr == substr) 
			{
				for (i = 0; i < match.len; i++)
				{
					if (!hawk_is_ooch_space(match.ptr[i])) goto exit_loop;
				}

				/* the match that is all spaces at the 
				 * beginning of the input string is skipped */
				cursub.ptr += match.len;
				cursub.len -= match.len;

				/* adjust the substring by skipping the leading
				 * spaces and retry matching */
				realsub.ptr = (hawk_ooch_t*)substr + match.len;
				realsub.len -= match.len;
			}
			else break;
		}
		else break;
	}

exit_loop:
	if (cursub.len <= 0)
	{
		tok->ptr = realsub.ptr;
		tok->len = realsub.len;
		*errnum = HAWK_ENOERR;
		return HAWK_NULL; 
	}

	tok->ptr = realsub.ptr;
	tok->len = match.ptr - realsub.ptr;

	for (i = 0; i < match.len; i++)
	{
		if (!hawk_is_ooch_space(match.ptr[i]))
		{
			/* the match contains a non-space character. */
			*errnum = HAWK_ENOERR;
			return (hawk_ooch_t*)match.ptr+match.len;
		}
	}

	/* the match is all spaces */
	*errnum = HAWK_ENOERR;
	if (rtx->gbl.striprecspc > 0 || (rtx->gbl.striprecspc < 0 && (rtx->awk->opt.trait & HAWK_STRIPRECSPC)))
	{
		/* if the match reached the last character in the input string,
		 * it returns HAWK_NULL to terminate tokenization. */
		return (match.ptr+match.len >= substr+sublen)? 
			HAWK_NULL: ((hawk_ooch_t*)match.ptr+match.len);
	}
	else
	{
		/* if the match went beyond the the last character in the input 
		 * string, it returns HAWK_NULL to terminate tokenization. */
		return (match.ptr+match.len > substr+sublen)? 
			HAWK_NULL: ((hawk_ooch_t*)match.ptr+match.len);
	}
}

hawk_ooch_t* hawk_rtx_strxnfld (
	hawk_rtx_t* rtx, hawk_ooch_t* str, hawk_oow_t len,
	hawk_ooch_t fs, hawk_ooch_t ec, hawk_ooch_t lq, hawk_ooch_t rq,
	hawk_oocs_t* tok)
{
	hawk_ooch_t* p = str;
	hawk_ooch_t* end = str + len;
	int escaped = 0, quoted = 0;
	hawk_ooch_t* ts; /* token start */
	hawk_ooch_t* tp; /* points to one char past the last token char */
	hawk_ooch_t* xp; /* points to one char past the last effective char */

	/* skip leading spaces */
	while (p < end && hawk_is_ooch_space(*p)) p++;

	/* initialize token pointers */
	ts = tp = xp = p; 

	while (p < end)
	{
		char c = *p;

		if (escaped)
		{
			*tp++ = c; xp = tp; p++;
			escaped = 0;
		}
		else
		{
			if (c == ec)
			{
				escaped = 1;
				p++;
			}
			else if (quoted)
			{
				if (c == rq)
				{
					quoted = 0;
					p++;
				}
				else
				{
					*tp++ = c; xp = tp; p++;
				}
			}
			else 
			{
				if (c == fs)
				{
					tok->ptr = ts;
					tok->len = xp - ts;
					p++;

					if (hawk_is_ooch_space(fs))
					{
						while (p < end && *p == fs) p++;
						if (p >= end) return HAWK_NULL;
					}

					return p;
				}
		
				if (c == lq)
				{
					quoted = 1;
					p++;
				}
				else
				{
					*tp++ = c; p++;
					if (!hawk_is_ooch_space(c)) xp = tp; 
				}
			}
		}
	}

	if (escaped) 
	{
		/* if it is still escaped, the last character must be 
		 * the escaper itself. treat it as a normal character */
		*xp++ = ec;
	}
	
	tok->ptr = ts;
	tok->len = xp - ts;
	return HAWK_NULL;
}

#if defined(USE_REX)
static HAWK_INLINE int rexerr_to_errnum (int err)
{
	switch (err)
	{
		case HAWK_REX_ENOERR:   return HAWK_ENOERR;
		case HAWK_REX_ENOMEM:   return HAWK_ENOMEM;
	 	case HAWK_REX_ENOCOMP:  return HAWK_EREXBL;
	 	case HAWK_REX_ERECUR:   return HAWK_EREXRECUR;
	 	case HAWK_REX_ERPAREN:  return HAWK_EREXRPAREN;
	 	case HAWK_REX_ERBRACK:  return HAWK_EREXRBRACK;
	 	case HAWK_REX_ERBRACE:  return HAWK_EREXRBRACE;
	 	case HAWK_REX_ECOLON:   return HAWK_EREXCOLON;
	 	case HAWK_REX_ECRANGE:  return HAWK_EREXCRANGE;
	 	case HAWK_REX_ECCLASS:  return HAWK_EREXCCLASS;
	 	case HAWK_REX_EBOUND:   return HAWK_EREXBOUND;
	 	case HAWK_REX_ESPCAWP:  return HAWK_EREXSPCAWP;
	 	case HAWK_REX_EPREEND:  return HAWK_EREXPREEND;
		default:                return HAWK_EINTERN;
	}
}
#endif

int hawk_buildrex (
	hawk_t* awk, const hawk_ooch_t* ptn, hawk_oow_t len, 
	hawk_errnum_t* errnum, void** code, void** icode)
{
#if defined(USE_REX)
	hawk_rex_errnum_t err;
	void* p;

	if (code || icode)
	{
		p = hawk_buildrex (
			hawk_getmmgr(awk), awk->opt.depth.s.rex_build,
			((awk->opt.trait & HAWK_REXBOUND)? 0: HAWK_REX_NOBOUND),
			ptn, len, &err
		);
		if (p == HAWK_NULL) 
		{
			*errnum = rexerr_to_errnum(err);
			return -1;
		}
	
		if (code) *code = p;
		if (icode) *icode = p;
	}

	return 0;
#else
	hawk_tre_t* tre = HAWK_NULL; 
	hawk_tre_t* itre = HAWK_NULL;
	int opt = HAWK_TRE_EXTENDED;

	if (code)
	{
		tre = hawk_tre_open(awk, 0);
		if (tre == HAWK_NULL)
		{
			*errnum = HAWK_ENOMEM;
			return -1;
		}

		if (!(awk->opt.trait & HAWK_REXBOUND)) opt |= HAWK_TRE_NOBOUND;

		if (hawk_tre_compx (tre, ptn, len, HAWK_NULL, opt) <= -1)
		{
#if 0 /* TODO */

			if (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMEM) *errnum = HAWK_ENOMEM;
			else
				SETERR1 (awk, HAWK_EREXBL, str->ptr, str->len, loc);
#endif
			*errnum = (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMEM)? 
				HAWK_ENOMEM: HAWK_EREXBL;
			hawk_tre_close (tre);
			return -1;
		}
	}

	if (icode) 
	{
		itre = hawk_tre_open(awk, 0);
		if (itre == HAWK_NULL)
		{
			if (tre) hawk_tre_close (tre);
			*errnum = HAWK_ENOMEM;
			return -1;
		}

		/* ignorecase is a compile option for TRE */
		if (hawk_tre_compx (itre, ptn, len, HAWK_NULL, opt | HAWK_TRE_IGNORECASE) <= -1)
		{
#if 0 /* TODO */

			if (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMEM) *errnum = HAWK_ENOMEM;
			else
				SETERR1 (awk, HAWK_EREXBL, str->ptr, str->len, loc);
#endif
			*errnum = (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMEM)? 
				HAWK_ENOMEM: HAWK_EREXBL;
			hawk_tre_close (itre);
			if (tre) hawk_tre_close (tre);
			return -1;
		}
	}

	if (code) *code = tre;
	if (icode) *icode = itre;
	return 0;
#endif
}

#if !defined(USE_REX)
static int matchtre (
	hawk_t* awk, hawk_tre_t* tre, int opt, 
	const hawk_oocs_t* str, hawk_oocs_t* mat, 
	hawk_oocs_t submat[9], hawk_errnum_t* errnum)
{
	int n;
	/*hawk_tre_match_t match[10] = { { 0, 0 }, };*/
	hawk_tre_match_t match[10];

	HAWK_MEMSET (match, 0, HAWK_SIZEOF(match));
	n = hawk_tre_execx(tre, str->ptr, str->len, match, HAWK_COUNTOF(match), opt);
	if (n <= -1)
	{
		if (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMATCH) return 0;

#if 0 /* TODO: */
		*errnum = (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMEM)? 
			HAWK_ENOMEM: HAWK_EREXMA;
		SETERR0 (sed, errnum, loc);
#endif
		*errnum = (HAWK_TRE_ERRNUM(tre) == HAWK_TRE_ENOMEM)? 
			HAWK_ENOMEM: HAWK_EREXMA;
		return -1;
	}

	HAWK_ASSERT (awk, match[0].rm_so != -1);
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
#endif

int hawk_matchrex (
	hawk_t* awk, void* code, int icase,
	const hawk_oocs_t* str, const hawk_oocs_t* substr,
	hawk_oocs_t* match, hawk_oocs_t submat[9], hawk_errnum_t* errnum)
{
#if defined(USE_REX)
	int x;
	hawk_rex_errnum_t err;

	/* submatch is not supported */
	x = hawk_matchrex (
		hawk_getmmgr(awk), awk->opt.depth.s.rex_match, code, 
		(icase? HAWK_REX_IGNORECASE: 0), str, substr, match, &err);
	if (x <= -1) *errnum = rexerr_to_errnum(err);
	return x;
#else
	int x;
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING ??? */

	x = matchtre (
		awk, code,
		((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, errnum
	);
	return x;
#endif
}

void hawk_freerex (hawk_t* awk, void* code, void* icode)
{
	if (code)
	{
#if defined(USE_REX)
		hawk_freerex ((awk)->mmgr, code);
#else
		hawk_tre_close (code);
#endif
	}

	if (icode && icode != code)
	{
#if defined(USE_REX)
		hawk_freerex ((awk)->mmgr, icode);
#else
		hawk_tre_close (icode);
#endif
	}
}

int hawk_rtx_matchrex (
	hawk_rtx_t* rtx, hawk_val_t* val,
	const hawk_oocs_t* str, const hawk_oocs_t* substr, hawk_oocs_t* match, hawk_oocs_t submat[9])
{
	void* code;
	int icase, x;
	hawk_errnum_t awkerr;
#if defined(USE_REX)
	hawk_rex_errnum_t rexerr;
#endif

	icase = rtx->gbl.ignorecase;

	if (HAWK_RTX_GETVALTYPE (rtx, val) == HAWK_VAL_REX)
	{
		code = ((hawk_val_rex_t*)val)->code[icase];
	}
	else 
	{
		/* convert to a string and build a regular expression */
		hawk_oocs_t tmp;

		tmp.ptr = hawk_rtx_getvaloocstr (rtx, val, &tmp.len);
		if (tmp.ptr == HAWK_NULL) return -1;

		x = icase? hawk_buildrex(rtx->awk, tmp.ptr, tmp.len, &awkerr, HAWK_NULL, &code):
		           hawk_buildrex(rtx->awk, tmp.ptr, tmp.len, &awkerr, &code, HAWK_NULL);
		hawk_rtx_freevaloocstr (rtx, val, tmp.ptr);
		if (x <= -1)
		{
			hawk_rtx_seterrnum (rtx, awkerr, HAWK_NULL);
			return -1;
		}
	}
	
#if defined(USE_REX)
	/* submatch not supported */
	x = hawk_matchrex (
		hawk_rtx_getmmgr(rtx), rtx->awk->opt.depth.s.rex_match,
		code, (icase? HAWK_REX_IGNORECASE: 0),
		str, substr, match, &rexerr);
	if (x <= -1) hawk_rtx_seterrnum (rtx, rexerr_to_errnum(rexerr), HAWK_NULL);
#else
	x = matchtre (
		rtx->awk, code,
		((str->ptr == substr->ptr)? HAWK_TRE_BACKTRACKING: (HAWK_TRE_BACKTRACKING | HAWK_TRE_NOTBOL)),
		substr, match, submat, &awkerr
	);
	if (x <= -1) hawk_rtx_seterrnum (rtx, awkerr, HAWK_NULL);
#endif

	if (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_REX) 
	{
		/* nothing to free */
	}
	else
	{
		if (icase) 
			hawk_freerex (rtx->awk, HAWK_NULL, code);
		else
			hawk_freerex (rtx->awk, code, HAWK_NULL);
	}

	return x;
}
