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
/*
  tre-mem.h - TRE memory allocator interface

  This software is released under a BSD-style license.
  See the file LICENSE for details and copyright.

*/

#ifndef _HAWK_TRE_MEM_H_
#define _HAWK_TRE_MEM_H_

#include "hawk-prv.h"

#define TRE_MEM_BLOCK_SIZE 1024

typedef struct hawk_tre_mem_list {
  void *data;
  struct hawk_tre_mem_list *next;
} hawk_tre_mem_list_t;

typedef struct hawk_tre_mem_struct {
  hawk_gem_t* gem;
  hawk_tre_mem_list_t *blocks;
  hawk_tre_mem_list_t *current;
  char *ptr;
  size_t n;
  int failed;
  void **provided;
} *hawk_tre_mem_t;


#if defined(__cplusplus)
extern "C" {
#endif

hawk_tre_mem_t hawk_tre_mem_new_impl(hawk_gem_t* gem, int provided, void *provided_block);
void *hawk_tre_mem_alloc_impl(hawk_tre_mem_t mem, int provided, void *provided_block,
			 int zero, size_t size);

/* Returns a new memory allocator or NULL if out of memory. */
#define hawk_tre_mem_new(gem)  hawk_tre_mem_new_impl(gem, 0, NULL)

/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
   allocated block or NULL if an underlying malloc() failed. */
#define hawk_tre_mem_alloc(mem, size) hawk_tre_mem_alloc_impl(mem, 0, NULL, 0, size)

/* Allocates a block of `size' bytes from `mem'.  Returns a pointer to the
   allocated block or NULL if an underlying malloc() failed.  The memory
   is set to zero. */
#define hawk_tre_mem_calloc(mem, size) hawk_tre_mem_alloc_impl(mem, 0, NULL, 1, size)

/* Frees the memory allocator and all memory allocated with it. */
void hawk_tre_mem_destroy(hawk_tre_mem_t mem);

#if defined(__cplusplus)
}
#endif

#endif
