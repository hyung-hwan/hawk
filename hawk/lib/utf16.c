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

/* TODO: handle different endians - UTF16BE or UTF16LE */

enum
{
	HIGH_SURROGATE_START = 0xD800,
	HIGH_SURROGATE_END   = 0xDBFF,
	LOW_SURROGATE_START  = 0xDC00,
	LOW_SURROGATE_END    = 0xDFFF
};

hawk_oow_t hawk_uc_to_utf16 (hawk_uch_t uc, hawk_bch_t* utf16, hawk_oow_t size)
{
	hawk_uint16_t* u16 = (hawk_uint16_t*)utf16;

	if (uc <= 0xFFFF) 
	{
		u16[0] = (hawk_uint16_t)uc;
		return 2;
	}
#if (HAWK_SIZEOF_UCH_T > 2)
	else if (uc <= 0x10FFFF) 
	{
		u16[0] = HIGH_SURROGATE_START | (((uc >> 16) & 0x1F) - 1) | (uc >> 10);
		u16[1] = LOW_SURROGATE_START | (uc & 0x3FF);
		return 4;
	}
#endif

	return 0; /* illegal character */
}

hawk_oow_t hawk_utf16_to_uc (const hawk_bch_t* utf16, hawk_oow_t size, hawk_uch_t* uc)
{
	const hawk_uint16_t* u16 = (const hawk_uint16_t*)utf16;

	if (size < 2) return 0; /* incomplete sequence */

	if (u16[0] < HIGH_SURROGATE_START || u16[0] > LOW_SURROGATE_END) 
	{
		/* BMP - U+0000 - U+D7FF, U+E000 - U+FFFF */
		*uc = u16[0];
		return 2;
	}
#if (HAWK_SIZEOF_UCH_T > 2)
	else if (u16[0] >= HIGH_SURROGATE_START && u16[0] <= HIGH_SURROGATE_END) /* high-surrogate */
	{
		if (size < 4) return 0; /* incomplete */
		if (u16[1] >= LOW_SURROGATE_START && u16[1] <= LOW_SURROGATE_END) /* low-surrogate */
		{
			*uc = (((u16[0] & 0x3FF) << 10) | (u16[1] & 0x3FF)) + 0x10000;
			return 4;
		}
	}
#endif

	return 0;
}
