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

#ifndef _HAWK_JSON_H_
#define _HAWK_JSON_H_

#include <hawk-cmn.h>
#include <hawk-gem.h>

/**
 * The hawk_json_t type defines a simple json parser.
 */
typedef struct hawk_json_t hawk_json_t;

#define HAWK_JSON_HDR \
	hawk_oow_t instsize_; \
	hawk_gem_t gem_

typedef struct hawk_json_alt_t hawk_json_alt_t;
struct hawk_json_alt_t
{
	/* ensure that hawk_json_t matches the beginning part of hawk_json_t */
	HAWK_JSON_HDR;
};

enum hawk_json_option_t
{
	HAWK_JSON_TRAIT
};
typedef enum hawk_json_option_t hawk_json_option_t;

enum hawk_json_trait_t
{
	/* no trait defined at this moment. XXXX is just a placeholder */
	HAWK_JSON_XXXX  = (1 << 0)
};
typedef enum hawk_json_trait_t hawk_json_trait_t;

/* ========================================================================= */

enum hawk_json_state_t
{
	HAWK_JSON_STATE_START,
	HAWK_JSON_STATE_IN_ARRAY,
	HAWK_JSON_STATE_IN_DIC,

	HAWK_JSON_STATE_IN_WORD_VALUE,
	HAWK_JSON_STATE_IN_NUMERIC_VALUE,
	HAWK_JSON_STATE_IN_STRING_VALUE,
	HAWK_JSON_STATE_IN_CHARACTER_VALUE
};
typedef enum hawk_json_state_t hawk_json_state_t;


/* ========================================================================= */
enum hawk_json_inst_t
{
	HAWK_JSON_INST_START_ARRAY,
	HAWK_JSON_INST_END_ARRAY,
	HAWK_JSON_INST_START_DIC,
	HAWK_JSON_INST_END_DIC,

	HAWK_JSON_INST_KEY,

	HAWK_JSON_INST_CHARACTER, /* there is no such element as character in real JSON */
	HAWK_JSON_INST_STRING,
	HAWK_JSON_INST_NUMBER,
	HAWK_JSON_INST_NIL,
	HAWK_JSON_INST_TRUE,
	HAWK_JSON_INST_FALSE,
};
typedef enum hawk_json_inst_t hawk_json_inst_t;

typedef int (*hawk_json_instcb_t) (
	hawk_json_t*           json,
	hawk_json_inst_t       inst,
	const hawk_oocs_t*     str
);

struct hawk_json_prim_t
{
	hawk_json_instcb_t        instcb;
};
typedef struct hawk_json_prim_t hawk_json_prim_t;

/* ========================================================================= */

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_json_t* hawk_json_open (
	hawk_mmgr_t*          mmgr,
	hawk_oow_t            xtnsize,
	hawk_cmgr_t*          cmgr,
	hawk_json_prim_t*     prim,
	hawk_errinf_t*        errinf
);

HAWK_EXPORT void hawk_json_close (
	hawk_json_t* json
);

HAWK_EXPORT void hawk_json_reset (
	hawk_json_t* json
);

HAWK_EXPORT int hawk_json_feedbchars (
	hawk_json_t*      json,
	const hawk_bch_t* ptr,
	hawk_oow_t        len,
	hawk_oow_t*       xlen
);

