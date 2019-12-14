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

#ifndef _HAWK_RBT_H_
#define _HAWK_RBT_H_

#include <hawk-cmn.h>

/** \file
 * This file provides a red-black tree encapsulated in the #hawk_rbt_t type that
 * implements a self-balancing binary search tree.Its interface is very close 
 * to #hawk_htb_t.
 *
 * This sample code adds a series of keys and values and print them
 * in descending key order.
 * \code
 * #include <hawk-rbt.h>
 * 
 * static hawk_rbt_walk_t walk (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, void* ctx)
 * {
 *   hawk_printf (HAWK_T("key = %d, value = %d\n"),
 *     *(int*)HAWK_RBT_KPTR(pair), *(int*)HAWK_RBT_VPTR(pair));
 *   return HAWK_RBT_WALK_FORWARD;
 * }
 * 
 * int main ()
 * {
 *   hawk_rbt_t* s1;
 *   int i;
 * 
 *   s1 = hawk_rbt_open (HAWK_MMGR_GETDFL(), 0, 1, 1); // error handling skipped
 *   hawk_rbt_setstyle (s1, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));
 * 
 *   for (i = 0; i < 20; i++)
 *   {
 *     int x = i * 20;
 *     hawk_rbt_insert (s1, &i, HAWK_SIZEOF(i), &x, HAWK_SIZEOF(x)); // eror handling skipped
 *   }
 * 
 *   hawk_rbt_rwalk (s1, walk, HAWK_NULL);
 * 
 *   hawk_rbt_close (s1);
 *   return 0;
 * }
 * \endcode
 */

typedef struct hawk_rbt_t hawk_rbt_t;
typedef struct hawk_rbt_pair_t hawk_rbt_pair_t;

/** 
 * The hawk_rbt_walk_t type defines values that the callback function can
 * return to control hawk_rbt_walk() and hawk_rbt_rwalk().
 */
enum hawk_rbt_walk_t
{
	HAWK_RBT_WALK_STOP    = 0,
	HAWK_RBT_WALK_FORWARD = 1
};
typedef enum hawk_rbt_walk_t hawk_rbt_walk_t;

/**
 * The hawk_rbt_id_t type defines IDs to indicate a key or a value in various
 * functions
 */
enum hawk_rbt_id_t
{
	HAWK_RBT_KEY = 0, /**< indicate a key */
	HAWK_RBT_VAL = 1  /**< indicate a value */
};
typedef enum hawk_rbt_id_t hawk_rbt_id_t;

/**
 * The hawk_rbt_copier_t type defines a pair contruction callback.
 */
typedef void* (*hawk_rbt_copier_t) (
	hawk_rbt_t* rbt  /**< red-black tree */,
	void*      dptr /**< pointer to a key or a value */, 
	hawk_oow_t  dlen /**< length of a key or a value */
);

/**
 * The hawk_rbt_freeer_t defines a key/value destruction callback.
 */
typedef void (*hawk_rbt_freeer_t) (
	hawk_rbt_t* rbt,  /**< red-black tree */
	void*      dptr, /**< pointer to a key or a value */
	hawk_oow_t  dlen  /**< length of a key or a value */
);

/**
 * The hawk_rbt_comper_t type defines a key comparator that is called when
 * the rbt needs to compare keys. A red-black tree is created with a default
 * comparator which performs bitwise comparison of two keys.
 * The comparator should return 0 if the keys are the same, 1 if the first
 * key is greater than the second key, -1 otherwise.
 */
typedef int (*hawk_rbt_comper_t) (
	const hawk_rbt_t* rbt,    /**< red-black tree */ 
	const void*      kptr1,  /**< key pointer */
	hawk_oow_t        klen1,  /**< key length */ 
	const void*      kptr2,  /**< key pointer */
	hawk_oow_t        klen2   /**< key length */
);

/**
 * The hawk_rbt_keeper_t type defines a value keeper that is called when 
 * a value is retained in the context that it should be destroyed because
 * it is identical to a new value. Two values are identical if their 
 * pointers and lengths are equal.
 */
typedef void (*hawk_rbt_keeper_t) (
	hawk_rbt_t* rbt,    /**< red-black tree */
	void*       vptr,   /**< value pointer */
	hawk_oow_t vlen    /**< value length */
);

/**
 * The hawk_rbt_walker_t defines a pair visitor.
 */
