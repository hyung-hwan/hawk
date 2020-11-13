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

char_t* tokenize_xchars (hawk_rtx_t* rtx, const char_t* s, hawk_oow_t len, const char_t* delim, hawk_oow_t delim_len, xcs_t* tok)
{
	const char_t* p = s, *d;
	const char_t* end = s + len;
	const char_t* sp = HAWK_NULL, * ep = HAWK_NULL;
	const char_t* delim_end = delim + delim_len;
	char_t c; 
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
			if (is_xch_space(*d)) 
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
		    delim[0] != ' ') delim_mode = __DELIM_NOSPACES;
	}

	if (delim_mode == __DELIM_NULL) 
	{ 
		/* when HAWK_NULL is given as "delim", it trims off the 
		 * leading and trailing spaces characters off the source
		 * string "s" eventually. */

		while (p < end && is_xch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;

			if (!is_xch_space(c)) 
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

		while (p < end && is_xch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;
			if (is_xch_space(c)) break;
			if (sp == HAWK_NULL) sp = p;
			ep = p++;
		}
		while (p < end && is_xch_space(*p)) p++;
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
		while (p < end && is_xch_space(*p)) p++;
		if (rtx->gbl.ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_ooch_upper(*p);
				if (is_xch_space(c)) 
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
				if (is_xch_space(c)) 
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
		tok->ptr = (char_t*)sp;
		tok->len = ep - sp + 1;
	}

	/* if HAWK_NULL is returned, this function should not be called again */
	if (p >= end) return HAWK_NULL;
	if (delim_mode == __DELIM_EMPTY || 
	    delim_mode == __DELIM_SPACES) return (char_t*)p;
	return (char_t*)++p;
}

char_t* split_xchars_to_fields (hawk_rtx_t* rtx, char_t* str, hawk_oow_t len, char_t fs, char_t ec, char_t lq, char_t rq, xcs_t* tok)
{
	char_t* p = str;
	char_t* end = str + len;
	int escaped = 0, quoted = 0;
	char_t* ts; /* token start */
	char_t* tp; /* points to one char past the last token char */
	char_t* xp; /* points to one char past the last effective char */

	/* skip leading spaces */
	while (p < end && is_xch_space(*p)) p++;

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

					if (is_xch_space(fs))
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
					if (!is_xch_space(c)) xp = tp; 
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
