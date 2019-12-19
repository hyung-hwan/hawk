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

#include "hawk-prv.h"

#define PRINT_IOERR -99

#define CMP_ERROR -99
#define DEF_BUF_CAPA 256
#define RTX_STACK_INCREMENT 512

/* Don't forget to grow IDXBUFSIZE if hawk_int_t is very large */
#if (HAWK_SIZEOF_INT_T <= 16) /* 128 bits */
#	define IDXBUFSIZE 64
#elif (HAWK_SIZEOF_INT_T <= 32) /* 256 bits */
#	define IDXBUFSIZE 128
#elif (HAWK_SIZEOF_INT_T <= 64) /* 512 bits */
#	define IDXBUFSIZE 192
#elif (HAWK_SIZEOF_INT_T <= 128) /* 1024 bits */
#	define IDXBUFSIZE 384
#elif (HAWK_SIZEOF_INT_T <= 256) /* 2048 bits */
#	define IDXBUFSIZE 640
#else
#	error unsupported. hawk_int_t too big
#endif

enum exit_level_t
{
	EXIT_NONE,
	EXIT_BREAK,
	EXIT_CONTINUE,
	EXIT_FUNCTION,
	EXIT_NEXT,
	EXIT_GLOBAL,
	EXIT_ABORT
};

struct pafv_t
{
	hawk_val_t**       args;
	hawk_oow_t         nargs;
	const hawk_ooch_t* argspec;
};

#define DEFAULT_CONVFMT  HAWK_T("%.6g")
#define DEFAULT_OFMT     HAWK_T("%.6g")
#define DEFAULT_OFS      HAWK_T(" ")
#define DEFAULT_ORS      HAWK_T("\n")
#define DEFAULT_ORS_CRLF HAWK_T("\r\n")
#define DEFAULT_SUBSEP   HAWK_T("\034")

/* the index of a positional variable should be a positive interger
 * equal to or less than the maximum value of the type by which
 * the index is represented. but it has an extra check against the
 * maximum value of hawk_oow_t as the reference is represented
 * in a pointer variable of hawk_val_ref_t and sizeof(void*) is
 * equal to sizeof(hawk_oow_t). */
#define IS_VALID_POSIDX(idx) \
	((idx) >= 0 && \
	 (idx) <= HAWK_TYPE_MAX(hawk_int_t) && \
	 (idx) <= HAWK_TYPE_MAX(hawk_oow_t))

#define SETERR_ARGX_LOC(rtx,code,ea,loc) \
	hawk_rtx_seterror ((rtx), (code), (ea), (loc))

#define CLRERR(rtx) SETERR_ARGX_LOC(rtx,HAWK_ENOERR,HAWK_NULL,HAWK_NULL)

#define SETERR_ARG_LOC(rtx,code,ep,el,loc) \
	do { \
		hawk_oocs_t __ea; \
		__ea.len = (el); __ea.ptr = (ep); \
		hawk_rtx_seterror ((rtx), (code), &__ea, (loc)); \
	} while (0)

#define SETERR_ARGX(rtx,code,ea) SETERR_ARGX_LOC(rtx,code,ea,HAWK_NULL)
#define SETERR_ARG(rtx,code,ep,el) SETERR_ARG_LOC(rtx,code,ep,el,HAWK_NULL)
#define SETERR_LOC(rtx,code,loc) SETERR_ARGX_LOC(rtx,code,HAWK_NULL,loc)
#define SETERR_COD(rtx,code) SETERR_ARGX_LOC(rtx,code,HAWK_NULL,HAWK_NULL)

#define ADJERR_LOC(rtx,l) do { (rtx)->_gem.errloc = *(l); } while (0)

static hawk_oow_t push_arg_from_vals (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data);
static hawk_oow_t push_arg_from_nde (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data);

static int init_rtx (hawk_rtx_t* rtx, hawk_t* awk, hawk_rio_cbs_t* rio);
static void fini_rtx (hawk_rtx_t* rtx, int fini_globals);

static int init_globals (hawk_rtx_t* rtx);
static void refdown_globals (hawk_rtx_t* run, int pop);

static int run_pblocks  (hawk_rtx_t* rtx);
static int run_pblock_chain (hawk_rtx_t* rtx, hawk_chain_t* cha);
static int run_pblock (hawk_rtx_t* rtx, hawk_chain_t* cha, hawk_oow_t bno);
static int run_block (hawk_rtx_t* rtx, hawk_nde_blk_t* nde);
static int run_statement (hawk_rtx_t* rtx, hawk_nde_t* nde);
static int run_if (hawk_rtx_t* rtx, hawk_nde_if_t* nde);
static int run_while (hawk_rtx_t* rtx, hawk_nde_while_t* nde);
static int run_for (hawk_rtx_t* rtx, hawk_nde_for_t* nde);
static int run_foreach (hawk_rtx_t* rtx, hawk_nde_foreach_t* nde);
static int run_break (hawk_rtx_t* rtx, hawk_nde_break_t* nde);
static int run_continue (hawk_rtx_t* rtx, hawk_nde_continue_t* nde);
static int run_return (hawk_rtx_t* rtx, hawk_nde_return_t* nde);
static int run_exit (hawk_rtx_t* rtx, hawk_nde_exit_t* nde);
static int run_next (hawk_rtx_t* rtx, hawk_nde_next_t* nde);
static int run_nextfile (hawk_rtx_t* rtx, hawk_nde_nextfile_t* nde);
static int run_delete (hawk_rtx_t* rtx, hawk_nde_delete_t* nde);
static int run_reset (hawk_rtx_t* rtx, hawk_nde_reset_t* nde);
static int run_print (hawk_rtx_t* rtx, hawk_nde_print_t* nde);
static int run_printf (hawk_rtx_t* rtx, hawk_nde_print_t* nde);

static int output_formatted (
	hawk_rtx_t* run, int out_type, const hawk_ooch_t* dst, 
	const hawk_ooch_t* fmt, hawk_oow_t fmt_len, hawk_nde_t* args);
static int output_formatted_bytes (
	hawk_rtx_t* run, int out_type, const hawk_ooch_t* dst, 
	const hawk_bch_t* fmt, hawk_oow_t fmt_len, hawk_nde_t* args);

