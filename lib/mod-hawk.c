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

#include "mod-hawk.h"
#include "hawk-prv.h"
#include <hawk-mtx.h>


struct mod_data_t
{
	hawk_mtx_t mq_mtx;
};
typedef struct mod_data_t mod_data_t;
/* ----------------------------------------------------------------- */

/*
 * function abc(a, b, c) { return a + b + c; }
 * BEGIN { print hawk::call("abc", 10, 20, 30); }
 */
struct pafs_t
{
	int is_fun;

	const hawk_ooch_t* argspec_ptr;
	hawk_oow_t argspec_len;

	hawk_oow_t stack_base;
	hawk_oow_t start_index;
	hawk_oow_t end_index;
};

static hawk_oow_t push_args_from_stack (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data)
{
	struct pafs_t* pasf = (struct pafs_t*)data;
	hawk_oow_t org_stack_base, i, j;
	hawk_val_t* v;

	if (HAWK_RTX_STACK_AVAIL(rtx) < pasf->end_index - pasf->start_index + 1)
	{
		hawk_rtx_seterrnum (rtx, &call->loc, HAWK_ESTACK);
		return (hawk_oow_t)-1;
	}

	org_stack_base = rtx->stack_base;
	for (i = pasf->start_index, j = 0; i <= pasf->end_index; i++, j++)
	{
		hawk_ooch_t spec;

		rtx->stack_base = pasf->stack_base;
		v = HAWK_RTX_STACK_ARG(rtx, i);
		rtx->stack_base = org_stack_base;

		/* if not sufficient number of spec characters given, take the last value and use it */
		spec = (pasf->argspec_len <= 0)? '\0': pasf->argspec_ptr[((j < pasf->argspec_len)? j: pasf->argspec_len - 1)];

		if (HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_REF)
		{
			if (pasf->is_fun)
			{
				/* take out the actual value and pass it to the callee
				 * only if the callee is a user-defined function */
				v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			}
		}
		else
		{
			if (spec == 'r') /* 'R' allows a normal value. so only checking 'r' here */
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
	hawk_oow_t nargs, f_nargs;
	hawk_nde_fncall_t call;
	struct pafs_t pafs;
	hawk_val_t* v;

	/* this function is similar to hawk_rtx_callfun() but it is more simplified */
	HAWK_MEMSET (&call, 0, HAWK_SIZEOF(call));
	nargs = hawk_rtx_getnargs(rtx);
	f_nargs = nargs - 1;

	fun = hawk_rtx_valtofun(rtx, hawk_rtx_getarg(rtx, 0));
	if (fun)
	{
		if (f_nargs > fun->nargs)
		{
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EARGTM);
			return -1; /* hard failure */
		}

		/* user-defined function call */
		call.type = HAWK_NDE_FNCALL_FUN;
		call.u.fun.name = fun->name;

		pafs.is_fun = 1;
		pafs.argspec_ptr = fun->argspec;
		pafs.argspec_len = fun->argspec? hawk_count_oocstr(fun->argspec): 0;
	}
	else
	{
		/* find the name in the modules */
		hawk_fnc_t fnc, * fncp;
		mod_data_t* md;

		md = (mod_data_t*)fi->mod->ctx;

		/* hawk_querymodulewithname() called by hawk_rtx_valtofnc()
		 * may update some shared data under the hawk object.
		 * use a mutex for shared data safety */
		/* TODO: this mutex protection is wrong in that if a call to hawk_querymodulewithname()
		 *       is made outside this hawk module, the call is not protected under
		 *       the same mutex. FIX THIS */
		hawk_mtx_lock(&md->mq_mtx, HAWK_NULL);
		fncp = hawk_rtx_valtofnc(rtx, hawk_rtx_getarg(rtx, 0), &fnc);
		hawk_mtx_unlock(&md->mq_mtx);
		if (!fncp) return -1; /* hard failure */

		if (f_nargs < fnc.spec.arg.min  || f_nargs > fnc.spec.arg.max)
		{
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EARGTM);
			return -1;
		}

		call.type = HAWK_NDE_FNCALL_FNC;
		call.u.fnc.info.name.ptr = fnc.name.ptr;
		call.u.fnc.info.name.len = fnc.name.len;
		call.u.fnc.info.mod = fnc.mod;
		call.u.fnc.spec = fnc.spec;

		pafs.is_fun = 0;
		pafs.argspec_ptr = call.u.fnc.spec.arg.spec;
		pafs.argspec_len = call.u.fnc.spec.arg.spec? hawk_count_oocstr(call.u.fnc.spec.arg.spec): 0;
	}

	call.nargs = f_nargs;
	/* keep HAWK_NULL in call.args so that hawk_rtx_evalcall() knows it's a fake call structure */
	call.arg_base = rtx->stack_base + 5; /* 5 = 4(stack frame prologue) + 1(the first argument to hawk::call()) */

	pafs.stack_base = rtx->stack_base;
	pafs.start_index = 1;
	pafs.end_index = nargs - 1;

	v = hawk_rtx_evalcall(rtx, &call, fun, push_args_from_stack, (void*)&pafs, HAWK_NULL, HAWK_NULL);
	if (HAWK_UNLIKELY(!v)) return -1; /* hard failure */

	hawk_rtx_setretval(rtx, v);
	return 0;
}

