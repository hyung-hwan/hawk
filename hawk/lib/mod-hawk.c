/*
 * $Id$
 *
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

#include "mod-hawk.h"
#include "hawk-prv.h"


/* ----------------------------------------------------------------- */
struct mod_ctx_t
{
	hawk_rbt_t* rtxtab;
};
typedef struct mod_ctx_t mod_ctx_t;

/* ----------------------------------------------------------------- */
struct rtx_data_t
{
	struct
	{
		hawk_uint8_t __static_buf[256];
		hawk_uint8_t* ptr;
		hawk_oow_t capa;
		hawk_oow_t len;
	} pack;
};
typedef struct rtx_data_t rtx_data_t;

/* ----------------------------------------------------------------- */

static HAWK_INLINE rtx_data_t* rtx_to_data (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
        mod_ctx_t* mctx = (mod_ctx_t*)fi->mod->ctx;
        hawk_rbt_pair_t* pair;
        pair = hawk_rbt_search(mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx));
        HAWK_ASSERT (pair != HAWK_NULL);
        return (rtx_data_t*)HAWK_RBT_VPTR(pair);
}

/* ----------------------------------------------------------------- */

/*
 * function abc(a, b, c) { return a + b + c; }
 * BEGIN { print hawk::call("abc", 10, 20, 30); }
 */
struct pafs_t
{
	hawk_fun_t* fun;
	hawk_oow_t stack_base;
	hawk_oow_t start_index;
	hawk_oow_t end_index;
};

static hawk_oow_t push_args_from_stack (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data)
{
	struct pafs_t* pasf = (struct pafs_t*)data;
	hawk_oow_t org_stack_base, spec_len, i, j;
	hawk_val_t* v;

	if (HAWK_RTX_STACK_AVAIL(rtx) < pasf->end_index - pasf->start_index + 1)
	{
		hawk_rtx_seterrnum (rtx, &call->loc, HAWK_ESTACK);
		return (hawk_oow_t)-1;
	}

	spec_len = pasf->fun->argspec? hawk_count_oocstr(pasf->fun->argspec): 0;
	
	org_stack_base = rtx->stack_base;
	for (i = pasf->start_index, j = 0; i <= pasf->end_index; i++, j++) 
	{
		hawk_ooch_t spec;

		rtx->stack_base = pasf->stack_base;
		v = HAWK_RTX_STACK_ARG(rtx, i);
		rtx->stack_base = org_stack_base;

		/* if not sufficient number of spec characters given, take the last value and use it */
		spec = (spec_len <= 0)? '\0': pasf->fun->argspec[((j < spec_len)? j: spec_len - 1)];

		if (HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_REF)
		{
			v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
		}
		else
		{
			if (spec == 'r')
			{
				hawk_rtx_seterrnum (rtx, &call->loc, HAWK_ENOTREF);
				return (hawk_oow_t)-1;
			}
		}

		HAWK_RTX_STACK_PUSH (rtx, v);
		hawk_rtx_refupval (rtx, v);
	}

	return pasf->end_index - pasf->start_index + 1;
}

static int fnc_call (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_fun_t* fun;
	hawk_oow_t nargs;
	hawk_nde_fncall_t call;
	struct pafs_t pafs;
	hawk_val_t* v;

	/* this function is similar to hawk_rtx_callfun() but it is more simplified */

	fun = hawk_rtx_valtofun(rtx, hawk_rtx_getarg(rtx, 0));
	if (!fun) return -1; /* hard failure */

	nargs = hawk_rtx_getnargs(rtx);
	if (nargs - 1 > fun->nargs)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EARGTM);
		return -1; /* hard failure */
	}

	HAWK_MEMSET (&call, 0, HAWK_SIZEOF(call));
	call.type = HAWK_NDE_FNCALL_FUN;
	call.u.fun.name = fun->name;
	call.nargs = nargs - 1;
	/* keep HAWK_NULL in call.args so that hawk_rtx_evalcall() knows it's a fake call structure */
	call.arg_base = rtx->stack_base + 5; /* 5 = 4(stack frame prologue) + 1(the first argument to hawk::call()) */

	pafs.fun = fun;
	pafs.stack_base = rtx->stack_base;
	pafs.start_index = 1;
	pafs.end_index = nargs - 1;

	v = hawk_rtx_evalcall(rtx, &call, fun, push_args_from_stack, (void*)&pafs, HAWK_NULL, HAWK_NULL);
	if (HAWK_UNLIKELY(!v)) return -1; /* hard failure */

	hawk_rtx_setretval (rtx, v);
	return 0;
}

