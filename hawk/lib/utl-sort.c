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

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <hawk-utl.h>

#define qsort_min(a,b) (((a)<(b))? a: b)

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE,parmi,parmj,n) do { \
	hawk_oow_t i = (n) / HAWK_SIZEOF (TYPE); \
	register TYPE *pi = (TYPE*)(parmi); \
	register TYPE *pj = (TYPE*)(parmj); \
	do { 						\
		register TYPE t = *pi;	\
		*pi++ = *pj;	\
		*pj++ = t;		\
	} while (--i > 0);	\
} while(0)


#define get_swaptype(a, elemsize) (((hawk_oob_t*)(a) - (hawk_oob_t*)0) % HAWK_SIZEOF(long) || elemsize % HAWK_SIZEOF(long)? 2 : elemsize == HAWK_SIZEOF(long)? 0 : 1)

#define swap(a, b, elemsize) do { \
	switch (swaptype) \
	{ \
		case 0: \
		{ \
			long t = *(long*)(a); \
			*(long*)(a) = *(long*)(b); \
			*(long*)(b) = t; \
			break; \
		} \
		case 1: \
			swapcode(long, a, b, elemsize); \
			break; \
		default: \
			swapcode(hawk_oob_t, a, b, elemsize); \
			break; \
	} \
} while(0)
	
	
#define vecswap(a,b,n) do {  \
	if ((n) > 0)  \
	{ \
		if (swaptype <= 1) swapcode(long, a, b, n); \
		else swapcode(hawk_oob_t, a, b, n);  \
	} \
} while(0)

static HAWK_INLINE hawk_oob_t* med3 (hawk_oob_t* a, hawk_oob_t* b, hawk_oob_t* c, hawk_sort_comper_t comper, void* ctx)
{
	if (comper(a, b, ctx) < 0) 
	{
		if (comper(b, c, ctx) < 0) return b;
		return (comper(a, c, ctx) < 0)? c: a;
	}
	else 
	{
		if (comper(b, c, ctx) > 0) return b;
		return (comper(a, c, ctx) > 0)? c: a;
	}
}

static HAWK_INLINE hawk_oob_t* med3x (hawk_oob_t* a, hawk_oob_t* b, hawk_oob_t* c, hawk_sort_comperx_t comper, void* ctx)
{
	int n;

	if (comper(a, b, ctx, &n) <= -1) return HAWK_NULL;
	if (n < 0)
	{
		if (comper(b, c, ctx, &n) <= -1) return HAWK_NULL;
		if (n < 0) return b;

		if (comper(a, c, ctx, &n) <= -1) return HAWK_NULL;
		return (n < 0)? c: a;
	}
	else 
	{
		if (comper(b, c, ctx, &n) <= -1) return HAWK_NULL;
		if (n > 0) return b;
		if (comper(a, c, ctx, &n) <= -1) return HAWK_NULL;
		return (n > 0)? c: a;
	}
}