/* hawk::function_exists("xxxx");
 * hawk::function_exists("sys::getpid") */
static int fnc_function_exists (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_oocs_t name;
	int rx;

	a0 = hawk_rtx_getarg(rtx, 0);
	name.ptr = hawk_rtx_getvaloocstr(rtx, a0, &name.len);
	if (HAWK_UNLIKELY(!name.ptr))
	{
		rx = 0;
	}
	else
	{
		if (hawk_count_oocstr(name.ptr) != name.len) rx = 0;
		else
		{
			rx = (hawk_rtx_findfunwithoocstr(rtx, name.ptr) != HAWK_NULL);
			if (!rx)
			{
				rx = (hawk_findfncwithoocs(hawk_rtx_gethawk(rtx), &name) != HAWK_NULL);
				if (!rx)
				{
					hawk_mod_sym_t sym;
					mod_data_t* md;

					md = (mod_data_t*)fi->mod->ctx;
					/* hawk_query_module_with_name() may update some shared data under
					 * the hawk object. use a mutex for shared data safety */
					hawk_mtx_lock(&md->mq_mtx, HAWK_NULL);
					rx = (hawk_querymodulewithname(hawk_rtx_gethawk(rtx), name.ptr, &sym) != HAWK_NULL);
					hawk_mtx_unlock(&md->mq_mtx);
				}
			}
		}
		hawk_rtx_freevaloocstr(rtx, a0, name.ptr);
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, rx));
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
		hawk_rtx_freevaloocstr(rtx, a0, str);
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, rx));
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

	HAWK_ASSERT(HAWK_IN_INT_RANGE(gen));
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, gen));
	return 0;
}

static int fnc_gc_get_pressure (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t gen;
	hawk_int_t pressure;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &gen) <= -1) gen = 0;
	if (gen < 0) gen = 0;
	else if (gen >= HAWK_COUNTOF(rtx->gc.g)) gen = HAWK_COUNTOF(rtx->gc.g) - 1;

	pressure = rtx->gc.pressure[gen];

	HAWK_ASSERT(HAWK_IN_INT_RANGE(pressure));
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, pressure));
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

	HAWK_ASSERT(HAWK_IN_INT_RANGE(threshold));
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, threshold));
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
		if (threshold >= HAWK_INT_MAX) threshold = HAWK_INT_MAX;
		rtx->gc.threshold[gen] = threshold; /* update */
	}
	else
	{
		threshold = rtx->gc.threshold[gen]; /* no update. but retrieve the existing value */
	}

	HAWK_ASSERT(HAWK_IN_INT_RANGE(threshold));
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, threshold));
	return 0;
}

