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

#ifndef _HAWK_TRE_H_
#define _HAWK_TRE_H_

#include <hawk-cmn.h>

struct hawk_tre_t
{
	hawk_gem_t*       gem;
	hawk_oow_t        re_nsub;  /* Number of parenthesized subexpressions. */
	void*             value;	 /* For internal use only. */
};

typedef int hawk_tre_off_t;

typedef struct hawk_tre_match_t hawk_tre_match_t;
struct hawk_tre_match_t
{
	hawk_tre_off_t rm_so;
	hawk_tre_off_t rm_eo;
};

enum hawk_tre_cflag_t
{
	HAWK_TRE_EXTENDED    = (1 << 0),
	HAWK_TRE_IGNORECASE  = (1 << 1),
	HAWK_TRE_NEWLINE     = (1 << 2),
	HAWK_TRE_NOSUBREG    = (1 << 3),
	HAWK_TRE_LITERAL     = (1 << 4),
	HAWK_TRE_RIGHTASSOC  = (1 << 5),
	HAWK_TRE_UNGREEDY    = (1 << 6),

	/* Disable {n,m} occrrence specifier
	 * in the HAWK_TRE_EXTENDED mode.
	 * it doesn't affect the BRE's \{\}. */
	HAWK_TRE_NOBOUND     = (1 << 7),

	/* Enable non-standard extensions:
	 *  - Enable (?:text) for no submatch backreference.
	 *  - Enable perl-like (?...) extensions like (?i)
	 *    if HAWK_TRE_EXTENDED is also set.
	 */
	HAWK_TRE_NONSTDEXT   = (1 << 8)
};

enum hawk_tre_eflag_t
{
	HAWK_TRE_BACKTRACKING = (1 << 0),
	HAWK_TRE_NOTBOL       = (1 << 1),
	HAWK_TRE_NOTEOL       = (1 << 2)
};

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_tre_t* hawk_tre_open (
	hawk_gem_t* gem,
	hawk_oow_t  xtnsize
);

HAWK_EXPORT void hawk_tre_close (
	hawk_tre_t*  tre
);

HAWK_EXPORT int hawk_tre_init (
	hawk_tre_t*  tre,
	hawk_gem_t*  gem
);

HAWK_EXPORT void hawk_tre_fini (
	hawk_tre_t* tre
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_tre_getxtn (hawk_tre_t* tre) { return (void*)(tre + 1); }
#else
#define hawk_tre_getxtn(tre) ((void*)((hawk_tre_t*)(tre) + 1))
#endif

HAWK_EXPORT int hawk_tre_compx (
	hawk_tre_t*        tre,
	const hawk_ooch_t* regex,
	hawk_oow_t         n,
	unsigned int*      nsubmat,
	int                cflags
);

HAWK_EXPORT int hawk_tre_comp (
	hawk_tre_t*        tre,
	const hawk_ooch_t* regex,
	unsigned int*      nsubmat,
	int                cflags
);

HAWK_EXPORT int hawk_tre_execx (
	hawk_tre_t*        tre,
	const hawk_ooch_t* str,
	hawk_oow_t         len,
	hawk_tre_match_t*  pmatch,
	hawk_oow_t         nmatch,
	int                eflags,
	hawk_gem_t*        errgem
);

HAWK_EXPORT int hawk_tre_exec (
	hawk_tre_t*        tre,
	const hawk_ooch_t* str,
	hawk_tre_match_t*  pmatch,
	hawk_oow_t         nmatch,
	int                eflags,
	hawk_gem_t*        errgem
);

HAWK_EXPORT int hawk_tre_execuchars (
	hawk_tre_t*        tre,
	const hawk_uch_t*  str,
	hawk_oow_t         len,
	hawk_tre_match_t*  pmatch,
	hawk_oow_t         nmatch,
	int                eflags,
	hawk_gem_t*        errgem
);

HAWK_EXPORT int hawk_tre_execbchars (
	hawk_tre_t*        tre,
	const hawk_bch_t*  str,
	hawk_oow_t         len,
	hawk_tre_match_t*  pmatch,
	hawk_oow_t         nmatch,
	int                eflags,
	hawk_gem_t*        errgem
);

#if defined(__cplusplus)
}
#endif

#endif
