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

#include "hawk-prv.h"

#define PRINT_IOERR -99

#define CMP_ERROR -99
#define DEF_BUF_CAPA 256
#define HAWK_RTX_STACK_INCREMENT 512

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
#define DEFAULT_FS       HAWK_T(" ")
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

#define CLRERR(rtx) hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENOERR)
#define ADJERR_LOC(rtx,l) do { (rtx)->_gem.errloc = *(l); } while (0)

static hawk_oow_t push_arg_from_vals (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data);
static hawk_oow_t push_arg_from_nde (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data);

static int init_rtx (hawk_rtx_t* rtx, hawk_t* hawk, hawk_rio_cbs_t* rio);
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
static int run_forin (hawk_rtx_t* rtx, hawk_nde_forin_t* nde);
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

static int output_formatted (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* dst, const hawk_ooch_t* fmt, hawk_oow_t fmt_len, hawk_nde_t* args);
static int output_formatted_bytes (hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* dst, const hawk_bch_t* fmt, hawk_oow_t fmt_len, hawk_nde_t* args);

static hawk_val_t* eval_expression (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_expression0 (hawk_rtx_t* rtx, hawk_nde_t* nde);

static hawk_val_t* eval_group (hawk_rtx_t* rtx, hawk_nde_t* nde);

static hawk_val_t* eval_assignment (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* do_assignment (hawk_rtx_t* rtx, hawk_nde_t* var, hawk_val_t* val);
static hawk_val_t* do_assignment_nonindexed (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val);
static hawk_val_t* do_assignment_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val);
static hawk_val_t* do_assignment_positional (hawk_rtx_t* rtx, hawk_nde_pos_t* pos, hawk_val_t* val);

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

static hawk_val_t* eval_fncall_fnc (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_fncall_fun (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_fncall_var (hawk_rtx_t* rtx, hawk_nde_t* nde);

static int get_reference (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_val_t*** ref);
static hawk_val_t** get_reference_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* var);

static hawk_val_t* eval_char (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_bchr (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_int (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_flt (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_str (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_mbs (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_rex (hawk_rtx_t* rtx, hawk_nde_t* nde);
static hawk_val_t* eval_xnil (hawk_rtx_t* rtx, hawk_nde_t* nde);
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

static int read_record (hawk_rtx_t* rtx);

static hawk_ooch_t* idxnde_to_str (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_ooch_t* buf, hawk_oow_t* len, hawk_nde_t** remidx, hawk_int_t* firstidxint);
static hawk_ooi_t idxnde_to_int (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_nde_t** remidx);

typedef hawk_val_t* (*binop_func_t) (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right);
typedef hawk_val_t* (*eval_expr_t) (hawk_rtx_t* rtx, hawk_nde_t* nde);

#define POS_VAL(rtx, idx) \
	(((idx) == 0)? (rtx)->inrec.d0: \
	 ((idx) > 0 && (idx) <= (hawk_int_t)(rtx)->inrec.nflds)? (rtx)->inrec.flds[(idx) - 1].val: \
	 hawk_val_zls)

HAWK_INLINE hawk_oow_t hawk_rtx_getnargs (hawk_rtx_t* rtx)
{
	return (hawk_oow_t)HAWK_RTX_STACK_NARGS(rtx);
}

HAWK_INLINE hawk_val_t* hawk_rtx_getarg (hawk_rtx_t* rtx, hawk_oow_t idx)
{
	return HAWK_RTX_STACK_ARG(rtx, idx);
}

HAWK_INLINE hawk_val_t* hawk_rtx_getgbl (hawk_rtx_t* rtx, int id)
{
	HAWK_ASSERT (id >= 0 && id < (int)HAWK_ARR_SIZE(rtx->hawk->parse.gbls));
	return HAWK_RTX_STACK_GBL(rtx, id);
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

	old = HAWK_RTX_STACK_GBL (rtx, idx);

	vtype = HAWK_RTX_GETVALTYPE (rtx, val);
	old_vtype = HAWK_RTX_GETVALTYPE (rtx, old);

	if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
	{
		hawk_errnum_t errnum = HAWK_ENOERR;
		const hawk_ooch_t* errfmt;

		if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
		{
			if (old_vtype == HAWK_VAL_NIL)
			{
				/* a nil valul can be overridden with any values */
				/* ok. no error */
			}
			else if (!assign && old_vtype == vtype)
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
				errnum = HAWK_ESCALARTONONSCA;
				errfmt = HAWK_T("not allowed to change a scalar value in '%.*js' to a nonscalar value");
			}
		}
		else
		{
			if (old_vtype == HAWK_VAL_MAP || old_vtype == HAWK_VAL_ARR) 
			{
				errnum = HAWK_ENONSCATOSCALAR;
				errfmt = HAWK_T("not allowed to change a nonscalar value in '%.*js' to a scalar value");
			}
		}

		if (errnum != HAWK_ENOERR)
		{
			/* once a variable becomes a map, it cannot be assigned 
			 * others value than another map. you can only add a member
			 * using indexing. */
			if (var)
			{
				/* global variable */
				hawk_rtx_seterrfmt (rtx, &var->loc, errnum, errfmt, var->id.name.len, var->id.name.ptr);
			}
			else
			{
				/* hawk_rtx_setgbl() has been called */
				hawk_oocs_t ea;
				ea.ptr = (hawk_ooch_t*)hawk_getgblname(hawk_rtx_gethawk(rtx), idx, &ea.len);
				hawk_rtx_seterrfmt (rtx, HAWK_NULL, errnum, errfmt, ea.len, ea.ptr);
			}

			return -1;
		}
	}

	if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
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
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_ESCALARTONONSCA, HAWK_T("not allowed to change a scalar value in '%.*js' to a nonscalar value"), ea.len, ea.ptr);
			return -1;
		}
	}

	if (old == val && idx != HAWK_GBL_NF) /* read the comment in the case block for HAWK_GBL_NF below */
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
			hawk_oocs_t str;

			str.ptr = hawk_rtx_valtooocstrdup(rtx, val, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			for (i = 0; i < str.len; i++)
			{
				if (str.ptr[i] == '\0')
				{
					/* '\0' is included in the value */
					hawk_rtx_freemem (rtx, str.ptr);
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ECONVFMTCHR);
					return -1;
				}
			}

			if (rtx->gbl.convfmt.ptr) hawk_rtx_freemem (rtx, rtx->gbl.convfmt.ptr);
			rtx->gbl.convfmt.ptr = str.ptr;
			rtx->gbl.convfmt.len = str.len;
			break;
		}

		case HAWK_GBL_FNR:
		{
			int n;
			hawk_int_t lv;

			n = hawk_rtx_valtoint(rtx, val, &lv);
			if (HAWK_UNLIKELY(n <= -1)) return -1;

			rtx->gbl.fnr = lv;
			break;
		}
	
		case HAWK_GBL_FS:
		{
			hawk_ooch_t* fs_ptr;
			hawk_oow_t fs_len;

			/* due to the expression evaluation rule, the 
			 * regular expression can not be an assigned value */
			HAWK_ASSERT (vtype != HAWK_VAL_REX);

			fs_ptr = hawk_rtx_getvaloocstr(rtx, val, &fs_len);
			if (HAWK_UNLIKELY(!fs_ptr)) return -1;

			if (fs_len > 1 && !(fs_len == 5 && fs_ptr[0] == '?'))
			{
				/* it's a regular expression if FS contains multiple characters.
				 * however, it's not a regular expression if it's 5 character
				 * string beginning with a question mark. */
				hawk_tre_t* rex, * irex;

				if (hawk_rtx_buildrex(rtx, fs_ptr, fs_len, &rex, &irex) <= -1)
				{
					hawk_rtx_freevaloocstr (rtx, val, fs_ptr);
					return -1;
				}

				if (rtx->gbl.fs[0]) hawk_rtx_freerex (rtx, rtx->gbl.fs[0], rtx->gbl.fs[1]);

				rtx->gbl.fs[0] = rex;
				rtx->gbl.fs[1] = irex;
			}

			hawk_rtx_freevaloocstr (rtx, val, fs_ptr);
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

			if (lv < 0)
			{
				hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EINVAL, HAWK_T("negative value into NF"));
				return -1;
			}

			if (lv < (hawk_int_t)rtx->inrec.nflds || (assign && lv == (hawk_int_t)rtx->inrec.nflds))
			{
				/* when NF is assigned a value, it should rebuild $X values.
				 * even when there is no change in the value like NF = NF,
				 * it has to rebuil $X values with the current OFS value.
				 *  { OFS=":"; NF=NF; print $0; }
				 * the NF=value assignment is indicated by a non-zero value in the 'assign' variable.
				 * 'assign' is 0 if this function is called from a different context such as 
				 * explicit call to hawk_rtx_setgbl().
				 */

				if (hawk_rtx_truncrec(rtx, (hawk_oow_t)lv) <= -1)
				{
					/* adjust the error line */
					/*if (var) ADJERR_LOC (rtx, &var->loc);*/
					return -1;
				}
			}
			else if (lv > (hawk_int_t)rtx->inrec.nflds)
			{
				hawk_oocs_t cs;
				cs.ptr = HAWK_T("");
				cs.len = 0;
				if (hawk_rtx_setrec(rtx, lv, &cs, 0) <= -1) return -1;
			}

			/* for all other globals, it returns before this switch/case block is reached 
			 * if the same value is assigned. but NF change requires extra action to take
			 * as coded before this switch/case block. */
			if (old == val) return 0;  

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
			hawk_oocs_t str;

			str.ptr = hawk_rtx_valtooocstrdup(rtx, val, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			for (i = 0; i < str.len; i++)
			{
				if (str.ptr[i] == '\0')
				{
					/* '\0' is included in the value */
					hawk_rtx_freemem (rtx, str.ptr);
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ECONVFMTCHR);
					return -1;
				}
			}

			if (rtx->gbl.ofmt.ptr) hawk_rtx_freemem (rtx, rtx->gbl.ofmt.ptr);
			rtx->gbl.ofmt.ptr = str.ptr;
			rtx->gbl.ofmt.len = str.len;
		}

		case HAWK_GBL_OFS:
		{
			hawk_oocs_t str;

			str.ptr = hawk_rtx_valtooocstrdup(rtx, val, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			if (rtx->gbl.ofs.ptr) hawk_rtx_freemem (rtx, rtx->gbl.ofs.ptr);
			rtx->gbl.ofs.ptr = str.ptr;
			rtx->gbl.ofs.len = str.len;

			break;
		}

		case HAWK_GBL_ORS:
		{	
			hawk_oocs_t str;

			str.ptr = hawk_rtx_valtooocstrdup(rtx, val, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			if (rtx->gbl.ors.ptr) hawk_rtx_freemem (rtx, rtx->gbl.ors.ptr);
			rtx->gbl.ors.ptr = str.ptr;
			rtx->gbl.ors.len = str.len;

			break;
		}

		case HAWK_GBL_RS:
		{
			hawk_oocs_t rss;

			/* due to the expression evaluation rule, the 
			 * regular expression can not be an assigned 
			 * value */
			HAWK_ASSERT (vtype != HAWK_VAL_REX);

			rss.ptr = hawk_rtx_getvaloocstr(rtx, val, &rss.len);
			if (!rss.ptr) return -1;

			if (rtx->gbl.rs[0])
			{
				hawk_rtx_freerex (rtx, rtx->gbl.rs[0], rtx->gbl.rs[1]);
				rtx->gbl.rs[0] = HAWK_NULL;
				rtx->gbl.rs[1] = HAWK_NULL;
			}

			if (rss.len > 1)
			{
				hawk_tre_t* rex, * irex;

				/* compile the regular expression */
				if (hawk_rtx_buildrex(rtx, rss.ptr, rss.len, &rex, &irex) <= -1)
				{
					hawk_rtx_freevaloocstr (rtx, val, rss.ptr);
					return -1;
				}

				rtx->gbl.rs[0] = rex;
				rtx->gbl.rs[1] = irex;
			}

			hawk_rtx_freevaloocstr (rtx, val, rss.ptr);
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

		case HAWK_GBL_STRIPSTRSPC:
		{
			hawk_int_t l;
			hawk_flt_t r;
			int vt;

			vt = hawk_rtx_valtonum(rtx, val, &l, &r);
			if (vt <= -1) return -1;

			if (vt == 0) 
				rtx->gbl.stripstrspc = ((l > 0)? 1: (l < 0)? -1: 0);
			else 
				rtx->gbl.stripstrspc = ((r > 0.0)? 1: (r < 0.0)? -1: 0);
			break;
		}

		case HAWK_GBL_SUBSEP:
		{
			hawk_oocs_t str;

			str.ptr = hawk_rtx_valtooocstrdup(rtx, val, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			if (rtx->gbl.subsep.ptr) hawk_rtx_freemem (rtx, rtx->gbl.subsep.ptr);
			rtx->gbl.subsep.ptr = str.ptr;
			rtx->gbl.subsep.len = str.len;

			break;
		}
	}

	hawk_rtx_refdownval (rtx, old);
	HAWK_RTX_STACK_GBL(rtx,idx) = val;
	hawk_rtx_refupval (rtx, val);

	for (ecb = (rtx)->ecb; ecb; ecb = ecb->next)
	{
		if (ecb->gblset) ecb->gblset (rtx, idx, val);
	}

	return 0;
}

HAWK_INLINE void hawk_rtx_setretval (hawk_rtx_t* rtx, hawk_val_t* val)
{
	hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_RETVAL(rtx));
	HAWK_RTX_STACK_RETVAL(rtx) = val;
	/* should use the same trick as run_return */
	hawk_rtx_refupval (rtx, val);
}

HAWK_INLINE int hawk_rtx_setgbl (hawk_rtx_t* rtx, int id, hawk_val_t* val)
{
	HAWK_ASSERT (id >= 0 && id < (int)HAWK_ARR_SIZE(rtx->hawk->parse.gbls));
	return set_global(rtx, id, HAWK_NULL, val, 0);
}

int hawk_rtx_setgbltostrbyname (hawk_rtx_t* rtx, const hawk_ooch_t* name, const hawk_ooch_t* val)
{
	int id, n;
	hawk_val_t* v;

	HAWK_ASSERT (hawk_isvalidident(hawk_rtx_gethawk(rtx), name));

	id = hawk_findgblwithoocstr(hawk_rtx_gethawk(rtx), name, 1);
	v = hawk_rtx_makestrvalwithoocstr(rtx, val);
	if (!v) return -1;

	hawk_rtx_refupval (rtx, v);
	if (id <= -1)
	{
		/* not found. it's not a builtin/declared global variable */
		hawk_nde_var_t var;

		/* make a fake variable node for assignment and use it */
		HAWK_MEMSET (&var, 0, HAWK_SIZEOF(var));
		var.type = HAWK_NDE_NAMED;
		var.id.name.ptr = (hawk_ooch_t*)name;
		var.id.name.len = hawk_count_oocstr(name);
		var.id.idxa = (hawk_oow_t)-1;
		var.idx = HAWK_NULL;

		n = do_assignment_nonindexed(rtx, &var, v)? 0: -1;
	}
	else
	{
		n = set_global(rtx, id, HAWK_NULL, v, 0);
	}
	hawk_rtx_refdownval (rtx, v);

	return n;
}

int hawk_rtx_setscriptnamewithuchars (hawk_rtx_t* rtx, const hawk_uch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	tmp = hawk_rtx_makestrvalwithuchars(rtx, name, len);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_setgbl(rtx, HAWK_GBL_SCRIPTNAME, tmp);
	hawk_rtx_refdownval (rtx, tmp);

	return n;
}

int hawk_rtx_setscriptnamewithbchars (hawk_rtx_t* rtx, const hawk_bch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	tmp = hawk_rtx_makestrvalwithbchars(rtx, name, len);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_setgbl(rtx, HAWK_GBL_SCRIPTNAME, tmp);
	hawk_rtx_refdownval (rtx, tmp);

	return n;
}

int hawk_rtx_setfilenamewithuchars (hawk_rtx_t* rtx, const hawk_uch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	tmp = hawk_rtx_makestrvalwithuchars(rtx, name, len);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_setgbl(rtx, HAWK_GBL_FILENAME, tmp);
	hawk_rtx_refdownval (rtx, tmp);

	return n;
}

int hawk_rtx_setfilenamewithbchars (hawk_rtx_t* rtx, const hawk_bch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	tmp = hawk_rtx_makestrvalwithbchars(rtx, name, len);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_setgbl(rtx, HAWK_GBL_FILENAME, tmp);
	hawk_rtx_refdownval (rtx, tmp);

	return n;
}


int hawk_rtx_setofilenamewithuchars (hawk_rtx_t* rtx, const hawk_uch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	if (rtx->hawk->opt.trait & HAWK_NEXTOFILE)
	{
		tmp = hawk_rtx_makestrvalwithuchars(rtx, name, len);
		if (HAWK_UNLIKELY(!tmp)) return -1;

		hawk_rtx_refupval (rtx, tmp);
		n = hawk_rtx_setgbl(rtx, HAWK_GBL_OFILENAME, tmp);
		hawk_rtx_refdownval (rtx, tmp);
	}
	else n = 0;

	return n;
}

int hawk_rtx_setofilenamewithbchars (hawk_rtx_t* rtx, const hawk_bch_t* name, hawk_oow_t len)
{
	hawk_val_t* tmp;
	int n;

	if (rtx->hawk->opt.trait & HAWK_NEXTOFILE)
	{
		tmp = hawk_rtx_makestrvalwithbchars(rtx, name, len);
		if (HAWK_UNLIKELY(!tmp)) return -1;

		hawk_rtx_refupval (rtx, tmp);
		n = hawk_rtx_setgbl(rtx, HAWK_GBL_OFILENAME, tmp);
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

hawk_rtx_t* hawk_rtx_open (hawk_t* hawk, hawk_oow_t xtnsize, hawk_rio_cbs_t* rio)
{
	hawk_rtx_t* rtx;
	struct module_init_ctx_t mic;
	hawk_oow_t i;

	/* clear the hawk error code */
	hawk_seterrnum (hawk, HAWK_NULL, HAWK_ENOERR);

	/* check if the code has ever been parsed */
	if (hawk->tree.ngbls == 0 && 
	    hawk->tree.begin == HAWK_NULL &&
	    hawk->tree.end == HAWK_NULL &&
	    hawk->tree.chain_size == 0 &&
	    hawk_htb_getsize(hawk->tree.funs) == 0)
	{
		hawk_seterrnum (hawk, HAWK_NULL, HAWK_EPERM);
		return HAWK_NULL;
	}
	
	/* allocate the storage for the rtx object */
	rtx = (hawk_rtx_t*)hawk_allocmem(hawk, HAWK_SIZEOF(hawk_rtx_t) + xtnsize);
	if (HAWK_UNLIKELY(!rtx))
	{
		/* if it fails, the failure is reported thru the hawk object */
		return HAWK_NULL;
	}

	/* initialize the rtx object */
	HAWK_MEMSET (rtx, 0, HAWK_SIZEOF(hawk_rtx_t) + xtnsize);
	rtx->_instsize = HAWK_SIZEOF(hawk_rtx_t);
	if (HAWK_UNLIKELY(init_rtx(rtx, hawk, rio) <= -1)) 
	{
		/* because the error information is in the gem part, 
		 * it should be ok to copy over rtx error to hawk even if
		 * rtx initialization fails. */
		hawk_rtx_errortohawk (rtx, hawk); 
		hawk_freemem (hawk, rtx);
		return HAWK_NULL;
	}

	if (HAWK_UNLIKELY(init_globals(rtx) <= -1))
	{
		hawk_rtx_errortohawk (rtx, hawk);
		fini_rtx (rtx, 0);
		hawk_freemem (hawk, rtx);
		return HAWK_NULL;
	}

	mic.count = 0;
	mic.rtx = rtx;
	hawk_rbt_walk (rtx->hawk->modtab, init_module, &mic);
	if (mic.count != HAWK_RBT_SIZE(rtx->hawk->modtab))
	{
		if (mic.count > 0)
		{
			struct module_fini_ctx_t mfc;
			mfc.limit = mic.count;
			mfc.count = 0;
			hawk_rbt_walk (rtx->hawk->modtab, fini_module, &mfc);
		}

		fini_rtx (rtx, 1);
		hawk_freemem (hawk, rtx);
		return HAWK_NULL;
	}

	/* chain the ctos slots */
	rtx->ctos.fi = HAWK_COUNTOF(rtx->ctos.b) - 1;
	for (i = HAWK_COUNTOF(rtx->ctos.b); i > 1;)
	{
		--i;
		rtx->ctos.b[i].c[0] = i - 1;
	}
	/* rtx->ctos.b[0] is not used as the free index of 0 indicates the end of the list.
	 * see hawk_rtx_getvaloocstr(). */

	/* chain the bctos slots */
	rtx->bctos.fi = HAWK_COUNTOF(rtx->bctos.b) - 1;
	for (i = HAWK_COUNTOF(rtx->bctos.b); i > 1;)
	{
		--i;
		rtx->bctos.b[i].c[0] = i - 1;
	}
	/* rtx->bctos.b[0] is not used as the free index of 0 indicates the end of the list.
	 * see hawk_rtx_getvaloocstr(). */

	return rtx;
}

void hawk_rtx_close (hawk_rtx_t* rtx)
{
	hawk_rtx_ecb_t* ecb;
	struct module_fini_ctx_t mfc;

	mfc.limit = 0;
	mfc.count = 0;
	mfc.rtx = rtx;
	hawk_rbt_walk (rtx->hawk->modtab, fini_module, &mfc);

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
	return (rtx->exit_level == EXIT_ABORT || rtx->hawk->haltall);
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

static int init_rtx (hawk_rtx_t* rtx, hawk_t* hawk, hawk_rio_cbs_t* rio)
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
	hawk_oow_t stack_limit, i;

	rtx->_gem = hawk->_gem;
	rtx->hawk = hawk;

	CLRERR (rtx);

	stack_limit = hawk->parse.pragma.rtx_stack_limit > 0? hawk->parse.pragma.rtx_stack_limit: hawk->opt.rtx_stack_limit;
	if (stack_limit < HAWK_MIN_RTX_STACK_LIMIT) stack_limit = HAWK_MIN_RTX_STACK_LIMIT;
	rtx->stack = hawk_rtx_allocmem(rtx, stack_limit * HAWK_SIZEOF(void*));
	if (HAWK_UNLIKELY(!rtx->stack)) goto oops_0;
	rtx->stack_top = 0;
	rtx->stack_base = 0;
	rtx->stack_limit = stack_limit;

	rtx->exit_level = EXIT_NONE;

	rtx->vmgr.ichunk = HAWK_NULL;
	rtx->vmgr.ifree = HAWK_NULL;
	rtx->vmgr.rchunk = HAWK_NULL;
	rtx->vmgr.rfree = HAWK_NULL;

	for (i = 0; i < HAWK_COUNTOF(rtx->gc.g); i++)
	{
		/* initialize circular doubly-linked list */
		rtx->gc.g[i].gc_next = &rtx->gc.g[i];
		rtx->gc.g[i].gc_prev = &rtx->gc.g[i];

		/* initialize some counters */
		rtx->gc.pressure[i] = 0;
		rtx->gc.threshold[i] = (HAWK_COUNTOF(rtx->gc.g) - i) * 10;
		if (i == 0 && rtx->gc.threshold[i] < 100) rtx->gc.threshold[i] = 100; /* minimum threshold for gen 0 is 100 */
	}

	rtx->gc.pressure[i] = 0; /* pressure is larger than other elements by 1 in size */

	rtx->inrec.buf_pos = 0;
	rtx->inrec.buf_len = 0;
	rtx->inrec.flds = HAWK_NULL;
	rtx->inrec.nflds = 0;
	rtx->inrec.maxflds = 0;
	rtx->inrec.d0 = hawk_val_nil;

	if (HAWK_UNLIKELY(hawk_ooecs_init(&rtx->inrec.line, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1)) goto oops_1;
	if (HAWK_UNLIKELY(hawk_ooecs_init(&rtx->inrec.linew, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1)) goto oops_2;
	if (HAWK_UNLIKELY(hawk_ooecs_init(&rtx->inrec.lineg, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1)) goto oops_3;
	if (HAWK_UNLIKELY(hawk_becs_init(&rtx->inrec.linegb, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1)) goto oops_4;
	if (HAWK_UNLIKELY(hawk_ooecs_init(&rtx->format.out, hawk_rtx_getgem(rtx), 256) <= -1)) goto oops_5;
	if (HAWK_UNLIKELY(hawk_ooecs_init(&rtx->format.fmt, hawk_rtx_getgem(rtx), 256) <= -1)) goto oops_6;

	if (HAWK_UNLIKELY(hawk_becs_init(&rtx->formatmbs.out, hawk_rtx_getgem(rtx), 256) <= -1)) goto oops_7;
	if (HAWK_UNLIKELY(hawk_becs_init(&rtx->formatmbs.fmt, hawk_rtx_getgem(rtx), 256) <= -1)) goto oops_8;

	if (HAWK_UNLIKELY(hawk_becs_init(&rtx->fnc.bout, hawk_rtx_getgem(rtx), 256) <= -1)) goto oops_9;
	if (HAWK_UNLIKELY(hawk_ooecs_init(&rtx->fnc.oout, hawk_rtx_getgem(rtx), 256) <= -1)) goto oops_10;


	rtx->named = hawk_htb_open(hawk_rtx_getgem(rtx), HAWK_SIZEOF(rtx), 1024, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	if (HAWK_UNLIKELY(!rtx->named)) goto oops_11;
	*(hawk_rtx_t**)hawk_htb_getxtn(rtx->named) = rtx;
	hawk_htb_setstyle (rtx->named, &style_for_named);

	rtx->format.tmp.ptr = (hawk_ooch_t*)hawk_rtx_allocmem(rtx, 4096 * HAWK_SIZEOF(hawk_ooch_t)); 
	if (HAWK_UNLIKELY(!rtx->format.tmp.ptr)) goto oops_12; /* the error is set on the hawk object after this jump is made */
	rtx->format.tmp.len = 4096;
	rtx->format.tmp.inc = 4096 * 2;

	rtx->formatmbs.tmp.ptr = (hawk_bch_t*)hawk_rtx_allocmem(rtx, 4096 * HAWK_SIZEOF(hawk_bch_t));
	if (HAWK_UNLIKELY(!rtx->formatmbs.tmp.ptr)) goto oops_13;
	rtx->formatmbs.tmp.len = 4096;
	rtx->formatmbs.tmp.inc = 4096 * 2;

	if (rtx->hawk->tree.chain_size > 0)
	{
		rtx->pattern_range_state = (hawk_oob_t*)hawk_rtx_allocmem(rtx, rtx->hawk->tree.chain_size * HAWK_SIZEOF(hawk_oob_t));
		if (HAWK_UNLIKELY(!rtx->pattern_range_state)) goto oops_14;
		HAWK_MEMSET (rtx->pattern_range_state, 0, rtx->hawk->tree.chain_size * HAWK_SIZEOF(hawk_oob_t));
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
	rtx->gbl.striprecspc = -1; /* means 'not set' */
	rtx->gbl.stripstrspc = -1; /* means 'not set' */

	return 0;

oops_14:
	hawk_rtx_freemem (rtx, rtx->formatmbs.tmp.ptr);
oops_13:
	hawk_rtx_freemem (rtx, rtx->format.tmp.ptr);
oops_12:
	hawk_htb_close (rtx->named);
oops_11:
	hawk_ooecs_fini (&rtx->fnc.oout);
oops_10:
	hawk_becs_fini (&rtx->fnc.bout);
oops_9:
	hawk_becs_fini (&rtx->formatmbs.fmt);
oops_8:
	hawk_becs_fini (&rtx->formatmbs.out);
oops_7:
	hawk_ooecs_fini (&rtx->format.fmt);
oops_6:
	hawk_ooecs_fini (&rtx->format.out);
oops_5:
	hawk_becs_fini (&rtx->inrec.linegb);
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
	if (rtx->forin.ptr)
	{
		while (rtx->forin.size > 0)
		{
			hawk_rtx_refdownval (rtx, rtx->forin.ptr[--rtx->forin.size]);
		}
		hawk_rtx_freemem (rtx, rtx->forin.ptr);
		rtx->forin.ptr = HAWK_NULL;
		rtx->forin.capa = 0;
	}

	if (rtx->pattern_range_state)
		hawk_rtx_freemem (rtx, rtx->pattern_range_state);

	/* close all pending io's */
	/* TODO: what if this operation fails? */
	hawk_rtx_clearallios (rtx);
	HAWK_ASSERT (rtx->rio.chain == HAWK_NULL);

	if (rtx->gbl.rs[0])
	{
		hawk_rtx_freerex (rtx, rtx->gbl.rs[0], rtx->gbl.rs[1]);
		rtx->gbl.rs[0] = HAWK_NULL;
		rtx->gbl.rs[1] = HAWK_NULL;
	}
	if (rtx->gbl.fs[0])
	{
		hawk_rtx_freerex (rtx, rtx->gbl.fs[0], rtx->gbl.fs[1]);
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

	hawk_ooecs_fini (&rtx->fnc.oout);
	hawk_becs_fini (&rtx->fnc.bout);

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
	hawk_becs_fini (&rtx->inrec.linegb);
	hawk_ooecs_fini (&rtx->inrec.lineg);
	hawk_ooecs_fini (&rtx->inrec.linew);
	hawk_ooecs_fini (&rtx->inrec.line);

	if (fini_globals) refdown_globals (rtx, 1);

	/* destroy the stack if necessary */
	if (rtx->stack)
	{
		HAWK_ASSERT (rtx->stack_top == 0);

		hawk_rtx_freemem (rtx, rtx->stack);
		rtx->stack = HAWK_NULL;
		rtx->stack_top = 0;
		rtx->stack_base = 0;
		rtx->stack_limit = 0;
	}

	/* destroy named variables */
	hawk_htb_close (rtx->named);

#if defined(HAWK_ENABLE_GC)
	/* collect garbage after having released global variables and named global variables */
	hawk_rtx_gc (rtx, HAWK_RTX_GC_GEN_FULL);
#endif

	/* destroy values in free list */
	while (rtx->rcache_count > 0)
	{
		hawk_val_ref_t* tmp = rtx->rcache[--rtx->rcache_count];
		hawk_rtx_freeval (rtx, (hawk_val_t*)tmp, 0);
	}

#if defined(HAWK_ENABLE_STR_CACHE)
	{
		int i;
		for (i = 0; i < HAWK_COUNTOF(rtx->str_cache_count); i++)
		{
			while (rtx->str_cache_count[i] > 0)
			{
				hawk_val_str_t* t = rtx->str_cache[i][--rtx->str_cache_count[i]];
				hawk_rtx_freeval (rtx, (hawk_val_t*)t, 0);
			}
		}
	}
#endif

#if defined(HAWK_ENABLE_MBS_CACHE)
	{
		int i;
		for (i = 0; i < HAWK_COUNTOF(rtx->mbs_cache_count); i++)
		{
			while (rtx->mbs_cache_count[i] > 0)
			{
				hawk_val_mbs_t* t = rtx->mbs_cache[i][--rtx->mbs_cache_count[i]];
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
	int n;

	tmp1 = hawk_rtx_makeintval(rtx, fnr);
	if (!tmp1) return -1;

	hawk_rtx_refupval (rtx, tmp1);

	if (nr == fnr) tmp2 = tmp1;
	else
	{
		tmp2 = hawk_rtx_makeintval(rtx, nr);
		if (HAWK_UNLIKELY(!tmp2)) { n = -1; goto oops; }
		hawk_rtx_refupval (rtx, tmp2);
	}

	n = (hawk_rtx_setgbl(rtx, HAWK_GBL_FNR, tmp1) <= -1 ||
	     hawk_rtx_setgbl(rtx, HAWK_GBL_NR, tmp2) <= -1)? -1: 0;

	if (tmp2 != tmp1) hawk_rtx_refdownval (rtx, tmp2);

oops:
	hawk_rtx_refdownval (rtx, tmp1);
	return n;
}

/* 
 * create global variables into the runtime stack
 * each variable is initialized to nil or zero.
 */
static int prepare_globals (hawk_rtx_t* rtx)
{
	hawk_oow_t saved_stack_top;
	hawk_oow_t ngbls;

	ngbls = rtx->hawk->tree.ngbls;

	if (HAWK_UNLIKELY(HAWK_RTX_STACK_AVAIL(rtx) < ngbls))
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ESTACK);
		return -1;
	}

	saved_stack_top = rtx->stack_top;

	/* initialize all global variables to nil by push nils to the stack */
	while (ngbls > 0)
	{
		--ngbls;
		HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil);
	}

	/* override NF to zero */
	if (hawk_rtx_setgbl(rtx, HAWK_GBL_NF, HAWK_VAL_ZERO) <= -1)
	{
		/* restore the stack_top this way instead of calling HAWK_RTX_STACK_POP()
		 * as many times as successful HAWK_RTX_STACK_PUSH(). it is ok because
		 * the values pushed so far are hawk_val_nils and HAWK_VAL_ZEROs.
		 */
		rtx->stack_top = saved_stack_top;
		return -1;
	}

	/* return success */
	return 0;
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
	static struct gtab_t gtab[9] =
	{
		{ HAWK_GBL_CONVFMT,    { DEFAULT_CONVFMT, DEFAULT_CONVFMT  } },
		{ HAWK_GBL_FILENAME,   { HAWK_NULL,       HAWK_NULL        } },
		{ HAWK_GBL_FS,         { DEFAULT_FS,      DEFAULT_FS       } },
		{ HAWK_GBL_OFILENAME,  { HAWK_NULL,       HAWK_NULL        } },
		{ HAWK_GBL_OFMT,       { DEFAULT_OFMT,    DEFAULT_OFMT     } },
		{ HAWK_GBL_OFS,        { DEFAULT_OFS,     DEFAULT_OFS      } },
		{ HAWK_GBL_ORS,        { DEFAULT_ORS,     DEFAULT_ORS_CRLF } },
		{ HAWK_GBL_SCRIPTNAME, { HAWK_NULL,       HAWK_NULL        } },
		{ HAWK_GBL_SUBSEP,     { DEFAULT_SUBSEP,  DEFAULT_SUBSEP   } }
	};

	hawk_val_t* tmp;
	hawk_oow_t i, j;
	int stridx;

	stridx = (rtx->hawk->opt.trait & HAWK_CRLF)? 1: 0;
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

		HAWK_ASSERT (HAWK_RTX_STACK_GBL(rtx,gtab[i].idx) == hawk_val_nil);

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

static void refdown_globals (hawk_rtx_t* rtx, int pop)
{
	hawk_oow_t ngbls;
       
	ngbls = rtx->hawk->tree.ngbls;
	while (ngbls > 0)
	{
		--ngbls;
		hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_GBL(rtx,ngbls));
		if (pop) HAWK_RTX_STACK_POP (rtx);
		else HAWK_RTX_STACK_GBL(rtx,ngbls) = hawk_val_nil;
	}
}

static int init_globals (hawk_rtx_t* rtx)
{
	/* the stack must be clean when this function is invoked */
	HAWK_ASSERT (rtx->stack_base == 0);
	HAWK_ASSERT (rtx->stack_top == 0); 

	if (prepare_globals(rtx) <= -1) return -1;
	if (update_fnr(rtx, 0, 0) <= -1 || defaultify_globals(rtx) <= -1) goto oops;
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
	data->val = HAWK_RTX_STACK_RETVAL(data->rtx);
	hawk_rtx_refupval (data->rtx, data->val);
}

static hawk_val_t* run_bpae_loop (hawk_rtx_t* rtx)
{
	hawk_nde_t* nde;
	hawk_oow_t nargs, i;
	hawk_val_t* retv;
	int ret = 0;

	/* set nargs to zero */
	nargs = 0;
	HAWK_RTX_STACK_NARGS(rtx) = (void*)nargs;

	/* execute the BEGIN block */
	for (nde = rtx->hawk->tree.begin; 
	     ret == 0 && nde != HAWK_NULL && rtx->exit_level < EXIT_GLOBAL;
	     nde = nde->next)
	{
		hawk_nde_blk_t* blk;

		blk = (hawk_nde_blk_t*)nde;
		HAWK_ASSERT (blk->type == HAWK_NDE_BLK);

		rtx->active_block = blk;
		rtx->exit_level = EXIT_NONE;
		if (run_block(rtx, blk) <= -1) ret = -1;
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
	    (rtx->hawk->tree.chain != HAWK_NULL ||
	     rtx->hawk->tree.end != HAWK_NULL) && 
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
	for (nde = rtx->hawk->tree.end;
	     ret == 0 && nde != HAWK_NULL && rtx->exit_level < EXIT_ABORT;
	     nde = nde->next) 
	{
		hawk_nde_blk_t* blk;

		blk = (hawk_nde_blk_t*)nde;
		HAWK_ASSERT (blk->type == HAWK_NDE_BLK);

		rtx->active_block = blk;
		rtx->exit_level = EXIT_NONE;
		if (run_block(rtx, blk) <= -1) ret = -1;
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
	nargs = (hawk_oow_t)HAWK_RTX_STACK_NARGS(rtx);
	HAWK_ASSERT (nargs == 0);
	for (i = 0; i < nargs; i++) 
		hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_ARG(rtx,i));

	/* get the return value in the current stack frame */
	retv = HAWK_RTX_STACK_RETVAL(rtx);

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
	hawk_oow_t saved_stack_top;

	rtx->exit_level = EXIT_NONE;

	/* make a new stack frame */

	if (HAWK_UNLIKELY(HAWK_RTX_STACK_AVAIL(rtx) < 4))
	{
		/* restore the stack top in a cheesy(?) way. 
		 * it is ok to do so as the values pushed are
		 * nils and binary numbers. */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ESTACK);
		return HAWK_NULL;
	}

	saved_stack_top = rtx->stack_top; 	/* remember the current stack top */
	HAWK_RTX_STACK_PUSH (rtx, (void*)rtx->stack_base); /* push the current stack base */
	HAWK_RTX_STACK_PUSH (rtx, (void*)saved_stack_top); /* push the current stack top before push the current stack base */
	HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil); /* secure space for a return value */
	HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil); /* secure space for HAWK_RTX_STACK_NARGS */

	/* enter the new stack frame */
	rtx->stack_base = saved_stack_top; /* let the stack top remembered be the base of a new stack frame */

	/* run the BEGIN/pattern-action/END loop */
	retv = run_bpae_loop(rtx);

	/* exit the stack frame */
	HAWK_ASSERT ((rtx->stack_top - rtx->stack_base) == 4); /* at this point, the current stack frame should have the 4 entries pushed above */
	rtx->stack_top = (hawk_oow_t)rtx->stack[rtx->stack_base + 1];
	rtx->stack_base = (hawk_oow_t)rtx->stack[rtx->stack_base + 0];

	/* reset the exit level */
	rtx->exit_level = EXIT_NONE;

	/* flush all buffered io data */
	hawk_rtx_flushallios (rtx);

	return retv;
}

hawk_val_t* hawk_rtx_execwithucstrarr (hawk_rtx_t* rtx, const hawk_uch_t* args[], hawk_oow_t nargs)
{
	hawk_val_t* v;

	v = (rtx->hawk->parse.pragma.entry[0] != '\0')?
		hawk_rtx_callwithooucstrarr(rtx, rtx->hawk->parse.pragma.entry, args, nargs):
		hawk_rtx_loop(rtx);

#if defined(HAWK_ENABLE_GC)
	/* i assume this function is a usual hawk program starter.
	 * call garbage collection after a whole program finishes */
	hawk_rtx_gc (rtx, HAWK_RTX_GC_GEN_FULL);
#endif

	return v;
}

hawk_val_t* hawk_rtx_execwithbcstrarr (hawk_rtx_t* rtx, const hawk_bch_t* args[], hawk_oow_t nargs)
{
	hawk_val_t* v;

	v = (rtx->hawk->parse.pragma.entry[0] != '\0')?
		hawk_rtx_callwithoobcstrarr(rtx, rtx->hawk->parse.pragma.entry, args, nargs):
		hawk_rtx_loop(rtx);

#if defined(HAWK_ENABLE_GC)
	/* i assume this function is a usual hawk program starter.
	 * call garbage collection after a whole program finishes */
	hawk_rtx_gc (rtx, HAWK_RTX_GC_GEN_FULL);
#endif

	return v;
}

/* find an AWK function by name */
static hawk_fun_t* find_fun (hawk_rtx_t* rtx, const hawk_ooch_t* name)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search(rtx->hawk->tree.funs, name, hawk_count_oocstr(name));
	if (!pair)
	{
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EFUNNF, HAWK_T("unable to find function '%js'"), name);
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

	HAWK_ASSERT (fun != HAWK_NULL);

	pafv.args = args;
	pafv.nargs = nargs;
	pafv.argspec = fun->argspec;

	if (HAWK_UNLIKELY(rtx->exit_level >= EXIT_GLOBAL))
	{
		/* cannot call the function again when exit() is called
		 * in an AWK program or hawk_rtx_halt() is invoked */
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EPERM, HAWK_T("now allowed to call '%.*js' after exit"), fun->name.len, fun->name.ptr);
		return HAWK_NULL;
	}
	/*rtx->exit_level = EXIT_NONE;*/

#if 0
	if (fun->argspec)
	{
		/* this function contains pass-by-reference parameters.
		 * i don't support the call here as it requires variables */
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EPERM, HAWK_T("not allowed to call '%.*js' with pass-by-reference parameters"), fun->name.len, fun->name.ptr);
		return HAWK_NULL;
	}
#endif

	/* forge a fake node containing a function call */
	HAWK_MEMSET (&call, 0, HAWK_SIZEOF(call));
	call.type = HAWK_NDE_FNCALL_FUN;
	call.u.fun.name = fun->name;
	call.nargs = nargs;
	/* keep HAWK_NULL in call.args so that hawk_rtx_evalcall() knows it's a fake call structure */

	/* check if the number of arguments given is more than expected */
	if (nargs > fun->nargs)
	{
		/* TODO: is this correct? what if i want to 
		 *       allow arbitrary numbers of arguments? */
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EARGTM, HAWK_T("too many arguments to '%.*js'"), fun->name.len, fun->name.ptr);
		return HAWK_NULL;
	}

	/* now that the function is found and ok, let's execute it */

	crdata.rtx = rtx;
	crdata.val = HAWK_NULL;

	v = hawk_rtx_evalcall(rtx, &call, fun, push_arg_from_vals, (void*)&pafv, capture_retval_on_exit, &crdata);

	if (HAWK_UNLIKELY(!v))
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

	/* flush all buffered io data */
	hawk_rtx_flushallios (rtx);

	/* return the return value with its reference count at least 1.
	 * the caller of this function should count down its reference. */
	return v;
}

/* call an AWK function by name */
hawk_val_t* hawk_rtx_callwithucstr (hawk_rtx_t* rtx, const hawk_uch_t* name, hawk_val_t* args[], hawk_oow_t nargs)
{
	hawk_fun_t* fun;

	fun = hawk_rtx_findfunwithucstr(rtx, name);
	if (HAWK_UNLIKELY(!fun)) return HAWK_NULL;

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
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	for (i = 0; i < nargs; i++)
	{
		v[i] = hawk_rtx_makestrvalwithucstr(rtx, args[i]);
		if (HAWK_UNLIKELY(!v[i]))
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
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	for (i = 0; i < nargs; i++)
	{
		v[i] = hawk_rtx_makestrvalwithbcstr(rtx, args[i]);
		if (HAWK_UNLIKELY(!v[i]))
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

hawk_val_t* hawk_rtx_callwithooucstrarr (hawk_rtx_t* rtx, const hawk_ooch_t* name, const hawk_uch_t* args[], hawk_oow_t nargs)
{
	hawk_oow_t i;
	hawk_val_t** v, * ret;

	v = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*v) * nargs);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	for (i = 0; i < nargs; i++)
	{
		v[i] = hawk_rtx_makestrvalwithucstr(rtx, args[i]);
		if (HAWK_UNLIKELY(!v[i]))
		{
			ret = HAWK_NULL;
			goto oops;
		}

		hawk_rtx_refupval (rtx, v[i]);
	}

	ret = hawk_rtx_callwithoocstr(rtx, name, v, nargs);

oops:
	while (i > 0) 
	{
		hawk_rtx_refdownval (rtx, v[--i]);
	}
	hawk_rtx_freemem (rtx, v);
	return ret;
}

hawk_val_t* hawk_rtx_callwithoobcstrarr (hawk_rtx_t* rtx, const hawk_ooch_t* name, const hawk_bch_t* args[], hawk_oow_t nargs)
{
	hawk_oow_t i;
	hawk_val_t** v, * ret;

	v = hawk_rtx_allocmem(rtx, HAWK_SIZEOF(*v) * nargs);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	for (i = 0; i < nargs; i++)
	{
		v[i] = hawk_rtx_makestrvalwithbcstr(rtx, args[i]);
		if (HAWK_UNLIKELY(!v[i]))
		{
			ret = HAWK_NULL;
			goto oops;
		}

		hawk_rtx_refupval (rtx, v[i]);
	}

	ret = hawk_rtx_callwithoocstr(rtx, name, v, nargs);

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
	if (rtx->hawk->tree.chain != HAWK_NULL) \
	{ \
		if (rtx->hawk->tree.chain->pattern != HAWK_NULL) \
			ADJERR_LOC (rtx, &rtx->hawk->tree.chain->pattern->loc); \
		else if (rtx->hawk->tree.chain->action != HAWK_NULL) \
			ADJERR_LOC (rtx, &rtx->hawk->tree.chain->action->loc); \
	} \
	else if (rtx->hawk->tree.end != HAWK_NULL) \
	{ \
		ADJERR_LOC (run, &rtx->hawk->tree.end->loc); \
	} 

	rtx->inrec.buf_pos = 0;
	rtx->inrec.buf_len = 0;
	rtx->inrec.eof = 0;

	/* run each pattern block */
	while (rtx->exit_level < EXIT_GLOBAL)
	{
		rtx->exit_level = EXIT_NONE;

		n = read_record(rtx);
		if (n <= -1) 
		{
			ADJUST_ERROR (rtx);
			return -1; /* error */
		}
		if (n == 0) break; /* end of input */

		if (rtx->hawk->tree.chain)
		{
			if (run_pblock_chain(rtx, rtx->hawk->tree.chain) <= -1) return -1;
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
			if (HAWK_UNLIKELY(!v1)) return -1;

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
			HAWK_ASSERT (ptn->next->next == HAWK_NULL);
			HAWK_ASSERT (rtx->pattern_range_state != HAWK_NULL);

			if (rtx->pattern_range_state[bno] == 0)
			{
				hawk_val_t* v1;

				v1 = eval_expression(rtx, ptn);
				if (HAWK_UNLIKELY(!v1)) return -1;
				hawk_rtx_refupval (rtx, v1);
				rtx->pattern_range_state[bno] = hawk_rtx_valtobool(rtx, v1);
				hawk_rtx_refdownval (rtx, v1);
			}

			if (rtx->pattern_range_state[bno] == 1)
			{
				hawk_val_t* v2;

				v2 = eval_expression(rtx, ptn->next);
				if (HAWK_UNLIKELY(!v2)) return -1;
				hawk_rtx_refupval (rtx, v2);
				rtx->pattern_range_state[bno] = !hawk_rtx_valtobool(rtx, v2);
				hawk_rtx_refdownval (rtx, v2);

				rtx->active_block = blk;
				if (run_block(rtx, blk) <= -1) return -1;
			}
		}
	}

	return 0;
}

static HAWK_INLINE int run_block0 (hawk_rtx_t* rtx, hawk_nde_blk_t* nde)
{
	hawk_nde_t* p;
	/*hawk_oow_t saved_stack_top;*/
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

	HAWK_ASSERT (nde->type == HAWK_NDE_BLK);
	/*saved_stack_top = rtx->stack_top;*/

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("securing space for local variables nlcls = %d\n"), (int)nde->nlcls);
#endif

	if (nde->nlcls > 0)
	{
		hawk_oow_t tmp = nde->nlcls;

		/* ensure sufficient space for local variables */
		if (HAWK_UNLIKELY(HAWK_RTX_STACK_AVAIL(rtx) < tmp))
		{
			hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ESTACK);
			return -1;
		}
	
		do
		{
			--tmp;
			HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil);
			/* refupval is not required for hawk_val_nil */
		}
		while (tmp > 0);
	}
	else if (nde->nlcls != nde->org_nlcls)
	{
		/* this part can be reached if:
		 *   - the block is nested
		 *   - the block declares local variables
		 *   - the local variables have been migrated to the outer-most block by the parser.
		 *
		 * if the parser skips migrations, this part isn't reached but the normal push/pop
		 * will take place for a nested block with local variable declaration.
		 */
		hawk_oow_t tmp, end;

		/* when the outer-most block has been entered, the space large enough for all local 
		 * variables defined has been secured. nullify part of the stack to initialze local 
		 * variables defined for a nested block */
		end = nde->outer_nlcls + nde->org_nlcls;
		for (tmp = nde->outer_nlcls; tmp < end; tmp++)
		{
			hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_LCL(rtx, tmp));
			HAWK_RTX_STACK_LCL(rtx, tmp) = hawk_val_nil;
		}
	}

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("executing block statements\n"));
#endif

	p = nde->body;
	while (p && rtx->exit_level == EXIT_NONE)
	{
		if (HAWK_UNLIKELY(run_statement(rtx, p) <= -1))
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

	if (nde->nlcls > 0)
	{
		hawk_oow_t tmp = nde->nlcls;

		do
		{
			--tmp;
			hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_LCL(rtx, tmp));
			HAWK_RTX_STACK_POP (rtx);
		}
		while (tmp > 0);
	}

	return n;
}

static int run_block (hawk_rtx_t* rtx, hawk_nde_blk_t* nde)
{
	int n;

	if (rtx->hawk->opt.depth.s.block_run > 0 &&
	    rtx->depth.block >= rtx->hawk->opt.depth.s.block_run)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EBLKNST);
		return -1;;
	}

	rtx->depth.block++;
	n = run_block0(rtx, nde);
	rtx->depth.block--;

	return n;
}

#define ON_STATEMENT(rtx,nde) do { \
	hawk_rtx_ecb_t* ecb; \
	if ((rtx)->hawk->haltall) (rtx)->exit_level = EXIT_ABORT; \
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

		case HAWK_NDE_FORIN:
			xret = run_forin(rtx, (hawk_nde_forin_t*)nde);
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
			if (rtx->hawk->opt.trait & HAWK_TOLERANT) goto __fallback__;
			xret = run_print(rtx, (hawk_nde_print_t*)nde);
			break;

		case HAWK_NDE_PRINTF:
			if (rtx->hawk->opt.trait & HAWK_TOLERANT) goto __fallback__;
			xret = run_printf(rtx, (hawk_nde_print_t*)nde);
			break;

		default:
		__fallback__:
			tmp = eval_expression(rtx, nde);
			if (HAWK_UNLIKELY(!tmp)) xret = -1;
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
	HAWK_ASSERT (nde->test->next == HAWK_NULL);

	test = eval_expression(rtx, nde->test);
	if (HAWK_UNLIKELY(!test)) return -1;

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
		HAWK_ASSERT (nde->test->next == HAWK_NULL);

		while (1)
		{
			ON_STATEMENT (rtx, nde->test);

			test = eval_expression(rtx, nde->test);
			if (HAWK_UNLIKELY(!test)) return -1;

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
		HAWK_ASSERT (nde->test->next == HAWK_NULL);

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
			if (HAWK_UNLIKELY(!test)) return -1;

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

	if (nde->init)
	{
		HAWK_ASSERT (nde->init->next == HAWK_NULL);

		ON_STATEMENT (rtx, nde->init);
		val = eval_expression(rtx,nde->init);
		if (HAWK_UNLIKELY(!val)) return -1;

		hawk_rtx_refupval (rtx, val);
		hawk_rtx_refdownval (rtx, val);
	}

	while (1)
	{
		if (nde->test)
		{
			hawk_val_t* test;

			/* no chained expressions for the test expression of
			 * the for statement are allowed */
			HAWK_ASSERT (nde->test->next == HAWK_NULL);

			ON_STATEMENT (rtx, nde->test);
			test = eval_expression(rtx, nde->test);
			if (HAWK_UNLIKELY(!test)) return -1;

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
		}	
		else
		{
			if (run_statement(rtx,nde->body) <= -1) return -1;
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
			HAWK_ASSERT (nde->incr->next == HAWK_NULL);

			ON_STATEMENT (rtx, nde->incr);
			val = eval_expression(rtx, nde->incr);
			if (HAWK_UNLIKELY(!val)) return -1;

			hawk_rtx_refupval (rtx, val);
			hawk_rtx_refdownval (rtx, val);
		}
	}

	return 0;
}

static int run_forin (hawk_rtx_t* rtx, hawk_nde_forin_t* nde)
{
	hawk_nde_exp_t* test;
	hawk_val_t* rv;
	hawk_val_type_t rvtype;
	
	int ret;
	
	test = (hawk_nde_exp_t*)nde->test;
	HAWK_ASSERT (test->type == HAWK_NDE_EXP_BIN && test->opcode == HAWK_BINOP_IN);

	/* chained expressions should not be allowed by the parser first of all */
	HAWK_ASSERT (test->right->next == HAWK_NULL); 

	rv = eval_expression(rtx, test->right);
	if (HAWK_UNLIKELY(!rv)) return -1;

	ret = 0;

	hawk_rtx_refupval (rtx, rv);
	rvtype = HAWK_RTX_GETVALTYPE(rtx, rv);
	switch (rvtype)
	{
		case HAWK_VAL_NIL:
			/* just return without excuting the loop body */
			goto done1;

		case HAWK_VAL_MAP:
		{
			hawk_map_t* map;
			hawk_map_pair_t* pair;
			hawk_map_itr_t itr;
			hawk_oow_t old_forin_size, i;

			map = ((hawk_val_map_t*)rv)->map;

			old_forin_size = rtx->forin.size;
			if (rtx->forin.capa - rtx->forin.size < hawk_map_getsize(map))
			{
				hawk_val_t** tmp;
				hawk_oow_t newcapa;

				newcapa = rtx->forin.size + hawk_map_getsize(map);
				newcapa = HAWK_ALIGN_POW2(newcapa, 128);
				tmp = hawk_rtx_reallocmem(rtx, rtx->forin.ptr, newcapa * HAWK_SIZEOF(*tmp));
				if (HAWK_UNLIKELY(!tmp)) 
				{
					ADJERR_LOC (rtx, &test->left->loc);
					ret = -1;
					goto done2;
				}

				rtx->forin.ptr = tmp;
				rtx->forin.capa = newcapa;
			}

			/* take a snapshot of the keys first so that the actual pairs become mutation safe
			 * this proctection is needed in case the body contains destructive statements like 'delete' or '@reset'*/
			hawk_init_map_itr (&itr, 0);
			pair = hawk_map_getfirstpair(map, &itr);
			while (pair)
			{
				hawk_val_t* str;

				str = (hawk_val_t*)hawk_rtx_makenstrvalwithoochars(rtx, HAWK_HTB_KPTR(pair), HAWK_HTB_KLEN(pair));
				if (HAWK_UNLIKELY(!str)) 
				{
					ADJERR_LOC (rtx, &test->left->loc);
					ret = -1;
					goto done2;
				}

				rtx->forin.ptr[rtx->forin.size++] = str;
				hawk_rtx_refupval (rtx, str);

				pair = hawk_map_getnextpair(map, &itr);
			}

			/* iterate over the keys in the snapshot */
			for (i = old_forin_size;  i < rtx->forin.size; i++)
			{
				if (HAWK_UNLIKELY(!do_assignment(rtx, test->left, rtx->forin.ptr[i])) || HAWK_UNLIKELY(run_statement(rtx, nde->body) <= -1))
				{
					ret = -1;
					goto done2;
				}

				if (rtx->exit_level == EXIT_BREAK)
				{
					rtx->exit_level = EXIT_NONE;
					goto done2;
				}
				else if (rtx->exit_level == EXIT_CONTINUE)
				{
					rtx->exit_level = EXIT_NONE;
				}
				else if (rtx->exit_level != EXIT_NONE) 
				{
					goto done2;
				}
			}

		done2:
			while (rtx->forin.size > old_forin_size)
				hawk_rtx_refdownval (rtx, rtx->forin.ptr[--rtx->forin.size]);

			break;
		}

		case HAWK_VAL_ARR:
		{
			hawk_arr_t* arr;
			hawk_oow_t arr_size;
			hawk_oow_t old_forin_size, i;

			arr = ((hawk_val_arr_t*)rv)->arr;
			arr_size = HAWK_ARR_SIZE(arr);

			old_forin_size = rtx->forin.size;
			if (rtx->forin.capa - rtx->forin.size < hawk_arr_getsize(arr))
			{
				hawk_val_t** tmp;
				hawk_oow_t newcapa;

				newcapa = rtx->forin.size + hawk_arr_getsize(arr);
				newcapa = HAWK_ALIGN_POW2(newcapa, 128);
				tmp = hawk_rtx_reallocmem(rtx, rtx->forin.ptr, newcapa * HAWK_SIZEOF(*tmp));
				if (HAWK_UNLIKELY(!tmp)) 
				{
					ADJERR_LOC (rtx, &test->left->loc);
					ret = -1;
					goto done3;
				}

				rtx->forin.ptr = tmp;
				rtx->forin.capa = newcapa;
			}

			/* take a snapshot of the keys first so that the actual pairs become mutation safe
			 * this proctection is needed in case the body contains destructive statements like 'delete' or '@reset'.
			 * besides, there can be empty slots. after a[1] = 20; a[9] = 30;,  a[2], a[3], etc are empty. */
			for  (i = 0; i < arr_size; i++)
			{
				if (HAWK_ARR_SLOT(arr, i))
				{
					hawk_val_t* tmp;

					tmp = (hawk_val_t*)hawk_rtx_makeintval(rtx, i);
					if (HAWK_UNLIKELY(!tmp)) 
					{
						ADJERR_LOC (rtx, &test->left->loc);
						ret = -1;
						goto done3;
					}

					rtx->forin.ptr[rtx->forin.size++] = tmp;
					hawk_rtx_refupval (rtx, tmp);
				}
			}

			/* iterate over the keys in the snapshot */
			for (i = old_forin_size;  i < rtx->forin.size; i++)
			{
				if (HAWK_UNLIKELY(!do_assignment(rtx, test->left, rtx->forin.ptr[i])) || HAWK_UNLIKELY(run_statement(rtx, nde->body) <= -1))
				{
					ret = -1;
					goto done3;
				}

				if (rtx->exit_level == EXIT_BREAK)
				{
					rtx->exit_level = EXIT_NONE;
					goto done3;
				}
				else if (rtx->exit_level == EXIT_CONTINUE)
				{
					rtx->exit_level = EXIT_NONE;
				}
				else if (rtx->exit_level != EXIT_NONE) 
				{
					goto done3;
				}
			}

		done3:
			while (rtx->forin.size > old_forin_size)
				hawk_rtx_refdownval (rtx, rtx->forin.ptr[--rtx->forin.size]);

			break;
		}

		default:
			hawk_rtx_seterrnum (rtx, &test->right->loc, HAWK_EINROP);
			ret = -1;
			goto done1;
	}


done1:
	hawk_rtx_refdownval (rtx, rv);
	return ret;
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
	if (nde->val)
	{
		hawk_val_t* val;

		/* chained expressions should not be allowed 
		 * by the parser first of all */
		HAWK_ASSERT (nde->val->next == HAWK_NULL); 

		val = eval_expression(rtx, nde->val);
		if (HAWK_UNLIKELY(!val)) return -1;

		if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
		{
			hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, val);

			if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
			{
				/* cannot return a map */
				hawk_rtx_refupval (rtx, val);
				hawk_rtx_refdownval (rtx, val);
				hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ENONSCARET);
				return -1;
			}
		}

		hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_RETVAL(rtx));
		HAWK_RTX_STACK_RETVAL(rtx) = val;

		/* NOTE: see eval_call() for the trick */
		hawk_rtx_refupval (rtx, val); 
	}
	
	rtx->exit_level = EXIT_FUNCTION;
	return 0;
}

