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

/* Shared regular-expression primitives for Hawk and Sed. */
#ifndef _HAWK_REX_H_
#define _HAWK_REX_H_

#include <hawk-gem.h>

/** Compilation flags. Absence of EXTENDED selects BRE. An engine must reject
 * unsupported requested semantics with HAWK_ENOSUP, rather than ignore them. */
enum hawk_rex_cflag_t
{
	HAWK_REX_EXTENDED = (1 << 0),
	HAWK_REX_IGNORECASE = (1 << 1),
	HAWK_REX_NOBOUND = (1 << 2),
	HAWK_REX_NEWLINE = (1 << 3),
	HAWK_REX_NONSTDEXT = (1 << 4)
};
typedef enum hawk_rex_cflag_t hawk_rex_cflag_t;

enum hawk_rex_eflag_t
{
	HAWK_REX_NOTBOL = (1 << 0),
	HAWK_REX_NOTEOL = (1 << 1)
};
typedef enum hawk_rex_eflag_t hawk_rex_eflag_t;

enum hawk_rex_input_type_t
{
	HAWK_REX_INPUT_BCH,
	HAWK_REX_INPUT_UCH
};
typedef enum hawk_rex_input_type_t hawk_rex_input_type_t;

/** The original subject and a search interval [start, end). Lengths and
 * offsets count BCH bytes or UCH code units, never encoded UTF-8 characters.
 * BCH input contains unsigned byte values; no implicit decoding is permitted.
 * Anchors refer to the original subject, subject to NOTBOL/NOTEOL.
 * ptr must be non-NULL, including for an empty subject. The subject can
 * contain NULs and is borrowed for this invocation only. */
struct hawk_rex_input_t
{
	hawk_rex_input_type_t type;
	const void* ptr;
	hawk_oow_t len;
	hawk_oow_t start;
	hawk_oow_t end;
};
typedef struct hawk_rex_input_t hawk_rex_input_t;

#define HAWK_REX_NOMATCH ((hawk_oow_t)-1)

/** Half-open offsets into the original subject. Entry 0 is the entire match;
 * entry n is capture n. Both offsets must be NOMATCH for an absent group or
 * a slot beyond the pattern's capture count. An empty capture has equal,
 * valid offsets. The entire match must lie inside the search interval. */
struct hawk_rex_match_t
{
	hawk_oow_t start;
	hawk_oow_t end;
};
typedef struct hawk_rex_match_t hawk_rex_match_t;

/** Return 0 with a non-NULL owned code and the number of subexpressions, or
 * -1 with a diagnostic in gem. Clean up partial allocations on failure.
 * Pattern storage is borrowed only during this call and must be non-NULL,
 * including for an empty pattern. The compilation gem must outlive code. */
typedef int (*hawk_rex_build_fn_t) (
	hawk_gem_t*              gem,
	void*                    ctx,
	const hawk_ooch_t*       pattern,
	hawk_oow_t               len,
	int                      flags,
	void**                   code,
	hawk_oow_t*              nsub
);

/** Return 1 for a match, 0 for no match, or -1 with a diagnostic in gem.
 * Outputs are ignored unless the result is 1. The execution gem can differ
 * from the compilation gem: use it for per-call scratch and diagnostics.
 * Compiled code must be immutable and concurrently reusable by independent
 * execution instances. Do not retain input, output, or execution-gem pointers.
 * matches can be NULL when count is zero. */
typedef int (*hawk_rex_match_fn_t) (
	hawk_gem_t*              gem,
	void*                    ctx,
	const void*              code,
	const hawk_rex_input_t*  input,
	int                      flags,
	hawk_rex_match_t*        matches,
	hawk_oow_t               count
);

/** Release code through its original allocator. No execution gem is supplied. */
typedef void (*hawk_rex_free_fn_t) (
	void*                    ctx,
	void*                    code
);

/** Install all three callbacks or none. The table is copied, ctx is borrowed.
 * Keep the binding and ctx alive and unchanged until all compiled handles,
 * parsed programs, and execution instances using them have been destroyed.
 * An all-zero table selects the owner's existing built-in TRE implementation.
 * Initialize the entire table, including when extending hawk_prm_t. */
struct hawk_rex_prm_t
{
	hawk_rex_build_fn_t build;
	hawk_rex_match_fn_t match;
	hawk_rex_free_fn_t free;
	void* ctx;
};
typedef struct hawk_rex_prm_t hawk_rex_prm_t;

#if defined(__cplusplus)
extern "C" {
#endif

/** Validate callback completeness. Return 0 or -1 with HAWK_EINVAL in gem. */
HAWK_EXPORT int hawk_rex_checkprm (
	hawk_gem_t*              gem,
	const hawk_rex_prm_t*    prm
);

/** Invoke a custom builder. Outputs remain unchanged on failure. These shared
 * helpers require installed callbacks; an empty table is an owner-level
 * request for TRE, not a separately constructed engine. */
HAWK_EXPORT int hawk_rex_build (
	hawk_gem_t*              gem,
	const hawk_rex_prm_t*    prm,
	const hawk_ooch_t*       pattern,
	hawk_oow_t               len,
	int                      flags,
	void**                   code,
	hawk_oow_t*              nsub
);

/** Invoke a custom matcher and validate returned offsets. Outputs are only
 * usable on return 1, and may have been overwritten on return 0 or -1. */
HAWK_EXPORT int hawk_rex_match (
	hawk_gem_t*              gem,
	const hawk_rex_prm_t*    prm,
	const void*              code,
	const hawk_rex_input_t*  input,
	int                      flags,
	hawk_rex_match_t*        matches,
	hawk_oow_t               count
);

/** Return the immutable primitive table adapting hawk_rgx_t. Copy it into
 * hawk_prm_t.rex or pass it to HAWK_SED_REXPRM before compilation.
 * RGX currently rejects NEWLINE and NONSTDEXT. Opposite-width input is
 * converted per call with a one-byte/one-UCH MB8 mapping. In a BCH build,
 * UCH values greater than 255 fail with HAWK_ENOSUP. No UTF-8 decoding or
 * offset remapping occurs. The standalone hawk_rgx_* API is independent of
 * this interface. */
HAWK_EXPORT const hawk_rex_prm_t* hawk_rex_getrgxprm (void);

#if defined(__cplusplus)
}
#endif

#endif