HAWK_EXPORT int hawk_json_feeduchars (
	hawk_json_t*      json,
	const hawk_uch_t* ptr,
	hawk_oow_t        len,
	hawk_oow_t*       xlen
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_json_feed hawk_json_feeduchars
#else
#	define hawk_json_feed hawk_json_feedbchars
#endif

HAWK_EXPORT hawk_json_state_t hawk_json_getstate (
	hawk_json_t* json
);

HAWK_EXPORT int hawk_json_setoption (
	hawk_json_t*         json,
	hawk_json_option_t   id,
	const void*          value
);

HAWK_EXPORT int hawk_json_getoption (
	hawk_json_t*         json,
	hawk_json_option_t   id,
	void*                value
);

/* ------------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
/**
 * The hawk_json_getxtn() function returns the pointer to the extension area
 * placed behind the actual json object.
 */
static HAWK_INLINE void* hawk_json_getxtn (hawk_json_t* json) { return (void*)((hawk_uint8_t*)json + ((hawk_json_alt_t*)json)->instsize_); }

/**
 * The hawk_json_getgem() function gets the pointer to the gem structure of the
 * json object.
 */
static HAWK_INLINE hawk_gem_t* hawk_json_getgem (hawk_json_t* json) { return &((hawk_json_alt_t*)json)->gem_; }

/**
 * The hawk_json_getmmgr() function gets the memory manager ujson in
 * hawk_json_open().
 */
static HAWK_INLINE hawk_mmgr_t* hawk_json_getmmgr (hawk_json_t* json) { return ((hawk_json_alt_t*)json)->gem_.mmgr; }
static HAWK_INLINE hawk_cmgr_t* hawk_json_getcmgr (hawk_json_t* json) { return ((hawk_json_alt_t*)json)->gem_.cmgr; }
static HAWK_INLINE void hawk_json_setcmgr (hawk_json_t* json, hawk_cmgr_t* cmgr) { ((hawk_json_alt_t*)json)->gem_.cmgr = cmgr; }
#else
#define hawk_json_getxtn(json) ((void*)((hawk_uint8_t*)json + ((hawk_json_alt_t*)json)->instsize_))
#define hawk_json_getgem(json) (&((hawk_json_alt_t*)(json))->gem_)
#define hawk_json_getmmgr(json) (((hawk_json_alt_t*)(json))->gem_.mmgr)
#define hawk_json_getcmgr(json) (((hawk_json_alt_t*)(json))->gem_.cmgr)
#define hawk_json_setcmgr(json,_cmgr) (((hawk_json_alt_t*)(json))->gem_.cmgr = (_cmgr))
#endif /* HAWK_HAVE_INLINE */

/**
 * The hawk_json_geterrnum() function returns the number of the last error
 * occurred.
 * \return error number
 */

/**
 * The hawk_json_geterror() function gets an error number, an error location,
 * and an error message. The information is set to the memory area pointed
 * to by each parameter.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_errnum_t hawk_json_geterrnum (hawk_json_t* json) { return hawk_gem_geterrnum(hawk_json_getgem(json)); }
static HAWK_INLINE const hawk_loc_t* hawk_json_geterrloc (hawk_json_t* json) { return hawk_gem_geterrloc(hawk_json_getgem(json)); }
static HAWK_INLINE const hawk_bch_t* hawk_json_geterrbmsg (hawk_json_t* json) { return hawk_gem_geterrbmsg(hawk_json_getgem(json)); }
static HAWK_INLINE const hawk_uch_t* hawk_json_geterrumsg (hawk_json_t* json) { return hawk_gem_geterrumsg(hawk_json_getgem(json)); }
static HAWK_INLINE void hawk_json_geterrbinf (hawk_json_t* json, hawk_errbinf_t* errinf) { return hawk_gem_geterrbinf(hawk_json_getgem(json), errinf); }
static HAWK_INLINE void hawk_json_geterruinf (hawk_json_t* json, hawk_erruinf_t* errinf) { return hawk_gem_geterruinf(hawk_json_getgem(json), errinf); }
static HAWK_INLINE void hawk_json_geterror (hawk_json_t* json, hawk_errnum_t* errnum, const hawk_ooch_t** errmsg, hawk_loc_t* errloc) { return hawk_gem_geterror(hawk_json_getgem(json), errnum, errmsg, errloc); }
#else
#define hawk_json_geterrnum(json) hawk_gem_geterrnum(hawk_json_getgem(json))
#define hawk_json_geterrloc(json) hawk_gem_geterrloc(hawk_json_getgem(json))
#define hawk_json_geterrbmsg(json) hawk_gem_geterrbmsg(hawk_json_getgem(json))
#define hawk_json_geterrumsg(json) hawk_gem_geterrumsg(hawk_json_getgem(json))
#define hawk_json_geterrbinf(json, errinf) (hawk_gem_geterrbinf(hawk_json_getgem(json), errinf))
#define hawk_json_geterruinf(json, errinf) (hawk_gem_geterruinf(hawk_json_getgem(json), errinf))
#define hawk_json_geterror(json, errnum, errmsg, errloc) (hawk_gem_geterror(hawk_json_getgem(json), errnum, errmsg, errloc))
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_json_geterrmsg hawk_json_geterrumsg
#	define hawk_json_geterrinf hawk_json_geterruinf
#else
#	define hawk_json_geterrmsg hawk_json_geterrbmsg
#	define hawk_json_geterrinf hawk_json_geterrbinf
#endif


/**
 * The hawk_json_seterrnum() function sets the error information omitting
 * error location. You must pass a non-NULL for \a errarg if the specified
 * error number \a errnum requires one or more arguments to format an
 * error message.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_json_seterrnum (hawk_json_t* json, const hawk_loc_t* errloc, hawk_errnum_t errnum) { hawk_gem_seterrnum (hawk_json_getgem(json), errloc, errnum); }
static HAWK_INLINE void hawk_json_seterrinf (hawk_json_t* json, const hawk_errinf_t* errinf) { hawk_gem_seterrinf (hawk_json_getgem(json), errinf); }
static HAWK_INLINE void hawk_json_seterror (hawk_json_t* json, const hawk_loc_t*  errloc, hawk_errnum_t errnum, const hawk_oocs_t* errarg) { hawk_gem_seterror(hawk_json_getgem(json), errloc, errnum, errarg); }
static HAWK_INLINE const hawk_ooch_t* hawk_json_backuperrmsg (hawk_json_t* json) { return hawk_gem_backuperrmsg(hawk_json_getgem(json)); }
#else
#define hawk_json_seterrnum(json, errloc, errnum) hawk_gem_seterrnum(hawk_json_getgem(json), errloc, errnum)
#define hawk_json_seterrinf(json, errinf) hawk_gem_seterrinf(hawk_json_getgem(json), errinf)
#define hawk_json_seterror(json, errloc, errnum, errarg) hawk_gem_seterror(hawk_json_getgem(json), errloc, errnum, errarg)
#define hawk_json_backuperrmsg(json) hawk_gem_backuperrmsg(hawk_json_getgem(json))
#endif

HAWK_EXPORT void hawk_json_seterrbfmt (
	hawk_json_t*       json,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_bch_t* fmt,
	...
);

HAWK_EXPORT void hawk_json_seterrufmt (
	hawk_json_t*       json,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_uch_t* fmt,
	...
);


HAWK_EXPORT void hawk_json_seterrbvfmt (
	hawk_json_t*         json,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_bch_t*   errfmt,
	va_list             ap
);

HAWK_EXPORT void hawk_json_seterruvfmt (
	hawk_json_t*         json,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_uch_t*   errfmt,
	va_list             ap
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT void* hawk_json_allocmem (
	hawk_json_t* json,
	hawk_oow_t   size
);

HAWK_EXPORT void* hawk_json_callocmem (
	hawk_json_t* json,
	hawk_oow_t   size
);

HAWK_EXPORT void* hawk_json_reallocmem (
	hawk_json_t*  json,
	void*         ptr,
	hawk_oow_t    size
);

HAWK_EXPORT void hawk_json_freemem (
	hawk_json_t* json,
	void*        ptr
);

/* ------------------------------------------------------------------------- */

/**
 * The hawk_json_openstd() function creates a stream editor with the default
 * memory manager and initializes it.
 * \return pointer to a stream editor on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_json_t* hawk_json_openstd (
	hawk_oow_t        xtnsize,  /**< extension size in bytes */
	hawk_json_prim_t* prim,
	hawk_errinf_t*    errinf
);

/**
 * The hawk_json_openstdwithmmgr() function creates a stream editor with a
 * user-defined memory manager. It is equivalent to hawk_json_openstd(),
 * except that you can specify your own memory manager.
 * \return pointer to a stream editor on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_json_t* hawk_json_openstdwithmmgr (
	hawk_mmgr_t*      mmgr,     /**< memory manager */
	hawk_oow_t        xtnsize,  /**< extension size in bytes */
	hawk_cmgr_t*      cmgr,
	hawk_json_prim_t* prim,
	hawk_errinf_t*    errinf
);

#if defined(__cplusplus)
}
#endif

#endif
