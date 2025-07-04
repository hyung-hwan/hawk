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

#ifndef _HAWK_UTL_H_
#define _HAWK_UTL_H_

#include <hawk-cmn.h>
#include <hawk-str.h>
#include <stdarg.h>

#define HAWK_EPOCH_YEAR  (1970)
#define HAWK_EPOCH_MON   (1)
#define HAWK_EPOCH_DAY   (1)
#define HAWK_EPOCH_WDAY  (4)

/* windows specific epoch time */
#define HAWK_EPOCH_YEAR_WIN   (1601)
#define HAWK_EPOCH_MON_WIN    (1)
#define HAWK_EPOCH_DAY_WIN    (1)

/* =========================================================================
 * ENDIAN CHANGE OF A CONSTANT
 * ========================================================================= */
#define HAWK_CONST_BSWAP16(x) \
	((hawk_uint16_t)((((hawk_uint16_t)(x) & ((hawk_uint16_t)0xff << 0)) << 8) | \
	                (((hawk_uint16_t)(x) & ((hawk_uint16_t)0xff << 8)) >> 8)))

#define HAWK_CONST_BSWAP32(x) \
	((hawk_uint32_t)((((hawk_uint32_t)(x) & ((hawk_uint32_t)0xff <<  0)) << 24) | \
	                (((hawk_uint32_t)(x) & ((hawk_uint32_t)0xff <<  8)) <<  8) | \
	                (((hawk_uint32_t)(x) & ((hawk_uint32_t)0xff << 16)) >>  8) | \
	                (((hawk_uint32_t)(x) & ((hawk_uint32_t)0xff << 24)) >> 24)))

#if defined(HAWK_HAVE_UINT64_T)
#define HAWK_CONST_BSWAP64(x) \
	((hawk_uint64_t)((((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff <<  0)) << 56) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff <<  8)) << 40) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff << 16)) << 24) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff << 24)) <<  8) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff << 32)) >>  8) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff << 40)) >> 24) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff << 48)) >> 40) | \
	                (((hawk_uint64_t)(x) & ((hawk_uint64_t)0xff << 56)) >> 56)))
#endif

#if defined(HAWK_HAVE_UINT128_T)
#define HAWK_CONST_BSWAP128(x) \
	((hawk_uint128_t)((((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 0)) << 120) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 8)) << 104) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 16)) << 88) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 24)) << 72) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 32)) << 56) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 40)) << 40) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 48)) << 24) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 56)) << 8) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 64)) >> 8) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 72)) >> 24) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 80)) >> 40) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 88)) >> 56) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 96)) >> 72) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 104)) >> 88) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 112)) >> 104) | \
	                 (((hawk_uint128_t)(x) & ((hawk_uint128_t)0xff << 120)) >> 120)))
#endif

#if defined(HAWK_ENDIAN_LITTLE)

#	if defined(HAWK_HAVE_UINT16_T)
#	define HAWK_CONST_NTOH16(x) HAWK_CONST_BSWAP16(x)
#	define HAWK_CONST_HTON16(x) HAWK_CONST_BSWAP16(x)
#	define HAWK_CONST_HTOBE16(x) HAWK_CONST_BSWAP16(x)
#	define HAWK_CONST_HTOLE16(x) (x)
#	define HAWK_CONST_BE16TOH(x) HAWK_CONST_BSWAP16(x)
#	define HAWK_CONST_LE16TOH(x) (x)
#	endif

#	if defined(HAWK_HAVE_UINT32_T)
#	define HAWK_CONST_NTOH32(x) HAWK_CONST_BSWAP32(x)
#	define HAWK_CONST_HTON32(x) HAWK_CONST_BSWAP32(x)
#	define HAWK_CONST_HTOBE32(x) HAWK_CONST_BSWAP32(x)
#	define HAWK_CONST_HTOLE32(x) (x)
#	define HAWK_CONST_BE32TOH(x) HAWK_CONST_BSWAP32(x)
#	define HAWK_CONST_LE32TOH(x) (x)
#	endif

#	if defined(HAWK_HAVE_UINT64_T)
#	define HAWK_CONST_NTOH64(x) HAWK_CONST_BSWAP64(x)
#	define HAWK_CONST_HTON64(x) HAWK_CONST_BSWAP64(x)
#	define HAWK_CONST_HTOBE64(x) HAWK_CONST_BSWAP64(x)
#	define HAWK_CONST_HTOLE64(x) (x)
#	define HAWK_CONST_BE64TOH(x) HAWK_CONST_BSWAP64(x)
#	define HAWK_CONST_LE64TOH(x) (x)
#	endif

#	if defined(HAWK_HAVE_UINT128_T)
#	define HAWK_CONST_NTOH128(x) HAWK_CONST_BSWAP128(x)
#	define HAWK_CONST_HTON128(x) HAWK_CONST_BSWAP128(x)
#	define HAWK_CONST_HTOBE128(x) HAWK_CONST_BSWAP128(x)
#	define HAWK_CONST_HTOLE128(x) (x)
#	define HAWK_CONST_BE128TOH(x) HAWK_CONST_BSWAP128(x)
#	define HAWK_CONST_LE128TOH(x) (x)
#endif

#elif defined(HAWK_ENDIAN_BIG)

#	if defined(HAWK_HAVE_UINT16_T)
#	define HAWK_CONST_NTOH16(x) (x)
#	define HAWK_CONST_HTON16(x) (x)
#	define HAWK_CONST_HTOBE16(x) (x)
#	define HAWK_CONST_HTOLE16(x) HAWK_CONST_BSWAP16(x)
#	define HAWK_CONST_BE16TOH(x) (x)
#	define HAWK_CONST_LE16TOH(x) HAWK_CONST_BSWAP16(x)
#	endif

#	if defined(HAWK_HAVE_UINT32_T)
#	define HAWK_CONST_NTOH32(x) (x)
#	define HAWK_CONST_HTON32(x) (x)
#	define HAWK_CONST_HTOBE32(x) (x)
#	define HAWK_CONST_HTOLE32(x) HAWK_CONST_BSWAP32(x)
#	define HAWK_CONST_BE32TOH(x) (x)
#	define HAWK_CONST_LE32TOH(x) HAWK_CONST_BSWAP32(x)
#	endif

