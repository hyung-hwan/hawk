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


int hawk_comp_uchars (const hawk_uch_t* str1, hawk_oow_t len1, const hawk_uch_t* str2, hawk_oow_t len2, int ignorecase)
{
	hawk_uchu_t c1, c2;
	const hawk_uch_t* end1 = str1 + len1;
	const hawk_uch_t* end2 = str2 + len2;

	if (ignorecase)
	{
		while (str1 < end1)
		{
			c1 = hawk_to_uch_lower(*str1);
			if (str2 < end2) 
			{
				c2 = hawk_to_uch_lower(*str2);
				if (c1 > c2) return 1;
				if (c1 < c2) return -1;
			}
			else return 1;
			str1++; str2++;
		}
	}
	else
	{
		while (str1 < end1)
		{
			c1 = *str1;
			if (str2 < end2) 
			{
				c2 = *str2;
				if (c1 > c2) return 1;
				if (c1 < c2) return -1;
			}
			else return 1;
			str1++; str2++;
		}
	}

	return (str2 < end2)? -1: 0;
}

int hawk_comp_bchars (const hawk_bch_t* str1, hawk_oow_t len1, const hawk_bch_t* str2, hawk_oow_t len2, int ignorecase)
{
	hawk_bchu_t c1, c2;
	const hawk_bch_t* end1 = str1 + len1;
	const hawk_bch_t* end2 = str2 + len2;

	if (ignorecase)
	{
		while (str1 < end1)
		{
			c1 = hawk_to_bch_lower(*str1);
			if (str2 < end2) 
			{
				c2 = hawk_to_bch_lower(*str2);
				if (c1 > c2) return 1;
				if (c1 < c2) return -1;
			}
			else return 1;
			str1++; str2++;
		}
	}
	else
	{
		while (str1 < end1)
		{
			c1 = *str1;
			if (str2 < end2) 
			{
				c2 = *str2;
				if (c1 > c2) return 1;
				if (c1 < c2) return -1;
			}
			else return 1;
			str1++; str2++;
		}
	}

	return (str2 < end2)? -1: 0;
}