typedef hawk_rbt_walk_t (*hawk_rbt_walker_t) (
	hawk_rbt_t*      rbt,   /**< red-black tree */
	hawk_rbt_pair_t* pair,  /**< pointer to a key/value pair */
	void*            ctx    /**< pointer to user-defined data */
);

/**
 * The hawk_rbt_cbserter_t type defines a callback function for hawk_rbt_cbsert().
 * The hawk_rbt_cbserter() function calls it to allocate a new pair for the 
 * key pointed to by \a kptr of the length \a klen and the callback context
 * \a ctx. The second parameter \a pair is passed the pointer to the existing
 * pair for the key or #HAWK_NULL in case of no existing key. The callback
 * must return a pointer to a new or a reallocated pair. When reallocating the
 * existing pair, this callback must destroy the existing pair and return the 
 * newly reallocated pair. It must return #HAWK_NULL for failure.
 */
typedef hawk_rbt_pair_t* (*hawk_rbt_cbserter_t) (
	hawk_rbt_t*      rbt,    /**< red-black tree */
	hawk_rbt_pair_t* pair,   /**< pair pointer */
	void*           kptr,   /**< key pointer */
	hawk_oow_t       klen,   /**< key length */
	void*           ctx     /**< callback context */
);

enum hawk_rbt_pair_color_t
{
	HAWK_RBT_RED,
	HAWK_RBT_BLACK
};
typedef enum hawk_rbt_pair_color_t hawk_rbt_pair_color_t;

/**
 * The hawk_rbt_pair_t type defines red-black tree pair. A pair is composed 
 * of a key and a value. It maintains pointers to the beginning of a key and 
 * a value plus their length. The length is scaled down with the scale factor 
 * specified in an owning tree. Use macros defined in the 
 */
struct hawk_rbt_pair_t
{
	struct
	{
		void*     ptr;
		hawk_oow_t len;
	} key;

	struct
	{
		void*       ptr;
		hawk_oow_t len;
	} val;

	/* management information below */
	hawk_rbt_pair_color_t color;
	hawk_rbt_pair_t* parent;
	hawk_rbt_pair_t* child[2]; /* left and right */
};

typedef struct hawk_rbt_style_t hawk_rbt_style_t;

/**
 * The hawk_rbt_style_t type defines callback function sets for key/value 
 * pair manipulation. 
 */
struct hawk_rbt_style_t
{
	hawk_rbt_copier_t copier[2]; /**< key and value copier */
	hawk_rbt_freeer_t freeer[2]; /**< key and value freeer */
	hawk_rbt_comper_t comper;    /**< key comparator */
	hawk_rbt_keeper_t keeper;    /**< value keeper */
};

/**
 * The hawk_rbt_style_kind_t type defines the type of predefined
 * callback set for pair manipulation.
 */
enum hawk_rbt_style_kind_t
{
	/** store the key and the value pointer */
	HAWK_RBT_STYLE_DEFAULT,
	/** copy both key and value into the pair */
	HAWK_RBT_STYLE_INLINE_COPIERS,
	/** copy the key into the pair but store the value pointer */
	HAWK_RBT_STYLE_INLINE_KEY_COPIER,
	/** copy the value into the pair but store the key pointer */
	HAWK_RBT_STYLE_INLINE_VALUE_COPIER
};

typedef enum hawk_rbt_style_kind_t  hawk_rbt_style_kind_t;

/**
 * The hawk_rbt_t type defines a red-black tree.
 */
struct hawk_rbt_t
{
	hawk_t*                 hawk;
	const hawk_rbt_style_t* style;
	hawk_oob_t              scale[2];  /**< length scale */
	hawk_rbt_pair_t         xnil;      /**< internal nil node */
	hawk_oow_t              size;      /**< number of pairs */
	hawk_rbt_pair_t*        root;      /**< root pair */
};

/**
 * The HAWK_RBT_COPIER_SIMPLE macros defines a copier that remembers the
 * pointer and length of data in a pair.
 */
#define HAWK_RBT_COPIER_SIMPLE ((hawk_rbt_copier_t)1)

/**
 * The HAWK_RBT_COPIER_INLINE macros defines a copier that copies data into
 * a pair.
 */
#define HAWK_RBT_COPIER_INLINE ((hawk_rbt_copier_t)2)