static int fnc_gcrefs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	a0 = hawk_rtx_getarg(rtx, 0);
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, HAWK_VTR_IS_POINTER(a0)? a0->v_refs: 0));
	return 0;
}

/* -------------------------------------------------------------------------- */

static int fnc_size (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* similar to length, but it returns the ubound + 1 for the array */
	return hawk_fnc_length (rtx, fi, 1);
}

static int fnc_length (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return hawk_fnc_length (rtx, fi, 0);
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
			hawk_rtx_freeval(rtx, tmp, 0);
			return -1;
		}
	}

	hawk_rtx_setretval(rtx, tmp);
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
				hawk_rtx_freeval(rtx, tmp, 0);
				return -1;
			}
		}

		v = (++i >= nargs)? hawk_val_nil: hawk_rtx_getarg(rtx, i);
		v = hawk_rtx_setmapvalfld(rtx, tmp, out.u.cpldup.ptr, out.u.cpldup.len, v);
		if (out.u.cpldup.ptr != idxbuf) hawk_rtx_freemem(rtx, out.u.cpldup.ptr);

		if (HAWK_UNLIKELY(!v))
		{
			hawk_rtx_freeval(rtx, tmp, 0);
			return -1;
		}

		/* if (i >= nargs) break;  this check is probably not needed. another i++ in the 3rd segment of the for statement should be mostly harmless. potential overflow issue is not a real issue as the number of arguments can be so high. */
	}

	hawk_rtx_setretval(rtx, tmp);
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

	hawk_rtx_setretval(rtx, r);
	return 0;
}

static int fnc_ismap (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;

	a0 = hawk_rtx_getarg(rtx, 0);

	r = hawk_rtx_makeintval(rtx, HAWK_RTX_GETVALTYPE(rtx, a0) == HAWK_VAL_MAP);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval(rtx, r);
	return 0;
}

static int fnc_isarr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;

	a0 = hawk_rtx_getarg(rtx, 0);

	r = hawk_rtx_makeintval(rtx, HAWK_RTX_GETVALTYPE(rtx, a0) == HAWK_VAL_ARR);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval(rtx, r);
	return 0;
}

static int fnc_modlibdirs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_val_t* r;

	r = hawk_rtx_makestrvalwithoocstr(rtx, (hawk->opt.mod[0].len > 0)? (const hawk_ooch_t*)hawk->opt.mod[0].ptr: (const hawk_ooch_t*)HAWK_T(HAWK_DEFAULT_MODLIBDIRS));
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval(rtx, r);
	return 0;
}

static int fnc_type (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;
	int _type;

	a0 = hawk_rtx_getarg(rtx, 0);
	_type = hawk_rtx_getvaltype(rtx, a0);

	r = hawk_rtx_makeintval(rtx, _type);
	if (HAWK_UNLIKELY(!r)) return -1;

	hawk_rtx_setretval(rtx, r);
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

	hawk_rtx_setretval(rtx, r);
	return 0;
}

/* -------------------------------------------------------------------------- */

static int fnc_hash (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_int_t v;
	const hawk_ooch_t* name;

	a0 = hawk_rtx_getarg(rtx, 0);
	v = hawk_rtx_hashval(rtx, a0); /* ignore v <= -1 which is an error */

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, v));
	return 0;
}

/* -------------------------------------------------------------------------- */

