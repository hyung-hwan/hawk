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

#ifndef _HAWK_ARR_H_
#define _HAWK_ARR_H_

#include <hawk-cmn.h>

/** @file
 * This file provides a linear dynamic array. It grows dynamically as items
 * are added.
 */

enum hawk_arr_walk_t
{
	HAWK_ARR_WALK_STOP     = 0,
	HAWK_ARR_WALK_FORWARD  = 1,
	HAWK_ARR_WALK_BACKWARD = 2
};

typedef struct hawk_arr_t      hawk_arr_t;
typedef struct hawk_arr_slot_t hawk_arr_slot_t;
typedef enum   hawk_arr_walk_t hawk_arr_walk_t;

#define HAWK_ARR_COPIER_SIMPLE  ((hawk_arr_copier_t)1)
#define HAWK_ARR_COPIER_INLINE  ((hawk_arr_copier_t)2)

#define HAWK_ARR_COPIER_DEFAULT (HAWK_ARR_COPIER_SIMPLE)
#define HAWK_ARR_FREEER_DEFAULT (HAWK_NULL)
#define HAWK_ARR_COMPER_DEFAULT (hawk_arr_dflcomp)
#define HAWK_ARR_KEEPER_DEFAULT (HAWK_NULL)
#define HAWK_ARR_SIZER_DEFAULT  (HAWK_NULL)

#define HAWK_ARR_NIL ((hawk_oow_t)-1)

#define HAWK_ARR_SIZE(arr)        ((arr)->size)
#define HAWK_ARR_CAPA(arr)        ((arr)->capa)
#define HAWK_ARR_TALLY(arr)       ((arr)->tally)

#define HAWK_ARR_SLOT(arr,index)  ((arr)->slot[index])
#define HAWK_ARR_DPTL(arr,index)  ((const hawk_ptl_t*)&(arr)->slot[index]->val)
#define HAWK_ARR_DPTR(arr,index)  ((arr)->slot[index]->val.ptr)
#define HAWK_ARR_DLEN(arr,index)  ((arr)->slot[index]->val.len)

/**
 *  The hawk_arr_copier_t type defines a callback function for slot construction.
 *  A slot is contructed when a user adds data to an array. The user can
 *  define how the data to add can be maintained in the array. A dynamic
 *  array not specified with any copiers stores the data pointer and
 *  the data length into a slot. A special copier HAWK_ARR_COPIER_INLINE copies
 *  the contents of the data a user provided into the slot.
 *
 *  A copier should return the pointer to the copied data. If it fails to copy
 *  data, it may return HAWK_NULL. You need to set a proper freeer to free up
 *  memory allocated for copy.
 */
typedef void* (*hawk_arr_copier_t) (
	hawk_arr_t* arr    /**< array */,
	void*       dptr   /**< pointer to data to copy */,
	hawk_oow_t  dlen   /**< length of data to copy */
);

/**
 * The hawk_arr_freeer_t type defines a slot destruction callback.
 */
typedef void (*hawk_arr_freeer_t) (
	hawk_arr_t* arr    /**< array */,
	void*       dptr   /**< pointer to data to free */,
	hawk_oow_t  dlen   /**< length of data to free */
);

/**
 * The hawk_arr_comper_t type defines a key comparator that is called when
 * the arry needs to compare data. A linear dynamic array is created with a
 * default comparator that performs bitwise comparison.
 *
 * The default comparator compares data in a memcmp-like fashion.
 * It is not suitable when you want to implement a heap of numbers
 * greater than a byte size. You must implement a comparator that
 * takes the whole element and performs comparison in such a case.
 *
 * The comparator should return 0 if the data are the same, a negative
 * integer if the first data is less than the second data, a positive
 * integer otherwise.
 *
 */
typedef int (*hawk_arr_comper_t) (
	hawk_arr_t*  arr    /* array */,
	const void*  dptr1  /* data pointer */,
	hawk_oow_t   dlen1  /* data length */,
	const void*  dptr2  /* data pointer */,
	hawk_oow_t   dlen2  /* data length */
);

/**
 * The hawk_arr_keeper_t type defines a value keeper that is called when
 * a value is retained in the context that it should be destroyed because
 * it is identical to a new value. Two values are identical if their beginning
 * pointers and their lengths are equal.
 */
typedef void (*hawk_arr_keeper_t) (
	hawk_arr_t* arr     /**< array */,
	void*       vptr    /**< pointer to a value */,
	hawk_oow_t  vlen    /**< length of a value */
);

/**
 * The hawk_arr_sizer_t type defines an array size claculator that is called
 * when the array needs to be resized.
 */
typedef hawk_oow_t (*hawk_arr_sizer_t) (
	hawk_arr_t* arr,  /**< array */
	hawk_oow_t  hint  /**< sizing hint */
);


typedef struct hawk_arr_style_t hawk_arr_style_t;

struct hawk_arr_style_t
{
	hawk_arr_copier_t copier; /* data copier */
	hawk_arr_freeer_t freeer; /* data freeer */
	hawk_arr_comper_t comper; /* data comparator */
	hawk_arr_keeper_t keeper; /* data keeper */
	hawk_arr_sizer_t  sizer;  /* size calculator */
};