void hawk_qsort (void* base, hawk_oow_t nmemb, hawk_oow_t size, hawk_sort_comper_t comper, void* ctx)
{
	hawk_oob_t* pa, * pb, * pc, * pd, * pl, * pm, * pn;
	int swaptype, swap_cnt;
	long r;
	hawk_oow_t d;
	register hawk_oob_t* a = (hawk_oob_t*)base;

loop:	
	swaptype = get_swaptype(a, size);

	swap_cnt = 0;
	if (nmemb < 7) 
	{
		hawk_oob_t* end = (hawk_oob_t*)a + (nmemb * size);
		for (pm = (hawk_oob_t*)a + size; pm < end; pm += size)
		{
			for (pl = pm; pl > (hawk_oob_t*)a && comper(pl - size, pl, ctx) > 0; pl -= size)
			{
				swap(pl, pl - size, size);
			}
		}
		return;
	}
	pm = (hawk_oob_t*)a + (nmemb / 2) * size;
	if (nmemb > 7) 
	{
		pl = (hawk_oob_t*)a;
		pn = (hawk_oob_t*)a + (nmemb - 1) * size;
		if (nmemb > 40) 
		{
			d = (nmemb / 8) * size;
			pl = med3(pl, pl + d, pl + 2 * d, comper, ctx);
			pm = med3(pm - d, pm, pm + d, comper, ctx);
			pn = med3(pn - 2 * d, pn - d, pn, comper, ctx);
		}
		pm = med3(pl, pm, pn, comper, ctx);
	}
	swap(a, pm, size);
	pa = pb = (hawk_oob_t*)a + size;

	pc = pd = (hawk_oob_t*)a + (nmemb - 1) * size;
	for (;;) 
	{
		while (pb <= pc && (r = comper(pb, a, ctx)) <= 0) 
		{
			if (r == 0) 
			{
				swap_cnt = 1;
				swap(pa, pb, size);
				pa += size;
			}
			pb += size;
		}
		while (pb <= pc && (r = comper(pc, a, ctx)) >= 0) 
		{
			if (r == 0) 
			{
				swap_cnt = 1;
				swap(pc, pd, size);
				pd -= size;
			}
			pc -= size;
		}
		if (pb > pc) break;
		swap (pb, pc, size);
		swap_cnt = 1;
		pb += size;
		pc -= size;
	}

	if (swap_cnt == 0) 
	{
		 /* switch to insertion sort */
		for (pm = (hawk_oob_t*)a + size; 
		     pm < (hawk_oob_t*)a + nmemb * size; pm += size)
		{
			for (pl = pm; pl > (hawk_oob_t*)a && comper(pl - size, pl, ctx) > 0; pl -= size)
			{
				swap(pl, pl - size, size);
			}
		}
		return;
	}

	pn = (hawk_oob_t*)a + nmemb * size;
	r = qsort_min(pa - (hawk_oob_t*)a, pb - pa);
	vecswap (a, pb - r, r);
	r = qsort_min (pd - pc, pn - pd - size);
	vecswap (pb, pn - r, r);

	if ((r = pb - pa) > size) hawk_qsort(a, r / size, size, comper, ctx);

	if ((r = pd - pc) > size) 
	{
		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		nmemb = r / size;
		goto loop;
	}
/*	qsort(pn - r, r / size, size, comper);*/
}

int hawk_qsortx (void* base, hawk_oow_t nmemb, hawk_oow_t size, hawk_sort_comperx_t comper, void* ctx)
{
	hawk_oob_t* pa, * pb, * pc, * pd, * pl, * pm, * pn;
	int swaptype, swap_cnt;
	long r;
	int n;
	hawk_oow_t d;
	register hawk_oob_t* a = (hawk_oob_t*)base;

loop:	
	swaptype = get_swaptype(a, size);

	swap_cnt = 0;
	if (nmemb < 7) 
	{
		hawk_oob_t* end = (hawk_oob_t*)a + (nmemb * size);
		for (pm = (hawk_oob_t*)a + size; pm < end; pm += size)
		{
			pl = pm;
			while (pl > (hawk_oob_t*)a)
			{
				hawk_oob_t* pl2 = pl - size;
				if (comper(pl2, pl, ctx, &n) <= -1) return -1;
				if (n <= 0) break;
				swap (pl, pl2, size);
				pl = pl2;
			}
		}
		return 0;
	}
	pm = (hawk_oob_t*)a + (nmemb / 2) * size;
	if (nmemb > 7) 
	{
		pl = (hawk_oob_t*)a;
		pn = (hawk_oob_t*)a + (nmemb - 1) * size;
		if (nmemb > 40) 
		{
			d = (nmemb / 8) * size;
			pl = med3x(pl, pl + d, pl + 2 * d, comper, ctx);
			if (!pl) return -1;
			pm = med3x(pm - d, pm, pm + d, comper, ctx);
			if (!pm) return -1;
			pn = med3x(pn - 2 * d, pn - d, pn, comper, ctx);
			if (!pn) return -1;
		}
		pm = med3x(pl, pm, pn, comper, ctx);
		if (!pm) return -1;
	}
	swap(a, pm, size);
	pa = pb = (hawk_oob_t*)a + size;

	pc = pd = (hawk_oob_t*)a + (nmemb - 1) * size;
	for (;;) 
	{
		while (pb <= pc)
		{
			if (comper(pb, a, ctx, &n) <= -1) return -1;
			if (n > 0) break;

			if (n == 0) 
			{
				swap_cnt = 1;
				swap(pa, pb, size);
				pa += size;
			}
			pb += size;
		}
		while (pb <= pc)
		{
			if (comper(pc, a, ctx, &n) <= -1) return -1;
			if (n < 0) break;

			if (n == 0) 
			{
				swap_cnt = 1;
				swap(pc, pd, size);
				pd -= size;
			}
			pc -= size;
		}
		if (pb > pc) break;
		swap (pb, pc, size);
		swap_cnt = 1;
		pb += size;
		pc -= size;
	}

	if (swap_cnt == 0) 
	{
		/* switch to insertion sort */
		hawk_oob_t* end = (hawk_oob_t*)a + (nmemb * size);
		for (pm = (hawk_oob_t*)a + size; pm < end; pm += size)
		{
			pl = pm;
			while (pl > (hawk_oob_t*)a)
			{
				if (comper(pl - size, pl, ctx, &n) <= -1) return -1;
				if (n <= 0) break;
				swap(pl, pl - size, size);
				pl = pl - size;
			}
		}
		return 0;
	}

	pn = (hawk_oob_t*)a + nmemb * size;
	r = qsort_min(pa - (hawk_oob_t*)a, pb - pa);
	vecswap (a, pb - r, r);
	r = qsort_min (pd - pc, pn - pd - size);
	vecswap (pb, pn - r, r);

	if ((r = pb - pa) > size) 
	{
		if (hawk_qsortx(a, r / size, size, comper, ctx) <= -1) return -1;
	}

	if ((r = pd - pc) > size) 
	{
		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		nmemb = r / size;
		goto loop;
	}
/*	qsortx(pn - r, r / size, size, comper);*/

	return 0;
}

