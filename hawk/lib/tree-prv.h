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

#ifndef _HAWK_TREE_PRV_H_
#define _HAWK_TREE_PRV_H_


enum hawk_in_type_t
{
	/* the order of these values match 
	 * __in_type_map and __in_opt_map in rio.c */

	HAWK_IN_PIPE,
	HAWK_IN_RWPIPE,
	HAWK_IN_FILE,
	HAWK_IN_CONSOLE
};
typedef enum hawk_in_type_t hawk_in_type_t;


enum hawk_out_type_t
{
	/* the order of these values match 
	 * __out_type_map and __out_opt_map in rio.c */

	HAWK_OUT_PIPE,
	HAWK_OUT_RWPIPE, /* dual direction pipe */
	HAWK_OUT_FILE,
	HAWK_OUT_APFILE, /* file for appending */
	HAWK_OUT_CONSOLE
};
typedef enum hawk_out_type_t hawk_out_type_t;

typedef struct hawk_nde_blk_t       hawk_nde_blk_t;
typedef struct hawk_nde_grp_t       hawk_nde_grp_t;
typedef struct hawk_nde_ass_t       hawk_nde_ass_t;
typedef struct hawk_nde_exp_t       hawk_nde_exp_t;
typedef struct hawk_nde_cnd_t       hawk_nde_cnd_t;
typedef struct hawk_nde_pos_t       hawk_nde_pos_t;

typedef struct hawk_nde_char_t      hawk_nde_char_t;
typedef struct hawk_nde_bchr_t      hawk_nde_bchr_t;
typedef struct hawk_nde_int_t       hawk_nde_int_t;
typedef struct hawk_nde_flt_t       hawk_nde_flt_t;
typedef struct hawk_nde_str_t       hawk_nde_str_t;
typedef struct hawk_nde_mbs_t       hawk_nde_mbs_t;
typedef struct hawk_nde_rex_t       hawk_nde_rex_t;
typedef struct hawk_nde_fun_t       hawk_nde_fun_t;
typedef struct hawk_nde_xnil_t      hawk_nde_xnil_t;

typedef struct hawk_nde_var_t       hawk_nde_var_t;
typedef struct hawk_nde_fncall_t    hawk_nde_fncall_t;
typedef struct hawk_nde_getline_t   hawk_nde_getline_t;

typedef struct hawk_nde_if_t        hawk_nde_if_t;
typedef struct hawk_nde_while_t     hawk_nde_while_t;
typedef struct hawk_nde_for_t       hawk_nde_for_t;
typedef struct hawk_nde_forin_t     hawk_nde_forin_t;
typedef struct hawk_nde_break_t     hawk_nde_break_t;
typedef struct hawk_nde_continue_t  hawk_nde_continue_t;
typedef struct hawk_nde_return_t    hawk_nde_return_t;
typedef struct hawk_nde_exit_t      hawk_nde_exit_t;
typedef struct hawk_nde_next_t      hawk_nde_next_t;
typedef struct hawk_nde_nextfile_t  hawk_nde_nextfile_t;
typedef struct hawk_nde_delete_t    hawk_nde_delete_t;
typedef struct hawk_nde_reset_t     hawk_nde_reset_t;
typedef struct hawk_nde_print_t     hawk_nde_print_t;

/* HAWK_NDE_BLK - block statement including top-level blocks */
struct hawk_nde_blk_t
{
	HAWK_NDE_HDR;
	hawk_oow_t nlcls; /* number of local variables */
	hawk_oow_t org_nlcls; /* the original number of local variables before pushing to the top-level block  */
	hawk_oow_t outer_nlcls; /* the number of local variables in the outer blocks accumulated */
	hawk_nde_t* body;
};

/* HAWK_NDE_GRP - expression group */
struct hawk_nde_grp_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* body;
};

/* HAWK_NDE_ASS - assignment */
struct hawk_nde_ass_t
{
	HAWK_NDE_HDR;
	int opcode;
	hawk_nde_t* left;
	hawk_nde_t* right;
};

/* HAWK_NDE_EXP_BIN, HAWK_NDE_EXP_UNR, 
 * HAWK_NDE_EXP_INCPRE, HAWK_AW_NDE_EXP_INCPST */
struct hawk_nde_exp_t
{
	HAWK_NDE_HDR;
	int opcode;
	hawk_nde_t* left;
	hawk_nde_t* right; /* HAWK_NULL for UNR, INCPRE, INCPST */
};

/* HAWK_NDE_CND */
struct hawk_nde_cnd_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* test;
	hawk_nde_t* left;
	hawk_nde_t* right;
};

/* HAWK_NDE_POS - positional - $1, $2, $x, etc */
struct hawk_nde_pos_t  
{
	HAWK_NDE_HDR;
	hawk_nde_t* val;
};

/* HAWK_NDE_CHAR */
struct hawk_nde_char_t
{
	HAWK_NDE_HDR;
	hawk_ooch_t val;
};


/* HAWK_NDE_BCHR */
struct hawk_nde_bchr_t
{
	HAWK_NDE_HDR;
	hawk_bch_t val;
};

/* HAWK_NDE_INT */
struct hawk_nde_int_t
{
	HAWK_NDE_HDR;
	hawk_int_t val;
	hawk_ooch_t* str; 
	hawk_oow_t len;
};

