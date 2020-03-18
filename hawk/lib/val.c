/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#include "hawk-prv.h"

#define CHUNKSIZE HAWK_VAL_CHUNK_SIZE

static hawk_val_nil_t awk_nil = { HAWK_VAL_NIL, 0, 1, 0, 0 };
static hawk_val_str_t awk_zls = { HAWK_VAL_STR, 0, 1, 0, 0, { HAWK_T(""), 0 } };
static hawk_val_mbs_t awk_zlm = { HAWK_VAL_MBS, 0, 1, 0, 0, { HAWK_BT(""), 0 } };

hawk_val_t* hawk_val_nil = (hawk_val_t*)&awk_nil;
hawk_val_t* hawk_val_zls = (hawk_val_t*)&awk_zls; 
hawk_val_t* hawk_val_zlm = (hawk_val_t*)&awk_zlm;


/* --------------------------------------------------------------------- */

/* TOOD: */
static hawk_val_t* gc_calloc (hawk_rtx_t* rtx, hawk_oow_t size)
{
	hawk_gc_info_t* gci;

	gci = (hawk_gc_info_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*gci) + size);
	if (HAWK_UNLIKELY(!gci)) return HAWK_NULL;

	return hawk_gcinfo_to_val(gci);
}

/* --------------------------------------------------------------------- */

hawk_val_t* hawk_get_awk_nil_val (void)
{
	return (hawk_val_t*)&awk_nil;
}

int hawk_rtx_isnilval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	return val == (hawk_val_t*)&awk_nil || (HAWK_VTR_IS_POINTER(val) && val->v_type == HAWK_VAL_NIL);
}

hawk_val_t* hawk_rtx_makenilval (hawk_rtx_t* rtx)
{
	return (hawk_val_t*)&awk_nil;
}

hawk_val_t* hawk_rtx_makeintval (hawk_rtx_t* rtx, hawk_int_t v)
{
	hawk_val_int_t* val;

	if (HAWK_IN_QUICKINT_RANGE(v)) return HAWK_QUICKINT_TO_VTR(v);

	if (!rtx->vmgr.ifree)
	{
		hawk_val_ichunk_t* c;
		/*hawk_val_int_t* x;*/
		hawk_oow_t i;

		/* use hawk_val_ichunk structure to avoid
		 * any alignment issues on platforms requiring
		 * aligned memory access - using the code commented out
		 * will cause a fault on such a platform */

		/* c = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(hawk_val_chunk_t) + HAWK_SIZEOF(hawk_val_int_t)*CHUNKSIZE); */
		c = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(hawk_val_ichunk_t));
		if (!c) return HAWK_NULL;
		
		c->next = rtx->vmgr.ichunk;
		/*run->vmgr.ichunk = c;*/
		rtx->vmgr.ichunk = (hawk_val_chunk_t*)c;

		/*x = (hawk_val_int_t*)(c + 1);
		for (i = 0; i < CHUNKSIZE-1; i++) 
			x[i].nde = (hawk_nde_int_t*)&x[i+1];
		x[i].nde = HAWK_NULL;

		run->vmgr.ifree = x;
		*/

		for (i = 0; i < CHUNKSIZE-1; i++)
			c->slot[i].nde = (hawk_nde_int_t*)&c->slot[i+1];
		c->slot[i].nde = HAWK_NULL;

		rtx->vmgr.ifree = &c->slot[0];
	}

	val = rtx->vmgr.ifree;
	rtx->vmgr.ifree = (hawk_val_int_t*)val->nde;

	val->v_type = HAWK_VAL_INT;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->i_val = v;
	val->nde = HAWK_NULL;

#if defined(DEBUG_VAL)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("makeintval => %jd [%p] - [%O]\n"), (hawk_intmax_t)v, val, val);
#endif
	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makefltval (hawk_rtx_t* rtx, hawk_flt_t v)
{
	hawk_val_flt_t* val;

	if (rtx->vmgr.rfree == HAWK_NULL)
	{
		hawk_val_rchunk_t* c;
		/*hawk_val_flt_t* x;*/
		hawk_oow_t i;

		/* c = hawk_rtx_allocmem (rtx, HAWK_SIZEOF(hawk_val_chunk_t) + HAWK_SIZEOF(hawk_val_flt_t) * CHUNKSIZE); */
		c = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(hawk_val_rchunk_t));
		if (!c) return HAWK_NULL;

		c->next = rtx->vmgr.rchunk;
		/*run->vmgr.rchunk = c;*/
		rtx->vmgr.rchunk = (hawk_val_chunk_t*)c;

		/*
		x = (hawk_val_flt_t*)(c + 1);
		for (i = 0; i < CHUNKSIZE-1; i++) 
			x[i].nde = (hawk_nde_flt_t*)&x[i+1];
		x[i].nde = HAWK_NULL;

		run->vmgr.rfree = x;
		*/

		for (i = 0; i < CHUNKSIZE-1; i++)
			c->slot[i].nde = (hawk_nde_flt_t*)&c->slot[i+1];
		c->slot[i].nde = HAWK_NULL;

		rtx->vmgr.rfree = &c->slot[0];
	}

	val = rtx->vmgr.rfree;
	rtx->vmgr.rfree = (hawk_val_flt_t*)val->nde;

	val->v_type = HAWK_VAL_FLT;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->val = v;
	val->nde = HAWK_NULL;

#if defined(DEBUG_VAL)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("makefltval => %Lf [%p] - [%O]\n"), (double)v, val, val);
#endif
	return (hawk_val_t*)val;
}

static HAWK_INLINE hawk_val_t* make_str_val (hawk_rtx_t* rtx, const hawk_ooch_t* str1, hawk_oow_t len1, const hawk_ooch_t* str2, hawk_oow_t len2)
{
	hawk_val_str_t* val = HAWK_NULL;
	hawk_oow_t aligned_len;
#if defined(ENABLE_FEATURE_SCACHE)
	hawk_oow_t i;
#endif

	if (HAWK_UNLIKELY(len1 <= 0 && len2 <= 0)) return hawk_val_zls;
	aligned_len = HAWK_ALIGN_POW2((len1 + len2 + 1), FEATURE_SCACHE_BLOCK_UNIT);

#if defined(ENABLE_FEATURE_SCACHE)
	i = aligned_len / FEATURE_SCACHE_BLOCK_UNIT;
	if (i < HAWK_COUNTOF(rtx->scache_count))
	{
		if (rtx->scache_count[i] > 0)
		{
			val = rtx->scache[i][--rtx->scache_count[i]];
			goto init;
		}
	}
#endif

	val = (hawk_val_str_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_str_t) + (aligned_len * HAWK_SIZEOF(hawk_ooch_t)));
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

#if defined(ENABLE_FEATURE_SCACHE)
init:
#endif
	val->v_type = HAWK_VAL_STR;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->val.len = len1 + len2;
	val->val.ptr = (hawk_ooch_t*)(val + 1);
	if (HAWK_LIKELY(str1)) hawk_copy_oochars_to_oocstr_unlimited (&val->val.ptr[0], str1, len1);
	if (str2) hawk_copy_oochars_to_oocstr_unlimited (&val->val.ptr[len1], str2, len2);
	val->val.ptr[val->val.len] = '\0';

#if defined(DEBUG_VAL)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("make_str_val => %p - [%O]\n"), val, val);
#endif
	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makestrvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t len)
{
#if defined(HAWK_OOCH_IS_UCH)
	return make_str_val(rtx, ucs, len, HAWK_NULL, 0);
#else
	hawk_val_t* v;
	hawk_bch_t* bcs;
	hawk_oow_t bcslen;

	bcs = hawk_rtx_duputobchars(rtx, ucs, len, &bcslen);
	if (HAWK_UNLIKELY(!bcs)) return HAWK_NULL;

	v = make_str_val(rtx, bcs, bcslen, HAWK_NULL, 0);
	hawk_rtx_freemem (rtx, bcs);
	return v;
#endif
}

hawk_val_t* hawk_rtx_makestrvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t len)
{
#if defined(HAWK_OOCH_IS_UCH)
	hawk_val_t* v;
	hawk_uch_t* ucs;
	hawk_oow_t ucslen;

	ucs = hawk_rtx_dupbtouchars(rtx, bcs, len, &ucslen, 1);
	if (HAWK_UNLIKELY(!ucs)) return HAWK_NULL;

	v = make_str_val(rtx, ucs, ucslen, HAWK_NULL, 0);
	hawk_rtx_freemem (rtx, ucs);
	return v;
#else
	return make_str_val(rtx, bcs, len, HAWK_NULL, 0);
#endif
}

