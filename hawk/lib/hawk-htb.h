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

#ifndef _HAWK_HTB_H_
#define _HAWK_HTB_H_

#include <hawk-cmn.h>

/**@file
 * This file provides a hash table encapsulated in the #hawk_htb_t type that 
 * maintains buckets for key/value pairs with the same key hash chained under
 * the same bucket. Its interface is very close to #hawk_rbt_t.
 *
 * This sample code adds a series of keys and values and print them
 * in the randome order.
 * @code
 * #include <hawk-htb.h>
 * 
 * static hawk_htb_walk_t walk (hawk_htb_t* htb, hawk_htb_pair_t* pair, void* ctx)
 * {
 *   hawk_printf (HAWK_T("key = %d, value = %d\n"),
 *     *(int*)HAWK_HTB_KPTR(pair), *(int*)HAWK_HTB_VPTR(pair));
 *   return HAWK_HTB_WALK_FORWARD;
 * }
 * 
 * int main ()
 * {
 *   hawk_htb_t* s1;
 *   int i;
 * 
 *   hawk_open_stdsios ();
 *   s1 = hawk_htb_open (HAWK_MMGR_GETDFL(), 0, 30, 75, 1, 1); // error handling skipped
 *   hawk_htb_setstyle (s1, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_COPIERS));
 * 
 *   for (i = 0; i < 20; i++)
 *   {
 *     int x = i * 20;
 *     hawk_htb_insert (s1, &i, HAWK_SIZEOF(i), &x, HAWK_SIZEOF(x)); // eror handling skipped
 *   }
 * 
 *   hawk_htb_walk (s1, walk, HAWK_NULL);
 * 
 *   hawk_htb_close (s1);
 *   hawk_close_stdsios ();
 *   return 0;
 * }
 * @endcode
 */

typedef struct hawk_htb_t hawk_htb_t;
typedef struct hawk_htb_pair_t hawk_htb_pair_t;

/** 
 * The hawk_htb_walk_t type defines values that the callback function can
 * return to control hawk_htb_walk().
 */
enum hawk_htb_walk_t
{
        HAWK_HTB_WALK_STOP    = 0,
        HAWK_HTB_WALK_FORWARD = 1
};
typedef enum hawk_htb_walk_t hawk_htb_walk_t;

/**
 * The hawk_htb_id_t type defines IDs to indicate a key or a value in various
 * functions.
 */
enum hawk_htb_id_t
{
	HAWK_HTB_KEY = 0,
	HAWK_HTB_VAL = 1
};
typedef enum hawk_htb_id_t hawk_htb_id_t;

/**
 * The hawk_htb_copier_t type defines a pair contruction callback.
 * A special copier #HAWK_HTB_COPIER_INLINE is provided. This copier enables
 * you to copy the data inline to the internal node. No freeer is invoked
 * when the node is freeed.
 */
typedef void* (*hawk_htb_copier_t) (
	hawk_htb_t* htb  /* hash table */,
	void*      dptr /* pointer to a key or a value */, 
	hawk_oow_t dlen /* length of a key or a value */
);

/**
 * The hawk_htb_freeer_t defines a key/value destruction callback
 * The freeer is called when a node containing the element is destroyed.
 */
typedef void (*hawk_htb_freeer_t) (
	hawk_htb_t* htb,  /**< hash table */
	void*      dptr, /**< pointer to a key or a value */
	hawk_oow_t dlen  /**< length of a key or a value */
);


/**
 * The hawk_htb_comper_t type defines a key comparator that is called when
 * the htb needs to compare keys. A hash table is created with a default
 * comparator which performs bitwise comparison of two keys.
 * The comparator should return 0 if the keys are the same and a non-zero
 * integer otherwise.
 */
typedef int (*hawk_htb_comper_t) (
	const hawk_htb_t* htb,    /**< hash table */ 
	const void*      kptr1,  /**< key pointer */
	hawk_oow_t       klen1,  /**< key length */ 
	const void*      kptr2,  /**< key pointer */ 
	hawk_oow_t       klen2   /**< key length */
);