static int run_exit (hawk_rtx_t* rtx, hawk_nde_exit_t* nde)
{
	if (nde->val)
	{
		hawk_val_t* val;

		/* chained expressions should not be allowed 
		 * by the parser first of all */
		HAWK_ASSERT (nde->val->next == HAWK_NULL); 

		val = eval_expression(rtx, nde->val);
		if (HAWK_UNLIKELY(!val)) return -1;

		hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_RETVAL_GBL(rtx));
		HAWK_RTX_STACK_RETVAL_GBL(rtx) = val; /* global return value */

		hawk_rtx_refupval (rtx, val);
	}

	rtx->exit_level = (nde->abort)? EXIT_ABORT: EXIT_GLOBAL;
	return 0;
}

static int run_next (hawk_rtx_t* rtx, hawk_nde_next_t* nde)
{
	/* the parser checks if next has been called in the begin/end
	 * block or whereever inappropriate. so the rtxtime doesn't 
	 * check that explicitly */
	if  (rtx->active_block == (hawk_nde_blk_t*)rtx->hawk->tree.begin)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ERNEXTBEG);
		return -1;
	}
	else if (rtx->active_block == (hawk_nde_blk_t*)rtx->hawk->tree.end)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ERNEXTEND);
		return -1;
	}

	rtx->exit_level = EXIT_NEXT;
	return 0;
}

