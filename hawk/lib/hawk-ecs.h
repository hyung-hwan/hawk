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

#ifndef _HAWK_ECS_H_
#define _HAWK_ECS_H_

#include <hawk-cmn.h>
#include <stdarg.h>

/** string pointer and length as a aggregate */
#define HAWK_BECS_BCS(s)      (&((s)->val))  
/** string length */
#define HAWK_BECS_LEN(s)      ((s)->val.len)
/** string pointer */
#define HAWK_BECS_PTR(s)      ((s)->val.ptr)
/** pointer to a particular position */
#define HAWK_BECS_CPTR(s,idx) (&(s)->val.ptr[idx])
/** string capacity */
#define HAWK_BECS_CAPA(s)     ((s)->capa)
/** character at the given position */
#define HAWK_BECS_CHAR(s,idx) ((s)->val.ptr[idx]) 
/**< last character. unsafe if length <= 0 */
#define HAWK_BECS_LASTCHAR(s) ((s)->val.ptr[(s)->val.len-1])

/** string pointer and length as a aggregate */
#define HAWK_UECS_UCS(s)      (&((s)->val))  
/** string length */
#define HAWK_UECS_LEN(s)      ((s)->val.len)
/** string pointer */
#define HAWK_UECS_PTR(s)      ((s)->val.ptr)
/** pointer to a particular position */
#define HAWK_UECS_CPTR(s,idx) (&(s)->val.ptr[idx])
/** string capacity */
#define HAWK_UECS_CAPA(s)     ((s)->capa)
/** character at the given position */
#define HAWK_UECS_CHAR(s,idx) ((s)->val.ptr[idx]) 
/**< last character. unsafe if length <= 0 */
#define HAWK_UECS_LASTCHAR(s) ((s)->val.ptr[(s)->val.len-1])

typedef struct hawk_becs_t hawk_becs_t;
typedef struct hawk_uecs_t hawk_uecs_t;

typedef hawk_oow_t (*hawk_becs_sizer_t) (
	hawk_becs_t* data,
	hawk_oow_t   hint
);

typedef hawk_oow_t (*hawk_uecs_sizer_t) (
	hawk_uecs_t* data,
	hawk_oow_t   hint
);

#if defined(HAWK_OOCH_IS_UCH)
#	define HAWK_OOECS_OOCS(s)     HAWK_UECS_UCS(s)
#	define HAWK_OOECS_LEN(s)      HAWK_UECS_LEN(s)
#	define HAWK_OOECS_PTR(s)      HAWK_UECS_PTR(s)
#	define HAWK_OOECS_CPTR(s,idx) HAWK_UECS_CPTR(s,idx)
#	define HAWK_OOECS_CAPA(s)     HAWK_UECS_CAPA(s)
#	define HAWK_OOECS_CHAR(s,idx) HAWK_UECS_CHAR(s,idx)
#	define HAWK_OOECS_LASTCHAR(s) HAWK_UECS_LASTCHAR(s)
#	define hawk_ooecs_t           hawk_uecs_t
#	define hawk_ooecs_sizer_t     hawk_uecs_sizer_t
#else
#	define HAWK_OOECS_OOCS(s)     HAWK_BECS_BCS(s)
#	define HAWK_OOECS_LEN(s)      HAWK_BECS_LEN(s)
#	define HAWK_OOECS_PTR(s)      HAWK_BECS_PTR(s)
#	define HAWK_OOECS_CPTR(s,idx) HAWK_BECS_CPTR(s,idx)
#	define HAWK_OOECS_CAPA(s)     HAWK_BECS_CAPA(s)
#	define HAWK_OOECS_CHAR(s,idx) HAWK_BECS_CHAR(s,idx)
#	define HAWK_OOECS_LASTCHAR(s) HAWK_BECS_LASTCHAR(s)
#	define hawk_ooecs_t           hawk_becs_t
#	define hawk_ooecs_sizer_t     hawk_becs_sizer_t
#endif


/**
 * The hawk_becs_t type defines a dynamically resizable multi-byte string.
 */
struct hawk_becs_t
{
	hawk_gem_t*       gem;
	hawk_becs_sizer_t sizer; /**< buffer resizer function */
	hawk_bcs_t        val;   /**< buffer/string pointer and lengh */
	hawk_oow_t        capa;  /**< buffer capacity */
};

/**
 * The hawk_uecs_t type defines a dynamically resizable wide-character string.
 */
struct hawk_uecs_t
{
	hawk_gem_t*       gem;
	hawk_uecs_sizer_t sizer; /**< buffer resizer function */
	hawk_ucs_t        val;   /**< buffer/string pointer and lengh */
	hawk_oow_t        capa;  /**< buffer capacity */
};


