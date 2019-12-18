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

/*
 * This is the TRE memory allocator modified for Hawk.
 * See the original license notice below.
 */

/*
 tre-mem.c - TRE memory allocator

 This software is released under a BSD-style license.
 See the file LICENSE for details and copyright.
 */

/*
 This memory allocator is for allocating small memory blocks efficiently
 in terms of memory overhead and execution speed.  The allocated blocks
 cannot be freed individually, only all at once.  There can be multiple
 allocators, though.
 */

#include <hawk-pma.h>
#include "hawk-prv.h"

/* Returns number of bytes to add to (char *)ptr to make it
   properly aligned for the type. */
#define ALIGN(ptr, type) \
	((((hawk_uintptr_t)ptr) % HAWK_SIZEOF(type))? \
		(HAWK_SIZEOF(type) - (((hawk_uintptr_t)ptr) % HAWK_SIZEOF(type))) : 0)


hawk_pma_t* hawk_pma_open (hawk_gem_t* gem, hawk_oow_t xtnsize) 
{
	hawk_pma_t* pma;

	pma = (hawk_pma_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(*pma) + xtnsize);
	if (!pma) return HAWK_NULL;

	if (hawk_pma_init(pma, gem) <= -1)
	{
		hawk_gem_freemem (gem, pma);
		return HAWK_NULL;
	}

	HAWK_MEMSET (pma + 1, 0, xtnsize);
	return pma;
}

void hawk_pma_close (hawk_pma_t* pma)
{
	hawk_pma_fini (pma);
	hawk_gem_freemem (pma->gem, pma);
}

int hawk_pma_init (hawk_pma_t* pma, hawk_gem_t* gem)
{
	HAWK_MEMSET (pma, 0, HAWK_SIZEOF(*pma));
	pma->gem = gem;
	return 0;
}

/* Frees the memory allocator and all memory allocated with it. */
void hawk_pma_fini (hawk_pma_t* pma)
{
	hawk_pma_clear (pma);
}

void hawk_pma_clear (hawk_pma_t* pma)
{
	hawk_gem_t* gem = pma->gem;
	hawk_pma_blk_t* tmp, * l = pma->blocks;

	while (l != HAWK_NULL)
	{
		tmp = l->next;
		hawk_gem_freemem (gem, l);
		l = tmp;
	}
	
	HAWK_MEMSET (pma, 0, HAWK_SIZEOF(*pma));
	pma->gem = gem;
}
/* Returns a new memory allocator or NULL if out of memory. */

/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
 allocated block or NULL if an underlying malloc() failed. */
void* hawk_pma_alloc (hawk_pma_t* pma, hawk_oow_t size)
{
	void *ptr;

	if (pma->failed) return HAWK_NULL;

	if (pma->n < size)
	{
		/* We need more memory than is available in the current block.
		 Allocate a new block. */

		hawk_pma_blk_t* l;
		int block_size;
		if (size * 8 > HAWK_PMA_BLOCK_SIZE)
			block_size = size * 8;
		else
			block_size = HAWK_PMA_BLOCK_SIZE;

		l = hawk_gem_allocmem(pma->gem, HAWK_SIZEOF(*l) + block_size);
		if (l == HAWK_NULL)
		{
			pma->failed = 1;
			return HAWK_NULL;
		}
		l->data = (void*)(l + 1);

		l->next = HAWK_NULL;
		if (pma->current != HAWK_NULL) pma->current->next = l;
		if (pma->blocks == HAWK_NULL) pma->blocks = l;
		pma->current = l;
		pma->ptr = l->data;
		pma->n = block_size;
	}

	/* Make sure the next pointer will be aligned. */
	size += ALIGN((hawk_uintptr_t)(pma->ptr + size), hawk_uintptr_t);

	/* Allocate from current block. */
	ptr = pma->ptr;
	pma->ptr += size;
	pma->n -= size;

	return ptr;
}

void* hawk_pma_calloc (hawk_pma_t* pma, hawk_oow_t size)
{
	void* ptr = hawk_pma_alloc(pma, size);
	if (ptr) HAWK_MEMSET (ptr, 0, size);
	return ptr;
}

void* hawk_pma_realloc (hawk_pma_t* pma, void* blk, hawk_oow_t size)
{
	/* do nothing. you can't resize an individual memory chunk */
	return HAWK_NULL;
}

void hawk_pma_free (hawk_pma_t* pma, void* blk)
{
	/* do nothing. you can't free an individual memory chunk */
}

