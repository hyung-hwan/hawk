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