static int run_nextinfile (hawk_rtx_t* rtx, hawk_nde_nextfile_t* nde)
{
	int n;

	/* normal nextfile statement */
	if  (rtx->active_block == (hawk_nde_blk_t*)rtx->hawk->tree.begin)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ERNEXTFBEG);
		return -1;
	}
	else if (rtx->active_block == (hawk_nde_blk_t*)rtx->hawk->tree.end)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ERNEXTFEND);
		return -1;
	}

	n = hawk_rtx_nextio_read(rtx, HAWK_IN_CONSOLE, HAWK_T(""));
	if (n <= -1)
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
	if (update_fnr(rtx, 0, rtx->gbl.nr) <= -1) 
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
	if (n <= -1)
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
	return (nde->out)? run_nextoutfile(rtx, nde): run_nextinfile(rtx, nde);
}


static hawk_val_t* assign_newmapval_in_map (hawk_rtx_t* rtx, hawk_map_t* container, hawk_ooch_t* idxptr, hawk_oow_t idxlen)
{
	hawk_val_t* tmp;
	hawk_map_pair_t* pair;

	tmp = hawk_rtx_makemapval(rtx);
	if (HAWK_UNLIKELY(!tmp)) return HAWK_NULL;

	/* as this is the assignment, it needs to update the reference count of the target value. */
	hawk_rtx_refupval (rtx, tmp);
	pair = hawk_map_upsert(container, idxptr, idxlen, tmp, 0);
	if (HAWK_UNLIKELY(!pair)) 
	{
		hawk_rtx_refdownval (rtx, tmp);  /* decrement upon upsert() failure */
		return HAWK_NULL;
	}

	HAWK_ASSERT (tmp == HAWK_MAP_VPTR(pair));
	return tmp;
}

static hawk_val_t* assign_newarrval_in_arr (hawk_rtx_t* rtx, hawk_arr_t* container, hawk_ooi_t idx)
{
	hawk_val_t* tmp;

	tmp = hawk_rtx_makearrval(rtx, -1);
	if (HAWK_UNLIKELY(!tmp)) return HAWK_NULL;

	/* as this is the assignment, it needs to update the reference count of the target value. */
	hawk_rtx_refupval (rtx, tmp);
	if (HAWK_UNLIKELY(hawk_arr_upsert(container, idx, tmp, 0) == HAWK_ARR_NIL))
	{
		hawk_rtx_refdownval (rtx, tmp);  /* decrement upon upsert() failure */
		return HAWK_NULL;
	}
	return tmp;
}

static hawk_val_t* fetch_topval_from_var (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	hawk_val_t* val;

	switch (var->type)
	{

		case HAWK_NDE_NAMED:
		case HAWK_NDE_NAMEDIDX:
		{
			hawk_htb_pair_t* pair;
			pair = hawk_htb_search(rtx->named, var->id.name.ptr, var->id.name.len);
			val = pair? (hawk_val_t*)HAWK_HTB_VPTR(pair): hawk_val_nil;
			break;
		}

		case HAWK_NDE_GBL:
		case HAWK_NDE_GBLIDX:
			val = HAWK_RTX_STACK_GBL(rtx, var->id.idxa);
			break;

		case HAWK_NDE_LCL:
		case HAWK_NDE_LCLIDX:
			val = HAWK_RTX_STACK_LCL(rtx, var->id.idxa);
			break;

		default:
			HAWK_ASSERT (var->type == HAWK_NDE_ARG || var->type == HAWK_NDE_ARGIDX);
			val = HAWK_RTX_STACK_ARG(rtx, var->id.idxa);
			break;
	}

	return val;
}

static hawk_val_t* assign_topval_to_var (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val)
{
	switch (var->type)
	{
		case HAWK_NDE_NAMED:
		case HAWK_NDE_NAMEDIDX:
		{
			/* doesn't have to decrease the reference count 
			 * of the previous value here as it is done by 
			 * hawk_htb_upsert */
			hawk_rtx_refupval (rtx, val);
			if (HAWK_UNLIKELY(hawk_htb_upsert(rtx->named, var->id.name.ptr, var->id.name.len, val, 0) == HAWK_NULL))
			{
				hawk_rtx_refdownval (rtx, val);
				ADJERR_LOC (rtx, &var->loc);
				return HAWK_NULL;
			}
			break;
		}

		case HAWK_NDE_GBL:
		case HAWK_NDE_GBLIDX:
		{
			int x;

			hawk_rtx_refupval (rtx, val);
			x = hawk_rtx_setgbl(rtx, (int)var->id.idxa, val);
			hawk_rtx_refdownval (rtx, val);
			if (HAWK_UNLIKELY(x <= -1)) 
			{
				ADJERR_LOC (rtx, &var->loc);
				return HAWK_NULL;
			}
			break;
		}

		case HAWK_NDE_LCL:
		case HAWK_NDE_LCLIDX:
		{
			hawk_val_t* old;
			old = HAWK_RTX_STACK_LCL(rtx,var->id.idxa);
			hawk_rtx_refdownval (rtx, old);
			HAWK_RTX_STACK_LCL(rtx,var->id.idxa) = val;
			hawk_rtx_refupval (rtx, val);
			break;
		}

		default: /* HAWK_NDE_ARG, HAWK_NDE_ARGIDX */
		{
			hawk_val_t* old;
			old = HAWK_RTX_STACK_ARG(rtx,var->id.idxa);
			hawk_rtx_refdownval (rtx, old);
			HAWK_RTX_STACK_ARG(rtx,var->id.idxa) = val;
			hawk_rtx_refupval (rtx, val);
			break;
		}
	}

	return val;
}

static hawk_val_t* assign_newmapval_to_var (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	hawk_val_t* tmp;

	HAWK_ASSERT (var->type >= HAWK_NDE_NAMED && var->type <= HAWK_NDE_ARGIDX);

	tmp = hawk_rtx_makemapval(rtx);
	if (HAWK_UNLIKELY(!tmp)) 
	{
		ADJERR_LOC (rtx, &var->loc);
		return HAWK_NULL;
	}

	return assign_topval_to_var(rtx, var, tmp);
}


static HAWK_INLINE int delete_indexed (hawk_rtx_t* rtx, hawk_val_t* vv, hawk_nde_var_t* var)
{
	hawk_map_t* map;
	hawk_ooch_t* str = HAWK_NULL;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[HAWK_IDX_BUF_SIZE];

	hawk_arr_t* arr;
	hawk_ooi_t idx;

	hawk_nde_t* remidx;
	hawk_val_type_t vtype;

	HAWK_ASSERT (var->idx != HAWK_NULL);

	/* delete x["abc"];
	 * delete x[20,"abc"]; */

	vtype = HAWK_RTX_GETVALTYPE(rtx, vv);
	if (vtype == HAWK_VAL_MAP)
	{
		len = HAWK_COUNTOF(idxbuf);
		str = idxnde_to_str(rtx, var->idx, idxbuf, &len, &remidx, HAWK_NULL);
		if (HAWK_UNLIKELY(!str)) goto oops;
		map = ((hawk_val_map_t*)vv)->map;
	}
	else
	{
		HAWK_ASSERT (vtype == HAWK_VAL_ARR);
		idx = idxnde_to_int(rtx, var->idx, &remidx);
		if (HAWK_UNLIKELY(idx <= -1)) goto oops;
		arr = ((hawk_val_arr_t*)vv)->arr;
	}

#if defined(HAWK_ENABLE_GC)
	while (remidx)
	{
		hawk_val_type_t container_vtype;

		if (vtype == HAWK_VAL_MAP)
		{
			hawk_map_pair_t* pair;
			pair = hawk_map_search(map, str, len);
			vv = pair? (hawk_val_t*)HAWK_MAP_VPTR(pair): hawk_val_nil;
		}
		else
		{
			vv = (idx < HAWK_ARR_SIZE(arr) && HAWK_ARR_SLOT(arr, idx))? ((hawk_val_t*)HAWK_ARR_DPTR(arr, idx)): hawk_val_nil;
		}
		container_vtype = vtype;
		vtype = HAWK_RTX_GETVALTYPE(rtx, vv);

		switch (vtype)
		{
			case HAWK_VAL_MAP:
			val_map:
				if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
				len = HAWK_COUNTOF(idxbuf);
				str = idxnde_to_str(rtx, remidx, idxbuf, &len, &remidx, HAWK_NULL);
				if (HAWK_UNLIKELY(!str)) goto oops;
				map = ((hawk_val_map_t*)vv)->map;
				break;

			case HAWK_VAL_ARR:
			val_arr:
				idx = idxnde_to_int(rtx, remidx, &remidx);
				if (HAWK_UNLIKELY(idx <= -1)) goto oops;
				arr = ((hawk_val_arr_t*)vv)->arr;
				break;

			default:
				if (vtype == HAWK_VAL_NIL || (rtx->hawk->opt.trait & HAWK_FLEXMAP))
				{
					if (container_vtype == HAWK_VAL_MAP)
					{
						vv = assign_newmapval_in_map(rtx, map, str, len);
						if (HAWK_UNLIKELY(!vv)) { ADJERR_LOC(rtx, &var->loc); goto oops; }
						vtype = HAWK_VAL_MAP;
						goto val_map;
					}
					else
					{
						vv = assign_newarrval_in_arr(rtx, arr, idx);
						if (HAWK_UNLIKELY(!vv)) { ADJERR_LOC(rtx, &var->loc); goto oops; }
						vtype = HAWK_VAL_ARR;
						goto val_arr;
					}
				}
				else
				{
					hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENOTDEL, HAWK_T("nested scalar value under '%.*js' not deletable"), var->id.name.len, var->id.name.ptr);
					goto oops;
				}
		}
	}
#endif

	if (vtype == HAWK_VAL_MAP)
		hawk_map_delete (map, str, len);
	else
		hawk_arr_uplete (arr, idx,  1); /* no reindexing by compaction. keep the place unset */

	if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
	return 0;

oops:
	if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
	return -1;
}

static int run_delete (hawk_rtx_t* rtx, hawk_nde_delete_t* nde)
{
	hawk_nde_var_t* var;
	hawk_val_t* val;
	hawk_val_type_t vtype;

	var = (hawk_nde_var_t*)nde->var;

	val = fetch_topval_from_var(rtx, var);
	HAWK_ASSERT (val != HAWK_NULL);

	vtype = HAWK_RTX_GETVALTYPE(rtx, val);
	switch (vtype)
	{
		case HAWK_VAL_NIL:
			/* value not set. create a map and assign it to the variable */
			if (HAWK_UNLIKELY(assign_newmapval_to_var(rtx, var) == HAWK_NULL)) goto oops;
			break;

		case HAWK_VAL_MAP:
			if (var->type == HAWK_NDE_NAMEDIDX || var->type == HAWK_NDE_GBLIDX ||
			    var->type == HAWK_NDE_LCLIDX || var->type == HAWK_NDE_ARGIDX)
			{
				if (delete_indexed(rtx, val, var) <= -1) goto oops;
			}
			else
			{
				/*
				BEGIN {
				  @local a;
				  a[1] = 20; a[2] = "hello"; a[3][4][5]="ttt"; 
				  print typename(a), length(a); 
				  delete a; 
				  print typename(a), length(a); 
				}
				*/
				hawk_map_clear (((hawk_val_map_t*)val)->map);
			}
			break;

		case HAWK_VAL_ARR:
			if (var->type == HAWK_NDE_NAMEDIDX || var->type == HAWK_NDE_GBLIDX ||
			    var->type == HAWK_NDE_LCLIDX || var->type == HAWK_NDE_ARGIDX)
			{
				if (delete_indexed(rtx, val, var) <= -1) goto oops;
			}
			else
			{
				hawk_arr_clear (((hawk_val_arr_t*)val)->arr);
			}
			break;

		default:
			hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENOTDEL, HAWK_T("'%.*js' not deletable"), var->id.name.len, var->id.name.ptr);
			goto oops;
	}

	return 0;

oops:
	ADJERR_LOC (rtx, &var->loc);
	return -1;

}

static int run_reset (hawk_rtx_t* rtx, hawk_nde_reset_t* nde)
{
	hawk_nde_var_t* var;

	var = (hawk_nde_var_t*)nde->var;

	HAWK_ASSERT (var->type >= HAWK_NDE_NAMED && var->type <= HAWK_NDE_ARG);

	switch (var->type)
	{
		case HAWK_NDE_NAMED:
			/* if a named variable has an index part, something is definitely wrong */
			HAWK_ASSERT (var->idx == HAWK_NULL);

			/* a named variable can be reset if removed from the internal map to manage it */
			hawk_htb_delete (rtx->named, var->id.name.ptr, var->id.name.len);
			return 0;

		case HAWK_NDE_GBL:
		case HAWK_NDE_LCL:
		case HAWK_NDE_ARG:
		{
			hawk_val_t* val;

			HAWK_ASSERT (var->idx == HAWK_NULL);

			val = fetch_topval_from_var(rtx, var);
			HAWK_ASSERT (val != HAWK_NULL);

			if (HAWK_RTX_GETVALTYPE(rtx, val) != HAWK_VAL_NIL)
			{
				assign_topval_to_var(rtx, var, hawk_val_nil); /* this must not fail */
			}
			return 0;
		}

		default:
			/* the reset statement can only be called with plain variables */
			HAWK_ASSERT (!"should never happen - wrong target for @reset");
			hawk_rtx_seterrnum (rtx, &var->loc, HAWK_EBADARG);
			return -1;
	}
}

static hawk_val_t* io_nde_to_str(hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_oocs_t* dst, int seterr)
{
	hawk_val_t* v;

	v = eval_expression(rtx, nde);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, v);
	dst->ptr = hawk_rtx_getvaloocstr(rtx, v, &dst->len);
	if (HAWK_UNLIKELY(!dst)) 
	{
		hawk_rtx_refdownval (rtx, v);
		return HAWK_NULL;
	}

	if (seterr && dst->len <= 0)
	{
		hawk_rtx_seterrfmt (rtx, &nde->loc, HAWK_EIONMEM, HAWK_T("empty I/O name"));
	}
	else
	{
		hawk_oow_t len;

		len = dst->len;
		while (len > 0)
		{
			if (dst->ptr[--len] == '\0')
			{
				hawk_rtx_seterrfmt (rtx, &nde->loc, HAWK_EIONMNL, HAWK_T("invalid I/O name of length %zu containing '\\0'"), dst->len); 
				dst->len = 0; /* indicate that the name is not valid */
				break;
			}
		}
	}

	return v;
}

