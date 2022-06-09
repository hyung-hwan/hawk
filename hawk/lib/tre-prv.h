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

/*
  tre-internal.h - TRE internal definitions

This is the license, copyright notice, and disclaimer for TRE, a regex
matching package (library and tools) with support for approximate
matching.

Copyright (c) 2001-2009 Ville Laurikari <vl@iki.fi>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _HAWK_TRE_PRV_H_
#define _HAWK_TRE_PRV_H_

/* TODO: MAKE TRE WORK LIKE GNU

PATTERN: \(.\{0,1\}\)\(~[^,]*\)\([0-9]\)\(\.*\),\([^;]*\)\(;\([^;]*\(\3[^;]*\)\).*X*\1\(.*\)\)
INPUT: ~02.,3~3;0123456789;9876543210

------------------------------------------------------
samples/cmn/tre01 gives the following output. this does not seem wrong, though.

SUBMATCH[7],[8],[9].

SUBMATCH[0] = [~02.,3~3;0123456789;9876543210]
SUBMATCH[1] = []
SUBMATCH[2] = [~0]
SUBMATCH[3] = [2]
SUBMATCH[4] = [.]
SUBMATCH[5] = [3~3]
SUBMATCH[6] = [;0123456789;9876543210]
SUBMATCH[7] = [012]
SUBMATCH[8] = [2]
SUBMATCH[9] = [3456789;9876543210]

------------------------------------------------------

Using the GNU regcomp(),regexec(), the following
is printed.

#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
int main (int argc, char* argv[])
{
     regex_t tre;
     regmatch_t mat[10];
     int i;
     regcomp (&tre, argv[1], 0);
     regexec (&tre, argv[2], 10, mat, 0);
     for (i = 0; i < 10; i++)
     {
          if (mat[i].rm_so == -1) break;
          printf ("SUBMATCH[%u] = [%.*s]\n", i,(mat[i].rm_eo - mat[i].rm_so), &argv[2][mat[i].rm_so]);
     }
     regfree (&tre);
     return 0;
}

SUBMATCH[0] = [~02.,3~3;0123456789;9876543210]
SUBMATCH[1] = []
SUBMATCH[2] = [~0]
SUBMATCH[3] = [2]
SUBMATCH[4] = [.]
SUBMATCH[5] = [3~3]
SUBMATCH[6] = [;0123456789;9876543210]
SUBMATCH[7] = [0123456789]
SUBMATCH[8] = [23456789]
SUBMATCH[9] = []


------------------------------------------------------
One more example here:
$ ./tre01 "\(x*\)ab\(\(c*\1\)\(.*\)\)" "abcdefg"
Match: YES
SUBMATCH[0] = [abcdefg]
SUBMATCH[1] = []
SUBMATCH[2] = [cdefg]
SUBMATCH[3] = []
SUBMATCH[4] = [cdefg]

$ ./reg "\(x*\)ab\(\(c*\1\)\(.*\)\)" "abcdefg"
SUBMATCH[0] = [abcdefg]
SUBMATCH[1] = []
SUBMATCH[2] = [cdefg]
SUBMATCH[3] = [c]
SUBMATCH[4] = [defg]
*/

#include <hawk-tre.h>
#include <hawk-chr.h>
#include "hawk-prv.h"

#if defined(HAWK_OOCH_IS_UCH)
#	define TRE_WCHAR

/*#	define TRE_MULTIBYTE*/
/*#	define TRE_MBSTATE*/
#endif

#define TRE_REGEX_T_FIELD value
/*#define assert(x) HAWK_ASSERT(x)*/
#define assert(x)
#if defined(NULL)
#	undef NULL
#endif
#define NULL HAWK_NULL

#define tre_islower(c)  hawk_is_ooch_lower(c)
#define tre_isupper(c)  hawk_is_ooch_upper(c)
#define tre_isalpha(c)  hawk_is_ooch_alpha(c)
#define tre_isdigit(c)  hawk_is_ooch_digit(c)
#define tre_isxdigit(c) hawk_is_ooch_xdigit(c)
#define tre_isalnum(c)  hawk_is_ooch_alnum(c)