#if 0

/* 
 * Below is an example of a naive qsort implementation
 */

#define swap(a,b,size) \
	do  { \
		hawk_oow_t i = 0; \
		hawk_oob_t* p1 = (a); \
		hawk_oob_t* p2 = (b); \
		for (i = 0; i < size; i++) { \
			hawk_oob_t t = *p1; \
			*p1 = *p2; \
			*p2 = t; \
			p1++; p2++; \
		} \
	} while (0)

#define REF(x,i) (&((x)[(i)*size]))

void hawk_qsort (void* base, hawk_oow_t nmemb, hawk_oow_t size, void* arg,
	int (*compar)(const void*, const void*, void*))
{
	hawk_oow_t pivot, start, end;
	hawk_oob_t* p = (hawk_oob_t*)base;

	if (nmemb <= 1) return;
	if (nmemb == 2) {
		if (compar(REF(p,0), REF(p,1), arg) > 0)
			swap (REF(p,0), REF(p,1), size);
		return;
	}

	pivot = nmemb >> 1; /* choose the middle as the pivot index */
	swap (REF(p,pivot), REF(p,nmemb-1), size); /* swap the pivot with the last item */

	start = 0; end = nmemb - 2; 

	while (1) {
		/* look for the larger value than pivot */
		while (start <= end && 
		       compar(REF(p,start), REF(p,nmemb-1), arg) <= 0) start++;

		/* look for the less value than pivot. */
		while (end > start && 
		       compar(REF(p,end), REF(p,nmemb-1), arg) >= 0) end--;

		if (start >= end) break; /* no more to swap */
		swap (REF(p,start), REF(p,end), size);
		start++; end--;
	}

	swap (REF(p,nmemb-1), REF(p,start), size);
	pivot = start;  /* adjust the pivot index */

	hawk_qsort (REF(p,0), pivot, size, arg, compar);
	hawk_qsort (REF(p,pivot+1), nmemb - pivot - 1, size, arg, compar);
}

/* 
 * For easier understanding, see the following
 */

#define swap(a, b) do { a ^= b; b ^= a; a ^= b; } while (0)

void qsort (int* x, size_t n)
{
	size_t index, start, end;
	int pivot;

	if (n <= 1) return;
	if (n == 2) {
		if (x[0] > x[1]) swap (x[0], x[1]);
		return;
	}

	index = n / 2; /* choose the middle as the pivot index */
	pivot = x[index]; /* store the pivot value */
	swap (x[index], x[n - 1]); /* swap the pivot with the last item */

	start = 0; end = n - 2; 
	while (1) {
		/* look for the larger value than pivot */
		while (start <= end && x[start] <= pivot) start++;

		/* look for the less value than pivot. */
		while (end > start && x[end] >= pivot) end--;

		if (start >= end) {
			/* less values all on the left, 
			 * larger values all on the right
			 */
			break;
		}

		swap (x[start], x[end]);
		start++; end--;
	}

	x[n - 1] = x[start]; /* restore the pivot value */
	x[start] = pivot;
	index = start;  /* adjust the pivot index */

	qsort (x, index);
	qsort (&x[index + 1], n - index - 1);
}

#endif