static int run_print (hawk_rtx_t* rtx, hawk_nde_print_t* nde)
{
	hawk_oocs_t out;
	hawk_val_t* out_v = HAWK_NULL;
	int n, xret = 0;

	HAWK_ASSERT (
		(nde->out_type == HAWK_OUT_PIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_RWPIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_FILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_APFILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_CONSOLE && nde->out == HAWK_NULL));

	/* check if destination has been specified. */
	if (nde->out)
	{
		out_v = io_nde_to_str(rtx, nde->out, &out, 1);
		if (!out_v || out.len <= 0) goto oops_1;
	}
	else
	{
		out.ptr = (hawk_ooch_t*)HAWK_T("");
		out.len = 0;
	}

	/* check if print is followed by any arguments */
	if (!nde->args)
	{
		/* if it doesn't have any arguments, print the entire input record */
		n = hawk_rtx_writeiostr(rtx, nde->out_type, out.ptr, HAWK_OOECS_PTR(&rtx->inrec.line), HAWK_OOECS_LEN(&rtx->inrec.line));
		if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/)
		{
			if (rtx->hawk->opt.trait & HAWK_TOLERANT)
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
			HAWK_ASSERT (nde->args->next == HAWK_NULL);
			head = ((hawk_nde_grp_t*)nde->args)->body;
		}
		else head = nde->args;

		for (np = head; np != HAWK_NULL; np = np->next)
		{
			if (np != head)
			{
				n = hawk_rtx_writeiostr(rtx, nde->out_type, out.ptr, rtx->gbl.ofs.ptr, rtx->gbl.ofs.len);
				if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/) 
				{
					if (rtx->hawk->opt.trait & HAWK_TOLERANT)
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
			if (HAWK_UNLIKELY(!v)) goto oops_1;

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_writeioval(rtx, nde->out_type, out.ptr, v);
			hawk_rtx_refdownval (rtx, v);

			if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/) 
			{
				if (rtx->hawk->opt.trait & HAWK_TOLERANT)
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
	n = hawk_rtx_writeiostr(rtx, nde->out_type, out.ptr, rtx->gbl.ors.ptr, rtx->gbl.ors.len);
	if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/)
	{
		if (rtx->hawk->opt.trait & HAWK_TOLERANT)
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
		hawk_rtx_freevaloocstr (rtx, out_v, out.ptr);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return xret;

oops:
	ADJERR_LOC (rtx, &nde->loc);

oops_1:
	if (out_v)
	{
		hawk_rtx_freevaloocstr (rtx, out_v, out.ptr);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return -1;
}

static int run_printf (hawk_rtx_t* rtx, hawk_nde_print_t* nde)
{
	hawk_oocs_t out;
	hawk_val_t* out_v = HAWK_NULL;
	hawk_val_t* v;
	hawk_val_type_t vtype;
	hawk_nde_t* head;
	int n, xret = 0;

	HAWK_ASSERT (
		(nde->out_type == HAWK_OUT_PIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_RWPIPE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_FILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_APFILE && nde->out != HAWK_NULL) ||
		(nde->out_type == HAWK_OUT_CONSOLE && nde->out == HAWK_NULL));

	if (nde->out)
	{
		out_v = io_nde_to_str(rtx, nde->out, &out, 1);
		if (!out_v || out.len <= 0) goto oops_1;
	}
	else
	{
		out.ptr = (hawk_ooch_t*)HAWK_T("");
		out.len = 0;
	}

	/*( valid printf statement should have at least one argument. the parser must ensure this. */
	HAWK_ASSERT (nde->args != HAWK_NULL);

	if (nde->args->type == HAWK_NDE_GRP)
	{
		/* parenthesized print */
		HAWK_ASSERT (nde->args->next == HAWK_NULL);
		head = ((hawk_nde_grp_t*)nde->args)->body;
	}
	else head = nde->args;

	/* valid printf statement should have at least one argument. the parser must ensure this */
	HAWK_ASSERT (head != HAWK_NULL);

	v = eval_expression(rtx, head);
	if (HAWK_UNLIKELY(!v)) goto oops_1;

	hawk_rtx_refupval (rtx, v);
	vtype = HAWK_RTX_GETVALTYPE(rtx, v);
	switch (vtype)
	{
		case HAWK_VAL_STR:
			/* perform the formatted output */
			n = output_formatted(rtx, nde->out_type, out.ptr, ((hawk_val_str_t*)v)->val.ptr, ((hawk_val_str_t*)v)->val.len, head->next);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1)
			{
				if (n == PRINT_IOERR) xret = n;
				else goto oops;
			}
			break;

		case HAWK_VAL_MBS:
			/* perform the formatted output */
			n = output_formatted_bytes(rtx, nde->out_type, out.ptr, ((hawk_val_mbs_t*)v)->val.ptr, ((hawk_val_mbs_t*)v)->val.len, head->next);
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
			n = hawk_rtx_writeioval(rtx, nde->out_type, out.ptr, v);
			hawk_rtx_refdownval (rtx, v);

			if (n <= -1 /*&& rtx->errinf.num != HAWK_EIOIMPL*/)
			{
				if (rtx->hawk->opt.trait & HAWK_TOLERANT) xret = PRINT_IOERR;
				else goto oops;
			}
			break;
	}

	if (hawk_rtx_flushio(rtx, nde->out_type, out.ptr) <= -1)
	{
		if (rtx->hawk->opt.trait & HAWK_TOLERANT) xret = PRINT_IOERR;
		else goto oops_1;
	}

	if (out_v)
	{
		hawk_rtx_freevaloocstr (rtx, out_v, out.ptr);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return xret;

oops:
	ADJERR_LOC (rtx, &nde->loc);

oops_1:
	if (out_v)
	{
		hawk_rtx_freevaloocstr (rtx, out_v, out.ptr);
		hawk_rtx_refdownval (rtx, out_v);
	}

	return -1;
}

static int output_formatted (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* dst, 
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
		return (rtx->hawk->opt.trait & HAWK_TOLERANT)? PRINT_IOERR: -1;
	}

	return 0;
}

static int output_formatted_bytes (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* dst, 
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
		return (rtx->hawk->opt.trait & HAWK_TOLERANT)? PRINT_IOERR: -1;
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

	v = eval_expression0(rtx, nde);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	if (HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_REX)
	{
		hawk_oocs_t vs;
		int free_vs = 0;

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
		#if 0
			/* the internal value representing $0 should always be of the string type 
			 * once it has been set/updated. it is nil initially. */
			HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, rtx->inrec.d0) == HAWK_VAL_STR);
			vs.ptr = ((hawk_val_str_t*)rtx->inrec.d0)->val.ptr;
			vs.len = ((hawk_val_str_t*)rtx->inrec.d0)->val.len;
		#else
			vs.ptr = hawk_rtx_getvaloocstr(rtx, rtx->inrec.d0, &vs.len);
			if (!vs.ptr)
			{
				ADJERR_LOC (rtx, &nde->loc);
				hawk_rtx_refdownval (rtx, v);
				return HAWK_NULL;
			}

			free_vs  = 1;
		#endif
		}

		n = hawk_rtx_matchvalwithoocs(rtx, v, &vs, &vs, HAWK_NULL, HAWK_NULL);
		hawk_rtx_refdownval (rtx, v);
		if (free_vs) hawk_rtx_freevaloocstr (rtx, rtx->inrec.d0, vs.ptr);
		if (n <= -1)
		{
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		v = hawk_rtx_makeintval(rtx, (n != 0));
		HAWK_ASSERT (v != HAWK_NULL); /* this will never fail as the value is 0 or 1 */
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
		eval_char,
		eval_bchr,
		eval_int,
		eval_flt,
		eval_str,
		eval_mbs,
		eval_rex,
		eval_xnil,
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

	HAWK_ASSERT (nde->type >= HAWK_NDE_GRP && (nde->type - HAWK_NDE_GRP) < HAWK_COUNTOF(__evaluator));

	v = __evaluator[nde->type - HAWK_NDE_GRP](rtx, nde);

	if (HAWK_UNLIKELY(v && rtx->exit_level >= EXIT_GLOBAL))
	{
		hawk_rtx_refupval (rtx, v);
		hawk_rtx_refdownval (rtx, v);

		/* returns HAWK_NULL as if an error occurred but
		 * clears the error number. run_main will 
		 * detect this condition and treat it as a 
		 * non-error condition.*/
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOERR);
		return HAWK_NULL;
	}

	/* this function returns a regular expression without further mathiching.
	 * it is done in eval_expression(). */
	return v;
}

static hawk_val_t* eval_group (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
#if 0
	/* eval_binop_in evaluates the HAWK_NDE_GRP specially.
	 * so this function should never be reached. */
	HAWK_ASSERT (!"should never happen - NDE_GRP only for in");
	hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EINTERN);
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

	HAWK_ASSERT (np != HAWK_NULL);

loop:
	val = eval_expression(rtx, np);
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

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

	HAWK_ASSERT (ass->left != HAWK_NULL);
	HAWK_ASSERT (ass->right != HAWK_NULL);

	HAWK_ASSERT (ass->right->next == HAWK_NULL);
	val = eval_expression(rtx, ass->right);
	if (HAWK_UNLIKELY(!val)) return HAWK_NULL;

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

		HAWK_ASSERT (ass->left->next == HAWK_NULL);
		val2 = eval_expression(rtx, ass->left);
		if (HAWK_UNLIKELY(!val2))
		{
			hawk_rtx_refdownval (rtx, val);
			return HAWK_NULL;
		}

		hawk_rtx_refupval (rtx, val2);

		HAWK_ASSERT (ass->opcode >= 0);
		HAWK_ASSERT (ass->opcode < HAWK_COUNTOF(binop_func));
		HAWK_ASSERT (binop_func[ass->opcode] != HAWK_NULL);

		tmp = binop_func[ass->opcode](rtx, val2, val);
		if (HAWK_UNLIKELY(!tmp))
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
			ret = do_assignment_nonindexed(rtx, (hawk_nde_var_t*)var, val);
			break;

		case HAWK_NDE_NAMEDIDX:
		case HAWK_NDE_GBLIDX:
		case HAWK_NDE_LCLIDX:
		case HAWK_NDE_ARGIDX:
		{
		#if !defined(HAWK_ENABLE_GC)
			hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, val);
			if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
			{
				/* a map cannot become a member of a map */
				errnum = HAWK_ENONSCATOIDX;
				goto exit_on_error;
			}
		#endif

			ret = do_assignment_indexed(rtx, (hawk_nde_var_t*)var, val);
			break;
		}

		case HAWK_NDE_POS:
		{
			hawk_val_type_t vtype = HAWK_RTX_GETVALTYPE(rtx, val);
			if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
			{
				/* a map cannot be assigned to a positional */
				errnum = HAWK_ENONSCATOPOS;
				goto exit_on_error;
			}

			ret = do_assignment_positional(rtx, (hawk_nde_pos_t*)var, val);
			break;
		}

		default:
			HAWK_ASSERT (!"should never happen - invalid variable type");
			errnum = HAWK_EINTERN;
			goto exit_on_error;
	}

	return ret;

exit_on_error:
	hawk_rtx_seterrnum (rtx, &var->loc, errnum);
	return HAWK_NULL;
}

static hawk_val_t* do_assignment_nonindexed (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val)
{
	hawk_val_type_t vtype;

	HAWK_ASSERT (
		var->type == HAWK_NDE_NAMED ||
		var->type == HAWK_NDE_GBL ||
		var->type == HAWK_NDE_LCL ||
		var->type == HAWK_NDE_ARG
	);

	HAWK_ASSERT (var->idx == HAWK_NULL);
	vtype = HAWK_RTX_GETVALTYPE (rtx, val);

	switch (var->type)
	{
		case HAWK_NDE_NAMED:
		{
			hawk_htb_pair_t* pair;

			pair = hawk_htb_search(rtx->named, var->id.name.ptr, var->id.name.len);

			if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
			{
				hawk_val_type_t old_vtype;
				if (pair && ((old_vtype = HAWK_RTX_GETVALTYPE(rtx, (hawk_val_t*)HAWK_HTB_VPTR(pair))) == HAWK_VAL_MAP || old_vtype == HAWK_VAL_ARR))
				{
					/* old value is a map - it can only be accessed through indexing. */
					if (vtype == old_vtype)
						hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATONONSCA, HAWK_T("not allowed to change a nonscalar value in '%.*js' to another nonscalar value"), var->id.name.len, var->id.name.ptr);
					else
						hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATOSCALAR, HAWK_T("not allowed to change a nonscalar value in '%.*js' to a scalar value"), var->id.name.len, var->id.name.ptr);
					return HAWK_NULL;
				}
				else if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
				{
					/* old value is not a map but a new value is a map.
					 * a map cannot be assigned to a variable if FLEXMAP is off. */
					hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATOVAR, HAWK_T("not allowed to assign a nonscalar value to a variable '%.*js'"), var->id.name.len, var->id.name.ptr);
					return HAWK_NULL;
				}
			}

			if (hawk_htb_upsert(rtx->named, var->id.name.ptr, var->id.name.len, val, 0) == HAWK_NULL)
			{
				ADJERR_LOC (rtx, &var->loc);
				return HAWK_NULL;
			}

			hawk_rtx_refupval (rtx, val);
			break;
		}

		case HAWK_NDE_GBL:
		{
			if (set_global(rtx, var->id.idxa, var, val, 1) == -1) 
			{
				ADJERR_LOC (rtx, &var->loc);
				return HAWK_NULL;
			}
			break;
		}

		case HAWK_NDE_LCL:
		{
			hawk_val_t* old = HAWK_RTX_STACK_LCL(rtx,var->id.idxa);

			if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
			{
				hawk_val_type_t old_type;

				if ((old_type = HAWK_RTX_GETVALTYPE(rtx, old)) == HAWK_VAL_MAP || old_type == HAWK_VAL_ARR)
				{
					/* old value is a map - it can only be accessed through indexing. */
					if (vtype == old_type)
						hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATONONSCA, HAWK_T("not allowed to change a nonscalar value in '%.*js' to another nonscalar value"), var->id.name.len, var->id.name.ptr);
					else
						hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATOSCALAR, HAWK_T("not allowed to change a nonscalar value in '%.*js' to a scalar value"), var->id.name.len, var->id.name.ptr);
					return HAWK_NULL;
				}
				else if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
				{
					/* old value is not a map but a new value is a map.
					 * a map cannot be assigned to a variable if FLEXMAP is off. */
					hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATOVAR, HAWK_T("not allowed to assign a nonscalar value to a variable '%.*js'"), var->id.name.len, var->id.name.ptr);
					return HAWK_NULL;
				}
			}

			hawk_rtx_refdownval (rtx, old);
			HAWK_RTX_STACK_LCL(rtx,var->id.idxa) = val;
			hawk_rtx_refupval (rtx, val);
			break;
		}

		case HAWK_NDE_ARG:
		{
			hawk_val_t* old = HAWK_RTX_STACK_ARG(rtx,var->id.idxa);

			if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP))
			{
				hawk_val_type_t old_type;

				if ((old_type = HAWK_RTX_GETVALTYPE(rtx, old) == HAWK_VAL_MAP) || old_type == HAWK_VAL_ARR)
				{
					/* old value is a map - it can only be accessed through indexing. */
					if (vtype == old_type)
						hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATONONSCA, HAWK_T("not allowed to change a nonscalar value in '%.*js' to another nonscalar value"), var->id.name.len, var->id.name.ptr);
					else
						hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATOSCALAR, HAWK_T("not allowed to change a nonscalar value in '%.*js' to a scalar value"), var->id.name.len, var->id.name.ptr);
					return HAWK_NULL;
				}
				else if (vtype == HAWK_VAL_MAP || vtype == HAWK_VAL_ARR)
				{
					/* old value is not a map but a new value is a map.
					 * a map cannot be assigned to a variable if FLEXMAP is off. */
					hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ENONSCATOVAR, HAWK_T("not allowed to assign a nonscalar value to a variable '%.*js'"), var->id.name.len, var->id.name.ptr);
					return HAWK_NULL;
				}
			}

			hawk_rtx_refdownval (rtx, old);
			HAWK_RTX_STACK_ARG(rtx,var->id.idxa) = val;
			hawk_rtx_refupval (rtx, val);
			break;
		}

		default:
			hawk_rtx_seterrnum (rtx, &var->loc, HAWK_EINTERN);
			return HAWK_NULL;
	}

	return val;
}

static hawk_val_t* do_assignment_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* var, hawk_val_t* val)
{
	hawk_map_t* map;
	hawk_ooch_t* str = HAWK_NULL;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[HAWK_IDX_BUF_SIZE];

	hawk_ooi_t idx;
	hawk_arr_t* arr;

	hawk_nde_t* remidx;
	hawk_val_t* vv; /* existing value pointed to by var */
	hawk_val_type_t vtype;

	HAWK_ASSERT (
		(var->type == HAWK_NDE_NAMEDIDX ||
		 var->type == HAWK_NDE_GBLIDX ||
		 var->type == HAWK_NDE_LCLIDX ||
		 var->type == HAWK_NDE_ARGIDX) && var->idx != HAWK_NULL);
#if !defined(HAWK_ENABLE_GC)
	HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, val) != HAWK_VAL_MAP && 
	             HAWK_RTX_GETVALTYPE(rtx, val) != HAWK_VAL_ARR);
#endif

	/* check the value that the assigned variable points to */
	vv = fetch_topval_from_var(rtx, var);
	vtype = HAWK_RTX_GETVALTYPE(rtx, vv);
	switch (vtype)
	{
		case HAWK_VAL_MAP:
		case HAWK_VAL_ARR:
		{
		val_map_or_arr:
			if (vtype == HAWK_VAL_MAP)
			{
				len = HAWK_COUNTOF(idxbuf);
				str = idxnde_to_str(rtx, var->idx, idxbuf, &len, &remidx, HAWK_NULL);
				if (HAWK_UNLIKELY(!str)) goto oops;
				map = ((hawk_val_map_t*)vv)->map;
			}
			else
			{
				idx = idxnde_to_int(rtx, var->idx, &remidx);
				if (idx <= -1) goto oops;
				arr = ((hawk_val_arr_t*)vv)->arr;
			}

		#if defined(HAWK_ENABLE_GC)
			while (remidx)
			{
				hawk_val_type_t container_vtype;

				if (vtype == HAWK_VAL_MAP)
				{
					hawk_map_pair_t* pair;
					pair = hawk_map_search(map, str, len);
					vv = pair? (hawk_val_t*)HAWK_MAP_VPTR(pair): hawk_val_nil;
				}
				else
				{
					vv = (idx < HAWK_ARR_SIZE(arr) && HAWK_ARR_SLOT(arr, idx))? ((hawk_val_t*)HAWK_ARR_DPTR(arr, idx)): hawk_val_nil;
				}
				container_vtype = vtype;
				vtype = HAWK_RTX_GETVALTYPE(rtx, vv);

				switch (vtype)
				{
					case HAWK_VAL_MAP:
					val_map:
						if (str != idxbuf) hawk_rtx_freemem (rtx, str);
						len = HAWK_COUNTOF(idxbuf);
						str = idxnde_to_str(rtx, remidx, idxbuf, &len, &remidx, HAWK_NULL);
						if (HAWK_UNLIKELY(!str)) goto oops;
						map = ((hawk_val_map_t*)vv)->map;
						break;

					case HAWK_VAL_ARR:
					val_arr:
						idx = idxnde_to_int(rtx, remidx, &remidx);
						if (idx <= -1) goto oops;
						arr = ((hawk_val_arr_t*)vv)->arr;
						break;

					default:
						if (vtype == HAWK_VAL_NIL || (rtx->hawk->opt.trait & HAWK_FLEXMAP))
						{
							/* BEGIN { @local a, b; b="hello"; a[1]=b; a[1][2]=20; print a[1][2];} */
							/* BEGIN { a[1]="hello"; a[1][2]=20; print a[1][2];  } */
							/* BEGIN { b="hello"; a[1]=b; a[1][2]=20; print a[1][2]; } */

							/* well, this is not the first level index.
							 * the logic is different from the first level where reset_variable is called.
							 * here it simply creates a new map. */
							if (container_vtype == HAWK_VAL_MAP)
							{
								vv = assign_newmapval_in_map(rtx, map, str, len);
								if (HAWK_UNLIKELY(!vv)) { ADJERR_LOC(rtx, &var->loc); goto oops; }
								vtype = HAWK_VAL_MAP;
								goto val_map;
							}
							else
							{
								vv = assign_newarrval_in_arr(rtx, arr, idx);
								if (HAWK_UNLIKELY(!vv)) { ADJERR_LOC(rtx, &var->loc); goto oops; }
								vtype = HAWK_VAL_ARR;
								goto val_arr;
							}
						}
						else
						{
							hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ESCALARTONONSCA, HAWK_T("not allowed to change a nested scalar value under '%.*js' to a nonscalar value"), var->id.name.len, var->id.name.ptr);
							goto oops;
						}
				}
			}
		#endif

		#if defined(DEBUG_RUN)
			hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("**** index str=>%s, map->ref=%d, map->type=%d\n"), str, (int)v->ref, (int)v->type);
		#endif

			if (vtype == HAWK_VAL_MAP)
			{
				if (HAWK_UNLIKELY(hawk_map_upsert(map, str, len, val, 0) == HAWK_NULL)) 
				{
					ADJERR_LOC (rtx, &var->loc);
					goto oops;
				}
			}
			else
			{
				if (HAWK_UNLIKELY(hawk_arr_upsert(arr, idx, val, 0) == HAWK_ARR_NIL))
				{
					ADJERR_LOC (rtx, &var->loc);
					goto oops;
				}
			}

			hawk_rtx_refupval (rtx, val);
			if (str && str != idxbuf) hawk_rtx_freemem (rtx, str); 
			return val;
		}

		default:
			if (vtype == HAWK_VAL_NIL || (rtx->hawk->opt.trait & HAWK_FLEXMAP))
			{
				/* 1. switch a nil value to a map value at the first level */
				/* 2. if FLEXMAP is on, you can switch a scalar value to a map value */
				vv = assign_newmapval_to_var(rtx, var);
				if (HAWK_UNLIKELY(!vv)) return HAWK_NULL;
				vtype = HAWK_VAL_MAP;
				goto val_map_or_arr;
			}
			else
			{
				/* you can't manipulate a variable pointing to
				 * a scalar value with an index if FLEXMAP is off. */
				hawk_rtx_seterrfmt (rtx, &var->loc, HAWK_ESCALARTONONSCA, HAWK_T("not allowed to change a scalar value in '%.*js' to a nonscalar value"), var->id.name.len, var->id.name.ptr);
				goto oops;
			}
	}

oops:
	if (str && str != idxbuf) hawk_rtx_freemem (rtx, str); 
	return HAWK_NULL;
}

static hawk_val_t* do_assignment_positional (hawk_rtx_t* rtx, hawk_nde_pos_t* pos, hawk_val_t* val)
{
	hawk_val_t* v;
	hawk_val_type_t vtype;
	hawk_int_t lv;
	hawk_oocs_t str;
	int n;

	v = eval_expression(rtx, pos->val);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, v);
	n = hawk_rtx_valtoint(rtx, v, &lv);
	hawk_rtx_refdownval (rtx, v);

	if (n <= -1) 
	{
		hawk_rtx_seterrnum (rtx, &pos->loc, HAWK_EPOSIDX);
		return HAWK_NULL;
	}

	if (!IS_VALID_POSIDX(lv)) 
	{
		hawk_rtx_seterrnum (rtx, &pos->loc, HAWK_EPOSIDX);
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
		if (hawk_rtx_valtostr(rtx, val, &out) <= -1)
		{
			ADJERR_LOC (rtx, &pos->loc);
			return HAWK_NULL;
		}

		str = out.u.cpldup;
	}
	
	n = hawk_rtx_setrec(rtx, (hawk_oow_t)lv, &str, 0);

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

