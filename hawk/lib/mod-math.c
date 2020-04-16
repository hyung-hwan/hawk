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

#include "mod-math.h"
#include "hawk-prv.h"

#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#if defined(HAVE_QUADMATH_H)
#	include <quadmath.h>
#elif defined(HAWK_USE_AWK_FLTMAX) && (HAWK_SIZEOF_FLT_T == 16) && defined(HAWK_FLTMAX_REQUIRE_QUADMATH)
#	error QUADMATH.H NOT AVAILABLE or NOT COMPILABLE
#endif

#if !defined(HAWK_HAVE_CFG_H)
#	if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#		define HAVE_CEIL
#		define HAVE_FLOOR
#		if !defined(__WATCOMC__) && !defined(__BORLANDC__)
#		define HAVE_ROUND
#		endif
#		define HAVE_SINH
#		define HAVE_COSH
#		define HAVE_TANH
#		define HAVE_ASIN
#		define HAVE_ACOS

#		define HAVE_SIN
#		define HAVE_COS
#		define HAVE_TAN
#		define HAVE_ATAN
#		define HAVE_ATAN2
#		define HAVE_LOG
#		define HAVE_LOG10
#		define HAVE_EXP
#		define HAVE_SQRT
#	endif
#endif
typedef struct modctx_t
{
	unsigned int seed;
#if defined(HAVE_INITSTATE_R) && defined(HAVE_SRANDOM_R) && defined(HAVE_RANDOM_R)
	struct random_data prand;
	char prand_bin[256]; /* or hawk_uint8_t? */
#endif
} modctx_t;

static int fnc_math_1 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi, hawk_math1_t f)
{
	hawk_oow_t nargs;
	hawk_val_t* a0;
	hawk_flt_t rv;
	hawk_val_t* r;
	int n;

	nargs = hawk_rtx_getnargs (rtx);
	HAWK_ASSERT (nargs == 1);

	a0 = hawk_rtx_getarg(rtx, 0);

	n = hawk_rtx_valtoflt(rtx, a0, &rv);
	if (n <= -1) return -1;

	r = hawk_rtx_makefltval (rtx, f(hawk_rtx_gethawk(rtx), rv));
	if (r == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_math_2 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi, hawk_math2_t f)
{
	hawk_oow_t nargs;
	hawk_val_t* a0, * a1;
	hawk_flt_t rv0, rv1;
	hawk_val_t* r;
	int n;

	nargs = hawk_rtx_getnargs (rtx);
	HAWK_ASSERT (nargs == 2);

	a0 = hawk_rtx_getarg(rtx, 0);
	a1 = hawk_rtx_getarg(rtx, 1);

	n = hawk_rtx_valtoflt(rtx, a0, &rv0);
	if (n <= -1) return -1;

	n = hawk_rtx_valtoflt (rtx, a1, &rv1);
	if (n <= -1) return -1;

	r = hawk_rtx_makefltval (rtx, f(hawk_rtx_gethawk(rtx), rv0, rv1));
	if (r == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}


static hawk_flt_t math_ceil (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_CEILQ)
	return ceilq (x);
#elif defined(HAVE_CEILL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return ceill (x);
#elif defined(HAVE_CEIL)
	return ceil (x);
#elif defined(HAVE_CEILF)
	return ceilf (x);
#else
	#error ### no ceil function available ###
#endif
}

static hawk_flt_t math_floor (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_FLOORQ)
	return floorq (x);
#elif defined(HAVE_FLOORL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return floorl (x);
#elif defined(HAVE_FLOOR)
	return floor (x);
#elif defined(HAVE_FLOORF)
	return floorf (x);
#else
	#error ### no floor function available ###
#endif
}

static hawk_flt_t math_round (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_ROUNDQ)
	return roundq (x);
#elif defined(HAVE_ROUNDL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return roundl (x);
#elif defined(HAVE_ROUND)
	return round (x);
#elif defined(HAVE_ROUNDF)
	return roundf (x);