/* HAWK_NDE_FLT */
struct hawk_nde_flt_t
{
	HAWK_NDE_HDR;
	hawk_flt_t val;
	hawk_ooch_t* str;
	hawk_oow_t len;
};

/* HAWK_NDE_STR */
struct hawk_nde_str_t
{
	HAWK_NDE_HDR;
	hawk_ooch_t* ptr;
	hawk_oow_t len;
};

/* HAWK_NDE_MBS */
struct hawk_nde_mbs_t
{
	HAWK_NDE_HDR;
	hawk_bch_t* ptr;
	hawk_oow_t   len;
};

/* HAWK_NDE_REX */
struct hawk_nde_rex_t
{
	HAWK_NDE_HDR;
	hawk_oocs_t  str;
	hawk_tre_t*  code[2]; /* [0]: case sensitive, [1]: case insensitive */
};

struct hawk_nde_xnil_t
{
	HAWK_NDE_HDR;
};

/* HAWK_NDE_FUN - function as a value */
struct hawk_nde_fun_t
{
	HAWK_NDE_HDR;
	hawk_oocs_t name; /* function name */
	hawk_fun_t* funptr; /* HAWK_NULL or actual pointer */
};

/* HAWK_NDE_NAMED, HAWK_NDE_GBL, 
 * HAWK_NDE_LCL, HAWK_NDE_ARG 
 * HAWK_NDE_NAMEDIDX, HAWK_NDE_GBLIDX, 
 * HAWK_NDE_LCLIDX, HAWK_NDE_ARGIDX */
struct hawk_nde_var_t
{
	HAWK_NDE_HDR;
	struct 
	{
		hawk_oocs_t name;
		hawk_oow_t idxa;
	} id;
	hawk_nde_t* idx; /* HAWK_NULL for non-XXXXIDX */
};

/* HAWK_NDE_FNCALL_FNC, HAWK_NDE_FNCALL_FUN, HAWK_NDE_FNCALL_VAR */
struct hawk_nde_fncall_t
{
	HAWK_NDE_HDR;
	union
	{
		struct
		{
			hawk_oocs_t name;
			hawk_fun_t* fun; /* cache it */
		} fun;

		/* minimum information of a intrinsic function 
		 * needed during run-time. */
		struct
		{
			hawk_fnc_info_t info;
			hawk_fnc_spec_t spec;
		} fnc;

		struct
		{
			hawk_nde_var_t* var;
		} var;
	} u;
	hawk_nde_t* args;
	hawk_oow_t nargs;
	hawk_oow_t arg_base; /* special field set by hawk::call() and handled by hawk_rtx_evalcall() */
};

/* HAWK_NDE_GETLINE */
struct hawk_nde_getline_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* var;
	int mbs;
	hawk_in_type_t in_type; /* HAWK_IN_CONSOLE, HAWK_IN_FILE, etc */
	hawk_nde_t* in;
};

/* HAWK_NDE_IF */
struct hawk_nde_if_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* test;
	hawk_nde_t* then_part;
	hawk_nde_t* else_part; /* optional */
};

/* HAWK_NDE_WHILE, HAWK_NDE_DOWHILE */
struct hawk_nde_while_t
{
	HAWK_NDE_HDR; 
	hawk_nde_t* test;
	hawk_nde_t* body;
};

/* HAWK_NDE_FOR */
struct hawk_nde_for_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* init; /* optional */
	hawk_nde_t* test; /* optional */
	hawk_nde_t* incr; /* optional */
	hawk_nde_t* body;
};

/* HAWK_NDE_FORIN */
struct hawk_nde_forin_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* test;
	hawk_nde_t* body;
};

/* HAWK_NDE_BREAK */
struct hawk_nde_break_t
{
	HAWK_NDE_HDR;
};

/* HAWK_NDE_CONTINUE */
struct hawk_nde_continue_t
{
	HAWK_NDE_HDR;
};

/* HAWK_NDE_RETURN */
struct hawk_nde_return_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* val; /* optional (no return code if HAWK_NULL) */	
};

/* HAWK_NDE_EXIT */
struct hawk_nde_exit_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* val; /* optional (no exit code if HAWK_NULL) */
	int abort;
};

/* HAWK_NDE_NEXT */
struct hawk_nde_next_t
{
	HAWK_NDE_HDR;
};

/* HAWK_NDE_NEXTFILE */
struct hawk_nde_nextfile_t
{
	HAWK_NDE_HDR;
	int out;
};

/* HAWK_NDE_DELETE */
struct hawk_nde_delete_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* var;
};

/* HAWK_NDE_RESET */
struct hawk_nde_reset_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* var;
};

/* HAWK_NDE_PRINT */
struct hawk_nde_print_t
{
	HAWK_NDE_HDR;
	hawk_nde_t* args;
	hawk_out_type_t out_type; /* HAWK_OUT_XXX */
	hawk_nde_t* out;
};

#if defined(__cplusplus)
extern "C" {
#endif

/* print the entire tree */
int hawk_prnpt (hawk_t* hawk, hawk_nde_t* tree);
/* print a single top-level node */
int hawk_prnnde (hawk_t* hawk, hawk_nde_t* node); 
/* print the pattern part */
int hawk_prnptnpt (hawk_t* hawk, hawk_nde_t* tree);

void hawk_clrpt (hawk_t* hawk, hawk_nde_t* tree);

#if defined(__cplusplus)
}
#endif

#endif