/* hawk::function_exists("xxxx"); */
static int fnc_function_exists (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_ooch_t* str;
	hawk_oow_t len;
	int rx;

	a0 = hawk_rtx_getarg(rtx, 0);
	str = hawk_rtx_getvaloocstr(rtx, a0, &len);
	if (HAWK_UNLIKELY(!str))
	{
		rx = 0;
	}
	else
	{
		rx = (hawk_rtx_findfunwithoocstr(rtx, str) != HAWK_NULL);
		hawk_rtx_freevaloocstr (rtx, a0, str);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}
/* -------------------------------------------------------------------------- */

static int fnc_cmgr_exists (hawk_rtx_t* rtx, const hawk_fnc_info_t*  fi)
{
	hawk_val_t* a0;
	hawk_ooch_t* str;
	hawk_oow_t len;
	int rx;

	a0 = hawk_rtx_getarg(rtx, 0);
	str = hawk_rtx_getvaloocstr(rtx, a0, &len);
	if (HAWK_UNLIKELY(!str))
	{
		rx = 0;
	}
	else
	{
		rx = (hawk_get_cmgr_by_name(str) != HAWK_NULL);
		hawk_rtx_freevaloocstr (rtx, a0, str);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* -------------------------------------------------------------------------- */

/*
   hawk::gc();
   hawk::gc_get_threshold(gen)
   hawk::gc_set_threshold(gen, threshold)
   hawk::GC_NUM_GENS
 */

static int fnc_gc (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t gen = -1;

#if defined(HAWK_ENABLE_GC)
	if (hawk_rtx_getnargs(rtx) >= 1 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &gen) <= -1) gen = -1;
	gen = hawk_rtx_gc(rtx, gen);
#endif

	HAWK_ASSERT (HAWK_IN_QINT_RANGE(gen));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, gen));
	return 0;
}

static int fnc_gc_get_threshold (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t gen;
	hawk_int_t threshold;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &gen) <= -1) gen = 0;
	if (gen < 0) gen = 0;
	else if (gen >= HAWK_COUNTOF(rtx->gc.g)) gen = HAWK_COUNTOF(rtx->gc.g) - 1;

	threshold = rtx->gc.threshold[gen];

	HAWK_ASSERT (HAWK_IN_QINT_RANGE(threshold));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, threshold));
	return 0;
}

static int fnc_gc_set_threshold (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t gen;
	hawk_int_t threshold;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &gen) <= -1) gen = 0;
	if (gen < 0) gen = 0;
	else if (gen >= HAWK_COUNTOF(rtx->gc.g)) gen = HAWK_COUNTOF(rtx->gc.g) - 1;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &threshold) <= -1) threshold = -1;

	if (threshold >= 0) 
	{
		if (threshold >= HAWK_QINT_MAX) threshold = HAWK_QINT_MAX;
		rtx->gc.threshold[gen] = threshold; /* update */
	}
	else 
	{
		threshold = rtx->gc.threshold[gen]; /* no update. but retrieve the existing value */
	}

	HAWK_ASSERT (HAWK_IN_QINT_RANGE(threshold));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, threshold));
	return 0;
}

static int fnc_gcrefs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	a0 = hawk_rtx_getarg(rtx, 0);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, HAWK_VTR_IS_POINTER(a0)? a0->v_refs: 0));
	return 0;
}

/* -------------------------------------------------------------------------- */

static int fnc_array (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* tmp;
	hawk_oow_t nargs, i;

	nargs = hawk_rtx_getnargs(rtx);

	tmp = hawk_rtx_makearrval(rtx, ((nargs > 0)? nargs: -1));
	if (HAWK_UNLIKELY(!tmp)) return -1; /* hard failure */

	for (i = 0; i < nargs; i++)
	{
		if (HAWK_UNLIKELY(hawk_rtx_setarrvalfld(rtx, tmp, i + 1, hawk_rtx_getarg(rtx, i)) == HAWK_NULL))
		{
			hawk_rtx_freeval (rtx, tmp, 0);
			return -1;
		}
	}

	hawk_rtx_setretval (rtx, tmp);
	return 0;
}

