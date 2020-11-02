/*
 * $Id$
 *
    Copyright (c) 2006-20202 Chung, Hyung-Hwan. All rights reserved.

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

#ifndef _HAWK_XMA_H_
#define _HAWK_XMA_H_

/** @file
 * This file defines an extravagant memory allocator. Why? It may be so.
 * The memory allocator allows you to maintain memory blocks from a
 * larger memory chunk allocated with an outer memory allocator.
 * Typically, an outer memory allocator is a standard memory allocator
 * like malloc(). You can isolate memory blocks into a particular chunk.
 *
 * See the example below. Note it omits error handling.
 *
 * @code
 * #include <hawk-xma.h>
 * #include <stdio.h>
 * #include <stdarg.h>
 * void dumper (void* ctx, const char* fmt, ...)
 * {
 * 	va_list ap;
 * 	va_start (ap, fmt);
 * 	vfprintf (fmt, ap);
 * 	va_end (ap);
 * }
 * int main ()
 * {
 *   hawk_xma_t* xma;
 *   void* ptr1, * ptr2;
 *
 *   // create a new memory allocator obtaining a 100K byte zone 
 *   // with the default memory allocator
 *   xma = hawk_xma_open(HAWK_NULL, 0, 100000L); 
 *
 *   ptr1 = hawk_xma_alloc(xma, 5000); // allocate a 5K block from the zone
 *   ptr2 = hawk_xma_alloc(xma, 1000); // allocate a 1K block from the zone
 *   ptr1 = hawk_xma_realloc(xma, ptr1, 6000); // resize the 5K block to 6K.
 *
 *   hawk_xma_dump (xma, dumper, HAWK_NULL); // dump memory blocks 
 *
 *   // the following two lines are not actually needed as the allocator
 *   // is closed after them.
 *   hawk_xma_free (xma, ptr2); // dispose of the 1K block
 *   hawk_xma_free (xma, ptr1); // dispose of the 6K block
 *
 *   hawk_xma_close (xma); //  destroy the memory allocator
 *   return 0;
 * }
 * @endcode
 */
#include <hawk-cmn.h>

#if defined(HAWK_BUILD_DEBUG)
#	define HAWK_XMA_ENABLE_STAT
#endif

/** @struct hawk_xma_t
 * The hawk_xma_t type defines a simple memory allocator over a memory zone.
 * It can obtain a relatively large zone of memory and manage it.
 */
typedef struct hawk_xma_t hawk_xma_t;

/**
 * The hawk_xma_fblk_t type defines a memory block allocated.
 */
typedef struct hawk_xma_fblk_t hawk_xma_fblk_t;
typedef struct hawk_xma_mblk_t hawk_xma_mblk_t;

#define HAWK_XMA_FIXED 32
#define HAWK_XMA_SIZE_BITS ((HAWK_SIZEOF_OOW_T*8)-1)

struct hawk_xma_t
{
	hawk_mmgr_t* _mmgr;

	hawk_uint8_t* start; /* zone beginning */
	hawk_uint8_t* end; /* zone end */
	int           internal;

	/** pointer array to free memory blocks */
	hawk_xma_fblk_t* xfree[HAWK_XMA_FIXED + HAWK_XMA_SIZE_BITS + 1]; 

	/** pre-computed value for fast xfree index calculation */
	hawk_oow_t     bdec;

#if defined(HAWK_XMA_ENABLE_STAT)
	struct
	{
		hawk_oow_t total;
		hawk_oow_t alloc;
		hawk_oow_t avail;
		hawk_oow_t nused;
		hawk_oow_t nfree;
	} stat;
#endif
};

/**
 * The hawk_xma_dumper_t type defines a printf-like output function
 * for hawk_xma_dump().
 */