#define HAWK_RBT_COPIER_DEFAULT (HAWK_RBT_COPIER_SIMPLE)
#define HAWK_RBT_FREEER_DEFAULT (HAWK_NULL)
#define HAWK_RBT_COMPER_DEFAULT (hawk_rbt_dflcomp)
#define HAWK_RBT_KEEPER_DEFAULT (HAWK_NULL)

/**
 * The HAWK_RBT_SIZE() macro returns the number of pairs in red-black tree.
 */
#define HAWK_RBT_SIZE(m)   ((const hawk_oow_t)(m)->size)
#define HAWK_RBT_KSCALE(m) ((const int)(m)->scale[HAWK_RBT_KEY])
#define HAWK_RBT_VSCALE(m) ((const int)(m)->scale[HAWK_RBT_VAL])

#define HAWK_RBT_KPTL(p) (&(p)->key)
#define HAWK_RBT_VPTL(p) (&(p)->val)

#define HAWK_RBT_KPTR(p) ((p)->key.ptr)
#define HAWK_RBT_KLEN(p) ((p)->key.len)
#define HAWK_RBT_VPTR(p) ((p)->val.ptr)
#define HAWK_RBT_VLEN(p) ((p)->val.len)

#define HAWK_RBT_NEXT(p) ((p)->next)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_get_rbt_style() functions returns a predefined callback set for
 * pair manipulation.
 */
HAWK_EXPORT const hawk_rbt_style_t* hawk_get_rbt_style (
	hawk_rbt_style_kind_t kind
);

/**
 * The hawk_rbt_open() function creates a red-black tree.
 * \return hawk_rbt_t pointer on success, HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_rbt_t* hawk_rbt_open (
	hawk_t*      hawk,
	hawk_oow_t   xtnsize, /**< extension size in bytes */
	int          kscale,  /**< key scale */
	int          vscale   /**< value scale */
);

/**
 * The hawk_rbt_close() function destroys a red-black tree.
 */
HAWK_EXPORT void hawk_rbt_close (
	hawk_rbt_t* rbt   /**< red-black tree */
);

/**
 * The hawk_rbt_init() function initializes a red-black tree
 */
HAWK_EXPORT int hawk_rbt_init (
	hawk_rbt_t*  rbt,    /**< red-black tree */
	hawk_t*      hawk,
	int          kscale, /**< key scale */
	int          vscale  /**< value scale */
);

/**
 * The hawk_rbt_fini() funtion finalizes a red-black tree
 */
HAWK_EXPORT void hawk_rbt_fini (
	hawk_rbt_t* rbt  /**< red-black tree */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_rbt_getxtn (hawk_rbt_t* rbt) { return (void*)(rbt + 1); }
#else
#define hawk_rbt_getxtn(awk) ((void*)((hawk_rbt_t*)(rbt) + 1))
#endif

/**
 * The hawk_rbt_getstyle() function gets manipulation callback function set.
 */
HAWK_EXPORT const hawk_rbt_style_t* hawk_rbt_getstyle (
	const hawk_rbt_t* rbt /**< red-black tree */
);

/**
 * The hawk_rbt_setstyle() function sets internal manipulation callback 
 * functions for data construction, destruction, comparison, etc.
 * The callback structure pointed to by \a style must outlive the tree
 * pointed to by \a htb as the tree doesn't copy the contents of the 
 * structure.
 */
HAWK_EXPORT void hawk_rbt_setstyle (
	hawk_rbt_t*             rbt,  /**< red-black tree */
	const hawk_rbt_style_t* style /**< callback function set */
);

/**
 * The hawk_rbt_getsize() function gets the number of pairs in red-black tree.
 */
HAWK_EXPORT hawk_oow_t hawk_rbt_getsize (
	const hawk_rbt_t* rbt  /**< red-black tree */
);

/**
 * The hawk_rbt_search() function searches red-black tree to find a pair with a 
 * matching key. It returns the pointer to the pair found. If it fails
 * to find one, it returns HAWK_NULL.
 * \return pointer to the pair with a maching key, 
 *         or HAWK_NULL if no match is found.
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_search (
	const hawk_rbt_t* rbt,   /**< red-black tree */
	const void*      kptr,  /**< key pointer */
	hawk_oow_t        klen   /**< the size of the key */
);

/**
 * The hawk_rbt_upsert() function searches red-black tree for the pair with a 
 * matching key. If one is found, it updates the pair. Otherwise, it inserts
 * a new pair with the key and the value given. It returns the pointer to the 
 * pair updated or inserted.
 * \return a pointer to the updated or inserted pair on success, 
 *         HAWK_NULL on failure. 
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_upsert (
	hawk_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hawk_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t  vlen   /**< value length */
);