/**
 * The hawk_htb_keeper_t type defines a value keeper that is called when 
 * a value is retained in the context that it should be destroyed because
 * it is identical to a new value. Two values are identical if their 
 * pointers and lengths are equal.
 */
typedef void (*hawk_htb_keeper_t) (
	hawk_htb_t* htb,    /**< hash table */
	void*      vptr,   /**< value pointer */
	hawk_oow_t vlen    /**< value length */
);

/**
 * The hawk_htb_sizer_t type defines a bucket size claculator that is called
 * when hash table should resize the bucket. The current bucket size + 1 is 
 * passed as the hint.
 */
typedef hawk_oow_t (*hawk_htb_sizer_t) (
	hawk_htb_t* htb,  /**< htb */
	hawk_oow_t  hint  /**< sizing hint */
);

/**
 * The hawk_htb_hasher_t type defines a key hash function
 */
typedef hawk_oow_t (*hawk_htb_hasher_t) (
	const hawk_htb_t*  htb,   /**< hash table */
	const void*       kptr,  /**< key pointer */
	hawk_oow_t        klen   /**< key length in bytes */
);

/**
 * The hawk_htb_walker_t defines a pair visitor.
 */
typedef hawk_htb_walk_t (*hawk_htb_walker_t) (
	hawk_htb_t*      htb,   /**< htb */
	hawk_htb_pair_t* pair,  /**< pointer to a key/value pair */
	void*           ctx    /**< pointer to user-defined data */
);

/**
 * The hawk_htb_cbserter_t type defines a callback function for hawk_htb_cbsert().
 * The hawk_htb_cbserter() function calls it to allocate a new pair for the 
 * key pointed to by @a kptr of the length @a klen and the callback context
 * @a ctx. The second parameter @a pair is passed the pointer to the existing
 * pair for the key or #HAWK_NULL in case of no existing key. The callback
 * must return a pointer to a new or a reallocated pair. When reallocating the
 * existing pair, this callback must destroy the existing pair and return the 
 * newly reallocated pair. It must return #HAWK_NULL for failure.
 */
typedef hawk_htb_pair_t* (*hawk_htb_cbserter_t) (
	hawk_htb_t*      htb,    /**< hash table */
	hawk_htb_pair_t* pair,   /**< pair pointer */
	void*            kptr,   /**< key pointer */
	hawk_oow_t       klen,   /**< key length */
	void*            ctx     /**< callback context */
);


/**
 * The hawk_htb_pair_t type defines hash table pair. A pair is composed of a key
 * and a value. It maintains pointers to the beginning of a key and a value
 * plus their length. The length is scaled down with the scale factor 
 * specified in an owning hash table. 
 */
struct hawk_htb_pair_t
{
	hawk_ptl_t key;
	hawk_ptl_t val;

	/* management information below */
	hawk_htb_pair_t* next; 
};

typedef struct hawk_htb_style_t hawk_htb_style_t;

struct hawk_htb_style_t
{
	hawk_htb_copier_t copier[2];
	hawk_htb_freeer_t freeer[2];
	hawk_htb_comper_t comper;   /**< key comparator */
	hawk_htb_keeper_t keeper;   /**< value keeper */
	hawk_htb_sizer_t  sizer;    /**< bucket capacity recalculator */
	hawk_htb_hasher_t hasher;   /**< key hasher */
};

/**
 * The hawk_htb_style_kind_t type defines the type of predefined
 * callback set for pair manipulation.
 */
enum hawk_htb_style_kind_t
{
	/** store the key and the value pointer */
	HAWK_HTB_STYLE_DEFAULT,
	/** copy both key and value into the pair */
	HAWK_HTB_STYLE_INLINE_COPIERS,
	/** copy the key into the pair but store the value pointer */
	HAWK_HTB_STYLE_INLINE_KEY_COPIER,
	/** copy the value into the pair but store the key pointer */
	HAWK_HTB_STYLE_INLINE_VALUE_COPIER
};

typedef enum hawk_htb_style_kind_t  hawk_htb_style_kind_t;

/**
 * The hawk_htb_t type defines a hash table.
 */
struct hawk_htb_t
{
	hawk_gem_t* gem;

	const hawk_htb_style_t* style;