static hawk_val_t* eval_binary (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	static binop_func_t binop_func[] =
	{
		/* the order of the functions should be inline with
		 * the operator declaration in rtx.h */

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

	HAWK_ASSERT (exp->type == HAWK_NDE_EXP_BIN);

	switch (exp->opcode)
	{
		case HAWK_BINOP_LAND:
			res = eval_binop_land(rtx, exp->left, exp->right);
			break;

		case HAWK_BINOP_LOR:
			res = eval_binop_lor(rtx, exp->left, exp->right);
			break;

		case HAWK_BINOP_IN:
			/* treat the in operator specially */
			res = eval_binop_in(rtx, exp->left, exp->right);
			break;

		case HAWK_BINOP_NM:
			res = eval_binop_nm(rtx, exp->left, exp->right);
			break;

		case HAWK_BINOP_MA:
			res = eval_binop_ma(rtx, exp->left, exp->right);
			break;
	
		default:
			HAWK_ASSERT (exp->left->next == HAWK_NULL);
			left = eval_expression(rtx, exp->left);
			if (HAWK_UNLIKELY(!left)) return HAWK_NULL;

			hawk_rtx_refupval (rtx, left);

			HAWK_ASSERT (exp->right->next == HAWK_NULL);
			right = eval_expression(rtx, exp->right);
			if (HAWK_UNLIKELY(!right)) 
			{
				hawk_rtx_refdownval (rtx, left);
				return HAWK_NULL;
			}

			hawk_rtx_refupval (rtx, right);

			HAWK_ASSERT (exp->opcode >= 0 && exp->opcode < HAWK_COUNTOF(binop_func));
			HAWK_ASSERT (binop_func[exp->opcode] != HAWK_NULL);

			res = binop_func[exp->opcode](rtx, left, right);
			if (HAWK_UNLIKELY(!res)) ADJERR_LOC (rtx, &nde->loc);

			hawk_rtx_refdownval (rtx, left);
			hawk_rtx_refdownval (rtx, right);
	}

	return res;
}

static hawk_val_t* eval_binop_lor (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right)
{
	/*
	hawk_val_t* res = HAWK_NULL;

	res = hawk_rtx_makeintval (
		rtx, 
		hawk_rtx_valtobool(rtx,left) || 
		hawk_rtx_valtobool(rtx,right)
	);
	if (res == HAWK_NULL)
	{
		ADJERR_LOC (rtx, &left->loc);
		return HAWK_NULL;
	}

	return res;
	*/

	/* short-circuit evaluation required special treatment */
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (left->next == HAWK_NULL);
	lv = eval_expression(rtx, left);
	if (HAWK_UNLIKELY(!lv)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, lv);
	if (hawk_rtx_valtobool(rtx, lv)) 
	{
		res = HAWK_VAL_ONE;
	}
	else
	{
		HAWK_ASSERT (right->next == HAWK_NULL);
		rv = eval_expression(rtx, right);
		if (HAWK_UNLIKELY(!rv))
		{
			hawk_rtx_refdownval (rtx, lv);
			return HAWK_NULL;
		}
		hawk_rtx_refupval (rtx, rv);

		res = hawk_rtx_valtobool(rtx,rv)? 
			HAWK_VAL_ONE: HAWK_VAL_ZERO;
		hawk_rtx_refdownval (rtx, rv);
	}

	hawk_rtx_refdownval (rtx, lv);

	return res;
}

static hawk_val_t* eval_binop_land (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right)
{
	/*
	hawk_val_t* res = HAWK_NULL;

	res = hawk_rtx_makeintval (
		rtx, 
		hawk_rtx_valtobool(rtx,left) &&
		hawk_rtx_valtobool(rtx,right)
	);
	if (res == HAWK_NULL) 
	{
		ADJERR_LOC (rtx, &left->loc);
		return HAWK_NULL;
	}

	return res;
	*/

	/* short-circuit evaluation required special treatment */
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (left->next == HAWK_NULL);
	lv = eval_expression(rtx, left);
	if (HAWK_UNLIKELY(!lv)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, lv);
	if (!hawk_rtx_valtobool(rtx, lv)) 
	{
		res = HAWK_VAL_ZERO;
	}
	else
	{
		HAWK_ASSERT (right->next == HAWK_NULL);
		rv = eval_expression(rtx, right);
		if (HAWK_UNLIKELY(!rv))
		{
			hawk_rtx_refdownval (rtx, lv);
			return HAWK_NULL;
		}
		hawk_rtx_refupval (rtx, rv);

		res = hawk_rtx_valtobool(rtx,rv)? HAWK_VAL_ONE: HAWK_VAL_ZERO;
		hawk_rtx_refdownval (rtx, rv);
	}

	hawk_rtx_refdownval (rtx, lv);

	return res;
}

static hawk_val_t* eval_binop_in (hawk_rtx_t* rtx, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_val_t* res;
	hawk_val_t* ropv;
	hawk_val_type_t ropvtype;
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[HAWK_IDX_BUF_SIZE];
	hawk_nde_t* remidx;
	hawk_int_t idxint;

#if defined(HAWK_ENABLE_GC)
	if (right->type < HAWK_NDE_NAMED || right->type > HAWK_NDE_ARGIDX)
#else
	if (right->type < HAWK_NDE_NAMED || right->type > HAWK_NDE_ARG)
#endif
	{
		/* the compiler should have handled this case */
		HAWK_ASSERT (!"should never happen - it needs a plain variable");
		hawk_rtx_seterrnum (rtx, &right->loc, HAWK_EINTERN);
		return HAWK_NULL;
	}

	/* evaluate the left-hand side of the operator */
	len = HAWK_COUNTOF(idxbuf);
	str = (left->type == HAWK_NDE_GRP)? /* it is inefficinet to call idxnde_to_str() for an array. but i don't know if the right hand side is a map or an array yet */
		idxnde_to_str(rtx, ((hawk_nde_grp_t*)left)->body, idxbuf, &len, &remidx, HAWK_NULL):
		idxnde_to_str(rtx, left, idxbuf, &len, &remidx, &idxint);
	if (HAWK_UNLIKELY(!str)) return HAWK_NULL;

	/* There is no way to express a true multi-dimensional indices for the 'in' operator.
	 * So remidx must be NULL here. 
	 *   a[10][20] <--- no way to express the test of 20 under 10 in a.
	 * You may use multi-level test conjoined with a logical and operator in such a case.
	 *   ((10 in a) && (20 in a[10])) 
	 *
	 *  '(10, 20) in a' is to test for a[10,20] if 'a' is a map.
	 */
	HAWK_ASSERT (remidx == HAWK_NULL);

	/* evaluate the right-hand side of the operator */
	HAWK_ASSERT (right->next == HAWK_NULL);
	ropv = eval_expression(rtx, right);
	if (HAWK_UNLIKELY(!ropv)) 
	{
		if (str != idxbuf) hawk_rtx_freemem (rtx, str);
		return HAWK_NULL;
	}

	hawk_rtx_refupval (rtx, ropv);

	ropvtype = HAWK_RTX_GETVALTYPE(rtx, ropv);
	switch (ropvtype)
	{
		case HAWK_VAL_NIL:
			res = HAWK_VAL_ZERO;
			break;

		case HAWK_VAL_MAP:
		{
			
			hawk_map_t* map;

			map = ((hawk_val_map_t*)ropv)->map;
			res = (hawk_map_search(map, str, len) == HAWK_NULL)? HAWK_VAL_ZERO: HAWK_VAL_ONE;
			break;
		}

		case HAWK_VAL_ARR:
		{
			hawk_arr_t* arr;

			if (left->type == HAWK_NDE_GRP)
			{
				hawk_rtx_seterrnum (rtx, &left->loc, HAWK_EARRIDXMULTI);
				goto oops;
			}

			arr = ((hawk_val_arr_t*)ropv)->arr;
			res = (idxint < 0 || idxint >= HAWK_ARR_SIZE(arr) || !HAWK_ARR_SLOT(arr, idxint))? HAWK_VAL_ZERO: HAWK_VAL_ONE;
			break;
		}

		default:
			hawk_rtx_seterrnum (rtx, &right->loc, HAWK_EINROP);
			goto oops;
	}

	if (str != idxbuf) hawk_rtx_freemem (rtx, str);
	hawk_rtx_refdownval (rtx, ropv);
	return res;

oops:
	if (str != idxbuf) hawk_rtx_freemem (rtx, str);
	hawk_rtx_refdownval (rtx, ropv);

	return HAWK_NULL;
}

static hawk_val_t* eval_binop_bor (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint(rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint(rtx, right, &l2) <= -1)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval(rtx, l1 | l2);
}

static hawk_val_t* eval_binop_bxor (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint(rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint(rtx, right, &l2) <= -1)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval(rtx, l1 ^ l2);
}

static hawk_val_t* eval_binop_band (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_int_t l1, l2;

	if (hawk_rtx_valtoint(rtx, left, &l1) <= -1 ||
	    hawk_rtx_valtoint(rtx, right, &l2) <= -1)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	return hawk_rtx_makeintval(rtx, l1 & l2);
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
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
			return -1;
	}
}

/* -------------------------------------------------------------------- */
static HAWK_INLINE int __cmp_nil_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return 0;
}

static HAWK_INLINE int __cmp_nil_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_oochu_t v = HAWK_RTX_GETCHARFROMVAL(rtx, right);
	return (v < 0)? 1: ((v > 0)? -1: 0);
}

static HAWK_INLINE int __cmp_nil_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_bchu_t v = HAWK_RTX_GETBCHRFROMVAL(rtx, right);
	return (v < 0)? 1: ((v > 0)? -1: 0);
}

static HAWK_INLINE int __cmp_nil_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_int_t v = HAWK_RTX_GETINTFROMVAL(rtx, right);
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
	return (HAWK_MAP_SIZE(((hawk_val_map_t*)right)->map) == 0)? 0: -1;
}

static HAWK_INLINE int __cmp_nil_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return (HAWK_ARR_SIZE(((hawk_val_arr_t*)right)->arr) == 0)? 0: -1;
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_char_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_char(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_char_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_oochu_t v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	hawk_oochu_t v2 = HAWK_RTX_GETCHARFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
}

static HAWK_INLINE int __cmp_char_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_oochu_t v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	hawk_bchu_t v2 = HAWK_RTX_GETBCHRFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
}

static HAWK_INLINE int __cmp_char_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/*
	hawk_oochu_t v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	hawk_int_t v2 = HAWK_RTX_GETINTFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
	*/
	hawk_ooch_t v1;
	hawk_ooch_t* str0;
	hawk_oow_t len0;
	int n;

	v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	str0 = hawk_rtx_getvaloocstr(rtx, right, &len0);
	if (!str0) return CMP_ERROR;
	n = hawk_comp_oochars(&v1, 1, str0, len0, rtx->gbl.ignorecase);
	hawk_rtx_freevaloocstr (rtx, right, str0);
	return n;
}

static HAWK_INLINE int __cmp_char_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/*
	hawk_ooch_t v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	if (v1 > ((hawk_val_flt_t*)right)->val) return 1;
	if (v1 < ((hawk_val_flt_t*)right)->val) return -1;
	return 0;
	*/
	return __cmp_char_int(rtx, left, right, op_hint);
}

static HAWK_INLINE int __cmp_char_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_ooch_t v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	return hawk_comp_oochars(&v1, 1, ((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, rtx->gbl.ignorecase);
}

static HAWK_INLINE int __cmp_char_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_ooch_t v1 = HAWK_RTX_GETCHARFROMVAL(rtx, left);
	hawk_bch_t bc;
	if (v1 > 0xFF) return 1;
	bc = v1;
	return hawk_comp_bchars(&bc, 1, ((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, rtx->gbl.ignorecase);
}

static HAWK_INLINE int __cmp_char_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_char_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_char_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_bchr_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_bchr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_bchr_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_bchr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_bchr_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_bchu_t v1 = HAWK_RTX_GETBCHRFROMVAL(rtx, left);
	hawk_bchu_t v2 = HAWK_RTX_GETBCHRFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
}

static HAWK_INLINE int __cmp_bchr_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/*
	hawk_bchu_t v1 = HAWK_RTX_GETBCHRFROMVAL(rtx, left);
	hawk_int_t v2 = HAWK_RTX_GETINTFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
	*/
	hawk_bch_t v1;
	hawk_bch_t* str0;
	hawk_oow_t len0;
	int n;

	v1 = HAWK_RTX_GETBCHRFROMVAL(rtx, left);
	str0 = hawk_rtx_getvalbcstr(rtx, right, &len0);
	if (!str0) return CMP_ERROR;
	n = hawk_comp_bchars(&v1, 1, str0, len0, rtx->gbl.ignorecase);
	hawk_rtx_freevalbcstr (rtx, right, str0);
	return n;
}

static HAWK_INLINE int __cmp_bchr_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/*
	hawk_bchu_t v1 = HAWK_RTX_GETBCHRFROMVAL(rtx, left);
	if (v1 > ((hawk_val_flt_t*)right)->val) return 1;
	if (v1 < ((hawk_val_flt_t*)right)->val) return -1;
	return 0;
	*/
	return __cmp_bchr_int(rtx, left, right, op_hint);
}