static hawk_val_t* eval_expression (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_expression0 (hawk_rtx_t* rtx, hawk_nde_t* nde);

static hawk_val_t* eval_group (hawk_rtx_t* rtx, hawk_nde_t* nde);

static hawk_val_t* eval_assignment (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* do_assignment (hawk_rtx_t* rtx, hawk_nde_t* var, hawk_val_t* val);
static hawk_val_t* do_assignment_nonidx (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val);
static hawk_val_t* do_assignment_idx (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val);
static hawk_val_t* do_assignment_pos (hawk_rtx_t* rtx, hawk_nde_pos_t* pos, hawk_val_t* val);

static hawk_val_t* eval_binary (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_binop_lor (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right);
static hawk_val_t* eval_binop_land (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right);
static hawk_val_t* eval_binop_in (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right);
static hawk_val_t* eval_binop_bor (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_bxor (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_band (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);

static hawk_val_t* eval_binop_teq (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_tne (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_eq (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_ne (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_gt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_ge (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_lt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_le (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_lshift (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_rshift (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_plus (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_minus (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_mul (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_div (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_idiv (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_mod (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_exp (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_concat (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
static hawk_val_t* eval_binop_ma (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right);
static hawk_val_t* eval_binop_nm (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right);

static hawk_val_t* eval_unary (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_incpre (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_incpst (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_cnd (hawk_rtx_t* rtx, hawk_nde_t* nde);

static hawk_val_t* eval_fncall_fun_ex (hawk_rtx_t* rtx, hawk_nde_t* nde, void(*errhandler)(void*), void* eharg);

static hawk_val_t* eval_fncall_fnc (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_fncall_fun (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_fncall_var (hawk_rtx_t* rtx, hawk_nde_t* nde);

static hawk_val_t* __eval_call (
	hawk_rtx_t* rtx,
	hawk_nde_t* nde, 
	hawk_fun_t* fun, 
	hawk_oow_t(*argpusher)(hawk_rtx_t*,hawk_nde_fncall_t*,void*),
	void* apdata, /* data to argpusher */
	void(*errhandler)(void*),
	void* eharg);

static int get_reference (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_val_t*** ref);
static hawk_val_t** get_reference_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* nde, hawk_val_t** val);

static hawk_val_t* eval_int (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_flt (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_str (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_mbs (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_rex (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_fun (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_named (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_gbl (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_lcl (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_arg (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_namedidx (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_gblidx (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_lclidx (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_argidx (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_pos (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_getline (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_print (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_printf (hawk_rtx_t* rtx, hawk_nde_t* nde);

static int __raw_push (hawk_rtx_t* rtx, void* val);
#define __raw_pop(rtx) \
	do { \
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), (rtx)->stack_top > (rtx)->stack_base); \
		(rtx)->stack_top--; \
	} while (0)

static int read_record (hawk_rtx_t* rtx);
static int shorten_record (hawk_rtx_t* rtx, hawk_oow_t nflds);

static hawk_ooch_t* idxnde_to_str (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_ooch_t* buf, hawk_oow_t* len);

typedef hawk_val_t* (*binop_func_t) (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
typedef hawk_val_t* (*eval_expr_t) (hawk_rtx_t* rtx, hawk_nde_t* nde);


HAWK_INLINE hawk_oow_t hawk_rtx_getnargs (hawk_rtx_t* rtx)
{
	return (hawk_oow_t) RTX_STACK_NARGS(rtx);
}

HAWK_INLINE hawk_val_t* hawk_rtx_getarg (hawk_rtx_t* rtx, hawk_oow_t idx)
{
	return RTX_STACK_ARG(rtx, idx);
}

HAWK_INLINE hawk_val_t* hawk_rtx_getgbl (hawk_rtx_t* rtx, int id)
{
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), id >= 0 && id < (int)HAWK_ARR_SIZE(rtx->awk->parse.gbls));
	return RTX_STACK_GBL(rtx, id);
}

const hawk_oocs_t* hawk_rtx_getsubsep (hawk_rtx_t* rtx)
{
	return &rtx->gbl.subsep;
}

/* internal function to set a value to a global variable.
 * this function can handle a few special global variables that
 * require special treatment. */
static int set_global (hawk_rtx_t* rtx, int idx, hawk_nde_var_t* var, hawk_val_t* val, int assign)
{
	hawk_val_t* old;
	hawk_rtx_ecb_t* ecb;
	hawk_val_type_t vtype, old_vtype;

	old = RTX_STACK_GBL (rtx, idx);

	vtype = HAWK_RTX_GETVALTYPE (rtx, val);
	old_vtype = HAWK_RTX_GETVALTYPE (rtx, old);

	if (!(rtx->awk->opt.trait & HAWK_FLEXMAP))
	{
		hawk_errnum_t errnum = HAWK_ENOERR;

		if (vtype == HAWK_VAL_MAP)
		{
			if (old_vtype == HAWK_VAL_NIL)
			{
				/* a nil valul can be overridden with any values */
				/* ok. no error */
			}
			else if (!assign && old_vtype == HAWK_VAL_MAP)
			{
				/* when both are maps, how should this operation be 
				 * interpreted?
				 *
				 * is it an assignment?
				 *    old = new
				 *
				 * or is it to delete all elements in the array
				 * and add new items?
				 *    for (i in old) delete old[i];
				 *    for (i in new) old[i] = new[i];
				 *
				 * i interpret this operation as the latter.
				 */

				/* ok. no error */
			}
			else
			{
				errnum = HAWK_ENSCALARTOMAP;
			}
		}
		else
		{
			if (old_vtype == HAWK_VAL_MAP) errnum = HAWK_ENMAPTOSCALAR;
		}
	
		if (errnum != HAWK_ENOERR)
		{
			/* once a variable becomes a map, it cannot be assigned 
			 * others value than another map. you can only add a member
			 * using indexing. */
			if (var)
			{
				/* global variable */
				SETERR_ARGX_LOC (rtx, errnum, &var->id.name, &var->loc);
			}
			else
			{
				/* hawk_rtx_setgbl() has been called */
				hawk_oocs_t ea;
				ea.ptr = (hawk_ooch_t*)hawk_getgblname(hawk_rtx_gethawk(rtx), idx, &ea.len);
				SETERR_ARGX (rtx, errnum, &ea);
			}

			return -1;
		}
	}

	if (vtype == HAWK_VAL_MAP)
	{
		if (idx >= HAWK_MIN_GBL_ID && idx <= HAWK_MAX_GBL_ID)
		{
			/* short-circuit check block to prevent the basic built-in 
			 * variables from being assigned a map. if you happen to add
			 * one and if that's allowed to be a map, you may have to 
			 * change the condition here. */

/* TODO: use global variable attribute. can it be a map? can it be a scalar? is it read-only???? */

			hawk_oocs_t ea;
			ea.ptr = (hawk_ooch_t*)hawk_getgblname(hawk_rtx_gethawk(rtx), idx, &ea.len);
			SETERR_ARGX (rtx, HAWK_ENSCALARTOMAP, &ea);
			return -1;
		}
	}

	if (old == val) 
	{
		/* if the old value is the same as the new value, don't take any actions.
		 * note that several inspections have been performed before this check,
		 * mainly for consistency. anyway, this condition can be met if you execute
		 * a statement like 'ARGV=ARGV'. */
		return 0; 
	}

	/* perform actual assignment or assignment-like operation */
	switch (idx)
	{
		case HAWK_GBL_CONVFMT:
		{
			hawk_oow_t i;
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr (rtx, val, &out) <= -1)
				return -1;

			for (i = 0; i < out.u.cpldup.len; i++)
			{
				if (out.u.cpldup.ptr[i] == HAWK_T('\0'))
				{
					/* '\0' is included in the value */
					hawk_rtx_freemem (rtx, out.u.cpldup.ptr);
					SETERR_COD (rtx, HAWK_ECONVFMTCHR);
					return -1;
				}
			}

			if (rtx->gbl.convfmt.ptr) hawk_rtx_freemem (rtx, rtx->gbl.convfmt.ptr);
			rtx->gbl.convfmt.ptr = out.u.cpldup.ptr;
			rtx->gbl.convfmt.len = out.u.cpldup.len;
			break;
		}

		case HAWK_GBL_FNR:
		{
			int n;
			hawk_int_t lv;

			n = hawk_rtx_valtoint (rtx, val, &lv);
			if (n <= -1) return -1;

			rtx->gbl.fnr = lv;
			break;
		}
	
		case HAWK_GBL_FS:
		{
			hawk_ooch_t* fs_ptr;
			hawk_oow_t fs_len;

			if (vtype == HAWK_VAL_STR)
			{
				fs_ptr = ((hawk_val_str_t*)val)->val.ptr;
				fs_len = ((hawk_val_str_t*)val)->val.len;
			}
			else
			{
				hawk_rtx_valtostr_out_t out;

				/* due to the expression evaluation rule, the 
				 * regular expression can not be an assigned value */
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), vtype != HAWK_VAL_REX);

				out.type = HAWK_RTX_VALTOSTR_CPLDUP;
				if (hawk_rtx_valtostr (rtx, val, &out) <= -1) return -1;
				fs_ptr = out.u.cpldup.ptr;
				fs_len = out.u.cpldup.len;
			}

			if (fs_len > 1 && !(fs_len == 5 && fs_ptr[0] == HAWK_T('?')))
			{
				/* it's a regular expression if FS contains multiple characters.
				 * however, it's not a regular expression if it's 5 character
				 * string beginning with a question mark. */
				hawk_tre_t* rex, * irex;
				hawk_errnum_t errnum;

				if (hawk_buildrex(hawk_rtx_gethawk(rtx), fs_ptr, fs_len, &errnum, &rex, &irex) <= -1)
				{
					SETERR_COD (rtx, errnum);
					if (vtype != HAWK_VAL_STR) hawk_rtx_freemem (rtx, fs_ptr);
					return -1;
				}

				if (rtx->gbl.fs[0]) hawk_freerex (hawk_rtx_gethawk(rtx), rtx->gbl.fs[0], rtx->gbl.fs[1]);

				rtx->gbl.fs[0] = rex;
				rtx->gbl.fs[1] = irex;
			}

			if (vtype != HAWK_VAL_STR) hawk_rtx_freemem (rtx, fs_ptr);
			break;
		}

		case HAWK_GBL_IGNORECASE:
		{
			hawk_int_t l;
			hawk_flt_t r;
			int vt;

			vt = hawk_rtx_valtonum(rtx, val, &l, &r);
			if (vt <= -1) return -1;

			if (vt == 0) 
				rtx->gbl.ignorecase = ((l > 0)? 1: (l < 0)? -1: 0);
			else 
				rtx->gbl.ignorecase = ((r > 0.0)? 1: (r < 0.0)? -1: 0);
			break;
		}

		case HAWK_GBL_NF:
		{
			int n;
			hawk_int_t lv;

			n = hawk_rtx_valtoint(rtx, val, &lv);
			if (n <= -1) return -1;

			if (lv < (hawk_int_t)rtx->inrec.nflds)
			{
				if (shorten_record(rtx, (hawk_oow_t)lv) == -1)
				{
					/* adjust the error line */
					/*if (var) ADJERR_LOC (rtx, &var->loc);*/
					return -1;
				}
			}

			break;
		}


		case HAWK_GBL_NR:
		{
			int n;
			hawk_int_t lv;

			n = hawk_rtx_valtoint(rtx, val, &lv);
			if (n <= -1) return -1;

			rtx->gbl.nr = lv;
			break;
		}

		case HAWK_GBL_OFMT:
		{
			hawk_oow_t i;
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, val, &out) <= -1) return -1;

			for (i = 0; i < out.u.cpldup.len; i++)
			{
				if (out.u.cpldup.ptr[i] == HAWK_T('\0'))
				{
					hawk_rtx_freemem (rtx, out.u.cpldup.ptr);
					SETERR_COD (rtx, HAWK_EOFMTCHR);
					return -1;
				}
			}

			if (rtx->gbl.ofmt.ptr) hawk_rtx_freemem (rtx, rtx->gbl.ofmt.ptr);
			rtx->gbl.ofmt.ptr = out.u.cpldup.ptr;
			rtx->gbl.ofmt.len = out.u.cpldup.len;

			break;
		}

		case HAWK_GBL_OFS:
		{
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, val, &out) <= -1) return -1;
			if (rtx->gbl.ofs.ptr) hawk_rtx_freemem (rtx, rtx->gbl.ofs.ptr);
			rtx->gbl.ofs.ptr = out.u.cpldup.ptr;
			rtx->gbl.ofs.len = out.u.cpldup.len;

			break;
		}

		case HAWK_GBL_ORS:
		{	
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, val, &out) <= -1) return -1;
			if (rtx->gbl.ors.ptr) hawk_rtx_freemem (rtx, rtx->gbl.ors.ptr);
			rtx->gbl.ors.ptr = out.u.cpldup.ptr;
			rtx->gbl.ors.len = out.u.cpldup.len;

			break;
		}

		case HAWK_GBL_RS:
		{
			hawk_oocs_t rss;

			if (vtype == HAWK_VAL_STR)
			{
				rss = ((hawk_val_str_t*)val)->val;
			}
			else
			{
				hawk_rtx_valtostr_out_t out;

				/* due to the expression evaluation rule, the 
				 * regular expression can not be an assigned 
				 * value */
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), vtype != HAWK_VAL_REX);

				out.type = HAWK_RTX_VALTOSTR_CPLDUP;
				if (hawk_rtx_valtostr(rtx, val, &out) <= -1) return -1;

				rss = out.u.cpldup;
			}

			if (rtx->gbl.rs[0])
			{
				hawk_freerex (hawk_rtx_gethawk(rtx), rtx->gbl.rs[0], rtx->gbl.rs[1]);
				rtx->gbl.rs[0] = HAWK_NULL;
				rtx->gbl.rs[1] = HAWK_NULL;
			}

			if (rss.len > 1)
			{
				hawk_tre_t* rex, * irex;
				hawk_errnum_t errnum;
				
				/* compile the regular expression */
				if (hawk_buildrex(hawk_rtx_gethawk(rtx), rss.ptr, rss.len, &errnum, &rex, &irex) <= -1)
				{
					SETERR_COD (rtx, errnum);
					if (vtype != HAWK_VAL_STR) hawk_rtx_freemem (rtx, rss.ptr);
					return -1;
				}

				rtx->gbl.rs[0] = rex;
				rtx->gbl.rs[1] = irex;
			}

			if (vtype != HAWK_VAL_STR) hawk_rtx_freemem (rtx, rss.ptr);

			break;
		}

		case HAWK_GBL_STRIPRECSPC:
		{
			hawk_int_t l;
			hawk_flt_t r;
			int vt;

			vt = hawk_rtx_valtonum(rtx, val, &l, &r);
			if (vt <= -1) return -1;

			if (vt == 0) 
				rtx->gbl.striprecspc = ((l > 0)? 1: (l < 0)? -1: 0);
			else 
				rtx->gbl.striprecspc = ((r > 0.0)? 1: (r < 0.0)? -1: 0);
			break;
		}

		case HAWK_GBL_SUBSEP:
		{
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, val, &out) <= -1) return -1;

			if (rtx->gbl.subsep.ptr) hawk_rtx_freemem (rtx, rtx->gbl.subsep.ptr);
			rtx->gbl.subsep.ptr = out.u.cpldup.ptr;
			rtx->gbl.subsep.len = out.u.cpldup.len;

			break;
		}
	}

	hawk_rtx_refdownval (rtx, old);
	RTX_STACK_GBL(rtx,idx) = val;
	hawk_rtx_refupval (rtx, val);

	for (ecb = (rtx)->ecb; ecb; ecb = ecb->next)
	{
		if (ecb->gblset) ecb->gblset (rtx, idx, val);
	}

	return 0;
}

HAWK_INLINE void hawk_rtx_setretval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	hawk_rtx_refdownval (rtx, RTX_STACK_RETVAL(rtx));
	RTX_STACK_RETVAL(rtx) = val;
	/* should use the same trick as run_return */
	hawk_rtx_refupval (rtx, val);
}

HAWK_INLINE int hawk_rtx_setgbl (hawk_rtx_t* rtx, int id, hawk_val_t* val)
{
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), id >= 0 && id < (int)HAWK_ARR_SIZE(rtx->awk->parse.gbls));
	return set_global (rtx, id, HAWK_NULL, val, 0);
}

int hawk_rtx_setfilename (hawk_rtx_t* rtx, const hawk_ooch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	tmp = hawk_rtx_makestrvalwithoochars(rtx, name, len);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_setgbl (rtx, HAWK_GBL_FILENAME, tmp);
	hawk_rtx_refdownval (rtx, tmp);

	return n;
}

int hawk_rtx_setofilename (hawk_rtx_t* rtx, const hawk_ooch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	if (rtx->awk->opt.trait & HAWK_NEXTOFILE)
	{
		tmp = hawk_rtx_makestrvalwithoochars(rtx, name, len);
		if (tmp == HAWK_NULL) return -1;

		hawk_rtx_refupval (rtx, tmp);
		n = hawk_rtx_setgbl (rtx, HAWK_GBL_OFILENAME, tmp);
		hawk_rtx_refdownval (rtx, tmp);
	}
	else n = 0;

	return n;
}

hawk_htb_t* hawk_rtx_getnvmap (hawk_rtx_t* rtx)
{
	return rtx->named;
}

struct module_init_ctx_t
{
	hawk_oow_t count;
	hawk_rtx_t* rtx;
};

struct module_fini_ctx_t
{
	hawk_oow_t limit;
	hawk_oow_t count;
	hawk_rtx_t* rtx;
};

static hawk_rbt_walk_t init_module (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, void* ctx)
{
	hawk_mod_data_t* md;
	struct module_init_ctx_t* mic;

	mic = (struct module_init_ctx_t*)ctx;

	md = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
	if (md->mod.init && md->mod.init(&md->mod, mic->rtx) <= -1)
		return HAWK_RBT_WALK_STOP;

	mic->count++;
	return HAWK_RBT_WALK_FORWARD;
}

static hawk_rbt_walk_t fini_module (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, void* ctx)
{
	hawk_mod_data_t* md;
	struct module_fini_ctx_t* mfc;

	mfc = (struct module_fini_ctx_t*)ctx;

	if (mfc->limit > 0 && mfc->count >= mfc->limit) 
		return HAWK_RBT_WALK_STOP;

	md = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
	if (md->mod.fini) md->mod.fini (&md->mod, mfc->rtx);

	mfc->count++;
	return HAWK_RBT_WALK_FORWARD;
}

hawk_rtx_t* hawk_rtx_open (hawk_t* awk, hawk_oow_t xtnsize, hawk_rio_cbs_t* rio)
{
	hawk_rtx_t* rtx;
	struct module_init_ctx_t mic;

	/* clear the awk error code */
	hawk_seterrnum (awk, HAWK_ENOERR, HAWK_NULL);

	/* check if the code has ever been parsed */
	if (awk->tree.ngbls == 0 && 
	    awk->tree.begin == HAWK_NULL &&
	    awk->tree.end == HAWK_NULL &&
	    awk->tree.chain_size == 0 &&
	    hawk_htb_getsize(awk->tree.funs) == 0)
	{
		hawk_seterrnum (awk, HAWK_EPERM, HAWK_NULL);
		return HAWK_NULL;
	}
	
	/* allocate the storage for the rtx object */
	rtx = (hawk_rtx_t*)hawk_allocmem(awk, HAWK_SIZEOF(hawk_rtx_t) + xtnsize);
	if (!rtx)
	{
		/* if it fails, the failure is reported thru the awk object */
		return HAWK_NULL;
	}

	/* initialize the rtx object */
	HAWK_MEMSET (rtx, 0, HAWK_SIZEOF(hawk_rtx_t) + xtnsize);
	rtx->_instsize = HAWK_SIZEOF(hawk_rtx_t);
	if (init_rtx(rtx, awk, rio) <= -1) 
	{
		hawk_freemem (awk, rtx);
		return HAWK_NULL;
	}

	if (init_globals(rtx) <= -1)
	{
		hawk_rtx_errortohawk (rtx, awk);
		fini_rtx (rtx, 0);
		hawk_freemem (awk, rtx);
		return HAWK_NULL;
	}

	mic.count = 0;
	mic.rtx = rtx;
	hawk_rbt_walk (rtx->awk->modtab, init_module, &mic);
	if (mic.count != HAWK_RBT_SIZE(rtx->awk->modtab))
	{
		if (mic.count > 0)
		{
			struct module_fini_ctx_t mfc;
			mfc.limit = mic.count;
			mfc.count = 0;
			hawk_rbt_walk (rtx->awk->modtab, fini_module, &mfc);
		}

		fini_rtx (rtx, 1);
		hawk_freemem (awk, rtx);
		return HAWK_NULL;
	}

	return rtx;
}

void hawk_rtx_close (hawk_rtx_t* rtx)
{
	hawk_rtx_ecb_t* ecb;
	struct module_fini_ctx_t mfc;

	mfc.limit = 0;
	mfc.count = 0;
	mfc.rtx = rtx;
	hawk_rbt_walk (rtx->awk->modtab, fini_module, &mfc);

	for (ecb = rtx->ecb; ecb; ecb = ecb->next)
	{
		if (ecb->close) ecb->close (rtx);
	}

	/* NOTE:
	 *  the close callbacks are called before data in rtx
	 *  is destroyed. if the destruction count on any data
	 *  destroyed by the close callback, something bad 
	 *  will happen.
	 */
	fini_rtx (rtx, 1);

	hawk_freemem (hawk_rtx_gethawk(rtx), rtx);
}

void hawk_rtx_halt (hawk_rtx_t* rtx)
{
	rtx->exit_level = EXIT_ABORT;
}

int hawk_rtx_ishalt (hawk_rtx_t* rtx)
{
	return (rtx->exit_level == EXIT_ABORT || rtx->awk->haltall);
}

void hawk_rtx_getrio (hawk_rtx_t* rtx, hawk_rio_cbs_t* rio)
{
	rio->pipe = rtx->rio.handler[HAWK_RIO_PIPE];
	rio->file = rtx->rio.handler[HAWK_RIO_FILE];
	rio->console = rtx->rio.handler[HAWK_RIO_CONSOLE];
}

void hawk_rtx_setrio (hawk_rtx_t* rtx, const hawk_rio_cbs_t* rio)
{
	rtx->rio.handler[HAWK_RIO_PIPE] = rio->pipe;
	rtx->rio.handler[HAWK_RIO_FILE] = rio->file;
	rtx->rio.handler[HAWK_RIO_CONSOLE] = rio->console;
}

hawk_rtx_ecb_t* hawk_rtx_popecb (hawk_rtx_t* rtx)
{
	hawk_rtx_ecb_t* top = rtx->ecb;
	if (top) rtx->ecb = top->next;
	return top;
}

void hawk_rtx_pushecb (hawk_rtx_t* rtx, hawk_rtx_ecb_t* ecb)
{
	ecb->next = rtx->ecb;
	rtx->ecb = ecb;
}

static void free_namedval (hawk_htb_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_refdownval (*(hawk_rtx_t**)hawk_htb_getxtn(map), dptr);
}

static void same_namedval (hawk_htb_t* map, void* dptr, hawk_oow_t dlen)
{
	hawk_rtx_refdownval_nofree (*(hawk_rtx_t**)hawk_htb_getxtn(map), dptr);
}

static int init_rtx (hawk_rtx_t* rtx, hawk_t* awk, hawk_rio_cbs_t* rio)
{
	static hawk_htb_style_t style_for_named =
	{
		{
			HAWK_HTB_COPIER_INLINE,
			HAWK_HTB_COPIER_DEFAULT 
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			free_namedval 
		},
		HAWK_HTB_COMPER_DEFAULT,
		same_namedval,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	};
	hawk_oow_t stack_limit;

	rtx->_gem = awk->_gem;
	rtx->awk = awk;

	CLRERR (rtx);

	stack_limit = awk->parse.pragma.rtx_stack_limit > 0? awk->parse.pragma.rtx_stack_limit: awk->opt.rtx_stack_limit;
	if (stack_limit < HAWK_MIN_RTX_STACK_LIMIT) stack_limit = HAWK_MIN_RTX_STACK_LIMIT;
	rtx->stack = hawk_rtx_allocmem(rtx, stack_limit * HAWK_SIZEOF(void*));
	if (!rtx->stack) goto oops_0;
	rtx->stack_top = 0;
	rtx->stack_base = 0;
	rtx->stack_limit = stack_limit;

	rtx->exit_level = EXIT_NONE;

	rtx->vmgr.ichunk = HAWK_NULL;
	rtx->vmgr.ifree = HAWK_NULL;
	rtx->vmgr.rchunk = HAWK_NULL;
	rtx->vmgr.rfree = HAWK_NULL;

	rtx->inrec.buf_pos = 0;
	rtx->inrec.buf_len = 0;
	rtx->inrec.flds = HAWK_NULL;
	rtx->inrec.nflds = 0;
	rtx->inrec.maxflds = 0;
	rtx->inrec.d0 = hawk_val_nil;

	if (hawk_ooecs_init(&rtx->inrec.line, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1) goto oops_1;
	if (hawk_ooecs_init(&rtx->inrec.linew, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1) goto oops_2;
	if (hawk_ooecs_init(&rtx->inrec.lineg, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1) goto oops_3;
	if (hawk_ooecs_init(&rtx->format.out, hawk_rtx_getgem(rtx), 256) <= -1) goto oops_4;
	if (hawk_ooecs_init(&rtx->format.fmt, hawk_rtx_getgem(rtx), 256) <= -1) goto oops_5;

	if (hawk_becs_init(&rtx->formatmbs.out, hawk_rtx_getgem(rtx), 256) <= -1) goto oops_6;
	if (hawk_becs_init(&rtx->formatmbs.fmt, hawk_rtx_getgem(rtx), 256) <= -1) goto oops_7;

	rtx->named = hawk_htb_open(hawk_rtx_getgem(rtx), HAWK_SIZEOF(rtx), 1024, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	if (!rtx->named) goto oops_8;
	*(hawk_rtx_t**)hawk_htb_getxtn(rtx->named) = rtx;
	hawk_htb_setstyle (rtx->named, &style_for_named);

	rtx->format.tmp.ptr = (hawk_ooch_t*)hawk_rtx_allocmem(rtx, 4096 * HAWK_SIZEOF(hawk_ooch_t)); 
	if (!rtx->format.tmp.ptr) goto oops_9; /* the error is set on the awk object after this jump is made */
	rtx->format.tmp.len = 4096;
	rtx->format.tmp.inc = 4096 * 2;

	rtx->formatmbs.tmp.ptr = (hawk_bch_t*)hawk_rtx_allocmem(rtx, 4096 * HAWK_SIZEOF(hawk_bch_t));
	if (!rtx->formatmbs.tmp.ptr) goto oops_10;
	rtx->formatmbs.tmp.len = 4096;
	rtx->formatmbs.tmp.inc = 4096 * 2;

	if (rtx->awk->tree.chain_size > 0)
	{
		rtx->pattern_range_state = (hawk_oob_t*)hawk_rtx_allocmem(rtx, rtx->awk->tree.chain_size * HAWK_SIZEOF(hawk_oob_t));
		if (!rtx->pattern_range_state) goto oops_11;
		HAWK_MEMSET (rtx->pattern_range_state, 0, rtx->awk->tree.chain_size * HAWK_SIZEOF(hawk_oob_t));
	}
	else rtx->pattern_range_state = HAWK_NULL;

	if (rio)
	{
		rtx->rio.handler[HAWK_RIO_PIPE] = rio->pipe;
		rtx->rio.handler[HAWK_RIO_FILE] = rio->file;
		rtx->rio.handler[HAWK_RIO_CONSOLE] = rio->console;
		rtx->rio.chain = HAWK_NULL;
	}

	rtx->gbl.rs[0] = HAWK_NULL;
	rtx->gbl.rs[1] = HAWK_NULL;
	rtx->gbl.fs[0] = HAWK_NULL;
	rtx->gbl.fs[1] = HAWK_NULL;
	rtx->gbl.ignorecase = 0;
	rtx->gbl.striprecspc = -1;

	return 0;

oops_11:
	hawk_rtx_freemem (rtx, rtx->formatmbs.tmp.ptr);
oops_10:
	hawk_rtx_freemem (rtx, rtx->format.tmp.ptr);
oops_9:
	hawk_htb_close (rtx->named);
oops_8:
	hawk_becs_fini (&rtx->formatmbs.fmt);
oops_7:
	hawk_becs_fini (&rtx->formatmbs.out);
oops_6:
	hawk_ooecs_fini (&rtx->format.fmt);
oops_5:
	hawk_ooecs_fini (&rtx->format.out);
oops_4:
	hawk_ooecs_fini (&rtx->inrec.lineg);
oops_3:
	hawk_ooecs_fini (&rtx->inrec.linew);
oops_2:
	hawk_ooecs_fini (&rtx->inrec.line);
oops_1:
	hawk_rtx_freemem (rtx, rtx->stack);
oops_0:
	return -1;
}

static void fini_rtx (hawk_rtx_t* rtx, int fini_globals)
{
	if (rtx->pattern_range_state)
		hawk_rtx_freemem (rtx, rtx->pattern_range_state);

	/* close all pending io's */
	/* TODO: what if this operation fails? */
	hawk_rtx_cleario (rtx);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->rio.chain == HAWK_NULL);

	if (rtx->gbl.rs[0])
	{
		hawk_freerex (hawk_rtx_gethawk(rtx), rtx->gbl.rs[0], rtx->gbl.rs[1]);
		rtx->gbl.rs[0] = HAWK_NULL;
		rtx->gbl.rs[1] = HAWK_NULL;
	}
	if (rtx->gbl.fs[0])
	{
		hawk_freerex (hawk_rtx_gethawk(rtx), rtx->gbl.fs[0], rtx->gbl.fs[1]);
		rtx->gbl.fs[0] = HAWK_NULL;
		rtx->gbl.fs[1] = HAWK_NULL;
	}

	if (rtx->gbl.convfmt.ptr != HAWK_NULL &&
	    rtx->gbl.convfmt.ptr != DEFAULT_CONVFMT)
	{
		hawk_rtx_freemem (rtx, rtx->gbl.convfmt.ptr);
		rtx->gbl.convfmt.ptr = HAWK_NULL;
		rtx->gbl.convfmt.len = 0;
	}

	if (rtx->gbl.ofmt.ptr != HAWK_NULL && 
	    rtx->gbl.ofmt.ptr != DEFAULT_OFMT)
	{
		hawk_rtx_freemem (rtx, rtx->gbl.ofmt.ptr);
		rtx->gbl.ofmt.ptr = HAWK_NULL;
		rtx->gbl.ofmt.len = 0;
	}

	if (rtx->gbl.ofs.ptr != HAWK_NULL && 
	    rtx->gbl.ofs.ptr != DEFAULT_OFS)
	{
		hawk_rtx_freemem (rtx, rtx->gbl.ofs.ptr);
		rtx->gbl.ofs.ptr = HAWK_NULL;
		rtx->gbl.ofs.len = 0;
	}

	if (rtx->gbl.ors.ptr != HAWK_NULL && 
	    rtx->gbl.ors.ptr != DEFAULT_ORS &&
	    rtx->gbl.ors.ptr != DEFAULT_ORS_CRLF)
	{
		hawk_rtx_freemem (rtx, rtx->gbl.ors.ptr);
		rtx->gbl.ors.ptr = HAWK_NULL;
		rtx->gbl.ors.len = 0;
	}

	if (rtx->gbl.subsep.ptr != HAWK_NULL && 
	    rtx->gbl.subsep.ptr != DEFAULT_SUBSEP)
	{
		hawk_rtx_freemem (rtx, rtx->gbl.subsep.ptr);
		rtx->gbl.subsep.ptr = HAWK_NULL;
		rtx->gbl.subsep.len = 0;
	}

	hawk_rtx_freemem (rtx, rtx->formatmbs.tmp.ptr);
	rtx->formatmbs.tmp.ptr = HAWK_NULL;
	rtx->formatmbs.tmp.len = 0;
	hawk_becs_fini (&rtx->formatmbs.fmt);
	hawk_becs_fini (&rtx->formatmbs.out);

	hawk_rtx_freemem (rtx, rtx->format.tmp.ptr);
	rtx->format.tmp.ptr = HAWK_NULL;
	rtx->format.tmp.len = 0;
	hawk_ooecs_fini (&rtx->format.fmt);
	hawk_ooecs_fini (&rtx->format.out);

	/* destroy input record. hawk_rtx_clrrec() should be called
	 * before the stack has been destroyed because it may try
	 * to change the value to HAWK_GBL_NF. */
	hawk_rtx_clrrec (rtx, 0);  
	if (rtx->inrec.flds)
	{
		hawk_rtx_freemem (rtx, rtx->inrec.flds);
		rtx->inrec.flds = HAWK_NULL;
		rtx->inrec.maxflds = 0;
	}
	hawk_ooecs_fini (&rtx->inrec.lineg);
	hawk_ooecs_fini (&rtx->inrec.linew);
	hawk_ooecs_fini (&rtx->inrec.line);

	if (fini_globals) refdown_globals (rtx, 1);

	/* destroy the stack if necessary */
	if (rtx->stack)
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->stack_top == 0);

		hawk_rtx_freemem (rtx, rtx->stack);
		rtx->stack = HAWK_NULL;
		rtx->stack_top = 0;
		rtx->stack_base = 0;
		rtx->stack_limit = 0;
	}

	/* destroy named variables */
	hawk_htb_close (rtx->named);

	/* destroy values in free list */
	while (rtx->rcache_count > 0)
	{
		hawk_val_ref_t* tmp = rtx->rcache[--rtx->rcache_count];
		hawk_rtx_freeval (rtx, (hawk_val_t*)tmp, 0);
	}

#if defined(ENABLE_FEATURE_SCACHE)
	{
		int i;
		for (i = 0; i < HAWK_COUNTOF(rtx->scache_count); i++)
		{
			while (rtx->scache_count[i] > 0)
			{
				hawk_val_str_t* t = rtx->scache[i][--rtx->scache_count[i]];
				hawk_rtx_freeval (rtx, (hawk_val_t*)t, 0);
			}
		}
	}
#endif

	hawk_rtx_freevalchunk (rtx, rtx->vmgr.ichunk);
	hawk_rtx_freevalchunk (rtx, rtx->vmgr.rchunk);
	rtx->vmgr.ichunk = HAWK_NULL;
	rtx->vmgr.rchunk = HAWK_NULL;
}

static int update_fnr (hawk_rtx_t* rtx, hawk_int_t fnr, hawk_int_t nr)
{
	hawk_val_t* tmp1, * tmp2;

	tmp1 = hawk_rtx_makeintval(rtx, fnr);
	if (!tmp1) return -1;

	hawk_rtx_refupval (rtx, tmp1);

	if (nr == fnr) tmp2 = tmp1;
	else
	{
		tmp2 = hawk_rtx_makeintval(rtx, nr);
		if (!tmp2)
		{
			hawk_rtx_refdownval (rtx, tmp1);
			return -1;
		}

		hawk_rtx_refupval (rtx, tmp2);
	}

	if (hawk_rtx_setgbl (rtx, HAWK_GBL_FNR, tmp1) == -1)
	{
		if (nr != fnr) hawk_rtx_refdownval (rtx, tmp2);
		hawk_rtx_refdownval (rtx, tmp1);
		return -1;
	}

	if (hawk_rtx_setgbl (rtx, HAWK_GBL_NR, tmp2) == -1)
	{
		if (nr != fnr) hawk_rtx_refdownval (rtx, tmp2);
		hawk_rtx_refdownval (rtx, tmp1);
		return -1;
	}

	if (nr != fnr) hawk_rtx_refdownval (rtx, tmp2);
	hawk_rtx_refdownval (rtx, tmp1);
	return 0;
}

/* 
 * create global variables into the runtime stack
 * each variable is initialized to nil or zero.
 */
static int prepare_globals (hawk_rtx_t* rtx)
{
	hawk_oow_t saved_stack_top;
	hawk_oow_t ngbls;

	saved_stack_top = rtx->stack_top;
	ngbls = rtx->awk->tree.ngbls;

	/* initialize all global variables to nil by push nils to the stack */
	while (ngbls > 0)
	{
		--ngbls;
		if (__raw_push(rtx,hawk_val_nil) <= -1)
		{
			SETERR_COD (rtx, HAWK_ENOMEM);
			goto oops;
		}
	}	

	/* override NF to zero */
	if (hawk_rtx_setgbl(rtx, HAWK_GBL_NF, HAWK_VAL_ZERO) <= -1) goto oops;

	/* return success */
	return 0;

oops:
	/* restore the stack_top this way instead of calling __raw_pop()
	 * as many times as successful __raw_push(). it is ok because
	 * the values pushed so far are hawk_val_nils and HAWK_VAL_ZEROs.
	 */
	rtx->stack_top = saved_stack_top;
	return -1;
}

/*
 * assign initial values to the global variables whose desired initial
 * values are not nil or zero. some are handled in prepare_globals () and 
 * update_fnr().
 */
static int defaultify_globals (hawk_rtx_t* rtx)
{
	struct gtab_t
	{
		int idx;
		const hawk_ooch_t* str[2];
	};
	static struct gtab_t gtab[7] =
	{
		{ HAWK_GBL_CONVFMT,    { DEFAULT_CONVFMT, DEFAULT_CONVFMT  } },
		{ HAWK_GBL_FILENAME,   { HAWK_NULL,        HAWK_NULL         } },
		{ HAWK_GBL_OFILENAME,  { HAWK_NULL,        HAWK_NULL         } },
		{ HAWK_GBL_OFMT,       { DEFAULT_OFMT,    DEFAULT_OFMT     } },
		{ HAWK_GBL_OFS,        { DEFAULT_OFS,     DEFAULT_OFS      } },
		{ HAWK_GBL_ORS,        { DEFAULT_ORS,     DEFAULT_ORS_CRLF } },
		{ HAWK_GBL_SUBSEP,     { DEFAULT_SUBSEP,  DEFAULT_SUBSEP   } },
	};

	hawk_val_t* tmp;
	hawk_oow_t i, j;
	int stridx;

	stridx = (rtx->awk->opt.trait & HAWK_CRLF)? 1: 0;
	for (i = 0; i < HAWK_COUNTOF(gtab); i++)
	{
		if (gtab[i].str[stridx] == HAWK_NULL || gtab[i].str[stridx][0] == HAWK_T('\0'))
		{
			tmp = hawk_val_zls;
		}
		else 
		{
			tmp = hawk_rtx_makestrvalwithoocstr (rtx, gtab[i].str[stridx]);
			if (tmp == HAWK_NULL) return -1;
		}
		
		hawk_rtx_refupval (rtx, tmp);

		HAWK_ASSERT (hawk_rtx_gethawk(rtx), RTX_STACK_GBL(rtx,gtab[i].idx) == hawk_val_nil);

		if (hawk_rtx_setgbl(rtx, gtab[i].idx, tmp) == -1)
		{
			for (j = 0; j < i; j++)
			{
				hawk_rtx_setgbl (rtx, gtab[i].idx, hawk_val_nil);
			}

			hawk_rtx_refdownval (rtx, tmp);
			return -1;
		}

		hawk_rtx_refdownval (rtx, tmp);
	}

	return 0;
}

static void refdown_globals (hawk_rtx_t* run, int pop)
{
	hawk_oow_t ngbls;
       
	ngbls = run->awk->tree.ngbls;
	while (ngbls > 0)
	{
		--ngbls;
		hawk_rtx_refdownval (run, RTX_STACK_GBL(run,ngbls));
		if (pop) __raw_pop (run);
		else RTX_STACK_GBL(run,ngbls) = hawk_val_nil;
	}
}

static int init_globals (hawk_rtx_t* rtx)
{
	/* the stack must be clean when this function is invoked */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->stack_base == 0);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->stack_top == 0); 

	if (prepare_globals (rtx) == -1) return -1;
	if (update_fnr (rtx, 0, 0) == -1) goto oops;
	if (defaultify_globals (rtx) == -1) goto oops;
	return 0;

oops:
	refdown_globals (rtx, 1);
	return -1;
}

struct capture_retval_data_t
{
	hawk_rtx_t* rtx;
	hawk_val_t* val;	
};

static void capture_retval_on_exit (void* arg)
{
	struct capture_retval_data_t* data;

	data = (struct capture_retval_data_t*)arg;
	data->val = RTX_STACK_RETVAL(data->rtx);
	hawk_rtx_refupval (data->rtx, data->val);
}

static int enter_stack_frame (hawk_rtx_t* rtx)
{
	hawk_oow_t saved_stack_top;

	/* remember the current stack top */
	saved_stack_top = rtx->stack_top;

	/* push the current stack base */
	if (__raw_push(rtx,(void*)rtx->stack_base) <= -1) goto oops;

	/* push the current stack top before push the current stack base */
	if (__raw_push(rtx,(void*)saved_stack_top) <= -1) goto oops;
	
	/* secure space for a return value */
	if (__raw_push(rtx,hawk_val_nil) <= -1) goto oops;
	
	/* secure space for RTX_STACK_NARGS */
	if (__raw_push(rtx,hawk_val_nil) <= -1) goto oops;

	/* let the stack top remembered be the base of a new stack frame */
	rtx->stack_base = saved_stack_top;
	return 0;

oops:
	/* restore the stack top in a cheesy(?) way. 
	 * it is ok to do so as the values pushed are
	 * nils and binary numbers. */
	rtx->stack_top = saved_stack_top;
	SETERR_COD (rtx, HAWK_ENOMEM);
	return -1;
}

static void exit_stack_frame (hawk_rtx_t* run)
{
	/* At this point, the current stack frame should have 
	 * the 4 entries pushed in enter_stack_frame(). */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), (run->stack_top-run->stack_base) == 4);

	run->stack_top = (hawk_oow_t)run->stack[run->stack_base + 1];
	run->stack_base = (hawk_oow_t)run->stack[run->stack_base + 0];
}

static hawk_val_t* run_bpae_loop (hawk_rtx_t* rtx)
{
	hawk_nde_t* nde;
	hawk_oow_t nargs, i;
	hawk_val_t* retv;
	int ret = 0;

	/* set nargs to zero */
	nargs = 0;
	RTX_STACK_NARGS(rtx) = (void*)nargs;

	/* execute the BEGIN block */
	for (nde = rtx->awk->tree.begin; 
	     ret == 0 && nde != HAWK_NULL && rtx->exit_level < EXIT_GLOBAL;
	     nde = nde->next)
	{
		hawk_nde_blk_t* blk;

		blk = (hawk_nde_blk_t*)nde;
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), blk->type == HAWK_NDE_BLK);

		rtx->active_block = blk;
		rtx->exit_level = EXIT_NONE;
		if (run_block (rtx, blk) == -1) ret = -1;
	}

	if (ret <= -1 && hawk_rtx_geterrnum(rtx) == HAWK_ENOERR) 
	{
		/* an error is returned with no error number set.
		 * this trait is used by eval_expression() to
		 * abort the evaluation when exit() is executed 
		 * during function evaluation */
		ret = 0;
		CLRERR (rtx); /* clear it just in case */
	}

	/* run pattern block loops */
	if (ret == 0 && 
	    (rtx->awk->tree.chain != HAWK_NULL ||
	     rtx->awk->tree.end != HAWK_NULL) && 
	     rtx->exit_level < EXIT_GLOBAL)
	{
		if (run_pblocks(rtx) <= -1) ret = -1;
	}

	if (ret <= -1 && hawk_rtx_geterrnum(rtx) == HAWK_ENOERR)
	{
		/* an error is returned with no error number set.
		 * this trait is used by eval_expression() to
		 * abort the evaluation when exit() is executed 
		 * during function evaluation */
		ret = 0;
		CLRERR (rtx); /* clear it just in case */
	}

	/* execute END blocks. the first END block is executed if the 
	 * program is not explicitly aborted with hawk_rtx_halt().*/
	for (nde = rtx->awk->tree.end;
	     ret == 0 && nde != HAWK_NULL && rtx->exit_level < EXIT_ABORT;
	     nde = nde->next) 
	{
		hawk_nde_blk_t* blk;

		blk = (hawk_nde_blk_t*)nde;
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), blk->type == HAWK_NDE_BLK);

		rtx->active_block = blk;
		rtx->exit_level = EXIT_NONE;
		if (run_block (rtx, blk) <= -1) ret = -1;
		else if (rtx->exit_level >= EXIT_GLOBAL) 
		{
			/* once exit is called inside one of END blocks,
			 * subsequent END blocks must not be executed */
			break;
		}
	}

	if (ret <= -1 && hawk_rtx_geterrnum(rtx) == HAWK_ENOERR)
	{
		/* an error is returned with no error number set.
		 * this trait is used by eval_expression() to
		 * abort the evaluation when exit() is executed 
		 * during function evaluation */
		ret = 0;
		CLRERR (rtx); /* clear it just in case */
	}

	/* derefrence all arguments. however, there should be no arguments 
	 * pushed to the stack as asserted below. we didn't push any arguments
	 * for BEGIN/pattern action/END block execution.*/
	nargs = (hawk_oow_t)RTX_STACK_NARGS(rtx);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nargs == 0);
	for (i = 0; i < nargs; i++) 
		hawk_rtx_refdownval (rtx, RTX_STACK_ARG(rtx,i));

	/* get the return value in the current stack frame */
	retv = RTX_STACK_RETVAL(rtx);

	if (ret <= -1)
	{
		/* end the life of the global return value upon error */
		hawk_rtx_refdownval (rtx, retv);
		retv = HAWK_NULL;
	}

	return retv;
}

/* start the BEGIN-pattern block-END loop */
hawk_val_t* hawk_rtx_loop (hawk_rtx_t* rtx)
{
	hawk_val_t* retv = HAWK_NULL;

	rtx->exit_level = EXIT_NONE;

	if (enter_stack_frame (rtx) == 0)
	{
		retv = run_bpae_loop (rtx);
		exit_stack_frame (rtx);
	}

	/* reset the exit level */
	rtx->exit_level = EXIT_NONE;
	return retv;
}

/* find an AWK function by name */
static hawk_fun_t* find_fun (hawk_rtx_t* rtx, const hawk_ooch_t* name)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search(rtx->awk->tree.funs, name, hawk_count_oocstr(name));

	if (!pair)
	{
		hawk_oocs_t nm;

		nm.ptr = (hawk_ooch_t*)name;
		nm.len = hawk_count_oocstr(name);

		SETERR_ARGX (rtx, HAWK_EFUNNF, &nm);
		return HAWK_NULL;
	}

	return (hawk_fun_t*)HAWK_HTB_VPTR(pair);
}

hawk_fun_t* hawk_rtx_findfunwithbcstr (hawk_rtx_t* rtx, const hawk_bch_t* name)
{
#if defined(HAWK_OOCH_IS_BCH)
	return find_fun(rtx, name);
#else
	hawk_ucs_t wcs;
	hawk_fun_t* fun;
	wcs.ptr = hawk_rtx_dupbtoucstr(rtx, name, &wcs.len, 0);
	if (!wcs.ptr) return HAWK_NULL;
	fun = find_fun(rtx, wcs.ptr);
	hawk_rtx_freemem (rtx, wcs.ptr);
	return fun;
#endif
}

hawk_fun_t* hawk_rtx_findfunwithucstr (hawk_rtx_t* rtx, const hawk_uch_t* name)
{
#if defined(HAWK_OOCH_IS_BCH)
	hawk_bcs_t mbs;
	hawk_fun_t* fun;
	mbs.ptr = hawk_rtx_duputobcstr(rtx, name, &mbs.len);
	if (!mbs.ptr) return HAWK_NULL;
	fun = find_fun(rtx, mbs.ptr);
	hawk_rtx_freemem (rtx, mbs.ptr);
	return fun;
#else
	return find_fun(rtx, name);
#endif
}

/* call an AWK function by the function structure */
hawk_val_t* hawk_rtx_callfun (hawk_rtx_t* rtx, hawk_fun_t* fun, hawk_val_t* args[], hawk_oow_t nargs)
{
	struct capture_retval_data_t crdata;
	hawk_val_t* v;
	struct pafv_t pafv/*= { args, nargs }*/;
	hawk_nde_fncall_t call;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), fun != HAWK_NULL);

	pafv.args = args;
	pafv.nargs = nargs;
	pafv.argspec = fun->argspec;

	if (rtx->exit_level >= EXIT_GLOBAL) 
	{
		/* cannot call the function again when exit() is called
		 * in an AWK program or hawk_rtx_halt() is invoked */
		SETERR_COD (rtx, HAWK_EPERM);
		return HAWK_NULL;
	}
	/*rtx->exit_level = EXIT_NONE;*/

#if 0
	if (fun->argspec)
	{
		/* this function contains pass-by-reference parameters.
		 * i don't support the call here as it requires variables */
		hawk_rtx_seterrfmt (rtx, HAWK_EPERM, HAWK_NULL, HAWK_T("not allowed to call '%.*js' with pass-by-reference parameters"), fun->name.len, fun->name.ptr);
		return HAWK_NULL;
	}
#endif

	/* forge a fake node containing a function call */
	HAWK_MEMSET (&call, 0, HAWK_SIZEOF(call));
	call.type = HAWK_NDE_FNCALL_FUN;
	call.u.fun.name = fun->name;
	call.nargs = nargs;
	/* keep HAWK_NULL in call.args so that __eval_call() knows it's a fake call structure */

	/* check if the number of arguments given is more than expected */
	if (nargs > fun->nargs)
	{
		/* TODO: is this correct? what if i want to 
		 *       allow arbitrary numbers of arguments? */
		SETERR_COD (rtx, HAWK_EARGTM);
		return HAWK_NULL;
	}

	/* now that the function is found and ok, let's execute it */

	crdata.rtx = rtx;
	crdata.val = HAWK_NULL;

	v = __eval_call(rtx, (hawk_nde_t*)&call, fun, push_arg_from_vals, (void*)&pafv, capture_retval_on_exit, &crdata);

	if (!v)
	{
		/* an error occurred. let's check if it is caused by exit().
		 * if so, the return value should have been captured into
		 * crdata.val. */
		if (crdata.val) v = crdata.val; /* yet it is */
	}
	else
	{
		/* the return value captured in termination by exit()
		 * is reference-counted up in capture_retval_on_exit().
		 * let's do the same thing for the return value normally
		 * returned. */
		hawk_rtx_refupval (rtx, v);
	}

	/* return the return value with its reference count at least 1.
	 * the caller of this function should count down its reference. */
	return v;
}

/* call an AWK function by name */
hawk_val_t* hawk_rtx_callwithucstr (hawk_rtx_t* rtx, const hawk_uch_t* name, hawk_val_t* args[], hawk_oow_t nargs)
{
	hawk_fun_t* fun;

	fun = hawk_rtx_findfunwithucstr(rtx, name);
	if (!fun) return HAWK_NULL;

	return hawk_rtx_callfun(rtx, fun, args, nargs);
}

hawk_val_t* hawk_rtx_callwithbcstr (hawk_rtx_t* rtx, const hawk_bch_t* name, hawk_val_t* args[], hawk_oow_t nargs)
{
	hawk_fun_t* fun;

	fun = hawk_rtx_findfunwithbcstr(rtx, name);
	if (!fun) return HAWK_NULL;

	return hawk_rtx_callfun(rtx, fun, args, nargs);
}

hawk_val_t* hawk_rtx_callwithucstrarr (hawk_rtx_t* rtx, const hawk_uch_t* name, const hawk_uch_t* args[], hawk_oow_t nargs)
{
	hawk_oow_t i;
	hawk_val_t** v, * ret;

	v = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*v) * nargs);
	if (!v) return HAWK_NULL;

	for (i = 0; i < nargs; i++)
	{
		v[i] = hawk_rtx_makestrvalwithucstr(rtx, args[i]);
		if (!v[i])
		{
			ret = HAWK_NULL;
			goto oops;
		}

		hawk_rtx_refupval (rtx, v[i]);
	}

	ret = hawk_rtx_callwithucstr(rtx, name, v, nargs);

oops:
	while (i > 0) 
	{
		hawk_rtx_refdownval (rtx, v[--i]);
	}
	hawk_rtx_freemem (rtx, v);
	return ret;
}

hawk_val_t* hawk_rtx_callwithbcstrarr (hawk_rtx_t* rtx, const hawk_bch_t* name, const hawk_bch_t* args[], hawk_oow_t nargs)
{
	hawk_oow_t i;
	hawk_val_t** v, * ret;

	v = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*v) * nargs);
	if (!v) return HAWK_NULL;

	for (i = 0; i < nargs; i++)
	{
		v[i] = hawk_rtx_makestrvalwithbcstr(rtx, args[i]);
		if (!v[i])
		{
			ret = HAWK_NULL;
			goto oops;
		}

		hawk_rtx_refupval (rtx, v[i]);
	}

	ret = hawk_rtx_callwithbcstr(rtx, name, v, nargs);

oops:
	while (i > 0) 
	{
		hawk_rtx_refdownval (rtx, v[--i]);
	}
	hawk_rtx_freemem (rtx, v);
	return ret;
}

static int run_pblocks (hawk_rtx_t* rtx)
{
	int n;

#define ADJUST_ERROR(run) \
	if (rtx->awk->tree.chain != HAWK_NULL) \
	{ \
		if (rtx->awk->tree.chain->pattern != HAWK_NULL) \
			ADJERR_LOC (rtx, &rtx->awk->tree.chain->pattern->loc); \
		else if (rtx->awk->tree.chain->action != HAWK_NULL) \
			ADJERR_LOC (rtx, &rtx->awk->tree.chain->action->loc); \
	} \
	else if (rtx->awk->tree.end != HAWK_NULL) \
	{ \
		ADJERR_LOC (run, &rtx->awk->tree.end->loc); \
	} 

	rtx->inrec.buf_pos = 0;
	rtx->inrec.buf_len = 0;
	rtx->inrec.eof = 0;

	/* run each pattern block */
	while (rtx->exit_level < EXIT_GLOBAL)
	{
		rtx->exit_level = EXIT_NONE;

		n = read_record(rtx);
		if (n == -1) 
		{
			ADJUST_ERROR (rtx);
			return -1; /* error */
		}
		if (n == 0) break; /* end of input */

		if (rtx->awk->tree.chain)
		{
			if (run_pblock_chain(rtx, rtx->awk->tree.chain) == -1) return -1;
		}
	}

#undef ADJUST_ERROR
	return 0;
}

static int run_pblock_chain (hawk_rtx_t* rtx, hawk_chain_t* chain)
{
	hawk_oow_t bno = 0;

	while (rtx->exit_level < EXIT_GLOBAL && chain)
	{
		if (rtx->exit_level == EXIT_NEXT)
		{
			rtx->exit_level = EXIT_NONE;
			break;
		}

		if (run_pblock(rtx, chain, bno) <= -1) return -1;

		chain = chain->next;
		bno++;
	}

	return 0;
}

static int run_pblock (hawk_rtx_t* rtx, hawk_chain_t* cha, hawk_oow_t bno)
{
	hawk_nde_t* ptn;
	hawk_nde_blk_t* blk;

	ptn = cha->pattern;
	blk = (hawk_nde_blk_t*)cha->action;

	if (!ptn)
	{
		/* just execute the block */
		rtx->active_block = blk;
		if (run_block(rtx, blk) <= -1) return -1;
	}
	else
	{
		if (!ptn->next)
		{
			/* pattern { ... } */
			hawk_val_t* v1;

			v1 = eval_expression(rtx, ptn);
			if (!v1) return -1;

			hawk_rtx_refupval (rtx, v1);

			if (hawk_rtx_valtobool(rtx, v1))
			{
				rtx->active_block = blk;
				if (run_block(rtx, blk) <= -1)
				{
					hawk_rtx_refdownval (rtx, v1);
					return -1;
				}
			}

			hawk_rtx_refdownval (rtx, v1);
		}
		else
		{
			/* pattern, pattern { ... } */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), ptn->next->next == HAWK_NULL);
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->pattern_range_state != HAWK_NULL);

			if (rtx->pattern_range_state[bno] == 0)
			{
				hawk_val_t* v1;

				v1 = eval_expression(rtx, ptn);
				if (!v1) return -1;
				hawk_rtx_refupval (rtx, v1);

				if (hawk_rtx_valtobool(rtx, v1))
				{
					rtx->active_block = blk;
					if (run_block(rtx, blk) <= -1)
					{
						hawk_rtx_refdownval (rtx, v1);
						return -1;
					}

					rtx->pattern_range_state[bno] = 1;
				}

				hawk_rtx_refdownval (rtx, v1);
			}
			else if (rtx->pattern_range_state[bno] == 1)
			{
				hawk_val_t* v2;

				v2 = eval_expression(rtx, ptn->next);
				if (!v2) return -1;
				hawk_rtx_refupval (rtx, v2);

				rtx->active_block = blk;
				if (run_block(rtx, blk) <= -1)
				{
					hawk_rtx_refdownval(rtx, v2);
					return -1;
				}

				if (hawk_rtx_valtobool(rtx, v2)) rtx->pattern_range_state[bno] = 0;

				hawk_rtx_refdownval (rtx, v2);
			}
		}
	}

	return 0;
}

static HAWK_INLINE int run_block0 (hawk_rtx_t* rtx, hawk_nde_blk_t* nde)
{
	hawk_nde_t* p;
	hawk_oow_t nlcls;
	hawk_oow_t saved_stack_top;
	int n = 0;

	if (nde == HAWK_NULL)
	{
		/* blockless pattern - execute print $0*/
		hawk_rtx_refupval (rtx, rtx->inrec.d0);

		n = hawk_rtx_writeiostr(rtx, HAWK_OUT_CONSOLE, HAWK_T(""), HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line));
		if (n <= -1)
		{
			hawk_rtx_refdownval (rtx, rtx->inrec.d0);
			ADJERR_LOC (rtx, &nde->loc);
			return -1;
		}

		n = hawk_rtx_writeiostr(rtx, HAWK_OUT_CONSOLE, HAWK_T(""), rtx->gbl.ors.ptr, rtx->gbl.ors.len);
		if (n <= -1)
		{
			hawk_rtx_refdownval (rtx, rtx->inrec.d0);
			ADJERR_LOC (rtx, &nde->loc);
			return -1;
		}

		hawk_rtx_refdownval (rtx, rtx->inrec.d0);
		return 0;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->type == HAWK_NDE_BLK);

	p = nde->body;
	nlcls = nde->nlcls;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), 
		HAWK_T("securing space for local variables nlcls = %d\n"), 
		(int)nlcls);
#endif

	saved_stack_top = rtx->stack_top;

	/* secure space for local variables */
	while (nlcls > 0)
	{
		--nlcls;
		if (__raw_push(rtx,hawk_val_nil) <= -1)
		{
			/* restore stack top */
			rtx->stack_top = saved_stack_top;
			return -1;
		}

		/* refupval is not required for hawk_val_nil */
	}

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("executing block statements\n"));
#endif

	while (p != HAWK_NULL && rtx->exit_level == EXIT_NONE)
	{
		if (run_statement(rtx, p) <= -1)
		{
			n = -1;
			break;
		}
		p = p->next;
	}

	/* pop off local variables */
#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("popping off local variables\n"));
#endif
	nlcls = nde->nlcls;
	while (nlcls > 0)
	{
		--nlcls;
		hawk_rtx_refdownval (rtx, RTX_STACK_LCL(rtx,nlcls));
		__raw_pop (rtx);
	}

	return n;
}

static int run_block (hawk_rtx_t* rtx, hawk_nde_blk_t* nde)
{
	int n;

	if (rtx->awk->opt.depth.s.block_run > 0 &&
	    rtx->depth.block >= rtx->awk->opt.depth.s.block_run)
	{
		SETERR_LOC (rtx, HAWK_EBLKNST, &nde->loc);
		return -1;;
	}

	rtx->depth.block++;
	n = run_block0(rtx, nde);
	rtx->depth.block--;

	return n;
}

#define ON_STATEMENT(rtx,nde) do { \
	hawk_rtx_ecb_t* ecb; \
	if ((rtx)->awk->haltall) (rtx)->exit_level = EXIT_ABORT; \
	for (ecb = (rtx)->ecb; ecb; ecb = ecb->next) \
		if (ecb->stmt) ecb->stmt (rtx, nde);  \
} while(0)