	hawk_uint8_t     scale[2]; /**< length scale */
	hawk_uint8_t     factor;   /**< load factor in percentage */

	hawk_oow_t       size;
	hawk_oow_t       capa;
	hawk_oow_t       threshold;

	hawk_htb_pair_t** bucket;
};

struct hawk_htb_itr_t
{
	hawk_htb_pair_t* pair;
	hawk_oow_t       buckno;
};

typedef struct hawk_htb_itr_t hawk_htb_itr_t;

/**
 * The HAWK_HTB_COPIER_SIMPLE macros defines a copier that remembers the
 * pointer and length of data in a pair.
 **/
#define HAWK_HTB_COPIER_SIMPLE ((hawk_htb_copier_t)1)

/**
 * The HAWK_HTB_COPIER_INLINE macros defines a copier that copies data into
 * a pair.
 **/
#define HAWK_HTB_COPIER_INLINE ((hawk_htb_copier_t)2)

#define HAWK_HTB_COPIER_DEFAULT (HAWK_HTB_COPIER_SIMPLE)
#define HAWK_HTB_FREEER_DEFAULT (HAWK_NULL)
#define HAWK_HTB_COMPER_DEFAULT (hawk_htb_dflcomp)
#define HAWK_HTB_KEEPER_DEFAULT (HAWK_NULL)
#define HAWK_HTB_SIZER_DEFAULT  (HAWK_NULL)
#define HAWK_HTB_HASHER_DEFAULT (hawk_htb_dflhash)

/**
 * The HAWK_HTB_SIZE() macro returns the number of pairs in a hash table.
 */
#define HAWK_HTB_SIZE(m) (*(const hawk_oow_t*)&(m)->size)

/**
 * The HAWK_HTB_CAPA() macro returns the maximum number of pairs that can be
 * stored in a hash table without further reorganization.
 */
#define HAWK_HTB_CAPA(m) (*(const hawk_oow_t*)&(m)->capa)

#define HAWK_HTB_FACTOR(m) (*(const int*)&(m)->factor)
#define HAWK_HTB_KSCALE(m) (*(const int*)&(m)->scale[HAWK_HTB_KEY])
#define HAWK_HTB_VSCALE(m) (*(const int*)&(m)->scale[HAWK_HTB_VAL])

#define HAWK_HTB_KPTL(p) (&(p)->key)
#define HAWK_HTB_VPTL(p) (&(p)->val)

#define HAWK_HTB_KPTR(p) ((p)->key.ptr)
#define HAWK_HTB_KLEN(p) ((p)->key.len)
#define HAWK_HTB_VPTR(p) ((p)->val.ptr)
#define HAWK_HTB_VLEN(p) ((p)->val.len)

#define HAWK_HTB_NEXT(p) ((p)->next)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_get_htb_style() functions returns a predefined callback set for
 * pair manipulation.
 */
HAWK_EXPORT const hawk_htb_style_t* hawk_get_htb_style (
	hawk_htb_style_kind_t kind
);

/**
 * The hawk_htb_open() function creates a hash table with a dynamic array 
 * bucket and a list of values chained. The initial capacity should be larger
 * than 0. The load factor should be between 0 and 100 inclusive and the load
 * factor of 0 disables bucket resizing. If you need extra space associated
 * with hash table, you may pass a non-zero value for @a xtnsize.
 * The HAWK_HTB_XTN() macro and the hawk_htb_getxtn() function return the 
 * pointer to the beginning of the extension.
 * The @a kscale and @a vscale parameters specify the unit of the key and 
 * value size. 
 * @return #hawk_htb_t pointer on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_htb_t* hawk_htb_open (
	hawk_gem_t* gem,
	hawk_oow_t  xtnsize, /**< extension size in bytes */
	hawk_oow_t  capa,    /**< initial capacity */
	int         factor,  /**< load factor */
	int         kscale,  /**< key scale - 1 to 255 */
	int         vscale   /**< value scale - 1 to 255 */
);


/**
 * The hawk_htb_close() function destroys a hash table.
 */
HAWK_EXPORT void hawk_htb_close (
	hawk_htb_t* htb /**< hash table */
);

