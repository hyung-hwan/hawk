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
  tre-parse.c - Regexp parser definitions

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

#ifndef _HAWK_LIB_TRE_PARSE_H_
#define _HAWK_LIB_TRE_PARSE_H_

/* Parse context. */
typedef struct
{
	/* Memory allocator.	The AST is allocated using this. */
	tre_mem_t mem;
	/* Stack used for keeping track of regexp syntax. */
	tre_stack_t *stack;
	/* The parse result. */
	tre_ast_node_t *result;
	/* The regexp to parse and its length. */
	const tre_char_t *re;
	/* The first character of the entire regexp. */
	const tre_char_t *re_start;
	/* The first character after the end of the regexp. */
	const tre_char_t *re_end;
	/* HAWK: changed int to hawk_oow_t */
	/*int len;*/
	hawk_oow_t len;
	/* END HAWK */
	/* Current submatch ID. */
	int submatch_id;
	/* Current position (number of literal). */
	int position;
	/* The highest back reference or -1 if none seen so far. */
	int max_backref;
	/* This flag is set if the regexp uses approximate matching. */
	int have_approx;
	/* Compilation flags. */
	int cflags;
	/* If this flag is set the top-level submatch is not captured. */
	int nofirstsub;
	/* The currently set approximate matching parameters. */
	int params[TRE_PARAM_LAST];
} tre_parse_ctx_t;

/* Parses a wide character regexp pattern into a syntax tree.  This parser
   handles both syntaxes (BRE and ERE), including the TRE extensions. */
reg_errcode_t tre_parse(tre_parse_ctx_t *ctx);

#endif /* TRE_PARSE_H */

/* EOF */