#else

	hawk_flt_t f, d;

	f = math_floor (hawk, x);
	d = x - f; /* get fraction */

	if (d > (hawk_flt_t)0.5)
	{
		/* round up to the nearest */
		f =  f + (hawk_flt_t)1.0;
	}
	else if (d == (hawk_flt_t)0.5)
	{
	#if 1
		/* round half away from zero */
		if (x >= 0)
		{
			f = x + (hawk_flt_t)0.5;
		}
		else
		{
			f = x - (hawk_flt_t)0.5;
		}
	#else
		/* round half to even - C99's rint() does this, i guess. */
		d = f - (hawk_flt_t)2.0 * math_floor(hawk, f * (hawk_flt_t)0.5);
		if (d == (hawk_flt_t)1.0) f =  f + (hawk_flt_t)1.0;
	#endif
	}

	/* this implementation doesn't keep the signbit for -0.0.
	 * The signbit() function defined in C99 may get used to
	 * preserve the sign bit. but this is a fall-back rountine
	 * for a system without round also defined in C99. 
	 * don't get annoyed by the lost sign bit for the value of 0.0.
	 */

	return f;
	
#endif
}

static hawk_flt_t math_sinh (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_SINHQ)
	return sinhq (x);
#elif defined(HAVE_SINHL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return sinhl (x);
#elif defined(HAVE_SINH)
	return sinh (x);
#elif defined(HAVE_SINHF)
	return sinhf (x);
#else
	#error ### no sinh function available ###
#endif
}

static hawk_flt_t math_cosh (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_COSHQ)
	return coshq (x);
#elif defined(HAVE_COSHL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return coshl (x);
#elif defined(HAVE_COSH)
	return cosh (x);
#elif defined(HAVE_COSHF)
	return coshf (x);
#else
	#error ### no cosh function available ###
#endif
}

static hawk_flt_t math_tanh (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_TANHQ)
	return tanhq (x);
#elif defined(HAVE_TANHL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return tanhl (x);
#elif defined(HAVE_TANH)
	return tanh (x);
#elif defined(HAVE_TANHF)
	return tanhf (x);
#else
	#error ### no tanh function available ###
#endif
}

static hawk_flt_t math_asin (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_ASINQ)
	return asinq (x);
#elif defined(HAVE_ASINL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return asinl (x);
#elif defined(HAVE_ASIN)
	return asin (x);
#elif defined(HAVE_ASINF)
	return asinf (x);
#else
	#error ### no asin function available ###
#endif
}

static hawk_flt_t math_acos (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_ACOSQ)
	return acosq (x);
#elif defined(HAVE_ACOSL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return acosl (x);
#elif defined(HAVE_ACOS)
	return acos (x);
#elif defined(HAVE_ACOSF)
	return acosf (x);
#else
	#error ### no acos function available ###
#endif
}

/* ----------------------------------------------------------------------- */


static hawk_flt_t math_sin (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_SINQ)
	return sinq (x);
#elif defined(HAVE_SINL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return sinl (x);
#elif defined(HAVE_SIN)
	return sin (x);
#elif defined(HAVE_SINF)
	return sinf (x);
#else
	#error ### no sin function available ###
#endif
}

static hawk_flt_t math_cos (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_COSQ)
	return cosq (x);
#elif defined(HAVE_COSL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return cosl (x);
#elif defined(HAVE_COS)
	return cos (x);
#elif defined(HAVE_COSF)
	return cosf (x);
#else
	#error ### no cos function available ###
#endif
}

static hawk_flt_t math_tan (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_TANQ)
	return tanq (x);
#elif defined(HAVE_TANL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return tanl (x);
#elif defined(HAVE_TAN)
	return tan (x);
#elif defined(HAVE_TANF)
	return tanf (x);
#else
	#error ### no tan function available ###
#endif
}

static hawk_flt_t math_atan (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_ATANQ)
	return atanq (x);
#elif defined(HAVE_ATANL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return atanl (x);
#elif defined(HAVE_ATAN)
	return atan (x);
#elif defined(HAVE_ATANF)
	return atanf (x);
#else
	#error ### no atan function available ###
#endif
}

static hawk_flt_t math_atan2 (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_ATAN2Q)
	return atan2q (x, y);
#elif defined(HAVE_ATAN2L) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return atan2l (x, y);
#elif defined(HAVE_ATAN2)
	return atan2 (x, y);
#elif defined(HAVE_ATAN2F)
	return atan2f (x, y);
#else
	#error ### no atan2 function available ###
#endif
}

static HAWK_INLINE hawk_flt_t math_log (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_LOGQ)
	return logq (x);