static HAWK_INLINE int __cmp_bchr_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_bchu_t v1 = HAWK_RTX_GETBCHRFROMVAL(rtx, left);
	hawk_oochu_t oc = v1;
	return hawk_comp_oochars(&oc, 1, ((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, rtx->gbl.ignorecase);
}

static HAWK_INLINE int __cmp_bchr_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_bch_t v1 = HAWK_RTX_GETBCHRFROMVAL(rtx, left);
	return hawk_comp_bchars(&v1, 1, ((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, rtx->gbl.ignorecase);
}

static HAWK_INLINE int __cmp_bchr_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_bchr_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_bchr_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_int_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_int(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_int_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_int(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_int_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_bchr_int(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_int_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_int_t v1 = HAWK_RTX_GETINTFROMVAL(rtx, left);
	hawk_int_t v2 = HAWK_RTX_GETINTFROMVAL(rtx, right);
	return (v1 > v2)? 1: ((v1 < v2)? -1: 0);
}

static HAWK_INLINE int __cmp_int_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
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

	/* SCO CC doesn't seem to handle right->v_nstr > 0 properly */
	if ((hawk->opt.trait & HAWK_NCMPONSTR) || right->v_nstr /*> 0*/)
	{
		hawk_int_t ll, v1;
		hawk_flt_t rr;

		n = hawk_oochars_to_num(
			HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0),
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

	str0 = hawk_rtx_getvaloocstr(rtx, left, &len0);
	if (!str0) return CMP_ERROR;
	n = hawk_comp_oochars(str0, len0, ((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freevaloocstr (rtx, left, str0);
	return n;
}

static HAWK_INLINE int __cmp_int_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_bch_t* str0;
	hawk_oow_t len0;
	int n;

	if ((hawk->opt.trait & HAWK_NCMPONSTR) || right->v_nstr /*> 0*/)
	{
		hawk_int_t ll, v1;
		hawk_flt_t rr;

		n = hawk_bchars_to_num (
			HAWK_OOCHARS_TO_NUM_MAKE_OPTION(1, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0),
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

	str0 = hawk_rtx_getvalbcstr(rtx, left, &len0);
	if (!str0) return -1;
	n = hawk_comp_bchars(str0, len0, ((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freevalbcstr (rtx, left, str0);
	return n;
}

static HAWK_INLINE int __cmp_int_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_int_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_int_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}
/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_flt_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_flt(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_flt_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_flt(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_flt_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_bchr_flt(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_flt_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_int_flt(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_flt_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	if (((hawk_val_flt_t*)left)->val > ((hawk_val_flt_t*)right)->val) return 1;
	if (((hawk_val_flt_t*)left)->val < ((hawk_val_flt_t*)right)->val) return -1;
	return 0;
}

static HAWK_INLINE int __cmp_flt_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_ooch_t* str0;
	hawk_oow_t len0;
	int n;

	/* SCO CC doesn't seem to handle right->v_nstr > 0 properly */
	if ((hawk->opt.trait & HAWK_NCMPONSTR) || right->v_nstr /*> 0*/)
	{
		const hawk_ooch_t* end;
		hawk_flt_t rr;

		rr = hawk_oochars_to_flt(((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, &end, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx));
		if (end == ((hawk_val_str_t*)right)->val.ptr + ((hawk_val_str_t*)right)->val.len)
		{
			return (((hawk_val_flt_t*)left)->val > rr)? 1:
			       (((hawk_val_flt_t*)left)->val < rr)? -1: 0;
		}
	}

	str0 = hawk_rtx_getvaloocstr(rtx, left, &len0);
	if (!str0) return CMP_ERROR;
	n = hawk_comp_oochars(str0, len0, ((hawk_val_str_t*)right)->val.ptr, ((hawk_val_str_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freevaloocstr (rtx, left, str0);
	return n;
}

static HAWK_INLINE int __cmp_flt_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_bch_t* str0;
	hawk_oow_t len0;
	int n;

	if ((hawk->opt.trait & HAWK_NCMPONSTR) || right->v_nstr /*> 0*/)
	{
		const hawk_bch_t* end;
		hawk_flt_t rr;

		rr = hawk_bchars_to_flt(((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, &end, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx));
		if (end == ((hawk_val_mbs_t*)right)->val.ptr + ((hawk_val_mbs_t*)right)->val.len)
		{
			return (((hawk_val_flt_t*)left)->val > rr)? 1:
			       (((hawk_val_flt_t*)left)->val < rr)? -1: 0;
		}
	}

	str0 = hawk_rtx_getvalbcstr(rtx, left, &len0);
	if (HAWK_UNLIKELY(!str0)) return CMP_ERROR;
	n = hawk_comp_bchars(str0, len0, ((hawk_val_mbs_t*)right)->val.ptr, ((hawk_val_mbs_t*)right)->val.len, rtx->gbl.ignorecase);
	hawk_rtx_freevalbcstr (rtx, left, str0);
	return n;
}

static HAWK_INLINE int __cmp_flt_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_flt_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_flt_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_str_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_str(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_str_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_str(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_str_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_bchr_str(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
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
	hawk_val_str_t* ls, * rs;
	int stripspc;

	ls = (hawk_val_str_t*)left;
	rs = (hawk_val_str_t*)right;

	if (HAWK_LIKELY(ls->v_nstr == 0 || rs->v_nstr == 0))
	{
		/* both are definitely strings */
		return hawk_comp_oochars(ls->val.ptr, ls->val.len, rs->val.ptr, rs->val.len, rtx->gbl.ignorecase);
	}

	stripspc = HAWK_RTX_IS_STRIPSTRSPC_ON(rtx);

	if (ls->v_nstr == 1)
	{
		hawk_int_t ll;

		ll = hawk_oochars_to_int(ls->val.ptr, ls->val.len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc, stripspc, 0), HAWK_NULL, HAWK_NULL);

		if (rs->v_nstr == 1)
		{
			hawk_int_t rr;
			
			rr = hawk_oochars_to_int(rs->val.ptr, rs->val.len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc, stripspc, 0), HAWK_NULL, HAWK_NULL);

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
		else 
		{
			hawk_flt_t rr;

			HAWK_ASSERT (rs->v_nstr == 2);

			rr = hawk_oochars_to_flt(rs->val.ptr, rs->val.len, HAWK_NULL, stripspc);

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
	}
	else
	{
		hawk_flt_t ll;

		HAWK_ASSERT (ls->v_nstr == 2);

		ll = hawk_oochars_to_flt(ls->val.ptr, ls->val.len, HAWK_NULL, stripspc);
		
		if (rs->v_nstr == 1)
		{
			hawk_int_t rr;
			
			rr = hawk_oochars_to_int(rs->val.ptr, rs->val.len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc, stripspc, 0), HAWK_NULL, HAWK_NULL);

			return (ll > rr)? 1:
			       (ll < rr)? -1: 0;
		}
		else 
		{
			hawk_flt_t rr;

			HAWK_ASSERT (rs->v_nstr == 2);

			rr = hawk_oochars_to_flt(rs->val.ptr, rs->val.len, HAWK_NULL, stripspc);

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

static HAWK_INLINE int __cmp_str_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}


/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_mbs_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_mbs(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_mbs_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_mbs(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_mbs_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_bchr_mbs(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
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

static HAWK_INLINE int __cmp_mbs_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_fun_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_fun_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
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

static HAWK_INLINE int __cmp_fun_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
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

static HAWK_INLINE int __cmp_map_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_map(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_map_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_bchr_map(rtx, right, left, inverse_cmp_op(op_hint));
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
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
	return CMP_ERROR; 
}

static HAWK_INLINE int __cmp_map_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* can't compare a map with an array */
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
	return CMP_ERROR; 
}

/* -------------------------------------------------------------------- */

static HAWK_INLINE int __cmp_arr_nil (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_nil_arr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_arr_char (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_char_arr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_arr_bchr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_bchr_arr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_arr_int (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_int_arr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_arr_flt (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	int n;
	n = __cmp_flt_arr(rtx, right, left, inverse_cmp_op(op_hint));
	if (n == CMP_ERROR) return CMP_ERROR;
	return -n;
}

static HAWK_INLINE int __cmp_arr_str (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_arr_mbs (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_arr_fun (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	return __cmp_ensure_not_equal(rtx, op_hint);
}

static HAWK_INLINE int __cmp_arr_map (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* can't compare a map with a map */
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
	return CMP_ERROR; 
}

static HAWK_INLINE int __cmp_arr_arr (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right, cmp_op_t op_hint)
{
	/* can't compare a map with an array */
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
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
		/* this table must be synchronized with the HAWK_VAL_XXX values in hawk.h */
		__cmp_nil_nil,   __cmp_nil_char,   __cmp_nil_bchr,   __cmp_nil_int,   __cmp_nil_flt,   __cmp_nil_str,   __cmp_nil_mbs,   __cmp_nil_fun,   __cmp_nil_map,   __cmp_nil_arr,  
		__cmp_char_nil,  __cmp_char_char,  __cmp_char_bchr,  __cmp_char_int,  __cmp_char_flt,  __cmp_char_str,  __cmp_char_mbs,  __cmp_char_fun,  __cmp_char_map,  __cmp_char_arr,
		__cmp_bchr_nil,  __cmp_bchr_char,  __cmp_bchr_bchr,  __cmp_bchr_int,  __cmp_bchr_flt,  __cmp_bchr_str,  __cmp_bchr_mbs,  __cmp_bchr_fun,  __cmp_bchr_map,  __cmp_bchr_arr,
		__cmp_int_nil,   __cmp_int_char,   __cmp_int_bchr,   __cmp_int_int,   __cmp_int_flt,   __cmp_int_str,   __cmp_int_mbs,   __cmp_int_fun,   __cmp_int_map,   __cmp_int_arr,
		__cmp_flt_nil,   __cmp_flt_char,   __cmp_flt_bchr,   __cmp_flt_int,   __cmp_flt_flt,   __cmp_flt_str,   __cmp_flt_mbs,   __cmp_flt_fun,   __cmp_flt_map,   __cmp_flt_arr,
		__cmp_str_nil,   __cmp_str_char,   __cmp_str_bchr,   __cmp_str_int,   __cmp_str_flt,   __cmp_str_str,   __cmp_str_mbs,   __cmp_str_fun,   __cmp_str_map,   __cmp_str_arr,
		__cmp_mbs_nil,   __cmp_mbs_char,   __cmp_mbs_bchr,   __cmp_mbs_int,   __cmp_mbs_flt,   __cmp_mbs_str,   __cmp_mbs_mbs,   __cmp_mbs_fun,   __cmp_mbs_map,   __cmp_mbs_arr,
		__cmp_fun_nil,   __cmp_fun_char,   __cmp_fun_bchr,   __cmp_fun_int,   __cmp_fun_flt,   __cmp_fun_str,   __cmp_fun_mbs,   __cmp_fun_fun,   __cmp_fun_map,   __cmp_fun_arr,
		__cmp_map_nil,   __cmp_map_char,   __cmp_map_bchr,   __cmp_map_int,   __cmp_map_flt,   __cmp_map_str,   __cmp_map_mbs,   __cmp_map_fun,   __cmp_map_map,   __cmp_map_arr,
		__cmp_arr_nil,   __cmp_arr_char,   __cmp_arr_bchr,   __cmp_arr_int,   __cmp_arr_flt,   __cmp_arr_str,   __cmp_arr_mbs,   __cmp_arr_fun,   __cmp_arr_map,   __cmp_arr_arr
	};

	lvtype = HAWK_RTX_GETVALTYPE(rtx, left);
	rvtype = HAWK_RTX_GETVALTYPE(rtx, right);
	if (!(rtx->hawk->opt.trait & HAWK_FLEXMAP) && (lvtype == HAWK_VAL_MAP || rvtype == HAWK_VAL_MAP || lvtype == HAWK_VAL_ARR || rvtype == HAWK_VAL_ARR))
	{
		/* a map can't be compared againt other values in the non-flexible mode. */
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return -1;
	}

	HAWK_ASSERT (lvtype >= HAWK_VAL_NIL && lvtype <= HAWK_VAL_ARR);
	HAWK_ASSERT (rvtype >= HAWK_VAL_NIL && rvtype <= HAWK_VAL_ARR);

	/* mapping fomula and table layout assume:
	 *  HAWK_VAL_NIL  = 0
	 *  HAWK_VAL_CHAR = 1
	 *  HAWK_VAL_BCHR = 2
	 *  HAWK_VAL_INT  = 3
	 *  HAWK_VAL_FLT  = 4
	 *  HAWK_VAL_STR  = 5
	 *  HAWK_VAL_MBS  = 6
	 *  HAWK_VAL_FUN  = 7
	 *  HAWK_VAL_MAP  = 8
	 *  HAWK_VAL_ARR  = 9
	 * 
	 * op_hint indicate the operation in progress when this function is called.
	 * this hint doesn't require the comparison function to compare using this
	 * operation. the comparision function should return 0 if equal, -1 if less,
	 * 1 if greater, CMP_ERROR upon error regardless of this hint.
	 */
	n = func[lvtype * 10 + rvtype](rtx, left, right, op_hint);
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

				case HAWK_VAL_CHAR:
					/* since a CHAR value is only reprensented in the value pointer, 
					 * n is guaranteed to be 0 here. so the following check isn't needed */
					n = (HAWK_RTX_GETCHARFROMVAL(rtx, left) == HAWK_RTX_GETCHARFROMVAL(rtx, right));
					break;

				case HAWK_VAL_BCHR:
					 /* since a BCHR value is only reprensented in the value pointer, 
					 * n is guaranteed to be 0 here. so the following check isn't needed */
					n = (HAWK_RTX_GETBCHRFROMVAL(rtx, left) == HAWK_RTX_GETBCHRFROMVAL(rtx, right));
					break;

				case HAWK_VAL_INT:
					n = (HAWK_RTX_GETINTFROMVAL(rtx, left) == HAWK_RTX_GETINTFROMVAL(rtx, right));
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
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
	HAWK_ASSERT (n3 >= 0 && n3 <= 3);

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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	HAWK_ASSERT (n3 >= 0 && n3 <= 3);
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	HAWK_ASSERT (n3 >= 0 && n3 <= 3);
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			if  (l2 == 0) 
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EDIVBY0);
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			if (l2 == 0) 
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EDIVBY0);
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

	/* the mod function must be provided when the hawk object is created to be able to calculate floating-pointer remainder */
	HAWK_ASSERT (rtx->hawk->prm.math.mod != HAWK_NULL);

	n1 = hawk_rtx_valtonum(rtx, left, &l1, &r1);
	n2 = hawk_rtx_valtonum(rtx, right, &l2, &r2);

	if (n1 <= -1 || n2 <= -1)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
		return HAWK_NULL;
	}

	n3 = n1 + (n2 << 1);
	switch (n3)
	{
		case 0:
			if  (l2 == 0) 
			{
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EDIVBY0);
				return HAWK_NULL;
			}
			res = hawk_rtx_makeintval(rtx, (hawk_int_t)l1 % (hawk_int_t)l2);
			break;

		case 1:
			res = hawk_rtx_makefltval(rtx, rtx->hawk->prm.math.mod(hawk_rtx_gethawk(rtx), (hawk_flt_t)r1, (hawk_flt_t)l2));
			break;

		case 2:
			res = hawk_rtx_makefltval(rtx, rtx->hawk->prm.math.mod(hawk_rtx_gethawk(rtx), (hawk_flt_t)l1, (hawk_flt_t)r2));
			break;

		case 3:
			res = hawk_rtx_makefltval(rtx, rtx->hawk->prm.math.mod(hawk_rtx_gethawk(rtx), (hawk_flt_t)r1, (hawk_flt_t)r2));
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EOPERAND);
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
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EDIVBY0);
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
				hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EDIVBY0);
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
				rtx->hawk->prm.math.pow(hawk_rtx_gethawk(rtx), (hawk_flt_t)l1, (hawk_flt_t)r2)
			);
			break;

		case 3:
			/* left - real, right - real */
			res = hawk_rtx_makefltval (
				rtx,
				rtx->hawk->prm.math.pow(hawk_rtx_gethawk(rtx), (hawk_flt_t)r1,(hawk_flt_t)r2)
			);
			break;
	}

	return res;
}

static hawk_val_t* eval_binop_concat (hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right)
{
	hawk_val_t* res;

	switch (HAWK_RTX_GETVALTYPE(rtx, left))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		{
			hawk_bcs_t l, r;

			l.ptr = hawk_rtx_getvalbcstr(rtx, left, &l.len);
			if (HAWK_UNLIKELY(!l.ptr)) return HAWK_NULL;

			r.ptr = hawk_rtx_getvalbcstr(rtx, right, &r.len);
			if (HAWK_UNLIKELY(!r.ptr)) 
			{
				hawk_rtx_freevalbcstr (rtx, left, l.ptr);
				return HAWK_NULL;
			}

			res = (hawk_val_t*)hawk_rtx_makembsvalwithbchars2(rtx, l.ptr, l.len, r.ptr, r.len);

			hawk_rtx_freevalbcstr (rtx, right, r.ptr);
			hawk_rtx_freevalbcstr (rtx, left, l.ptr);
			break;
		}

		default:
		{
			hawk_oocs_t l, r;

			l.ptr = hawk_rtx_getvaloocstr(rtx, left, &l.len);
			if (HAWK_UNLIKELY(!l.ptr)) return HAWK_NULL;

			r.ptr = hawk_rtx_getvaloocstr(rtx, right, &r.len);
			if (HAWK_UNLIKELY(!r.ptr)) 
			{
				hawk_rtx_freevaloocstr (rtx, left, l.ptr);
				return HAWK_NULL;
			}

			res = (hawk_val_t*)hawk_rtx_makestrvalwithoochars2(rtx, l.ptr, l.len, r.ptr, r.len);

			hawk_rtx_freevaloocstr (rtx, right, r.ptr);
			hawk_rtx_freevaloocstr (rtx, left, l.ptr);
			break;
		}
	}

	return res;
}

static hawk_val_t* eval_binop_match0 (
	hawk_rtx_t* rtx, hawk_val_t* left, hawk_val_t* right,
	const hawk_loc_t* lloc, const hawk_loc_t* rloc, int ret)
{
	hawk_val_t* res;
	hawk_oocs_t out;
	int n;

	out.ptr = hawk_rtx_getvaloocstr(rtx, left, &out.len);
	if (HAWK_UNLIKELY(!out.ptr)) return HAWK_NULL;

	n = hawk_rtx_matchvalwithoocs(rtx, right, &out, &out, HAWK_NULL, HAWK_NULL);
	hawk_rtx_freevaloocstr (rtx, left, out.ptr);

	if (HAWK_UNLIKELY(n <= -1)) 
	{
		ADJERR_LOC (rtx, lloc);
		return HAWK_NULL;
	}

	res = hawk_rtx_makeintval(rtx, (n == ret));
	if (HAWK_UNLIKELY(!res)) 
	{
		ADJERR_LOC (rtx, lloc);
		return HAWK_NULL;
	}

	return res;
}

static hawk_val_t* eval_binop_ma (hawk_rtx_t* run, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (left->next == HAWK_NULL);
	HAWK_ASSERT (right->next == HAWK_NULL);

	lv = eval_expression(run, left);
	if (HAWK_UNLIKELY(!lv)) return HAWK_NULL;

	hawk_rtx_refupval (run, lv);

	rv = eval_expression0(run, right);
	if (HAWK_UNLIKELY(!rv))
	{
		hawk_rtx_refdownval (run, lv);
		return HAWK_NULL;
	}

	hawk_rtx_refupval (run, rv);

	res = eval_binop_match0(run, lv, rv, &left->loc, &right->loc, 1);

	hawk_rtx_refdownval (run, rv);
	hawk_rtx_refdownval (run, lv);

	return res;
}

static hawk_val_t* eval_binop_nm (hawk_rtx_t* run, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_val_t* lv, * rv, * res;

	HAWK_ASSERT (left->next == HAWK_NULL);
	HAWK_ASSERT (right->next == HAWK_NULL);

	lv = eval_expression(run, left);
	if (HAWK_UNLIKELY(!lv)) return HAWK_NULL;

	hawk_rtx_refupval (run, lv);

	rv = eval_expression0(run, right);
	if (HAWK_UNLIKELY(!rv))
	{
		hawk_rtx_refdownval (run, lv);
		return HAWK_NULL;
	}

	hawk_rtx_refupval (run, rv);

	res = eval_binop_match0(run, lv, rv, &left->loc, &right->loc, 0);

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

	HAWK_ASSERT (
		exp->type == HAWK_NDE_EXP_UNR);
	HAWK_ASSERT (
		exp->left != HAWK_NULL && exp->right == HAWK_NULL);
	HAWK_ASSERT (
		exp->opcode == HAWK_UNROP_PLUS ||
		exp->opcode == HAWK_UNROP_MINUS ||
		exp->opcode == HAWK_UNROP_LNOT ||
		exp->opcode == HAWK_UNROP_BNOT);

	HAWK_ASSERT (exp->left->next == HAWK_NULL);
	left = eval_expression(rtx, exp->left);
	if (HAWK_UNLIKELY(!left)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, left);

	switch (exp->opcode)
	{
		case HAWK_UNROP_MINUS:
			n = hawk_rtx_valtonum(rtx, left, &l, &r);
			if (HAWK_UNLIKELY(n <= -1)) goto exit_func;

			res = (n == 0)? hawk_rtx_makeintval(rtx, -l):
			                hawk_rtx_makefltval(rtx, -r);
			break;

		case HAWK_UNROP_LNOT:
			if (HAWK_RTX_GETVALTYPE (rtx, left) == HAWK_VAL_STR)
			{
				/* 0 if the string length is greater than 0.
				 * 1 if it's empty */
				res = hawk_rtx_makeintval(rtx, !(((hawk_val_str_t*)left)->val.len > 0));
			}
			else
			{
				n = hawk_rtx_valtonum(rtx, left, &l, &r);
				if (HAWK_UNLIKELY(n <= -1)) goto exit_func;

				res = (n == 0)? hawk_rtx_makeintval(rtx, !l):
				                hawk_rtx_makefltval(rtx, !r);
			}
			break;

		case HAWK_UNROP_BNOT:
			n = hawk_rtx_valtoint(rtx, left, &l);
			if (HAWK_UNLIKELY(n <= -1)) goto exit_func;

			res = hawk_rtx_makeintval(rtx, ~l);
			break;

		case HAWK_UNROP_PLUS:
			n = hawk_rtx_valtonum(rtx, left, &l, &r);
			if (HAWK_UNLIKELY(n <= -1)) goto exit_func;

			res = (n == 0)? hawk_rtx_makeintval(rtx, l):
			                hawk_rtx_makefltval(rtx, r);
			break;
	}

exit_func:
	hawk_rtx_refdownval (rtx, left);
	if (HAWK_UNLIKELY(!res)) ADJERR_LOC (rtx, &nde->loc);
	return res;
}

static hawk_val_t* eval_incpre (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* left, * res;
	hawk_val_type_t left_vtype;
	hawk_int_t inc_val_int;
	hawk_flt_t inc_val_flt;
	hawk_nde_exp_t* exp = (hawk_nde_exp_t*)nde;

	HAWK_ASSERT (exp->type == HAWK_NDE_EXP_INCPRE);
	HAWK_ASSERT (exp->left != HAWK_NULL && exp->right == HAWK_NULL);

	/* this way of checking if the l-value is assignable is
	 * ugly as it is dependent on the node types defined in hawk.h
	 * but let's keep going this way for the time being. */
	if (exp->left->type < HAWK_NDE_NAMED ||
	    /*exp->left->type > HAWK_NDE_ARGIDX) XXX */
	    exp->left->type > HAWK_NDE_POS)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EOPERAND);
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
		HAWK_ASSERT (!"should never happen - invalid opcode");
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EINTERN);
		return HAWK_NULL;
	}

	HAWK_ASSERT (exp->left->next == HAWK_NULL);
	left = eval_expression(rtx, exp->left);
	if (HAWK_UNLIKELY(!left)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, left);
	left_vtype = HAWK_RTX_GETVALTYPE(rtx, left);

	switch (left_vtype)
	{
		case HAWK_VAL_INT:
		{
			hawk_int_t r = HAWK_RTX_GETINTFROMVAL (rtx, left);
			res = hawk_rtx_makeintval(rtx, r + inc_val_int);
			if (HAWK_UNLIKELY(!res))
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
			res = hawk_rtx_makefltval(rtx, r + inc_val_flt);
			if (HAWK_UNLIKELY(!res))
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

			n = hawk_rtx_valtonum(rtx, left, &v1, &v2);
			if (HAWK_UNLIKELY(n <= -1))
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
				HAWK_ASSERT (n == 1);
				res = hawk_rtx_makefltval (rtx, v2 + inc_val_flt);
			}

			if (HAWK_UNLIKELY(!res))
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			break;
		}
	}

	if (HAWK_UNLIKELY(do_assignment(rtx, exp->left, res) == HAWK_NULL))
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

	HAWK_ASSERT (exp->type == HAWK_NDE_EXP_INCPST);
	HAWK_ASSERT (exp->left != HAWK_NULL && exp->right == HAWK_NULL);

	/* this way of checking if the l-value is assignable is
	 * ugly as it is dependent on the node types defined in hawk.h.
	 * but let's keep going this way for the time being. */
	if (exp->left->type < HAWK_NDE_NAMED ||
	    /*exp->left->type > HAWK_NDE_ARGIDX) XXX */
	    exp->left->type > HAWK_NDE_POS)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EOPERAND);
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
		HAWK_ASSERT (!"should never happen - invalid opcode");
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EINTERN);
		return HAWK_NULL;
	}

	HAWK_ASSERT (exp->left->next == HAWK_NULL);
	left = eval_expression(rtx, exp->left);
	if (HAWK_UNLIKELY(!left)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, left);

	left_vtype = HAWK_RTX_GETVALTYPE (rtx, left);

	switch (left_vtype)
	{
		case HAWK_VAL_INT:
		{
			hawk_int_t r = HAWK_RTX_GETINTFROMVAL(rtx, left);
			res = hawk_rtx_makeintval(rtx, r);
			if (HAWK_UNLIKELY(!res))
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			res2 = hawk_rtx_makeintval(rtx, r + inc_val_int);
			if (HAWK_UNLIKELY(!res2))
			{
				hawk_rtx_refdownval (rtx, left);
				hawk_rtx_freeval (rtx, res, HAWK_RTX_FREEVAL_CACHE);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			break;
		}

		case HAWK_VAL_FLT:
		{
			hawk_flt_t r = ((hawk_val_flt_t*)left)->val;
			res = hawk_rtx_makefltval(rtx, r);
			if (HAWK_UNLIKELY(!res))
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			res2 = hawk_rtx_makefltval(rtx, r + inc_val_flt);
			if (HAWK_UNLIKELY(!res2))
			{
				hawk_rtx_refdownval (rtx, left);
				hawk_rtx_freeval (rtx, res, HAWK_RTX_FREEVAL_CACHE);
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

			n = hawk_rtx_valtonum(rtx, left, &v1, &v2);
			if (HAWK_UNLIKELY(n <= -1))
			{
				hawk_rtx_refdownval (rtx, left);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			if (n == 0) 
			{
				res = hawk_rtx_makeintval(rtx, v1);
				if (HAWK_UNLIKELY(!res))
				{
					hawk_rtx_refdownval (rtx, left);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}

				res2 = hawk_rtx_makeintval(rtx, v1 + inc_val_int);
				if (HAWK_UNLIKELY(!res2))
				{
					hawk_rtx_refdownval (rtx, left);
					hawk_rtx_freeval (rtx, res, HAWK_RTX_FREEVAL_CACHE);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}
			}
			else /* if (n == 1) */
			{
				HAWK_ASSERT (n == 1);
				res = hawk_rtx_makefltval(rtx, v2);
				if (HAWK_UNLIKELY(!res))
				{
					hawk_rtx_refdownval (rtx, left);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}

				res2 = hawk_rtx_makefltval(rtx, v2 + inc_val_flt);
				if (HAWK_UNLIKELY(!res2))
				{
					hawk_rtx_refdownval (rtx, left);
					hawk_rtx_freeval (rtx, res, HAWK_RTX_FREEVAL_CACHE);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}
			}

			break;
		}
	}

	if (HAWK_UNLIKELY(do_assignment(rtx, exp->left, res2) == HAWK_NULL))
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

	HAWK_ASSERT (cnd->test->next == HAWK_NULL);

	tv = eval_expression(run, cnd->test);
	if (HAWK_UNLIKELY(!tv)) return HAWK_NULL;

	hawk_rtx_refupval (run, tv);

	HAWK_ASSERT (cnd->left->next == HAWK_NULL && cnd->right->next == HAWK_NULL);
	v = (hawk_rtx_valtobool(run, tv))? eval_expression(run, cnd->left): eval_expression(run, cnd->right);

	hawk_rtx_refdownval (run, tv);
	return v;
}

static hawk_val_t* eval_fncall_fnc (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	/* intrinsic function */
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	/* the parser must make sure that the number of arguments is proper */ 
	HAWK_ASSERT (call->nargs >= call->u.fnc.spec.arg.min && call->nargs <= call->u.fnc.spec.arg.max); 
	return hawk_rtx_evalcall(rtx, call, HAWK_NULL, push_arg_from_nde, (void*)call->u.fnc.spec.arg.spec, HAWK_NULL, HAWK_NULL);
}

static HAWK_INLINE hawk_val_t* eval_fncall_fun (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	hawk_fun_t* fun;
	hawk_htb_pair_t* pair;

	if (!call->u.fun.fun)
	{
		/* there can be multiple runtime instances for a single hawk object.
		 * changing the parse tree from one runtime instance can affect
		 * other instances. however i do change the parse tree without protection
		 * hoping that the pointer assignment is atomic. (call->u.fun.fun = fun).
		 * i don't mind each instance performs a search duplicately for a short while */

		pair = hawk_htb_search(rtx->hawk->tree.funs, call->u.fun.name.ptr, call->u.fun.name.len);
		if (!pair) 
		{
			hawk_rtx_seterrfmt (rtx, &nde->loc, HAWK_EFUNNF, HAWK_T("function '%.*js' not found"), call->u.fun.name.len, call->u.fun.name.ptr);
			return HAWK_NULL;
		}

		fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (fun != HAWK_NULL);

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
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EARGTM);
		return HAWK_NULL;
	}

	/* push_arg_from_nde() has special handling for references when the function
	 * argument spec contains 'r' or 'R'.
	 * a reference is passed to a built-in function as a reference value
	 * but its evaluation result is passed to user-defined function. 
	 * I pass HAWK_NULL to prevent special handling.
	 * the value change for a reference variable inside a user-defined function is reflected by hawk_rtx_evalcall()
	 * specially whereas a built-in function must call hawk_rtx_setrefval() to update the reference 
	 */
	return hawk_rtx_evalcall(rtx, call, fun, push_arg_from_nde, HAWK_NULL/*fun->argspec*/, HAWK_NULL, HAWK_NULL);
}

static hawk_val_t* eval_fncall_var (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_fncall_t* call = (hawk_nde_fncall_t*)nde;
	hawk_val_t* fv, * rv;
	hawk_fun_t* fun;

	fv = eval_expression(rtx, (hawk_nde_t*)call->u.var.var);
	if (HAWK_UNLIKELY(!fv)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, fv);
	fun = hawk_rtx_valtofun(rtx, fv);
	if (HAWK_UNLIKELY(!fun))
	{
		if (hawk_rtx_geterrnum(rtx) == HAWK_EINVAL)
			hawk_rtx_seterrfmt (rtx, &nde->loc, HAWK_ENOTFUN, HAWK_T("non-function value in %.*js"), call->u.var.var->id.name.len, call->u.var.var->id.name.ptr);
		ADJERR_LOC (rtx, &nde->loc);
		rv = HAWK_NULL;
	}
	else if (call->nargs > fun->nargs)
	{
		/* TODO: is this correct? what if i want to 
		*       allow arbitarary numbers of arguments? */
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EARGTM);
		rv = HAWK_NULL;
	}
	else
	{
		/* pass HAWK_NULL for the argument spec regardless of the actual spec.
		 * see comments in eval_fncall_fun() for more */
		rv = hawk_rtx_evalcall(rtx, call, fun, push_arg_from_nde, HAWK_NULL/*fun->argspec*/, HAWK_NULL, HAWK_NULL);
	}
	hawk_rtx_refdownval (rtx, fv);

	return rv;
}

hawk_val_t* hawk_rtx_evalcall (
	hawk_rtx_t* rtx, hawk_nde_fncall_t* call, hawk_fun_t* fun, 
	hawk_oow_t(*argpusher)(hawk_rtx_t*,hawk_nde_fncall_t*,void*), void* apdata,
	void(*errhandler)(void*), void* eharg)
{
	hawk_oow_t saved_stack_top, saved_arg_stack_top;
	hawk_oow_t nargs, i, stack_req;
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

	HAWK_ASSERT (HAWK_SIZEOF(void*) >= HAWK_SIZEOF(rtx->stack_top));
	HAWK_ASSERT (HAWK_SIZEOF(void*) >= HAWK_SIZEOF(rtx->stack_base));

	saved_stack_top = rtx->stack_top;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("setting up function stack frame top=%zd base=%zd\n"), (hawk_oow_t)rtx->stack_top, (hawk_oow_t)rtx->stack_base);
#endif

	/* make a new stack frame */
	stack_req = 4 + call->nargs;
	if (fun) 
	{
		HAWK_ASSERT (fun->nargs >= call->nargs); /* the compiler must guarantee this */
		stack_req += fun->nargs - call->nargs;
	}
	/* if fun is HAWK_NULL, there is no way for this function to know expected argument numbers.
	 * the argument pusher must ensure the stack availality before pushing arguments */

	if (HAWK_UNLIKELY(HAWK_RTX_STACK_AVAIL(rtx) < stack_req))
	{
		hawk_rtx_seterrnum (rtx, &call->loc, HAWK_ESTACK);
		return HAWK_NULL;
	}

	HAWK_RTX_STACK_PUSH (rtx, (void*)rtx->stack_base);
	HAWK_RTX_STACK_PUSH (rtx, (void*)saved_stack_top);
	HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil); /* space for return value */
	HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil); /* space for number of arguments */

	saved_arg_stack_top = rtx->stack_top;

	/* push all arguments onto the stack */
	nargs = argpusher(rtx, call, apdata);
	if (nargs == (hawk_oow_t)-1) goto oops_making_stack_frame;

	HAWK_ASSERT (nargs == call->nargs);

	if (fun)
	{
		/* extra step for normal hawk functions */
		while (nargs < fun->nargs)
		{
			/* push as many nils as the number of missing actual arguments */
			HAWK_RTX_STACK_PUSH (rtx, hawk_val_nil);
			nargs++;
		}
	}

	/* entering a new stack frame */
	rtx->stack_base = saved_stack_top;
	HAWK_RTX_STACK_NARGS(rtx) = (void*)nargs;

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("running function body\n"));
#endif

	if (fun)
	{
		/* normal hawk function */
		HAWK_ASSERT (fun->body->type == HAWK_NDE_BLK);
		n = run_block(rtx, (hawk_nde_blk_t*)fun->body);
	}
	else
	{
		n = 0;

		/* intrinsic function */
		HAWK_ASSERT (call->nargs >= call->u.fnc.spec.arg.min && call->nargs <= call->u.fnc.spec.arg.max);

		if (call->u.fnc.spec.impl)
		{
			n = call->u.fnc.spec.impl(rtx, &call->u.fnc.info);
			if (HAWK_UNLIKELY(n <= -1)) ADJERR_LOC (rtx, &call->loc);
		}
	}

	/* refdown args in the rtx.stack */
	nargs = (hawk_oow_t)HAWK_RTX_STACK_NARGS(rtx);
#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("block rtx complete nargs = %d\n"), (int)nargs); 
#endif

	i = 0;
	if (fun && fun->argspec && call->nargs > 0)  /* hawk_rtx_callfun() sets up a fake call structure with call->nargs > 0 but call->args == HAWK_NULL */
	{
		/* set back the values for pass-by-reference parameters of normal functions.
		 * the intrinsic functions are not handled here but their implementation would
		 * call hawk_rtx_setrefval() */

		/* even if fun->argspec exists, call->nargs may still be 0. so i test if call->nargs > 0.
		 *   function x(a1, &a2) {}
		 *   BEGIN { x(); }
		 * all parameters are nils in this case. nargs and fun->nargs are 2 but call->nargs is 0.
		 */

		if (call->args)
		{
			hawk_oow_t cur_stack_base = rtx->stack_base;
			hawk_oow_t prev_stack_base = (hawk_oow_t)rtx->stack[rtx->stack_base + 0];
			hawk_nde_t* p = call->args;

			for (; i < call->nargs; i++)
			{
				if (n >= 0 && (fun->argspec[i] == 'r' || fun->argspec[i] == 'R'))
				{
					hawk_val_t** ref;
					hawk_val_ref_t refv;
					hawk_val_t* av;
					int r;

					av = HAWK_RTX_STACK_ARG(rtx, i);
					if (HAWK_RTX_GETVALTYPE(rtx, av) == HAWK_VAL_REF)
					{
						/* the argument still has the reference type. 
						 * this means, the argument has not been set. 
						 * 
						 *   function f1(&a, &b) { b = 20 } 
						 * 
						 * since a is not set in f1, the value for a is still the pushed value which is a reference
						 */

						/* ---- DO NOTHING ---- */
					}
					else
					{
						/* if an argument passed is a local variable or a parameter to the previous caller,
						 * the argument node information stored is relative to the previous stack frame.
						 * i revert rtx->stack_base to the previous stack frame base before calling 
						 * get_reference() and restors it back to the current base. this tactic
						 * is very ugly because the assumptions for this is depecallnt on get_reference()
						 * implementation */
						rtx->stack_base = prev_stack_base; /* UGLY */
						r = get_reference(rtx, p, &ref);
						rtx->stack_base = cur_stack_base; /* UGLY */

						/* if argspec is 'r', get_reference() must succeed all the time.
						 * if argspec is 'R', it may fail. if it happens, don't copy the value */
						if (HAWK_LIKELY(r >= 0))
						{
							HAWK_RTX_INIT_REF_VAL (&refv, p->type - HAWK_NDE_NAMED, ref, 9); /* initialize a fake reference variable. 9 chosen randomly */
							if (HAWK_UNLIKELY(hawk_rtx_setrefval(rtx, &refv, av) <= -1)) 
							{
								n = -1;
								ADJERR_LOC (rtx, &call->loc);
							}
						}
					}
				}

				hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_ARG(rtx,i));
				p = p->next;
			}
		}
		else if (call->arg_base > 0) /* special case. set by hawk::call() */
		{
			/*
			 *  function f1(a,&b) { b *= 20; }
			 *  BEGIN { q = 4; hawk::call(r, "f1", 20, q); print q; }
			 *
			 * the fourth argument to hawk::call() must map to the second argument to f1().
			 * hawk::call() accepts the third to the last arguments as reference if possible.
			 * this function attempts to copy back the pass-by-reference values to 
			 * one stack frame up.
			 *
			 *  f1(1, 2) is an error as 2 is not referenceable.
			 *  hakw::call(r, "f1", 1, 2) is not an error but can't capture changes made inside f1.
			 */
			for (; i < call->nargs; i++)
			{
				if (n >= 0 && (fun->argspec[i] == 'r' || fun->argspec[i] == 'R'))
				{
					hawk_val_t* v, * av;

					av = HAWK_RTX_STACK_ARG(rtx, i);
					if (HAWK_RTX_GETVALTYPE(rtx, av) == HAWK_VAL_REF)
					{
						/* ---- DO NOTHING ---- */
					}
					else
					{
						v = rtx->stack[call->arg_base + i]; /* UGLY */
						if (HAWK_RTX_GETVALTYPE(rtx, v) == HAWK_VAL_REF)
						{
							if (HAWK_UNLIKELY(hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)v, av) <= -1)) 
							{
								n = -1;
								ADJERR_LOC (rtx, &call->loc);
							}
						}
					}
				}

				hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_ARG(rtx,i));
			}
		}
	}

	for (; i < nargs; i++)
	{
		hawk_rtx_refdownval (rtx, HAWK_RTX_STACK_ARG(rtx,i));
	}

#if defined(DEBUG_RUN)
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_T("got return value\n"));
#endif

	v = HAWK_RTX_STACK_RETVAL(rtx);
	if (HAWK_UNLIKELY(n <= -1))
	{
		if (hawk_rtx_geterrnum(rtx) == HAWK_ENOERR && errhandler != HAWK_NULL) 
		{
			/* errhandler is passed only when hawk_rtx_evalcall() is
			 * invoked from hawk_rtx_call(). Ucallr this 
			 * circumstance, this stack frame is the first
			 * activated and the stack base is the first element
			 * after the global variables. so HAWK_RTX_STACK_RETVAL(rtx)
			 * effectively becomes HAWK_RTX_STACK_RETVAL_GBL(rtx).
			 * As hawk_rtx_evalcall() returns HAWK_NULL on error and
			 * the reference count of HAWK_RTX_STACK_RETVAL(rtx) should be
			 * decremented, it can't get the return value
			 * if it turns out to be terminated by exit().
			 * The return value could be destroyed by then.
			 * Unlikely, rtx_bpae_loop() just checks if rtx->errinf.num
			 * is HAWK_ENOERR and gets HAWK_RTX_STACK_RETVAL_GBL(rtx)
			 * to determine if it is terminated by exit().
			 *
			 * The handler capture_retval_on_exit() 
			 * increments the reference of HAWK_RTX_STACK_RETVAL(rtx)
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
		HAWK_RTX_STACK_RETVAL(rtx) = hawk_val_nil;
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
	return (n <= -1)? HAWK_NULL: v;

oops_making_stack_frame:
	while (rtx->stack_top > saved_arg_stack_top)
	{
		/* call hawk_rtx_refdownval() for all arguments. 
		 * it is safe because nil or quickint is immune to excessive hawk_rtx_refdownval() calls */
		hawk_rtx_refdownval(rtx, rtx->stack[rtx->stack_top - 1]);
		HAWK_RTX_STACK_POP (rtx);
	}
	HAWK_ASSERT (rtx->stack_top - saved_stack_top == 4);
	while (rtx->stack_top > saved_stack_top)
	{
		/* the stack frame prologue does not have a reference-counted value 
		 * before being entered. so no hawk_rtx_refdownval().
		 * three slots contains raw integers for internal use.. only one 
		 * slot that contains the return value would be reference counted
		 * after it is set to a reference counted value. here, it never happens */
		HAWK_RTX_STACK_POP (rtx);
	}
	ADJERR_LOC (rtx, &call->loc);
	return HAWK_NULL;
}

static hawk_oow_t push_arg_from_vals (hawk_rtx_t* rtx, hawk_nde_fncall_t* call, void* data)
{
	struct pafv_t* pafv = (struct pafv_t*)data;
	hawk_oow_t nargs = 0;

	if (HAWK_UNLIKELY(HAWK_RTX_STACK_AVAIL(rtx) < pafv->nargs))
	{
		hawk_rtx_seterrnum (rtx, &call->loc, HAWK_ESTACK);
		return (hawk_oow_t)-1;
	}

	for (nargs = 0; nargs < pafv->nargs; nargs++)
	{
		if (pafv->argspec && (pafv->argspec[nargs] == 'r' || pafv->argspec[nargs] == 'R'))
		{
			hawk_val_t** ref;
			hawk_val_t* v;

			ref = (hawk_val_t**)&pafv->args[nargs];
			v = hawk_rtx_makerefval(rtx, HAWK_VAL_REF_LCL, ref); /* this type(HAWK_VAL_REF_LCL) is fake */
			if (HAWK_UNLIKELY(!v))
			{
				ADJERR_LOC (rtx, &call->loc);
				return (hawk_oow_t)-1;
			}

			HAWK_RTX_STACK_PUSH (rtx, v);
			hawk_rtx_refupval (rtx, v);
		}
		else
		{
			HAWK_RTX_STACK_PUSH (rtx, pafv->args[nargs]);
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
	const hawk_ooch_t* arg_spec = (const hawk_ooch_t*)data;
	hawk_oow_t spec_len;

	if (HAWK_UNLIKELY(HAWK_RTX_STACK_AVAIL(rtx) < call->nargs))
	{
		hawk_rtx_seterrnum (rtx, &call->loc, HAWK_ESTACK);
		return (hawk_oow_t)-1;
	}

	/* in practice, this function gets NULL for a user-defined function regardless of its actual spec.
	 * it may get a non-NULL arg_spec for builtin/module functions */
	spec_len = arg_spec? hawk_count_oocstr(arg_spec): 0;
	for (p = call->args, nargs = 0; p; p = p->next, nargs++)
	{
		hawk_ooch_t spec;

		/* if not sufficient number of spec characters given, take the last value and use it */
		spec = (spec_len <= 0)? '\0': arg_spec[((nargs < spec_len)? nargs: spec_len - 1)];

		switch (spec)
		{
			case 'R': /* make reference if a referenceable is given. otherwise accept a normal value - useful for hawk::call() */
			case 'r': /* make reference. a non-referenceable value is rejected */
			{
				hawk_val_t** ref;

				if (get_reference(rtx, p, &ref) <= -1) 
				{
					if (spec == 'R') goto normal_arg;
					return (hawk_oow_t)-1; /* return -1 without unwinding stack as hawk_rtx_evalcall() does it */
				}

				/* 'p->type - HAWK_NDE_NAMED' must produce a relevant HAWK_VAL_REF_XXX value. */
				v = hawk_rtx_makerefval(rtx, p->type - HAWK_NDE_NAMED, ref);
				break;
			}

			case 'x':
				/* a regular expression is passed to the function as it is */
				v = eval_expression0(rtx, p);
				break;

			default:
			normal_arg:
				v = eval_expression(rtx, p);
				break;
		}

		if (HAWK_UNLIKELY(!v)) return (hawk_oow_t)-1; /* return -1 without unwinding stack as hawk_rtx_evalcall() does it */

		HAWK_RTX_STACK_PUSH (rtx, v);
		hawk_rtx_refupval (rtx, v);
	}

	HAWK_ASSERT (call->nargs == nargs);
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
			/* *ref = (hawk_val_t**)&HAWK_RTX_STACK_GBL(rtx,tgt->id.idxa); */
			*ref = (hawk_val_t**)((hawk_oow_t)tgt->id.idxa);
			return 0;

		case HAWK_NDE_LCL:
			*ref = (hawk_val_t**)&HAWK_RTX_STACK_LCL(rtx,tgt->id.idxa);
			return 0;

		case HAWK_NDE_ARG:
			*ref = (hawk_val_t**)&HAWK_RTX_STACK_ARG(rtx,tgt->id.idxa);
			return 0;

		case HAWK_NDE_NAMEDIDX:
		case HAWK_NDE_GBLIDX:
		case HAWK_NDE_LCLIDX:
		case HAWK_NDE_ARGIDX:
			tmp = get_reference_indexed(rtx, tgt);
			if (HAWK_UNLIKELY(!tmp)) return -1;
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
			if (HAWK_UNLIKELY(!v)) return -1;

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &lv);
			hawk_rtx_refdownval (rtx, v);

			if (HAWK_UNLIKELY(n <= -1))
			{
				hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EPOSIDX);
				return -1;
			}

			if (!IS_VALID_POSIDX(lv)) 
			{
				hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EPOSIDX);
				return -1;
			}

			*ref = (hawk_val_t**)((hawk_oow_t)lv);
			return 0;
		}

		default:
			hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_ENOTREF);
			return -1;
	}
}

