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

#include "mod-digest.h"
#include <hawk-md5.h>
#include <hawk-sha1.h>
#include <hawk-sha256.h>

#include "../lib/hawk-prv.h"

typedef void (*digest_fnc_t) (hawk_uint8_t* digest, const void* data, hawk_oow_t len);

#define DIGEST_CTX_MAGIC   ((hawk_uint32_t)0x48444731ul) /* HDG1 */
#define DIGEST_CTX_VERSION ((hawk_uint32_t)1)

enum digest_algorithm_t
{
	DIGEST_ALGORITHM_MD5 = 1,
	DIGEST_ALGORITHM_SHA1,
	DIGEST_ALGORITHM_SHA256
};

typedef struct digest_ctx_t digest_ctx_t;
struct digest_ctx_t
{
	hawk_uint32_t magic;
	hawk_uint32_t version;
	hawk_uint32_t algorithm;
	hawk_uint32_t finalized;
	union
	{
		hawk_md5_ctx_t md5;
		hawk_sha1_ctx_t sha1;
		hawk_sha256_ctx_t sha256;
	} state;
};

static int set_invalid_context_error (hawk_rtx_t* rtx, const hawk_ooch_t* message)
{
	hawk_rtx_seterrfmt(rtx, HAWK_NULL, HAWK_EINVAL, HAWK_T("invalid digest context - %js"), message);
	return -1;
}

static int get_digest_context (hawk_rtx_t* rtx, hawk_val_t* val, hawk_val_bob_t** bob, digest_ctx_t* ctx)
{
	hawk_val_bob_t* bv;

	while (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_REF)
	{
		val = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)val);
		if (HAWK_UNLIKELY(!val))
			return set_invalid_context_error(rtx, HAWK_T("invalid reference"));
	}

	if (HAWK_RTX_GETVALTYPE(rtx, val) != HAWK_VAL_BOB)
		return set_invalid_context_error(rtx, HAWK_T("not a binary object"));

	bv = (hawk_val_bob_t*)val;
	if (bv->val.len != HAWK_SIZEOF(*ctx))
		return set_invalid_context_error(rtx, HAWK_T("wrong object size"));

	HAWK_MEMCPY(ctx, bv->val.ptr, HAWK_SIZEOF(*ctx));
	if (ctx->magic != DIGEST_CTX_MAGIC || ctx->version != DIGEST_CTX_VERSION ||
	    ctx->algorithm < DIGEST_ALGORITHM_MD5 || ctx->algorithm > DIGEST_ALGORITHM_SHA256)
	{
		return set_invalid_context_error(rtx, HAWK_T("unrecognized object"));
	}

	if (ctx->finalized)
		return set_invalid_context_error(rtx, HAWK_T("already finalized"));

	*bob = bv;
	return 0;
}

static int make_digest (hawk_rtx_t* rtx, digest_fnc_t digest_fnc, hawk_oow_t digest_len)
{
	hawk_val_t* arg;
	hawk_val_t* retv;
	hawk_bcs_t input;
	hawk_uint8_t digest[HAWK_SHA256_DIGEST_LEN];

	HAWK_ASSERT(digest_len <= HAWK_SIZEOF(digest));

	arg = hawk_rtx_getarg(rtx, 0);
	input.ptr = hawk_rtx_getvalbcstr(rtx, arg, &input.len);
	if (HAWK_UNLIKELY(!input.ptr)) return -1;

	digest_fnc(digest, input.ptr, input.len);
	hawk_rtx_freevalbcstr(rtx, arg, input.ptr);

	retv = hawk_rtx_makembsvalwithbchars(rtx, (const hawk_bch_t*)digest, digest_len);
	if (HAWK_UNLIKELY(!retv)) return -1;

	hawk_rtx_setretval(rtx, retv);
	return 0;
}

static int fnc_md5 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return make_digest(rtx, hawk_md5_digest, HAWK_MD5_DIGEST_LEN);
}

static int fnc_sha1 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return make_digest(rtx, hawk_sha1_digest, HAWK_SHA1_DIGEST_LEN);
}

static int fnc_sha256 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return make_digest(rtx, hawk_sha256_digest, HAWK_SHA256_DIGEST_LEN);
}

