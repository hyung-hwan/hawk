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

#include "hawk-prv.h"

#define CHUNKSIZE HAWK_VAL_CHUNK_SIZE

static hawk_val_nil_t hawk_nil = {
	HAWK_SFN(v_refs)   0,
	HAWK_SFN(v_type)   HAWK_VAL_NIL,
	HAWK_SFN(v_static) 1,
	HAWK_SFN(v_nstr)   0,
	HAWK_SFN(v_gc)     0
};

static hawk_val_bool_t hawk_true = {
	HAWK_SFN(v_refs)   0,
	HAWK_SFN(v_type)   HAWK_VAL_BOOL,
	HAWK_SFN(v_static) 1,
	HAWK_SFN(v_nstr)   0,
	HAWK_SFN(v_gc)     0,
	HAWK_SFN(val)      1
};

static hawk_val_bool_t hawk_false = {
	HAWK_SFN(v_refs)   0,
	HAWK_SFN(v_type)   HAWK_VAL_BOOL,
	HAWK_SFN(v_static) 1,
	HAWK_SFN(v_nstr)   0,
	HAWK_SFN(v_gc)     0,
	HAWK_SFN(val)      0
};

/* zero-length string */
static hawk_val_str_t hawk_zls = {
	HAWK_SFN(v_refs)   0,
	HAWK_SFN(v_type)   HAWK_VAL_STR,
	HAWK_SFN(v_static) 1,
	HAWK_SFN(v_nstr)   0,
	HAWK_SFN(v_gc)     0,
	HAWK_SFN(val)      { HAWK_T(""), 0 }
};
/* zero-length byte string */
static hawk_val_mbs_t hawk_zlbs = {
	HAWK_SFN(v_refs)   0,
	HAWK_SFN(v_type)   HAWK_VAL_MBS,
	HAWK_SFN(v_static) 1,
	HAWK_SFN(v_nstr)   0,
	HAWK_SFN(v_gc)     0,
	HAWK_SFN(val)      { HAWK_BT(""), 0 }
};

hawk_val_t* hawk_val_nil = (hawk_val_t*)&hawk_nil;
hawk_val_t* hawk_val_true = (hawk_val_t*)&hawk_true;
hawk_val_t* hawk_val_false = (hawk_val_t*)&hawk_false;
hawk_val_t* hawk_val_zls = (hawk_val_t*)&hawk_zls;
hawk_val_t* hawk_val_zlbs = (hawk_val_t*)&hawk_zlbs;

static const hawk_ooch_t* val_type_name[] =
{
	/* synchronize this table with enum hawk_val_type_t in hawk.h */
	HAWK_T("nil"),
	HAWK_T("bool"),
	HAWK_T("char"),
	HAWK_T("bchar"),
	HAWK_T("int"),
	HAWK_T("flt"),
	HAWK_T("str"),
	HAWK_T("mbs"),
	HAWK_T("fun"),
	HAWK_T("map"),
	HAWK_T("array"),

	HAWK_T("rex"),
	HAWK_T("ref"),
	HAWK_T("bob")
};

/* --------------------------------------------------------------------- */

#if defined(HAWK_ENABLE_GC)

/*
BEGIN {
  @local a, b, c, nil;
  for (i = 1; i < 10; i++) a[i] = i;
  a[11] = a;
  a[12] = a;
  a = @nil;
  b[1] = a;
  c[1] = 0;
}

BEGIN {
   @local a, b, c, nil;
   j[1] = 20;
   j[2] = 20;
   for (i = 1; i < 10; i++) a[i] = i;
   a[9] = j;
   a[10] = "world";
   a[11] = a;
   a[12] = a;
   a[13] = "hello";
   j[3] = a;
   a[14] = j;
   a = @nil;
   b[1] = a;
   c[1] = 0;
}

BEGIN {
   @local a, b, c, j, nil;
   j[1] = 20;
   j[2] = 20;
   for (i = 1; i < 10; i++) a[i] = i;
   a[9] = j;
   a[10] = "world";
   a[11] = a;
   a[12] = a;
   a[13] = "hello";
   j[3] = a;
   a[14] = j;
   a = @nil;
   b[1] = a;
   c[1] = 0;
}

BEGIN {
   @local a, b, c, nil;
   j[1] = 20;
   j[2] = 20;
   for (i = 1; i < 10; i++) a[i] = i;
   a[9] = j;
   a[10] = "world";
   a[11] = a;
   a[12] = a;
   a[13] = "hello";
   j[3] = a;
   a[14] = j;
   a = @nil;
   j = @nil; hawk::gc();
   b[1] = a;
   c[1] = 0;
}

*/

/*#define DEBUG_GC*/

#define GCH_MOVED HAWK_TYPE_MAX(hawk_uintptr_t)
#define GCH_UNREACHABLE (GCH_MOVED - 1)

static HAWK_INLINE void gc_chain_gch (hawk_gch_t* list, hawk_gch_t* gch)
{
	gch->gc_next = list;
	gch->gc_prev = list->gc_prev;
	gch->gc_prev->gc_next = gch;
	list->gc_prev = gch;
}

static HAWK_INLINE void gc_chain_val (hawk_gch_t* list, hawk_val_t* v)
{
	gc_chain_gch(list, hawk_val_to_gch(v));
}

static HAWK_INLINE void gc_move_all_gchs (hawk_gch_t* src, hawk_gch_t* dst)
{
	/* append src to dst */
	if (src->gc_next != src)
	{
		hawk_gch_t* last;

		last = dst->gc_prev;
		last->gc_next = src->gc_next;
		last->gc_next->gc_prev = last;
		dst->gc_prev = src->gc_prev;
		dst->gc_prev->gc_next = dst;
	}
	src->gc_prev = src;
	src->gc_next = src;
}

static HAWK_INLINE void gc_unchain_gch (hawk_gch_t* gch)
{
	gch->gc_prev->gc_next = gch->gc_next;
	gch->gc_next->gc_prev = gch->gc_prev;
}

static HAWK_INLINE void gc_unchain_val (hawk_val_t* v)
{
	gc_unchain_gch(hawk_val_to_gch(v));
}

static void gc_trace_refs (hawk_gch_t* list)
{
	hawk_gch_t* gch;
	hawk_val_t* v, * iv;
	hawk_map_itr_t itr;
	hawk_map_pair_t* pair;

	/* phase 1 - copy the reference count field from the value header to the gc header */
	gch = list->gc_next;
	while (gch != list)
	{
		gch->gc_refs = hawk_gch_to_val(gch)->v_refs;
		gch = gch->gc_next;
	}

	/* phase 2 - decrement the reference count in the gc header whenever a reference is found */
	gch = list->gc_next;
	while (gch != list)
	{
		v = hawk_gch_to_val(gch);

		if (v->v_type == HAWK_VAL_MAP)
		{
			hawk_map_t* map;

			map = ((hawk_val_map_t*)v)->map;
			hawk_init_map_itr(&itr, 0);
			pair = hawk_map_getfirstpair(map, &itr);
			while (pair)
			{
				iv = (hawk_val_t*)HAWK_MAP_VPTR(pair);
				if (HAWK_VTR_IS_POINTER(iv) && iv->v_gc)
				{
					hawk_val_to_gch(iv)->gc_refs--;
				}
				pair = hawk_map_getnextpair(map, &itr);
			}
		}
		else /* if (v->v_type == HAWK_VAL_ARR) */
		{
			hawk_oow_t size, i;
			hawk_arr_t* arr;

			HAWK_ASSERT(v->v_type == HAWK_VAL_ARR); /* only HAWK_VAL_MAP and HAWK_VAL_ARR */

			arr = ((hawk_val_arr_t*)v)->arr;
			size = HAWK_ARR_SIZE(arr);
			for (i = 0; i < size; i++)
			{
				if (HAWK_ARR_SLOT(arr, i))
				{
					iv = (hawk_val_t*)HAWK_ARR_DPTR(arr, i);
					if (HAWK_VTR_IS_POINTER(iv) && iv->v_gc)
					{
						hawk_val_to_gch(iv)->gc_refs--;
					}
				}
			}
		}

		gch = gch->gc_next;
	}
}

static void gc_dump_refs (hawk_rtx_t* rtx, hawk_gch_t* list)
{
	hawk_gch_t* gch;
	hawk_oow_t count  = 0;

	gch = list->gc_next;
	while (gch != list)
	{
		hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] GCH %p gc_refs %d\n", gch, (int)gch->gc_refs);
		gch = gch->gc_next;
	}
	hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] dumped %ju values\n", count);
}

static void gc_move_reachables (hawk_gch_t* list, hawk_gch_t* reachable_list)
{
	hawk_gch_t* gch, * tmp;
	hawk_val_t* v, * iv;
	hawk_map_itr_t itr;
	hawk_map_pair_t* pair;

	gch = list->gc_next;
	while (gch != list)
	{
		tmp = gch->gc_next;
		if (gch->gc_refs > 0)
		{
			gc_unchain_gch(gch);
			gc_chain_gch(reachable_list, gch);
			gch->gc_refs = GCH_MOVED;
		}
		gch = tmp;
	}

	gch = reachable_list->gc_next;
	while (gch != reachable_list)
	{
		v = hawk_gch_to_val(gch);

		if (v->v_type == HAWK_VAL_MAP)
		{
			hawk_map_t* map;

			/* the key part is a string. don't care. but if a generic value is allowed as a key, this should change... */
			map = ((hawk_val_map_t*)v)->map;

			hawk_init_map_itr (&itr, 0);
			pair = hawk_map_getfirstpair(map, &itr);
			while (pair)
			{
				iv = (hawk_val_t*)HAWK_MAP_VPTR(pair);
				if (HAWK_VTR_IS_POINTER(iv) && iv->v_gc)
				{
					tmp = hawk_val_to_gch(iv);
					if (tmp->gc_refs != GCH_MOVED)
					{
						gc_unchain_gch(tmp);
						gc_chain_gch(reachable_list, tmp);
						tmp->gc_refs = GCH_MOVED;
					}
				}
				pair = hawk_map_getnextpair(map, &itr);
			}
		}
		else /* if (v->v_type == HAWK_VAL_ARR) */
		{
			hawk_oow_t size, i;
			hawk_arr_t* arr;

			HAWK_ASSERT(v->v_type == HAWK_VAL_ARR); /* only HAWK_VAL_MAP and HAWK_VAL_ARR */

			arr = ((hawk_val_arr_t*)v)->arr;
			size = HAWK_ARR_SIZE(arr);
			for (i = 0; i < size; i++)
			{
				if (HAWK_ARR_SLOT(arr, i))
				{
					iv = (hawk_val_t*)HAWK_ARR_DPTR(arr, i);
					if (HAWK_VTR_IS_POINTER(iv) && iv->v_gc)
					{
						tmp = hawk_val_to_gch(iv);
						if (tmp->gc_refs != GCH_MOVED)
						{
							gc_unchain_gch(tmp);
							gc_chain_gch(reachable_list, tmp);
							tmp->gc_refs = GCH_MOVED;
						}
					}
				}
			}
		}

		gch = gch->gc_next;
	}
}

static HAWK_INLINE void gc_free_val (hawk_rtx_t* rtx, hawk_val_t* v)
{
	hawk_rtx_freemem(rtx, hawk_val_to_gch(v));
}

static void gc_free_unreachables (hawk_rtx_t* rtx, hawk_gch_t* list)
{
	hawk_gch_t* gch;

	/* there might be recursive cross references among unreachable values.
	 * simple traversal and sequential disposal causes various issues */

	/* mark all unreabale values */
	gch = list->gc_next;
	while (gch != list)
	{
		gch->gc_refs = GCH_UNREACHABLE;
	#if defined(DEBUG_GC)
		hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] MARKED UNREACHABLE GCH %p\n", gch);
	#endif
		gch = gch->gc_next;
	}

	/* free the values without actually freeing the outer shell */
	gch = list->gc_next;
	while (gch != list)
	{
		hawk_rtx_freeval(rtx, hawk_gch_to_val(gch), HAWK_RTX_FREEVAL_GC_PRESERVE);
		gch = gch->gc_next;
	}

	/* free the outer shell forcibly*/
	while (list->gc_next != list)
	{
		gch = list->gc_next; /* the first entry in the list */

	#if defined(DEBUG_GC)
		hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] FREEING UNREACHABLE GCH %p gc_refs %zu v_refs %zu\n", gch, gch->gc_refs, hawk_gch_to_val(gch)->v_refs);
	#endif
		/* do what hawk_rtx_freeval() would do without HAWK_RTX_FREEVAL_GC_PRESERVE */
		gc_unchain_gch(gch);
		gc_free_val(rtx, hawk_gch_to_val(gch));
	}
}

static HAWK_INLINE void gc_collect_garbage_in_generation (hawk_rtx_t* rtx, int gen)
{
	hawk_oow_t i, newgen;

#if defined(DEBUG_GC)
	hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR,  "[GC] **started - gen %d**\n", gen);
#endif

	newgen = (gen < HAWK_COUNTOF(rtx->gc.g) - 1)? (gen + 1): gen;
	for (i = 0; i < gen; i++)
	{
		gc_move_all_gchs (&rtx->gc.g[i], &rtx->gc.g[gen]);
	}

	if (rtx->gc.g[gen].gc_next != &rtx->gc.g[gen])
	{
		hawk_gch_t reachable;

		gc_trace_refs (&rtx->gc.g[gen]);

		reachable.gc_prev = &reachable;
		reachable.gc_next = &reachable;
		gc_move_reachables (&rtx->gc.g[gen], &reachable);

		/* only unreachables are left in rtx->gc.g[0] */
	#if defined(DEBUG_GC)
	/*gc_dump_refs(rtx, &rtx->gc.g[0]);*/
	#endif
		gc_free_unreachables(rtx, &rtx->gc.g[gen]);
		HAWK_ASSERT(rtx->gc.g[gen].gc_next == &rtx->gc.g[gen]);

		/* move all reachables back to the main list */
		gc_move_all_gchs (&reachable, &rtx->gc.g[newgen]);
	}

	/* [NOTE] pressure is greater than other elements by 1 in size.
	 *        i store the number of collections for gen 0 in pressure[1].
	 *        so i can avoid some comparison when doing this */
	rtx->gc.pressure[gen + 1]++; /* number of collections done for gen */
	rtx->gc.pressure[gen] = 0;   /* reset the number of collections of the previous generation */
	rtx->gc.pressure[0] = 0; /* reset the number of allocations since last gc. this line is redundant if gen is 0. */

#if defined(DEBUG_GC)
	hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR,  "[GC] **ended**\n");
#endif
}

