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

#if defined(HAWK_ENABLE_CMGR_ALL) || defined(HAWK_ENABLE_CMGR_BIG5)

#include "big5.h"

hawk_oow_t hawk_uc_to_big5 (hawk_uch_t uc, hawk_bch_t* bc, hawk_oow_t size)
{
	hawk_uint16_t xc;

	if (size <= 0) return 0;

	if (uc <= 0x7F)
	{
		bc[0] = uc;
		return 1;
	}

	if (uc >= 0x00A2 && uc <= 0x00F7)
		xc = uc_to_big5_0[uc - 0x00A2];
	else if (uc >= 0x02C7 && uc <= 0x0451)
		xc = uc_to_big5_1[uc - 0x02C7];
	else if (uc >= 0x2013 && uc <= 0x22BF)
		xc = uc_to_big5_2[uc - 0x2013];
	else if (uc >= 0x2460 && uc <= 0x2642)
		xc = uc_to_big5_3[uc - 0x2460];
	else if (uc >= 0x3000 && uc <= 0x3129)
		xc = uc_to_big5_4[uc - 0x3000];
	else if (uc >= 0x32A3 && uc <= 0x32A3)
		xc = uc_to_big5_5[uc - 0x32A3];
	else if (uc >= 0x338E && uc <= 0x33D5)
		xc = uc_to_big5_6[uc - 0x338E];
	else if (uc >= 0x4E00 && uc <= 0x9483)
		xc = uc_to_big5_7[uc - 0x4E00];
	else if (uc >= 0x9577 && uc <= 0x9FA4)
		xc = uc_to_big5_8[uc - 0x9577];
	else if (uc >= 0xFA0C && uc <= 0xFA0D)
		xc = uc_to_big5_9[uc - 0xFA0C];
	else if (uc >= 0xFE30 && uc <= 0xFFFD)
		xc = uc_to_big5_10[uc - 0xFE30];
	else
		return 0;

	if (xc == 0) return 0;
	if (size <= 1) return 0;

	bc[0] = (hawk_bchu_t)(xc >> 8);
	bc[1] = (hawk_bchu_t)(xc & 0xFF);
	return 2;
}

hawk_oow_t hawk_big5_to_uc (const hawk_bch_t* bc, hawk_oow_t size, hawk_uch_t* uc)
{
	hawk_uint16_t xc, out;

	if (size <= 0) return 0;

	xc = (hawk_bchu_t)bc[0];
	if (xc < 0x80)
	{
		*uc = xc;
		return 1;
	}

	if (size <= 1) return 0;

	xc = (xc << 8) | (hawk_bchu_t)bc[1];
	if (xc >= 0xA140 && xc <= 0xC7FC)
		out = big5_to_uc_0[xc - 0xA140];
	else if (xc >= 0xC940 && xc <= 0xF9DC)
		out = big5_to_uc_1[xc - 0xC940];
	else
		return 0;

	if (out == 0) return 0;
	*uc = out;
	return 2;
}

#endif