#define tre_isspace(c)  hawk_is_ooch_space(c)
#define tre_isprint(c)  hawk_is_ooch_print(c)
#define tre_isgraph(c)  hawk_is_ooch_graph(c)
#define tre_iscntrl(c)  hawk_is_ooch_cntrl(c)
#define tre_ispunct(c)  hawk_is_ooch_punct(c)
#define tre_isblank(c)  hawk_is_ooch_blank(c)

#define tre_tolower(c)  hawk_to_ooch_lower(c)
#define tre_toupper(c)  hawk_to_ooch_upper(c)

typedef hawk_ooch_t tre_char_t;
typedef hawk_ooci_t tre_cint_t;

#define size_t hawk_oow_t
#define regex_t hawk_tre_t
#define regmatch_t hawk_tre_match_t
#define reg_errcode_t hawk_errnum_t

#define REG_OK       HAWK_ENOERR
#define REG_ESPACE   HAWK_ENOMEM
#define REG_NOMATCH  HAWK_EREXNOMAT
#define REG_BADPAT   HAWK_EREXBADPAT
#define REG_ECOLLATE HAWK_EREXCOLLATE
#define REG_ECTYPE   HAWK_EREXCTYPE
#define REG_EESCAPE  HAWK_EREXESCAPE
#define REG_ESUBREG  HAWK_EREXSUBREG
#define REG_EBRACK   HAWK_EREXBRACK
#define REG_EPAREN   HAWK_EREXPAREN
#define REG_EBRACE   HAWK_EREXBRACE
#define REG_BADBR    HAWK_EREXBADBR
#define REG_ERANGE   HAWK_EREXRANGE
#define REG_BADRPT   HAWK_EREXBADRPT

/* The maximum number of iterations in a bound expression. */
#undef RE_DUP_MAX
#define RE_DUP_MAX 255

/* POSIX tre_regcomp() flags. */
#define REG_EXTENDED    HAWK_TRE_EXTENDED
#define REG_ICASE       HAWK_TRE_IGNORECASE
#define REG_NEWLINE     HAWK_TRE_NEWLINE
#define REG_NOSUB       HAWK_TRE_NOSUBREG
/* Extra tre_regcomp() flags. */
#define REG_LITERAL     HAWK_TRE_LITERAL 
#define REG_RIGHT_ASSOC HAWK_TRE_RIGHTASSOC
#define REG_UNGREEDY    HAWK_TRE_UNGREEDY 
#define REG_NOBOUND     HAWK_TRE_NOBOUND
#define REG_NONSTDEXT   HAWK_TRE_NONSTDEXT

/* POSIX tre_regexec() flags. */
#define REG_NOTBOL HAWK_TRE_NOTBOL
#define REG_NOTEOL HAWK_TRE_NOTEOL
#define REG_BACKTRACKING_MATCHER HAWK_TRE_BACKTRACKING


#define tre_strlen(c) hawk_count_oocstr(c)



/* use the noseterr version because various tre functions return 
 * REG_ESPACE upon memory shortage and the wrapper functions
 * uses the returned code to set the error number on the 
 * hawk_tre_t wrapper object */
#define xmalloc(gem,size) hawk_gem_allocmem_noseterr(gem,size)
#define xrealloc(gem,ptr,new_size) hawk_gem_reallocmem_noseterr(gem, ptr, new_size)
#define xcalloc(gem,nmemb,size) hawk_gem_callocmem_noseterr(gem, (nmemb) * (size))
#define xfree(gem,ptr) hawk_gem_freemem(gem,ptr)

/* tre-ast.h */
#define tre_ast_new_node hawk_tre_astnewnode
#define tre_ast_new_literal hawk_tre_astnewliteral
#define tre_ast_new_iter hawk_tre_astnewiter
#define tre_ast_new_union hawk_tre_astnewunion
#define tre_ast_new_catenation hawk_tre_astnewcatenation