static HAWK_INLINE int gc_collect_garbage_auto (hawk_rtx_t* rtx)
{
	hawk_oow_t i;

	i = HAWK_COUNTOF(rtx->gc.g);
	while (i > 1)
	{
		--i;
		if (rtx->gc.pressure[i] >= rtx->gc.threshold[i])
		{
			gc_collect_garbage_in_generation(rtx, i);
			return i;
		}
	}

	gc_collect_garbage_in_generation(rtx, 0);
	return 0;
}

int hawk_rtx_gc (hawk_rtx_t* rtx, int gen)
{
	if (gen < 0)
	{
		return gc_collect_garbage_auto (rtx);
	}
	else
	{
		if (gen >= HAWK_COUNTOF(rtx->gc.g)) gen = HAWK_COUNTOF(rtx->gc.g) - 1;
		gc_collect_garbage_in_generation(rtx, gen);
		return gen;
	}
}


static HAWK_INLINE hawk_val_t* gc_calloc_val (hawk_rtx_t* rtx, hawk_oow_t size)
{
	hawk_gch_t* gch;
	int gc_gen = 0;

	if (HAWK_UNLIKELY(rtx->gc.pressure[0] >= rtx->gc.threshold[0]))
	{
		/* invoke generational garbage collection */
		gc_gen = gc_collect_garbage_auto(rtx);
	}

	gch = (hawk_gch_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*gch) + size);
	if (HAWK_UNLIKELY(!gch))
	{
		if (gc_gen < HAWK_COUNTOF(rtx->gc.g) - 1)
		{
			/* perform full gc if full gc has not been triggerred at the beginning of this function */
			hawk_rtx_gc(rtx, HAWK_COUNTOF(rtx->gc.g) - 1);
		}
		gch = (hawk_gch_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*gch) + size);
		if (HAWK_UNLIKELY(!gch)) return HAWK_NULL;
	}

	rtx->gc.pressure[0]++; /* increment of the number of allocation attempt */
	return hawk_gch_to_val(gch);
}

#endif

/* --------------------------------------------------------------------- */

int hawk_get_val_type (hawk_val_t* val)
{
	return HAWK_GET_VAL_TYPE(val);
}

const hawk_ooch_t* hawk_get_val_type_name (hawk_val_t* val)
{
	return val_type_name[HAWK_GET_VAL_TYPE(val)];
}

hawk_val_t* hawk_get_nil_val (void)
{
	return (hawk_val_t*)&hawk_nil;
}

int hawk_rtx_isnilval (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return val == (hawk_val_t*)&hawk_nil || (HAWK_VTR_IS_POINTER(val) && val->v_type == HAWK_VAL_NIL);
}

hawk_val_t* hawk_rtx_makenilval (hawk_rtx_t* rtx)
{
	return (hawk_val_t*)&hawk_nil;
}

hawk_val_t* hawk_rtx_makebooltrueval (hawk_rtx_t* rtx)
{
	return (hawk_val_t*)&hawk_true;
}

hawk_val_t* hawk_rtx_makeboolfalseval (hawk_rtx_t* rtx)
{
	return (hawk_val_t*)&hawk_false;
}

hawk_val_t* hawk_rtx_makeboolval (hawk_rtx_t* rtx, int bv)
{
	return bv? (hawk_val_t*)&hawk_true: (hawk_val_t*)&hawk_false;
}

hawk_val_t* hawk_rtx_makecharval (hawk_rtx_t* rtx, hawk_ooch_t v)
{
	return HAWK_CHAR_TO_VTR((hawk_oochu_t)v);
}

hawk_val_t* hawk_rtx_makebchrval (hawk_rtx_t* rtx, hawk_bch_t v)
{
	return HAWK_BCHR_TO_VTR((hawk_bchu_t)v);
}

hawk_val_t* hawk_rtx_makeintval_full (hawk_rtx_t* rtx, hawk_int_t v)
{
	hawk_val_int_t* val;

	HAWK_ASSERT(!HAWK_IN_INT_RANGE(v));
	/*if (HAWK_IN_INT_RANGE(v)) return HAWK_INT_TO_VTR(v);*/

	if (!rtx->vmgr.ifree)
	{
		hawk_val_ichunk_t* c;
		/*hawk_val_int_t* x;*/
		hawk_oow_t i;

		/* use hawk_val_ichunk structure to avoid
		 * any alignment issues on platforms requiring
		 * aligned memory access - using the code commented out
		 * will cause a fault on such a platform */
		c = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(hawk_val_ichunk_t));
		if (HAWK_UNLIKELY(!c)) return HAWK_NULL;

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
	val->v_nstr = 0;
	val->v_gc = 0;
	val->i_val = v;
	val->nde = HAWK_NULL;

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("makeintval => %jd [%p] - [%O]\n"), (hawk_intmax_t)v, val, val);
#endif
	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makeintval (hawk_rtx_t* rtx, hawk_int_t v)
{
	return HAWK_IN_INT_RANGE(v)? HAWK_INT_TO_VTR(v): hawk_rtx_makeintval_full(rtx, v);
}

hawk_val_t* hawk_rtx_makefltval (hawk_rtx_t* rtx, hawk_flt_t v)
{
	hawk_val_flt_t* val;

	if (!rtx->vmgr.rfree)
	{
		hawk_val_rchunk_t* c;
		hawk_oow_t i;

		c = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(hawk_val_rchunk_t));
		if (HAWK_UNLIKELY(!c)) return HAWK_NULL;

		c->next = rtx->vmgr.rchunk;
		rtx->vmgr.rchunk = (hawk_val_chunk_t*)c;

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
	val->v_nstr = 0;
	val->v_gc = 0;
	val->val = v;
	val->nde = HAWK_NULL;

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("makefltval => %Lf [%p] - [%O]\n"), (double)v, val, val);
#endif
	return (hawk_val_t*)val;
}

static HAWK_INLINE hawk_val_t* make_str_val (hawk_rtx_t* rtx, const hawk_ooch_t* str1, hawk_oow_t len1, const hawk_ooch_t* str2, hawk_oow_t len2)
{
	hawk_val_str_t* val = HAWK_NULL;
	hawk_oow_t aligned_len;
#if defined(HAWK_ENABLE_STR_CACHE)
	hawk_oow_t i;
#endif

	if (HAWK_UNLIKELY(len1 <= 0 && len2 <= 0)) return hawk_val_zls;
	aligned_len = HAWK_ALIGN_POW2((len1 + len2 + 1), HAWK_STR_CACHE_BLOCK_UNIT);

#if defined(HAWK_ENABLE_STR_CACHE)
	i = aligned_len / HAWK_STR_CACHE_BLOCK_UNIT;
	if (i < HAWK_COUNTOF(rtx->str_cache_count))
	{
		if (rtx->str_cache_count[i] > 0)
		{
			val = rtx->str_cache[i][--rtx->str_cache_count[i]];
			goto init;
		}
	}
#endif

	val = (hawk_val_str_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_str_t) + (aligned_len * HAWK_SIZEOF(hawk_ooch_t)));
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

#if defined(HAWK_ENABLE_STR_CACHE)
init:
#endif
	val->v_type = HAWK_VAL_STR;
	val->v_refs = 0;
	val->v_static = 0;
	val->v_nstr = 0;
	val->v_gc = 0;
	val->val.len = len1 + len2;
	val->val.ptr = (hawk_ooch_t*)(val + 1);
	if (HAWK_LIKELY(str1)) hawk_copy_oochars_to_oocstr_unlimited(&val->val.ptr[0], str1, len1);
	if (str2) hawk_copy_oochars_to_oocstr_unlimited(&val->val.ptr[len1], str2, len2);
	val->val.ptr[val->val.len] = '\0';

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("make_str_val => %p - [%O]\n"), val, val);
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
	hawk_rtx_freemem(rtx, bcs);
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
	hawk_rtx_freemem(rtx, ucs);
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
	hawk_rtx_freemem(rtx, bcs);
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
	hawk_rtx_freemem(rtx, ucs);
	return v;
#else
	return make_str_val(rtx, bcs1, len1, bcs2, len2);
#endif
}

/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makenumorstrvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ptr, hawk_oow_t len, int mode)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	if (len == 1 && ptr[0] == '.') goto make_str;

	if (mode > 0 || (mode == 0 && HAWK_RTX_IS_NUMSTRDETECT_ON(rtx)))
	{
		// always on or auto detect
		x = hawk_uchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
		if (x == 0) return hawk_rtx_makeintval_inline(rtx, l);
		else if (x >= 1) return hawk_rtx_makefltval(rtx, r);
	}

make_str:
	return hawk_rtx_makestrvalwithuchars(rtx, ptr, len);
}

hawk_val_t* hawk_rtx_makenumorstrvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len, int mode)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	if (len == 1 && ptr[0] == '.') goto make_str;


	if (mode > 0 || (mode == 0 && HAWK_RTX_IS_NUMSTRDETECT_ON(rtx)))
	{
		// always on or auto detect
		x = hawk_bchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
		if (x == 0) return hawk_rtx_makeintval_inline(rtx, l);
		else if (x >= 1) return hawk_rtx_makefltval(rtx, r);
	}

make_str:
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
		HAWK_ASSERT(x == 0 || x == 1);
		v->v_nstr = x + 1; /* long -> 1, real -> 2 */
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
		HAWK_ASSERT(x == 0 || x == 1);
		v->v_nstr = x + 1; /* long -> 1, real -> 2 */
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

static HAWK_INLINE hawk_val_t* make_mbs_val (hawk_rtx_t* rtx, const hawk_bch_t* mbs1, hawk_oow_t len1, const hawk_bch_t* mbs2, hawk_oow_t len2)
{
	hawk_val_mbs_t* val = HAWK_NULL;
	hawk_oow_t aligned_len;
#if defined(HAWK_ENABLE_MBS_CACHE)
	hawk_oow_t i;
#endif

	if (HAWK_UNLIKELY(len1 <= 0 && len2 <= 0)) return hawk_val_zlbs;
	aligned_len = HAWK_ALIGN_POW2((len1 + len2 + 1), HAWK_MBS_CACHE_BLOCK_UNIT);

#if defined(HAWK_ENABLE_MBS_CACHE)
	i = aligned_len / HAWK_MBS_CACHE_BLOCK_UNIT;
	if (i < HAWK_COUNTOF(rtx->mbs_cache_count))
	{
		if (rtx->mbs_cache_count[i] > 0)
		{
			val = rtx->mbs_cache[i][--rtx->mbs_cache_count[i]];
			goto init;
		}
	}
#endif

	val = (hawk_val_mbs_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_mbs_t) + (aligned_len * HAWK_SIZEOF(hawk_bch_t)));
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

#if defined(HAWK_ENABLE_MBS_CACHE)
init:
#endif
	val->v_type = HAWK_VAL_MBS;
	val->v_refs = 0;
	val->v_static = 0;
	val->v_nstr = 0;
	val->v_gc = 0;
	val->val.len = len1 + len2;
	val->val.ptr = (hawk_bch_t*)(val + 1);
	if (HAWK_LIKELY(mbs1)) hawk_copy_bchars_to_bcstr_unlimited(&val->val.ptr[0], mbs1, len1);
	if (mbs2) hawk_copy_bchars_to_bcstr_unlimited(&val->val.ptr[len1], mbs2, len2);
	val->val.ptr[val->val.len] = '\0';

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("make_mbs_val => %p - [%O]\n"), val, val);
#endif
	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makembsvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len)
{
	return make_mbs_val(rtx, ptr, len, HAWK_NULL, 0);
}

hawk_val_t* hawk_rtx_makembsvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t len)
{
	hawk_val_t* val;
	hawk_bch_t* bcs;
	hawk_oow_t bcslen;

	if (HAWK_UNLIKELY(len <= 0)) return hawk_val_zlbs;

	bcs = hawk_rtx_duputobchars(rtx, ucs, len, &bcslen);
	if (HAWK_UNLIKELY(!bcs)) return HAWK_NULL;

	val = make_mbs_val(rtx, bcs, bcslen, HAWK_NULL, 0);
	hawk_rtx_freemem(rtx, bcs);

	return val;
}

hawk_val_t* hawk_rtx_makembsvalwithbcs (hawk_rtx_t* rtx, const hawk_bcs_t* bcs)
{
	return make_mbs_val(rtx, bcs->ptr, bcs->len, HAWK_NULL, 0);
}

hawk_val_t* hawk_rtx_makembsvalwithucs (hawk_rtx_t* rtx, const hawk_ucs_t* ucs)
{
	return hawk_rtx_makembsvalwithuchars(rtx, ucs->ptr, ucs->len);
}

hawk_val_t* hawk_rtx_makembsvalwithbcstr (hawk_rtx_t* rtx, const hawk_bch_t* bcs)
{
	return make_mbs_val(rtx, bcs, hawk_count_bcstr(bcs), HAWK_NULL, 0);
}

hawk_val_t* hawk_rtx_makembsvalwithucstr (hawk_rtx_t* rtx, const hawk_uch_t* ucs)
{
	return hawk_rtx_makembsvalwithuchars(rtx, ucs, hawk_count_ucstr(ucs));
}
/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makembsvalwithuchars2 (hawk_rtx_t* rtx, const hawk_uch_t* ucs1, hawk_oow_t len1, const hawk_uch_t* ucs2, hawk_oow_t len2)
{
	hawk_val_t* v;
	hawk_bch_t* bcs;
	hawk_oow_t bcslen;

	bcs = hawk_rtx_dupu2tobchars(rtx, ucs1, len1, ucs2, len2, &bcslen);
	if (HAWK_UNLIKELY(!bcs)) return HAWK_NULL;

	v = make_mbs_val(rtx, bcs, bcslen, HAWK_NULL, 0);
	hawk_rtx_freemem(rtx, bcs);
	return v;
}

hawk_val_t* hawk_rtx_makembsvalwithbchars2 (hawk_rtx_t* rtx, const hawk_bch_t* bcs1, hawk_oow_t len1, const hawk_bch_t* bcs2, hawk_oow_t len2)
{
	return make_mbs_val(rtx, bcs1, len1, bcs2, len2);
}

/* --------------------------------------------------------------------- */

hawk_val_t* hawk_rtx_makenumormbsvalwithuchars (hawk_rtx_t* rtx, const hawk_uch_t* ptr, hawk_oow_t len, int mode)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	if (len == 1 && ptr[0] == '.') goto make_str;

	if (mode > 0 || (mode == 0 && HAWK_RTX_IS_NUMSTRDETECT_ON(rtx)))
	{
		x = hawk_uchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
		if (x == 0) return hawk_rtx_makeintval_inline(rtx, l);
		else if (x >= 1) return hawk_rtx_makefltval(rtx, r);
	}

make_str:
	return hawk_rtx_makembsvalwithuchars(rtx, ptr, len);
}

