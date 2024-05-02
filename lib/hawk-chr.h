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

#ifndef _HAWK_CHR_H_
#define _HAWK_CHR_H_

#include <hawk-cmn.h>

enum hawk_ooch_prop_t
{
	HAWK_OOCH_PROP_UPPER  = (1 << 0),
#define HAWK_UCH_PROP_UPPER HAWK_OOCH_PROP_UPPER
#define HAWK_BCH_PROP_UPPER HAWK_OOCH_PROP_UPPER

	HAWK_OOCH_PROP_LOWER  = (1 << 1),
#define HAWK_UCH_PROP_LOWER HAWK_OOCH_PROP_LOWER
#define HAWK_BCH_PROP_LOWER HAWK_OOCH_PROP_LOWER

	HAWK_OOCH_PROP_ALPHA  = (1 << 2),
#define HAWK_UCH_PROP_ALPHA HAWK_OOCH_PROP_ALPHA
#define HAWK_BCH_PROP_ALPHA HAWK_OOCH_PROP_ALPHA

	HAWK_OOCH_PROP_DIGIT  = (1 << 3),
#define HAWK_UCH_PROP_DIGIT HAWK_OOCH_PROP_DIGIT
#define HAWK_BCH_PROP_DIGIT HAWK_OOCH_PROP_DIGIT

	HAWK_OOCH_PROP_XDIGIT = (1 << 4),
#define HAWK_UCH_PROP_XDIGIT HAWK_OOCH_PROP_XDIGIT
#define HAWK_BCH_PROP_XDIGIT HAWK_OOCH_PROP_XDIGIT

	HAWK_OOCH_PROP_ALNUM  = (1 << 5),
#define HAWK_UCH_PROP_ALNUM HAWK_OOCH_PROP_ALNUM
#define HAWK_BCH_PROP_ALNUM HAWK_OOCH_PROP_ALNUM

	HAWK_OOCH_PROP_SPACE  = (1 << 6),
#define HAWK_UCH_PROP_SPACE HAWK_OOCH_PROP_SPACE
#define HAWK_BCH_PROP_SPACE HAWK_OOCH_PROP_SPACE

	HAWK_OOCH_PROP_PRINT  = (1 << 8),
#define HAWK_UCH_PROP_PRINT HAWK_OOCH_PROP_PRINT
#define HAWK_BCH_PROP_PRINT HAWK_OOCH_PROP_PRINT

	HAWK_OOCH_PROP_GRAPH  = (1 << 9),
#define HAWK_UCH_PROP_GRAPH HAWK_OOCH_PROP_GRAPH
#define HAWK_BCH_PROP_GRAPH HAWK_OOCH_PROP_GRAPH

	HAWK_OOCH_PROP_CNTRL  = (1 << 10),
#define HAWK_UCH_PROP_CNTRL HAWK_OOCH_PROP_CNTRL
#define HAWK_BCH_PROP_CNTRL HAWK_OOCH_PROP_CNTRL

	HAWK_OOCH_PROP_PUNCT  = (1 << 11),
#define HAWK_UCH_PROP_PUNCT HAWK_OOCH_PROP_PUNCT
#define HAWK_BCH_PROP_PUNCT HAWK_OOCH_PROP_PUNCT

	HAWK_OOCH_PROP_BLANK  = (1 << 12)
#define HAWK_UCH_PROP_BLANK HAWK_OOCH_PROP_BLANK
#define HAWK_BCH_PROP_BLANK HAWK_OOCH_PROP_BLANK
};

typedef enum hawk_ooch_prop_t hawk_ooch_prop_t;
typedef enum hawk_ooch_prop_t hawk_uch_prop_t;
typedef enum hawk_ooch_prop_t hawk_bch_prop_t;

#define HAWK_DIGIT_TO_NUM(c) (((c) >= '0' && (c) <= '9')? ((c) - '0'): -1)

#define HAWK_XDIGIT_TO_NUM(c) \
	(((c) >= '0' && (c) <= '9')? ((c) - '0'): \
	 ((c) >= 'A' && (c) <= 'F')? ((c) - 'A' + 10): \
	 ((c) >= 'a' && (c) <= 'f')? ((c) - 'a' + 10): -1)