/**
 * The hawk_rbt_ensert() function inserts a new pair with the key and the value
 * given. If there exists a pair with the key given, the function returns 
 * the pair containing the key.
 * \return pointer to a pair on success, HAWK_NULL on failure. 
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_ensert (
	hawk_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hawk_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t  vlen   /**< value length */
);

/**
 * The hawk_rbt_insert() function inserts a new pair with the key and the value
 * given. If there exists a pair with the key given, the function returns 
 * HAWK_NULL without channging the value.
 * \return pointer to the pair created on success, HAWK_NULL on failure. 
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_insert (
	hawk_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hawk_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t  vlen   /**< value length */
);

/**
 * The hawk_rbt_update() function updates the value of an existing pair
 * with a matching key.
 * \return pointer to the pair on success, HAWK_NULL on no matching pair
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_update (
	hawk_rbt_t* rbt,   /**< red-black tree */
	void*      kptr,  /**< key pointer */
	hawk_oow_t  klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t  vlen   /**< value length */
);

/**
 * The hawk_rbt_cbsert() function inserts a key/value pair by delegating pair 
 * allocation to a callback function. Depending on the callback function,
 * it may behave like hawk_rbt_insert(), hawk_rbt_upsert(), hawk_rbt_update(),
 * hawk_rbt_ensert(), or totally differently. The sample code below inserts
 * a new pair if the key is not found and appends the new value to the
 * existing value delimited by a comma if the key is found.
 *
 * \code
 * hawk_rbt_walk_t print_map_pair (hawk_rbt_t* map, hawk_rbt_pair_t* pair, void* ctx)
 * {
 *   hawk_printf (HAWK_T("%.*s[%d] => %.*s[%d]\n"),
 *     HAWK_RBT_KLEN(pair), HAWK_RBT_KPTR(pair), (int)HAWK_RBT_KLEN(pair),
 *     HAWK_RBT_VLEN(pair), HAWK_RBT_VPTR(pair), (int)HAWK_RBT_VLEN(pair));
 *   return HAWK_RBT_WALK_FORWARD;
 * }
 * 
 * hawk_rbt_pair_t* cbserter (
 *   hawk_rbt_t* rbt, hawk_rbt_pair_t* pair,
 *   void* kptr, hawk_oow_t klen, void* ctx)
 * {
 *   hawk_cstr_t* v = (hawk_cstr_t*)ctx;
 *   if (pair == HAWK_NULL)
 *   {
 *     // no existing key for the key 
 *     return hawk_rbt_allocpair (rbt, kptr, klen, v->ptr, v->len);
 *   }
 *   else
 *   {
 *     // a pair with the key exists. 
 *     // in this sample, i will append the new value to the old value 
 *     // separated by a comma
 *     hawk_rbt_pair_t* new_pair;
 *     hawk_ooch_t comma = HAWK_T(',');
 *     hawk_oob_t* vptr;
 * 
 *     // allocate a new pair, but without filling the actual value. 
 *     // note vptr is given HAWK_NULL for that purpose 
 *     new_pair = hawk_rbt_allocpair (
 *       rbt, kptr, klen, HAWK_NULL, pair->vlen + 1 + v->len); 
 *     if (new_pair == HAWK_NULL) return HAWK_NULL;
 * 
 *     // fill in the value space 
 *     vptr = new_pair->vptr;
 *     hawk_memcpy (vptr, pair->vptr, pair->vlen*HAWK_SIZEOF(hawk_ooch_t));
 *     vptr += pair->vlen*HAWK_SIZEOF(hawk_ooch_t);
 *     hawk_memcpy (vptr, &comma, HAWK_SIZEOF(hawk_ooch_t));
 *     vptr += HAWK_SIZEOF(hawk_ooch_t);
 *     hawk_memcpy (vptr, v->ptr, v->len*HAWK_SIZEOF(hawk_ooch_t));
 * 
 *     // this callback requires the old pair to be destroyed 
 *     hawk_rbt_freepair (rbt, pair);
 * 
 *     // return the new pair 
 *     return new_pair;
 *   }
 * }
 * 
 * int main ()
 * {
 *   hawk_rbt_t* s1;
 *   int i;
 *   hawk_ooch_t* keys[] = { HAWK_T("one"), HAWK_T("two"), HAWK_T("three") };
 *   hawk_ooch_t* vals[] = { HAWK_T("1"), HAWK_T("2"), HAWK_T("3"), HAWK_T("4"), HAWK_T("5") };
 * 
 *   s1 = hawk_rbt_open (
 *     HAWK_MMGR_GETDFL(), 0,
 *     HAWK_SIZEOF(hawk_ooch_t), HAWK_SIZEOF(hawk_ooch_t)
 *   ); // note error check is skipped 
 *   hawk_rbt_setstyle (s1, &style1);
 * 
 *   for (i = 0; i < HAWK_COUNTOF(vals); i++)
 *   {
 *     hawk_cstr_t ctx;
 *     ctx.ptr = vals[i]; ctx.len = hawk_strlen(vals[i]);
 *     hawk_rbt_cbsert (s1,
 *       keys[i%HAWK_COUNTOF(keys)], hawk_strlen(keys[i%HAWK_COUNTOF(keys)]),
 *       cbserter, &ctx
 *     ); // note error check is skipped
 *   }
 *   hawk_rbt_walk (s1, print_map_pair, HAWK_NULL);
 * 
 *   hawk_rbt_close (s1);
 *   return 0;
 * }
 * \endcode
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_cbsert (
	hawk_rbt_t*         rbt,      /**< red-black tree */
	void*              kptr,     /**< key pointer */
	hawk_oow_t          klen,     /**< key length */
	hawk_rbt_cbserter_t cbserter, /**< callback function */
	void*              ctx       /**< callback context */
);