hawk_val_t* hawk_rtx_makenumormbsvalwithbchars (hawk_rtx_t* rtx, const hawk_bch_t* ptr, hawk_oow_t len, int mode)
{
	int x;
	hawk_int_t l;
	hawk_flt_t r;

	if (len == 1 && ptr[0] == '.') goto make_str;

	if (mode > 0 || (mode == 0 && HAWK_RTX_IS_NUMSTRDETECT_ON(rtx)))
	{
		x = hawk_bchars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 1, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr, len, &l, &r);
		if (x == 0) return hawk_rtx_makeintval_inline(rtx, l);
		else if (x >= 1) return hawk_rtx_makefltval(rtx, r);
	}

make_str:
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
	val->v_nstr = 0;
	val->v_gc = 0;
	val->str.len = str->len;

	val->str.ptr = (hawk_ooch_t*)(val + 1);
	hawk_copy_oochars_to_oocstr_unlimited(val->str.ptr, str->ptr, str->len);

	val->code[0] = code[0];
	val->code[1] = code[1];

	return (hawk_val_t*)val;
}

/* --------------------------------------------------------------------- */

static void free_arrval (hawk_arr_t* arr, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_t* rtx = *(hawk_rtx_t**)hawk_arr_getxtn(arr);
	hawk_val_t* v = (hawk_val_t*)dptr;

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("refdown in arr free - [%O]\n"), v);
#endif

#if defined(HAWK_ENABLE_GC)
	if (HAWK_VTR_IS_POINTER(v) && v->v_gc && hawk_val_to_gch(v)->gc_refs == GCH_UNREACHABLE)
	{
		/* do nothing if the element is unreachable.
		 * this behavior pairs up with gc_free_unreachables() to
		 * achieve safe disposal of a value */
		return;
	}
#endif

	hawk_rtx_refdownval_inline(rtx, v);
}

static void same_arrval (hawk_arr_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_t* rtx = *(hawk_rtx_t**)hawk_arr_getxtn(map);
#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("refdown nofree in arr free - [%O]\n"), dptr);
#endif
	hawk_rtx_refdownval_nofree_inline(rtx, dptr);
}

hawk_val_t* hawk_rtx_makearrval (hawk_rtx_t* rtx, hawk_ooi_t init_capa)
{
	static hawk_arr_style_t style =
	{
	/* the key is copied inline into a pair and is freed when the pair
	 * is destroyed. not setting copier for a value means that the pointer
	 * to the data allocated somewhere else is remembered in a pair. but
	 * freeing the actual value is handled by free_arrval and same_arrval */

		HAWK_ARR_COPIER_DEFAULT,
		free_arrval,
		HAWK_ARR_COMPER_DEFAULT,
		same_arrval,
		HAWK_ARR_SIZER_DEFAULT
	};
#if defined(HAWK_ENABLE_GC)
	int retried = 0;
#endif
	hawk_val_arr_t* val;
	int x;

#if defined(HAWK_ENABLE_GC)
retry:
	val = (hawk_val_arr_t*)gc_calloc_val(rtx, HAWK_SIZEOF(hawk_val_arr_t) + HAWK_SIZEOF(hawk_arr_t) + HAWK_SIZEOF(rtx));
#else
	val = (hawk_val_arr_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_arr_t) + HAWK_SIZEOF(hawk_arr_t) + HAWK_SIZEOF(rtx));
#endif
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_ARR;
	val->v_refs = 0;
	val->v_static = 0;
	val->v_nstr = 0;
	val->v_gc = 0;
	val->arr = (hawk_arr_t*)(val + 1);

	if (init_capa < 0) init_capa = 64; /* TODO: what is the best initial value? */
	x = hawk_arr_init(val->arr, hawk_rtx_getgem(rtx), init_capa);
	if (HAWK_UNLIKELY(x <= -1))
	{
#if defined(HAWK_ENABLE_GC)
		gc_free_val(rtx, (hawk_val_t*)val);
		if (HAWK_LIKELY(!retried))
		{
			/* this arr involves non-gc allocatinon, which happens outside gc_calloc_val().
			 * reattempt to allocate after full gc like gc_calloc_val() */
			hawk_rtx_gc(rtx, HAWK_COUNTOF(rtx->gc.g) - 1);
			retried = 1;
			goto retry;
		}
#else
		hawk_rtx_freemem(rtx, val);
#endif
		return HAWK_NULL;
	}
	*(hawk_rtx_t**)hawk_arr_getxtn(val->arr) = rtx;
	hawk_arr_setstyle (val->arr, &style);

#if defined(HAWK_ENABLE_GC)
	gc_chain_val (&rtx->gc.g[0], (hawk_val_t*)val);
	val->v_gc = 1; /* only array and map are to be garbaged collected as of now */
	#if defined(DEBUG_GC)
	hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] MADE GCH %p VAL(ARR) %p\n", hawk_val_to_gch(val), val);
	#endif
#endif

	return (hawk_val_t*)val;
}

/* --------------------------------------------------------------------- */

static void free_mapval (hawk_map_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_t* rtx = *(hawk_rtx_t**)hawk_map_getxtn(map);
	hawk_val_t* v = (hawk_val_t*)dptr;

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("refdown in map free - [%O]\n"), v);
#endif

#if defined(HAWK_ENABLE_GC)
	if (HAWK_VTR_IS_POINTER(v) && v->v_gc && hawk_val_to_gch(v)->gc_refs == GCH_UNREACHABLE)
	{
		/* do nothing if the element is unreachable.
		 * this behavior pairs up with gc_free_unreachables() to
		 * achieve safe disposal of a value */
		return;
	}
#endif

	hawk_rtx_refdownval_inline(rtx, v);
}

static void same_mapval (hawk_map_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_t* rtx = *(hawk_rtx_t**)hawk_map_getxtn(map);
#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("refdown nofree in map free - [%O]\n"), dptr);
#endif
	hawk_rtx_refdownval_nofree_inline(rtx, dptr);
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
#if defined(HAWK_ENABLE_GC)
	int retried = 0;
#endif
	hawk_val_map_t* val;
	int x;

#if defined(HAWK_ENABLE_GC)
retry:
	val = (hawk_val_map_t*)gc_calloc_val(rtx, HAWK_SIZEOF(hawk_val_map_t) + HAWK_SIZEOF(hawk_map_t) + HAWK_SIZEOF(rtx));
#else
	val = (hawk_val_map_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_map_t) + HAWK_SIZEOF(hawk_map_t) + HAWK_SIZEOF(rtx));
#endif
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_MAP;
	val->v_refs = 0;
	val->v_static = 0;
	val->v_nstr = 0;
	val->v_gc = 0;
	val->map = (hawk_map_t*)(val + 1);

	x = hawk_map_init(val->map, hawk_rtx_getgem(rtx), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	if (HAWK_UNLIKELY(x <= -1))
	{
#if defined(HAWK_ENABLE_GC)
		gc_free_val(rtx, (hawk_val_t*)val);
		if (HAWK_LIKELY(!retried))
		{
			/* this map involves non-gc allocatinon, which happens outside gc_calloc_val().
			 * reattempt to allocate after full gc like gc_calloc_val() */
			hawk_rtx_gc(rtx, HAWK_COUNTOF(rtx->gc.g) - 1);
			retried = 1;
			goto retry;
		}
#else
		hawk_rtx_freemem(rtx, val);
#endif
		return HAWK_NULL;
	}
	*(hawk_rtx_t**)hawk_map_getxtn(val->map) = rtx;
	hawk_map_setstyle(val->map, &style);

#if defined(HAWK_ENABLE_GC)
	gc_chain_val(&rtx->gc.g[0], (hawk_val_t*)val);
	val->v_gc = 1; /* only array and map are to be garbaged collected as of now */
	#if defined(DEBUG_GC)
	hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] MADE GCH %p VAL(MAP) %p\n", hawk_val_to_gch(val), val);
	#endif
#endif

	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makemapvalwithdata (hawk_rtx_t* rtx, hawk_val_map_data_t data[], hawk_oow_t count)
{
	hawk_val_t* map, * tmp;

	hawk_oow_t i;

	map = hawk_rtx_makemapval(rtx);
	if (HAWK_UNLIKELY(!map)) return HAWK_NULL;

	hawk_rtx_refupval_inline(rtx, map);

	for (i = 0; i < count; i++)
	{
		hawk_val_map_data_t* p;

		p = &data[i];

		switch (p->type)
		{
			case HAWK_VAL_MAP_DATA_INT:
			{
				hawk_int_t iv = 0;

				if (p->type_size > 0 && p->type_size <= HAWK_SIZEOF(iv))
				{
				#if defined(HAWK_ENDIAN_LITTLE)
					HAWK_MEMCPY(&iv, p->vptr, p->type_size);
				#else
					HAWK_MEMCPY((hawk_uint8_t*)&iv + (HAWK_SIZEOF(iv) - p->type_size), p->vptr, p->type_size);
				#endif
				}
				else
				{
					iv = *(hawk_int_t*)p->vptr;
				}
				tmp = hawk_rtx_makeintval_inline(rtx, iv);
				break;
			}

			case HAWK_VAL_MAP_DATA_FLT:
				tmp = hawk_rtx_makefltval(rtx, *(hawk_flt_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_OOCSTR:
				tmp = hawk_rtx_makestrvalwithoocstr(rtx, (hawk_ooch_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_BCSTR:
				tmp = hawk_rtx_makestrvalwithbcstr(rtx, (hawk_bch_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_UCSTR:
				tmp = hawk_rtx_makestrvalwithucstr(rtx, (hawk_uch_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_OOCS:
				tmp = hawk_rtx_makestrvalwithoocs(rtx, (hawk_oocs_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_BCS:
				tmp = hawk_rtx_makestrvalwithbcs(rtx, (hawk_bcs_t*)p->vptr);
				break;

			case HAWK_VAL_MAP_DATA_UCS:
				tmp = hawk_rtx_makestrvalwithucs(rtx, (hawk_ucs_t*)p->vptr);
				break;

			default:
				tmp = HAWK_NULL;
				hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
				break;
		}

		if (tmp == HAWK_NULL || hawk_rtx_setmapvalfld(rtx, map, p->key.ptr, p->key.len, tmp) == HAWK_NULL)
		{
			if (tmp) hawk_rtx_freeval(rtx, tmp, HAWK_RTX_FREEVAL_CACHE);
			hawk_rtx_refdownval_inline(rtx, map);
			return HAWK_NULL;
		}
	}


	hawk_rtx_refdownval_nofree_inline(rtx, map);
	return map;
}

hawk_val_t* hawk_rtx_setmapvalfld (hawk_rtx_t* rtx, hawk_val_t* map, const hawk_ooch_t* kptr, hawk_oow_t klen, hawk_val_t* v)
{
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);

	if (hawk_map_upsert(((hawk_val_map_t*)map)->map, (hawk_ooch_t*)kptr, klen, v, 0) == HAWK_NULL) return HAWK_NULL;

	/* the value is passed in by an external party. we can't refup()
	 * and refdown() the value if htb_upsert() fails. that way, the value
	 * can be destroyed if it was passed with the reference count of 0.
	 * so we increment the reference count when htb_upsert() is complete */
	hawk_rtx_refupval_inline(rtx, v);

	return v;
}

hawk_val_t* hawk_rtx_getmapvalfld (hawk_rtx_t* rtx, hawk_val_t* map, const hawk_ooch_t* kptr, hawk_oow_t klen)
{
	hawk_map_pair_t* pair;

	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);

	pair = hawk_map_search(((hawk_val_map_t*)map)->map, kptr, klen);
	if (!pair)
	{
		/* the given key is not found in the map.
		 * we return NULL here as this function is called by
		 * a user unlike the hawk statement accessing the map key.
		 * so you can easily determine if the key is found by
		 * checking the error number.
		 */
		return HAWK_NULL;
	}

	return HAWK_MAP_VPTR(pair);
}

hawk_val_map_itr_t* hawk_rtx_getfirstmapvalitr (hawk_rtx_t* rtx, hawk_val_t* map, hawk_val_map_itr_t* itr)
{
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);
	hawk_init_map_itr(itr, 0); /* override the caller provided direction to 0 */
	itr->pair = hawk_map_getfirstpair(((hawk_val_map_t*)map)->map, itr);
	return itr->pair? itr: HAWK_NULL;
}

hawk_val_map_itr_t* hawk_rtx_getnextmapvalitr (hawk_rtx_t* rtx, hawk_val_t* map, hawk_val_map_itr_t* itr)
{
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, map) == HAWK_VAL_MAP);
	itr->pair = hawk_map_getnextpair(((hawk_val_map_t*)map)->map, itr);
	return itr->pair? itr: HAWK_NULL;
}

int hawk_rtx_scalearrval (hawk_rtx_t* rtx, hawk_val_t* arr, hawk_ooi_t capa)
{
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, arr) == HAWK_VAL_ARR);
	if (!hawk_arr_setcapa(((hawk_val_arr_t*)arr)->arr, capa)) return -1;
	return 0;
}

hawk_val_t* hawk_rtx_setarrvalfld (hawk_rtx_t* rtx, hawk_val_t* arr, hawk_ooi_t index, hawk_val_t* v)
{
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, arr) == HAWK_VAL_ARR);

	if (hawk_arr_upsert(((hawk_val_arr_t*)arr)->arr, index, v, 0) == HAWK_ARR_NIL) return HAWK_NULL;

	/* the value is passed in by an external party. we can't refup()
	 * and refdown() the value if htb_upsert() fails. that way, the value
	 * can be destroyed if it was passed with the reference count of 0.
	 * so we increment the reference count when htb_upsert() is complete */
	hawk_rtx_refupval_inline(rtx, v);

	return v;
}

hawk_val_t* hawk_rtx_getarrvalfld (hawk_rtx_t* rtx, hawk_val_t* arr, hawk_ooi_t index)
{
	hawk_arr_t* _arr;
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, arr) == HAWK_VAL_ARR);
	_arr = ((hawk_val_arr_t*)arr)->arr;
	if (index < 0 || index >= HAWK_ARR_SIZE(_arr) || !HAWK_ARR_SLOT(_arr, index)) return HAWK_NULL;
	return HAWK_ARR_DPTR(_arr, index);
}

hawk_ooi_t hawk_rtx_getarrvaltally (hawk_rtx_t* rtx, hawk_val_t* arr)
{
	hawk_arr_t* _arr;
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, arr) == HAWK_VAL_ARR);
	_arr = ((hawk_val_arr_t*)arr)->arr;
	return HAWK_ARR_TALLY(_arr);
}