hawk_val_t* hawk_rtx_makestrvalwithbcstr (hawk_rtx_t* rtx, const hawk_bch_t* bcs)
{
	return hawk_rtx_makestrvalwithbchars(rtx, bcs, hawk_count_bcstr(bcs));
}

hawk_val_t* hawk_rtx_makestrvalwithucstr (hawk_rtx_t* rtx, const hawk_uch_t* ucs)
{
	return hawk_rtx_makestrvalwithuchars(rtx, ucs, hawk_count_ucstr(ucs));
}

hawk_val_t* hawk_rtx_makestrvalwithucs (hawk_rtx_t* rtx, const hawk_ucs_t* ucs)
{
	return hawk_rtx_makestrvalwithuchars(rtx, ucs->ptr, ucs->len);
}

hawk_val_t* hawk_rtx_makestrvalwithbcs (hawk_rtx_t* rtx, const hawk_bcs_t* bcs)
{
	return hawk_rtx_makestrvalwithbchars(rtx, bcs->ptr, bcs->len);
}

hawk_val_t* hawk_rtx_makestrvalwithuchars2 (hawk_rtx_t* rtx, const hawk_uch_t* ucs1, hawk_oow_t len1, const hawk_uch_t* ucs2, hawk_oow_t len2)
{
#if defined(HAWK_OOCH_IS_UCH)
	return make_str_val(rtx, ucs1, len1, ucs2, len2);
#else
	hawk_val_t* v;
	hawk_bch_t* bcs;
	hawk_oow_t bcslen;

	bcs = hawk_rtx_dupu2tobchars(rtx, ucs1, len1, ucs2, len2, &bcslen);
	if (HAWK_UNLIKELY(!bcs)) return HAWK_NULL;

	v = make_str_val(rtx, bcs, bcslen, HAWK_NULL, 0);
	hawk_rtx_freemem (rtx, bcs);
	return v;
#endif
}

hawk_val_t* hawk_rtx_makestrvalwithbchars2 (hawk_rtx_t* rtx, const hawk_bch_t* bcs1, hawk_oow_t len1, const hawk_bch_t* bcs2, hawk_oow_t len2)
{
#if defined(HAWK_OOCH_IS_UCH)
	hawk_val_t* v;
	hawk_uch_t* ucs;
	hawk_oow_t ucslen;

	ucs = hawk_rtx_dupb2touchars(rtx, bcs1, len1, bcs2, len2, &ucslen, 1);
	if (HAWK_UNLIKELY(!ucs)) return HAWK_NULL;

	v = make_str_val(rtx, ucs, ucslen, HAWK_NULL, 0);
	hawk_rtx_freemem (rtx, ucs);
	return v;
#else
	return make_str_val(rtx, bcs1, len1, bcs2, len2);
#endif
}

/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makenumorstrvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ptr, hawk_oow_t len)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	x = hawk_uchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
	if (x == 0) return hawk_rtx_makeintval(rtx, l);
	else if (x >= 1) return hawk_rtx_makefltval(rtx, r);

	return hawk_rtx_makestrvalwithuchars(rtx, ptr, len);
}

hawk_val_t* hawk_rtx_makenumorstrvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	x = hawk_bchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
	if (x == 0) return hawk_rtx_makeintval(rtx, l);
	else if (x >= 1) return hawk_rtx_makefltval(rtx, r);

	return hawk_rtx_makestrvalwithbchars(rtx, ptr, len);
}


/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makenstrvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ptr, hawk_oow_t len)
{
	int x;
	hawk_val_t* v;
	hawk_int_t l;
	hawk_flt_t r;

	x = hawk_uchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
	v = hawk_rtx_makestrvalwithuchars(rtx, ptr, len);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	if (x >= 0) 
	{
		/* set the numeric string flag if a string
		 * can be converted to a number */
		HAWK_ASSERT (x == 0 || x == 1);
		v->nstr = x + 1; /* long -> 1, real -> 2 */
	}

	return v;
}

hawk_val_t* hawk_rtx_makenstrvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len)
{
	int x;
	hawk_val_t* v;
	hawk_int_t l;
	hawk_flt_t r;

	x = hawk_bchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
	v = hawk_rtx_makestrvalwithbchars(rtx, ptr, len);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	if (x >= 0) 
	{
		/* set the numeric string flag if a string
		 * can be converted to a number */
		HAWK_ASSERT (x == 0 || x == 1);
		v->nstr = x + 1; /* long -> 1, real -> 2 */
	}

	return v;
}

hawk_val_t* hawk_rtx_makenstrvalwithucstr (hawk_rtx_t* rtx, const hawk_uch_t* str)
{
	return hawk_rtx_makenstrvalwithuchars(rtx, str, hawk_count_ucstr(str));
}

hawk_val_t* hawk_rtx_makenstrvalwithbcstr (hawk_rtx_t* rtx, const hawk_bch_t* str)
{
	return hawk_rtx_makenstrvalwithbchars(rtx, str, hawk_count_bcstr(str));
}

hawk_val_t* hawk_rtx_makenstrvalwithucs (hawk_rtx_t* rtx, const hawk_ucs_t* str)
{
	return hawk_rtx_makenstrvalwithuchars(rtx, str->ptr, str->len);
}

hawk_val_t* hawk_rtx_makenstrvalwithbcs (hawk_rtx_t* rtx, const hawk_bcs_t* str)
{
	return hawk_rtx_makenstrvalwithbchars(rtx, str->ptr, str->len);
}


/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makembsvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len)
{
	hawk_val_mbs_t* val = HAWK_NULL;
	hawk_oow_t xsz;

	if (HAWK_UNLIKELY(len <= 0)) return hawk_val_zlm;

	xsz = len * HAWK_SIZEOF(*ptr);

	val = (hawk_val_mbs_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_mbs_t) + xsz + HAWK_SIZEOF(*ptr));
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_MBS;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->val.len = len;
	val->val.ptr = (hawk_bch_t*)(val + 1);
	if (ptr) HAWK_MEMCPY (val->val.ptr, ptr, xsz);
	val->val.ptr[len] = HAWK_BT('\0');

	return (hawk_val_t*)val;
}


hawk_val_t* hawk_rtx_makembsvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t len)
{
	hawk_val_t* val;
	hawk_bch_t* bcs;
	hawk_oow_t bcslen;

	if (HAWK_UNLIKELY(len <= 0)) return hawk_val_zlm;

	bcs = hawk_rtx_duputobchars(rtx, ucs, len, &bcslen);
	if (HAWK_UNLIKELY(!bcs)) return HAWK_NULL;

	val = hawk_rtx_makembsvalwithbchars(rtx, bcs, bcslen);
	hawk_rtx_freemem (rtx, bcs);

	return val;

}

hawk_val_t* hawk_rtx_makembsvalwithbcs (hawk_rtx_t* rtx, const hawk_bcs_t* bcs)
{
	return hawk_rtx_makembsvalwithbchars(rtx, bcs->ptr, bcs->len);
}

hawk_val_t* hawk_rtx_makembsvalwithucs (hawk_rtx_t* rtx, const hawk_ucs_t* ucs)
{
	return hawk_rtx_makembsvalwithuchars(rtx, ucs->ptr, ucs->len);
}

/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makenumormbsvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ptr, hawk_oow_t len)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	x = hawk_uchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
	if (x == 0) return hawk_rtx_makeintval(rtx, l);
	else if (x >= 1) return hawk_rtx_makefltval(rtx, r);

	return hawk_rtx_makembsvalwithuchars(rtx, ptr, len);
}

hawk_val_t* hawk_rtx_makenumormbsvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	x = hawk_bchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
	if (x == 0) return hawk_rtx_makeintval(rtx, l);
	else if (x >= 1) return hawk_rtx_makefltval(rtx, r);

	return hawk_rtx_makembsvalwithbchars(rtx, ptr, len);
}

/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makerexval (hawk_rtx_t* rtx, const hawk_oocs_t* str, hawk_tre_t* code[2])
{
	hawk_val_rex_t* val;
	hawk_oow_t totsz;

	/* the regular expression value holds:
	 * - header
	 * - a raw string plus with added a terminating '\0'
	 * the total size is just large enough for all these.
	 */
	totsz = HAWK_SIZEOF(*val) + (HAWK_SIZEOF(*str->ptr) * (str->len + 1));
	val = (hawk_val_rex_t*)hawk_rtx_callocmem(rtx, totsz);
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_REX;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->str.len = str->len;

	val->str.ptr = (hawk_ooch_t*)(val + 1);
	hawk_copy_oochars_to_oocstr_unlimited (val->str.ptr, str->ptr, str->len);

	val->code[0] = code[0];
	val->code[1] = code[1];

	return (hawk_val_t*)val;
}