/**
 * The hawk_rbt_delete() function deletes a pair with a matching key 
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_rbt_delete (
	hawk_rbt_t*  rbt,   /**< red-black tree */
	const void* kptr,  /**< key pointer */
	hawk_oow_t   klen   /**< key size */
);

/**
 * The hawk_rbt_clear() function empties a red-black tree.
 */
HAWK_EXPORT void hawk_rbt_clear (
	hawk_rbt_t* rbt /**< red-black tree */
);

/**
 * The hawk_rbt_walk() function traverses a red-black tree in preorder 
 * from the leftmost child.
 */
HAWK_EXPORT void hawk_rbt_walk (
	hawk_rbt_t*       rbt,    /**< red-black tree */
	hawk_rbt_walker_t walker, /**< callback function for each pair */
	void*            ctx     /**< pointer to user-specific data */
);

/**
 * The hawk_rbt_walk() function traverses a red-black tree in preorder 
 * from the rightmost child.
 */
HAWK_EXPORT void hawk_rbt_rwalk (
	hawk_rbt_t*       rbt,    /**< red-black tree */
	hawk_rbt_walker_t walker, /**< callback function for each pair */
	void*            ctx     /**< pointer to user-specific data */
);

/**
 * The hawk_rbt_allocpair() function allocates a pair for a key and a value 
 * given. But it does not chain the pair allocated into the red-black tree \a rbt.
 * Use this function at your own risk. 
 *
 * Take note of he following special behavior when the copier is 
 * #HAWK_RBT_COPIER_INLINE.
 * - If \a kptr is #HAWK_NULL, the key space of the size \a klen is reserved but
 *   not propagated with any data.
 * - If \a vptr is #HAWK_NULL, the value space of the size \a vlen is reserved
 *   but not propagated with any data.
 */
HAWK_EXPORT hawk_rbt_pair_t* hawk_rbt_allocpair (
	hawk_rbt_t*  rbt,
	void*       kptr, 
	hawk_oow_t   klen,
	void*       vptr,
	hawk_oow_t   vlen
);

/**
 * The hawk_rbt_freepair() function destroys a pair. But it does not detach
 * the pair destroyed from the red-black tree \a rbt. Use this function at your
 * own risk.
 */
HAWK_EXPORT void hawk_rbt_freepair (
	hawk_rbt_t*      rbt,
	hawk_rbt_pair_t* pair
);

/**
 * The hawk_rbt_dflcomp() function defines the default key comparator.
 */
HAWK_EXPORT int hawk_rbt_dflcomp (
	const hawk_rbt_t* rbt,
	const void*      kptr1,
	hawk_oow_t        klen1,
	const void*      kptr2,
	hawk_oow_t        klen2 
);

#if defined(__cplusplus)
}
#endif

#endif