static int run_statement (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	int xret;
	hawk_val_t* tmp;

	ON_STATEMENT (rtx, nde);

	switch (nde->type) 
	{
		case HAWK_NDE_NULL:
			/* do nothing */
			xret = 0;
			break;

		case HAWK_NDE_BLK:
			xret = run_block(rtx, (hawk_nde_blk_t*)nde);
			break;

		case HAWK_NDE_IF:
			xret = run_if(rtx, (hawk_nde_if_t*)nde);
			break;

		case HAWK_NDE_WHILE:
		case HAWK_NDE_DOWHILE:
			xret = run_while(rtx, (hawk_nde_while_t*)nde);
			break;

		case HAWK_NDE_FOR:
			xret = run_for(rtx, (hawk_nde_for_t*)nde);
			break;

		case HAWK_NDE_FOREACH:
			xret = run_foreach(rtx, (hawk_nde_foreach_t*)nde);
			break;

		case HAWK_NDE_BREAK:
			xret = run_break(rtx, (hawk_nde_break_t*)nde);
			break;

		case HAWK_NDE_CONTINUE:
			xret = run_continue(rtx, (hawk_nde_continue_t*)nde);
			break;

		case HAWK_NDE_RETURN:
			xret = run_return(rtx, (hawk_nde_return_t*)nde);
			break;

		case HAWK_NDE_EXIT:
			xret = run_exit(rtx, (hawk_nde_exit_t*)nde);
			break;

		case HAWK_NDE_NEXT:
			xret = run_next(rtx, (hawk_nde_next_t*)nde);
			break;

		case HAWK_NDE_NEXTFILE:
			xret = run_nextfile(rtx, (hawk_nde_nextfile_t*)nde);
			break;

		case HAWK_NDE_DELETE:
			xret = run_delete(rtx, (hawk_nde_delete_t*)nde);
			break;

		case HAWK_NDE_RESET:
			xret = run_reset(rtx, (hawk_nde_reset_t*)nde);
			break;

		case HAWK_NDE_PRINT:
			if (rtx->awk->opt.trait & HAWK_TOLERANT) goto __fallback__;
			xret = run_print(rtx, (hawk_nde_print_t*)nde);
			break;

		case HAWK_NDE_PRINTF:
			if (rtx->awk->opt.trait & HAWK_TOLERANT) goto __fallback__;
			xret = run_printf(rtx, (hawk_nde_print_t*)nde);
			break;

		default:
		__fallback__:
			tmp = eval_expression(rtx, nde);
			if (tmp == HAWK_NULL) xret = -1;
			else
			{
				/* destroy the value if not referenced */
				hawk_rtx_refupval (rtx, tmp);
				hawk_rtx_refdownval (rtx, tmp);
				xret = 0;
			}
			break;
	}

	return xret;
}

static int run_if (hawk_rtx_t* rtx, hawk_nde_if_t* nde)
{
	hawk_val_t* test;
	int n = 0;

	/* the test expression for the if statement cannot have 
	 * chained expressions. this should not be allowed by the
	 * parser first of all */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->test->next == HAWK_NULL);

	test = eval_expression (rtx, nde->test);
	if (test == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, test);
	if (hawk_rtx_valtobool(rtx, test))
	{
		n = run_statement(rtx, nde->then_part);
	}
	else if (nde->else_part)
	{
		n = run_statement(rtx, nde->else_part);
	}

	hawk_rtx_refdownval (rtx, test); /* TODO: is this correct?*/
	return n;
}

static int run_while (hawk_rtx_t* rtx, hawk_nde_while_t* nde)
{
	hawk_val_t* test;

	if (nde->type == HAWK_NDE_WHILE)
	{
		/* no chained expressions are allowed for the test 
		 * expression of the while statement */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->test->next == HAWK_NULL);

		while (1)
		{
			ON_STATEMENT (rtx, nde->test);

			test = eval_expression(rtx, nde->test);
			if (!test) return -1;

			hawk_rtx_refupval (rtx, test);

			if (hawk_rtx_valtobool(rtx, test))
			{
				if (run_statement(rtx,nde->body) <= -1)
				{
					hawk_rtx_refdownval (rtx, test);
					return -1;
				}
			}
			else
			{
				hawk_rtx_refdownval (rtx, test);
				break;
			}

			hawk_rtx_refdownval (rtx, test);

			if (rtx->exit_level == EXIT_BREAK)
			{
				rtx->exit_level = EXIT_NONE;
				break;
			}
			else if (rtx->exit_level == EXIT_CONTINUE)
			{
				rtx->exit_level = EXIT_NONE;
			}
			else if (rtx->exit_level != EXIT_NONE) break;

		}
	}
	else if (nde->type == HAWK_NDE_DOWHILE)
	{
		/* no chained expressions are allowed for the test 
		 * expression of the while statement */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->test->next == HAWK_NULL);

		do
		{
			if (run_statement(rtx,nde->body) <= -1) return -1;

			if (rtx->exit_level == EXIT_BREAK)
			{
				rtx->exit_level = EXIT_NONE;
				break;
			}
			else if (rtx->exit_level == EXIT_CONTINUE)
			{
				rtx->exit_level = EXIT_NONE;
			}
			else if (rtx->exit_level != EXIT_NONE) break;

			ON_STATEMENT (rtx, nde->test);

			test = eval_expression(rtx, nde->test);
			if (!test) return -1;

			hawk_rtx_refupval (rtx, test);

			if (!hawk_rtx_valtobool(rtx, test))
			{
				hawk_rtx_refdownval (rtx, test);
				break;
			}

			hawk_rtx_refdownval (rtx, test);
		}
		while (1);
	}

	return 0;
}

static int run_for (hawk_rtx_t* rtx, hawk_nde_for_t* nde)
{
	hawk_val_t* val;

	if (nde->init != HAWK_NULL)
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->init->next == HAWK_NULL);

		ON_STATEMENT (rtx, nde->init);
		val = eval_expression(rtx,nde->init);
		if (val == HAWK_NULL) return -1;

		hawk_rtx_refupval (rtx, val);
		hawk_rtx_refdownval (rtx, val);
	}

	while (1)
	{
		if (nde->test != HAWK_NULL)
		{
			hawk_val_t* test;

			/* no chained expressions for the test expression of
			 * the for statement are allowed */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->test->next == HAWK_NULL);

			ON_STATEMENT (rtx, nde->test);
			test = eval_expression (rtx, nde->test);
			if (test == HAWK_NULL) return -1;

			hawk_rtx_refupval (rtx, test);
			if (hawk_rtx_valtobool(rtx, test))
			{
				if (run_statement(rtx,nde->body) == -1)
				{
					hawk_rtx_refdownval (rtx, test);
					return -1;
				}
			}
			else
			{
				hawk_rtx_refdownval (rtx, test);
				break;
			}

			hawk_rtx_refdownval (rtx, test);
		}	
		else
		{
			if (run_statement(rtx,nde->body) == -1) return -1;
		}

		if (rtx->exit_level == EXIT_BREAK)
		{	
			rtx->exit_level = EXIT_NONE;
			break;
		}
		else if (rtx->exit_level == EXIT_CONTINUE)
		{
			rtx->exit_level = EXIT_NONE;
		}
		else if (rtx->exit_level != EXIT_NONE) break;

		if (nde->incr != HAWK_NULL)
		{
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->incr->next == HAWK_NULL);

			ON_STATEMENT (rtx, nde->incr);
			val = eval_expression (rtx, nde->incr);
			if (val == HAWK_NULL) return -1;

			hawk_rtx_refupval (rtx, val);
			hawk_rtx_refdownval (rtx, val);
		}
	}

	return 0;
}

struct foreach_walker_t
{
	hawk_rtx_t* rtx;
	hawk_nde_t* var;
	hawk_nde_t* body;
	int ret;
};

static hawk_htb_walk_t walk_foreach (hawk_htb_t* map, hawk_htb_pair_t* pair, void* arg)
{
	struct foreach_walker_t* w = (struct foreach_walker_t*)arg;
	hawk_val_t* str;

	str = (hawk_val_t*)hawk_rtx_makestrvalwithoochars(w->rtx, HAWK_HTB_KPTR(pair), HAWK_HTB_KLEN(pair));
	if (str == HAWK_NULL) 
	{
		ADJERR_LOC (w->rtx, &w->var->loc);
		w->ret = -1;
		return HAWK_HTB_WALK_STOP;
	}

	hawk_rtx_refupval (w->rtx, str);
	if (do_assignment(w->rtx, w->var, str) == HAWK_NULL)
	{
		hawk_rtx_refdownval (w->rtx, str);
		w->ret = -1;
		return HAWK_HTB_WALK_STOP;
	}

	if (run_statement(w->rtx, w->body) == -1)
	{
		hawk_rtx_refdownval (w->rtx, str);
		w->ret = -1;
		return HAWK_HTB_WALK_STOP;
	}
	
	hawk_rtx_refdownval (w->rtx, str);

	if (w->rtx->exit_level == EXIT_BREAK)
	{	
		w->rtx->exit_level = EXIT_NONE;
		return HAWK_HTB_WALK_STOP;
	}
	else if (w->rtx->exit_level == EXIT_CONTINUE)
	{
		w->rtx->exit_level = EXIT_NONE;
	}
	else if (w->rtx->exit_level != EXIT_NONE) 
	{
		return HAWK_HTB_WALK_STOP;
	}

	return HAWK_HTB_WALK_FORWARD;
}

static int run_foreach (hawk_rtx_t* rtx, hawk_nde_foreach_t* nde)
{
	hawk_nde_exp_t* test;
	hawk_val_t* rv;
	hawk_htb_t* map;
	struct foreach_walker_t walker;
	hawk_val_type_t rvtype;

	test = (hawk_nde_exp_t*)nde->test;
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), test->type == HAWK_NDE_EXP_BIN && test->opcode == HAWK_BINOP_IN);

	/* chained expressions should not be allowed 
	 * by the parser first of all */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), test->right->next == HAWK_NULL); 

	rv = eval_expression(rtx, test->right);
	if (rv == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, rv);
	rvtype = HAWK_RTX_GETVALTYPE(rtx, rv);
	if (rvtype == HAWK_VAL_NIL) 
	{
		/* just return without excuting the loop body */
		hawk_rtx_refdownval (rtx, rv);
		return 0;
	}
	else if (rvtype != HAWK_VAL_MAP)
	{
		hawk_rtx_refdownval (rtx, rv);
		SETERR_LOC (rtx, HAWK_ENOTMAPIN, &test->right->loc);
		return -1;
	}
	map = ((hawk_val_map_t*)rv)->map;

	walker.rtx = rtx;
	walker.var = test->left;
	walker.body = nde->body;
	walker.ret = 0;
	hawk_htb_walk (map, walk_foreach, &walker);

	hawk_rtx_refdownval (rtx, rv);
	return walker.ret;
}

static int run_break (hawk_rtx_t* run, hawk_nde_break_t* nde)
{
	run->exit_level = EXIT_BREAK;
	return 0;
}

static int run_continue (hawk_rtx_t* rtx, hawk_nde_continue_t* nde)
{
	rtx->exit_level = EXIT_CONTINUE;
	return 0;
}

static int run_return (hawk_rtx_t* rtx, hawk_nde_return_t* nde)
{
	if (nde->val != HAWK_NULL)
	{
		hawk_val_t* val;

		/* chained expressions should not be allowed 
		 * by the parser first of all */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->val->next == HAWK_NULL); 

		val = eval_expression (rtx, nde->val);
		if (val == HAWK_NULL) return -1;

		if (!(rtx->awk->opt.trait & HAWK_FLEXMAP))
		{
			if (HAWK_RTX_GETVALTYPE (rtx, val) == HAWK_VAL_MAP)
			{
				/* cannot return a map */
				hawk_rtx_refupval (rtx, val);
				hawk_rtx_refdownval (rtx, val);
				SETERR_LOC (rtx, HAWK_EMAPRET, &nde->loc);
				return -1;
			}
		}

		hawk_rtx_refdownval (rtx, RTX_STACK_RETVAL(rtx));
		RTX_STACK_RETVAL(rtx) = val;

		/* NOTE: see eval_call() for the trick */
		hawk_rtx_refupval (rtx, val); 
	}
	
	rtx->exit_level = EXIT_FUNCTION;
	return 0;
}

static int run_exit (hawk_rtx_t* rtx, hawk_nde_exit_t* nde)
{
	if (nde->val != HAWK_NULL)
	{
		hawk_val_t* val;

		/* chained expressions should not be allowed 
		 * by the parser first of all */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->val->next == HAWK_NULL); 

		val = eval_expression (rtx, nde->val);
		if (val == HAWK_NULL) return -1;

		hawk_rtx_refdownval (rtx, RTX_STACK_RETVAL_GBL(rtx));
		RTX_STACK_RETVAL_GBL(rtx) = val; /* global return value */

		hawk_rtx_refupval (rtx, val);
	}

	rtx->exit_level = (nde->abort)? EXIT_ABORT: EXIT_GLOBAL;
	return 0;
}

static int run_next (hawk_rtx_t* run, hawk_nde_next_t* nde)
{
	/* the parser checks if next has been called in the begin/end
	 * block or whereever inappropriate. so the runtime doesn't 
	 * check that explicitly */
	if  (run->active_block == (hawk_nde_blk_t*)run->awk->tree.begin)
	{
		SETERR_LOC (run, HAWK_ERNEXTBEG, &nde->loc);
		return -1;
	}
	else if (run->active_block == (hawk_nde_blk_t*)run->awk->tree.end)
	{
		SETERR_LOC (run, HAWK_ERNEXTEND, &nde->loc);
		return -1;
	}

	run->exit_level = EXIT_NEXT;
	return 0;
}

static int run_nextinfile (hawk_rtx_t* rtx, hawk_nde_nextfile_t* nde)
{
	int n;

	/* normal nextfile statement */
	if  (rtx->active_block == (hawk_nde_blk_t*)rtx->awk->tree.begin)
	{
		SETERR_LOC (rtx, HAWK_ERNEXTFBEG, &nde->loc);
		return -1;
	}
	else if (rtx->active_block == (hawk_nde_blk_t*)rtx->awk->tree.end)
	{
		SETERR_LOC (rtx, HAWK_ERNEXTFEND, &nde->loc);
		return -1;
	}

	n = hawk_rtx_nextio_read (rtx, HAWK_IN_CONSOLE, HAWK_T(""));
	if (n == -1)
	{
		ADJERR_LOC (rtx, &nde->loc);
		return -1;
	}

	if (n == 0)
	{
		/* no more input console */
		rtx->exit_level = EXIT_GLOBAL;
		return 0;
	}

	/* FNR resets to 0, NR remains the same */
	if (update_fnr (rtx, 0, rtx->gbl.nr) == -1) 
	{
		ADJERR_LOC (rtx, &nde->loc);
		return -1;
	}

	rtx->exit_level = EXIT_NEXT;
	return 0;

}

static int run_nextoutfile (hawk_rtx_t* rtx, hawk_nde_nextfile_t* nde)
{
	int n;

	/* nextofile can be called from BEGIN and END block unlike nextfile */

	n = hawk_rtx_nextio_write (rtx, HAWK_OUT_CONSOLE, HAWK_T(""));
	if (n == -1)
	{
		/* adjust the error line */
		ADJERR_LOC (rtx, &nde->loc);
		return -1;
	}

	if (n == 0)
	{
		/* should it terminate the program there is no more 
		 * output console? no. there will just be no more console 
		 * output */
		/*rtx->exit_level = EXIT_GLOBAL;*/
		return 0;
	}

	return 0;
}

static int run_nextfile (hawk_rtx_t* rtx, hawk_nde_nextfile_t* nde)
{
	return (nde->out)? 
		run_nextoutfile (rtx, nde): 
		run_nextinfile (rtx, nde);
}

static int delete_indexed (
	hawk_rtx_t* rtx, hawk_htb_t* map, hawk_nde_var_t* var)
{
	hawk_val_t* idx;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), var->idx != HAWK_NULL);

	idx = eval_expression (rtx, var->idx);
	if (idx == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, idx);

	if (HAWK_RTX_GETVALTYPE(rtx, idx) == HAWK_VAL_STR)
	{
		/* delete x["abc"] */
		hawk_htb_delete (map, ((hawk_val_str_t*)idx)->val.ptr, ((hawk_val_str_t*)idx)->val.len);
		hawk_rtx_refdownval (rtx, idx);
	}
	else
	{
		/* delete x[20] */
		hawk_ooch_t buf[IDXBUFSIZE];
		hawk_rtx_valtostr_out_t out;

		/* try with a fixed-size buffer */
		out.type = HAWK_RTX_VALTOSTR_CPLCPY;
		out.u.cplcpy.ptr = buf;
		out.u.cplcpy.len = HAWK_COUNTOF(buf);
		if (hawk_rtx_valtostr(rtx, idx, &out) <= -1)
		{
			int n;

			/* retry it in dynamic mode */
			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			n = hawk_rtx_valtostr(rtx, idx, &out);
			hawk_rtx_refdownval (rtx, idx);
			if (n <= -1)
			{
				/* change the error line */
				ADJERR_LOC (rtx, &var->loc);
				return -1;
			}
			else
			{
				hawk_htb_delete (map, out.u.cpldup.ptr, out.u.cpldup.len);
				hawk_rtx_freemem (rtx, out.u.cpldup.ptr);
			}
		}
		else 
		{
			hawk_rtx_refdownval (rtx, idx);
			hawk_htb_delete (map, out.u.cplcpy.ptr, out.u.cplcpy.len);
		}
	}

	return 0;
}

static int run_delete_named (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	hawk_htb_pair_t* pair;

	/* if a named variable has an index part and a named indexed variable
	 * doesn't have an index part, the program is definitely wrong */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		(var->type == HAWK_NDE_NAMED && var->idx == HAWK_NULL) ||
		(var->type == HAWK_NDE_NAMEDIDX && var->idx != HAWK_NULL));

	pair = hawk_htb_search(rtx->named, var->id.name.ptr, var->id.name.len);
	if (pair == HAWK_NULL)
	{
		hawk_val_t* tmp;

		/* value not set for the named variable. 
		 * create a map and assign it to the variable */

		tmp = hawk_rtx_makemapval(rtx);
		if (tmp == HAWK_NULL) 
		{
			/* adjust error line */
			ADJERR_LOC (rtx, &var->loc);
			return -1;
		}

		pair = hawk_htb_upsert(rtx->named, var->id.name.ptr, var->id.name.len, tmp, 0);
		if (pair == HAWK_NULL)
		{
			hawk_rtx_refupval (rtx, tmp);
			hawk_rtx_refdownval (rtx, tmp);
			ADJERR_LOC (rtx, &var->loc);
			return -1;
		}

		/* as this is the assignment, it needs to update
		 * the reference count of the target value. */
		hawk_rtx_refupval (rtx, tmp);
	}
	else
	{
		hawk_val_t* val;
		hawk_htb_t* map;

		val = (hawk_val_t*)HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), val != HAWK_NULL);

		if (HAWK_RTX_GETVALTYPE (rtx, val) != HAWK_VAL_MAP)
		{
			SETERR_ARGX_LOC (rtx, HAWK_ENOTDEL, &var->id.name, &var->loc);
			return -1;
		}

		map = ((hawk_val_map_t*)val)->map;
		if (var->type == HAWK_NDE_NAMEDIDX)
		{
			if (delete_indexed(rtx, map, var) <= -1) return -1;
		}
		else
		{
			hawk_htb_clear (map);
		}
	}

	return 0;
}

static int run_delete_unnamed (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	hawk_val_t* val;
	hawk_val_type_t vtype;

	switch (var->type)
	{
		case HAWK_NDE_GBL:
		case HAWK_NDE_GBLIDX:
			val = RTX_STACK_GBL(rtx,var->id.idxa);
			break;
	
		case HAWK_NDE_LCL:
		case HAWK_NDE_LCLIDX:
			val = RTX_STACK_LCL(rtx,var->id.idxa);
			break;

		default:
			val = RTX_STACK_ARG(rtx,var->id.idxa);
			break;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), val != HAWK_NULL);

	vtype = HAWK_RTX_GETVALTYPE(rtx, val);
	if (vtype == HAWK_VAL_NIL)
	{
		hawk_val_t* tmp;

		/* value not set.
		 * create a map and assign it to the variable */

		tmp = hawk_rtx_makemapval(rtx);
		if (tmp == HAWK_NULL) 
		{
			ADJERR_LOC (rtx, &var->loc);
			return -1;
		}

		/* no need to reduce the reference count of
		 * the previous value because it was nil. */
		switch (var->type)
		{
			case HAWK_NDE_GBL:
			case HAWK_NDE_GBLIDX:
			{
				int x;

				hawk_rtx_refupval (rtx, tmp);
				x = hawk_rtx_setgbl(rtx, (int)var->id.idxa, tmp);
				hawk_rtx_refdownval (rtx, tmp);

				if (x <= -1)
				{
					ADJERR_LOC (rtx, &var->loc);
					return -1;
				}
				break;
			}

			case HAWK_NDE_LCL:
			case HAWK_NDE_LCLIDX:
				RTX_STACK_LCL(rtx,var->id.idxa) = tmp;
				hawk_rtx_refupval (rtx, tmp);
				break;

			default:
				RTX_STACK_ARG(rtx,var->id.idxa) = tmp;
				hawk_rtx_refupval (rtx, tmp);
				break;
		}
	}
	else
	{
		hawk_htb_t* map;

		if (vtype != HAWK_VAL_MAP)
		{
			SETERR_ARGX_LOC (rtx, HAWK_ENOTDEL, &var->id.name, &var->loc);
			return -1;
		}

		map = ((hawk_val_map_t*)val)->map;
		if (var->type == HAWK_NDE_GBLIDX ||
		    var->type == HAWK_NDE_LCLIDX ||
		    var->type == HAWK_NDE_ARGIDX)
		{
			if (delete_indexed(rtx, map, var) <= -1) return -1;
		}
		else
		{
			hawk_htb_clear (map);
		}
	}

	return 0;
}

static int run_delete (hawk_rtx_t* rtx, hawk_nde_delete_t* nde)
{
	hawk_nde_var_t* var;

	var = (hawk_nde_var_t*)nde->var;

	switch (var->type)
	{
		case HAWK_NDE_NAMED:
		case HAWK_NDE_NAMEDIDX:
			return run_delete_named(rtx, var);
		
		case HAWK_NDE_GBL:
		case HAWK_NDE_LCL:
		case HAWK_NDE_ARG:
		case HAWK_NDE_GBLIDX:
		case HAWK_NDE_LCLIDX:
		case HAWK_NDE_ARGIDX:
			return run_delete_unnamed(rtx, var);

		default:
			/* the delete statement cannot be called with other nodes than
			 * the variables such as a named variable, a named indexed variable, etc */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - wrong target for delete");
			SETERR_LOC (rtx, HAWK_EBADARG, &var->loc);
			return -1;
	}

}

static int reset_variable (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	switch (var->type)
	{
		case HAWK_NDE_NAMED:
			/* if a named variable has an index part, something is definitely wrong */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), var->type == HAWK_NDE_NAMED && var->idx == HAWK_NULL);

			/* a named variable can be reset if removed from a internal map 
			   to manage it */
			hawk_htb_delete (rtx->named, var->id.name.ptr, var->id.name.len);
			return 0;

		case HAWK_NDE_GBL:
		case HAWK_NDE_LCL:
		case HAWK_NDE_ARG:
		{
			hawk_val_t* val;

			switch (var->type)
			{
				case HAWK_NDE_GBL:
					val = RTX_STACK_GBL(rtx,var->id.idxa);
					break;

				case HAWK_NDE_LCL:
					val = RTX_STACK_LCL(rtx,var->id.idxa);
					break;

				case HAWK_NDE_ARG:
					val = RTX_STACK_ARG(rtx,var->id.idxa);
					break;
			}

			HAWK_ASSERT (hawk_rtx_gethawk(rtx), val != HAWK_NULL);

			if (HAWK_RTX_GETVALTYPE (rtx, val) != HAWK_VAL_NIL)
			{
				hawk_rtx_refdownval (rtx, val);
				switch (var->type)
				{
					case HAWK_NDE_GBL:
						RTX_STACK_GBL(rtx,var->id.idxa) = hawk_val_nil;
						break;
					
					case HAWK_NDE_LCL:
						RTX_STACK_LCL(rtx,var->id.idxa) = hawk_val_nil;
						break;

					case HAWK_NDE_ARG:
						RTX_STACK_ARG(rtx,var->id.idxa) = hawk_val_nil;
						break;
				}
			}
			return 0;
		}

		default:
			/* the reset statement can only be called with plain variables */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - wrong target for reset");
			SETERR_LOC (rtx, HAWK_EBADARG, &var->loc);
			return -1;
	}
}

static int run_reset (hawk_rtx_t* rtx, hawk_nde_reset_t* nde)
{
	return reset_variable (rtx, (hawk_nde_var_t*)nde->var);
}