int hawk_comp_ucstr (const hawk_uch_t* str1, const hawk_uch_t* str2, int ignorecase)
{
	if (ignorecase)
	{
		while (hawk_to_uch_lower(*str1) == hawk_to_uch_lower(*str2))
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((hawk_uchu_t)hawk_to_uch_lower(*str1) > (hawk_uchu_t)hawk_to_uch_lower(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((hawk_uchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
	}
}

int hawk_comp_bcstr (const hawk_bch_t* str1, const hawk_bch_t* str2, int ignorecase)
{
	if (ignorecase)
	{
		while (hawk_to_bch_lower(*str1) == hawk_to_bch_lower(*str2))
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((hawk_bchu_t)hawk_to_bch_lower(*str1) > (hawk_bchu_t)hawk_to_bch_lower(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			if (*str1 == '\0') return 0;
			str1++; str2++;
		}

		return ((hawk_bchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
	}
}

int hawk_comp_ucstr_limited (const hawk_uch_t* str1, const hawk_uch_t* str2, hawk_oow_t maxlen, int ignorecase)
{
	if (maxlen == 0) return 0;

	if (ignorecase)
	{
		while (hawk_to_uch_lower(*str1) == hawk_to_uch_lower(*str2))
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((hawk_uchu_t)hawk_to_uch_lower(*str1) > (hawk_uchu_t)hawk_to_uch_lower(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((hawk_uchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
	}
}

int hawk_comp_bcstr_limited (const hawk_bch_t* str1, const hawk_bch_t* str2, hawk_oow_t maxlen, int ignorecase)
{
	if (maxlen == 0) return 0;

	if (ignorecase)
	{
		while (hawk_to_bch_lower(*str1) == hawk_to_bch_lower(*str2))
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((hawk_bchu_t)hawk_to_bch_lower(*str1) > (hawk_bchu_t)hawk_to_bch_lower(*str2))? 1: -1;
	}
	else
	{
		while (*str1 == *str2)
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((hawk_bchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
	}
}

int hawk_comp_uchars_ucstr (const hawk_uch_t* str1, hawk_oow_t len, const hawk_uch_t* str2, int ignorecase)
{
	/* for "abc\0" of length 4 vs "abc", the fourth character
	 * of the first string is equal to the terminating null of
	 * the second string. the first string is still considered 
	 * bigger */
	if (ignorecase)
	{
		const hawk_uch_t* end = str1 + len;
		hawk_uch_t c1;
		hawk_uch_t c2;
		while (str1 < end && *str2 != '\0') 
		{
			c1 = hawk_to_uch_lower(*str1);
			c2 = hawk_to_uch_lower(*str2);
			if (c1 != c2) return ((hawk_uchu_t)c1 > (hawk_uchu_t)c2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
	else
	{
		const hawk_uch_t* end = str1 + len;
		while (str1 < end && *str2 != '\0') 
		{
			if (*str1 != *str2) return ((hawk_uchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
}

int hawk_comp_bchars_bcstr (const hawk_bch_t* str1, hawk_oow_t len, const hawk_bch_t* str2, int ignorecase)
{
	/* for "abc\0" of length 4 vs "abc", the fourth character
	 * of the first string is equal to the terminating null of
	 * the second string. the first string is still considered 
	 * bigger */
	if (ignorecase)
	{
		const hawk_bch_t* end = str1 + len;
		hawk_bch_t c1;
		hawk_bch_t c2;
		while (str1 < end && *str2 != '\0') 
		{
			c1 = hawk_to_bch_lower(*str1);
			c2 = hawk_to_bch_lower(*str2);
			if (c1 != c2) return ((hawk_bchu_t)c1 > (hawk_bchu_t)c2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
	else
	{
		const hawk_bch_t* end = str1 + len;
		while (str1 < end && *str2 != '\0') 
		{
			if (*str1 != *str2) return ((hawk_bchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
			str1++; str2++;
		}
		return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
	}
}

hawk_oow_t hawk_concat_uchars_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* str, hawk_oow_t len)
{
	hawk_uch_t* p, * p2;
	const hawk_uch_t* end;
	hawk_oow_t blen;

	blen = hawk_count_ucstr(buf);
	if (blen >= bsz) return blen; /* something wrong */

	p = buf + blen;
	p2 = buf + bsz - 1;

	end = str + len;

	while (p < p2) 
	{
		if (str >= end) break;
		*p++ = *str++;
	}

	if (bsz > 0) *p = '\0';
	return p - buf;
}

hawk_oow_t hawk_concat_bchars_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* str, hawk_oow_t len)
{
	hawk_bch_t* p, * p2;
	const hawk_bch_t* end;
	hawk_oow_t blen;

	blen = hawk_count_bcstr(buf);
	if (blen >= bsz) return blen; /* something wrong */

	p = buf + blen;
	p2 = buf + bsz - 1;

	end = str + len;

	while (p < p2) 
	{
		if (str >= end) break;
		*p++ = *str++;
	}

	if (bsz > 0) *p = '\0';
	return p - buf;
}

hawk_oow_t hawk_concat_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* str)
{
	hawk_uch_t* p, * p2;
	hawk_oow_t blen;

	blen = hawk_count_ucstr(buf);
	if (blen >= bsz) return blen; /* something wrong */

	p = buf + blen;
	p2 = buf + bsz - 1;

	while (p < p2) 
	{
		if (*str == '\0') break;
		*p++ = *str++;
	}

	if (bsz > 0) *p = '\0';
	return p - buf;
}

hawk_oow_t hawk_concat_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* str)
{
	hawk_bch_t* p, * p2;
	hawk_oow_t blen;

	blen = hawk_count_bcstr(buf);
	if (blen >= bsz) return blen; /* something wrong */

	p = buf + blen;
	p2 = buf + bsz - 1;

	while (p < p2) 
	{
		if (*str == '\0') break;
		*p++ = *str++;
	}

	if (bsz > 0) *p = '\0';
	return p - buf;
}

void hawk_copy_uchars (hawk_uch_t* dst, const hawk_uch_t* src, hawk_oow_t len)
{
	/* take note of no forced null termination */
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

void hawk_copy_bchars (hawk_bch_t* dst, const hawk_bch_t* src, hawk_oow_t len)
{
	/* take note of no forced null termination */
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
}

hawk_oow_t hawk_copy_uchars_to_ucstr (hawk_uch_t* dst, hawk_oow_t dlen, const hawk_uch_t* src, hawk_oow_t slen)
{
	hawk_oow_t i;
	if (dlen <= 0) return 0;
	if (dlen <= slen) slen = dlen - 1;
	for (i = 0; i < slen; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}

hawk_oow_t hawk_copy_bchars_to_bcstr (hawk_bch_t* dst, hawk_oow_t dlen, const hawk_bch_t* src, hawk_oow_t slen)
{
	hawk_oow_t i;
	if (dlen <= 0) return 0;
	if (dlen <= slen) slen = dlen - 1;
	for (i = 0; i < slen; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}

hawk_oow_t hawk_copy_uchars_to_ucstr_unlimited (hawk_uch_t* dst, const hawk_uch_t* src, hawk_oow_t len)
{
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}

hawk_oow_t hawk_copy_bchars_to_bcstr_unlimited (hawk_bch_t* dst, const hawk_bch_t* src, hawk_oow_t len)
{
	hawk_oow_t i;
	for (i = 0; i < len; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}

hawk_oow_t hawk_copy_ucstr_to_uchars (hawk_uch_t* dst, hawk_oow_t len, const hawk_uch_t* src)
{
	/* no null termination */
	hawk_uch_t* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	return p - dst;
}

hawk_oow_t hawk_copy_bcstr_to_bchars (hawk_bch_t* dst, hawk_oow_t len, const hawk_bch_t* src)
{
	/* no null termination */
	hawk_bch_t* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	return p - dst;
}

hawk_oow_t hawk_copy_ucstr (hawk_uch_t* dst, hawk_oow_t len, const hawk_uch_t* src)
{
	hawk_uch_t* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	if (len > 0) *p = '\0';
	return p - dst;
}

hawk_oow_t hawk_copy_bcstr (hawk_bch_t* dst, hawk_oow_t len, const hawk_bch_t* src)
{
	hawk_bch_t* p, * p2;

	p = dst; p2 = dst + len - 1;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	if (len > 0) *p = '\0';
	return p - dst;
}

hawk_oow_t hawk_copy_ucstr_unlimited (hawk_uch_t* dst, const hawk_uch_t* src)
{
	hawk_uch_t* org = dst;
	while ((*dst++ = *src++) != '\0');
	return dst - org - 1;
}

hawk_oow_t hawk_copy_bcstr_unlimited (hawk_bch_t* dst, const hawk_bch_t* src)
{
	hawk_bch_t* org = dst;
	while ((*dst++ = *src++) != '\0');
	return dst - org - 1;
}

hawk_oow_t hawk_copy_fmt_ucstrs_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, const hawk_uch_t* str[])
{
	hawk_uch_t* b = buf;
	hawk_uch_t* end = buf + bsz - 1;
	const hawk_uch_t* f = fmt;

	if (bsz <= 0) return 0;

	while (*f != '\0')
	{
		if (*f == '\\')
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it
			 * normally also. */
			if (f[1] != '\0') f++;
		}
		else if (*f == '$')
		{
			if (f[1] == '{' && (f[2] >= '0' && f[2] <= '9'))
			{
				const hawk_uch_t* tmp;
				hawk_oow_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - '0');
				while (*f >= '0' && *f <= '9');

				if (*f != '}')
				{
					f = tmp;
					goto normal;
				}

				f++;

				tmp = str[idx];
				while (*tmp != '\0')
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == '$') f++;
		}

	normal:
		if (b >= end) break;
		*b++ = *f++;
	}

fini:
	*b = '\0';
	return b - buf;
}

hawk_oow_t hawk_copy_fmt_bcstrs_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, const hawk_bch_t* str[])
{
	hawk_bch_t* b = buf;
	hawk_bch_t* end = buf + bsz - 1;
	const hawk_bch_t* f = fmt;

	if (bsz <= 0) return 0;

	while (*f != '\0')
	{
		if (*f == '\\')
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it
			 * normally also. */
			if (f[1] != '\0') f++;
		}
		else if (*f == '$')
		{
			if (f[1] == '{' && (f[2] >= '0' && f[2] <= '9'))
			{
				const hawk_bch_t* tmp;
				hawk_oow_t idx = 0;

				tmp = f;
				f += 2;

				do idx = idx * 10 + (*f++ - '0');
				while (*f >= '0' && *f <= '9');

				if (*f != '}')
				{
					f = tmp;
					goto normal;
				}

				f++;

				tmp = str[idx];
				while (*tmp != '\0')
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == '$') f++;
		}

	normal:
		if (b >= end) break;
		*b++ = *f++;
	}

fini:
	*b = '\0';
	return b - buf;
}

hawk_oow_t hawk_copy_fmt_ucses_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, const hawk_ucs_t str[])
{
	hawk_uch_t* b = buf;
	hawk_uch_t* end = buf + bsz - 1;
	const hawk_uch_t* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != '\0')
	{
		if (*f == '\\')
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != '\0') f++;
		}
		else if (*f == '$')
		{
			if (f[1] == '{' && (f[2] >= '0' && f[2] <= '9'))
			{
				const hawk_uch_t* tmp, * tmpend;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - '0');
				while (*f >= '0' && *f <= '9');
	
				if (*f != '}')
				{
					f = tmp;
					goto normal;
				}
 
				f++;
				
				tmp = str[idx].ptr;
				tmpend = tmp + str[idx].len;
 
				while (tmp < tmpend)
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == '$') f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = '\0';
	return b - buf;
}

hawk_oow_t hawk_copy_fmt_bcses_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, const hawk_bcs_t str[])
{
	hawk_bch_t* b = buf;
	hawk_bch_t* end = buf + bsz - 1;
	const hawk_bch_t* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != '\0')
	{
		if (*f == '\\')
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != '\0') f++;
		}
		else if (*f == '$')
		{
			if (f[1] == '{' && (f[2] >= '0' && f[2] <= '9'))
			{
				const hawk_bch_t* tmp, * tmpend;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - '0');
				while (*f >= '0' && *f <= '9');
	
				if (*f != '}')
				{
					f = tmp;
					goto normal;
				}
 
				f++;
				
				tmp = str[idx].ptr;
				tmpend = tmp + str[idx].len;
 
				while (tmp < tmpend)
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == '$') f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = '\0';
	return b - buf;
}

hawk_oow_t hawk_count_ucstr (const hawk_uch_t* str)
{
	const hawk_uch_t* ptr = str;
	while (*ptr != '\0') ptr++;
	return ptr - str;
} 

hawk_oow_t hawk_count_bcstr (const hawk_bch_t* str)
{
	const hawk_bch_t* ptr = str;
	while (*ptr != '\0') ptr++;
	return ptr - str;
} 

hawk_oow_t hawk_count_ucstr_limited (const hawk_uch_t* str, hawk_oow_t maxlen)
{
	hawk_oow_t i;
	for (i = 0; i < maxlen; i++)
	{
		if (str[i] == '\0') break;
	}
	return i;
}

hawk_oow_t hawk_count_bcstr_limited (const hawk_bch_t* str, hawk_oow_t maxlen)
{
	hawk_oow_t i;
	for (i = 0; i < maxlen; i++)
	{
		if (str[i] == '\0') break;
	}
	return i;
}

int hawk_equal_uchars (const hawk_uch_t* str1, const hawk_uch_t* str2, hawk_oow_t len)
{
	hawk_oow_t i;

	/* NOTE: you should call this function after having ensured that
	 *       str1 and str2 are in the same length */

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}

int hawk_equal_bchars (const hawk_bch_t* str1, const hawk_bch_t* str2, hawk_oow_t len)
{
	hawk_oow_t i;

	/* NOTE: you should call this function after having ensured that
	 *       str1 and str2 are in the same length */

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}

void hawk_fill_uchars (hawk_uch_t* dst, hawk_uch_t ch, hawk_oow_t len)
{
        hawk_oow_t i;
        for (i = 0; i < len; i++) dst[i] = ch;
}

void hawk_fill_bchars (hawk_bch_t* dst, hawk_bch_t ch, hawk_oow_t len)
{
        hawk_oow_t i;
        for (i = 0; i < len; i++) dst[i] = ch;
}

hawk_uch_t* hawk_find_uchar_in_uchars (const hawk_uch_t* ptr, hawk_oow_t len, hawk_uch_t c)
{
	const hawk_uch_t* end;

	end = ptr + len;
	while (ptr < end)
	{
		if (*ptr == c) return (hawk_uch_t*)ptr;
		ptr++;
	}

	return HAWK_NULL;
}

hawk_bch_t* hawk_find_bchar_in_bchars (const hawk_bch_t* ptr, hawk_oow_t len, hawk_bch_t c)
{
	const hawk_bch_t* end;

	end = ptr + len;
	while (ptr < end)
	{
		if (*ptr == c) return (hawk_bch_t*)ptr;
		ptr++;
	}

	return HAWK_NULL;
}

hawk_uch_t* hawk_rfind_uchar_in_uchars (const hawk_uch_t* ptr, hawk_oow_t len, hawk_uch_t c)
{
	const hawk_uch_t* cur;

	cur = ptr + len;
	while (cur > ptr)
	{
		--cur;
		if (*cur == c) return (hawk_uch_t*)cur;
	}

	return HAWK_NULL;
}

hawk_bch_t* hawk_rfind_bchar_in_bchars (const hawk_bch_t* ptr, hawk_oow_t len, hawk_bch_t c)
{
	const hawk_bch_t* cur;

	cur = ptr + len;
	while (cur > ptr)
	{
		--cur;
		if (*cur == c) return (hawk_bch_t*)cur;
	}

	return HAWK_NULL;
}

hawk_uch_t* hawk_find_uchar_in_ucstr (const hawk_uch_t* ptr, hawk_uch_t c)
{
	while (*ptr != '\0')
	{
		if (*ptr == c) return (hawk_uch_t*)ptr;
		ptr++;
	}

	return HAWK_NULL;
}

hawk_bch_t* hawk_find_bchar_in_bcstr (const hawk_bch_t* ptr, hawk_bch_t c)
{
	while (*ptr != '\0')
	{
		if (*ptr == c) return (hawk_bch_t*)ptr;
		ptr++;
	}

	return HAWK_NULL;
}

hawk_uch_t* hawk_rfind_uchar_in_ucstr (const hawk_uch_t* str, hawk_uch_t c)
{
	const hawk_uch_t* ptr = str;
	while (*ptr != '\0') ptr++;

	while (ptr > str)
	{
		--ptr;
		if (*ptr == c) return (hawk_uch_t*)ptr;
	}

	return HAWK_NULL;
}

hawk_bch_t* hawk_rfind_bchar_in_bcstr (const hawk_bch_t* str, hawk_bch_t c)
{
	const hawk_bch_t* ptr = str;
	while (*ptr != '\0') ptr++;

	while (ptr > str)
	{
		--ptr;
		if (*ptr == c) return (hawk_bch_t*)ptr;
	}

	return HAWK_NULL;
}

hawk_uch_t* hawk_find_uchars_in_uchars (const hawk_uch_t* str, hawk_oow_t strsz, const hawk_uch_t* sub, hawk_oow_t subsz, int ignorecase)
{
	const hawk_uch_t* end, * subp;

	if (subsz == 0) return (hawk_uch_t*)str;
	if (strsz < subsz) return HAWK_NULL;
	
	end = str + strsz - subsz;
	subp = sub + subsz;

	if (HAWK_UNLIKELY(ignorecase))
	{
		while (str <= end) 
		{
			const hawk_uch_t* x = str;
			const hawk_uch_t* y = sub;

			while (1)
			{
				if (y >= subp) return (hawk_uch_t*)str;
				if (hawk_to_uch_lower(*x) != hawk_to_uch_lower(*y)) break;
				x++; y++;
			}

			str++;
		}
	}
	else
	{
		while (str <= end) 
		{
			const hawk_uch_t* x = str;
			const hawk_uch_t* y = sub;

			while (1)
			{
				if (y >= subp) return (hawk_uch_t*)str;
				if (*x != *y) break;
				x++; y++;
			}

			str++;
		}
	}

	return HAWK_NULL;
}

hawk_bch_t* hawk_find_bchars_in_bchars (const hawk_bch_t* str, hawk_oow_t strsz, const hawk_bch_t* sub, hawk_oow_t subsz, int ignorecase)
{
	const hawk_bch_t* end, * subp;

	if (subsz == 0) return (hawk_bch_t*)str;
	if (strsz < subsz) return HAWK_NULL;
	
	end = str + strsz - subsz;
	subp = sub + subsz;

	if (HAWK_UNLIKELY(ignorecase))
	{
		while (str <= end) 
		{
			const hawk_bch_t* x = str;
			const hawk_bch_t* y = sub;

			while (1)
			{
				if (y >= subp) return (hawk_bch_t*)str;
				if (hawk_to_bch_lower(*x) != hawk_to_bch_lower(*y)) break;
				x++; y++;
			}

			str++;
		}
	}
	else
	{
		while (str <= end) 
		{
			const hawk_bch_t* x = str;
			const hawk_bch_t* y = sub;

			while (1)
			{
				if (y >= subp) return (hawk_bch_t*)str;
				if (*x != *y) break;
				x++; y++;
			}

			str++;
		}
	}

	return HAWK_NULL;
}

hawk_uch_t* hawk_rfind_uchars_in_uchars (const hawk_uch_t* str, hawk_oow_t strsz, const hawk_uch_t* sub, hawk_oow_t subsz, int ignorecase)
{
	const hawk_uch_t* p = str + strsz;
	const hawk_uch_t* subp = sub + subsz;

	if (subsz == 0) return (hawk_uch_t*)p;
	if (strsz < subsz) return HAWK_NULL;

	p = p - subsz;

	if (HAWK_UNLIKELY(ignorecase))
	{
		while (p >= str) 
		{
			const hawk_uch_t* x = p;
			const hawk_uch_t* y = sub;

			while (1) 
			{
				if (y >= subp) return (hawk_uch_t*)p;
				if (hawk_to_uch_lower(*x) != hawk_to_uch_lower(*y)) break;
				x++; y++;
			}

			p--;
		}
	}
	else
	{
		while (p >= str) 
		{
			const hawk_uch_t* x = p;
			const hawk_uch_t* y = sub;

			while (1) 
			{
				if (y >= subp) return (hawk_uch_t*)p;
				if (*x != *y) break;
				x++; y++;
			}	

			p--;
		}
	}

	return HAWK_NULL;
}

hawk_bch_t* hawk_rfind_bchars_in_bchars (const hawk_bch_t* str, hawk_oow_t strsz, const hawk_bch_t* sub, hawk_oow_t subsz, int ignorecase)
{
	const hawk_bch_t* p = str + strsz;
	const hawk_bch_t* subp = sub + subsz;

	if (subsz == 0) return (hawk_bch_t*)p;
	if (strsz < subsz) return HAWK_NULL;

	p = p - subsz;

	if (HAWK_UNLIKELY(ignorecase))
	{
		while (p >= str) 
		{
			const hawk_bch_t* x = p;
			const hawk_bch_t* y = sub;

			while (1) 
			{
				if (y >= subp) return (hawk_bch_t*)p;
				if (hawk_to_bch_lower(*x) != hawk_to_bch_lower(*y)) break;
				x++; y++;
			}

			p--;
		}
	}
	else
	{
		while (p >= str) 
		{
			const hawk_bch_t* x = p;
			const hawk_bch_t* y = sub;

			while (1) 
			{
				if (y >= subp) return (hawk_bch_t*)p;
				if (*x != *y) break;
				x++; y++;
			}	

			p--;
		}
	}

	return HAWK_NULL;
}

hawk_oow_t hawk_compact_uchars (hawk_uch_t* str, hawk_oow_t len)
{
	hawk_uch_t* p = str, * q = str, * end = str + len;
	int followed_by_space = 0;
	int state = 0;

	while (p < end) 
	{
		if (state == 0) 
		{
			if (!hawk_is_uch_space(*p)) 
			{
				*q++ = *p;
				state = 1;
			}
		}
		else if (state == 1) 
		{
			if (hawk_is_uch_space(*p)) 
			{
				if (!followed_by_space) 
				{
					followed_by_space = 1;
					*q++ = *p;
				}
			}
			else 
			{
				followed_by_space = 0;
				*q++ = *p;	
			}
		}

		p++;
	}

	return (followed_by_space) ? (q - str -1): (q - str);
}

hawk_oow_t hawk_compact_bchars (hawk_bch_t* str, hawk_oow_t len)
{
	hawk_bch_t* p = str, * q = str, * end = str + len;
	int followed_by_space = 0;
	int state = 0;

	while (p < end) 
	{
		if (state == 0) 
		{
			if (!hawk_is_bch_space(*p)) 
			{
				*q++ = *p;
				state = 1;
			}
		}
		else if (state == 1) 
		{
			if (hawk_is_bch_space(*p)) 
			{
				if (!followed_by_space) 
				{
					followed_by_space = 1;
					*q++ = *p;
				}
			}
			else 
			{
				followed_by_space = 0;
				*q++ = *p;	
			}
		}

		p++;
	}

	return (followed_by_space) ? (q - str -1): (q - str);
}

hawk_oow_t hawk_rotate_uchars (hawk_uch_t* str, hawk_oow_t len, int dir, hawk_oow_t n)
{
	hawk_oow_t first, last, count, index, nk;
	hawk_uch_t c;

	if (dir == 0 || len == 0) return len;
	if ((n %= len) == 0) return len;

	if (dir > 0) n = len - n;
	first = 0; nk = len - n; count = 0;

	while (count < n)
	{
		last = first + nk;
		index = first;
		c = str[first];
		do
		{
			count++;
			while (index < nk)
			{
				str[index] = str[index + n];
				index += n;
			}
			if (index == last) break;
			str[index] = str[index - nk];
			index -= nk;
		}
		while (1);
		str[last] = c; first++;
	}
	return len;
}

hawk_oow_t hawk_rotate_bchars (hawk_bch_t* str, hawk_oow_t len, int dir, hawk_oow_t n)
{
	hawk_oow_t first, last, count, index, nk;
	hawk_bch_t c;

	if (dir == 0 || len == 0) return len;
	if ((n %= len) == 0) return len;

	if (dir > 0) n = len - n;
	first = 0; nk = len - n; count = 0;

	while (count < n)
	{
		last = first + nk;
		index = first;
		c = str[first];
		do
		{
			count++;
			while (index < nk)
			{
				str[index] = str[index + n];
				index += n;
			}
			if (index == last) break;
			str[index] = str[index - nk];
			index -= nk;
		}
		while (1);
		str[last] = c; first++;
	}
	return len;
}

hawk_uch_t* hawk_trim_uchars (const hawk_uch_t* str, hawk_oow_t* len, int flags)
{
	const hawk_uch_t* p = str, * end = str + *len;

	if (p < end)
	{
		const hawk_uch_t* s = HAWK_NULL, * e = HAWK_NULL;

		do
		{
			if (!hawk_is_uch_space(*p))
			{
				if (s == HAWK_NULL) s = p;
				e = p;
			}
			p++;
		}
		while (p < end);

		if (e)
		{
			if (flags & HAWK_TRIM_UCHARS_RIGHT) 
			{
				*len -= end - e - 1;
			}
			if (flags & HAWK_TRIM_UCHARS_LEFT) 
			{
				*len -= s - str;
				str = s;
			}
		}
		else
		{
			/* the entire string need to be deleted */
			if ((flags & HAWK_TRIM_UCHARS_RIGHT) || 
			    (flags & HAWK_TRIM_UCHARS_LEFT)) *len = 0;
		}
	}

	return (hawk_uch_t*)str;
}

hawk_bch_t* hawk_trim_bchars (const hawk_bch_t* str, hawk_oow_t* len, int flags)
{
	const hawk_bch_t* p = str, * end = str + *len;

	if (p < end)
	{
		const hawk_bch_t* s = HAWK_NULL, * e = HAWK_NULL;

		do
		{
			if (!hawk_is_bch_space(*p))
			{
				if (s == HAWK_NULL) s = p;
				e = p;
			}
			p++;
		}
		while (p < end);

		if (e)
		{
			if (flags & HAWK_TRIM_BCHARS_RIGHT) 
			{
				*len -= end - e - 1;
			}
			if (flags & HAWK_TRIM_BCHARS_LEFT) 
			{
				*len -= s - str;
				str = s;
			}
		}
		else
		{
			/* the entire string need to be deleted */
			if ((flags & HAWK_TRIM_BCHARS_RIGHT) || 
			    (flags & HAWK_TRIM_BCHARS_LEFT)) *len = 0;
		}
	}

	return (hawk_bch_t*)str;
}

int hawk_split_ucstr (hawk_uch_t* s, const hawk_uch_t* delim, hawk_uch_t lquote, hawk_uch_t rquote, hawk_uch_t escape)
{
	hawk_uch_t* p = s, *d;
	hawk_uch_t* sp = HAWK_NULL, * ep = HAWK_NULL;
	int delim_mode;
	int cnt = 0;

	if (delim == HAWK_NULL) delim_mode = 0;
	else 
	{
		delim_mode = 1;
		for (d = (hawk_uch_t*)delim; *d != '\0'; d++)
			if (!hawk_is_uch_space(*d)) delim_mode = 2;
	}

	if (delim_mode == 0) 
	{
		/* skip preceding space characters */
		while (hawk_is_uch_space(*p)) p++;

		/* when 0 is given as "delim", it has an effect of cutting
		   preceding and trailing space characters off "s". */
		if (lquote != '\0' && *p == lquote) 
		{
			hawk_copy_ucstr_unlimited (p, p + 1);

			for (;;) 
			{
				if (*p == '\0') return -1;

				if (escape != '\0' && *p == escape) 
				{
					hawk_copy_ucstr_unlimited (p, p + 1);
				}
				else 
				{
					if (*p == rquote) 
					{
						p++;
						break;
					}
				}

				if (sp == 0) sp = p;
				ep = p;
				p++;
			}
			while (hawk_is_uch_space(*p)) p++;
			if (*p != '\0') return -1;

			if (sp == 0 && ep == 0) s[0] = '\0';
			else 
			{
				ep[1] = '\0';
				if (s != (hawk_uch_t*)sp) hawk_copy_ucstr_unlimited (s, sp);
				cnt++;
			}
		}
		else 
		{
			while (*p) 
			{
				if (!hawk_is_uch_space(*p)) 
				{
					if (sp == 0) sp = p;
					ep = p;
				}
				p++;
			}

			if (sp == 0 && ep == 0) s[0] = '\0';
			else 
			{
				ep[1] = '\0';
				if (s != (hawk_uch_t*)sp) hawk_copy_ucstr_unlimited (s, sp);
				cnt++;
			}
		}
	}
	else if (delim_mode == 1) 
	{
		hawk_uch_t* o;

		while (*p) 
		{
			o = p;
			while (hawk_is_uch_space(*p)) p++;
			if (o != p) { hawk_copy_ucstr_unlimited (o, p); p = o; }

			if (lquote != '\0' && *p == lquote) 
			{
				hawk_copy_ucstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == '\0') return -1;

					if (escape != '\0' && *p == escape) 
					{
						hawk_copy_ucstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = '\0';
							cnt++;
							break;
						}
					}
					p++;
				}
			}
			else 
			{
				o = p;
				for (;;) 
				{
					if (*p == '\0') 
					{
						if (o != p) cnt++;
						break;
					}
					if (hawk_is_uch_space(*p)) 
					{
						*p++ = '\0';
						cnt++;
						break;
					}
					p++;
				}
			}
		}
	}
	else /* if (delim_mode == 2) */
	{
		hawk_uch_t* o;
		int ok;

		while (*p != '\0') 
		{
			o = p;
			while (hawk_is_uch_space(*p)) p++;
			if (o != p) { hawk_copy_ucstr_unlimited (o, p); p = o; }

			if (lquote != '\0' && *p == lquote) 
			{
				hawk_copy_ucstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == '\0') return -1;

					if (escape != '\0' && *p == escape) 
					{
						hawk_copy_ucstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = '\0';
							cnt++;
							break;
						}
					}
					p++;
				}

				ok = 0;
				while (hawk_is_uch_space(*p)) p++;
				if (*p == '\0') ok = 1;
				for (d = (hawk_uch_t*)delim; *d != '\0'; d++) 
				{
					if (*p == *d) 
					{
						ok = 1;
						hawk_copy_ucstr_unlimited (p, p + 1);
						break;
					}
				}
				if (ok == 0) return -1;
			}
			else 
			{
				o = p; sp = ep = 0;

				for (;;) 
				{
					if (*p == '\0') 
					{
						if (ep) 
						{
							ep[1] = '\0';
							p = &ep[1];
						}
						cnt++;
						break;
					}
					for (d = (hawk_uch_t*)delim; *d != '\0'; d++) 
					{
						if (*p == *d)  
						{
							if (sp == HAWK_NULL) 
							{
								hawk_copy_ucstr_unlimited (o, p); p = o;
								*p++ = '\0';
							}
							else 
							{
								hawk_copy_ucstr_unlimited (&ep[1], p);
								hawk_copy_ucstr_unlimited (o, sp);
								o[ep - sp + 1] = '\0';
								p = &o[ep - sp + 2];
							}
							cnt++;
							/* last empty field after delim */
							if (*p == '\0') cnt++;
							goto exit_point;
						}
					}

					if (!hawk_is_uch_space(*p)) 
					{
						if (sp == HAWK_NULL) sp = p;
						ep = p;
					}
					p++;
				}
exit_point:
				;
			}
		}
	}

	return cnt;
}

int hawk_split_bcstr (hawk_bch_t* s, const hawk_bch_t* delim, hawk_bch_t lquote, hawk_bch_t rquote, hawk_bch_t escape)
{
	hawk_bch_t* p = s, *d;
	hawk_bch_t* sp = HAWK_NULL, * ep = HAWK_NULL;
	int delim_mode;
	int cnt = 0;

	if (delim == HAWK_NULL) delim_mode = 0;
	else 
	{
		delim_mode = 1;
		for (d = (hawk_bch_t*)delim; *d != '\0'; d++)
			if (!hawk_is_bch_space(*d)) delim_mode = 2;
	}

	if (delim_mode == 0) 
	{
		/* skip preceding space characters */
		while (hawk_is_bch_space(*p)) p++;

		/* when 0 is given as "delim", it has an effect of cutting
		   preceding and trailing space characters off "s". */
		if (lquote != '\0' && *p == lquote) 
		{
			hawk_copy_bcstr_unlimited (p, p + 1);

			for (;;) 
			{
				if (*p == '\0') return -1;

				if (escape != '\0' && *p == escape) 
				{
					hawk_copy_bcstr_unlimited (p, p + 1);
				}
				else 
				{
					if (*p == rquote) 
					{
						p++;
						break;
					}
				}

				if (sp == 0) sp = p;
				ep = p;
				p++;
			}
			while (hawk_is_bch_space(*p)) p++;
			if (*p != '\0') return -1;

			if (sp == 0 && ep == 0) s[0] = '\0';
			else 
			{
				ep[1] = '\0';
				if (s != (hawk_bch_t*)sp) hawk_copy_bcstr_unlimited (s, sp);
				cnt++;
			}
		}
		else 
		{
			while (*p) 
			{
				if (!hawk_is_bch_space(*p)) 
				{
					if (sp == 0) sp = p;
					ep = p;
				}
				p++;
			}

			if (sp == 0 && ep == 0) s[0] = '\0';
			else 
			{
				ep[1] = '\0';
				if (s != (hawk_bch_t*)sp) hawk_copy_bcstr_unlimited (s, sp);
				cnt++;
			}
		}
	}
	else if (delim_mode == 1) 
	{
		hawk_bch_t* o;

		while (*p) 
		{
			o = p;
			while (hawk_is_bch_space(*p)) p++;
			if (o != p) { hawk_copy_bcstr_unlimited (o, p); p = o; }

			if (lquote != '\0' && *p == lquote) 
			{
				hawk_copy_bcstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == '\0') return -1;

					if (escape != '\0' && *p == escape) 
					{
						hawk_copy_bcstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = '\0';
							cnt++;
							break;
						}
					}
					p++;
				}
			}
			else 
			{
				o = p;
				for (;;) 
				{
					if (*p == '\0') 
					{
						if (o != p) cnt++;
						break;
					}
					if (hawk_is_bch_space(*p)) 
					{
						*p++ = '\0';
						cnt++;
						break;
					}
					p++;
				}
			}
		}
	}
	else /* if (delim_mode == 2) */
	{
		hawk_bch_t* o;
		int ok;

		while (*p != '\0') 
		{
			o = p;
			while (hawk_is_bch_space(*p)) p++;
			if (o != p) { hawk_copy_bcstr_unlimited (o, p); p = o; }

			if (lquote != '\0' && *p == lquote) 
			{
				hawk_copy_bcstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == '\0') return -1;

					if (escape != '\0' && *p == escape) 
					{
						hawk_copy_bcstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = '\0';
							cnt++;
							break;
						}
					}
					p++;
				}

				ok = 0;
				while (hawk_is_bch_space(*p)) p++;
				if (*p == '\0') ok = 1;
				for (d = (hawk_bch_t*)delim; *d != '\0'; d++) 
				{
					if (*p == *d) 
					{
						ok = 1;
						hawk_copy_bcstr_unlimited (p, p + 1);
						break;
					}
				}
				if (ok == 0) return -1;
			}
			else 
			{
				o = p; sp = ep = 0;

				for (;;) 
				{
					if (*p == '\0') 
					{
						if (ep) 
						{
							ep[1] = '\0';
							p = &ep[1];
						}
						cnt++;
						break;
					}
					for (d = (hawk_bch_t*)delim; *d != '\0'; d++) 
					{
						if (*p == *d)  
						{
							if (sp == HAWK_NULL) 
							{
								hawk_copy_bcstr_unlimited (o, p); p = o;
								*p++ = '\0';
							}
							else 
							{
								hawk_copy_bcstr_unlimited (&ep[1], p);
								hawk_copy_bcstr_unlimited (o, sp);
								o[ep - sp + 1] = '\0';
								p = &o[ep - sp + 2];
							}
							cnt++;
							/* last empty field after delim */
							if (*p == '\0') cnt++;
							goto exit_point;
						}
					}

					if (!hawk_is_bch_space(*p)) 
					{
						if (sp == HAWK_NULL) sp = p;
						ep = p;
					}
					p++;
				}
exit_point:
				;
			}
		}
	}

	return cnt;
}

hawk_uch_t* hawk_tokenize_uchars (const hawk_uch_t* s, hawk_oow_t len, const hawk_uch_t* delim, hawk_oow_t delim_len, hawk_ucs_t* tok, int ignorecase)
{
	const hawk_uch_t* p = s, *d;
	const hawk_uch_t* end = s + len;
	const hawk_uch_t* sp = HAWK_NULL, * ep = HAWK_NULL;
	const hawk_uch_t* delim_end = delim + delim_len;
	hawk_uch_t c; 
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
			if (hawk_is_uch_space(*d)) 
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
		if (delim_mode == __DELIM_SPACES && delim_len == 1 && delim[0] != ' ') delim_mode = __DELIM_NOSPACES;
	}		
	
	if (delim_mode == __DELIM_NULL) 
	{ 
		/* when HAWK_NULL is given as "delim", it trims off the 
		 * leading and trailing spaces characters off the source
		 * string "s" eventually. */

		while (p < end && hawk_is_uch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;

			if (!hawk_is_uch_space(c)) 
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

		while (p < end && hawk_is_uch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;
			if (hawk_is_uch_space(c)) break;
			if (sp == HAWK_NULL) sp = p;
			ep = p++;
		}
		while (p < end && hawk_is_uch_space(*p)) p++;
	}
	else if (delim_mode == __DELIM_NOSPACES)
	{
		/* each token is delimited by one of charaters 
		 * in the delimeter set "delim". */
		if (ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_uch_lower(*p);
				for (d = delim; d < delim_end; d++) 
				{
					if (c == hawk_to_uch_lower(*d)) goto exit_loop;
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
		while (p < end && hawk_is_uch_space(*p)) p++;
		if (ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_uch_lower(*p);
				if (hawk_is_uch_space(c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == hawk_to_uch_lower(*d)) goto exit_loop;
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
				if (hawk_is_uch_space(c)) 
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
		tok->ptr = (hawk_uch_t*)sp;
		tok->len = ep - sp + 1;
	}

	/* if HAWK_NULL is returned, this function should not be called again */
	if (p >= end) return HAWK_NULL;
	if (delim_mode == __DELIM_EMPTY || delim_mode == __DELIM_SPACES) return (hawk_uch_t*)p;
	return (hawk_uch_t*)++p;
}

hawk_bch_t* hawk_tokenize_bchars (const hawk_bch_t* s, hawk_oow_t len, const hawk_bch_t* delim, hawk_oow_t delim_len, hawk_bcs_t* tok, int ignorecase)
{
	const hawk_bch_t* p = s, *d;
	const hawk_bch_t* end = s + len;
	const hawk_bch_t* sp = HAWK_NULL, * ep = HAWK_NULL;
	const hawk_bch_t* delim_end = delim + delim_len;
	hawk_bch_t c; 
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
			if (hawk_is_bch_space(*d)) 
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
		if (delim_mode == __DELIM_SPACES && delim_len == 1 && delim[0] != ' ') delim_mode = __DELIM_NOSPACES;
	}		
	
	if (delim_mode == __DELIM_NULL) 
	{ 
		/* when HAWK_NULL is given as "delim", it trims off the 
		 * leading and trailing spaces characters off the source
		 * string "s" eventually. */

		while (p < end && hawk_is_bch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;

			if (!hawk_is_bch_space(c)) 
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

		while (p < end && hawk_is_bch_space(*p)) p++;
		while (p < end) 
		{
			c = *p;
			if (hawk_is_bch_space(c)) break;
			if (sp == HAWK_NULL) sp = p;
			ep = p++;
		}
		while (p < end && hawk_is_bch_space(*p)) p++;
	}
	else if (delim_mode == __DELIM_NOSPACES)
	{
		/* each token is delimited by one of charaters 
		 * in the delimeter set "delim". */
		if (ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_bch_lower(*p);
				for (d = delim; d < delim_end; d++) 
				{
					if (c == hawk_to_bch_lower(*d)) goto exit_loop;
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
		while (p < end && hawk_is_bch_space(*p)) p++;
		if (ignorecase)
		{
			while (p < end) 
			{
				c = hawk_to_bch_lower(*p);
				if (hawk_is_bch_space(c)) 
				{
					p++;
					continue;
				}
				for (d = delim; d < delim_end; d++) 
				{
					if (c == hawk_to_bch_lower(*d)) goto exit_loop;
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
				if (hawk_is_bch_space(c)) 
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
		tok->ptr = (hawk_bch_t*)sp;
		tok->len = ep - sp + 1;
	}

	/* if HAWK_NULL is returned, this function should not be called again */
	if (p >= end) return HAWK_NULL;
	if (delim_mode == __DELIM_EMPTY || delim_mode == __DELIM_SPACES) return (hawk_bch_t*)p;
	return (hawk_bch_t*)++p;
}

hawk_oow_t hawk_byte_to_ucstr (hawk_uint8_t byte, hawk_uch_t* buf, hawk_oow_t size, int flagged_radix, hawk_uch_t fill)
{
	hawk_uch_t tmp[(HAWK_SIZEOF(hawk_uint8_t) * HAWK_BITS_PER_BYTE)];
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

hawk_oow_t hawk_byte_to_bcstr (hawk_uint8_t byte, hawk_bch_t* buf, hawk_oow_t size, int flagged_radix, hawk_bch_t fill)
{
	hawk_bch_t tmp[(HAWK_SIZEOF(hawk_uint8_t) * HAWK_BITS_PER_BYTE)];
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

hawk_oow_t hawk_int_to_ucstr (hawk_int_t value, int radix, const hawk_uch_t* prefix, hawk_uch_t* buf, hawk_oow_t size)
{
	hawk_int_t t, rem;
	hawk_oow_t len, ret, i;
	hawk_oow_t prefix_len;

	prefix_len = (prefix != HAWK_NULL)? hawk_count_ucstr(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == HAWK_NULL) 
		{
			/* if buf is not given, 
			 * return the number of bytes required */
			return prefix_len + 1;
		}

		if (size < prefix_len+1) 
		{
			/* buffer too small */
			return (hawk_oow_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = '0';
		if (size > prefix_len+1) buf[prefix_len+1] = '\0';
		return prefix_len+1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == HAWK_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (hawk_oow_t)-1; /* buffer too small */
	if (size > len) buf[len] = '\0';
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (hawk_uch_t)rem + 'a' - 10;
		else
			buf[--len] = (hawk_uch_t)rem + '0';
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = '-';
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}

hawk_oow_t hawk_int_to_bcstr (hawk_int_t value, int radix, const hawk_bch_t* prefix, hawk_bch_t* buf, hawk_oow_t size)
{
	hawk_int_t t, rem;
	hawk_oow_t len, ret, i;
	hawk_oow_t prefix_len;

	prefix_len = (prefix != HAWK_NULL)? hawk_count_bcstr(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == HAWK_NULL) 
		{
			/* if buf is not given, 
			 * return the number of bytes required */
			return prefix_len + 1;
		}

		if (size < prefix_len+1) 
		{
			/* buffer too small */
			return (hawk_oow_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = '0';
		if (size > prefix_len+1) buf[prefix_len+1] = '\0';
		return prefix_len+1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == HAWK_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (hawk_oow_t)-1; /* buffer too small */
	if (size > len) buf[len] = '\0';
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (hawk_bch_t)rem + 'a' - 10;
		else
			buf[--len] = (hawk_bch_t)rem + '0';
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = '-';
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}

hawk_oow_t hawk_uint_to_ucstr (hawk_uint_t value, int radix, const hawk_uch_t* prefix, hawk_uch_t* buf, hawk_oow_t size)
{
	hawk_uint_t t, rem;
	hawk_oow_t len, ret, i;
	hawk_oow_t prefix_len;

	prefix_len = (prefix != HAWK_NULL)? hawk_count_ucstr(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == HAWK_NULL) 
		{
			/* if buf is not given, 
			 * return the number of bytes required */
			return prefix_len + 1;
		}

		if (size < prefix_len+1) 
		{
			/* buffer too small */
			return (hawk_oow_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = '0';
		if (size > prefix_len+1) buf[prefix_len+1] = '\0';
		return prefix_len+1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == HAWK_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (hawk_oow_t)-1; /* buffer too small */
	if (size > len) buf[len] = '\0';
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (hawk_uch_t)rem + 'a' - 10;
		else
			buf[--len] = (hawk_uch_t)rem + '0';
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = '-';
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}

hawk_oow_t hawk_uint_to_bcstr (hawk_uint_t value, int radix, const hawk_bch_t* prefix, hawk_bch_t* buf, hawk_oow_t size)
{
	hawk_uint_t t, rem;
	hawk_oow_t len, ret, i;
	hawk_oow_t prefix_len;

	prefix_len = (prefix != HAWK_NULL)? hawk_count_bcstr(prefix): 0;

	t = value;
	if (t == 0)
	{
		/* zero */
		if (buf == HAWK_NULL) 
		{
			/* if buf is not given, 
			 * return the number of bytes required */
			return prefix_len + 1;
		}

		if (size < prefix_len+1) 
		{
			/* buffer too small */
			return (hawk_oow_t)-1;
		}

		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
		buf[prefix_len] = '0';
		if (size > prefix_len+1) buf[prefix_len+1] = '\0';
		return prefix_len+1;
	}

	/* non-zero values */
	len = prefix_len;
	if (t < 0) { t = -t; len++; }
	while (t > 0) { len++; t /= radix; }

	if (buf == HAWK_NULL)
	{
		/* if buf is not given, return the number of bytes required */
		return len;
	}

	if (size < len) return (hawk_oow_t)-1; /* buffer too small */
	if (size > len) buf[len] = '\0';
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (hawk_bch_t)rem + 'a' - 10;
		else
			buf[--len] = (hawk_bch_t)rem + '0';
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = '-';
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}

hawk_int_t hawk_uchars_to_int (const hawk_uch_t* str, hawk_oow_t len, int option, const hawk_uch_t** endptr, int* is_sober)
{
	hawk_int_t n = 0;
	const hawk_uch_t* p, * pp;
	const hawk_uch_t* end;
	hawk_oow_t rem;
	int digit, negative = 0;
	int base = HAWK_UCHARS_TO_INTMAX_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (HAWK_UCHARS_TO_INTMAX_GET_OPTION_LTRIM(option))
	{
		/* strip off leading spaces */
		while (p < end && hawk_is_uch_space(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == '-') 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == '+') p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == '0') 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == 'x' || *p == 'X')
			{
				p++; base = 16;
			} 
			else if (*p == 'b' || *p == 'B')
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == '0' && (*(p + 1) == 'b' || *(p + 1) == 'B')) p += 2; 
	}

	/* process the digits */
	pp = p;
	while (p < end)
	{
		digit = HAWK_ZDIGIT_TO_NUM(*p, base);
		if (digit >= base) break;
		n = n * base + digit;
		p++;
	}

	if (HAWK_UCHARS_TO_INTMAX_GET_OPTION_E(option))
	{
		if (*p == 'E' || *p == 'e')
		{
			hawk_int_t e = 0, i;
			int e_neg = 0;
			p++;
			if (*p == '+')
			{
				p++;
			}
			else if (*p == '-')
			{
				p++; e_neg = 1;
			}
			while (p < end)
			{
				digit = HAWK_ZDIGIT_TO_NUM(*p, base);
				if (digit >= base) break;
				e = e * base + digit;
				p++;
			}
			if (e_neg)
				for (i = 0; i < e; i++) n /= 10;
			else
				for (i = 0; i < e; i++) n *= 10;
		}
	}

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (HAWK_UCHARS_TO_INTMAX_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_uch_space(*p)) p++;
	}

	if (endptr) *endptr = p;
	return (negative)? -n: n;
}

hawk_int_t hawk_bchars_to_int (const hawk_bch_t* str, hawk_oow_t len, int option, const hawk_bch_t** endptr, int* is_sober)
{
	hawk_int_t n = 0;
	const hawk_bch_t* p, * pp;
	const hawk_bch_t* end;
	hawk_oow_t rem;
	int digit, negative = 0;
	int base = HAWK_BCHARS_TO_INTMAX_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (HAWK_BCHARS_TO_INTMAX_GET_OPTION_LTRIM(option))
	{
		/* strip off leading spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == '-') 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == '+') p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == '0') 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == 'x' || *p == 'X')
			{
				p++; base = 16;
			} 
			else if (*p == 'b' || *p == 'B')
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == '0' && (*(p + 1) == 'b' || *(p + 1) == 'B')) p += 2; 
	}

	/* process the digits */
	pp = p;
	while (p < end)
	{
		digit = HAWK_ZDIGIT_TO_NUM(*p, base);
		if (digit >= base) break;
		n = n * base + digit;
		p++;
	}

	if (HAWK_BCHARS_TO_INTMAX_GET_OPTION_E(option))
	{
		if (*p == 'E' || *p == 'e')
		{
			hawk_int_t e = 0, i;
			int e_neg = 0;
			p++;
			if (*p == '+')
			{
				p++;
			}
			else if (*p == '-')
			{
				p++; e_neg = 1;
			}
			while (p < end)
			{
				digit = HAWK_ZDIGIT_TO_NUM(*p, base);
				if (digit >= base) break;
				e = e * base + digit;
				p++;
			}
			if (e_neg)
				for (i = 0; i < e; i++) n /= 10;
			else
				for (i = 0; i < e; i++) n *= 10;
		}
	}

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (HAWK_BCHARS_TO_INTMAX_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	if (endptr) *endptr = p;
	return (negative)? -n: n;
}

hawk_uint_t hawk_uchars_to_uint (const hawk_uch_t* str, hawk_oow_t len, int option, const hawk_uch_t** endptr, int* is_sober)
{
	hawk_uint_t n = 0;
	const hawk_uch_t* p, * pp;
	const hawk_uch_t* end;
	hawk_oow_t rem;
	int digit;
	int base = HAWK_UCHARS_TO_UINTMAX_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (HAWK_UCHARS_TO_UINTMAX_GET_OPTION_LTRIM(option))
	{
		/* strip off leading spaces */
		while (p < end && hawk_is_uch_space(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == '+') p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == '0') 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == 'x' || *p == 'X')
			{
				p++; base = 16;
			} 
			else if (*p == 'b' || *p == 'B')
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == '0' && (*(p + 1) == 'b' || *(p + 1) == 'B')) p += 2; 
	}

	/* process the digits */
	pp = p;
	while (p < end)
	{
		digit = HAWK_ZDIGIT_TO_NUM(*p, base);
		if (digit >= base) break;
		n = n * base + digit;
		p++;
	}

	if (HAWK_UCHARS_TO_UINTMAX_GET_OPTION_E(option))
	{
		if (*p == 'E' || *p == 'e')
		{
			hawk_uint_t e = 0, i;
			int e_neg = 0;
			p++;
			if (*p == '+')
			{
				p++;
			}
			else if (*p == '-')
			{
				p++; e_neg = 1;
			}
			while (p < end)
			{
				digit = HAWK_ZDIGIT_TO_NUM(*p, base);
				if (digit >= base) break;
				e = e * base + digit;
				p++;
			}
			if (e_neg)
				for (i = 0; i < e; i++) n /= 10;
			else
				for (i = 0; i < e; i++) n *= 10;
		}
	}

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (HAWK_UCHARS_TO_UINTMAX_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_uch_space(*p)) p++;
	}

	if (endptr) *endptr = p;
	return n;
}

hawk_uint_t hawk_bchars_to_uint (const hawk_bch_t* str, hawk_oow_t len, int option, const hawk_bch_t** endptr, int* is_sober)
{
	hawk_uint_t n = 0;
	const hawk_bch_t* p, * pp;
	const hawk_bch_t* end;
	hawk_oow_t rem;
	int digit;
	int base = HAWK_BCHARS_TO_UINTMAX_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (HAWK_BCHARS_TO_UINTMAX_GET_OPTION_LTRIM(option))
	{
		/* strip off leading spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == '+') p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == '0') 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == 'x' || *p == 'X')
			{
				p++; base = 16;
			} 
			else if (*p == 'b' || *p == 'B')
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == '0' && (*(p + 1) == 'b' || *(p + 1) == 'B')) p += 2; 
	}

	/* process the digits */
	pp = p;
	while (p < end)
	{
		digit = HAWK_ZDIGIT_TO_NUM(*p, base);
		if (digit >= base) break;
		n = n * base + digit;
		p++;
	}

	if (HAWK_BCHARS_TO_UINTMAX_GET_OPTION_E(option))
	{
		if (*p == 'E' || *p == 'e')
		{
			hawk_uint_t e = 0, i;
			int e_neg = 0;
			p++;
			if (*p == '+')
			{
				p++;
			}
			else if (*p == '-')
			{
				p++; e_neg = 1;
			}
			while (p < end)
			{
				digit = HAWK_ZDIGIT_TO_NUM(*p, base);
				if (digit >= base) break;
				e = e * base + digit;
				p++;
			}
			if (e_neg)
				for (i = 0; i < e; i++) n /= 10;
			else
				for (i = 0; i < e; i++) n *= 10;
		}
	}

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (HAWK_BCHARS_TO_UINTMAX_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	if (endptr) *endptr = p;
	return n;
}