static int fnc_map (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* tmp;
	hawk_oow_t nargs, i;
	hawk_ooch_t idxbuf[HAWK_IDX_BUF_SIZE];

	tmp = hawk_rtx_makemapval(rtx);
	if (HAWK_UNLIKELY(!tmp)) return -1; /* hard failure */

	nargs = hawk_rtx_getnargs(rtx);

	for (i = 0; i < nargs; i++)
	{
		hawk_rtx_valtostr_out_t out;
		hawk_val_t* v;

		v = hawk_rtx_getarg(rtx, i);
		out.type = HAWK_RTX_VALTOSTR_CPLCPY;
		out.u.cplcpy.ptr = idxbuf;
		out.u.cplcpy.len = HAWK_COUNTOF(idxbuf);
		if (hawk_rtx_valtostr(rtx, v, &out) <= -1)
		{
			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, v, &out) <= -1)
			{
				hawk_rtx_freeval (rtx, tmp, 0);
				return -1;
			}
		}

		v = (++i >= nargs)? hawk_val_nil: hawk_rtx_getarg(rtx, i);
		v = hawk_rtx_setmapvalfld(rtx, tmp, out.u.cpldup.ptr, out.u.cpldup.len, v);
		if (out.u.cpldup.ptr != idxbuf) hawk_rtx_freemem (rtx, out.u.cpldup.ptr);

		if (HAWK_UNLIKELY(!v))
		{
			hawk_rtx_freeval (rtx, tmp, 0);
			return -1;
		}

		/* if (i >= nargs) break;  this check is probably not needed. another i++ in the 3rd segment of the for statement should be mostly harmless. potential overflow issue is not a real issue as the number of arguments can be so high. */
	}

	hawk_rtx_setretval (rtx, tmp);
	return 0;
}

/* -------------------------------------------------------------------------- */

