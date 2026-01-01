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

#ifndef _HAWK_FMT_H_
#define _HAWK_FMT_H_

#include <hawk-cmn.h>
#include <stdarg.h>

/** \file
 * This file defines various formatting functions.
 */

/**
 * The hawk_fmt_intmax_flag_t type defines enumerators to change the
 * behavior of hawk_fmt_intmax() and hawk_fmt_uintmax().
 */
enum hawk_fmt_intmax_flag_t
{
	/* Use lower 6 bits to represent base between 2 and 36 inclusive.
	 * Upper bits are used for these flag options */

	/** Don't truncate if the buffer is not large enough */
	HAWK_FMT_INTMAX_NOTRUNC = (0x40 << 0),
#define HAWK_FMT_INTMAX_NOTRUNC             HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_UINTMAX_NOTRUNC            HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_INTMAX_TO_BCSTR_NOTRUNC    HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_UINTMAX_TO_BCSTR_NOTRUNC   HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_INTMAX_TO_UCSTR_NOTRUNC    HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_UINTMAX_TO_UCSTR_NOTRUNC   HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_INTMAX_TO_OOCSTR_NOTRUNC   HAWK_FMT_INTMAX_NOTRUNC
#define HAWK_FMT_UINTMAX_TO_OOCSTR_NOTRUNC  HAWK_FMT_INTMAX_NOTRUNC

	/** Don't append a terminating null */
	HAWK_FMT_INTMAX_NONULL = (0x40 << 1),
#define HAWK_FMT_INTMAX_NONULL             HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_UINTMAX_NONULL            HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_INTMAX_TO_BCSTR_NONULL    HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_UINTMAX_TO_BCSTR_NONULL   HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_INTMAX_TO_UCSTR_NONULL    HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_UINTMAX_TO_UCSTR_NONULL   HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_INTMAX_TO_OOCSTR_NONULL   HAWK_FMT_INTMAX_NONULL
#define HAWK_FMT_UINTMAX_TO_OOCSTR_NONULL  HAWK_FMT_INTMAX_NONULL

	/** Produce no digit for a value of zero  */
	HAWK_FMT_INTMAX_NOZERO = (0x40 << 2),
#define HAWK_FMT_INTMAX_NOZERO             HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_UINTMAX_NOZERO            HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_INTMAX_TO_BCSTR_NOZERO    HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_UINTMAX_TO_BCSTR_NOZERO   HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_INTMAX_TO_UCSTR_NOZERO    HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_UINTMAX_TO_UCSTR_NOZERO   HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_INTMAX_TO_OOCSTR_NOZERO   HAWK_FMT_INTMAX_NOZERO
#define HAWK_FMT_UINTMAX_TO_OOCSTR_NOZERO  HAWK_FMT_INTMAX_NOZERO

	/** Produce a leading zero for a non-zero value */
	HAWK_FMT_INTMAX_ZEROLEAD = (0x40 << 3),
#define HAWK_FMT_INTMAX_ZEROLEAD             HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_UINTMAX_ZEROLEAD            HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_INTMAX_TO_BCSTR_ZEROLEAD    HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_UINTMAX_TO_BCSTR_ZEROLEAD   HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_INTMAX_TO_UCSTR_ZEROLEAD    HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_UINTMAX_TO_UCSTR_ZEROLEAD   HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_INTMAX_TO_OOCSTR_ZEROLEAD   HAWK_FMT_INTMAX_ZEROLEAD
#define HAWK_FMT_UINTMAX_TO_OOCSTR_ZEROLEAD  HAWK_FMT_INTMAX_ZEROLEAD

