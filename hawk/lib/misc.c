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
	hawk_tre_t* rex, hawk_oocs_t* tok)
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
		n = hawk_rtx_matchrex(rtx, rex, &s, &cursub, &match, HAWK_NULL);
		if (n <= -1) return HAWK_NULL;

		if (n == 0)
		{
			/* no match has been found. return the entire string as a token */
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOERR); /* reset HAWK_EREXNOMAT to no error */
			tok->ptr = realsub.ptr;
			tok->len = realsub.len;
			return HAWK_NULL; 
		}

		HAWK_ASSERT (n == 1);

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
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOERR);

	if (cursub.len <= 0)
	{
		tok->ptr = realsub.ptr;
		tok->len = realsub.len;
		return HAWK_NULL; 
	}

	tok->ptr = realsub.ptr;
	tok->len = match.ptr - realsub.ptr;

	for (i = 0; i < match.len; i++)
	{
		if (!hawk_is_ooch_space(match.ptr[i]))
		{
			/* the match contains a non-space character. */
			return (hawk_ooch_t*)match.ptr+match.len;
		}
	}

	/* the match is all spaces */
	if (rtx->gbl.striprecspc > 0 || (rtx->gbl.striprecspc < 0 && (rtx->awk->opt.trait & HAWK_STRIPRECSPC)))
	{
		/* if the match reached the last character in the input string,
		 * it returns HAWK_NULL to terminate tokenization. */
		return (match.ptr+match.len >= substr+sublen)? HAWK_NULL: ((hawk_ooch_t*)match.ptr+match.len);
	}
	else
	{
		/* if the match went beyond the the last character in the input 
		 * string, it returns HAWK_NULL to terminate tokenization. */
		return (match.ptr+match.len > substr+sublen)? HAWK_NULL: ((hawk_ooch_t*)match.ptr+match.len);
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

static int matchtre (hawk_tre_t* tre, int opt, const hawk_oocs_t* str, hawk_oocs_t* mat, hawk_oocs_t submat[9], hawk_gem_t* errgem)
{
	int n;
	/*hawk_tre_match_t match[10] = { { 0, 0 }, };*/
	hawk_tre_match_t match[10];

	HAWK_MEMSET (match, 0, HAWK_SIZEOF(match));
	n = hawk_tre_execx(tre, str->ptr, str->len, match, HAWK_COUNTOF(match), opt, errgem);
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

int hawk_rtx_matchval (hawk_rtx_t* rtx, hawk_val_t* val, const hawk_oocs_t* str, const hawk_oocs_t* substr, hawk_oocs_t* match, hawk_oocs_t submat[9])
{
	int ignorecase, x;
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING ??? */
	hawk_tre_t* code;

	ignorecase = rtx->gbl.ignorecase;

	if (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_REX)
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
	
	x = matchtre(
		code, ((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, hawk_rtx_getgem(rtx)
	);

	if (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_REX) 
	{
		/* nothing to free */
	}
	else
	{
		hawk_tre_close (code);
	}

	return x;
}

int hawk_rtx_matchrex (hawk_rtx_t* rtx, hawk_tre_t* code, const hawk_oocs_t* str, const hawk_oocs_t* substr, hawk_oocs_t* match, hawk_oocs_t submat[9])
{
	int opt = HAWK_TRE_BACKTRACKING; /* TODO: option... HAWK_TRE_BACKTRACKING or others??? */
	return matchtre(
		code, ((str->ptr == substr->ptr)? opt: (opt | HAWK_TRE_NOTBOL)),
		substr, match, submat, hawk_rtx_getgem(rtx)
	);
}
