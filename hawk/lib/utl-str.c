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

#include <hawk-utl.h>
#include <hawk-chr.h>

/* some naming conventions
 *  bchars, uchars -> pointer and length
 *  bcstr, ucstr -> null-terminated string pointer
 *  btouchars -> bchars to uchars
 *  utobchars -> uchars to bchars
 *  btoucstr -> bcstr to ucstr
 *  utobcstr -> ucstr to bcstr
 */

int hawk_equal_uchars (const hawk_uch_t* str1, const hawk_uch_t* str2, hawk_oow_t len)
{
	hawk_oow_t i;

	/* [NOTE] you should call this function after having ensured that
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

	/* [NOTE] you should call this function after having ensured that
	 *        str1 and str2 are in the same length */

	for (i = 0; i < len; i++)
	{
		if (str1[i] != str2[i]) return 0;
	}

	return 1;
}

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
	if  (ignorecase)
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

int hawk_comp_ucstr_limited (const hawk_uch_t* str1, const hawk_uch_t* str2, hawk_oow_t maxlen)
{
	if (maxlen == 0) return 0;

	while (*str1 == *str2)
	{
		 if (*str1 == '\0' || maxlen == 1) return 0;
		 str1++; str2++; maxlen--;
	}

	return ((hawk_uchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
}

int hawk_comp_bcstr_limited (const hawk_bch_t* str1, const hawk_bch_t* str2, hawk_oow_t maxlen)
{
	if (maxlen == 0) return 0;

	while (*str1 == *str2)
	{
		 if (*str1 == '\0' || maxlen == 1) return 0;
		 str1++; str2++; maxlen--;
	}

	return ((hawk_bchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
}

int hawk_comp_ucstr_bcstr (const hawk_uch_t* str1, const hawk_bch_t* str2)
{
	while (*str1 == *str2)
	{
		if (*str1 == '\0') return 0;
		str1++; str2++;
	}

	return ((hawk_uchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
}

int hawk_comp_uchars_ucstr (const hawk_uch_t* str1, hawk_oow_t len, const hawk_uch_t* str2)
{
	/* for "abc\0" of length 4 vs "abc", the fourth character
	 * of the first string is equal to the terminating null of
	 * the second string. the first string is still considered 
	 * bigger */
	const hawk_uch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hawk_uchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

int hawk_comp_uchars_bcstr (const hawk_uch_t* str1, hawk_oow_t len, const hawk_bch_t* str2)
{
	const hawk_uch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hawk_uchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

int hawk_comp_bchars_bcstr (const hawk_bch_t* str1, hawk_oow_t len, const hawk_bch_t* str2)
{
	const hawk_bch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hawk_bchu_t)*str1 > (hawk_bchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

int hawk_comp_bchars_ucstr (const hawk_bch_t* str1, hawk_oow_t len, const hawk_uch_t* str2)
{
	const hawk_bch_t* end = str1 + len;
	while (str1 < end && *str2 != '\0') 
	{
		if (*str1 != *str2) return ((hawk_bchu_t)*str1 > (hawk_uchu_t)*str2)? 1: -1;
		str1++; str2++;
	}
	return (str1 < end)? 1: (*str2 == '\0'? 0: -1);
}

/* ----------------------------------------------------------------------- */

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

hawk_oow_t hawk_copy_uchars_to_ucstr (hawk_uch_t* dst, hawk_uch_t dlen, const hawk_uch_t* src, hawk_oow_t slen)
{
	hawk_oow_t i;
	if (dlen <= 0) return 0;
	if (dlen <= slen) slen = dlen - 1;
	for (i = 0; i < slen; i++) dst[i] = src[i];
	dst[i] = '\0';
	return i;
}

hawk_oow_t hawk_copy_bchars_to_bcstr (hawk_bch_t* dst, hawk_bch_t dlen, const hawk_bch_t* src, hawk_oow_t slen)
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

/* ----------------------------------------------------------------------- */

hawk_oow_t hawk_count_ucstr (const hawk_uch_t* str)
{
	const hawk_uch_t* ptr = str;
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

hawk_oow_t hawk_count_bcstr (const hawk_bch_t* str)
{
	const hawk_bch_t* ptr = str;
	while (*ptr != '\0') ptr++;
	return ptr - str;
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

/* ----------------------------------------------------------------------- */

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


/* ----------------------------------------------------------------------- */

hawk_uch_t* hawk_find_uchar (const hawk_uch_t* ptr, hawk_oow_t len, hawk_uch_t c)
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

hawk_bch_t* hawk_find_bchar (const hawk_bch_t* ptr, hawk_oow_t len, hawk_bch_t c)
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

hawk_uch_t* hawk_rfind_uchar (const hawk_uch_t* ptr, hawk_oow_t len, hawk_uch_t c)
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

hawk_bch_t* hawk_rfind_bchar (const hawk_bch_t* ptr, hawk_oow_t len, hawk_bch_t c)
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
/* ----------------------------------------------------------------------- */

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

/* ----------------------------------------------------------------------- */

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

/* ----------------------------------------------------------------------- */

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

/* ----------------------------------------------------------------------- */

hawk_uch_t* hawk_trim_uchars (hawk_uch_t* str, hawk_oow_t* len, int flags)
{
	hawk_uch_t* p = str, * end = str + *len;

	if (p < end)
	{
		hawk_uch_t* s = HAWK_NULL, * e = HAWK_NULL;

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

	return str;
}

hawk_bch_t* hawk_trim_bchars (hawk_bch_t* str, hawk_oow_t* len, int flags)
{
	hawk_bch_t* p = str, * end = str + *len;

	if (p < end)
	{
		hawk_bch_t* s = HAWK_NULL, * e = HAWK_NULL;

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

	return str;
}
/* ----------------------------------------------------------------------- */

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
		for (d = (hawk_uch_t*)delim; *d != HAWK_UT('\0'); d++)
			if (!hawk_is_uch_space(*d)) delim_mode = 2;
	}

	if (delim_mode == 0) 
	{
		/* skip preceding space characters */
		while (hawk_is_uch_space(*p)) p++;

		/* when 0 is given as "delim", it has an effect of cutting
		   preceding and trailing space characters off "s". */
		if (lquote != HAWK_UT('\0') && *p == lquote) 
		{
			hawk_copy_ucstr_unlimited (p, p + 1);

			for (;;) 
			{
				if (*p == HAWK_UT('\0')) return -1;

				if (escape != HAWK_UT('\0') && *p == escape) 
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
			if (*p != HAWK_UT('\0')) return -1;

			if (sp == 0 && ep == 0) s[0] = HAWK_UT('\0');
			else 
			{
				ep[1] = HAWK_UT('\0');
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

			if (sp == 0 && ep == 0) s[0] = HAWK_UT('\0');
			else 
			{
				ep[1] = HAWK_UT('\0');
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

			if (lquote != HAWK_UT('\0') && *p == lquote) 
			{
				hawk_copy_ucstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == HAWK_UT('\0')) return -1;

					if (escape != HAWK_UT('\0') && *p == escape) 
					{
						hawk_copy_ucstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = HAWK_UT('\0');
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
					if (*p == HAWK_UT('\0')) 
					{
						if (o != p) cnt++;
						break;
					}
					if (hawk_is_uch_space (*p)) 
					{
						*p++ = HAWK_UT('\0');
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

		while (*p != HAWK_UT('\0')) 
		{
			o = p;
			while (hawk_is_uch_space(*p)) p++;
			if (o != p) { hawk_copy_ucstr_unlimited (o, p); p = o; }

			if (lquote != HAWK_UT('\0') && *p == lquote) 
			{
				hawk_copy_ucstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == HAWK_UT('\0')) return -1;

					if (escape != HAWK_UT('\0') && *p == escape) 
					{
						hawk_copy_ucstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = HAWK_UT('\0');
							cnt++;
							break;
						}
					}
					p++;
				}

				ok = 0;
				while (hawk_is_uch_space(*p)) p++;
				if (*p == HAWK_UT('\0')) ok = 1;
				for (d = (hawk_uch_t*)delim; *d != HAWK_UT('\0'); d++) 
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
					if (*p == HAWK_UT('\0')) 
					{
						if (ep) 
						{
							ep[1] = HAWK_UT('\0');
							p = &ep[1];
						}
						cnt++;
						break;
					}
					for (d = (hawk_uch_t*)delim; *d != HAWK_UT('\0'); d++) 
					{
						if (*p == *d)  
						{
							if (sp == HAWK_NULL) 
							{
								hawk_copy_ucstr_unlimited (o, p); p = o;
								*p++ = HAWK_UT('\0');
							}
							else 
							{
								hawk_copy_ucstr_unlimited (&ep[1], p);
								hawk_copy_ucstr_unlimited (o, sp);
								o[ep - sp + 1] = HAWK_UT('\0');
								p = &o[ep - sp + 2];
							}
							cnt++;
							/* last empty field after delim */
							if (*p == HAWK_UT('\0')) cnt++;
							goto exit_point;
						}
					}

					if (!hawk_is_uch_space (*p)) 
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
		for (d = (hawk_bch_t*)delim; *d != HAWK_BT('\0'); d++)
			if (!hawk_is_bch_space(*d)) delim_mode = 2;
	}

	if (delim_mode == 0) 
	{
		/* skip preceding space characters */
		while (hawk_is_bch_space(*p)) p++;

		/* when 0 is given as "delim", it has an effect of cutting
		   preceding and trailing space characters off "s". */
		if (lquote != HAWK_BT('\0') && *p == lquote) 
		{
			hawk_copy_bcstr_unlimited (p, p + 1);

			for (;;) 
			{
				if (*p == HAWK_BT('\0')) return -1;

				if (escape != HAWK_BT('\0') && *p == escape) 
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
			if (*p != HAWK_BT('\0')) return -1;

			if (sp == 0 && ep == 0) s[0] = HAWK_BT('\0');
			else 
			{
				ep[1] = HAWK_BT('\0');
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

			if (sp == 0 && ep == 0) s[0] = HAWK_BT('\0');
			else 
			{
				ep[1] = HAWK_BT('\0');
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

			if (lquote != HAWK_BT('\0') && *p == lquote) 
			{
				hawk_copy_bcstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == HAWK_BT('\0')) return -1;

					if (escape != HAWK_BT('\0') && *p == escape) 
					{
						hawk_copy_bcstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = HAWK_BT('\0');
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
					if (*p == HAWK_BT('\0')) 
					{
						if (o != p) cnt++;
						break;
					}
					if (hawk_is_bch_space (*p)) 
					{
						*p++ = HAWK_BT('\0');
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

		while (*p != HAWK_BT('\0')) 
		{
			o = p;
			while (hawk_is_bch_space(*p)) p++;
			if (o != p) { hawk_copy_bcstr_unlimited (o, p); p = o; }

			if (lquote != HAWK_BT('\0') && *p == lquote) 
			{
				hawk_copy_bcstr_unlimited (p, p + 1);

				for (;;) 
				{
					if (*p == HAWK_BT('\0')) return -1;

					if (escape != HAWK_BT('\0') && *p == escape) 
					{
						hawk_copy_bcstr_unlimited (p, p + 1);
					}
					else 
					{
						if (*p == rquote) 
						{
							*p++ = HAWK_BT('\0');
							cnt++;
							break;
						}
					}
					p++;
				}

				ok = 0;
				while (hawk_is_bch_space(*p)) p++;
				if (*p == HAWK_BT('\0')) ok = 1;
				for (d = (hawk_bch_t*)delim; *d != HAWK_BT('\0'); d++) 
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
					if (*p == HAWK_BT('\0')) 
					{
						if (ep) 
						{
							ep[1] = HAWK_BT('\0');
							p = &ep[1];
						}
						cnt++;
						break;
					}
					for (d = (hawk_bch_t*)delim; *d != HAWK_BT('\0'); d++) 
					{
						if (*p == *d)  
						{
							if (sp == HAWK_NULL) 
							{
								hawk_copy_bcstr_unlimited (o, p); p = o;
								*p++ = HAWK_BT('\0');
							}
							else 
							{
								hawk_copy_bcstr_unlimited (&ep[1], p);
								hawk_copy_bcstr_unlimited (o, sp);
								o[ep - sp + 1] = HAWK_BT('\0');
								p = &o[ep - sp + 2];
							}
							cnt++;
							/* last empty field after delim */
							if (*p == HAWK_BT('\0')) cnt++;
							goto exit_point;
						}
					}

					if (!hawk_is_bch_space (*p)) 
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

/* ----------------------------------------------------------------------- */

hawk_oow_t hawk_int_to_oocstr (hawk_int_t value, int radix, const hawk_ooch_t* prefix, hawk_ooch_t* buf, hawk_oow_t size)
{
	hawk_int_t t, rem;
	hawk_oow_t len, ret, i;
	hawk_oow_t prefix_len;

	prefix_len = (prefix != HAWK_NULL)? hawk_count_oocstr(prefix): 0;

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
		buf[prefix_len] = HAWK_T('0');
		if (size > prefix_len+1) buf[prefix_len+1] = HAWK_T('\0');
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
	if (size > len) buf[len] = HAWK_T('\0');
	ret = len;

	t = value;
	if (t < 0) t = -t;

	while (t > 0) 
	{
		rem = t % radix;
		if (rem >= 10)
			buf[--len] = (hawk_ooch_t)rem + HAWK_T('a') - 10;
		else
			buf[--len] = (hawk_ooch_t)rem + HAWK_T('0');
		t /= radix;
	}

	if (value < 0) 
	{
		for (i = 1; i <= prefix_len; i++) 
		{
			buf[i] = prefix[i-1];
			len--;
		}
		buf[--len] = HAWK_T('-');
	}
	else
	{
		for (i = 0; i < prefix_len; i++) buf[i] = prefix[i];
	}

	return ret;
}

/* ----------------------------------------------------------------------- */

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

int hawk_conv_uchars_to_bchars_with_cmgr (
	const hawk_uch_t* ucs, hawk_oow_t* ucslen,
	hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_cmgr_t* cmgr)
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

static hawk_cmgr_t builtin_cmgr[] =
{
	/* keep the order aligned with hawk_cmgr_id_t values in <hawk-utl.h> */
	{ hawk_utf8_to_uc,  hawk_uc_to_utf8 },
	{ hawk_utf16_to_uc, hawk_uc_to_utf16 },
	{ hawk_mb8_to_uc,   hawk_uc_to_mb8 }
};

hawk_cmgr_t* hawk_get_cmgr_by_id (hawk_cmgr_id_t id)
{
	return &builtin_cmgr[id];
}


static struct
{
	const hawk_bch_t* name;
	hawk_cmgr_id_t     id;
} builtin_cmgr_tab[] =
{
	{ "utf8",    HAWK_CMGR_UTF8 },
	{ "utf16",   HAWK_CMGR_UTF16 },
	{ "mb8",     HAWK_CMGR_MB8 }
};

hawk_cmgr_t* hawk_get_cmgr_by_bcstr (const hawk_bch_t* name)
{
	if (name)
	{
		hawk_oow_t i;

		for (i = 0; i < HAWK_COUNTOF(builtin_cmgr_tab); i++)
		{
			if (hawk_comp_bcstr(name, builtin_cmgr_tab[i].name, 0) == 0)
			{
				return &builtin_cmgr[builtin_cmgr_tab[i].id];
			}
		 }
	}

	return HAWK_NULL;
}

hawk_cmgr_t* hawk_get_cmgr_by_ucstr (const hawk_uch_t* name)
{
	if (name)
	{
		hawk_oow_t i;

		for (i = 0; i < HAWK_COUNTOF(builtin_cmgr_tab); i++)
		{
			if (hawk_comp_ucstr_bcstr(name, builtin_cmgr_tab[i].name) == 0)
			{
				return &builtin_cmgr[builtin_cmgr_tab[i].id];
			}
		 }
	}

	return HAWK_NULL;
}

/* ----------------------------------------------------------------------- */

int hawk_conv_utf8_to_uchars (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* the source is length bound */
	return hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF8], 0);
}

int hawk_conv_uchars_to_utf8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	return hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF8]);
}

int hawk_conv_utf8_to_ucstr (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* null-terminated. */
	return hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF8], 0);
}

int hawk_conv_ucstr_to_utf8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	return hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF8]);
}

/* ----------------------------------------------------------------------- */

int hawk_conv_utf16_to_uchars (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* the source is length bound */
	return hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF16], 0);
}

int hawk_conv_uchars_to_utf16 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	return hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF16]);
}

int hawk_conv_utf16_to_ucstr (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* null-terminated. */
	return hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF16], 0);
}

int hawk_conv_ucstr_to_utf16 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	return hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF16]);
}

/* ----------------------------------------------------------------------- */

int hawk_conv_mb8_to_uchars (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* the source is length bound */
	return hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_MB8], 0);
}

int hawk_conv_uchars_to_mb8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	return hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_MB8]);
}

int hawk_conv_mb8_to_ucstr (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* null-terminated. */
	return hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_MB8], 0);
}

int hawk_conv_ucstr_to_mb8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	return hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_MB8]);
}

/* ----------------------------------------------------------------------- */