#	if defined(HAWK_HAVE_UINT64_T)
#	define HAWK_CONST_NTOH64(x) (x)
#	define HAWK_CONST_HTON64(x) (x)
#	define HAWK_CONST_HTOBE64(x) (x)
#	define HAWK_CONST_HTOLE64(x) HAWK_CONST_BSWAP64(x)
#	define HAWK_CONST_BE64TOH(x) (x)
#	define HAWK_CONST_LE64TOH(x) HAWK_CONST_BSWAP64(x)
#	endif

#	if defined(HAWK_HAVE_UINT128_T)
#	define HAWK_CONST_NTOH128(x) (x)
#	define HAWK_CONST_HTON128(x) (x)
#	define HAWK_CONST_HTOBE128(x) (x)
#	define HAWK_CONST_HTOLE128(x) HAWK_CONST_BSWAP128(x)
#	define HAWK_CONST_BE128TOH(x) (x)
#	define HAWK_CONST_LE128TOH(x) HAWK_CONST_BSWAP128(x)
#	endif

#else
#	error UNKNOWN ENDIAN
#endif


/* =========================================================================
 * HASH
 * ========================================================================= */
#if (HAWK_SIZEOF_OOW_T == 4)
#	define HAWK_HASH_FNV_MAGIC_INIT (0x811c9dc5)
#	define HAWK_HASH_FNV_MAGIC_PRIME (0x01000193)
#elif (HAWK_SIZEOF_OOW_T == 8)
#	define HAWK_HASH_FNV_MAGIC_INIT (0xCBF29CE484222325)
#	define HAWK_HASH_FNV_MAGIC_PRIME (0x100000001B3l)
#elif (HAWK_SIZEOF_OOW_T == 16)
#	define HAWK_HASH_FNV_MAGIC_INIT (0x6C62272E07BB014262B821756295C58D)
#	define HAWK_HASH_FNV_MAGIC_PRIME (0x1000000000000000000013B)
#endif

#if defined(HAWK_HASH_FNV_MAGIC_INIT)
	/* FNV-1 hash */
#	define HAWK_HASH_INIT HAWK_HASH_FNV_MAGIC_INIT
#	define HAWK_HASH_VALUE(hv,v) (((hv) ^ (v)) * HAWK_HASH_FNV_MAGIC_PRIME)

#else
	/* SDBM hash */
#	define HAWK_HASH_INIT 0
#	define HAWK_HASH_VALUE(hv,v) (((hv) << 6) + ((hv) << 16) - (hv) + (v))
#endif

#define HAWK_HASH_VPTL(hv, ptr, len, type) do { \
	hv = HAWK_HASH_INIT; \
	HAWK_HASH_MORE_VPTL (hv, ptr, len, type); \
} while(0)

#define HAWK_HASH_MORE_VPTL(hv, ptr, len, type) do { \
	type* __hawk_hash_more_vptl_p = (type*)(ptr); \
	type* __hawk_hash_more_vptl_q = (type*)__hawk_hash_more_vptl_p + (len); \
	while (__hawk_hash_more_vptl_p < __hawk_hash_more_vptl_q) \
	{ \
		hv = HAWK_HASH_VALUE(hv, *__hawk_hash_more_vptl_p); \
		__hawk_hash_more_vptl_p++; \
	} \
} while(0)

#define HAWK_HASH_VPTR(hv, ptr, type) do { \
	hv = HAWK_HASH_INIT; \
	HAWK_HASH_MORE_VPTR (hv, ptr, type); \
} while(0)

#define HAWK_HASH_MORE_VPTR(hv, ptr, type) do { \
	type* __hawk_hash_more_vptr_p = (type*)(ptr); \
	while (*__hawk_hash_more_vptr_p) \
	{ \
		hv = HAWK_HASH_VALUE(hv, *__hawk_hash_more_vptr_p); \
		__hawk_hash_more_vptr_p++; \
	} \
} while(0)

#define HAWK_HASH_BYTES(hv, ptr, len) HAWK_HASH_VPTL(hv, ptr, len, const hawk_uint8_t)
#define HAWK_HASH_MORE_BYTES(hv, ptr, len) HAWK_HASH_MORE_VPTL(hv, ptr, len, const hawk_uint8_t)

#define HAWK_HASH_BCHARS(hv, ptr, len) HAWK_HASH_VPTL(hv, ptr, len, const hawk_bch_t)
#define HAWK_HASH_MORE_BCHARS(hv, ptr, len) HAWK_HASH_MORE_VPTL(hv, ptr, len, const hawk_bch_t)

#define HAWK_HASH_UCHARS(hv, ptr, len) HAWK_HASH_VPTL(hv, ptr, len, const hawk_uch_t)
#define HAWK_HASH_MORE_UCHARS(hv, ptr, len) HAWK_HASH_MORE_VPTL(hv, ptr, len, const hawk_uch_t)

#define HAWK_HASH_BCSTR(hv, ptr) HAWK_HASH_VPTR(hv, ptr, const hawk_bch_t)
#define HAWK_HASH_MORE_BCSTR(hv, ptr) HAWK_HASH_MORE_VPTR(hv, ptr, const hawk_bch_t)

#define HAWK_HASH_UCSTR(hv, ptr) HAWK_HASH_VPTR(hv, ptr, const hawk_uch_t)
#define HAWK_HASH_MORE_UCSTR(hv, ptr) HAWK_HASH_MORE_VPTR(hv, ptr, const hawk_uch_t)

/* =========================================================================
 * CMGR
 * ========================================================================= */
enum hawk_cmgr_id_t
{
	HAWK_CMGR_UTF8,
	HAWK_CMGR_UTF16,
	HAWK_CMGR_MB8
};
typedef enum hawk_cmgr_id_t hawk_cmgr_id_t;

/* =========================================================================
 * QUICK SORT
 * ========================================================================= */

/**
 * The hawk_sort_comper_t type defines a sort callback function.
 * The callback function is called by sort functions for each comparison
 * performed. It should return 0 if \a ptr1 and \a ptr2 are
 * euqal, a positive integer if \a ptr1 is greater than \a ptr2, a negative
 * if \a ptr2 is greater than \a ptr1. Both \a ptr1 and \a ptr2 are
 * pointers to any two items in the array. \a ctx which is a pointer to
 * user-defined data passed to a sort function is passed to the callback
 * with no modification.
 */
typedef int (*hawk_sort_comper_t) (
	const void* ptr1,
	const void* ptr2,
	void*       ctx
);

/**
 * The hawk_sort_comperx_t type defines a sort callback function.
 * It should return 0 on success and -1 on failure. the comparsion
 * result must be put back into the variable pointed to by \a cv.
 */
