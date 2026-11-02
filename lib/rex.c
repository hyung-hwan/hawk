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

/* shared custom-engine dispatch and an adapter for the standalone RGX engine. */
#include "hawk-rex.h"
#include "hawk-rgx.h"
#include "rex-prv.h"
#include "hawk-utl.h"
#include "hawk-str.h"

int hawk_rex_checkprm (hawk_gem_t* gem, const hawk_rex_prm_t* prm)
{
	if (!prm || ((prm->build || prm->match || prm->free) &&
	             (!prm->build || !prm->match || !prm->free)))
	{
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}
	return 0;
}

int hawk_rex_build (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm,
	const hawk_ooch_t* pattern, hawk_oow_t len, int flags,
	void** code, hawk_oow_t* nsub)
{
	void* tmp = HAWK_NULL;
	hawk_oow_t ns = 0;
	int n;

	if (hawk_rex_checkprm(gem, prm) <= -1) return -1;
	if (!prm->build || !pattern || !code || !nsub ||
	    (flags & ~(HAWK_REX_EXTENDED | HAWK_REX_IGNORECASE | HAWK_REX_NOBOUND | HAWK_REX_NEWLINE | HAWK_REX_NONSTDEXT)))
	{
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	n = prm->build(gem, prm->ctx, pattern, len, flags, &tmp, &ns);
	if (n == -1) return -1; /* The builder owns its failure cleanup. */
	if (n != 0 || !tmp || ns == HAWK_REX_NOMATCH)
	{
		if (tmp) prm->free(prm->ctx, tmp);
		hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_EINVAL, "invalid rex builder result");
		return -1;
	}

	*code = tmp;
	*nsub = ns;

	return 0;
}

int hawk_rex_buildpair (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm,
	const hawk_ooch_t* pattern, hawk_oow_t len, int flags,
	void** code, void** icode)
{
	void* c = HAWK_NULL;
	void* ic = HAWK_NULL;
	hawk_oow_t nsub;

	if (code && hawk_rex_build(gem, prm, pattern, len, flags, &c, &nsub) <= -1) return -1;

	if (icode && hawk_rex_build(gem, prm, pattern, len, flags | HAWK_REX_IGNORECASE, &ic, &nsub) <= -1)
	{
		if (c) prm->free(prm->ctx, c);
		return -1;
	}

	if (code) *code = c;
	if (icode) *icode = ic;

	return 0;
}

int hawk_rex_match (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm, const void* code,
	const hawk_rex_input_t* input, int flags,
	hawk_rex_match_t* matches, hawk_oow_t count)
{
	hawk_oow_t i;
	int n;

	if (hawk_rex_checkprm(gem, prm) <= -1) return -1;

	if (!prm->match || !code || !input || !input->ptr ||
	    (input->type != HAWK_REX_INPUT_BCH && input->type != HAWK_REX_INPUT_UCH) ||
	    input->start > input->end || input->end > input->len ||
	    (count > 0 && !matches) || (flags & ~(HAWK_REX_NOTBOL | HAWK_REX_NOTEOL)))
	{
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	for (i = 0; i < count; i++) matches[i].start = matches[i].end = HAWK_REX_NOMATCH;

	n = prm->match(gem, prm->ctx, code, input, flags, matches, count);
	if (n == -1 || n == 0) return n;
	if (n != 1) goto invalid;

	for (i = 0; i < count; i++)
	{
		if (i > 0 && matches[i].start == HAWK_REX_NOMATCH && matches[i].end == HAWK_REX_NOMATCH) continue;
		if (matches[i].start < input->start || matches[i].start > matches[i].end || matches[i].end > input->end) goto invalid;
		if (i > 0 && (matches[i].start < matches[0].start || matches[i].end > matches[0].end)) goto invalid;
	}

	return 1;

invalid:
	hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_EINVAL, "invalid rex matcher result");
	return -1;
}

#define match_xcs hawk_rex_matchbcs
#define xcs_t hawk_bcs_t
#define input_type HAWK_REX_INPUT_BCH
#include "rex-imp.h"
#undef match_xcs
#undef xcs_t
#undef input_type

#define match_xcs hawk_rex_matchucs
#define xcs_t hawk_ucs_t
#define input_type HAWK_REX_INPUT_UCH
#include "rex-imp.h"
#undef match_xcs
#undef xcs_t
#undef input_type