static int run_print (hawk_rtx_t* rtx, hawk_nde_print_t* nde)
{
	hawk_ooch_t* out = HAWK_NULL;
	hawk_val_t* out_v = HAWK_NULL;
	const hawk_ooch_t* dst;
	int n, xret = 0;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		(nde->out_type == HAWK_OUT_PIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_RWPIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_FILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_APFILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_CONSOLE && nde->out == HAWK_NULL));

	/* check if destination has been specified. */
	if (nde->out)
	{
		hawk_oow_t len;

		/* if so, resolve the destination name */
		out_v = eval_expression(rtx, nde->out);
		if (!out_v) return -1;

		hawk_rtx_refupval (rtx, out_v);

		out = hawk_rtx_getvaloocstr(rtx, out_v, &len);
		if (!out) goto oops;

		if (len <= 0) 
		{
			/* the destination name is empty */
			SETERR_LOC (rtx, HAWK_EIONMEM, &nde->loc);
			goto oops;
		}

		/* it needs to check if the destination name contains
		 * any invalid characters to the underlying system */
		while (len > 0)
		{
			if (out[--len] == HAWK_T('\0'))
			{
				/* provide length up to one character before 
				 * the first null not to contains a null
				 * in an error message */
				SETERR_ARG_LOC (
					rtx, HAWK_EIONMNL, 
					out, hawk_count_oocstr(out), &nde->loc);

				/* if so, it skips writing */
				goto oops_1;
			}
		}
	}

	/* transforms the destination to suit the usage with io */
	dst = (out == HAWK_NULL)? HAWK_T(""): out;

	/* check if print is followed by any arguments */
	if (!nde->args)
	{
		/* if it doesn't have any arguments, print the entire 
		 * input record */
		n = hawk_rtx_writeiostr(rtx, nde->out_type, dst, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line));
		if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/)
		{
			if (rtx->awk->opt.trait & HAWK_TOLERANT)
			{
				xret = PRINT_IOERR;
			}
			else
			{
				goto oops;
			}
		}
	}
	else
	{
		/* if it has any arguments, print the arguments separated by
		 * the value OFS */
		hawk_nde_t* head, * np;
		hawk_val_t* v;

		if (nde->args->type == HAWK_NDE_GRP)
		{
			/* parenthesized print */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->args->next == HAWK_NULL);
			head = ((hawk_nde_grp_t*)nde->args)->body;
		}
		else head = nde->args;

		for (np = head; np != HAWK_NULL; np = np->next)
		{
			if (np != head)
			{
				n = hawk_rtx_writeiostr(rtx, nde->out_type, dst, rtx->gbl.ofs.ptr, rtx->gbl.ofs.len);
				if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/) 
				{
					if (rtx->awk->opt.trait & HAWK_TOLERANT)
					{
						xret = PRINT_IOERR;
					}
					else
					{
						goto oops;
					}
				}
			}

			v = eval_expression(rtx, np);
			if (v == HAWK_NULL) goto oops_1;

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_writeioval(rtx, nde->out_type, dst, v);
			hawk_rtx_refdownval (rtx, v);

			if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/) 
			{
				if (rtx->awk->opt.trait & HAWK_TOLERANT)
				{
					xret = PRINT_IOERR;
				}
				else
				{
					goto oops;
				}
			}
		}
	}

	/* print the value ORS to terminate the operation */
	n = hawk_rtx_writeiostr(rtx, nde->out_type, dst, rtx->gbl.ors.ptr, rtx->gbl.ors.len);
	if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/)
	{
		if (rtx->awk->opt.trait & HAWK_TOLERANT)
		{
			xret = PRINT_IOERR;
		}
		else
		{
			goto oops;
		}
	}

	/* unlike printf, flushio() is not needed here as print 
	 * inserts <NL> that triggers auto-flush */
	if (out_v)
	{
		if (out) hawk_rtx_freevaloocstr (rtx, out_v, out);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return xret;

oops:
	ADJERR_LOC (rtx, &nde->loc);

oops_1:
	if (out_v)
	{
		if (out) hawk_rtx_freevaloocstr (rtx, out_v, out);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return -1;
}

static int run_printf (hawk_rtx_t* rtx, hawk_nde_print_t* nde)
{
	hawk_ooch_t* out = HAWK_NULL;
	hawk_val_t* out_v = HAWK_NULL;
	hawk_val_t* v;
	hawk_val_type_t vtype;
	const hawk_ooch_t* dst;
	hawk_nde_t* head;
	int n, xret = 0;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		(nde->out_type == HAWK_OUT_PIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_RWPIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_FILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_APFILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_CONSOLE && nde->out == HAWK_NULL));

	if (nde->out)
	{
		hawk_oow_t len;

		out_v = eval_expression(rtx, nde->out);
		if (!out_v) return -1;

		hawk_rtx_refupval (rtx, out_v);

		out = hawk_rtx_getvaloocstr(rtx, out_v, &len);
		if (!out) goto oops;

		if (len <= 0) 
		{
			/* the output destination name is empty. */
			SETERR_LOC (rtx, HAWK_EIONMEM, &nde->loc);
			goto oops_1;
		}

		while (len > 0)
		{
			if (out[--len] == HAWK_T('\0'))
			{
				/* provide length up to one character before 
				 * the first null not to contains a null
				 * in an error message */
				SETERR_ARG_LOC (rtx, HAWK_EIONMNL, out, hawk_count_oocstr(out), &nde->loc);

				/* the output destination name contains a null 
				 * character. */
				goto oops_1;
			}
		}
	}

	dst = (out == HAWK_NULL)? HAWK_T(""): out;

	/*( valid printf statement should have at least one argument. the parser must ensure this. */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->args != HAWK_NULL);

	if (nde->args->type == HAWK_NDE_GRP)
	{
		/* parenthesized print */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->args->next == HAWK_NULL);
		head = ((hawk_nde_grp_t*)nde->args)->body;
	}
	else head = nde->args;

	/* valid printf statement should have at least one argument. the parser must ensure this */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), head != HAWK_NULL);

	v = eval_expression(rtx, head);
	if (v == HAWK_NULL) goto oops_1;
	
	hawk_rtx_refupval (rtx, v);
	vtype = HAWK_RTX_GETVALTYPE(rtx, v);
	switch (vtype)
	{
		case HAWK_VAL_STR:
			/* perform the formatted output */
			n = output_formatted(rtx, nde->out_type, dst, ((hawk_val_str_t*)v)->val.ptr, ((hawk_val_str_t*)v)->val.len, head->next);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1)
			{
				if (n == PRINT_IOERR) xret = n;
				else goto oops;
			}
			break;

		case HAWK_VAL_MBS:
			/* perform the formatted output */
			n = output_formatted_bytes(rtx, nde->out_type, dst, ((hawk_val_mbs_t*)v)->val.ptr, ((hawk_val_mbs_t*)v)->val.len, head->next);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1)
			{
				if (n == PRINT_IOERR) xret = n;
				else goto oops;
			}
			break;

		default:
			/* the remaining arguments are ignored as the format cannot 
			 * contain any % characters. e.g. printf (1, "xxxx") */
			n = hawk_rtx_writeioval(rtx, nde->out_type, dst, v);
			hawk_rtx_refdownval (rtx, v);

			if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/)
			{
				if (rtx->awk->opt.trait & HAWK_TOLERANT) xret = PRINT_IOERR;
				else goto oops;
			}
			break;
	}

	if (hawk_rtx_flushio(rtx, nde->out_type, dst) <= -1)
	{
		if (rtx->awk->opt.trait & HAWK_TOLERANT) xret = PRINT_IOERR;
		else goto oops_1;
	}

	if (out_v)
	{
		if (out) hawk_rtx_freevaloocstr (rtx, out_v, out);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return xret;

oops:
	ADJERR_LOC (rtx, &nde->loc);

oops_1:
	if (out_v)
	{
		if (out) hawk_rtx_freevaloocstr (rtx, out_v, out);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return -1;
}

static int output_formatted (
	hawk_rtx_t* rtx, int out_type, const hawk_ooch_t* dst, 
	const hawk_ooch_t* fmt, hawk_oow_t fmt_len, hawk_nde_t* args)
{
	hawk_ooch_t* ptr;
	hawk_oow_t len;
	int n;

	ptr = hawk_rtx_format(rtx, HAWK_NULL, HAWK_NULL, fmt, fmt_len, 0, args, &len);
	if (!ptr) return -1;

	n = hawk_rtx_writeiostr(rtx, out_type, dst, ptr, len);
	if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/) 
	{
		return (rtx->awk->opt.trait & HAWK_TOLERANT)? PRINT_IOERR: -1;
	}

	return 0;
}

static int output_formatted_bytes (
	hawk_rtx_t* rtx, int out_type, const hawk_ooch_t* dst, 
	const hawk_bch_t* fmt, hawk_oow_t fmt_len, hawk_nde_t* args)
{
	hawk_bch_t* ptr;
	hawk_oow_t len;
	int n;

	ptr = hawk_rtx_formatmbs(rtx, HAWK_NULL, HAWK_NULL, fmt, fmt_len, 0, args, &len);
	if (!ptr) return -1;

	n = hawk_rtx_writeiobytes(rtx, out_type, dst, ptr, len);
	if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/) 
	{
		return (rtx->awk->opt.trait & HAWK_TOLERANT)? PRINT_IOERR: -1;
	}

	return 0;
}

static hawk_val_t* eval_expression (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* v;
	int n;

#if 0
	if (rtx->exit_level >= EXIT_GLOBAL) 
	{
		/* returns HAWK_NULL as if an error occurred but
		 * clears the error number. run_main will 
		 * detect this condition and treat it as a 
		 * non-error condition.*/
		rtx->errinf.num = HAWK_ENOERR;
		return HAWK_NULL;
	}
#endif

	v = eval_expression0 (rtx, nde);
	if (v == HAWK_NULL) return HAWK_NULL;

	if (HAWK_RTX_GETVALTYPE (rtx, v) == HAWK_VAL_REX)
	{
		hawk_oocs_t vs;

		/* special case where a regular expression is used in
		 * without any match operators:
		 *    print /abc/;
		 * perform match against $0.
		 */
		hawk_rtx_refupval (rtx, v);

		if (HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.d0) == HAWK_VAL_NIL)
		{
			/* the record has never been read. 
			 * probably, this function has been triggered
			 * by the statements in the BEGIN block */
			vs.ptr = HAWK_T("");
			vs.len = 0;
		}
		else
		{
			/* the internal value representing $0 should always be of the string type once it has been set/updated. it is nil initially. */
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.d0) == HAWK_VAL_STR);
			vs.ptr = ((hawk_val_str_t*)rtx->inrec.d0)->val.ptr;
			vs.len = ((hawk_val_str_t*)rtx->inrec.d0)->val.len;
		}

		n = hawk_rtx_matchval(rtx, v, &vs, &vs, HAWK_NULL, HAWK_NULL);
		if (n <= -1)
		{
			ADJERR_LOC (rtx, &nde->loc);
			hawk_rtx_refdownval (rtx, v);
			return HAWK_NULL;
		}
		hawk_rtx_refdownval (rtx, v);

		v = hawk_rtx_makeintval(rtx, (n != 0));
		if (v == HAWK_NULL) 
		{
			/* adjust error line */
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}
	}

	return v;
}

static hawk_val_t* eval_expression0 (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	static eval_expr_t __evaluator[] =
	{
		/* the order of functions here should match the order
		 * of node types(hawk_nde_type_t) declared in hawk.h */
		eval_group,
		eval_assignment,
		eval_binary,
		eval_unary,
		eval_incpre,
		eval_incpst,
		eval_cnd,
		eval_fncall_fnc,
		eval_fncall_fun,
		eval_fncall_var,
		eval_int,
		eval_flt,
		eval_str,
		eval_mbs,
		eval_rex,
		eval_fun,
		eval_named,
		eval_gbl,
		eval_lcl,
		eval_arg,
		eval_namedidx,
		eval_gblidx,
		eval_lclidx,
		eval_argidx,
		eval_pos,
		eval_getline,
		eval_print,
		eval_printf
	};

	hawk_val_t* v;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->type >= HAWK_NDE_GRP && (nde->type - HAWK_NDE_GRP) < HAWK_COUNTOF(__evaluator));

	v = __evaluator[nde->type-HAWK_NDE_GRP](rtx, nde);

	if (v != HAWK_NULL && rtx->exit_level >= EXIT_GLOBAL)
	{
		hawk_rtx_refupval (rtx, v);
		hawk_rtx_refdownval (rtx, v);

		/* returns HAWK_NULL as if an error occurred but
		 * clears the error number. run_main will 
		 * detect this condition and treat it as a 
		 * non-error condition.*/
		hawk_rtx_seterrnum (rtx, HAWK_ENOERR, HAWK_NULL);
		return HAWK_NULL;
	}

	return v;
}

static hawk_val_t* eval_group (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
#if 0
	/* eval_binop_in evaluates the HAWK_NDE_GRP specially.
	 * so this function should never be reached. */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - NDE_GRP only for in");
	SETERR_LOC (rtx, HAWK_EINTERN, &nde->loc);
	return HAWK_NULL;
#endif

	/* a group can be evauluated in a normal context
	 * if a program is parsed with HAWK_TOLERANT on. 
	 * before the introduction of this option, the grouped
	 * expression was valid only coerced with the 'in' 
	 * operator. 
	 * */

	/* when a group is evaluated in a normal context,
	 * we return the last expression as a value. */

	hawk_val_t* val;
	hawk_nde_t* np;

	np = ((hawk_nde_grp_t*)nde)->body;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), np != HAWK_NULL);

loop:
	val = eval_expression (rtx, np);
	if (val == HAWK_NULL) return HAWK_NULL;

	np = np->next;
	if (np)
	{
		hawk_rtx_refupval (rtx, val);
		hawk_rtx_refdownval (rtx, val);
		goto loop;
	}

	return val;
}

static hawk_val_t* eval_assignment (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val, * ret;
	hawk_nde_ass_t* ass = (hawk_nde_ass_t*)nde;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), ass->left != HAWK_NULL);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), ass->right != HAWK_NULL);

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), ass->right->next == HAWK_NULL);
	val = eval_expression (rtx, ass->right);
	if (val == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (rtx, val);

	if (ass->opcode != HAWK_ASSOP_NONE)
	{
		hawk_val_t* val2, * tmp;
		static binop_func_t binop_func[] =
		{
			/* this table must match hawk_assop_type_t in rtx.h */
			HAWK_NULL, /* HAWK_ASSOP_NONE */
			eval_binop_plus,
			eval_binop_minus,
			eval_binop_mul,
			eval_binop_div,
			eval_binop_idiv,
			eval_binop_mod,
			eval_binop_exp,
			eval_binop_concat,
			eval_binop_rshift,
			eval_binop_lshift,
			eval_binop_band,
			eval_binop_bxor,
			eval_binop_bor
		};

		HAWK_ASSERT (hawk_rtx_gethawk(rtx), ass->left->next == HAWK_NULL);
		val2 = eval_expression(rtx, ass->left);
		if (val2 == HAWK_NULL)
		{
			hawk_rtx_refdownval (rtx, val);
			return HAWK_NULL;
		}

		hawk_rtx_refupval (rtx, val2);

		HAWK_ASSERT (hawk_rtx_gethawk(rtx), ass->opcode >= 0);
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), ass->opcode < HAWK_COUNTOF(binop_func));
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), binop_func[ass->opcode] != HAWK_NULL);

		tmp = binop_func[ass->opcode](rtx, val2, val);
		if (tmp == HAWK_NULL)
		{
			hawk_rtx_refdownval (rtx, val2);
			hawk_rtx_refdownval (rtx, val);
			return HAWK_NULL;
		}

		hawk_rtx_refdownval (rtx, val2);
		hawk_rtx_refdownval (rtx, val);

		val = tmp;
		hawk_rtx_refupval (rtx, val);
	}

	ret = do_assignment(rtx, ass->left, val);
	hawk_rtx_refdownval (rtx, val);

	return ret;
}

static hawk_val_t* do_assignment (hawk_rtx_t* rtx, hawk_nde_t* var, hawk_val_t* val)
{
	hawk_val_t* ret;
	hawk_errnum_t errnum;

	switch (var->type)
	{
		case HAWK_NDE_NAMED:
		case HAWK_NDE_GBL:
		case HAWK_NDE_LCL:
		case HAWK_NDE_ARG:
			ret = do_assignment_nonidx(rtx, (hawk_nde_var_t*)var, val);
			break;

		case HAWK_NDE_NAMEDIDX:
		case HAWK_NDE_GBLIDX:
		case HAWK_NDE_LCLIDX:
		case HAWK_NDE_ARGIDX:
			if (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_MAP)
			{
				/* a map cannot become a member of a map */
				errnum = HAWK_EMAPTOIDX;
				goto exit_on_error;
			}

			ret = do_assignment_idx(rtx, (hawk_nde_var_t*)var, val);
			break;

		case HAWK_NDE_POS:
			if (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_MAP)
			{
				/* a map cannot be assigned to a positional */
				errnum = HAWK_EMAPTOPOS;
				goto exit_on_error;
			}

			ret = do_assignment_pos(rtx, (hawk_nde_pos_t*)var, val);
			break;

		default:
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - invalid variable type");
			errnum = HAWK_EINTERN;
			goto exit_on_error;
	}

	return ret;

exit_on_error:
	SETERR_LOC (rtx, errnum, &var->loc);
	return HAWK_NULL;
}

static hawk_val_t* do_assignment_nonidx (hawk_rtx_t* run, hawk_nde_var_t* var, hawk_val_t* val)
{
	hawk_val_type_t vtype;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		var->type == HAWK_NDE_NAMED ||
		var->type == HAWK_NDE_GBL ||
		var->type == HAWK_NDE_LCL ||
		var->type == HAWK_NDE_ARG
	);

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), var->idx == HAWK_NULL);
	vtype = HAWK_RTX_GETVALTYPE (rtx, val);

	switch (var->type)
	{
		case HAWK_NDE_NAMED:
		{
			hawk_htb_pair_t* pair;

			pair = hawk_htb_search (run->named, var->id.name.ptr, var->id.name.len);

			if (!(run->awk->opt.trait & HAWK_FLEXMAP))
			{
				if (pair && HAWK_RTX_GETVALTYPE (rtx, (hawk_val_t*)HAWK_HTB_VPTR(pair)) == HAWK_VAL_MAP)
				{
					/* old value is a map - it can only be accessed through indexing. */
					hawk_errnum_t errnum;
					errnum = (vtype == HAWK_VAL_MAP)? HAWK_ENMAPTOMAP: HAWK_ENMAPTOSCALAR;
					SETERR_ARGX_LOC (run, errnum, &var->id.name, &var->loc);
					return HAWK_NULL;
				}
				else if (vtype == HAWK_VAL_MAP)
				{
					/* old value is not a map but a new value is a map.
					 * a map cannot be assigned to a variable if FLEXMAP is off. */
					SETERR_ARGX_LOC (run, HAWK_EMAPTONVAR, &var->id.name, &var->loc);
					return HAWK_NULL;
				}
			}

			if (hawk_htb_upsert (run->named, var->id.name.ptr, var->id.name.len, val, 0) == HAWK_NULL)
			{
				ADJERR_LOC (run, &var->loc);
				return HAWK_NULL;
			}

			hawk_rtx_refupval (run, val);
			break;
		}

		case HAWK_NDE_GBL:
		{
			if (set_global(run, var->id.idxa, var, val, 1) == -1) 
			{
				ADJERR_LOC (run, &var->loc);
				return HAWK_NULL;
			}
			break;
		}

		case HAWK_NDE_LCL:
		{
			hawk_val_t* old = RTX_STACK_LCL(run,var->id.idxa);

			if (!(run->awk->opt.trait & HAWK_FLEXMAP))
			{
				if (HAWK_RTX_GETVALTYPE (rtx, old) == HAWK_VAL_MAP)
				{
					/* old value is a map - it can only be accessed through indexing. */
					hawk_errnum_t errnum;
					errnum = (vtype == HAWK_VAL_MAP)? HAWK_ENMAPTOMAP: HAWK_ENMAPTOSCALAR;
					SETERR_ARGX_LOC (run, errnum, &var->id.name, &var->loc);
					return HAWK_NULL;
				}
				else if (vtype == HAWK_VAL_MAP)
				{
					/* old value is not a map but a new value is a map.
					 * a map cannot be assigned to a variable if FLEXMAP is off. */
					SETERR_ARGX_LOC (run, HAWK_EMAPTONVAR, &var->id.name, &var->loc);
					return HAWK_NULL;
				}
			}

			hawk_rtx_refdownval (run, old);
			RTX_STACK_LCL(run,var->id.idxa) = val;
			hawk_rtx_refupval (run, val);
			break;
		}

		case HAWK_NDE_ARG:
		{
			hawk_val_t* old = RTX_STACK_ARG(run,var->id.idxa);

			if (!(run->awk->opt.trait & HAWK_FLEXMAP))
			{
				if (HAWK_RTX_GETVALTYPE(rtx, old) == HAWK_VAL_MAP)
				{
					/* old value is a map - it can only be accessed through indexing. */
					hawk_errnum_t errnum;
					errnum = (vtype == HAWK_VAL_MAP)? HAWK_ENMAPTOMAP: HAWK_ENMAPTOSCALAR;
					SETERR_ARGX_LOC (run, errnum, &var->id.name, &var->loc);
					return HAWK_NULL;
				}
				else if (vtype == HAWK_VAL_MAP)
				{
					/* old value is not a map but a new value is a map.
					 * a map cannot be assigned to a variable if FLEXMAP is off. */
					SETERR_ARGX_LOC (run, HAWK_EMAPTONVAR, &var->id.name, &var->loc);
					return HAWK_NULL;
				}
			}

			hawk_rtx_refdownval (run, old);
			RTX_STACK_ARG(run,var->id.idxa) = val;
			hawk_rtx_refupval (run, val);
			break;
		}

	}

	return val;
}

static hawk_val_t* do_assignment_idx (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val)
{
	hawk_val_map_t* map;
	hawk_val_type_t mvtype;
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[IDXBUFSIZE];

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		(var->type == HAWK_NDE_NAMEDIDX ||
		 var->type == HAWK_NDE_GBLIDX ||
		 var->type == HAWK_NDE_LCLIDX ||
		 var->type == HAWK_NDE_ARGIDX) && var->idx != HAWK_NULL);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), HAWK_RTX_GETVALTYPE (rtx, val) != HAWK_VAL_MAP);

retry:
	switch (var->type)
	{
		case HAWK_NDE_NAMEDIDX:
		{
			hawk_htb_pair_t* pair;
			pair = hawk_htb_search(rtx->named, var->id.name.ptr, var->id.name.len);
			map = (pair == HAWK_NULL)? (hawk_val_map_t*)hawk_val_nil: (hawk_val_map_t*)HAWK_HTB_VPTR(pair);
			break;
		}

		case HAWK_NDE_GBLIDX:
			map = (hawk_val_map_t*)RTX_STACK_GBL(rtx,var->id.idxa);
			break;

		case HAWK_NDE_LCLIDX:
			map = (hawk_val_map_t*)RTX_STACK_LCL(rtx,var->id.idxa);
			break;

		default:  /* HAWK_NDE_ARGIDX */
			map = (hawk_val_map_t*)RTX_STACK_ARG(rtx,var->id.idxa);
			break;
	} 

	mvtype = HAWK_RTX_GETVALTYPE (rtx, map);
	if (mvtype == HAWK_VAL_NIL)
	{
		hawk_val_t* tmp;

		/* the map is not initialized yet */
		tmp = hawk_rtx_makemapval (rtx);
		if (tmp == HAWK_NULL) 
		{
			ADJERR_LOC (rtx, &var->loc);
			return HAWK_NULL;
		}

		switch (var->type)
		{
			case HAWK_NDE_NAMEDIDX:
			{
				/* doesn't have to decrease the reference count 
				 * of the previous value here as it is done by 
				 * hawk_htb_upsert */
				hawk_rtx_refupval (rtx, tmp);
				if (hawk_htb_upsert(rtx->named, var->id.name.ptr, var->id.name.len, tmp, 0) == HAWK_NULL)
				{
					hawk_rtx_refdownval (rtx, tmp);
					ADJERR_LOC (rtx, &var->loc);
					return HAWK_NULL;
				}

				break;
			}

			case HAWK_NDE_GBLIDX:
			{
				int x;

				hawk_rtx_refupval (rtx, tmp);
				x = hawk_rtx_setgbl (rtx, (int)var->id.idxa, tmp);
				hawk_rtx_refdownval (rtx, tmp);
				if (x <= -1)
				{
					ADJERR_LOC (rtx, &var->loc);
					return HAWK_NULL;
				}
				break;
			}

			case HAWK_NDE_LCLIDX:
				hawk_rtx_refdownval (rtx, (hawk_val_t*)map);
				RTX_STACK_LCL(rtx,var->id.idxa) = tmp;
				hawk_rtx_refupval (rtx, tmp);
				break;
			
			default: /* HAWK_NDE_ARGIDX */
				hawk_rtx_refdownval (rtx, (hawk_val_t*)map);
				RTX_STACK_ARG(rtx,var->id.idxa) = tmp;
				hawk_rtx_refupval (rtx, tmp);
				break;
		}

		map = (hawk_val_map_t*)tmp;
	}
	else if (mvtype != HAWK_VAL_MAP)
	{
		if (rtx->awk->opt.trait & HAWK_FLEXMAP)
		{
			/* if FLEXMAP is on, you can switch a scalar value to a map */
			hawk_nde_var_t fake;

			/* create a fake non-indexed variable node */
			fake = *var;
			/* NOTE: type conversion by decrementing by 4 is 
			 *       dependent on the hawk_nde_type_t 
			 *       enumerators defined in <hawk.h> */
			fake.type = var->type - 4; 
			fake.idx = HAWK_NULL;

			reset_variable (rtx, &fake);
			goto retry;
		}
		else
		{
			/* you can't manipulate a variable pointing to
			 * a scalar value with an index if FLEXMAP is off. */
			SETERR_LOC (rtx, HAWK_ESCALARTOMAP, &var->loc);
			return HAWK_NULL;
		}
	}

	len = HAWK_COUNTOF(idxbuf);
	str = idxnde_to_str(rtx, var->idx, idxbuf, &len);
	if (str == HAWK_NULL) return HAWK_NULL;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("**** index str=>%s, map->ref=%d, map->type=%d\n"), 
		str, (int)map->ref, (int)map->type);
#endif

	if (hawk_htb_upsert(map->map, str, len, val, 0) == HAWK_NULL)
	{
		if (str != idxbuf) hawk_rtx_freemem (rtx, str);
		ADJERR_LOC (rtx, &var->loc);
		return HAWK_NULL;
	}

	if (str != idxbuf) hawk_rtx_freemem (rtx, str);
	hawk_rtx_refupval (rtx, val);
	return val;
}

static hawk_val_t* do_assignment_pos (hawk_rtx_t* rtx, hawk_nde_pos_t* pos, hawk_val_t* val)
{
	hawk_val_t* v;
	hawk_val_type_t vtype;
	hawk_int_t lv;
	hawk_oocs_t str;
	int n;

	v = eval_expression (rtx, pos->val);
	if (v == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (rtx, v);
	n = hawk_rtx_valtoint (rtx, v, &lv);
	hawk_rtx_refdownval (rtx, v);

	if (n <= -1) 
	{
		SETERR_LOC (rtx, HAWK_EPOSIDX, &pos->loc);
		return HAWK_NULL;
	}

	if (!IS_VALID_POSIDX(lv)) 
	{
		SETERR_LOC (rtx, HAWK_EPOSIDX, &pos->loc);
		return HAWK_NULL;
	}

	vtype = HAWK_RTX_GETVALTYPE (rtx, val);
	if (vtype == HAWK_VAL_STR)
	{
		str = ((hawk_val_str_t*)val)->val;
	}
	else
	{
		hawk_rtx_valtostr_out_t out;

		out.type = HAWK_RTX_VALTOSTR_CPLDUP;
		if (hawk_rtx_valtostr (rtx, val, &out) <= -1)
		{
			ADJERR_LOC (rtx, &pos->loc);
			return HAWK_NULL;
		}

		str = out.u.cpldup;
	}
	
	n = hawk_rtx_setrec (rtx, (hawk_oow_t)lv, &str);

	if (vtype == HAWK_VAL_STR) 
	{
		/* do nothing */
	}
	else
	{
		hawk_rtx_freemem (rtx, str.ptr);
	}

	if (n <= -1) return HAWK_NULL;
	return (lv == 0)? rtx->inrec.d0: rtx->inrec.flds[lv-1].val;
}

static hawk_val_t* eval_binary (hawk_rtx_t* run, hawk_nde_t* nde)
{
	static binop_func_t binop_func[] =
	{
		/* the order of the functions should be inline with
		 * the operator declaration in run.h */

		HAWK_NULL, /* eval_binop_lor */
		HAWK_NULL, /* eval_binop_land */
		HAWK_NULL, /* eval_binop_in */

		eval_binop_bor,
		eval_binop_bxor,
		eval_binop_band,

		eval_binop_teq,
		eval_binop_tne,
		eval_binop_eq,
		eval_binop_ne,
		eval_binop_gt,
		eval_binop_ge,
		eval_binop_lt,
		eval_binop_le,

		eval_binop_lshift,
		eval_binop_rshift,
		
		eval_binop_plus,
		eval_binop_minus,
		eval_binop_mul,
		eval_binop_div,
		eval_binop_idiv,
		eval_binop_mod,
		eval_binop_exp,

		eval_binop_concat,
		HAWK_NULL, /* eval_binop_ma */
		HAWK_NULL  /* eval_binop_nm */
	};

	hawk_nde_exp_t* exp = (hawk_nde_exp_t*)nde;
	hawk_val_t* left, * right, * res;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->type == HAWK_NDE_EXP_BIN);

	if (exp->opcode == HAWK_BINOP_LAND)
	{
		res = eval_binop_land (run, exp->left, exp->right);
	}
	else if (exp->opcode == HAWK_BINOP_LOR)
	{
		res = eval_binop_lor (run, exp->left, exp->right);
	}
	else if (exp->opcode == HAWK_BINOP_IN)
	{
		/* treat the in operator specially */
		res = eval_binop_in (run, exp->left, exp->right);
	}
	else if (exp->opcode == HAWK_BINOP_NM)
	{
		res = eval_binop_nm (run, exp->left, exp->right);
	}
	else if (exp->opcode == HAWK_BINOP_MA)
	{
		res = eval_binop_ma (run, exp->left, exp->right);
	}
	else
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->left->next == HAWK_NULL);
		left = eval_expression (run, exp->left);
		if (left == HAWK_NULL) return HAWK_NULL;

		hawk_rtx_refupval (run, left);

		HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->right->next == HAWK_NULL);
		right = eval_expression (run, exp->right);
		if (right == HAWK_NULL) 
		{
			hawk_rtx_refdownval (run, left);
			return HAWK_NULL;
		}

		hawk_rtx_refupval (run, right);

		HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->opcode >= 0 && 
			exp->opcode < HAWK_COUNTOF(binop_func));
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), binop_func[exp->opcode] != HAWK_NULL);

		res = binop_func[exp->opcode] (run, left, right);
		if (res == HAWK_NULL) ADJERR_LOC (run, &nde->loc);

		hawk_rtx_refdownval (run, left);
		hawk_rtx_refdownval (run, right);
	}

	return res;
}

static hawk_val_t* eval_binop_lor (
	hawk_rtx_t* run, hawk_nde_t* left, hawk_nde_t* right)
{
	/*
	hawk_val_t* res = HAWK_NULL;

	res = hawk_rtx_makeintval (
		run, 
		hawk_rtx_valtobool(run,left) || 
		hawk_rtx_valtobool(run,right)
	);
	if (res == HAWK_NULL)
	{
		ADJERR_LOC (run, &left->loc);
		return HAWK_NULL;
	}

	return res;
	*/

	/* short-circuit evaluation required special treatment */
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), left->next == HAWK_NULL);
	lv = eval_expression (run, left);
	if (lv == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (run, lv);
	if (hawk_rtx_valtobool(run, lv)) 
	{
		res = HAWK_VAL_ONE;
	}
	else
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), right->next == HAWK_NULL);
		rv = eval_expression (run, right);
		if (rv == HAWK_NULL)
		{
			hawk_rtx_refdownval (run, lv);
			return HAWK_NULL;
		}
		hawk_rtx_refupval (run, rv);

		res = hawk_rtx_valtobool(run,rv)? 
			HAWK_VAL_ONE: HAWK_VAL_ZERO;
		hawk_rtx_refdownval (run, rv);
	}

	hawk_rtx_refdownval (run, lv);

	return res;
}

static hawk_val_t* eval_binop_land (hawk_rtx_t* run, hawk_nde_t* left, hawk_nde_t* right)
{
	/*
	hawk_val_t* res = HAWK_NULL;

	res = hawk_rtx_makeintval (
		run, 
		hawk_rtx_valtobool(run,left) &&
		hawk_rtx_valtobool(run,right)
	);
	if (res == HAWK_NULL) 
	{
		ADJERR_LOC (run, &left->loc);
		return HAWK_NULL;
	}

	return res;
	*/

	/* short-circuit evaluation required special treatment */
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), left->next == HAWK_NULL);
	lv = eval_expression (run, left);
	if (lv == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (run, lv);
	if (!hawk_rtx_valtobool(run, lv)) 
	{
		res = HAWK_VAL_ZERO;
	}
	else
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), right->next == HAWK_NULL);
		rv = eval_expression (run, right);
		if (rv == HAWK_NULL)
		{
			hawk_rtx_refdownval (run, lv);
			return HAWK_NULL;
		}
		hawk_rtx_refupval (run, rv);

		res = hawk_rtx_valtobool(run,rv)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
		hawk_rtx_refdownval (run, rv);
	}

	hawk_rtx_refdownval (run, lv);

	return res;
}

static hawk_val_t* eval_binop_in (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_val_t* rv;
	hawk_val_type_t rvtype;
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[IDXBUFSIZE];

	if (right->type != HAWK_NDE_GBL &&
	    right->type != HAWK_NDE_LCL &&
	    right->type != HAWK_NDE_ARG &&
	    right->type != HAWK_NDE_NAMED)
	{
		/* the compiler should have handled this case */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - it needs a plain variable");
		SETERR_LOC (rtx, HAWK_EINTERN, &right->loc);
		return HAWK_NULL;
	}

	/* evaluate the left-hand side of the operator */
	len = HAWK_COUNTOF(idxbuf);
	str = (left->type == HAWK_NDE_GRP)?
		idxnde_to_str (rtx, ((hawk_nde_grp_t*)left)->body, idxbuf, &len):
		idxnde_to_str (rtx, left, idxbuf, &len);
	if (str == HAWK_NULL) return HAWK_NULL;

	/* evaluate the right-hand side of the operator */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), right->next == HAWK_NULL);
	rv = eval_expression (rtx, right);
	if (rv == HAWK_NULL) 
	{
		if (str != idxbuf) hawk_rtx_freemem (rtx, str);
		return HAWK_NULL;
	}

	hawk_rtx_refupval (rtx, rv);

	rvtype = HAWK_RTX_GETVALTYPE (rtx, rv);
	if (rvtype == HAWK_VAL_NIL)
	{
		if (str != idxbuf) hawk_rtx_freemem (rtx, str);
		hawk_rtx_refdownval (rtx, rv);
		return HAWK_VAL_ZERO;
	}
	else if (rvtype == HAWK_VAL_MAP)
	{
		hawk_val_t* res;
		hawk_htb_t* map;

		map = ((hawk_val_map_t*)rv)->map;
		res = (hawk_htb_search(map, str, len) == HAWK_NULL)? HAWK_VAL_ZERO: HAWK_VAL_ONE;

		if (str != idxbuf) hawk_rtx_freemem (rtx, str);
		hawk_rtx_refdownval (rtx, rv);
		return res;
	}

	/* need a map */
	if (str != idxbuf) hawk_rtx_freemem (rtx, str);
	hawk_rtx_refdownval (rtx, rv);

	SETERR_LOC (rtx, HAWK_ENOTMAPNILIN, &right->loc);
	return HAWK_NULL;
}