typedef int (*hawk_sort_comperx_t) (
	const void* ptr1,
	const void* ptr2,
	void*       ctx,
	int*        cv
);



/* =========================================================================
 * SUBSTITUTION
 * ========================================================================= */
#define HAWK_SUBST_NOBUF ((void*)1)

/**
 * The hawk_subst_ucs_t type defines a callback function for hawk_subst_for_uchars_to_ucstr()
 * and hawk_subst_for_ucstr_to_ucstr() to substitue a new value for an identifier \a ident.
 */
typedef hawk_uch_t* (*hawk_subst_for_ucs_t) (
	hawk_uch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_ucs_t* ident,
	void*             ctx
);

/**
 * The hawk_subst_bcs_t type defines a callback function for hawk_subst_for_bchars_to_bcstr()
 * and hawk_subst_for_bcstr_to_bcstr() to substitue a new value for an identifier \a ident.
 */
typedef hawk_bch_t* (*hawk_subst_for_bcs_t) (
	hawk_bch_t*       buf,
	hawk_oow_t        bsz,
	const hawk_bcs_t* ident,
	void*             ctx
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_subst_for_oocs_t hawk_subst_for_ucs_t
#else
#	define hawk_subst_for_oocs_t hawk_subst_for_bcs_t
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/* =========================================================================
 * HASH
 * ========================================================================= */
#if 0
HAWK_EXPORT hawk_oow_t hawk_hash_bytes_ (
	const hawk_oob_t* ptr,
	hawk_oow_t        len
);

#if defined(HAWK_HAVE_INLINE)
	static HAWK_INLINE hawk_oow_t hawk_hash_bytes (const hawk_oob_t* ptr, hawk_oow_t len)
	{
		hawk_oow_t hv;
		HAWK_HASH_BYTES (hv, ptr, len);
		/* constrain the hash value to be representable in a small integer
		 * for convenience sake */
		return hv % ((hawk_oow_t)HAWK_SMOOI_MAX + 1);
	}

	static HAWK_INLINE hawk_oow_t hawk_hash_bchars (const hawk_bch_t* ptr, hawk_oow_t len)
	{
		return hawk_hash_bytes((const hawk_oob_t*)ptr, len * HAWK_SIZEOF(hawk_bch_t));
	}

	static HAWK_INLINE hawk_oow_t hawk_hash_uchars (const hawk_uch_t* ptr, hawk_oow_t len)
	{
		return hawk_hash_bytes((const hawk_oob_t*)ptr, len * HAWK_SIZEOF(hawk_uch_t));
	}

	static HAWK_INLINE hawk_oow_t hawk_hash_words (const hawk_oow_t* ptr, hawk_oow_t len)
	{
		return hawk_hash_bytes((const hawk_oob_t*)ptr, len * HAWK_SIZEOF(hawk_oow_t));
	}

	static HAWK_INLINE hawk_oow_t hawk_hash_halfwords (const hawk_oohw_t* ptr, hawk_oow_t len)
	{
		return hawk_hash_bytes((const hawk_oob_t*)ptr, len * HAWK_SIZEOF(hawk_oohw_t));
	}
#else
#	define hawk_hash_bytes(ptr,len)     hawk_hash_bytes_(ptr, len)
#	define hawk_hash_bchars(ptr,len)    hawk_hash_bytes_((const hawk_oob_t*)(ptr), (len) * HAWK_SIZEOF(hawk_bch_t))
#	define hawk_hash_uchars(ptr,len)    hawk_hash_bytes_((const hawk_oob_t*)(ptr), (len) * HAWK_SIZEOF(hawk_uch_t))
#	define hawk_hash_words(ptr,len)     hawk_hash_bytes_((const hawk_oob_t*)(ptr), (len) * HAWK_SIZEOF(hawk_oow_t))
#	define hawk_hash_halfwords(ptr,len) hawk_hash_bytes_((const hawk_oob_t*)(ptr), (len) * HAWK_SIZEOF(hawk_oohw_t))
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_hash_oochars(ptr,len) hawk_hash_uchars(ptr,len)
#else
#	define hawk_hash_oochars(ptr,len) hawk_hash_bchars(ptr,len)
#endif

#endif

/* =========================================================================
 * STRING
 * ========================================================================= */

HAWK_EXPORT hawk_oow_t hawk_subst_for_uchars_to_ucstr (
	hawk_uch_t*           buf,
	hawk_oow_t            bsz,
	const hawk_uch_t*     fmt,
	hawk_oow_t            fsz,
	hawk_subst_for_ucs_t  subst,
	void*                 ctx
);

HAWK_EXPORT hawk_oow_t hawk_subst_for_bchars_to_bcstr (
	hawk_bch_t*           buf,
	hawk_oow_t            bsz,
	const hawk_bch_t*     fmt,
	hawk_oow_t            fsz,
	hawk_subst_for_bcs_t  subst,
	void*                 ctx
);

HAWK_EXPORT hawk_oow_t hawk_subst_for_ucstr_to_ucstr (
	hawk_uch_t*           buf,
	hawk_oow_t            bsz,
	const hawk_uch_t*     fmt,
	hawk_subst_for_ucs_t  subst,
	void*                 ctx
);

HAWK_EXPORT hawk_oow_t hawk_subst_for_bcstr_to_bcstr (
	hawk_bch_t*           buf,
	hawk_oow_t            bsz,
	const hawk_bch_t*     fmt,
	hawk_subst_for_bcs_t  subst,
	void*                 ctx
);

HAWK_EXPORT void hawk_unescape_ucstr (
	hawk_uch_t* str
);

HAWK_EXPORT void hawk_unescape_bcstr (
	hawk_bch_t* str
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_subst_for_oochars_to_oocstr hawk_subst_for_uchars_to_ucstr
#	define hawk_subst_for_oocstr_to_oocstr hawk_subst_for_ucstr_to_ucstr
#	define hawk_unescape_oocstr hawk_unescape_ucstr
#else
#	define hawk_subst_for_oochars_to_oocstr hawk_subst_for_bchars_to_bcstr
#	define hawk_subst_for_oocstr_to_oocstr hawk_subst_for_bcstr_to_bcstr
#	define hawk_unescape_oocstr hawk_unescape_bcstr
#endif

/* ------------------------------------------------------------------------- */

HAWK_EXPORT int hawk_conv_bcstr_to_ucstr_with_cmgr (
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	hawk_cmgr_t*      cmgr,
	int               all
);

HAWK_EXPORT int hawk_conv_ucstr_to_bcstr_with_cmgr (
	const hawk_uch_t* ucs,
	hawk_oow_t*       ucslen,
	hawk_bch_t*       bcs,
	hawk_oow_t*       bcslen,
	hawk_cmgr_t*      cmgr
);

HAWK_EXPORT int hawk_conv_bchars_to_uchars_with_cmgr (
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	hawk_cmgr_t*      cmgr,
	int               all
);

HAWK_EXPORT int hawk_conv_uchars_to_bchars_with_cmgr (
	const hawk_uch_t* ucs,
	hawk_oow_t*       ucslen,
	hawk_bch_t*       bcs,
	hawk_oow_t*       bcslen,
	hawk_cmgr_t*      cmgr
);

HAWK_EXPORT int hawk_conv_bchars_to_uchars_upto_stopper_with_cmgr (
	const hawk_bch_t* bcs,
	hawk_oow_t*       bcslen,
	hawk_uch_t*       ucs,
	hawk_oow_t*       ucslen,
	hawk_uch_t        stopper,
	hawk_cmgr_t*      cmgr
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_int_to_oocstr (
	hawk_int_t         value,
	int                radix,
	const hawk_ooch_t* prefix,
	hawk_ooch_t*       buf,
	hawk_oow_t         size
);

#define HAWK_OOCHARS_TO_INT_MAKE_OPTION(ltrim,rtrim,base) (((!!(ltrim)) << 2) | ((!!(rtrim)) << 3) | ((base) << 8))
#define HAWK_OOCHARS_TO_INT_GET_OPTION_LTRIM(option) ((option) & 4)
#define HAWK_OOCHARS_TO_INT_GET_OPTION_RTRIM(option) ((option) & 8)
#define HAWK_OOCHARS_TO_INT_GET_OPTION_BASE(option) ((option) >> 8)

/**
 * The hawk_uchars_to_int() function converts a wide character string to an integer.
 */
HAWK_EXPORT hawk_int_t hawk_uchars_to_int (
	const hawk_uch_t*    str,
	hawk_oow_t           len,
	int                  option,
	const hawk_uch_t**   endptr,
	int*                 is_sober
);

/**
 * The hawk_bchars_to_int() function converts a multi-byte string to an integer.
 */
HAWK_EXPORT hawk_int_t hawk_bchars_to_int (
	const hawk_bch_t*    str,
	hawk_oow_t           len,
	int                  option,
	const hawk_bch_t**   endptr,
	int*                 is_sober
);

/**
 * The hawk_uchars_to_flt() function converts a wide character string to a floating-point
 * number.
 */
HAWK_EXPORT hawk_flt_t hawk_uchars_to_flt (
	const hawk_uch_t*   str,
	hawk_oow_t          len,
	const hawk_uch_t**  endptr,
	int                 stripspc
);

/**
 * The hawk_bchars_to_flt() function converts a multi-byte string to a floating-point
 * number.
 */
HAWK_EXPORT hawk_flt_t hawk_bchars_to_flt (
	const hawk_bch_t*   str,
	hawk_oow_t          len,
	const hawk_bch_t**  endptr,
	int                 stripspc
);

/**
 * The hawk_oochars_to_num() function converts a string to a number.
 * A numeric string in the valid decimal, hexadecimal(0x), binary(0b),
 * octal(0) notation is converted to an integer and it is stored into
 * memory pointed to by \a l; A string containng '.', 'E', or 'e' is
 * converted to a floating-pointer number and it is stored into memory
 * pointed to by \a r. If \a strict is 0, the function takes up to the last
 * valid character and never fails. If \a strict is 1, an invalid
 * character causes the function to return an error.
 *
 * \return 0 if converted to an integer,
 *         1 if converted to a floating-point number
 *         -1 on error.
 */
#define HAWK_OOCHARS_TO_NUM_MAKE_OPTION(nopartial,reqsober,stripspc,base) (((!!(nopartial)) << 0) | ((!!(reqsober)) << 1) | ((!!(stripspc)) << 2) | ((base) << 8))
#define HAWK_OOCHARS_TO_NUM_GET_OPTION_NOPARTIAL(option) ((option) & 1)
#define HAWK_OOCHARS_TO_NUM_GET_OPTION_REQSOBER(option) ((option) & 2)
#define HAWK_OOCHARS_TO_NUM_GET_OPTION_STRIPSPC(option) ((option) & 4)
#define HAWK_OOCHARS_TO_NUM_GET_OPTION_BASE(option) ((option) >> 8)

HAWK_EXPORT int hawk_bchars_to_num (
	int                option,
	const hawk_bch_t*  ptr, /**< points to a string to convert */
	hawk_oow_t         len, /**< number of characters in a string */
	hawk_int_t*        l,   /**< stores a converted integer */
	hawk_flt_t*        r    /**< stores a converted floating-poing number */
);

HAWK_EXPORT int hawk_uchars_to_num (
	int                option,
	const hawk_uch_t*  ptr, /**< points to a string to convert */
	hawk_oow_t         len, /**< number of characters in a string */
	hawk_int_t*        l,   /**< stores a converted integer */
	hawk_flt_t*        r    /**< stores a converted floating-poing number */
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_oochars_to_int hawk_uchars_to_int
#	define hawk_oochars_to_flt hawk_uchars_to_flt
#	define hawk_oochars_to_num hawk_uchars_to_num
#else
#	define hawk_oochars_to_int hawk_bchars_to_int
#	define hawk_oochars_to_flt hawk_bchars_to_flt
#	define hawk_oochars_to_num hawk_bchars_to_num
#endif

/* ------------------------------------------------------------------------- */

HAWK_EXPORT int hawk_uchars_to_bin (
	const hawk_uch_t* hex,
	hawk_oow_t        hexlen,
	hawk_uint8_t*     buf,
	hawk_oow_t        buflen
);

HAWK_EXPORT int hawk_bchars_to_bin (
	const hawk_bch_t* hex,
	hawk_oow_t        hexlen,
	hawk_uint8_t*     buf,
	hawk_oow_t        buflen
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_oochars_to_bin hawk_uchars_to_bin
#else
#	define hawk_oochars_to_bin hawk_bchars_to_bin
#endif

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_cmgr_t* hawk_get_cmgr_by_id (
	hawk_cmgr_id_t id
);

HAWK_EXPORT hawk_cmgr_t* hawk_get_cmgr_by_bcstr (
	const hawk_bch_t* name
);

HAWK_EXPORT hawk_cmgr_t* hawk_get_cmgr_by_ucstr (
	const hawk_uch_t* name
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_get_cmgr_by_name(name) hawk_get_cmgr_by_ucstr(name)
#else
#	define hawk_get_cmgr_by_name(name) hawk_get_cmgr_by_bcstr(name)
#endif

#define hawk_get_utf8_cmgr() hawk_get_cmgr_by_id(HAWK_CMGR_UTF8)
#define hawk_get_utf16_cmgr() hawk_get_cmgr_by_id(HAWK_CMGR_UTF16)
#define hawk_get_mb8_cmgr() hawk_get_cmgr_by_id(HAWK_CMGR_MB8)

/* ------------------------------------------------------------------------- */

/**
 * The hawk_conv_uchars_to_utf8() function converts a unicode character string \a ucs
 * to a UTF8 string and writes it into the buffer pointed to by \a bcs, but
 * not more than \a bcslen bytes including the terminating null.
 *
 * Upon return, \a bcslen is modified to the actual number of bytes written to
 * \a bcs excluding the terminating null; \a ucslen is modified to the number of
 * wide characters converted.
 *
 * You may pass #HAWK_NULL for \a bcs to dry-run conversion or to get the
 * required buffer size for conversion. -2 is never returned in this case.
 *
 * \return
 * - 0 on full conversion,
 * - -1 on no or partial conversion for an illegal character encountered,
 * - -2 on no or partial conversion for a small buffer.
 *
 * \code
 *   const hawk_uch_t ucs[] = { 'H', 'e', 'l', 'l', 'o' };
 *   hawk_bch_t bcs[10];
 *   hawk_oow_t ucslen = 5;
 *   hawk_oow_t bcslen = HAWK_COUNTOF(bcs);
 *   n = hawk_conv_uchars_to_utf8 (ucs, &ucslen, bcs, &bcslen);
 *   if (n <= -1)
 *   {
 *      // conversion error
 *   }
 * \endcode
 */
HAWK_EXPORT int hawk_conv_uchars_to_utf8 (
	const hawk_uch_t*    ucs,
	hawk_oow_t*          ucslen,
	hawk_bch_t*          bcs,
	hawk_oow_t*          bcslen
);

/**
 * The hawk_conv_utf8_to_uchars() function converts a UTF8 string to a uncide string.
 *
 * It never returns -2 if \a ucs is #HAWK_NULL.
 *
 * \code
 *  const hawk_bch_t* bcs = "test string";
 *  hawk_uch_t ucs[100];
 *  hawk_oow_t ucslen = HAWK_COUNTOF(buf), n;
 *  hawk_oow_t bcslen = 11;
 *  int n;
 *  n = hawk_conv_utf8_to_uchars (bcs, &bcslen, ucs, &ucslen);
 *  if (n <= -1) { invalid/incomplenete sequence or buffer to small }
 * \endcode
 *
 * The resulting \a ucslen can still be greater than 0 even if the return
 * value is negative. The value indiates the number of characters converted
 * before the error has occurred.
 *
 * \return 0 on success.
 *         -1 if \a bcs contains an illegal character.
 *         -2 if the wide-character string buffer is too small.
 *         -3 if \a bcs is not a complete sequence.
 */
HAWK_EXPORT int hawk_conv_utf8_to_uchars (
	const hawk_bch_t*   bcs,
	hawk_oow_t*         bcslen,
	hawk_uch_t*         ucs,
	hawk_oow_t*         ucslen
);


HAWK_EXPORT int hawk_conv_ucstr_to_utf8 (
	const hawk_uch_t*    ucs,
	hawk_oow_t*          ucslen,
	hawk_bch_t*          bcs,
	hawk_oow_t*          bcslen
);

HAWK_EXPORT int hawk_conv_utf8_to_ucs (
	const hawk_bch_t*   bcs,
	hawk_oow_t*         bcslen,
	hawk_uch_t*         ucs,
	hawk_oow_t*         ucslen
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT int hawk_conv_uchars_to_utf16 (
	const hawk_uch_t*    ucs,
	hawk_oow_t*          ucslen,
	hawk_bch_t*          bcs,
	hawk_oow_t*          bcslen
);

HAWK_EXPORT int hawk_conv_utf16_to_uchars (
	const hawk_bch_t*   bcs,
	hawk_oow_t*         bcslen,
	hawk_uch_t*         ucs,
	hawk_oow_t*         ucslen
);


HAWK_EXPORT int hawk_conv_ucstr_to_utf16 (
	const hawk_uch_t*    ucs,
	hawk_oow_t*          ucslen,
	hawk_bch_t*          bcs,
	hawk_oow_t*          bcslen
);

HAWK_EXPORT int hawk_conv_utf16_to_ucs (
	const hawk_bch_t*   bcs,
	hawk_oow_t*         bcslen,
	hawk_uch_t*         ucs,
	hawk_oow_t*         ucslen
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT int hawk_conv_uchars_to_mb8 (
	const hawk_uch_t*    ucs,
	hawk_oow_t*          ucslen,
	hawk_bch_t*          bcs,
	hawk_oow_t*          bcslen
);

HAWK_EXPORT int hawk_conv_mb8_to_uchars (
	const hawk_bch_t*   bcs,
	hawk_oow_t*         bcslen,
	hawk_uch_t*         ucs,
	hawk_oow_t*         ucslen
);

HAWK_EXPORT int hawk_conv_ucstr_to_mb8 (
	const hawk_uch_t*    ucs,
	hawk_oow_t*          ucslen,
	hawk_bch_t*          bcs,
	hawk_oow_t*          bcslen
);

HAWK_EXPORT int hawk_conv_mb8_to_ucs (
	const hawk_bch_t*   bcs,
	hawk_oow_t*         bcslen,
	hawk_uch_t*         ucs,
	hawk_oow_t*         ucslen
);

/* =========================================================================
 * PATH STRING
 * ========================================================================= */
HAWK_EXPORT const hawk_uch_t* hawk_get_base_name_ucstr (
	const hawk_uch_t* path
);

HAWK_EXPORT const hawk_bch_t* hawk_get_base_name_bcstr (
	const hawk_bch_t* path
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_get_base_name_oocstr hawk_get_base_name_ucstr
#else
#	define hawk_get_base_name_oocstr hawk_get_base_name_bcstr
#endif

/* =========================================================================
 * BIT SWAP
 * ========================================================================= */
#if defined(HAWK_HAVE_INLINE)

#if defined(HAWK_HAVE_UINT16_T)
static HAWK_INLINE hawk_uint16_t hawk_bswap16 (hawk_uint16_t x)
{
#if defined(HAWK_HAVE_BUILTIN_BSWAP16)
	return __builtin_bswap16(x);
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	__asm__ /*volatile*/ ("xchgb %b0, %h0" : "=Q"(x): "0"(x));
	return x;
#elif defined(__GNUC__) && defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 6))
	__asm__ /*volatile*/ ("rev16 %0, %0" : "+r"(x));
	return x;
#else
	return (x << 8) | (x >> 8);
#endif
}
#endif

#if defined(HAWK_HAVE_UINT32_T)
static HAWK_INLINE hawk_uint32_t hawk_bswap32 (hawk_uint32_t x)
{
#if defined(HAWK_HAVE_BUILTIN_BSWAP32)
	return __builtin_bswap32(x);
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	__asm__ /*volatile*/ ("bswapl %0" : "=r"(x) : "0"(x));
	return x;
#elif defined(__GNUC__) && defined(__aarch64__)
	__asm__ /*volatile*/ ("rev32 %0, %0" : "+r"(x));
	return x;
#elif defined(__GNUC__) && defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 6))
	__asm__ /*volatile*/ ("rev %0, %0" : "+r"(x));
	return x;
#elif defined(__GNUC__) && defined(__ARM_ARCH)
	hawk_uint32_t tmp;
	__asm__ /*volatile*/ (
		"eor %1, %0, %0, ror #16\n\t"
		"bic %1, %1, #0x00ff0000\n\t"
		"mov %0, %0, ror #8\n\t"
		"eor %0, %0, %1, lsr #8\n\t"
		:"+r"(x), "=&r"(tmp)
	);
	return x;
#else
	return ((x >> 24)) |
	       ((x >>  8) & ((hawk_uint32_t)0xff << 8)) |
	       ((x <<  8) & ((hawk_uint32_t)0xff << 16)) |
	       ((x << 24));
#endif
}
#endif

#if defined(HAWK_HAVE_UINT64_T)
static HAWK_INLINE hawk_uint64_t hawk_bswap64 (hawk_uint64_t x)
{
#if defined(HAWK_HAVE_BUILTIN_BSWAP64)
	return __builtin_bswap64(x);
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64))
	__asm__ /*volatile*/ ("bswapq %0" : "=r"(x) : "0"(x));
	return x;
#elif defined(__GNUC__) && defined(__aarch64__)
	__asm__ /*volatile*/ ("rev %0, %0" : "+r"(x));
	return x;
#else
	return ((x >> 56)) |
	       ((x >> 40) & ((hawk_uint64_t)0xff << 8)) |
	       ((x >> 24) & ((hawk_uint64_t)0xff << 16)) |
	       ((x >>  8) & ((hawk_uint64_t)0xff << 24)) |
	       ((x <<  8) & ((hawk_uint64_t)0xff << 32)) |
	       ((x << 24) & ((hawk_uint64_t)0xff << 40)) |
	       ((x << 40) & ((hawk_uint64_t)0xff << 48)) |
	       ((x << 56));
#endif
}
#endif

#if defined(HAWK_HAVE_UINT128_T)
static HAWK_INLINE hawk_uint128_t hawk_bswap128 (hawk_uint128_t x)
{
#if defined(HAWK_HAVE_BUILTIN_BSWAP128)
	return __builtin_bswap128(x);
#else
	return ((x >> 120)) |
	       ((x >> 104) & ((hawk_uint128_t)0xff << 8)) |
	       ((x >>  88) & ((hawk_uint128_t)0xff << 16)) |
	       ((x >>  72) & ((hawk_uint128_t)0xff << 24)) |
	       ((x >>  56) & ((hawk_uint128_t)0xff << 32)) |
	       ((x >>  40) & ((hawk_uint128_t)0xff << 40)) |
	       ((x >>  24) & ((hawk_uint128_t)0xff << 48)) |
	       ((x >>   8) & ((hawk_uint128_t)0xff << 56)) |
	       ((x <<   8) & ((hawk_uint128_t)0xff << 64)) |
	       ((x <<  24) & ((hawk_uint128_t)0xff << 72)) |
	       ((x <<  40) & ((hawk_uint128_t)0xff << 80)) |
	       ((x <<  56) & ((hawk_uint128_t)0xff << 88)) |
	       ((x <<  72) & ((hawk_uint128_t)0xff << 96)) |
	       ((x <<  88) & ((hawk_uint128_t)0xff << 104)) |
	       ((x << 104) & ((hawk_uint128_t)0xff << 112)) |
	       ((x << 120));
#endif
}
#endif

#else

#if defined(HAWK_HAVE_UINT16_T)
#	if defined(HAWK_HAVE_BUILTIN_BSWAP16)
#	define hawk_bswap16(x) ((hawk_uint16_t)__builtin_bswap16((hawk_uint16_t)(x)))
#	else
#	define hawk_bswap16(x) ((hawk_uint16_t)(((hawk_uint16_t)(x)) << 8) | (((hawk_uint16_t)(x)) >> 8))
#	endif
#endif

#if defined(HAWK_HAVE_UINT32_T)
#	if defined(HAWK_HAVE_BUILTIN_BSWAP32)
#	define hawk_bswap32(x) ((hawk_uint32_t)__builtin_bswap32((hawk_uint32_t)(x)))
#	else
#	define hawk_bswap32(x) ((hawk_uint32_t)(((((hawk_uint32_t)(x)) >> 24)) | \
	                                      ((((hawk_uint32_t)(x)) >>  8) & ((hawk_uint32_t)0xff << 8)) | \
	                                      ((((hawk_uint32_t)(x)) <<  8) & ((hawk_uint32_t)0xff << 16)) | \
	                                      ((((hawk_uint32_t)(x)) << 24))))
#	endif
#endif

#if defined(HAWK_HAVE_UINT64_T)
#	if defined(HAWK_HAVE_BUILTIN_BSWAP64)
#	define hawk_bswap64(x) ((hawk_uint64_t)__builtin_bswap64((hawk_uint64_t)(x)))
#	else
#	define hawk_bswap64(x) ((hawk_uint64_t)(((((hawk_uint64_t)(x)) >> 56)) | \
	                                      ((((hawk_uint64_t)(x)) >> 40) & ((hawk_uint64_t)0xff << 8)) | \
	                                      ((((hawk_uint64_t)(x)) >> 24) & ((hawk_uint64_t)0xff << 16)) | \
	                                      ((((hawk_uint64_t)(x)) >>  8) & ((hawk_uint64_t)0xff << 24)) | \
	                                      ((((hawk_uint64_t)(x)) <<  8) & ((hawk_uint64_t)0xff << 32)) | \
	                                      ((((hawk_uint64_t)(x)) << 24) & ((hawk_uint64_t)0xff << 40)) | \
	                                      ((((hawk_uint64_t)(x)) << 40) & ((hawk_uint64_t)0xff << 48)) | \
	                                      ((((hawk_uint64_t)(x)) << 56))))
#	endif
#endif

#if defined(HAWK_HAVE_UINT128_T)
#	if defined(HAWK_HAVE_BUILTIN_BSWAP128)
#	define hawk_bswap128(x) ((hawk_uint128_t)__builtin_bswap128((hawk_uint128_t)(x)))
#	else
#	define hawk_bswap128(x) ((hawk_uint128_t)(((((hawk_uint128_t)(x)) >> 120)) |  \
	                                        ((((hawk_uint128_t)(x)) >> 104) & ((hawk_uint128_t)0xff << 8)) | \
	                                        ((((hawk_uint128_t)(x)) >>  88) & ((hawk_uint128_t)0xff << 16)) | \
	                                        ((((hawk_uint128_t)(x)) >>  72) & ((hawk_uint128_t)0xff << 24)) | \
	                                        ((((hawk_uint128_t)(x)) >>  56) & ((hawk_uint128_t)0xff << 32)) | \
	                                        ((((hawk_uint128_t)(x)) >>  40) & ((hawk_uint128_t)0xff << 40)) | \
	                                        ((((hawk_uint128_t)(x)) >>  24) & ((hawk_uint128_t)0xff << 48)) | \
	                                        ((((hawk_uint128_t)(x)) >>   8) & ((hawk_uint128_t)0xff << 56)) | \
	                                        ((((hawk_uint128_t)(x)) <<   8) & ((hawk_uint128_t)0xff << 64)) | \
	                                        ((((hawk_uint128_t)(x)) <<  24) & ((hawk_uint128_t)0xff << 72)) | \
	                                        ((((hawk_uint128_t)(x)) <<  40) & ((hawk_uint128_t)0xff << 80)) | \
	                                        ((((hawk_uint128_t)(x)) <<  56) & ((hawk_uint128_t)0xff << 88)) | \
	                                        ((((hawk_uint128_t)(x)) <<  72) & ((hawk_uint128_t)0xff << 96)) | \
	                                        ((((hawk_uint128_t)(x)) <<  88) & ((hawk_uint128_t)0xff << 104)) | \
	                                        ((((hawk_uint128_t)(x)) << 104) & ((hawk_uint128_t)0xff << 112)) | \
	                                        ((((hawk_uint128_t)(x)) << 120))))
#	endif
#endif

#endif /* HAWK_HAVE_INLINE */


#if defined(HAWK_ENDIAN_LITTLE)

#	if defined(HAWK_HAVE_UINT16_T)
#	define hawk_hton16(x) hawk_bswap16(x)
#	define hawk_ntoh16(x) hawk_bswap16(x)
#	define hawk_htobe16(x) hawk_bswap16(x)
#	define hawk_be16toh(x) hawk_bswap16(x)
#	define hawk_htole16(x) ((hawk_uint16_t)(x))
#	define hawk_le16toh(x) ((hawk_uint16_t)(x))
#	endif

#	if defined(HAWK_HAVE_UINT32_T)
#	define hawk_hton32(x) hawk_bswap32(x)
#	define hawk_ntoh32(x) hawk_bswap32(x)
#	define hawk_htobe32(x) hawk_bswap32(x)
#	define hawk_be32toh(x) hawk_bswap32(x)
#	define hawk_htole32(x) ((hawk_uint32_t)(x))
#	define hawk_le32toh(x) ((hawk_uint32_t)(x))
#	endif

#	if defined(HAWK_HAVE_UINT64_T)
#	define hawk_hton64(x) hawk_bswap64(x)
#	define hawk_ntoh64(x) hawk_bswap64(x)
#	define hawk_htobe64(x) hawk_bswap64(x)
#	define hawk_be64toh(x) hawk_bswap64(x)
#	define hawk_htole64(x) ((hawk_uint64_t)(x))
#	define hawk_le64toh(x) ((hawk_uint64_t)(x))
#	endif

#	if defined(HAWK_HAVE_UINT128_T)

#	define hawk_hton128(x) hawk_bswap128(x)
#	define hawk_ntoh128(x) hawk_bswap128(x)
#	define hawk_htobe128(x) hawk_bswap128(x)
#	define hawk_be128toh(x) hawk_bswap128(x)
#	define hawk_htole128(x) ((hawk_uint128_t)(x))
#	define hawk_le128toh(x) ((hawk_uint128_t)(x))
#	endif

#elif defined(HAWK_ENDIAN_BIG)

#	if defined(HAWK_HAVE_UINT16_T)
#	define hawk_hton16(x) ((hawk_uint16_t)(x))
#	define hawk_ntoh16(x) ((hawk_uint16_t)(x))
#	define hawk_htobe16(x) ((hawk_uint16_t)(x))
#	define hawk_be16toh(x) ((hawk_uint16_t)(x))
#	define hawk_htole16(x) hawk_bswap16(x)
#	define hawk_le16toh(x) hawk_bswap16(x)
#	endif

#	if defined(HAWK_HAVE_UINT32_T)
#	define hawk_hton32(x) ((hawk_uint32_t)(x))
#	define hawk_ntoh32(x) ((hawk_uint32_t)(x))
#	define hawk_htobe32(x) ((hawk_uint32_t)(x))
#	define hawk_be32toh(x) ((hawk_uint32_t)(x))
#	define hawk_htole32(x) hawk_bswap32(x)
#	define hawk_le32toh(x) hawk_bswap32(x)
#	endif

#	if defined(HAWK_HAVE_UINT64_T)
#	define hawk_hton64(x) ((hawk_uint64_t)(x))
#	define hawk_ntoh64(x) ((hawk_uint64_t)(x))
#	define hawk_htobe64(x) ((hawk_uint64_t)(x))
#	define hawk_be64toh(x) ((hawk_uint64_t)(x))
#	define hawk_htole64(x) hawk_bswap64(x)
#	define hawk_le64toh(x) hawk_bswap64(x)
#	endif

#	if defined(HAWK_HAVE_UINT128_T)
#	define hawk_hton128(x) ((hawk_uint128_t)(x))
#	define hawk_ntoh128(x) ((hawk_uint128_t)(x))
#	define hawk_htobe128(x) ((hawk_uint128_t)(x))
#	define hawk_be128toh(x) ((hawk_uint128_t)(x))
#	define hawk_htole128(x) hawk_bswap128(x)
#	define hawk_le128toh(x) hawk_bswap128(x)
#	endif

#else
#	error UNKNOWN ENDIAN
#endif

/* =========================================================================
 * BIT POSITION
 * ========================================================================= */
static HAWK_INLINE int hawk_get_pos_of_msb_set_pow2 (hawk_oow_t x)
{
	/* the caller must ensure that x is power of 2. if x happens to be zero,
	 * the return value is undefined as each method used may give different result. */
#if defined(HAWK_HAVE_BUILTIN_CTZLL) && (HAWK_SIZEOF_OOW_T == HAWK_SIZEOF_LONG_LONG)
	return __builtin_ctzll(x); /* count the number of trailing zeros */
#elif defined(HAWK_HAVE_BUILTIN_CTZL) && (HAWK_SIZEOF_OOW_T == HAWK_SIZEOF_LONG)
	return __builtin_ctzl(x); /* count the number of trailing zeros */
#elif defined(HAWK_HAVE_BUILTIN_CTZ) && (HAWK_SIZEOF_OOW_T == HAWK_SIZEOF_INT)
	return __builtin_ctz(x); /* count the number of trailing zeros */
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	hawk_oow_t pos;
	/* use the Bit Scan Forward instruction */
#if 1
	__asm__ volatile (
		"bsf %1,%0\n\t"
		: "=r"(pos) /* output */
		: "r"(x) /* input */
	);
#else
	__asm__ volatile (
		"bsf %[X],%[EXP]\n\t"
		: [EXP]"=r"(pos) /* output */
		: [X]"r"(x) /* input */
	);
#endif
	return (int)pos;
#elif defined(__GNUC__) && defined(__aarch64__) || (defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 5)))
	hawk_oow_t n;
	/* CLZ is available in ARMv5T and above. there is no instruction to
	 * count trailing zeros or something similar. using RBIT with CLZ
	 * would be good in ARMv6T2 and above to avoid further calculation
	 * afte CLZ */
	__asm__ volatile (
		"clz %0,%1\n\t"
		: "=r"(n) /* output */
		: "r"(x) /* input */
	);
	return (int)(HAWK_OOW_BITS - n - 1);
	/* TODO: PPC - use cntlz, cntlzw, cntlzd, SPARC - use lzcnt, MIPS clz */
#else
	int pos = 0;
	while (x >>= 1) pos++;
	return pos;
#endif
}

static HAWK_INLINE int hawk_get_pos_of_msb_set (hawk_oow_t x)
{
	/* x doesn't have to be power of 2. if x is zero, the result is undefined */
#if defined(HAWK_HAVE_BUILTIN_CLZLL) && (HAWK_SIZEOF_OOW_T == HAWK_SIZEOF_LONG_LONG)
	return HAWK_OOW_BITS - __builtin_clzll(x) - 1; /* count the number of leading zeros */
#elif defined(HAWK_HAVE_BUILTIN_CLZL) && (HAWK_SIZEOF_OOW_T == HAWK_SIZEOF_LONG)
	return HAWK_OOW_BITS - __builtin_clzl(x) - 1; /* count the number of leading zeros */
#elif defined(HAWK_HAVE_BUILTIN_CLZ) && (HAWK_SIZEOF_OOW_T == HAWK_SIZEOF_INT)
	return HAWK_OOW_BITS - __builtin_clz(x) - 1; /* count the number of leading zeros */
#elif defined(__GNUC__) && (defined(__x86_64) || defined(__amd64) || defined(__i386) || defined(i386))
	/* bit scan reverse. not all x86 CPUs have LZCNT. */
	hawk_oow_t pos;
	__asm__ volatile (
		"bsr %1,%0\n\t"
		: "=r"(pos) /* output */
		: "r"(x) /* input */
	);
	return (int)pos;
#elif defined(__GNUC__) && defined(__aarch64__) || (defined(__arm__) && (defined(__ARM_ARCH) && (__ARM_ARCH >= 5)))
	hawk_oow_t n;
	__asm__ volatile (
		"clz %0,%1\n\t"
		: "=r"(n) /* output */
		: "r"(x) /* input */
	);
	return (int)(HAWK_OOW_BITS - n - 1);
	/* TODO: PPC - use cntlz, cntlzw, cntlzd, SPARC - use lzcnt, MIPS clz */
#else
	int pos = 0;
	while (x >>= 1) pos++;
	return pos;
#endif
}

/* =========================================================================
 * QUICK SORT
 * ========================================================================= */

HAWK_EXPORT void hawk_qsort (
	void*              base,
	hawk_oow_t         nmemb,
	hawk_oow_t         size,
	hawk_sort_comper_t comper,
	void*              ctx
);

HAWK_EXPORT int hawk_qsortx (
	void*               base,
	hawk_oow_t          nmemb,
	hawk_oow_t          size,
	hawk_sort_comperx_t comper,
	void*               ctx
);

/* =========================================================================
 * TIME
 * ========================================================================= */
HAWK_EXPORT int hawk_get_ntime (
	hawk_ntime_t* t
);

HAWK_EXPORT int hawk_set_ntime (
	const hawk_ntime_t* t
);

/**
 * The hawk_add_ntime() adds two time structures pointed to by x and y and
 * stores the result in z. Upon overflow, it sets z to the largest value
 * the hawk_ntime_t type can represent. Upon underflow, it sets z to the
 * smallest value. If you don't need this extra range check, you may use
 * the HAWK_ADD_NTIME() macro.
 */
HAWK_EXPORT void hawk_add_ntime (
	hawk_ntime_t*       z,
	const hawk_ntime_t* x,
	const hawk_ntime_t* y
);

HAWK_EXPORT void hawk_sub_ntime (
	hawk_ntime_t*       z,
	const hawk_ntime_t* x,
	const hawk_ntime_t* y
);

/* =========================================================================
 * RANDOM NUMBER GENERATOR
 * ========================================================================= */
HAWK_EXPORT hawk_uint32_t hawk_rand31 (
	hawk_uint32_t seed
);

/* =========================================================================
 * ASSERTION
 * ========================================================================= */
HAWK_EXPORT void hawk_assert_fail (
	const hawk_bch_t* expr,
	const hawk_bch_t* file,
	hawk_oow_t line
);

#if defined(__cplusplus)
}
#endif

#endif
