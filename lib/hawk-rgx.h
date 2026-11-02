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

#ifndef _HAWK_RGX_H_
#define _HAWK_RGX_H_

#include <hawk-gem.h>
#include <hawk-ecs.h>
#include <hawk-str.h>

/** \file
 *
 * Regular Expression Syntax
 *
 * RGX uses basic regular expression (BRE) syntax by default. Set
 * HAWK_RGX_EXTENDED before compilation to select extended syntax (ERE).
 * Both dialects have explicit limitations; full POSIX conformance is not
 * claimed. BRE groups use \\( and \\), and intervals use \\{n,m\\}.
 * Unescaped parentheses, braces, +, ?, and | are literal in BRE. The GNU
 * extensions \\+ and \\? provide one-or-more and zero-or-one repetition;
 * \\| separates alternatives. ERE uses the grammar shown below.
 * HAWK_RGX_EXTENDED affects compilation only; changing it does not
 * reinterpret a previously compiled graph. HAWK_RGX_STRICT is independent
 * of the dialect. HAWK_RGX_NOBOUND disables intervals in both dialects.
 * In BRE, ^ anchors at the beginning of a branch, and $ at its end,
 * including beside \\| alternatives. A leading * is literal.
 * Character matching and ranges use character values rather than locale
 * collation rules. Locale-specific collating symbols such as [[.a.]] and
 * equivalence classes such as [[=a=]] are deliberately unsupported.
 * Compilation rejects these constructs with HAWK_EREXCOLLATE and a diagnostic
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
 * accept \UXXXXXXXX. Octal escapes are not enabled.
 * Outside brackets, \1 through \9 are numeric backreferences to previously
 * closed groups. Forward references and references to open or nonexistent
 * groups fail compilation with HAWK_EREXSUBREG. A reference to a group that
 * did not participate fails to match; a participating empty group matches
 * an empty string. Backreferences honor HAWK_RGX_IGNORECASE.
 *
 * Outside bracket expressions, backslash quotes ERE special characters.
 * In BRE it introduces grouping, intervals, and GNU alternation/quantifiers
 * described above. Character escapes and backreferences work in both modes.
 * By default inside bracket expressions, backslash is an ordinary character: [\]
 * matches a backslash, and [\n] matches either a backslash or the letter n.
 * Use actual character values inside brackets to match tabs, newlines, or
 * other characters that would otherwise be written with escape sequences.
 * HAWK_RGX_BRACKETESC instead makes backslash quote the next bracket member,
 * matching Hawk's TRE behavior. Thus [\[] matches '[' and [\\] matches a
 * backslash. Quoted punctuation is literal; \n means 'n', not newline, and
 * \1 means '1', not a backreference, inside brackets. The shared rex adapter
 * enables this option for Hawk and Sed; the standalone default is unchanged.
 * Compilation errors in shared BRE/ERE syntax use the same HAWK_EREX* codes
 * as TRE: BRACK for an unterminated bracket, ESCAPE for a trailing backslash
 * outside brackets, CTYPE for a malformed named class, and BRACE/BADBR for
 * missing interval terminators/invalid interval contents. The engines still
 * differ in accepted extensions and strictness. Execution before compilation
 * reports HAWK_EREXBADPAT.
 *
 * Some examples of compiled regular expressions are shown below.
 *
 * \code
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
 * \endcode
 *
 */

enum hawk_rgx_option_t
{
	/**< do not allow a special character at normal character position. */
	HAWK_RGX_STRICT  = (1 << 0),

	/**< disable interval quantifiers in both BRE and ERE. */
	HAWK_RGX_NOBOUND = (1 << 1),

	/**< perform case-insensitive match */
	HAWK_RGX_IGNORECASE = (1 << 2),

	/**< compile ERE syntax; without this option, compile BRE syntax. */
	HAWK_RGX_EXTENDED = (1 << 3),

	/**< let backslash quote the next bracket member, as in Hawk's TRE. */
	HAWK_RGX_BRACKETESC = (1 << 4)
};

/** Per-call execution flags; do not pass these to hawk_rgx_setopt(). */
enum hawk_rgx_eflag_t
{
	HAWK_RGX_NOTBOL = (1 << 0), /**< the subject beginning does not satisfy ^ */
	HAWK_RGX_NOTEOL = (1 << 1)  /**< the subject end does not satisfy $ */
};