static void free_mapval (hawk_map_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_t* rtx = *(hawk_rtx_t**)hawk_map_getxtn(map);

#if defined(DEBUG_VAL)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("refdown in map free - [%O]\n"), dptr);
#endif

	hawk_rtx_refdownval (rtx, dptr);
}

static void same_mapval (hawk_map_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_t* run = *(hawk_rtx_t**)hawk_map_getxtn(map);
#if defined(DEBUG_VAL)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("refdown nofree in map free - [%O]\n"), dptr);
#endif
	hawk_rtx_refdownval_nofree (run, dptr);
}

hawk_val_t* hawk_rtx_makemapval (hawk_rtx_t* rtx)
{
	static hawk_map_style_t style =
	{
	/* the key is copied inline into a pair and is freed when the pair
	 * is destroyed. not setting copier for a value means that the pointer 
	 * to the data allocated somewhere else is remembered in a pair. but 
	 * freeing the actual value is handled by free_mapval and same_mapval */
		{
			HAWK_MAP_COPIER_INLINE,
			HAWK_MAP_COPIER_DEFAULT
		},
		{
			HAWK_MAP_FREEER_DEFAULT,
			free_mapval
		},
		HAWK_MAP_COMPER_DEFAULT,
		same_mapval,
	#if defined(HAWK_MAP_IS_HTB)
		HAWK_MAP_SIZER_DEFAULT,
		HAWK_MAP_HASHER_DEFAULT
	#endif
	};
	hawk_val_map_t* val;

	val = (hawk_val_map_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_map_t) + HAWK_SIZEOF(hawk_map_t) + HAWK_SIZEOF(rtx));
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_MAP;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->map = (hawk_map_t*)(val + 1);

	if (hawk_map_init(val->map, hawk_rtx_getgem(rtx), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1)
	{
		hawk_rtx_freemem (rtx, val);
		return HAWK_NULL;
	}
	*(hawk_rtx_t**)hawk_map_getxtn(val->map) = rtx;
	hawk_map_setstyle (val->map, &style);

	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makemapvalwithdata (hawk_rtx_t* rtx, hawk_val_map_data_t data[])
{
	hawk_val_t* map, * tmp;
	hawk_val_map_data_t* p;

	map = hawk_rtx_makemapval(rtx);
	if (HAWK_UNLIKELY(!map)) return HAWK_NULL;

	for (p = data; p->key.ptr; p++)
	{
		switch (p->type)
		{
			case HAWK_VAL_MAP_DATA_INT:
			{
				hawk_int_t iv = 0;

				if (p->type_size > 0 && p->type_size <= HAWK_SIZEOF(iv))
				{
				#if defined(HAWK_ENDIAN_LITTLE)
					HAWK_MEMCPY (&iv, p->vptr, p->type_size);
				#else
					HAWK_MEMCPY ((hawk_uint8_t*)&iv + (HAWK_SIZEOF(iv) - p->type_size), p->vptr, p->type_size);
				#endif
				}
				else
				{
					iv = *(hawk_int_t*)p->vptr;
				}
				tmp = hawk_rtx_makeintval(rtx, iv);
				break;
			}

			case HAWK_VAL_MAP_DATA_FLT:
				tmp = hawk_rtx_makefltval(rtx, *(hawk_flt_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_STR:
				tmp = hawk_rtx_makestrvalwithoocstr(rtx, (hawk_ooch_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_MBS:
				tmp = hawk_rtx_makestrvalwithbcstr(rtx, (hawk_bch_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_WCS:
				tmp = hawk_rtx_makestrvalwithucstr(rtx, (hawk_uch_t*)p->vptr);
				break;
					
			case HAWK_VAL_MAP_DATA_XSTR:
			case HAWK_VAL_MAP_DATA_CSTR:
				tmp = hawk_rtx_makestrvalwithoocs(rtx, (hawk_oocs_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_MXSTR:
			case HAWK_VAL_MAP_DATA_MCSTR:
				tmp = hawk_rtx_makestrvalwithbcs(rtx, (hawk_bcs_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_WXSTR:
			case HAWK_VAL_MAP_DATA_WCSTR:
				tmp = hawk_rtx_makestrvalwithucs(rtx, (hawk_ucs_t*)p->vptr);
				break;

			default:
				tmp = HAWK_NULL;
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
				break;
		}

		if (tmp == HAWK_NULL || hawk_rtx_setmapvalfld (rtx, map, p->key.ptr, p->key.len, tmp) == HAWK_NULL)
		{
			if (tmp) hawk_rtx_freeval (rtx, tmp, 1);
			hawk_rtx_freeval (rtx, map, 1);
			return HAWK_NULL;
		}
	}

	return map;
}

hawk_val_t* hawk_rtx_setmapvalfld (
	hawk_rtx_t* rtx, hawk_val_t* map, 
	const hawk_ooch_t* kptr, hawk_oow_t klen, hawk_val_t* v)
{
	HAWK_ASSERT (HAWK_RTX_GETVALTYPE (rtx, map) == HAWK_VAL_MAP);

	if (hawk_map_upsert (((hawk_val_map_t*)map)->map, (hawk_ooch_t*)kptr, klen, v, 0) == HAWK_NULL) return HAWK_NULL;

	/* the value is passed in by an external party. we can't refup()
	 * and refdown() the value if htb_upsert() fails. that way, the value
	 * can be destroyed if it was passed with the reference count of 0.
	 * so we increment the reference count when htb_upsert() is complete */
	hawk_rtx_refupval (rtx, v);

	return v;
}

hawk_val_t* hawk_rtx_getmapvalfld (hawk_rtx_t* rtx, hawk_val_t* map, const hawk_ooch_t* kptr, hawk_oow_t klen)
{
	hawk_map_pair_t* pair;

	HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);

	pair = hawk_map_search(((hawk_val_map_t*)map)->map, kptr, klen);
	if (!pair)
	{
		/* the given key is not found in the map.
		 * we return NULL here as this function is called by 
		 * a user unlike the awk statement accessing the map key.
		 * so you can easily determine if the key is found by
		 * checking the error number.
		 */
		return HAWK_NULL;
	}

	return HAWK_MAP_VPTR(pair);
}

hawk_val_map_itr_t* hawk_rtx_getfirstmapvalitr (hawk_rtx_t* rtx, hawk_val_t* map, hawk_val_map_itr_t* itr)
{
	HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);
	hawk_init_map_itr (itr, 0); /* override the caller provided direction to 0 */
	itr->pair = hawk_map_getfirstpair(((hawk_val_map_t*)map)->map, itr);
	return itr->pair? itr: HAWK_NULL;
}

hawk_val_map_itr_t* hawk_rtx_getnextmapvalitr (hawk_rtx_t* rtx, hawk_val_t* map, hawk_val_map_itr_t* itr)
{
	HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);
	itr->pair = hawk_map_getnextpair(((hawk_val_map_t*)map)->map, itr);
	return itr->pair? itr: HAWK_NULL;
}

hawk_val_t* hawk_rtx_makerefval (hawk_rtx_t* rtx, int id, hawk_val_t** adr)
{
	hawk_val_ref_t* val;

	if (rtx->rcache_count > 0)
	{
		val = rtx->rcache[--rtx->rcache_count];
	}
	else
	{
		val = (hawk_val_ref_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*val));
		if (!val) return HAWK_NULL;
	}

	HAWK_RTX_INIT_REF_VAL (val, id, adr, 0);
	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makefunval (hawk_rtx_t* rtx, const hawk_fun_t* fun)
{
	hawk_val_fun_t* val;

	val = (hawk_val_fun_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*val));
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_FUN;
	val->v_refs = 0;
	val->v_static = 0;
	val->nstr = 0;
	val->fcb = 0;
	val->fun = (hawk_fun_t*)fun;

	return (hawk_val_t*)val;
}

int HAWK_INLINE hawk_rtx_isstaticval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	return HAWK_VTR_IS_POINTER(val) && IS_STATICVAL(val);
}

int hawk_rtx_getvaltype (hawk_rtx_t* rtx, hawk_val_t* val)
{
	return HAWK_RTX_GETVALTYPE(rtx, val);
}

const hawk_ooch_t* hawk_rtx_getvaltypename(hawk_rtx_t* rtx, hawk_val_t* val)
{
	static const hawk_ooch_t* __val_type_name[] =
	{
		/* synchronize this table with enum hawk_val_type_t in awk.h */
		HAWK_T("nil"),
		HAWK_T("int"),
		HAWK_T("flt"),
		HAWK_T("str"),
		HAWK_T("mbs"),
		HAWK_T("fun"),
		HAWK_T("map"),
		HAWK_T("rex"),
		HAWK_T("ref")
	};

	return __val_type_name[HAWK_RTX_GETVALTYPE(rtx, val)];
}


int hawk_rtx_getintfromval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	return HAWK_RTX_GETINTFROMVAL(rtx, val);
}

void hawk_rtx_freeval (hawk_rtx_t* rtx, hawk_val_t* val, int cache)
{
	hawk_val_type_t vtype;

	if (HAWK_VTR_IS_POINTER(val))
	{
		if (IS_STATICVAL(val)) return;

	#if defined(DEBUG_VAL)
		hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("freeing [cache=%d] - [%O]\n"), cache, val);
	#endif

		vtype = HAWK_RTX_GETVALTYPE (rtx, val);
		switch (vtype)
		{
			case HAWK_VAL_NIL:
			{
				hawk_rtx_freemem (rtx, val);
				break;
			}
			
			case HAWK_VAL_INT:
			{
				((hawk_val_int_t*)val)->nde = (hawk_nde_int_t*)rtx->vmgr.ifree;
				rtx->vmgr.ifree = (hawk_val_int_t*)val;
				break;
			}

			case HAWK_VAL_FLT:
			{
				((hawk_val_flt_t*)val)->nde = (hawk_nde_flt_t*)rtx->vmgr.rfree;
				rtx->vmgr.rfree = (hawk_val_flt_t*)val;
				break;
			}

			case HAWK_VAL_STR:
			{
			#if defined(ENABLE_FEATURE_SCACHE)
				if (cache)
				{
					hawk_val_str_t* v = (hawk_val_str_t*)val;
					int i;
		
					i = v->val.len / FEATURE_SCACHE_BLOCK_UNIT;
					if (i < HAWK_COUNTOF(rtx->scache_count) &&
					    rtx->scache_count[i] < HAWK_COUNTOF(rtx->scache[i]))
					{
						rtx->scache[i][rtx->scache_count[i]++] = v;
						v->nstr = 0;
					}
					else hawk_rtx_freemem (rtx, val);
				}
				else 
			#endif
					hawk_rtx_freemem (rtx, val);

				break;
			}

			case HAWK_VAL_MBS:
				hawk_rtx_freemem (rtx, val);
				break;

			case HAWK_VAL_REX:
			{
				/* don't free ptr as it is inlined to val
				hawk_rtx_freemem (rtx, ((hawk_val_rex_t*)val)->ptr);
				 */
			
				/* code is just a pointer to a regular expression stored
				 * in parse tree nodes. so don't free it.
				hawk_freerex (rtx->hawk, ((hawk_val_rex_t*)val)->code[0], ((hawk_val_rex_t*)val)->code[1]);
				 */

				hawk_rtx_freemem (rtx, val);
				break;
			}

			case HAWK_VAL_FUN:
				hawk_rtx_freemem (rtx, val);
				break;

			case HAWK_VAL_MAP:
			{
				hawk_map_fini (((hawk_val_map_t*)val)->map);
				hawk_rtx_freemem (rtx, val);
				break;
			}

			case HAWK_VAL_REF:
			{
				if (cache && rtx->rcache_count < HAWK_COUNTOF(rtx->rcache))
				{
					rtx->rcache[rtx->rcache_count++] = (hawk_val_ref_t*)val;
				}
				else hawk_rtx_freemem (rtx, val);
				break;
			}

		}
	}
}

void hawk_rtx_refupval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	if (HAWK_VTR_IS_POINTER(val))
	{
		if (IS_STATICVAL(val)) return;

	#if defined(DEBUG_VAL)
		hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("ref up [ptr=%p] [count=%d] - [%O]\n"), val, (int)val->v_refs, val);
	#endif
		val->v_refs++;
	}
}

void hawk_rtx_refdownval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	if (HAWK_VTR_IS_POINTER(val))
	{
		if (IS_STATICVAL(val)) return;

	#if defined(DEBUG_VAL)
		hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("ref down [ptr=%p] [count=%d] - [%O]\n"), val, (int)val->v_refs, val);
	#endif

		/* the reference count of a value should be greater than zero for it to be decremented. check the source code for any bugs */
		HAWK_ASSERT (val->v_refs > 0);

		val->v_refs--;
		if (val->v_refs <= 0) 
		{
			hawk_rtx_freeval(rtx, val, 1);
		}
	}
}

