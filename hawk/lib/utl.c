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

#include "hawk-prv.h"

/* ----------------------------------------------------------------------- */

int hawk_comp_ucstr_bcstr (const hawk_uch_t* str1, const hawk_bch_t* str2, int ignorecase)
{
	if (ignorecase)
	{
		while (hawk_to_uch_lower(*str1) == hawk_to_bch_lower(*str2))
 		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((hawk_uchu_t)hawk_to_uch_lower(*str1) > (hawk_bchu_t)hawk_to_bch_lower(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((hawk_uchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
	}
}

int hawk_comp_uchars_bcstr (const hawk_uch_t* str1, hawk_oow_t len, const hawk_bch_t* str2, int ignorecase)
{
	/* for "abc\0" of length 4 vs "abc", the fourth character
	 * of the first string is equal to the terminating null of
	 * the second string. the first string is still considered
	 * bigger */
	if (ignorecase)
	{
		const hawk_uch_t* end = str1 + len;
		hawk_uch_t c1;
		hawk_bch_t c2;
		while (str1 < end && *str2 != '\0')
		{
			c1 = hawk_to_uch_lower(*str1);
			c2 = hawk_to_bch_lower(*str2);
			if (c1 != c2) return ((hawk_uchu_t)c1 > (hawk_bchu_t)c2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
	else
	{
		const hawk_uch_t* end = str1 + len;
		while (str1 < end && *str2 != '\0')
		{
			if (*str1 != *str2) return ((hawk_uchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
}

int hawk_comp_bchars_ucstr (const hawk_bch_t* str1, hawk_oow_t len, const hawk_uch_t* str2, int ignorecase)
{
	if (ignorecase)
	{
		const hawk_bch_t* end = str1 + len;
		hawk_bch_t c1;
		hawk_uch_t c2;
		while (str1 < end && *str2 != '\0')
		{
			c1 = hawk_to_bch_lower(*str1);
			c2 = hawk_to_uch_lower(*str2);
			if (c1 != c2) return ((hawk_bchu_t)c1 > (hawk_uchu_t)c2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
	else
	{
		const hawk_bch_t* end = str1 + len;
		while (str1 < end && *str2 != '\0')
		{
			if (*str1 != *str2) return ((hawk_bchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
}

/* ----------------------------------------------------------------------- */

void hawk_copy_bchars_to_uchars (hawk_uch_t* dst, const hawk_bch_t* src, hawk_oow_t len)
{
	/* copy without conversions.
	 * use hawk_convbtouchars() for conversion encoding */
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

void hawk_copy_uchars_to_bchars (hawk_bch_t* dst, const hawk_uch_t* src, hawk_oow_t len)
{
	/* copy without conversions.
	 * use hawk_convutobchars() for conversion encoding */
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

/* ----------------------------------------------------------------------- */

hawk_oow_t hawk_byte_to_ucstr (hawk_oob_t byte, hawk_uch_t* buf, hawk_oow_t size, int flagged_radix, hawk_uch_t fill)
{
	hawk_uch_t tmp[(HAWK_SIZEOF(hawk_oob_t) * HAWK_BITS_PER_BYTE)];
	hawk_uch_t* p = tmp, * bp = buf, * be = buf + size - 1;
	int radix;
	hawk_uch_t radix_char;

	radix = (flagged_radix & HAWK_BYTE_TO_UCSTR_RADIXMASK);
	radix_char = (flagged_radix & HAWK_BYTE_TO_UCSTR_LOWERCASE)? 'a': 'A';
	if (radix < 2 || radix > 36 || size <= 0) return 0;

	do 
	{
		hawk_uint8_t digit = byte % radix;	
		if (digit < 10) *p++ = digit + '0';
		else *p++ = digit + radix_char - 10;
		byte /= radix;
	}
	while (byte > 0);

	if (fill != '\0') 
	{
		while (size - 1 > p - tmp) 
		{
			*bp++ = fill;
			size--;
		}
	}

	while (p > tmp && bp < be) *bp++ = *--p;
	*bp = '\0';
	return bp - buf;
}

hawk_oow_t hawk_byte_to_bcstr (hawk_oob_t byte, hawk_bch_t* buf, hawk_oow_t size, int flagged_radix, hawk_bch_t fill)
{
	hawk_bch_t tmp[(HAWK_SIZEOF(hawk_oob_t) * HAWK_BITS_PER_BYTE)];
	hawk_bch_t* p = tmp, * bp = buf, * be = buf + size - 1;
	int radix;
	hawk_bch_t radix_char;

	radix = (flagged_radix & HAWK_BYTE_TO_BCSTR_RADIXMASK);
	radix_char = (flagged_radix & HAWK_BYTE_TO_BCSTR_LOWERCASE)? 'a': 'A';
	if (radix < 2 || radix > 36 || size <= 0) return 0;

	do 
	{
		hawk_uint8_t digit = byte % radix;	
		if (digit < 10) *p++ = digit + '0';
		else *p++ = digit + radix_char - 10;
		byte /= radix;
	}
	while (byte > 0);

	if (fill != '\0') 
	{
		while (size - 1 > p - tmp) 
		{
			*bp++ = fill;
			size--;
		}
	}

	while (p > tmp && bp < be) *bp++ = *--p;
	*bp = '\0';
	return bp - buf;
}

/* ------------------------------------------------------------------------ */

int hawk_conv_bchars_to_uchars_with_cmgr (
	const hawk_bch_t* bcs, hawk_oow_t* bcslen,
	hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_cmgr_t* cmgr, int all)
{
	const hawk_bch_t* p;
	int ret = 0;
	hawk_oow_t mlen;

	if (ucs)
	{
		/* destination buffer is specified.
		 * copy the conversion result to the buffer */

		hawk_uch_t* q, * qend;

		p = bcs;
		q = ucs;
		qend = ucs + *ucslen;
		mlen = *bcslen;

		while (mlen > 0)
		{
			hawk_oow_t n;

			if (q >= qend)
			{
				/* buffer too small */
				ret = -2;
				break;
			}

			n = cmgr->bctouc(p, mlen, q);
			if (n == 0)
			{
				/* invalid sequence */
				if (all)
				{
					n = 1;
					*q = '?';
				}
				else
				{
					ret = -1;
					break;
				}
			}
			if (n > mlen)
			{
				/* incomplete sequence */
				if (all)
				{
					n = 1;
					*q = '?';
				}
				else
				{
					ret = -3;
					break;
				}
			}

			q++;
			p += n;
			mlen -= n;
		}

		*ucslen = q - ucs;
		*bcslen = p - bcs;
	}
	else
	{
		/* no destination buffer is specified. perform conversion
		 * but don't copy the result. the caller can call this function
		 * without a buffer to find the required buffer size, allocate
		 * a buffer with the size and call this function again with
		 * the buffer. */

		hawk_uch_t w;
		hawk_oow_t wlen = 0;

		p = bcs;
		mlen = *bcslen;

		while (mlen > 0)
		{
			hawk_oow_t n;

			n = cmgr->bctouc(p, mlen, &w);
			if (n == 0)
			{
				/* invalid sequence */
				if (all) n = 1;
				else
				{
					ret = -1;
					break;
				}
			}
			if (n > mlen)
			{
				/* incomplete sequence */
				if (all) n = 1;
				else
				{
					ret = -3;
					break;
				}
			}

			p += n;
			mlen -= n;
			wlen += 1;
		}

		*ucslen = wlen;
		*bcslen = p - bcs;
	}

	return ret;
}

int hawk_conv_bcstr_to_ucstr_with_cmgr (
	const hawk_bch_t* bcs, hawk_oow_t* bcslen,
	hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_cmgr_t* cmgr, int all)
{
	const hawk_bch_t* bp;
	hawk_oow_t mlen, wlen;
	int n;

	for (bp = bcs; *bp != '\0'; bp++) /* nothing */ ;

	mlen = bp - bcs; wlen = *ucslen;
	n = hawk_conv_bchars_to_uchars_with_cmgr(bcs, &mlen, ucs, &wlen, cmgr, all);
	if (ucs)
	{
		/* null-terminate the target buffer if it has room for it. */
		if (wlen < *ucslen) ucs[wlen] = '\0';
		else n = -2; /* buffer too small */
	}
	*bcslen = mlen; *ucslen = wlen;

	return n;
}

int hawk_conv_uchars_to_bchars_with_cmgr (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_cmgr_t* cmgr)
{
	const hawk_uch_t* p = ucs;
	const hawk_uch_t* end = ucs + *ucslen;
	int ret = 0;

	if (bcs)
	{
		hawk_oow_t rem = *bcslen;

		while (p < end)
		{
			hawk_oow_t n;

			if (rem <= 0)
			{
				ret = -2; /* buffer too small */
				break;
			}

			n = cmgr->uctobc(*p, bcs, rem);
			if (n == 0)
			{
				ret = -1;
				break; /* illegal character */
			}
			if (n > rem)
			{
				ret = -2; /* buffer too small */
				break;
			}
			bcs += n; rem -= n; p++;
		}

		*bcslen -= rem;
	}
	else
	{
		hawk_bch_t bcsbuf[HAWK_BCSIZE_MAX];
		hawk_oow_t mlen = 0;

		while (p < end)
		{
			hawk_oow_t n;

			n = cmgr->uctobc(*p, bcsbuf, HAWK_COUNTOF(bcsbuf));
			if (n == 0)
			{
				ret = -1;
				break; /* illegal character */
			}

			/* it assumes that bcsbuf is large enough to hold a character */
			/*HAWK_ASSERT (hawk, n <= HAWK_COUNTOF(bcsbuf));*/

			p++; mlen += n;
		}

		/* this length excludes the terminating null character.
		 * this function doesn't even null-terminate the result. */
		*bcslen = mlen;
	}

	*ucslen = p - ucs;
	return ret;
}

int hawk_conv_ucstr_to_bcstr_with_cmgr (
	const hawk_uch_t* ucs, hawk_oow_t* ucslen,
	hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_cmgr_t* cmgr)
{
	const hawk_uch_t* p = ucs;
	int ret = 0;

	if (bcs)
	{
		hawk_oow_t rem = *bcslen;

		while (*p != '\0')
		{
			hawk_oow_t n;

			if (rem <= 0)
			{
				ret = -2;
				break;
			}

			n = cmgr->uctobc(*p, bcs, rem);
			if (n == 0)
			{
				ret = -1;
				break; /* illegal character */
			}
			if (n > rem)
			{
				ret = -2;
				break; /* buffer too small */
			}

			bcs += n; rem -= n; p++;
		}

		/* update bcslen to the length of the bcs string converted excluding
		 * terminating null */
		*bcslen -= rem;

		/* null-terminate the multibyte sequence if it has sufficient space */
		if (rem > 0) *bcs = '\0';
		else
		{
			/* if ret is -2 and cs[cslen] == '\0',
			 * this means that the bcs buffer was lacking one
			 * slot for the terminating null */
			ret = -2; /* buffer too small */
		}
	}
	else
	{
		hawk_bch_t bcsbuf[HAWK_BCSIZE_MAX];
		hawk_oow_t mlen = 0;

		while (*p != '\0')
		{
			hawk_oow_t n;

			n = cmgr->uctobc(*p, bcsbuf, HAWK_COUNTOF(bcsbuf));
			if (n == 0)
			{
				ret = -1;
				break; /* illegal character */
			}

			/* it assumes that bcs is large enough to hold a character */
			/*HAWK_ASSERT (hawk, n <= HAWK_COUNTOF(bcs));*/

			p++; mlen += n;
		}

		/* this length holds the number of resulting multi-byte characters
		 * excluding the terminating null character */
		*bcslen = mlen;
	}

	*ucslen = p - ucs;  /* the number of wide characters handled. */
	return ret;
}

int hawk_conv_bchars_to_uchars_upto_stopper_with_cmgr (
	const hawk_bch_t* bcs, hawk_oow_t* bcslen,
	hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_uch_t stopper, hawk_cmgr_t* cmgr)
{
	const hawk_bch_t* p;
	int ret = 0;
	hawk_oow_t blen;

	hawk_uch_t w;
	hawk_oow_t ulen = 0;
	hawk_uch_t* wend;

	p = bcs;
	blen = *bcslen;

	if (ucs) wend = ucs + *ucslen;

	/* since it needs to break when a stopper is met,
	 * i can't perform bulky conversion using the buffer
	 * provided. so conversion is conducted character by
	 * character */
	while (blen > 0)
	{
		hawk_oow_t n;

		n = cmgr->bctouc(p, blen, &w);
		if (n == 0)
		{
			/* invalid sequence */
			ret = -1;
			break;
		}
		if (n > blen)
		{
			/* incomplete sequence */
			ret = -3;
			break;
		}

		if (ucs)
		{
			if (ucs >= wend) break;
			*ucs++ = w;
		}

		p += n;
		blen -= n;
		ulen += 1;

		if (w == stopper) break;
	}

	*ucslen = ulen;
	*bcslen = p - bcs;

	return ret;
}

/* ----------------------------------------------------------------------- */

#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	define IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')
#else
#	define IS_PATH_SEP(c) ((c) == '/')
#endif

const hawk_uch_t* hawk_get_base_name_ucstr (const hawk_uch_t* path)
{
	const hawk_uch_t* p, * last = HAWK_NULL;

	for (p = path; *p != '\0'; p++)
	{
		if (IS_PATH_SEP(*p)) last = p;
	}

	return last? (last +1): path;
}

const hawk_bch_t* hawk_get_base_name_bcstr (const hawk_bch_t* path)
{
	const hawk_bch_t* p, * last = HAWK_NULL;

	for (p = path; *p != '\0'; p++)
	{
		if (IS_PATH_SEP(*p)) last = p;
	}

	return last? (last +1): path;
}