/**
 * The hawk_arr_style_kind_t type defines the type of predefined
 * callback set for pair manipulation.
 */
enum hawk_arr_style_kind_t
{
	/** store element pointers */
	HAWK_ARR_STYLE_DEFAULT,
	/** copy elements */
	HAWK_ARR_STYLE_INLINE_COPIER
};

typedef enum hawk_arr_style_kind_t  hawk_arr_style_kind_t;


typedef hawk_arr_walk_t (*hawk_arr_walker_t) (
	hawk_arr_t*      arr   /* array */,
	hawk_oow_t       index /* index to the visited slot */,
	void*            ctx   /* user-defined context */
);

/**
 * The hawk_arr_t type defines a linear dynamic array.
 */
struct hawk_arr_t
{
	hawk_gem_t*       gem;
	const hawk_arr_style_t* style;
	hawk_uint8_t      scale;  /* scale factor */
	hawk_oow_t        heap_pos_offset; /* offset in the data element where position
	                                   * is stored when heap operation is performed. */
	hawk_oow_t        size;   /* number of items */
	hawk_oow_t        tally;  /* number of items set */
	hawk_oow_t        capa;   /* capacity */
	hawk_arr_slot_t** slot;
};

/**
 * The hawk_arr_slot_t type defines a linear dynamic array slot
 */