void hawk_rtx_refdownval_nofree (hawk_rtx_t* rtx, hawk_val_t* val)
{
	if (HAWK_VTR_IS_POINTER(val))
	{
		if (IS_STATICVAL(val)) return;
	
		/* the reference count of a value should be greater than zero for it to be decremented. check the source code for any bugs */
		HAWK_ASSERT (val->v_refs > 0);

		val->v_refs--;
	}
}

void hawk_rtx_freevalchunk (hawk_rtx_t* rtx, hawk_val_chunk_t* chunk)
{
	while (chunk != HAWK_NULL)
	{
		hawk_val_chunk_t* next = chunk->next;
		hawk_rtx_freemem (rtx, chunk);
		chunk = next;
	}
}

static int val_ref_to_bool (hawk_rtx_t* rtx, const hawk_val_ref_t* ref)
{
	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			hawk_oow_t idx;

			idx = (hawk_oow_t)ref->adr;
			if (idx == 0)
			{
				return HAWK_OOECS_LEN(&rtx->inrec.line) > 0;
			}
			else if (idx <= rtx->inrec.nflds)
			{
				return rtx->inrec.flds[idx-1].len > 0;
			}
			else
			{
				/* the index is greater than the number of records.
				 * it's an empty string. so false */
				return 0;
			}
		}
		case HAWK_VAL_REF_GBL:
		{
			hawk_oow_t idx;
			idx = (hawk_oow_t)ref->adr;
			return hawk_rtx_valtobool(rtx, RTX_STACK_GBL (rtx, idx));
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;

			/* A reference value is not able to point to another 
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT (HAWK_RTX_GETVALTYPE (rtx, *xref)!= HAWK_VAL_REF); 

			/* make a recursive call back to the caller */
			return hawk_rtx_valtobool(rtx, *xref);
		}
	}
}

int hawk_rtx_valtobool (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	hawk_val_type_t vtype;

	if (val == HAWK_NULL) return 0;

	vtype = HAWK_RTX_GETVALTYPE(rtx, val);
	switch (vtype)
	{
		case HAWK_VAL_NIL:
			return 0;
		case HAWK_VAL_INT:
			return HAWK_RTX_GETINTFROMVAL(rtx, val) != 0;
		case HAWK_VAL_FLT:
			return ((hawk_val_flt_t*)val)->val != 0.0;
		case HAWK_VAL_STR:
			return ((hawk_val_str_t*)val)->val.len > 0;
		case HAWK_VAL_MBS:
			return ((hawk_val_mbs_t*)val)->val.len > 0;
		case HAWK_VAL_REX: /* TODO: is this correct? */
			return ((hawk_val_rex_t*)val)->str.len > 0;
		case HAWK_VAL_FUN:
			/* return always true */
			return 1;
		case HAWK_VAL_MAP:
			/* true if the map size is greater than 0. false if not */
			return HAWK_MAP_SIZE(((hawk_val_map_t*)val)->map) > 0;
		case HAWK_VAL_REF:
			return val_ref_to_bool(rtx, (hawk_val_ref_t*)val);
	}

	/* the type of a value should be one of HAWK_VAL_XXX enumerators defined in hawk-prv.h */
	HAWK_ASSERT (!"should never happen - invalid value type");
		
	return 0;
}

static int str_to_str (hawk_rtx_t* rtx, const hawk_ooch_t* str, hawk_oow_t str_len, hawk_rtx_valtostr_out_t* out)
{
	int type = out->type & ~HAWK_RTX_VALTOSTR_PRINT;

	switch (type)
	{
		case HAWK_RTX_VALTOSTR_CPL:
		{
			out->u.cpl.len = str_len;
			out->u.cpl.ptr = (hawk_ooch_t*)str;
			return 0;
		}

		case HAWK_RTX_VALTOSTR_CPLCPY:
		{
			if (str_len >= out->u.cplcpy.len)
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
				/*out->u.cplcpy.len = str_len + 1;*/ /* set the required length */
				return -1;
			}

			out->u.cplcpy.len = hawk_copy_oochars_to_oocstr_unlimited(out->u.cplcpy.ptr, str, str_len);
			return 0;
		}

		case HAWK_RTX_VALTOSTR_CPLDUP:
		{
			hawk_ooch_t* tmp;

			tmp = hawk_rtx_dupoochars(rtx, str, str_len);
			if (!tmp) return -1;

			out->u.cpldup.ptr = tmp;
			out->u.cpldup.len = str_len;
			return 0;
		}

		case HAWK_RTX_VALTOSTR_STRP:
		{
			hawk_oow_t n;
			hawk_ooecs_clear (out->u.strp);
			n = hawk_ooecs_ncat(out->u.strp, str, str_len);
			if (n == (hawk_oow_t)-1) return -1;
			return 0;
		}

		case HAWK_RTX_VALTOSTR_STRPCAT:
		{
			hawk_oow_t n;
			n = hawk_ooecs_ncat(out->u.strpcat, str, str_len);
			if (n == (hawk_oow_t)-1) return -1;
			return 0;
		}
	}

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