#elif defined(HAVE_LOGL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return logl (x);
#elif defined(HAVE_LOG)
	return log (x);
#elif defined(HAVE_LOGF)
	return logf (x);
#else
	#error ### no log function available ###
#endif
}

static hawk_flt_t math_log2 (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_LOG2Q)
	return log2q (x);
#elif defined(HAVE_LOG2L) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return log2l (x);
#elif defined(HAVE_LOG2)
	return log2 (x);
#elif defined(HAVE_LOG2F)
	return log2f (x);
#else
	return math_log(hawk, x) / math_log(hawk, 2.0);
#endif
}

static hawk_flt_t math_log10 (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_LOG10Q)
	return log10q (x);
#elif defined(HAVE_LOG10L) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return log10l (x);
#elif defined(HAVE_LOG10)
	return log10 (x);
#elif defined(HAVE_LOG10F)
	return log10f (x);
#else
	#error ### no log10 function available ###
#endif
}

static hawk_flt_t math_exp (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_EXPQ)
	return expq (x);
#elif defined(HAVE_EXPL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return expl (x);
#elif defined(HAVE_EXP)
	return exp (x);
#elif defined(HAVE_EXPF)
	return expf (x);
#else
	#error ### no exp function available ###
#endif
}

static hawk_flt_t math_sqrt (hawk_t* hawk, hawk_flt_t x)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_SQRTQ)
	return sqrtq (x);
#elif defined(HAVE_SQRTL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return sqrtl (x);
#elif defined(HAVE_SQRT)
	return sqrt (x);
#elif defined(HAVE_SQRTF)
	return sqrtf (x);
#else
	#error ### no sqrt function available ###
#endif
}

/* ----------------------------------------------------------------------- */

static int fnc_ceil (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_ceil);
}

static int fnc_floor (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_floor);
}

static int fnc_round (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_round);
}

static int fnc_sinh (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_sinh);
}

static int fnc_cosh (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_cosh);
}

static int fnc_tanh (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_tanh);
}

static int fnc_asin (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_asin);
}

static int fnc_acos (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_acos);
}


/* ----------------------------------------------------------------------- */

static int fnc_sin (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_sin);
}

static int fnc_cos (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_cos);
}

static int fnc_tan (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_tan);
}

static int fnc_atan (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_atan);
}

static int fnc_atan2 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_2 (rtx, fi, math_atan2);
}

static int fnc_log (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_log);
}

static int fnc_log2 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_log2);
}

static int fnc_log10 (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_log10);
}

static int fnc_exp (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_exp);
}

static int fnc_sqrt (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return fnc_math_1 (rtx, fi, math_sqrt);
}

/* ----------------------------------------------------------------------- */

static int fnc_rand (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
/*#define RANDV_MAX HAWK_TYPE_MAX(hawk_int_t)*/
#define RANDV_MAX RAND_MAX
	hawk_val_t* r;
	hawk_int32_t randv;
	modctx_t* modctx;

	modctx = (modctx_t*)fi->mod->ctx;
#if defined(HAVE_INITSTATE_R) && defined(HAVE_SRANDOM_R) && defined(HAVE_RANDOM_R)
	random_r (&modctx->prand, &randv);
#elif defined(HAVE_RANDOM)
	randv = random();
#else
	randv = rand();
#endif

	r = hawk_rtx_makefltval(rtx, (hawk_flt_t)randv / RANDV_MAX);
	if (r == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
#undef RANDV_MAX
}

static int fnc_srand (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_oow_t nargs;
	hawk_val_t* a0;
	hawk_int_t lv;
	hawk_val_t* r;
	int n;
	hawk_int_t prev;
	modctx_t* modctx;

	modctx = (modctx_t*)fi->mod->ctx;
	nargs = hawk_rtx_getnargs(rtx);
	HAWK_ASSERT (nargs == 0 || nargs == 1);

	prev = modctx->seed;

	if (nargs <= 0)
	{
		hawk_ntime_t tv;
		hawk_get_time (&tv);
		modctx->seed = tv.sec + tv.nsec;
	#if defined(HAVE_INITSTATE_R) && defined(HAVE_SRANDOM_R) && defined(HAVE_RANDOM_R)
		srandom_r (modctx->seed, &modctx->prand);
	#elif defined(HAVE_RANDOM)
		srandom (modctx->seed);
	#else
		srand (modctx->seed);
	#endif
	}
	else
	{
		a0 = hawk_rtx_getarg(rtx, 0);
		n = hawk_rtx_valtoint(rtx, a0, &lv);
		if (n <= -1) return -1;
	#if defined(HAVE_INITSTATE_R) && defined(HAVE_SRANDOM_R) && defined(HAVE_RANDOM_R)
		srandom_r (lv, &modctx->prand);
	#elif defined(HAVE_RANDOM)
		srandom (lv);
	#else
		srand (lv);
	#endif
	}
	
	r = hawk_rtx_makeintval (rtx, prev);
	if (r == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, r);
	return 0;
}

/* ----------------------------------------------------------------------- */

typedef struct fnctab_t fnctab_t;
struct fnctab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_fnc_t info;
};

