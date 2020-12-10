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

static const hawk_ooch_t* assop_str[] =
{
	/* this table must match hawk_assop_type_t in run.h */
	HAWK_T("="),
	HAWK_T("+="),
	HAWK_T("-="),
	HAWK_T("*="),
	HAWK_T("/="),
	HAWK_T("\\="),
	HAWK_T("%="),
	HAWK_T("**="), /* exponentation, also ^= */
	HAWK_T("%%="),
	HAWK_T(">>="),
	HAWK_T("<<="),
	HAWK_T("&="),
	HAWK_T("^^="),
	HAWK_T("|=")
};

static const hawk_ooch_t* binop_str[][2] =
{
	{ HAWK_T("||"), HAWK_T("||") },
	{ HAWK_T("&&"), HAWK_T("&&") },
	{ HAWK_T("in"), HAWK_T("in") },

	{ HAWK_T("|"),  HAWK_T("|") },
	{ HAWK_T("^^"), HAWK_T("^^") },
	{ HAWK_T("&"),  HAWK_T("&") },

	{ HAWK_T("==="), HAWK_T("===") },
	{ HAWK_T("!=="), HAWK_T("!==") },
	{ HAWK_T("=="), HAWK_T("==") },
	{ HAWK_T("!="), HAWK_T("!=") },
	{ HAWK_T(">"),  HAWK_T(">") },
	{ HAWK_T(">="), HAWK_T(">=") },
	{ HAWK_T("<"),  HAWK_T("<") },
	{ HAWK_T("<="), HAWK_T("<=") },

	{ HAWK_T("<<"), HAWK_T("<<") },
	{ HAWK_T(">>"), HAWK_T(">>") },

	{ HAWK_T("+"),  HAWK_T("+") },
	{ HAWK_T("-"),  HAWK_T("-") },
	{ HAWK_T("*"),  HAWK_T("*") },
	{ HAWK_T("/"),  HAWK_T("/") },
	{ HAWK_T("\\"), HAWK_T("\\") },
	{ HAWK_T("%"),  HAWK_T("%") },
	{ HAWK_T("**"), HAWK_T("**") }, /* exponentation, also ^ */

	{ HAWK_T(" "),  HAWK_T("%%") }, /* take note of this entry */
	{ HAWK_T("~"),  HAWK_T("~") },
	{ HAWK_T("!~"), HAWK_T("!~") }
};

static const hawk_ooch_t* unrop_str[] =
{
	HAWK_T("+"),
	HAWK_T("-"),
	HAWK_T("!"),
	HAWK_T("~")
};

static const hawk_ooch_t* incop_str[] =
{
	HAWK_T("++"),
	HAWK_T("--"),
	HAWK_T("++"),
	HAWK_T("--")
};

static const hawk_ooch_t* getline_inop_str[] =
{
	HAWK_T("|"),
	HAWK_T("||"),
	HAWK_T("<"),
	HAWK_T("")
};

static const hawk_ooch_t* print_outop_str[] =
{
	HAWK_T("|"),
	HAWK_T("||"),
	HAWK_T(">"),
	HAWK_T(">>"),
	HAWK_T("")
};

#define PUT_SRCSTR(hawk,str) do { if (hawk_putsrcoocstr (hawk, str) == -1) return -1; } while(0)

#define PUT_NL(hawk) do { \
	if (hawk->opt.trait & HAWK_CRLF) PUT_SRCSTR (hawk, HAWK_T("\r")); \
	PUT_SRCSTR (hawk, HAWK_T("\n")); \
} while(0)

#define PUT_SRCSTRN(hawk,str,len) do { \
	if (hawk_putsrcoochars (hawk, str, len) == -1) return -1; \
} while(0)

#define PRINT_TABS(hawk,depth) do { \
	if (print_tabs(hawk,depth) == -1) return -1; \
} while(0)

#define PRINT_EXPR(hawk,nde) do { \
	if (print_expr(hawk,nde) == -1) return -1; \
} while(0)

#define PRINT_EXPR_LIST(hawk,nde) do { \
	if (print_expr_list(hawk,nde) == -1) return -1; \
} while(0)

#define PRINT_EXPR_LIST_FOR_IDX(hawk,nde) do { \
	if (print_expr_list_for_idx(hawk,nde) == -1) return -1; \
} while(0)

#define PRINT_STMTS(hawk,nde,depth) do { \
	if (print_stmts(hawk,nde,depth) == -1) return -1; \
} while(0)

static int print_tabs (hawk_t* hawk, int depth);
static int print_expr (hawk_t* hawk, hawk_nde_t* nde);
static int print_expr_list (hawk_t* hawk, hawk_nde_t* tree);
static int print_expr_list_for_idx (hawk_t* hawk, hawk_nde_t* tree);
static int print_stmts (hawk_t* hawk, hawk_nde_t* tree, int depth);

static int print_tabs (hawk_t* hawk, int depth)
{
	while (depth > 0) 
	{
		PUT_SRCSTR (hawk, HAWK_T("\t"));
		depth--;
	}

	return 0;
}

static int print_printx (hawk_t* hawk, hawk_nde_print_t* px)
{
	hawk_oocs_t kw;

	if (px->type == HAWK_NDE_PRINT) 
	{
		hawk_getkwname (hawk, HAWK_KWID_PRINT, &kw);
		PUT_SRCSTRN (hawk, kw.ptr, kw.len);
	}
	else
	{
		hawk_getkwname (hawk, HAWK_KWID_PRINTF, &kw);
		PUT_SRCSTRN (hawk, kw.ptr, kw.len);
	}

	if (px->args != HAWK_NULL)
	{
		PUT_SRCSTR (hawk, HAWK_T(" "));
		PRINT_EXPR_LIST (hawk, px->args);
	}

	if (px->out != HAWK_NULL)
	{
		PUT_SRCSTR (hawk, HAWK_T(" "));
		PUT_SRCSTR (hawk, print_outop_str[px->out_type]);
		PUT_SRCSTR (hawk, HAWK_T(" "));
		PRINT_EXPR (hawk, px->out);
	}

	return 0;
}

