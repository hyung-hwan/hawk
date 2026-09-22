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

#ifndef HAWK_SHA256_H
#define HAWK_SHA256_H

#include <hawk.h>

#define HAWK_SHA256_DIGEST_LEN (32)

struct hawk_sha256_ctx_t
{
    hawk_uint8_t data[64];
    hawk_uint8_t datalen;

    hawk_uint32_t bitlen_lo; /* for systems that don't have 64 bit integer types */
    hawk_uint32_t bitlen_hi;

    hawk_uint32_t state[8];
};

typedef struct hawk_sha256_ctx_t hawk_sha256_ctx_t;

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT void hawk_sha256_init (
	hawk_sha256_ctx_t* ctx
);

HAWK_EXPORT void hawk_sha256_update (
	hawk_sha256_ctx_t* ctx,
	const void*        data,
	hawk_oow_t         len
);

HAWK_EXPORT void hawk_sha256_final (
	hawk_sha256_ctx_t* ctx,
	hawk_uint8_t       hash[HAWK_SHA256_DIGEST_LEN]
);

/* convenience function for quick digestion */
HAWK_EXPORT void hawk_sha256_digest (
	hawk_uint8_t      hash[HAWK_SHA256_DIGEST_LEN],
	const void*       data,
	hawk_oow_t        dlen
);

#if defined(__cplusplus)
}
#endif

#endif