#if defined(HAWK_OOCH_IS_BCH)
#	define mbs_to_str(rtx,str,str_len,out) str_to_str(rtx,str,str_len,out)
#else
static int mbs_to_str (hawk_rtx_t* rtx, const hawk_bch_t* str, hawk_oow_t str_len, hawk_rtx_valtostr_out_t* out)
{
	int type = out->type & ~HAWK_RTX_VALTOSTR_PRINT;

	switch (type)
	{
		case HAWK_RTX_VALTOSTR_CPL:
			/* conversion is required. i can't simply return it. let CPL
			 * behave like CPLCPY. fall thru */
		case HAWK_RTX_VALTOSTR_CPLCPY:
		{
			hawk_oow_t ucslen;
			if (HAWK_UNLIKELY(out->u.cplcpy.len <= 0))
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EBUFFULL);
				return -1;
			}
			/* hawk_rtx_convbtouchars() doesn't null terminate the result. -1 to secure space for '\0' */
			ucslen = out->u.cplcpy.len - 1; 
			if (hawk_rtx_convbtouchars(rtx, str, &str_len, out->u.cplcpy.ptr, &ucslen, 1) <= -1) return -1;

			out->u.cplcpy.ptr[ucslen] = HAWK_T('\0');
			out->u.cplcpy.len = ucslen;

			return 0;
		}

		case HAWK_RTX_VALTOSTR_CPLDUP:
		{
			hawk_ooch_t* tmp;
			hawk_oow_t wcslen;

			tmp = hawk_rtx_dupbtouchars(rtx, str, str_len, &wcslen, 1);
			if (!tmp) return -1;

			out->u.cpldup.ptr = tmp;
			out->u.cpldup.len = wcslen;
			return 0;
		}

		case HAWK_RTX_VALTOSTR_STRP:
			hawk_ooecs_clear (out->u.strp);
			if (hawk_uecs_ncatbchars(out->u.strp, str, str_len, hawk_rtx_getcmgr(rtx), 1) == (hawk_oow_t)-1) return -1;
			return 0;

		case HAWK_RTX_VALTOSTR_STRPCAT:
			if (hawk_uecs_ncatbchars(out->u.strpcat, str, str_len, hawk_rtx_getcmgr(rtx), 1) == (hawk_oow_t)-1) return -1;
			return 0;
	}

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
	return -1;
}
#endif


static int val_int_to_str (hawk_rtx_t* rtx, const hawk_val_int_t* v, hawk_rtx_valtostr_out_t* out)
{
	hawk_ooch_t* tmp;
	hawk_oow_t rlen = 0;
	int type = out->type & ~HAWK_RTX_VALTOSTR_PRINT;
	hawk_int_t orgval = HAWK_RTX_GETINTFROMVAL (rtx, v);
	hawk_uint_t t;

	if (orgval == 0) rlen++;
	else
	{
		/* non-zero values */
		if (orgval < 0) 
		{
			t = orgval * -1; rlen++; 
		}
		else t = orgval;
		while (t > 0) { rlen++; t /= 10; }
	}

	switch (type)
	{
		case HAWK_RTX_VALTOSTR_CPL:
			/* CPL and CPLCP behave the same for int_t.
			 * i just fall through assuming that cplcpy 
			 * and cpl are the same type. the following
			 * assertion at least ensure that they have
			 * the same size. */ 
			HAWK_ASSERT (HAWK_SIZEOF(out->u.cpl) == HAWK_SIZEOF(out->u.cplcpy));

		case HAWK_RTX_VALTOSTR_CPLCPY:
			if (rlen >= out->u.cplcpy.len)
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
				/* store the buffer size needed */
				out->u.cplcpy.len = rlen + 1; 
				return -1;
			}

			tmp = out->u.cplcpy.ptr;
			tmp[rlen] = HAWK_T('\0');
			out->u.cplcpy.len = rlen;
			break;

		case HAWK_RTX_VALTOSTR_CPLDUP:
			tmp = hawk_rtx_allocmem(rtx, (rlen + 1) * HAWK_SIZEOF(hawk_ooch_t));
			if (!tmp) return -1;

			tmp[rlen] = HAWK_T('\0');
			out->u.cpldup.ptr = tmp;
			out->u.cpldup.len = rlen;
			break;

		case HAWK_RTX_VALTOSTR_STRP:
		{
			hawk_oow_t n;

			hawk_ooecs_clear (out->u.strp);
			HAWK_ASSERT (HAWK_OOECS_LEN(out->u.strp) == 0);

			/* point to the beginning of the buffer */
			tmp = HAWK_OOECS_PTR(out->u.strp);

			/* extend the buffer */
			n = hawk_ooecs_nccat(out->u.strp, HAWK_T(' '), rlen);
			if (n == (hawk_oow_t)-1) return -1;
			break;
		}

		case HAWK_RTX_VALTOSTR_STRPCAT:
		{
			hawk_oow_t n;

			/* point to the insertion point */
			tmp = HAWK_OOECS_PTR(out->u.strpcat) + HAWK_OOECS_LEN(out->u.strpcat);

			/* extend the buffer */
			n = hawk_ooecs_nccat(out->u.strpcat, HAWK_T(' '), rlen);
			if (n == (hawk_oow_t)-1) return -1;
			break;
		}

		default:
		{
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
			return -1;
		}
	}

	if (orgval == 0) tmp[0] = HAWK_T('0'); 
	else
	{
		t = (orgval < 0)? (orgval * -1): orgval;

		/* fill in the buffer with digits */
		while (t > 0) 
		{
			tmp[--rlen] = (hawk_ooch_t)(t % 10) + HAWK_T('0');
			t /= 10;
		}

		/* insert the negative sign if necessary */
		if (orgval < 0) tmp[--rlen] = HAWK_T('-');
	}

	return 0;
}

static int val_flt_to_str (hawk_rtx_t* rtx, const hawk_val_flt_t* v, hawk_rtx_valtostr_out_t* out)
{
	hawk_ooch_t* tmp;
	hawk_oow_t tmp_len;
	hawk_ooecs_t buf, fbu;
	int buf_inited = 0, fbu_inited = 0;
	int type = out->type & ~HAWK_RTX_VALTOSTR_PRINT;

	if (out->type & HAWK_RTX_VALTOSTR_PRINT)
	{
		tmp = rtx->gbl.ofmt.ptr;
		tmp_len = rtx->gbl.ofmt.len;
	}
	else
	{
		tmp = rtx->gbl.convfmt.ptr;
		tmp_len = rtx->gbl.convfmt.len;
	}

	if (hawk_ooecs_init(&buf, hawk_rtx_getgem(rtx), 256) <= -1) return -1;
	buf_inited = 1;

	if (hawk_ooecs_init(&fbu, hawk_rtx_getgem(rtx), 256) <= -1)
	{
		hawk_ooecs_fini (&buf);
		return -1;
	}
	fbu_inited = 1;

	tmp = hawk_rtx_format(rtx, &buf, &fbu, tmp, tmp_len, (hawk_oow_t)-1, (hawk_nde_t*)v, &tmp_len);
	if (tmp == HAWK_NULL) goto oops;

	switch (type)
	{
		case HAWK_RTX_VALTOSTR_CPL:
			/* CPL and CPLCP behave the same for flt_t.
			 * i just fall through assuming that cplcpy 
			 * and cpl are the same type. the following
			 * assertion at least ensure that they have
			 * the same size. */ 
			HAWK_ASSERT (HAWK_SIZEOF(out->u.cpl) == HAWK_SIZEOF(out->u.cplcpy));

		case HAWK_RTX_VALTOSTR_CPLCPY:
			if (out->u.cplcpy.len <= tmp_len)
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
				/* store the buffer size required */
				out->u.cplcpy.len = tmp_len + 1; 
				goto oops;
			}

			hawk_copy_oochars_to_oocstr_unlimited (out->u.cplcpy.ptr, tmp, tmp_len);
			out->u.cplcpy.len = tmp_len;
			break;

		case HAWK_RTX_VALTOSTR_CPLDUP:
		{
			hawk_ooecs_yield (&buf, HAWK_NULL, 0);
			out->u.cpldup.ptr = tmp;
			out->u.cpldup.len = tmp_len;
			break;
		}

		case HAWK_RTX_VALTOSTR_STRP:
		{
			hawk_oow_t n;
			hawk_ooecs_clear (out->u.strp);
			n = hawk_ooecs_ncat(out->u.strp, tmp, tmp_len);
			if (n == (hawk_oow_t)-1) goto oops;
			break;
		}

		case HAWK_RTX_VALTOSTR_STRPCAT:
		{
			hawk_oow_t n;
			n = hawk_ooecs_ncat(out->u.strpcat, tmp, tmp_len);
			if (n == (hawk_oow_t)-1) goto oops;
			break;
		}

		default:
		{
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
			goto oops;
		}
	}

	hawk_ooecs_fini (&fbu);
	hawk_ooecs_fini (&buf);
	return 0;

