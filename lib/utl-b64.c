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

/* base64 as defined in rfc 4648. sections 4 and 5 differ only in the last two
 * characters of the alphabet, so the two variants are one implementation with
 * a different table. */

static const hawk_bch_t b64_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";

static const hawk_bch_t b64url_chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-_";

/* the index of a character in the alphabet, or -1 if it is not in it. the
 * ranges are tested rather than a 256-entry table looked up, because the table
 * is the kind of thing that is wrong in one cell and passes every test that
 * does not happen to use that character. */
static int b64_value (hawk_bch_t c, int options)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;

	if (options & HAWK_BASE64_URL)
	{
		if (c == '-') return 62;
		if (c == '_') return 63;
	}
	else
	{
		if (c == '+') return 62;
		if (c == '/') return 63;
	}

	return -1;
}

static int b64_is_space (hawk_bch_t c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

int hawk_conv_bin_to_base64 (const void* bin, hawk_oow_t* binlen, hawk_bch_t* b64, hawk_oow_t* b64len, int options)
{
	const hawk_uint8_t* p = (const hawk_uint8_t*)bin;
	const hawk_bch_t* alpha = (options & HAWK_BASE64_URL)? b64url_chars: b64_chars;
	hawk_oow_t inlen = *binlen;
	hawk_oow_t cap = *b64len;
	hawk_oow_t i = 0, o = 0;

	if (!b64)
	{
		/* sizing only. the length is a function of the input alone. */
		*b64len = (options & HAWK_BASE64_NOPAD)? HAWK_BASE64_NOPAD_LEN(inlen): HAWK_BASE64_LEN(inlen);
		return 0;
	}

	/* three octets become four characters, and a group is written whole or
	 * not at all - half of one decodes to something that was never the input */
	while (inlen - i >= 3)
	{
		if (o + 4 > cap) goto nospace;
		b64[o++] = alpha[p[i] >> 2];
		b64[o++] = alpha[((p[i] & 0x03) << 4) | (p[i + 1] >> 4)];
		b64[o++] = alpha[((p[i + 1] & 0x0F) << 2) | (p[i + 2] >> 6)];
		b64[o++] = alpha[p[i + 2] & 0x3F];
		i += 3;
	}

	if (i < inlen)
	{
		/* one or two octets are left. padded, they still take a full group and
		 * the pads say how many of the four characters carry data; unpadded,
		 * only the characters that carry data are written. */
		hawk_oow_t rem = inlen - i;

		if (o + ((options & HAWK_BASE64_NOPAD)? rem + 1: 4) > cap) goto nospace;

		b64[o++] = alpha[p[i] >> 2];
		if (rem == 1)
		{
			b64[o++] = alpha[(p[i] & 0x03) << 4];
			if (!(options & HAWK_BASE64_NOPAD))
			{
				b64[o++] = '=';
				b64[o++] = '=';
			}
		}
		else
		{
			b64[o++] = alpha[((p[i] & 0x03) << 4) | (p[i + 1] >> 4)];
			b64[o++] = alpha[(p[i + 1] & 0x0F) << 2];
			if (!(options & HAWK_BASE64_NOPAD)) b64[o++] = '=';
		}
		i = inlen;
	}

	*binlen = i;
	*b64len = o;
	return 0;

nospace:
	*binlen = i;
	*b64len = o;
	return -2;
}

int hawk_conv_base64_to_bin (const hawk_bch_t* b64, hawk_oow_t* b64len, void* bin, hawk_oow_t* binlen, int options)
{
	hawk_uint8_t* out = (hawk_uint8_t*)bin;
	hawk_oow_t inlen = *b64len;
	hawk_oow_t cap = bin? *binlen: 0;
	hawk_oow_t i, o = 0;
	hawk_uint32_t acc = 0;
	int ngrp = 0;  /* characters gathered towards the current group of four */
	int padded = 0;
	int npad = 0;

	for (i = 0; i < inlen; i++)
	{
		hawk_bch_t c = b64[i];
		int v;

		if (b64_is_space(c)) continue;

		if (c == '=')
		{
			/* padding only means something once a group has enough characters
			 * to carry an octet. anywhere else it is not a short group, it is
			 * a malformed one. If padding is present, require the exact number
			 * needed to complete the final group. */
			if (ngrp < 2 || npad >= 4 - ngrp) goto illegal;
			padded = 1;
			npad++;
			continue;
		}

		/* data after the padding would be a second message sharing the buffer,
		 * and taking it would mean decoding something the padding said had
		 * ended */
		if (padded) goto illegal;

		v = b64_value(c, options);
		if (v < 0) goto illegal;

		acc = (acc << 6) | (hawk_uint32_t)v;
		if (++ngrp == 4)
		{
			if (out)
			{
				if (o + 3 > cap) goto nospace;
				out[o + 0] = (hawk_uint8_t)(acc >> 16);
				out[o + 1] = (hawk_uint8_t)(acc >> 8);
				out[o + 2] = (hawk_uint8_t)acc;
			}
			o += 3;
			ngrp = 0;
			acc = 0;
		}
	}

	/* what is left over is a short final group. two characters carry one
	 * octet and three carry two; a single character carries six bits, which
	 * is no octet at all and cannot have come from an encoder. */
	if (padded && ngrp + npad != 4) goto illegal;
	if (ngrp == 1) goto illegal;

	if (ngrp == 2)
	{
		if (out)
		{
			if (o + 1 > cap) goto nospace;
			out[o] = (hawk_uint8_t)(acc >> 4);
		}
		o += 1;
	}
	else if (ngrp == 3)
	{
		if (out)
		{
			if (o + 2 > cap) goto nospace;
			out[o + 0] = (hawk_uint8_t)(acc >> 10);
			out[o + 1] = (hawk_uint8_t)(acc >> 2);
		}
		o += 2;
	}

	*b64len = i;
	*binlen = o;
	return 0;

illegal:
	*b64len = i;
	*binlen = o;
	return -1;

nospace:
	*b64len = i;
	*binlen = o;
	return -2;
}
