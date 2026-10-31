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

#ifndef _HAWK_REX_H_
#define _HAWK_REX_H_

#include <hawk-gem.h>
#include <hawk-ecs.h>
#include <hawk-str.h>

/** @file
 *
 * Regular Expression Syntax
 *
 * Rex provides an ERE-style syntax with explicit limitations; it does not
 * implement full POSIX regex conformance or a separate BRE syntax mode.
 * Character matching and ranges use character values rather than locale
 * collation rules. Locale-specific collating symbols such as [[.a.]] and
 * equivalence classes such as [[=a=]] are deliberately unsupported.
 * Compilation rejects these constructs with HAWK_ENOSUP and a diagnostic
 * identifying the construct. Named character classes such as [[:alpha:]]
 * remain supported through Hawk's character classification routines.
 *
 *  regular expression := branch_set
 *  branch_set := branch [ '|' branch ]*
 *  atom := char | '.' | '^' | '$' | subgroup
 *  subgroup = '(' branch_set ')'
 *  bound := '?' | '*' | '+' | {n,m}
 *  bounded_atom := atom [ bound ]
 *  branch := bounded_atom bounded_atom*
 *
 * Outside bracket expressions, escape sequences include \xXX, \t, \n,
 * \r, \f, \b, \v, and \a.
 * Wide-character builds also accept \uXXXX, and 32-bit character builds
 * accept \UXXXXXXXX. Octal escapes and numeric backreferences are not enabled.
 *
 * Outside bracket expressions, a special character preceded by a backslash
 * loses its special role and matches the character itself literally.
 * Inside bracket expressions, backslash is an ordinary character: [\]
 * matches a backslash, and [\n] matches either a backslash or the letter n.
 * Use actual character values inside brackets to match tabs, newlines, or
 * other characters that would otherwise be written with escape sequences.
 *
 * Some examples of compiled regular expressions are shown below.
 *
 * @code
 * ab
 * START --@ NOP --@  CHAR(a) --@ CHAR(b) --@ END
 *
 * a(bc)d
 * START --@ NOP --@ CHAR(A) --@ GROUP --@ CHAR(d) --@ END
 *                   u.g.head => | @
 *                               | | <= u.g.end
 *                               | +---------------------------+
 *                               |                             | <= u.ge.group
 *                               @                             @
 *                              NOP -@ CHAR(b) -@ CHAR(c) -@ GROUPEND
 *
 * ab|cd
 * START --@ NOP --@  BRANCH --+--@ CHAR(a) --@ CHAR(b) --+--@ END
 *                             |                          |
 *                             | <= u.b.alter             |
 *                             +--@ CHAR(c) --@ CHAR(d) --+
 * @endcode
 *
 * @todo
 * - support \\n to refer to the nth matching substring
 */

enum hawk_rex_option_t
{
	/**< do not allow a special character at normal character position. */
	HAWK_REX_STRICT  = (1 << 0),

	/**< do not support the {n,m} style occurrence specifier. */
	HAWK_REX_NOBOUND = (1 << 1),

	/**< perform case-insensitive match */
	HAWK_REX_IGNORECASE = (1 << 2)
};

enum hawk_rex_node_id_t
{
	HAWK_REX_NODE_START,
	HAWK_REX_NODE_END,
	HAWK_REX_NODE_NOP,
	HAWK_REX_NODE_BOL,  /* beginning of line */
	HAWK_REX_NODE_EOL,  /* end of line */
	HAWK_REX_NODE_ANY,  /* dot */
	HAWK_REX_NODE_CHAR, /* single character */
	HAWK_REX_NODE_CSET, /* character set */
	HAWK_REX_NODE_BRANCH,
	HAWK_REX_NODE_GROUP,
	HAWK_REX_NODE_GROUPEND,
	HAWK_REX_NODE_BACKREF /* back reference */
};
typedef enum hawk_rex_node_id_t hawk_rex_node_id_t;

typedef struct hawk_rex_node_t hawk_rex_node_t;
struct hawk_rex_node_t
{
	/* for internal management. not used for startnode */
	hawk_rex_node_t* link;

	/* Connect to the next node in the graph. For a branch, this is its
	 * first alternative; u.b.alter holds the other alternative. */
	hawk_rex_node_t* next;

	hawk_rex_node_id_t id;
	union
	{
		struct
		{
			hawk_gem_t*     gem; /* borrowed owner of all nodes in this graph */
			hawk_rex_node_t* link;
		} s;

		hawk_ooch_t c;

		struct
		{
			int negated;
			hawk_ooecs_t* member;
		} cset;

		struct
		{
			hawk_rex_node_t* alter;
		} b;

		struct
		{
			hawk_rex_node_t* head;
			hawk_rex_node_t* end;
			int pseudo;
		} g;

		struct
		{
			hawk_rex_node_t* group;
			int pseudo;
		} ge;

		struct
		{
			int index;
		} bref;
	} u;

	struct
	{
		hawk_oow_t min;
		hawk_oow_t max;
	} occ;
};

enum hawk_rex_cset_code_t
{
	HAWK_REX_CSET_CHAR,
	HAWK_REX_CSET_RANGE,
	HAWK_REX_CSET_CLASS
};