static int fnc_isnil (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;

	a0 = hawk_rtx_getarg(rtx, 0);

	r = hawk_rtx_makeintval(rtx, HAWK_RTX_GETVALTYPE(rtx, a0) == HAWK_VAL_NIL);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_ismap (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;

	a0 = hawk_rtx_getarg(rtx, 0);

	r = hawk_rtx_makeintval(rtx, HAWK_RTX_GETVALTYPE(rtx, a0) == HAWK_VAL_MAP);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_isarr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;

	a0 = hawk_rtx_getarg(rtx, 0);

	r = hawk_rtx_makeintval(rtx, HAWK_RTX_GETVALTYPE(rtx, a0) == HAWK_VAL_ARR);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_modlibdirs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_val_t* r;

	r = hawk_rtx_makestrvalwithoocstr(rtx, (hawk->opt.mod[0].len > 0)? hawk->opt.mod[0].ptr: HAWK_T(HAWK_DEFAULT_MODLIBDIRS));
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_typename (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;
	const hawk_ooch_t* name;

	a0 = hawk_rtx_getarg(rtx, 0);
	name = hawk_rtx_getvaltypename(rtx, a0);

	r = hawk_rtx_makestrvalwithoocstr(rtx, name);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

/* -------------------------------------------------------------------------- */

#define ENDIAN_BIG    1
#define ENDIAN_LITTLE 2
#if defined(HAWK_ENDIAN_BIG)
#	define ENDIAN_NATIVE ENDIAN_BIG
#else
#	define ENDIAN_NATIVE ENDIAN_LITTLE
#endif

static hawk_oow_t pack_int16_t (hawk_uint8_t* dst, hawk_uint16_t val, int endian)
{
	if (endian == ENDIAN_NATIVE)
	{
		*dst++ = val;
		*dst++ = val >> 8;
	}
	else
	{
		*dst++ = val >> 8;
		*dst++ = val;
	}

	return 2;
}

static hawk_oow_t pack_int32_t (hawk_uint8_t* dst, hawk_uint32_t val, int endian)
{
	if (endian == ENDIAN_NATIVE)
	{
		*dst++ = val;
		*dst++ = val >> 8;
		*dst++ = val >> 16;
		*dst++ = val >> 24;
	}
	else
	{
		*dst++ = val >> 24;
		*dst++ = val >> 16;
		*dst++ = val >> 8;
		*dst++ = val;
	}

	return 4;
}

static hawk_oow_t pack_int64_t (hawk_uint8_t* dst, hawk_uint64_t val, int endian)
{
	if (endian == ENDIAN_NATIVE)
	{
		*dst++ = val;
		*dst++ = val >> 8;
		*dst++ = val >> 16;
		*dst++ = val >> 24;
		*dst++ = val >> 32;
		*dst++ = val >> 40;
		*dst++ = val >> 48;
		*dst++ = val >> 56;
	}
	else
	{
		*dst++ = val >> 56;
		*dst++ = val >> 48;
		*dst++ = val >> 40;
		*dst++ = val >> 32;
		*dst++ = val >> 24;
		*dst++ = val >> 16;
		*dst++ = val >> 8;
		*dst++ = val;
	}

	return 8;
}

static int ensure_pack_buf (hawk_rtx_t* rtx, rtx_data_t* rdp, hawk_oow_t reqsz)
{
	if (rdp->pack.len >= rdp->pack.capa - reqsz)
	{
		hawk_uint8_t* tmp;
		hawk_oow_t newcapa;

		newcapa = HAWK_ALIGN_POW2(rdp->pack.capa + reqsz, 256);
		if (rdp->pack.ptr == rdp->pack.__static_buf)
		{
			tmp = hawk_rtx_allocmem(rtx, newcapa);
			if (HAWK_UNLIKELY(!tmp)) return -1;
			HAWK_MEMCPY (tmp, rdp->pack.__static_buf, rdp->pack.len);
		}
		else
		{
			tmp = hawk_rtx_reallocmem(rtx, rdp->pack.ptr, newcapa);
			if (HAWK_UNLIKELY(!tmp)) return -1;
		}

		rdp->pack.ptr = tmp;
		rdp->pack.capa = newcapa;
	}

	return 0;
}

static int pack_data (hawk_rtx_t* rtx, const hawk_oocs_t* fmt, const hawk_fnc_info_t* fi, rtx_data_t* rdp)
{
	hawk_oow_t rep_cnt, rep_set, rc;
	const hawk_ooch_t* fmtp, *fmte;
	hawk_oow_t arg_idx, arg_cnt;
	int endian = ENDIAN_NATIVE;


#define PACK_CHECK_ARG_AND_BUF(reqarg, reqsz) do { \
	if (arg_cnt - arg_idx < reqarg)  \
	{ \
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EARGTF); \
		return -1; \
	} \
	if (ensure_pack_buf(rtx, rdp, reqsz)  <= -1) return -1; \
} while(0)

	rdp->pack.len = 0;

	arg_idx = 1; /* set past the format specifier */
	arg_cnt = hawk_rtx_getnargs(rtx);

	rep_cnt = 1;
	rep_set = 0;

	fmte = fmt->ptr + fmt->len;
	for (fmtp = fmt->ptr; fmtp < fmte; fmtp++) 
	{
		switch (*fmtp) 
		{
			case '=': /* native */
				endian = ENDIAN_NATIVE;
				break;

			case '<': /* little-endian */
				endian = ENDIAN_LITTLE;
				break;

			case '>': /* big-endian */
				endian = ENDIAN_BIG;
				break;

			case '!': /* network (= big-endian) */
				endian = ENDIAN_BIG;
				break;

			case 'b': /* byte, char */
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_int8_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.ptr[rdp->pack.len++] = (hawk_int8_t)v;
				}
				break;
			}

			case 'B':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_uint8_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.ptr[rdp->pack.len++] = (hawk_uint8_t)v;
				}
				break;
			}

			case 'h':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_int16_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.len += pack_int16_t(&rdp->pack.ptr[rdp->pack.len], (hawk_int16_t)v, endian);
				}
				break;
			}

			case 'H':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_uint16_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.len += pack_int16_t(&rdp->pack.ptr[rdp->pack.len], (hawk_uint16_t)v, endian);
				}
				break;
			}

			case 'i':
			case 'l':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_int32_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.len += pack_int32_t(&rdp->pack.ptr[rdp->pack.len], (hawk_int32_t)v, endian);
				}
				break;
			}

			case 'I': /* fall through */
			case 'L':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_uint32_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.len += pack_int16_t(&rdp->pack.ptr[rdp->pack.len], (hawk_uint32_t)v, endian);
				}
				break;
			}

			case 'q':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_int64_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.len += pack_int64_t(rdp->pack.ptr, (hawk_int64_t)v, endian);
				}
				break;
			}

			case 'Q':
			{
				hawk_int_t v;
				PACK_CHECK_ARG_AND_BUF (rep_cnt, HAWK_SIZEOF(hawk_uint64_t) * rep_cnt);
				for (rc = 0; rc < rep_cnt; rc++)
				{
					if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, arg_idx++), &v) <= -1) return -1;
					rdp->pack.len += pack_int64_t(rdp->pack.ptr, (hawk_uint64_t)v, endian);
				}
				break;
			}