/**
 * The hawk_htb_init() function initializes a hash table
 */
HAWK_EXPORT int hawk_htb_init (
	hawk_htb_t* htb,    /**< hash table */
	hawk_gem_t* gem,
	hawk_oow_t  capa,    /**< initial capacity */
	int         factor,  /**< load factor */
	int         kscale,  /**< key scale */
	int         vscale   /**< value scale */
);

/**
 * The hawk_htb_fini() funtion finalizes a hash table
 */
HAWK_EXPORT void hawk_htb_fini (
	hawk_htb_t* htb
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_htb_getxtn (hawk_htb_t* htb) { return (void*)(htb + 1); }
#else
#define hawk_htb_getxtn(htb) ((void*)((hawk_htb_t*)(htb) + 1))
#endif

/**
 * The hawk_htb_getstyle() function gets manipulation callback function set.
 */
HAWK_EXPORT const hawk_htb_style_t* hawk_htb_getstyle (
	const hawk_htb_t* htb /**< hash table */
);

/**
 * The hawk_htb_setstyle() function sets internal manipulation callback 
 * functions for data construction, destruction, resizing, hashing, etc.
 * The callback structure pointed to by \a style must outlive the hash
 * table pointed to by \a htb as the hash table doesn't copy the contents
 * of the structure.
 */
HAWK_EXPORT void hawk_htb_setstyle (
	hawk_htb_t*              htb,  /**< hash table */
	const hawk_htb_style_t*  style /**< callback function set */
);

/**
 * The hawk_htb_getsize() function gets the number of pairs in hash table.
 */
HAWK_EXPORT hawk_oow_t hawk_htb_getsize (
	const hawk_htb_t* htb
);

/**
 * The hawk_htb_getcapa() function gets the number of slots allocated 
 * in a hash bucket.
 */
HAWK_EXPORT hawk_oow_t hawk_htb_getcapa (
	const hawk_htb_t* htb /**< hash table */
);

/**
 * The hawk_htb_search() function searches a hash table to find a pair with a 
 * matching key. It returns the pointer to the pair found. If it fails
 * to find one, it returns HAWK_NULL.
 * @return pointer to the pair with a maching key, 
 *         or #HAWK_NULL if no match is found.
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_search (
	const hawk_htb_t* htb,   /**< hash table */
	const void*      kptr,  /**< key pointer */
	hawk_oow_t       klen   /**< key length */
);

/**
 * The hawk_htb_upsert() function searches a hash table for the pair with a 
 * matching key. If one is found, it updates the pair. Otherwise, it inserts
 * a new pair with the key and value given. It returns the pointer to the 
 * pair updated or inserted.
 * @return pointer to the updated or inserted pair on success, 
 *         #HAWK_NULL on failure. 
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_upsert (
	hawk_htb_t* htb,   /**< hash table */
	void*      kptr,  /**< key pointer */
	hawk_oow_t klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t vlen   /**< value length */
);

/**
 * The hawk_htb_ensert() function inserts a new pair with the key and the value
 * given. If there exists a pair with the key given, the function returns 
 * the pair containing the key.
 * @return pointer to a pair on success, #HAWK_NULL on failure. 
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_ensert (
	hawk_htb_t* htb,   /**< hash table */
	void*      kptr,  /**< key pointer */
	hawk_oow_t klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t vlen   /**< value length */
);

/**
 * The hawk_htb_insert() function inserts a new pair with the key and the value
 * given. If there exists a pair with the key given, the function returns 
 * #HAWK_NULL without channging the value.
 * @return pointer to the pair created on success, #HAWK_NULL on failure. 
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_insert (
	hawk_htb_t* htb,   /**< hash table */
	void*      kptr,  /**< key pointer */
	hawk_oow_t klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t vlen   /**< value length */
);

/**
 * The hawk_htb_update() function updates the value of an existing pair
 * with a matching key.
 * @return pointer to the pair on success, #HAWK_NULL on no matching pair
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_update (
	hawk_htb_t* htb,   /**< hash table */
	void*      kptr,  /**< key pointer */
	hawk_oow_t klen,  /**< key length */
	void*      vptr,  /**< value pointer */
	hawk_oow_t vlen   /**< value length */
);

