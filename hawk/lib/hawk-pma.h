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
 tre-mem.h - TRE memory allocator interface

 This software is released under a BSD-style license.
 See the file LICENSE for details and copyright.

 */

#ifndef _HAWK_PMA_H_
#define _HAWK_PMA_H_

/** @file
 * This file defines a pool-based block allocator.
 */

#include <hawk-cmn.h>

#define HAWK_PMA_BLOCK_SIZE 1024

typedef struct hawk_pma_blk_t hawk_pma_blk_t;

struct hawk_pma_blk_t
{
	void *data;
	hawk_pma_blk_t* next;
};

/**
 * The hawk_pma_t type defines a pool-based block allocator. You can allocate
 * blocks of memories but you can't resize or free individual blocks allocated.
 * Instead, you can destroy the whole pool once you're done with all the
 * blocks allocated.
 */
typedef struct hawk_pma_t hawk_pma_t;

struct hawk_pma_t
{
	hawk_t* hawk;

	hawk_pma_blk_t* blocks;
	hawk_pma_blk_t* current;

	char *ptr;
	hawk_oow_t n;
	int failed;
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_pma_open() function creates a pool-based memory allocator.
 */
HAWK_EXPORT hawk_pma_t* hawk_pma_open (
	hawk_t*     hawk,    /**< hawk */
	hawk_oow_t  xtnsize  /**< extension size in bytes */
);

/**
 * The hawk_pma_close() function destroys a pool-based memory allocator.
 */
HAWK_EXPORT void hawk_pma_close (
	hawk_pma_t* pma /**< memory allocator */
);

HAWK_EXPORT int hawk_pma_init (
	hawk_pma_t*  pma, /**< memory allocator */
	hawk_t*      hawk /**< hawk */
);

HAWK_EXPORT void hawk_pma_fini (
	hawk_pma_t* pma /**< memory allocator */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_pma_getxtn (hawk_pma_t* pma) { return (void*)(pma + 1); }
#else
#define hawk_pma_getxtn(awk) ((void*)((hawk_pma_t*)(pma) + 1))
#endif

/** 
 * The hawk_pma_clear() function frees all the allocated memory blocks 
 * by freeing the entire memory pool. 
 */
HAWK_EXPORT void hawk_pma_clear (
	hawk_pma_t* pma /**< memory allocator */
);

/**
 * The hawk_pma_alloc() function allocates a memory block of the @a size bytes.
 * @return pointer to a allocated block on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT void* hawk_pma_alloc (
	hawk_pma_t* pma, /**< memory allocator */
	hawk_oow_t size /**< block size */
);	

/**
 * The hawk_pma_alloc() function allocates a memory block of the @a size bytes
 * and initializes the whole block with 0.
 * @return pointer to a allocated block on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT void* hawk_pma_calloc (
	hawk_pma_t* pma, /**< memory allocator */
	hawk_oow_t size /**< block size */
);	

/**
 * The hawk_pma_free() function is provided for completeness, and doesn't
 * resize an individual block @a blk. 
 */
HAWK_EXPORT void* hawk_pma_realloc (
	hawk_pma_t* pma,  /**< memory allocator */
	void*       blk,  /**< memory block */
	hawk_oow_t size  /**< new size in bytes */
);

/**
 * The hawk_pma_free() function is provided for completeness, and doesn't
 * free an individual block @a blk. 
 */
HAWK_EXPORT void hawk_pma_free (
	hawk_pma_t* pma, /**< memory allocator */
	void*       blk  /**< memory block */
);

#endif
