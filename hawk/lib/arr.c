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

#include <hawk-arr.h>
#include "hawk-prv.h"

#define slot_t   hawk_arr_slot_t
#define copier_t hawk_arr_copier_t
#define freeer_t hawk_arr_freeer_t
#define comper_t hawk_arr_comper_t
#define sizer_t  hawk_arr_sizer_t
#define keeper_t hawk_arr_keeper_t
#define walker_t hawk_arr_walker_t


#define TOB(arr,len) ((len)*(arr)->scale)
#define DPTR(slot)   ((slot)->val.ptr)
#define DLEN(slot)   ((slot)->val.len)

/* the default comparator is not proper for number comparision.
 * the first different byte decides whice side is greater */
int hawk_arr_dflcomp (hawk_arr_t* arr, const void* dptr1, hawk_oow_t dlen1, const void* dptr2, hawk_oow_t dlen2)
{
	/*
	if (dlen1 == dlen2) return HAWK_MEMCMP(dptr1, dptr2, TOB(arr,dlen1));
	return 1;
	*/

	int n;
	hawk_oow_t min;

	min = (dlen1 < dlen2)? dlen1: dlen2;
	n = HAWK_MEMCMP(dptr1, dptr2, TOB(arr,min));
	if (n == 0 && dlen1 != dlen2) 
	{
		n = (dlen1 > dlen2)? 1: -1;
	}

	return n;
}

static HAWK_INLINE slot_t* alloc_slot (hawk_arr_t* arr, void* dptr, hawk_oow_t dlen)
{
	slot_t* n;

	if (arr->style->copier == HAWK_ARR_COPIER_SIMPLE)
	{
		n = (slot_t*)hawk_gem_allocmem(arr->gem, HAWK_SIZEOF(slot_t));
		if (HAWK_UNLIKELY(!n)) return HAWK_NULL;
		DPTR(n) = dptr; /* store the pointer */
	}
	else if (arr->style->copier == HAWK_ARR_COPIER_INLINE)
	{
		n = (slot_t*)hawk_gem_allocmem(arr->gem, HAWK_SIZEOF(slot_t) + TOB(arr,dlen));
		if (HAWK_UNLIKELY(!n)) return HAWK_NULL; 

		HAWK_MEMCPY (n + 1, dptr, TOB(arr,dlen)); /* copy data contents */
		DPTR(n) = n + 1;
	}
	else
	{
		n = (slot_t*)hawk_gem_allocmem(arr->gem, HAWK_SIZEOF(slot_t));
		if (HAWK_UNLIKELY(!n)) return HAWK_NULL;
		DPTR(n) = arr->style->copier(arr, dptr, dlen); /* call the custom copier */
		if (HAWK_UNLIKELY(!DPTR(n))) 
		{
			hawk_gem_freemem (arr->gem, n);
			return HAWK_NULL;
		}
	}

	DLEN(n) = dlen; 

	return n;
}

hawk_arr_t* hawk_arr_open (hawk_gem_t* gem, hawk_oow_t xtnsize, hawk_oow_t capa)
{
	hawk_arr_t* arr;

	arr = (hawk_arr_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_arr_t) + xtnsize);
	if (HAWK_UNLIKELY(!arr)) return HAWK_NULL;

	if (hawk_arr_init(arr, gem, capa) <= -1)
	{
		hawk_gem_freemem (gem, arr);
		return HAWK_NULL;
	}

	HAWK_MEMSET (arr + 1, 0, xtnsize);
	return arr;
}

void hawk_arr_close (hawk_arr_t* arr)
{
	hawk_arr_fini (arr);
	hawk_gem_freemem (arr->gem, arr);
}


static hawk_arr_style_t style[] =
{
	{
		HAWK_ARR_COPIER_DEFAULT,
		HAWK_ARR_FREEER_DEFAULT,
		HAWK_ARR_COMPER_DEFAULT,
		HAWK_ARR_KEEPER_DEFAULT,
		HAWK_ARR_SIZER_DEFAULT
	},

	{
		HAWK_ARR_COPIER_INLINE,
		HAWK_ARR_FREEER_DEFAULT,
		HAWK_ARR_COMPER_DEFAULT,
		HAWK_ARR_KEEPER_DEFAULT,
		HAWK_ARR_SIZER_DEFAULT
	}
};

const hawk_arr_style_t* hawk_get_arr_style (hawk_arr_style_kind_t kind)
{
	return &style[kind];
}

