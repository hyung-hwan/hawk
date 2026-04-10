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

#if defined(HAWK_ENABLE_CMGR_ALL) || defined(HAWK_ENABLE_CMGR_GBK)

#include "gbk.h"

hawk_oow_t hawk_uc_to_gbk (hawk_uch_t uc, hawk_bch_t* bc, hawk_oow_t size)
{
	hawk_uint16_t gc;

	if (size <= 0) return 0; /* buffer too small */

	if (uc <= 0x7F)
	{
		bc[0] = uc;
		return 1;
	}

	if (uc == 0x20AC)
	{
		bc[0] = 0x80;
		return 1;
	}

	if (uc < 0x00A4 || uc > 0xFFE5) return 0;

	gc = uc_to_gbk_0[uc - 0x00A4];
	if (gc == 0) return 0;

	if (gc <= 0x00FF)
	{
		bc[0] = (hawk_bchu_t)gc;
		return 1;
	}

	if (size <= 1) return 0; /* buffer too small */

	bc[0] = (hawk_bchu_t)(gc >> 8);
	bc[1] = (hawk_bchu_t)(gc & 0xFF);
	return 2;
}

hawk_oow_t hawk_gbk_to_uc (const hawk_bch_t* bc, hawk_oow_t size, hawk_uch_t* uc)
{
	hawk_uint16_t gc, xc;

	if (size <= 0) return 0; /* input too small */

	gc = (hawk_bchu_t)bc[0];
	if (gc < 0x80)
	{
		*uc = gc;
		return 1;
	}

	if (gc == 0x80)
	{
		*uc = 0x20AC;
		return 1;
	}

	if (size <= 1) return 0; /* insufficient input */

	gc = (gc << 8) | (hawk_bchu_t)bc[1];
	if (gc < 0x8140 || gc > 0xFE4F) return 0;

	xc = gbk_to_uc_0[gc - 0x8140];
	if (xc == 0) return 0;

	*uc = xc;
	return 2;
}

#endif