hawk_val_arr_itr_t* hawk_rtx_getfirstarrvalitr (hawk_rtx_t* rtx, hawk_val_t* arr, hawk_val_arr_itr_t* itr)
{
	hawk_ptl_t* ptl;
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, arr) == HAWK_VAL_ARR);
	ptl = hawk_arr_getfirstelem(((hawk_val_arr_t*)arr)->arr, &itr->itr);
	if (!ptl) return HAWK_NULL;
	itr->elem = ptl->ptr;
	return itr;
}

hawk_val_arr_itr_t* hawk_rtx_getnextarrvalitr (hawk_rtx_t* rtx, hawk_val_t* arr, hawk_val_arr_itr_t* itr)
{
	hawk_ptl_t* ptl;
	HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, arr) == HAWK_VAL_ARR);
	ptl = hawk_arr_getnextelem(((hawk_val_arr_t*)arr)->arr, &itr->itr);
	if (!ptl) return HAWK_NULL;
	itr->elem = ptl->ptr;
	return itr;
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
	val->v_nstr = 0;
	val->v_gc = 0;
	val->fun = (hawk_fun_t*)fun;

	return (hawk_val_t*)val;
}

hawk_val_t* hawk_rtx_makebobval (hawk_rtx_t* rtx, const void* ptr, hawk_oow_t len)
{
	hawk_val_bob_t* val;

	val = (hawk_val_bob_t*)hawk_rtx_callocmem(rtx, HAWK_SIZEOF(hawk_val_bob_t) + len);
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

	val->v_type = HAWK_VAL_BOB;
	val->v_refs = 0;
	val->v_static = 0;
	val->v_nstr = 0;
	val->v_gc = 0;
	val->val.len = len;
	val->val.ptr = (hawk_bch_t*)(val + 1);
	HAWK_MEMCPY(val + 1, ptr, len);

#if defined(DEBUG_VAL)
	hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("make_bob_val => %p - [%O]\n"), val, val);
#endif
	return (hawk_val_t*)val;
}

int hawk_rtx_isstaticval (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return HAWK_VTR_IS_POINTER(val) && HAWK_IS_STATICVAL(val);
}

int hawk_rtx_isimmval (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return !HAWK_VTR_IS_POINTER(val);
}

int hawk_rtx_isimmorstaticval (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return !HAWK_VTR_IS_POINTER(val) || HAWK_IS_STATICVAL(val);
}

int hawk_rtx_getvaltype (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return HAWK_RTX_GETVALTYPE(rtx, val);
}

const hawk_ooch_t* hawk_rtx_getvaltypename(hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return val_type_name[HAWK_RTX_GETVALTYPE(rtx, val)];
}

int hawk_rtx_getintfromval (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	return HAWK_RTX_GETINTFROMVAL(rtx, val);
}

void hawk_rtx_freeval (hawk_rtx_t* rtx, hawk_val_t* val, int flags)
{
	hawk_val_type_t vtype;

	if (HAWK_VTR_IS_POINTER(val))
	{
		if (HAWK_IS_STATICVAL(val)) return;

	#if defined(DEBUG_VAL)
		hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("freeing [cache=%d] - [%O]\n"), cache, val);
	#endif

		vtype = HAWK_RTX_GETVALTYPE(rtx, val);
		switch (vtype)
		{
			case HAWK_VAL_NIL:
			case HAWK_VAL_BOOL:
			{
				hawk_rtx_freemem(rtx, val);
				break;
			}

			case HAWK_VAL_BCHR:
			case HAWK_VAL_CHAR:
			{
				/* this never happens */
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
			#if defined(HAWK_ENABLE_STR_CACHE)
				if (flags & HAWK_RTX_FREEVAL_CACHE)
				{
					hawk_val_str_t* v = (hawk_val_str_t*)val;
					hawk_oow_t aligned_len;
					int i;

					aligned_len = HAWK_ALIGN_POW2((v->val.len + 1), HAWK_STR_CACHE_BLOCK_UNIT);
					i = aligned_len / HAWK_STR_CACHE_BLOCK_UNIT;
					if (i < HAWK_COUNTOF(rtx->str_cache_count) &&
					    rtx->str_cache_count[i] < HAWK_COUNTOF(rtx->str_cache[i]))
					{
						rtx->str_cache[i][rtx->str_cache_count[i]++] = v;
						v->v_nstr = 0;
					}
					else hawk_rtx_freemem(rtx, val);
					break;
				}
			#endif

				hawk_rtx_freemem(rtx, val);
				break;
			}

			case HAWK_VAL_MBS:
			#if defined(HAWK_ENABLE_MBS_CACHE)
				if (flags & HAWK_RTX_FREEVAL_CACHE)
				{
					hawk_val_mbs_t* v = (hawk_val_mbs_t*)val;
					hawk_oow_t aligned_len;
					int i;

					aligned_len = HAWK_ALIGN_POW2((v->val.len + 1), HAWK_MBS_CACHE_BLOCK_UNIT);
					i = aligned_len / HAWK_MBS_CACHE_BLOCK_UNIT;
					if (i < HAWK_COUNTOF(rtx->mbs_cache_count) &&
					    rtx->mbs_cache_count[i] < HAWK_COUNTOF(rtx->mbs_cache[i]))
					{
						rtx->mbs_cache[i][rtx->mbs_cache_count[i]++] = v;
					}
					else hawk_rtx_freemem(rtx, val);
					break;
				}
			#endif

				hawk_rtx_freemem(rtx, val);
				break;

			case HAWK_VAL_REX:
			{
				/* don't free ptr as it is inlined to val
				hawk_rtx_freemem(rtx, ((hawk_val_rex_t*)val)->ptr);
				 */

				/* code is just a pointer to a regular expression stored
				 * in parse tree nodes. so don't free it.
				hawk_freerex (rtx->hawk, ((hawk_val_rex_t*)val)->code[0], ((hawk_val_rex_t*)val)->code[1]);
				 */

				hawk_rtx_freemem(rtx, val);
				break;
			}

			case HAWK_VAL_FUN:
				hawk_rtx_freemem(rtx, val);
				break;

			case HAWK_VAL_MAP:
			#if defined(HAWK_ENABLE_GC)

				#if defined(DEBUG_GC)
				hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] FREEING GCH %p VAL(MAP) %p - flags %d\n", hawk_val_to_gch(val), val, flags);
				#endif

				hawk_map_fini(((hawk_val_map_t*)val)->map);
				if (!(flags & HAWK_RTX_FREEVAL_GC_PRESERVE))
				{
					gc_unchain_val (val);
					gc_free_val(rtx, val);
				}
			#else
				hawk_map_fini(((hawk_val_map_t*)val)->map);
				hawk_rtx_freemem(rtx, val);
			#endif
				break;

			case HAWK_VAL_ARR:
			#if defined(HAWK_ENABLE_GC)

				#if defined(DEBUG_GC)
				hawk_logbfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, "[GC] FREEING GCH %p VAL(ARR) %p - flags %d\n", hawk_val_to_gch(val), val, flags);
				#endif

				hawk_arr_fini(((hawk_val_arr_t*)val)->arr);
				if (!(flags & HAWK_RTX_FREEVAL_GC_PRESERVE))
				{
					gc_unchain_val (val);
					gc_free_val(rtx, val);
				}
			#else
				hawk_arr_fini(((hawk_val_arr_t*)val)->arr);
				hawk_rtx_freemem(rtx, val);
			#endif
				break;

			case HAWK_VAL_REF:
				if ((flags & HAWK_RTX_FREEVAL_CACHE) && rtx->rcache_count < HAWK_COUNTOF(rtx->rcache))
				{
					rtx->rcache[rtx->rcache_count++] = (hawk_val_ref_t*)val;
				}
				else hawk_rtx_freemem(rtx, val);
				break;

			case HAWK_VAL_BOB:
				hawk_rtx_freemem(rtx, val);
				break;
		}
	}
}

void hawk_rtx_refupval(hawk_rtx_t* rtx, hawk_val_t* val)
{
	hawk_rtx_refupval_inline(rtx, val);
}

void hawk_rtx_refdownval(hawk_rtx_t* rtx, hawk_val_t* val)
{
	hawk_rtx_refdownval_inline(rtx, val);
}

void hawk_rtx_refdownval_nofree (hawk_rtx_t* rtx, hawk_val_t* val)
{
	hawk_rtx_refdownval_nofree_inline(rtx, val);
}

void hawk_rtx_freevalchunk (hawk_rtx_t* rtx, hawk_val_chunk_t* chunk)
{
	while (chunk != HAWK_NULL)
	{
		hawk_val_chunk_t* next = chunk->next;
		hawk_rtx_freemem(rtx, chunk);
		chunk = next;
	}
}

static int val_ref_to_bool (hawk_rtx_t* rtx, const hawk_val_ref_t* ref)
{
	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			hawk_oow_t idx = (hawk_oow_t)ref->adr;
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
			hawk_oow_t idx = (hawk_oow_t)ref->adr;
			return hawk_rtx_valtobool(rtx, HAWK_RTX_STACK_GBL(rtx, idx));
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;

			/* A reference value is not able to point to another
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, *xref)!= HAWK_VAL_REF);

			/* make a recursive call back to the caller */
			return hawk_rtx_valtobool(rtx, *xref);
		}
	}
}

int hawk_rtx_valtobool (hawk_rtx_t* rtx, const hawk_val_t* val)
{
	hawk_val_type_t vtype;

	HAWK_ASSERT(val != HAWK_NULL);

	vtype = HAWK_RTX_GETVALTYPE(rtx, val);
	switch (vtype)
	{
		case HAWK_VAL_NIL:
			return 0;

		case HAWK_VAL_BOOL:
			return ((hawk_val_bool_t*)val)->val != 0;

		case HAWK_VAL_BCHR:
		case HAWK_VAL_CHAR:
			/* return always true - treat it like a 1-letter string */
			return 1;

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
		case HAWK_VAL_ARR:
			return HAWK_ARR_SIZE(((hawk_val_arr_t*)val)->arr) > 0;
		case HAWK_VAL_REF:
			return val_ref_to_bool(rtx, (hawk_val_ref_t*)val);
		case HAWK_VAL_BOB:
			return ((hawk_val_bob_t*)val)->val.len > 0;
	}

	/* the type of a value should be one of HAWK_VAL_XXX enumerators defined in hawk-prv.h */
	HAWK_ASSERT(!"should never happen - invalid value type");

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
				hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
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
			hawk_ooecs_clear(out->u.strp);
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

	hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
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
				hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EBUFFULL);
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
			hawk_ooecs_clear(out->u.strp);
			if (hawk_uecs_ncatbchars(out->u.strp, str, str_len, hawk_rtx_getcmgr(rtx), 1) == (hawk_oow_t)-1) return -1;
			return 0;

		case HAWK_RTX_VALTOSTR_STRPCAT:
			if (hawk_uecs_ncatbchars(out->u.strpcat, str, str_len, hawk_rtx_getcmgr(rtx), 1) == (hawk_oow_t)-1) return -1;
			return 0;
	}

	hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
	return -1;
}
#endif


static int val_int_to_str (hawk_rtx_t* rtx, const hawk_val_int_t* v, hawk_rtx_valtostr_out_t* out)
{
	hawk_ooch_t* tmp;
	hawk_oow_t rlen = 0;
	int type = out->type & ~HAWK_RTX_VALTOSTR_PRINT;
	hawk_int_t orgval = HAWK_RTX_GETINTFROMVAL(rtx, v);
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
			HAWK_ASSERT(HAWK_SIZEOF(out->u.cpl) == HAWK_SIZEOF(out->u.cplcpy));

		case HAWK_RTX_VALTOSTR_CPLCPY:
			if (rlen >= out->u.cplcpy.len)
			{
				hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
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

			hawk_ooecs_clear(out->u.strp);
			HAWK_ASSERT(HAWK_OOECS_LEN(out->u.strp) == 0);

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
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
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

static int val_flt_to_str (hawk_rtx_t* rtx, const hawk_val_flt_t* v, hawk_rtx_valtostr_out_t* out, const hawk_rtx_gbl_t* gbl)
{
	hawk_ooch_t* tmp;
	hawk_oow_t tmp_len;
	hawk_ooecs_t* buf, * fbu;
	int type = out->type & ~HAWK_RTX_VALTOSTR_PRINT;

	if (out->type & HAWK_RTX_VALTOSTR_PRINT)
	{
		tmp = gbl->ofmt.ptr;
		tmp_len = gbl->ofmt.len;
	}
	else
	{
		tmp = gbl->convfmt.ptr;
		tmp_len = gbl->convfmt.len;
	}

	buf = &rtx->format.fltout;
	fbu = &rtx->format.fltfmt;
	hawk_ooecs_clear(buf);
	hawk_ooecs_clear(fbu);

	tmp = hawk_rtx_format(rtx, buf, fbu, tmp, tmp_len, (hawk_oow_t)-1, (hawk_nde_t*)v, &tmp_len);
	if (HAWK_UNLIKELY(!tmp)) goto oops;

	switch (type)
	{
		case HAWK_RTX_VALTOSTR_CPL:
			/* CPL and CPLCP behave the same for flt_t.
			 * i just fall through assuming that cplcpy
			 * and cpl are the same type. the following
			 * assertion at least ensure that they have
			 * the same size. */
			HAWK_ASSERT(HAWK_SIZEOF(out->u.cpl) == HAWK_SIZEOF(out->u.cplcpy));

		case HAWK_RTX_VALTOSTR_CPLCPY:
			if (out->u.cplcpy.len <= tmp_len)
			{
				hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
				/* store the buffer size required */
				out->u.cplcpy.len = tmp_len + 1;
				goto oops;
			}

			hawk_copy_oochars_to_oocstr_unlimited(out->u.cplcpy.ptr, tmp, tmp_len);
			out->u.cplcpy.len = tmp_len;
			break;

		case HAWK_RTX_VALTOSTR_CPLDUP:
		{
			out->u.cpldup.ptr = hawk_rtx_dupoochars(rtx, tmp, tmp_len);
			if (HAWK_UNLIKELY(!out->u.cpldup.ptr)) goto oops;
			out->u.cpldup.len = tmp_len;
			break;
		}

		case HAWK_RTX_VALTOSTR_STRP:
		{
			hawk_oow_t n;
			hawk_ooecs_clear(out->u.strp);
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
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
			goto oops;
		}
	}

	return 0;

oops:
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
			return hawk_rtx_valtostr(rtx, HAWK_RTX_STACK_GBL(rtx, idx), out);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;

			/* A reference value is not able to point to another
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, *xref) != HAWK_VAL_REF);

			/* make a recursive call back to the caller */
			return hawk_rtx_valtostr(rtx, *xref, out);
		}
	}
}

typedef struct valrepr_frame_t valrepr_frame_t;
struct valrepr_frame_t
{
	const hawk_val_t* val;
	hawk_map_pair_t* pair;
	hawk_oow_t index, remaining;
	int type, dense, first;
};