int hawk_arr_init (hawk_arr_t* arr, hawk_gem_t* gem, hawk_oow_t capa)
{
	HAWK_MEMSET (arr, 0, HAWK_SIZEOF(*arr));

	arr->gem = gem;
	arr->size = 0;
	arr->tally = 0;
	arr->capa = 0;
	arr->slot = HAWK_NULL;
	arr->scale = 1;
	arr->heap_pos_offset = HAWK_ARR_NIL;
	arr->style = &style[0];

	return (hawk_arr_setcapa(arr, capa) == HAWK_NULL)? -1: 0;
}

void hawk_arr_fini (hawk_arr_t* arr)
{
	hawk_arr_clear (arr);

	if (arr->slot)
	{
		hawk_gem_freemem (arr->gem, arr->slot);
		arr->slot = HAWK_NULL;
		arr->capa = 0;
		arr->size = 0 ;
		arr->tally = 0;
	}
}

int hawk_arr_getscale (hawk_arr_t* arr)
{
	return arr->scale;
}

void hawk_arr_setscale (hawk_arr_t* arr, int scale)
{
	/* The scale should be larger than 0 and less than or equal to the 
	 * maximum value that the hawk_uint8_t type can hold */
	HAWK_ASSERT (scale > 0 && scale <= HAWK_TYPE_MAX(hawk_uint8_t));

	if (scale <= 0) scale = 1;
	if (scale > HAWK_TYPE_MAX(hawk_uint8_t)) scale = HAWK_TYPE_MAX(hawk_uint8_t);

	arr->scale = scale;
}

const hawk_arr_style_t* hawk_arr_getstyle (hawk_arr_t* arr)
{
	return &arr->style;
}

void hawk_arr_setstyle (hawk_arr_t* arr, const hawk_arr_style_t* style)
{
	HAWK_ASSERT (style != HAWK_NULL);
	arr->style = style;
}

hawk_oow_t hawk_arr_getsize (hawk_arr_t* arr)
{
	return arr->size;
}

hawk_oow_t hawk_arr_getcapa (hawk_arr_t* arr)
{
	return arr->capa;
}

hawk_arr_t* hawk_arr_setcapa (hawk_arr_t* arr, hawk_oow_t capa)
{
	void* tmp;

	if (capa == arr->capa) return arr;

	if (capa < arr->size)
	{
		/* to trigger freeers on the items truncated */
		hawk_arr_delete (arr, capa, arr->size - capa);
		HAWK_ASSERT (arr->size <= capa);
	}

	if (capa > 0) 
	{
		tmp = (slot_t**)hawk_gem_reallocmem(arr->gem, arr->slot, HAWK_SIZEOF(*arr->slot) * capa);
		if (HAWK_UNLIKELY(!tmp)) return HAWK_NULL;
	}
	else 
	{
		if (arr->slot)
		{
			hawk_arr_clear (arr);
			hawk_gem_freemem (arr->gem, arr->slot);
		}

		tmp = HAWK_NULL;

		HAWK_ASSERT (arr->size == 0);
		HAWK_ASSERT (arr->tally == 0);
	}

	arr->slot = tmp;
	arr->capa = capa;
	
	return arr;
}

hawk_oow_t hawk_arr_search (hawk_arr_t* arr, hawk_oow_t pos, const void* dptr, hawk_oow_t dlen)
{
	hawk_oow_t i;

	for (i = pos; i < arr->size; i++) 
	{
		if (arr->slot[i] == HAWK_NULL) continue;
		if (arr->style->comper(arr, DPTR(arr->slot[i]), DLEN(arr->slot[i]), dptr, dlen) == 0) return i;
	}

	hawk_gem_seterrnum (arr->gem, HAWK_NULL, HAWK_ENOENT);
	return HAWK_ARR_NIL;
}

hawk_oow_t hawk_arr_rsearch (hawk_arr_t* arr, hawk_oow_t pos, const void* dptr, hawk_oow_t dlen)
{
	hawk_oow_t i;

	if (arr->size > 0)
	{
		if (pos >= arr->size) pos = arr->size - 1;

		for (i = pos + 1; i-- > 0; ) 
		{
			if (arr->slot[i] == HAWK_NULL) continue;
			if (arr->style->comper(arr, DPTR(arr->slot[i]), DLEN(arr->slot[i]), dptr, dlen) == 0) return i;
		}
	}

	hawk_gem_seterrnum (arr->gem, HAWK_NULL, HAWK_ENOENT);
	return HAWK_ARR_NIL;
}

hawk_oow_t hawk_arr_upsert (hawk_arr_t* arr, hawk_oow_t pos, void* dptr, hawk_oow_t dlen)
{
	if (pos < arr->size) return hawk_arr_update (arr, pos, dptr, dlen);
	return hawk_arr_insert(arr, pos, dptr, dlen);
}