#if 0
			case 'f':
				f = va_arg(args, double);
				pack_float(&bp, f, *ep);
				break;

			case 'd':
				d = va_arg(args, double);
				pack_double(&bp, d, *ep);
				break;
#endif


			case 's':
			case 'p':
			{
				hawk_val_t* a;
				hawk_bcs_t tmp;

				PACK_CHECK_ARG_AND_BUF (1, HAWK_SIZEOF(hawk_uint8_t) * rep_cnt);

				a = hawk_rtx_getarg(rtx, arg_idx++);

				tmp.ptr = hawk_rtx_getvalbcstr(rtx, a, &tmp.len);
				if (HAWK_UNLIKELY(!tmp.ptr)) return -1;

				if (rep_cnt > tmp.len)
				{
					hawk_rtx_freevalbcstr (rtx, a, tmp.ptr);
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
					return -1;
				}
				for (rc = 0; rc < rep_cnt; rc++) rdp->pack.ptr[rdp->pack.len++] = tmp.ptr[rc];
				hawk_rtx_freevalbcstr (rtx, a, tmp.ptr);
				break;
			}

			case 'x': /* zero-padding */
				PACK_CHECK_ARG_AND_BUF (0, rep_cnt * HAWK_SIZEOF(hawk_uint8_t));
				for (rc = 0; rc < rep_cnt; rc++) rdp->pack.ptr[rdp->pack.len++] = 0;
				break;

			default:
				/* handle below outside 'switch' */
				if (hawk_is_ooch_digit(*fmtp))
				{
					if (!rep_set) 
					{
						rep_cnt = 0;
						rep_set = 1;
					}
					rep_cnt = rep_cnt * 10 + (*fmtp - '0');
				}
				else if (!hawk_is_ooch_space(*fmtp)) 
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
					return -1;
				}
				break;
		}

		if (!hawk_is_ooch_digit(*fmtp) && !hawk_is_ooch_space(*fmtp))
		{
			rep_cnt = 1;
			rep_set = 0;
		}
	}

	return 0;
}

static int fnc_pack (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	rtx_data_t* rdp = rtx_to_data(rtx, fi);
	hawk_val_t* a0;
	hawk_oocs_t fmt;
	int x;

	a0 = hawk_rtx_getarg(rtx, 0);
	fmt.ptr = hawk_rtx_getvaloocstr(rtx, a0, &fmt.len);
	if (HAWK_UNLIKELY(!fmt.ptr)) return -1;

/* TODO: how to indicate failure??? */
	x = pack_data(rtx, &fmt, fi, rdp);
	hawk_rtx_freevaloocstr (rtx, a0, fmt.ptr);

	if (x >= 0)
	{
		hawk_val_t* r;
		r = hawk_rtx_makembsvalwithbchars(rtx, rdp->pack.ptr, rdp->pack.len);
		if (HAWK_UNLIKELY(!r)) return -1;
		hawk_rtx_setretval (rtx, r);
	}
	return 0;
}

static int fnc_unpack (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
/* TODO: */
	return 0;
}

/* -------------------------------------------------------------------------- */

typedef struct fnctab_t fnctab_t;
struct fnctab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_fnc_t info;
};

typedef struct inttab_t inttab_t;
struct inttab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_int_t info;
};

#define A_MAX HAWK_TYPE_MAX(int)