/**
 * The hawk_htb_cbsert() function inserts a key/value pair by delegating pair 
 * allocation to a callback function. Depending on the callback function,
 * it may behave like hawk_htb_insert(), hawk_htb_upsert(), hawk_htb_update(),
 * hawk_htb_ensert(), or totally differently. The sample code below inserts
 * a new pair if the key is not found and appends the new value to the
 * existing value delimited by a comma if the key is found.
 *
 * @code
 * #include <hawk-htb.h>
 *
 * hawk_htb_walk_t print_map_pair (hawk_htb_t* map, hawk_htb_pair_t* pair, void* ctx)
 * {
 *   hawk_printf (HAWK_T("%.*s[%d] => %.*s[%d]\n"),
 *     HAWK_HTB_KLEN(pair), HAWK_HTB_KPTR(pair), (int)HAWK_HTB_KLEN(pair),
 *     HAWK_HTB_VLEN(pair), HAWK_HTB_VPTR(pair), (int)HAWK_HTB_VLEN(pair));
 *   return HAWK_HTB_WALK_FORWARD;
 * }
 * 
 * hawk_htb_pair_t* cbserter (
 *   hawk_htb_t* htb, hawk_htb_pair_t* pair,
 *   void* kptr, hawk_oow_t klen, void* ctx)
 * {
 *   hawk_cstr_t* v = (hawk_cstr_t*)ctx;
 *   if (pair == HAWK_NULL)
 *   {
 *     // no existing key for the key 
 *     return hawk_htb_allocpair (htb, kptr, klen, v->ptr, v->len);
 *   }
 *   else
 *   {
 *     // a pair with the key exists. 
 *     // in this sample, i will append the new value to the old value 
 *     // separated by a comma
 *     hawk_htb_pair_t* new_pair;
 *     hawk_ooch_t comma = HAWK_T(',');
 *     hawk_uint8_t* vptr;
 * 
 *     // allocate a new pair, but without filling the actual value. 
 *     // note vptr is given HAWK_NULL for that purpose 
 *     new_pair = hawk_htb_allocpair (
 *       htb, kptr, klen, HAWK_NULL, HAWK_HTB_VLEN(pair) + 1 + v->len); 
 *     if (new_pair == HAWK_NULL) return HAWK_NULL;
 * 
 *     // fill in the value space 
 *     vptr = HAWK_HTB_VPTR(new_pair);
 *     hawk_memcpy (vptr, HAWK_HTB_VPTR(pair), HAWK_HTB_VLEN(pair)*HAWK_SIZEOF(hawk_ooch_t));
 *     vptr += HAWK_HTB_VLEN(pair)*HAWK_SIZEOF(hawk_ooch_t);
 *     hawk_memcpy (vptr, &comma, HAWK_SIZEOF(hawk_ooch_t));
 *     vptr += HAWK_SIZEOF(hawk_ooch_t);
 *     hawk_memcpy (vptr, v->ptr, v->len*HAWK_SIZEOF(hawk_ooch_t));
 * 
 *     // this callback requires the old pair to be destroyed 
 *     hawk_htb_freepair (htb, pair);
 * 
 *     // return the new pair 
 *     return new_pair;
 *   }
 * }
 * 
 * int main ()
 * {
 *   hawk_htb_t* s1;
 *   int i;
 *   hawk_ooch_t* keys[] = { HAWK_T("one"), HAWK_T("two"), HAWK_T("three") };
 *   hawk_ooch_t* vals[] = { HAWK_T("1"), HAWK_T("2"), HAWK_T("3"), HAWK_T("4"), HAWK_T("5") };
 * 
 *   hawk_open_stdsios ();
 *   s1 = hawk_htb_open (
 *     HAWK_MMGR_GETDFL(), 0, 10, 70,
 *     HAWK_SIZEOF(hawk_ooch_t), HAWK_SIZEOF(hawk_ooch_t)
 *   ); // note error check is skipped 
 *   hawk_htb_setstyle (s1, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_COPIERS));
 * 
 *   for (i = 0; i < HAWK_COUNTOF(vals); i++)
 *   {
 *     hawk_cstr_t ctx;
 *     ctx.ptr = vals[i]; ctx.len = hawk_count_oocstr(vals[i]);
 *     hawk_htb_cbsert (s1,
 *       keys[i%HAWK_COUNTOF(keys)], hawk_count_oocstr(keys[i%HAWK_COUNTOF(keys)]),
 *       cbserter, &ctx
 *     ); // note error check is skipped
 *   }
 *   hawk_htb_walk (s1, print_map_pair, HAWK_NULL);
 * 
 *   hawk_htb_close (s1);
 *   hawk_close_stdsios ();
 *   return 0;
 * }
 * @endcode
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_cbsert (
	hawk_htb_t*         htb,      /**< hash table */
	void*               kptr,     /**< key pointer */
	hawk_oow_t          klen,     /**< key length */
	hawk_htb_cbserter_t cbserter, /**< callback function */
	void*               ctx       /**< callback context */
);