/** @struct hawk_rex_t
 * The hawk_rex_t type defines a regular expression processor.
 * It compiles a graph and finds the leftmost-longest whole match in an
 * explicit-length string. Captured subgroup results and backreferences are
 * not implemented. This engine is independent of the interpreter's TRE API.
 *
 * The supplied gem is borrowed and handles allocation, character conversion,
 * and diagnostics. It must outlive the object and any graph compiled with it.
 * Errors use hawk_errnum_t in the gem; there is no separate object diagnostic.
 */
typedef struct hawk_rex_t hawk_rex_t;
struct hawk_rex_t
{
	hawk_gem_t* gem;
	int option;
	hawk_rex_node_t* code;
};

#if defined(__cplusplus)
extern "C" {
#endif

/** Allocate an object and zero its extension bytes. An optional graph is
 * adopted and released at close unless detached with hawk_rex_yield().
 * Return NULL on failure, leaving a supplied graph owned by the caller. */
HAWK_EXPORT hawk_rex_t* hawk_rex_open (
	hawk_gem_t*      gem, /**< borrowed allocation/diagnostic context */
	hawk_oow_t       xtn, /**< extension size in bytes */
	hawk_rex_node_t* code  /**< compiled regular expression code */
);

/** Finalize the graph and free an object allocated by hawk_rex_open(). */
HAWK_EXPORT void hawk_rex_close (
	hawk_rex_t* rex
);

/** Initialize caller-owned storage, adopting an optional graph. Return 0.
 * Use hawk_rex_fini(), rather than close(), for this storage. */
HAWK_EXPORT int hawk_rex_init (
	hawk_rex_t* rex,
	hawk_gem_t* gem,
	hawk_rex_node_t* code
);

/**
 * The hawk_rex_fini() function finalizes a statically initialized
 * regular expression object @a rex.
 */
HAWK_EXPORT void hawk_rex_fini (
	hawk_rex_t* rex
);

/** Return the borrowed gem used for this object's work and diagnostics. */
HAWK_EXPORT hawk_gem_t* hawk_rex_getgem (
	hawk_rex_t* rex
);

/** Return extension storage reserved by hawk_rex_open(). */
HAWK_EXPORT void* hawk_rex_getxtn (
	hawk_rex_t* rex
);

/**
 * The hawk_rex_yield() function gives up the ownership of a compiled regular
 * expression. Once yielded, the compiled regular expression is not destroyed
 * when @a rex is closed or finalized.
 * @return start node of a compiled regular expression
 */
HAWK_EXPORT hawk_rex_node_t* hawk_rex_yield (
	hawk_rex_t* rex /**< regular expression processor */
);

/**
 * The hawk_rex_getopt() function returns the current options.
 */
HAWK_EXPORT int hawk_rex_getopt (
	const hawk_rex_t* rex /**< regular expression processor */
);

/**
 * The hawk_rex_setopt() function overrides the current options with options.
 */
HAWK_EXPORT void hawk_rex_setopt (
	hawk_rex_t* rex, /**< regular expression processor */
	int         opts /**< 0 or bitwise OR of ::hawk_rex_option_t enumerators */
);

/** Read the shared gem's current error code. Successful regex calls do not
 * clear a previous diagnostic; use operation return values to detect errors. */
HAWK_EXPORT hawk_errnum_t hawk_rex_geterrnum (
	const hawk_rex_t* rex
);

/** Borrow the gem's error message; later diagnostic updates may replace it. */
HAWK_EXPORT const hawk_ooch_t* hawk_rex_geterrmsg (
	const hawk_rex_t* rex
);

/** Compile len character units, including embedded NULs. Return the object's
 * graph or NULL on failure. Failed compilation retains the previous graph;
 * successful compilation replaces and frees it. The pattern is not retained. */
HAWK_EXPORT hawk_rex_node_t* hawk_rex_comp (
	hawk_rex_t*        rex,
	const hawk_ooch_t* ptn,
	hawk_oow_t         len
);

/** Search substr, which must be a slice inside str's live storage. Anchors
 * refer to str's full bounds. Return 1 for a match, 0 for none, or -1 on error.
 * A non-NULL matstr receives a borrowed input slice only when a match succeeds.
 * Pattern/input lengths and offsets are in hawk_ooch_t units, not UTF-8 bytes
 * in a wide-character build. Input storage is never modified. */
HAWK_EXPORT int hawk_rex_exec (
	hawk_rex_t*        rex,
	const hawk_oocs_t* str,
	const hawk_oocs_t* substr,
	hawk_oocs_t*        matstr
);


/** Compile and detach a graph owned by gem; return NULL on failure. */
HAWK_EXPORT hawk_rex_node_t* hawk_rex_build (
	hawk_gem_t*       gem,
	int               option,
	const hawk_ooch_t* ptn,
	hawk_oow_t        len
);

/** Match a borrowed graph without taking ownership. Return 1, 0, or -1. */
HAWK_EXPORT int hawk_rex_match (
	hawk_gem_t*        gem,
	hawk_rex_node_t*   code,
	int                option,
	const hawk_oocs_t* str,
	const hawk_oocs_t* substr,
	hawk_oocs_t*       match
);

/** Release a detached graph through the gem recorded in its start node. */
HAWK_EXPORT void hawk_rex_freecode (
	hawk_rex_node_t* code
);

#if defined(__cplusplus)
}
#endif

#endif
