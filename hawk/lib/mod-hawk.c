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

/*
   hawk::gc();
   hawk::gc_get_threshold(gen)
   hawk::gc_set_threshold(gen, threshold)
   hawk::GC_NUM_GENS
 */

static int fnc_gc (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t gen = -1;

	if (hawk_rtx_getnargs(rtx) >= 1 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &gen) <= -1) gen = -1;
	gen = hawk_rtx_gc(rtx, gen);

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(gen));
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

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(threshold));
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
		if (threshold >= HAWK_QUICKINT_MAX) threshold = HAWK_QUICKINT_MAX;
		rtx->gc.threshold[gen] = threshold; /* update */
	}
	else 
	{
		threshold = rtx->gc.threshold[gen]; /* no update. but retrieve the existing value */
	}

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(threshold));
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

	tmp = hawk_rtx_makearrval(rtx, -1);
	if (HAWK_UNLIKELY(!tmp)) return -1; /* hard failure */

/* TODO: take arguments and put them to the array*/
	hawk_rtx_setretval (rtx, tmp);
	return 0;
}

static int fnc_array_size (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_oow_t iv;

	a0 = hawk_rtx_getarg(rtx, 0);

	if (HAWK_RTX_GETVALTYPE(rtx, a0) != HAWK_VAL_ARR)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOTARR);
		return -1;
	}

	iv = HAWK_ARR_SIZE(((hawk_val_arr_t*)a0)->arr);
	HAWK_ASSERT  (iv >= 0 && iv <= HAWK_QUICKINT_MAX);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, iv));
	return 0;
}

static int fnc_array_tally (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_oow_t iv;

	a0 = hawk_rtx_getarg(rtx, 0);

	if (HAWK_RTX_GETVALTYPE(rtx, a0) != HAWK_VAL_ARR)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOTARR);
		return -1;
	}

	iv = HAWK_ARR_TALLY(((hawk_val_arr_t*)a0)->arr);
	HAWK_ASSERT  (iv >= 0 && iv <= HAWK_QUICKINT_MAX);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, iv));
	return 0;
}

static int fnc_map (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* tmp;

	tmp = hawk_rtx_makemapval(rtx);
	if (HAWK_UNLIKELY(!tmp)) return -1; /* hard failure */

/* TODO: take arguments and put them to the map */
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
	if (r == HAWK_NULL) return -1;

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

static int fnc_typename (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* a0;
	hawk_val_t* r;
	const hawk_ooch_t* name;

	a0 = hawk_rtx_getarg(rtx, 0);
	name = hawk_rtx_getvaltypename(rtx, a0);

	r = hawk_rtx_makestrvalwithoocstr(rtx, name);
	if (r == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, r);
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
	{ HAWK_T("array"),            { { 0, A_MAX                },  fnc_array,                 0 } },
	{ HAWK_T("array_size"),       { { 1, 1                    },  fnc_array_size,            0 } },
	{ HAWK_T("array_tally"),      { { 1, 1                    },  fnc_array_tally,           0 } },

	{ HAWK_T("call"),             { { 1, A_MAX, HAWK_T("vR")  },  fnc_call,                  0 } },
	{ HAWK_T("function_exists"),  { { 1, 1,     HAWK_NULL     },  fnc_function_exists,       0 } },
	{ HAWK_T("gc"),               { { 0, 1,     HAWK_NULL     },  fnc_gc,                    0 } },
	{ HAWK_T("gcrefs"),           { { 1, 1,     HAWK_NULL     },  fnc_gcrefs,                0 } },
	{ HAWK_T("gc_get_threshold"), { { 1, 1,     HAWK_NULL     },  fnc_gc_get_threshold,      0 } },
	{ HAWK_T("gc_set_threshold"), { { 2, 2,     HAWK_NULL     },  fnc_gc_set_threshold,      0 } },
	{ HAWK_T("isarray"),          { { 1, 1,     HAWK_NULL     },  fnc_isarr,                 0 } },
	{ HAWK_T("ismap"),            { { 1, 1,     HAWK_NULL     },  fnc_ismap,                 0 } },
	{ HAWK_T("isnil"),            { { 1, 1,     HAWK_NULL     },  fnc_isnil,                 0 } },
	{ HAWK_T("map"),              { { 0, A_MAX, HAWK_NULL     },  fnc_map,                   0 } },
	{ HAWK_T("typename"),         { { 1, 1,     HAWK_NULL     },  fnc_typename,              0 } }
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
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* TODO: anything */
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	/* TODO: anything */
}

int hawk_mod_hawk (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;
	/*
	mod->ctx...
	 */

	return 0;
}