static hawk_val_t** get_reference_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	hawk_map_t* map;
	hawk_ooch_t* str = HAWK_NULL;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[HAWK_IDX_BUF_SIZE];

	hawk_arr_t* arr;
	hawk_ooi_t idx;

	hawk_val_t* v;
	hawk_val_type_t vtype;
	hawk_nde_t* remidx;

	HAWK_ASSERT (var->idx != HAWK_NULL);

	v = fetch_topval_from_var(rtx, var);
	vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			v = assign_newmapval_to_var(rtx, var);
			if (HAWK_UNLIKELY(!v)) goto oops;
			vtype = HAWK_VAL_MAP;
			goto val_map_init;

		case HAWK_VAL_MAP:
		val_map_init:
			len = HAWK_COUNTOF(idxbuf);
			str = idxnde_to_str(rtx, var->idx, idxbuf, &len, &remidx, HAWK_NULL);
			if (HAWK_UNLIKELY(!str)) goto oops;
			map = ((hawk_val_map_t*)v)->map;
			break;

		case HAWK_VAL_ARR:
			idx = idxnde_to_int(rtx, var->idx, &remidx);
			if (HAWK_UNLIKELY(idx <= -1)) goto oops;
			arr = ((hawk_val_arr_t*)v)->arr;
			break;

		default:
			hawk_rtx_seterrnum (rtx, &var->loc, HAWK_ENOTIDXACC);
			goto oops;
	}

#if defined(HAWK_ENABLE_GC)
	while (remidx)
	{
		hawk_val_type_t container_vtype;

		if (vtype == HAWK_VAL_MAP)
		{
			hawk_map_pair_t* pair;
			pair = hawk_map_search(map, str, len);
			v = pair? (hawk_val_t*)HAWK_MAP_VPTR(pair): hawk_val_nil;
		}
		else
		{
			v = (idx < HAWK_ARR_SIZE(arr) && HAWK_ARR_SLOT(arr, idx))? ((hawk_val_t*)HAWK_ARR_DPTR(arr, idx)): hawk_val_nil;
		}
		container_vtype = vtype;
		vtype = HAWK_RTX_GETVALTYPE(rtx, v);

		switch (vtype)
		{
			case HAWK_VAL_MAP:
			val_map:
				if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
				len = HAWK_COUNTOF(idxbuf);
				str = idxnde_to_str(rtx, remidx, idxbuf, &len, &remidx, HAWK_NULL);
				if (HAWK_UNLIKELY(!str)) goto oops;
				map = ((hawk_val_map_t*)v)->map;
				break;

			case HAWK_VAL_ARR:
			val_arr:
				idx = idxnde_to_int(rtx, remidx, &remidx);
				if (HAWK_UNLIKELY(idx <= -1)) goto oops;
				arr = ((hawk_val_arr_t*)v)->arr;
				break;

			default:
				if (vtype == HAWK_VAL_NIL /* || (rtx->hawk->opt.trait & HAWK_FLEXMAP) no flexmap because this is in a 'get' context */) 
				{
					if (container_vtype == HAWK_VAL_MAP)
					{
						v = assign_newmapval_in_map(rtx, map, str, len);
						if (HAWK_UNLIKELY(!v)) { ADJERR_LOC (rtx, &var->loc); goto oops; }
						vtype = HAWK_VAL_MAP;
						goto val_map;
					}
					else
					{
						v = assign_newarrval_in_arr(rtx, arr, idx);
						if (HAWK_UNLIKELY(!v)) { ADJERR_LOC (rtx, &var->loc); goto oops; }
						vtype = HAWK_VAL_ARR;
						goto val_arr;
					}
				}

				hawk_rtx_seterrnum (rtx, &var->loc, HAWK_ENOTIDXACC);
				goto oops;
		}
	}
#endif

	if (vtype == HAWK_VAL_MAP)
	{
		hawk_map_pair_t* pair;
		pair = hawk_map_search(map, str, len);
		if (!pair)
		{
			/* if the value doesn't exist for the given key, insert a nil for it to create a placeholder for the reference */
			pair = hawk_map_upsert(map, str, len, hawk_val_nil, 0);
			if (HAWK_UNLIKELY(!pair)) { ADJERR_LOC(rtx, &var->loc); goto oops; }
			HAWK_ASSERT (HAWK_MAP_VPTR(pair) == hawk_val_nil);
			/* no reference count increment as hawk_val_nil is upserted
			hawk_rtx_refupval (rtx, HAWK_MAP_VPTR(pair)); */
		}

		if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
		return (hawk_val_t**)&HAWK_MAP_VPTR(pair);
	}
	else
	{
		if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
		if (idx >= HAWK_ARR_SIZE(arr) || !HAWK_ARR_SLOT(arr, idx))
		{
			/* if the value doesn't exist for the given index, insert a nil at that position to create a placeholder for the reference */
			if (HAWK_UNLIKELY(hawk_arr_upsert (arr, idx, hawk_val_nil, 0) == HAWK_ARR_NIL)) { ADJERR_LOC(rtx, &var->loc);  goto oops; }
		}
		return (hawk_val_t**)&HAWK_ARR_DPTR(arr, idx);
	}

oops:
	if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
	return HAWK_NULL;
}

static hawk_val_t* eval_char (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makecharval(rtx, ((hawk_nde_char_t*)nde)->val);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_bchr (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makebchrval(rtx, ((hawk_nde_bchr_t*)nde)->val);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_int (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makeintval(rtx, ((hawk_nde_int_t*)nde)->val);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	else if (HAWK_VTR_IS_POINTER(val)) ((hawk_val_int_t*)val)->nde = nde;
	return val;
}

static hawk_val_t* eval_flt (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makefltval(rtx, ((hawk_nde_flt_t*)nde)->val);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	else ((hawk_val_flt_t*)val)->nde = nde;
	return val;
}

static hawk_val_t* eval_str (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makestrvalwithoochars(rtx, ((hawk_nde_str_t*)nde)->ptr, ((hawk_nde_str_t*)nde)->len);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_mbs (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makembsvalwithbchars(rtx, ((hawk_nde_mbs_t*)nde)->ptr, ((hawk_nde_mbs_t*)nde)->len);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_rex (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makerexval(rtx, &((hawk_nde_rex_t*)nde)->str, ((hawk_nde_rex_t*)nde)->code);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_xnil (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_val_t* val;
	val = hawk_rtx_makenilval(rtx);
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
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
		/* for a program like this,
			BEGIN { @local x; abc(); x = abc; x(); } function abc() { print "abc"; }
		 * abc is defined after BEGIN but is know to be a function inside BEGIN.
		 * the funptr field in the node can be HAWK_NULL */

		/* TODO: support builtin functions?, support module functions? */
		pair = hawk_htb_search(rtx->hawk->tree.funs, ((hawk_nde_fun_t*)nde)->name.ptr, ((hawk_nde_fun_t*)nde)->name.len);
		if (!pair)
		{
			/* it's unlikely to be not found in the function table for the parser structure. but keep this code in case */
			hawk_rtx_seterrfmt (rtx, &nde->loc, HAWK_EFUNNF, HAWK_T("function '%.*js' not found"), ((hawk_nde_fun_t*)nde)->name.len, ((hawk_nde_fun_t*)nde)->name.ptr);
			return HAWK_NULL;
		}

		fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (fun != HAWK_NULL);
	}

	val = hawk_rtx_makefunval(rtx, fun); 
	if (HAWK_UNLIKELY(!val)) ADJERR_LOC (rtx, &nde->loc);
	return val;
}

static hawk_val_t* eval_named (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_htb_pair_t* pair;
	pair = hawk_htb_search(rtx->named, ((hawk_nde_var_t*)nde)->id.name.ptr, ((hawk_nde_var_t*)nde)->id.name.len);
	return (pair == HAWK_NULL)? hawk_val_nil: HAWK_HTB_VPTR(pair);
}

static hawk_val_t* eval_gbl (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return HAWK_RTX_STACK_GBL(rtx, ((hawk_nde_var_t*)nde)->id.idxa);
}

static hawk_val_t* eval_lcl (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return HAWK_RTX_STACK_LCL(rtx, ((hawk_nde_var_t*)nde)->id.idxa);
}

static hawk_val_t* eval_arg (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return HAWK_RTX_STACK_ARG(rtx, ((hawk_nde_var_t*)nde)->id.idxa);
}

static hawk_val_t* eval_indexed (hawk_rtx_t* rtx, hawk_nde_var_t* var)
{
	hawk_map_t* map; /* containing map */
	hawk_ooch_t* str = HAWK_NULL;
	hawk_oow_t len;
	hawk_ooch_t idxbuf[HAWK_IDX_BUF_SIZE];

	hawk_arr_t* arr; /* containing array */
	hawk_ooi_t idx;

	hawk_nde_t* remidx;

	hawk_val_t* v;
	hawk_val_type_t vtype;

	v = fetch_topval_from_var(rtx, var);
	vtype = HAWK_RTX_GETVALTYPE(rtx, v);

	HAWK_ASSERT (var->idx != HAWK_NULL);

	switch (vtype)
	{
		case HAWK_VAL_NIL:
			v = assign_newmapval_to_var(rtx, var);
			if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
			vtype = HAWK_VAL_MAP;
			goto init_val_map;

		case HAWK_VAL_MAP:
		init_val_map:
			len = HAWK_COUNTOF(idxbuf);
			str = idxnde_to_str(rtx, var->idx, idxbuf, &len, &remidx, HAWK_NULL);
			if (HAWK_UNLIKELY(!str)) goto oops;
			map = ((hawk_val_map_t*)v)->map;
			break;

		case HAWK_VAL_ARR:
			idx = idxnde_to_int(rtx, var->idx, &remidx);
			if (idx <= -1) goto oops;
			arr = ((hawk_val_arr_t*)v)->arr;
			break;

		default:
			hawk_rtx_seterrnum (rtx, &var->loc, HAWK_ENOTIDXACC);
			return HAWK_NULL;
	}

#if defined(HAWK_ENABLE_GC)
	while (remidx)
	{
		hawk_val_type_t container_vtype;

		if (vtype == HAWK_VAL_MAP)
		{
			hawk_map_pair_t* pair;
			pair = hawk_map_search(map, str, len);
			v = pair? (hawk_val_t*)HAWK_MAP_VPTR(pair): hawk_val_nil;
		}
		else
		{
			v = (idx < HAWK_ARR_SIZE(arr) && HAWK_ARR_SLOT(arr, idx))? ((hawk_val_t*)HAWK_ARR_DPTR(arr, idx)): hawk_val_nil;
		}
		container_vtype = vtype;
		vtype = HAWK_RTX_GETVALTYPE(rtx, v);

		switch (vtype)
		{
			case HAWK_VAL_MAP:
			val_map:
				if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
				len = HAWK_COUNTOF(idxbuf);
				str = idxnde_to_str(rtx, remidx, idxbuf, &len, &remidx, HAWK_NULL);
				if (HAWK_UNLIKELY(!str)) goto oops;
				map = ((hawk_val_map_t*)v)->map;
				break;

			case HAWK_VAL_ARR:
			val_arr:
				idx = idxnde_to_int(rtx, remidx, &remidx);
				if (HAWK_UNLIKELY(idx <= -1)) goto oops;
				arr = ((hawk_val_arr_t*)v)->arr;
				break;

			default:
				if (vtype == HAWK_VAL_NIL /* || (rtx->hawk->opt.trait & HAWK_FLEXMAP) no flexmap because this is in a 'get' context */)
				{
					if (container_vtype == HAWK_VAL_MAP)
					{
						v = assign_newmapval_in_map(rtx, map, str, len);
						if (HAWK_UNLIKELY(!v)) { ADJERR_LOC (rtx, &var->loc); goto oops; }
						vtype = HAWK_VAL_MAP;
						goto val_map;
					}
					else
					{
						v = assign_newarrval_in_arr(rtx, arr, idx);
						if (HAWK_UNLIKELY(!v)) { ADJERR_LOC (rtx, &var->loc); goto oops; }
						vtype = HAWK_VAL_ARR;
						goto val_arr;
					}
				}

				hawk_rtx_seterrnum (rtx, &var->loc, HAWK_ENOTIDXACC);
				goto oops;
		}
	}
#endif

	if (vtype == HAWK_VAL_MAP)
	{
		hawk_map_pair_t* pair;
		pair = hawk_map_search(map, str, len);
		if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
		return pair? (hawk_val_t*)HAWK_MAP_VPTR(pair): hawk_val_nil;
	}
	else
	{
		/* return nil if the index is out of range or the element at the index is not set.
		 * no check for a negative index as it's guaranteed to be positive by idxnde_to_int() */
		if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
		return (idx < HAWK_ARR_SIZE(arr) && HAWK_ARR_SLOT(arr, idx))? ((hawk_val_t*)HAWK_ARR_DPTR(arr, idx)): hawk_val_nil;
	}

oops:
	if (str && str != idxbuf) hawk_rtx_freemem (rtx, str);
	return HAWK_NULL;


}

static hawk_val_t* eval_namedidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde);
}

static hawk_val_t* eval_gblidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde);
}

static hawk_val_t* eval_lclidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde);
}

static hawk_val_t* eval_argidx (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return eval_indexed(rtx, (hawk_nde_var_t*)nde);
}

static hawk_val_t* eval_pos (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_pos_t* pos = (hawk_nde_pos_t*)nde;
	hawk_val_t* v;
	hawk_int_t lv;
	int n;

	v = eval_expression(rtx, pos->val);
	if (HAWK_UNLIKELY(!v)) return HAWK_NULL;

	hawk_rtx_refupval (rtx, v);
	n = hawk_rtx_valtoint(rtx, v, &lv);
	hawk_rtx_refdownval (rtx, v);
	if (n <= -1) 
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EPOSIDX);
		return HAWK_NULL;
	}

	if (lv < 0)
	{
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EPOSIDX);
		return HAWK_NULL;
	}

	v = POS_VAL(rtx, lv);
#if 0
	if (lv == 0) v = rtx->inrec.d0;
	else if (lv > 0 && lv <= (hawk_int_t)rtx->inrec.nflds) 
		v = rtx->inrec.flds[lv-1].val;
	else v = hawk_val_zls; /*hawk_val_nil;*/
#endif

	return v;
}