enum hawk_rgx_node_id_t
{
	HAWK_RGX_NODE_START,
	HAWK_RGX_NODE_END,
	HAWK_RGX_NODE_NOP,
	HAWK_RGX_NODE_BOL,  /* beginning of line */
	HAWK_RGX_NODE_EOL,  /* end of line */
	HAWK_RGX_NODE_ANY,  /* dot */
	HAWK_RGX_NODE_CHAR, /* single character */
	HAWK_RGX_NODE_CSET, /* character set */
	HAWK_RGX_NODE_BRANCH,
	HAWK_RGX_NODE_GROUP,
	HAWK_RGX_NODE_GROUPEND,
	HAWK_RGX_NODE_BACKREF /* back reference */
};
typedef enum hawk_rgx_node_id_t hawk_rgx_node_id_t;

typedef struct hawk_rgx_node_t hawk_rgx_node_t;
struct hawk_rgx_node_t
{
	/* for internal management. not used for startnode */
	hawk_rgx_node_t* link;

	/* Connect to the next node in the graph. For a branch, this is its
	 * first alternative; u.b.alter holds the other alternative. */
	hawk_rgx_node_t* next;

	hawk_rgx_node_id_t id;
	union
	{
		struct
		{
			hawk_gem_t*     gem; /* borrowed owner of all nodes in this graph */
			hawk_rgx_node_t* link;
			hawk_oow_t nsub; /* numbered capturing groups */
			int backrefs; /* execution must retain captures even without output */
		} s;

		hawk_ooch_t c;

		struct
		{
			int negated;
			hawk_ooecs_t* member;
		} cset;

		struct
		{
			hawk_rgx_node_t* alter;
		} b;

		struct
		{
			hawk_rgx_node_t* head;
			hawk_rgx_node_t* end;
			int pseudo;
			hawk_oow_t index; /* zero-based capture index; unused for pseudo groups */
		} g;