hawk_oow_t hawk_arr_insert (hawk_arr_t* arr, hawk_oow_t pos, void* dptr, hawk_oow_t dlen)
{
	hawk_oow_t i;
	slot_t* slot;

	/* allocate the slot first */
	slot = alloc_slot(arr, dptr, dlen);
	if (HAWK_UNLIKELY(!slot)) return HAWK_ARR_NIL;

	/* do resizeing if necessary. 
	 * resizing is performed after slot allocation because that way, it 
	 * doesn't modify arr on any errors */
	if (pos >= arr->capa || arr->size >= arr->capa) 
	{
		hawk_oow_t capa, mincapa;

		/* get the minimum capacity needed */
		mincapa = (pos >= arr->size)? (pos + 1): (arr->size + 1);

		if (arr->style->sizer)
		{
			capa = arr->style->sizer(arr, mincapa);
		}
		else
		{
			if (arr->capa <= 0) 
			{
				HAWK_ASSERT (arr->size <= 0);
				capa = HAWK_ALIGN_POW2(pos + 1, 64);
			}
			else 
			{
				hawk_oow_t bound = (pos >= arr->size)? pos: arr->size;
				capa = HAWK_ALIGN_POW2(bound + 1, 64);
				do { capa = arr->capa * 2; } while (capa <= bound);
			}
		}

		do
		{
			if (hawk_arr_setcapa(arr, capa) != HAWK_NULL) break;

			if (capa <= mincapa)
			{
				if (arr->style->freeer) arr->style->freeer (arr, DPTR(slot), DLEN(slot));
				hawk_gem_freemem (arr->gem, slot);
				return HAWK_ARR_NIL;
			}

			capa--; /* let it retry after lowering the capacity */
		} 
		while (1);

		if (pos >= arr->capa || arr->size >= arr->capa)  /* can happen if the sizer() callback isn't good enough */
		{
			/* the buffer is not still enough after resizing */
			if (arr->style->freeer) arr->style->freeer (arr, DPTR(slot), DLEN(slot));
			hawk_gem_freemem (arr->gem, slot);
			hawk_gem_seterrnum (arr->gem, HAWK_NULL, HAWK_EBUFFULL);
			return HAWK_ARR_NIL;
		}
	}

	/* fill in the gap with HAWK_NULL */
	for (i = arr->size; i < pos; i++) arr->slot[i] = HAWK_NULL;

	/* shift values to the next cell */
	for (i = arr->size; i > pos; i--) arr->slot[i] = arr->slot[i-1];

	/*  set the value */
	arr->slot[pos] = slot;

	if (pos > arr->size) arr->size = pos + 1;
	else arr->size++;
	arr->tally++;

	return pos;
}

hawk_oow_t hawk_arr_update (hawk_arr_t* arr, hawk_oow_t pos, void* dptr, hawk_oow_t dlen)
{
	slot_t* c;

	if (pos >= arr->size) 
	{
		hawk_gem_seterrnum (arr->gem, HAWK_NULL, HAWK_EINVAL);
		return HAWK_ARR_NIL;
	}

	c = arr->slot[pos];
	if (c == HAWK_NULL)
	{
		/* no previous data */
		arr->slot[pos] = alloc_slot(arr, dptr, dlen);
		if (arr->slot[pos] == HAWK_NULL) return HAWK_ARR_NIL;
		arr->tally++;
	}
	else
	{
		if (dptr == DPTR(c) && dlen == DLEN(c))
		{
			/* updated to the same data */
			if (arr->style->keeper) arr->style->keeper (arr, dptr, dlen);
		}
		else
		{
			/* updated to different data */
			slot_t* slot = alloc_slot(arr, dptr, dlen);
			if (HAWK_UNLIKELY(!slot)) return HAWK_ARR_NIL;

			if (arr->style->freeer) arr->style->freeer (arr, DPTR(c), DLEN(c));
			hawk_gem_freemem (arr->gem, c);

			arr->slot[pos] = slot;
		}
	}

	return pos;
}

hawk_oow_t hawk_arr_delete (hawk_arr_t* arr, hawk_oow_t index, hawk_oow_t count)
{
	hawk_oow_t i;

	if (index >= arr->size) return 0;
	if (count > arr->size - index) count = arr->size - index;

	i = index;

	for (i = index; i < index + count; i++)
	{
		slot_t* c = arr->slot[i];
		if (c)
		{
			if (arr->style->freeer) arr->style->freeer (arr, DPTR(c), DLEN(c));
			hawk_gem_freemem (arr->gem, c);
			arr->slot[i] = HAWK_NULL;
			arr->tally--;
		}
	}

	for (i = index + count; i < arr->size; i++)
	{
		arr->slot[i - count] = arr->slot[i];
	}
	arr->slot[arr->size - 1] = HAWK_NULL;

	arr->size -= count;
	return count;
}