oops:
	if (fbu_inited) hawk_ooecs_fini (&fbu);
	if (buf_inited) hawk_ooecs_fini (&buf);
	return -1;
}

static int val_ref_to_str (hawk_rtx_t* rtx, const hawk_val_ref_t* ref, hawk_rtx_valtostr_out_t* out)
{
	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			hawk_oow_t idx;

			/* special case when the reference value is 
			 * pointing to the positional */

			idx = (hawk_oow_t)ref->adr;
			if (idx == 0)
			{
				return str_to_str(
					rtx,
					HAWK_OOECS_PTR(&rtx->inrec.line),
					HAWK_OOECS_LEN(&rtx->inrec.line),
					out
				);
			}
			else if (idx <= rtx->inrec.nflds)
			{
				return str_to_str(
					rtx,
					rtx->inrec.flds[idx-1].ptr,
					rtx->inrec.flds[idx-1].len,
					out
				);
			}
			else
			{
				return str_to_str(rtx, HAWK_T(""), 0, out);
			}
		}

		case HAWK_VAL_REF_GBL:
		{
			hawk_oow_t idx = (hawk_oow_t)ref->adr;
			return hawk_rtx_valtostr(rtx, RTX_STACK_GBL (rtx, idx), out);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;

			/* A reference value is not able to point to another 
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT (HAWK_RTX_GETVALTYPE (rtx, *xref) != HAWK_VAL_REF); 

			/* make a recursive call back to the caller */
			return hawk_rtx_valtostr(rtx, *xref, out);
		}
	}
}

int hawk_rtx_valtostr (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_rtx_valtostr_out_t* out)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			return str_to_str(rtx, HAWK_T(""), 0, out);

		case HAWK_VAL_INT:
			return val_int_to_str(rtx, (hawk_val_int_t*)v, out);

		case HAWK_VAL_FLT:
			return val_flt_to_str(rtx, (hawk_val_flt_t*)v, out);

		case HAWK_VAL_STR:
		{
			hawk_val_str_t* vs = (hawk_val_str_t*)v;
			return str_to_str(rtx, vs->val.ptr, vs->val.len, out);
		}

		case HAWK_VAL_MBS:
		{
			hawk_val_mbs_t* vs = (hawk_val_mbs_t*)v;
		#if defined(HAWK_OOCH_IS_BCH)
			return str_to_str(rtx, vs->val.ptr, vs->val.len, out);
		#else
			return mbs_to_str(rtx, vs->val.ptr, vs->val.len, out);
		#endif
		}

		case HAWK_VAL_FUN:
			return str_to_str(rtx, ((hawk_val_fun_t*)v)->fun->name.ptr, ((hawk_val_fun_t*)v)->fun->name.len, out);

		case HAWK_VAL_MAP:
			if (rtx->hawk->opt.trait & HAWK_FLEXMAP)
			{
				return str_to_str(rtx, HAWK_T("#MAP"), 4, out);
			}
			goto invalid;

		case HAWK_VAL_REF:
			return val_ref_to_str(rtx, (hawk_val_ref_t*)v, out);

		case HAWK_VAL_REX:
		default:
		invalid:
		#if defined(DEBUG_VAL)
			hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T(">>WRONG VALUE TYPE [%d] in hawk_rtx_valtostr\n"), v->type);
		#endif
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EVALTOSTR);
			return -1;
	}
}

hawk_bch_t* hawk_rtx_valtobcstrdupwithcmgr (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_oow_t* len, hawk_cmgr_t* cmgr)
{
	hawk_bch_t* mbs;
	hawk_val_type_t vtype;

	vtype = HAWK_RTX_GETVALTYPE(rtx,v);

	switch (vtype)
	{
		case HAWK_VAL_MBS:
			mbs = hawk_rtx_dupbchars(rtx, ((hawk_val_mbs_t*)v)->val.ptr, ((hawk_val_mbs_t*)v)->val.len);
			if (!mbs) return HAWK_NULL;
			if (len) *len = ((hawk_val_mbs_t*)v)->val.len;
			break;

		case HAWK_VAL_STR:
		{
		#if defined(HAWK_OOCH_IS_BCH)
			mbs = hawk_rtx_dupbchars(rtx, ((hawk_val_str_t*)v)->val.ptr, ((hawk_val_str_t*)v)->val.len);
			if (!mbs) return HAWK_NULL;
			if (len) *len = ((hawk_val_str_t*)v)->val.len;
		#else
			hawk_oow_t mbslen, wcslen;
			wcslen = ((hawk_val_str_t*)v)->val.len;
			mbs = hawk_rtx_duputobcharswithcmgr(rtx, ((hawk_val_str_t*)v)->val.ptr, wcslen, &mbslen, cmgr);
			if (!mbs) return HAWK_NULL;
			if (len) *len = mbslen;
		#endif
			break;
		}

		default:
		{
		#if defined(HAWK_OOCH_IS_BCH)
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, v, &out) <= -1) return HAWK_NULL;

			mbs = out.u.cpldup.ptr;
			if (len) *len = out.u.cpldup.len;
		#else
			hawk_oow_t mbslen;
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, v, &out) <= -1) return HAWK_NULL;
/* TODO IMPLEMENT hawk_rtx_valtobcs()... and use it */

			mbs = hawk_rtx_duputobcharswithcmgr(rtx, out.u.cpldup.ptr, out.u.cpldup.len, &mbslen, cmgr);
			hawk_rtx_freemem (rtx, out.u.cpldup.ptr);
			if (!mbs) return HAWK_NULL;
			if (len) *len = mbslen;
		#endif
			break;
		}
	}

	return mbs;
}

hawk_uch_t* hawk_rtx_valtoucstrdupwithcmgr (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_oow_t* len, hawk_cmgr_t* cmgr)
{
	hawk_uch_t* wcs;
	hawk_val_type_t vtype;

	vtype = HAWK_RTX_GETVALTYPE(rtx,v);

	switch (vtype)
	{
		case HAWK_VAL_MBS:
		{
			hawk_oow_t mbslen, wcslen;
			mbslen = ((hawk_val_mbs_t*)v)->val.len;
			wcs = hawk_rtx_dupbtoucharswithcmgr(rtx, ((hawk_val_mbs_t*)v)->val.ptr, mbslen, &wcslen, cmgr, 1);
			if (!wcs) return HAWK_NULL;
			if (len) *len = wcslen;
			break;
		}

		case HAWK_VAL_STR:
		{
		#if defined(HAWK_OOCH_IS_BCH)
			hawk_oow_t wcslen, mbslen;
			mbslen = ((hawk_val_str_t*)v)->val.len;
			wcs = hawk_rtx_dupbtoucharswithcmgr(rtx, ((hawk_val_str_t*)v)->val.ptr, mbslen, &wcslen, cmgr, 1);
		#else
			wcs = hawk_rtx_dupuchars(rtx, ((hawk_val_str_t*)v)->val.ptr, ((hawk_val_str_t*)v)->val.len);
		#endif
			if (!wcs) return HAWK_NULL;
		#if defined(HAWK_OOCH_IS_BCH)
			if (len) *len = wcslen;
		#else
			if (len) *len = ((hawk_val_str_t*)v)->val.len;
		#endif
			break;
		}

		default:
		{
		#if defined(HAWK_OOCH_IS_BCH)
			hawk_oow_t wcslen;
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, v, &out) <= -1) return HAWK_NULL;

			wcs = hawk_rtx_dupbtoucharswithcmgr(rtx, out.u.cpldup.ptr, out.u.cpldup.len, &wcslen, cmgr, 1);
			hawk_rtx_freemem (rtx, out.u.cpldup.ptr);
			if (!wcs) return HAWK_NULL;

			if (len) *len = wcslen;
		#else
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, v, &out) <= -1) return HAWK_NULL;

			wcs = out.u.cpldup.ptr;
			if (len) *len = out.u.cpldup.len;
		#endif
			break;
		}
	}
	return wcs;
}