#define A_MAX HAWK_TYPE_MAX(hawk_oow_t)

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("array"),            { { 0, A_MAX, HAWK_NULL     },  fnc_array,                 0 } },
	{ HAWK_T("call"),             { { 1, A_MAX, HAWK_T("vR")  },  fnc_call,                  0 } },
	{ HAWK_T("cmgr_exists"),      { { 1, 1,     HAWK_NULL     },  fnc_cmgr_exists,           0 } },
	{ HAWK_T("function_exists"),  { { 1, 1,     HAWK_NULL     },  fnc_function_exists,       0 } },
	{ HAWK_T("gc"),               { { 0, 1,     HAWK_NULL     },  fnc_gc,                    0 } },
	{ HAWK_T("gc_get_pressure"),  { { 1, 1,     HAWK_NULL     },  fnc_gc_get_pressure,       0 } },
	{ HAWK_T("gc_get_threshold"), { { 1, 1,     HAWK_NULL     },  fnc_gc_get_threshold,      0 } },
	{ HAWK_T("gc_set_threshold"), { { 2, 2,     HAWK_NULL     },  fnc_gc_set_threshold,      0 } },
	{ HAWK_T("gcrefs"),           { { 1, 1,     HAWK_NULL     },  fnc_gcrefs,                0 } },
	{ HAWK_T("hash"),             { { 1, 1,     HAWK_NULL     },  fnc_hash,                  0 } },
	{ HAWK_T("isarray"),          { { 1, 1,     HAWK_NULL     },  fnc_isarr,                 0 } },
	{ HAWK_T("ismap"),            { { 1, 1,     HAWK_NULL     },  fnc_ismap,                 0 } },
	{ HAWK_T("isnil"),            { { 1, 1,     HAWK_NULL     },  fnc_isnil,                 0 } },
	{ HAWK_T("length"),           { { 1, 1,     HAWK_NULL     },  fnc_length,                0 } },
	{ HAWK_T("map"),              { { 0, A_MAX, HAWK_NULL     },  fnc_map,                   0 } },
	{ HAWK_T("modlibdirs"),       { { 0, 0,     HAWK_NULL     },  fnc_modlibdirs,            0 } },
	{ HAWK_T("size"),             { { 1, 1,     HAWK_NULL     },  fnc_size,                  0 } },
	{ HAWK_T("type"),             { { 1, 1,     HAWK_NULL     },  fnc_type,                  0 } },
	{ HAWK_T("typename"),         { { 1, 1,     HAWK_NULL     },  fnc_typename,              0 } }
};

static hawk_mod_int_tab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("GC_NUM_GENS"), { HAWK_GC_NUM_GENS } },

	/* synchronize this table with enum hawk_val_type_t in hawk.h */
	/* the names follow the val_type_name table in val.c */
	{ HAWK_T("VAL_ARRAY"),  { HAWK_VAL_ARR } },
	{ HAWK_T("VAL_BCHAR"),  { HAWK_VAL_BCHR } },
	{ HAWK_T("VAL_CHAR"),   { HAWK_VAL_CHAR } },
	{ HAWK_T("VAL_FLT"),    { HAWK_VAL_FLT } },
	{ HAWK_T("VAL_FUN"),    { HAWK_VAL_FUN } },
	{ HAWK_T("VAL_INT"),    { HAWK_VAL_INT } },
	{ HAWK_T("VAL_MAP"),    { HAWK_VAL_MAP } },
	{ HAWK_T("VAL_MBS"),    { HAWK_VAL_MBS } },
	{ HAWK_T("VAL_NIL"),    { HAWK_VAL_NIL } },
	{ HAWK_T("VAL_STR"),    { HAWK_VAL_STR } },

	{ HAWK_T("VAL_REF"),    { HAWK_VAL_REF } },
	{ HAWK_T("VAL_REX"),    { HAWK_VAL_REX } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	if (hawk_findmodsymfnc_noseterr(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym) >= 0) return 0;
	return hawk_findmodsymint(hawk, inttab, HAWK_COUNTOF(inttab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* nothing to do */
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* nothing to do */
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	mod_data_t* md = (mod_data_t*)mod->ctx;

	hawk_mtx_fini(&md->mq_mtx);
	hawk_freemem(hawk, md);
}

int hawk_mod_hawk (hawk_mod_t* mod, hawk_t* hawk)
{
	mod_data_t* md;

	md = hawk_allocmem(hawk, HAWK_SIZEOF(*md));
	if (HAWK_UNLIKELY(!md)) return -1;

	hawk_mtx_init(&md->mq_mtx, hawk_getgem(hawk));

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;
	mod->ctx = md;

	return 0;
}
