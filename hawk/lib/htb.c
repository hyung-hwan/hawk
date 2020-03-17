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

#include <hawk-htb.h>
#include "hawk-prv.h"

#define pair_t          hawk_htb_pair_t
#define copier_t        hawk_htb_copier_t
#define freeer_t        hawk_htb_freeer_t
#define hasher_t        hawk_htb_hasher_t
#define comper_t        hawk_htb_comper_t
#define keeper_t        hawk_htb_keeper_t
#define sizer_t         hawk_htb_sizer_t
#define walker_t        hawk_htb_walker_t
#define cbserter_t      hawk_htb_cbserter_t
#define style_t         hawk_htb_style_t
#define style_kind_t    hawk_htb_style_kind_t

#define KPTR(p)  HAWK_HTB_KPTR(p)
#define KLEN(p)  HAWK_HTB_KLEN(p)
#define VPTR(p)  HAWK_HTB_VPTR(p)
#define VLEN(p)  HAWK_HTB_VLEN(p)
#define NEXT(p)  HAWK_HTB_NEXT(p)

#define KTOB(htb,len) ((len)*(htb)->scale[HAWK_HTB_KEY])
#define VTOB(htb,len) ((len)*(htb)->scale[HAWK_HTB_VAL])

HAWK_INLINE pair_t* hawk_htb_allocpair (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	pair_t* n;
	copier_t kcop, vcop;
	hawk_oow_t as;

	kcop = htb->style->copier[HAWK_HTB_KEY];
	vcop = htb->style->copier[HAWK_HTB_VAL];

	as = HAWK_SIZEOF(pair_t);
	if (kcop == HAWK_HTB_COPIER_INLINE) as += HAWK_ALIGN_POW2(KTOB(htb,klen), HAWK_SIZEOF_VOID_P);
	if (vcop == HAWK_HTB_COPIER_INLINE) as += VTOB(htb,vlen);

	n = (pair_t*) hawk_gem_allocmem(htb->gem, as);
	if (HAWK_UNLIKELY(!n)) return HAWK_NULL;

	NEXT(n) = HAWK_NULL;

	KLEN(n) = klen;
	if (kcop == HAWK_HTB_COPIER_SIMPLE)
	{
		KPTR(n) = kptr;
	}
	else if (kcop == HAWK_HTB_COPIER_INLINE)
	{
		KPTR(n) = n + 1;
		/* if kptr is HAWK_NULL, the inline copier does not fill
		 * the actual key area */
		if (kptr) HAWK_MEMCPY (KPTR(n), kptr, KTOB(htb,klen));
	}
	else 
	{
		KPTR(n) = kcop(htb, kptr, klen);
		if (KPTR(n) == HAWK_NULL)
		{
			hawk_gem_freemem (htb->gem, n);
			return HAWK_NULL;
		}
	}

	VLEN(n) = vlen;
	if (vcop == HAWK_HTB_COPIER_SIMPLE)
	{
		VPTR(n) = vptr;
	}
	else if (vcop == HAWK_HTB_COPIER_INLINE)
	{
		VPTR(n) = n + 1;
		if (kcop == HAWK_HTB_COPIER_INLINE) 
			VPTR(n) = (hawk_uint8_t*)VPTR(n) + HAWK_ALIGN_POW2(KTOB(htb,klen), HAWK_SIZEOF_VOID_P);
		/* if vptr is HAWK_NULL, the inline copier does not fill
		 * the actual value area */
		if (vptr) HAWK_MEMCPY (VPTR(n), vptr, VTOB(htb,vlen));
	}
	else 
	{
		VPTR(n) = vcop (htb, vptr, vlen);
		if (VPTR(n) != HAWK_NULL)
		{
			if (htb->style->freeer[HAWK_HTB_KEY] != HAWK_NULL)
				htb->style->freeer[HAWK_HTB_KEY] (htb, KPTR(n), KLEN(n));
			hawk_gem_freemem (htb->gem, n);
			return HAWK_NULL;
		}
	}

	return n;
}

HAWK_INLINE void hawk_htb_freepair (hawk_htb_t* htb, pair_t* pair)
{
	if (htb->style->freeer[HAWK_HTB_KEY] != HAWK_NULL) 
		htb->style->freeer[HAWK_HTB_KEY] (htb, KPTR(pair), KLEN(pair));
	if (htb->style->freeer[HAWK_HTB_VAL] != HAWK_NULL)
		htb->style->freeer[HAWK_HTB_VAL] (htb, VPTR(pair), VLEN(pair));
	hawk_gem_freemem (htb->gem, pair);	
}