static hawk_val_t* eval_binop_bor (
	hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint (rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint (rtx, right, &l2) <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval (rtx, l1 | l2);
}

static hawk_val_t* eval_binop_bxor (
	hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint (rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint (rtx, right, &l2) <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval (rtx, l1 ^ l2);
}

static hawk_val_t* eval_binop_band (
	hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint (rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint (rtx, right, &l2) <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval (rtx, l1 & l2);
}

/* -------------------------------------------------------------------- */

enum cmp_op_t
{
	CMP_OP_NONE = 0,
	CMP_OP_EQ = 1,
	CMP_OP_NE = 2,
	CMP_OP_GT = 3,
	CMP_OP_GE = 4,
	CMP_OP_LT = 5,
	CMP_OP_LE = 6
};
typedef enum cmp_op_t cmp_op_t;

static HAWK_INLINE cmp_op_t inverse_cmp_op (cmp_op_t op)
{
	static cmp_op_t inverse_cmp_op_tab[] = 
	{
		CMP_OP_NONE,
		CMP_OP_NE,
		CMP_OP_EQ,
		CMP_OP_LT,
		CMP_OP_LE,
		CMP_OP_GT,
		CMP_OP_GE
	};
	return inverse_cmp_op_tab[op];
}

static HAWK_INLINE int __cmp_ensure_not_equal (hawk_rtx_t* rtx, cmp_op_t op_hint)
{
	/* checking equality is mostly obvious. however, it is not possible
	 * to test if one is less/greater than the other for some operands.
	 * this function return a number that ensures to make NE to true and 
	 * all other operations false */

	switch (op_hint)
	{
		case CMP_OP_EQ:
		case CMP_OP_NE:
			return 1;  /* not equal */

		case CMP_OP_GT:
		case CMP_OP_LT:
			return 0; /* make GT or LT to be false by claiming equal */

		case CMP_OP_GE:
			return -1; /* make GE false by claiming less */
		case CMP_OP_LE: 
			return 1;  /* make LE false by claiming greater */

		default:
			SETERR_COD (rtx, HAWK_EOPERAND);
			return -1;
	}
}

/* -------------------------------------------------------------------- */
static HAWK_INLINE int __cmp_nil_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return 0;
}

static HAWK_INLINE int __cmp_nil_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_int_t v = HAWK_RTX_GETINTFROMVAL (rtx, right);
	return (v < 0)? 1: ((v > 0)? -1: 0);
}

static HAWK_INLINE int __cmp_nil_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	if (((hawk_val_flt_t*)right)->val < 0) return 1;
	if (((hawk_val_flt_t*)right)->val > 0) return -1;
	return 0;
}

static HAWK_INLINE int __cmp_nil_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (((hawk_val_str_t*)right)->val.len == 0)? 0: -1;
}

static HAWK_INLINE int __cmp_nil_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (((hawk_val_mbs_t*)right)->val.len == 0)? 0: -1;
}

static HAWK_INLINE int __cmp_nil_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* != -> true, all others -> false */
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_nil_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (HAWK_HTB_SIZE(((hawk_val_map_t*)right)->map) == 0)? 0: -1;
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_int_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_int_t v = HAWK_RTX_GETINTFROMVAL(rtx, left);
	return (v > 0)? 1: ((v < 0)? -1: 0);
}

static HAWK_INLINE int __cmp_int_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{

	hawk_int_t v1 = HAWK_RTX_GETINTFROMVAL(rtx, left);
	hawk_int_t v2 = HAWK_RTX_GETINTFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
}

static HAWK_INLINE int __cmp_int_flt (
	hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_int_t v1 = HAWK_RTX_GETINTFROMVAL (rtx, left);
	if (v1 > ((hawk_val_flt_t*)right)->val) return 1;
	if (v1 < ((hawk_val_flt_t*)right)->val) return -1;
	return 0;
}

static HAWK_INLINE int __cmp_int_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_ooch_t* str0;
	hawk_oow_t len0;
	int n;

	/* SCO CC doesn't seem to handle right->nstr > 0 properly */
	if ((hawk->opt.trait & HAWK_NCMPONSTR) || right->nstr /*> 0*/)
	{
		hawk_int_t ll, v1;
		hawk_flt_t rr;

		n = hawk_oochars_to_num(
			HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, (hawk->opt.trait & HAWK_STRIPSTRSPC), 0),
			((hawk_val_str_t*)right)->val.ptr,
			((hawk_val_str_t*)right)->val.len, 
			&ll, &rr
		);

		v1 = HAWK_RTX_GETINTFROMVAL(rtx, left);
		if (n == 0)
		{
			/* a numeric integral string */
			return (v1 > ll)? 1: ((v1 < ll)? -1: 0);
		}
		else if (n > 0)
		{
			/* a numeric floating-point string */
			return (v1 > rr)? 1: ((v1 < rr)? -1: 0);
		}
	}

	str0 = hawk_rtx_valtooocstrdup(rtx, left, &len0);
	if (!str0) return CMP_ERROR;

	n = hawk_comp_oochars(str0, len0, ((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freemem (rtx, str0);
	return n;
}

static HAWK_INLINE int __cmp_int_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_bch_t* str0;
	hawk_oow_t len0;
	int n;

	if ((hawk->opt.trait & HAWK_NCMPONSTR) || right->nstr /*> 0*/)
	{
		hawk_int_t ll, v1;
		hawk_flt_t rr;

		n = hawk_bchars_to_num (
			HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, (hawk->opt.trait & HAWK_STRIPSTRSPC), 0),
			((hawk_val_mbs_t*)right)->val.ptr,
			((hawk_val_mbs_t*)right)->val.len, 
			&ll, &rr
		);

		v1 = HAWK_RTX_GETINTFROMVAL(rtx, left);
		if (n == 0)
		{
			/* a numeric integral string */
			return (v1 > ll)? 1: ((v1 < ll)? -1: 0);
		}
		else if (n > 0)
		{
			/* a numeric floating-point string */
			return (v1 > rr)? 1: ((v1 < rr)? -1: 0);
		}
	}

	str0 = hawk_rtx_valtobcstrdup(rtx, left, &len0);
	if (!str0) return -1;

	n = hawk_comp_bchars(str0, len0, ((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freemem (rtx, str0);
	return n;
}

static HAWK_INLINE int __cmp_int_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_int_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* compare an integer and the size of a map */
	hawk_int_t v1 = HAWK_RTX_GETINTFROMVAL(rtx, left);
	hawk_int_t v2 = HAWK_HTB_SIZE(((hawk_val_map_t*)right)->map);
	if (v1 > v2) return 1;
	if (v1 < v2) return -1;
	return 0;
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_flt_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	if (((hawk_val_flt_t*)left)->val > 0) return 1;
	if (((hawk_val_flt_t*)left)->val < 0) return -1;
	return 0;
}

static HAWK_INLINE int __cmp_flt_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_int_t v2 = HAWK_RTX_GETINTFROMVAL (rtx, right);
	if (((hawk_val_flt_t*)left)->val > v2) return 1;
	if (((hawk_val_flt_t*)left)->val < v2) return -1;
	return 0;
}

static HAWK_INLINE int __cmp_flt_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	if (((hawk_val_flt_t*)left)->val > 
	    ((hawk_val_flt_t*)right)->val) return 1;
	if (((hawk_val_flt_t*)left)->val < 
	    ((hawk_val_flt_t*)right)->val) return -1;
	return 0;
}

static HAWK_INLINE int __cmp_flt_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_ooch_t* str0;
	hawk_oow_t len0;
	int n;

	/* SCO CC doesn't seem to handle right->nstr > 0 properly */
	if (hawk->opt.trait & HAWK_NCMPONSTR || right->nstr /*> 0*/)
	{
		const hawk_ooch_t* end;
		hawk_flt_t rr;

		rr = hawk_oochars_to_flt(((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, &end, (hawk->opt.trait & HAWK_STRIPSTRSPC));
		if (end == ((hawk_val_str_t*)right)->val.ptr + ((hawk_val_str_t*)right)->val.len)
		{
			return (((hawk_val_flt_t*)left)->val > rr)? 1:
			       (((hawk_val_flt_t*)left)->val < rr)? -1: 0;
		}
	}

	str0 = hawk_rtx_valtooocstrdup(rtx, left, &len0);
	if (!str0) return CMP_ERROR;

	n = hawk_comp_oochars(str0, len0, ((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freemem (rtx, str0);
	return n;
}

static HAWK_INLINE int __cmp_flt_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_bch_t* str0;
	hawk_oow_t len0;
	int n;

	if (hawk->opt.trait & HAWK_NCMPONSTR || right->nstr /*> 0*/)
	{
		const hawk_bch_t* end;
		hawk_flt_t rr;

		rr = hawk_bchars_to_flt(((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, &end, (hawk->opt.trait & HAWK_STRIPSTRSPC));
		if (end == ((hawk_val_mbs_t*)right)->val.ptr + ((hawk_val_mbs_t*)right)->val.len)
		{
			return (((hawk_val_flt_t*)left)->val > rr)? 1:
			       (((hawk_val_flt_t*)left)->val < rr)? -1: 0;
		}
	}

	str0 = hawk_rtx_valtobcstrdup(rtx, left, &len0);
	if (!str0) return CMP_ERROR;

	n = hawk_comp_bchars(str0, len0, ((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freemem (rtx, str0);
	return n;
}

static HAWK_INLINE int __cmp_flt_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_flt_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* compare a float with the size of a map */
	hawk_int_t v2 = HAWK_HTB_SIZE(((hawk_val_map_t*)right)->map);
	if (((hawk_val_flt_t*)left)->val > v2) return 1;
	if (((hawk_val_flt_t*)left)->val < v2) return -1;
	return 0;
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_str_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (((hawk_val_str_t*)left)->val.len == 0)? 0: 1;
}

static HAWK_INLINE int __cmp_str_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_int_str(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_str_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_flt_str(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_str_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_val_str_t* ls, * rs;
	

	ls = (hawk_val_str_t*)left;
	rs = (hawk_val_str_t*)right;

	if (ls->nstr == 0 || rs->nstr == 0)
	{
		/* both are definitely strings */
		return hawk_comp_oochars(ls->val.ptr, ls->val.len, rs->val.ptr, rs->val.len, rtx->gbl.ignorecase);
	}

	if (ls->nstr == 1)
	{
		hawk_int_t ll;

		ll = hawk_oochars_to_int(ls->val.ptr, ls->val.len, 0, HAWK_NULL, (hawk->opt.trait & HAWK_STRIPSTRSPC));

		if (rs->nstr == 1)
		{
			hawk_int_t rr;
			
			rr = hawk_oochars_to_int(rs->val.ptr, rs->val.len, 0, HAWK_NULL, (hawk->opt.trait & HAWK_STRIPSTRSPC));

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
		else 
		{
			hawk_flt_t rr;

			HAWK_ASSERT (hawk, rs->nstr == 2);

			rr = hawk_oochars_to_flt(rs->val.ptr, rs->val.len, HAWK_NULL, (hawk->opt.trait & HAWK_STRIPSTRSPC));

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
	}
	else
	{
		hawk_flt_t ll;

		HAWK_ASSERT (hawk, ls->nstr == 2);

		ll = hawk_oochars_to_flt(ls->val.ptr, ls->val.len, HAWK_NULL, (hawk->opt.trait & HAWK_STRIPSTRSPC));
		
		if (rs->nstr == 1)
		{
			hawk_int_t rr;
			
			rr = hawk_oochars_to_int(rs->val.ptr, rs->val.len, 0, HAWK_NULL, (hawk->opt.trait & HAWK_STRIPSTRSPC));

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
		else 
		{
			hawk_flt_t rr;

			HAWK_ASSERT (hawk, rs->nstr == 2);

			rr = hawk_oochars_to_flt(rs->val.ptr, rs->val.len, HAWK_NULL, (hawk->opt.trait & HAWK_STRIPSTRSPC));

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
	}
}

static HAWK_INLINE int __cmp_str_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_val_str_t* ls = (hawk_val_str_t*)left;
	hawk_val_mbs_t* rs = (hawk_val_mbs_t*)right;

#if (HAWK_SIZEOF_BCH_T != HAWK_SIZEOF_UINT8_T)
#	error Unsupported size of hawk_bch_t
#endif

#if defined(HAWK_OOCH_IS_BCH)
	return hawk_comp_bchars(ls->val.ptr, ls->val.len, rs->val.ptr, rs->val.len, rtx->gbl.ignorecase);
#else
	hawk_bch_t* mbsptr;
	hawk_oow_t mbslen;
	int n;

	mbsptr = hawk_rtx_duputobchars(rtx, ls->val.ptr, ls->val.len, &mbslen);
	if (!mbsptr) return CMP_ERROR;
	n = hawk_comp_bchars(mbsptr, mbslen, (const hawk_bch_t*)rs->val.ptr, rs->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freemem (rtx, mbsptr);
	return n;
#endif
}

static HAWK_INLINE int __cmp_str_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_str_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_mbs_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (((hawk_val_mbs_t*)left)->val.len == 0)? 0: 1;
}

static HAWK_INLINE int __cmp_mbs_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_int_mbs(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_mbs_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_flt_mbs(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_mbs_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_str_mbs(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_mbs_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_val_mbs_t* ls = (hawk_val_mbs_t*)left;
	hawk_val_mbs_t* rs = (hawk_val_mbs_t*)right;
#if (HAWK_SIZEOF_BCH_T != HAWK_SIZEOF_UINT8_T)
#	error Unsupported size of hawk_bch_t
#endif
	return hawk_comp_bchars(ls->val.ptr, ls->val.len, rs->val.ptr, rs->val.len, rtx->gbl.ignorecase);
}

static HAWK_INLINE int __cmp_mbs_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_mbs_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_fun_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (((hawk_val_fun_t*)left)->fun == ((hawk_val_fun_t*)right)->fun)? 0: __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_map_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_map(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_map_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_int_map(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_map_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_flt_map(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_map_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_map_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_map_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_map_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* can't compare a map with a map */
	SETERR_COD (rtx, HAWK_EOPERAND);
	return CMP_ERROR; 
}
/* -------------------------------------------------------------------- */


static HAWK_INLINE int __cmp_val (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint, int* ret)
{
	int n;
	hawk_val_type_t lvtype, rvtype;
	typedef int (*cmp_val_t) (hawk_rtx_t*, hawk_val_t*, hawk_val_t*, cmp_op_t op_hint);

	static cmp_val_t func[] =
	{
		/* this table must be synchronized with 
		 * the HAWK_VAL_XXX values in awk.h */
		__cmp_nil_nil, __cmp_nil_int, __cmp_nil_flt, __cmp_nil_str, __cmp_nil_mbs, __cmp_nil_fun, __cmp_nil_map,  
		__cmp_int_nil, __cmp_int_int, __cmp_int_flt, __cmp_int_str, __cmp_int_mbs, __cmp_int_fun, __cmp_int_map,
		__cmp_flt_nil, __cmp_flt_int, __cmp_flt_flt, __cmp_flt_str, __cmp_flt_mbs, __cmp_flt_fun, __cmp_flt_map,
		__cmp_str_nil, __cmp_str_int, __cmp_str_flt, __cmp_str_str, __cmp_str_mbs, __cmp_str_fun, __cmp_str_map,
		__cmp_mbs_nil, __cmp_mbs_int, __cmp_mbs_flt, __cmp_mbs_str, __cmp_mbs_mbs, __cmp_mbs_fun, __cmp_mbs_map,
		__cmp_fun_nil, __cmp_fun_int, __cmp_fun_flt, __cmp_fun_str, __cmp_fun_mbs, __cmp_fun_fun, __cmp_fun_map,
		__cmp_map_nil, __cmp_map_int, __cmp_map_flt, __cmp_map_str, __cmp_map_mbs, __cmp_map_fun, __cmp_map_map
	};

	lvtype = HAWK_RTX_GETVALTYPE(rtx, left);
	rvtype = HAWK_RTX_GETVALTYPE(rtx, right);
	if (!(rtx->awk->opt.trait & HAWK_FLEXMAP) && 
	    (lvtype == HAWK_VAL_MAP || rvtype == HAWK_VAL_MAP))
	{
		/* a map can't be compared againt other values */
		SETERR_COD (rtx, HAWK_EOPERAND);
		return -1;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), lvtype >= HAWK_VAL_NIL && lvtype <= HAWK_VAL_MAP);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), rvtype >= HAWK_VAL_NIL && rvtype <= HAWK_VAL_MAP);

	/* mapping fomula and table layout assume:
	 *  HAWK_VAL_NIL  = 0
	 *  HAWK_VAL_INT  = 1
	 *  HAWK_VAL_FLT  = 2
	 *  HAWK_VAL_STR  = 3
	 *  HAWK_VAL_MBS  = 4
	 *  HAWK_VAL_FUN  = 5
	 *  HAWK_VAL_MAP  = 6
	 * 
	 * op_hint indicate the operation in progress when this function is called.
	 * this hint doesn't require the comparison function to compare using this
	 * operation. the comparision function should return 0 if equal, -1 if less,
	 * 1 if greater, CMP_ERROR upon error regardless of this hint.
	 */
	n = func[lvtype * 7 + rvtype](rtx, left, right, op_hint);
	if (n == CMP_ERROR) return -1;

	*ret = n;
	return 0;
}

int hawk_rtx_cmpval (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, int* ret)
{
	return __cmp_val(rtx, left, right, CMP_OP_NONE, ret);
}

static int teq_val (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	hawk_val_type_t lt, rt;

	if (left == right) n = 1;
	else
	{
		lt = HAWK_RTX_GETVALTYPE(rtx, left);
		rt = HAWK_RTX_GETVALTYPE(rtx, right);

		if (lt != rt) n = 0;
		else
		{
			switch (lt)
			{
				case HAWK_VAL_NIL:
					n = 1; 
					break;

				case HAWK_VAL_INT:
					n = (HAWK_RTX_GETINTFROMVAL (rtx, left) == HAWK_RTX_GETINTFROMVAL (rtx, right));
					break;

				case HAWK_VAL_FLT:
					n = ((hawk_val_flt_t*)left)->val == ((hawk_val_flt_t*)right)->val;
					break;

				case HAWK_VAL_STR:
					n = hawk_comp_oochars (
						((hawk_val_str_t*)left)->val.ptr,
						((hawk_val_str_t*)left)->val.len,
						((hawk_val_str_t*)right)->val.ptr,
						((hawk_val_str_t*)right)->val.len,
						rtx->gbl.ignorecase) == 0;
					break;

				case HAWK_VAL_MBS:
					n = hawk_comp_bchars (
						((hawk_val_mbs_t*)left)->val.ptr,
						((hawk_val_mbs_t*)left)->val.len,
						((hawk_val_mbs_t*)right)->val.ptr,
						((hawk_val_mbs_t*)right)->val.len,
						rtx->gbl.ignorecase) == 0;
					break;

				case HAWK_VAL_FUN:
					n = ((hawk_val_fun_t*)left)->fun == ((hawk_val_fun_t*)right)->fun;
					break;

				default:
					/* map-x and map-y are always different regardless of
					 * their contents. however, if they are pointing to the
					 * same map value, it won't reach here but will be 
					 * handled by the first check in this function */
					n = 0;
					break;
			}
		}
	}

	return n;
}

static hawk_val_t* eval_binop_teq (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	return teq_val(rtx, left, right)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_tne (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	return teq_val(rtx, left, right)? HAWK_VAL_ZERO: HAWK_VAL_ONE;
}

static hawk_val_t* eval_binop_eq (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	if (__cmp_val(rtx, left, right, CMP_OP_EQ, &n) <= -1) return HAWK_NULL;
	return (n == 0)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_ne (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	if (__cmp_val(rtx, left, right, CMP_OP_NE, &n) <= -1) return HAWK_NULL;
	return (n != 0)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_gt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	if (__cmp_val(rtx, left, right, CMP_OP_GT, &n) <= -1) return HAWK_NULL;
	return (n > 0)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_ge (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	if (__cmp_val(rtx, left, right, CMP_OP_GE, &n) <= -1) return HAWK_NULL;
	return (n >= 0)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_lt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	if (__cmp_val(rtx, left, right, CMP_OP_LT, &n) <= -1) return HAWK_NULL;
	return (n < 0)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_le (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n;
	if (__cmp_val(rtx, left, right, CMP_OP_LE, &n) <= -1) return HAWK_NULL;
	return (n <= 0)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
}

static hawk_val_t* eval_binop_lshift (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint(rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint(rtx, right, &l2) <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval(rtx, l1 << l2);
}

static hawk_val_t* eval_binop_rshift (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint(rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint(rtx, right, &l2) <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval (rtx, l1 >> l2);
}

static hawk_val_t* eval_binop_plus (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2;

	n1 = hawk_rtx_valtonum(rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum(rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}
	/*
	n1  n2    n3
	0   0   = 0
	1   0   = 1
	0   1   = 2
	1   1   = 3
	*/
	n3 = n1 + (n2 << 1);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), n3 >= 0 && n3 <= 3);

	return (n3 == 0)? hawk_rtx_makeintval(rtx,(hawk_int_t)l1+(hawk_int_t)l2):
	       (n3 == 1)? hawk_rtx_makefltval(rtx,(hawk_flt_t)r1+(hawk_flt_t)l2):
	       (n3 == 2)? hawk_rtx_makefltval(rtx,(hawk_flt_t)l1+(hawk_flt_t)r2):
	                  hawk_rtx_makefltval(rtx,(hawk_flt_t)r1+(hawk_flt_t)r2);
}

static hawk_val_t* eval_binop_minus (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2;

	n1 = hawk_rtx_valtonum(rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum(rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), n3 >= 0 && n3 <= 3);
	return (n3 == 0)? hawk_rtx_makeintval(rtx,(hawk_int_t)l1-(hawk_int_t)l2):
	       (n3 == 1)? hawk_rtx_makefltval(rtx,(hawk_flt_t)r1-(hawk_flt_t)l2):
	       (n3 == 2)? hawk_rtx_makefltval(rtx,(hawk_flt_t)l1-(hawk_flt_t)r2):
	                  hawk_rtx_makefltval(rtx,(hawk_flt_t)r1-(hawk_flt_t)r2);
}

static hawk_val_t* eval_binop_mul (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2;

	n1 = hawk_rtx_valtonum (rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum (rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), n3 >= 0 && n3 <= 3);
	return (n3 == 0)? hawk_rtx_makeintval(rtx,(hawk_int_t)l1*(hawk_int_t)l2):
	       (n3 == 1)? hawk_rtx_makefltval(rtx,(hawk_flt_t)r1*(hawk_flt_t)l2):
	       (n3 == 2)? hawk_rtx_makefltval(rtx,(hawk_flt_t)l1*(hawk_flt_t)r2):
	                  hawk_rtx_makefltval(rtx,(hawk_flt_t)r1*(hawk_flt_t)r2);
}

static hawk_val_t* eval_binop_div (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2;
	hawk_val_t* res;

	n1 = hawk_rtx_valtonum (rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum (rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1) 
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			if  (l2 == 0) 
			{
				SETERR_COD (rtx, HAWK_EDIVBY0);
				return HAWK_NULL;
			}

			if (((hawk_int_t)l1 % (hawk_int_t)l2) == 0)
			{
				res = hawk_rtx_makeintval (
					rtx, (hawk_int_t)l1 / (hawk_int_t)l2);
			}
			else
			{
				res = hawk_rtx_makefltval (
					rtx, (hawk_flt_t)l1 / (hawk_flt_t)l2);
			}
			break;

		case 1:
			res = hawk_rtx_makefltval (
				rtx, (hawk_flt_t)r1 / (hawk_flt_t)l2);
			break;

		case 2:
			res = hawk_rtx_makefltval (
				rtx, (hawk_flt_t)l1 / (hawk_flt_t)r2);
			break;

		case 3:
			res = hawk_rtx_makefltval (
				rtx, (hawk_flt_t)r1 / (hawk_flt_t)r2);
			break;
	}

	return res;
}

static hawk_val_t* eval_binop_idiv (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2, quo;
	hawk_val_t* res;

	n1 = hawk_rtx_valtonum (rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum (rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1) 
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			if (l2 == 0) 
			{
				SETERR_COD (rtx, HAWK_EDIVBY0);
				return HAWK_NULL;
			}
			res = hawk_rtx_makeintval (
				rtx, (hawk_int_t)l1 / (hawk_int_t)l2);
			break;

		case 1:
			quo = (hawk_flt_t)r1 / (hawk_flt_t)l2;
			res = hawk_rtx_makeintval (rtx, (hawk_int_t)quo);
			break;

		case 2:
			quo = (hawk_flt_t)l1 / (hawk_flt_t)r2;
			res = hawk_rtx_makeintval (rtx, (hawk_int_t)quo);
			break;

		case 3:
			quo = (hawk_flt_t)r1 / (hawk_flt_t)r2;
			res = hawk_rtx_makeintval (rtx, (hawk_int_t)quo);
			break;
	}

	return res;
}

static hawk_val_t* eval_binop_mod (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2;
	hawk_val_t* res;

	/* the mod function must be provided when the awk object is created to be able to calculate floating-pointer remainder */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->awk->prm.math.mod != HAWK_NULL);

	n1 = hawk_rtx_valtonum(rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum(rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1)
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			if  (l2 == 0) 
			{
				SETERR_COD (rtx, HAWK_EDIVBY0);
				return HAWK_NULL;
			}
			res = hawk_rtx_makeintval(rtx, (hawk_int_t)l1 % (hawk_int_t)l2);
			break;

		case 1:
			res = hawk_rtx_makefltval(rtx, rtx->awk->prm.math.mod(hawk_rtx_gethawk(rtx), (hawk_flt_t)r1, (hawk_flt_t)l2));
			break;

		case 2:
			res = hawk_rtx_makefltval(rtx, rtx->awk->prm.math.mod(hawk_rtx_gethawk(rtx), (hawk_flt_t)l1, (hawk_flt_t)r2));
			break;

		case 3:
			res = hawk_rtx_makefltval(rtx, rtx->awk->prm.math.mod(hawk_rtx_gethawk(rtx), (hawk_flt_t)r1, (hawk_flt_t)r2));
			break;
	}

	return res;
}

static hawk_val_t* eval_binop_exp (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	int n1, n2, n3;
	hawk_int_t l1, l2;
	hawk_flt_t r1, r2;
	hawk_val_t* res;

	n1 = hawk_rtx_valtonum (rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum (rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1) 
	{
		SETERR_COD (rtx, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			/* left - int, right - int */
			if (l2 >= 0)
			{
				hawk_int_t v = 1;
				while (l2-- > 0) v *= l1;
				res = hawk_rtx_makeintval (rtx, v);
			}
			else if (l1 == 0)
			{
				SETERR_COD (rtx, HAWK_EDIVBY0);
				return HAWK_NULL;
			}
			else
			{
				hawk_flt_t v = 1.0;
				l2 *= -1;
				while (l2-- > 0) v /= l1;
				res = hawk_rtx_makefltval (rtx, v);
			}
			break;

		case 1:
			/* left - real, right - int */
			if (l2 >= 0)
			{
				hawk_flt_t v = 1.0;
				while (l2-- > 0) v *= r1;
				res = hawk_rtx_makefltval (rtx, v);
			}
			else if (r1 == 0.0)
			{
				SETERR_COD (rtx, HAWK_EDIVBY0);
				return HAWK_NULL;
			}
			else
			{
				hawk_flt_t v = 1.0;
				l2 *= -1;
				while (l2-- > 0) v /= r1;
				res = hawk_rtx_makefltval (rtx, v);
			}
			break;

		case 2:
			/* left - int, right - real */
			res = hawk_rtx_makefltval (
				rtx, 
				rtx->awk->prm.math.pow(hawk_rtx_gethawk(rtx), (hawk_flt_t)l1, (hawk_flt_t)r2)
			);
			break;

		case 3:
			/* left - real, right - real */
			res = hawk_rtx_makefltval (
				rtx,
				rtx->awk->prm.math.pow(hawk_rtx_gethawk(rtx), (hawk_flt_t)r1,(hawk_flt_t)r2)
			);
			break;
	}

	return res;
}

static hawk_val_t* eval_binop_concat (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_val_t* res;
	hawk_rtx_valtostr_out_t lout, rout;

	lout.type = HAWK_RTX_VALTOSTR_CPLDUP;
	if (hawk_rtx_valtostr(rtx, left, &lout) <= -1) return HAWK_NULL;

	rout.type = HAWK_RTX_VALTOSTR_CPLDUP;
	if (hawk_rtx_valtostr(rtx, right, &rout) <= -1)
	{
		hawk_rtx_freemem (rtx, lout.u.cpldup.ptr);
		return HAWK_NULL;
	}

	res = hawk_rtx_makestrvalwithoochars2(
		rtx, 
		lout.u.cpldup.ptr, lout.u.cpldup.len,
		rout.u.cpldup.ptr, rout.u.cpldup.len
	);

	hawk_rtx_freemem (rtx, rout.u.cpldup.ptr);
	hawk_rtx_freemem (rtx, lout.u.cpldup.ptr);

	return res;
}

static hawk_val_t* eval_binop_match0 (
	hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right,
	const hawk_loc_t* lloc, const hawk_loc_t* rloc, int ret)
{
	hawk_val_t* res;
	hawk_oocs_t out;
	int n;

	out.ptr = hawk_rtx_getvaloocstr (rtx, left, &out.len);
	if (out.ptr == HAWK_NULL) return HAWK_NULL;

	n = hawk_rtx_matchval(rtx, right, &out, &out, HAWK_NULL, HAWK_NULL);
	hawk_rtx_freevaloocstr (rtx, left, out.ptr);

	if (n <= -1) 
	{
		ADJERR_LOC (rtx, lloc);
		return HAWK_NULL;
	}

	res = hawk_rtx_makeintval (rtx, (n == ret));
	if (res == HAWK_NULL) 
	{
		ADJERR_LOC (rtx, lloc);
		return HAWK_NULL;
	}

	return res;
}

static hawk_val_t* eval_binop_ma (hawk_rtx_t* run, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), left->next == HAWK_NULL);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), right->next == HAWK_NULL);

	lv = eval_expression (run, left);
	if (lv == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (run, lv);

	rv = eval_expression0 (run, right);
	if (rv == HAWK_NULL)
	{
		hawk_rtx_refdownval (run, lv);
		return HAWK_NULL;
	}

	hawk_rtx_refupval (run, rv);

	res = eval_binop_match0 (run, lv, rv, &left->loc, &right->loc, 1);

	hawk_rtx_refdownval (run, rv);
	hawk_rtx_refdownval (run, lv);

	return res;
}

static hawk_val_t* eval_binop_nm (hawk_rtx_t* run, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), left->next == HAWK_NULL);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), right->next == HAWK_NULL);

	lv = eval_expression (run, left);
	if (lv == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (run, lv);

	rv = eval_expression0 (run, right);
	if (rv == HAWK_NULL)
	{
		hawk_rtx_refdownval (run, lv);
		return HAWK_NULL;
	}

	hawk_rtx_refupval (run, rv);

	res = eval_binop_match0 (run, lv, rv, &left->loc, &right->loc, 0);

	hawk_rtx_refdownval (run, rv);
	hawk_rtx_refdownval (run, lv);

	return res;
}

static hawk_val_t* eval_unary (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* left, * res = HAWK_NULL;
	hawk_nde_exp_t* exp = (hawk_nde_exp_t*)nde;
	int n;
	hawk_int_t l;
	hawk_flt_t r;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		exp->type == HAWK_NDE_EXP_UNR);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		exp->left != HAWK_NULL && exp->right == HAWK_NULL);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		exp->opcode == HAWK_UNROP_PLUS ||
		exp->opcode == HAWK_UNROP_MINUS ||
		exp->opcode == HAWK_UNROP_LNOT ||
		exp->opcode == HAWK_UNROP_BNOT);

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->left->next == HAWK_NULL);
	left = eval_expression (rtx, exp->left);
	if (left == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (rtx, left);

	switch (exp->opcode)
	{
		case HAWK_UNROP_MINUS:
			n = hawk_rtx_valtonum (rtx, left, &l, &r);
			if (n <= -1) goto exit_func;

			res = (n == 0)? hawk_rtx_makeintval (rtx, -l):
			                hawk_rtx_makefltval (rtx, -r);
			break;

		case HAWK_UNROP_LNOT:
			if (HAWK_RTX_GETVALTYPE (rtx, left) == HAWK_VAL_STR)
			{
				/* 0 if the string length is greater than 0.
				 * 1 if it's empty */
				res = hawk_rtx_makeintval (
					rtx, !(((hawk_val_str_t*)left)->val.len > 0));
			}
			else
			{
				n = hawk_rtx_valtonum (rtx, left, &l, &r);
				if (n <= -1) goto exit_func;

				res = (n == 0)? hawk_rtx_makeintval (rtx, !l):
				                hawk_rtx_makefltval (rtx, !r);
			}
			break;
	
		case HAWK_UNROP_BNOT:
			n = hawk_rtx_valtoint (rtx, left, &l);
			if (n <= -1) goto exit_func;

			res = hawk_rtx_makeintval (rtx, ~l);
			break;

		case HAWK_UNROP_PLUS:
			n = hawk_rtx_valtonum (rtx, left, &l, &r);
			if (n <= -1) goto exit_func;

			res = (n == 0)? hawk_rtx_makeintval (rtx, l):
			                hawk_rtx_makefltval (rtx, r);
			break;
	}

exit_func:
	hawk_rtx_refdownval (rtx, left);
	if (res == HAWK_NULL) ADJERR_LOC (rtx, &nde->loc);
	return res;
}

static hawk_val_t* eval_incpre (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* left, * res;
	hawk_val_type_t left_vtype;
	hawk_int_t inc_val_int;
	hawk_flt_t inc_val_flt;
	hawk_nde_exp_t* exp = (hawk_nde_exp_t*)nde;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->type == HAWK_NDE_EXP_INCPRE);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->left != HAWK_NULL && exp->right == HAWK_NULL);

	/* this way of checking if the l-value is assignable is
	 * ugly as it is dependent on the node types defined in hawk.h
	 * but let's keep going this way for the time being. */
	if (exp->left->type < HAWK_NDE_NAMED ||
	    /*exp->left->type > HAWK_NDE_ARGIDX) XXX */
	    exp->left->type > HAWK_NDE_POS)
	{
		SETERR_LOC (rtx, HAWK_EOPERAND, &nde->loc);
		return HAWK_NULL;
	}

	if (exp->opcode == HAWK_INCOP_PLUS) 
	{
		inc_val_int = 1;
		inc_val_flt = 1.0;
	}
	else if (exp->opcode == HAWK_INCOP_MINUS)
	{
		inc_val_int = -1;
		inc_val_flt = -1.0;
	}
	else
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - invalid opcode");
		SETERR_LOC (rtx, HAWK_EINTERN, &nde->loc);
		return HAWK_NULL;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->left->next == HAWK_NULL);
	left = eval_expression (rtx, exp->left);
	if (left == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (rtx, left);
	left_vtype = HAWK_RTX_GETVALTYPE (rtx, left);

	switch (left_vtype)
	{
		case HAWK_VAL_INT:
		{
			hawk_int_t r = HAWK_RTX_GETINTFROMVAL (rtx, left);
			res = hawk_rtx_makeintval (rtx, r + inc_val_int);
			if (res == HAWK_NULL) 
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}
			break;
		}

		case HAWK_VAL_FLT:
		{
			hawk_flt_t r = ((hawk_val_flt_t*)left)->val;
			res = hawk_rtx_makefltval (rtx, r + inc_val_flt);
			if (res == HAWK_NULL) 
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}
			break;
		}

		default:
		{
			hawk_int_t v1;
			hawk_flt_t v2;
			int n;

			n = hawk_rtx_valtonum (rtx, left, &v1, &v2);
			if (n <= -1)
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			if (n == 0) 
			{
				res = hawk_rtx_makeintval (rtx, v1 + inc_val_int);
			}
			else /* if (n == 1) */
			{
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), n == 1);
				res = hawk_rtx_makefltval (rtx, v2 + inc_val_flt);
			}

			if (res == HAWK_NULL) 
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			break;
		}
	}

	if (do_assignment (rtx, exp->left, res) == HAWK_NULL)
	{
		hawk_rtx_refdownval (rtx, left);
		return HAWK_NULL;
	}

	hawk_rtx_refdownval (rtx, left);
	return res;
}

static hawk_val_t* eval_incpst (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* left, * res, * res2;
	hawk_val_type_t left_vtype;
	hawk_int_t inc_val_int;
	hawk_flt_t inc_val_flt;
	hawk_nde_exp_t* exp = (hawk_nde_exp_t*)nde;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->type == HAWK_NDE_EXP_INCPST);
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->left != HAWK_NULL && exp->right == HAWK_NULL);

	/* this way of checking if the l-value is assignable is
	 * ugly as it is dependent on the node types defined in hawk.h.
	 * but let's keep going this way for the time being. */
	if (exp->left->type < HAWK_NDE_NAMED ||
	    /*exp->left->type > HAWK_NDE_ARGIDX) XXX */
	    exp->left->type > HAWK_NDE_POS)
	{
		SETERR_LOC (rtx, HAWK_EOPERAND, &nde->loc);
		return HAWK_NULL;
	}

	if (exp->opcode == HAWK_INCOP_PLUS) 
	{
		inc_val_int = 1;
		inc_val_flt = 1.0;
	}
	else if (exp->opcode == HAWK_INCOP_MINUS)
	{
		inc_val_int = -1;
		inc_val_flt = -1.0;
	}
	else
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), !"should never happen - invalid opcode");
		SETERR_LOC (rtx, HAWK_EINTERN, &nde->loc);
		return HAWK_NULL;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), exp->left->next == HAWK_NULL);
	left = eval_expression (rtx, exp->left);
	if (left == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (rtx, left);

	left_vtype = HAWK_RTX_GETVALTYPE (rtx, left);

	switch (left_vtype)
	{
		case HAWK_VAL_INT:
		{
			hawk_int_t r = HAWK_RTX_GETINTFROMVAL (rtx, left);
			res = hawk_rtx_makeintval (rtx, r);
			if (res == HAWK_NULL) 
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			res2 = hawk_rtx_makeintval (rtx, r + inc_val_int);
			if (res2 == HAWK_NULL)
			{
				hawk_rtx_refdownval (rtx, left);
				hawk_rtx_freeval (rtx, res, 1);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			break;
		}

		case HAWK_VAL_FLT:
		{
			hawk_flt_t r = ((hawk_val_flt_t*)left)->val;
			res = hawk_rtx_makefltval (rtx, r);
			if (res == HAWK_NULL) 
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			res2 = hawk_rtx_makefltval (rtx, r + inc_val_flt);
			if (res2 == HAWK_NULL)
			{
				hawk_rtx_refdownval (rtx, left);
				hawk_rtx_freeval (rtx, res, 1);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			break;
		}

		default:
		{
			hawk_int_t v1;
			hawk_flt_t v2;
			int n;

			n = hawk_rtx_valtonum (rtx, left, &v1, &v2);
			if (n <= -1)
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			if (n == 0) 
			{
				res = hawk_rtx_makeintval (rtx, v1);
				if (res == HAWK_NULL)
				{
					hawk_rtx_refdownval (rtx, left);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}

				res2 = hawk_rtx_makeintval (rtx, v1 + inc_val_int);
				if (res2 == HAWK_NULL)
				{
					hawk_rtx_refdownval (rtx, left);
					hawk_rtx_freeval (rtx, res, 1);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}
			}
			else /* if (n == 1) */
			{
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), n == 1);
				res = hawk_rtx_makefltval (rtx, v2);
				if (res == HAWK_NULL)
				{
					hawk_rtx_refdownval (rtx, left);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}

				res2 = hawk_rtx_makefltval (rtx, v2 + inc_val_flt);
				if (res2 == HAWK_NULL)
				{
					hawk_rtx_refdownval (rtx, left);
					hawk_rtx_freeval (rtx, res, 1);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}
			}

			break;
		}
	}

	if (do_assignment (rtx, exp->left, res2) == HAWK_NULL)
	{
		hawk_rtx_refdownval (rtx, left);
		return HAWK_NULL;
	}

	hawk_rtx_refdownval (rtx, left);
	return res;
}