static fnctab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("array"),            { { 0, A_MAX,  HAWK_NULL    },  fnc_array,                 0 } },
	{ HAWK_T("call"),             { { 1, A_MAX, HAWK_T("vR")  },  fnc_call,                  0 } },
	{ HAWK_T("cmgr_exists"),      { { 1, 1,     HAWK_NULL     },  fnc_cmgr_exists,           0 } },
	{ HAWK_T("function_exists"),  { { 1, 1,     HAWK_NULL     },  fnc_function_exists,       0 } },
	{ HAWK_T("gc"),               { { 0, 1,     HAWK_NULL     },  fnc_gc,                    0 } },
	{ HAWK_T("gcrefs"),           { { 1, 1,     HAWK_NULL     },  fnc_gcrefs,                0 } },
	{ HAWK_T("gc_get_threshold"), { { 1, 1,     HAWK_NULL     },  fnc_gc_get_threshold,      0 } },
	{ HAWK_T("gc_set_threshold"), { { 2, 2,     HAWK_NULL     },  fnc_gc_set_threshold,      0 } },
	{ HAWK_T("isarray"),          { { 1, 1,     HAWK_NULL     },  fnc_isarr,                 0 } },
	{ HAWK_T("ismap"),            { { 1, 1,     HAWK_NULL     },  fnc_ismap,                 0 } },
	{ HAWK_T("isnil"),            { { 1, 1,     HAWK_NULL     },  fnc_isnil,                 0 } },
	{ HAWK_T("map"),              { { 0, A_MAX, HAWK_NULL     },  fnc_map,                   0 } },
	{ HAWK_T("modlibdirs"),       { { 0, 0,     HAWK_NULL     },  fnc_modlibdirs,            0 } },
	{ HAWK_T("pack"),             { { 1, A_MAX, HAWK_NULL     },  fnc_pack,                  0 } },
	{ HAWK_T("typename"),         { { 1, 1,     HAWK_NULL     },  fnc_typename,              0 } },
	//{ HAWK_T("unpack"),           { { 1, A_MAX, HAWK_NULL     },  fnc_unpack,                0 } }
};

static inttab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("GC_NUM_GENS"), { HAWK_GC_NUM_GENS } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int left, right, mid, n;

	left = 0; right = HAWK_COUNTOF(fnctab) - 1;

	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = hawk_comp_oocstr(fnctab[mid].name, name, 0);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_FNC;
			sym->u.fnc = fnctab[mid].info;
			return 0;
		}
	}

	left = 0; right = HAWK_COUNTOF(inttab) - 1;
	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = hawk_comp_oocstr(inttab[mid].name, name, 0);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_INT;
			sym->u.in = inttab[mid].info;
			return 0;
		}
	}

	hawk_seterrfmt (hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js' not found"), name);
	return -1;
}

/* TODO: proper resource management */

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	mod_ctx_t* mctx = (mod_ctx_t*)mod->ctx;
	rtx_data_t rd, * rdp;
	hawk_rbt_pair_t* pair;

	HAWK_MEMSET (&rd, 0, HAWK_SIZEOF(rd));
	pair = hawk_rbt_insert(mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx), &rd, HAWK_SIZEOF(rd));
	if (HAWK_UNLIKELY(!pair)) return -1;

	rdp = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	rdp->pack.ptr = rdp->pack.__static_buf;
	rdp->pack.capa = HAWK_COUNTOF(rdp->pack.__static_buf);
	rdp->pack.len = 0;

	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	mod_ctx_t* mctx = (mod_ctx_t*)mod->ctx;
	hawk_rbt_pair_t* pair;

	pair = hawk_rbt_search(mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx));
	if (pair)
	{
		rtx_data_t* rdp;

		rdp = (rtx_data_t*)HAWK_RBT_VPTR(pair);

		if (rdp->pack.ptr != rdp->pack.__static_buf) hawk_rtx_freemem(rtx, rdp->pack.ptr);

		hawk_rbt_delete (mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx));
	}
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	mod_ctx_t* mctx = (mod_ctx_t*)mod->ctx;
	HAWK_ASSERT (HAWK_RBT_SIZE(mctx->rtxtab) == 0);
	hawk_rbt_close (mctx->rtxtab);
	hawk_freemem (hawk, mctx);
}

int hawk_mod_hawk (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	mod->ctx = hawk_callocmem(hawk, HAWK_SIZEOF(mod_ctx_t));
	if (HAWK_UNLIKELY(!mod->ctx)) return -1;

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (HAWK_UNLIKELY(!rbt))
	{
		hawk_freemem (hawk, mod->ctx);
		return -1;
	}
	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	((mod_ctx_t*)mod->ctx)->rtxtab = rbt;
	return 0;
}