static int fnc_init (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* arg;
	hawk_val_t* retv;
	hawk_bcs_t name;
	digest_ctx_t ctx;

	arg = hawk_rtx_getarg(rtx, 0);
	name.ptr = hawk_rtx_getvalbcstr(rtx, arg, &name.len);
	if (HAWK_UNLIKELY(!name.ptr)) return -1;

	HAWK_MEMSET(&ctx, 0, HAWK_SIZEOF(ctx));
	ctx.magic = DIGEST_CTX_MAGIC;
	ctx.version = DIGEST_CTX_VERSION;

	if (name.len == 3 && HAWK_MEMCMP(name.ptr, "md5", 3) == 0)
	{
		ctx.algorithm = DIGEST_ALGORITHM_MD5;
		hawk_md5_init(&ctx.state.md5);
	}
	else if (name.len == 4 && HAWK_MEMCMP(name.ptr, "sha1", 4) == 0)
	{
		ctx.algorithm = DIGEST_ALGORITHM_SHA1;
		hawk_sha1_init(&ctx.state.sha1);
	}
	else if (name.len == 6 && HAWK_MEMCMP(name.ptr, "sha256", 6) == 0)
	{
		ctx.algorithm = DIGEST_ALGORITHM_SHA256;
		hawk_sha256_init(&ctx.state.sha256);
	}
	else
	{
		hawk_rtx_freevalbcstr(rtx, arg, name.ptr);
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	hawk_rtx_freevalbcstr(rtx, arg, name.ptr);

	retv = hawk_rtx_makebobval(rtx, &ctx, HAWK_SIZEOF(ctx));
	if (HAWK_UNLIKELY(!retv)) return -1;

	hawk_rtx_setretval(rtx, retv);
	return 0;
}

static int fnc_update (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* ctx_arg;
	hawk_val_t* data_arg;
	hawk_val_bob_t* bob;
	digest_ctx_t ctx;
	hawk_bcs_t input;

	ctx_arg = hawk_rtx_getarg(rtx, 0);
	if (get_digest_context(rtx, ctx_arg, &bob, &ctx) <= -1) return -1;

	data_arg = hawk_rtx_getarg(rtx, 1);
	input.ptr = hawk_rtx_getvalbcstr(rtx, data_arg, &input.len);
	if (HAWK_UNLIKELY(!input.ptr)) return -1;

	switch (ctx.algorithm)
	{
		case DIGEST_ALGORITHM_MD5:
			hawk_md5_update(&ctx.state.md5, input.ptr, input.len);
			break;

		case DIGEST_ALGORITHM_SHA1:
			hawk_sha1_update(&ctx.state.sha1, input.ptr, input.len);
			break;

		case DIGEST_ALGORITHM_SHA256:
			hawk_sha256_update(&ctx.state.sha256, input.ptr, input.len);
			break;
	}

	hawk_rtx_freevalbcstr(rtx, data_arg, input.ptr);
	HAWK_MEMCPY(bob->val.ptr, &ctx, HAWK_SIZEOF(ctx));
	hawk_rtx_setretval(rtx, (hawk_val_t*)bob);
	return 0;
}

static int fnc_final (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* ctx_arg;
	hawk_val_t* retv;
	hawk_val_bob_t* bob;
	digest_ctx_t ctx;
	hawk_uint8_t digest[HAWK_SHA256_DIGEST_LEN];
	hawk_oow_t digest_len;

	ctx_arg = hawk_rtx_getarg(rtx, 0);
	if (get_digest_context(rtx, ctx_arg, &bob, &ctx) <= -1) return -1;

	switch (ctx.algorithm)
	{
		case DIGEST_ALGORITHM_MD5:
			hawk_md5_final(&ctx.state.md5, digest);
			digest_len = HAWK_MD5_DIGEST_LEN;
			break;

		case DIGEST_ALGORITHM_SHA1:
			hawk_sha1_final(&ctx.state.sha1, digest);
			digest_len = HAWK_SHA1_DIGEST_LEN;
			break;

		default:
			hawk_sha256_final(&ctx.state.sha256, digest);
			digest_len = HAWK_SHA256_DIGEST_LEN;
			break;
	}

	retv = hawk_rtx_makembsvalwithbchars(rtx, (const hawk_bch_t*)digest, digest_len);
	if (HAWK_UNLIKELY(!retv)) return -1;

	ctx.finalized = 1;
	HAWK_MEMCPY(bob->val.ptr, &ctx, HAWK_SIZEOF(ctx));
	hawk_rtx_setretval(rtx, retv);
	return 0;
}

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("final"),  { { 1, 1, HAWK_NULL }, fnc_final,  0 } },
	{ HAWK_T("init"),   { { 1, 1, HAWK_NULL }, fnc_init,   0 } },
	{ HAWK_T("md5"),    { { 1, 1, HAWK_NULL }, fnc_md5,    0 } },
	{ HAWK_T("sha1"),   { { 1, 1, HAWK_NULL }, fnc_sha1,   0 } },
	{ HAWK_T("sha256"), { { 1, 1, HAWK_NULL }, fnc_sha256, 0 } },
	{ HAWK_T("update"), { { 2, 2, HAWK_NULL }, fnc_update, 0 } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* nothing to do */
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	/* nothing to do */
}

int hawk_mod_digest (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = query;
	mod->unload = unload;
	mod->init = init;
	mod->fini = fini;
	mod->ctx = HAWK_NULL;
	return 0;
}