static HAWK_INLINE pair_t* change_pair_val (hawk_htb_t* htb, pair_t* pair, void* vptr, hawk_oow_t vlen)
{
	if (VPTR(pair) == vptr && VLEN(pair) == vlen) 
	{
		/* if the old value and the new value are the same,
		 * it just calls the handler for this condition. 
		 * No value replacement occurs. */
		if (htb->style->keeper != HAWK_NULL)
		{
			htb->style->keeper (htb, vptr, vlen);
		}
	}
	else
	{
		copier_t vcop = htb->style->copier[HAWK_HTB_VAL];
		void* ovptr = VPTR(pair);
		hawk_oow_t ovlen = VLEN(pair);

		/* place the new value according to the copier */
		if (vcop == HAWK_HTB_COPIER_SIMPLE)
		{
			VPTR(pair) = vptr;
			VLEN(pair) = vlen;
		}
		else if (vcop == HAWK_HTB_COPIER_INLINE)
		{
			if (ovlen == vlen)
			{
				if (vptr) HAWK_MEMCPY (VPTR(pair), vptr, VTOB(htb,vlen));
			}
			else
			{
				/* need to reconstruct the pair */
				pair_t* p = hawk_htb_allocpair(htb, KPTR(pair), KLEN(pair), vptr, vlen);
				if (HAWK_UNLIKELY(!p)) return HAWK_NULL;
				hawk_htb_freepair (htb, pair);
				return p;
			}
		}
		else 
		{
			void* nvptr = vcop(htb, vptr, vlen);
			if (HAWK_UNLIKELY(!nvptr)) return HAWK_NULL;
			VPTR(pair) = nvptr;
			VLEN(pair) = vlen;
		}

		/* free up the old value */
		if (htb->style->freeer[HAWK_HTB_VAL] != HAWK_NULL) 
		{
			htb->style->freeer[HAWK_HTB_VAL] (htb, ovptr, ovlen);
		}
	}

	return pair;
}

