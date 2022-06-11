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
		while (hawk_to_uch_lower(*str1) == hawk_to_uch_lower(*str2))
		{
			 if (*str1 == '\0' || maxlen == 1) return 0;
			 str1++; str2++; maxlen--;
		}

		return ((hawk_bchu_t)hawk_to_uch_lower(*str1) > (hawk_bchu_t)hawk_to_uch_lower(*str2))? 1: -1;
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

int hawk_comp_uchars_bcstr (const hawk_uch_t* str1, hawk_oow_t len, const hawk_bch_t* str2, int ignorecase)
{
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

int hawk_comp_bchars_bcstr (const hawk_bch_t* str1, hawk_oow_t len, const hawk_bch_t* str2, int ignorecase)
{
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

/* ------------------------------------------------------------------------ */

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

hawk_oow_t hawk_copy_ucstr_to_uchars (hawk_uch_t* dst, hawk_oow_t dlen, const hawk_uch_t* src)
{
	/* no null termination */
	hawk_uch_t* p, * p2;

	p = dst; p2 = dst + dlen;

	while (p < p2)
	{
		if (*src == '\0') break;
		*p++ = *src++;
	}

	return p - dst;
}

hawk_oow_t hawk_copy_bcstr_to_bchars (hawk_bch_t* dst, hawk_oow_t dlen, const hawk_bch_t* src)
{
	/* no null termination */
	hawk_bch_t* p, * p2;

	p = dst; p2 = dst + dlen;

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


hawk_oow_t hawk_copy_ucstrs_to_uchars (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, const hawk_uch_t* str[])
{
	hawk_uch_t* b = buf;
	hawk_uch_t* end = buf + bsz - 1;
	const hawk_uch_t* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != HAWK_UT('\0'))
	{
		if (*f == HAWK_UT('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != HAWK_UT('\0')) f++;
		}
		else if (*f == HAWK_UT('$'))
		{
			if (f[1] == HAWK_UT('{') && 
			    (f[2] >= HAWK_UT('0') && f[2] <= HAWK_UT('9')))
			{
				const hawk_uch_t* tmp;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - HAWK_UT('0'));
				while (*f >= HAWK_UT('0') && *f <= HAWK_UT('9'));
	
				if (*f != HAWK_UT('}'))
				{
					f = tmp;
					goto normal;
				}
 
				f++;
				
				tmp = str[idx];
				while (*tmp != HAWK_UT('\0')) 
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == HAWK_UT('$')) f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = HAWK_UT('\0');
	return b - buf;
}

hawk_oow_t hawk_copy_bcstrs_to_bchars (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, const hawk_bch_t* str[])
{
	hawk_bch_t* b = buf;
	hawk_bch_t* end = buf + bsz - 1;
	const hawk_bch_t* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != HAWK_BT('\0'))
	{
		if (*f == HAWK_BT('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != HAWK_BT('\0')) f++;
		}
		else if (*f == HAWK_BT('$'))
		{
			if (f[1] == HAWK_BT('{') && 
			    (f[2] >= HAWK_BT('0') && f[2] <= HAWK_BT('9')))
			{
				const hawk_bch_t* tmp;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - HAWK_BT('0'));
				while (*f >= HAWK_BT('0') && *f <= HAWK_BT('9'));
	
				if (*f != HAWK_BT('}'))
				{
					f = tmp;
					goto normal;
				}
 
				f++;
				
				tmp = str[idx];
				while (*tmp != HAWK_BT('\0')) 
				{
					if (b >= end) goto fini;
					*b++ = *tmp++;
				}
				continue;
			}
			else if (f[1] == HAWK_BT('$')) f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = HAWK_BT('\0');
	return b - buf;
}



hawk_oow_t hawk_copy_ufcs_to_uchars (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, const hawk_ucs_t str[])
{
	hawk_uch_t* b = buf;
	hawk_uch_t* end = buf + bsz - 1;
	const hawk_uch_t* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != HAWK_UT('\0'))
	{
		if (*f == HAWK_UT('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != HAWK_UT('\0')) f++;
		}
		else if (*f == HAWK_UT('$'))
		{
			if (f[1] == HAWK_UT('{') && 
			    (f[2] >= HAWK_UT('0') && f[2] <= HAWK_UT('9')))
			{
				const hawk_uch_t* tmp, * tmpend;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - HAWK_UT('0'));
				while (*f >= HAWK_UT('0') && *f <= HAWK_UT('9'));
	
				if (*f != HAWK_UT('}'))
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
			else if (f[1] == HAWK_UT('$')) f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = HAWK_UT('\0');
	return b - buf;
}

hawk_oow_t hawk_copy_bfcs_to_bchars (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, const hawk_bcs_t str[])
{
	hawk_bch_t* b = buf;
	hawk_bch_t* end = buf + bsz - 1;
	const hawk_bch_t* f = fmt;
 
	if (bsz <= 0) return 0;
 
	while (*f != HAWK_BT('\0'))
	{
		if (*f == HAWK_BT('\\'))
		{
			/* get the escaped character and treat it normally.
			 * if the escaper is the last character, treat it 
			 * normally also. */
			if (f[1] != HAWK_BT('\0')) f++;
		}
		else if (*f == HAWK_BT('$'))
		{
			if (f[1] == HAWK_BT('{') && 
			    (f[2] >= HAWK_BT('0') && f[2] <= HAWK_BT('9')))
			{
				const hawk_bch_t* tmp, * tmpend;
				hawk_oow_t idx = 0;
 
				tmp = f;
				f += 2;
 
				do idx = idx * 10 + (*f++ - HAWK_BT('0'));
				while (*f >= HAWK_BT('0') && *f <= HAWK_BT('9'));
	
				if (*f != HAWK_BT('}'))
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
			else if (f[1] == HAWK_BT('$')) f++;
		}
 
	normal:
		if (b >= end) break;
		*b++ = *f++;
	}
 
fini:
	*b = HAWK_BT('\0');
	return b - buf;
}


/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */

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


/* ------------------------------------------------------------------------ */

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

hawk_uch_t* hawk_rfind_uchar_in_ucstr (const hawk_uch_t* ptr, hawk_uch_t c)
{
	return hawk_rfind_uchar_in_uchars(ptr, hawk_count_ucstr(ptr), c);
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

hawk_bch_t* hawk_rfind_bchar_in_bcstr (const hawk_bch_t* ptr, hawk_bch_t c)
{
	return hawk_rfind_bchar_in_bchars(ptr, hawk_count_bcstr(ptr), c);
}

/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */

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
/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */
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
	if (delim_mode == __DELIM_EMPTY || 
	    delim_mode == __DELIM_SPACES) return (hawk_uch_t*)p;
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
	if (delim_mode == __DELIM_EMPTY || 
	    delim_mode == __DELIM_SPACES) return (hawk_bch_t*)p;
	return (hawk_bch_t*)++p;
}

/* ------------------------------------------------------------------------ */

void hawk_unescape_ucstr (hawk_uch_t* str)
{
	hawk_uch_t c, * p1, * p2;
	hawk_uint32_t c_acc;
	int escaped = 0, digit_count;

	p1 = str;
	p2 = str;
	while ((c = *p1++) != '\0')
	{
		if (escaped == 3)
		{
			/* octal */
			if (c >= '0' && c <= '7')
			{
				c_acc = c_acc * 8 + c - '0';
				digit_count++;
				
				if (digit_count >= escaped) 
				{
					/* should i limit the max to 0xFF/0377? 
					 if (c_acc > 0377) c_acc = 0377; */
					escaped = 0;
					*p2++ = c_acc;
				}
				continue;
			}
			else
			{
				escaped = 0;
				*p2++ = c_acc;
				goto normal_char;
			}
		}
		else if (escaped == 2 || escaped == 4 || escaped == 8)
		{
			/* hexadecimal */
			if (c >= '0' && c <= '9')
			{
				c_acc = c_acc * 16 + c - '0';
				digit_count++;
				if (digit_count >= escaped) 
				{
					*p2++ = c_acc;
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'A' && c <= 'F')
			{
				c_acc = c_acc * 16 + c - 'A' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					*p2++ = c_acc;
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'a' && c <= 'f')
			{
				c_acc = c_acc * 16 + c - 'a' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					*p2++ = c_acc;
					escaped = 0;
				}
				continue;
			}
			else
			{
				if (digit_count == 0) 
				{
					/* no valid character after the escaper.
					 * keep the escaper as it is. consider this input:
					 *   \xGG
					 * 'c' is at the first G. this part is to restore the
					 * \x part. since \x is not followed by any hexadecimal
					 * digits, it's literally 'x' */
					*p2++ = (escaped == 2)? 'x':
				             (escaped == 4)? 'u': 'U';
				}
				else *p2++ = c_acc;

				escaped = 0;
				goto normal_char;
			}
		}

		if (escaped == 1)
		{
			switch (c) 
			{
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'f': c = '\f'; break;
				case 'b': c = '\b'; break;
				case 'v': c = '\v'; break;
				case 'a': c = '\a'; break;

				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					escaped = 3;
					digit_count = 1;
					c_acc = c - '0';
					continue;

				case 'x':
					escaped = 2;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'u':
					escaped = 4;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'U':
					escaped = 8;
					digit_count = 0;
					c_acc = 0;
					continue;
				}

			*p2++ = c;
			escaped = 0;
			continue;
		}

	normal_char:
		if (c == '\\') 
		{
			escaped = 1;
			continue;
		}

		*p2++ = c;
	}

	*p2 = '\0';
}

/* ------------------------------------------------------------------------ */

void hawk_unescape_bcstr (hawk_bch_t* str)
{
	hawk_bch_t c, * p1, * p2;
	hawk_uint32_t c_acc;
	int escaped = 0, digit_count;
	hawk_cmgr_t* utf8_cmgr;

	utf8_cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	p1 = str;
	p2 = str;
	while ((c = *p1++) != '\0')
	{
		if (escaped == 3)
		{
			/* octal */
			if (c >= '0' && c <= '7')
			{
				c_acc = c_acc * 8 + c - '0';
				digit_count++;
				
				if (digit_count >= escaped) 
				{
					/* should i limit the max to 0xFF/0377? 
					 if (c_acc > 0377) c_acc = 0377; */
					escaped = 0;
					*p2++ = c_acc;
				}
				continue;
			}
			else
			{
				escaped = 0;
				*p2++ = c_acc;
				goto normal_char;
			}
		}
		else if (escaped == 2 || escaped == 4 || escaped == 8)
		{
			/* hexadecimal */
			if (c >= '0' && c <= '9')
			{
				c_acc = c_acc * 16 + c - '0';
				digit_count++;
				if (digit_count >= escaped) 
				{
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'A' && c <= 'F')
			{
				c_acc = c_acc * 16 + c - 'A' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'a' && c <= 'f')
			{
				c_acc = c_acc * 16 + c - 'a' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
					escaped = 0;
				}
				continue;
			}
			else
			{
				/* non digit or xdigit */
				
				if (digit_count == 0) 
				{
					/* no valid character after the escaper.
					 * keep the escaper as it is. consider this input:
					 *   \xGG
					 * 'c' is at the first G. this part is to restore the
					 * \x part. since \x is not followed by any hexadecimal
					 * digits, it's literally 'x' */
					*p2++ = (escaped == 2)? 'x':
					        (escaped == 4)? 'u': 'U';
				}
				else 
				{
					/* for a unicode, the utf8 conversion can never outgrow the input string of the hexadecimal notation with an escaper.
					 * so it must be safe to specify a very large buffer size to uctobc() */
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
				}

				escaped = 0;
				goto normal_char;
			}
		}

		if (escaped == 1)
		{
			switch (c) 
			{
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'f': c = '\f'; break;
				case 'b': c = '\b'; break;
				case 'v': c = '\v'; break;
				case 'a': c = '\a'; break;

				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					escaped = 3;
					digit_count = 1;
					c_acc = c - '0';
					continue;

				case 'x':
					escaped = 2;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'u':
					escaped = 4;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'U':
					escaped = 8;
					digit_count = 0;
					c_acc = 0;
					continue;
				}

			*p2++ = c;
			escaped = 0;
			continue;
		}

	normal_char:
		if (c == '\\') 
		{
			escaped = 1;
			continue;
		}

		*p2++ = c;
	}

	*p2 = '\0';
}

/* ------------------------------------------------------------------------ */

static const hawk_uch_t* scan_dollar_for_subst_u (const hawk_uch_t* f, hawk_oow_t l, hawk_ucs_t* ident, hawk_ucs_t* dfl, int depth)
{
	const hawk_uch_t* end = f + l;

	HAWK_ASSERT (l >= 2);
	
	f += 2; /* skip ${ */ 
	if (ident) ident->ptr = (hawk_uch_t*)f;

	while (1)
	{
		if (f >= end) return HAWK_NULL;
		if (*f == '}' || *f == ':') break;
		f++;
	}

	if (*f == ':')
	{
		if (f >= end || *(f + 1) != '=') 
		{
			/* not := */
			return HAWK_NULL; 
		}

		if (ident) ident->len = f - ident->ptr;

		f += 2; /* skip := */

		if (dfl) dfl->ptr = (hawk_uch_t*)f;
		while (1)
		{
			if (f >= end) return HAWK_NULL;

			else if (*f == '$' && *(f + 1) == '{')
			{
				if (depth >= 64) return HAWK_NULL; /* depth too deep */

				/* TODO: remove recursion */
				f = scan_dollar_for_subst_u(f, end - f, HAWK_NULL, HAWK_NULL, depth + 1);
				if (!f) return HAWK_NULL;
			}
			else if (*f == '}') 
			{
				/* ending bracket  */
				if (dfl) dfl->len = f - dfl->ptr;
				return f + 1;
			}
			else f++;
		}
	}
	else if (*f == '}') 
	{
		if (ident) ident->len = f - ident->ptr;
		if (dfl) 
		{
			dfl->ptr = HAWK_NULL;
			dfl->len = 0;
		}
		return f + 1;
	}

	/* this part must not be reached */
	return HAWK_NULL;
}

static hawk_uch_t* exapnd_dollar_for_subst_u (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_ucs_t* ident, const hawk_ucs_t* dfl, hawk_subst_for_ucs_t subst, void* ctx)
{
	hawk_uch_t* tmp;

	tmp = subst(buf, bsz, ident, ctx);
	if (tmp == HAWK_NULL)
	{
		/* substitution failed */
		if (dfl->len > 0)
		{
			/* take the default value */
			hawk_oow_t len;

			/* TODO: remove recursion */
			len = hawk_subst_for_uchars_to_ucstr(buf, bsz, dfl->ptr, dfl->len, subst, ctx);
			tmp = buf + len;
		}
		else tmp = buf;
	}

	return tmp;
}

hawk_oow_t hawk_subst_for_uchars_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, hawk_oow_t fsz, hawk_subst_for_ucs_t subst, void* ctx)
{
	hawk_uch_t* b = buf;
	hawk_uch_t* end = buf + bsz - 1;
	const hawk_uch_t* f = fmt;
	const hawk_uch_t* fend = fmt + fsz;

	if (buf != HAWK_SUBST_NOBUF && bsz <= 0) return 0;

	while (f < fend)
	{
		if (*f == '$' && f < fend - 1)
		{
			if (*(f + 1) == '{')
			{
				const hawk_uch_t* tmp;
				hawk_ucs_t ident, dfl;

				tmp = scan_dollar_for_subst_u(f, fend - f, &ident, &dfl, 0);
				if (!tmp || ident.len <= 0) goto normal;
				f = tmp;

				if (buf != HAWK_SUBST_NOBUF)
				{
					b = exapnd_dollar_for_subst_u(b, end - b + 1, &ident, &dfl, subst, ctx);
					if (b >= end) goto fini;
				}
				else
				{
					/* the buffer points to HAWK_SUBST_NOBUF. */
					tmp = exapnd_dollar_for_subst_u(buf, bsz, &ident, &dfl, subst, ctx);
					/* increment b by the length of the expanded string */
					b += (tmp - buf);
				}

				continue;
			}
			else if (*(f + 1) == '$') 
			{
				/* $$ -> $. */
				f++;
			}
		}

	normal:
		if (buf != HAWK_SUBST_NOBUF)
		{
			if (b >= end) break;
			*b = *f;
		}
		b++; f++;
	}

fini:
	if (buf != HAWK_SUBST_NOBUF) *b = '\0';
	return b - buf;
}

hawk_oow_t hawk_subst_for_ucstr_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, hawk_subst_for_ucs_t subst, void* ctx)
{
	return hawk_subst_for_uchars_to_ucstr(buf, bsz, fmt, hawk_count_ucstr(fmt), subst, ctx);
}

/* ------------------------------------------------------------------------ */

static const hawk_bch_t* scan_dollar_for_subst_b (const hawk_bch_t* f, hawk_oow_t l, hawk_bcs_t* ident, hawk_bcs_t* dfl, int depth)
{
	const hawk_bch_t* end = f + l;

	HAWK_ASSERT (l >= 2);
	
	f += 2; /* skip ${ */ 
	if (ident) ident->ptr = (hawk_bch_t*)f;

	while (1)
	{
		if (f >= end) return HAWK_NULL;
		if (*f == '}' || *f == ':') break;
		f++;
	}

	if (*f == ':')
	{
		if (f >= end || *(f + 1) != '=') 
		{
			/* not := */
			return HAWK_NULL; 
		}

		if (ident) ident->len = f - ident->ptr;

		f += 2; /* skip := */

		if (dfl) dfl->ptr = (hawk_bch_t*)f;
		while (1)
		{
			if (f >= end) return HAWK_NULL;

			else if (*f == '$' && *(f + 1) == '{')
			{
				if (depth >= 64) return HAWK_NULL; /* depth too deep */

				/* TODO: remove recursion */
				f = scan_dollar_for_subst_b(f, end - f, HAWK_NULL, HAWK_NULL, depth + 1);
				if (!f) return HAWK_NULL;
			}
			else if (*f == '}') 
			{
				/* ending bracket  */
				if (dfl) dfl->len = f - dfl->ptr;
				return f + 1;
			}
			else f++;
		}
	}
	else if (*f == '}') 
	{
		if (ident) ident->len = f - ident->ptr;
		if (dfl) 
		{
			dfl->ptr = HAWK_NULL;
			dfl->len = 0;
		}
		return f + 1;
	}

	/* this part must not be reached */
	return HAWK_NULL;
}

static hawk_bch_t* exapnd_dollar_for_subst_b (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bcs_t* ident, const hawk_bcs_t* dfl, hawk_subst_for_bcs_t subst, void* ctx)
{
	hawk_bch_t* tmp;

	tmp = subst(buf, bsz, ident, ctx);
	if (tmp == HAWK_NULL)
	{
		/* substitution failed */
		if (dfl->len > 0)
		{
			/* take the default value */
			hawk_oow_t len;

			/* TODO: remove recursion */
			len = hawk_subst_for_bchars_to_bcstr(buf, bsz, dfl->ptr, dfl->len, subst, ctx);
			tmp = buf + len;
		}
		else tmp = buf;
	}

	return tmp;
}

hawk_oow_t hawk_subst_for_bchars_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, hawk_oow_t fsz, hawk_subst_for_bcs_t subst, void* ctx)
{
	hawk_bch_t* b = buf;
	hawk_bch_t* end = buf + bsz - 1;
	const hawk_bch_t* f = fmt;
	const hawk_bch_t* fend = fmt + fsz;

	if (buf != HAWK_SUBST_NOBUF && bsz <= 0) return 0;

	while (f < fend)
	{
		if (*f == '$' && f < fend - 1)
		{
			if (*(f + 1) == '{')
			{
				const hawk_bch_t* tmp;
				hawk_bcs_t ident, dfl;

				tmp = scan_dollar_for_subst_b(f, fend - f, &ident, &dfl, 0);
				if (!tmp || ident.len <= 0) goto normal;
				f = tmp;

				if (buf != HAWK_SUBST_NOBUF)
				{
					b = exapnd_dollar_for_subst_b(b, end - b + 1, &ident, &dfl, subst, ctx);
					if (b >= end) goto fini;
				}
				else
				{
					/* the buffer points to HAWK_SUBST_NOBUF. */
					tmp = exapnd_dollar_for_subst_b(buf, bsz, &ident, &dfl, subst, ctx);
					/* increment b by the length of the expanded string */
					b += (tmp - buf);
				}

				continue;
			}
			else if (*(f + 1) == '$') 
			{
				/* $$ -> $. */
				f++;
			}
		}

	normal:
		if (buf != HAWK_SUBST_NOBUF)
		{
			if (b >= end) break;
			*b = *f;
		}
		b++; f++;
	}

fini:
	if (buf != HAWK_SUBST_NOBUF) *b = '\0';
	return b - buf;
}

hawk_oow_t hawk_subst_for_bcstr_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, hawk_subst_for_bcs_t subst, void* ctx)
{
	return hawk_subst_for_bchars_to_bcstr(buf, bsz, fmt, hawk_count_bcstr(fmt), subst, ctx);
}
/* ------------------------------------------------------------------------ */
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
			buf[--len] = (hawk_ooch_t)rem + 'a' - 10;
		else
			buf[--len] = (hawk_ooch_t)rem + '0';
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

/* ------------------------------------------------------------------------ */

hawk_int_t hawk_uchars_to_int (const hawk_uch_t* str, hawk_oow_t len, int option, const hawk_uch_t** endptr, int* is_sober)
{
	hawk_int_t n = 0;
	const hawk_uch_t* p, * pp;
	const hawk_uch_t* end;
	hawk_oow_t rem;
	int digit, negative = 0;
	int base = HAWK_OOCHARS_TO_INT_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;

	if (HAWK_OOCHARS_TO_INT_GET_OPTION_LTRIM(option))
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

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp); 

	if (HAWK_OOCHARS_TO_INT_GET_OPTION_RTRIM(option))
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
	int base = HAWK_OOCHARS_TO_INT_GET_OPTION_BASE(option);

	p = str; 
	end = str + len;
	
	if (HAWK_OOCHARS_TO_INT_GET_OPTION_LTRIM(option))
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

	/* base 8: at least a zero digit has been seen.
	 * other case: p > pp to be able to have at least 1 meaningful digit. */
	if (is_sober) *is_sober = (base == 8 || p > pp);

	if (HAWK_OOCHARS_TO_INT_GET_OPTION_RTRIM(option))
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	if (endptr) *endptr = p;
	return (negative)? -n: n;
}

/*
 * hawk_uchars_to_flt/hawk_bchars_to_flt is almost a replica of strtod.
 *
 * strtod.c --
 *
 *      Source code for the "strtod" library procedure.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/*
 *                double(64bits)    extended(80-bits)    quadruple(128-bits)
 *  exponent      11 bits           15 bits              15 bits
 *  fraction      52 bits           63 bits              112 bits
 *  sign          1 bit             1 bit                1 bit
 *  integer                         1 bit
 */         
#define FLT_MAX_EXPONENT 511

hawk_flt_t hawk_uchars_to_flt (const hawk_uch_t* str, hawk_oow_t len, const hawk_uch_t** endptr, int stripspc)
{
	/* 
	 * Table giving binary powers of 10. Entry is 10^2^i.  
	 * Used to convert decimal exponents into floating-point numbers.
	 */ 
	static hawk_flt_t powers_of_10[] = 
	{
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	hawk_flt_t fraction, dbl_exp, * d;
	const hawk_uch_t* p, * end;
	hawk_uci_t c;
	int exp = 0; /* Exponent read from "EX" field */

	/* 
	 * Exponent that derives from the fractional part.  Under normal 
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However, if I is very long, the last digits of I get dropped 
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, frac_exp is 
	 * incremented one for each dropped digit. 
	 */

	int frac_exp;
	int mant_size; /* Number of digits in mantissa. */
	int dec_pt;    /* Number of mantissa digits BEFORE decimal point */
	const hawk_uch_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;
	end = str + len;

	/* Strip off leading blanks and check for a sign */
	/*while (AWK_IS_SPACE(*p)) p++;*/
	if (stripspc)
	{
		/* strip off leading spaces */ 
		while (p < end && hawk_is_uch_space(*p)) p++;
	}

	/*while (*p != _T('\0')) */
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

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	/*for (mant_size = 0; ; mant_size++) */
	for (mant_size = 0; p < end; mant_size++) 
	{
		c = *p;
		if (!hawk_is_uch_digit(c)) 
		{
			if (c != '.' || dec_pt >= 0) break;
			dec_pt = mant_size;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mant_size;
	if (dec_pt < 0) 
	{
		dec_pt = mant_size;
	} 
	else 
	{
		mant_size--;	/* One of the digits was the point */
	}

	if (mant_size > 18)  /* TODO: is 18 correct for hawk_flt_t??? */
	{
		frac_exp = dec_pt - 18;
		mant_size = 18;
	} 
	else 
	{
		frac_exp = dec_pt - mant_size;
	}

	if (mant_size == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		p = pexp;
		goto done;
	} 
	else 
	{
		int frac1, frac2;

		frac1 = 0;
		for ( ; mant_size > 9; mant_size--) 
		{
			c = *p;
			p++;
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - '0');
		}

		frac2 = 0;
		for (; mant_size > 0; mant_size--) 
		{
			c = *p++;
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (p < end && (*p == 'E' || *p == 'e')) 
	{
		p++;

		if (p < end) 
		{
			if (*p == '-') 
			{
				exp_negative = 1;
				p++;
			} 
			else 
			{
				if (*p == '+') p++;
				exp_negative = 0;
			}
		}
		else exp_negative = 0;

		if (!(p < end && hawk_is_uch_digit(*p))) 
		{
			/*p = pexp;*/
			/*goto done;*/
			goto no_exp;
		}

		while (p < end && hawk_is_uch_digit(*p)) 
		{
			exp = exp * 10 + (*p - '0');
			p++;
		}
	}

no_exp:
	if (exp_negative) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		exp_negative = 1;
		exp = -exp;
	} 
	else exp_negative = 0;

	if (exp > FLT_MAX_EXPONENT) exp = FLT_MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	if (stripspc)
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_uch_space(*p)) p++;
	}
	if (endptr) *endptr = p;
	return (negative)? -fraction: fraction;
}

hawk_flt_t hawk_bchars_to_flt (const hawk_bch_t* str, hawk_oow_t len, const hawk_bch_t** endptr, int stripspc)
{
	/* 
	 * Table giving binary powers of 10. Entry is 10^2^i.  
	 * Used to convert decimal exponents into floating-point numbers.
	 */ 
	static hawk_flt_t powers_of_10[] = 
	{
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	hawk_flt_t fraction, dbl_exp, * d;
	const hawk_bch_t* p, * end;
	hawk_bci_t c;
	int exp = 0; /* Exponent read from "EX" field */

	/* 
	 * Exponent that derives from the fractional part.  Under normal 
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However, if I is very long, the last digits of I get dropped 
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, frac_exp is 
	 * incremented one for each dropped digit. 
	 */

	int frac_exp;
	int mant_size; /* Number of digits in mantissa. */
	int dec_pt;    /* Number of mantissa digits BEFORE decimal point */
	const hawk_bch_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;
	end = str + len;

	/* Strip off leading blanks and check for a sign */
	/*while (AWK_IS_SPACE(*p)) p++;*/
	if (stripspc)
	{
		/* strip off leading spaces */ 
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	/*while (*p != _T('\0')) */
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

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	/*for (mant_size = 0; ; mant_size++) */
	for (mant_size = 0; p < end; mant_size++) 
	{
		c = *p;
		if (!hawk_is_bch_digit(c)) 
		{
			if (c != '.' || dec_pt >= 0) break;
			dec_pt = mant_size;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mant_size;
	if (dec_pt < 0) 
	{
		dec_pt = mant_size;
	} 
	else 
	{
		mant_size--;	/* One of the digits was the point */
	}

	if (mant_size > 18)  /* TODO: is 18 correct for hawk_flt_t??? */
	{
		frac_exp = dec_pt - 18;
		mant_size = 18;
	} 
	else 
	{
		frac_exp = dec_pt - mant_size;
	}

	if (mant_size == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		p = pexp;
		goto done;
	} 
	else 
	{
		int frac1, frac2;

		frac1 = 0;
		for ( ; mant_size > 9; mant_size--) 
		{
			c = *p;
			p++;
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - '0');
		}

		frac2 = 0;
		for (; mant_size > 0; mant_size--) 
		{
			c = *p++;
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (p < end && (*p == 'E' || *p == 'e')) 
	{
		p++;

		if (p < end) 
		{
			if (*p == '-') 
			{
				exp_negative = 1;
				p++;
			} 
			else 
			{
				if (*p == '+') p++;
				exp_negative = 0;
			}
		}
		else exp_negative = 0;

		if (!(p < end && hawk_is_bch_digit(*p))) 
		{
			/*p = pexp;*/
			/*goto done;*/
			goto no_exp;
		}

		while (p < end && hawk_is_bch_digit(*p)) 
		{
			exp = exp * 10 + (*p - '0');
			p++;
		}
	}

no_exp:
	if (exp_negative) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		exp_negative = 1;
		exp = -exp;
	} 
	else exp_negative = 0;

	if (exp > FLT_MAX_EXPONENT) exp = FLT_MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	if (stripspc)
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}
	if (endptr) *endptr = p;
	return (negative)? -fraction: fraction;
}

int hawk_uchars_to_num (int option, const hawk_uch_t* ptr, hawk_oow_t len, hawk_int_t* l, hawk_flt_t* r)
{
	const hawk_uch_t* endptr;
	const hawk_uch_t* end;

	int nopartial = HAWK_OOCHARS_TO_NUM_GET_OPTION_NOPARTIAL(option);
	int reqsober = HAWK_OOCHARS_TO_NUM_GET_OPTION_REQSOBER(option);
	int stripspc = HAWK_OOCHARS_TO_NUM_GET_OPTION_STRIPSPC(option);
	int base = HAWK_OOCHARS_TO_NUM_GET_OPTION_BASE(option);
	int is_sober;

	end = ptr + len;
	*l = hawk_uchars_to_int(ptr, len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc,0,base), &endptr, &is_sober);
	if (endptr < end)
	{
		if (*endptr == '.' || *endptr == 'E' || *endptr == 'e')
		{
		handle_float:
			*r = hawk_uchars_to_flt(ptr, len, &endptr, stripspc);
			if (nopartial && endptr < end) return -1;
			return 1; /* flt */
		}
		else if (hawk_is_uch_digit(*endptr))
		{
			const hawk_uch_t* p = endptr;
			do { p++; } while (p < end && hawk_is_uch_digit(*p));
			if (p < end && (*p == '.' || *p == 'E' || *p == 'e')) 
			{
				/* it's probably an floating-point number. 
				 * 
 				 *  BEGIN { b=99; printf "%f\n", (0 b 1.112); }
				 *
				 * for the above code, this function gets '0991.112'.
				 * and endptr points to '9' after hawk_uchars_to_int() as
				 * the numeric string beginning with 0 is treated 
				 * as an octal number. 
				 * 
				 * getting side-tracked, 
				 *   BEGIN { b=99; printf "%f\n", (0 b 1.000); }
				 * the above code cause this function to get 0991, not 0991.000
				 * because of the default CONVFMT '%.6g' which doesn't produce '.000'.
				 * so such a number is not treated as a floating-point number.
				 */
				goto handle_float;
			}
		}

		if (stripspc)
		{
			while (endptr < end && hawk_is_uch_space(*endptr)) endptr++;
		}
	}

	if (reqsober && !is_sober) return -1;
	if (nopartial && endptr < end) return -1;
	return 0; /* int */
}

int hawk_bchars_to_num (int option, const hawk_bch_t* ptr, hawk_oow_t len, hawk_int_t* l, hawk_flt_t* r)
{
	const hawk_bch_t* endptr;
	const hawk_bch_t* end;

	int nopartial = HAWK_OOCHARS_TO_NUM_GET_OPTION_NOPARTIAL(option);
	int reqsober = HAWK_OOCHARS_TO_NUM_GET_OPTION_REQSOBER(option);
	int stripspc = HAWK_OOCHARS_TO_NUM_GET_OPTION_STRIPSPC(option);
	int base = HAWK_OOCHARS_TO_NUM_GET_OPTION_BASE(option);
	int is_sober;

	end = ptr + len;
	*l = hawk_bchars_to_int(ptr, len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc,0,base), &endptr, &is_sober);
	if (endptr < end)
	{
		if (*endptr == '.' || *endptr == 'E' || *endptr == 'e')
		{
		handle_float:
			*r = hawk_bchars_to_flt(ptr, len, &endptr, stripspc);
			if (nopartial && endptr < end) return -1;
			return 1; /* flt */
		}
		else if (hawk_is_uch_digit(*endptr))
		{
			const hawk_bch_t* p = endptr;
			do { p++; } while (p < end && hawk_is_bch_digit(*p));
			if (p < end && (*p == '.' || *p == 'E' || *p == 'e')) 
			{
				/* it's probably an floating-point number. 
				 * 
 				 *  BEGIN { b=99; printf "%f\n", (0 b 1.112); }
				 *
				 * for the above code, this function gets '0991.112'.
				 * and endptr points to '9' after hawk_bchars_to_int() as
				 * the numeric string beginning with 0 is treated 
				 * as an octal number. 
				 * 
				 * getting side-tracked, 
				 *   BEGIN { b=99; printf "%f\n", (0 b 1.000); }
				 * the above code cause this function to get 0991, not 0991.000
				 * because of the default CONVFMT '%.6g' which doesn't produce '.000'.
				 * so such a number is not treated as a floating-point number.
				 */
				goto handle_float;
			}
		}

		if (stripspc)
		{
			while (endptr < end && hawk_is_bch_space(*endptr)) endptr++;
		}
	}

	if (reqsober && !is_sober) return -1;
	if (nopartial && endptr < end) return -1;
	return 0; /* int */
}

/* ------------------------------------------------------------------------ */

int hawk_uchars_to_bin (const hawk_uch_t* hex, hawk_oow_t hexlen, hawk_uint8_t* buf, hawk_oow_t buflen)
{
	const hawk_uch_t* end = hex + hexlen;
	hawk_oow_t bi = 0;

	while (hex < end && bi < buflen)
	{
		int v;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		if (hex >= end) return -1;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		bi++;
	}

	return 0;
}

int hawk_bchars_to_bin (const hawk_bch_t* hex, hawk_oow_t hexlen, hawk_uint8_t* buf, hawk_oow_t buflen)
{
	const hawk_bch_t* end = hex + hexlen;
	hawk_oow_t bi = 0;

	while (hex < end && bi < buflen)
	{
		int v;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		if (hex >= end) return -1;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		bi++;
	}

	return 0;
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

/* ------------------------------------------------------------------------ */

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
			if (hawk_comp_ucstr_bcstr(name, builtin_cmgr_tab[i].name, 0) == 0)
			{
				return &builtin_cmgr[builtin_cmgr_tab[i].id];
			}
		 }
	}

	return HAWK_NULL;
}

/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------------ */
