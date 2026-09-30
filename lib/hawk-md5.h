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

#ifndef _HAWK_MD5_H_
#define _HAWK_MD5_H_

#include <hawk.h>

#define HAWK_MD5_DIGEST_LEN (16)
#define HAWK_MD5_BLOCK_LEN  (64)

struct hawk_md5_ctx_t
{
	hawk_uint32_t  count[2];
	hawk_uint32_t  state[4];
	hawk_uint8_t   buffer[HAWK_MD5_BLOCK_LEN];
};
typedef struct hawk_md5_ctx_t hawk_md5_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

HAWK_EXPORT void hawk_md5_init (
	hawk_md5_ctx_t* ctx
);

HAWK_EXPORT void hawk_md5_update (
	hawk_md5_ctx_t* ctx,
	const void*     data,
	hawk_oow_t      len
);

/**
 * The hawk_md5_final() function writes the digest of everything fed to
 * hawk_md5_update() so far. The context is spent once this returns; feed a
 * fresh one through hawk_md5_init() to hash anything else.
 */
HAWK_EXPORT void hawk_md5_final (
	hawk_md5_ctx_t* ctx,
	hawk_uint8_t    hash[HAWK_MD5_DIGEST_LEN]
);

/**
 * The hawk_md5_digest() function hashes one buffer that is already whole,
 * which is the init/update/final sequence with nothing in between.
 */
HAWK_EXPORT void hawk_md5_digest (
	hawk_uint8_t   hash[HAWK_MD5_DIGEST_LEN],
	const void*    data,
	hawk_oow_t     dlen
);

#ifdef __cplusplus
}
#endif

#endif
