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
  tre-ast.c - Abstract syntax tree (AST) routines

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

#include "tre-ast.h"

tre_ast_node_t *
tre_ast_new_node(tre_mem_t mem, tre_ast_type_t type, size_t size)
{
	tre_ast_node_t *node;

	node = tre_mem_calloc(mem, sizeof(*node));
	if (!node) return NULL;
	node->obj = tre_mem_calloc(mem, size);
	if (!node->obj) return NULL;
	node->type = type;
	node->nullable = -1;
	node->submatch_id = -1;

	return node;
}

tre_ast_node_t * tre_ast_new_literal(tre_mem_t mem, int code_min, int code_max, int position)
{
	tre_ast_node_t *node;
	tre_literal_t *lit;

	node = tre_ast_new_node(mem, LITERAL, sizeof(tre_literal_t));
	if (!node) return NULL;
	lit = node->obj;
	lit->code_min = code_min;
	lit->code_max = code_max;
	lit->position = position;

	return node;
}

tre_ast_node_t *
tre_ast_new_iter(tre_mem_t mem, tre_ast_node_t *arg, int min, int max,
                 int minimal)
{
	tre_ast_node_t *node;
	tre_iteration_t *iter;

	node = tre_ast_new_node(mem, ITERATION, sizeof(tre_iteration_t));
	if (!node) return NULL;
	iter = node->obj;
	iter->arg = arg;
	iter->min = min;
	iter->max = max;
	iter->minimal = minimal;
	node->num_submatches = arg->num_submatches;

	return node;
}

tre_ast_node_t *
tre_ast_new_union(tre_mem_t mem, tre_ast_node_t *left, tre_ast_node_t *right)
{
	tre_ast_node_t *node;

	node = tre_ast_new_node(mem, UNION, sizeof(tre_union_t));
	if (node == NULL) return NULL;
	((tre_union_t *)node->obj)->left = left;
	((tre_union_t *)node->obj)->right = right;
	node->num_submatches = left->num_submatches + right->num_submatches;

	return node;
}

tre_ast_node_t *
tre_ast_new_catenation(tre_mem_t mem, tre_ast_node_t *left,
                       tre_ast_node_t *right)
{
	tre_ast_node_t *node;

	node = tre_ast_new_node(mem, CATENATION, sizeof(tre_catenation_t));
	if (node == NULL) return NULL;
	((tre_catenation_t *)node->obj)->left = left;
	((tre_catenation_t *)node->obj)->right = right;
	node->num_submatches = left->num_submatches + right->num_submatches;

	return node;
}

#ifdef TRE_DEBUG

static void
tre_findent(FILE *stream, int i)
{
	while (i-- > 0)
		fputc(' ', stream);
}

void
tre_print_params(int *params)
{
	int i;
	if (params)
	{
		DPRINT(("params ["));
		for (i = 0; i < TRE_PARAM_LAST; i++)
		{
			if (params[i] == TRE_PARAM_UNSET)
				DPRINT(("unset"));
			else if (params[i] == TRE_PARAM_DEFAULT)
				DPRINT(("default"));
			else
				DPRINT(("%d", params[i]));
			if (i < TRE_PARAM_LAST - 1)
				DPRINT((", "));
		}
		DPRINT(("]"));
	}
}

static void
tre_do_print(FILE *stream, tre_ast_node_t *ast, int indent)
{
	int code_min, code_max, pos;
	int num_tags = ast->num_tags;
	tre_literal_t *lit;
	tre_iteration_t *iter;

	tre_findent(stream, indent);
	switch (ast->type)
	{
	case LITERAL:
		lit = ast->obj;
		code_min = lit->code_min;
		code_max = lit->code_max;
		pos = lit->position;
		if (IS_EMPTY(lit))
		{
			fprintf(stream, "literal empty\n");
		}
		else if (IS_ASSERTION(lit))
		{
			int i;
			char *assertions[] = { "bol", "eol", "ctype", "!ctype",
			                       "bow", "eow", "wb", "!wb"
			                     };
			if (code_max >= ASSERT_LAST << 1)
				assert(0);
			fprintf(stream, "assertions: ");
			for (i = 0; (1 << i) <= ASSERT_LAST; i++)
				if (code_max & (1 << i))
					fprintf(stream, "%s ", assertions[i]);
			fprintf(stream, "\n");
		}
		else if (IS_TAG(lit))
		{
			fprintf(stream, "tag %d\n", code_max);
		}
		else if (IS_BACKREF(lit))
		{
			fprintf(stream, "backref %d, pos %d\n", code_max, pos);
		}
		else if (IS_PARAMETER(lit))
		{
			tre_print_params(lit->u.params);
			fprintf(stream, "\n");
		}
		else
		{
			fprintf(stream, "literal (%c, %c) (%d, %d), pos %d, sub %d, "
			        "%d tags\n", (int)code_min, (int)code_max, (int)code_min, (int)code_max, pos,
			        ast->submatch_id, num_tags);
		}
		break;
	case ITERATION:
		iter = ast->obj;
		fprintf(stream, "iteration {%d, %d}, sub %d, %d tags, %s\n",
		        iter->min, iter->max, ast->submatch_id, num_tags,
		        iter->minimal ? "minimal" : "greedy");
		tre_do_print(stream, iter->arg, indent + 2);
		break;
	case UNION:
		fprintf(stream, "union, sub %d, %d tags\n", ast->submatch_id, num_tags);
		tre_do_print(stream, ((tre_union_t *)ast->obj)->left, indent + 2);
		tre_do_print(stream, ((tre_union_t *)ast->obj)->right, indent + 2);
		break;
	case CATENATION:
		fprintf(stream, "catenation, sub %d, %d tags\n", ast->submatch_id,
		        num_tags);
		tre_do_print(stream, ((tre_catenation_t *)ast->obj)->left, indent + 2);
		tre_do_print(stream, ((tre_catenation_t *)ast->obj)->right, indent + 2);
		break;
	default:
		assert(0);
		break;
	}
}

static void
tre_ast_fprint(FILE *stream, tre_ast_node_t *ast)
{
	tre_do_print(stream, ast, 0);
}

void
tre_ast_print(tre_ast_node_t *tree)
{
	printf("AST:\n");
	tre_ast_fprint(stdout, tree);
}

#endif /* TRE_DEBUG */

/* EOF */
