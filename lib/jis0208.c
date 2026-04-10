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

#if defined(HAWK_ENABLE_CMGR_ALL) || defined(HAWK_ENABLE_CMGR_JIS0208)

#include "jis0208.h"

hawk_oow_t hawk_uc_to_jis0208 (hawk_uch_t uc, hawk_bch_t* bc, hawk_oow_t size)
{
	hawk_uint16_t jc;

	if (size <= 0) return size + 1; /* buffer too small */

	if (uc <= 0x7F)
	{
		bc[0] = uc;
		return 1;
	}

	if (uc >= 0x00A1 && uc <= 0x00FF)
		jc = uc_to_jis0208_0[uc - 0x00A1];
	else if (uc >= 0x0391 && uc <= 0x0451)
		jc = uc_to_jis0208_1[uc - 0x0391];
	else if (uc >= 0x2010 && uc <= 0x2FFF)
		jc = uc_to_jis0208_2[uc - 0x2010];
	else if (uc >= 0x3000 && uc <= 0x3FFF)
		jc = uc_to_jis0208_3[uc - 0x3000];
	else if (uc >= 0x4E00 && uc <= 0x5FFF)
		jc = uc_to_jis0208_4[uc - 0x4E00];
	else if (uc >= 0x6000 && uc <= 0x6FFF)
		jc = uc_to_jis0208_5[uc - 0x6000];
	else if (uc >= 0x7000 && uc <= 0x7FFF)
		jc = uc_to_jis0208_6[uc - 0x7000];
	else if (uc >= 0x8000 && uc <= 0x8FFF)
		jc = uc_to_jis0208_7[uc - 0x8000];
	else if (uc >= 0x9000 && uc <= 0x9FA0)
		jc = uc_to_jis0208_8[uc - 0x9000];
	else if (uc >= 0xFF01 && uc <= 0xFFE5)
		jc = uc_to_jis0208_9[uc - 0xFF01];
	else
		return 0; /* not convertable */

	if (jc == 0) return 0;
	if (size <= 1) return size + 1; /* buffer too small */

	bc[0] = (hawk_bchu_t)(jc >> 8);
	bc[1] = (hawk_bchu_t)(jc & 0xFF);
	return 2;
}

hawk_oow_t hawk_jis0208_to_uc (const hawk_bch_t* bc, hawk_oow_t size, hawk_uch_t* uc)
{
	hawk_uint16_t jc, xc;

	if (size <= 0) return 0; /* input too small */

	if (size >= 2)
	{
		jc = ((hawk_bchu_t)bc[0] << 8) | (hawk_bchu_t)bc[1];
		if (jc >= 0x2121 && jc <= 0x7426)
		{
			xc = jis0208_to_uc_0[jc - 0x2121];
			if (xc != 0)
			{
				*uc = xc;
				return 2;
			}
		}
	}

	jc = (hawk_bchu_t)bc[0];
	if (jc < 0x80)
	{
		*uc = jc;
		return 1;
	}

	return 0; /* not convertable */
}

#endif