hawk_oow_t hawk_arr_uplete (hawk_arr_t* arr, hawk_oow_t index, hawk_oow_t count)
{
	hawk_oow_t i;

	if (index >= arr->size) return 0;
	if (count > arr->size - index) count = arr->size - index;

	i = index;

	for (i = index; i < index + count; i++)
	{
		slot_t* c = arr->slot[i];
		if (c)
		{
			if (arr->style->freeer) arr->style->freeer (arr, DPTR(c), DLEN(c));
			hawk_gem_freemem (arr->gem, c);
			arr->slot[i] = HAWK_NULL;
			arr->tally--;
		}
	}

	return count;
}

void hawk_arr_clear (hawk_arr_t* arr)
{
	hawk_oow_t i;

	for (i = 0; i < arr->size; i++) 
	{
		slot_t* c = arr->slot[i];
		if (c != HAWK_NULL)
		{
			if (arr->style->freeer) arr->style->freeer (arr, DPTR(c), DLEN(c));
			hawk_gem_freemem (arr->gem, c);
			arr->slot[i] = HAWK_NULL;
		}
	}

	arr->size = 0;
	arr->tally = 0;
}

hawk_oow_t hawk_arr_walk (hawk_arr_t* arr, walker_t walker, void* ctx)
{
	hawk_arr_walk_t w = HAWK_ARR_WALK_FORWARD;
	hawk_oow_t i = 0, nwalks = 0;

	if (arr->size <= 0) return 0;

	while (1)	
	{
		if (arr->slot[i])
		{
			w = walker(arr, i, ctx);
			nwalks++;
		}

		if (w == HAWK_ARR_WALK_STOP) break;

		if (w == HAWK_ARR_WALK_FORWARD) 
		{
			i++;
			if (i >= arr->size) break;
		}
		if (w == HAWK_ARR_WALK_BACKWARD) 
		{
			if (i <= 0) break;
			i--;
		}
	}

	return nwalks;
}

hawk_oow_t hawk_arr_rwalk (hawk_arr_t* arr, walker_t walker, void* ctx)
{
	hawk_arr_walk_t w = HAWK_ARR_WALK_BACKWARD;
	hawk_oow_t i, nwalks = 0;

	if (arr->size <= 0) return 0;
	i = arr->size - 1;

	while (1)	
	{
		if (arr->slot[i] != HAWK_NULL) 
		{
                	w = walker (arr, i, ctx);
			nwalks++;
		}

		if (w == HAWK_ARR_WALK_STOP) break;

		if (w == HAWK_ARR_WALK_FORWARD) 
		{
			i++;
			if (i >= arr->size) break;
		}
		if (w == HAWK_ARR_WALK_BACKWARD) 
		{
			if (i <= 0) break;
			i--;
		}
	}

	return nwalks;
}

hawk_oow_t hawk_arr_pushstack (hawk_arr_t* arr, void* dptr, hawk_oow_t dlen)
{
	return hawk_arr_insert (arr, arr->size, dptr, dlen);
}

void hawk_arr_popstack (hawk_arr_t* arr)
{
	HAWK_ASSERT (arr->size > 0);
	hawk_arr_delete (arr, arr->size - 1, 1);
}

#define HEAP_PARENT(x) (((x)-1) / 2)
#define HEAP_LEFT(x)   ((x)*2 + 1)
#define HEAP_RIGHT(x)  ((x)*2 + 2)

#define HEAP_UPDATE_POS(arr, index) \
	do { \
		if (arr->heap_pos_offset != HAWK_ARR_NIL) \
			*(hawk_oow_t*)((hawk_uint8_t*)DPTR(arr->slot[index]) + arr->heap_pos_offset) = index; \
	} while(0)

static hawk_oow_t sift_up (hawk_arr_t* arr, hawk_oow_t index)
{
	hawk_oow_t parent;

	if (index > 0)
	{
		int n;

		parent = HEAP_PARENT(index);
		n = arr->style->comper(arr, DPTR(arr->slot[index]), DLEN(arr->slot[index]), DPTR(arr->slot[parent]), DLEN(arr->slot[parent]));
		if (n > 0)
		{
			slot_t* tmp;

			tmp = arr->slot[index];

			while (1)
			{
				arr->slot[index] = arr->slot[parent];
				HEAP_UPDATE_POS (arr, index);

				index = parent;
				parent = HEAP_PARENT(parent);

				if (index <= 0) break;

				n = arr->style->comper(arr, DPTR(tmp), DLEN(tmp), DPTR(arr->slot[parent]), DLEN(arr->slot[parent]));
				if (n <= 0) break;
			} 

			arr->slot[index] = tmp;
			HEAP_UPDATE_POS (arr, index);
		}
	}
	return index;
}

