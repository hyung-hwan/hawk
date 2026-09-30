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

/* sha-1 as specified in fips 180-4. it is kept beside the sha-256
 * implementation and shaped the same way, because the two differ only in the
 * compression function and the digest width - the buffering and the padding
 * are identical. */

#include <hawk-sha1.h>
#include "hawk-prv.h"

#define ROTLEFT(word,bits) (((word) << (bits)) | ((word) >> (32-(bits))))

/* the block is read octet by octet into big-endian words rather than cast over,
 * so the result does not depend on the byte order of the machine and the input
 * needs no particular alignment. */
static void sha1_transform (hawk_sha1_ctx_t* ctx, const hawk_uint8_t data[])
{
	hawk_uint32_t a, b, c, d, e, i, j, t, m[80];

	for (i = 0, j = 0; i < 16; ++i, j += 4)
		m[i] = ((hawk_uint32_t)data[j] << 24) | ((hawk_uint32_t)data[j + 1] << 16) | ((hawk_uint32_t)data[j + 2] << 8) | ((hawk_uint32_t)data[j + 3]);
	for ( ; i < 80; ++i)
		m[i] = ROTLEFT(m[i - 3] ^ m[i - 8] ^ m[i - 14] ^ m[i - 16], 1);

	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4];

	/* the round function and the constant change every twenty rounds. that
	 * is the whole of what distinguishes the four quarters. */
	for (i = 0; i < 80; ++i)
	{
		if (i < 20)      t = ROTLEFT(a,5) + ((b & c) | (~b & d))          + e + 0x5a827999 + m[i];
		else if (i < 40) t = ROTLEFT(a,5) + (b ^ c ^ d)                   + e + 0x6ed9eba1 + m[i];
		else if (i < 60) t = ROTLEFT(a,5) + ((b & c) | (b & d) | (c & d)) + e + 0x8f1bbcdc + m[i];
		else             t = ROTLEFT(a,5) + (b ^ c ^ d)                   + e + 0xca62c1d6 + m[i];

		e = d; d = c; c = ROTLEFT(b,30); b = a; a = t;
	}

	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e;
}

/* the message length is counted in bits and the padding needs 64 of them. the
 * counter is kept as two 32-bit halves so that no 64-bit integer type is
 * required, and the carry between them is done by hand. */
static void bitlen_add (hawk_sha1_ctx_t* ctx, hawk_uint32_t nbits)
{
	ctx->bitlen_lo += nbits;
	/* unsigned arithmetic wraps, so a result below what was added is the
	 * carry out of the low half */
	if (ctx->bitlen_lo < nbits) ctx->bitlen_hi++;
}

void hawk_sha1_init (hawk_sha1_ctx_t* ctx)
{
	ctx->datalen = 0;
	ctx->bitlen_lo = 0;
	ctx->bitlen_hi = 0;
	ctx->state[0] = 0x67452301; ctx->state[1] = 0xefcdab89; ctx->state[2] = 0x98badcfe; ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xc3d2e1f0;
}

void hawk_sha1_update (hawk_sha1_ctx_t* ctx, const void* data, hawk_oow_t dlen)
{
	hawk_oow_t i;
	const hawk_uint8_t* dptr = (const hawk_uint8_t*)data;

	for (i = 0; i < dlen; ++i)
	{
		ctx->data[ctx->datalen++] = dptr[i];
		if (ctx->datalen == HAWK_SHA1_BLOCK_LEN)
		{
			sha1_transform(ctx, ctx->data);
			bitlen_add(ctx, HAWK_SHA1_BLOCK_LEN * 8);
			ctx->datalen = 0;
		}
	}
}

void hawk_sha1_final (hawk_sha1_ctx_t* ctx, hawk_uint8_t hash[HAWK_SHA1_DIGEST_LEN])
{
	hawk_uint32_t i = ctx->datalen;

	/* the padding is a single 1 bit, then zeros, then the length in the last
	 * 8 octets. if what is left in the block cannot hold the length, the block
	 * is padded out and processed and the length goes into one more. */
	if (ctx->datalen < 56)
	{
		ctx->data[i++] = 0x80;
		while (i < 56) ctx->data[i++] = 0x00;
	}
	else
	{
		ctx->data[i++] = 0x80;
		while (i < HAWK_SHA1_BLOCK_LEN) ctx->data[i++] = 0x00;
		sha1_transform(ctx, ctx->data);
		HAWK_MEMSET(ctx->data, 0, 56);
	}

	/* the octets still unaccounted for are the ones in this last partial
	 * block. datalen is below the block length here, so this cannot overflow. */
	bitlen_add(ctx, ctx->datalen * 8);

	ctx->data[63] = ctx->bitlen_lo;
	ctx->data[62] = ctx->bitlen_lo >> 8;
	ctx->data[61] = ctx->bitlen_lo >> 16;
	ctx->data[60] = ctx->bitlen_lo >> 24;
	ctx->data[59] = ctx->bitlen_hi;
	ctx->data[58] = ctx->bitlen_hi >> 8;
	ctx->data[57] = ctx->bitlen_hi >> 16;
	ctx->data[56] = ctx->bitlen_hi >> 24;
	sha1_transform(ctx, ctx->data);

	for (i = 0; i < 4; ++i)
	{
		hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
	}
}

void hawk_sha1_digest (hawk_uint8_t hash[HAWK_SHA1_DIGEST_LEN], const void* data, hawk_oow_t dlen)
{
	hawk_sha1_ctx_t ctx;
	hawk_sha1_init(&ctx);
	hawk_sha1_update(&ctx, data, dlen);
	hawk_sha1_final(&ctx, hash);
}