static int print_expr (hawk_t* hawk, hawk_nde_t* nde)
{
	hawk_oocs_t kw;
	
	switch (nde->type) 
	{
		case HAWK_NDE_GRP:
		{	
			hawk_nde_t* p = ((hawk_nde_grp_t*)nde)->body;

			PUT_SRCSTR (hawk, HAWK_T("("));
			while (p != HAWK_NULL) 
			{
				PRINT_EXPR (hawk, p);
				if (p->next != HAWK_NULL) 
					PUT_SRCSTR (hawk, HAWK_T(","));
				p = p->next;
			}
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_ASS:
		{
			hawk_nde_ass_t* px = (hawk_nde_ass_t*)nde;

			PRINT_EXPR (hawk, px->left);
			PUT_SRCSTR (hawk, HAWK_T(" "));
			PUT_SRCSTR (hawk, assop_str[px->opcode]);
			PUT_SRCSTR (hawk, HAWK_T(" "));
			PRINT_EXPR (hawk, px->right);

			HAWK_ASSERT (px->right->next == HAWK_NULL);
			break;
		}

		case HAWK_NDE_EXP_BIN:
		{
			hawk_nde_exp_t* px = (hawk_nde_exp_t*)nde;

			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR (hawk, px->left);
			HAWK_ASSERT (px->left->next == HAWK_NULL);

			PUT_SRCSTR (hawk, HAWK_T(" "));
			PUT_SRCSTR (hawk, binop_str[px->opcode][(hawk->opt.trait & HAWK_BLANKCONCAT)? 0: 1]);
			PUT_SRCSTR (hawk, HAWK_T(" "));

			if (px->right->type == HAWK_NDE_ASS) 
				PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR (hawk, px->right);
			if (px->right->type == HAWK_NDE_ASS) 
				PUT_SRCSTR (hawk, HAWK_T(")"));
			HAWK_ASSERT (px->right->next == HAWK_NULL); 
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_EXP_UNR:
		{
			hawk_nde_exp_t* px = (hawk_nde_exp_t*)nde;
			HAWK_ASSERT (px->right == HAWK_NULL);

			PUT_SRCSTR (hawk, HAWK_T("("));
			PUT_SRCSTR (hawk, unrop_str[px->opcode]);
			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR (hawk, px->left);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_EXP_INCPRE:
		{
			hawk_nde_exp_t* px = (hawk_nde_exp_t*)nde;
			HAWK_ASSERT (px->right == HAWK_NULL);

			PUT_SRCSTR (hawk, incop_str[px->opcode]);
			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR (hawk, px->left);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_EXP_INCPST:
		{
			hawk_nde_exp_t* px = (hawk_nde_exp_t*)nde;
			HAWK_ASSERT (px->right == HAWK_NULL);

			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR (hawk, px->left);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			PUT_SRCSTR (hawk, incop_str[px->opcode]);
			break;
		}

		case HAWK_NDE_CND:
		{
			hawk_nde_cnd_t* px = (hawk_nde_cnd_t*)nde;

			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR (hawk, px->test);
			PUT_SRCSTR (hawk, HAWK_T(")?"));

			PRINT_EXPR (hawk, px->left);
			PUT_SRCSTR (hawk, HAWK_T(":"));
			PRINT_EXPR (hawk, px->right);
			break;
		}

		case HAWK_NDE_CHAR:
		{
			hawk_ooch_t tmp = ((hawk_nde_char_t*)nde)->val;
			hawk_ooch_t buf[16]; 

			PUT_SRCSTR (hawk, HAWK_T("\'"));
			if (tmp == '\0')
				PUT_SRCSTR (hawk, HAWK_T("\\0"));
			else if (tmp == '\'')
				PUT_SRCSTR (hawk, HAWK_T("\\'"));
			else if (hawk_is_ooch_print(tmp))
				PUT_SRCSTRN (hawk, &tmp, 1);
		#if defined(HAWK_OOCH_IS_UCH)
			else if (tmp <= 0xFFFF)
			{
				hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("\\u%04x"), (hawk_oochu_t)tmp);
				PUT_SRCSTR (hawk, buf);
			}
			else
			{
				hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("\\U%08x"), (hawk_oochu_t)tmp);
				PUT_SRCSTR (hawk, buf);
			}
		#else
			else
			{
				hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("\\x%02x"), (hawk_oochu_t)tmp);
				PUT_SRCSTR (hawk, buf);
			}
		#endif

			PUT_SRCSTR (hawk, HAWK_T("\'"));
			break;
		}

		case HAWK_NDE_BCHR:
		{
			hawk_bch_t tmp = ((hawk_nde_char_t*)nde)->val;
			hawk_ooch_t buf[16]; 

			PUT_SRCSTR (hawk, HAWK_T("@b\'"));
			if (tmp == '\0')
				PUT_SRCSTR (hawk, HAWK_T("\\0"));
			else if (tmp == '\'')
				PUT_SRCSTR (hawk, HAWK_T("\\'"));
			else if (hawk_is_bch_print(tmp))
			{
				hawk_ooch_t oc = (hawk_bchu_t)tmp;
				PUT_SRCSTRN (hawk, &oc, 1);
			}
			else
			{
				hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("\\x%02x"), (hawk_bchu_t)tmp);
				PUT_SRCSTR (hawk, buf);
			}

			PUT_SRCSTR (hawk, HAWK_T("\'"));
			break;
		}

		case HAWK_NDE_INT:
		{
			if (((hawk_nde_int_t*)nde)->str)
			{
				PUT_SRCSTRN (hawk, ((hawk_nde_int_t*)nde)->str, ((hawk_nde_int_t*)nde)->len);
			}
			else
			{
				/* Note that the array sizing fomula is not accurate 
				 * but should be good enoug consiering the followings.
				 *
				 * size minval                               digits  sign
				 *   1  -128                                      3  1
				 *   2  -32768                                    5  1
				 *   4  -2147483648                              10  1
				 *   8  -9223372036854775808                     19  1
				 *  16  -170141183460469231731687303715884105728 39  1
				 */
				hawk_ooch_t buf[HAWK_SIZEOF(hawk_int_t) * 3 + 2]; 

				hawk_fmt_intmax_to_oocstr (
					buf, HAWK_COUNTOF(buf),
					((hawk_nde_int_t*)nde)->val,
					10, 
					-1,
					HAWK_T('\0'),
					HAWK_NULL
				);
				PUT_SRCSTR (hawk, buf);
			}
			break;
		}

		case HAWK_NDE_FLT:
		{
			if (((hawk_nde_flt_t*)nde)->str)
			{
				PUT_SRCSTRN (hawk, ((hawk_nde_flt_t*)nde)->str, ((hawk_nde_flt_t*)nde)->len);
			}
			else
			{
				hawk_ooch_t buf[96];

			#if defined(HAWK_USE_FLTMAX)
				/*hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("%jf"), ((hawk_nde_flt_t*)nde)->val);*/
				hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("%jjf"), &((hawk_nde_flt_t*)nde)->val);
			#else
				hawk_fmttooocstr (hawk, buf, HAWK_COUNTOF(buf), HAWK_T("%zf"), ((hawk_nde_flt_t*)nde)->val);
			#endif
				PUT_SRCSTR (hawk, buf);
			}
			break;
		}

		case HAWK_NDE_STR:
		{
			hawk_ooch_t* ptr;
			hawk_oow_t len, i;

			PUT_SRCSTR (hawk, HAWK_T("\""));

			ptr = ((hawk_nde_str_t*)nde)->ptr;
			len = ((hawk_nde_str_t*)nde)->len;
			for (i = 0; i < len; i++)
			{
				/* TODO: maybe more de-escaping?? */
				switch (ptr[i])
				{
					case HAWK_T('\n'):
						PUT_SRCSTR (hawk, HAWK_T("\\n"));
						break;
					case HAWK_T('\r'):
						PUT_SRCSTR (hawk, HAWK_T("\\r"));
						break;
					case HAWK_T('\t'):
						PUT_SRCSTR (hawk, HAWK_T("\\t"));
						break;
					case HAWK_T('\f'):
						PUT_SRCSTR (hawk, HAWK_T("\\f"));
						break;
					case HAWK_T('\b'):
						PUT_SRCSTR (hawk, HAWK_T("\\b"));
						break;
					case HAWK_T('\v'):
						PUT_SRCSTR (hawk, HAWK_T("\\v"));
						break;
					case HAWK_T('\a'):
						PUT_SRCSTR (hawk, HAWK_T("\\a"));
						break;
					case HAWK_T('\0'):
						PUT_SRCSTR (hawk, HAWK_T("\\0"));
						break;
					case HAWK_T('\"'):
						PUT_SRCSTR (hawk, HAWK_T("\\\""));
						break;
					case HAWK_T('\\'):
						PUT_SRCSTR (hawk, HAWK_T("\\\\"));
						break;
					default:
						PUT_SRCSTRN (hawk, &ptr[i], 1);
						break;
				}
			}
			PUT_SRCSTR (hawk, HAWK_T("\""));
			break;
		}

		case HAWK_NDE_MBS:
		{
			hawk_bch_t* ptr;
			hawk_oow_t len, i;

			PUT_SRCSTR (hawk, HAWK_T("@b\""));
			ptr = ((hawk_nde_mbs_t*)nde)->ptr;
			len = ((hawk_nde_mbs_t*)nde)->len;
			for (i = 0; i < len; i++)
			{
				/* TODO: maybe more de-escaping?? */
				switch (ptr[i])
				{
					case HAWK_BT('\n'):
						PUT_SRCSTR (hawk, HAWK_T("\\n"));
						break;
					case HAWK_BT('\r'):
						PUT_SRCSTR (hawk, HAWK_T("\\r"));
						break;
					case HAWK_BT('\t'):
						PUT_SRCSTR (hawk, HAWK_T("\\t"));
						break;
					case HAWK_BT('\f'):
						PUT_SRCSTR (hawk, HAWK_T("\\f"));
						break;
					case HAWK_BT('\b'):
						PUT_SRCSTR (hawk, HAWK_T("\\b"));
						break;
					case HAWK_BT('\v'):
						PUT_SRCSTR (hawk, HAWK_T("\\v"));
						break;
					case HAWK_BT('\a'):
						PUT_SRCSTR (hawk, HAWK_T("\\a"));
						break;
					case HAWK_BT('\0'):
						PUT_SRCSTR (hawk, HAWK_T("\\0"));
						break;
					case HAWK_BT('\"'):
						PUT_SRCSTR (hawk, HAWK_T("\\\""));
						break;
					case HAWK_BT('\\'):
						PUT_SRCSTR (hawk, HAWK_T("\\\\"));
						break;
					default:
					{
					#if defined(HAWK_OOCH_IS_BCH)
						PUT_SRCSTRN (hawk, &ptr[i], 1);
					#else
						hawk_ooch_t oc = (hawk_bchu_t)ptr[i];
						if (HAWK_BYTE_PRINTABLE(oc))
						{
							PUT_SRCSTRN (hawk, &oc, 1);
						}
						else
						{
							hawk_bch_t xbuf[3];
							hawk_byte_to_bcstr (oc, xbuf, HAWK_COUNTOF(xbuf), 16, '0');
							PUT_SRCSTR (hawk, HAWK_T("\\x"));
							oc = (hawk_bchu_t)xbuf[0]; PUT_SRCSTRN (hawk, &oc, 1);
							oc = (hawk_bchu_t)xbuf[1]; PUT_SRCSTRN (hawk, &oc, 1);
						}
					#endif
						break;
					}
				}
			}
			PUT_SRCSTR (hawk, HAWK_T("\""));
			break;
		}

		case HAWK_NDE_REX:
		{
			PUT_SRCSTR (hawk, HAWK_T("/"));
			PUT_SRCSTRN (hawk,
				((hawk_nde_rex_t*)nde)->str.ptr, 
				((hawk_nde_rex_t*)nde)->str.len);
			PUT_SRCSTR (hawk, HAWK_T("/"));
			break;
		}

		case HAWK_NDE_XNIL:
		{
			PUT_SRCSTR (hawk, HAWK_T("@nil"));
			break;
		}

		case HAWK_NDE_FUN:
		{
			PUT_SRCSTRN (hawk,
				((hawk_nde_fun_t*)nde)->name.ptr, 
				((hawk_nde_fun_t*)nde)->name.len);
			break;
		}

		case HAWK_NDE_ARG:
		{
			hawk_ooch_t tmp[HAWK_SIZEOF(hawk_int_t)*8+2]; 
			hawk_oow_t n;
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;
			HAWK_ASSERT (px->id.idxa != (hawk_oow_t)-1);

			n = hawk_int_to_oocstr(px->id.idxa, 10, HAWK_NULL, tmp, HAWK_COUNTOF(tmp));

			PUT_SRCSTR (hawk, HAWK_T("__p"));
			PUT_SRCSTRN (hawk, tmp, n);

			HAWK_ASSERT (px->idx == HAWK_NULL);
			break;
		}

		case HAWK_NDE_ARGIDX:
		{
			hawk_oow_t n;
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;
			HAWK_ASSERT (px->id.idxa != (hawk_oow_t)-1);
			HAWK_ASSERT (px->idx != HAWK_NULL);

			PUT_SRCSTR (hawk, HAWK_T("__p"));
			n = hawk_int_to_oocstr(px->id.idxa, 10, HAWK_NULL, hawk->tmp.fmt, HAWK_COUNTOF(hawk->tmp.fmt));
			PUT_SRCSTRN (hawk, hawk->tmp.fmt, n);
			PRINT_EXPR_LIST_FOR_IDX (hawk, px->idx);
			break;
		}

		case HAWK_NDE_NAMED:
		{
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;
			HAWK_ASSERT (px->id.idxa == (hawk_oow_t)-1);
			HAWK_ASSERT (px->idx == HAWK_NULL);

			PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
			break;
		}

		case HAWK_NDE_NAMEDIDX:
		{
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;
			HAWK_ASSERT (px->id.idxa == (hawk_oow_t)-1);
			HAWK_ASSERT (px->idx != HAWK_NULL);

			PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
			PRINT_EXPR_LIST_FOR_IDX (hawk, px->idx);
			break;
		}

		case HAWK_NDE_GBL:
		{
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;

			if (px->id.idxa != (hawk_oow_t)-1) 
			{
				/* deparsing is global. so i can't honor hawk->parse.pragma.trait 
				 * which can change in each input file. let me just check hawk->opt.trait */
				if (!(hawk->opt.trait & HAWK_IMPLICIT)) 
				{
					/* no implicit(named) variable is allowed.
					 * use the actual name */
					PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
				}
				else if (px->id.idxa < hawk->tree.ngbls_base)
				{
					/* static global variables */
					PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
				}
				else
				{
					hawk_ooch_t tmp[HAWK_SIZEOF(hawk_int_t)*8+2]; 
					hawk_oow_t n;

					PUT_SRCSTR (hawk, HAWK_T("__g"));
					n = hawk_int_to_oocstr(px->id.idxa, 10, HAWK_NULL, tmp, HAWK_COUNTOF(tmp));
					PUT_SRCSTRN (hawk, tmp, n);
				}
			}
			else 
			{
				PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
			}
			HAWK_ASSERT (px->idx == HAWK_NULL);
			break;
		}

		case HAWK_NDE_GBLIDX:
		{
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;

			if (px->id.idxa != (hawk_oow_t)-1) 
			{
				/* deparsing is global. so i can't honor hawk->parse.pragma.trait 
				 * which can change in each input file. let me just check hawk->opt.trait */
				if (!(hawk->opt.trait & HAWK_IMPLICIT))
				{
					/* no implicit(named) variable is allowed.
					 * use the actual name */
					PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
				}
				else if (px->id.idxa < hawk->tree.ngbls_base)
				{
					/* static global variables */
					PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
				}
				else
				{
					hawk_ooch_t tmp[HAWK_SIZEOF(hawk_int_t)*8+2]; 
					hawk_oow_t n;

					PUT_SRCSTR (hawk, HAWK_T("__g"));
					n = hawk_int_to_oocstr(px->id.idxa, 10, HAWK_NULL, tmp, HAWK_COUNTOF(tmp));
					PUT_SRCSTRN (hawk, tmp, n);
				}
			}
			else 
			{
				PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
			}
			HAWK_ASSERT (px->idx != HAWK_NULL);
			PRINT_EXPR_LIST_FOR_IDX (hawk, px->idx);
			break;
		}


		case HAWK_NDE_LCL:
		{
			hawk_oow_t n;
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;

			if (px->id.idxa != (hawk_oow_t)-1) 
			{
				PUT_SRCSTR (hawk, HAWK_T("__l"));
				n = hawk_int_to_oocstr(px->id.idxa, 10, HAWK_NULL, hawk->tmp.fmt, HAWK_COUNTOF(hawk->tmp.fmt));
				PUT_SRCSTRN (hawk, hawk->tmp.fmt, n);
			}
			else 
			{
				PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
			}
			HAWK_ASSERT (px->idx == HAWK_NULL);
			break;
		}

		case HAWK_NDE_LCLIDX:
		{
			hawk_oow_t n;
			hawk_nde_var_t* px = (hawk_nde_var_t*)nde;

			if (px->id.idxa != (hawk_oow_t)-1) 
			{
				PUT_SRCSTR (hawk, HAWK_T("__l"));
				n = hawk_int_to_oocstr(px->id.idxa, 10, HAWK_NULL, hawk->tmp.fmt, HAWK_COUNTOF(hawk->tmp.fmt));
				PUT_SRCSTRN (hawk, hawk->tmp.fmt, n);
			}
			else 
			{
				PUT_SRCSTRN (hawk, px->id.name.ptr, px->id.name.len);
			}
			HAWK_ASSERT (px->idx != HAWK_NULL);
			PRINT_EXPR_LIST_FOR_IDX (hawk, px->idx);
			break;
		}

		case HAWK_NDE_POS:
		{
			PUT_SRCSTR (hawk, HAWK_T("$("));
			PRINT_EXPR (hawk, ((hawk_nde_pos_t*)nde)->val);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_FNCALL_FNC:
		{
			hawk_nde_fncall_t* px = (hawk_nde_fncall_t*)nde;
			PUT_SRCSTRN (hawk, px->u.fnc.info.name.ptr, px->u.fnc.info.name.len);
			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR_LIST (hawk, px->args);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_FNCALL_FUN:
		{
			hawk_nde_fncall_t* px = (hawk_nde_fncall_t*)nde;
			PUT_SRCSTRN (hawk, px->u.fun.name.ptr, px->u.fun.name.len);
			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR_LIST (hawk, px->args);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_FNCALL_VAR:
		{
			hawk_nde_fncall_t* px = (hawk_nde_fncall_t*)nde;
			PRINT_EXPR (hawk, (hawk_nde_t*)px->u.var.var);
			PUT_SRCSTR (hawk, HAWK_T("("));
			PRINT_EXPR_LIST (hawk, px->args);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_GETLINE:
		{
			hawk_nde_getline_t* px = (hawk_nde_getline_t*)nde;

			PUT_SRCSTR (hawk, HAWK_T("("));

			if (px->in && (px->in_type == HAWK_IN_PIPE || px->in_type == HAWK_IN_RWPIPE))
			{
				PRINT_EXPR (hawk, px->in);
				PUT_SRCSTR (hawk, HAWK_T(" "));
				PUT_SRCSTR (hawk, getline_inop_str[px->in_type]);
				PUT_SRCSTR (hawk, HAWK_T(" "));
			}

			hawk_getkwname (hawk, (px->mbs? HAWK_KWID_GETBLINE: HAWK_KWID_GETLINE), &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			if (px->var)
			{
				PUT_SRCSTR (hawk, HAWK_T(" "));
				PRINT_EXPR (hawk, px->var);
			}

			if (px->in && px->in_type == HAWK_IN_FILE)
			{
				PUT_SRCSTR (hawk, HAWK_T(" "));
				PUT_SRCSTR (hawk, getline_inop_str[px->in_type]);
				PUT_SRCSTR (hawk, HAWK_T(" "));
				PRINT_EXPR (hawk, px->in);
			}	  

			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		case HAWK_NDE_PRINT:
		case HAWK_NDE_PRINTF:
		{
			PUT_SRCSTR (hawk, HAWK_T("("));
			if (print_printx(hawk, (hawk_nde_print_t*)nde) <= -1) return -1;
			PUT_SRCSTR (hawk, HAWK_T(")"));
			break;
		}

		default:
		{
			hawk_seterrnum (hawk, HAWK_NULL, HAWK_EINTERN);
			return -1;
		}
	}

	return 0;
}

static int print_expr_list (hawk_t* hawk, hawk_nde_t* tree)
{
	hawk_nde_t* p = tree;

	while (p) 
	{
		PRINT_EXPR (hawk, p);
		p = p->next;
		if (p) PUT_SRCSTR (hawk, HAWK_T(","));
	}

	return 0;
}

static int print_expr_list_for_idx (hawk_t* hawk, hawk_nde_t* tree)
{
	hawk_nde_t* p = tree;

	PUT_SRCSTR (hawk, HAWK_T("["));
	while (p) 
	{
		PRINT_EXPR (hawk, p);
		p = p->next;
		if (p) 
		{
			if (p->type == HAWK_NDE_NULL) 
			{
				/* the parser injects a HAWK_NDE_NULL node between the true multi-dimensional indices
				 * if the true multi-dimensional indices are enabled(i.e. HAWK_ENABLE_GC is defined)
				 * e.g. a[x][y][z]
				 * let me print this regardless of */
				PUT_SRCSTR (hawk, HAWK_T("]["));
				p = p->next;
			}
			else PUT_SRCSTR (hawk, HAWK_T(","));
		}
	}
	PUT_SRCSTR (hawk, HAWK_T("]"));

	return 0;
}

static int print_stmt (hawk_t* hawk, hawk_nde_t* p, int depth)
{
	hawk_oow_t i;
	hawk_oocs_t kw;

	switch (p->type) 
	{
		case HAWK_NDE_NULL:
		{
			PRINT_TABS (hawk, depth);
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_BLK:
		{
			hawk_oow_t n;
			hawk_nde_blk_t* px = (hawk_nde_blk_t*)p;

			PRINT_TABS (hawk, depth);
			PUT_SRCSTR (hawk, HAWK_T("{"));
			PUT_NL (hawk);

			if (px->org_nlcls > 0) 
			{
				PRINT_TABS (hawk, depth + 1);

				hawk_getkwname (hawk, HAWK_KWID_XLOCAL, &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
				PUT_SRCSTR (hawk, HAWK_T(" "));

				/* though the parser pushed up all local variables to the outer-most level,
				 * the code here restores the original declarations with org_nlcls and prv_nlcls */
				for (i = 0; i < px->org_nlcls - 1; i++) 
				{
					PUT_SRCSTR (hawk, HAWK_T("__l"));
					n = hawk_int_to_oocstr(px->outer_nlcls + i, 10, HAWK_NULL, hawk->tmp.fmt, HAWK_COUNTOF(hawk->tmp.fmt));
					PUT_SRCSTRN (hawk, hawk->tmp.fmt, n);
					PUT_SRCSTR (hawk, HAWK_T(", "));
				}

				PUT_SRCSTR (hawk, HAWK_T("__l"));
				n = hawk_int_to_oocstr(px->outer_nlcls + i, 10, HAWK_NULL, hawk->tmp.fmt, HAWK_COUNTOF(hawk->tmp.fmt));
				PUT_SRCSTRN (hawk, hawk->tmp.fmt, n);
				PUT_SRCSTR (hawk, HAWK_T(";"));
				PUT_NL (hawk);
			}

			PRINT_STMTS (hawk, px->body, depth + 1);	
			PRINT_TABS (hawk, depth);
			PUT_SRCSTR (hawk, HAWK_T("}"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_IF: 
		{
			hawk_nde_if_t* px = (hawk_nde_if_t*)p;

			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_IF, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" ("));	
			PRINT_EXPR (hawk, px->test);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			PUT_NL (hawk);

			HAWK_ASSERT (px->then_part != HAWK_NULL);
			if (px->then_part->type == HAWK_NDE_BLK)
				PRINT_STMTS (hawk, px->then_part, depth);
			else
				PRINT_STMTS (hawk, px->then_part, depth + 1);

			if (px->else_part != HAWK_NULL) 
			{
				PRINT_TABS (hawk, depth);
				hawk_getkwname (hawk, HAWK_KWID_ELSE, &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
				PUT_NL (hawk);
				if (px->else_part->type == HAWK_NDE_BLK)
					PRINT_STMTS (hawk, px->else_part, depth);
				else
					PRINT_STMTS (hawk, px->else_part, depth + 1);
			}
			break;
		}

		case HAWK_NDE_WHILE: 
		{
			hawk_nde_while_t* px = (hawk_nde_while_t*)p;

			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_WHILE, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" ("));
			PRINT_EXPR (hawk, px->test);
			PUT_SRCSTR (hawk, HAWK_T(")"));
			PUT_NL (hawk);
			if (px->body->type == HAWK_NDE_BLK) 
			{
				PRINT_STMTS (hawk, px->body, depth);
			}
			else 
			{
				PRINT_STMTS (hawk, px->body, depth + 1);
			}
			break;
		}

		case HAWK_NDE_DOWHILE: 
		{
			hawk_nde_while_t* px = (hawk_nde_while_t*)p;

			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_DO, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_NL (hawk);
			if (px->body->type == HAWK_NDE_BLK) 
			{
				PRINT_STMTS (hawk, px->body, depth);
			}
			else 
			{
				PRINT_STMTS (hawk, px->body, depth + 1);
			}

			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_WHILE, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" ("));
			PRINT_EXPR (hawk, px->test);
			PUT_SRCSTR (hawk, HAWK_T(");"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_FOR:
		{
			hawk_nde_for_t* px = (hawk_nde_for_t*)p;

			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_FOR, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" ("));
			if (px->init != HAWK_NULL) 
			{
				PRINT_EXPR (hawk, px->init);
			}
			PUT_SRCSTR (hawk, HAWK_T("; "));
			if (px->test != HAWK_NULL) 
			{
				PRINT_EXPR (hawk, px->test);
			}
			PUT_SRCSTR (hawk, HAWK_T("; "));
			if (px->incr != HAWK_NULL) 
			{
				PRINT_EXPR (hawk, px->incr);
			}
			PUT_SRCSTR (hawk, HAWK_T(")"));
			PUT_NL (hawk);

			if (px->body->type == HAWK_NDE_BLK) 
			{
				PRINT_STMTS (hawk, px->body, depth);
			}
			else 
			{
				PRINT_STMTS (hawk, px->body, depth + 1);
			}
			break;
		}

		case HAWK_NDE_FORIN:
		{
			hawk_nde_forin_t* px = (hawk_nde_forin_t*)p;

			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_FOR, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" "));
			PRINT_EXPR (hawk, px->test);
			PUT_NL (hawk);
			if (px->body->type == HAWK_NDE_BLK) 
			{
				PRINT_STMTS (hawk, px->body, depth);
			}
			else 
			{
				PRINT_STMTS (hawk, px->body, depth + 1);
			}
			break;
		}

		case HAWK_NDE_BREAK:
		{
			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_BREAK, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_CONTINUE:
		{
			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_CONTINUE, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_RETURN:
		{
			PRINT_TABS (hawk, depth);
			if (((hawk_nde_return_t*)p)->val == HAWK_NULL) 
			{
				hawk_getkwname (hawk, HAWK_KWID_RETURN, &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
				PUT_SRCSTR (hawk, HAWK_T(";"));
				PUT_NL (hawk);
			}
			else 
			{
				hawk_getkwname (hawk, HAWK_KWID_RETURN, &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
				PUT_SRCSTR (hawk, HAWK_T(" "));
				HAWK_ASSERT (((hawk_nde_return_t*)p)->val->next == HAWK_NULL);

				PRINT_EXPR (hawk, ((hawk_nde_return_t*)p)->val);
				PUT_SRCSTR (hawk, HAWK_T(";"));
				PUT_NL (hawk);
			}
			break;
		}

		case HAWK_NDE_EXIT:
		{
			hawk_nde_exit_t* px = (hawk_nde_exit_t*)p;
			PRINT_TABS (hawk, depth);

			if (px->val == HAWK_NULL) 
			{
				hawk_getkwname (hawk, (px->abort? HAWK_KWID_XABORT: HAWK_KWID_EXIT), &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
				PUT_SRCSTR (hawk, HAWK_T(";"));
				PUT_NL (hawk);
			}
			else 
			{
				hawk_getkwname (hawk, (px->abort? HAWK_KWID_XABORT: HAWK_KWID_EXIT), &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
				PUT_SRCSTR (hawk, HAWK_T(" "));
				HAWK_ASSERT (px->val->next == HAWK_NULL);
				PRINT_EXPR (hawk, px->val);
				PUT_SRCSTR (hawk, HAWK_T(";"));
				PUT_NL (hawk);
			}
			break;
		}

		case HAWK_NDE_NEXT:
		{
			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_NEXT, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_NEXTFILE:
		{
			PRINT_TABS (hawk, depth);
			if (((hawk_nde_nextfile_t*)p)->out)
			{
				hawk_getkwname (hawk, HAWK_KWID_NEXTOFILE, &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			}
			else
			{
				hawk_getkwname (hawk, HAWK_KWID_NEXTFILE, &kw);
				PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			}
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}

		case HAWK_NDE_DELETE:
		{
			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_DELETE, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" "));
			hawk_prnpt (hawk, ((hawk_nde_delete_t*)p)->var);
			break;
		}

		case HAWK_NDE_RESET:
		{
			PRINT_TABS (hawk, depth);
			hawk_getkwname (hawk, HAWK_KWID_XRESET, &kw);
			PUT_SRCSTRN (hawk, kw.ptr, kw.len);
			PUT_SRCSTR (hawk, HAWK_T(" "));
			hawk_prnpt (hawk, ((hawk_nde_reset_t*)p)->var);
			break;
		}

		case HAWK_NDE_PRINT:
		case HAWK_NDE_PRINTF:
		{
			PRINT_TABS (hawk, depth);
			if (print_printx (hawk, (hawk_nde_print_t*)p) <= -1) return -1;
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}

		default:
		{
			PRINT_TABS (hawk, depth);
			PRINT_EXPR (hawk, p);
			PUT_SRCSTR (hawk, HAWK_T(";"));
			PUT_NL (hawk);
			break;
		}
	}

	return 0;
}

static int print_stmts (hawk_t* hawk, hawk_nde_t* tree, int depth)
{
	hawk_nde_t* p = tree;

	while (p) 
	{
		if (print_stmt(hawk, p, depth) <= -1) return -1;
		p = p->next;
	}

	return 0;
}

int hawk_prnpt (hawk_t* hawk, hawk_nde_t* tree)
{
	return print_stmts(hawk, tree, 0);
}

int hawk_prnnde (hawk_t* hawk, hawk_nde_t* tree)
{
	return print_stmt(hawk, tree, 0);
}

int hawk_prnptnpt (hawk_t* hawk, hawk_nde_t* tree)
{
	hawk_nde_t* nde = tree;

	while (nde)
	{
		if (print_expr(hawk, nde) <= -1) return -1;
		if (!nde->next) break;

		PUT_SRCSTR (hawk, HAWK_T(","));
		nde = nde->next;
	}

	return 0;
}

void hawk_clrpt (hawk_t* hawk, hawk_nde_t* tree)
{
	hawk_nde_t* p = tree;
	hawk_nde_t* next;

	while (p) 
	{
		next = p->next;

		switch (p->type) 
		{
			case HAWK_NDE_NULL:
			{
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_BLK:
			{
				hawk_clrpt (hawk, ((hawk_nde_blk_t*)p)->body);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_IF:
			{
				hawk_nde_if_t* px = (hawk_nde_if_t*)p;
				hawk_clrpt (hawk, px->test);
				hawk_clrpt (hawk, px->then_part);
				if (px->else_part) hawk_clrpt (hawk, px->else_part);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_WHILE:
			case HAWK_NDE_DOWHILE:
			{
				hawk_clrpt (hawk, ((hawk_nde_while_t*)p)->test);
				hawk_clrpt (hawk, ((hawk_nde_while_t*)p)->body);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FOR:
			{
				hawk_nde_for_t* px = (hawk_nde_for_t*)p;
				if (px->init) hawk_clrpt (hawk, px->init);
				if (px->test) hawk_clrpt (hawk, px->test);
				if (px->incr) hawk_clrpt (hawk, px->incr);
				hawk_clrpt (hawk, px->body);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FORIN:
			{
				hawk_clrpt (hawk, ((hawk_nde_forin_t*)p)->test);
				hawk_clrpt (hawk, ((hawk_nde_forin_t*)p)->body);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_BREAK:
			{
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_CONTINUE:
			{
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_RETURN:
			{
				hawk_nde_return_t* px = (hawk_nde_return_t*)p;
				if (px->val) hawk_clrpt (hawk, px->val);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_EXIT:
			{
				if (((hawk_nde_exit_t*)p)->val != HAWK_NULL) 
					hawk_clrpt (hawk, ((hawk_nde_exit_t*)p)->val);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_NEXT:
			case HAWK_NDE_NEXTFILE:
			{
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_DELETE:
			{
				hawk_clrpt (hawk, ((hawk_nde_delete_t*)p)->var);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_RESET:
			{
				hawk_clrpt (hawk, ((hawk_nde_reset_t*)p)->var);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_PRINT:
			case HAWK_NDE_PRINTF:
			{
				hawk_nde_print_t* px = (hawk_nde_print_t*)p;
				if (px->args) hawk_clrpt (hawk, px->args);
				if (px->out) hawk_clrpt (hawk, px->out);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_GRP:
			{
				hawk_clrpt (hawk, ((hawk_nde_grp_t*)p)->body);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_ASS:
			{
				hawk_clrpt (hawk, ((hawk_nde_ass_t*)p)->left);
				hawk_clrpt (hawk, ((hawk_nde_ass_t*)p)->right);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_EXP_BIN:
			{
				hawk_nde_exp_t* px = (hawk_nde_exp_t*)p;
				HAWK_ASSERT (px->left->next == HAWK_NULL);
				HAWK_ASSERT (px->right->next == HAWK_NULL);

				hawk_clrpt (hawk, px->left);
				hawk_clrpt (hawk, px->right);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_EXP_UNR:
			case HAWK_NDE_EXP_INCPRE:
			case HAWK_NDE_EXP_INCPST:
			{
				hawk_nde_exp_t* px = (hawk_nde_exp_t*)p;
				HAWK_ASSERT (px->right == HAWK_NULL);
				hawk_clrpt (hawk, px->left);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_CND:
			{
				hawk_clrpt (hawk, ((hawk_nde_cnd_t*)p)->test);
				hawk_clrpt (hawk, ((hawk_nde_cnd_t*)p)->left);
				hawk_clrpt (hawk, ((hawk_nde_cnd_t*)p)->right);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_CHAR:
			case HAWK_NDE_BCHR:
			{
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_INT:
			{
				if (((hawk_nde_int_t*)p)->str)
					hawk_freemem (hawk, ((hawk_nde_int_t*)p)->str);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FLT:
			{
				if (((hawk_nde_flt_t*)p)->str)
					hawk_freemem (hawk, ((hawk_nde_flt_t*)p)->str);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_STR:
			{
				hawk_freemem (hawk, ((hawk_nde_str_t*)p)->ptr);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_MBS:
			{
				hawk_freemem (hawk, ((hawk_nde_mbs_t*)p)->ptr);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_REX:
			{
				hawk_nde_rex_t* rex = (hawk_nde_rex_t*)p;
				hawk_freerex (hawk, rex->code[0], rex->code[1]);
				hawk_freemem (hawk, ((hawk_nde_rex_t*)p)->str.ptr);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_XNIL:
			{
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FUN:
			{
				hawk_freemem (hawk, ((hawk_nde_fun_t*)p)->name.ptr);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_NAMED:
			case HAWK_NDE_GBL:
			case HAWK_NDE_LCL:
			case HAWK_NDE_ARG:
			{
				hawk_nde_var_t* px = (hawk_nde_var_t*)p;
				HAWK_ASSERT (px->idx == HAWK_NULL);
				if (px->id.name.ptr) hawk_freemem (hawk, px->id.name.ptr);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_NAMEDIDX:
			case HAWK_NDE_GBLIDX:
			case HAWK_NDE_LCLIDX:
			case HAWK_NDE_ARGIDX:
			{
				hawk_nde_var_t* px = (hawk_nde_var_t*)p;
				HAWK_ASSERT (px->idx != HAWK_NULL);
				hawk_clrpt (hawk, px->idx);
				if (px->id.name.ptr) hawk_freemem (hawk, px->id.name.ptr);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_POS:
			{
				hawk_clrpt (hawk, ((hawk_nde_pos_t*)p)->val);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FNCALL_FNC:
			{
				hawk_nde_fncall_t* px = (hawk_nde_fncall_t*)p;
				/* hawk_freemem (hawk, px->u.fnc); */
				hawk_freemem (hawk, px->u.fnc.info.name.ptr);
				hawk_clrpt (hawk, px->args);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FNCALL_FUN:
			{
				hawk_nde_fncall_t* px = (hawk_nde_fncall_t*)p;
				hawk_freemem (hawk, px->u.fun.name.ptr);
				hawk_clrpt (hawk, px->args);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_FNCALL_VAR:
			{
				hawk_nde_fncall_t* px = (hawk_nde_fncall_t*)p;
				hawk_clrpt (hawk, (hawk_nde_t*)px->u.var.var);
				hawk_clrpt (hawk, px->args);
				hawk_freemem (hawk, p);
				break;
			}

			case HAWK_NDE_GETLINE:
			{
				hawk_nde_getline_t* px = (hawk_nde_getline_t*)p;
				if (px->var) hawk_clrpt (hawk, px->var);
				if (px->in) hawk_clrpt (hawk, px->in);
				hawk_freemem (hawk, p);
				break;
			}

			default:
			{
				HAWK_ASSERT (!"should never happen - invalid node type");
				hawk_freemem (hawk, p);
				break;
			}
		}

		p = next;
	}
}