typedef struct valrepr_ctx_t valrepr_ctx_t;
struct valrepr_ctx_t
{
	hawk_ooecs_t text;
	hawk_rtx_t* rtx;
	hawk_rtx_valrepr_opt_t limit;
	valrepr_frame_t* stack;
	hawk_oow_t size, capa, closers, checkpoint, steps;
	int separator, truncated;
};

/* Reserve room for a truncation marker, its separator, and every open closer. */
#define VALREPR_TAIL_RESERVE 13

static int valrepr_append (valrepr_ctx_t* ctx, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t used = HAWK_OOECS_LEN(&ctx->text);
	hawk_oow_t capa = HAWK_OOECS_CAPA(&ctx->text);

	HAWK_ASSERT(used <= ctx->limit.max_chars && len <= ctx->limit.max_chars - used);
	if (len > capa - used)
	{
		/* Grow geometrically within the budget. Reserve explicitly so ECS
		 * doesn't retry successively smaller allocations after an OOM. */
		capa = capa > ctx->limit.max_chars / 2? ctx->limit.max_chars: capa * 2;
		if (capa < used + len) capa = used + len;
		if (hawk_ooecs_setcapa(&ctx->text, capa) == (hawk_oow_t)-1) return -1;
	}
	return hawk_ooecs_ncat(&ctx->text, ptr, len) == (hawk_oow_t)-1? -1: 0;
}

static int valrepr_step (valrepr_ctx_t* ctx)
{
	if (ctx->steps == 0)
	{
		ctx->truncated = 1;
		return -1;
	}
	ctx->steps--;
	return 0;
}

static int valrepr_put (valrepr_ctx_t* ctx, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t used = HAWK_OOECS_LEN(&ctx->text);

	if (ctx->closers > ctx->limit.max_chars - VALREPR_TAIL_RESERVE ||
	    used > ctx->limit.max_chars - VALREPR_TAIL_RESERVE - ctx->closers ||
	    len > ctx->limit.max_chars - VALREPR_TAIL_RESERVE - ctx->closers - used)
	{
		ctx->truncated = 1;
		return -1;
	}
	return valrepr_append(ctx, ptr, len);
}

static int valrepr_escape (valrepr_ctx_t* ctx, hawk_uint32_t c, hawk_ooch_t quote)
{
	static const hawk_ooch_t hex[] = HAWK_T("0123456789abcdef");
	hawk_ooch_t buf[10];
	hawk_oow_t i, digits;

	if (c == '\\' || (quote && c == quote))
	{
		buf[0] = '\\';
		buf[1] = c;
		return valrepr_put(ctx, buf, 2);
	}
	switch (c)
	{
		case '\n': return valrepr_put(ctx, HAWK_T("\\n"), 2);
		case '\r': return valrepr_put(ctx, HAWK_T("\\r"), 2);
		case '\t': return valrepr_put(ctx, HAWK_T("\\t"), 2);
	}
	if (c >= 0x20 && c <= 0x7e)
	{
		buf[0] = c;
		return valrepr_put(ctx, buf, 1);
	}
	digits = c <= 0xff? 2: c <= 0xffff? 4: 8;
	buf[0] = '\\';
	buf[1] = digits == 2? 'x': digits == 4? 'u': 'U';
	for (i = 0; i < digits; i++) buf[2 + i] = hex[(c >> ((digits - i - 1) * 4)) & 0xf];
	return valrepr_put(ctx, buf, digits + 2);
}

static int valrepr_quote (valrepr_ctx_t* ctx, const void* ptr, hawk_oow_t len, int bytes, hawk_ooch_t quote)
{
	hawk_oow_t i;

	if (bytes && valrepr_put(ctx, HAWK_T("@b"), 2) <= -1) return -1;
	if (valrepr_put(ctx, &quote, 1) <= -1) return -1;
	for (i = 0; i < len; i++)
	{
		hawk_uint32_t c;

		if (valrepr_step(ctx) <= -1) return -1;
		if (bytes) c = ((const hawk_uint8_t*)ptr)[i];
		else c = (hawk_oochu_t)((const hawk_ooch_t*)ptr)[i];
		if (valrepr_escape(ctx, c, quote) <= -1) return -1;
	}
	return valrepr_put(ctx, &quote, 1);
}

static int valrepr_integer (valrepr_ctx_t* ctx, hawk_uintmax_t value)
{
	hawk_ooch_t buf[HAWK_SIZEOF(hawk_uintmax_t) * 3 + 1];
	int n;

	n = hawk_fmt_uintmax_to_oocstr(buf, HAWK_COUNTOF(buf), value, 10, -1, 0, HAWK_NULL);
	if (n <= -1) return -1;
	return valrepr_put(ctx, buf, n);
}

static int valrepr_leaf (valrepr_ctx_t* ctx, const hawk_val_t* val, int type)
{
	switch (type)
	{
		case HAWK_VAL_NIL:
			return valrepr_put(ctx, HAWK_T("@nil"), 4);
		case HAWK_VAL_BOOL:
			return ((const hawk_val_bool_t*)val)->val?
				valrepr_put(ctx, HAWK_T("@true"), 5): valrepr_put(ctx, HAWK_T("@false"), 6);
		case HAWK_VAL_CHAR:
		{
			hawk_ooch_t c = HAWK_RTX_GETCHARFROMVAL(ctx->rtx, val);
			return valrepr_quote(ctx, &c, 1, 0, '\'');
		}
		case HAWK_VAL_BCHR:
		{
			hawk_bch_t c = HAWK_RTX_GETBCHRFROMVAL(ctx->rtx, val);
			return valrepr_quote(ctx, &c, 1, 1, '\'');
		}
		case HAWK_VAL_STR:
			return valrepr_quote(ctx, ((const hawk_val_str_t*)val)->val.ptr,
				((const hawk_val_str_t*)val)->val.len, 0, '"');
		case HAWK_VAL_MBS:
			return valrepr_quote(ctx, ((const hawk_val_mbs_t*)val)->val.ptr,
				((const hawk_val_mbs_t*)val)->val.len, 1, '"');
		case HAWK_VAL_INT:
		{
			hawk_int_t n = HAWK_RTX_GETINTFROMVAL(ctx->rtx, val);
			hawk_uint_t u = (hawk_uint_t)n;

			if (n < 0)
			{
				if (valrepr_put(ctx, HAWK_T("-"), 1) <= -1) return -1;
				u = (hawk_uint_t)0 - u;
			}
			return valrepr_integer(ctx, u);
		}
		case HAWK_VAL_FLT:
		{
			hawk_ooch_t buf[128];
			hawk_flt_t n = ((const hawk_val_flt_t*)val)->val;
			hawk_oow_t len, i;

			/* Fixed precision bounds memory and avoids user CONVFMT/OFMT calling
			 * back into value formatting. Diagnostic numbers need not round-trip. */
		#if defined(HAWK_USE_FLTMAX)
			len = hawk_rtx_fmttooocstr(ctx->rtx, buf, HAWK_COUNTOF(buf), HAWK_T("%.17jjg"), &n);
		#else
			len = hawk_rtx_fmttooocstr(ctx->rtx, buf, HAWK_COUNTOF(buf), HAWK_T("%.17zg"), n);
		#endif
			if (len == (hawk_oow_t)-1) return -1;
			/* Escape a non-ASCII locale decimal separator as well. */
			for (i = 0; i < len; i++)
			{
				if (valrepr_step(ctx) <= -1 || valrepr_escape(ctx, (hawk_oochu_t)buf[i], 0) <= -1) return -1;
			}
			return 0;
		}
		case HAWK_VAL_FUN:
		{
			const hawk_oocs_t* name = &((const hawk_val_fun_t*)val)->fun->name;
			hawk_oow_t i;

			if (!name->len) return valrepr_put(ctx, HAWK_T("<FUN>"), 5);
			if (valrepr_put(ctx, HAWK_T("<FUN:"), 5) <= -1) return -1;
			for (i = 0; i < name->len; i++)
			{
				if (valrepr_step(ctx) <= -1 || valrepr_escape(ctx, (hawk_oochu_t)name->ptr[i], 0) <= -1) return -1;
			}
			return valrepr_put(ctx, HAWK_T(">"), 1);
		}
		case HAWK_VAL_REX:
			if (valrepr_put(ctx, HAWK_T("<REX:"), 5) <= -1 ||
			    valrepr_quote(ctx, ((const hawk_val_rex_t*)val)->str.ptr,
			        ((const hawk_val_rex_t*)val)->str.len, 0, '"') <= -1) return -1;
			return valrepr_put(ctx, HAWK_T(">"), 1);
		case HAWK_VAL_BOB:
			if (valrepr_put(ctx, HAWK_T("<BOB:"), 5) <= -1 ||
			    valrepr_integer(ctx, ((const hawk_val_bob_t*)val)->val.len) <= -1) return -1;
			return valrepr_put(ctx, HAWK_T(" bytes>"), 7);
		default:
			hawk_rtx_seterrnum(ctx->rtx, HAWK_NULL, HAWK_EVALTOSTR);
			return -1;
	}
}

static hawk_map_pair_t* valrepr_map_next (valrepr_ctx_t* ctx, valrepr_frame_t* frame)
{
	hawk_map_t* map = ((const hawk_val_map_t*)frame->val)->map;
	hawk_map_pair_t* pair;

#if defined(HAWK_MAP_IS_HTB)
	pair = frame->pair;
	while (!pair && frame->index < map->capa)
	{
		if (valrepr_step(ctx) <= -1) return HAWK_NULL;
		pair = map->bucket[frame->index++];
	}
	if (pair) frame->pair = pair->next;
#else
	/* Walk links directly so iterator protection cannot retain addresses in
	 * the reallocatable frame array. Charge each tree step to the work budget. */
	pair = frame->pair;
	if (!pair || pair->child[1] != &map->xnil)
	{
		pair = pair? pair->child[1]: map->root;
		while (pair != &map->xnil && pair->child[0] != &map->xnil)
		{
			if (valrepr_step(ctx) <= -1) return HAWK_NULL;
			pair = pair->child[0];
		}
	}
	else
	{
		while (pair->parent != &map->xnil && pair == pair->parent->child[1])
		{
			if (valrepr_step(ctx) <= -1) return HAWK_NULL;
			pair = pair->parent;
		}
		pair = pair->parent;
	}
	if (pair == &map->xnil) pair = HAWK_NULL;
	frame->pair = pair;
#endif
	return pair;
}

static const hawk_ooch_t* valrepr_closer (const valrepr_frame_t* frame, hawk_oow_t* len)
{
	*len = 1;
	return frame->type == HAWK_VAL_MAP? HAWK_T("}"): HAWK_T("]");
}

int hawk_rtx_valtorepr (hawk_rtx_t* rtx, const hawk_val_t* val, hawk_rtx_valtostr_out_t* out, const hawk_rtx_valrepr_opt_t* opt)
{
	valrepr_ctx_t ctx;
	const hawk_val_t* current = val;
	hawk_oow_t len;
	int mode, type, n = -1;

	HAWK_MEMSET(&ctx, 0, HAWK_SIZEOF(ctx));
	ctx.rtx = rtx;
	ctx.limit.max_depth = opt && opt->max_depth? opt->max_depth: HAWK_RTX_VALREPR_DEFAULT_MAX_DEPTH;
	ctx.limit.max_chars = opt && opt->max_chars? opt->max_chars: HAWK_RTX_VALREPR_DEFAULT_MAX_CHARS;
	ctx.limit.max_steps = opt && opt->max_steps? opt->max_steps: HAWK_RTX_VALREPR_DEFAULT_MAX_STEPS;
	mode = out->type & ~(HAWK_RTX_VALTOSTR_PRINT | HAWK_RTX_VALTOSTR_REPR);
	if (mode < HAWK_RTX_VALTOSTR_CPL || mode > HAWK_RTX_VALTOSTR_STRPCAT ||
	    ctx.limit.max_chars < 16 || ctx.limit.max_chars > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(hawk_ooch_t) - 1)
	{
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}
	/* Every container also consumes output, so no useful stack can be larger. */
	if (ctx.limit.max_depth > ctx.limit.max_chars) ctx.limit.max_depth = ctx.limit.max_chars;
	if (ctx.limit.max_depth > HAWK_TYPE_MAX(hawk_oow_t) / HAWK_SIZEOF(*ctx.stack))
	{
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}
	ctx.steps = ctx.limit.max_steps;
	if (hawk_ooecs_init(&ctx.text, hawk_rtx_getgem(rtx), ctx.limit.max_chars < 128? ctx.limit.max_chars: 128) <= -1) return -1;

	for (;;)
	{
		if (current)
		{
			hawk_oow_t i;

			if (valrepr_step(&ctx) <= -1) goto stopped;
			type = HAWK_RTX_GETVALTYPE(rtx, current);
			if (type == HAWK_VAL_REF)
			{
				current = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)current);
				if (!current) goto done;
				continue;
			}
			if (type == HAWK_VAL_MAP || type == HAWK_VAL_ARR)
			{
				valrepr_frame_t* frame;
				const hawk_ooch_t* opener;
				hawk_oow_t close_len;

				for (i = 0; i < ctx.size; i++)
				{
					if (valrepr_step(&ctx) <= -1) goto stopped;
					if (ctx.stack[i].val == current) break;
				}
				if (i < ctx.size || ctx.size >= ctx.limit.max_depth)
				{
					if (valrepr_put(&ctx, i < ctx.size? HAWK_T("<cycle>"): HAWK_T("<depth-limit>"), i < ctx.size? 7: 13) <= -1) goto stopped;
					current = HAWK_NULL;
					continue;
				}
				if (ctx.size == ctx.capa)
				{
					hawk_oow_t capa = ctx.capa? ctx.capa: 4;
					void* tmp;

					if (ctx.capa) capa = ctx.capa > ctx.limit.max_depth / 2? ctx.limit.max_depth: ctx.capa * 2;
					if (capa > ctx.limit.max_depth) capa = ctx.limit.max_depth;
					tmp = hawk_rtx_reallocmem(rtx, ctx.stack, capa * HAWK_SIZEOF(*ctx.stack));
					if (!tmp) goto done;
					ctx.stack = tmp;
					ctx.capa = capa;
				}
				frame = &ctx.stack[ctx.size];
				HAWK_MEMSET(frame, 0, HAWK_SIZEOF(*frame));
				frame->val = current;
				frame->type = type;
				frame->first = 1;
				if (type == HAWK_VAL_ARR)
				{
					hawk_arr_t* arr = ((const hawk_val_arr_t*)current)->arr;

					frame->remaining = HAWK_ARR_TALLY(arr);
					frame->dense = !frame->remaining || (HAWK_ARR_SIZE(arr) - 1 == frame->remaining && !HAWK_ARR_SLOT(arr, 0));
					opener = HAWK_T("@[");
				}
				else
				{
					frame->remaining = HAWK_MAP_SIZE(((const hawk_val_map_t*)current)->map);
					opener = HAWK_T("@{");
				}
				valrepr_closer(frame, &close_len);
				ctx.closers += close_len;
				if (valrepr_put(&ctx, opener, 2) <= -1)
				{
					ctx.closers -= close_len;
					goto stopped;
				}
				ctx.size++;
			}
			else if (valrepr_leaf(&ctx, current, type) <= -1) goto stopped;
			current = HAWK_NULL;
		}
		if (!ctx.size) break;
		{
			valrepr_frame_t* frame = &ctx.stack[ctx.size - 1];

			ctx.checkpoint = HAWK_OOECS_LEN(&ctx.text);
			ctx.separator = !frame->first;
			if (!frame->remaining)
			{
				const hawk_ooch_t* closer = valrepr_closer(frame, &len);

				ctx.size--;
				ctx.closers -= len;
				if (valrepr_put(&ctx, closer, len) <= -1) goto stopped;
				continue;
			}
			if (!frame->first && valrepr_put(&ctx, HAWK_T(", "), 2) <= -1) goto stopped;
			if (frame->type == HAWK_VAL_MAP)
			{
				hawk_map_pair_t* pair = valrepr_map_next(&ctx, frame);

				if (!pair) goto stopped;
				if (valrepr_quote(&ctx, HAWK_MAP_KPTR(pair), HAWK_MAP_KLEN(pair), 0, '"') <= -1 ||
				    valrepr_put(&ctx, HAWK_T(": "), 2) <= -1) goto stopped;
				current = HAWK_MAP_VPTR(pair);
			}
			else
			{
				hawk_arr_t* arr = ((const hawk_val_arr_t*)frame->val)->arr;

				while (frame->index < HAWK_ARR_SIZE(arr))
				{
					if (valrepr_step(&ctx) <= -1) goto stopped;
					if (HAWK_ARR_SLOT(arr, frame->index)) break;
					frame->index++;
				}
				if (frame->index >= HAWK_ARR_SIZE(arr)) goto stopped;
				if (!frame->dense && (valrepr_integer(&ctx, frame->index) <= -1 ||
				    valrepr_put(&ctx, HAWK_T(": "), 2) <= -1)) goto stopped;
				current = HAWK_ARR_DPTR(arr, frame->index++);
			}
			frame->remaining--;
			frame->first = 0;
		}
	}
	goto copy_result;