hawk_ooch_t* hawk_rtx_getvaloocstrwithcmgr (hawk_rtx_t* rtx, hawk_val_t* v, hawk_oow_t* len, hawk_cmgr_t* cmgr)
{
	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_STR:
#if 0
		plain_str:
#endif
			if (len) *len = ((hawk_val_str_t*)v)->val.len;
			return ((hawk_val_str_t*)v)->val.ptr;

#if 0
/* i'm commenting out this part because hawk_rtx_setrefval() changes v->adr 
 * and leads hawk_rtx_freevaloocstr() to check a wrong value obejct.
 * if you know that a value is a reference, you can get the referenced value
 * with hawk_rtx_getrefval() and call this function over it */
		case HAWK_VAL_REF:
			v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			if (HAWK_RTX_GETVALTYPE(rtx, v1) == HAWK_VAL_STR) goto plain_str;
			/* fall through */
#endif

		default:
			return hawk_rtx_valtooocstrdupwithcmgr(rtx, v, len, cmgr);
	}
}

void hawk_rtx_freevaloocstr (hawk_rtx_t* rtx, hawk_val_t* v, hawk_ooch_t* str)
{
	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_STR:
#if 0
		plain_str:
#endif
			if (str != ((hawk_val_str_t*)v)->val.ptr) hawk_rtx_freemem (rtx, str);
			break;

#if 0
		case HAWK_VAL_REF:
			v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			if (v && HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_STR) goto plain_str;
			/* fall through */
#endif

		default:
			hawk_rtx_freemem (rtx, str);
			break;
	}
}

hawk_bch_t* hawk_rtx_getvalbcstrwithcmgr (hawk_rtx_t* rtx, hawk_val_t* v, hawk_oow_t* len, hawk_cmgr_t* cmgr)
{
	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_MBS:
#if 0
		plain_mbs:
#endif
			if (len) *len = ((hawk_val_mbs_t*)v)->val.len;
			return ((hawk_val_mbs_t*)v)->val.ptr;

#if 0
/* i'm commenting out this part because hawk_rtx_setrefval() changes v->adr 
 * and leads hawk_rtx_freevalbcstr() to check a wrong value obejct.
 * if you know that a value is a reference, you can get the referenced value
 * with hawk_rtx_getrefval() and call this function over it */
		case HAWK_VAL_REF:
			v1 = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			if (v1 && HAWK_RTX_GETVALTYPE(rtx, v1) == HAWK_VAL_MBS) goto plain_mbs;
			/* fall through */
#endif

		default:
			return hawk_rtx_valtobcstrdupwithcmgr(rtx, v, len, cmgr);
	}
}

void hawk_rtx_freevalbcstr (hawk_rtx_t* rtx, hawk_val_t* v, hawk_bch_t* str)
{
	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_MBS:
#if 0
		plain_mbs:
#endif
			if (str != ((hawk_val_mbs_t*)v)->val.ptr) hawk_rtx_freemem (rtx, str);
			break;

#if 0
		case HAWK_VAL_REF:
			v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			if (v && HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_MBS) goto plain_mbs;
			/* fall through */
#endif

		default:
			hawk_rtx_freemem (rtx, str);
			break;
	}
}

static int val_ref_to_num (hawk_rtx_t* rtx, const hawk_val_ref_t* ref, hawk_int_t* l, hawk_flt_t* r)
{
	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			hawk_oow_t idx;
	       
			idx = (hawk_oow_t)ref->adr;
			if (idx == 0)
			{
				return hawk_oochars_to_num(
					HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0),
					HAWK_OOECS_PTR(&rtx->inrec.line),
					HAWK_OOECS_LEN(&rtx->inrec.line),
					l, r
				);
			}
			else if (idx <= rtx->inrec.nflds)
			{
				return hawk_oochars_to_num(
					HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0),
					rtx->inrec.flds[idx-1].ptr,
					rtx->inrec.flds[idx-1].len,
					l, r
				);
			}
			else
			{
				return hawk_oochars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), HAWK_T(""), 0, l, r);
			}
		}

		case HAWK_VAL_REF_GBL:
		{
			hawk_oow_t idx = (hawk_oow_t)ref->adr;
			return hawk_rtx_valtonum(rtx, RTX_STACK_GBL (rtx, idx), l, r);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;

			/* A reference value is not able to point to another 
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, *xref) != HAWK_VAL_REF); 

			/* make a recursive call back to the caller */
			return hawk_rtx_valtonum(rtx, *xref, l, r);
		}
	}
}


int hawk_rtx_valtonum (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_int_t* l, hawk_flt_t* r)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			*l = 0;
			return 0;

		case HAWK_VAL_INT:
			*l = HAWK_RTX_GETINTFROMVAL(rtx, v);
			return 0; /* long */

		case HAWK_VAL_FLT:
			*r = ((hawk_val_flt_t*)v)->val;
			return 1; /* real */

		case HAWK_VAL_STR:
			return hawk_oochars_to_num(
				HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0),
				((hawk_val_str_t*)v)->val.ptr,
				((hawk_val_str_t*)v)->val.len,
				l, r
			);

		case HAWK_VAL_MBS:
			return hawk_bchars_to_num(
				HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0),
				((hawk_val_mbs_t*)v)->val.ptr,
				((hawk_val_mbs_t*)v)->val.len,
				l, r
			);

		case HAWK_VAL_FUN:
			/* unable to convert a function to a number */
			goto invalid;

		case HAWK_VAL_MAP:
			if (rtx->hawk->opt.trait & HAWK_FLEXMAP)
			{
				*l = HAWK_MAP_SIZE(((hawk_val_map_t*)v)->map);
				return 0; /* long */
			}
			goto invalid;

		case HAWK_VAL_REF:
			return val_ref_to_num(rtx, (hawk_val_ref_t*)v, l, r);

		case HAWK_VAL_REX:
		default:
		invalid:
		#if defined(DEBUG_VAL)
			hawk_logfmt (hawk, HAWK_T(">>WRONG VALUE TYPE [%d] in hawk_rtx_valtonum()\n"), v->type);
		#endif
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EVALTONUM);
			return -1; /* error */
	}
}

int hawk_rtx_valtoint (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_int_t* l)
{
	int n;
	hawk_flt_t r;

	n = hawk_rtx_valtonum(rtx, v, l, &r);
	if (n == 1) 
	{
		*l = (hawk_int_t)r;
		n = 0;
	}

	return n;
}

int hawk_rtx_valtoflt (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_flt_t* r)
{
	int n;
	hawk_int_t l;

	n = hawk_rtx_valtonum(rtx, v, &l, r);
	if (n == 0) *r = (hawk_flt_t)l;
	else if (n == 1) n = 0;

	return n;
}

/* ========================================================================== */

static HAWK_INLINE hawk_uint_t hash (hawk_uint8_t* ptr, hawk_oow_t len)
{
	hawk_uint_t h;
	HAWK_HASH_BYTES (h, ptr, len);
	return h;
}

hawk_int_t hawk_rtx_hashval (hawk_rtx_t* rtx, hawk_val_t* v)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE (rtx, v);
	hawk_int_t hv;

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			hv = 0;
			break;

		case HAWK_VAL_INT:
		{
			hawk_int_t tmp = HAWK_RTX_GETINTFROMVAL(rtx, v);
			/*hv = ((hawk_val_int_t*)v)->val;*/
			hv = (hawk_int_t)hash((hawk_uint8_t*)&tmp, HAWK_SIZEOF(tmp));
			break;
		}

		case HAWK_VAL_FLT:
		{
			hawk_val_flt_t* dv = (hawk_val_flt_t*)v;
			hv = (hawk_int_t)hash((hawk_uint8_t*)&dv->val, HAWK_SIZEOF(dv->val));
			break;
		}

		case HAWK_VAL_STR:
		{
			hawk_val_str_t* dv = (hawk_val_str_t*)v;
			hv = (hawk_int_t)hash((hawk_uint8_t*)dv->val.ptr, dv->val.len * HAWK_SIZEOF(*dv->val.ptr));
			break;
		}

		case HAWK_VAL_MBS:
		{
			hawk_val_mbs_t* dv = (hawk_val_mbs_t*)v;
			hv = (hawk_int_t)hash((hawk_uint8_t*)dv->val.ptr, dv->val.len * HAWK_SIZEOF(*dv->val.ptr));
			break;
		}

		default:

#if defined(DEBUG_VAL)
			hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T(">>WRONG VALUE TYPE [%d] in hawk_rtx_hashval()\n"), v->type);
#endif
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EHASHVAL);
			return -1;
	}

	/* turn off the sign bit */
	return hv  & ~(((hawk_uint_t)1) << ((HAWK_SIZEOF(hawk_uint_t) * 8) - 1));
}

