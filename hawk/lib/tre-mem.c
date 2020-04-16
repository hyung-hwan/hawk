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

#include "tre-prv.h"
#include "tre-mem.h"


/* Returns a new memory allocator or NULL if out of memory. */
hawk_tre_mem_t
hawk_tre_mem_new_impl(hawk_gem_t* gem, int provided, void *provided_block)
{
  hawk_tre_mem_t mem;
  if (provided)
  {
      mem = provided_block;
      HAWK_MEMSET(mem, 0, sizeof(*mem));
  }
  else
  {
    mem = xcalloc(gem, 1, sizeof(*mem));
    if (mem == NULL) return NULL;
  }

  mem->gem = gem;
  return mem;
}


/* Frees the memory allocator and all memory allocated with it. */
void
hawk_tre_mem_destroy(hawk_tre_mem_t mem)
{
  hawk_tre_mem_list_t *tmp, *l = mem->blocks;

  while (l != NULL)
    {
      xfree(mem->gem, l->data);
      tmp = l->next;
      xfree(mem->gem, l);
      l = tmp;
    }
  xfree(mem->gem, mem);
}


/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
   allocated block or NULL if an underlying malloc() failed. */
void *
hawk_tre_mem_alloc_impl(hawk_tre_mem_t mem, int provided, void *provided_block,
		   int zero, size_t size)
{
  void *ptr;

  if (mem->failed)
    {
      DPRINT(("tre_mem_alloc: oops, called after failure?!\n"));
      return NULL;
    }

  if (mem->n < size)
    {
      /* We need more memory than is available in the current block.
	 Allocate a new block. */
      hawk_tre_mem_list_t *l;
      if (provided)
	{
	  DPRINT(("tre_mem_alloc: using provided block\n"));
	  if (provided_block == NULL)
	    {
	      DPRINT(("tre_mem_alloc: provided block was NULL\n"));
	      mem->failed = 1;
	      return NULL;
	    }
	  mem->ptr = provided_block;
	  mem->n = TRE_MEM_BLOCK_SIZE;
	}
      else
	{
	  int block_size;
	  if (size * 8 > TRE_MEM_BLOCK_SIZE)
	    block_size = size * 8;
	  else
	    block_size = TRE_MEM_BLOCK_SIZE;
	  DPRINT(("tre_mem_alloc: allocating new %d byte block\n",
		  block_size));

	  l = xmalloc(mem->gem, sizeof(*l));
	  if (l == NULL)
	    {
	      mem->failed = 1;
	      return NULL;
	    }
	  l->data = xmalloc(mem->gem, block_size);
	  if (l->data == NULL)
	    {
	      xfree(mem->gem, l);
	      mem->failed = 1;
	      return NULL;
	    }
	  l->next = NULL;
	  if (mem->current != NULL)
	    mem->current->next = l;
	  if (mem->blocks == NULL)
	    mem->blocks = l;
	  mem->current = l;
	  mem->ptr = l->data;
	  mem->n = block_size;
	}
    }

  /* Make sure the next pointer will be aligned. */
  size += ALIGN((hawk_uintptr_t)(mem->ptr + size), hawk_uintptr_t);

  /* Allocate from current block. */
  ptr = mem->ptr;
  mem->ptr += size;
  mem->n -= size;

  /* Set to zero if needed. */
  if (zero) HAWK_MEMSET(ptr, 0, size);
  return ptr;
}

/* EOF */