static hawk_oow_t sift_down (hawk_arr_t* arr, hawk_oow_t index)
{
	hawk_oow_t base;

	base = arr->size / 2;

	if (index < base) /* at least 1 child is under the 'index' position */
	{
		slot_t* tmp;

		tmp = arr->slot[index];

		do
		{
			hawk_oow_t left, right, child;
			int n;

			left= HEAP_LEFT(index);
			right = HEAP_RIGHT(index);

			if (right < arr->size)
			{
				n = arr->style->comper(arr,
					DPTR(arr->slot[right]), DLEN(arr->slot[right]),
					DPTR(arr->slot[left]), DLEN(arr->slot[left]));
				child = (n > 0)? right: left;
			}
			else
			{
				child = left;
			}

			n = arr->style->comper(arr, DPTR(tmp), DLEN(tmp), DPTR(arr->slot[child]), DLEN(arr->slot[child]));
			if (n > 0) break;

			arr->slot[index] = arr->slot[child];
			HEAP_UPDATE_POS (arr, index);
			index = child;
		}
		while (index < base);

		arr->slot[index] = tmp;
		HEAP_UPDATE_POS (arr, index);
	}

	return index;
}

hawk_oow_t hawk_arr_pushheap (hawk_arr_t* arr, void* dptr, hawk_oow_t dlen)
{
	hawk_oow_t index;

	/* add a value at the back of the array  */
	index = arr->size;
	if (hawk_arr_insert(arr, index, dptr, dlen) == HAWK_ARR_NIL) return HAWK_ARR_NIL;
	HEAP_UPDATE_POS (arr, index);

	HAWK_ASSERT (arr->size == index + 1);

	/* move the item upto the top if it's greater than the parent items */
	sift_up (arr, index);
	return arr->size;
}

void hawk_arr_popheap (hawk_arr_t* arr)
{
	HAWK_ASSERT (arr->size > 0);
	hawk_arr_deleteheap (arr, 0);
}

void hawk_arr_deleteheap (hawk_arr_t* arr, hawk_oow_t index)
{
	slot_t* tmp;

	HAWK_ASSERT (arr->size > 0);
	HAWK_ASSERT (index < arr->size);

	/* remember the item to destroy */
	tmp = arr->slot[index];

	arr->size = arr->size - 1;
	if (arr->size > 0 && index != arr->size)
	{
		int n;

		/* move the last item to the deleting position */
		arr->slot[index] = arr->slot[arr->size];
		HEAP_UPDATE_POS (arr, index);

		/* move it up if the last item is greater than the item to be deleted,
		 * move it down otherwise. */
		n = arr->style->comper(arr, DPTR(arr->slot[index]), DLEN(arr->slot[index]), DPTR(tmp), DLEN(tmp));
		if (n > 0) sift_up (arr, index);
		else if (n < 0) sift_down (arr, index);
	}

	/* destroy the actual item */
	if (arr->style->freeer) arr->style->freeer (arr, DPTR(tmp), DLEN(tmp));
	hawk_gem_freemem (arr->gem, tmp);

	/* empty the last slot */
	arr->slot[arr->size] = HAWK_NULL;
	arr->tally--;
}

hawk_oow_t hawk_arr_updateheap (hawk_arr_t* arr, hawk_oow_t index, void* dptr, hawk_oow_t dlen)
{
	slot_t* tmp;
	int n;

	tmp = arr->slot[index];
	HAWK_ASSERT (tmp != HAWK_NULL);

	n = arr->style->comper(arr, dptr, dlen, DPTR(tmp), DLEN(tmp));
	if (n)
	{
		if (hawk_arr_update(arr, index, dptr, dlen) == HAWK_ARR_NIL) return HAWK_ARR_NIL;
		HEAP_UPDATE_POS (arr, index);

		if (n > 0) sift_up (arr, index);
		else sift_down (arr, index);
	}

	return index;
}

hawk_oow_t hawk_arr_getheapposoffset (hawk_arr_t* arr)
{
	return arr->heap_pos_offset;
}

void hawk_arr_setheapposoffset (hawk_arr_t* arr, hawk_oow_t offset)
{
	arr->heap_pos_offset = offset;
}