static fnctab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("acos"),    { { 1, 1, HAWK_NULL },     fnc_acos,       0 } },
	{ HAWK_T("asin"),    { { 1, 1, HAWK_NULL },     fnc_asin,       0 } },
	{ HAWK_T("atan"),    { { 1, 1, HAWK_NULL },     fnc_atan,       0 } },
	{ HAWK_T("atan2"),   { { 2, 2, HAWK_NULL },     fnc_atan2,      0 } },
	{ HAWK_T("ceil"),    { { 1, 1, HAWK_NULL },     fnc_ceil,       0 } },
	{ HAWK_T("cos"),     { { 1, 1, HAWK_NULL },     fnc_cos,        0 } },
	{ HAWK_T("cosh"),    { { 1, 1, HAWK_NULL },     fnc_cosh,       0 } },
	{ HAWK_T("exp"),     { { 1, 1, HAWK_NULL },     fnc_exp,        0 } },
	{ HAWK_T("floor"),   { { 1, 1, HAWK_NULL },     fnc_floor,      0 } },
	{ HAWK_T("log"),     { { 1, 1, HAWK_NULL },     fnc_log,        0 } },
	{ HAWK_T("log10"),   { { 1, 1, HAWK_NULL },     fnc_log10,      0 } },
	{ HAWK_T("log2"),    { { 1, 1, HAWK_NULL },     fnc_log2,       0 } },
	{ HAWK_T("rand"),    { { 0, 0, HAWK_NULL },     fnc_rand,       0 } },
	{ HAWK_T("round"),   { { 1, 1, HAWK_NULL },     fnc_round,      0 } },
	{ HAWK_T("sin"),     { { 1, 1, HAWK_NULL },     fnc_sin,        0 } },
	{ HAWK_T("sinh"),    { { 1, 1, HAWK_NULL },     fnc_sinh,       0 } },
	{ HAWK_T("sqrt"),    { { 1, 1, HAWK_NULL },     fnc_sqrt,       0 } },
	{ HAWK_T("srand"),   { { 0, 1, HAWK_NULL },     fnc_srand,      0 } },
	{ HAWK_T("tan"),     { { 1, 1, HAWK_NULL },     fnc_tan,        0 } },
	{ HAWK_T("tanh"),    { { 1, 1, HAWK_NULL },     fnc_tanh,       0 } }
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

#if 0
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
#endif

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
	modctx_t* modctx;

	modctx = (modctx_t*)mod->ctx;
	hawk_freemem (hawk, modctx);
}

int hawk_mod_math (hawk_mod_t* mod, hawk_t* hawk)
{
	modctx_t* modctx;
	hawk_ntime_t tv;

	modctx = hawk_allocmem(hawk, HAWK_SIZEOF(*modctx));
	if (modctx == HAWK_NULL) return -1;

	HAWK_MEMSET (modctx, 0, HAWK_SIZEOF(*modctx));

	hawk_get_time (&tv);
	modctx->seed = tv.sec + tv.nsec;
#if defined(HAVE_INITSTATE_R) && defined(HAVE_SRANDOM_R) && defined(HAVE_RANDOM_R)
	initstate_r (0, modctx->prand_bin, HAWK_SIZEOF(modctx->prand_bin), &modctx->prand);
	srandom_r (modctx->seed, &modctx->prand);
#elif defined(HAVE_RANDOM)
	srandom (modctx->seed);
#else
	srand (modctx->seed);
#endif

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;
	mod->ctx = modctx;

	return 0;
}

