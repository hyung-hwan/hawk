/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#ifndef _HAWK_RUN_PRV_H_
#define _HAWK_RUN_PRV_H_

enum hawk_assop_type_t
{
	/* if you change this, you have to change assop_str in tree.c.
	 * synchronize it wit: 
	 *   - binop_func in eval_assignment of run.c 
	 *   - assop in assing_to_opcode of parse.c
	 *   - TOK_XXX_ASSN in tok_t in parse.c
	 *   - assop_str in tree.c
	 */
	HAWK_ASSOP_NONE, 
	HAWK_ASSOP_PLUS,   /* += */
	HAWK_ASSOP_MINUS,  /* -= */
	HAWK_ASSOP_MUL,    /* *= */
	HAWK_ASSOP_DIV,    /* /= */
	HAWK_ASSOP_IDIV,   /* //= */
	HAWK_ASSOP_MOD,    /* %= */
	HAWK_ASSOP_EXP,    /* **= */
	HAWK_ASSOP_CONCAT, /* %%= */
	HAWK_ASSOP_RS,     /* >>= */
	HAWK_ASSOP_LS,     /* <<= */
	HAWK_ASSOP_BAND,   /* &= */
	HAWK_ASSOP_BXOR,   /* ^^= */
	HAWK_ASSOP_BOR     /* |= */
};

enum hawk_binop_type_t
{
	/* if you change this, you have to change 
	 * binop_str in tree.c and binop_func in run.c accordingly. */ 
	HAWK_BINOP_LOR,
	HAWK_BINOP_LAND,
	HAWK_BINOP_IN,

	HAWK_BINOP_BOR,
	HAWK_BINOP_BXOR,
	HAWK_BINOP_BAND,

	HAWK_BINOP_TEQ,
	HAWK_BINOP_TNE,
	HAWK_BINOP_EQ,
	HAWK_BINOP_NE,
	HAWK_BINOP_GT,
	HAWK_BINOP_GE,
	HAWK_BINOP_LT,
	HAWK_BINOP_LE,

	HAWK_BINOP_LS,
	HAWK_BINOP_RS,

	HAWK_BINOP_PLUS,
	HAWK_BINOP_MINUS,
	HAWK_BINOP_MUL,
	HAWK_BINOP_DIV,
	HAWK_BINOP_IDIV,
	HAWK_BINOP_MOD,
	HAWK_BINOP_EXP,

	HAWK_BINOP_CONCAT,
	HAWK_BINOP_MA,
	HAWK_BINOP_NM
};

enum hawk_unrop_type_t
{
	/* if you change this, you have to change 
	 * __unrop_str in tree.c accordingly. */ 
	HAWK_UNROP_PLUS,
	HAWK_UNROP_MINUS,
	HAWK_UNROP_LNOT,
	HAWK_UNROP_BNOT
};

enum hawk_incop_type_t
{
	/* if you change this, you have to change 
	 * __incop_str in tree.c accordingly. */ 
	HAWK_INCOP_PLUS,
	HAWK_INCOP_MINUS
};

#if defined(__cplusplus)
extern "C" {
#endif

hawk_ooch_t* hawk_rtx_format (
	hawk_rtx_t*        rtx,
	hawk_ooecs_t*      out, 
	hawk_ooecs_t*      fbu,
	const hawk_ooch_t* fmt, 
	hawk_oow_t         fmt_len, 
	hawk_oow_t         nargs_on_stack, 
	hawk_nde_t*        args, 
	hawk_oow_t*        len
);

hawk_bch_t* hawk_rtx_formatmbs (
	hawk_rtx_t*        rtx,
	hawk_becs_t*       out, 
	hawk_becs_t*       fbu,
	const hawk_bch_t*  fmt, 
	hawk_oow_t         fmt_len, 
	hawk_oow_t         nargs_on_stack, 
	hawk_nde_t*        args, 
	hawk_oow_t*        len
);

int hawk_rtx_cmpval (
	hawk_rtx_t*   rtx,
	hawk_val_t*   left,
	hawk_val_t*   right,
	int*          ret
);

#if defined(__cplusplus)
}
#endif

#endif