stopped:
	if (!ctx.truncated) goto done;
	/* Rewind the unfinished member, not just its last escape/character. The
	 * reserved tail makes truncation explicit without leaving open delimiters. */
	hawk_ooecs_setlen(&ctx.text, ctx.checkpoint);
	if (ctx.separator && valrepr_append(&ctx, HAWK_T(", "), 2) <= -1) goto done;
	if (valrepr_append(&ctx, HAWK_T("<truncated>"), 11) <= -1) goto done;
	while (ctx.size > 0)
	{
		const hawk_ooch_t* closer = valrepr_closer(&ctx.stack[--ctx.size], &len);

		if (valrepr_append(&ctx, closer, len) <= -1) goto done;
	}

copy_result:
	if (mode == HAWK_RTX_VALTOSTR_CPLDUP)
	{
		n = hawk_ooecs_yield(&ctx.text, &out->u.cpldup, 0);
	}
	else
	{
		hawk_rtx_valtostr_out_t dst = *out;

		dst.type = mode == HAWK_RTX_VALTOSTR_CPL? HAWK_RTX_VALTOSTR_CPLCPY: mode;
		if (dst.type == HAWK_RTX_VALTOSTR_CPLCPY && dst.u.cplcpy.len <= HAWK_OOECS_LEN(&ctx.text))
		{
			out->u.cplcpy.len = HAWK_OOECS_LEN(&ctx.text) + 1;
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EBUFFULL);
			goto done;
		}
		n = str_to_str(rtx, HAWK_OOECS_PTR(&ctx.text), HAWK_OOECS_LEN(&ctx.text), &dst);
		if (n >= 0) out->u = dst.u;
	}

done:
	if (ctx.stack) hawk_rtx_freemem(rtx, ctx.stack);
	hawk_ooecs_fini(&ctx.text);
	return n;
}

int hawk_rtx_valtostr (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_rtx_valtostr_out_t* out)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	if (out->type & HAWK_RTX_VALTOSTR_REPR) return hawk_rtx_valtorepr(rtx, v, out, HAWK_NULL);

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			return str_to_str(rtx, HAWK_T(""), 0, out);

		case HAWK_VAL_BOOL:
			return str_to_str(rtx, (((hawk_val_bool_t*)v)->val?  HAWK_T("1"): HAWK_T("0")), 1, out);

		case HAWK_VAL_BCHR:
		{
			hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
		#if defined(HAWK_OOCH_IS_BCH)
			return str_to_str(rtx, &tmp, 1, out);
		#else
			return mbs_to_str(rtx, &tmp, 1, out);
		#endif
		}

		case HAWK_VAL_CHAR:
		{
			hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
			return str_to_str(rtx, &tmp, 1, out);
		}

		case HAWK_VAL_INT:
			return val_int_to_str(rtx, (hawk_val_int_t*)v, out);

		case HAWK_VAL_FLT:
			return val_flt_to_str(rtx, (hawk_val_flt_t*)v, out, &rtx->gbl);

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
				return str_to_str(rtx, HAWK_T("<MAP>"), 5, out);
			}
			goto invalid;

		case HAWK_VAL_ARR:
			if (rtx->hawk->opt.trait & HAWK_FLEXMAP)
			{
				return str_to_str(rtx, HAWK_T("<ARRAY>"), 7, out);
			}
			goto invalid;

		case HAWK_VAL_REF:
			return val_ref_to_str(rtx, (hawk_val_ref_t*)v, out);

		case HAWK_VAL_REX:
		default:
		invalid:
		#if defined(DEBUG_VAL)
			hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T(">>WRONG VALUE TYPE [%d] in hawk_rtx_valtostr\n"), v->type);
		#endif
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EVALTOSTR);
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
		case HAWK_VAL_BCHR:
		{
			hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
			mbs = hawk_rtx_dupbchars(rtx, &tmp, 1);
			if (HAWK_UNLIKELY(!mbs)) return HAWK_NULL;
			if (len) *len = 1;
			break;
		}

		case HAWK_VAL_MBS:
			mbs = hawk_rtx_dupbchars(rtx, ((hawk_val_mbs_t*)v)->val.ptr, ((hawk_val_mbs_t*)v)->val.len);
			if (HAWK_UNLIKELY(!mbs)) return HAWK_NULL;
			if (len) *len = ((hawk_val_mbs_t*)v)->val.len;
			break;

		case HAWK_VAL_BOB:
			mbs = hawk_rtx_dupbchars(rtx, (hawk_bch_t*)((hawk_val_bob_t*)v)->val.ptr, ((hawk_val_bob_t*)v)->val.len);
			if (HAWK_UNLIKELY(!mbs)) return HAWK_NULL;
			if (len) *len = ((hawk_val_mbs_t*)v)->val.len;
			break;

		case HAWK_VAL_CHAR:
		case HAWK_VAL_STR:
		{
			const hawk_ooch_t* ptr;
			hawk_oow_t slen;
			hawk_ooch_t tmp;

			if (vtype == HAWK_VAL_CHAR)
			{
				tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
				ptr = &tmp;
				slen = 1;
			}
			else
			{
				ptr = ((hawk_val_str_t*)v)->val.ptr;
				slen = ((hawk_val_str_t*)v)->val.len;
			}
		#if defined(HAWK_OOCH_IS_BCH)
			mbs = hawk_rtx_dupbchars(rtx, ptr, slen);
			if (HAWK_UNLIKELY(!mbs)) return HAWK_NULL;
			if (len) *len = slen;
		#else
			{
				hawk_oow_t mbslen, wcslen;
				wcslen = slen;
				mbs = hawk_rtx_duputobcharswithcmgr(rtx, ptr, wcslen, &mbslen, cmgr);
				if (HAWK_UNLIKELY(!mbs)) return HAWK_NULL;
				if (len) *len = mbslen;
			}
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
			hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
			if (HAWK_UNLIKELY(!mbs)) return HAWK_NULL;
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
		case HAWK_VAL_BCHR:
		{
			hawk_bch_t tmp;
			hawk_oow_t mbslen, wcslen;

			tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
			mbslen = 1;
			wcs = hawk_rtx_dupbtoucharswithcmgr(rtx, &tmp, mbslen, &wcslen, cmgr, 1);
			if (!wcs) return HAWK_NULL;
			if (len) *len = wcslen;
			break;
		}
		case HAWK_VAL_MBS:
		{
			hawk_oow_t mbslen, wcslen;
			mbslen = ((hawk_val_mbs_t*)v)->val.len;
			wcs = hawk_rtx_dupbtoucharswithcmgr(rtx, ((hawk_val_mbs_t*)v)->val.ptr, mbslen, &wcslen, cmgr, 1);
			if (!wcs) return HAWK_NULL;
			if (len) *len = wcslen;
			break;
		}

		case HAWK_VAL_CHAR:
		{
			hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
		#if defined(HAWK_OOCH_IS_BCH)
			hawk_oow_t wcslen, mbslen;
			mbslen = 1;
			wcs = hawk_rtx_dupbtoucharswithcmgr(rtx, &tmp, mbslen, &wcslen, cmgr, 1);
		#else
			wcs = hawk_rtx_dupuchars(rtx, &tmp, 1);
		#endif
			if (!wcs) return HAWK_NULL;
		#if defined(HAWK_OOCH_IS_BCH)
			if (len) *len = wcslen;
		#else
			if (len) *len = 1;
		#endif
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
			hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
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
	hawk_ooch_t c;
	hawk_oow_t l;

	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_NIL:
			c = '\0';
			l = 0;
			goto ctos;
		case HAWK_VAL_BOOL:
			c = ((hawk_val_bool_t*)v)->val? '1': '0';
			l = 1;
			goto ctos;
		case HAWK_VAL_CHAR:
			c = HAWK_RTX_GETCHARFROMVAL(rtx, v);
			l = 1;
		ctos:
			if (rtx->ctos.fi) /* free slot available */
			{
				/* use a ctos slot to avoid duplication */
				hawk_oow_t fi;
				fi = rtx->ctos.fi;
				rtx->ctos.fi = rtx->ctos.b[rtx->ctos.fi].c[0];
				rtx->ctos.b[fi].c[0] = c;
				rtx->ctos.b[fi].c[1] = '\0';
				if (len) *len = l;
				HAWK_ASSERT((void*)&rtx->ctos.b[fi] == (void*)rtx->ctos.b[fi].c);
				return rtx->ctos.b[fi].c;
			}
			goto duplicate;

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
		duplicate:
			return hawk_rtx_valtooocstrdupwithcmgr(rtx, v, len, cmgr);
	}
}

void hawk_rtx_freevaloocstr(hawk_rtx_t* rtx, hawk_val_t* v, hawk_ooch_t* str)
{
	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_NIL:
		case HAWK_VAL_BOOL:
		case HAWK_VAL_CHAR:
		{
			hawk_ctos_b_t* b = (hawk_ctos_b_t*)str;
			if (b >= &rtx->ctos.b[0] && b < &rtx->ctos.b[HAWK_COUNTOF(rtx->ctos.b)])
			{
				hawk_oow_t fi;
				fi = b - &rtx->ctos.b[0];
				rtx->ctos.b[fi].c[0] = rtx->ctos.fi;
				rtx->ctos.fi = fi;
				break;
			}

			goto freemem;
		}

		case HAWK_VAL_STR:
#if 0
		plain_str:
#endif
			if (str != ((hawk_val_str_t*)v)->val.ptr) hawk_rtx_freemem(rtx, str);
			break;

#if 0
		case HAWK_VAL_REF:
			v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			if (v && HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_STR) goto plain_str;
			/* fall through */
#endif

		default:
		freemem:
			hawk_rtx_freemem(rtx, str);
			break;
	}
}

hawk_bch_t* hawk_rtx_getvalbcstrwithcmgr (hawk_rtx_t* rtx, hawk_val_t* v, hawk_oow_t* len, hawk_cmgr_t* cmgr)
{
	hawk_bch_t c;
	hawk_oow_t l;

	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_NIL:
			c = '\0';
			l = 0;
			goto bctos;

		case HAWK_VAL_BOOL:
			c = ((hawk_val_bool_t*)v)->val? '1': '0';
			l = 1;
			goto bctos;

		case HAWK_VAL_BCHR:
			c = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
			l = 1;
		bctos:
			if (rtx->bctos.fi) /* free slot available */
			{
				/* use a bctos slot to avoid duplication */
				hawk_oow_t fi;
				fi = rtx->bctos.fi;
				rtx->bctos.fi = rtx->bctos.b[rtx->bctos.fi].c[0];
				rtx->bctos.b[fi].c[0] = c;
				rtx->bctos.b[fi].c[1] = '\0';
				if (len) *len = l;
				HAWK_ASSERT((void*)&rtx->bctos.b[fi] == (void*)rtx->bctos.b[fi].c);
				return (hawk_bch_t*)rtx->bctos.b[fi].c; /* type-cast hawk_bchu_t* to hawk_bch_t* */
			}
			goto duplicate;

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

		case HAWK_VAL_BOB: /* BOB must be duplicated */
		default:
		duplicate:
			return hawk_rtx_valtobcstrdupwithcmgr(rtx, v, len, cmgr);
	}
}

void hawk_rtx_freevalbcstr (hawk_rtx_t* rtx, hawk_val_t* v, hawk_bch_t* str)
{
	switch (HAWK_RTX_GETVALTYPE(rtx, v))
	{
		case HAWK_VAL_NIL:
		case HAWK_VAL_BOOL:
		case HAWK_VAL_BCHR:
		{
			hawk_bctos_b_t* b = (hawk_bctos_b_t*)str;
			if (b >= &rtx->bctos.b[0] && b < &rtx->bctos.b[HAWK_COUNTOF(rtx->bctos.b)])
			{
				hawk_oow_t fi;
				fi = b - &rtx->bctos.b[0];
				rtx->bctos.b[fi].c[0] = rtx->bctos.fi;
				rtx->bctos.fi = fi;
				break;
			}

			goto freemem;
		}

		case HAWK_VAL_MBS:
#if 0
		plain_mbs:
#endif
			if (str != ((hawk_val_mbs_t*)v)->val.ptr) hawk_rtx_freemem(rtx, str);
			break;

#if 0
		case HAWK_VAL_REF:
			v = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)v);
			if (v && HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_MBS) goto plain_mbs;
			/* fall through */