#define HAWK_ZDIGIT_TO_NUM(c,base) \
	(((c) >= '0' && (c) <= '9')? ((c) - '0'): \
	 ((c) >= 'A' && (c) <= 'Z')? ((c) - 'A' + 10): \
	 ((c) >= 'a' && (c) <= 'z')? ((c) - 'a' + 10): base)

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT int hawk_is_uch_type (hawk_uch_t c, hawk_uch_prop_t type);
HAWK_EXPORT int hawk_is_uch_upper (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_lower (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_alpha (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_digit (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_xdigit (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_alnum (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_space (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_print (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_graph (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_cntrl (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_punct (hawk_uch_t c);
HAWK_EXPORT int hawk_is_uch_blank (hawk_uch_t c);
HAWK_EXPORT hawk_uch_t hawk_to_uch_upper (hawk_uch_t c);
HAWK_EXPORT hawk_uch_t hawk_to_uch_lower (hawk_uch_t c);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT int hawk_is_bch_type (hawk_bch_t c, hawk_bch_prop_t type);

#if defined(__has_builtin)
#	if __has_builtin(__builtin_isupper)
#		define hawk_is_bch_upper __builtin_isupper
#	endif
#	if __has_builtin(__builtin_islower)
#		define hawk_is_bch_lower __builtin_islower
#	endif
#	if __has_builtin(__builtin_isalpha)
#		define hawk_is_bch_alpha __builtin_isalpha
#	endif
#	if __has_builtin(__builtin_isdigit)
#		define hawk_is_bch_digit __builtin_isdigit
#	endif
#	if __has_builtin(__builtin_isxdigit)
#		define hawk_is_bch_xdigit __builtin_isxdigit
#	endif
#	if __has_builtin(__builtin_isalnum)
#		define hawk_is_bch_alnum __builtin_isalnum
#	endif
#	if __has_builtin(__builtin_isspace)
#		define hawk_is_bch_space __builtin_isspace
#	endif
#	if __has_builtin(__builtin_isprint)
#		define hawk_is_bch_print __builtin_isprint
#	endif
#	if __has_builtin(__builtin_isgraph)
#		define hawk_is_bch_graph __builtin_isgraph
#	endif
#	if __has_builtin(__builtin_iscntrl)
#		define hawk_is_bch_cntrl __builtin_iscntrl
#	endif
#	if __has_builtin(__builtin_ispunct)
#		define hawk_is_bch_punct __builtin_ispunct
#	endif
#	if __has_builtin(__builtin_isblank)
#		define hawk_is_bch_blank __builtin_isblank
#	endif
#	if __has_builtin(__builtin_toupper)
#		define hawk_to_bch_upper __builtin_toupper
#	endif
#	if __has_builtin(__builtin_tolower)
#		define hawk_to_bch_lower __builtin_tolower
#	endif
#elif (__GNUC__ >= 14)
#	define hawk_is_bch_upper __builtin_isupper
#	define hawk_is_bch_lower __builtin_islower
#	define hawk_is_bch_alpha __builtin_isalpha
#	define hawk_is_bch_digit __builtin_isdigit
#	define hawk_is_bch_xdigit __builtin_isxdigit
#	define hawk_is_bch_alnum __builtin_isalnum
#	define hawk_is_bch_space __builtin_isspace
#	define hawk_is_bch_print __builtin_isprint
#	define hawk_is_bch_graph __builtin_isgraph
#	define hawk_is_bch_cntrl __builtin_iscntrl
#	define hawk_is_bch_punct __builtin_ispunct
#	define hawk_is_bch_blank __builtin_isblank
#	define hawk_to_bch_upper __builtin_toupper
#	define hawk_to_bch_lower __builtin_tolower
#endif

/* the bch class functions support no locale.
 * these implemenent latin-1 only */

#if !defined(hawk_is_bch_upper) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_upper (hawk_bch_t c) { return (hawk_bcu_t)c - 'A' < 26; }
#elif !defined(hawk_is_bch_upper)
#	define hawk_is_bch_upper(c) ((hawk_bcu_t)(c) - 'A' < 26)
#endif

#if !defined(hawk_is_bch_lower) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_lower (hawk_bch_t c) { return (hawk_bcu_t)c - 'a' < 26; }
#elif !defined(hawk_is_bch_lower)
#	define hawk_is_bch_lower(c) ((hawk_bcu_t)(c) - 'a' < 26)
#endif

#if !defined(hawk_is_bch_alpha) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_alpha (hawk_bch_t c) { return ((hawk_bcu_t)c | 32) - 'a' < 26; }
#elif !defined(hawk_is_bch_alpha)
#	define hawk_is_bch_alpha(c) (((hawk_bcu_t)(c) | 32) - 'a' < 26)
#endif

#if !defined(hawk_is_bch_digit) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_digit (hawk_bch_t c) { return (hawk_bcu_t)c - '0' < 10; }
#elif !defined(hawk_is_bch_digit)
#	define hawk_is_bch_digit(c) ((hawk_bcu_t)(c) - '0' < 10)
#endif

#if !defined(hawk_is_bch_xdigit) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_xdigit (hawk_bch_t c) { return hawk_is_bch_digit(c) || ((hawk_bcu_t)c | 32) - 'a' < 6; }
#elif !defined(hawk_is_bch_xdigit)
#	define hawk_is_bch_xdigit(c) (hawk_is_bch_digit(c) || ((hawk_bcu_t)(c) | 32) - 'a' < 6)
#endif

#if !defined(hawk_is_bch_alnum) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_alnum (hawk_bch_t c) { return hawk_is_bch_alpha(c) || hawk_is_bch_digit(c); }
#elif !defined(hawk_is_bch_alnum)
#	define hawk_is_bch_alnum(c) (hawk_is_bch_alpha(c) || hawk_is_bch_digit(c))
#endif

#if !defined(hawk_is_bch_space) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_space (hawk_bch_t c) { return c == ' ' || (hawk_bcu_t)c - '\t' < 5; }
#elif !defined(hawk_is_bch_space)
#	define hawk_is_bch_space(c) ((c) == ' ' || (hawk_bcu_t)(c) - '\t' < 5)
#endif

#if !defined(hawk_is_bch_print) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_print (hawk_bch_t c) { return (hawk_bcu_t)c - ' ' < 95; }
#elif !defined(hawk_is_bch_print)
#	define hawk_is_bch_print(c) ((hawk_bcu_t)(c) - ' ' < 95)
#endif

#if !defined(hawk_is_bch_graph) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_graph (hawk_bch_t c) { return (hawk_bcu_t)c - '!' < 94; }
#elif !defined(hawk_is_bch_graph)
#	define hawk_is_bch_graph(c) ((hawk_bcu_t)(c) - '!' < 94)
#endif

#if !defined(hawk_is_bch_cntrl) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_cntrl (hawk_bch_t c) { return (hawk_bcu_t)c < ' ' || (hawk_bcu_t)c == 127; }
#elif !defined(hawk_is_bch_cntrl)
#	define hawk_is_bch_cntrl(c) ((hawk_bcu_t)(c) < ' ' || (hawk_bcu_t)(c) == 127)
#endif

#if !defined(hawk_is_bch_punct) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_punct (hawk_bch_t c) { return hawk_is_bch_graph(c) && !hawk_is_bch_alnum(c); }
#elif !defined(hawk_is_bch_punct)
#	define hawk_is_bch_punct(c) (hawk_is_bch_graph(c) && !hawk_is_bch_alnum(c))
#endif

#if !defined(hawk_is_bch_blank) && defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_is_bch_blank (hawk_bch_t c) { return c == ' ' || c == '\t'; }
#elif !defined(hawk_is_bch_blank)
#	define hawk_is_bch_blank(c) ((c) == ' ' || (c) == '\t')
#endif

#if !defined(hawk_to_bch_upper)
HAWK_EXPORT hawk_bch_t hawk_to_bch_upper (hawk_bch_t c);
#endif
#if !defined(hawk_to_bch_lower)
HAWK_EXPORT hawk_bch_t hawk_to_bch_lower (hawk_bch_t c);
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_is_ooch_type hawk_is_uch_type
#	define hawk_is_ooch_upper hawk_is_uch_upper
#	define hawk_is_ooch_lower hawk_is_uch_lower
#	define hawk_is_ooch_alpha hawk_is_uch_alpha
#	define hawk_is_ooch_digit hawk_is_uch_digit
#	define hawk_is_ooch_xdigit hawk_is_uch_xdigit
#	define hawk_is_ooch_alnum hawk_is_uch_alnum
#	define hawk_is_ooch_space hawk_is_uch_space
#	define hawk_is_ooch_print hawk_is_uch_print
#	define hawk_is_ooch_graph hawk_is_uch_graph
#	define hawk_is_ooch_cntrl hawk_is_uch_cntrl
#	define hawk_is_ooch_punct hawk_is_uch_punct
#	define hawk_is_ooch_blank hawk_is_uch_blank
#	define hawk_to_ooch_upper hawk_to_uch_upper
#	define hawk_to_ooch_lower hawk_to_uch_lower
#else
#	define hawk_is_ooch_type hawk_is_bch_type
#	define hawk_is_ooch_upper hawk_is_bch_upper
#	define hawk_is_ooch_lower hawk_is_bch_lower
#	define hawk_is_ooch_alpha hawk_is_bch_alpha
#	define hawk_is_ooch_digit hawk_is_bch_digit
#	define hawk_is_ooch_xdigit hawk_is_bch_xdigit
#	define hawk_is_ooch_alnum hawk_is_bch_alnum
#	define hawk_is_ooch_space hawk_is_bch_space
#	define hawk_is_ooch_print hawk_is_bch_print
#	define hawk_is_ooch_graph hawk_is_bch_graph
#	define hawk_is_ooch_cntrl hawk_is_bch_cntrl
#	define hawk_is_ooch_punct hawk_is_bch_punct
#	define hawk_is_ooch_blank hawk_is_bch_blank
#	define hawk_to_ooch_upper hawk_to_bch_upper
#	define hawk_to_ooch_lower hawk_to_bch_lower
#endif

HAWK_EXPORT int hawk_ucstr_to_uch_prop (
	const hawk_uch_t*  name,
	hawk_uch_prop_t*   id
);

HAWK_EXPORT int hawk_uchars_to_uch_prop (
	const hawk_uch_t*  name,
	hawk_oow_t         len,
	hawk_uch_prop_t*   id
);

HAWK_EXPORT int hawk_bcstr_to_bch_prop (
	const hawk_bch_t*  name,
	hawk_bch_prop_t*   id
);

HAWK_EXPORT int hawk_bchars_to_bch_prop (
	const hawk_bch_t*  name,
	hawk_oow_t         len,
	hawk_bch_prop_t*   id
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_oocstr_to_ooch_prop hawk_ucstr_to_uch_prop
#	define hawk_oochars_to_ooch_prop hawk_uchars_to_uch_prop
#else
#	define hawk_oocstr_to_ooch_prop hawk_bcstr_to_bch_prop
#	define hawk_oochars_to_ooch_prop hawk_bchars_to_bch_prop
#endif

/* ------------------------------------------------------------------------- */

HAWK_EXPORT int hawk_get_ucwidth (
        hawk_uch_t uc
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_uc_to_utf8 (
	hawk_uch_t    uc,
	hawk_bch_t*   utf8,
	hawk_oow_t    size
);

HAWK_EXPORT hawk_oow_t hawk_utf8_to_uc (
	const hawk_bch_t* utf8,
	hawk_oow_t        size,
	hawk_uch_t*       uc
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_uc_to_utf16 (
	hawk_uch_t    uc,
	hawk_bch_t*   utf16,
	hawk_oow_t    size
);

HAWK_EXPORT hawk_oow_t hawk_utf16_to_uc (
	const hawk_bch_t* utf16,
	hawk_oow_t        size,
	hawk_uch_t*       uc
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_uc_to_mb8 (
	hawk_uch_t    uc,
	hawk_bch_t*   mb8,
	hawk_oow_t    size
);

HAWK_EXPORT hawk_oow_t hawk_mb8_to_uc (
	const hawk_bch_t* mb8,
	hawk_oow_t        size,
	hawk_uch_t*       uc
);


#if defined(__cplusplus)
}
#endif

#endif