	/** Use uppercase letters for alphabetic digits */
	HAWK_FMT_INTMAX_UPPERCASE = (0x40 << 4),
#define HAWK_FMT_INTMAX_UPPERCASE             HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_UINTMAX_UPPERCASE            HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_INTMAX_TO_BCSTR_UPPERCASE    HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_UINTMAX_TO_BCSTR_UPPERCASE   HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_INTMAX_TO_UCSTR_UPPERCASE    HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_UINTMAX_TO_UCSTR_UPPERCASE   HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_INTMAX_TO_OOCSTR_UPPERCASE   HAWK_FMT_INTMAX_UPPERCASE
#define HAWK_FMT_UINTMAX_TO_OOCSTR_UPPERCASE  HAWK_FMT_INTMAX_UPPERCASE

	/** Insert a plus sign for a positive integer including 0 */
	HAWK_FMT_INTMAX_PLUSSIGN = (0x40 << 5),
#define HAWK_FMT_INTMAX_PLUSSIGN             HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_UINTMAX_PLUSSIGN            HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_INTMAX_TO_BCSTR_PLUSSIGN    HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_UINTMAX_TO_BCSTR_PLUSSIGN   HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_INTMAX_TO_UCSTR_PLUSSIGN    HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_UINTMAX_TO_UCSTR_PLUSSIGN   HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_INTMAX_TO_OOCSTR_PLUSSIGN   HAWK_FMT_INTMAX_PLUSSIGN
#define HAWK_FMT_UINTMAX_TO_OOCSTR_PLUSSIGN  HAWK_FMT_INTMAX_PLUSSIGN

	/** Insert a space for a positive integer including 0 */
	HAWK_FMT_INTMAX_EMPTYSIGN = (0x40 << 6),
#define HAWK_FMT_INTMAX_EMPTYSIGN             HAWK_FMT_INTMAX_EMPTYSIGN
#define HAWK_FMT_UINTMAX_EMPTYSIGN            HAWK_FMT_INTMAX_EMPTYSIGN
#define HAWK_FMT_INTMAX_TO_BCSTR_EMPTYSIGN    HAWK_FMT_INTMAX_EMPTYSIGN
#define HAWK_FMT_UINTMAX_TO_BCSTR_EMPTYSIGN   HAWK_FMT_INTMAX_EMPTYSIGN
#define HAWK_FMT_INTMAX_TO_UCSTR_EMPTYSIGN    HAWK_FMT_INTMAX_EMPTYSIGN
#define HAWK_FMT_UINTMAX_TO_UCSTR_EMPTYSIGN   HAWK_FMT_INTMAX_EMPTYSIGN

	/** Fill the right part of the string */
	HAWK_FMT_INTMAX_FILLRIGHT = (0x40 << 7),
#define HAWK_FMT_INTMAX_FILLRIGHT             HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_UINTMAX_FILLRIGHT            HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_INTMAX_TO_BCSTR_FILLRIGHT    HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_UINTMAX_TO_BCSTR_FILLRIGHT   HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_INTMAX_TO_UCSTR_FILLRIGHT    HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_UINTMAX_TO_UCSTR_FILLRIGHT   HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_INTMAX_TO_OOCSTR_FILLRIGHT   HAWK_FMT_INTMAX_FILLRIGHT
#define HAWK_FMT_UINTMAX_TO_OOCSTR_FILLRIGHT  HAWK_FMT_INTMAX_FILLRIGHT

	/** Fill between the sign chacter and the digit part */
	HAWK_FMT_INTMAX_FILLCENTER = (0x40 << 8)
#define HAWK_FMT_INTMAX_FILLCENTER             HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_UINTMAX_FILLCENTER            HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_INTMAX_TO_BCSTR_FILLCENTER    HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_UINTMAX_TO_BCSTR_FILLCENTER   HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_INTMAX_TO_UCSTR_FILLCENTER    HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_UINTMAX_TO_UCSTR_FILLCENTER   HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_INTMAX_TO_OOCSTR_FILLCENTER   HAWK_FMT_INTMAX_FILLCENTER
#define HAWK_FMT_UINTMAX_TO_OOCSTR_FILLCENTER  HAWK_FMT_INTMAX_FILLCENTER
};