#endif

		case HAWK_VAL_BOB: /* BOB to MBS is always duplication */
		default:
		freemem:
			hawk_rtx_freemem(rtx, str);
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
			return hawk_rtx_valtonum(rtx, HAWK_RTX_STACK_GBL(rtx, idx), l, r);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;

			/* A reference value is not able to point to another
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, *xref) != HAWK_VAL_REF);

			/* make a recursive call back to the caller */
			return hawk_rtx_valtonum(rtx, *xref, l, r);
		}
	}
}


static HAWK_INLINE_ALWAYS int val_to_num (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_int_t* l, hawk_flt_t* r, const hawk_rtx_gbl_t* gbl)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			*l = 0;
			return 0;

		case HAWK_VAL_BOOL:
			*l = ((hawk_val_bool_t*)v)->val;
			return 0; /* long */

		case HAWK_VAL_BCHR:
		{
			hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
			return hawk_bchars_to_num(
				HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON_FOR_GBL(rtx, gbl), 0),
				&tmp, 1, l, r
			);
		}

		case HAWK_VAL_CHAR:
		{
			/* treat it as if it is a 1-letter string */
			hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
			return hawk_oochars_to_num(
				HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON_FOR_GBL(rtx, gbl), 0),
				&tmp, 1, l, r
			);
		}

		case HAWK_VAL_INT:
			*l = HAWK_RTX_GETINTFROMVAL(rtx, v);
			return 0; /* long */

		case HAWK_VAL_FLT:
			*r = ((hawk_val_flt_t*)v)->val;
			return 1; /* real */

		case HAWK_VAL_STR:
			return hawk_oochars_to_num(
				HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON_FOR_GBL(rtx, gbl), 0),
				((hawk_val_str_t*)v)->val.ptr,
				((hawk_val_str_t*)v)->val.len,
				l, r
			);

		case HAWK_VAL_MBS:
			return hawk_bchars_to_num(
				HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON_FOR_GBL(rtx, gbl), 0),
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

		case HAWK_VAL_ARR:
			if (rtx->hawk->opt.trait & HAWK_FLEXMAP)
			{
				*l = HAWK_ARR_SIZE(((hawk_val_arr_t*)v)->arr);
				return 0; /* long */
			}
			goto invalid;

		case HAWK_VAL_REF:
			return val_ref_to_num(rtx, (hawk_val_ref_t*)v, l, r);

		case HAWK_VAL_REX:
		case HAWK_VAL_BOB: /* never allow reference */
		default:
		invalid:
		#if defined(DEBUG_VAL)
			hawk_logfmt(hawk, HAWK_T(">>WRONG VALUE TYPE [%d] in hawk_rtx_valtonum()\n"), v->type);
		#endif
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EVALTONUM);
			return -1; /* error */
	}
}

int hawk_rtx_valtonum (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_int_t* l, hawk_flt_t* r)
{
	return val_to_num(rtx, v, l, r, &rtx->gbl);
}

int hawk_rtx_refprep_valtonum (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep, hawk_val_t* val, hawk_int_t* l, hawk_flt_t* r)
{
	return val_to_num(rtx, val, l, r, prep? &prep->gbl: &rtx->gbl);
}

int hawk_rtx_refprep_valtoint (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep, hawk_val_t* val, hawk_int_t* l)
{
	hawk_flt_t r;
	int n;

	if (!prep) return hawk_rtx_valtoint_inline(rtx, val, l);
	n = hawk_rtx_refprep_valtonum(rtx, prep, val, l, &r);
	if (n == 1)
	{
		const hawk_flt_t limit = (hawk_flt_t)((hawk_uint_t)1 << (HAWK_BITSOF(hawk_int_t) - 1));
		if (!(r >= -limit && r < limit))
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL,
				"non-finite or out-of-range floating-point value for integer conversion");
			return -1;
		}
		*l = (hawk_int_t)r;
		n = 0;
	}
	return n;
}

int hawk_rtx_valtoint (hawk_rtx_t* rtx, const hawk_val_t* v, hawk_int_t* l)
{
#if defined(HAWK_HAVE_INLINE)
	return hawk_rtx_valtoint_inline(rtx, v, l);
#else
	/* i can't call hawk_rtx_valtoint() in case HAWK_HAVE_INLINE is not defined
	 * because the macro hawk_rtx_valtoint_inline() is defined to be hawk_rtx_valtoint().
	 * repeat the same code here */

	int n;
	hawk_flt_t r;

	n = hawk_rtx_valtonum(rtx, v, l, &r);
	if (n == 1)
	{
#if 1
		/* use the exact power-of-two boundary. converting the maximum
		 * signed integer to floating point can round it up to limit.
		 * negating the combined comparison also rejects NaN. */
		const hawk_flt_t limit = (hawk_flt_t)((hawk_uint_t)1 << (HAWK_BITSOF(hawk_int_t) - 1));
		if (HAWK_UNLIKELY(!(r >= -limit && r < limit)))
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL,
				"non-finite or out-of-range floating-point value for integer conversion");
			return -1;
		}
#endif

		*l = (hawk_int_t)r;
		n = 0;
	}

	return n;
#endif
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


hawk_fun_t* hawk_rtx_valtofun (hawk_rtx_t* rtx, hawk_val_t* v)
{
	hawk_fun_t* fun;
	hawk_val_type_t vtype;

	vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_FUN:
			fun = ((hawk_val_fun_t*)v)->fun;
			break;

		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bcs_t x;
			x.ptr = hawk_rtx_getvalbcstr(rtx, v, &x.len);
			if (HAWK_UNLIKELY(!x.ptr)) return HAWK_NULL;
			if (hawk_count_bcstr(x.ptr) != x.len)
			{
				hawk_rtx_freevalbcstr(rtx, v, x.ptr);
				goto error_inval;
			}
			fun = hawk_rtx_findfunwithbcstr(rtx, x.ptr);
			hawk_rtx_freevalbcstr(rtx, v, x.ptr);
			if (!fun) return HAWK_NULL;
			break;
		}

		case HAWK_VAL_CHAR:
		case HAWK_VAL_STR:
		{
			hawk_oocs_t x;
			x.ptr = hawk_rtx_getvaloocstr(rtx, v, &x.len);
			if (HAWK_UNLIKELY(!x.ptr)) return HAWK_NULL;
			if (hawk_count_oocstr(x.ptr) != x.len)
			{
				hawk_rtx_freevaloocstr(rtx, v, x.ptr);
				goto error_inval;
			}
			fun = hawk_rtx_findfunwithoocstr(rtx, x.ptr);
			hawk_rtx_freevaloocstr(rtx, v, x.ptr);
			if (!fun) return HAWK_NULL;
			break;
		}

		default:
		error_inval:
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
			return HAWK_NULL;
	}

	return fun;
}

hawk_fnc_t* hawk_rtx_valtofnc (hawk_rtx_t* rtx, hawk_val_t* v, hawk_fnc_t* rfnc)
{
	/* this function looks for intrinsic functions as well as module functions.
	 * it combines the functionality of the following two functions.
	 *   hawk_findfncwithoocs() - finds an intrisic function
	 *   hawk_rtx_querymodulewithoocs() - finds a function defined in a module
	 */

	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_val_type_t vtype;

	vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		case HAWK_VAL_CHAR:
		case HAWK_VAL_STR:
		{
			hawk_oocs_t x;
			hawk_fnc_t* fnc;

			x.ptr = hawk_rtx_getvaloocstr(rtx, v, &x.len);
			if (HAWK_UNLIKELY(!x.ptr)) return HAWK_NULL;
			if (hawk_count_oocstr(x.ptr) != x.len)
			{
				hawk_rtx_freevaloocstr(rtx, v, x.ptr);
				goto error_inval;
			}

			fnc = hawk_findfncwithoocs(hawk, &x);
			if (fnc)
			{
				hawk_rtx_freevaloocstr(rtx, v, x.ptr);
				*rfnc = *fnc;
			}
			else
			{
				hawk_mod_t* mod;
				hawk_mod_sym_t sym;

				mod = hawk_rtx_querymodulewithoocs(rtx, &x, &sym, 0);
				if (!mod)
				{
					hawk_rtx_freevaloocstr(rtx, v, x.ptr);
					return HAWK_NULL;
				}

				if (sym.type != HAWK_MOD_FNC || (hawk->opt.trait & sym.u.fnc_.trait) != sym.u.fnc_.trait)
				{
					hawk_rtx_seterrfmt(rtx, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%.*js' not a function or unsupported"), x.len, x.ptr);
					hawk_rtx_freevaloocstr(rtx, v, x.ptr);
					return HAWK_NULL;
				}

				hawk_rtx_freevaloocstr(rtx, v, x.ptr);

				HAWK_MEMSET(rfnc, 0, HAWK_SIZEOF(*rfnc));
				rfnc->name.ptr = sym.name;
				rfnc->name.len = hawk_count_oocstr(sym.name);
				rfnc->spec = sym.u.fnc_;
				/* TODO: FIX THIS - set the owner name */
				/*rfnc->owner = name of the module?*/
				rfnc->mod = mod;
			}
			break;
		}

		default:
		error_inval:
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
			return HAWK_NULL;
	}

	return rfnc;
}

/* ========================================================================== */

static HAWK_INLINE hawk_uint_t hash (hawk_uint8_t* ptr, hawk_oow_t len)
{
	hawk_uint_t h;
	HAWK_HASH_BYTES(h, ptr, len);
	return h;
}

hawk_int_t hawk_rtx_hashval (hawk_rtx_t* rtx, hawk_val_t* v)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, v);
	hawk_int_t hv;

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			hv = 0;
			break;

		case HAWK_VAL_BOOL:
		{
			hawk_val_bool_t* bv = (hawk_val_bool_t*)v;
			hv = (hawk_int_t)hash((hawk_uint8_t*)&bv->val, HAWK_SIZEOF(bv->val));
			break;
		}

		case HAWK_VAL_BCHR:
		{
			hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
			hv = (hawk_int_t)hash((hawk_uint8_t*)&tmp, HAWK_SIZEOF(tmp));
			break;
		}

		case HAWK_VAL_CHAR:
		{
			hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
			hv = (hawk_int_t)hash((hawk_uint8_t*)&tmp, HAWK_SIZEOF(tmp));
			break;
		}

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

		case HAWK_VAL_BOB:
		{
			hawk_val_bob_t* dv = (hawk_val_bob_t*)v;
			hv = (hawk_int_t)hash((hawk_uint8_t*)dv->val.ptr, dv->val.len);
			break;
		}

		default:

#if defined(DEBUG_VAL)
			hawk_logfmt(hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T(">>WRONG VALUE TYPE [%d] in hawk_rtx_hashval()\n"), v->type);
#endif
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EHASHVAL);
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
			v = HAWK_RTX_STACK_GBL(rtx, idx);
			return HAWK_RTX_GETVALTYPE(rtx, v);
		}

		case HAWK_VAL_REF_NAMED:
		{
			hawk_oow_t idx;
			hawk_val_t* v;
			idx = (hawk_oow_t)ref->adr;
			HAWK_ASSERT(idx < rtx->named_slot_count);
			v = HAWK_RTX_STACK_NAMED(rtx, idx);
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
			HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, v) != HAWK_VAL_REF);
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
			return HAWK_RTX_STACK_GBL(rtx, idx);
		}

		case HAWK_VAL_REF_NAMED:
		{
			hawk_oow_t idx;
			idx = (hawk_oow_t)ref->adr;
			HAWK_ASSERT(idx < rtx->named_slot_count);
			return HAWK_RTX_STACK_NAMED(rtx, idx);
		}

		default:
		{
			hawk_val_t** xref = (hawk_val_t**)ref->adr;
			/* A reference value is not able to point to another
			 * refernce value for the way values are represented
			 * in HAWK */
			HAWK_ASSERT(HAWK_RTX_GETVALTYPE(rtx, *xref) != HAWK_VAL_REF);
			return *xref;
		}
	}
}

hawk_val_t* hawk_rtx_refprep_getgbl (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep, int id)
{
	hawk_oow_t i;

	if (prep)
	{
		for (i = 0; i < prep->nglobals; i++)
			if (prep->globals[i].id == id) return prep->globals[i].val;
	}
	return HAWK_RTX_STACK_GBL(rtx, id);
}

hawk_ooch_t* hawk_rtx_refprep_dupstr (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep, hawk_val_t* val, hawk_oow_t* len)
{
	hawk_rtx_valtostr_out_t out;

	if (!prep || HAWK_RTX_GETVALTYPE(rtx, val) != HAWK_VAL_FLT)
		return hawk_rtx_valtooocstrdup(rtx, val, len);
	out.type = HAWK_RTX_VALTOSTR_CPLDUP;
	if (val_flt_to_str(rtx, (hawk_val_flt_t*)val, &out, &prep->gbl) <= -1) return HAWK_NULL;
	*len = out.u.cpldup.len;
	return out.u.cpldup.ptr;
}

static hawk_val_t* refprep_getslot (hawk_rtx_refprep_t* prep, hawk_val_t** slot)
{
	hawk_oow_t i;

	if (prep)
	{
		for (i = 0; i < prep->nslots; i++)
			if (prep->slots[i].slot == slot) return prep->slots[i].val;
	}
	return *slot;
}

static int refprep_setslot (hawk_rtx_t* rtx, hawk_rtx_refprep_t* prep, hawk_val_t** slot, hawk_val_t* val)
{
	hawk_oow_t i;

	for (i = 0; i < prep->nslots; i++)
		if (prep->slots[i].slot == slot) break;
	HAWK_ASSERT(i < HAWK_COUNTOF(prep->slots));
	hawk_rtx_refupval_inline(rtx, val);
	if (i < prep->nslots) hawk_rtx_refdownval_inline(rtx, prep->slots[i].val);
	else
	{
		prep->slots[i].slot = slot;
		prep->slots[i].old = *slot;
		prep->nslots++;
	}
	prep->slots[i].val = val;
	return 0;
}