/**
 * The hawk_htb_delete() function deletes a pair with a matching key 
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_htb_delete (
	hawk_htb_t* htb,   /**< hash table */
	const void* kptr, /**< key pointer */
	hawk_oow_t klen   /**< key length */
);

/**
 * The hawk_htb_clear() function empties a hash table
 */
HAWK_EXPORT void hawk_htb_clear (
	hawk_htb_t* htb /**< hash table */
);

/**
 * The hawk_htb_walk() function traverses a hash table.
 */
HAWK_EXPORT void hawk_htb_walk (
	hawk_htb_t*       htb,    /**< hash table */
	hawk_htb_walker_t walker, /**< callback function for each pair */
	void*            ctx     /**< pointer to user-specific data */
);

HAWK_EXPORT void hawk_init_htb_itr (
	hawk_htb_itr_t* itr
);

/**
 * The hawk_htb_getfirstpair() function returns the pointer to the first pair
 * in a hash table.
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_getfirstpair (
	hawk_htb_t*     htb,   /**< hash table */
	hawk_htb_itr_t* itr    /**< iterator*/
);

/**
 * The hawk_htb_getnextpair() function returns the pointer to the next pair 
 * to the current pair @a pair in a hash table.
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_getnextpair (
	hawk_htb_t*      htb,    /**< hash table */
	hawk_htb_itr_t*  itr    /**< iterator*/
);

/**
 * The hawk_htb_allocpair() function allocates a pair for a key and a value 
 * given. But it does not chain the pair allocated into the hash table @a htb.
 * Use this function at your own risk. 
 *
 * Take note of he following special behavior when the copier is 
 * #HAWK_HTB_COPIER_INLINE.
 * - If @a kptr is #HAWK_NULL, the key space of the size @a klen is reserved but
 *   not propagated with any data.
 * - If @a vptr is #HAWK_NULL, the value space of the size @a vlen is reserved
 *   but not propagated with any data.
 */
HAWK_EXPORT hawk_htb_pair_t* hawk_htb_allocpair (
	hawk_htb_t* htb,
	void*      kptr, 
	hawk_oow_t klen,	
	void*      vptr,
	hawk_oow_t vlen
);

/**
 * The hawk_htb_freepair() function destroys a pair. But it does not detach
 * the pair destroyed from the hash table @a htb. Use this function at your
 * own risk.
 */
HAWK_EXPORT void hawk_htb_freepair (
	hawk_htb_t*      htb,
	hawk_htb_pair_t* pair
);

/**
 * The hawk_htb_dflhash() function is a default hash function.
 */
HAWK_EXPORT hawk_oow_t hawk_htb_dflhash (
	const hawk_htb_t*  htb,
	const void*       kptr,
	hawk_oow_t        klen
);

/**
 * The hawk_htb_dflcomp() function is default comparator.
 */
HAWK_EXPORT int hawk_htb_dflcomp (
	const hawk_htb_t* htb,
	const void*      kptr1,
	hawk_oow_t       klen1,
	const void*      kptr2,
	hawk_oow_t       klen2
);

#if defined(__cplusplus)
}
#endif

#endif
