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
  tre-ast.h - Abstract syntax tree (AST) definitions

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


#ifndef _HAWK_LIB_CMN_TRE_AST_H_
#define _HAWK_LIB_CMN_TRE_AST_H_

#include "tre-prv.h"
#include "tre-mem.h"
#include "tre-compile.h"

/* The different AST node types. */
typedef enum
{
	LITERAL,
	CATENATION,
	ITERATION,
	UNION
} tre_ast_type_t;

/* Special subtypes of TRE_LITERAL. */
#define EMPTY	  -1   /* Empty leaf (denotes empty string). */
#define ASSERTION -2   /* Assertion leaf. */
#define TAG	  -3   /* Tag leaf. */
#define BACKREF	  -4   /* Back reference leaf. */
#define PARAMETER -5   /* Parameter. */

#define IS_SPECIAL(x)	((x)->code_min < 0)
#define IS_EMPTY(x)	((x)->code_min == EMPTY)
#define IS_ASSERTION(x) ((x)->code_min == ASSERTION)
#define IS_TAG(x)	((x)->code_min == TAG)
#define IS_BACKREF(x)	((x)->code_min == BACKREF)
#define IS_PARAMETER(x) ((x)->code_min == PARAMETER)


/* A generic AST node.  All AST nodes consist of this node on the top
   level with `obj' pointing to the actual content. */
typedef struct
{
	tre_ast_type_t type;   /* Type of the node. */
	void *obj;             /* Pointer to actual node. */
	int nullable;
	int submatch_id;
	int num_submatches;
	int num_tags;
	tre_pos_and_tags_t *firstpos;
	tre_pos_and_tags_t *lastpos;
} tre_ast_node_t;


/* A "literal" node.  These are created for assertions, back references,
   tags, matching parameter settings, and all expressions that match one
   character. */
typedef struct
{
	long code_min;
	long code_max;
	int position;
	union
	{
		tre_ctype_t class;
		int *params;
	} u;
	tre_ctype_t *neg_classes;
} tre_literal_t;

/* A "catenation" node.	 These are created when two regexps are concatenated.
   If there are more than one subexpressions in sequence, the `left' part
   holds all but the last, and `right' part holds the last subexpression
   (catenation is left associative). */
typedef struct
{
	tre_ast_node_t *left;
	tre_ast_node_t *right;
} tre_catenation_t;

/* An "iteration" node.	 These are created for the "*", "+", "?", and "{m,n}"
   operators. */
typedef struct
{
	/* Subexpression to match. */
	tre_ast_node_t *arg;
	/* Minimum number of consecutive matches. */
	int min;
	/* Maximum number of consecutive matches. */
	int max;
	/* If 0, match as many characters as possible, if 1 match as few as
	   possible.	Note that this does not always mean the same thing as
	   matching as many/few repetitions as possible. */
	unsigned int minimal:1;
	/* Approximate matching parameters (or NULL). */
	int *params;
} tre_iteration_t;

/* An "union" node.  These are created for the "|" operator. */
typedef struct
{
	tre_ast_node_t *left;
	tre_ast_node_t *right;
} tre_union_t;

tre_ast_node_t* tre_ast_new_node(tre_mem_t mem, tre_ast_type_t type, size_t size);

tre_ast_node_t* tre_ast_new_literal(tre_mem_t mem, int code_min, int code_max, int position);

tre_ast_node_t* tre_ast_new_iter(tre_mem_t mem, tre_ast_node_t *arg, int min, int max, int minimal);

tre_ast_node_t* tre_ast_new_union(tre_mem_t mem, tre_ast_node_t *left, tre_ast_node_t *right);

tre_ast_node_t* tre_ast_new_catenation(tre_mem_t mem, tre_ast_node_t *left, tre_ast_node_t *right);

#ifdef TRE_DEBUG
void tre_ast_print(tre_ast_node_t *tree);
/* XXX - rethink AST printing API */
void tre_print_params(int *params);
#endif /* TRE_DEBUG */

#endif /* TRE_AST_H */

/* EOF */
