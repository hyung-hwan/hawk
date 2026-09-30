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

#ifndef _HAWK_SHA1_H_
#define _HAWK_SHA1_H_

#include <hawk.h>

#define HAWK_SHA1_DIGEST_LEN (20)
#define HAWK_SHA1_BLOCK_LEN  (64)

struct hawk_sha1_ctx_t
{
	hawk_uint8_t  data[HAWK_SHA1_BLOCK_LEN];
	hawk_uint8_t  datalen;

	/* the padding encodes the message length in bits as a 64-bit quantity.
	 * it is held as two 32-bit halves so that nothing here depends on a
	 * 64-bit integer type being available. */
	hawk_uint32_t bitlen_lo;
	hawk_uint32_t bitlen_hi;

	hawk_uint32_t state[5];
};
typedef struct hawk_sha1_ctx_t hawk_sha1_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

HAWK_EXPORT void hawk_sha1_init (
	hawk_sha1_ctx_t* ctx
);

HAWK_EXPORT void hawk_sha1_update (
	hawk_sha1_ctx_t* ctx,
	const void*      data,
	hawk_oow_t       len
);

/**
 * The hawk_sha1_final() function writes the digest of everything fed to
 * hawk_sha1_update() so far. The context is spent once this returns; feed a
 * fresh one through hawk_sha1_init() to hash anything else.
 */
HAWK_EXPORT void hawk_sha1_final (
	hawk_sha1_ctx_t* ctx,
	hawk_uint8_t     hash[HAWK_SHA1_DIGEST_LEN]
);

/**
 * The hawk_sha1_digest() function hashes one buffer that is already whole,
 * which is the init/update/final sequence with nothing in between.
 */
HAWK_EXPORT void hawk_sha1_digest (
	hawk_uint8_t    hash[HAWK_SHA1_DIGEST_LEN],
	const void*     data,
	hawk_oow_t      dlen
);

#ifdef __cplusplus
}
#endif

#endif