static hawk_val_t* eval_cnd (hawk_rtx_t* run, hawk_nde_t* nde)
{
	hawk_val_t* tv, * v;
	hawk_nde_cnd_t* cnd = (hawk_nde_cnd_t*)nde;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), cnd->test->next == HAWK_NULL);

	tv = eval_expression (run, cnd->test);
	if (tv == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (run, tv);

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), cnd->left->next == HAWK_NULL && cnd->right->next == HAWK_NULL);
	v = (hawk_rtx_valtobool(run, tv))? eval_expression(run, cnd->left): eval_expression(run, cnd->right);

	hawk_rtx_refdownval (run, tv);
	return v;
}

static hawk_val_t* eval_fncall_fnc (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	/* intrinsic function */
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	/* the parser must make sure that the number of arguments is proper */ 
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), call->nargs >= call->u.fnc.spec.arg.min && call->nargs <= call->u.fnc.spec.arg.max); 
	return __eval_call(rtx, nde, HAWK_NULL, push_arg_from_nde, (void*)call->u.fnc.spec.arg.spec, HAWK_NULL, HAWK_NULL);
}

static HAWK_INLINE hawk_val_t* eval_fncall_fun_ex (hawk_rtx_t* rtx, hawk_nde_t* nde, void(*errhandler)(void*), void* eharg)
{
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	hawk_fun_t* fun;
	hawk_htb_pair_t* pair;

	if (!call->u.fun.fun)
	{
		/* there can be multiple runtime instances for a single awk object.
		 * changing the parse tree from one runtime instance can affect
		 * other instances. however i do change the parse tree without protection
		 * hoping that the pointer assignment is atomic. (call->u.fun.fun = fun).
		 * i don't mind each instance performs a search duplicately for a short while */

		pair = hawk_htb_search(rtx->awk->tree.funs, call->u.fun.name.ptr, call->u.fun.name.len);
		if (!pair) 
		{
			SETERR_ARGX_LOC (rtx, HAWK_EFUNNF, &call->u.fun.name, &nde->loc);
			return HAWK_NULL;
		}

		fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), fun != HAWK_NULL);

		/* cache the search result */
		call->u.fun.fun = fun;
	}
	else
	{
		/* use the cached function */
		fun = call->u.fun.fun;
	}

	if (call->nargs > fun->nargs)
	{
		/* TODO: is this correct? what if i want to 
		 *       allow arbitarary numbers of arguments? */
		SETERR_LOC (rtx, HAWK_EARGTM, &nde->loc);
		return HAWK_NULL;
	}

	return __eval_call(rtx, nde, fun, push_arg_from_nde, HAWK_NULL, errhandler, eharg);
}

static hawk_val_t* eval_fncall_fun (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_fncall_fun_ex(rtx, nde, HAWK_NULL, HAWK_NULL);
}

static hawk_val_t* eval_fncall_var (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	hawk_val_t* fv, * rv;

	fv = eval_expression(rtx, (hawk_nde_t*)call->u.var.var);
	if (!fv) return HAWK_NULL;

	hawk_rtx_refupval (rtx, fv);
	if (HAWK_RTX_GETVALTYPE(rtx, fv) != HAWK_VAL_FUN)
	{
		SETERR_ARGX_LOC (rtx, HAWK_ENOTFUN, &call->u.var.var->id.name, &nde->loc);
		rv = HAWK_NULL;
	}
	else
	{
		hawk_fun_t* fun = ((hawk_val_fun_t*)fv)->fun;
		rv = __eval_call(rtx, nde, fun, push_arg_from_nde, HAWK_NULL, HAWK_NULL, HAWK_NULL);
	}
	hawk_rtx_refdownval (rtx, fv);

	return rv;
}

/* run->stack_base has not been set for this  
 * stack frame. so the RTX_STACK_ARG macro cannot be used as in 
 * hawk_rtx_refdownval (run, RTX_STACK_ARG(run,nargs));*/ 
#define UNWIND_RTX_STACK_ARG(rtx,nargs) \
	do { \
		while ((nargs) > 0) \
		{ \
			--(nargs); \
			hawk_rtx_refdownval ((rtx), (rtx)->stack[(rtx)->stack_top-1]); \
			__raw_pop (rtx); \
		} \
	} while (0)

#define UNWIND_RTX_STACK_BASE(rtx) \
	do { \
		__raw_pop (rtx); /* nargs */ \
		__raw_pop (rtx); /* return */ \
		__raw_pop (rtx); /* prev stack top */ \
		__raw_pop (rtx); /* prev stack back */  \
	} while (0)

#define UNWIND_RTX_STACK(rtx,nargs) \
	do { \
		UNWIND_RTX_STACK_ARG (rtx,nargs); \
		UNWIND_RTX_STACK_BASE (rtx); \
	} while (0)

static hawk_val_t* __eval_call (
	hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_fun_t* fun, 
	hawk_oow_t(*argpusher)(hawk_rtx_t*,hawk_nde_fncall_t*,void*), void* apdata,
	void(*errhandler)(void*), void* eharg)
{
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	hawk_oow_t saved_stack_top;
	hawk_oow_t nargs, i;
	hawk_val_t* v;
	int n;

	/* 
	 * ---------------------
	 *  lcln               <- stack top
	 * ---------------------
	 *  ....
	 * ---------------------
	 *  lcl0               local variables are pushed by run_block
	 * =====================
	 *  argn                     
	 * ---------------------
	 *  ....
	 * ---------------------
	 *  arg1
	 * ---------------------
	 *  arg0 
	 * ---------------------
	 *  nargs 
	 * ---------------------
	 *  return value
	 * ---------------------
	 *  previous stack top
	 * ---------------------
	 *  previous stack base  <- stack base
	 * =====================
	 *  0 (nargs)            <- stack top
	 * ---------------------
	 *  return value
	 * ---------------------
	 *  previous stack top
	 * ---------------------
	 *  previous stack base  <- stack base
	 * =====================
	 *  gbln
	 * ---------------------
	 *  ....
	 * ---------------------
	 *  gbl0
	 * ---------------------
	 */

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), HAWK_SIZEOF(void*) >= HAWK_SIZEOF(rtx->stack_top));
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), HAWK_SIZEOF(void*) >= HAWK_SIZEOF(rtx->stack_base));

	saved_stack_top = rtx->stack_top;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("setting up function stack frame top=%zd base=%zd\n"), (hawk_oow_t)rtx->stack_top, (hawk_oow_t)rtx->stack_base);
#endif

	if (__raw_push(rtx,(void*)rtx->stack_base) <= -1) 
	{
		SETERR_LOC (rtx, HAWK_ENOMEM, &nde->loc);
		return HAWK_NULL;
	}

	if (__raw_push(rtx,(void*)saved_stack_top) <= -1) 
	{
		__raw_pop (rtx);
		SETERR_LOC (rtx, HAWK_ENOMEM, &nde->loc);
		return HAWK_NULL;
	}

	/* secure space for a return value. */
	if (__raw_push(rtx,hawk_val_nil) <= -1)
	{
		__raw_pop (rtx);
		__raw_pop (rtx);
		SETERR_LOC (rtx, HAWK_ENOMEM, &nde->loc);
		return HAWK_NULL;
	}

	/* secure space for nargs */
	if (__raw_push(rtx,hawk_val_nil) <= -1)
	{
		__raw_pop (rtx);
		__raw_pop (rtx);
		__raw_pop (rtx);
		SETERR_LOC (rtx, HAWK_ENOMEM, &nde->loc);
		return HAWK_NULL;
	}

	/* push all arguments onto the stack */
	nargs = argpusher(rtx, call, apdata);
	if (nargs == (hawk_oow_t)-1)
	{
		UNWIND_RTX_STACK_BASE (rtx);
		return HAWK_NULL;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nargs == call->nargs);

	if (fun)
	{
		/* extra step for normal awk functions */

		if (fun->argspec && call->args) /* hawk_rtx_callfun() sets up a fake call structure with nargs > 0 but args == HAWK_NULL */
		{
			/* sanity check for pass-by-reference parameters of a normal awk function.
			 * it tests if each pass-by-reference argument is referenceable. */

			hawk_nde_t* p = call->args;
			for (i = 0; i < nargs; i++)
			{
				if (fun->argspec[i] == HAWK_T('r'))
				{
					hawk_val_t** ref;

					if (get_reference(rtx, p, &ref) <= -1)
					{
						UNWIND_RTX_STACK (rtx, nargs);
						return HAWK_NULL;
					}
				}
				p = p->next;
			}
		}

		while (nargs < fun->nargs)
		{
			/* push as many nils as the number of missing actual arguments */
			if (__raw_push(rtx, hawk_val_nil) <= -1)
			{
				UNWIND_RTX_STACK (rtx, nargs);
				SETERR_LOC (rtx, HAWK_ENOMEM, &nde->loc);
				return HAWK_NULL;
			}

			nargs++;
		}
	}

	rtx->stack_base = saved_stack_top;
	RTX_STACK_NARGS(rtx) = (void*)nargs;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("running function body\n"));
#endif

	if (fun)
	{
		/* normal awk function */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), fun->body->type == HAWK_NDE_BLK);
		n = run_block(rtx,(hawk_nde_blk_t*)fun->body);
	}
	else
	{
		n = 0;

		/* intrinsic function */
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), call->nargs >= call->u.fnc.spec.arg.min && call->nargs <= call->u.fnc.spec.arg.max);

		if (call->u.fnc.spec.impl)
		{
			hawk_rtx_seterrnum (rtx, HAWK_ENOERR, HAWK_NULL);

			n = call->u.fnc.spec.impl(rtx, &call->u.fnc.info);

			if (n <= -1)
			{
				if (hawk_rtx_geterrnum(rtx) == HAWK_ENOERR)
				{
					/* the handler has not set the error.
					 * fix it */ 
					SETERR_ARGX_LOC (
						rtx, HAWK_EFNCIMPL, 
						&call->u.fnc.info.name, &nde->loc
					);
				}
				else
				{
					ADJERR_LOC (rtx, &nde->loc);
				}

				/* correct the return code just in case */
				if (n < -1) n = -1;
			}
		}
	}

	/* refdown args in the rtx.stack */
	nargs = (hawk_oow_t)RTX_STACK_NARGS(rtx);
#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("block rtx complete nargs = %d\n"), (int)nargs); 
#endif

	if (fun && fun->argspec && call->args && call->nargs > 0)  /* hawk_rtx_callfun() sets up a fake call structure with nargs > 0 but args == HAWK_NULL */
	{
		/* set back the values for pass-by-reference parameters of normal functions.
		 * the intrinsic functions are not handled here but their implementation would
		 * call hawk_rtx_setrefval() */

		/* even if fun->argspec exists, call->nargs may still be 0. so i test if call->nargs > 0.
		 *   function x(a1, &a2) {}
		 *   BEGIN { x(); }
		 * all parameters are nils in this case. nargs and fun->nargs are 2 but call->nargs is 0.
		 */

		hawk_oow_t cur_stack_base = rtx->stack_base;
		hawk_oow_t prev_stack_base = (hawk_oow_t)rtx->stack[rtx->stack_base + 0];

		hawk_nde_t* p = call->args;
		for (i = 0; i < call->nargs; i++)
		{
			if (fun->argspec[i] == HAWK_T('r'))
			{
				hawk_val_t** ref;
				hawk_val_ref_t refv;

				/* if an argument passed is a local variable or a parameter to the previous caller,
				 * the argument node information stored is relative to the previous stack frame.
				 * i revert rtx->stack_base to the previous stack frame base before calling 
				 * get_reference() and restors it back to the current base. this tactic
				 * is very ugly because the assumptions for this is dependent on get_reference()
				 * implementation */
				rtx->stack_base = prev_stack_base; /* UGLY */
				get_reference (rtx, p, &ref); /* no failure check as it must succeed here for the check done above */
				rtx->stack_base = cur_stack_base; /* UGLY */

				HAWK_RTX_INIT_REF_VAL (&refv, p->type - HAWK_NDE_NAMED, ref, 9); /* initialize a fake reference variable. 9 chosen randomly */
				hawk_rtx_setrefval (rtx, &refv, RTX_STACK_ARG(rtx, i));
			}

			hawk_rtx_refdownval (rtx, RTX_STACK_ARG(rtx,i));
			p = p->next;
		}

		for (; i < nargs; i++)
		{
			hawk_rtx_refdownval (rtx, RTX_STACK_ARG(rtx,i));
		}
	}
	else
	{
		for (i = 0; i < nargs; i++)
		{
			hawk_rtx_refdownval (rtx, RTX_STACK_ARG(rtx,i));
		}
	}

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("got return value\n"));
#endif

	v = RTX_STACK_RETVAL(rtx);
	if (n == -1)
	{
		if (hawk_rtx_geterrnum(rtx) == HAWK_ENOERR && errhandler != HAWK_NULL) 
		{
			/* errhandler is passed only when __eval_call() is
			 * invoked from hawk_rtx_call(). Under this 
			 * circumstance, this stack frame is the first
			 * activated and the stack base is the first element
			 * after the global variables. so RTX_STACK_RETVAL(rtx)
			 * effectively becomes RTX_STACK_RETVAL_GBL(rtx).
			 * As __eval_call() returns HAWK_NULL on error and
			 * the reference count of RTX_STACK_RETVAL(rtx) should be
			 * decremented, it can't get the return value
			 * if it turns out to be terminated by exit().
			 * The return value could be destroyed by then.
			 * Unlikely, rtx_bpae_loop() just checks if rtx->errinf.num
			 * is HAWK_ENOERR and gets RTX_STACK_RETVAL_GBL(rtx)
			 * to determine if it is terminated by exit().
			 *
			 * The handler capture_retval_on_exit() 
			 * increments the reference of RTX_STACK_RETVAL(rtx)
			 * and stores the pointer into accompanying space.
			 * This way, the return value is preserved upon
			 * termination by exit() out to the caller.
			 */
			errhandler (eharg);
		}

		/* if the earlier operations failed and this function
		 * has to return a error, the return value is just
		 * destroyed and replaced by nil */
		hawk_rtx_refdownval (rtx, v);
		RTX_STACK_RETVAL(rtx) = hawk_val_nil;
	}
	else
	{
		/* this trick has been mentioned in rtx_return.
		 * adjust the reference count of the return value.
		 * the value must not be freed even if the reference count
		 * reached zero because its reference has been incremented 
		 * in rtx_return or directly by hawk_rtx_setretval()
		 * regardless of its reference count. */
		hawk_rtx_refdownval_nofree (rtx, v);
	}

	rtx->stack_top =  (hawk_oow_t)rtx->stack[rtx->stack_base + 1];
	rtx->stack_base = (hawk_oow_t)rtx->stack[rtx->stack_base + 0];

	if (rtx->exit_level == EXIT_FUNCTION) rtx->exit_level = EXIT_NONE;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("returning from function top=%zd, base=%zd\n"), (hawk_oow_t)rtx->stack_top, (hawk_oow_t)rtx->stack_base); 
#endif
	return (n == -1)? HAWK_NULL: v;
}

static hawk_oow_t push_arg_from_vals (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data)
{
	struct pafv_t* pafv = (struct pafv_t*)data;
	hawk_oow_t nargs = 0;

	for (nargs = 0; nargs < pafv->nargs; nargs++)
	{
		if (pafv->argspec && pafv->argspec[nargs] == HAWK_T('r'))
		{
			hawk_val_t** ref;
			hawk_val_t* v;

			ref = (hawk_val_t**)&pafv->args[nargs];
			v = hawk_rtx_makerefval(rtx, HAWK_VAL_REF_LCL, ref); /* this type(HAWK_VAL_REF_LCL) is fake */
			if (!v)
			{
				UNWIND_RTX_STACK_ARG (rtx, nargs);
				SETERR_LOC (rtx, HAWK_ENOMEM, &call->loc);
				return (hawk_oow_t)-1;
			}

			if (__raw_push(rtx, v)  <= -1)
			{
				hawk_rtx_refupval (rtx, v);
				hawk_rtx_refdownval (rtx, v);

				UNWIND_RTX_STACK_ARG (rtx, nargs);
				SETERR_LOC (rtx, HAWK_ENOMEM, &call->loc);
				return (hawk_oow_t)-1;
			}

			hawk_rtx_refupval (rtx, v);
		}
		else
		{
			if (__raw_push(rtx, pafv->args[nargs]) <= -1) 
			{
				/* ugly - arg needs to be freed if it doesn't have
				 * any reference. but its reference has not been 
				 * updated yet as it is carried out after successful
				 * stack push. so it adds up a reference and 
				 * dereferences it */
				hawk_rtx_refupval (rtx, pafv->args[nargs]);
				hawk_rtx_refdownval (rtx, pafv->args[nargs]);

				UNWIND_RTX_STACK_ARG (rtx, nargs);
				SETERR_LOC (rtx, HAWK_ENOMEM, &call->loc);
				return (hawk_oow_t)-1;
			}

			hawk_rtx_refupval (rtx, pafv->args[nargs]);
		}
	}

	return nargs; 
}

static hawk_oow_t push_arg_from_nde (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data)
{
	hawk_nde_t* p;
	hawk_val_t* v;
	hawk_oow_t nargs;
	const hawk_ooch_t* fnc_arg_spec = (const hawk_ooch_t*)data;

	for (p = call->args, nargs = 0; p != HAWK_NULL; p = p->next, nargs++)
	{
		/* if fnc_arg_spec is to be provided, it must contain as many characters as nargs */

		if (fnc_arg_spec && fnc_arg_spec[nargs] == HAWK_T('r'))
		{
			hawk_val_t** ref;

			if (get_reference(rtx, p, &ref) <= -1)
			{
				UNWIND_RTX_STACK_ARG (rtx, nargs);
				return (hawk_oow_t)-1;
			}

			/* 'p->type - HAWK_NDE_NAMED' must produce a relevant HAWK_VAL_REF_XXX value. */
			v = hawk_rtx_makerefval(rtx, p->type - HAWK_NDE_NAMED, ref);
		}
		else if (fnc_arg_spec && fnc_arg_spec[nargs] == HAWK_T('x'))
		{
			/* a regular expression is passed to 
			 * the function as it is */
			v = eval_expression0(rtx, p);
		}
		else
		{
			v = eval_expression(rtx, p);
		}

		if (!v)
		{
			UNWIND_RTX_STACK_ARG (rtx, nargs);
			return (hawk_oow_t)-1;
		}

		if (__raw_push(rtx,v) <= -1) 
		{
			/* ugly - v needs to be freed if it doesn't have
			 * any reference. but its reference has not been 
			 * updated yet as it is carried out after the 
			 * successful stack push. so it adds up a reference 
			 * and dereferences it */
			hawk_rtx_refupval (rtx, v);
			hawk_rtx_refdownval (rtx, v);

			UNWIND_RTX_STACK_ARG (rtx, nargs);
			SETERR_LOC (rtx, HAWK_ENOMEM, &call->loc);
			return (hawk_oow_t)-1;
		}

		hawk_rtx_refupval (rtx, v);
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), call->nargs == nargs);
	return nargs;
}

static int get_reference (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_val_t*** ref)
{
	hawk_nde_var_t* tgt = (hawk_nde_var_t*)nde;
	hawk_val_t** tmp;

	/* refer to eval_indexed for application of a similar concept */

	switch (nde->type)
	{
		case HAWK_NDE_NAMED:
		{
			hawk_htb_pair_t* pair;

			pair = hawk_htb_search(rtx->named, tgt->id.name.ptr, tgt->id.name.len);
			if (pair == HAWK_NULL)
			{
				/* it is bad that the named variable has to be created here.
				 * would there be any better ways to avoid this? */
				pair = hawk_htb_upsert(rtx->named, tgt->id.name.ptr, tgt->id.name.len, hawk_val_nil, 0);
				if (!pair) 
				{
					ADJERR_LOC (rtx, &nde->loc);
					return -1;
				}
			}

			*ref = (hawk_val_t**)&HAWK_HTB_VPTR(pair);
			return 0;
		}
		
		case HAWK_NDE_GBL:
			/* *ref = (hawk_val_t**)&RTX_STACK_GBL(rtx,tgt->id.idxa); */
			*ref = (hawk_val_t**)((hawk_oow_t)tgt->id.idxa);
			return 0;

		case HAWK_NDE_LCL:
			*ref = (hawk_val_t**)&RTX_STACK_LCL(rtx,tgt->id.idxa);
			return 0;

		case HAWK_NDE_ARG:
			*ref = (hawk_val_t**)&RTX_STACK_ARG(rtx,tgt->id.idxa);
			return 0;

		case HAWK_NDE_NAMEDIDX:
		{
			hawk_htb_pair_t* pair;

			pair = hawk_htb_search(rtx->named, tgt->id.name.ptr, tgt->id.name.len);
			if (pair == HAWK_NULL)
			{
				pair = hawk_htb_upsert(rtx->named, tgt->id.name.ptr, tgt->id.name.len, hawk_val_nil, 0);
				if (pair == HAWK_NULL) 
				{
					ADJERR_LOC (rtx, &nde->loc);
					return -1;
				}
			}

			tmp = get_reference_indexed(rtx, tgt, (hawk_val_t**)&HAWK_HTB_VPTR(pair));
			if (tmp == HAWK_NULL) return -1;
			*ref = tmp;
			return 0;
		}
		
		case HAWK_NDE_GBLIDX:
			tmp = get_reference_indexed(rtx, tgt, (hawk_val_t**)&RTX_STACK_GBL(rtx,tgt->id.idxa));
			if (tmp == HAWK_NULL) return -1;
			*ref = tmp;
			return 0;

		case HAWK_NDE_LCLIDX:
			tmp = get_reference_indexed(rtx, tgt, (hawk_val_t**)&RTX_STACK_LCL(rtx,tgt->id.idxa));
			if (tmp == HAWK_NULL) return -1;
			*ref = tmp;
			return 0;

		case HAWK_NDE_ARGIDX:
			tmp = get_reference_indexed(rtx, tgt, (hawk_val_t**)&RTX_STACK_ARG(rtx,tgt->id.idxa));
			if (tmp == HAWK_NULL) return -1;
			*ref = tmp;
			return 0;

		case HAWK_NDE_POS:
		{
			int n;
			hawk_int_t lv;
			hawk_val_t* v;
	
			/* the position number is returned for the positional 
			 * variable unlike other reference types. */
			v = eval_expression(rtx, ((hawk_nde_pos_t*)nde)->val);
			if (v == HAWK_NULL) return -1;

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &lv);
			hawk_rtx_refdownval (rtx, v);

			if (n <= -1) 
			{
				SETERR_LOC (rtx, HAWK_EPOSIDX, &nde->loc);
				return -1;
			}

			if (!IS_VALID_POSIDX(lv)) 
			{
				SETERR_LOC (rtx, HAWK_EPOSIDX, &nde->loc);
				return -1;
			}

			*ref = (hawk_val_t**)((hawk_oow_t)lv);
			return 0;
		}

		default:
			SETERR_LOC (rtx, HAWK_ENOTREF, &nde->loc);
			return -1;
	}
}