static style_t style[] =
{
    	/* == HAWK_HTB_STYLE_DEFAULT == */
	{
		{
			HAWK_HTB_COPIER_DEFAULT,
			HAWK_HTB_COPIER_DEFAULT
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			HAWK_HTB_FREEER_DEFAULT
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	},

	/* == HAWK_HTB_STYLE_INLINE_COPIERS == */
	{
		{
			HAWK_HTB_COPIER_INLINE,
			HAWK_HTB_COPIER_INLINE
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			HAWK_HTB_FREEER_DEFAULT
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	},

	/* == HAWK_HTB_STYLE_INLINE_KEY_COPIER == */
	{
		{
			HAWK_HTB_COPIER_INLINE,
			HAWK_HTB_COPIER_DEFAULT
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			HAWK_HTB_FREEER_DEFAULT
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	},

	/* == HAWK_HTB_STYLE_INLINE_VALUE_COPIER == */
	{
		{
			HAWK_HTB_COPIER_DEFAULT,
			HAWK_HTB_COPIER_INLINE
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			HAWK_HTB_FREEER_DEFAULT
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	}
};

const style_t* hawk_get_htb_style (style_kind_t kind)
{
	return &style[kind];
}

hawk_htb_t* hawk_htb_open (hawk_gem_t* gem, hawk_oow_t xtnsize, hawk_oow_t capa, int factor, int kscale, int vscale)
{
	hawk_htb_t* htb;

	htb = (hawk_htb_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_htb_t) + xtnsize);
	if (HAWK_UNLIKELY(!htb)) return HAWK_NULL;

	if (hawk_htb_init(htb, gem, capa, factor, kscale, vscale) <= -1)
	{
		hawk_gem_freemem (gem, htb);
		return HAWK_NULL;
	}

	HAWK_MEMSET (htb + 1, 0, xtnsize);
	return htb;
}

void hawk_htb_close (hawk_htb_t* htb)
{
	hawk_htb_fini (htb);
	hawk_gem_freemem (htb->gem, htb);
}

int hawk_htb_init (hawk_htb_t* htb, hawk_gem_t* gem, hawk_oow_t capa, int factor, int kscale, int vscale)
{
	/* The initial capacity should be greater than 0. 
	 * Otherwise, it is adjusted to 1 in the release mode */
	HAWK_ASSERT (capa > 0);

	/* The load factor should be between 0 and 100 inclusive. 
	 * In the release mode, a value out of the range is adjusted to 100 */
	HAWK_ASSERT (factor >= 0 && factor <= 100);

	HAWK_ASSERT (kscale >= 0 && kscale <= HAWK_TYPE_MAX(hawk_uint8_t));
	HAWK_ASSERT (vscale >= 0 && vscale <= HAWK_TYPE_MAX(hawk_uint8_t));

	/* some initial adjustment */
	if (capa <= 0) capa = 1;
	if (factor > 100) factor = 100;

	/* do not zero out the extension */
	HAWK_MEMSET (htb, 0, HAWK_SIZEOF(*htb));
	htb->gem = gem;

	htb->bucket = hawk_gem_allocmem(gem, capa * HAWK_SIZEOF(pair_t*));
	if (HAWK_UNLIKELY(!htb->bucket)) return -1;

	/*for (i = 0; i < capa; i++) htb->bucket[i] = HAWK_NULL;*/
	HAWK_MEMSET (htb->bucket, 0, capa * HAWK_SIZEOF(pair_t*));

	htb->factor = factor;
	htb->scale[HAWK_HTB_KEY] = (kscale < 1)? 1: kscale;
	htb->scale[HAWK_HTB_VAL] = (vscale < 1)? 1: vscale;

	htb->size = 0;
	htb->capa = capa;
	htb->threshold = htb->capa * htb->factor / 100;
	if (htb->capa > 0 && htb->threshold <= 0) htb->threshold = 1;

	htb->style = &style[0];
	return 0;
}

void hawk_htb_fini (hawk_htb_t* htb)
{
	hawk_htb_clear (htb);
	hawk_gem_freemem (htb->gem, htb->bucket);
}

const style_t* hawk_htb_getstyle (const hawk_htb_t* htb)
{
	return htb->style;
}

void hawk_htb_setstyle (hawk_htb_t* htb, const style_t* style)
{
	HAWK_ASSERT (style != HAWK_NULL);
	htb->style = style;
}

hawk_oow_t hawk_htb_getsize (const hawk_htb_t* htb)
{
	return htb->size;
}

hawk_oow_t hawk_htb_getcapa (const hawk_htb_t* htb)
{
	return htb->capa;
}

pair_t* hawk_htb_search (const hawk_htb_t* htb, const void* kptr, hawk_oow_t klen)
{
	pair_t* pair;
	hawk_oow_t hc;

	hc = htb->style->hasher(htb,kptr,klen) % htb->capa;
	pair = htb->bucket[hc];

	while (pair != HAWK_NULL) 
	{
		if (htb->style->comper(htb, KPTR(pair), KLEN(pair), kptr, klen) == 0)
		{
			return pair;
		}

		pair = NEXT(pair);
	}

	hawk_gem_seterrnum (htb->gem, HAWK_NULL, HAWK_ENOENT);
	return HAWK_NULL;
}

static HAWK_INLINE int reorganize (hawk_htb_t* htb)
{
	hawk_oow_t i, hc, new_capa;
	pair_t** new_buck;

	if (htb->style->sizer)
	{
		new_capa = htb->style->sizer (htb, htb->capa + 1);

		/* if no change in capacity, return success 
		 * without reorganization */
		if (new_capa == htb->capa) return 0; 

		/* adjust to 1 if the new capacity is not reasonable */
		if (new_capa <= 0) new_capa = 1;
	}
	else
	{
		/* the bucket is doubled until it grows up to 65536 slots.
		 * once it has reached it, it grows by 65536 slots */
		new_capa = (htb->capa >= 65536)? (htb->capa + 65536): (htb->capa << 1);
	}

	new_buck = (pair_t**)hawk_gem_allocmem(htb->gem, new_capa * HAWK_SIZEOF(pair_t*));
	if (HAWK_UNLIKELY(!new_buck)) 
	{
		/* reorganization is disabled once it fails */
		htb->threshold = 0;
		return -1;
	}

	/*for (i = 0; i < new_capa; i++) new_buck[i] = HAWK_NULL;*/
	HAWK_MEMSET (new_buck, 0, new_capa * HAWK_SIZEOF(pair_t*));

	for (i = 0; i < htb->capa; i++)
	{
		pair_t* pair = htb->bucket[i];

		while (pair != HAWK_NULL) 
		{
			pair_t* next = NEXT(pair);

			hc = htb->style->hasher(htb, KPTR(pair), KLEN(pair)) % new_capa;

			NEXT(pair) = new_buck[hc];
			new_buck[hc] = pair;

			pair = next;
		}
	}

	hawk_gem_freemem (htb->gem, htb->bucket);
	htb->bucket = new_buck;
	htb->capa = new_capa;
	htb->threshold = htb->capa * htb->factor / 100;

	return 0;
}

/* insert options */
#define UPSERT 1
#define UPDATE 2
#define ENSERT 3
#define INSERT 4

static HAWK_INLINE pair_t* insert (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen, int opt)
{
	pair_t* pair, * p, * prev, * next;
	hawk_oow_t hc;

	hc = htb->style->hasher(htb,kptr,klen) % htb->capa;
	pair = htb->bucket[hc];
	prev = HAWK_NULL;

	while (pair != HAWK_NULL) 
	{
		next = NEXT(pair);

		if (htb->style->comper (htb, KPTR(pair), KLEN(pair), kptr, klen) == 0) 
		{
			/* found a pair with a matching key */
			switch (opt)
			{
				case UPSERT:
				case UPDATE:
					p = change_pair_val (htb, pair, vptr, vlen);
					if (p == HAWK_NULL) 
					{
						/* error in changing the value */
						return HAWK_NULL; 
					}
					if (p != pair) 
					{
						/* old pair destroyed. new pair reallocated.
						 * relink to include the new pair but to drop
						 * the old pair. */
						if (prev == HAWK_NULL) htb->bucket[hc] = p;
						else NEXT(prev) = p;
						NEXT(p) = next; 
					}
					return p;

				case ENSERT:
					/* return existing pair */
					return pair; 

				case INSERT:
					/* return failure */
					hawk_gem_seterrnum (htb->gem, HAWK_NULL, HAWK_EEXIST);
					return HAWK_NULL;
			}
		}

		prev = pair;
		pair = next;
	}

	if (opt == UPDATE) 
	{
		hawk_gem_seterrnum (htb->gem, HAWK_NULL, HAWK_ENOENT);
		return HAWK_NULL;
	}

	if (htb->threshold > 0 && htb->size >= htb->threshold)
	{
		/* ingore reorganization error as it simply means
		 * more bucket collision and performance penalty. */
		if (reorganize(htb) == 0) 
		{
			hc = htb->style->hasher(htb,kptr,klen) % htb->capa;
		}
	}

	HAWK_ASSERT (pair == HAWK_NULL);

	pair = hawk_htb_allocpair (htb, kptr, klen, vptr, vlen);
	if (HAWK_UNLIKELY(!pair)) return HAWK_NULL; /* error */

	NEXT(pair) = htb->bucket[hc];
	htb->bucket[hc] = pair;
	htb->size++;

	return pair; /* new key added */
}

pair_t* hawk_htb_upsert (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert (htb, kptr, klen, vptr, vlen, UPSERT);
}

pair_t* hawk_htb_ensert (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert (htb, kptr, klen, vptr, vlen, ENSERT);
}

pair_t* hawk_htb_insert (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert (htb, kptr, klen, vptr, vlen, INSERT);
}


pair_t* hawk_htb_update (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert (htb, kptr, klen, vptr, vlen, UPDATE);
}

pair_t* hawk_htb_cbsert (hawk_htb_t* htb, void* kptr, hawk_oow_t klen, cbserter_t cbserter, void* ctx)
{
	pair_t* pair, * p, * prev, * next;
	hawk_oow_t hc;

	hc = htb->style->hasher(htb,kptr,klen) % htb->capa;
	pair = htb->bucket[hc];
	prev = HAWK_NULL;

	while (pair != HAWK_NULL) 
	{
		next = NEXT(pair);

		if (htb->style->comper(htb, KPTR(pair), KLEN(pair), kptr, klen) == 0) 
		{
			/* found a pair with a matching key */
			p = cbserter(htb, pair, kptr, klen, ctx);
			if (p == HAWK_NULL) 
			{
				/* error returned by the callback function */
				return HAWK_NULL; 
			}
			if (p != pair) 
			{
				/* old pair destroyed. new pair reallocated.
				 * relink to include the new pair but to drop
				 * the old pair. */
				if (prev == HAWK_NULL) htb->bucket[hc] = p;
				else NEXT(prev) = p;
				NEXT(p) = next; 
			}
			return p;
		}

		prev = pair;
		pair = next;
	}

	if (htb->threshold > 0 && htb->size >= htb->threshold)
	{
		/* ingore reorganization error as it simply means
		 * more bucket collision and performance penalty. */
		if (reorganize(htb) == 0)
		{
			hc = htb->style->hasher(htb,kptr,klen) % htb->capa;
		}
	}

	HAWK_ASSERT (pair == HAWK_NULL);

	pair = cbserter(htb, HAWK_NULL, kptr, klen, ctx);
	if (HAWK_UNLIKELY(!pair)) return HAWK_NULL; /* error */

	NEXT(pair) = htb->bucket[hc];
	htb->bucket[hc] = pair;
	htb->size++;

	return pair; /* new key added */
}

int hawk_htb_delete (hawk_htb_t* htb, const void* kptr, hawk_oow_t klen)
{
	pair_t* pair, * prev;
	hawk_oow_t hc;

	hc = htb->style->hasher(htb,kptr,klen) % htb->capa;
	pair = htb->bucket[hc];
	prev = HAWK_NULL;

	while (pair != HAWK_NULL) 
	{
		if (htb->style->comper (htb, KPTR(pair), KLEN(pair), kptr, klen) == 0) 
		{
			if (prev == HAWK_NULL) 
				htb->bucket[hc] = NEXT(pair);
			else NEXT(prev) = NEXT(pair);

			hawk_htb_freepair (htb, pair);
			htb->size--;

			return 0;
		}

		prev = pair;
		pair = NEXT(pair);
	}

	hawk_gem_seterrnum (htb->gem, HAWK_NULL, HAWK_ENOENT);
	return -1;
}

void hawk_htb_clear (hawk_htb_t* htb)
{
	hawk_oow_t i;
	pair_t* pair, * next;

	for (i = 0; i < htb->capa; i++) 
	{
		pair = htb->bucket[i];

		while (pair != HAWK_NULL) 
		{
			next = NEXT(pair);
			hawk_htb_freepair (htb, pair);
			htb->size--;
			pair = next;
		}

		htb->bucket[i] = HAWK_NULL;
	}
}

void hawk_htb_walk (hawk_htb_t* htb, walker_t walker, void* ctx)
{
	hawk_oow_t i;
	pair_t* pair, * next;

	for (i = 0; i < htb->capa; i++) 
	{
		pair = htb->bucket[i];

		while (pair != HAWK_NULL) 
		{
			next = NEXT(pair);
			if (walker(htb, pair, ctx) == HAWK_HTB_WALK_STOP) return;
			pair = next;
		}
	}
}


void hawk_init_htb_itr (hawk_htb_itr_t* itr)
{
	itr->pair = HAWK_NULL;
	itr->buckno = 0;
}

pair_t* hawk_htb_getfirstpair (hawk_htb_t* htb, hawk_htb_itr_t* itr)
{
	hawk_oow_t i;
	pair_t* pair;

	for (i = 0; i < htb->capa; i++)
	{
		pair = htb->bucket[i];
		if (pair) 
		{
			itr->buckno = i;
			itr->pair = pair;
			return pair;
		}
	}

	return HAWK_NULL;
}

pair_t* hawk_htb_getnextpair (hawk_htb_t* htb, hawk_htb_itr_t* itr)
{
	hawk_oow_t i;
	pair_t* pair;

	pair = NEXT(itr->pair);
	if (pair) 
	{
		/* no change in bucket number */
		itr->pair = pair;
		return pair;
	}

	for (i = itr->buckno + 1; i < htb->capa; i++)
	{
		pair = htb->bucket[i];
		if (pair) 
		{
			itr->buckno = i;
			itr->pair = pair;
			return pair;
		}
	}

	return HAWK_NULL;
}

hawk_oow_t hawk_htb_dflhash (const hawk_htb_t* htb, const void* kptr, hawk_oow_t klen)
{
	hawk_oow_t h;
	HAWK_HASH_BYTES (h, kptr, klen);
	return h ; 
}

int hawk_htb_dflcomp (const hawk_htb_t* htb, const void* kptr1, hawk_oow_t klen1, const void* kptr2, hawk_oow_t klen2)
{
	if (klen1 == klen2) return HAWK_MEMCMP (kptr1, kptr2, KTOB(htb,klen1));
	/* it just returns 1 to indicate that they are different. */
	return 1;
}