static int rgx_build (
	hawk_gem_t* gem, void* ctx, const hawk_ooch_t* pattern,
	hawk_oow_t len, int flags, void** code, hawk_oow_t* nsub)
{
	hawk_rgx_t* rgx;
	/* The adapter follows Hawk/Sed's existing TRE bracket syntax. Direct
	 * hawk_rgx_* users retain literal bracket backslashes by default. */
	int opt = HAWK_RGX_BRACKETESC;

	if (flags & (HAWK_REX_NEWLINE | HAWK_REX_NONSTDEXT))
	{
		hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_ENOSUP, "RGX does not support the requested rex compilation flags");
		return -1;
	}
	if (flags & HAWK_REX_EXTENDED) opt |= HAWK_RGX_EXTENDED;
	if (flags & HAWK_REX_IGNORECASE) opt |= HAWK_RGX_IGNORECASE;
	if (flags & HAWK_REX_NOBOUND) opt |= HAWK_RGX_NOBOUND;

	rgx = hawk_rgx_open(gem, 0, HAWK_NULL);
	if (HAWK_UNLIKELY(!rgx)) return -1;

	hawk_rgx_setopt(rgx, opt);
	if (!hawk_rgx_comp(rgx, pattern, len))
	{
		hawk_rgx_close(rgx);
		return -1;
	}

	*code = rgx;
	*nsub = hawk_rgx_getnsub(rgx);
	return 0;
}

static int rgx_match (
	hawk_gem_t* gem, void* ctx, const void* code,
	const hawk_rex_input_t* input, int flags,
	hawk_rex_match_t* matches, hawk_oow_t count)
{
	const hawk_rgx_t* rgx = code;
	hawk_oocs_t str, substr;
	hawk_oocs_t local[10];
	hawk_oocs_t* captures = local;
	hawk_ooch_t* converted = HAWK_NULL;
	hawk_oow_t i, ncap = hawk_rgx_getnsub(rgx) + 1;
	int n, opt = 0;

	if (ncap > count) ncap = count;
#if defined(HAWK_OOCH_IS_UCH)
	if (input->type == HAWK_REX_INPUT_UCH) str.ptr = (hawk_ooch_t*)input->ptr;
#else
	if (input->type == HAWK_REX_INPUT_BCH) str.ptr = (hawk_ooch_t*)input->ptr;
#endif
	else
	{
		/* MB8 is a width conversion, not a text decoding: each byte maps to
		 * exactly one UCH, so offsets remain in the caller's original units. */
		if (input->len >= HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(*converted))
		{
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOMEM);
			return -1;
		}

		converted = hawk_gem_allocmem(gem, (input->len + 1) * HAWK_SIZEOF(*converted));
		if (HAWK_UNLIKELY(!converted)) return -1;

		for (i = 0; i < input->len; i++)
		{
#if defined(HAWK_OOCH_IS_UCH)
			converted[i] = ((const hawk_uint8_t*)input->ptr)[i];
#else
			if (((const hawk_uch_t*)input->ptr)[i] > 255)
			{
				hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_ENOSUP, "RGX byte build cannot match UCH values above 255");
				hawk_gem_freemem(gem, converted);
				return -1;
			}
			converted[i] = ((const hawk_uch_t*)input->ptr)[i];
#endif
		}
		str.ptr = converted;
	}
	str.len = input->len;
	substr.ptr = str.ptr + input->start;
	substr.len = input->end - input->start;
	if (ncap > HAWK_COUNTOF(local))
	{
		if (ncap > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(*captures))
		{
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOMEM);
			goto oops;
		}

		captures = hawk_gem_allocmem(gem, ncap * HAWK_SIZEOF(*captures));
		if (HAWK_UNLIKELY(!captures)) goto oops;
	}

	if (flags & HAWK_REX_NOTBOL) opt |= HAWK_RGX_NOTBOL;
	if (flags & HAWK_REX_NOTEOL) opt |= HAWK_RGX_NOTEOL;

	n = hawk_rgx_matchx(gem, rgx->code, rgx->option, &str, &substr, captures, ncap, opt);
	if (n == 1)
	{
		for (i = 0; i < count; i++) matches[i].start = matches[i].end = HAWK_REX_NOMATCH;
		for (i = 0; i < ncap; i++)
		{
			if (captures[i].ptr)
			{
				matches[i].start = captures[i].ptr - str.ptr;
				matches[i].end = matches[i].start + captures[i].len;
			}
		}
	}

	if (captures != local) hawk_gem_freemem(gem, captures);
	if (converted) hawk_gem_freemem(gem, converted);

	return n;

oops:
	if (captures != local) hawk_gem_freemem(gem, captures);
	if (converted) hawk_gem_freemem(gem, converted);
	return -1;
}

static void rgx_free (void* ctx, void* code)
{
	hawk_rgx_close(code);
}

const hawk_rex_prm_t* hawk_rex_getrgxprm (void)
{
	static const hawk_rex_prm_t prm =
	{
		rgx_build,
		rgx_match,
		rgx_free,
		HAWK_NULL
	};
	return &prm;
}