typedef int (*hawk_xma_dumper_t) (
	void*            ctx,
	const hawk_bch_t* fmt,
	...
);

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_xma_open() function creates a memory allocator. It obtains a memory
 * zone of the @a zonesize bytes with the memory manager @a mmgr. It also makes
 * available the extension area of the @a xtnsize bytes that you can get the
 * pointer to with hawk_xma_getxtn().
 *
 * @return pointer to a memory allocator on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_xma_t* hawk_xma_open (
	hawk_mmgr_t* mmgr,    /**< memory manager */
	hawk_oow_t   xtnsize, /**< extension size in bytes */
	void*        zoneptr,
	hawk_oow_t   zonesize /**< zone size in bytes */
);

/**
 * The hawk_xma_close() function destroys a memory allocator. It also frees
 * the memory zone obtained, which invalidates the memory blocks within 
 * the zone. Call this function to destroy a memory allocator created with
 * hawk_xma_open().
 */
HAWK_EXPORT void hawk_xma_close (
	hawk_xma_t* xma /**< memory allocator */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_mmgr_t* hawk_xma_getmmgr (hawk_xma_t* xma) { return xma->_mmgr; }
#else
#	define hawk_xma_getmmgr(xma) (((hawk_xma_t*)(xma))->_mmgr)
#endif

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_xma_getxtn (hawk_xma_t* xma) { return (void*)(xma + 1); }
#else
#define hawk_xma_getxtn(xma) ((void*)((hawk_xma_t*)(xma) + 1))
#endif

/**
 * The hawk_xma_init() initializes a memory allocator. If you have the hawk_xma_t
 * structure statically declared or already allocated, you may pass the pointer
 * to this function instead of calling hawk_xma_open(). It obtains a memory zone
 * of @a zonesize bytes with the memory manager @a mmgr. Unlike hawk_xma_open(),
 * it does not accept the extension size, thus not creating an extention area.
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_xma_init (
	hawk_xma_t*  xma,     /**< memory allocator */
	hawk_mmgr_t* mmgr,    /**< memory manager */
	void*       zoneptr,  /**< pointer to memory zone. if #HAWK_NULL, a zone is auto-created */
	hawk_oow_t   zonesize /**< zone size in bytes */
);

/**
 * The hawk_xma_fini() function finalizes a memory allocator. Call this 
 * function to finalize a memory allocator initialized with hawk_xma_init().
 */
HAWK_EXPORT void hawk_xma_fini (
	hawk_xma_t* xma /**< memory allocator */
);

/**
 * The hawk_xma_alloc() function allocates @a size bytes.
 * @return pointer to a memory block on success, #HAWK_NULL on failure
 */
HAWK_EXPORT void* hawk_xma_alloc (
	hawk_xma_t* xma, /**< memory allocator */
	hawk_oow_t  size /**< size in bytes */
);

HAWK_EXPORT void* hawk_xma_calloc (
	hawk_xma_t* xma,
	hawk_oow_t  size
);

/**
 * The hawk_xma_alloc() function resizes the memory block @a b to @a size bytes.
 * @return pointer to a resized memory block on success, #HAWK_NULL on failure
 */
HAWK_EXPORT void* hawk_xma_realloc (
	hawk_xma_t* xma,  /**< memory allocator */
	void*      b,    /**< memory block */
	hawk_oow_t  size  /**< new size in bytes */
);

/**
 * The hawk_xma_alloc() function frees the memory block @a b.
 */
HAWK_EXPORT void hawk_xma_free (
	hawk_xma_t* xma, /**< memory allocator */
	void*      b    /**< memory block */
);

/**
 * The hawk_xma_dump() function dumps the contents of the memory zone
 * with the output function @a dumper provided. The debug build shows
 * more statistical counters.
 */
HAWK_EXPORT void hawk_xma_dump (
	hawk_xma_t*       xma,    /**< memory allocator */
	hawk_xma_dumper_t dumper, /**< output function */
	void*            ctx     /**< first parameter to output function */
);

#if defined(__cplusplus)
}
#endif

#endif