/* =========================================================================
 * FORMATTED OUTPUT
 * ========================================================================= */
typedef struct hawk_fmtout_t hawk_fmtout_t;

typedef int (*hawk_fmtout_putbchars_t) (
	hawk_fmtout_t*     fmtout,
	const hawk_bch_t*  ptr,
	hawk_oow_t         len
);

typedef int (*hawk_fmtout_putuchars_t) (
	hawk_fmtout_t*     fmtout,
	const hawk_uch_t*  ptr,
	hawk_oow_t         len
);

typedef int (*hawk_fmtout_putval_t) (
	hawk_fmtout_t*     fmtout,
	const hawk_val_t*  val
);

enum hawk_fmtout_fmt_type_t
{
	HAWK_FMTOUT_FMT_TYPE_BCH = 0,
	HAWK_FMTOUT_FMT_TYPE_UCH
};
typedef enum hawk_fmtout_fmt_type_t hawk_fmtout_fmt_type_t;

struct hawk_fmtout_t
{
	hawk_oow_t              count; /* out */

	hawk_mmgr_t*            mmgr; /* in */
	hawk_fmtout_putbchars_t putbchars; /* in */
	hawk_fmtout_putuchars_t putuchars; /* in */
	hawk_fmtout_putval_t    putval;  /* in - %v is not handled if it's not set. */
	hawk_bitmask_t          mask;   /* in */
	void*                   ctx;    /* in */

