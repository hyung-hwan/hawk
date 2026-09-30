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

typedef void (*digest_fnc_t) (hawk_uint8_t* digest, const void* data, hawk_oow_t len);

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

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("md5"),    { { 1, 1, HAWK_NULL }, fnc_md5,    0 } },
	{ HAWK_T("sha1"),   { { 1, 1, HAWK_NULL }, fnc_sha1,   0 } },
	{ HAWK_T("sha256"), { { 1, 1, HAWK_NULL }, fnc_sha256, 0 } }
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