static hawk_val_t** get_reference_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* nde, hawk_val_t** val)
{
	hawk_htb_pair_t* pair;
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[IDXBUFSIZE];
	hawk_val_type_t vtype;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), val != HAWK_NULL);

	vtype = HAWK_RTX_GETVALTYPE(rtx, *val);
	if (vtype == HAWK_VAL_NIL)
	{
		hawk_val_t* tmp;

		tmp = hawk_rtx_makemapval (rtx);
		if (tmp == HAWK_NULL)
		{
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		hawk_rtx_refdownval (rtx, *val);
		*val = tmp;
		hawk_rtx_refupval (rtx, (hawk_val_t*)*val);
	}
	else if (vtype != HAWK_VAL_MAP) 
	{
		SETERR_LOC (rtx, HAWK_ENOTMAP, &nde->loc);
		return HAWK_NULL;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->idx != HAWK_NULL);

	len = HAWK_COUNTOF(idxbuf);
	str = idxnde_to_str (rtx, nde->idx, idxbuf, &len);
	if (str == HAWK_NULL) return HAWK_NULL;

	pair = hawk_htb_search((*(hawk_val_map_t**)val)->map, str, len);
	if (pair == HAWK_NULL)
	{
		pair = hawk_htb_upsert((*(hawk_val_map_t**)val)->map, str, len, hawk_val_nil, 0);
		if (pair == HAWK_NULL)
		{
			if (str != idxbuf) hawk_rtx_freemem (rtx, str);
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		hawk_rtx_refupval (rtx, HAWK_HTB_VPTR(pair));
	}

	if (str != idxbuf) hawk_rtx_freemem (rtx, str);
	return (hawk_val_t**)&HAWK_HTB_VPTR(pair);
}

static hawk_val_t* eval_int (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makeintval(rtx, ((hawk_nde_int_t*)nde)->val);
	if (val == HAWK_NULL) ADJERR_LOC (rtx, &nde->loc);
	else if (HAWK_VTR_IS_POINTER(val)) ((hawk_val_int_t*)val)->nde = nde;
	return val;
}

static hawk_val_t* eval_flt (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makefltval(rtx, ((hawk_nde_flt_t*)nde)->val);
	if (val == HAWK_NULL) ADJERR_LOC (rtx, &nde->loc);
	else ((hawk_val_flt_t*)val)->nde = nde;
	return val;
}

static hawk_val_t* eval_str (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makestrvalwithoochars(rtx, ((hawk_nde_str_t*)nde)->ptr, ((hawk_nde_str_t*)nde)->len);
	if (val == HAWK_NULL) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_mbs (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makembsval(rtx, ((hawk_nde_mbs_t*)nde)->ptr, ((hawk_nde_mbs_t*)nde)->len);
	if (val == HAWK_NULL) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_rex (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makerexval(rtx, &((hawk_nde_rex_t*)nde)->str, ((hawk_nde_rex_t*)nde)->code);
	if (val == HAWK_NULL) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_fun (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	hawk_fun_t* fun;
	

	fun = ((hawk_nde_fun_t*)nde)->funptr;
	if (!fun)
	{
		hawk_htb_pair_t* pair;

		/* TODO: support bultin functions?, support module functions? */
		pair = hawk_htb_search(rtx->awk->tree.funs, ((hawk_nde_fun_t*)nde)->name.ptr, ((hawk_nde_fun_t*)nde)->name.len);
		if (!pair)
		{
			SETERR_ARGX_LOC (rtx, HAWK_EFUNNF, &((hawk_nde_fun_t*)nde)->name, &nde->loc);
			return HAWK_NULL;
		}

		fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), fun != HAWK_NULL);
	}

	val = hawk_rtx_makefunval(rtx, fun); 
	if (!val) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_named (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_htb_pair_t* pair;
	pair = hawk_htb_search (rtx->named, ((hawk_nde_var_t*)nde)->id.name.ptr, ((hawk_nde_var_t*)nde)->id.name.len);
	return (pair == HAWK_NULL)? hawk_val_nil: HAWK_HTB_VPTR(pair);
}

static hawk_val_t* eval_gbl (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return RTX_STACK_GBL(rtx,((hawk_nde_var_t*)nde)->id.idxa);
}

static hawk_val_t* eval_lcl (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return RTX_STACK_LCL(rtx,((hawk_nde_var_t*)nde)->id.idxa);
}

static hawk_val_t* eval_arg (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return RTX_STACK_ARG(rtx,((hawk_nde_var_t*)nde)->id.idxa);
}

static hawk_val_t* eval_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* nde, hawk_val_t** val)
{
	hawk_htb_pair_t* pair;
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[IDXBUFSIZE];
	hawk_val_type_t vtype;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), val != HAWK_NULL);

	vtype = HAWK_RTX_GETVALTYPE(rtx, *val);
	if (vtype == HAWK_VAL_NIL)
	{
		hawk_val_t* tmp;

		tmp = hawk_rtx_makemapval(rtx);
		if (tmp == HAWK_NULL)
		{
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		hawk_rtx_refdownval (rtx, *val);
		*val = tmp;
		hawk_rtx_refupval (rtx, (hawk_val_t*)*val);
	}
	else if (vtype != HAWK_VAL_MAP) 
	{
		SETERR_LOC (rtx, HAWK_ENOTMAP, &nde->loc);
		return HAWK_NULL;
	}

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde->idx != HAWK_NULL);

	len = HAWK_COUNTOF(idxbuf);
	str = idxnde_to_str(rtx, nde->idx, idxbuf, &len);
	if (str == HAWK_NULL) return HAWK_NULL;

	pair = hawk_htb_search((*(hawk_val_map_t**)val)->map, str, len);
	if (str != idxbuf) hawk_rtx_freemem (rtx, str);

	return (pair == HAWK_NULL)? hawk_val_nil: (hawk_val_t*)HAWK_HTB_VPTR(pair);
}

static hawk_val_t* eval_namedidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_var_t* tgt = (hawk_nde_var_t*)nde;
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search(rtx->named, tgt->id.name.ptr, tgt->id.name.len);
	if (pair == HAWK_NULL)
	{
		pair = hawk_htb_upsert(rtx->named, tgt->id.name.ptr, tgt->id.name.len, hawk_val_nil, 0);
		if (pair == HAWK_NULL) 
		{
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		hawk_rtx_refupval (rtx, HAWK_HTB_VPTR(pair)); 
	}

	return eval_indexed (rtx, tgt, (hawk_val_t**)&HAWK_HTB_VPTR(pair));
}

static hawk_val_t* eval_gblidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde, (hawk_val_t**)&RTX_STACK_GBL(rtx,((hawk_nde_var_t*)nde)->id.idxa));
}

static hawk_val_t* eval_lclidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde, (hawk_val_t**)&RTX_STACK_LCL(rtx,((hawk_nde_var_t*)nde)->id.idxa));
}

static hawk_val_t* eval_argidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde, (hawk_val_t**)&RTX_STACK_ARG(rtx,((hawk_nde_var_t*)nde)->id.idxa));
}

static hawk_val_t* eval_pos (hawk_rtx_t* run, hawk_nde_t* nde)
{
	hawk_nde_pos_t* pos = (hawk_nde_pos_t*)nde;
	hawk_val_t* v;
	hawk_int_t lv;
	int n;

	v = eval_expression (run, pos->val);
	if (v == HAWK_NULL) return HAWK_NULL;

	hawk_rtx_refupval (run, v);
	n = hawk_rtx_valtoint (run, v, &lv);
	hawk_rtx_refdownval (run, v);
	if (n <= -1) 
	{
		SETERR_LOC (run, HAWK_EPOSIDX, &nde->loc);
		return HAWK_NULL;
	}

	if (lv < 0)
	{
		SETERR_LOC (run, HAWK_EPOSIDX, &nde->loc);
		return HAWK_NULL;
	}
	if (lv == 0) v = run->inrec.d0;
	else if (lv > 0 && lv <= (hawk_int_t)run->inrec.nflds) 
		v = run->inrec.flds[lv-1].val;
	else v = hawk_val_zls; /*hawk_val_nil;*/

	return v;
}

static hawk_val_t* eval_getline (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_getline_t* p;
	hawk_val_t* v, * tmp;
	hawk_ooch_t* dst;
	hawk_ooecs_t* buf;
	int n, x;
	hawk_val_type_t vtype;

	p = (hawk_nde_getline_t*)nde;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), 
		(p->in_type == HAWK_IN_PIPE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_RWPIPE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_FILE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_CONSOLE && p->in == HAWK_NULL));

	if (p->in)
	{
		hawk_oow_t len;
		hawk_rtx_valtostr_out_t out;

		v = eval_expression(rtx, p->in);
		if (!v) return HAWK_NULL;

		hawk_rtx_refupval (rtx, v);
		dst = hawk_rtx_getvaloocstr(rtx, v, &len);
		if (!dst) 
		{
			hawk_rtx_refdownval (rtx, v);
			return HAWK_NULL;
		}

		if (len <= 0)
		{
		
			hawk_rtx_freevaloocstr (rtx, v, dst);
			hawk_rtx_refdownval (rtx, v);

			n = -1;
			goto skip_read;
		}

		while (len > 0)
		{
			if (dst[--len] == HAWK_T('\0'))
			{
				/* the input source name contains a null 
				 * character. make getline return -1.
				 * unlike print & printf, it is not a hard
				 * error  */
				hawk_rtx_freevaloocstr (rtx, v, dst);
				hawk_rtx_refdownval (rtx, v);
				n = -1;
				goto skip_read;
			}
		}
	}
	else dst = (hawk_ooch_t*)HAWK_T("");

	buf = &rtx->inrec.lineg;
read_console_again:
	hawk_ooecs_clear (&rtx->inrec.lineg);

	n = hawk_rtx_readio(rtx, p->in_type, dst, buf);

	if (p->in) 
	{
		hawk_rtx_freevaloocstr (rtx, v, dst);
		hawk_rtx_refdownval (rtx, v);
	}

	if (n <= -1) 
	{
		/* make getline return -1 */
		n = -1;
	}
	else if (n > 0)
	{
		if (p->in_type == HAWK_IN_CONSOLE)
		{
			HAWK_ASSERT (hawk_rtx_gethawk(rtx), p->in == HAWK_NULL);
			if (rtx->nrflt.limit > 0)
			{
				/* record filter based on record number(NR) */
				if (((rtx->gbl.nr / rtx->nrflt.limit) % rtx->nrflt.size) != rtx->nrflt.rank)
				{
					if (update_fnr(rtx, rtx->gbl.fnr + 1, rtx->gbl.nr + 1) <= -1) return HAWK_NULL;
					/* this jump is a bit dirty. the 'if' block below hawk_rtx_readio()
					 * will never be true. but this makes code confusing */
					goto read_console_again; 
				}
			}
		}

		if (p->var == HAWK_NULL)
		{
			/* set $0 with the input value */
			x = hawk_rtx_setrec(rtx, 0, HAWK_OOECS_OOCS(buf));
			if (x <= -1) return HAWK_NULL;
		}
		else
		{
			hawk_val_t* v;

			v = hawk_rtx_makestrvalwithoocs(rtx, HAWK_OOECS_OOCS(buf));
			if (v == HAWK_NULL)
			{
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			hawk_rtx_refupval (rtx, v);
			tmp = do_assignment(rtx, p->var, v);
			hawk_rtx_refdownval (rtx, v);
			if (tmp == HAWK_NULL) return HAWK_NULL;
		}

		/* update FNR & NR if reading from console */
		if (p->in_type == HAWK_IN_CONSOLE &&
		    update_fnr(rtx, rtx->gbl.fnr + 1, rtx->gbl.nr + 1) <= -1) 
		{
			return HAWK_NULL;
		}
	}
	
skip_read:
	tmp = hawk_rtx_makeintval (rtx, n);
	if (!tmp) ADJERR_LOC (rtx, &nde->loc);
	return tmp;
}

static hawk_val_t* eval_print (hawk_rtx_t* run, hawk_nde_t* nde)
{
	int n = run_print(run, (hawk_nde_print_t*)nde);
	if (n == PRINT_IOERR) n = -1; /* let print return -1 */
	else if (n <= -1) return HAWK_NULL;
	return hawk_rtx_makeintval(run, n);
}

static hawk_val_t* eval_printf (hawk_rtx_t* run, hawk_nde_t* nde)
{
	int n = run_printf(run, (hawk_nde_print_t*)nde);
	if (n == PRINT_IOERR) n = -1; /* let print return -1 */
	else if (n <= -1) return HAWK_NULL;
	return hawk_rtx_makeintval(run, n);
}

static int __raw_push (hawk_rtx_t* rtx, void* val)
{
	if (rtx->stack_top >= rtx->stack_limit)
	{
		/*
		void** tmp;
		hawk_oow_t n;

		n = rtx->stack_limit + RTX_STACK_INCREMENT;

		tmp = (void**)hawk_rtx_reallocmem(rtx, rtx->stack, n * HAWK_SIZEOF(void*)); 
		if (!tmp) return -1;

		rtx->stack = tmp;
		rtx->stack_limit = n;
		*/
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_ESTACK, HAWK_T("runtime stack full"));
		return -1;
	}

	rtx->stack[rtx->stack_top++] = val;
	return 0;
}

static int read_record (hawk_rtx_t* rtx)
{
	hawk_ooi_t n;
	hawk_ooecs_t* buf;

read_again:
	if (hawk_rtx_clrrec (rtx, 0) == -1) return -1;

	buf = &rtx->inrec.line;
	n = hawk_rtx_readio (rtx, HAWK_IN_CONSOLE, HAWK_T(""), buf);
	if (n <= -1) 
	{
		hawk_rtx_clrrec (rtx, 0);
		return -1;
	}

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("record len = %d str=[%.*js]\n"), (int)HAWK_OOECS_LEN(buf), HAWK_OOECS_LEN(buf), HAWK_OOECS_PTR(buf));
#endif
	if (n == 0) 
	{
		HAWK_ASSERT (hawk_rtx_gethawk(rtx), HAWK_OOECS_LEN(buf) == 0);
		return 0;
	}

	if (rtx->nrflt.limit > 0)
	{
		if (((rtx->gbl.nr / rtx->nrflt.limit) % rtx->nrflt.size) != rtx->nrflt.rank)
		{
			if (update_fnr (rtx, rtx->gbl.fnr + 1, rtx->gbl.nr + 1) <= -1) return -1;
			goto read_again;
		}
	}

	if (hawk_rtx_setrec(rtx, 0, HAWK_OOECS_OOCS(buf)) <= -1 ||
	    update_fnr(rtx, rtx->gbl.fnr + 1, rtx->gbl.nr + 1) <= -1) return -1;

	return 1;
}

static int shorten_record (hawk_rtx_t* rtx, hawk_oow_t nflds)
{
	hawk_val_t* v;
	hawk_ooch_t* ofs_free = HAWK_NULL, * ofs_ptr;
	hawk_oow_t ofs_len, i;
	hawk_ooecs_t tmp;
	hawk_val_type_t vtype;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nflds <= rtx->inrec.nflds);

	if (nflds > 1)
	{
		v = RTX_STACK_GBL(rtx, HAWK_GBL_OFS);
		hawk_rtx_refupval (rtx, v);
		vtype = HAWK_RTX_GETVALTYPE(rtx, v);

		if (vtype == HAWK_VAL_NIL)
		{
			/* OFS not set */
			ofs_ptr = HAWK_T(" ");
			ofs_len = 1;
		}
		else if (vtype == HAWK_VAL_STR)
		{
			ofs_ptr = ((hawk_val_str_t*)v)->val.ptr;
			ofs_len = ((hawk_val_str_t*)v)->val.len;
		}
		else
		{
			hawk_rtx_valtostr_out_t out;

			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr (rtx, v, &out) <= -1) return -1;

			ofs_ptr = out.u.cpldup.ptr;
			ofs_len = out.u.cpldup.len;
			ofs_free = ofs_ptr;
		}
	}

	if (hawk_ooecs_init(&tmp, hawk_rtx_getgem(rtx), HAWK_OOECS_LEN(&rtx->inrec.line)) <= -1)
	{
		if (ofs_free) hawk_rtx_freemem (rtx, ofs_free);
		if (nflds > 1) hawk_rtx_refdownval (rtx, v);
		return -1;
	}

	for (i = 0; i < nflds; i++)
	{
		if (i > 0 && hawk_ooecs_ncat(&tmp,ofs_ptr,ofs_len) == (hawk_oow_t)-1)
		{
			hawk_ooecs_fini (&tmp);
			if (ofs_free) hawk_rtx_freemem (rtx, ofs_free);
			if (nflds > 1) hawk_rtx_refdownval (rtx, v);
			return -1;
		}

		if (hawk_ooecs_ncat(&tmp, rtx->inrec.flds[i].ptr, rtx->inrec.flds[i].len) == (hawk_oow_t)-1)
		{
			hawk_ooecs_fini (&tmp);
			if (ofs_free) hawk_rtx_freemem (rtx, ofs_free);
			if (nflds > 1) hawk_rtx_refdownval (rtx, v);
			return -1;
		}
	}

	if (ofs_free) hawk_rtx_freemem (rtx, ofs_free);
	if (nflds > 1) hawk_rtx_refdownval (rtx, v);

	v = (hawk_val_t*)hawk_rtx_makestrvalwithoocs(rtx, HAWK_OOECS_OOCS(&tmp));
	if (!v) 
	{
		hawk_ooecs_fini (&tmp);
		return -1;
	}

	hawk_rtx_refdownval (rtx, rtx->inrec.d0);
	rtx->inrec.d0 = v;
	hawk_rtx_refupval (rtx, rtx->inrec.d0);

	hawk_ooecs_swap (&tmp, &rtx->inrec.line);
	hawk_ooecs_fini (&tmp);

	for (i = nflds; i < rtx->inrec.nflds; i++)
	{
		hawk_rtx_refdownval (rtx, rtx->inrec.flds[i].val);
	}

	rtx->inrec.nflds = nflds;
	return 0;
}