static hawk_val_t* __eval_getline (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_getline_t* p;
	hawk_val_t* v = HAWK_NULL, * tmp;
	hawk_oocs_t dst;
	hawk_ooecs_t* buf;
	int n, x;

	p = (hawk_nde_getline_t*)nde;

	HAWK_ASSERT (
		(p->in_type == HAWK_IN_PIPE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_RWPIPE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_FILE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_CONSOLE && p->in == HAWK_NULL));

	if (p->in)
	{
		v = io_nde_to_str(rtx, p->in, &dst, 0);
		if (!v || dst.len <= 0) 
		{
			hawk_rtx_freevaloocstr (rtx, v, dst.ptr);
			hawk_rtx_refdownval (rtx, v);
			n = -1;
			goto skip_read;
		}
	}
	else
	{
		dst.ptr = (hawk_ooch_t*)HAWK_T("");
		dst.len = 0;
	}

	buf = &rtx->inrec.lineg;
read_console_again:
	hawk_ooecs_clear (&rtx->inrec.lineg);

	n = hawk_rtx_readio(rtx, p->in_type, dst.ptr, buf);

	if (v) 
	{
		hawk_rtx_freevaloocstr (rtx, v, dst.ptr);
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
			HAWK_ASSERT (p->in == HAWK_NULL);
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
			x = hawk_rtx_setrec(rtx, 0, HAWK_OOECS_OOCS(buf), 1);
			if (x <= -1) return HAWK_NULL;
		}
		else
		{
			hawk_val_t* v;

			/* treat external input numerically if it can compose a number. */
			/*v = hawk_rtx_makestrvalwithoocs(rtx, HAWK_OOECS_OOCS(buf));*/
			v = hawk_rtx_makenumorstrvalwithoochars(rtx, HAWK_OOECS_PTR(buf), HAWK_OOECS_LEN(buf));
			if (HAWK_UNLIKELY(!v))
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
	tmp = hawk_rtx_makeintval(rtx, n);
	if (!tmp) ADJERR_LOC (rtx, &nde->loc);
	return tmp;
}

static hawk_val_t* __eval_getbline (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	hawk_nde_getline_t* p;
	hawk_val_t* v = HAWK_NULL, * tmp;
	hawk_oocs_t dst;
	hawk_becs_t* buf;
	int n;

	p = (hawk_nde_getline_t*)nde;

	HAWK_ASSERT (
		(p->in_type == HAWK_IN_PIPE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_RWPIPE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_FILE && p->in != HAWK_NULL) ||
		(p->in_type == HAWK_IN_CONSOLE && p->in == HAWK_NULL));

	if (p->in)
	{
		v = io_nde_to_str(rtx, p->in, &dst, 0);
		if (!v || dst.len <= 0) 
		{
			hawk_rtx_freevaloocstr (rtx, v, dst.ptr);
			hawk_rtx_refdownval (rtx, v);
			n = -1;
			goto skip_read;
		}
	}
	else
	{
		dst.ptr = (hawk_ooch_t*)HAWK_T("");
		dst.len = 0;
	}

	buf = &rtx->inrec.linegb;
read_console_again:
	hawk_becs_clear (&rtx->inrec.linegb);

	n = hawk_rtx_readiobytes(rtx, p->in_type, dst.ptr, buf);

	if (v) 
	{
		hawk_rtx_freevaloocstr (rtx, v, dst.ptr);
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
			HAWK_ASSERT (p->in == HAWK_NULL);
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
			/*x = hawk_rtx_setbrec(rtx, 0, HAWK_BECS_BCS(buf));
			if (x <= -1) return HAWK_NULL;*/
/* TODO: can i support this? */
			hawk_rtx_seterrfmt(rtx, &nde->loc, HAWK_ENOIMPL, HAWK_T("getbline without a variable not supported"));
			return HAWK_NULL;
		}
		else
		{
			hawk_val_t* v;

			/* treat external input numerically if it can compose a number. */
			/*v = hawk_rtx_makembsvalwithbcs(rtx, HAWK_BECS_BCS(buf));*/
			v = hawk_rtx_makenumormbsvalwithbchars(rtx, HAWK_BECS_PTR(buf), HAWK_BECS_LEN(buf));
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
	tmp = hawk_rtx_makeintval(rtx, n);
	if (!tmp) ADJERR_LOC (rtx, &nde->loc);
	return tmp;
}

static hawk_val_t* eval_getline (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	return ((hawk_nde_getline_t*)nde)->mbs? __eval_getbline(rtx, nde): __eval_getline(rtx, nde);
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

static int read_record (hawk_rtx_t* rtx)
{
	hawk_ooi_t n;
	hawk_ooecs_t* buf;

read_again:
	if (hawk_rtx_clrrec(rtx, 0) <= -1) return -1;

	buf = &rtx->inrec.line;
	n = hawk_rtx_readio(rtx, HAWK_IN_CONSOLE, HAWK_T(""), buf);
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
		HAWK_ASSERT (HAWK_OOECS_LEN(buf) == 0);
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

	if (hawk_rtx_setrec(rtx, 0, HAWK_OOECS_OOCS(buf), 1) <= -1 ||
	    update_fnr(rtx, rtx->gbl.fnr + 1, rtx->gbl.nr + 1) <= -1) return -1;

	return 1;
}

static hawk_ooch_t* idxnde_to_str (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_ooch_t* buf, hawk_oow_t* len, hawk_nde_t** remidx, hawk_int_t* firstidxint)
{
	hawk_ooch_t* str;
	hawk_val_t* idx;
	hawk_int_t idxint;

	HAWK_ASSERT (nde != HAWK_NULL);

	if (!nde->next)
	{
		hawk_rtx_valtostr_out_t out;

		/* single node index */
		idx = eval_expression(rtx, nde);
		if (HAWK_UNLIKELY(!idx)) return HAWK_NULL;

		hawk_rtx_refupval (rtx, idx);

		if (firstidxint)
		{
			if (hawk_rtx_valtoint(rtx, idx, &idxint) <= -1)
			{
				hawk_rtx_refdownval (rtx, idx);
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}
		}

		str = HAWK_NULL;

		if (buf)
		{
			/* try with a fixed-size buffer if given */
			out.type = HAWK_RTX_VALTOSTR_CPLCPY;
			out.u.cplcpy.ptr = buf;
			out.u.cplcpy.len = *len;

			if (hawk_rtx_valtostr(rtx, idx, &out) >= 0)
			{
				str = out.u.cplcpy.ptr;
				*len = out.u.cplcpy.len;
				HAWK_ASSERT (str == buf);
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
		*remidx = HAWK_NULL;
	}
	else
	{
		/* multidimensional index - e.g. [1,2,3] */
		hawk_ooecs_t idxstr;
		hawk_oocs_t tmp;
		hawk_rtx_valtostr_out_t out;
		hawk_nde_t* xnde;
		int first = 1;

		out.type = HAWK_RTX_VALTOSTR_STRPCAT;
		out.u.strpcat = &idxstr;

		if (hawk_ooecs_init(&idxstr, hawk_rtx_getgem(rtx), DEF_BUF_CAPA) <= -1) 
		{
			ADJERR_LOC (rtx, &nde->loc);
			return HAWK_NULL;
		}

		xnde = nde;
#if defined(HAWK_ENABLE_GC)
		while (nde && nde->type != HAWK_NDE_NULL)
#else
		while (nde)
#endif
		{
			idx = eval_expression(rtx, nde);
			if (HAWK_UNLIKELY(!idx)) 
			{
				hawk_ooecs_fini (&idxstr);
				return HAWK_NULL;
			}

			hawk_rtx_refupval (rtx, idx);

			if (firstidxint && first)
			{
				if (hawk_rtx_valtoint(rtx, idx, &idxint) <= -1)
				{
					hawk_rtx_refdownval (rtx, idx);
					hawk_ooecs_fini (&idxstr);
					ADJERR_LOC (rtx, &nde->loc);
					return HAWK_NULL;
				}

				first = 0;
			}

			if (xnde != nde && hawk_ooecs_ncat(&idxstr, rtx->gbl.subsep.ptr, rtx->gbl.subsep.len) == (hawk_oow_t)-1)
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
				ADJERR_LOC (rtx, &nde->loc);
				return HAWK_NULL;
			}

			hawk_rtx_refdownval (rtx, idx);
			nde = nde->next;
		}

		hawk_ooecs_yield (&idxstr, &tmp, 0);
		str = tmp.ptr;
		*len = tmp.len;

		hawk_ooecs_fini (&idxstr);

		/* if nde is not HAWK_NULL, it should be of the HAWK_NDE_NULL type */
		*remidx = nde? nde->next: nde;
	}

	if (firstidxint) *firstidxint = idxint;
	return str;
}

static hawk_ooi_t idxnde_to_int (hawk_rtx_t* rtx, hawk_nde_t* nde, hawk_nde_t** remidx)
{
	hawk_int_t v;
	hawk_val_t* tmp;
	int n;

	if (nde->next && nde->next->type != HAWK_NDE_NULL)
	{
		/* multidimensional indices inside a single brakcet is not allowed for an array */
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EARRIDXMULTI);
		return -1;
	}

	tmp = eval_expression(rtx, nde);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_valtoint(rtx, tmp, &v);
	hawk_rtx_refdownval (rtx, tmp);
	if (HAWK_UNLIKELY(n <= -1))
	{
		ADJERR_LOC (rtx, &nde->loc);
		return -1;
	}

	if (v < 0 || v > HAWK_QINT_MAX) 
	{
		/* array index out of permitted range  */
		hawk_rtx_seterrnum (rtx, &nde->loc, HAWK_EARRIDXRANGE);
		return -1;
	}

	*remidx = nde->next? nde->next->next: HAWK_NULL;
	return (hawk_ooi_t)v;
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
	HAWK_ASSERT (rtx->format.tmp.ptr != HAWK_NULL);

	if (nargs_on_stack == (hawk_oow_t)-1) 
	{
		/* dirty hack to support a single value argument instead of a tree node */
		val = (hawk_val_t*)args;
		nargs_on_stack = 2; /* indicate 2 arguments of a formatting specifier and the given value */
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
		wp[WP_WIDTH] = 0; /* width */
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

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &wp[wp_idx]);
			hawk_rtx_refdownval (rtx, v);
			if (HAWK_UNLIKELY(n <= -1)) return HAWK_NULL; 

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

			if (!args || val) stack_arg_idx++;
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
			FMT_CHAR (fmt[i]); i++;
			wp[WP_PRECISION] = 0;
			wp_idx = WP_PRECISION; /* change index to precision */
			goto wp_mod_main;
		}

		if (i >= fmt_len) break;

		if (wp[WP_WIDTH] < 0)
		{
			wp[WP_WIDTH] = -wp[WP_WIDTH];
			flags |= FLAG_MINUS;
		}

		if (fmt[i] == 'd' || fmt[i] == 'i' || 
		    fmt[i] == 'x' || fmt[i] == 'X' ||
		    fmt[i] == 'b' || fmt[i] == 'B' ||
		    fmt[i] == 'o' || fmt[i] == 'u')
		{
			hawk_val_t* v;
			hawk_int_t l;
			int n;

			int fmt_flags;
			int fmt_uint = 0;
			int fmt_width;
			hawk_ooch_t fmt_fill = HAWK_T('\0');
			const hawk_ooch_t* fmt_prefix = HAWK_NULL;

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &l);
			hawk_rtx_refdownval (rtx, v);
			if (HAWK_UNLIKELY(n <= -1)) return HAWK_NULL; 

			fmt_flags = HAWK_FMT_INTMAX_NOTRUNC | HAWK_FMT_INTMAX_NONULL;

			if (l == 0 && wp_idx == WP_PRECISION && wp[WP_PRECISION] == 0)
			{
				/* printf ("%.d", 0); printf ("%.0d", 0); printf ("%.*d", 0, 0); */
				/* A zero value with a precision of zero produces no character. */
				fmt_flags |= HAWK_FMT_INTMAX_NOZERO;
			}

			if (wp[WP_WIDTH] > 0)
			{
				/* justification for width greater than 0 */
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
						if (wp_idx != WP_PRECISION) /* if precision is not specified, wp_idx is at WP_WIDTH */
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
				case 'B':
				case 'b':
					fmt_flags |= 2;
					fmt_uint = 1;
					if (l && (flags & FLAG_HASH)) 
					{
						/* A nonzero value is prefixed with 0b */
						fmt_prefix = HAWK_T("0b");
					}
					break;

				case 'X':
					fmt_flags |= HAWK_FMT_INTMAX_UPPERCASE;
				case 'x':
					fmt_flags |= 16;
					fmt_uint = 1;
					if (l && (flags & FLAG_HASH)) 
					{
						/* A nonzero value is prefixed with 0x */
						fmt_prefix = HAWK_T("0x");
					}
					break;

				case 'o':
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

				case 'u':
					fmt_uint = 1;
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

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
					return HAWK_NULL;
				}
				v = hawk_rtx_getarg(rtx, stack_arg_idx);
			}
			else
			{
				if (val)  /* nargs_on_stack == (hawk_oow_t)-1 */ 
				{
					if (stack_arg_idx >= nargs_on_stack)
					{
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoflt(rtx, v, &r);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL;

		#if defined(HAWK_USE_FLTMAX)
			/*FMT_CHAR (HAWK_T('j'));*/
			FMT_STR (HAWK_T("jj"), 2); /* see fmt.c for info on jj */
			FMT_CHAR (fmt[i]);
			/*if (hawk_ooecs_fcat(out, HAWK_OOECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;*/
			if (hawk_ooecs_fcat(out, HAWK_OOECS_PTR(fbu), &r) == (hawk_oow_t)-1) return HAWK_NULL;
		#else
			FMT_CHAR (HAWK_T('z'));
			FMT_CHAR (fmt[i]);
			if (hawk_ooecs_fcat(out, HAWK_OOECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;
		#endif
		}
		else if (fmt[i] == HAWK_T('c')) 
		{
			hawk_ooch_t ch;
			hawk_oow_t ch_len;
			hawk_val_t* v;
			hawk_val_type_t vtype;

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			vtype = HAWK_RTX_GETVALTYPE(rtx, v);
			switch (vtype)
			{
				case HAWK_VAL_NIL:
					ch = HAWK_T('\0');
					ch_len = 0;
					break;

				case HAWK_VAL_CHAR:
					ch = (hawk_ooch_t)HAWK_RTX_GETCHARFROMVAL(rtx, v);
					ch_len = 1;
					break;

				case HAWK_VAL_BCHR:
					ch = (hawk_ooch_t)HAWK_RTX_GETBCHRFROMVAL(rtx, v);
					ch_len = 1;
					break;

				case HAWK_VAL_INT:
					ch = (hawk_ooch_t)HAWK_RTX_GETINTFROMVAL(rtx, v);
					ch_len = 1;
					break;

				case HAWK_VAL_FLT:
					ch = (hawk_ooch_t)((hawk_val_flt_t*)v)->val;
					ch_len = 1;
					break;

				case HAWK_VAL_STR:
					/* printf("%c", "") => produces '\0' character */
					ch = (((hawk_val_str_t*)v)->val.len  > 0)? ((hawk_val_str_t*)v)->val.ptr[0]: '\0';
					ch_len = 1;
					break;

				case HAWK_VAL_MBS:
					ch = (((hawk_val_mbs_t*)v)->val.len > 0)? ((hawk_val_mbs_t*)v)->val.ptr[0]: '\0';
					ch_len = 1;
					break;

				default:
					hawk_rtx_refdownval (rtx, v);
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EVALTOCHR);
					return HAWK_NULL;
			}

			if (wp[WP_PRECISION] <= 0 || wp[WP_PRECISION] > (hawk_int_t)ch_len) 
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
		else if (fmt[i] == 's' || fmt[i] == 'k' || fmt[i] == 'K' || fmt[i] == 'w' || fmt[i] == 'W') 
		{
			hawk_val_t* v;

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			if (val)
			{
				/* val_flt_to_str() in val.c calls hawk_rtx_format() with nargs_on_stack of (hawk_oow_t)-1 and the actual value.
				 * the actual value is assigned to 'val' at the beginning of this function.
				 *
				 * the following code can drive here.
				 *     BEGIN { CONVFMT="%s"; a=98.76 ""; }
				 *
				 * when the first attempt to convert 98.76 to a textual form invokes this function with %s and 98.76.
				 * it comes to this part because the format specifier is 's'. since the floating-point type is not
				 * specially handled, hawk_rtx_valtooocstrdup() is called below. it calls val_flt_to_str() again, 
				 * which eventually creates recursion and stack depletion.
				 *
				 * assuming only val_flt_to_str() calls it this way, i must convert the floating point number
				 * to text in a rather crude way without calling hawk_rtx_valtooocstrdup().
				 */
				hawk_flt_t r;
				int n;

				HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_FLT);

				hawk_rtx_refupval (rtx, v);
				n = hawk_rtx_valtoflt(rtx, v, &r);
				hawk_rtx_refdownval (rtx, v);
				if (n <= -1) return HAWK_NULL;

				/* format the value as if '%g' is given */
			#if defined(HAWK_USE_FLTMAX)
				FMT_STR (HAWK_T("jjg"), 3); /* see fmt.c for info on jj */
				if (hawk_ooecs_fcat(out, HAWK_OOECS_PTR(fbu), &r) == (hawk_oow_t)-1) return HAWK_NULL;
			#else
				FMT_STR (HAWK_T("zg"), 2);
				if (hawk_ooecs_fcat(out, HAWK_OOECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;
			#endif
			}
			else
			{
				hawk_ooch_t* str_ptr, * str_free = HAWK_NULL, ooch_tmp;
				hawk_bch_t bch_tmp;
				hawk_oow_t str_len;
				hawk_int_t k;
				hawk_val_type_t vtype;
				int bytetostr_flagged_radix = 16;

				hawk_rtx_refupval (rtx, v);

				vtype = HAWK_RTX_GETVALTYPE(rtx, v);
				switch (vtype)
				{
					case HAWK_VAL_NIL:
						str_ptr = HAWK_T("");
						str_len = 0;
						break;

					case HAWK_VAL_CHAR:
						ooch_tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
						str_ptr = &ooch_tmp;
						str_len = 1;
						break;

					case HAWK_VAL_STR:
						str_ptr = ((hawk_val_str_t*)v)->val.ptr;
						str_len = ((hawk_val_str_t*)v)->val.len;
						break;

					case HAWK_VAL_BCHR:
					#if defined(HAWK_OOCH_IS_BCH)
						ooch_tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
						str_ptr = &ooch_tmp;
						str_len = 1;
					#else
						if (fmt[i] == HAWK_T('s')) goto duplicate;
						bch_tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
						str_ptr = (hawk_ooch_t*)&bch_tmp;
						str_len = 1;
					#endif
						break;

					case HAWK_VAL_MBS:
					#if defined(HAWK_OOCH_IS_BCH)
						str_ptr = ((hawk_val_mbs_t*)v)->val.ptr;
						str_len = ((hawk_val_mbs_t*)v)->val.len;
					#else
						if (fmt[i] == HAWK_T('s')) goto duplicate;
						str_ptr = (hawk_ooch_t*)((hawk_val_mbs_t*)v)->val.ptr;
						str_len = ((hawk_val_mbs_t*)v)->val.len;
					#endif
						break;

					default:
					duplicate:
						str_ptr = hawk_rtx_valtooocstrdup(rtx, v, &str_len);
						if (!str_ptr)
						{
							hawk_rtx_refdownval (rtx, v);
							return HAWK_NULL;
						}

						str_free = str_ptr;
						break;
					
				}

				if (wp_idx != WP_PRECISION || wp[WP_PRECISION] <= -1 || wp[WP_PRECISION] > (hawk_int_t)str_len) 
				{
					/* precision not specified, or specified to a negative value or greater than the actual length */
					wp[WP_PRECISION] = (hawk_int_t)str_len;
				}
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

				if (fmt[i] == 'k' || fmt[i] == 'w') bytetostr_flagged_radix |= HAWK_BYTE_TO_BCSTR_LOWERCASE;

				for (k = 0; k < wp[WP_PRECISION]; k++) 
				{
					hawk_ooch_t curc;

				#if defined(HAWK_OOCH_IS_BCH)
					curc = str_ptr[k];
				#else
					if ((vtype == HAWK_VAL_MBS || vtype == HAWK_VAL_BCHR) && fmt[i] != HAWK_T('s')) 
						curc = (hawk_uint8_t)((hawk_bch_t*)str_ptr)[k];
					else curc = str_ptr[k];
				#endif

					if ((fmt[i] != 's' && !HAWK_BYTE_PRINTABLE(curc)) || fmt[i] == 'w' || fmt[i] == 'W')
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
		}
		else 
		{
			if (fmt[i] != HAWK_T('%')) OUT_STR (HAWK_OOECS_PTR(fbu), HAWK_OOECS_LEN(fbu));
			OUT_CHAR (fmt[i]);
			goto skip_taking_arg;
		}

		if (!args || val) stack_arg_idx++;
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
	if (HAWK_UNLIKELY(!(buf)->ptr)) \
	{ \
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
	if (HAWK_UNLIKELY(!(buf)->ptr)) \
	{ \
		(buf)->len = 0; \
		return HAWK_NULL; \
	} \
} while(0)

	/* run->format.tmp.ptr should have been assigned a pointer to a block of memory before this function has been called */
	HAWK_ASSERT (rtx->formatmbs.tmp.ptr != HAWK_NULL);

	if (nargs_on_stack == (hawk_oow_t)-1) 
	{
		/* dirty hack to support a single value argument instead of a tree node */
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
		wp[WP_WIDTH] = 0; /* width */
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

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &wp[wp_idx]);
			hawk_rtx_refdownval (rtx, v);
			if (HAWK_UNLIKELY(n <= -1)) return HAWK_NULL; 

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

			if (!args || val) stack_arg_idx++;
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
			FMT_MCHAR (fmt[i]); i++;

			wp[WP_PRECISION] = 0;
			wp_idx = WP_PRECISION; /* change index to precision */
			goto wp_mod_main;
		}

		if (i >= fmt_len) break;

		if (wp[WP_WIDTH] < 0)
		{
			wp[WP_WIDTH] = -wp[WP_WIDTH];
			flags |= FLAG_MINUS;
		}

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

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoint(rtx, v, &l);
			hawk_rtx_refdownval (rtx, v);
			if (HAWK_UNLIKELY(n <= -1)) return HAWK_NULL; 

			fmt_flags = HAWK_FMT_INTMAX_NOTRUNC | HAWK_FMT_INTMAX_NONULL;

			if (l == 0 && wp_idx == WP_PRECISION && wp[WP_PRECISION] == 0)
			{
				/* printf ("%.d", 0); printf ("%.0d", 0); printf ("%.*d", 0, 0); */
				/* A zero value with a precision of zero produces no character. */
				fmt_flags |= HAWK_FMT_INTMAX_NOZERO;
			}

			if (wp[WP_WIDTH] > 0)
			{
				/* justification for width greater than 0 */
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
						if (wp_idx != WP_PRECISION) /* if precision is not set, wp_idx is at WP_WIDTH */
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

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			hawk_rtx_refupval (rtx, v);
			n = hawk_rtx_valtoflt(rtx, v, &r);
			hawk_rtx_refdownval (rtx, v);
			if (n <= -1) return HAWK_NULL;

		#if defined(HAWK_USE_FLTMAX)
			/*FMT_MCHAR (HAWK_BT('j'));*/
			FMT_MBS (HAWK_BT("jj"), 2); /* see fmt.c for info on jj */
			FMT_MCHAR (fmt[i]);
			/*if (hawk_becs_fcat(out, HAWK_BECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;*/
			if (hawk_becs_fcat(out, HAWK_BECS_PTR(fbu), &r) == (hawk_oow_t)-1) return HAWK_NULL;
		#else
			FMT_MCHAR (HAWK_BT('z'));
			FMT_MCHAR (fmt[i]);
			if (hawk_becs_fcat(out, HAWK_BECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;
		#endif
		}
		else if (fmt[i] == HAWK_BT('c')) 
		{
			hawk_bch_t ch;
			hawk_oow_t ch_len;
			hawk_val_t* v;
			hawk_val_type_t vtype;

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
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

				case HAWK_VAL_CHAR:
					ch = (hawk_bch_t)HAWK_RTX_GETCHARFROMVAL(rtx, v); /* the value may get truncated */
					ch_len = 1;
					break;

				case HAWK_VAL_BCHR:
					ch = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
					ch_len = 1;
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
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EVALTOCHR);
					return HAWK_NULL;
			}

			if (wp[WP_PRECISION] <= 0 || wp[WP_PRECISION] > (hawk_int_t)ch_len) 
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
		else if (fmt[i] == 's' || fmt[i] == 'k' || fmt[i] == 'K' || fmt[i] == 'w' || fmt[i] == 'W')
		{
			hawk_val_t* v;

			if (!args)
			{
				if (stack_arg_idx >= nargs_on_stack)
				{
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
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
						hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EFMTARG);
						return HAWK_NULL;
					}
					v = val;
				}
				else 
				{
					v = eval_expression(rtx, args);
					if (HAWK_UNLIKELY(!v)) return HAWK_NULL;
				}
			}

			if (val)
			{
				/* val_flt_to_str() in val.c calls hawk_rtx_format() with nargs_on_stack of (hawk_oow_t)-1 and the actual value.
				 * the actual value is assigned to 'val' at the beginning of this function.
				 *
				 * the following code can drive here.
				 *     BEGIN { CONVFMT="%s"; a=98.76 ""; }
				 *
				 * when the first attempt to convert 98.76 to a textual form invokes this function with %s and 98.76.
				 * it comes to this part because the format specifier is 's'. since the floating-point type is not
				 * specially handled, hawk_rtx_valtooocstrdup() is called below. it calls val_flt_to_str() again, 
				 * which eventually creates recursion and stack depletion.
				 *
				 * assuming only val_flt_to_str() calls it this way, i must convert the floating point number
				 * to text in a rather crude way without calling hawk_rtx_valtooocstrdup().
				 */
				hawk_flt_t r;
				int n;

				HAWK_ASSERT (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_FLT);

				hawk_rtx_refupval (rtx, v);
				n = hawk_rtx_valtoflt(rtx, v, &r);
				hawk_rtx_refdownval (rtx, v);
				if (n <= -1) return HAWK_NULL;

				/* format the value as if '%g' is given */
			#if defined(HAWK_USE_FLTMAX)
				FMT_MBS (HAWK_BT("jjg"), 3); /* see fmt.c for info on jj */
				if (hawk_becs_fcat(out, HAWK_BECS_PTR(fbu), &r) == (hawk_oow_t)-1) return HAWK_NULL;
			#else
				FMT_MBS (HAWK_BT("zg"), 2);
				if (hawk_becs_fcat(out, HAWK_BECS_PTR(fbu), r) == (hawk_oow_t)-1) return HAWK_NULL;
			#endif
			}
			else
			{
				hawk_bch_t* str_ptr, * str_free = HAWK_NULL, bchr_tmp;
				hawk_ooch_t ooch_tmp;
				hawk_oow_t str_len;
				hawk_int_t k;
				hawk_val_type_t vtype;
				int bytetombs_flagged_radix = 16;

				hawk_rtx_refupval (rtx, v);

				vtype = HAWK_RTX_GETVALTYPE(rtx, v);
				switch (vtype)
				{
					case HAWK_VAL_NIL:
						str_ptr = HAWK_BT("");
						str_len = 0;
						break;

					case HAWK_VAL_BCHR:
						bchr_tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
						str_ptr = &bchr_tmp;
						str_len = 1;
						break;

					case HAWK_VAL_MBS:
						str_ptr = ((hawk_val_mbs_t*)v)->val.ptr;
						str_len = ((hawk_val_mbs_t*)v)->val.len;
						break;

					case HAWK_VAL_CHAR:
					#if defined(HAWK_OOCH_IS_BCH)
						bchr_tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, v);
						str_ptr = &bchr_tmp;
						str_len = 1;
					#else
						if (fmt[i] == HAWK_BT('s')) goto duplicate;
						ooch_tmp = HAWK_RTX_GETCHARFROMVAL(rtx, v);
						str_ptr = (hawk_bch_t*)&ooch_tmp;
						str_len = 1 * (HAWK_SIZEOF_OOCH_T / HAWK_SIZEOF_BCH_T);
					#endif
						break;

					case HAWK_VAL_STR:
					#if defined(HAWK_OOCH_IS_BCH)
						str_ptr = ((hawk_val_str_t*)v)->val.ptr;
						str_len = ((hawk_val_str_t*)v)->val.len;
					#else
						if (fmt[i] == HAWK_BT('s')) goto duplicate;
						/* arrange to print the wide character string byte by byte regardless of byte order */
						str_ptr = (hawk_bch_t*)((hawk_val_str_t*)v)->val.ptr;
						str_len = ((hawk_val_str_t*)v)->val.len * (HAWK_SIZEOF_OOCH_T / HAWK_SIZEOF_BCH_T);
					#endif
						break;

					default:
					duplicate:
						str_ptr = hawk_rtx_valtobcstrdup(rtx, v, &str_len);
						if (!str_ptr)
						{
							hawk_rtx_refdownval (rtx, v);
							return HAWK_NULL;
						}

						str_free = str_ptr;
						break;
				}

				if (wp_idx != WP_PRECISION || wp[WP_PRECISION] <= -1 || wp[WP_PRECISION] > (hawk_int_t)str_len) 
				{
					/* precision not specified, or specified to a negative value or greater than the actual length */
					wp[WP_PRECISION] = (hawk_int_t)str_len;
				}
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

				if (fmt[i] == 'k' || fmt[i] == 'w') bytetombs_flagged_radix |= HAWK_BYTE_TO_BCSTR_LOWERCASE;

				for (k = 0; k < wp[WP_PRECISION]; k++) 
				{
					hawk_bch_t curc;

					curc = str_ptr[k];

					if ((fmt[i] != 's' && !HAWK_BYTE_PRINTABLE(curc)) || fmt[i] == 'w' || fmt[i] == 'W')
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
		}
		else 
		{
			if (fmt[i] != HAWK_BT('%')) OUT_MBS (HAWK_BECS_PTR(fbu), HAWK_BECS_LEN(fbu));
			OUT_MCHAR (fmt[i]);
			goto skip_taking_arg;
		}

		if (!args || val) stack_arg_idx++;
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

/* ------------------------------------------------------------------------ */

int hawk_rtx_buildrex (hawk_rtx_t* rtx, const hawk_ooch_t* ptn, hawk_oow_t len, hawk_tre_t** code, hawk_tre_t** icode)
{
	return hawk_gem_buildrex(hawk_rtx_getgem(rtx), ptn, len, !(rtx->hawk->opt.trait & HAWK_REXBOUND), code, icode);
}