struct hawk_arr_slot_t
{
	hawk_ptl_t val;
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_get_arr_style() functions returns a predefined callback set for
 * pair manipulation.
 */
HAWK_EXPORT const hawk_arr_style_t* hawk_get_arr_style (
	hawk_arr_style_kind_t kind
);

/**
 * The hawk_arr_open() function creates a linear dynamic array.
 */
HAWK_EXPORT hawk_arr_t* hawk_arr_open (
	hawk_gem_t* gem,
	hawk_oow_t  ext,  /**< extension size in bytes */
	hawk_oow_t  capa  /**< initial array capacity */
);

/**
 * The hawk_arr_close() function destroys a linear dynamic array.
 */
HAWK_EXPORT void hawk_arr_close (
	hawk_arr_t* arr /**< array */
);

/**
 * The hawk_arr_init() function initializes a linear dynamic array.
 */
HAWK_EXPORT int hawk_arr_init (
	hawk_arr_t*  arr,
	hawk_gem_t*  gem,
	hawk_oow_t   capa
);

/**
 * The hawk_arr_fini() function finalizes a linear dynamic array.
 */
HAWK_EXPORT void hawk_arr_fini (
	hawk_arr_t* arr /**< array */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_arr_getxtn (hawk_arr_t* arr) { return (void*)(arr + 1); }
#else
#define hawk_arr_getxtn(arr) ((void*)((hawk_arr_t*)(arr) + 1))
#endif

/**
 * The hawk_arr_getscale() function returns the scale factor
 */
HAWK_EXPORT int hawk_arr_getscale (
	hawk_arr_t* arr   /**< array */
);

/**
 * The hawk_arr_setscale() function sets the scale factor of the length
 * of a key and a value. A scale factor determines the actual length of
 * a key and a value in bytes. A arr is created with a scale factor of 1.
 * The scale factor should be larger than 0 and less than 256.
 * It is a bad idea to change the scale factor when @a arr is not empty.
 */
HAWK_EXPORT void hawk_arr_setscale (
	hawk_arr_t* arr   /**< array */,
	int scale        /**< scale factor - 1 to 255 */
);

HAWK_EXPORT hawk_arr_copier_t hawk_arr_getcopier (
	hawk_arr_t* arr   /* array */
);


HAWK_EXPORT void hawk_arr_setstyle (
	hawk_arr_t*             arr,
	const hawk_arr_style_t* style
);

HAWK_EXPORT const hawk_arr_style_t* hawk_arr_getstyle (
	hawk_arr_t* arr
);

HAWK_EXPORT hawk_oow_t hawk_arr_getsize (
	hawk_arr_t* arr
);

HAWK_EXPORT hawk_oow_t hawk_arr_getcapa (
	hawk_arr_t* arr
);

HAWK_EXPORT hawk_arr_t* hawk_arr_setcapa (
	hawk_arr_t* arr,
	hawk_oow_t capa
);

HAWK_EXPORT hawk_oow_t hawk_arr_search (
	hawk_arr_t*  arr,
	hawk_oow_t  pos,
	const void* dptr,
	hawk_oow_t  dlen
);

HAWK_EXPORT hawk_oow_t hawk_arr_rsearch (
	hawk_arr_t*  arr,
	hawk_oow_t  pos,
	const void* dptr,
	hawk_oow_t  dlen
);

HAWK_EXPORT hawk_oow_t hawk_arr_upsert (
	hawk_arr_t* arr,
	hawk_oow_t index,
	void*      dptr,
	hawk_oow_t dlen
);

HAWK_EXPORT hawk_oow_t hawk_arr_insert (
	hawk_arr_t* arr,
	hawk_oow_t index,
	void*      dptr,
	hawk_oow_t dlen
);

HAWK_EXPORT hawk_oow_t hawk_arr_update (
	hawk_arr_t* arr,
	hawk_oow_t pos,
	void*      dptr,
	hawk_oow_t dlen
);

/**
 * The hawk_arr_delete() function deletes the as many data as the count
 * from the index. It returns the number of data deleted.
 */
HAWK_EXPORT hawk_oow_t hawk_arr_delete (
	hawk_arr_t* arr,
	hawk_oow_t  index,
	hawk_oow_t  count
);

/**
 *  The hawk_arr_uplete() function deletes data slot without compaction.
 *  It returns the number of data affected.
 */
HAWK_EXPORT hawk_oow_t hawk_arr_uplete (
	hawk_arr_t* arr,
	hawk_oow_t  index,
	hawk_oow_t  count
);

HAWK_EXPORT void hawk_arr_clear (
	hawk_arr_t* arr
);

/**
 * The hawk_arr_walk() function calls the @a walker function for each
 * element in the array beginning from the first. The @a walker function
 * should return one of #HAWK_ARR_WALK_FORWARD, #HAWK_ARR_WALK_BACKWARD,
 * #HAWK_ARR_WALK_STOP.
 * @return number of calls to the @a walker fucntion made
 */
HAWK_EXPORT hawk_oow_t hawk_arr_walk (
	hawk_arr_t*       arr,
	hawk_arr_walker_t walker,
	void*            ctx
);

/**
 * The hawk_arr_rwalk() function calls the @a walker function for each
 * element in the array beginning from the last. The @a walker function
 * should return one of #HAWK_ARR_WALK_FORWARD, #HAWK_ARR_WALK_BACKWARD,
 * #HAWK_ARR_WALK_STOP.
 * @return number of calls to the @a walker fucntion made
 */
HAWK_EXPORT hawk_oow_t hawk_arr_rwalk (
	hawk_arr_t*       arr,
	hawk_arr_walker_t walker,
	void*            ctx
);

/**
 * The hawk_arr_pushstack() function appends data to the array. It is a utility
 * function to allow stack-like operations over an array. To do so, you should
 * not play with other non-stack related functions.
 */
HAWK_EXPORT hawk_oow_t hawk_arr_pushstack (
	hawk_arr_t* arr,
	void*      dptr,
	hawk_oow_t dlen
);

/**
 * The hawk_arr_popstack() function deletes the last array data. It is a utility
 * function to allow stack-like operations over an array. To do so, you should
 * not play with other non-stack related functions.
 * @note You must not call this function if @a arr is empty.
 */
HAWK_EXPORT void hawk_arr_popstack (
	hawk_arr_t* arr
);

/**
 * The hawk_arr_pushheap() function inserts data to an array while keeping the
 * largest data at position 0. It is a utiltiy funtion to implement a binary
 * max-heap over an array. Inverse the comparator to implement a min-heap.
 * @return number of array elements
 * @note You must not mess up the array with other non-heap related functions
 *       to keep the heap property.
 */
HAWK_EXPORT hawk_oow_t hawk_arr_pushheap (
	hawk_arr_t* arr,
	void*      dptr,
	hawk_oow_t dlen
);

/**
 * The hawk_arr_popheap() function deletes data at position 0 while keeping
 * the largest data at positon 0. It is a utiltiy funtion to implement a binary
 * max-heap over an array.
 * @note You must not mess up the array with other non-heap related functions
 *       to keep the heap property.
 */
HAWK_EXPORT void hawk_arr_popheap (
	hawk_arr_t* arr
);

HAWK_EXPORT void hawk_arr_deleteheap (
	hawk_arr_t* arr,
	hawk_oow_t index
);

HAWK_EXPORT hawk_oow_t hawk_arr_updateheap (
	hawk_arr_t* arr,
	hawk_oow_t index,
	void*      dptr,
	hawk_oow_t dlen
);

HAWK_EXPORT hawk_oow_t hawk_arr_getheapposoffset (
	hawk_arr_t* arr
);

/**
 * The hawk_arr_setheapposoffset() function sets the offset to a position holding
 * field within a data element. It assumes that the field is of the hawk_oow_t type.
 *
 * \code
 * struct data_t
 * {
 *    int v;
 *    hawk_oow_t pos;
 * };
 * struct data_t d;
 * hawk_arr_setheapposoffset (arr, HAWK_OFFSETOF(struct data_t, pos));
 * d.v = 20;
 * hawk_arr_pushheap (arr, &d, 1);
 * \endcode
 *
 * In the code above, the 'pos' field of the first element in the array must be 0.
 *
 * If it's set to HAWK_ARR_NIL, position is not updated when heapification is
 * performed.
 */
HAWK_EXPORT void hawk_arr_setheapposoffset (
	hawk_arr_t* arr,
	hawk_oow_t offset
);



/**
 * The hawk_arr_dflcomp() function is the default comparator
 */
HAWK_EXPORT int hawk_arr_dflcomp (
	hawk_arr_t* arr,
	const void* dptr1,
	hawk_oow_t  dlen1,
	const void* dptr2,
	hawk_oow_t dlen2
);

#if defined(__cplusplus)
}
#endif

#endif