hawk_val_type_t hawk_rtx_getrefvaltype (hawk_rtx_t* rtx, hawk_val_ref_t* ref)
{
	/* return the type of the value that the reference points to */
	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			hawk_oow_t idx;

			idx = (hawk_oow_t)ref->adr;
			if (idx == 0)
			{
				return HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.d0);
			}
			else if (idx <= rtx->inrec.nflds)
			{
				return HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.flds[idx-1].val);
			}
			else
			{
				return HAWK_RTX_GETVALTYPE(rtx, hawk_val_nil);
			}
		}

		case HAWK_VAL_REF_GBL:
		{
			hawk_oow_t idx;
			hawk_val_t* v;
			idx = (hawk_oow_t)ref->adr;
			v = RTX_STACK_GBL(rtx, idx);
			return HAWK_RTX_GETVALTYPE(rtx, v);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;
			hawk_val_t* v;

			/* A reference value is not able to point to another 
			 * refernce value for the way values are represented
			 * in HAWK */
			v = *xref;
			HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, v) != HAWK_VAL_REF); 
			return HAWK_RTX_GETVALTYPE(rtx, v);
		}
	}
}

hawk_val_t* hawk_rtx_getrefval (hawk_rtx_t* rtx, hawk_val_ref_t* ref)
{
	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			hawk_oow_t idx;

			idx = (hawk_oow_t)ref->adr;
			if (idx == 0)
			{
				return rtx->inrec.d0;
			}
			else if (idx <= rtx->inrec.nflds)
			{
				return rtx->inrec.flds[idx-1].val;
			}
			else
			{
				return hawk_val_nil;
			}
		}

		case HAWK_VAL_REF_GBL:
		{
			hawk_oow_t idx;
			idx = (hawk_oow_t)ref->adr;
			return RTX_STACK_GBL(rtx, idx);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;
			/* A reference value is not able to point to another 
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, *xref) != HAWK_VAL_REF); 
			return *xref;
		}
	}
}

int hawk_rtx_setrefval (hawk_rtx_t* rtx, hawk_val_ref_t* ref, hawk_val_t* val)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE (rtx, val);

	if (vtype == HAWK_VAL_REX || vtype == HAWK_VAL_REF)
	{
		/* though it is possible that an intrinsic function handler
		 * can accept a regular expression withtout evaluation when 'x'
		 * is specified for the parameter, this function doesn't allow
		 * regular expression to be set to a reference variable to
		 * avoid potential chaos. the nature of performing '/rex/ ~ $0' 
		 * for a regular expression without the match operator
		 * makes it difficult to be implemented. */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			switch (vtype)
			{
				case HAWK_VAL_MAP:
					/* a map is assigned to a positional. this is disallowed. */
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EMAPTOPOS);
					return -1;

				case HAWK_VAL_STR:
				{
					int x;
					/* handle this separately from the default case
					 * for no duplication. jumping to the default case
					 * and callinghawk_rtx_valtooocstrdup() would also work, anyway. */
					hawk_rtx_refupval (rtx, val);
					x = hawk_rtx_setrec(rtx, (hawk_oow_t)ref->adr, &((hawk_val_str_t*)val)->val, 0);
					hawk_rtx_refdownval (rtx, val);
					return x;
				}

				case HAWK_VAL_MBS:
				#if defined(HAWK_OOCH_IS_BCH)
				{
					/* same as str in the mchar mode */
					int x;
					hawk_rtx_refupval (rtx, val);
					x = hawk_rtx_setrec(rtx, (hawk_oow_t)ref->adr, &((hawk_val_mbs_t*)val)->val, 0);
					hawk_rtx_refdownval (rtx, val);
					return x;
				}
				#endif
					/* fall thru otherwise */

				default:
				{
					hawk_oocs_t str;
					int x;

					str.ptr = hawk_rtx_valtooocstrdup(rtx, val, &str.len);
					hawk_rtx_refupval (rtx, val);
					x = hawk_rtx_setrec(rtx, (hawk_oow_t)ref->adr, &str, 0);
					hawk_rtx_refdownval (rtx, val);
					hawk_rtx_freemem (rtx, str.ptr);
					return x;
				}
			}
		}

		case HAWK_VAL_REF_GBL:
			/* ref->adr is the index to the global variables, not a real pointer address for HAWK_VAL_REF_GBL */
			return hawk_rtx_setgbl(rtx, (int)ref->adr, val);

		case HAWK_VAL_REF_NAMEDIDX:
		case HAWK_VAL_REF_GBLIDX:
		case HAWK_VAL_REF_LCLIDX:
		case HAWK_VAL_REF_ARGIDX:
			if (vtype == HAWK_VAL_MAP)
			{
				/* an indexed variable cannot be assigned a map. 
				 * in other cases, it falls down to the default case. */
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EMAPTOIDX);
				return -1;
			}
			/* fall through */

		default:
		{
			hawk_val_t** rref;
			hawk_val_type_t rref_vtype;

			rref = (hawk_val_t**)ref->adr; /* old value pointer */
			rref_vtype = HAWK_RTX_GETVALTYPE(rtx, *rref); /* old value type */
			if (vtype == HAWK_VAL_MAP)
			{
				/* new value: map, old value: nil or map => ok */
				if (rref_vtype != HAWK_VAL_NIL && rref_vtype != HAWK_VAL_MAP)
				{
					if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
					{
						/* cannot change a scalar value to a map */
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ESCALARTOMAP);
						return -1;
					}
				}
			}
			else
			{
				/* new value: scalar, old value: nil or scalar => ok */
				if (rref_vtype == HAWK_VAL_MAP)
				{
					if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
					{
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EMAPTOSCALAR);
						return -1;
					}
				}
			}
			
			if (*rref != val)
			{
				/* if the new value is not the same as the old value */
				hawk_rtx_refdownval (rtx, *rref);
				*rref = val;
				hawk_rtx_refupval (rtx, *rref);
			}
			return 0;
		}
	}
}

#if 0

#define hawk_errputstrf hawk_errputstrf

static hawk_map_walk_t print_pair (hawk_map_t* map, hawk_map_pair_t* pair, void* arg)
{
	hawk_rtx_t* run = (hawk_rtx_t*)arg;

	HAWK_ASSERT (run == *(hawk_rtx_t**)hawk_map_getxtn(map));

	hawk_errputstrf (HAWK_T(" %.*s=>"), HAWK_MAP_KLEN(pair), HAWK_MAP_KPTR(pair));
	hawk_dprintval ((hawk_rtx_t*)arg, HAWK_MAP_VPTR(pair));
	hawk_errputstrf (HAWK_T(" "));

	return HAWK_MAP_WALK_FORWARD;
}

void hawk_dprintval (hawk_rtx_t* run, hawk_val_t* val)
{
	/* TODO: better value printing ... */

	switch (val->type)
	{
		case HAWK_VAL_NIL:
			hawk_errputstrf (HAWK_T("nil"));
		       	break;

		case HAWK_VAL_INT:
			hawk_errputstrf (HAWK_T("%jd"), 
				(hawk_intmax_t)((hawk_val_int_t*)val)->val);
			break;

		case HAWK_VAL_FLT:
			hawk_errputstrf (HAWK_T("%jf"), 
				(hawk_fltmax_t)((hawk_val_flt_t*)val)->val);
			break;

		case HAWK_VAL_STR:
			hawk_errputstrf (HAWK_T("%s"), ((hawk_val_str_t*)val)->ptr);
			break;

		case HAWK_VAL_MBS:
			hawk_errputstrf (HAWK_T("%hs"), ((hawk_val_mbs_t*)val)->ptr);
			break;

		case HAWK_VAL_REX:
			hawk_errputstrf (HAWK_T("/%s/"), ((hawk_val_rex_t*)val)->ptr);
			break;

		case HAWK_VAL_FUN:
			hawk_errputstrf (HAWK_T("%.*s"), ((hawk_val_fun_t*)val)->fun->name.len, ((hawk_val_fun_t*)val)->fun->name.ptr);
			break;

		case HAWK_VAL_MAP:
			hawk_errputstrf (HAWK_T("MAP["));
			hawk_map_walk (((hawk_val_map_t*)val)->map, print_pair, run);
			hawk_errputstrf (HAWK_T("]"));
			break;

		case HAWK_VAL_REF:
			hawk_errputstrf (HAWK_T("REF[id=%d,val="), ((hawk_val_ref_t*)val)->id);
			hawk_dprintval (run, *((hawk_val_ref_t*)val)->adr);
			hawk_errputstrf (HAWK_T("]"));
			break;

		default:
			hawk_errputstrf (HAWK_T("**** INTERNAL ERROR - INVALID VALUE TYPE ****\n"));
	}
}

#endif