	/* internally set a input */
	hawk_fmtout_fmt_type_t  fmt_type;
	const void*             fmt_str;
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_fmt_intmax_to_bcstr() function formats an integer \a value to a
 * multibyte string according to the given base and writes it to a buffer
 * pointed to by \a buf. It writes to the buffer at most \a size characters
 * including the terminating null. The base must be between 2 and 36 inclusive
 * and can be ORed with zero or more #hawk_fmt_intmax_to_bcstr_flag_t enumerators.
 * This ORed value is passed to the function via the \a base_and_flags
 * parameter. If the formatted string is shorter than \a bufsize, the redundant
 * slots are filled with the fill character \a fillchar if it is not a null
 * character. The filling behavior is determined by the flags shown below:
 *
 * - If #HAWK_FMT_INTMAX_TO_BCSTR_FILLRIGHT is set in \a base_and_flags, slots
 *   after the formatting string are filled.
 * - If #HAWK_FMT_INTMAX_TO_BCSTR_FILLCENTER is set in \a base_and_flags, slots
 *   before the formatting string are filled. However, if it contains the
 *   sign character, the slots between the sign character and the digit part
 *   are filled.
 * - If neither #HAWK_FMT_INTMAX_TO_BCSTR_FILLRIGHT nor #HAWK_FMT_INTMAX_TO_BCSTR_FILLCENTER
 *   , slots before the formatting string are filled.
 *
 * The \a precision parameter specified the minimum number of digits to
 * produce from the \a value. If \a value produces fewer digits than
 * \a precision, the actual digits are padded with '0' to meet the precision
 * requirement. You can pass a negative number if you don't wish to specify
 * precision.
 *
 * The terminating null is not added if #HAWK_FMT_INTMAX_TO_BCSTR_NONULL is set;
 * The #HAWK_FMT_INTMAX_TO_BCSTR_UPPERCASE flag indicates that the function should
 * use the uppercase letter for a alphabetic digit;
 * You can set #HAWK_FMT_INTMAX_TO_BCSTR_NOTRUNC if you require lossless formatting.
 * The #HAWK_FMT_INTMAX_TO_BCSTR_PLUSSIGN flag and #HAWK_FMT_INTMAX_TO_BCSTR_EMPTYSIGN
 * ensures that the plus sign and a space is added for a positive integer
 * including 0 respectively.
 * The #HAWK_FMT_INTMAX_TO_BCSTR_ZEROLEAD flag ensures that the numeric string
 * begins with '0' before applying the prefix.
 * You can set the #HAWK_FMT_INTMAX_TO_BCSTR_NOZERO flag if you want the value of
 * 0 to produce nothing. If both #HAWK_FMT_INTMAX_TO_BCSTR_NOZERO and
 * #HAWK_FMT_INTMAX_TO_BCSTR_ZEROLEAD are specified, '0' is still produced.
 *
 * If \a prefix is not #HAWK_NULL, it is inserted before the digits.
 *
 * \return
 *  - -1 if the base is not between 2 and 36 inclusive.
 *  - negated number of characters required for lossless formatting
 *   - if \a bufsize is 0.
 *   - if #HAWK_FMT_INTMAX_TO_BCSTR_NOTRUNC is set and \a bufsize is less than
 *     the minimum required for lossless formatting.
 *  - number of characters written to the buffer excluding a terminating
 *    null in all other cases.
 */
HAWK_EXPORT int hawk_fmt_intmax_to_bcstr (
	hawk_bch_t*        buf,             /**< buffer pointer */
	int                bufsize,         /**< buffer size */
	hawk_intmax_t      value,           /**< integer to format */
	int                base_and_flags,  /**< base ORed with flags */
	int                precision,       /**< precision */
	hawk_bch_t         fillchar,        /**< fill character */
	const hawk_bch_t*  prefix           /**< prefix */
);

/**
 * The hawk_fmt_intmax_to_ucstr() function formats an integer \a value to a
 * wide-character string according to the given base and writes it to a buffer
 * pointed to by \a buf. It writes to the buffer at most \a size characters
 * including the terminating null. The base must be between 2 and 36 inclusive
 * and can be ORed with zero or more #hawk_fmt_intmax_to_ucstr_flag_t enumerators.
 * This ORed value is passed to the function via the \a base_and_flags
 * parameter. If the formatted string is shorter than \a bufsize, the redundant
 * slots are filled with the fill character \a fillchar if it is not a null
 * character. The filling behavior is determined by the flags shown below:
 *
 * - If #HAWK_FMT_INTMAX_TO_UCSTR_FILLRIGHT is set in \a base_and_flags, slots
 *   after the formatting string are filled.
 * - If #HAWK_FMT_INTMAX_TO_UCSTR_FILLCENTER is set in \a base_and_flags, slots
 *   before the formatting string are filled. However, if it contains the
 *   sign character, the slots between the sign character and the digit part
 *   are filled.
 * - If neither #HAWK_FMT_INTMAX_TO_UCSTR_FILLRIGHT nor #HAWK_FMT_INTMAX_TO_UCSTR_FILLCENTER
 *   , slots before the formatting string are filled.
 *
 * The \a precision parameter specified the minimum number of digits to
 * produce from the \ value. If \a value produces fewer digits than
 * \a precision, the actual digits are padded with '0' to meet the precision
 * requirement. You can pass a negative number if don't wish to specify
 * precision.
 *
 * The terminating null is not added if #HAWK_FMT_INTMAX_TO_UCSTR_NONULL is set;
 * The #HAWK_FMT_INTMAX_TO_UCSTR_UPPERCASE flag indicates that the function should
 * use the uppercase letter for a alphabetic digit;
 * You can set #HAWK_FMT_INTMAX_TO_UCSTR_NOTRUNC if you require lossless formatting.
 * The #HAWK_FMT_INTMAX_TO_UCSTR_PLUSSIGN flag and #HAWK_FMT_INTMAX_TO_UCSTR_EMPTYSIGN
 * ensures that the plus sign and a space is added for a positive integer
 * including 0 respectively.
 * The #HAWK_FMT_INTMAX_TO_UCSTR_ZEROLEAD flag ensures that the numeric string
 * begins with 0 before applying the prefix.
 * You can set the #HAWK_FMT_INTMAX_TO_UCSTR_NOZERO flag if you want the value of
 * 0 to produce nothing. If both #HAWK_FMT_INTMAX_TO_UCSTR_NOZERO and
 * #HAWK_FMT_INTMAX_TO_UCSTR_ZEROLEAD are specified, '0' is still produced.
 *
 * If \a prefix is not #HAWK_NULL, it is inserted before the digits.
 *
 * \return
 *  - -1 if the base is not between 2 and 36 inclusive.
 *  - negated number of characters required for lossless formatting
 *   - if \a bufsize is 0.
 *   - if #HAWK_FMT_INTMAX_TO_UCSTR_NOTRUNC is set and \a bufsize is less than
 *     the minimum required for lossless formatting.
 *  - number of characters written to the buffer excluding a terminating
 *    null in all other cases.
 */
HAWK_EXPORT int hawk_fmt_intmax_to_ucstr (
	hawk_uch_t*       buf,             /**< buffer pointer */
	int               bufsize,         /**< buffer size */
	hawk_intmax_t     value,           /**< integer to format */
	int               base_and_flags,  /**< base ORed with flags */
	int               precision,       /**< precision */
	hawk_uch_t        fillchar,        /**< fill character */
	const hawk_uch_t* prefix           /**< prefix */
);


/**
 * The hawk_fmt_uintmax_to_bcstr() function formats an unsigned integer \a value
 * to a multibyte string buffer. It behaves the same as hawk_fmt_intmax_to_bcstr()
 * except that it handles an unsigned integer.
 */
HAWK_EXPORT int hawk_fmt_uintmax_to_bcstr (
	hawk_bch_t*       buf,             /**< buffer pointer */
	int               bufsize,         /**< buffer size */
	hawk_uintmax_t    value,           /**< integer to format */
	int               base_and_flags,  /**< base ORed with flags */
	int               precision,       /**< precision */
	hawk_bch_t        fillchar,        /**< fill character */
	const hawk_bch_t* prefix           /**< prefix */
);

/**
 * The hawk_fmt_uintmax_to_ucstr() function formats an unsigned integer \a value
 * to a unicode string buffer. It behaves the same as hawk_fmt_intmax_to_ucstr()
 * except that it handles an unsigned integer.
 */
HAWK_EXPORT int hawk_fmt_uintmax_to_ucstr (
	hawk_uch_t*       buf,             /**< buffer pointer */
	int               bufsize,         /**< buffer size */
	hawk_uintmax_t    value,           /**< integer to format */
	int               base_and_flags,  /**< base ORed with flags */
	int               precision,       /**< precision */
	hawk_uch_t        fillchar,        /**< fill character */
	const hawk_uch_t* prefix           /**< prefix */
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_fmt_intmax_to_oocstr hawk_fmt_intmax_to_bcstr
#	define hawk_fmt_uintmax_to_oocstr hawk_fmt_uintmax_to_bcstr
#else
#	define hawk_fmt_intmax_to_oocstr hawk_fmt_intmax_to_ucstr
#	define hawk_fmt_uintmax_to_oocstr hawk_fmt_uintmax_to_ucstr
#endif

/* TODO: hawk_fmt_fltmax_to_bcstr()... hawk_fmt_fltmax_to_ucstr() */

/* =========================================================================
 * FORMATTED OUTPUT
 * ========================================================================= */
HAWK_EXPORT int hawk_bfmt_outv (
	hawk_fmtout_t*    fmtout,
	const hawk_bch_t* fmt,
	va_list           ap
);

HAWK_EXPORT int hawk_ufmt_outv (
	hawk_fmtout_t*    fmtout,
	const hawk_uch_t* fmt,
	va_list           ap
);


HAWK_EXPORT int hawk_bfmt_out (
	hawk_fmtout_t*    fmtout,
	const hawk_bch_t* fmt,
	...
);

HAWK_EXPORT int hawk_ufmt_out (
	hawk_fmtout_t*    fmtout,
	const hawk_uch_t* fmt,
	...
);

#if defined(__cplusplus)
}
#endif

#endif