static hawk_ooch_t* idxnde_to_str (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_ooch_t* buf, hawk_oow_t* len)
{
	hawk_ooch_t* str;
	hawk_val_t* idx;

	HAWK_ASSERT (hawk_rtx_gethawk(rtx), nde != HAWK_NULL);

	if (!nde->next)
	{
		hawk_rtx_valtostr_out_t out;

		/* single node index */
		idx = eval_expression (rtx, nde);
		if (!idx) return HAWK_NULL;

		hawk_rtx_refupval (rtx, idx);

		str = HAWK_NULL;

		if (buf)
		{
			/* try with a fixed-size buffer if given */
			out.type = HAWK_RTX_VALTOSTR_CPLCPY;
			out.u.cplcpy.ptr = buf;
			out.u.cplcpy.len = *len;

			if (hawk_rtx_valtostr (rtx, idx, &out) >= 0)
			{
				str = out.u.cplcpy.ptr;
				*len = out.u.cplcpy.len;
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), str == buf);
			}
		}

		if (!str)
		{
			/* if no fixed-size buffer was given or the fixed-size 
			 * conversion failed, switch to the dynamic mode */
			out.type = HAWK_RTX_VALTOSTR_CPLDUP;
			if (hawk_rtx_valtostr(rtx, idx, &out) <= -1)
			{
				hawk_rtx_refdownval (rtx, idx);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			str = out.u.cpldup.ptr;
			*len = out.u.cpldup.len;
		}

		hawk_rtx_refdownval (rtx, idx);
	}
	else
	{
		/* multidimensional index */
		hawk_ooecs_t idxstr;
		hawk_oocs_t tmp;
		hawk_rtx_valtostr_out_t out;

		out.type = HAWK_RTX_VALTOSTR_STRPCAT;
		out.u.strpcat = &idxstr;

		if (hawk_ooecs_init(&idxstr, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1) 
		{
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		while (nde)
		{
			idx = eval_expression(rtx, nde);
			if (!idx) 
			{
				hawk_ooecs_fini (&idxstr);
				return HAWK_NULL;
			}

			hawk_rtx_refupval (rtx, idx);

			if (HAWK_OOECS_LEN(&idxstr) > 0 && hawk_ooecs_ncat(&idxstr, rtx->gbl.subsep.ptr, rtx->gbl.subsep.len) == (hawk_oow_t)-1)
			{
				hawk_rtx_refdownval (rtx, idx);
				hawk_ooecs_fini (&idxstr);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			if (hawk_rtx_valtostr(rtx, idx, &out) <= -1)
			{
				hawk_rtx_refdownval (rtx, idx);
				hawk_ooecs_fini (&idxstr);
				return HAWK_NULL;
			}

			hawk_rtx_refdownval (rtx, idx);
			nde = nde->next;
		}

		hawk_ooecs_yield (&idxstr, &tmp, 0);
		str = tmp.ptr;
		*len = tmp.len;

		hawk_ooecs_fini (&idxstr);
	}

	return str;
}

/* ========================================================================= */

hawk_ooch_t* hawk_rtx_format (
	hawk_rtx_t* rtx, hawk_ooecs_t* out, hawk_ooecs_t* fbu,
	const hawk_ooch_t* fmt, hawk_oow_t fmt_len, 
	hawk_oow_t nargs_on_stack, hawk_nde_t* args, hawk_oow_t* len)
{
	hawk_oow_t i;
	hawk_oow_t stack_arg_idx = 1;
	hawk_val_t* val;

#define OUT_CHAR(c) do { if (hawk_ooecs_ccat(out, (c)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define OUT_STR(ptr,len) do { if (hawk_ooecs_ncat(out, (ptr), (len)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define FMT_CHAR(c) do { if (hawk_ooecs_ccat(fbu, (c)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define FMT_STR(ptr,len) do { if (hawk_ooecs_ncat(fbu, (ptr), (len)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define GROW(buf) do { \
	if ((buf)->ptr) \
	{ \
		hawk_rtx_freemem (rtx, (buf)->ptr); \
		(buf)->ptr = HAWK_NULL; \
	} \
	(buf)->len += (buf)->inc; \
	(buf)->ptr = (hawk_ooch_t*)hawk_rtx_allocmem(rtx, (buf)->len * HAWK_SIZEOF(hawk_ooch_t)); \
	if ((buf)->ptr == HAWK_NULL) \
	{ \
		(buf)->len = 0; \
		return HAWK_NULL; \
	} \
}  while(0)

#define GROW_WITH_INC(buf,incv) do { \
	if ((buf)->ptr) \
	{ \
		hawk_rtx_freemem (rtx, (buf)->ptr); \
		(buf)->ptr = HAWK_NULL; \
	} \
	(buf)->len += ((incv) > (buf)->inc)? (incv): (buf)->inc; \
	(buf)->ptr = (hawk_ooch_t*)hawk_rtx_allocmem(rtx, (buf)->len * HAWK_SIZEOF(hawk_ooch_t)); \
	if ((buf)->ptr == HAWK_NULL) \
	{ \
		(buf)->len = 0; \
		return HAWK_NULL; \
	} \
} while(0)

	/* run->format.tmp.ptr should have been assigned a pointer to a block of memory before this function has been called */
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), rtx->format.tmp.ptr != HAWK_NULL);

	if (nargs_on_stack == (hawk_oow_t)-1) 
	{
		val = (hawk_val_t*)args;
		nargs_on_stack = 2;
	}
	else 
	{
		val = HAWK_NULL;
	}

	if (out == HAWK_NULL) out = &rtx->format.out;
	if (fbu == HAWK_NULL) fbu = &rtx->format.fmt;

	hawk_ooecs_clear (out);
	hawk_ooecs_clear (fbu);

	for (i = 0; i < fmt_len; i++)
	{
		hawk_int_t wp[2];
		int wp_idx;
#define WP_WIDTH     0
#define WP_PRECISION 1

		int flags;
#define FLAG_SPACE (1 << 0)
#define FLAG_HASH  (1 << 1)
#define FLAG_ZERO  (1 << 2)
#define FLAG_PLUS  (1 << 3)
#define FLAG_MINUS (1 << 4)

		if (HAWK_OOECS_LEN(fbu) == 0)
		{
			/* format specifier is empty */
			if (fmt[i] == HAWK_T('%')) 
			{
				/* add % to format specifier (fbu) */
				FMT_CHAR (fmt[i]);
			}
			else 
			{
				/* normal output */
				OUT_CHAR (fmt[i]);
			}
			continue;
		}

		/* handle flags */
		flags = 0;
		while (i < fmt_len)
		{
			switch (fmt[i])
			{
				case HAWK_T(' '):
					flags |= FLAG_SPACE;
					break;
				case HAWK_T('#'):
					flags |= FLAG_HASH;
					break;
				case HAWK_T('0'):
					flags |= FLAG_ZERO;
					break;
				case HAWK_T('+'):
					flags |= FLAG_PLUS;
					break;
				case HAWK_T('-'):
					flags |= FLAG_MINUS;
					break;
				default:
					goto wp_mod_init;
			}

			FMT_CHAR (fmt[i]); i++;
		}

wp_mod_init:
		wp[WP_WIDTH] = -1; /* width */
		wp[WP_PRECISION] = -1; /* precision */
		wp_idx = WP_WIDTH; /* width first */

wp_mod_main:
		if (i < fmt_len && fmt[i] == HAWK_T('*'))
		{
			/* variable width/precision modifier.
			 * take the width/precision from a parameter and
			 * transform it to a fixed length format */
			hawk_val_t* v;
			int n;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg (rtx, stack_arg_idx);
			}
			else
			{
				if (val) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &wp[wp_idx]);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL; 

			do
			{
				n = hawk_fmt_intmax_to_oocstr(
					rtx->format.tmp.ptr,
					rtx->format.tmp.len,
					wp[wp_idx], 
					10 | HAWK_FMT_INTMAX_NOTRUNC | HAWK_FMT_INTMAX_NONULL,
					-1,
					HAWK_T('\0'),
					HAWK_NULL
				);
				if (n <= -1)
				{
					/* -n is the number of characters required
					 * including terminating null  */
					GROW_WITH_INC (&rtx->format.tmp, -n);
					continue;
				}

				break;
			}
			while (1);

			FMT_STR(rtx->format.tmp.ptr, n);

			if (args == HAWK_NULL || val != HAWK_NULL) stack_arg_idx++;
			else args = args->next;
			i++;
		}
		else
		{
			/* fixed width/precision modifier */
			if (i < fmt_len && hawk_is_ooch_digit(fmt[i]))
			{
				wp[wp_idx] = 0;
				do
				{
					wp[wp_idx] = wp[wp_idx] * 10 + fmt[i] - HAWK_T('0');
					FMT_CHAR (fmt[i]); i++;
				}
				while (i < fmt_len && hawk_is_ooch_digit(fmt[i]));
			}
		}

		if (wp_idx == WP_WIDTH && i < fmt_len && fmt[i] == HAWK_T('.'))
		{
			wp[WP_PRECISION] = 0;
			FMT_CHAR (fmt[i]); i++;

			wp_idx = WP_PRECISION; /* change index to precision */
			goto wp_mod_main;
		}

		if (i >= fmt_len) break;

		if (fmt[i] == HAWK_T('d') || fmt[i] == HAWK_T('i') || 
		    fmt[i] == HAWK_T('x') || fmt[i] == HAWK_T('X') ||
		    fmt[i] == HAWK_T('b') || fmt[i] == HAWK_T('B') ||
		    fmt[i] == HAWK_T('o'))
		{
			hawk_val_t* v;
			hawk_int_t l;
			int n;

			int fmt_flags;
			int fmt_uint = 0;
			int fmt_width;
			hawk_ooch_t fmt_fill = HAWK_T('\0');
			const hawk_ooch_t* fmt_prefix = HAWK_NULL;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg (rtx, stack_arg_idx);
			}
			else
			{
				if (val != HAWK_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint (rtx, v, &l);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL; 

			fmt_flags = HAWK_FMT_INTMAX_NOTRUNC | HAWK_FMT_INTMAX_NONULL;

			if (l == 0 && wp[WP_PRECISION] == 0)
			{
				/* A zero value with a precision of zero produces
				 * no character. */
				fmt_flags |= HAWK_FMT_INTMAX_NOZERO;
			}

			if (wp[WP_WIDTH] != -1)
			{
				/* Width must be greater than 0 if specified */
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), wp[WP_WIDTH] > 0);

				/* justification when width is specified. */
				if (flags & FLAG_ZERO)
				{
					if (flags & FLAG_MINUS)
					{
						 /* FLAG_MINUS wins if both FLAG_ZERO 
						  * and FLAG_MINUS are specified. */
						fmt_fill = HAWK_T(' ');
						if (flags & FLAG_MINUS)
						{
							/* left justification. need to fill the right side */
							fmt_flags |= HAWK_FMT_INTMAX_FILLRIGHT;
						}
					}
					else
					{
						if (wp[WP_PRECISION] == -1)
						{
							/* precision not specified. 
							 * FLAG_ZERO can take effect */
							fmt_fill = HAWK_T('0');
							fmt_flags |= HAWK_FMT_INTMAX_FILLCENTER;
						}
						else
						{
							fmt_fill = HAWK_T(' ');
						}
					}
				}
				else
				{
					fmt_fill = HAWK_T(' ');
					if (flags & FLAG_MINUS)
					{
						/* left justification. need to fill the right side */
						fmt_flags |= HAWK_FMT_INTMAX_FILLRIGHT;
					}
				}
			}

			switch (fmt[i])
			{
				case HAWK_T('B'):
				case HAWK_T('b'):
					fmt_flags |= 2;
					fmt_uint = 1;
					if (l && (flags & FLAG_HASH)) 
					{
						/* A nonzero value is prefixed with 0b */
						fmt_prefix = HAWK_T("0b");
					}
					break;

				case HAWK_T('X'):
					fmt_flags |= HAWK_FMT_INTMAX_UPPERCASE;
				case HAWK_T('x'):
					fmt_flags |= 16;
					fmt_uint = 1;
					if (l && (flags & FLAG_HASH)) 
					{
						/* A nonzero value is prefixed with 0x */
						fmt_prefix = HAWK_T("0x");
					}
					break;

				case HAWK_T('o'):
					fmt_flags |= 8;
					fmt_uint = 1;
					if (flags & FLAG_HASH)
					{
						/* Force a leading zero digit including zero.
						 * 0 with FLAG_HASH and precision 0 still emits '0'.
						 * On the contrary, 'X' and 'x' emit no digits
						 * for 0 with FLAG_HASH and precision 0. */
						fmt_flags |= HAWK_FMT_INTMAX_ZEROLEAD;
					}
					break;
	
				default:
					fmt_flags |= 10;
					if (flags & FLAG_PLUS) 
						fmt_flags |= HAWK_FMT_INTMAX_PLUSSIGN;
					if (flags & FLAG_SPACE)
						fmt_flags |= HAWK_FMT_INTMAX_EMPTYSIGN;
					break;
			}


			if (wp[WP_WIDTH] > 0)
			{
				if (wp[WP_WIDTH] > rtx->format.tmp.len)
					GROW_WITH_INC (&rtx->format.tmp, wp[WP_WIDTH] - rtx->format.tmp.len);
				fmt_width = wp[WP_WIDTH];
			}
			else fmt_width = rtx->format.tmp.len;
			
			do
			{
				if (fmt_uint)
				{
					/* Explicit type-casting for 'l' from hawk_int_t 
					 * to hawk_uint_t is needed before passing it to 
					 * hawk_fmt_uintmax_to_oocstr(). 
 					 *
					 * Consider a value of -1 for example. 
					 * -1 is a value with all bits set. 
					 * If hawk_int_t is 4 bytes and hawk_uintmax_t
					 * is 8 bytes, the value is shown below for 
					 * each type respectively .
					 *     -1 - 0xFFFFFFFF (hawk_int_t)
					 *     -1 - 0xFFFFFFFFFFFFFFFF (hawk_uintmax_t)
					 * Implicit typecasting of -1 from hawk_int_t to
					 * to hawk_uintmax_t results in 0xFFFFFFFFFFFFFFFF,
					 * though 0xFFFFFFF is expected in hexadecimal.
					 */
					n = hawk_fmt_uintmax_to_oocstr (
						rtx->format.tmp.ptr,
						fmt_width,
						(hawk_uint_t)l, 
						fmt_flags,
						wp[WP_PRECISION],
						fmt_fill,
						fmt_prefix
					);
				}
				else
				{
					n = hawk_fmt_intmax_to_oocstr (
						rtx->format.tmp.ptr,
						fmt_width,
						l,
						fmt_flags,
						wp[WP_PRECISION],
						fmt_fill,
						fmt_prefix
					);
				}
				if (n <= -1)
				{
					/* -n is the number of characters required */
					GROW_WITH_INC (&rtx->format.tmp, -n);
					fmt_width = -n;
					continue;
				}

				break;
			}
			while (1);

			OUT_STR (rtx->format.tmp.ptr, n);
		}
		else if (fmt[i] == HAWK_T('e') || fmt[i] == HAWK_T('E') ||
		         fmt[i] == HAWK_T('g') || fmt[i] == HAWK_T('G') ||
		         fmt[i] == HAWK_T('f'))
		{
			hawk_val_t* v;
			hawk_flt_t r;
			int n;

		#if defined(HAWK_USE_AWK_FLTMAX)
			FMT_CHAR (HAWK_T('j'));
		#else
			FMT_CHAR (HAWK_T('z'));
		#endif
			FMT_CHAR (fmt[i]);

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg(rtx, stack_arg_idx);
			}
			else
			{
				if (val != HAWK_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoflt(rtx, v, &r);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL;

			if (hawk_ooecs_fcat(out, HAWK_OOECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;
		}
		else if (fmt[i] == HAWK_T('c')) 
		{
			hawk_ooch_t ch;
			hawk_oow_t ch_len;
			hawk_val_t* v;
			hawk_val_type_t vtype;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg (rtx, stack_arg_idx);
			}
			else
			{
				if (val != HAWK_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			vtype = HAWK_RTX_GETVALTYPE (rtx, v);
			switch (vtype)
			{
				case HAWK_VAL_NIL:
					ch = HAWK_T('\0');
					ch_len = 0;
					break;
			
				case HAWK_VAL_INT:
					ch = (hawk_ooch_t)HAWK_RTX_GETINTFROMVAL (rtx, v);
					ch_len = 1;
					break;

				case HAWK_VAL_FLT:
					ch = (hawk_ooch_t)((hawk_val_flt_t*)v)->val;
					ch_len = 1;
					break;

				case HAWK_VAL_STR:
					ch_len = ((hawk_val_str_t*)v)->val.len;
					if (ch_len > 0) 
					{
						ch = ((hawk_val_str_t*)v)->val.ptr[0];
						ch_len = 1;
					}
					else ch = HAWK_T('\0');
					break;

				case HAWK_VAL_MBS:
					ch_len = ((hawk_val_mbs_t*)v)->val.len;
					if (ch_len > 0) 
					{
						ch = ((hawk_val_mbs_t*)v)->val.ptr[0];
						ch_len = 1;
					}
					else ch = HAWK_T('\0');
					break;

				default:
					hawk_rtx_refdownval (rtx, v);
					SETERR_COD (rtx, HAWK_EVALTOCHR);
					return HAWK_NULL;
			}

			if (wp[WP_PRECISION] == -1 || wp[WP_PRECISION] == 0 || wp[WP_PRECISION] > (hawk_int_t)ch_len) 
			{
				wp[WP_PRECISION] = (hawk_int_t)ch_len;
			}

			if (wp[WP_PRECISION] > wp[WP_WIDTH]) wp[WP_WIDTH] = wp[WP_PRECISION];

			if (!(flags & FLAG_MINUS))
			{
				/* right align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_ooecs_ccat(out, HAWK_T(' ')) == (hawk_oow_t)-1) 
					{ 
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			if (wp[WP_PRECISION] > 0)
			{
				if (hawk_ooecs_ccat(out, ch) == (hawk_oow_t)-1) 
				{ 
					hawk_rtx_refdownval (rtx, v);
					return HAWK_NULL; 
				} 
			}

			if (flags & FLAG_MINUS)
			{
				/* left align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_ooecs_ccat (out, HAWK_T(' ')) == (hawk_oow_t)-1) 
					{ 
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			hawk_rtx_refdownval (rtx, v);
		}
		else if (fmt[i] == HAWK_T('s') || fmt[i] == HAWK_T('k') || fmt[i] == HAWK_T('K')) 
		{
			hawk_ooch_t* str_ptr, * str_free = HAWK_NULL;
			hawk_oow_t str_len;
			hawk_int_t k;
			hawk_val_t* v;
			hawk_val_type_t vtype;
			int bytetostr_flagged_radix = 16;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg (rtx, stack_arg_idx);
			}
			else
			{
				if (val)
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);

			vtype = HAWK_RTX_GETVALTYPE(rtx, v);
			switch (vtype)
			{
				case HAWK_VAL_NIL:
					str_ptr = HAWK_T("");
					str_len = 0;
					break;

				case HAWK_VAL_STR:
					str_ptr = ((hawk_val_str_t*)v)->val.ptr;
					str_len = ((hawk_val_str_t*)v)->val.len;
					break;

				case HAWK_VAL_MBS:
				#if defined(HAWK_OOCH_IS_BCH)
					str_ptr = ((hawk_val_mbs_t*)v)->val.ptr;
					str_len = ((hawk_val_mbs_t*)v)->val.len;
					break;
				#else
					if (fmt[i] != HAWK_T('s'))
					{
						str_ptr = (hawk_ooch_t*)((hawk_val_mbs_t*)v)->val.ptr;
						str_len = ((hawk_val_mbs_t*)v)->val.len;
						break;
					}
				#endif

				default:
				{
					if (v == val)
					{
						hawk_rtx_refdownval (rtx, v);
						SETERR_COD (rtx, HAWK_EFMTCNV);
						return HAWK_NULL;
					}

					str_ptr = hawk_rtx_valtooocstrdup(rtx, v, &str_len);
					if (!str_ptr)
					{
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL;
					}

					str_free = str_ptr;
					break;
				}
			}

			if (wp[WP_PRECISION] == -1 || wp[WP_PRECISION] > (hawk_int_t)str_len) wp[WP_PRECISION] = (hawk_int_t)str_len;
			if (wp[WP_PRECISION] > wp[WP_WIDTH]) wp[WP_WIDTH] = wp[WP_PRECISION];

			if (!(flags & FLAG_MINUS))
			{
				/* right align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_ooecs_ccat(out, HAWK_T(' ')) == (hawk_oow_t)-1) 
					{ 
						if (str_free) hawk_rtx_freemem (rtx, str_free);
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			if (fmt[i] == HAWK_T('k')) bytetostr_flagged_radix |= HAWK_BYTE_TO_BCSTR_LOWERCASE;

			for (k = 0; k < wp[WP_PRECISION]; k++) 
			{
				hawk_ooch_t curc;

			#if defined(HAWK_OOCH_IS_BCH)
				curc = str_ptr[k];
			#else
				if (vtype == HAWK_VAL_MBS && fmt[i] != HAWK_T('s')) 
					curc = (hawk_uint8_t)((hawk_bch_t*)str_ptr)[k];
				else curc = str_ptr[k];
			#endif

				if (fmt[i] != HAWK_T('s') && !HAWK_BYTE_PRINTABLE(curc))
				{
					hawk_ooch_t xbuf[3];
					if (curc <= 0xFF)
					{
						if (hawk_ooecs_ncat(out, HAWK_T("\\x"), 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr(curc, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
					}
					else if (curc <= 0xFFFF)
					{
						hawk_uint16_t u16 = curc;
						if (hawk_ooecs_ncat(out, HAWK_T("\\u"), 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr((u16 >> 8) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr(u16 & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
					}
					else
					{
						hawk_uint32_t u32 = curc;
						if (hawk_ooecs_ncat(out, HAWK_T("\\U"), 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr((u32 >> 24) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr((u32 >> 16) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr((u32 >> 8) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_oocstr(u32 & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetostr_flagged_radix, HAWK_T('0'));
						if (hawk_ooecs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
					}
				}
				else
				{
					if (hawk_ooecs_ccat(out, curc) == (hawk_oow_t)-1) 
					{
					s_fail:
						if (str_free) hawk_rtx_freemem (rtx, str_free);
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					}
				}
			}

			if (str_free) hawk_rtx_freemem (rtx, str_free);

			if (flags & FLAG_MINUS)
			{
				/* left align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_ooecs_ccat(out, HAWK_T(' ')) == (hawk_oow_t)-1) 
					{ 
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			hawk_rtx_refdownval (rtx, v);
		}
		else 
		{
			if (fmt[i] != HAWK_T('%')) OUT_STR (HAWK_OOECS_PTR(fbu), HAWK_OOECS_LEN(fbu));
			OUT_CHAR (fmt[i]);
			goto skip_taking_arg;
		}

		if (args == HAWK_NULL || val != HAWK_NULL) stack_arg_idx++;
		else args = args->next;
	skip_taking_arg:
		hawk_ooecs_clear (fbu);
	}

	/* flush uncompleted formatting sequence */
	OUT_STR (HAWK_OOECS_PTR(fbu), HAWK_OOECS_LEN(fbu));

	*len = HAWK_OOECS_LEN(out);
	return HAWK_OOECS_PTR(out);
}

/* ========================================================================= */

hawk_bch_t* hawk_rtx_formatmbs (
	hawk_rtx_t* rtx, hawk_becs_t* out, hawk_becs_t* fbu,
	const hawk_bch_t* fmt, hawk_oow_t fmt_len, 
	hawk_oow_t nargs_on_stack, hawk_nde_t* args, hawk_oow_t* len)
{
	hawk_oow_t i;
	hawk_oow_t stack_arg_idx = 1;
	hawk_val_t* val;

#define OUT_MCHAR(c) do { if (hawk_becs_ccat(out, (c)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define OUT_MBS(ptr,len) do { if (hawk_becs_ncat(out, (ptr), (len)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define FMT_MCHAR(c) do {  if (hawk_becs_ccat(fbu, (c)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define FMT_MBS(ptr,len) do { if (hawk_becs_ncat(fbu, (ptr), (len)) == (hawk_oow_t)-1) return HAWK_NULL; } while(0)

#define GROW_MBSBUF(buf) do { \
	if ((buf)->ptr) \
	{ \
		hawk_rtx_freemem (rtx, (buf)->ptr); \
		(buf)->ptr = HAWK_NULL; \
	} \
	(buf)->len += (buf)->inc; \
	(buf)->ptr = (hawk_bch_t*)hawk_rtx_allocmem(rtx, (buf)->len * HAWK_SIZEOF(hawk_bch_t)); \
	if ((buf)->ptr == HAWK_NULL) \
	{ \
		SETERR_COD (rtx, HAWK_ENOMEM); \
		(buf)->len = 0; \
		return HAWK_NULL; \
	} \
} while(0)

#define GROW_MBSBUF_WITH_INC(buf,incv) do { \
	if ((buf)->ptr) \
	{ \
		hawk_rtx_freemem (rtx, (buf)->ptr); \
		(buf)->ptr = HAWK_NULL; \
	} \
	(buf)->len += ((incv) > (buf)->inc)? (incv): (buf)->inc; \
	(buf)->ptr = (hawk_bch_t*)hawk_rtx_allocmem(rtx, (buf)->len * HAWK_SIZEOF(hawk_bch_t)); \
	if ((buf)->ptr == HAWK_NULL) \
	{ \
		SETERR_COD (rtx, HAWK_ENOMEM); \
		(buf)->len = 0; \
		return HAWK_NULL; \
	} \
} while(0)

	/* run->format.tmp.ptr should have been assigned a pointer to a block of memory before this function has been called */
	HAWK_ASSERT (hawk_rtx_gethawk(awk), rtx->formatmbs.tmp.ptr != HAWK_NULL)

	if (nargs_on_stack == (hawk_oow_t)-1) 
	{
		val = (hawk_val_t*)args;
		nargs_on_stack = 2;
	}
	else 
	{
		val = HAWK_NULL;
	}

	if (out == HAWK_NULL) out = &rtx->formatmbs.out;
	if (fbu == HAWK_NULL) fbu = &rtx->formatmbs.fmt;

	hawk_becs_clear (out);
	hawk_becs_clear (fbu);

	for (i = 0; i < fmt_len; i++)
	{
		hawk_int_t wp[2];
		int wp_idx;
#define WP_WIDTH     0
#define WP_PRECISION 1

		int flags;
#define FLAG_SPACE (1 << 0)
#define FLAG_HASH  (1 << 1)
#define FLAG_ZERO  (1 << 2)
#define FLAG_PLUS  (1 << 3)
#define FLAG_MINUS (1 << 4)

		if (HAWK_BECS_LEN(fbu) == 0)
		{
			/* format specifier is empty */
			if (fmt[i] == HAWK_BT('%')) 
			{
				/* add % to format specifier (fbu) */
				FMT_MCHAR (fmt[i]);
			}
			else 
			{
				/* normal output */
				OUT_MCHAR (fmt[i]);
			}
			continue;
		}

		/* handle flags */
		flags = 0;
		while (i < fmt_len)
		{
			switch (fmt[i])
			{
				case HAWK_BT(' '):
					flags |= FLAG_SPACE;
					break;
				case HAWK_BT('#'):
					flags |= FLAG_HASH;
					break;
				case HAWK_BT('0'):
					flags |= FLAG_ZERO;
					break;
				case HAWK_BT('+'):
					flags |= FLAG_PLUS;
					break;
				case HAWK_BT('-'):
					flags |= FLAG_MINUS;
					break;
				default:
					goto wp_mod_init;
			}

			FMT_MCHAR (fmt[i]); i++;
		}

wp_mod_init:
		wp[WP_WIDTH] = -1; /* width */
		wp[WP_PRECISION] = -1; /* precision */
		wp_idx = WP_WIDTH; /* width first */

wp_mod_main:
		if (i < fmt_len && fmt[i] == HAWK_BT('*'))
		{
			/* variable width/precision modifier.
			 * take the width/precision from a parameter and
			 * transform it to a fixed length format */
			hawk_val_t* v;
			int n;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg(rtx, stack_arg_idx);
			}
			else
			{
				if (val) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &wp[wp_idx]);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL; 

			do
			{
				n = hawk_fmt_intmax_to_bcstr (
					rtx->formatmbs.tmp.ptr,
					rtx->formatmbs.tmp.len,
					wp[wp_idx], 
					10 | HAWK_FMT_INTMAX_NOTRUNC | HAWK_FMT_INTMAX_NONULL,
					-1,
					HAWK_BT('\0'),
					HAWK_NULL
				);
				if (n <= -1)
				{
					/* -n is the number of characters required
					 * including terminating null  */
					GROW_MBSBUF_WITH_INC (&rtx->formatmbs.tmp, -n);
					continue;
				}

				break;
			}
			while (1);

			FMT_MBS(rtx->formatmbs.tmp.ptr, n);

			if (args == HAWK_NULL || val != HAWK_NULL) stack_arg_idx++;
			else args = args->next;
			i++;
		}
		else
		{
			/* fixed width/precision modifier */
			if (i < fmt_len && hawk_is_bch_digit(fmt[i]))
			{
				wp[wp_idx] = 0;
				do
				{
					wp[wp_idx] = wp[wp_idx] * 10 + fmt[i] - HAWK_BT('0');
					FMT_MCHAR (fmt[i]); i++;
				}
				while (i < fmt_len && hawk_is_bch_digit(fmt[i]));
			}
		}

		if (wp_idx == WP_WIDTH && i < fmt_len && fmt[i] == HAWK_BT('.'))
		{
			wp[WP_PRECISION] = 0;
			FMT_MCHAR (fmt[i]); i++;

			wp_idx = WP_PRECISION; /* change index to precision */
			goto wp_mod_main;
		}

		if (i >= fmt_len) break;

		if (fmt[i] == HAWK_BT('d') || fmt[i] == HAWK_BT('i') || 
		    fmt[i] == HAWK_BT('x') || fmt[i] == HAWK_BT('X') ||
		    fmt[i] == HAWK_BT('b') || fmt[i] == HAWK_BT('B') ||
		    fmt[i] == HAWK_BT('o'))
		{
			hawk_val_t* v;
			hawk_int_t l;
			int n;

			int fmt_flags;
			int fmt_uint = 0;
			int fmt_width;
			hawk_bch_t fmt_fill = HAWK_BT('\0');
			const hawk_bch_t* fmt_prefix = HAWK_NULL;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg (rtx, stack_arg_idx);
			}
			else
			{
				if (val != HAWK_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint (rtx, v, &l);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL; 

			fmt_flags = HAWK_FMT_INTMAX_NOTRUNC | HAWK_FMT_INTMAX_NONULL;

			if (l == 0 && wp[WP_PRECISION] == 0)
			{
				/* A zero value with a precision of zero produces
				 * no character. */
				fmt_flags |= HAWK_FMT_INTMAX_NOZERO;
			}

			if (wp[WP_WIDTH] != -1)
			{
				/* Width must be greater than 0 if specified */
				HAWK_ASSERT (hawk_rtx_gethawk(rtx), wp[WP_WIDTH] > 0);

				/* justification when width is specified. */
				if (flags & FLAG_ZERO)
				{
					if (flags & FLAG_MINUS)
					{
						 /* FLAG_MINUS wins if both FLAG_ZERO 
						  * and FLAG_MINUS are specified. */
						fmt_fill = HAWK_BT(' ');
						if (flags & FLAG_MINUS)
						{
							/* left justification. need to fill the right side */
							fmt_flags |= HAWK_FMT_INTMAX_FILLRIGHT;
						}
					}
					else
					{
						if (wp[WP_PRECISION] == -1)
						{
							/* precision not specified. 
							 * FLAG_ZERO can take effect */
							fmt_fill = HAWK_BT('0');
							fmt_flags |= HAWK_FMT_INTMAX_FILLCENTER;
						}
						else
						{
							fmt_fill = HAWK_BT(' ');
						}
					}
				}
				else
				{
					fmt_fill = HAWK_BT(' ');
					if (flags & FLAG_MINUS)
					{
						/* left justification. need to fill the right side */
						fmt_flags |= HAWK_FMT_INTMAX_FILLRIGHT;
					}
				}
			}

			switch (fmt[i])
			{
				case HAWK_BT('B'):
				case HAWK_BT('b'):
					fmt_flags |= 2;
					fmt_uint = 1;
					if (l && (flags & FLAG_HASH)) 
					{
						/* A nonzero value is prefixed with 0b */
						fmt_prefix = HAWK_BT("0b");
					}
					break;

				case HAWK_BT('X'):
					fmt_flags |= HAWK_FMT_INTMAX_UPPERCASE;
				case HAWK_BT('x'):
					fmt_flags |= 16;
					fmt_uint = 1;
					if (l && (flags & FLAG_HASH)) 
					{
						/* A nonzero value is prefixed with 0x */
						fmt_prefix = HAWK_BT("0x");
					}
					break;

				case HAWK_BT('o'):
					fmt_flags |= 8;
					fmt_uint = 1;
					if (flags & FLAG_HASH)
					{
						/* Force a leading zero digit including zero.
						 * 0 with FLAG_HASH and precision 0 still emits '0'.
						 * On the contrary, 'X' and 'x' emit no digits
						 * for 0 with FLAG_HASH and precision 0. */
						fmt_flags |= HAWK_FMT_INTMAX_ZEROLEAD;
					}
					break;
	
				default:
					fmt_flags |= 10;
					if (flags & FLAG_PLUS) 
						fmt_flags |= HAWK_FMT_INTMAX_PLUSSIGN;
					if (flags & FLAG_SPACE)
						fmt_flags |= HAWK_FMT_INTMAX_EMPTYSIGN;
					break;
			}


			if (wp[WP_WIDTH] > 0)
			{
				if (wp[WP_WIDTH] > rtx->formatmbs.tmp.len)
					GROW_MBSBUF_WITH_INC (&rtx->formatmbs.tmp, wp[WP_WIDTH] - rtx->formatmbs.tmp.len);
				fmt_width = wp[WP_WIDTH];
			}
			else fmt_width = rtx->formatmbs.tmp.len;
			
			do
			{
				if (fmt_uint)
				{
					/* Explicit type-casting for 'l' from hawk_int_t 
					 * to hawk_uint_t is needed before passing it to 
					 * hawk_fmt_uintmax_to_bcstr(). 
 					 *
					 * Consider a value of -1 for example. 
					 * -1 is a value with all bits set. 
					 * If hawk_int_t is 4 bytes and hawk_uintmax_t
					 * is 8 bytes, the value is shown below for 
					 * each type respectively .
					 *     -1 - 0xFFFFFFFF (hawk_int_t)
					 *     -1 - 0xFFFFFFFFFFFFFFFF (hawk_uintmax_t)
					 * Implicit typecasting of -1 from hawk_int_t to
					 * to hawk_uintmax_t results in 0xFFFFFFFFFFFFFFFF,
					 * though 0xFFFFFFF is expected in hexadecimal.
					 */
					n = hawk_fmt_uintmax_to_bcstr (
						rtx->formatmbs.tmp.ptr,
						fmt_width,
						(hawk_uint_t)l, 
						fmt_flags,
						wp[WP_PRECISION],
						fmt_fill,
						fmt_prefix
					);
				}
				else
				{
					n = hawk_fmt_intmax_to_bcstr (
						rtx->formatmbs.tmp.ptr,
						fmt_width,
						l,
						fmt_flags,
						wp[WP_PRECISION],
						fmt_fill,
						fmt_prefix
					);
				}
				if (n <= -1)
				{
					/* -n is the number of characters required */
					GROW_MBSBUF_WITH_INC (&rtx->formatmbs.tmp, -n);
					fmt_width = -n;
					continue;
				}

				break;
			}
			while (1);

			OUT_MBS (rtx->formatmbs.tmp.ptr, n);
		}
		else if (fmt[i] == HAWK_BT('e') || fmt[i] == HAWK_BT('E') ||
		         fmt[i] == HAWK_BT('g') || fmt[i] == HAWK_BT('G') ||
		         fmt[i] == HAWK_BT('f'))
		{
			hawk_val_t* v;
			hawk_flt_t r;
			int n;

		#if defined(HAWK_USE_AWK_FLTMAX)
			FMT_MCHAR (HAWK_BT('j'));
		#else
			FMT_MCHAR (HAWK_BT('z'));
		#endif
			FMT_MCHAR (fmt[i]);

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg(rtx, stack_arg_idx);
			}
			else
			{
				if (val != HAWK_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoflt(rtx, v, &r);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL;

			if (hawk_becs_fcat(out, HAWK_BECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;
		}
		else if (fmt[i] == HAWK_BT('c')) 
		{
			hawk_bch_t ch;
			hawk_oow_t ch_len;
			hawk_val_t* v;
			hawk_val_type_t vtype;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg(rtx, stack_arg_idx);
			}
			else
			{
				if (val != HAWK_NULL) 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			vtype = HAWK_RTX_GETVALTYPE (rtx, v);
			switch (vtype)
			{
				case HAWK_VAL_NIL:
					ch = HAWK_BT('\0');
					ch_len = 0;
					break;

				case HAWK_VAL_INT:
					ch = (hawk_bch_t)HAWK_RTX_GETINTFROMVAL(rtx, v);
					ch_len = 1;
					break;

				case HAWK_VAL_FLT:
					ch = (hawk_bch_t)((hawk_val_flt_t*)v)->val;
					ch_len = 1;
					break;

				case HAWK_VAL_STR:
					ch_len = ((hawk_val_str_t*)v)->val.len;
					if (ch_len > 0) 
					{
						/* value truncation is expected */
						ch = ((hawk_val_str_t*)v)->val.ptr[0];
						ch_len = 1;
					}
					else ch = HAWK_BT('\0');
					break;

				case HAWK_VAL_MBS:
					ch_len = ((hawk_val_mbs_t*)v)->val.len;
					if (ch_len > 0) 
					{
						ch = ((hawk_val_mbs_t*)v)->val.ptr[0];
						ch_len = 1;
					}
					else ch = HAWK_BT('\0');
					break;

				default:
					hawk_rtx_refdownval (rtx, v);
					SETERR_COD (rtx, HAWK_EVALTOCHR);
					return HAWK_NULL;
			}

			if (wp[WP_PRECISION] == -1 || wp[WP_PRECISION] == 0 || wp[WP_PRECISION] > (hawk_int_t)ch_len) 
			{
				wp[WP_PRECISION] = (hawk_int_t)ch_len;
			}

			if (wp[WP_PRECISION] > wp[WP_WIDTH]) wp[WP_WIDTH] = wp[WP_PRECISION];

			if (!(flags & FLAG_MINUS))
			{
				/* right align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_becs_ccat(out, HAWK_BT(' ')) == (hawk_oow_t)-1) 
					{ 
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			if (wp[WP_PRECISION] > 0)
			{
				if (hawk_becs_ccat(out, ch) == (hawk_oow_t)-1) 
				{ 
					hawk_rtx_refdownval (rtx, v);
					return HAWK_NULL; 
				} 
			}

			if (flags & FLAG_MINUS)
			{
				/* left align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_becs_ccat (out, HAWK_BT(' ')) == (hawk_oow_t)-1) 
					{ 
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			hawk_rtx_refdownval (rtx, v);
		}
		else if (fmt[i] == HAWK_BT('s') || fmt[i] == HAWK_BT('k') || fmt[i] == HAWK_BT('K')) 
		{
			hawk_bch_t* str_ptr, * str_free = HAWK_NULL;
			hawk_oow_t str_len;
			hawk_int_t k;
			hawk_val_t* v;
			hawk_val_type_t vtype;
			int bytetombs_flagged_radix = 16;

			if (args == HAWK_NULL)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					SETERR_COD (rtx, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg (rtx, stack_arg_idx);
			}
			else
			{
				if (val)
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						SETERR_COD (rtx, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression (rtx, args);
					if (v == HAWK_NULL) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);

			vtype = HAWK_RTX_GETVALTYPE(rtx, v);
			switch (vtype)
			{
				case HAWK_VAL_NIL:
					str_ptr = HAWK_BT("");
					str_len = 0;
					break;

				case HAWK_VAL_MBS:
					str_ptr = ((hawk_val_mbs_t*)v)->val.ptr;
					str_len = ((hawk_val_mbs_t*)v)->val.len;
					break;

				case HAWK_VAL_STR:
				#if defined(HAWK_OOCH_IS_BCH)
					str_ptr = ((hawk_val_str_t*)v)->val.ptr;
					str_len = ((hawk_val_str_t*)v)->val.len;
					break;
				#else
					if (fmt[i] != HAWK_BT('s'))
					{
						/* arrange to print the wide character string byte by byte regardless of byte order */
						str_ptr = (hawk_bch_t*)((hawk_val_str_t*)v)->val.ptr;
						str_len = ((hawk_val_str_t*)v)->val.len * (HAWK_SIZEOF_OOCH_T / HAWK_SIZEOF_BCH_T);
						break;
					}
					/* fall thru */
				#endif

				default:
				{
					if (v == val)
					{
						hawk_rtx_refdownval (rtx, v);
						SETERR_COD (rtx, HAWK_EFMTCNV);
						return HAWK_NULL;
					}

					str_ptr = hawk_rtx_valtobcstrdup(rtx, v, &str_len);
					if (!str_ptr)
					{
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL;
					}

					str_free = str_ptr;
					break;
				}
			}

			if (wp[WP_PRECISION] == -1 || wp[WP_PRECISION] > (hawk_int_t)str_len) wp[WP_PRECISION] = (hawk_int_t)str_len;
			if (wp[WP_PRECISION] > wp[WP_WIDTH]) wp[WP_WIDTH] = wp[WP_PRECISION];

			if (!(flags & FLAG_MINUS))
			{
				/* right align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_becs_ccat(out, HAWK_BT(' ')) == (hawk_oow_t)-1) 
					{ 
						if (str_free) hawk_rtx_freemem (rtx, str_free);
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			if (fmt[i] == HAWK_BT('k')) bytetombs_flagged_radix |= HAWK_BYTE_TO_BCSTR_LOWERCASE;

			for (k = 0; k < wp[WP_PRECISION]; k++) 
			{
				hawk_bch_t curc;

				curc = str_ptr[k];

				if (fmt[i] != HAWK_BT('s') && !HAWK_BYTE_PRINTABLE(curc))
				{
					hawk_bch_t xbuf[3];
					if (curc <= 0xFF)
					{
						if (hawk_becs_ncat(out, HAWK_BT("\\x"), 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr(curc, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
					}
					else if (curc <= 0xFFFF)
					{
						hawk_uint16_t u16 = curc;
						if (hawk_becs_ncat(out, HAWK_BT("\\u"), 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr((u16 >> 8) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr(u16 & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
					}
					else
					{
						hawk_uint32_t u32 = curc;
						if (hawk_becs_ncat(out, HAWK_BT("\\U"), 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr((u32 >> 24) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr((u32 >> 16) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr((u32 >> 8) & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
						hawk_byte_to_bcstr(u32 & 0xFF, xbuf, HAWK_COUNTOF(xbuf), bytetombs_flagged_radix, HAWK_BT('0'));
						if (hawk_becs_ncat(out, xbuf, 2) == (hawk_oow_t)-1) goto s_fail;
					}
				}
				else
				{
					if (hawk_becs_ccat(out, curc) == (hawk_oow_t)-1) 
					{
					s_fail:
						if (str_free) hawk_rtx_freemem (rtx, str_free);
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					}
				}
			}

			if (str_free) hawk_rtx_freemem (rtx, str_free);

			if (flags & FLAG_MINUS)
			{
				/* left align */
				while (wp[WP_WIDTH] > wp[WP_PRECISION]) 
				{
					if (hawk_becs_ccat(out, HAWK_BT(' ')) == (hawk_oow_t)-1) 
					{ 
						hawk_rtx_refdownval (rtx, v);
						return HAWK_NULL; 
					} 
					wp[WP_WIDTH]--;
				}
			}

			hawk_rtx_refdownval (rtx, v);
		}
		else 
		{
			if (fmt[i] != HAWK_BT('%')) OUT_MBS (HAWK_BECS_PTR(fbu), HAWK_BECS_LEN(fbu));
			OUT_MCHAR (fmt[i]);
			goto skip_taking_arg;
		}

		if (args == HAWK_NULL || val != HAWK_NULL) stack_arg_idx++;
		else args = args->next;
	skip_taking_arg:
		hawk_becs_clear (fbu);
	}

	/* flush uncompleted formatting sequence */
	OUT_MBS (HAWK_BECS_PTR(fbu), HAWK_BECS_LEN(fbu));

	*len = HAWK_BECS_LEN(out);
	return HAWK_BECS_PTR(out);
}

/* ------------------------------------------------------------------------- */

void hawk_rtx_setnrflt (hawk_rtx_t* rtx, const hawk_nrflt_t* nrflt)
{
	rtx->nrflt = *nrflt;
}

void hawk_rtx_getnrflt (hawk_rtx_t* rtx, hawk_nrflt_t* nrflt)
{
	*nrflt = rtx->nrflt;
}



/* ------------------------------------------------------------------------ */

hawk_oow_t hawk_rtx_fmttoucstr_ (hawk_rtx_t* rtx, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, ...) 
{
	va_list ap;
	hawk_oow_t n;
	va_start(ap, fmt);
	n = hawk_gem_vfmttoucstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap);
	va_end(ap);
	return n; 
}

hawk_oow_t hawk_rtx_fmttobcstr_ (hawk_rtx_t* rtx, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, ...) 
{
	va_list ap;
	hawk_oow_t n;
	va_start(ap, fmt);
	n = hawk_gem_vfmttobcstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap);
	va_end(ap);
	return n;
}