static HAWK_INLINE_ALWAYS int set_ref_val (hawk_rtx_t* rtx, hawk_val_ref_t* ref, hawk_val_t* val, hawk_rtx_refprep_t* prep)
{
	hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, val);

	if (vtype == HAWK_VAL_REX || vtype == HAWK_VAL_REF)
	{
		/* though it is possible that an intrinsic function handler
		 * can accept a regular expression withtout evaluation when 'x'
		 * is specified for the parameter, this function doesn't allow
		 * regular expression to be set to a reference variable to
		 * avoid potential chaos. the nature of performing '/rex/ ~ $0'
		 * for a regular expression without the match operator
		 * makes it difficult to be implemented. */
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	switch (ref->id)
	{
		case HAWK_VAL_REF_POS:
		{
			switch (vtype)
			{
				case HAWK_VAL_MAP:
				case HAWK_VAL_ARR:
					/* a map is assigned to a positional. this is disallowed. */
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENONSCATOPOS);
					return -1;

				default:
				{
					hawk_oocs_t str;
					int x;

					hawk_rtx_refupval_inline(rtx, val);
					str.ptr = prep? hawk_rtx_refprep_dupstr(rtx, prep, val, &str.len): hawk_rtx_getvaloocstr(rtx, val, &str.len);
					if (HAWK_UNLIKELY(!str.ptr))
					{
						hawk_rtx_refdownval_inline(rtx, val);
						return -1;
					}

					if (prep)
					{
						x = hawk_rtx_refprep_record(rtx, prep);
						if (x >= 0) x = hawk_rtx_setrecwithstate(rtx, prep->rec, prep, (hawk_oow_t)ref->adr, &str, 0);
					}
					else
					{
						x = hawk_rtx_setrec(rtx, (hawk_oow_t)ref->adr, &str, 0);
					}

					if (prep) hawk_rtx_freemem(rtx, str.ptr);
					else hawk_rtx_freevaloocstr(rtx, val, str.ptr);

					hawk_rtx_refdownval_inline(rtx, val);
					return x;
				}
			}
		}

		case HAWK_VAL_REF_GBL:
			/* ref->adr is the index to the global variables, not a real pointer address for HAWK_VAL_REF_GBL */
			return hawk_rtx_refprep_setgbl(rtx, prep, (int)ref->adr, val);

		case HAWK_VAL_REF_NAMED:
		{
			hawk_val_t* old;
			hawk_val_type_t old_vtype;

			HAWK_ASSERT((hawk_oow_t)ref->adr < rtx->named_slot_count);
			old = refprep_getslot(prep, &HAWK_RTX_STACK_NAMED(rtx, (hawk_oow_t)ref->adr));
			old_vtype = HAWK_RTX_GETVALTYPE(rtx, old);

			if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
			{
				if (old_vtype != HAWK_VAL_NIL && old_vtype != vtype)
				{
					if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
					{
						hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ESCALARTONONSCA);
						return -1;
					}
				}
			}
			else
			{
				if (old_vtype == HAWK_VAL_MAP || old_vtype == HAWK_VAL_ARR)
				{
					if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
					{
						hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENONSCATOSCALAR);
						return -1;
					}
				}
			}

			if (old != val)
			{
				if (prep) return refprep_setslot(rtx, prep, &HAWK_RTX_STACK_NAMED(rtx, (hawk_oow_t)ref->adr), val);
				return hawk_rtx_setnamedval(rtx, (hawk_oow_t)ref->adr, val);
			}
			return 0;
		}

		case HAWK_VAL_REF_NAMEDIDX:
		case HAWK_VAL_REF_GBLIDX:
		case HAWK_VAL_REF_LCLIDX:
		case HAWK_VAL_REF_ARGIDX:
		#if !defined(HAWK_ENABLE_GC)
			if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
			{
				/* an indexed variable cannot be assigned a map.
				 * in other cases, it falls down to the default case. */
				hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENONSCATOIDX);
				return -1;
			}
		#endif
			/* fall through */

		default:
		{
			hawk_val_t** rref;
			hawk_val_type_t rref_vtype;

			rref = (hawk_val_t**)ref->adr; /* old value pointer */
			rref_vtype = HAWK_RTX_GETVALTYPE(rtx, refprep_getslot(prep, rref)); /* old value type */
			if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
			{
				/* new value: map, old value: nil or map => ok */
				if (rref_vtype != HAWK_VAL_NIL && rref_vtype != vtype)
				{
					if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
					{
						/* cannot change a scalar value to a map */
						hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ESCALARTONONSCA);
						return -1;
					}
				}
			}
			else
			{
				/* new value: scalar, old value: nil or scalar => ok */
				if (rref_vtype == HAWK_VAL_MAP || rref_vtype == HAWK_VAL_ARR)
				{
					if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
					{
						hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENONSCATOSCALAR);
						return -1;
					}
				}
			}

			if (prep) return refprep_setslot(rtx, prep, rref, val);

			if (*rref != val)
			{
				/* if the new value is not the same as the old value */
				hawk_rtx_refdownval_inline(rtx, *rref);
				*rref = val;
				hawk_rtx_refupval_inline(rtx, *rref);
			}
			return 0;
		}
	}
}

int hawk_rtx_setrefval (hawk_rtx_t* rtx, hawk_val_ref_t* ref, hawk_val_t* val)
{
	return set_ref_val(rtx, ref, val, HAWK_NULL);
}

static void refprep_free_caches (hawk_rtx_t* rtx, hawk_rtx_gbl_t* owned, const hawk_rtx_gbl_t* borrowed)
{
	if (owned->rs[0] != borrowed->rs[0] && owned->rs[0])
		hawk_rtx_freerex(rtx, owned->rs[0], owned->rs[1]);

	if (owned->fs[0] != borrowed->fs[0] && owned->fs[0])
		hawk_rtx_freerex(rtx, owned->fs[0], owned->fs[1]);

	if (owned->convfmt.ptr != borrowed->convfmt.ptr && owned->convfmt.ptr)
		hawk_rtx_freemem(rtx, owned->convfmt.ptr);

	if (owned->ofmt.ptr != borrowed->ofmt.ptr && owned->ofmt.ptr)
		hawk_rtx_freemem(rtx, owned->ofmt.ptr);

	if (owned->ofs.ptr != borrowed->ofs.ptr && owned->ofs.ptr)
		hawk_rtx_freemem(rtx, owned->ofs.ptr);

	if (owned->ors.ptr != borrowed->ors.ptr && owned->ors.ptr)
		hawk_rtx_freemem(rtx, owned->ors.ptr);

	if (owned->subsep.ptr != borrowed->subsep.ptr && owned->subsep.ptr)
		hawk_rtx_freemem(rtx, owned->subsep.ptr);
}

int hawk_rtx_setrefvalpair (hawk_rtx_t* rtx, hawk_val_ref_t* ref1, hawk_val_t* val1, hawk_val_ref_t* ref2, hawk_val_t* val2)
{
	hawk_rtx_refprep_t prep;
	hawk_rtx_gbl_t old_gbl;
	hawk_errinf_t errinf;
	hawk_oow_t i;
	int n = -1;

	HAWK_MEMSET(&prep, 0, HAWK_SIZEOF(prep));
	prep.gbl = rtx->gbl;

	/* keep inputs alive if the first prepared write replaces the second input. */
	hawk_rtx_refupval_inline(rtx, val1);
	hawk_rtx_refupval_inline(rtx, val2);
	if (set_ref_val(rtx, ref1, val1, &prep) <= -1 ||
	    set_ref_val(rtx, ref2, val2, &prep) <= -1)
	{
		hawk_rtx_geterrinf(rtx, &errinf);
		refprep_free_caches(rtx, &prep.gbl, &rtx->gbl);
		goto done;
	}

	/* publish all locations before releasing old containers or calling observers.
	 * no conversion, allocation or fallible setter occurs in this phase. */
	old_gbl = rtx->gbl;
	rtx->gbl = prep.gbl;
	if (prep.rec) hawk_rtx_refprep_swaprec(rtx, prep.rec);
	for (i = 0; i < prep.nglobals; i++)
	{
		hawk_rtx_refupval_inline(rtx, prep.globals[i].val);
		HAWK_RTX_STACK_GBL(rtx, prep.globals[i].id) = prep.globals[i].val;
	}
	for (i = 0; i < prep.nslots; i++)
	{
		hawk_rtx_refupval_inline(rtx, prep.slots[i].val);
		*prep.slots[i].slot = prep.slots[i].val;
	}
	refprep_free_caches(rtx, &old_gbl, &rtx->gbl);

	for (i = 0; i < prep.nslots; i++)
		hawk_rtx_refdownval_inline(rtx, prep.slots[i].old);
	for (i = 0; i < prep.nglobals; i++)
		hawk_rtx_refdownval_inline(rtx, prep.globals[i].old);

	if (rtx->ecb_gblset_count > 0)
	{
		for (i = 0; i < prep.nglobals; i++)
		{
			hawk_rtx_ecb_t* ecb, * next;
			for (ecb = rtx->ecb; ecb != (hawk_rtx_ecb_t*)rtx; ecb = next)
			{
				next = ecb->next_;
				if (ecb->gblset) ecb->gblset(rtx, prep.globals[i].id, prep.globals[i].val, ecb->ctx);
			}
		}
	}
	n = 0;

done:
	if (prep.rec) hawk_rtx_refprep_freerec(rtx, prep.rec);

	for (i = 0; i < prep.nslots; i++)
		hawk_rtx_refdownval_inline(rtx, prep.slots[i].val);
	for (i = 0; i < prep.nglobals; i++)
		hawk_rtx_refdownval_inline(rtx, prep.globals[i].val);

	hawk_rtx_refdownval_inline(rtx, val2);
	hawk_rtx_refdownval_inline(rtx, val1);

	if (n <= -1) hawk_rtx_seterrinf(rtx, &errinf);
	return n;
}

#if 0

#define hawk_errputstrf hawk_errputstrf

static hawk_map_walk_t print_pair (hawk_map_t* map, hawk_map_pair_t* pair, void* arg)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)arg;

	HAWK_ASSERT(rtx == *(hawk_rtx_t**)hawk_map_getxtn(map));

	hawk_errputstrf(HAWK_T(" %.*s=>"), HAWK_MAP_KLEN(pair), HAWK_MAP_KPTR(pair));
	hawk_dprintval((hawk_rtx_t*)arg, HAWK_MAP_VPTR(pair));
	hawk_errputstrf(HAWK_T(" "));

	return HAWK_MAP_WALK_FORWARD;
}


static hawk_arr_walk_t print_pair (hawk_arr_t* arr, hawk_oow_t index, void* arg)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)arg;

	HAWK_ASSERT(rtx == *(hawk_rtx_t**)hawk_arr_getxtn(arr));

	hawk_errputstrf(HAWK_T(" %lu=>"), (unsigned long int)index);
	hawk_dprintval((hawk_rtx_t*)arg, (HAWK_ARR_SLOT(arr,index)? HAWK_ARR_DPTR(arr, index): hawk_val_nil));
	hawk_errputstrf(HAWK_T(" "));

	return HAWK_ARR_WALK_FORWARD;
}


void hawk_dprintval(hawk_rtx_t* run, hawk_val_t* val)
{
	/* TODO: better value printing ... */

	switch (val->type)
	{
		case HAWK_VAL_NIL:
			hawk_errputstrf(HAWK_T("@nil"));
			break;

		case HAWK_VAL_BOOL:
			hawk_errputstrf(((hawk_val_bool_t*)val)->val? HAWK_T("@true"): HAWK_T("@false"));
			break;

		case HAWK_VAL_CHAR:
		{
			hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, val);
			if (tmp == '\'')
				hawk_errputstrf(HAWK_T("'\\%c'"), tmp);
			else if (tmp == '\0')
				hawk_errputstrf(HAWK_T("'\\0'"));
			else if (hawk_is_ooch_print(tmp))
				hawk_errputstrf(HAWK_T("'%jc'"), tmp);
		#if defined(HAWK_OOCH_IS_UCH)
			else if (tmp <= 0xFFFF)
				hawk_errputstrf(HAWK_T("'\\u%04x'"), tmp);
			else
				hawk_errputstrf(HAWK_T("'\\U%08x'"), tmp);
		#else
			else
				hawk_errputstrf(HAWK_T("'\\x%02x'"), tmp);
		#endif
			break;
		}

		case HAWK_VAL_BCHR:
			hawk_errputstrf(HAWK_T("%hc"), HAWK_GETBCHRFROMVAL(val));
			break;

		case HAWK_VAL_CHAR:
			hawk_errputstrf(HAWK_T("%jc"), HAWK_GETCHARFROMVAL(val));
			break;

		case HAWK_VAL_INT:
			hawk_errputstrf(HAWK_T("%jd"), (hawk_intmax_t)HAWK_GETINTFROMVAL(val));
			break;

		case HAWK_VAL_FLT:
		#if defined(HAWK_USE_FLTMAX)
			/*hawk_errputstrf(HAWK_T("%jf"), ((hawk_val_flt_t*)val)->val);*/
			hawk_errputstrf(HAWK_T("%jjf"), &((hawk_val_flt_t*)val)->val);
		#else
			hawk_errputstrf(HAWK_T("%zf"), ((hawk_val_flt_t*)val)->val);
		#endif
			break;

		case HAWK_VAL_STR:
			hawk_errputstrf(HAWK_T("%.*s"), ((hawk_val_str_t*)val)->len, ((hawk_val_str_t*)val)->ptr);
			break;

		case HAWK_VAL_MBS:
			hawk_errputstrf(HAWK_T("%.*hs"), ((hawk_val_mbs_t*)val)->len, ((hawk_val_mbs_t*)val)->ptr);
			break;

		case HAWK_VAL_REX:
			hawk_errputstrf(HAWK_T("/%s/"), ((hawk_val_rex_t*)val)->ptr);
			break;

		case HAWK_VAL_FUN:
			hawk_errputstrf(HAWK_T("<FUN:%.*s>"), ((hawk_val_fun_t*)val)->fun->name.len, ((hawk_val_fun_t*)val)->fun->name.ptr);
			break;

		case HAWK_VAL_MAP:
			hawk_errputstrf(HAWK_T("MAP["));
			hawk_map_walk(((hawk_val_map_t*)val)->map, print_pair, run);
			hawk_errputstrf(HAWK_T("]"));
			break;

		case HAWK_VAL_ARR:
			hawk_errputstrf(HAWK_T("ARR["));
			hawk_arr_walk(((hawk_val_arr_t*)val)->arr, print_elem, run);
			hawk_errputstrf(HAWK_T("]"));
			break;

		case HAWK_VAL_REF:
			hawk_errputstrf(HAWK_T("REF[id=%d,val="), ((hawk_val_ref_t*)val)->id);
			hawk_dprintval(run, *((hawk_val_ref_t*)val)->adr);
			hawk_errputstrf(HAWK_T("]"));
			break;

		case HAWK_VAL_BOB:
			hawk_errputstrf(HAWK_T("%.*k"), ((hawk_val_bob_t*)val)->len, ((hawk_val_bob_t*)val)->ptr);
			break;

		default:
			hawk_errputstrf(HAWK_T("**** INTERNAL ERROR - INVALID VALUE TYPE ****\n"));
	}
}

#endif