		struct
		{
			hawk_rgx_node_t* group;
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

enum hawk_rgx_cset_code_t
{
	HAWK_RGX_CSET_CHAR,
	HAWK_RGX_CSET_RANGE,
	HAWK_RGX_CSET_CLASS
};

/** \struct hawk_rgx_t
 * The hawk_rgx_t type defines a regular expression processor.
 * It compiles a graph and finds the leftmost-longest whole match in an
 * explicit-length string. hawk_rgx_execx() also reports captured groups;
 * numeric pattern backreferences are supported as an ERE extension.
 * This engine is independent of the interpreter's TRE API.
 *
 * The supplied gem is borrowed and handles allocation, character conversion,
 * and diagnostics. It must outlive the object and any graph compiled with it.
 * Errors use hawk_errnum_t in the gem; there is no separate object diagnostic.
 */
typedef struct hawk_rgx_t hawk_rgx_t;
struct hawk_rgx_t
{
	hawk_gem_t* gem;
	int option;
	hawk_rgx_node_t* code;
};

#if defined(__cplusplus)
extern "C" {
#endif

/** Allocate an object and zero its extension bytes. An optional graph is
 * adopted and released at close unless detached with hawk_rgx_yield().
 * Return NULL on failure, leaving a supplied graph owned by the caller. */
HAWK_EXPORT hawk_rgx_t* hawk_rgx_open (
	hawk_gem_t*      gem, /**< borrowed allocation/diagnostic context */
	hawk_oow_t       xtn, /**< extension size in bytes */
	hawk_rgx_node_t* code  /**< compiled regular expression code */
);

/** Finalize the graph and free an object allocated by hawk_rgx_open(). */
HAWK_EXPORT void hawk_rgx_close (
	hawk_rgx_t* rex
);

/** Initialize caller-owned storage, adopting an optional graph. Return 0.
 * Use hawk_rgx_fini(), rather than close(), for this storage. */
HAWK_EXPORT int hawk_rgx_init (
	hawk_rgx_t* rex,
	hawk_gem_t* gem,
	hawk_rgx_node_t* code
);

/**
 * The hawk_rgx_fini() function finalizes a statically initialized
 * regular expression object \a rex.
 */
HAWK_EXPORT void hawk_rgx_fini (
	hawk_rgx_t* rex
);

/** Return the borrowed gem used for this object's work and diagnostics. */
HAWK_EXPORT hawk_gem_t* hawk_rgx_getgem (
	hawk_rgx_t* rex
);

/** Return extension storage reserved by hawk_rgx_open(). */
HAWK_EXPORT void* hawk_rgx_getxtn (
	hawk_rgx_t* rex
);

/**
 * The hawk_rgx_yield() function gives up the ownership of a compiled regular
 * expression. Once yielded, the compiled regular expression is not destroyed
 * when \a rex is closed or finalized.
 * \return start node of a compiled regular expression
 */
HAWK_EXPORT hawk_rgx_node_t* hawk_rgx_yield (
	hawk_rgx_t* rex /**< regular expression processor */
);

/**
 * The hawk_rgx_getopt() function returns the current options.
 */
HAWK_EXPORT int hawk_rgx_getopt (
	const hawk_rgx_t* rex /**< regular expression processor */
);

/**
 * The hawk_rgx_setopt() function overrides the current options with options.
 */
HAWK_EXPORT void hawk_rgx_setopt (
	hawk_rgx_t* rex, /**< regular expression processor */
	int         opts /**< 0 or bitwise OR of the hawk_rgx_option_t enumerators */
);

/** Read the shared gem's current error code. Successful regex calls do not
 * clear a previous diagnostic; use operation return values to detect errors. */
HAWK_EXPORT hawk_errnum_t hawk_rgx_geterrnum (
	const hawk_rgx_t* rex
);

/** Borrow the gem's error message; later diagnostic updates may replace it. */
HAWK_EXPORT const hawk_ooch_t* hawk_rgx_geterrmsg (
	const hawk_rgx_t* rex
);

/** Compile len character units, including embedded NULs. Return the object's
 * graph or NULL on failure. Failed compilation retains the previous graph;
 * successful compilation replaces and frees it. The pattern is not retained. */
HAWK_EXPORT hawk_rgx_node_t* hawk_rgx_comp (
	hawk_rgx_t*        rex,
	const hawk_ooch_t* ptn,
	hawk_oow_t         len
);

/** Search substr, which must be a slice inside str's live storage. Anchors
 * refer to str's full bounds. Return 1 for a match, 0 for none, or -1 on error.
 * A non-NULL matstr receives a borrowed input slice only when a match succeeds.
 * Pattern/input lengths and offsets are in hawk_ooch_t units, not UTF-8 bytes
 * in a wide-character build. Input storage is never modified.
 * flags is zero or a bitwise OR of hawk_rgx_eflag_t values. NOTBOL/NOTEOL
 * suppress the corresponding subject-boundary assertions, independently of
 * the search slice. They do not affect literal ^/$ characters. Flags apply
 * only to this execution and never change the object or compiled graph.
 * Unknown execution flag bits report HAWK_EINVAL without changing output. */
HAWK_EXPORT int hawk_rgx_exec (
	hawk_rgx_t*        rex,
	const hawk_oocs_t* str,
	const hawk_oocs_t* substr,
	hawk_oocs_t*       matstr,
	int                flags
);

/** Return the number of capturing groups, or zero without compiled code. */
HAWK_EXPORT hawk_oow_t hawk_rgx_getnsub (
	const hawk_rgx_t* rex
);

/** Like hawk_rgx_exec(), with whole-match and capture output. matches[0] is
 * the whole match; matches[1..] follow opening-parenthesis order. Unmatched
 * groups and surplus entries receive {NULL, 0}; an empty participating group
 * has a non-NULL pointer and zero length. Output changes only on success.
 * A zero count permits a NULL matches pointer. Equal-length whole matches
 * prefer participating, then longer, then earlier captures in group order.
 * Repeated groups report their last completed participation. */
HAWK_EXPORT int hawk_rgx_execx (
	hawk_rgx_t*        rex,
	const hawk_oocs_t* str,
	const hawk_oocs_t* substr,
	hawk_oocs_t*       matches,
	hawk_oow_t         count,
	int                flags
);


/** Compile and detach a graph owned by gem; return NULL on failure. */
HAWK_EXPORT hawk_rgx_node_t* hawk_rgx_build (
	hawk_gem_t*        gem,
	int                option,
	const hawk_ooch_t* ptn,
	hawk_oow_t         len
);

/** Match a borrowed graph without taking ownership. Return 1, 0, or -1. */
HAWK_EXPORT int hawk_rgx_match (
	hawk_gem_t*        gem,
	hawk_rgx_node_t*   code,
	int                option,
	const hawk_oocs_t* str,
	const hawk_oocs_t* substr,
	hawk_oocs_t*       match,
	int                flags
);

/** Borrow a compiled graph and return captures as in hawk_rgx_execx(). */
HAWK_EXPORT int hawk_rgx_matchx (
	hawk_gem_t*        gem,
	hawk_rgx_node_t*   code,
	int                option,
	const hawk_oocs_t* str,
	const hawk_oocs_t* substr,
	hawk_oocs_t*       matches,
	hawk_oow_t         count,
	int                flags
);

/** Release a detached graph through the gem recorded in its start node. */
HAWK_EXPORT void hawk_rgx_freecode (
	hawk_rgx_node_t* code
);

#if defined(__cplusplus)
}
#endif

#endif
