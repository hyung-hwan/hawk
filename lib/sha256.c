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

#include <hawk-sha256.h>
#include "hawk-prv.h"

#define ROTRIGHT(word,bits) (((word) >> (bits)) | ((word) << (32-(bits))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const hawk_uint32_t k[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform (hawk_sha256_ctx_t* ctx,  const hawk_uint8_t data[])
{
	hawk_uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

	for (i = 0, j = 0; i < 16; ++i, j += 4)
		m[i] = ((hawk_uint32_t)data[j] << 24) | ((hawk_uint32_t)data[j + 1] << 16) | ((hawk_uint32_t)data[j + 2] << 8) | ((hawk_uint32_t)data[j + 3]);
	for ( ; i < 64; ++i)
		m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];

	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

	for (i = 0; i < 64; ++i)
	{
		t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
		t2 = EP0(a) + MAJ(a,b,c);
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}

	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

/* the message length is counted in bits and the padding needs 64 of them.
 * this platform set has no 64-bit integer type to rely on - OpenVMS VAX
 * rejects both 'long long' and '__int64' - so the counter is kept as two
 * 32-bit halves and the carry is done by hand. */
static void bitlen_add (hawk_sha256_ctx_t* ctx, hawk_uint32_t nbits)
{
	ctx->bitlen_lo += nbits;
	/* unsigned arithmetic wraps, so a result below what was added is the
	 * carry out of the low half */
	if (ctx->bitlen_lo < nbits) ctx->bitlen_hi++;
}

void hawk_sha256_init (hawk_sha256_ctx_t* ctx)
{
	ctx->datalen = 0;
	ctx->bitlen_lo = 0;
	ctx->bitlen_hi = 0;
	ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c; ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

void hawk_sha256_update (hawk_sha256_ctx_t* ctx, const void* data, hawk_oow_t dlen)
{
	hawk_oow_t i;
	const hawk_uint8_t* dptr = (const hawk_uint8_t*)data;

	for (i = 0; i < dlen; ++i)
	{
		ctx->data[ctx->datalen++] = dptr[i];
		if (ctx->datalen == 64)
		{
			sha256_transform(ctx, ctx->data);
			bitlen_add(ctx, 512);
			ctx->datalen = 0;
		}
	}
}

void hawk_sha256_final (hawk_sha256_ctx_t* ctx, hawk_uint8_t hash[HAWK_SHA256_DIGEST_LEN])
{
	hawk_uint32_t i = ctx->datalen;

	if (ctx->datalen < 56)
	{
		ctx->data[i++] = 0x80;
		while (i < 56) ctx->data[i++] = 0x00;
	}
	else
	{
		ctx->data[i++] = 0x80;
		while (i < 64) ctx->data[i++] = 0x00;
		sha256_transform(ctx, ctx->data);
		HAWK_MEMSET(ctx->data, 0, 56);
	}

	/* datalen is below 64 here, so the multiply cannot overflow */
	bitlen_add(ctx, ctx->datalen * 8);

	ctx->data[63] = ctx->bitlen_lo;
	ctx->data[62] = ctx->bitlen_lo >> 8;
	ctx->data[61] = ctx->bitlen_lo >> 16;
	ctx->data[60] = ctx->bitlen_lo >> 24;
	ctx->data[59] = ctx->bitlen_hi;
	ctx->data[58] = ctx->bitlen_hi >> 8;
	ctx->data[57] = ctx->bitlen_hi >> 16;
	ctx->data[56] = ctx->bitlen_hi >> 24;
	sha256_transform(ctx, ctx->data);

	for (i = 0; i < 4; ++i)
	{
		hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
		hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
	}
}

void hawk_sha256_digest (hawk_uint8_t hash[HAWK_SHA256_DIGEST_LEN], const void* data, hawk_oow_t dlen)
{
	hawk_sha256_ctx_t ctx;
	hawk_sha256_init(&ctx);
	hawk_sha256_update(&ctx, data, dlen);
	hawk_sha256_final(&ctx, hash);
}