/* tre-mem.h */
#define tre_mem_t hawk_tre_mem_t
#define tre_mem_new hawk_tre_mem_new
#define tre_mem_alloc hawk_tre_mem_alloc
#define tre_mem_calloc hawk_tre_mem_calloc
#define tre_mem_destroy hawk_tre_mem_destroy

/* tre-parse.h */
#define tre_parse hawk_tre_parse

/* tre-stack.h */
#define tre_stack_destroy hawk_tre_stackfree
#define tre_stack_new hawk_tre_stacknew
#define tre_stack_num_objects hawk_tre_stacknumobjs
#define tre_stack_pop_int hawk_tre_stackpopint
#define tre_stack_pop_voidptr hawk_tre_stackpopvoidptr
#define tre_stack_push_int hawk_tre_stackpushint
#define tre_stack_push_voidptr hawk_tre_stackpushvoidptr

/* this tre.h */
#define tre_compile hawk_tre_compile
#define tre_free hawk_tre_free
#define tre_fill_pmatch hawk_tre_fillpmatch
#define tre_tnfa_run_backtrack hawk_tre_runbacktrack
#define tre_tnfa_run_parallel hawk_tre_runparallel
#define tre_have_backrefs hawk_tre_havebackrefs

/* Define the character types and functions. */
#ifdef TRE_WCHAR
/* [HAWK]
 * the TRE code uses the int type to represent a code point
 * in various part. in fact, it uses int, long, tre_cint_t intermixedly.
 * it's not easy to switch to a single type because lit->code_max is bitwise-ORed
 * with the assertions field which is of the int type.
 *
 * if TRE_CHAR_MAX is greater than INT_MAX, some comparion fails as TRE_CHAR_MAX
 * is treated as -1. here let me define TRE_CHAR_MAX to avoid this issue.
 *
 * however, if int is 2 bytes long,TRE_CHAR_MAX becomes 32767 which is way too small
 * to represent even upper-half of the  UCS-2 codepoints.
 */
#	if (HAWK_SIZEOF_UCH_T < HAWK_SIZEOF_INT) 
#		define TRE_CHAR_MAX HAWK_TYPE_MAX(hawk_uch_t)
#	else
#		define TRE_CHAR_MAX HAWK_TYPE_MAX(int)
#	endif

/*
#	ifdef TRE_MULTIBYTE
#		define TRE_MB_CUR_MAX (hawk_getmbcurmax())
#	else 
#		define TRE_MB_CUR_MAX 1
#	endif 
*/
#else /* !TRE_WCHAR */
#	define TRE_CHAR_MAX 255
/*#	define TRE_MB_CUR_MAX 1*/
#endif /* !TRE_WCHAR */

#define DPRINT(msg) 

typedef hawk_ooch_prop_t tre_ctype_t;
#define tre_isctype(c,t) hawk_is_ooch_type(c,t)

typedef enum { STR_WIDE, STR_BYTE, STR_MBS } tre_str_type_t;

/* Returns number of bytes to add to (char *)ptr to make it
   properly aligned for the type. */
#define ALIGN(ptr,type) \
	((((hawk_uintptr_t)ptr) % HAWK_SIZEOF(type))? \
	(HAWK_SIZEOF(type) - (((hawk_uintptr_t)ptr) % HAWK_SIZEOF(type))): 0)

#undef MAX
#undef MIN
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))

/* Define STRF to the correct printf formatter for strings. */
#ifdef TRE_WCHAR
#define STRF "ls"
#else /* !TRE_WCHAR */
#define STRF "s"
#endif /* !TRE_WCHAR */

/* TNFA transition type. A TNFA state is an array of transitions,
   the terminator is a transition with NULL `state'. */
typedef struct tnfa_transition tre_tnfa_transition_t;

struct tnfa_transition
{
	/* Range of accepted characters. */
	/* HAWK indicate that code_min .. code_max is not yet negated for ^ in a bracket */
	int negate_range;
	/* END HAWK */
	tre_cint_t code_min;
	tre_cint_t code_max;

