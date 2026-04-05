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
#include "ksc5601.h"

hawk_oow_t hawk_uc_to_ksc5601 (hawk_uch_t uc, hawk_bch_t* bc, hawk_oow_t size)
{
	hawk_uint16_t kc;

	if (size <= 0) return 0; /* buffer too small */

	if (uc <= 0x80) /* 7-bit ascii */
	{
		bc[0] = uc;
		return 1;
	}

	if (uc >= 0x00a1 && uc <= 0x0167)
		kc = uc_to_ksc_0[uc - 0x00a1];
	else if (uc >= 0x02c7 && uc <= 0x0451)
		kc = uc_to_ksc_1[uc - 0x02c7];
	else if (uc >= 0x2015 && uc <= 0x2312)
		kc = uc_to_ksc_2[uc - 0x2015];
	else if (uc >= 0x2460 && uc <= 0x266d)
		kc = uc_to_ksc_3[uc - 0x2460];
	else if (uc >= 0x3000 && uc <= 0x327f)
		kc = uc_to_ksc_4[uc - 0x3000];
	else if (uc >= 0x3380 && uc <= 0x33dd)
		kc = uc_to_ksc_5[uc - 0x3380];
	else if (uc >= 0x4e00 && uc <= 0x947f)
		kc = uc_to_ksc_6[uc - 0x4e00];
	else if (uc >= 0x9577 && uc <= 0x9f9c)
		kc = uc_to_ksc_7[uc - 0x9577];
	else if (uc >= 0xac00 && uc <= 0xd7a3)
		kc = uc_to_ksc_8[uc - 0xac00];
	else if (uc >= 0xf900 && uc <= 0xfa0b)
		kc = uc_to_ksc_9[uc - 0xf900];
	else if (uc >= 0xff01 && uc <= 0xffe6)
		kc = uc_to_ksc_10[uc - 0x01];
	else
		return 0; /* not convertable */

	if (size <= 1) return 0; /* buffer too small */

	bc[0] = (hawk_bchu_t)(kc >> 8);
	bc[1] = (hawk_bchu_t)(kc & 0xff);
	return 2;
}

hawk_oow_t hawk_ksc5601_to_uc (const hawk_bch_t* bc, hawk_oow_t size, hawk_uch_t* uc)
{
	hawk_uint16_t kc, xc;

	if (size <= 0) return 0; /* input too small */

	kc = (hawk_bchu_t)bc[0];
	if (kc < 0x80)
	{
		*uc = kc;
		return 1;
	}

	if (size <= 1) return 0; /* insufficient input */

	kc = (kc << 8) | (hawk_bchu_t)bc[1];
	if (kc >= 0x8141 && kc <= 0xc8fe) /* hangul */
	{
 		xc = ksc_to_uc_0[kc - 0x8141];
		if (xc == 0) return 0;
		*uc = xc;
		return 2;
	}
	else if (kc >= 0xcaa1 && kc <= 0xfdfe) /* hanja */
	{
 		xc = ksc_to_uc_1[kc - 0xcaa1];
		if (xc == 0) return 0;
		*uc = xc;
		return 2;
	}

	return 0; /* not convertable */
}