#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_becs_open() function creates a dynamically resizable multibyte string.
 */
HAWK_EXPORT hawk_becs_t* hawk_becs_open (
	hawk_gem_t* gem,
	hawk_oow_t  xtnsize,
	hawk_oow_t  capa
);

HAWK_EXPORT void hawk_becs_close (
	hawk_becs_t* becs
);

/**
 * The hawk_becs_init() function initializes a dynamically resizable string
 * If the parameter capa is 0, it doesn't allocate the internal buffer 
 * in advance and always succeeds.
 * \return 0 on success, -1 on failure.
 */
HAWK_EXPORT int hawk_becs_init (
	hawk_becs_t*  becs,
	hawk_gem_t*   gem,
	hawk_oow_t    capa
);

/**
 * The hawk_becs_fini() function finalizes a dynamically resizable string.
 */
HAWK_EXPORT void hawk_becs_fini (
	hawk_becs_t* becs
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_becs_getxtn (hawk_becs_t* becs) { return (void*)(becs + 1); }
#else
#define hawk_becs_getxtn(becs) ((void*)((hawk_becs_t*)(becs) + 1))
#endif

/**
 * The hawk_becs_yield() function assigns the buffer to an variable of the
 * #hawk_bcs_t type and recreate a new buffer of the \a new_capa capacity.
 * The function fails if it fails to allocate a new buffer.
 * \return 0 on success, and -1 on failure.
 */
HAWK_EXPORT int hawk_becs_yield (
	hawk_becs_t*  becs,    /**< string */
	hawk_bcs_t*   buf,    /**< buffer pointer */
	hawk_oow_t    newcapa /**< new capacity */
);

HAWK_EXPORT hawk_bch_t* hawk_becs_yieldptr (
	hawk_becs_t*   becs,    /**< string */
	hawk_oow_t     newcapa /**< new capacity */
);

/**
 * The hawk_becs_getsizer() function gets the sizer.
 * \return sizer function set or HAWK_NULL if no sizer is set.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_becs_sizer_t hawk_becs_getsizer (hawk_becs_t* becs) { return becs->sizer; }
#else
#	define hawk_becs_getsizer(becs) ((becs)->sizer)
#endif

/**
 * The hawk_becs_setsizer() function specify a new sizer for a dynamic string.
 * With no sizer specified, the dynamic string doubles the current buffer
 * when it needs to increase its size. The sizer function is passed a dynamic
 * string and the minimum capacity required to hold data after resizing.
 * The string is truncated if the sizer function returns a smaller number
 * than the hint passed.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_becs_setsizer (hawk_becs_t* becs, hawk_becs_sizer_t sizer) { becs->sizer = sizer; }
#else
#	define hawk_becs_setsizer(becs,sizerfn) ((becs)->sizer = (sizerfn))
#endif

/**
 * The hawk_becs_getcapa() function returns the current capacity.
 * You may use HAWK_STR_CAPA(str) macro for performance sake.
 * \return current capacity in number of characters.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_oow_t hawk_becs_getcapa (hawk_becs_t* becs) { return HAWK_BECS_CAPA(becs); }
#else
#	define hawk_becs_getcapa(becs) HAWK_BECS_CAPA(becs)
#endif

/**
 * The hawk_becs_setcapa() function sets the new capacity. If the new capacity
 * is smaller than the old, the overflowing characters are removed from
 * from the buffer.
 * \return (hawk_oow_t)-1 on failure, new capacity on success 
 */
HAWK_EXPORT hawk_oow_t hawk_becs_setcapa (
	hawk_becs_t* becs,
	hawk_oow_t   capa
);

/**
 * The hawk_becs_getlen() function return the string length.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_oow_t hawk_becs_getlen (hawk_becs_t* becs) { return HAWK_BECS_LEN(becs); }
#else
#	define hawk_becs_getlen(becs) HAWK_BECS_LEN(becs)
#endif

/**
 * The hawk_becs_setlen() function changes the string length.
 * \return (hawk_oow_t)-1 on failure, new length on success 
 */
HAWK_EXPORT hawk_oow_t hawk_becs_setlen (
	hawk_becs_t* becs,
	hawk_oow_t   len
);

/**
 * The hawk_becs_clear() funtion deletes all characters in a string and sets
 * the length to 0. It doesn't resize the internal buffer.
 */
HAWK_EXPORT void hawk_becs_clear (
	hawk_becs_t* becs
);

/**
 * The hawk_becs_swap() function exchanges the pointers to a buffer between
 * two strings. It updates the length and the capacity accordingly.
 */
HAWK_EXPORT void hawk_becs_swap (
	hawk_becs_t* becs1,
	hawk_becs_t* becs2
);

HAWK_EXPORT hawk_oow_t hawk_becs_cpy (
	hawk_becs_t*      becs,
	const hawk_bch_t* s
);

HAWK_EXPORT hawk_oow_t hawk_becs_ncpy (
	hawk_becs_t*       becs,
	const hawk_bch_t*  s,
	hawk_oow_t         len
);

HAWK_EXPORT hawk_oow_t hawk_becs_cat (
	hawk_becs_t*      becs,
	const hawk_bch_t* s
);

HAWK_EXPORT hawk_oow_t hawk_becs_ncat (
	hawk_becs_t*      becs,
	const hawk_bch_t* s,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_becs_nrcat (
	hawk_becs_t*      becs,
	const hawk_bch_t* s,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_becs_ccat (
	hawk_becs_t*  becs,
	hawk_bch_t    c
);

HAWK_EXPORT hawk_oow_t hawk_becs_nccat (
	hawk_becs_t*  becs,
	hawk_bch_t    c,
	hawk_oow_t    len
);

HAWK_EXPORT hawk_oow_t hawk_becs_del (
	hawk_becs_t* becs,
	hawk_oow_t   index,
	hawk_oow_t   size
);

HAWK_EXPORT hawk_oow_t hawk_becs_amend (
	hawk_becs_t*      becs,
	hawk_oow_t        index,
	hawk_oow_t        size,
	const hawk_bch_t* repl
);

HAWK_EXPORT hawk_oow_t hawk_becs_vfcat (
	hawk_becs_t*       str, 
	const hawk_bch_t*  fmt,
	va_list            ap
);

HAWK_EXPORT hawk_oow_t hawk_becs_fcat (
	hawk_becs_t*       str, 
	const hawk_bch_t*  fmt,
	...
);

HAWK_EXPORT hawk_oow_t hawk_becs_vfmt (
	hawk_becs_t*       str, 
	const hawk_bch_t*  fmt,
	va_list            ap
);

HAWK_EXPORT hawk_oow_t hawk_becs_fmt (
	hawk_becs_t*       str, 
	const hawk_bch_t*  fmt,
	...
);

/* ------------------------------------------------------------------------ */

/**
 * The hawk_uecs_open() function creates a dynamically resizable multibyte string.
 */
HAWK_EXPORT hawk_uecs_t* hawk_uecs_open (
	hawk_gem_t* gem,
	hawk_oow_t  xtnsize,
	hawk_oow_t  capa
);

HAWK_EXPORT void hawk_uecs_close (
	hawk_uecs_t* uecs
);

/**
 * The hawk_uecs_init() function initializes a dynamically resizable string
 * If the parameter capa is 0, it doesn't allocate the internal buffer 
 * in advance and always succeeds.
 * \return 0 on success, -1 on failure.
 */
HAWK_EXPORT int hawk_uecs_init (
	hawk_uecs_t*  uecs,
	hawk_gem_t*   gem,
	hawk_oow_t    capa
);

/**
 * The hawk_uecs_fini() function finalizes a dynamically resizable string.
 */
HAWK_EXPORT void hawk_uecs_fini (
	hawk_uecs_t* uecs
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_uecs_getxtn (hawk_uecs_t* uecs) { return (void*)(uecs + 1); }
#else
#define hawk_uecs_getxtn(uecs) ((void*)((hawk_uecs_t*)(uecs) + 1))
#endif

/**
 * The hawk_uecs_yield() function assigns the buffer to an variable of the
 * #hawk_ucs_t type and recreate a new buffer of the \a new_capa capacity.
 * The function fails if it fails to allocate a new buffer.
 * \return 0 on success, and -1 on failure.
 */
HAWK_EXPORT int hawk_uecs_yield (
	hawk_uecs_t*   uecs,    /**< string */
	hawk_ucs_t*    buf,    /**< buffer pointer */
	hawk_oow_t     newcapa /**< new capacity */
);

HAWK_EXPORT hawk_uch_t* hawk_uecs_yieldptr (
	hawk_uecs_t*   uecs,    /**< string */
	hawk_oow_t     newcapa /**< new capacity */
);

/**
 * The hawk_uecs_getsizer() function gets the sizer.
 * \return sizer function set or HAWK_NULL if no sizer is set.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_uecs_sizer_t hawk_uecs_getsizer (hawk_uecs_t* uecs) { return uecs->sizer; }
#else
#	define hawk_uecs_getsizer(uecs) ((uecs)->sizer)
#endif

/**
 * The hawk_uecs_setsizer() function specify a new sizer for a dynamic string.
 * With no sizer specified, the dynamic string doubles the current buffer
 * when it needs to increase its size. The sizer function is passed a dynamic
 * string and the minimum capacity required to hold data after resizing.
 * The string is truncated if the sizer function returns a smaller number
 * than the hint passed.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_uecs_setsizer (hawk_uecs_t* uecs, hawk_uecs_sizer_t sizer) { uecs->sizer = sizer; }
#else
#	define hawk_uecs_setsizer(uecs,sizerfn) ((uecs)->sizer = (sizerfn))
#endif

/**
 * The hawk_uecs_getcapa() function returns the current capacity.
 * You may use HAWK_STR_CAPA(str) macro for performance sake.
 * \return current capacity in number of characters.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_oow_t hawk_uecs_getcapa (hawk_uecs_t* uecs) { return HAWK_UECS_CAPA(uecs); }
#else
#	define hawk_uecs_getcapa(uecs) HAWK_UECS_CAPA(uecs)
#endif

/**
 * The hawk_uecs_setcapa() function sets the new capacity. If the new capacity
 * is smaller than the old, the overflowing characters are removed from
 * from the buffer.
 * \return (hawk_oow_t)-1 on failure, new capacity on success 
 */
HAWK_EXPORT hawk_oow_t hawk_uecs_setcapa (
	hawk_uecs_t* uecs,
	hawk_oow_t   capa
);

/**
 * The hawk_uecs_getlen() function return the string length.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_oow_t hawk_uecs_getlen (hawk_uecs_t* uecs) { return HAWK_UECS_LEN(uecs); }
#else
#	define hawk_uecs_getlen(uecs) HAWK_UECS_LEN(uecs)
#endif

/**
 * The hawk_uecs_setlen() function changes the string length.
 * \return (hawk_oow_t)-1 on failure, new length on success 
 */
HAWK_EXPORT hawk_oow_t hawk_uecs_setlen (
	hawk_uecs_t* uecs,
	hawk_oow_t   len
);


/**
 * The hawk_uecs_clear() funtion deletes all characters in a string and sets
 * the length to 0. It doesn't resize the internal buffer.
 */
HAWK_EXPORT void hawk_uecs_clear (
	hawk_uecs_t* uecs
);

/**
 * The hawk_uecs_swap() function exchanges the pointers to a buffer between
 * two strings. It updates the length and the capacity accordingly.
 */
HAWK_EXPORT void hawk_uecs_swap (
	hawk_uecs_t* uecs1,
	hawk_uecs_t* uecs2
);

HAWK_EXPORT hawk_oow_t hawk_uecs_cpy (
	hawk_uecs_t*      uecs,
	const hawk_uch_t* s
);

HAWK_EXPORT hawk_oow_t hawk_uecs_ncpy (
	hawk_uecs_t*      uecs,
	const hawk_uch_t* s,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_uecs_cat (
	hawk_uecs_t*      uecs,
	const hawk_uch_t* s
);

HAWK_EXPORT hawk_oow_t hawk_uecs_ncat (
	hawk_uecs_t*      uecs,
	const hawk_uch_t* s,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_uecs_nrcat (
	hawk_uecs_t*      uecs,
	const hawk_uch_t* s,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_oow_t hawk_uecs_ccat (
	hawk_uecs_t*  uecs,
	hawk_uch_t    c
);

HAWK_EXPORT hawk_oow_t hawk_uecs_nccat (
	hawk_uecs_t*  uecs,
	hawk_uch_t    c,
	hawk_oow_t    len
);

HAWK_EXPORT hawk_oow_t hawk_uecs_del (
	hawk_uecs_t* uecs,
	hawk_oow_t   index,
	hawk_oow_t   size
);

HAWK_EXPORT hawk_oow_t hawk_uecs_amend (
	hawk_uecs_t*       uecs,
	hawk_oow_t         index,
	hawk_oow_t         size,
	const hawk_uch_t*  repl
);

HAWK_EXPORT hawk_oow_t hawk_uecs_vfcat (
	hawk_uecs_t*       str, 
	const hawk_uch_t*  fmt,
	va_list            ap
);

HAWK_EXPORT hawk_oow_t hawk_uecs_fcat (
	hawk_uecs_t*       str, 
	const hawk_uch_t*  fmt,
	...
);

HAWK_EXPORT hawk_oow_t hawk_uecs_vfmt (
	hawk_uecs_t*       str, 
	const hawk_uch_t*  fmt,
	va_list            ap
);

HAWK_EXPORT hawk_oow_t hawk_uecs_fmt (
	hawk_uecs_t*       str, 
	const hawk_uch_t*  fmt,
	...
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_ooecs_open hawk_uecs_open
#	define hawk_ooecs_close hawk_uecs_close
#	define hawk_ooecs_init hawk_uecs_init
#	define hawk_ooecs_fini hawk_uecs_fini
#	define hawk_ooecs_getxtn hawk_uecs_getxtn
#	define hawk_ooecs_yield hawk_uecs_yield
#	define hawk_ooecs_yieldptr hawk_uecs_yieldptr
#	define hawk_ooecs_getsizer hawk_uecs_getsizer
#	define hawk_ooecs_setsizer hawk_uecs_setsizer
#	define hawk_ooecs_getcapa hawk_uecs_getcapa
#	define hawk_ooecs_setcapa hawk_uecs_setcapa
#	define hawk_ooecs_getlen hawk_uecs_getlen
#	define hawk_ooecs_setlen hawk_uecs_setlen
#	define hawk_ooecs_clear hawk_uecs_clear
#	define hawk_ooecs_swap hawk_uecs_swap
#	define hawk_ooecs_cpy hawk_uecs_cpy
#	define hawk_ooecs_ncpy hawk_uecs_ncpy
#	define hawk_ooecs_cat hawk_uecs_cat
#	define hawk_ooecs_ncat hawk_uecs_ncat
#	define hawk_ooecs_nrcat hawk_uecs_nrcat
#	define hawk_ooecs_ccat hawk_uecs_ccat
#	define hawk_ooecs_nccat hawk_uecs_nccat
#	define hawk_ooecs_del hawk_uecs_del
#	define hawk_ooecs_amend hawk_uecs_amend
#	define hawk_ooecs_vfcat hawk_uecs_vfcat
#	define hawk_ooecs_fcat hawk_uecs_fcat
#	define hawk_ooecs_vfmt hawk_uecs_vfmt
#	define hawk_ooecs_fmt hawk_uecs_fmt
#else
#	define hawk_ooecs_open hawk_becs_open
#	define hawk_ooecs_close hawk_becs_close
#	define hawk_ooecs_init hawk_becs_init
#	define hawk_ooecs_fini hawk_becs_fini
#	define hawk_ooecs_getxtn hawk_becs_getxtn
#	define hawk_ooecs_yield hawk_becs_yield
#	define hawk_ooecs_yieldptr hawk_becs_yieldptr
#	define hawk_ooecs_getsizer hawk_becs_getsizer
#	define hawk_ooecs_setsizer hawk_becs_setsizer
#	define hawk_ooecs_getcapa hawk_becs_getcapa
#	define hawk_ooecs_setcapa hawk_becs_setcapa
#	define hawk_ooecs_getlen hawk_becs_getlen
#	define hawk_ooecs_setlen hawk_becs_setlen
#	define hawk_ooecs_clear hawk_becs_clear
#	define hawk_ooecs_swap hawk_becs_swap
#	define hawk_ooecs_cpy hawk_becs_cpy
#	define hawk_ooecs_ncpy hawk_becs_ncpy
#	define hawk_ooecs_cat hawk_becs_cat
#	define hawk_ooecs_ncat hawk_becs_ncat
#	define hawk_ooecs_nrcat hawk_becs_nrcat
#	define hawk_ooecs_ccat hawk_becs_ccat
#	define hawk_ooecs_nccat hawk_becs_nccat
#	define hawk_ooecs_del hawk_becs_del
#	define hawk_ooecs_amend hawk_becs_amend
#	define hawk_ooecs_vfcat hawk_becs_vfcat
#	define hawk_ooecs_fcat hawk_becs_fcat
#	define hawk_ooecs_vfmt hawk_becs_vfmt
#	define hawk_ooecs_fmt hawk_becs_fmt
#endif

HAWK_EXPORT hawk_oow_t hawk_becs_ncatuchars (
	hawk_becs_t*       str,
	const hawk_uch_t*  s,
	hawk_oow_t         len,
	hawk_cmgr_t*       cmgr
);

HAWK_EXPORT hawk_oow_t hawk_uecs_ncatbchars (
	hawk_uecs_t*       str,
	const hawk_bch_t*  s,
	hawk_oow_t         len,
	hawk_cmgr_t*       cmgr,
	int                all
);

#if defined(__cplusplus)
}
#endif

#endif