	/* Pointer to the destination state. */
	tre_tnfa_transition_t *state;
	/* ID number of the destination state. */
	int state_id;
	/* -1 terminated array of tags (or NULL). */
	int *tags;
	/* Matching parameters settings (or NULL). */
	int *params;
	/* Assertion bitmap. */
	int assertions;
	/* Assertion parameters. */
	union
	{
		/* Character class assertion. */
		tre_ctype_t class;
		/* Back reference assertion. */
		int backref;
	} u;
	/* Negative character class assertions. */
	tre_ctype_t *neg_classes;
};


/* Assertions. */
#define ASSERT_AT_BOL		  1   /* Beginning of line. */
#define ASSERT_AT_EOL		  2   /* End of line. */
#define ASSERT_CHAR_CLASS	  4   /* Character class in `class'. */
#define ASSERT_CHAR_CLASS_NEG	  8   /* Character classes in `neg_classes'. */
#define ASSERT_AT_BOW		 16   /* Beginning of word. */
#define ASSERT_AT_EOW		 32   /* End of word. */
#define ASSERT_AT_WB		 64   /* Word boundary. */
#define ASSERT_AT_WB_NEG	128   /* Not a word boundary. */
#define ASSERT_BACKREF		256   /* A back reference in `backref'. */
#define ASSERT_LAST		256

/* Tag directions. */
typedef enum
{
	TRE_TAG_MINIMIZE = 0,
	TRE_TAG_MAXIMIZE = 1
} tre_tag_direction_t;

/* Parameters that can be changed dynamically while matching. */
typedef enum
{
	TRE_PARAM_COST_INS	    = 0,
	TRE_PARAM_COST_DEL	    = 1,
	TRE_PARAM_COST_SUBST	    = 2,
	TRE_PARAM_COST_MAX	    = 3,
	TRE_PARAM_MAX_INS	    = 4,
	TRE_PARAM_MAX_DEL	    = 5,
	TRE_PARAM_MAX_SUBST	    = 6,
	TRE_PARAM_MAX_ERR	    = 7,
	TRE_PARAM_DEPTH	    = 8,
	TRE_PARAM_LAST	    = 9
} tre_param_t;

/* Unset matching parameter */
#define TRE_PARAM_UNSET -1

/* Signifies the default matching parameter value. */
#define TRE_PARAM_DEFAULT -2

/* Instructions to compute submatch register values from tag values
   after a successful match.  */
struct tre_submatch_data
{
	/* Tag that gives the value for rm_so (submatch start offset). */
	int so_tag;
	/* Tag that gives the value for rm_eo (submatch end offset). */
	int eo_tag;
	/* List of submatches this submatch is contained in. */
	int *parents;
};

typedef struct tre_submatch_data tre_submatch_data_t;


/* TNFA definition. */
typedef struct tnfa tre_tnfa_t;

struct tnfa
{
	tre_tnfa_transition_t *transitions;
	unsigned int num_transitions;
	tre_tnfa_transition_t *initial;
	tre_tnfa_transition_t *final;
	tre_submatch_data_t *submatch_data;
#if 0
	char *firstpos_chars;
#endif
	int first_char;
	unsigned int num_submatches;
	tre_tag_direction_t *tag_directions;
	int *minimal_tags;
	int num_tags;
	int num_minimals;
	int end_tag;
	int num_states;
	int cflags;
	int have_backrefs;
	int have_approx;
	int params_depth;
};


int tre_compile (regex_t *preg, const tre_char_t *regex, size_t n, int cflags);

void tre_free (regex_t *preg);

void tre_fill_pmatch(
	size_t nmatch, regmatch_t pmatch[], int cflags,
	const tre_tnfa_t *tnfa, int *tags, int match_eo);

reg_errcode_t tre_tnfa_run_backtrack(
	hawk_gem_t* gem, const tre_tnfa_t *tnfa, const void *string,
	int len, tre_str_type_t type, int *match_tags,
	int eflags, int *match_end_ofs);


reg_errcode_t tre_tnfa_run_parallel(
	hawk_gem_t* gem, const tre_tnfa_t *tnfa, const void *string, int len,
	tre_str_type_t type, int *match_tags, int eflags,
	int *match_end_ofs);


#endif

/* EOF */
