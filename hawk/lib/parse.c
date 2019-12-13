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

#if !defined(HAWK_DEFAULT_MODPREFIX)
#	if defined(_WIN32)
#		define HAWK_DEFAULT_MODPREFIX "hawkawk-"
#	elif defined(__OS2__)
#		define HAWK_DEFAULT_MODPREFIX "awk-"
#	elif defined(__DOS__)
#		define HAWK_DEFAULT_MODPREFIX "awk-"
#	else
#		define HAWK_DEFAULT_MODPREFIX "libhawkawk-"
#	endif
#endif

#if !defined(HAWK_DEFAULT_MODPOSTFIX)
#	define HAWK_DEFAULT_MODPOSTFIX ""
#endif

enum tok_t
{
	TOK_EOF,
	TOK_NEWLINE,

	/* TOK_XXX_ASSNs should be in sync with assop in assign_to_opcode. 
	 * it also should be in the order as hawk_assop_type_t in run.h */
	TOK_ASSN,
	TOK_PLUS_ASSN,
	TOK_MINUS_ASSN,
	TOK_MUL_ASSN,
	TOK_DIV_ASSN,
	TOK_IDIV_ASSN,
	TOK_MOD_ASSN,
	TOK_EXP_ASSN, /* ^ - exponentiation */
	TOK_CONCAT_ASSN,
	TOK_RS_ASSN,
	TOK_LS_ASSN,
	TOK_BAND_ASSN,
	TOK_BXOR_ASSN,
	TOK_BOR_ASSN,
	/* end of TOK_XXX_ASSN */

	TOK_TEQ,
	TOK_TNE,
	TOK_EQ,
	TOK_NE,
	TOK_LE,
	TOK_LT,
	TOK_GE,
	TOK_GT,
	TOK_MA,   /* ~ - match */
	TOK_NM,   /* !~ -  not match */
	TOK_LNOT, /* ! - logical negation */
	TOK_PLUS,
	TOK_PLUSPLUS,
	TOK_MINUS,
	TOK_MINUSMINUS,
	TOK_MUL,
	TOK_DIV,
	TOK_IDIV,
	TOK_MOD,
	TOK_LOR,
	TOK_LAND,
	TOK_BOR,
	TOK_BXOR, /* ^^ - bitwise-xor */
	TOK_BAND,
	TOK_BNOT, /* ~~ - used for unary bitwise-not */
	TOK_RS,
	TOK_LS,
	TOK_IN,
	TOK_EXP,
	TOK_CONCAT, /* %% */

	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACK,
	TOK_RBRACK,

	TOK_DOLLAR,
	TOK_COMMA,
	TOK_SEMICOLON,
	TOK_COLON,
	TOK_DBLCOLON,
	TOK_QUEST,
	/*TOK_DBLAT,*/

	/* ==  begin reserved words == */
	/* === extended reserved words === */
	TOK_XGLOBAL,
	TOK_XLOCAL, 
	TOK_XINCLUDE,
	TOK_XINCLUDE_ONCE,
	TOK_XPRAGMA,
	TOK_XABORT,
	TOK_XRESET,

	/* === normal reserved words === */
	TOK_BEGIN,
	TOK_END,
	TOK_FUNCTION,

	TOK_IF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_FOR,
	TOK_DO,
	TOK_BREAK,
	TOK_CONTINUE,
	TOK_RETURN,
	TOK_EXIT,
	TOK_DELETE,
	TOK_NEXT,
	TOK_NEXTFILE,
	TOK_NEXTOFILE,

	TOK_PRINT,
	TOK_PRINTF,
	TOK_GETLINE,
	/* ==  end reserved words == */

	TOK_IDENT,
	TOK_INT,
	TOK_FLT,
	TOK_STR,
	TOK_MBS,
	TOK_REX,

	__TOKEN_COUNT__
};

enum
{
	PARSE_GBL,
	PARSE_FUNCTION,
	PARSE_BEGIN,
	PARSE_END,
	PARSE_BEGIN_BLOCK,
	PARSE_END_BLOCK,
	PARSE_PATTERN,
	PARSE_ACTION_BLOCK
};

enum
{
	PARSE_LOOP_NONE,
	PARSE_LOOP_WHILE,
	PARSE_LOOP_FOR,
	PARSE_LOOP_DOWHILE
};

typedef struct binmap_t binmap_t;

struct binmap_t
{
	int token;
	int binop;
};

static int parse_progunit (hawk_t* awk);
static hawk_t* collect_globals (hawk_t* awk);
static void adjust_static_globals (hawk_t* awk);
static hawk_oow_t find_global (hawk_t* awk, const hawk_oocs_t* name);
static hawk_t* collect_locals (hawk_t* awk, hawk_oow_t nlcls, int istop);

static hawk_nde_t* parse_function (hawk_t* awk);
static hawk_nde_t* parse_begin (hawk_t* awk);
static hawk_nde_t* parse_end (hawk_t* awk);
static hawk_chain_t* parse_action_block (hawk_t* awk, hawk_nde_t* ptn, int blockless);

static hawk_nde_t* parse_block_dc (hawk_t* awk, const hawk_loc_t* xloc, int istop);

static hawk_nde_t* parse_statement (hawk_t* awk, const hawk_loc_t* xloc);

static hawk_nde_t* parse_expr_withdc (hawk_t* awk, const hawk_loc_t* xloc);

static hawk_nde_t* parse_logical_or (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_logical_and (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_in (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_regex_match (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_bitwise_or (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_bitwise_xor (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_bitwise_and (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_equality (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_relational (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_shift (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_concat (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_additive (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_multiplicative (hawk_t* awk, const hawk_loc_t* xloc);

static hawk_nde_t* parse_unary (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_exponent (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_unary_exp (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_increment (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_primary (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_primary_ident (hawk_t* awk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_hashidx (hawk_t* awk, const hawk_oocs_t* name, const hawk_loc_t* xloc);

#define FNCALL_FLAG_NOARG (1 << 0) /* no argument */
#define FNCALL_FLAG_VAR   (1 << 1)
static hawk_nde_t* parse_fncall (hawk_t* awk, const hawk_oocs_t* name, hawk_fnc_t* fnc, const hawk_loc_t* xloc, int flags);

static hawk_nde_t* parse_primary_ident_segs (hawk_t* awk, const hawk_loc_t* xloc, const hawk_oocs_t* full, const hawk_oocs_t segs[], int nsegs);

static int get_token (hawk_t* awk);
static int preget_token (hawk_t* awk);
static int get_rexstr (hawk_t* awk, hawk_tok_t* tok);

static int skip_spaces (hawk_t* awk);
static int skip_comment (hawk_t* awk);
static int classify_ident (hawk_t* awk, const hawk_oocs_t* name);

static int deparse (hawk_t* awk);
static hawk_htb_walk_t deparse_func (hawk_htb_t* map, hawk_htb_pair_t* pair, void* arg);
static int put_char (hawk_t* awk, hawk_ooch_t c);
static int flush_out (hawk_t* awk);

static hawk_mod_t* query_module (
	hawk_t* awk, const hawk_oocs_t segs[], int nsegs,
	hawk_mod_sym_t* sym);

typedef struct kwent_t kwent_t;

struct kwent_t 
{ 
	hawk_oocs_t name;
	int type; 
	int trait; /* the entry is valid when this option is set */
};

static kwent_t kwtab[] = 
{
	/* keep this table in sync with the kw_t enums in <parse.h>.
	 * also keep it sorted by the first field for binary search */
	{ { HAWK_T("@abort"),         6 }, TOK_XABORT,        0 },
	{ { HAWK_T("@global"),        7 }, TOK_XGLOBAL,       0 },
	{ { HAWK_T("@include"),       8 }, TOK_XINCLUDE,      0 },
	{ { HAWK_T("@include_once"), 13 }, TOK_XINCLUDE_ONCE, 0 },
	{ { HAWK_T("@local"),         6 }, TOK_XLOCAL,        0 },
	{ { HAWK_T("@pragma"),        7 }, TOK_XPRAGMA,       0 },
	{ { HAWK_T("@reset"),         6 }, TOK_XRESET,        0 },
	{ { HAWK_T("BEGIN"),          5 }, TOK_BEGIN,         HAWK_PABLOCK },
	{ { HAWK_T("END"),            3 }, TOK_END,           HAWK_PABLOCK },
	{ { HAWK_T("break"),          5 }, TOK_BREAK,         0 },
	{ { HAWK_T("continue"),       8 }, TOK_CONTINUE,      0 },
	{ { HAWK_T("delete"),         6 }, TOK_DELETE,        0 },
	{ { HAWK_T("do"),             2 }, TOK_DO,            0 },
	{ { HAWK_T("else"),           4 }, TOK_ELSE,          0 },
	{ { HAWK_T("exit"),           4 }, TOK_EXIT,          0 },
	{ { HAWK_T("for"),            3 }, TOK_FOR,           0 },
	{ { HAWK_T("function"),       8 }, TOK_FUNCTION,      0 },
	{ { HAWK_T("getline"),        7 }, TOK_GETLINE,       HAWK_RIO },
	{ { HAWK_T("if"),             2 }, TOK_IF,            0 },
	{ { HAWK_T("in"),             2 }, TOK_IN,            0 },
	{ { HAWK_T("next"),           4 }, TOK_NEXT,          HAWK_PABLOCK },
	{ { HAWK_T("nextfile"),       8 }, TOK_NEXTFILE,      HAWK_PABLOCK },
	{ { HAWK_T("nextofile"),      9 }, TOK_NEXTOFILE,     HAWK_PABLOCK | HAWK_NEXTOFILE },
	{ { HAWK_T("print"),          5 }, TOK_PRINT,         HAWK_RIO },
	{ { HAWK_T("printf"),         6 }, TOK_PRINTF,        HAWK_RIO },
	{ { HAWK_T("return"),         6 }, TOK_RETURN,        0 },
	{ { HAWK_T("while"),          5 }, TOK_WHILE,         0 }
};

typedef struct global_t global_t;

struct global_t
{
	const hawk_ooch_t* name;
	hawk_oow_t namelen;
	int trait;
};

static global_t gtab[] =
{
	/* 
	 * this table must match the order of the hawk_gbl_id_t enumerators 
	 */

	/* output real-to-str conversion format for other cases than 'print' */
	{ HAWK_T("CONVFMT"),      7,  0 },

	/* current input file name */
	{ HAWK_T("FILENAME"),     8,  HAWK_PABLOCK },

	/* input record number in current file */
	{ HAWK_T("FNR"),          3,  HAWK_PABLOCK },

	/* input field separator */
	{ HAWK_T("FS"),           2,  0 },

	/* ignore case in string comparison */
	{ HAWK_T("IGNORECASE"),  10,  0 },

	/* number of fields in current input record 
	 * NF is also updated if you assign a value to $0. so it is not
	 * associated with HAWK_PABLOCK */
	{ HAWK_T("NF"),           2,  0 },

	/* input record number */
	{ HAWK_T("NR"),           2,  HAWK_PABLOCK },

	/* current output file name */
	{ HAWK_T("OFILENAME"),    9,  HAWK_PABLOCK | HAWK_NEXTOFILE },

	/* output real-to-str conversion format for 'print' */
	{ HAWK_T("OFMT"),         4,  HAWK_RIO }, 

	/* output field separator for 'print' */
	{ HAWK_T("OFS"),          3,  HAWK_RIO },

	/* output record separator. used for 'print' and blockless output */
	{ HAWK_T("ORS"),          3,  HAWK_RIO },

	{ HAWK_T("RLENGTH"),      7,  0 },
	{ HAWK_T("RS"),           2,  0 },

	{ HAWK_T("RSTART"),       6,  0 },

	/* it decides the field construction behavior when FS is a regular expression and 
	 * the field splitter is composed of whitespaces only. e.g) FS="[ \t]*";
	 * if set to a non-zero value, remove leading spaces and trailing spaces off a record
	 * before field splitting.
	 * if set to zero, leading spaces and trailing spaces result in 1 empty field respectively.
	 * if not set, the behavior is dependent on the awk->opt.trait & HAWK_STRIPRECSPC */
	{ HAWK_T("STRIPRECSPC"),  11, 0 }, 

	{ HAWK_T("SUBSEP"),       6,  0 }
};

#define GET_CHAR(awk) \
	do { if (get_char(awk) <= -1) return -1; } while(0)

#define GET_CHAR_TO(awk,c) \
	do { \
		if (get_char(awk) <= -1) return -1; \
		c = (awk)->sio.last.c; \
	} while(0)

#define SET_TOKEN_TYPE(awk,tok,code) \
	do { (tok)->type = (code); } while (0)

#define ADD_TOKEN_CHAR(awk,tok,c) \
	do { \
		if (hawk_ooecs_ccat((tok)->name,(c)) == (hawk_oow_t)-1) \
		{ \
			hawk_seterror (awk, HAWK_ENOMEM, HAWK_NULL, &(tok)->loc); \
			return -1; \
		} \
	} while (0)

#define ADD_TOKEN_STR(awk,tok,s,l) \
	do { \
		if (hawk_ooecs_ncat((tok)->name,(s),(l)) == (hawk_oow_t)-1) \
		{ \
			hawk_seterror (awk, HAWK_ENOMEM, HAWK_NULL, &(tok)->loc); \
			return -1; \
		} \
	} while (0)

#if defined(HAWK_OOCH_IS_BCH)

#	define ADD_TOKEN_UINT32(awk,tok,c) \
	do { \
		if (c <= 0xFF) ADD_TOKEN_CHAR(awk, tok, c); \
		else  \
		{ \
			hawk_bch_t __xbuf[HAWK_MBLEN_MAX + 1]; \
			hawk_oow_t __len, __i; \
			__len = hawk_uctoutf8(c, __xbuf, HAWK_COUNTOF(__xbuf)); /* use utf8 all the time */ \
			for (__i = 0; __i < __len; __i++) ADD_TOKEN_CHAR(awk, tok, __xbuf[__i]); \
		} \
	} while (0)
#else
#	define ADD_TOKEN_UINT32(awk,tok,c) ADD_TOKEN_CHAR(awk,tok,c);
#endif

#define MATCH(awk,tok_type) ((awk)->tok.type == (tok_type))
#define MATCH_RANGE(awk,tok_type_start,tok_type_end) ((awk)->tok.type >= (tok_type_start) && (awk)->tok.type <= (tok_type_end))

#define MATCH_TERMINATOR_NORMAL(awk) \
	(MATCH((awk),TOK_SEMICOLON) || MATCH((awk),TOK_NEWLINE))

#define MATCH_TERMINATOR_RBRACE(awk) \
	((awk->opt.trait & HAWK_NEWLINE) && MATCH((awk),TOK_RBRACE))

#define MATCH_TERMINATOR(awk) \
	(MATCH_TERMINATOR_NORMAL(awk) || MATCH_TERMINATOR_RBRACE(awk))

#define ISNOERR(awk) ((awk)->_gem.errnum == HAWK_ENOERR)

#define CLRERR(awk) \
	hawk_seterror (awk, HAWK_ENOERR, HAWK_NULL, HAWK_NULL)

#define SETERR_TOK(awk,code) \
	hawk_seterror (awk, code, HAWK_OOECS_OOCS((awk)->tok.name), &(awk)->tok.loc)

#define SETERR_COD(awk,code) \
	hawk_seterror (awk, code, HAWK_NULL, HAWK_NULL)

#define SETERR_LOC(awk,code,loc) \
	hawk_seterror (awk, code, HAWK_NULL, loc)

#define SETERR_ARG_LOC(awk,code,ep,el,loc) \
	do { \
		hawk_oocs_t __ea; \
		__ea.len = (el); __ea.ptr = (ep); \
		hawk_seterror ((awk), (code), &__ea, (loc)); \
	} while (0)

#define SETERR_ARG(awk,code,ep,el) SETERR_ARG_LOC(awk,code,ep,el,HAWK_NULL)

#define ADJERR_LOC(rtx,l) do { (awk)->_gem.errloc = *(l); } while (0)

static HAWK_INLINE int is_plain_var (hawk_nde_t* nde)
{
	return nde->type == HAWK_NDE_GBL ||
	       nde->type == HAWK_NDE_LCL ||
	       nde->type == HAWK_NDE_ARG ||
	       nde->type == HAWK_NDE_NAMED;
}

static HAWK_INLINE int is_var (hawk_nde_t* nde)
{
	return nde->type == HAWK_NDE_GBL ||
	       nde->type == HAWK_NDE_LCL ||
	       nde->type == HAWK_NDE_ARG ||
	       nde->type == HAWK_NDE_NAMED ||
	       nde->type == HAWK_NDE_GBLIDX ||
	       nde->type == HAWK_NDE_LCLIDX ||
	       nde->type == HAWK_NDE_ARGIDX ||
	       nde->type == HAWK_NDE_NAMEDIDX;
}

static int get_char (hawk_t* awk)
{
	hawk_ooi_t n;

	if (awk->sio.nungots > 0) 
	{
		/* there are something in the unget buffer */
		awk->sio.last = awk->sio.ungot[--awk->sio.nungots];
		return 0;
	}

	if (awk->sio.inp->b.pos >= awk->sio.inp->b.len)
	{
		CLRERR (awk);
		n = awk->sio.inf (
			awk, HAWK_SIO_CMD_READ, awk->sio.inp,
			awk->sio.inp->b.buf, HAWK_COUNTOF(awk->sio.inp->b.buf)
		);
		if (n <= -1)
		{
			if (ISNOERR(awk))
				SETERR_ARG (awk, HAWK_EREAD, HAWK_T("<SIN>"), 5);
			return -1;
		}

		if (n == 0)
		{
			awk->sio.inp->last.c = HAWK_OOCI_EOF;
			awk->sio.inp->last.line = awk->sio.inp->line;
			awk->sio.inp->last.colm = awk->sio.inp->colm;
			awk->sio.inp->last.file = awk->sio.inp->name;
			awk->sio.last = awk->sio.inp->last;
			return 0;
		}

		awk->sio.inp->b.pos = 0;
		awk->sio.inp->b.len = n;	
	}

	if (awk->sio.inp->last.c == HAWK_T('\n'))
	{
		/* if the previous charater was a newline,
		 * increment the line counter and reset column to 1.
		 * incrementing it line number here instead of
		 * updating inp->last causes the line number for
		 * TOK_EOF to be the same line as the last newline. */
		awk->sio.inp->line++;
		awk->sio.inp->colm = 1;
	}
	
	awk->sio.inp->last.c = awk->sio.inp->b.buf[awk->sio.inp->b.pos++];
	awk->sio.inp->last.line = awk->sio.inp->line;
	awk->sio.inp->last.colm = awk->sio.inp->colm++;
	awk->sio.inp->last.file = awk->sio.inp->name;
	awk->sio.last = awk->sio.inp->last;
	return 0;
}

static void unget_char (hawk_t* awk, const hawk_sio_lxc_t* c)
{
	/* Make sure that the unget buffer is large enough */
	HAWK_ASSERT (awk, awk->sio.nungots < HAWK_COUNTOF(awk->sio.ungot));
	awk->sio.ungot[awk->sio.nungots++] = *c;
}

const hawk_ooch_t* hawk_getgblname (hawk_t* awk, hawk_oow_t idx, hawk_oow_t* len)
{
	HAWK_ASSERT (awk, idx < HAWK_ARR_SIZE(awk->parse.gbls));

	*len = HAWK_ARR_DLEN(awk->parse.gbls,idx);
	return HAWK_ARR_DPTR(awk->parse.gbls,idx);
}

void hawk_getkwname (hawk_t* awk, hawk_kwid_t id, hawk_oocs_t* s)
{
	*s = kwtab[id].name;
}

static int parse (hawk_t* awk)
{
	int ret = -1; 
	hawk_ooi_t op;

	HAWK_ASSERT (awk, awk->sio.inf != HAWK_NULL);

	CLRERR (awk);
	op = awk->sio.inf(awk, HAWK_SIO_CMD_OPEN, awk->sio.inp, HAWK_NULL, 0);
	if (op <= -1)
	{
		/* cannot open the source file.
		 * it doesn't even have to call CLOSE */
		if (ISNOERR(awk)) 
			SETERR_ARG (awk, HAWK_EOPEN, HAWK_T("<SIN>"), 5);
		return -1;
	}

	adjust_static_globals (awk);

	/* get the first character and the first token */
	if (get_char (awk) <= -1 || get_token (awk)) goto oops; 

	while (1) 
	{
		while (MATCH(awk,TOK_NEWLINE)) 
		{
			if (get_token (awk) <= -1) goto oops;
		}
		if (MATCH(awk,TOK_EOF)) break;

		if (parse_progunit (awk) <= -1) goto oops;
	}

	if (!(awk->opt.trait & HAWK_IMPLICIT))
	{
		/* ensure that all functions called are defined in the EXPLICIT-only mode.
		 * o	therwise, the error detection will get delay until run-time. */

		hawk_htb_pair_t* p;
		hawk_oow_t buckno;

		p = hawk_htb_getfirstpair(awk->parse.funs, &buckno);
		while (p != HAWK_NULL)
		{
			if (hawk_htb_search (awk->tree.funs, HAWK_HTB_KPTR(p), HAWK_HTB_KLEN(p)) == HAWK_NULL)
			{
				hawk_nde_t* nde;

				/* see parse_fncall() for what is
				 * stored into awk->tree.funs */
				nde = (hawk_nde_t*)HAWK_HTB_VPTR(p);

				SETERR_ARG_LOC (awk, HAWK_EFUNNF, HAWK_HTB_KPTR(p), HAWK_HTB_KLEN(p), &nde->loc);

				goto oops;
			}

			p = hawk_htb_getnextpair (awk->parse.funs, p, &buckno);
		}
	}

	HAWK_ASSERT (awk, awk->tree.ngbls == HAWK_ARR_SIZE(awk->parse.gbls));
	ret = 0;

oops:
	if (ret <= -1)
	{
		/* an error occurred and control has reached here
		 * probably, some included files might not have been 
		 * closed. close them */
		while (awk->sio.inp != &awk->sio.arg)
		{
			hawk_sio_arg_t* prev;

			/* nothing much to do about a close error */
			awk->sio.inf (
				awk, HAWK_SIO_CMD_CLOSE, 
				awk->sio.inp, HAWK_NULL, 0);

			prev = awk->sio.inp->prev;

			HAWK_ASSERT (awk, awk->sio.inp->name != HAWK_NULL);
			hawk_freemem (awk, awk->sio.inp);

			awk->sio.inp = prev;
		}
	}
	else if (ret == 0) 
	{
		/* no error occurred so far */
		HAWK_ASSERT (awk, awk->sio.inp == &awk->sio.arg);
		CLRERR (awk);
	}

	if (awk->sio.inf(awk, HAWK_SIO_CMD_CLOSE, awk->sio.inp, HAWK_NULL, 0) != 0)
	{
		if (ret == 0)
		{
			/* this is to keep the earlier error above
			 * that might be more critical than this */
			if (ISNOERR(awk)) 
				SETERR_ARG (awk, HAWK_ECLOSE, HAWK_T("<SIN>"), 5);
			ret = -1;
		}
	}

	if (ret <= -1) 
	{
		/* clear the parse tree partially constructed on error */
		hawk_clear (awk);
	}

	return ret;
}

void hawk_clearsionames (hawk_t* awk)
{
	hawk_link_t* cur;
	while (awk->sio_names)
	{
		cur = awk->sio_names;
		awk->sio_names = cur->link;
		hawk_freemem (awk, cur);
	}
}

int hawk_parse (hawk_t* awk, hawk_sio_cbs_t* sio)
{
	int n;

	/* the source code istream must be provided */
	HAWK_ASSERT (awk, sio != HAWK_NULL);

	/* the source code input stream must be provided at least */
	HAWK_ASSERT (awk, sio->in != HAWK_NULL);

	if (!sio || !sio->in)
	{
		SETERR_COD (awk, HAWK_EINVAL);
		return -1;
	}

	HAWK_ASSERT (awk, awk->parse.depth.loop == 0);
	HAWK_ASSERT (awk, awk->parse.depth.expr == 0);

	hawk_clear (awk);
	hawk_clearsionames (awk);

	HAWK_MEMSET (&awk->sio, 0, HAWK_SIZEOF(awk->sio));
	awk->sio.inf = sio->in;
	awk->sio.outf = sio->out;
	awk->sio.last.c = HAWK_OOCI_EOF;
	awk->sio.arg.line = 1;
	awk->sio.arg.colm = 1;
	awk->sio.arg.pragma_trait = 0;
	awk->sio.inp = &awk->sio.arg;

	n = parse(awk);
	if (n == 0  && awk->sio.outf != HAWK_NULL) n = deparse (awk);

	HAWK_ASSERT (awk, awk->parse.depth.loop == 0);
	HAWK_ASSERT (awk, awk->parse.depth.expr == 0);

	return n;
}

static int end_include (hawk_t* awk)
{
	int x;
	hawk_sio_arg_t* cur;

	if (awk->sio.inp == &awk->sio.arg) return 0; /* no include */

	/* if it is an included file, close it and
	 * retry to read a character from an outer file */

	CLRERR (awk);
	x = awk->sio.inf(awk, HAWK_SIO_CMD_CLOSE, awk->sio.inp, HAWK_NULL, 0);

	/* if closing has failed, still destroy the
	 * sio structure first as normal and return
	 * the failure below. this way, the caller 
	 * does not call HAWK_SIO_CMD_CLOSE on 
	 * awk->sio.inp again. */

	cur = awk->sio.inp;
	awk->sio.inp = awk->sio.inp->prev;

	HAWK_ASSERT (awk, cur->name != HAWK_NULL);
	/* restore the pragma values */
	awk->parse.pragma.trait = cur->pragma_trait;
	hawk_freemem (awk, cur);
	awk->parse.depth.incl--;

	if (x != 0)
	{
		/* the failure mentioned above is returned here */
		if (ISNOERR(awk)) SETERR_ARG (awk, HAWK_ECLOSE, HAWK_T("<SIN>"), 5);
		return -1;
	}

	awk->sio.last = awk->sio.inp->last;
	return 1; /* ended the included file successfully */
}

static int ever_included (hawk_t* awk, hawk_sio_arg_t* arg)
{
	hawk_oow_t i;
	for (i = 0; i < awk->parse.incl_hist.count; i++)
	{
		if (HAWK_MEMCMP(&awk->parse.incl_hist.ptr[i * HAWK_SIZEOF(arg->unique_id)], arg->unique_id, HAWK_SIZEOF(arg->unique_id)) == 0) return 1;
	}
	return 0;
}

static int record_ever_included (hawk_t* awk, hawk_sio_arg_t* arg)
{
	if (awk->parse.incl_hist.count >= awk->parse.incl_hist.capa)
	{
		hawk_uint8_t* tmp;
		hawk_oow_t newcapa;

		newcapa = awk->parse.incl_hist.capa + 128;
		tmp = (hawk_uint8_t*)hawk_reallocmem(awk, awk->parse.incl_hist.ptr, newcapa * HAWK_SIZEOF(arg->unique_id));
		if (!tmp) return -1;

		awk->parse.incl_hist.ptr = tmp;
		awk->parse.incl_hist.capa = newcapa;
	}

	HAWK_MEMCPY (&awk->parse.incl_hist.ptr[awk->parse.incl_hist.count * HAWK_SIZEOF(arg->unique_id)], arg->unique_id, HAWK_SIZEOF(arg->unique_id));
	awk->parse.incl_hist.count++;
	return 0;
}

static int begin_include (hawk_t* awk, int once)
{
	hawk_sio_arg_t* arg = HAWK_NULL;
	hawk_link_t* link;

	if (hawk_count_oocstr(HAWK_OOECS_PTR(awk->tok.name)) != HAWK_OOECS_LEN(awk->tok.name))
	{
		/* a '\0' character included in the include file name.
		 * we don't support such a file name */
		SETERR_ARG_LOC (
			awk, 
			HAWK_EIONMNL,
			HAWK_OOECS_PTR(awk->tok.name),
			hawk_count_oocstr(HAWK_OOECS_PTR(awk->tok.name)),
			&awk->tok.loc
		);
		return -1;
	}

	if (awk->opt.incldirs.ptr)
	{
		/* include directory is set... */
/* TODO: search target files in these directories */
	}

	/* store the include-file name into a list
	 * and this list is not deleted after hawk_parse.
	 * the errinfo.loc.file can point to a include_oncestring here. */
	link = (hawk_link_t*)hawk_callocmem(awk, HAWK_SIZEOF(*link) + 
		HAWK_SIZEOF(*arg) + HAWK_SIZEOF(hawk_ooch_t) * (HAWK_OOECS_LEN(awk->tok.name) + 1));
	if (!link)
	{
		ADJERR_LOC (awk, &awk->ptok.loc);
		goto oops;
	}

	hawk_copy_oochars_to_oocstr_unlimited ((hawk_ooch_t*)(link + 1), HAWK_OOECS_PTR(awk->tok.name), HAWK_OOECS_LEN(awk->tok.name));
	link->link = awk->sio_names;
	awk->sio_names = link;

	arg = (hawk_sio_arg_t*)hawk_callocmem(awk, HAWK_SIZEOF(*awk));
	if (!arg)
	{
		ADJERR_LOC (awk, &awk->ptok.loc);
		goto oops;
	}

	arg->name = (const hawk_ooch_t*)(link + 1);
	arg->line = 1;
	arg->colm = 1;

	/* let the argument's prev field point to the current */
	arg->prev = awk->sio.inp;

	CLRERR (awk);
	if (awk->sio.inf(awk, HAWK_SIO_CMD_OPEN, arg, HAWK_NULL, 0) <= -1)
	{
		if (ISNOERR(awk)) SETERR_TOK (awk, HAWK_EOPEN);
		else awk->_gem.errloc = awk->tok.loc; /* adjust error location */
		goto oops;
	}

	/* store the pragma value */
	arg->pragma_trait = awk->parse.pragma.trait;
	/* but don't change awk->parse.pragma.trait. it means the included file inherits
	 * the existing progma values. 
	awk->parse.pragma.trait = (awk->opt.trait & HAWK_IMPLICIT);
	*/

	/* i update the current pointer after opening is successful */
	awk->sio.inp = arg;
	awk->parse.depth.incl++;

	if (once && ever_included(awk, arg))
	{
		end_include (awk); 
		/* it has been included previously. don't include this file again. */
		if (get_token(awk) <= -1) return -1; /* skip the include file name */
		if (MATCH(awk, TOK_SEMICOLON) || MATCH(awk, TOK_NEWLINE))
		{

			if (get_token(awk) <= -1) return -1; /* skip the semicolon */
		}
	}
	else
	{
		/* read in the first character in the included file. 
		 * so the next call to get_token() sees the character read
		 * from this file. */
		if (record_ever_included(awk, arg) <= -1 || get_char(awk) <= -1 || get_token(awk) <= -1)
		{
			end_include (awk); 
			/* i don't jump to oops since i've called 
			 * end_include() where awk->sio.inp/arg is freed. */
			return -1;
		}
	}

	return 0;

oops:
	/* i don't need to free 'link'  here since it's linked to awk->sio_names
	 * that's freed at the beginning of hawk_parse() or by hawk_close(). */
	if (arg) hawk_freemem (awk, arg);
	return -1;
}

static int parse_progunit (hawk_t* awk)
{
	/*
	@include "xxxx"
	@global xxx, xxxx;
	BEGIN { action }
	END { action }
	pattern { action }
	function name (parameter-list) { statement }
	*/

	HAWK_ASSERT (awk, awk->parse.depth.loop == 0);

	if (MATCH(awk, TOK_XGLOBAL)) 
	{
		hawk_oow_t ngbls;

		awk->parse.id.block = PARSE_GBL;

		if (get_token(awk) <= -1) return -1;

		HAWK_ASSERT (awk, awk->tree.ngbls == HAWK_ARR_SIZE(awk->parse.gbls));
		ngbls = awk->tree.ngbls;
		if (collect_globals(awk) == HAWK_NULL) 
		{
			hawk_arr_delete (awk->parse.gbls, ngbls, HAWK_ARR_SIZE(awk->parse.gbls) - ngbls);
			awk->tree.ngbls = ngbls;
			return -1;
		}
	}
	else if (MATCH(awk, TOK_XINCLUDE) || MATCH(awk, TOK_XINCLUDE_ONCE))
	{
		int once;

		if (awk->opt.depth.s.incl > 0 &&
		    awk->parse.depth.incl >=  awk->opt.depth.s.incl)
		{
			SETERR_LOC (awk, HAWK_EINCLTD, &awk->ptok.loc);
			return -1;
		}

		once = MATCH(awk, TOK_XINCLUDE_ONCE);
		if (get_token(awk) <= -1) return -1;

		if (!MATCH(awk, TOK_STR))
		{
			SETERR_LOC (awk, HAWK_EINCLSTR, &awk->ptok.loc);
			return -1;
		}

		if (begin_include(awk, once) <= -1) return -1;

		/* i just return without doing anything special
		 * after having setting up the environment for file 
		 * inclusion. the loop in parse() proceeds to call 
		 * parse_progunit() */
	}
	else if (MATCH(awk, TOK_XPRAGMA))
	{
		hawk_oocs_t name;

		if (get_token(awk) <= -1) return -1;
		if (!MATCH(awk, TOK_IDENT))
		{
			hawk_seterrfmt (awk, HAWK_EIDENT, &awk->ptok.loc, HAWK_T("identifier expected for '@pragma'"));
			return -1;
		}

		name.len = HAWK_OOECS_LEN(awk->tok.name);
		name.ptr = HAWK_OOECS_PTR(awk->tok.name);

		if (hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("implicit")) == 0)
		{
			/* @pragma implicit on
			 * @pragma implicit off */
			if (get_token(awk) <= -1) return -1;
			if (!MATCH(awk, TOK_IDENT))
			{
			error_ident_on_off_expected_for_implicit:
				hawk_seterrfmt (awk, HAWK_EIDENT, &awk->ptok.loc, HAWK_T("identifier 'on' or 'off' expected for 'implicit'"));
				return -1;
			}

			name.len = HAWK_OOECS_LEN(awk->tok.name);
			name.ptr = HAWK_OOECS_PTR(awk->tok.name);
			if (hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("on")) == 0)
			{
				awk->parse.pragma.trait |= HAWK_IMPLICIT;
			}
			else if (hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("off")) == 0)
			{
				awk->parse.pragma.trait &= ~HAWK_IMPLICIT;
			}
			else
			{
				goto error_ident_on_off_expected_for_implicit;
			}
		}
		else if (hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("stack_limit")) == 0)
		{
			hawk_int_t sl;

			/* @pragma stack_limit 99999 */
			if (get_token(awk) <= -1) return -1;
			if (!MATCH(awk, TOK_INT))
			{
				SETERR_TOK (awk, HAWK_EINTLIT);
				return -1;
			}

			sl = hawk_strxtoint(awk, HAWK_OOECS_PTR(awk->tok.name), HAWK_OOECS_LEN(awk->tok.name), 0, HAWK_NULL);
			if (sl < HAWK_MIN_RTX_STACK_LIMIT) sl = HAWK_MIN_RTX_STACK_LIMIT;
			else if (sl > HAWK_MAX_RTX_STACK_LIMIT) sl = HAWK_MAX_RTX_STACK_LIMIT;
			/* take the specified value if it's greater than the existing value */
			if (sl > awk->parse.pragma.rtx_stack_limit) awk->parse.pragma.rtx_stack_limit = sl;
			
		}
		else
		{
			hawk_seterrfmt (awk, HAWK_EIDENT, &awk->ptok.loc, HAWK_T("unknown @pragma identifier - %.*s"), (int)name.len, name.ptr);
			return -1;
		}

		if (get_token(awk) <= -1) return -1;
		if (MATCH(awk,TOK_SEMICOLON) && get_token(awk) <= -1) return -1;
	}
	else if (MATCH(awk, TOK_FUNCTION)) 
	{
		awk->parse.id.block = PARSE_FUNCTION;
		if (parse_function(awk) == HAWK_NULL) return -1;
	}
	else if (MATCH(awk, TOK_BEGIN)) 
	{
		if ((awk->opt.trait & HAWK_PABLOCK) == 0)
		{
			SETERR_TOK (awk, HAWK_EKWFNC);
			return -1;
		}

		awk->parse.id.block = PARSE_BEGIN;
		if (get_token(awk) <= -1) return -1; 

		if (MATCH(awk, TOK_NEWLINE) || MATCH(awk, TOK_EOF))
		{
			/* when HAWK_NEWLINE is set,
	   		 * BEGIN and { should be located on the same line */
			SETERR_LOC (awk, HAWK_EBLKBEG, &awk->ptok.loc);
			return -1;
		}

		if (!MATCH(awk, TOK_LBRACE)) 
		{
			SETERR_TOK (awk, HAWK_ELBRACE);
			return -1;
		}

		awk->parse.id.block = PARSE_BEGIN_BLOCK;
		if (parse_begin(awk) == HAWK_NULL) return -1;

		/* skip a semicolon after an action block if any */
		if (MATCH(awk, TOK_SEMICOLON) && get_token(awk) <= -1) return -1;
	}
	else if (MATCH(awk, TOK_END)) 
	{
		if ((awk->opt.trait & HAWK_PABLOCK) == 0)
		{
			SETERR_TOK (awk, HAWK_EKWFNC);
			return -1;
		}

		awk->parse.id.block = PARSE_END;
		if (get_token(awk) <= -1) return -1; 

		if (MATCH(awk, TOK_NEWLINE) || MATCH(awk, TOK_EOF))
		{
			/* when HAWK_NEWLINE is set,
	   		 * END and { should be located on the same line */
			SETERR_LOC (awk, HAWK_EBLKEND, &awk->ptok.loc);
			return -1;
		}

		if (!MATCH(awk, TOK_LBRACE)) 
		{
			SETERR_TOK (awk, HAWK_ELBRACE);
			return -1;
		}

		awk->parse.id.block = PARSE_END_BLOCK;
		if (parse_end(awk) == HAWK_NULL) return -1;

		/* skip a semicolon after an action block if any */
		if (MATCH(awk,TOK_SEMICOLON) && get_token (awk) <= -1) return -1;
	}
	else if (MATCH(awk, TOK_LBRACE))
	{
		/* patternless block */
		if ((awk->opt.trait & HAWK_PABLOCK) == 0)
		{
			SETERR_TOK (awk, HAWK_EKWFNC);
			return -1;
		}

		awk->parse.id.block = PARSE_ACTION_BLOCK;
		if (parse_action_block(awk, HAWK_NULL, 0) == HAWK_NULL) return -1;

		/* skip a semicolon after an action block if any */
		if (MATCH(awk, TOK_SEMICOLON) && get_token(awk) <= -1) return -1;
	}
	else
	{
		/* 
		expressions 
		/regular expression/
		pattern && pattern
		pattern || pattern
		!pattern
		(pattern)
		pattern, pattern
		*/
		hawk_nde_t* ptn;
		hawk_loc_t eloc;

		if ((awk->opt.trait & HAWK_PABLOCK) == 0)
		{
			SETERR_TOK (awk, HAWK_EKWFNC);
			return -1;
		}

		awk->parse.id.block = PARSE_PATTERN;

		eloc = awk->tok.loc;
		ptn = parse_expr_withdc (awk, &eloc);
		if (ptn == HAWK_NULL) return -1;

		HAWK_ASSERT (awk, ptn->next == HAWK_NULL);

		if (MATCH(awk,TOK_COMMA))
		{
			if (get_token (awk) <= -1) 
			{
				hawk_clrpt (awk, ptn);
				return -1;
			}	

			eloc = awk->tok.loc;
			ptn->next = parse_expr_withdc (awk, &eloc);

			if (ptn->next == HAWK_NULL) 
			{
				hawk_clrpt (awk, ptn);
				return -1;
			}
		}

		if (MATCH(awk,TOK_NEWLINE) || MATCH(awk,TOK_SEMICOLON) || MATCH(awk,TOK_EOF))
		{
			/* blockless pattern */
			int eof;
			hawk_loc_t ploc;


			eof = MATCH(awk,TOK_EOF);
			ploc  = awk->ptok.loc;

			awk->parse.id.block = PARSE_ACTION_BLOCK;
			if (parse_action_block (awk, ptn, 1) == HAWK_NULL)
			{
				hawk_clrpt (awk, ptn);
				return -1;	
			}

			if (!eof)
			{
				if (get_token(awk) <= -1) 
				{
					/* 'ptn' has been added to the chain. 
					 * it doesn't have to be cleared here
					 * as hawk_clear does it */
					/*hawk_clrpt (awk, ptn);*/
					return -1;
				}	
			}

			if ((awk->opt.trait & HAWK_RIO) != HAWK_RIO)
			{
				/* blockless pattern requires HAWK_RIO
				 * to be ON because the implicit block is
				 * "print $0" */
				SETERR_LOC (awk, HAWK_ENOSUP, &ploc);
				return -1;
			}
		}
		else
		{
			/* parse the action block */
			if (!MATCH(awk,TOK_LBRACE))
			{
				hawk_clrpt (awk, ptn);
				SETERR_TOK (awk, HAWK_ELBRACE);
				return -1;
			}

			awk->parse.id.block = PARSE_ACTION_BLOCK;
			if (parse_action_block (awk, ptn, 0) == HAWK_NULL) 
			{
				hawk_clrpt (awk, ptn);
				return -1;	
			}

			/* skip a semicolon after an action block if any */
			if (MATCH(awk,TOK_SEMICOLON) && 
			    get_token (awk) <= -1) return -1;
		}
	}

	return 0;
}

static hawk_nde_t* parse_function (hawk_t* awk)
{
	hawk_oocs_t name;
	hawk_nde_t* body = HAWK_NULL;
	hawk_fun_t* fun = HAWK_NULL;
	hawk_ooch_t* argspec = HAWK_NULL;
	hawk_oow_t argspeccapa = 0;
	hawk_oow_t nargs, g;
	hawk_htb_pair_t* pair;
	hawk_loc_t xloc;
	int rederr;

	/* eat up the keyword 'function' and get the next token */
	HAWK_ASSERT (awk, MATCH(awk,TOK_FUNCTION));
	if (get_token(awk) <= -1) return HAWK_NULL;  

	/* check if an identifier is in place */
	if (!MATCH(awk,TOK_IDENT)) 
	{
		/* cannot find a valid identifier for a function name */
		SETERR_TOK (awk, HAWK_EFUNNAM);
		return HAWK_NULL;
	}

	name.len = HAWK_OOECS_LEN(awk->tok.name);
	name.ptr = HAWK_OOECS_PTR(awk->tok.name);

	/* note that i'm assigning to rederr in the 'if' conditions below.
 	 * i'm not checking equality */
	    /* check if it is a builtin function */
	if ((hawk_findfncwithoocs(awk, &name) != HAWK_NULL && (rederr = HAWK_EFNCRED)) ||
	    /* check if it has already been defined as a function */
	    (hawk_htb_search (awk->tree.funs, name.ptr, name.len) != HAWK_NULL && (rederr = HAWK_EFUNRED)) ||
	    /* check if it conflicts with a named variable */
	    (hawk_htb_search (awk->parse.named, name.ptr, name.len) != HAWK_NULL && (rederr = HAWK_EVARRED)) ||
	    /* check if it coincides to be a global variable name */
	    (((g = find_global (awk, &name)) != HAWK_ARR_NIL) && (rederr = HAWK_EGBLRED)))
	{
		hawk_seterror (awk, rederr, &name, &awk->tok.loc);
		return HAWK_NULL;
	}

	/* duplicate the name before it's overridden by get_token() */
	name.ptr = hawk_dupoochars(awk, name.ptr, name.len);
	if (!name.ptr)
	{
		ADJERR_LOC (awk, &awk->tok.loc);
		return HAWK_NULL;
	}

	/* get the next token */
	if (get_token(awk) <= -1) goto oops;

	/* match a left parenthesis */
	if (!MATCH(awk,TOK_LPAREN)) 
	{
		/* a function name is not followed by a left parenthesis */
		SETERR_TOK (awk, HAWK_ELPAREN);
		goto oops;
	}

	/* get the next token */
	if (get_token(awk) <= -1) goto oops;

	/* make sure that parameter table is empty */
	HAWK_ASSERT (awk, HAWK_ARR_SIZE(awk->parse.params) == 0);

	/* read parameter list */
	if (MATCH(awk,TOK_RPAREN)) 
	{
		/* no function parameter found. get the next token */
		if (get_token(awk) <= -1) goto oops;
	}
	else 
	{
		while (1) 
		{
			hawk_ooch_t* pa;
			hawk_oow_t pal;

			if (MATCH(awk, TOK_BAND))
			{
				/* pass-by-reference argument */
				nargs = HAWK_ARR_SIZE(awk->parse.params);
				if (nargs >= argspeccapa)
				{
					hawk_oow_t i, newcapa = HAWK_ALIGN_POW2(nargs + 1, 64);
					argspec = hawk_reallocmem(awk, argspec, newcapa * HAWK_SIZEOF(*argspec));
					if (!argspec) goto oops;
					for (i = argspeccapa; i < newcapa; i++) argspec[i] = HAWK_T(' ');
					argspeccapa = newcapa;
				}
				argspec[nargs] = HAWK_T('r');
				if (get_token(awk) <= -1) goto oops;
			}

			if (!MATCH(awk,TOK_IDENT)) 
			{
				SETERR_TOK (awk, HAWK_EBADPAR);
				goto oops;
			}

			pa = HAWK_OOECS_PTR(awk->tok.name);
			pal = HAWK_OOECS_LEN(awk->tok.name);

			/* NOTE: the following is not a conflict. 
			 *       so the parameter is not checked against
			 *       global variables.
			 *  global x; 
			 *  function f (x) { print x; } 
			 *  x in print x is a parameter
			 */

			/* check if a parameter conflicts with the function 
			 * name or other parameters */
			if (((awk->opt.trait & HAWK_STRICTNAMING) &&
			     hawk_comp_oochars(pa, pal, name.ptr, name.len, 0) == 0) ||
			    hawk_arr_search(awk->parse.params, 0, pa, pal) != HAWK_ARR_NIL)
			{
				SETERR_ARG_LOC (awk, HAWK_EDUPPAR, pa, pal, &awk->tok.loc);
				goto oops;
			}

			/* push the parameter to the parameter list */
			if (HAWK_ARR_SIZE(awk->parse.params) >= HAWK_MAX_PARAMS)
			{
				SETERR_LOC (awk, HAWK_EPARTM, &awk->tok.loc);
				goto oops;
			}

			if (hawk_arr_insert(awk->parse.params, HAWK_ARR_SIZE(awk->parse.params), pa, pal) == HAWK_ARR_NIL)
			{
				SETERR_LOC (awk, HAWK_ENOMEM, &awk->tok.loc);
				goto oops;
			}

			if (get_token(awk) <= -1) goto oops;

			if (MATCH(awk,TOK_RPAREN)) break;

			if (!MATCH(awk,TOK_COMMA)) 
			{
				SETERR_TOK (awk, HAWK_ECOMMA);
				goto oops;
			}

			do
			{
				if (get_token(awk) <= -1) goto oops;
			}
			while (MATCH(awk,TOK_NEWLINE));
		}

		if (get_token(awk) <= -1) goto oops;
	}

	/* function body can be placed on a different line 
	 * from a function name and the parameters even if
	 * HAWK_NEWLINE is set. note TOK_NEWLINE is
	 * available only when the option is set. */
	while (MATCH(awk,TOK_NEWLINE))
	{
		if (get_token(awk) <= -1) goto oops;
	}

	/* check if the function body starts with a left brace */
	if (!MATCH(awk,TOK_LBRACE)) 
	{
		SETERR_TOK (awk, HAWK_ELBRACE);
		goto oops;
	}
	if (get_token(awk) <= -1) goto oops;

	/* remember the current function name so that the body parser
	 * can know the name of the current function being parsed */
	awk->tree.cur_fun.ptr = name.ptr;
	awk->tree.cur_fun.len = name.len;

	/* actual function body */
	xloc = awk->ptok.loc;
	body = parse_block_dc(awk, &xloc, 1);

	/* clear the current function name remembered */
	awk->tree.cur_fun.ptr = HAWK_NULL;
	awk->tree.cur_fun.len = 0;

	if (!body) goto oops;

	/* TODO: study furthur if the parameter names should be saved 
	 *       for some reasons - might be needed for better deparsing output */
	nargs = HAWK_ARR_SIZE(awk->parse.params);
	/* parameter names are not required anymore. clear them */
	hawk_arr_clear (awk->parse.params);

	fun = (hawk_fun_t*)hawk_callocmem(awk, HAWK_SIZEOF(*fun));
	if (fun == HAWK_NULL) 
	{
		ADJERR_LOC (awk, &awk->tok.loc);
		goto oops;
	}

	/*fun->name.ptr = HAWK_NULL;*/ /* function name is set below */
	fun->name.len = 0;
	fun->nargs = nargs;
	fun->argspec = argspec;
	fun->body = body;

	pair = hawk_htb_insert(awk->tree.funs, name.ptr, name.len, fun, 0);
	if (pair == HAWK_NULL)
	{
		/* if hawk_htb_insert() fails for other reasons than memory 
		 * shortage, there should be implementaion errors as duplicate
		 * functions are detected earlier in this function */
		SETERR_LOC (awk, HAWK_ENOMEM, &awk->tok.loc);
		goto oops;
	}

	/* do some trick to save a string. make it back-point at the key part 
	 * of the pair */
	fun->name.ptr = HAWK_HTB_KPTR(pair); 
	fun->name.len = HAWK_HTB_KLEN(pair);
	hawk_freemem (awk, name.ptr);

	/* remove an undefined function call entry from the parse.fun table */
	hawk_htb_delete (awk->parse.funs, fun->name.ptr, name.len);
	return body;

oops:
	if (body) hawk_clrpt (awk, body);
	if (argspec) hawk_freemem (awk, argspec);
	if (fun) hawk_freemem (awk, fun);
	hawk_freemem (awk, name.ptr);
	hawk_arr_clear (awk->parse.params);
	return HAWK_NULL;
}

static hawk_nde_t* parse_begin (hawk_t* awk)
{
	hawk_nde_t* nde;
	hawk_loc_t xloc;

	xloc = awk->tok.loc;
	HAWK_ASSERT (awk, MATCH(awk,TOK_LBRACE));

	if (get_token(awk) <= -1) return HAWK_NULL; 
	nde = parse_block_dc (awk, &xloc, 1);
	if (nde == HAWK_NULL) return HAWK_NULL;

	if (awk->tree.begin == HAWK_NULL)
	{
		awk->tree.begin = nde;
		awk->tree.begin_tail = nde;
	}
	else
	{
		awk->tree.begin_tail->next = nde;
		awk->tree.begin_tail = nde;
	}

	return nde;
}

static hawk_nde_t* parse_end (hawk_t* awk)
{
	hawk_nde_t* nde;
	hawk_loc_t xloc;

	xloc = awk->tok.loc;
	HAWK_ASSERT (awk, MATCH(awk,TOK_LBRACE));

	if (get_token(awk) <= -1) return HAWK_NULL; 
	nde = parse_block_dc (awk, &xloc, 1);
	if (nde == HAWK_NULL) return HAWK_NULL;

	if (awk->tree.end == HAWK_NULL)
	{
		awk->tree.end = nde;
		awk->tree.end_tail = nde;
	}
	else
	{
		awk->tree.end_tail->next = nde;
		awk->tree.end_tail = nde;
	}
	return nde;
}

static hawk_chain_t* parse_action_block (
	hawk_t* awk, hawk_nde_t* ptn, int blockless)
{
	hawk_nde_t* nde;
	hawk_chain_t* chain;
	hawk_loc_t xloc;

	xloc = awk->tok.loc;
	if (blockless) nde = HAWK_NULL;
	else
	{
		HAWK_ASSERT (awk, MATCH(awk,TOK_LBRACE));
		if (get_token(awk) <= -1) return HAWK_NULL; 
		nde = parse_block_dc(awk, &xloc, 1);
		if (nde == HAWK_NULL) return HAWK_NULL;
	}

	chain = (hawk_chain_t*)hawk_callocmem(awk, HAWK_SIZEOF(*chain));
	if (chain == HAWK_NULL) 
	{
		hawk_clrpt (awk, nde);
		ADJERR_LOC (awk, &xloc);
		return HAWK_NULL;
	}

	chain->pattern = ptn;
	chain->action = nde;
	chain->next = HAWK_NULL;

	if (awk->tree.chain == HAWK_NULL) 
	{
		awk->tree.chain = chain;
		awk->tree.chain_tail = chain;
		awk->tree.chain_size++;
	}
	else 
	{
		awk->tree.chain_tail->next = chain;
		awk->tree.chain_tail = chain;
		awk->tree.chain_size++;
	}

	return chain;
}

static hawk_nde_t* parse_block (hawk_t* awk, const hawk_loc_t* xloc, int istop) 
{
	hawk_nde_t* head, * curr, * nde;
	hawk_nde_blk_t* block;
	hawk_oow_t nlcls, nlcls_max, tmp;

	nlcls = HAWK_ARR_SIZE(awk->parse.lcls);
	nlcls_max = awk->parse.nlcls_max;

	/* local variable declarations */
	while (1) 
	{
		/* skip new lines before local declaration in a block*/
		while (MATCH(awk,TOK_NEWLINE))
		{
			if (get_token(awk) <= -1) return HAWK_NULL;
		}

		if (MATCH(awk,TOK_XINCLUDE) || MATCH(awk, TOK_XINCLUDE_ONCE))
		{
			/* @include ... */
			int once;

			if (awk->opt.depth.s.incl > 0 &&
			    awk->parse.depth.incl >=  awk->opt.depth.s.incl)
			{
				SETERR_LOC (awk, HAWK_EINCLTD, &awk->ptok.loc);
				return HAWK_NULL;
			}

			once = MATCH(awk, TOK_XINCLUDE_ONCE);
			if (get_token(awk) <= -1) return HAWK_NULL;
	
			if (!MATCH(awk,TOK_STR))
			{
				SETERR_LOC (awk, HAWK_EINCLSTR, &awk->ptok.loc);
				return HAWK_NULL;
			}

			if (begin_include(awk, once) <= -1) return HAWK_NULL;
		}
		else if (MATCH(awk,TOK_XLOCAL))
		{
			/* @local ... */
			if (get_token(awk) <= -1) 
			{
				hawk_arr_delete (awk->parse.lcls, nlcls, HAWK_ARR_SIZE(awk->parse.lcls)-nlcls);
				return HAWK_NULL;
			}
	
			if (collect_locals (awk, nlcls, istop) == HAWK_NULL)
			{
				hawk_arr_delete (awk->parse.lcls, nlcls, HAWK_ARR_SIZE(awk->parse.lcls)-nlcls);
				return HAWK_NULL;
			}
		}
		else break;
	}

	/* block body */
	head = HAWK_NULL; curr = HAWK_NULL;

	while (1) 
	{
		/* skip new lines within a block */
		while (MATCH(awk,TOK_NEWLINE))
		{
			if (get_token(awk) <= -1) return HAWK_NULL;
		}

		/* if EOF is met before the right brace, this is an error */
		if (MATCH(awk,TOK_EOF)) 
		{
			hawk_arr_delete (
				awk->parse.lcls, nlcls, 
				HAWK_ARR_SIZE(awk->parse.lcls) - nlcls);
			if (head != HAWK_NULL) hawk_clrpt (awk, head);
			SETERR_LOC (awk, HAWK_EEOF, &awk->tok.loc);
			return HAWK_NULL;
		}

		/* end the block when the right brace is met */
		if (MATCH(awk,TOK_RBRACE)) 
		{
			if (get_token(awk) <= -1) 
			{
				hawk_arr_delete (
					awk->parse.lcls, nlcls, 
					HAWK_ARR_SIZE(awk->parse.lcls)-nlcls);
				if (head != HAWK_NULL) hawk_clrpt (awk, head);
				return HAWK_NULL; 
			}

			break;
		}

		else if (MATCH(awk, TOK_XINCLUDE) || MATCH(awk, TOK_XINCLUDE_ONCE))
		{
			int once;

			if (awk->opt.depth.s.incl > 0 &&
			    awk->parse.depth.incl >=  awk->opt.depth.s.incl)
			{
				SETERR_LOC (awk, HAWK_EINCLTD, &awk->ptok.loc);
				return HAWK_NULL;
			}

			once = MATCH(awk, TOK_XINCLUDE_ONCE);
			if (get_token(awk) <= -1) return HAWK_NULL;

			if (!MATCH(awk,TOK_STR))
			{
				SETERR_LOC (awk, HAWK_EINCLSTR, &awk->ptok.loc);
				return HAWK_NULL;
			}

			if (begin_include(awk, once) <= -1) return HAWK_NULL;
		}
		else
		{
			/* parse an actual statement in a block */
			{
				hawk_loc_t sloc;
				sloc = awk->tok.loc;
				nde = parse_statement (awk, &sloc);
			}

			if (nde == HAWK_NULL) 
			{
				hawk_arr_delete (
					awk->parse.lcls, nlcls, 
					HAWK_ARR_SIZE(awk->parse.lcls)-nlcls);
				if (head != HAWK_NULL) hawk_clrpt (awk, head);
				return HAWK_NULL;
			}

			/* remove unnecessary statements such as adjacent 
			 * null statements */
			if (nde->type == HAWK_NDE_NULL) 
			{
				hawk_clrpt (awk, nde);
				continue;
			}
			if (nde->type == HAWK_NDE_BLK && 
			    ((hawk_nde_blk_t*)nde)->body == HAWK_NULL) 
			{
				hawk_clrpt (awk, nde);
				continue;
			}
			
			if (curr == HAWK_NULL) head = nde;
			else curr->next = nde;
			curr = nde;
		}
	}

	block = (hawk_nde_blk_t*)hawk_callocmem(awk, HAWK_SIZEOF(*block));
	if (block == HAWK_NULL) 
	{
		hawk_arr_delete (awk->parse.lcls, nlcls, HAWK_ARR_SIZE(awk->parse.lcls) - nlcls);
		hawk_clrpt (awk, head);
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	tmp = HAWK_ARR_SIZE(awk->parse.lcls);
	if (tmp > awk->parse.nlcls_max) awk->parse.nlcls_max = tmp;

	/* remove all lcls to move it up to the top level */
	hawk_arr_delete (awk->parse.lcls, nlcls, tmp - nlcls);

	/* adjust the number of lcls for a block without any statements */
	/* if (head == HAWK_NULL) tmp = 0; */

	block->type = HAWK_NDE_BLK;
	block->loc = *xloc;
	block->body = head;

	/* TODO: not only local variables but also nested blocks, 
	unless it is part of other constructs such as if, can be promoted 
	and merged to top-level block */

	/* migrate all block-local variables to a top-level block */
	if (istop) 
	{
		block->nlcls = awk->parse.nlcls_max - nlcls;
		awk->parse.nlcls_max = nlcls_max;
	}
	else 
	{
		/*block->nlcls = tmp - nlcls;*/
		block->nlcls = 0;
	}

	return (hawk_nde_t*)block;
}

static hawk_nde_t* parse_block_dc (hawk_t* awk, const hawk_loc_t* xloc, int istop) 
{
	hawk_nde_t* nde;

	if (awk->opt.depth.s.block_parse > 0 &&
	    awk->parse.depth.block >= awk->opt.depth.s.block_parse)
	{
		SETERR_LOC (awk, HAWK_EBLKNST, xloc);
		return HAWK_NULL;
	}

	awk->parse.depth.block++;
	nde = parse_block(awk, xloc, istop);
	awk->parse.depth.block--;

	return nde;
}

int hawk_initgbls (hawk_t* awk)
{
	int id;

	/* hawk_initgbls is not generic-purpose. call this from
	 * hawk_open only. */
	HAWK_ASSERT (awk, awk->tree.ngbls_base == 0 && awk->tree.ngbls == 0);

	awk->tree.ngbls_base = 0;
	awk->tree.ngbls = 0;

	for (id = HAWK_MIN_GBL_ID; id <= HAWK_MAX_GBL_ID; id++)
	{
		hawk_oow_t g;

		g = hawk_arr_insert(awk->parse.gbls, HAWK_ARR_SIZE(awk->parse.gbls), (hawk_ooch_t*)gtab[id].name, gtab[id].namelen);
		if (g == HAWK_ARR_NIL) return -1;

		HAWK_ASSERT (awk, (int)g == id);

		awk->tree.ngbls_base++;
		awk->tree.ngbls++;
	}

	HAWK_ASSERT (awk, awk->tree.ngbls_base == HAWK_MAX_GBL_ID-HAWK_MIN_GBL_ID+1);
	return 0;
}

static void adjust_static_globals (hawk_t* awk)
{
	int id;

	HAWK_ASSERT (awk, awk->tree.ngbls_base >= HAWK_MAX_GBL_ID - HAWK_MAX_GBL_ID + 1);

	for (id = HAWK_MIN_GBL_ID; id <= HAWK_MAX_GBL_ID; id++)
	{
		if ((awk->opt.trait & gtab[id].trait) != gtab[id].trait)
		{
			HAWK_ARR_DLEN(awk->parse.gbls,id) = 0;
		}
		else
		{
			HAWK_ARR_DLEN(awk->parse.gbls,id) = gtab[id].namelen;
		}
	}
}

static hawk_oow_t get_global (hawk_t* awk, const hawk_oocs_t* name)
{
	hawk_oow_t i;
	hawk_arr_t* gbls = awk->parse.gbls;

	for (i = HAWK_ARR_SIZE(gbls); i > 0; )
	{
		i--;
		if (hawk_comp_oochars(HAWK_ARR_DPTR(gbls,i), HAWK_ARR_DLEN(gbls,i), name->ptr, name->len, 0) == 0) return i;
	}

	return HAWK_ARR_NIL;
}

static hawk_oow_t find_global (hawk_t* awk, const hawk_oocs_t* name)
{
	hawk_oow_t i;
	hawk_arr_t* gbls = awk->parse.gbls;

	for (i = 0; i < HAWK_ARR_SIZE(gbls); i++)
	{
		if (hawk_comp_oochars(HAWK_ARR_DPTR(gbls,i), HAWK_ARR_DLEN(gbls,i), name->ptr, name->len, 0) == 0) return i;
	}

	return HAWK_ARR_NIL;
}

static int add_global (hawk_t* awk, const hawk_oocs_t* name, hawk_loc_t* xloc, int disabled)
{
	hawk_oow_t ngbls;

	/* check if it is a keyword */
	if (classify_ident(awk, name) != TOK_IDENT)
	{
		SETERR_ARG_LOC (awk, HAWK_EKWRED, name->ptr, name->len, xloc);
		return -1;
	}

	/* check if it conflicts with a builtin function name */
	if (hawk_findfncwithoocs(awk, name) != HAWK_NULL)
	{
		SETERR_ARG_LOC (awk, HAWK_EFNCRED, name->ptr, name->len, xloc);
		return -1;
	}

	/* check if it conflicts with a function name */
	if (hawk_htb_search(awk->tree.funs, name->ptr, name->len) != HAWK_NULL) 
	{
		SETERR_ARG_LOC (awk, HAWK_EFUNRED, name->ptr, name->len, xloc);
		return -1;
	}

	/* check if it conflicts with a function name 
	 * caught in the function call table */
	if (hawk_htb_search(awk->parse.funs, name->ptr, name->len) != HAWK_NULL)
	{
		SETERR_ARG_LOC (awk, HAWK_EFUNRED, name->ptr, name->len, xloc);
		return -1;
	}

	/* check if it conflicts with other global variable names */
	if (find_global(awk, name) != HAWK_ARR_NIL)
	{ 
		SETERR_ARG_LOC (awk, HAWK_EDUPGBL, name->ptr, name->len, xloc);
		return -1;
	}

#if 0
	/* TODO: need to check if it conflicts with a named variable to 
	 * disallow such a program shown below (IMPLICIT & EXPLICIT on)
	 *  BEGIN {X=20; x(); x(); x(); print X}
	 *  global X;
	 *  function x() { print X++; }
	 */
	if (hawk_htb_search (awk->parse.named, name, len) != HAWK_NULL)
	{
		SETERR_ARG_LOC (awk, HAWK_EVARRED, name, len, xloc);
		return -1;
	}
#endif

	ngbls = HAWK_ARR_SIZE (awk->parse.gbls);
	if (ngbls >= HAWK_MAX_GBLS)
	{
		SETERR_LOC (awk, HAWK_EGBLTM, xloc);
		return -1;
	}

	if (hawk_arr_insert (awk->parse.gbls, 
		HAWK_ARR_SIZE(awk->parse.gbls), 
		(hawk_ooch_t*)name->ptr, name->len) == HAWK_ARR_NIL)
	{
		SETERR_LOC (awk, HAWK_ENOMEM, xloc);
		return -1;
	}

	HAWK_ASSERT (awk, ngbls == HAWK_ARR_SIZE(awk->parse.gbls) - 1);

	/* the disabled item is inserted normally but 
	 * the name length is reset to zero. */
	if (disabled) HAWK_ARR_DLEN(awk->parse.gbls,ngbls) = 0;

	awk->tree.ngbls = HAWK_ARR_SIZE (awk->parse.gbls);
	HAWK_ASSERT (awk, ngbls == awk->tree.ngbls-1);

	/* return the id which is the index to the gbl table. */
	return (int)ngbls;
}

int hawk_addgblwithbcstr (hawk_t* awk, const hawk_bch_t* name)
{
	int n;
	hawk_bcs_t ncs;

	if (awk->tree.ngbls > awk->tree.ngbls_base) 
	{
		/* this function is not allowed after hawk_parse is called */
		SETERR_COD (awk, HAWK_EPERM);
		return -1;
	}

	ncs.ptr = (hawk_bch_t*)name;
	ncs.len = hawk_count_bcstr(name);;
	if (ncs.len <= 0)
	{
		SETERR_COD (awk, HAWK_EINVAL);
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	n = add_global(awk, &ncs, HAWK_NULL, 0);
#else
	{
		hawk_ucs_t wcs;
		wcs.ptr = hawk_dupbtoucstr(awk, ncs.ptr, &wcs.len, 0);
		if (!wcs.ptr) return -1;
		n = add_global(awk, &wcs, HAWK_NULL, 0);
		hawk_freemem (awk, wcs.ptr);
	}
#endif

	/* update the count of the static globals. 
	 * the total global count has been updated inside add_global. */
	if (n >= 0) awk->tree.ngbls_base++; 

	return n;
}

int hawk_addgblwithucstr (hawk_t* awk, const hawk_uch_t* name)
{
	int n;
	hawk_ucs_t ncs;

	if (awk->tree.ngbls > awk->tree.ngbls_base) 
	{
		/* this function is not allowed after hawk_parse is called */
		SETERR_COD (awk, HAWK_EPERM);
		return -1;
	}

	ncs.ptr = (hawk_uch_t*)name;
	ncs.len = hawk_count_ucstr(name);;
	if (ncs.len <= 0)
	{
		SETERR_COD (awk, HAWK_EINVAL);
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	{
		hawk_bcs_t mbs;
		mbs.ptr = hawk_duputobcstr(awk, ncs.ptr, &mbs.len);
		if (!mbs.ptr) return -1;
		n = add_global(awk, &mbs, HAWK_NULL, 0);
		hawk_freemem (awk, mbs.ptr);
	}
#else
	n = add_global(awk, &ncs, HAWK_NULL, 0);
#endif

	/* update the count of the static globals. 
	 * the total global count has been updated inside add_global. */
	if (n >= 0) awk->tree.ngbls_base++; 

	return n;
}

#define HAWK_NUM_STATIC_GBLS \
	(HAWK_MAX_GBL_ID-HAWK_MIN_GBL_ID+1)

int hawk_delgblwithbcstr (hawk_t* awk, const hawk_bch_t* name)
{
	hawk_oow_t n;
	hawk_bcs_t ncs;
	hawk_ucs_t wcs;

	ncs.ptr = (hawk_bch_t*)name;
	ncs.len = hawk_count_bcstr(name);

	if (awk->tree.ngbls > awk->tree.ngbls_base) 
	{
		/* this function is not allow after hawk_parse is called */
		hawk_seterrnum (awk, HAWK_EPERM, HAWK_NULL);
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &ncs);
		return -1;
	}
#else
	wcs.ptr = hawk_dupbtoucstr(awk, ncs.ptr, &wcs.len, 0);
	if (!wcs.ptr) return -1;
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, wcs.ptr, wcs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &wcs);
		hawk_freemem (awk, wcs.ptr);
		return -1;
	}
	hawk_freemem (awk, wcs.ptr);
#endif

	/* invalidate the name if deletion is requested.
	 * this approach does not delete the entry.
	 * if hawk_delgbl() is called with the same name
	 * again, the entry will be appended again. 
	 * never call this funciton unless it is really required. */
	/*
	awk->parse.gbls.buf[n].name.ptr[0] = HAWK_T('\0');
	awk->parse.gbls.buf[n].name.len = 0;
	*/
	n = hawk_arr_uplete(awk->parse.gbls, n, 1);
	HAWK_ASSERT (awk, n == 1);

	return 0;
}

int hawk_delgblwithucstr (hawk_t* awk, const hawk_uch_t* name)
{
	hawk_oow_t n;
	hawk_ucs_t ncs;
	hawk_bcs_t mbs;

	ncs.ptr = (hawk_uch_t*)name;
	ncs.len = hawk_count_ucstr(name);

	if (awk->tree.ngbls > awk->tree.ngbls_base) 
	{
		/* this function is not allow after hawk_parse is called */
		hawk_seterrnum (awk, HAWK_EPERM, HAWK_NULL);
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	mbs.ptr = hawk_duputobcstr(awk, ncs.ptr, &mbs.len);
	if (!mbs.ptr) return -1;
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, mbs.ptr, mbs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &mbs);
		hawk_freemem (awk, mbs.ptr);
		return -1;
	}
	hawk_freemem (awk, mbs.ptr);
#else
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &ncs);
		return -1;
	}
#endif

	/* invalidate the name if deletion is requested.
	 * this approach does not delete the entry.
	 * if hawk_delgbl() is called with the same name
	 * again, the entry will be appended again. 
	 * never call this funciton unless it is really required. */
	/*
	awk->parse.gbls.buf[n].name.ptr[0] = HAWK_T('\0');
	awk->parse.gbls.buf[n].name.len = 0;
	*/
	n = hawk_arr_uplete(awk->parse.gbls, n, 1);
	HAWK_ASSERT (awk, n == 1);

	return 0;
}

int hawk_findgblwithbcstr (hawk_t* awk, const hawk_bch_t* name)
{
	hawk_oow_t n;
	hawk_bcs_t ncs;
	hawk_ucs_t wcs;

	ncs.ptr = (hawk_bch_t*)name;
	ncs.len = hawk_count_bcstr(name);

#if defined(HAWK_OOCH_IS_BCH)
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &ncs);
		return -1;
	}
#else
	wcs.ptr = hawk_dupbtoucstr(awk, ncs.ptr, &wcs.len, 0);
	if (!wcs.ptr) return -1;
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, wcs.ptr, wcs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &wcs);
		hawk_freemem (awk, wcs.ptr);
		return -1;
	}
	hawk_freemem (awk, wcs.ptr);
#endif

	return (int)n;
}

int hawk_findgblwithucstr (hawk_t* awk, const hawk_uch_t* name)
{
	hawk_oow_t n;
	hawk_ucs_t ncs;
	hawk_bcs_t mbs;

	ncs.ptr = (hawk_uch_t*)name;
	ncs.len = hawk_count_ucstr(name);

#if defined(HAWK_OOCH_IS_BCH)
	mbs.ptr = hawk_duputobcstr(awk, ncs.ptr, &mbs.len);
	if (!mbs.ptr) return -1;
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, mbs.ptr, mbs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &mbs);
		hawk_freemem (awk, mbs.ptr);
		return -1;
	}
	hawk_freemem (awk, mbs.ptr);
#else
	n = hawk_arr_search(awk->parse.gbls, HAWK_NUM_STATIC_GBLS, ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrnum (awk, HAWK_ENOENT, &ncs);
		return -1;
	}
#endif

	return (int)n;
}


static hawk_t* collect_globals (hawk_t* awk)
{
	if (MATCH(awk,TOK_NEWLINE))
	{
		/* special check if the first name is on the 
		 * same line when HAWK_NEWLINE is on */
		SETERR_COD (awk, HAWK_EVARMS);
		return HAWK_NULL;
	}

	while (1) 
	{
		if (!MATCH(awk,TOK_IDENT)) 
		{
			SETERR_TOK (awk, HAWK_EBADVAR);
			return HAWK_NULL;
		}

		if (add_global(awk, HAWK_OOECS_OOCS(awk->tok.name), &awk->tok.loc, 0) <= -1) return HAWK_NULL;

		if (get_token(awk) <= -1) return HAWK_NULL;

		if (MATCH_TERMINATOR_NORMAL(awk)) 
		{
			/* skip a terminator (;, <NL>) */
			if (get_token(awk) <= -1) return HAWK_NULL;
			break;
		}

		/*
		 * unlike collect_locals(), the right brace cannot
		 * terminate a global declaration as it can never be
		 * placed within a block. 
		 * so do not perform MATCH_TERMINATOR_RBRACE(awk))
		 */

		if (!MATCH(awk,TOK_COMMA)) 
		{
			SETERR_TOK (awk, HAWK_ECOMMA);
			return HAWK_NULL;
		}

		do
		{
			if (get_token(awk) <= -1) return HAWK_NULL;
		} 
		while (MATCH(awk,TOK_NEWLINE));
	}

	return awk;
}

static hawk_t* collect_locals (hawk_t* awk, hawk_oow_t nlcls, int istop)
{
	if (MATCH(awk,TOK_NEWLINE))
	{
		/* special check if the first name is on the 
		 * same line when HAWK_NEWLINE is on */
		SETERR_COD (awk, HAWK_EVARMS);
		return HAWK_NULL;
	}

	while (1) 
	{
		hawk_oocs_t lcl;
		hawk_oow_t n;

		if (!MATCH(awk,TOK_IDENT)) 
		{
			SETERR_TOK (awk, HAWK_EBADVAR);
			return HAWK_NULL;
		}

		lcl = *HAWK_OOECS_OOCS(awk->tok.name);

		/* check if it conflicts with a builtin function name 
		 * function f() { local length; } */
		if (hawk_findfncwithoocs(awk, &lcl) != HAWK_NULL)
		{
			SETERR_ARG_LOC (awk, HAWK_EFNCRED, lcl.ptr, lcl.len, &awk->tok.loc);
			return HAWK_NULL;
		}

		if (istop)
		{
			/* check if it conflicts with a parameter name.
			 * the first level declaration is treated as the same
			 * scope as the parameter list */
			n = hawk_arr_search(awk->parse.params, 0, lcl.ptr, lcl.len);
			if (n != HAWK_ARR_NIL)
			{
				SETERR_ARG_LOC (awk, HAWK_EPARRED, lcl.ptr, lcl.len, &awk->tok.loc);
				return HAWK_NULL;
			}
		}

		if (awk->opt.trait & HAWK_STRICTNAMING)
		{
			/* check if it conflicts with the owning function */
			if (awk->tree.cur_fun.ptr != HAWK_NULL)
			{
				if (hawk_comp_oochars(lcl.ptr, lcl.len, awk->tree.cur_fun.ptr, awk->tree.cur_fun.len, 0) == 0)
				{
					SETERR_ARG_LOC (awk, HAWK_EFUNRED, lcl.ptr, lcl.len, &awk->tok.loc);
					return HAWK_NULL;
				}
			}
		}

		/* check if it conflicts with other local variable names */
		n = hawk_arr_search(awk->parse.lcls, nlcls, lcl.ptr, lcl.len);
		if (n != HAWK_ARR_NIL)
		{
			SETERR_ARG_LOC (awk, HAWK_EDUPLCL, lcl.ptr, lcl.len, &awk->tok.loc);
			return HAWK_NULL;
		}

		/* check if it conflicts with global variable names */
		n = find_global (awk, &lcl);
		if (n != HAWK_ARR_NIL)
		{
			if (n < awk->tree.ngbls_base)
			{
				/* it is a conflict only if it is one of a 
				 * static global variable */
				SETERR_ARG_LOC (awk, HAWK_EDUPLCL, lcl.ptr, lcl.len, &awk->tok.loc);
				return HAWK_NULL;
			}
		}

		if (HAWK_ARR_SIZE(awk->parse.lcls) >= HAWK_MAX_LCLS)
		{
			SETERR_LOC (awk, HAWK_ELCLTM, &awk->tok.loc);
			return HAWK_NULL;
		}

		if (hawk_arr_insert (awk->parse.lcls, HAWK_ARR_SIZE(awk->parse.lcls), lcl.ptr, lcl.len) == HAWK_ARR_NIL)
		{
			SETERR_LOC (awk, HAWK_ENOMEM, &awk->tok.loc);
			return HAWK_NULL;
		}

		if (get_token(awk) <= -1) return HAWK_NULL;

		if (MATCH_TERMINATOR_NORMAL(awk)) 
		{
			/* skip the terminator (;, <NL>) */
			if (get_token(awk) <= -1) return HAWK_NULL;
			break;
		}

		if (MATCH_TERMINATOR_RBRACE(awk))
		{
			/* should not skip } */
			break;
		}

		if (!MATCH(awk,TOK_COMMA))
		{
			SETERR_TOK (awk, HAWK_ECOMMA);
			return HAWK_NULL;
		}

		do
		{
			if (get_token(awk) <= -1) return HAWK_NULL;
		}
		while (MATCH(awk,TOK_NEWLINE));
	}

	return awk;
}

static hawk_nde_t* parse_if (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* test = HAWK_NULL;
	hawk_nde_t* then_part = HAWK_NULL;
	hawk_nde_t* else_part = HAWK_NULL;
	hawk_nde_if_t* nde;
	hawk_loc_t eloc, tloc;

	if (!MATCH(awk,TOK_LPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ELPAREN);
		return HAWK_NULL;
	}
	if (get_token(awk) <= -1) return HAWK_NULL;

	eloc = awk->tok.loc;
	test = parse_expr_withdc(awk, &eloc);
	if (test == HAWK_NULL) goto oops;

	if (!MATCH(awk,TOK_RPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ERPAREN);
		goto oops;
	}

/* TODO: optimization. if you know 'test' evaluates to true or false,
 *       you can drop the 'if' statement and take either the 'then_part'
 *       or 'else_part'. */

	if (get_token(awk) <= -1) goto oops;

	tloc = awk->tok.loc;
	then_part = parse_statement(awk, &tloc);
	if (then_part == HAWK_NULL) goto oops;

	/* skip any new lines before the else block */
	while (MATCH(awk,TOK_NEWLINE))
	{
		if (get_token(awk) <= -1) goto oops;
	} 

	if (MATCH(awk,TOK_ELSE)) 
	{
		if (get_token(awk) <= -1) goto oops;

		{
			hawk_loc_t eloc;
			eloc = awk->tok.loc;
			else_part = parse_statement(awk, &eloc);
			if (else_part == HAWK_NULL) goto oops;
		}
	}

	nde = (hawk_nde_if_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_IF;
	nde->loc = *xloc;
	nde->test = test;
	nde->then_part = then_part;
	nde->else_part = else_part;

	return (hawk_nde_t*)nde;

oops:
	if (else_part) hawk_clrpt (awk, else_part);
	if (then_part) hawk_clrpt (awk, then_part);
	if (test) hawk_clrpt (awk, test);
	return HAWK_NULL;
}

static hawk_nde_t* parse_while (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* test = HAWK_NULL;
	hawk_nde_t* body = HAWK_NULL;
	hawk_nde_while_t* nde;
	hawk_loc_t ploc;

	if (!MATCH(awk,TOK_LPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ELPAREN);
		goto oops;
	}
	if (get_token(awk) <= -1) goto oops;

	ploc = awk->tok.loc;
	test = parse_expr_withdc (awk, &ploc);
	if (test == HAWK_NULL) goto oops;

	if (!MATCH(awk,TOK_RPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ERPAREN);
		goto oops;
	}

	if (get_token(awk) <= -1)  goto oops;

	ploc = awk->tok.loc;
	body = parse_statement (awk, &ploc);
	if (body == HAWK_NULL) goto oops;

	nde = (hawk_nde_while_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_WHILE;
	nde->loc = *xloc;
	nde->test = test;
	nde->body = body;

	return (hawk_nde_t*)nde;

oops:
	if (body) hawk_clrpt (awk, body);
	if (test) hawk_clrpt (awk, test);
	return HAWK_NULL;
}

static hawk_nde_t* parse_for (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* init = HAWK_NULL, * test = HAWK_NULL;
	hawk_nde_t* incr = HAWK_NULL, * body = HAWK_NULL;
	hawk_nde_for_t* nde_for; 
	hawk_nde_foreach_t* nde_foreach;
	hawk_loc_t ploc;

	if (!MATCH(awk,TOK_LPAREN))
	{
		SETERR_TOK (awk, HAWK_ELPAREN);
		return HAWK_NULL;
	}
	if (get_token(awk) <= -1) return HAWK_NULL;
		
	if (!MATCH(awk,TOK_SEMICOLON)) 
	{
		/* this line is very ugly. it checks the entire next 
		 * expression or the first element in the expression
		 * is wrapped by a parenthesis */
		int no_foreach = MATCH(awk,TOK_LPAREN);

		ploc = awk->tok.loc;
		init = parse_expr_withdc (awk, &ploc);
		if (init == HAWK_NULL) goto oops;

		if (!no_foreach && init->type == HAWK_NDE_EXP_BIN &&
		    ((hawk_nde_exp_t*)init)->opcode == HAWK_BINOP_IN &&
		    is_plain_var(((hawk_nde_exp_t*)init)->left))
		{	
			/* switch to foreach - for (x in y) */
			
			if (!MATCH(awk,TOK_RPAREN))
			{
				SETERR_TOK (awk, HAWK_ERPAREN);
				goto oops;
			}

			if (get_token(awk) <= -1) goto oops;
			
			ploc = awk->tok.loc;
			body = parse_statement (awk, &ploc);
			if (body == HAWK_NULL) goto oops;

			nde_foreach = (hawk_nde_foreach_t*) hawk_callocmem (
				awk, HAWK_SIZEOF(*nde_foreach));
			if (nde_foreach == HAWK_NULL)
			{
				ADJERR_LOC (awk, xloc);
				goto oops;
			}

			nde_foreach->type = HAWK_NDE_FOREACH;
			nde_foreach->loc = *xloc;
			nde_foreach->test = init;
			nde_foreach->body = body;

			return (hawk_nde_t*)nde_foreach;
		}

		if (!MATCH(awk,TOK_SEMICOLON)) 
		{
			SETERR_TOK (awk, HAWK_ESCOLON);
			goto oops;
		}
	}

	do
	{
		if (get_token(awk) <= -1)  goto oops;
		/* skip new lines after the first semicolon */
	} 
	while (MATCH(awk,TOK_NEWLINE));

	if (!MATCH(awk,TOK_SEMICOLON)) 
	{
		ploc = awk->tok.loc;
		test = parse_expr_withdc (awk, &ploc);
		if (test == HAWK_NULL) goto oops;

		if (!MATCH(awk,TOK_SEMICOLON)) 
		{
			SETERR_TOK (awk, HAWK_ESCOLON);
			goto oops;
		}
	}

	do
	{
		if (get_token(awk) <= -1) goto oops;
		/* skip new lines after the second semicolon */
	}
	while (MATCH(awk,TOK_NEWLINE));
	
	if (!MATCH(awk,TOK_RPAREN)) 
	{
		{
			hawk_loc_t eloc;

			eloc = awk->tok.loc;
			incr = parse_expr_withdc (awk, &eloc);
			if (incr == HAWK_NULL) goto oops;
		}

		if (!MATCH(awk,TOK_RPAREN)) 
		{
			SETERR_TOK (awk, HAWK_ERPAREN);
			goto oops;
		}
	}

	if (get_token(awk) <= -1) goto oops;

	ploc = awk->tok.loc;
	body = parse_statement (awk, &ploc);
	if (body == HAWK_NULL) goto oops;

	nde_for = (hawk_nde_for_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde_for));
	if (nde_for == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde_for->type = HAWK_NDE_FOR;
	nde_for->loc = *xloc;
	nde_for->init = init;
	nde_for->test = test;
	nde_for->incr = incr;
	nde_for->body = body;

	return (hawk_nde_t*)nde_for;

oops:
	if (init) hawk_clrpt (awk, init);
	if (test) hawk_clrpt (awk, test);
	if (incr) hawk_clrpt (awk, incr);
	if (body) hawk_clrpt (awk, body);
	return HAWK_NULL;
}

static hawk_nde_t* parse_dowhile (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* test = HAWK_NULL; 
	hawk_nde_t* body = HAWK_NULL;
	hawk_nde_while_t* nde;
	hawk_loc_t ploc;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_DO);

	ploc = awk->tok.loc;
	body = parse_statement (awk, &ploc);
	if (body == HAWK_NULL) goto oops;

	while (MATCH(awk,TOK_NEWLINE))
	{
		if (get_token(awk) <= -1) goto oops;
	}

	if (!MATCH(awk,TOK_WHILE)) 
	{
		SETERR_TOK (awk, HAWK_EKWWHL);
		goto oops;
	}

	if (get_token(awk) <= -1) goto oops;

	if (!MATCH(awk,TOK_LPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ELPAREN);
		goto oops;
	}

	if (get_token(awk) <= -1) goto oops;

	ploc = awk->tok.loc;
	test = parse_expr_withdc (awk, &ploc);
	if (test == HAWK_NULL) goto oops;

	if (!MATCH(awk,TOK_RPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ERPAREN);
		goto oops;
	}

	if (get_token(awk) <= -1)  goto oops;
	
	nde = (hawk_nde_while_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_DOWHILE;
	nde->loc = *xloc;
	nde->test = test;
	nde->body = body;

	return (hawk_nde_t*)nde;

oops:
	if (body) hawk_clrpt (awk, body);
	if (test) hawk_clrpt (awk, test);
	HAWK_ASSERT (awk, nde == HAWK_NULL);
	return HAWK_NULL;
}

static hawk_nde_t* parse_break (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_break_t* nde;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_BREAK);
	if (awk->parse.depth.loop <= 0) 
	{
		SETERR_LOC (awk, HAWK_EBREAK, xloc);
		return HAWK_NULL;
	}

	nde = (hawk_nde_break_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_BREAK;
	nde->loc = *xloc;
	
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_continue (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_continue_t* nde;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_CONTINUE);
	if (awk->parse.depth.loop <= 0) 
	{
		SETERR_LOC (awk, HAWK_ECONTINUE, xloc);
		return HAWK_NULL;
	}

	nde = (hawk_nde_continue_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_CONTINUE;
	nde->loc = *xloc;
	
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_return (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_return_t* nde;
	hawk_nde_t* val;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_RETURN);

	nde = (hawk_nde_return_t*) hawk_callocmem ( awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_RETURN;
	nde->loc = *xloc;

	if (MATCH_TERMINATOR(awk))
	{
		/* no return value */
		val = HAWK_NULL;
	}
	else 
	{
		hawk_loc_t eloc;

		eloc = awk->tok.loc;
		val = parse_expr_withdc (awk, &eloc);
		if (val == HAWK_NULL) 
		{
			hawk_freemem (awk, nde);
			return HAWK_NULL;
		}
	}

	nde->val = val;
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_exit (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_exit_t* nde;
	hawk_nde_t* val;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_EXIT || awk->ptok.type == TOK_XABORT);

	nde = (hawk_nde_exit_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_EXIT;
	nde->loc = *xloc;
	nde->abort = (awk->ptok.type == TOK_XABORT);

	if (MATCH_TERMINATOR(awk)) 
	{
		/* no exit code */
		val = HAWK_NULL;
	}
	else 
	{
		hawk_loc_t eloc;

		eloc = awk->tok.loc;
		val = parse_expr_withdc (awk, &eloc);
		if (val == HAWK_NULL) 
		{
			hawk_freemem (awk, nde);
			return HAWK_NULL;
		}
	}

	nde->val = val;
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_next (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_next_t* nde;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_NEXT);

	if (awk->parse.id.block == PARSE_BEGIN_BLOCK)
	{
		SETERR_LOC (awk, HAWK_ENEXTBEG, xloc);
		return HAWK_NULL;
	}
	if (awk->parse.id.block == PARSE_END_BLOCK)
	{
		SETERR_LOC (awk, HAWK_ENEXTEND, xloc);
		return HAWK_NULL;
	}

	nde = (hawk_nde_next_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}
	nde->type = HAWK_NDE_NEXT;
	nde->loc = *xloc;
	
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_nextfile (
	hawk_t* awk, const hawk_loc_t* xloc, int out)
{
	hawk_nde_nextfile_t* nde;

	if (!out && awk->parse.id.block == PARSE_BEGIN_BLOCK)
	{
		SETERR_LOC (awk, HAWK_ENEXTFBEG, xloc);
		return HAWK_NULL;
	}
	if (!out && awk->parse.id.block == PARSE_END_BLOCK)
	{
		SETERR_LOC (awk, HAWK_ENEXTFEND, xloc);
		return HAWK_NULL;
	}

	nde = (hawk_nde_nextfile_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_NEXTFILE;
	nde->loc = *xloc;
	nde->out = out;
	
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_delete (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_delete_t* nde;
	hawk_nde_t* var = HAWK_NULL;
	hawk_loc_t dloc;
	hawk_nde_type_t type;
	int inparen = 0;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_DELETE || 
	            awk->ptok.type == TOK_XRESET);

	type = (awk->ptok.type == TOK_DELETE)?
		HAWK_NDE_DELETE: HAWK_NDE_RESET;

	if (MATCH(awk,TOK_LPAREN))
	{
		if (get_token(awk) <= -1) goto oops;
		inparen = 1;
	}

	if (!MATCH(awk,TOK_IDENT)) 
	{
		SETERR_TOK (awk, HAWK_EIDENT);
		goto oops;
	}

	dloc = awk->tok.loc;
	var = parse_primary_ident (awk, &dloc);
	if (var == HAWK_NULL) goto oops;

	if ((type == HAWK_NDE_DELETE && !is_var (var)) ||
	    (type == HAWK_NDE_RESET && !is_plain_var (var)))
	{
		SETERR_LOC (awk, HAWK_EBADARG, &dloc);
		goto oops;
	}

	if (inparen) 
	{
		if (!MATCH(awk,TOK_RPAREN))
		{
			SETERR_TOK (awk, HAWK_ERPAREN);
			goto oops;
		}

		if (get_token(awk) <= -1) goto oops;
	}

	nde = (hawk_nde_delete_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde->type = type;
	nde->loc = *xloc;
	nde->var = var;

	return (hawk_nde_t*)nde;

oops:
	if (var) hawk_clrpt (awk, var);
	return HAWK_NULL;
}

static hawk_nde_t* parse_print (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_print_t* nde;
	hawk_nde_t* args = HAWK_NULL; 
	hawk_nde_t* out = HAWK_NULL;
	int out_type;
	hawk_nde_type_t type;
	hawk_loc_t eloc;

	HAWK_ASSERT (awk, awk->ptok.type == TOK_PRINT || 
	            awk->ptok.type == TOK_PRINTF);

	type = (awk->ptok.type == TOK_PRINT)?
		HAWK_NDE_PRINT: HAWK_NDE_PRINTF;

	if (!MATCH_TERMINATOR(awk) &&
	    !MATCH(awk,TOK_GT) &&
	    !MATCH(awk,TOK_RS) &&
	    !MATCH(awk,TOK_BOR) &&
	    !MATCH(awk,TOK_LOR)) 
	{
		hawk_nde_t* args_tail;
		hawk_nde_t* tail_prev;

		int in_parens = 0, gm_in_parens = 0;
		hawk_oow_t opening_lparen_seq;

		if (MATCH(awk,TOK_LPAREN))
		{
			/* just remember the sequence number of the left 
			 * parenthesis before calling parse_expr_withdc() 
			 * that eventually calls parse_primary_lparen() */
			opening_lparen_seq = awk->parse.lparen_seq;
			in_parens = 1; /* maybe. not confirmed yet */

			/* print and printf provide weird syntaxs.
			 * 
			 *    1. print 10, 20;
			 *    2. print (10, 20);
			 *    3. print (10,20,30) in a;
			 *    4. print ((10,20,30) in a);
			 *
			 * Due to case 3, i can't consume LPAREN
			 * here and expect RPAREN later. 
			 */
		}

		eloc = awk->tok.loc;
		args = parse_expr_withdc (awk, &eloc);
		if (args == HAWK_NULL) goto oops;

		args_tail = args;
		tail_prev = HAWK_NULL;

		if (args->type != HAWK_NDE_GRP)
		{
			/* args->type == HAWK_NDE_GRP when print (a, b, c) 
			 * args->type != HAWK_NDE_GRP when print a, b, c */
			hawk_oow_t group_opening_lparen_seq;
			
			while (MATCH(awk,TOK_COMMA))
			{
				do 
				{
					if (get_token(awk) <= -1) goto oops;
				}
				while (MATCH(awk,TOK_NEWLINE));

				/* if it's grouped, i must check if the last group member
				 * is enclosed in parentheses.
				 *
				 * i set the condition to false whenever i see
				 * a new group member. */
				gm_in_parens = 0;
				if (MATCH(awk,TOK_LPAREN))
				{
					group_opening_lparen_seq = awk->parse.lparen_seq;
					gm_in_parens = 1; /* maybe */
				}

				eloc = awk->tok.loc;
				args_tail->next = parse_expr_withdc (awk, &eloc);
				if (args_tail->next == HAWK_NULL) goto oops;

				tail_prev = args_tail;
				args_tail = args_tail->next;

				if (gm_in_parens == 1 && awk->ptok.type == TOK_RPAREN &&
				    awk->parse.lparen_last_closed == group_opening_lparen_seq) 
				{
					/* confirm that the last group seen so far
					 * is parenthesized */
					gm_in_parens = 2;
				}
			}
		}

		/* print 1 > 2 would print 1 to the file named 2. 
		 * print (1 > 2) would print (1 > 2) on the console 
		 * 
		 * think of all these... there are many more possible combinations.
		 *
		 * print ((10,20,30) in a) > "x"; 
		 * print ((10,20,30) in a)
		 * print ((10,20,30) in a) > ("x"); 
		 * print ((10,20,30) in a) > (("x")); 
		 * function abc() { return "abc"; } BEGIN { print (1 > abc()); }
		 * function abc() { return "abc"; } BEGIN { print 1 > abc(); }
		 * print 1, 2, 3 > 4;
		 * print (1, 2, 3) > 4;
		 * print ((1, 2, 3) > 4);
		 * print 1, 2, 3 > 4 + 5;
		 * print 1, 2, (3 > 4) > 5;
		 * print 1, 2, (3 > 4) > 5 + 6;
		 */
		if (in_parens == 1 && awk->ptok.type == TOK_RPAREN && 
		    awk->parse.lparen_last_closed == opening_lparen_seq) 
		{
			in_parens = 2; /* confirmed */
		}

		if (in_parens != 2 && gm_in_parens != 2 && args_tail->type == HAWK_NDE_EXP_BIN)
		{
			int i;
			hawk_nde_exp_t* ep = (hawk_nde_exp_t*)args_tail;
			static struct 
			{
				int opc;
				int out;
				int opt;
			} tab[] =
			{
				{ 
					HAWK_BINOP_GT,     
					HAWK_OUT_FILE,      
					0
				},
				{ 
					HAWK_BINOP_RS, 
					HAWK_OUT_APFILE,
					0
				},
				{
					HAWK_BINOP_BOR,
					HAWK_OUT_PIPE,
					0
				},
				{
					HAWK_BINOP_LOR,
					HAWK_OUT_RWPIPE,
					HAWK_RWPIPE
				}
			};

			for (i = 0; i < HAWK_COUNTOF(tab); i++)
			{
				if (ep->opcode == tab[i].opc)
				{
					hawk_nde_t* tmp;

					if (tab[i].opt && !(awk->opt.trait & tab[i].opt)) break;

					tmp = args_tail;

					if (tail_prev != HAWK_NULL) 
						tail_prev->next = ep->left;
					else args = ep->left;

					out = ep->right;
					out_type = tab[i].out;

					hawk_freemem (awk, tmp);
					break;
				}
			}
		}
	}

	if (!out)
	{
		out_type = MATCH(awk,TOK_GT)?       HAWK_OUT_FILE:
		           MATCH(awk,TOK_RS)?       HAWK_OUT_APFILE:
		           MATCH(awk,TOK_BOR)?      HAWK_OUT_PIPE:
		           ((awk->opt.trait & HAWK_RWPIPE) &&
		            MATCH(awk,TOK_LOR))?    HAWK_OUT_RWPIPE:
		                                    HAWK_OUT_CONSOLE;

		if (out_type != HAWK_OUT_CONSOLE)
		{
			if (get_token(awk) <= -1) goto oops;

			eloc = awk->tok.loc;
			out = parse_expr_withdc (awk, &eloc);
			if (out == HAWK_NULL) goto oops;
		}
	}

	if (type == HAWK_NDE_PRINTF && !args)
	{
		SETERR_LOC (awk, HAWK_ENOARG, xloc);
		goto oops;
	}

	nde = (hawk_nde_print_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde->type = type;
	nde->loc = *xloc;
	nde->args = args;
	nde->out_type = out_type;
	nde->out = out;

	return (hawk_nde_t*)nde;

oops:
	if (args) hawk_clrpt (awk, args);
	if (out) hawk_clrpt (awk, out);
	return HAWK_NULL;
}

static hawk_nde_t* parse_statement_nb (
	hawk_t* awk, const hawk_loc_t* xloc)
{
	/* parse a non-block statement */
	hawk_nde_t* nde;

	/* keywords that don't require any terminating semicolon */
	if (MATCH(awk,TOK_IF)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		return parse_if (awk, xloc);
	}
	else if (MATCH(awk,TOK_WHILE)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		
		awk->parse.depth.loop++;
		nde = parse_while (awk, xloc);
		awk->parse.depth.loop--;

		return nde;
	}
	else if (MATCH(awk,TOK_FOR)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;

		awk->parse.depth.loop++;
		nde = parse_for (awk, xloc);
		awk->parse.depth.loop--;

		return nde;
	}

	/* keywords that require a terminating semicolon */
	if (MATCH(awk,TOK_DO)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;

		awk->parse.depth.loop++;
		nde = parse_dowhile (awk, xloc);
		awk->parse.depth.loop--;

		return nde;
	}
	else if (MATCH(awk,TOK_BREAK)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_break (awk, xloc);
	}
	else if (MATCH(awk,TOK_CONTINUE)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_continue (awk, xloc);
	}
	else if (MATCH(awk,TOK_RETURN)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_return (awk, xloc);
	}
	else if (MATCH(awk,TOK_EXIT) || MATCH(awk,TOK_XABORT)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_exit (awk, xloc);
	}
	else if (MATCH(awk,TOK_NEXT)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_next (awk, xloc);
	}
	else if (MATCH(awk,TOK_NEXTFILE)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_nextfile (awk, xloc, 0);
	}
	else if (MATCH(awk,TOK_NEXTOFILE))
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_nextfile (awk, xloc, 1);
	}
	else if (MATCH(awk,TOK_DELETE) || MATCH(awk,TOK_XRESET)) 
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
		nde = parse_delete (awk, xloc);
	}
	else if (!(awk->opt.trait & HAWK_TOLERANT))
	{
		/* in the non-tolerant mode, we treat print and printf
		 * as a separate statement */
		if (MATCH(awk,TOK_PRINT) || MATCH(awk,TOK_PRINTF))
		{
			if (get_token(awk) <= -1) return HAWK_NULL;
			nde = parse_print (awk, xloc);
		}
		else nde = parse_expr_withdc (awk, xloc);
	}
	else 
	{
		nde = parse_expr_withdc (awk, xloc);
	}

	if (nde == HAWK_NULL) return HAWK_NULL;

	if (MATCH_TERMINATOR_NORMAL(awk))
	{
		/* check if a statement ends with a semicolon or <NL> */
		if (get_token(awk) <= -1)
		{
			if (nde != HAWK_NULL) hawk_clrpt (awk, nde);
			return HAWK_NULL;
		}
	}
	else if (MATCH_TERMINATOR_RBRACE(awk))
	{
		/* do not skip the right brace as a statement terminator. 
		 * is there anything to do here? */
	}
	else
	{
		if (nde != HAWK_NULL) hawk_clrpt (awk, nde);
		SETERR_LOC (awk, HAWK_ESTMEND, &awk->ptok.loc);
		return HAWK_NULL;
	}

	return nde;
}

static hawk_nde_t* parse_statement (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde;

	/* skip new lines before a statement */
	while (MATCH(awk,TOK_NEWLINE))
	{
		if (get_token(awk) <= -1) return HAWK_NULL;
	}

	if (MATCH(awk,TOK_SEMICOLON)) 
	{
		/* null statement */
		nde = (hawk_nde_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
		if (!nde) 
		{
			ADJERR_LOC (awk, xloc);
			return HAWK_NULL;
		}

		nde->type = HAWK_NDE_NULL;
		nde->loc = *xloc;
		nde->next = HAWK_NULL;

		if (get_token(awk) <= -1) 
		{
			hawk_freemem (awk, nde);
			return HAWK_NULL;
		}
	}
	else if (MATCH(awk,TOK_LBRACE)) 
	{
		/* a block statemnt { ... } */
		hawk_loc_t tloc;

		tloc = awk->ptok.loc;
		if (get_token(awk) <= -1) return HAWK_NULL; 
		nde = parse_block_dc (awk, &tloc, 0);
	}
	else 
	{
		/* the statement id held in awk->parse.id.stmt denotes
		 * the token id of the statement currently being parsed.
		 * the current statement id is saved here because the 
		 * statement id can be changed in parse_statement_nb.
		 * it will, in turn, call parse_statement which will
		 * eventually change the statement id. */
		int old_id;
		hawk_loc_t tloc;

		old_id = awk->parse.id.stmt;
		tloc = awk->tok.loc;

		/* set the current statement id */
		awk->parse.id.stmt = awk->tok.type;

		/* proceed parsing the statement */
		nde = parse_statement_nb (awk, &tloc);

		/* restore the statement id saved previously */
		awk->parse.id.stmt = old_id;
	}

	return nde;
}

static int assign_to_opcode (hawk_t* awk)
{
	/* synchronize it with hawk_assop_type_t in run.h */
	static int assop[] =
	{
		HAWK_ASSOP_NONE,
		HAWK_ASSOP_PLUS,
		HAWK_ASSOP_MINUS,
		HAWK_ASSOP_MUL,
		HAWK_ASSOP_DIV,
		HAWK_ASSOP_IDIV,
		HAWK_ASSOP_MOD,
		HAWK_ASSOP_EXP,
		HAWK_ASSOP_CONCAT,
		HAWK_ASSOP_RS,
		HAWK_ASSOP_LS,
		HAWK_ASSOP_BAND,
		HAWK_ASSOP_BXOR,
		HAWK_ASSOP_BOR
	};

	if (awk->tok.type >= TOK_ASSN &&
	    awk->tok.type <= TOK_BOR_ASSN)
	{
		return assop[awk->tok.type - TOK_ASSN];
	}

	return -1;
}

static hawk_nde_t* parse_expr_basic (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde, * n1, * n2;
	
	nde = parse_logical_or (awk, xloc);
	if (nde == HAWK_NULL) return HAWK_NULL;

	if (MATCH(awk,TOK_QUEST))
	if (MATCH(awk,TOK_QUEST))
	{ 
		hawk_loc_t eloc;
		hawk_nde_cnd_t* cnd;

		if (get_token(awk) <= -1) 
		{
			hawk_clrpt (awk, nde);
			return HAWK_NULL;
		}

		eloc = awk->tok.loc;
		n1 = parse_expr_withdc (awk, &eloc);
		if (n1 == HAWK_NULL) 
		{
			hawk_clrpt (awk, nde);
			return HAWK_NULL;
		}

		if (!MATCH(awk,TOK_COLON)) 
		{
			hawk_clrpt (awk, nde);
			hawk_clrpt (awk, n1);
			SETERR_TOK (awk, HAWK_ECOLON);
			return HAWK_NULL;
		}
		if (get_token(awk) <= -1) 
		{
			hawk_clrpt (awk, nde);
			hawk_clrpt (awk, n1);
			return HAWK_NULL;
		}

		eloc = awk->tok.loc;
		n2 = parse_expr_withdc (awk, &eloc);
		if (n2 == HAWK_NULL)
		{
			hawk_clrpt (awk, nde);
			hawk_clrpt (awk, n1);
			return HAWK_NULL;
		}

		cnd = (hawk_nde_cnd_t*)hawk_callocmem(awk, HAWK_SIZEOF(*cnd));
		if (cnd == HAWK_NULL)
		{
			hawk_clrpt (awk, nde);
			hawk_clrpt (awk, n1);
			hawk_clrpt (awk, n2);
			ADJERR_LOC (awk, xloc);
			return HAWK_NULL;
		}

		cnd->type = HAWK_NDE_CND;
		cnd->loc = *xloc;
		cnd->next = HAWK_NULL;
		cnd->test = nde;
		cnd->left = n1;
		cnd->right = n2;

		nde = (hawk_nde_t*)cnd;
	}

	return nde;
}

static hawk_nde_t* parse_expr (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* x, * y;
	hawk_nde_ass_t* nde;
	int opcode;

	x = parse_expr_basic (awk, xloc);
	if (x == HAWK_NULL) return HAWK_NULL;

	opcode = assign_to_opcode (awk);
	if (opcode <= -1) 
	{
		/* no assignment operator found. */
		return x;
	}

	HAWK_ASSERT (awk, x->next == HAWK_NULL);
	if (!is_var(x) && x->type != HAWK_NDE_POS) 
	{
		hawk_clrpt (awk, x);
		SETERR_LOC (awk, HAWK_EASSIGN, xloc);
		return HAWK_NULL;
	}

	if (get_token(awk) <= -1) 
	{
		hawk_clrpt (awk, x);
		return HAWK_NULL;
	}

	{
		hawk_loc_t eloc;
		eloc = awk->tok.loc;
		y = parse_expr_withdc (awk, &eloc);
	}
	if (y == HAWK_NULL) 
	{
		hawk_clrpt (awk, x);
		return HAWK_NULL;
	}

	nde = (hawk_nde_ass_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		hawk_clrpt (awk, x);
		hawk_clrpt (awk, y);
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_ASS;
	nde->loc = *xloc;
	nde->next = HAWK_NULL;
	nde->opcode = opcode;
	nde->left = x;
	nde->right = y;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_expr_withdc (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde;

	/* perform depth check before parsing expression */

	if (awk->opt.depth.s.expr_parse > 0 &&
	    awk->parse.depth.expr >= awk->opt.depth.s.expr_parse)
	{
		SETERR_LOC (awk, HAWK_EEXPRNST, xloc);
		return HAWK_NULL;
	}

	awk->parse.depth.expr++;
	nde = parse_expr(awk, xloc);
	awk->parse.depth.expr--;

	return nde;
}

#define INT_BINOP_INT(x,op,y) \
	(((hawk_nde_int_t*)x)->val op ((hawk_nde_int_t*)y)->val)

#define INT_BINOP_FLT(x,op,y) \
	(((hawk_nde_int_t*)x)->val op ((hawk_nde_flt_t*)y)->val)

#define FLT_BINOP_INT(x,op,y) \
	(((hawk_nde_flt_t*)x)->val op ((hawk_nde_int_t*)y)->val)

#define FLT_BINOP_FLT(x,op,y) \
	(((hawk_nde_flt_t*)x)->val op ((hawk_nde_flt_t*)y)->val)

union folded_t
{
	hawk_int_t l;
	hawk_flt_t r;
};
typedef union folded_t folded_t;

static int fold_constants_for_binop (
	hawk_t* awk, hawk_nde_t* left, hawk_nde_t* right,
	int opcode, folded_t* folded)
{
	int fold = -1;

	/* TODO: can i shorten various comparisons below? 
 	 *       i hate to repeat similar code just for type difference */

	if (left->type == HAWK_NDE_INT &&
	    right->type == HAWK_NDE_INT)
	{
		fold = HAWK_NDE_INT;
		switch (opcode)
		{
			case HAWK_BINOP_PLUS:
				folded->l = INT_BINOP_INT(left,+,right);
				break;

			case HAWK_BINOP_MINUS:
				folded->l = INT_BINOP_INT(left,-,right);
				break;

			case HAWK_BINOP_MUL:
				folded->l = INT_BINOP_INT(left,*,right);
				break;

			case HAWK_BINOP_DIV:
				if (((hawk_nde_int_t*)right)->val == 0)
				{
					hawk_seterrnum (awk, HAWK_EDIVBY0, HAWK_NULL);
					fold = -2; /* error */
				}
				else if (INT_BINOP_INT(left,%,right))
				{
					folded->r = (hawk_flt_t)((hawk_nde_int_t*)left)->val / 
					            (hawk_flt_t)((hawk_nde_int_t*)right)->val;
					fold = HAWK_NDE_FLT;
				}
				else
				{
					folded->l = INT_BINOP_INT(left,/,right);
				}
				break;

			case HAWK_BINOP_IDIV:
				if (((hawk_nde_int_t*)right)->val == 0)
				{
					hawk_seterrnum (awk, HAWK_EDIVBY0, HAWK_NULL);
					fold = -2; /* error */
				}
				else
				{
					folded->l = INT_BINOP_INT(left,/,right);
				}
				break;

			case HAWK_BINOP_MOD:
				folded->l = INT_BINOP_INT(left,%,right);
				break;

			default:
				fold = -1; /* no folding */
				break;
		}
	}
	else if (left->type == HAWK_NDE_FLT &&
	         right->type == HAWK_NDE_FLT)
	{
		fold = HAWK_NDE_FLT;
		switch (opcode)
		{
			case HAWK_BINOP_PLUS:
				folded->r = FLT_BINOP_FLT(left,+,right);
				break;

			case HAWK_BINOP_MINUS:
				folded->r = FLT_BINOP_FLT(left,-,right);
				break;

			case HAWK_BINOP_MUL:
				folded->r = FLT_BINOP_FLT(left,*,right);
				break;

			case HAWK_BINOP_DIV:
				folded->r = FLT_BINOP_FLT(left,/,right);
				break;

			case HAWK_BINOP_IDIV:
				folded->l = (hawk_int_t)FLT_BINOP_FLT(left,/,right);
				fold = HAWK_NDE_INT;
				break;

			case HAWK_BINOP_MOD:
				folded->r = awk->prm.math.mod (
					awk, 
					((hawk_nde_flt_t*)left)->val, 
					((hawk_nde_flt_t*)right)->val
				);
				break;

			default:
				fold = -1;
				break;
		}
	}
	else if (left->type == HAWK_NDE_INT &&
	         right->type == HAWK_NDE_FLT)
	{
		fold = HAWK_NDE_FLT;
		switch (opcode)
		{
			case HAWK_BINOP_PLUS:
				folded->r = INT_BINOP_FLT(left,+,right);
				break;

			case HAWK_BINOP_MINUS:
				folded->r = INT_BINOP_FLT(left,-,right);
				break;

			case HAWK_BINOP_MUL:
				folded->r = INT_BINOP_FLT(left,*,right);
				break;

			case HAWK_BINOP_DIV:
				folded->r = INT_BINOP_FLT(left,/,right);
				break;

			case HAWK_BINOP_IDIV:
				folded->l = (hawk_int_t)
					((hawk_flt_t)((hawk_nde_int_t*)left)->val / 
					 ((hawk_nde_flt_t*)right)->val);
				fold = HAWK_NDE_INT;
				break;

			case HAWK_BINOP_MOD:
				folded->r = awk->prm.math.mod (
					awk, 
					(hawk_flt_t)((hawk_nde_int_t*)left)->val, 
					((hawk_nde_flt_t*)right)->val
				);
				break;

			default:
				fold = -1;
				break;
		}
	}
	else if (left->type == HAWK_NDE_FLT &&
	         right->type == HAWK_NDE_INT)
	{
		fold = HAWK_NDE_FLT;
		switch (opcode)
		{
			case HAWK_BINOP_PLUS:
				folded->r = FLT_BINOP_INT(left,+,right);
				break;

			case HAWK_BINOP_MINUS:
				folded->r = FLT_BINOP_INT(left,-,right);
				break;

			case HAWK_BINOP_MUL:
				folded->r = FLT_BINOP_INT(left,*,right);
				break;

			case HAWK_BINOP_DIV:
				folded->r = FLT_BINOP_INT(left,/,right);
				break;

			case HAWK_BINOP_IDIV:
				folded->l = (hawk_int_t)
					(((hawk_nde_int_t*)left)->val / 
					 (hawk_flt_t)((hawk_nde_int_t*)right)->val);
				fold = HAWK_NDE_INT;
				break;

			case HAWK_BINOP_MOD:
				folded->r = awk->prm.math.mod (
					awk, 
					((hawk_nde_flt_t*)left)->val, 
					(hawk_flt_t)((hawk_nde_int_t*)right)->val
				);
				break;

			default:
				fold = -1;
				break;
		}
	}

	return fold;
}

static hawk_nde_t* new_exp_bin_node (
	hawk_t* awk, const hawk_loc_t* loc,
	int opcode, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_nde_exp_t* tmp;

	tmp = (hawk_nde_exp_t*) hawk_callocmem (awk, HAWK_SIZEOF(*tmp));
	if (tmp)
	{
		tmp->type = HAWK_NDE_EXP_BIN;
		tmp->loc = *loc;
		tmp->opcode = opcode; 
		tmp->left = left;
		tmp->right = right;
	}
	else ADJERR_LOC (awk, loc);

	return (hawk_nde_t*)tmp;
}

static hawk_nde_t* new_int_node (
	hawk_t* awk, hawk_int_t lv, const hawk_loc_t* loc)
{
	hawk_nde_int_t* tmp;

	tmp = (hawk_nde_int_t*) hawk_callocmem (awk, HAWK_SIZEOF(*tmp));
	if (tmp)
	{
		tmp->type = HAWK_NDE_INT;
		tmp->loc = *loc;
		tmp->val = lv;
	}
	else ADJERR_LOC (awk, loc);

	return (hawk_nde_t*)tmp;
}

static hawk_nde_t* new_flt_node (
	hawk_t* awk, hawk_flt_t rv, const hawk_loc_t* loc)
{
	hawk_nde_flt_t* tmp;

	tmp = (hawk_nde_flt_t*) hawk_callocmem (awk, HAWK_SIZEOF(*tmp));
	if (tmp)
	{
		tmp->type = HAWK_NDE_FLT;
		tmp->loc = *loc;
		tmp->val = rv;
	}
	else ADJERR_LOC (awk, loc);

	return (hawk_nde_t*)tmp;
}

static HAWK_INLINE void update_int_node (
	hawk_t* awk, hawk_nde_int_t* node, hawk_int_t lv)
{
	node->val = lv;
	if (node->str)
	{
		hawk_freemem (awk, node->str);
		node->str = HAWK_NULL;
		node->len = 0;
	}
}

static HAWK_INLINE void update_flt_node (
	hawk_t* awk, hawk_nde_flt_t* node, hawk_flt_t rv)
{
	node->val = rv;
	if (node->str)
	{
		hawk_freemem (awk, node->str);
		node->str = HAWK_NULL;
		node->len = 0;
	}
}

static hawk_nde_t* parse_binary (
	hawk_t* awk, const hawk_loc_t* xloc, 
	int skipnl, const binmap_t* binmap,
	hawk_nde_t*(*next_level_func)(hawk_t*,const hawk_loc_t*))
{
	hawk_nde_t* left = HAWK_NULL; 
	hawk_nde_t* right = HAWK_NULL;
	hawk_loc_t rloc;

	left = next_level_func (awk, xloc);
	if (left == HAWK_NULL) goto oops;

	do
	{
		const binmap_t* p = binmap;
		int matched = 0;
		int opcode, fold;
		folded_t folded;

		while (p->token != TOK_EOF)
		{
			if (MATCH(awk,p->token)) 
			{
				opcode = p->binop;
				matched = 1;
				break;
			}
			p++;
		}
		if (!matched) break;

		do 
		{
			if (get_token(awk) <= -1) goto oops;
		}
		while (skipnl && MATCH(awk,TOK_NEWLINE));

		rloc = awk->tok.loc;
		right = next_level_func (awk, &rloc);
		if (right == HAWK_NULL) goto oops;

		fold = fold_constants_for_binop (awk, left, right, opcode, &folded);
		switch (fold)
		{
			case HAWK_NDE_INT:
				if (fold == left->type)
				{
					hawk_clrpt (awk, right); 
					right = HAWK_NULL;
					update_int_node (awk, (hawk_nde_int_t*)left, folded.l);
				}
				else if (fold == right->type)
				{
					hawk_clrpt (awk, left);
					update_int_node (awk, (hawk_nde_int_t*)right, folded.l);
					left = right;
					right = HAWK_NULL;
				}
				else 
				{
					hawk_clrpt (awk, right); right = HAWK_NULL;
					hawk_clrpt (awk, left); left = HAWK_NULL;

					left = new_int_node (awk, folded.l, xloc);
					if (left == HAWK_NULL) goto oops;
				}

				break;

			case HAWK_NDE_FLT:
				if (fold == left->type)
				{
					hawk_clrpt (awk, right);
					right = HAWK_NULL;
					update_flt_node (awk, (hawk_nde_flt_t*)left, folded.r);
				}
				else if (fold == right->type)
				{
					hawk_clrpt (awk, left); 
					update_flt_node (awk, (hawk_nde_flt_t*)right, folded.r);
					left = right; 
					right = HAWK_NULL;
				}
				else 
				{
					hawk_clrpt (awk, right); right = HAWK_NULL;
					hawk_clrpt (awk, left); left = HAWK_NULL;

					left = new_flt_node (awk, folded.r, xloc);
					if (left == HAWK_NULL) goto oops;
				}

				break;

			case -2:
				goto oops;

			default:
			{
				hawk_nde_t* tmp;

				tmp = new_exp_bin_node (awk, xloc, opcode, left, right);
				if (tmp == HAWK_NULL) goto oops;
				left = tmp; right = HAWK_NULL;
				break;
			}
		}
	}
	while (1);

	return left;

oops:
	if (right) hawk_clrpt (awk, right);
	if (left) hawk_clrpt (awk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_logical_or (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_LOR, HAWK_BINOP_LOR },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 1, map, parse_logical_and);
}

static hawk_nde_t* parse_logical_and (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_LAND, HAWK_BINOP_LAND },
		{ TOK_EOF,  0 }
	};

	return parse_binary (awk, xloc, 1, map, parse_in);
}

static hawk_nde_t* parse_in (hawk_t* awk, const hawk_loc_t* xloc)
{
	/* 
	static binmap_t map[] =
	{
		{ TOK_IN, HAWK_BINOP_IN },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_regex_match);
	*/

	hawk_nde_t* left = HAWK_NULL; 
	hawk_nde_t* right = HAWK_NULL;
	hawk_loc_t rloc;

	left = parse_regex_match (awk, xloc);
	if (left == HAWK_NULL) goto oops;

	do 
	{
		hawk_nde_t* tmp;

		if (!MATCH(awk,TOK_IN)) break;

		if (get_token(awk) <= -1) goto oops;

		rloc = awk->tok.loc;
		right = parse_regex_match (awk, &rloc);
		if (right == HAWK_NULL)  goto oops;

		if (!is_plain_var(right))
		{
			SETERR_LOC (awk, HAWK_ENOTVAR, &rloc);
			goto oops;
		}

		tmp = new_exp_bin_node (
			awk, xloc, HAWK_BINOP_IN, left, right);
		if (left == HAWK_NULL) goto oops;

		left = tmp;
		right = HAWK_NULL;
	}
	while (1);

	return left;

oops:
	if (right) hawk_clrpt (awk, right);
	if (left) hawk_clrpt (awk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_regex_match (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_MA,  HAWK_BINOP_MA },
		{ TOK_NM,  HAWK_BINOP_NM },
		{ TOK_EOF, 0 },
	};

	return parse_binary (awk, xloc, 0, map, parse_bitwise_or);
}

static hawk_nde_t* parse_bitwise_or (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_BOR, HAWK_BINOP_BOR },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_bitwise_xor);
}

static hawk_nde_t* parse_bitwise_xor (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_BXOR, HAWK_BINOP_BXOR },
		{ TOK_EOF,  0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_bitwise_and);
}

static hawk_nde_t* parse_bitwise_and (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_BAND, HAWK_BINOP_BAND },
		{ TOK_EOF,  0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_equality);
}

static hawk_nde_t* parse_equality (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_TEQ, HAWK_BINOP_TEQ },
		{ TOK_TNE, HAWK_BINOP_TNE },
		{ TOK_EQ, HAWK_BINOP_EQ },
		{ TOK_NE, HAWK_BINOP_NE },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_relational);
}

static hawk_nde_t* parse_relational (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_GT, HAWK_BINOP_GT },
		{ TOK_GE, HAWK_BINOP_GE },
		{ TOK_LT, HAWK_BINOP_LT },
		{ TOK_LE, HAWK_BINOP_LE },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_shift);
}

static hawk_nde_t* parse_shift (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_LS, HAWK_BINOP_LS },
		{ TOK_RS, HAWK_BINOP_RS },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_concat);
}

static hawk_nde_t* parse_concat (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* left = HAWK_NULL; 
	hawk_nde_t* right = HAWK_NULL;
	hawk_loc_t rloc;

	left = parse_additive (awk, xloc);
	if (left == HAWK_NULL) goto oops;

	do
	{
		hawk_nde_t* tmp;

		if (MATCH(awk,TOK_CONCAT))
		{
			if (get_token(awk) <= -1) goto oops;
		}
		else if (awk->opt.trait & HAWK_BLANKCONCAT)
		{
			if (MATCH(awk,TOK_LPAREN) || MATCH(awk,TOK_DOLLAR) ||
			   /* unary operators */
			   MATCH(awk,TOK_PLUS) || MATCH(awk,TOK_MINUS) ||
			   MATCH(awk,TOK_LNOT) || MATCH(awk,TOK_BNOT) || 
			   /* increment operators */
			   MATCH(awk,TOK_PLUSPLUS) || MATCH(awk,TOK_MINUSMINUS) ||
			   ((awk->opt.trait & HAWK_TOLERANT) && 
			    (awk->tok.type == TOK_PRINT || awk->tok.type == TOK_PRINTF)) ||
			   awk->tok.type >= TOK_GETLINE)
			{
				/* proceed to handle concatenation expression */
				/* nothing to to here. just fall through */
			}
			else break;
		}
		else break;

		rloc = awk->tok.loc;
		right = parse_additive (awk, &rloc);
		if (right == HAWK_NULL) goto oops;

		tmp = new_exp_bin_node (awk, xloc, HAWK_BINOP_CONCAT, left, right);
		if (tmp == HAWK_NULL) goto oops;
		left = tmp; right = HAWK_NULL;
	}
	while (1);

	return left;

oops:
	if (right) hawk_clrpt (awk, right);
	if (left) hawk_clrpt (awk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_additive (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_PLUS, HAWK_BINOP_PLUS },
		{ TOK_MINUS, HAWK_BINOP_MINUS },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_multiplicative);
}

static hawk_nde_t* parse_multiplicative (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_MUL,  HAWK_BINOP_MUL },
		{ TOK_DIV,  HAWK_BINOP_DIV },
		{ TOK_IDIV, HAWK_BINOP_IDIV },
		{ TOK_MOD,  HAWK_BINOP_MOD },
		/* { TOK_EXP, HAWK_BINOP_EXP }, */
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_unary);
}

static hawk_nde_t* parse_unary (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* left;
	hawk_loc_t uloc;
	int opcode;
	int fold;
	folded_t folded;

	opcode = (MATCH(awk,TOK_PLUS))?  HAWK_UNROP_PLUS:
	         (MATCH(awk,TOK_MINUS))? HAWK_UNROP_MINUS:
	         (MATCH(awk,TOK_LNOT))?  HAWK_UNROP_LNOT:
	         (MATCH(awk,TOK_BNOT))?  HAWK_UNROP_BNOT: -1;

	/*if (opcode <= -1) return parse_increment (awk);*/
	if (opcode <= -1) return parse_exponent (awk, xloc);

	if (awk->opt.depth.s.expr_parse > 0 &&
	    awk->parse.depth.expr >= awk->opt.depth.s.expr_parse)
	{
		SETERR_LOC (awk, HAWK_EEXPRNST, xloc);
		return HAWK_NULL;
	}

	if (get_token(awk) <= -1) return HAWK_NULL;

	awk->parse.depth.expr++;
	uloc = awk->tok.loc;
	left = parse_unary (awk, &uloc);
	awk->parse.depth.expr--;
	if (left == HAWK_NULL) return HAWK_NULL;

	fold = -1;
	if (left->type == HAWK_NDE_INT)
	{
		fold = HAWK_NDE_INT;
		switch (opcode)
		{
			case HAWK_UNROP_PLUS:
				folded.l = ((hawk_nde_int_t*)left)->val;
				break;

			case HAWK_UNROP_MINUS:
				folded.l = -((hawk_nde_int_t*)left)->val;
				break;

			case HAWK_UNROP_LNOT:
				folded.l = !((hawk_nde_int_t*)left)->val;
				break;

			case HAWK_UNROP_BNOT:
				folded.l = ~((hawk_nde_int_t*)left)->val;
				break;

			default:
				fold = -1;
				break;
		}
	}
	else if (left->type == HAWK_NDE_FLT)
	{
		fold = HAWK_NDE_FLT;
		switch (opcode)
		{
			case HAWK_UNROP_PLUS:
				folded.r = ((hawk_nde_flt_t*)left)->val;
				break;

			case HAWK_UNROP_MINUS:
				folded.r = -((hawk_nde_flt_t*)left)->val;
				break;

			case HAWK_UNROP_LNOT:
				folded.r = !((hawk_nde_flt_t*)left)->val;
				break;

			case HAWK_UNROP_BNOT:
				folded.l = ~((hawk_int_t)((hawk_nde_flt_t*)left)->val);
				fold = HAWK_NDE_INT;
				break;

			default:
				fold = -1;
				break;
		}
	}

	switch (fold)
	{
		case HAWK_NDE_INT:
			if (left->type == fold)
			{
				update_int_node (awk, (hawk_nde_int_t*)left, folded.l);
				return left;
			}
			else
			{
				HAWK_ASSERT (awk, left->type == HAWK_NDE_FLT);
				hawk_clrpt (awk, left);
				return new_int_node (awk, folded.l, xloc);
			}

		case HAWK_NDE_FLT:
			if (left->type == fold)
			{
				update_flt_node (awk, (hawk_nde_flt_t*)left, folded.r);
				return left;
			}
			else
			{
				HAWK_ASSERT (awk, left->type == HAWK_NDE_INT);
				hawk_clrpt (awk, left);
				return new_flt_node (awk, folded.r, xloc);
			}

		default:
		{
			hawk_nde_exp_t* nde; 

			nde = (hawk_nde_exp_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
			if (nde == HAWK_NULL)
			{
				hawk_clrpt (awk, left);
				SETERR_LOC (awk, HAWK_ENOMEM, xloc);
				return HAWK_NULL;
			}

			nde->type = HAWK_NDE_EXP_UNR;
			nde->loc = *xloc;
			nde->opcode = opcode;
			nde->left = left;
			/*nde->right = HAWK_NULL;*/
	
			return (hawk_nde_t*)nde;
		}
	}
}

static hawk_nde_t* parse_exponent (hawk_t* awk, const hawk_loc_t* xloc)
{
	static binmap_t map[] = 
	{
		{ TOK_EXP, HAWK_BINOP_EXP },
		{ TOK_EOF, 0 }
	};

	return parse_binary (awk, xloc, 0, map, parse_unary_exp);
}

static hawk_nde_t* parse_unary_exp (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_exp_t* nde; 
	hawk_nde_t* left;
	hawk_loc_t uloc;
	int opcode;

	opcode = (MATCH(awk,TOK_PLUS))?  HAWK_UNROP_PLUS:
	         (MATCH(awk,TOK_MINUS))? HAWK_UNROP_MINUS:
	         (MATCH(awk,TOK_LNOT))?  HAWK_UNROP_LNOT:
	         (MATCH(awk,TOK_BNOT))?  HAWK_UNROP_BNOT: -1;

	if (opcode <= -1) return parse_increment (awk, xloc);

	if (awk->opt.depth.s.expr_parse > 0 &&
	    awk->parse.depth.expr >= awk->opt.depth.s.expr_parse)
	{
		SETERR_LOC (awk, HAWK_EEXPRNST, xloc);
		return HAWK_NULL;
	}

	if (get_token(awk) <= -1) return HAWK_NULL;

	awk->parse.depth.expr++;
	uloc = awk->tok.loc;
	left = parse_unary (awk, &uloc);
	awk->parse.depth.expr--;
	if (left == HAWK_NULL) return HAWK_NULL;

	nde = (hawk_nde_exp_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		hawk_clrpt (awk, left);
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_EXP_UNR;
	nde->loc = *xloc;
	nde->opcode = opcode;
	nde->left = left;
	nde->right = HAWK_NULL;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_increment (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_exp_t* nde;
	hawk_nde_t* left;
	int type, opcode, opcode1, opcode2;
	hawk_loc_t ploc;

	/* check for prefix increment operator */
	opcode1 = MATCH(awk,TOK_PLUSPLUS)? HAWK_INCOP_PLUS:
	          MATCH(awk,TOK_MINUSMINUS)? HAWK_INCOP_MINUS: -1;

	if (opcode1 != -1)
	{
		/* there is a prefix increment operator */
		if (get_token(awk) <= -1) return HAWK_NULL;
	}

	ploc = awk->tok.loc;
	left = parse_primary (awk, &ploc);
	if (left == HAWK_NULL) return HAWK_NULL;

	/* check for postfix increment operator */
	opcode2 = MATCH(awk,TOK_PLUSPLUS)? HAWK_INCOP_PLUS:
	          MATCH(awk,TOK_MINUSMINUS)? HAWK_INCOP_MINUS: -1;

	if (!(awk->opt.trait & HAWK_BLANKCONCAT))
	{	
		if (opcode1 != -1 && opcode2 != -1)
		{
			/* both prefix and postfix increment operator. 
			 * not allowed */
			hawk_clrpt (awk, left);
			SETERR_LOC (awk, HAWK_EPREPST, xloc);
			return HAWK_NULL;
		}
	}

	if (opcode1 == -1 && opcode2 == -1)
	{
		/* no increment operators */
		return left;
	}

	if (opcode1 != -1) 
	{
		/* prefix increment operator.
		 * ignore a potential postfix operator */
		type = HAWK_NDE_EXP_INCPRE;
		opcode = opcode1;
	}
	else /*if (opcode2 != -1)*/
	{
		HAWK_ASSERT (awk, opcode2 != -1);

		/* postfix increment operator */
		type = HAWK_NDE_EXP_INCPST;
		opcode = opcode2;

		/* let's do it later 
		if (get_token(awk) <= -1) 
		{
			hawk_clrpt (awk, left);
			return HAWK_NULL;
		}
		*/
	}

	if (!is_var(left) && left->type != HAWK_NDE_POS)
	{
		if (type == HAWK_NDE_EXP_INCPST)
		{
			/* For an expression like 1 ++y,
			 * left is 1. so we leave ++ for y. */
			return left;
		}
		else
		{
			hawk_clrpt (awk, left);
			SETERR_LOC (awk, HAWK_EINCDECOPR, xloc);
			return HAWK_NULL;
		}
	}

	if (type == HAWK_NDE_EXP_INCPST)
	{
		/* consume the postfix operator */
		if (get_token(awk) <= -1) 
		{
			hawk_clrpt (awk, left);
			return HAWK_NULL;
		}
	}

	nde = (hawk_nde_exp_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		hawk_clrpt (awk, left);
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = type;
	nde->loc = *xloc;
	nde->opcode = opcode;
	nde->left = left;
	nde->right = HAWK_NULL;

	return (hawk_nde_t*)nde;
}

#define FNTYPE_UNKNOWN 0
#define FNTYPE_FNC 1
#define FNTYPE_FUN 2

static HAWK_INLINE int isfunname (hawk_t* awk, const hawk_oocs_t* name, hawk_fun_t** fun)
{
	hawk_htb_pair_t* pair;

	/* check if it is an awk function being processed currently */
	if (awk->tree.cur_fun.ptr)
	{
		if (hawk_comp_oochars(awk->tree.cur_fun.ptr, awk->tree.cur_fun.len, name->ptr, name->len, 0) == 0)
		{
			/* the current function begin parsed */
			return FNTYPE_FUN;
		}
	}

	/* check the funtion name in the function table */
	pair = hawk_htb_search(awk->tree.funs, name->ptr, name->len);
	if (pair)
	{
		/* one of the functions defined previously */
		if (fun) 
		{
			*fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
			HAWK_ASSERT (awk, *fun != HAWK_NULL);
		}
		return FNTYPE_FUN;
	}

	/* check if it is a function not resolved so far */
	if (hawk_htb_search(awk->parse.funs, name->ptr, name->len)) 
	{
		/* one of the function calls not resolved so far. */ 
		return FNTYPE_FUN;
	}

	return FNTYPE_UNKNOWN;
}

static HAWK_INLINE int isfnname (hawk_t* awk, const hawk_oocs_t* name)
{
	if (hawk_findfncwithoocs(awk, name) != HAWK_NULL) 
	{
		/* implicit function */
		return FNTYPE_FNC;
	}

	return isfunname(awk, name, HAWK_NULL);
}

static hawk_nde_t* parse_primary_int  (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_int_t* nde;

	/* create the node for the literal */
	nde = (hawk_nde_int_t*)new_int_node (
		awk, 
		hawk_strxtoint (awk, 
			HAWK_OOECS_PTR(awk->tok.name),
			HAWK_OOECS_LEN(awk->tok.name), 
			0, HAWK_NULL
		),
		xloc
	);
	if (nde == HAWK_NULL) return HAWK_NULL;

	HAWK_ASSERT (awk, 
		HAWK_OOECS_LEN(awk->tok.name) ==
		hawk_count_oocstr(HAWK_OOECS_PTR(awk->tok.name)));

	/* remember the literal in the original form */
	nde->len = HAWK_OOECS_LEN(awk->tok.name);
	nde->str = hawk_dupoocs(awk, HAWK_OOECS_OOCS(awk->tok.name));
	if (nde->str == HAWK_NULL || get_token(awk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT (awk, nde != HAWK_NULL);
	if (nde->str) hawk_freemem (awk, nde->str);
	hawk_freemem (awk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_flt  (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_flt_t* nde;

	/* create the node for the literal */
	nde = (hawk_nde_flt_t*) new_flt_node (
		awk, 
		hawk_strxtoflt (awk, 
			HAWK_OOECS_PTR(awk->tok.name), 
			HAWK_OOECS_LEN(awk->tok.name),
			HAWK_NULL
		),
		xloc
	);
	if (nde == HAWK_NULL) return HAWK_NULL;

	HAWK_ASSERT (awk, 
		HAWK_OOECS_LEN(awk->tok.name) ==
		hawk_count_oocstr(HAWK_OOECS_PTR(awk->tok.name)));

	/* remember the literal in the original form */
	nde->len = HAWK_OOECS_LEN(awk->tok.name);
	nde->str = hawk_dupoocs(awk, HAWK_OOECS_OOCS(awk->tok.name));
	if (nde->str == HAWK_NULL || get_token(awk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT (awk, nde != HAWK_NULL);
	if (nde->str) hawk_freemem (awk, nde->str);
	hawk_freemem (awk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_str  (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_str_t* nde;

	nde = (hawk_nde_str_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_STR;
	nde->loc = *xloc;
	nde->len = HAWK_OOECS_LEN(awk->tok.name);
	nde->ptr = hawk_dupoocs(awk, HAWK_OOECS_OOCS(awk->tok.name));
	if (nde->ptr == HAWK_NULL || get_token(awk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT (awk, nde != HAWK_NULL);
	if (nde->ptr) hawk_freemem (awk, nde->ptr);
	hawk_freemem (awk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_mbs (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_mbs_t* nde;

	nde = (hawk_nde_mbs_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_MBS;
	nde->loc = *xloc;

#if defined(HAWK_OOCH_IS_BCH)
	nde->len = HAWK_OOECS_LEN(awk->tok.name);
	nde->ptr = hawk_dupoocs(awk, HAWK_OOECS_OOCS(awk->tok.name));
	if (!nde->ptr) goto oops;
#else
	{
		/* the MBS token doesn't include a character greater than 0xFF in awk->tok.name 
		 * even though it is a unicode string. simply copy over without conversion */
		nde->len = HAWK_OOECS_LEN(awk->tok.name);
		nde->ptr = hawk_allocmem(awk, (nde->len + 1) * HAWK_SIZEOF(*nde->ptr));
		if (!nde->ptr)
		{
			hawk_seterror (awk, HAWK_ENOMEM, HAWK_NULL, xloc);
			goto oops;
		}

		hawk_copy_uchars_to_bchars (nde->ptr, HAWK_OOECS_PTR(awk->tok.name), nde->len);
		nde->ptr[nde->len] = '\0';
	}
#endif

	if (get_token(awk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT (awk, nde != HAWK_NULL);
	if (nde->ptr) hawk_freemem (awk, nde->ptr);
	hawk_freemem (awk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_rex (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_rex_t* nde;
	hawk_errnum_t errnum;

	/* the regular expression is tokenized here because 
	 * of the context-sensitivity of the slash symbol.
	 * if TOK_DIV is seen as a primary, it tries to compile
	 * it as a regular expression */
	hawk_ooecs_clear (awk->tok.name);

	if (MATCH(awk,TOK_DIV_ASSN) && 
	    hawk_ooecs_ccat (awk->tok.name, HAWK_T('=')) == (hawk_oow_t)-1)
	{
		SETERR_LOC (awk, HAWK_ENOMEM, xloc);
		return HAWK_NULL;
	}

	SET_TOKEN_TYPE (awk, &awk->tok, TOK_REX);
	if (get_rexstr (awk, &awk->tok) <= -1) return HAWK_NULL;

	HAWK_ASSERT (awk, MATCH(awk,TOK_REX));

	nde = (hawk_nde_rex_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_REX;
	nde->loc = *xloc;
	nde->str.len = HAWK_OOECS_LEN(awk->tok.name);
	nde->str.ptr = hawk_dupoocs(awk, HAWK_OOECS_OOCS(awk->tok.name));
	if (nde->str.ptr == HAWK_NULL) goto oops;

	if (hawk_buildrex (awk, HAWK_OOECS_PTR(awk->tok.name), HAWK_OOECS_LEN(awk->tok.name), &errnum, &nde->code[0], &nde->code[1]) <= -1)
	{
		SETERR_LOC (awk, errnum, xloc);
		goto oops;
	}

	if (get_token(awk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT (awk, nde != HAWK_NULL);
	if (nde->code[0]) hawk_freerex (awk, nde->code[0], nde->code[1]);
	if (nde->str.ptr) hawk_freemem (awk, nde->str.ptr);
	hawk_freemem (awk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_positional (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_pos_t* nde;
	hawk_loc_t ploc;

	nde = (hawk_nde_pos_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_POS;
	nde->loc = *xloc;

	if (get_token(awk) <= -1) return HAWK_NULL;

	ploc = awk->tok.loc;
	nde->val = parse_primary(awk, &ploc);
	if (!nde->val) goto oops;

	return (hawk_nde_t*)nde;

oops:
	if (nde) 
	{
		if (nde->val) hawk_clrpt (awk, nde->val);
		hawk_freemem (awk, nde);
	}
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_lparen (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde;
	hawk_nde_t* last;
	hawk_loc_t eloc;
	hawk_oow_t opening_lparen_seq;

	opening_lparen_seq = awk->parse.lparen_seq++;

	/* eat up the left parenthesis */
	if (get_token(awk) <= -1) return HAWK_NULL;

	/* parse the sub-expression inside the parentheses */
	eloc = awk->tok.loc;
	nde = parse_expr_withdc(awk, &eloc);
	if (!nde) return HAWK_NULL;

	/* parse subsequent expressions separated by a comma, if any */
	last = nde;
	HAWK_ASSERT (awk, last->next == HAWK_NULL);

	while (MATCH(awk,TOK_COMMA))
	{
		hawk_nde_t* tmp;

		do
		{
			if (get_token(awk) <= -1) goto oops;
		}
		while (MATCH(awk,TOK_NEWLINE));

		eloc = awk->tok.loc;
		tmp = parse_expr_withdc(awk, &eloc);
		if (!tmp) goto oops;

		HAWK_ASSERT (awk, tmp->next == HAWK_NULL);
		last->next = tmp;
		last = tmp;
	} 
	/* ----------------- */

	/* check for the closing parenthesis */
	if (!MATCH(awk,TOK_RPAREN)) 
	{
		SETERR_TOK (awk, HAWK_ERPAREN);
		goto oops;
	}

	/* remember the sequence number of the left parenthesis
	 * that' been just closed by the matching right parenthesis */
	awk->parse.lparen_last_closed = opening_lparen_seq;

	if (get_token(awk) <= -1) goto oops;

	/* check if it is a chained node */
	if (nde->next)
	{
		/* if so, it is an expression group */
		/* (expr1, expr2, expr2) */

		hawk_nde_grp_t* tmp;

		if ((awk->parse.id.stmt != TOK_PRINT && awk->parse.id.stmt != TOK_PRINTF) || awk->parse.depth.expr != 1)
		{
			if (!(awk->opt.trait & HAWK_TOLERANT) && !MATCH(awk,TOK_IN))
			{
				SETERR_TOK (awk, HAWK_EKWIN);
				goto oops;
			}
		}

		tmp = (hawk_nde_grp_t*)hawk_callocmem(awk, HAWK_SIZEOF(hawk_nde_grp_t));
		if (!tmp)
		{
			ADJERR_LOC (awk, xloc);
			goto oops;
		}

		tmp->type = HAWK_NDE_GRP;
		tmp->loc = *xloc;
		tmp->body = nde;

		nde = (hawk_nde_t*)tmp;
	}
	/* ----------------- */

	return nde;

oops:
	if (nde) hawk_clrpt (awk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_getline  (hawk_t* awk, const hawk_loc_t* xloc)
{
	/* parse the statement-level getline.
	 * getline after the pipe symbols(|,||) is parsed
	 * by parse_primary(). 
	 */

	hawk_nde_getline_t* nde;
	hawk_loc_t ploc;

	nde = (hawk_nde_getline_t*) hawk_callocmem (awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) goto oops;

	nde->type = HAWK_NDE_GETLINE;
	nde->loc = *xloc;
	nde->in_type = HAWK_IN_CONSOLE;

	if (get_token(awk) <= -1) goto oops;

	if (MATCH(awk,TOK_IDENT) || MATCH(awk,TOK_DOLLAR))
	{
		/* getline var 
		 * getline $XXX */

		if ((awk->opt.trait & HAWK_BLANKCONCAT) && MATCH(awk,TOK_IDENT))
		{
			/* i need to perform some precheck on if the identifier is
			 * really a variable */
			if (preget_token(awk) <= -1) goto oops;

			if (awk->ntok.type == TOK_DBLCOLON) goto novar;
			if (awk->ntok.type == TOK_LPAREN)
			{
				if (awk->ntok.loc.line == awk->tok.loc.line && 
				    awk->ntok.loc.colm == awk->tok.loc.colm + HAWK_OOECS_LEN(awk->tok.name))
				{
					/* it's in the form of a function call since
					 * there is no spaces between the identifier
					 * and the left parenthesis. */
					goto novar;
				}
			}

			if (isfnname(awk, HAWK_OOECS_OOCS(awk->tok.name)) != FNTYPE_UNKNOWN) goto novar;
		}

		ploc = awk->tok.loc;
		nde->var = parse_primary (awk, &ploc);
		if (nde->var == HAWK_NULL) goto oops;

		if (!is_var(nde->var) && nde->var->type != HAWK_NDE_POS)
		{
			/* this is 'getline' followed by an expression probably.
			 *    getline a() 
			 *    getline sys::WNOHANG
			 */
			SETERR_LOC (awk, HAWK_EBADARG, &ploc);
			goto oops;
		}
	}

novar:
	if (MATCH(awk, TOK_LT))
	{
		/* getline [var] < file */
		if (get_token(awk) <= -1) goto oops;

		ploc = awk->tok.loc;
		/* TODO: is this correct? */
		/*nde->in = parse_expr_withdc (awk, &ploc);*/
		nde->in = parse_primary (awk, &ploc);
		if (nde->in == HAWK_NULL) goto oops;

		nde->in_type = HAWK_IN_FILE;
	}

	return (hawk_nde_t*)nde;

oops:
	if (nde)
	{
		if (nde->in) hawk_clrpt (awk, nde->in);
		if (nde->var) hawk_clrpt (awk, nde->var);
		hawk_freemem (awk, nde);
	}
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_nopipe (hawk_t* awk, const hawk_loc_t* xloc)
{
	switch (awk->tok.type)
	{
		case TOK_IDENT:
			return parse_primary_ident(awk, xloc);

		case TOK_INT:
			return parse_primary_int(awk, xloc);

		case TOK_FLT:
			return parse_primary_flt(awk, xloc);

		case TOK_STR:
			return parse_primary_str(awk, xloc);

		case TOK_MBS:
			return parse_primary_mbs(awk, xloc);

		case TOK_DIV:
		case TOK_DIV_ASSN:
			return parse_primary_rex(awk, xloc);

		case TOK_DOLLAR:
			return parse_primary_positional(awk, xloc);

		case TOK_LPAREN:
			return parse_primary_lparen(awk, xloc);

		case TOK_GETLINE:
			return parse_primary_getline(awk, xloc);

		default:
			/* in the tolerant mode, we treat print and printf 
			 * as a function like getline */
			if ((awk->opt.trait & HAWK_TOLERANT) &&
			    (MATCH(awk,TOK_PRINT) || MATCH(awk,TOK_PRINTF)))
			{
				if (get_token(awk) <= -1) return HAWK_NULL;
				return parse_print (awk, xloc);
			}

			/* valid expression introducer is expected */
			if (MATCH(awk,TOK_NEWLINE))
			{
				SETERR_ARG_LOC (
					awk, HAWK_EEXPRNR, 
					HAWK_OOECS_PTR(awk->ptok.name), 
					HAWK_OOECS_LEN(awk->ptok.name),
					&awk->ptok.loc
				);
			}
			else 
			{
				SETERR_TOK (awk, HAWK_EEXPRNR);
			}
			return HAWK_NULL;
	}

}

static hawk_nde_t* parse_primary (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* left;
	hawk_nde_getline_t* nde;
	hawk_nde_t* var = HAWK_NULL;
	hawk_loc_t ploc;

	left = parse_primary_nopipe(awk, xloc);
	if (left == HAWK_NULL) goto oops;

	/* handle the piping part */
	do
	{
		int intype = -1;

		if (awk->opt.trait & HAWK_RIO)
		{
			if (MATCH(awk,TOK_BOR)) 
			{
				intype = HAWK_IN_PIPE;
			}
			else if (MATCH(awk,TOK_LOR) && (awk->opt.trait & HAWK_RWPIPE)) 
			{
				intype = HAWK_IN_RWPIPE;
			}
		}
		
		if (intype == -1) break;

		if (preget_token(awk) <= -1) goto oops;
		if (awk->ntok.type != TOK_GETLINE) break;

		/* consume ntok('getline') */
		get_token(awk); /* no error check needed as it's guaranteeded to succeed for preget_token() above */

		/* get the next token */
		if (get_token(awk) <= -1) goto oops;

		/* TODO: is this correct? */
		if (MATCH(awk,TOK_IDENT) || MATCH(awk,TOK_DOLLAR))
		{
			/* command | getline var 
			 * command || getline var */

			if ((awk->opt.trait & HAWK_BLANKCONCAT) && MATCH(awk,TOK_IDENT))
			{
				/* i need to perform some precheck on if the identifier is
				 * really a variable */
				if (preget_token(awk) <= -1) goto oops;

				if (awk->ntok.type == TOK_DBLCOLON) goto novar;
				if (awk->ntok.type == TOK_LPAREN)
				{
					if (awk->ntok.loc.line == awk->tok.loc.line && 
					    awk->ntok.loc.colm == awk->tok.loc.colm + HAWK_OOECS_LEN(awk->tok.name))
					{
						/* it's in the form of a function call since
						 * there is no spaces between the identifier
						 * and the left parenthesis. */
						goto novar;
					}
				}

				if (isfnname(awk, HAWK_OOECS_OOCS(awk->tok.name)) != FNTYPE_UNKNOWN) goto novar;
			}

			ploc = awk->tok.loc;
			var = parse_primary (awk, &ploc);
			if (var == HAWK_NULL) goto oops;

			if (!is_var(var) && var->type != HAWK_NDE_POS)
			{
				/* fucntion a() {}
				 * print ("ls -laF" | getline a()) */
				SETERR_LOC (awk, HAWK_EBADARG, &ploc);
				goto oops;
			}
		}

	novar:
		nde = (hawk_nde_getline_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
		if (nde == HAWK_NULL)
		{
			ADJERR_LOC (awk, xloc);
			goto oops;
		}

		nde->type = HAWK_NDE_GETLINE;
		nde->loc = *xloc;
		nde->var = var;
		nde->in_type = intype;
		nde->in = left;

		left = (hawk_nde_t*)nde;
		var = HAWK_NULL;
	}
	while (1);

	return left;

oops:
	if (var) hawk_clrpt (awk, var);
	hawk_clrpt (awk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_variable (hawk_t* awk, const hawk_loc_t* xloc, hawk_nde_type_t type, const hawk_oocs_t* name, hawk_oow_t idxa)
{
	hawk_nde_var_t* nde;
	int is_fcv = 0;

	if (MATCH(awk,TOK_LPAREN))
	{
	#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
/*
	if (MATCH(awk,TOK_LPAREN) && 
	    (!(awk->opt.trait & HAWK_BLANKCONCAT) || 
		(awk->tok.loc.line == xloc->line && 
		 awk->tok.loc.colm == xloc->colm + name->len)))
*/
		if (awk->tok.loc.line == xloc->line && awk->tok.loc.colm == xloc->colm + name->len)
		{
			is_fcv = 1;
		}
		else 
	#endif
		if (!(awk->opt.trait & HAWK_BLANKCONCAT))
		{
			/* if concatenation by blanks is not allowed, the explicit 
			 * concatenation operator(%%) must be used. so it is obvious 
			 * that it is a function call, which is illegal for a variable. 
			 * if implicit, "var_xxx (1)" may be concatenation of
			 * the value of var_xxx and 1.
			 */
			/* a variable is not a function */
			SETERR_ARG_LOC (awk, HAWK_EFUNNAM, name->ptr, name->len, xloc);
			return HAWK_NULL;
		}
	}

	nde = (hawk_nde_var_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = type;
	nde->loc = *xloc;
	/*nde->id.name.ptr = HAWK_NULL;*/
	nde->id.name.ptr = name->ptr;
	nde->id.name.len = name->len;
	nde->id.idxa = idxa;
	nde->idx = HAWK_NULL;

#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
	if (!is_fcv) return (hawk_nde_t*)nde;
	return parse_fncall(awk, (const hawk_oocs_t*)nde, HAWK_NULL, xloc, FNCALL_FLAG_VAR);
#else
	return (hawk_nde_t*)nde;
#endif
}

static int dup_ident_and_get_next (hawk_t* awk, const hawk_loc_t* xloc, hawk_oocs_t* name, int max)
{
	int nsegs = 0;

	HAWK_ASSERT (awk, MATCH(awk,TOK_IDENT));

	do 
	{
		name[nsegs].ptr = HAWK_OOECS_PTR(awk->tok.name);
		name[nsegs].len = HAWK_OOECS_LEN(awk->tok.name);

		/* duplicate the identifier */
		name[nsegs].ptr = hawk_dupoochars(awk, name[nsegs].ptr, name[nsegs].len);
		if (!name[nsegs].ptr)
		{
			ADJERR_LOC (awk, xloc);
			goto oops;
		}

		nsegs++;

		if (get_token(awk) <= -1) goto oops;

		if (!MATCH(awk,TOK_DBLCOLON)) break;

		if (get_token(awk) <= -1) goto oops;

		/* the identifier after ::
		 * allow reserved words as well since i view the whole name(mod::ident) 
		 * as one segment. however, i don't want the identifier part to begin
		 * with @. some extended keywords begin with @ like @include. 
		 * TOK_XGLOBAL to TOK_XRESET are excuded from the check for that reason. */
		if (!MATCH(awk, TOK_IDENT) && !(MATCH_RANGE(awk, TOK_BEGIN, TOK_GETLINE)))
		{
			SETERR_TOK (awk, HAWK_EIDENT);
			goto oops;
		}

		if (nsegs >= max)
		{
			SETERR_LOC (awk, HAWK_ESEGTM, xloc);
			goto oops;
		}
	}
	while (1);

	return nsegs;

oops:
	while (nsegs > 0) hawk_freemem (awk, name[--nsegs].ptr);
	return -1;
}

#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
static hawk_nde_t* parse_fun_as_value  (hawk_t* awk, const hawk_oocs_t* name, const hawk_loc_t* xloc, hawk_fun_t* funptr)
{
	hawk_nde_fun_t* nde;

	/* create the node for the literal */
	nde = (hawk_nde_fun_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_FUN;
	nde->loc = *xloc;
	nde->name.ptr = name->ptr;
	nde->name.len = name->len;
	nde->funptr = funptr;

	return (hawk_nde_t*)nde;
}
#endif

static hawk_nde_t* parse_primary_ident_noseg (hawk_t* awk, const hawk_loc_t* xloc, const hawk_oocs_t* name)
{
	hawk_fnc_t* fnc;
	hawk_oow_t idxa;
	hawk_nde_t* nde = HAWK_NULL;

	/* check if name is an intrinsic function name */
	fnc = hawk_findfncwithoocs(awk, name);
	if (fnc)
	{
		if (MATCH(awk,TOK_LPAREN) || fnc->dfl0)
		{
			if (fnc->spec.arg.min > fnc->spec.arg.max)
			{
				/* this intrinsic function is located in the specificed module.
				 * convert the function call to a module call. i do this to 
				 * exclude some instrinsic functions from the main engine. 
				 *  e.g) sin -> math::sin
				 *       cos -> math::cos 
				 */
				hawk_oocs_t segs[2];

				HAWK_ASSERT (awk, fnc->spec.arg.spec != HAWK_NULL);

				segs[0].ptr = (hawk_ooch_t*)fnc->spec.arg.spec;
				segs[0].len = hawk_count_oocstr(fnc->spec.arg.spec);
				segs[1] = *name;

				return parse_primary_ident_segs (awk, xloc, name, segs, 2);
			}

			/* fnc->dfl0 means that the function can be called without ().
			 * i.e. length */
			nde = parse_fncall(awk, name, fnc, xloc, ((!MATCH(awk,TOK_LPAREN) && fnc->dfl0)? FNCALL_FLAG_NOARG: 0));
		}
		else
		{
			/* an intrinsic function should be in the form of the function call */
			SETERR_TOK (awk, HAWK_ELPAREN);
		}
	}
	/* now we know that name is a normal identifier. */
	else if (MATCH(awk,TOK_LBRACK)) 
	{
		nde = parse_hashidx(awk, name, xloc);
	}
	else if ((idxa = hawk_arr_rsearch(awk->parse.lcls, HAWK_ARR_SIZE(awk->parse.lcls), name->ptr, name->len)) != HAWK_ARR_NIL)
	{
		/* local variable */
		nde = parse_variable(awk, xloc, HAWK_NDE_LCL, name, idxa);
	}
	else if ((idxa = hawk_arr_search(awk->parse.params, 0, name->ptr, name->len)) != HAWK_ARR_NIL)
	{
		/* parameter */
		nde = parse_variable(awk, xloc, HAWK_NDE_ARG, name, idxa);
	}
	else if ((idxa = get_global(awk, name)) != HAWK_ARR_NIL)
	{
		/* global variable */
		nde = parse_variable(awk, xloc, HAWK_NDE_GBL, name, idxa);
	}
	else
	{
		int fntype;
		hawk_fun_t* funptr = HAWK_NULL;

		fntype = isfunname(awk, name, &funptr);

		if (fntype)
		{
			HAWK_ASSERT (awk, fntype == FNTYPE_FUN);

			if (MATCH(awk,TOK_LPAREN))
			{
				/* must be a function name */
				HAWK_ASSERT (awk, hawk_htb_search(awk->parse.named, name->ptr, name->len) == HAWK_NULL);
				nde = parse_fncall(awk, name, HAWK_NULL, xloc, 0);
			}
			else
			{
				/* function name appeared without () */
			#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
				nde = parse_fun_as_value(awk, name, xloc, funptr);
			#else
				SETERR_ARG_LOC (awk, HAWK_EFUNRED, name->ptr, name->len, xloc);
			#endif
			}
		}
		/*else if (awk->opt.trait & HAWK_IMPLICIT) */
		else if (awk->parse.pragma.trait & HAWK_IMPLICIT)
		{
			/* if the name is followed by ( without spaces,
			 * it's considered a function call though the name 
			 * has not been seen/resolved.
			 * 
			 * it is a function call so long as it's followed
			 * by a left parenthesis if concatenation by blanks
			 * is not allowed.
			 */
			int is_fncall_var = 0;

			if (MATCH(awk,TOK_LPAREN) && 
			    (!(awk->opt.trait & HAWK_BLANKCONCAT) || 
			     (awk->tok.loc.line == xloc->line && 
			      awk->tok.loc.colm == xloc->colm + name->len)))
			{
				/* it is a function call to an undefined function yet */
				if (hawk_htb_search(awk->parse.named, name->ptr, name->len) != HAWK_NULL)
				{
					/* the function call conflicts with a named variable */
			#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
					is_fncall_var = 1;
					goto named_var;
			#else
					SETERR_ARG_LOC (awk, HAWK_EVARRED, name->ptr, name->len, xloc);
			#endif
				}
				else
				{
					nde = parse_fncall(awk, name, HAWK_NULL, xloc, 0);
				}
			}
			else
			{
				hawk_nde_var_t* tmp;

			#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
			named_var:
			#endif

				/* if there is a space between the name and the left parenthesis
				 * while the name is not resolved to anything, we treat the space
				 * as concatention by blanks. so we handle the name as a named 
				 * variable. */
				tmp = (hawk_nde_var_t*)hawk_callocmem(awk, HAWK_SIZEOF(*tmp));
				if (tmp == HAWK_NULL) ADJERR_LOC (awk, xloc);
				else
				{
					/* collect unique instances of a named variable 
					 * for reference */
					if (hawk_htb_upsert(awk->parse.named, name->ptr, name->len, HAWK_NULL, 0) == HAWK_NULL)
					{
						SETERR_LOC (awk, HAWK_ENOMEM, xloc);
						hawk_freemem (awk, tmp);
					}
					else
					{
						tmp->type = HAWK_NDE_NAMED;
						tmp->loc = *xloc;
						tmp->id.name.ptr = name->ptr;
						tmp->id.name.len = name->len;
						tmp->id.idxa = (hawk_oow_t)-1;
						tmp->idx = HAWK_NULL;
						nde = (hawk_nde_t*)tmp;

			#if defined(ENABLE_FEATURE_FUN_AS_VALUE)
						if (is_fncall_var) 
							nde = parse_fncall(awk, (const hawk_oocs_t*)nde, HAWK_NULL, xloc, FNCALL_FLAG_VAR);
			#endif
					}
				}
			}
		}
		else
		{
			if (MATCH(awk,TOK_LPAREN))
			{
				/* it is a function call as the name is followed 
				 * by ( with/without spaces and implicit variables are disabled. */
				nde = parse_fncall(awk, name, HAWK_NULL, xloc, 0);
			}
			else
			{
				/* undefined variable */
				SETERR_ARG_LOC (awk, HAWK_EUNDEF, name->ptr, name->len, xloc);
			}
		}
	}

	return nde;
}

static hawk_nde_t* parse_primary_ident_segs (hawk_t* awk, const hawk_loc_t* xloc, const hawk_oocs_t* full, const hawk_oocs_t segs[], int nsegs)
{
	/* parse xxx::yyy */

	hawk_nde_t* nde = HAWK_NULL;
	hawk_mod_t* mod;
	hawk_mod_sym_t sym;
	hawk_fnc_t fnc;

	CLRERR (awk);
	mod = query_module(awk, segs, nsegs, &sym);
	if (mod == HAWK_NULL)
	{
		if (ISNOERR(awk)) SETERR_LOC (awk, HAWK_ENOSUP, xloc);
		else ADJERR_LOC (awk, xloc);
	}
	else
	{
		switch (sym.type)
		{
			case HAWK_MOD_FNC:
				if ((awk->opt.trait & sym.u.fnc.trait) != sym.u.fnc.trait)
				{
					SETERR_ARG_LOC (awk, HAWK_EUNDEF, full->ptr, full->len, xloc);
					break;
				}

				if (MATCH(awk,TOK_LPAREN))
				{
					HAWK_MEMSET (&fnc, 0, HAWK_SIZEOF(fnc));
					fnc.name.ptr = full->ptr; 
					fnc.name.len = full->len;
					fnc.spec = sym.u.fnc;
					fnc.mod = mod;
					nde = parse_fncall (awk, full, &fnc, xloc, 0);
				}
				else
				{
					SETERR_TOK (awk, HAWK_ELPAREN);
				}
				break;

			case HAWK_MOD_INT:
				nde = new_int_node(awk, sym.u.in.val, xloc);
				/* i don't remember the symbol in the original form */
				break;

			case HAWK_MOD_FLT:
				nde = new_flt_node(awk, sym.u.flt.val, xloc);
				/* i don't remember the symbol in the original form */
				break;

			default:
				/* TODO: support MOD_VAR */
				SETERR_ARG_LOC (awk, HAWK_EUNDEF, full->ptr, full->len, xloc);
				break;
		}
	}

	return nde;
}

static hawk_nde_t* parse_primary_ident (hawk_t* awk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde = HAWK_NULL;
	hawk_oocs_t name[2]; /* TODO: support more than 2 segments??? */
	int nsegs;

	HAWK_ASSERT (awk, MATCH(awk,TOK_IDENT));

	nsegs = dup_ident_and_get_next(awk, xloc, name, HAWK_COUNTOF(name));
	if (nsegs <= -1) return HAWK_NULL;

	if (nsegs <= 1)
	{
		nde = parse_primary_ident_noseg(awk, xloc, &name[0]);
		if (!nde) hawk_freemem (awk, name[0].ptr);
	}
	else
	{
		hawk_oocs_t full; /* full name including :: */
		hawk_oow_t capa;
		int i;

		for (capa = 0, i = 0; i < nsegs; i++) capa += name[i].len + 2; /* +2 for :: */
		full.ptr = hawk_allocmem(awk, HAWK_SIZEOF(*full.ptr) * (capa + 1));
		if (full.ptr)
		{
			capa = hawk_copy_oochars_to_oocstr_unlimited(&full.ptr[0], name[0].ptr, name[0].len);
			for (i = 1; i < nsegs; i++) 
			{
				capa += hawk_copy_oocstr_unlimited(&full.ptr[capa], HAWK_T("::"));
				capa += hawk_copy_oochars_to_oocstr_unlimited(&full.ptr[capa], name[i].ptr, name[i].len);
			}
			full.ptr[capa] = HAWK_T('\0');
			full.len = capa;

			nde = parse_primary_ident_segs(awk, xloc, &full, name, nsegs);
			if (!nde || nde->type != HAWK_NDE_FNCALL_FNC) 
			{
				/* the FNC node takes the full name but other 
				 * nodes don't. so i need to free it. i know it's ugly. */
				hawk_freemem (awk, full.ptr);
			}
		}
		else
		{
			/* error number is set in hawk_allocmem */
			ADJERR_LOC (awk, xloc);
		}

		/* i don't need the name segments */
		while (nsegs > 0) hawk_freemem (awk, name[--nsegs].ptr);
	}

	return nde;
}

static hawk_nde_t* parse_hashidx (hawk_t* awk, const hawk_oocs_t* name, const hawk_loc_t* xloc)
{
	hawk_nde_t* idx, * tmp, * last;
	hawk_nde_var_t* nde;
	hawk_oow_t idxa;

	idx = HAWK_NULL;
	last = HAWK_NULL;

	do
	{
		if (get_token(awk) <= -1) 
		{
			if (idx != HAWK_NULL) hawk_clrpt (awk, idx);
			return HAWK_NULL;
		}

		{
			hawk_loc_t eloc;

			eloc = awk->tok.loc;
			tmp = parse_expr_withdc (awk, &eloc);
		}
		if (tmp == HAWK_NULL) 
		{
			if (idx != HAWK_NULL) hawk_clrpt (awk, idx);
			return HAWK_NULL;
		}

		if (idx == HAWK_NULL)
		{
			HAWK_ASSERT (awk, last == HAWK_NULL);
			idx = tmp; last = tmp;
		}
		else
		{
			last->next = tmp;
			last = tmp;
		}
	}
	while (MATCH(awk,TOK_COMMA));

	HAWK_ASSERT (awk, idx != HAWK_NULL);

	if (!MATCH(awk,TOK_RBRACK)) 
	{
		hawk_clrpt (awk, idx);
		SETERR_TOK (awk, HAWK_ERBRACK);
		return HAWK_NULL;
	}

	if (get_token(awk) <= -1) 
	{
		hawk_clrpt (awk, idx);
		return HAWK_NULL;
	}

	nde = (hawk_nde_var_t*)hawk_callocmem(awk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL) 
	{
		hawk_clrpt (awk, idx);
		ADJERR_LOC (awk, xloc);
		return HAWK_NULL;
	}

	/* search the local variable list */
	idxa = hawk_arr_rsearch(awk->parse.lcls, HAWK_ARR_SIZE(awk->parse.lcls), name->ptr, name->len);
	if (idxa != HAWK_ARR_NIL)
	{
		nde->type = HAWK_NDE_LCLIDX;
		nde->loc = *xloc;
		/*nde->id.name = HAWK_NULL; */
		nde->id.name.ptr = name->ptr;
		nde->id.name.len = name->len;
		nde->id.idxa = idxa;
		nde->idx = idx;

		return (hawk_nde_t*)nde;
	}

	/* search the parameter name list */
	idxa = hawk_arr_search(awk->parse.params, 0, name->ptr, name->len);
	if (idxa != HAWK_ARR_NIL)
	{
		nde->type = HAWK_NDE_ARGIDX;
		nde->loc = *xloc;
		/*nde->id.name = HAWK_NULL; */
		nde->id.name.ptr = name->ptr;
		nde->id.name.len = name->len;
		nde->id.idxa = idxa;
		nde->idx = idx;

		return (hawk_nde_t*)nde;
	}

	/* gets the global variable index */
	idxa = get_global (awk, name);
	if (idxa != HAWK_ARR_NIL)
	{
		nde->type = HAWK_NDE_GBLIDX;
		nde->loc = *xloc;
		/*nde->id.name = HAWK_NULL;*/
		nde->id.name.ptr = name->ptr;
		nde->id.name.len = name->len;
		nde->id.idxa = idxa;
		nde->idx = idx;

		return (hawk_nde_t*)nde;
	}

	/*if (awk->opt.trait & HAWK_IMPLICIT) */
	if (awk->parse.pragma.trait & HAWK_IMPLICIT)
	{
		int fnname = isfnname(awk, name);
		switch (fnname)
		{
			case FNTYPE_FNC:
				SETERR_ARG_LOC (awk, HAWK_EFNCRED, name->ptr, name->len, xloc);
				goto exit_func;

			case FNTYPE_FUN:
				SETERR_ARG_LOC (awk, HAWK_EFUNRED, name->ptr, name->len, xloc);
				goto exit_func;
		}

		HAWK_ASSERT (awk, fnname == 0);

		nde->type = HAWK_NDE_NAMEDIDX;
		nde->loc = *xloc;
		nde->id.name.ptr = name->ptr;
		nde->id.name.len = name->len;
		nde->id.idxa = (hawk_oow_t)-1;
		nde->idx = idx;

		return (hawk_nde_t*)nde;
	}

	/* undefined variable */
	SETERR_ARG_LOC (awk, HAWK_EUNDEF, name->ptr, name->len, xloc);

exit_func:
	hawk_clrpt (awk, idx);
	hawk_freemem (awk, nde);

	return HAWK_NULL;
}

static hawk_nde_t* parse_fncall (hawk_t* awk, const hawk_oocs_t* name, hawk_fnc_t* fnc, const hawk_loc_t* xloc, int flags)
{
	hawk_nde_t* head, * curr, * nde;
	hawk_nde_fncall_t* call;
	hawk_oow_t nargs;
	hawk_loc_t eloc;

	head = curr = HAWK_NULL;
	call = HAWK_NULL;
	nargs = 0;

	if (flags & FNCALL_FLAG_NOARG) goto make_node;
	if (get_token(awk) <= -1) goto oops;

	if (MATCH(awk,TOK_RPAREN)) 
	{
		/* no parameters to the function call */
		if (get_token(awk) <= -1) goto oops;
	}
	else 
	{
		/* parse function parameters */
		while (1) 
		{
			eloc = awk->tok.loc;
			nde = parse_expr_withdc(awk, &eloc);
			if (!nde) goto oops;

			if (!head) head = nde;
			else curr->next = nde;
			curr = nde;

			nargs++;

			if (MATCH(awk,TOK_RPAREN)) 
			{
				if (get_token(awk) <= -1) goto oops;
				break;
			}

			if (!MATCH(awk,TOK_COMMA)) 
			{
				SETERR_TOK (awk, HAWK_ECOMMA);
				goto oops;
			}

			do
			{
				if (get_token(awk) <= -1) goto oops;
			}
			while (MATCH(awk,TOK_NEWLINE));
		}

	}

make_node:
	call = (hawk_nde_fncall_t*)hawk_callocmem(awk, HAWK_SIZEOF(*call));
	if (!call) 
	{
		ADJERR_LOC (awk, xloc);
		goto oops;
	}

	if (flags & FNCALL_FLAG_VAR)
	{
		call->type = HAWK_NDE_FNCALL_VAR;
		call->loc = *xloc;
		call->u.var.var = (hawk_nde_var_t*)name; /* name is a pointer to a variable node */
		call->args = head;
		call->nargs = nargs;
	}
	else if (fnc)
	{
		call->type = HAWK_NDE_FNCALL_FNC;
		call->loc = *xloc;

		call->u.fnc.info.name.ptr = name->ptr;
		call->u.fnc.info.name.len = name->len;
		call->u.fnc.info.mod = fnc->mod;
		call->u.fnc.spec = fnc->spec;

		call->args = head;
		call->nargs = nargs;

		if (nargs > call->u.fnc.spec.arg.max)
		{
			SETERR_LOC (awk, HAWK_EARGTM, xloc);
			goto oops;
		}
		else if (nargs < call->u.fnc.spec.arg.min)
		{
			SETERR_LOC (awk, HAWK_EARGTF, xloc);
			goto oops;
		}
	}
	else
	{
		call->type = HAWK_NDE_FNCALL_FUN;
		call->loc = *xloc;
		call->u.fun.name.ptr = name->ptr; 
		call->u.fun.name.len = name->len;
		call->args = head;
		call->nargs = nargs;

		/* store a non-builtin function call into the awk->parse.funs table */
		if (!hawk_htb_upsert(awk->parse.funs, name->ptr, name->len, call, 0))
		{
			SETERR_LOC (awk, HAWK_ENOMEM, xloc);
			goto oops;
		}
	}

	return (hawk_nde_t*)call;

oops:
	if (call) hawk_freemem (awk, call);
	if (head) hawk_clrpt (awk, head);
	return HAWK_NULL;
}

static int get_number (hawk_t* awk, hawk_tok_t* tok)
{
	hawk_ooci_t c;

	HAWK_ASSERT (awk, HAWK_OOECS_LEN(tok->name) == 0);
	SET_TOKEN_TYPE (awk, tok, TOK_INT);

	c = awk->sio.last.c;

	if (c == HAWK_T('0'))
	{
		ADD_TOKEN_CHAR (awk, tok, c);
		GET_CHAR_TO (awk, c);

		if (c == HAWK_T('x') || c == HAWK_T('X'))
		{
			/* hexadecimal number */
			do 
			{
				ADD_TOKEN_CHAR (awk, tok, c);
				GET_CHAR_TO (awk, c);
			} 
			while (hawk_is_ooch_xdigit(c));

			return 0;
		}
		else if (c == HAWK_T('b') || c == HAWK_T('B'))
		{
			/* binary number */
			do
			{
				ADD_TOKEN_CHAR (awk, tok, c);
				GET_CHAR_TO (awk, c);
			} 
			while (c == HAWK_T('0') || c == HAWK_T('1'));

			return 0;
		}
		else if (c != '.')
		{
			/* octal number */
			while (c >= HAWK_T('0') && c <= HAWK_T('7'))
			{
				ADD_TOKEN_CHAR (awk, tok, c);
				GET_CHAR_TO (awk, c);
			}

			if (c == HAWK_T('8') || c == HAWK_T('9'))
			{
				hawk_ooch_t cc = (hawk_ooch_t)c;
				SETERR_ARG_LOC (awk, HAWK_ELXDIG, &cc, 1, &awk->tok.loc);
				return -1;
			}

			return 0;
		}
	}

	while (hawk_is_ooch_digit(c)) 
	{
		ADD_TOKEN_CHAR (awk, tok, c);
		GET_CHAR_TO (awk, c);
	} 

	if (c == HAWK_T('.'))
	{
		/* floating-point number */
		SET_TOKEN_TYPE (awk, tok, TOK_FLT);

		ADD_TOKEN_CHAR (awk, tok, c);
		GET_CHAR_TO (awk, c);

		while (hawk_is_ooch_digit(c))
		{
			ADD_TOKEN_CHAR (awk, tok, c);
			GET_CHAR_TO (awk, c);
		}
	}

	if (c == HAWK_T('E') || c == HAWK_T('e'))
	{
		SET_TOKEN_TYPE (awk, tok, TOK_FLT);

		ADD_TOKEN_CHAR (awk, tok, c);
		GET_CHAR_TO (awk, c);

		if (c == HAWK_T('+') || c == HAWK_T('-'))
		{
			ADD_TOKEN_CHAR (awk, tok, c);
			GET_CHAR_TO (awk, c);
		}

		while (hawk_is_ooch_digit(c))
		{
			ADD_TOKEN_CHAR (awk, tok, c);
			GET_CHAR_TO (awk, c);
		}
	}

	return 0;
}

/* i think allowing only up to 2 hexadigits is more useful though it 
 * may break compatibilty with other awk implementations. If you want
 * more than 2, define HEX_DIGIT_LIMIT_FOR_X to HAWK_TYPE_MAX(hawk_oow_t). */
/*#define HEX_DIGIT_LIMIT_FOR_X (HAWK_TYPE_MAX(hawk_oow_t))*/
#define HEX_DIGIT_LIMIT_FOR_X (2)

static int get_string (
	hawk_t* awk, hawk_ooch_t end_char, 
	hawk_ooch_t esc_char, int keep_esc_char, int byte_only,
	hawk_oow_t preescaped, hawk_tok_t* tok)
{
	hawk_ooci_t c;
	hawk_oow_t escaped = preescaped;
	hawk_oow_t digit_count = 0;
	hawk_uint32_t c_acc = 0;

	while (1)
	{
		GET_CHAR_TO (awk, c);

		if (c == HAWK_OOCI_EOF)
		{
			SETERR_LOC (awk, HAWK_ESTRNC, &awk->tok.loc);
			return -1;
		}

	#if defined(HAWK_OOCH_IS_BCH)
		/* nothing extra to handle byte_only */
	#else
		if (byte_only && c != HAWK_T('\\') && !HAWK_BYTE_PRINTABLE(c))
		{
			hawk_ooch_t wc = c;
			SETERR_ARG_LOC (awk, HAWK_EMBSCHR, &wc, 1, &awk->tok.loc);
			return -1;
		}
	#endif

		if (escaped == 3)
		{
			if (c >= HAWK_T('0') && c <= HAWK_T('7'))
			{
				c_acc = c_acc * 8 + c - HAWK_T('0');
				digit_count++;
				if (digit_count >= escaped) 
				{
					/* should i limit the max to 0xFF/0377? 
					 if (c_acc > 0377) c_acc = 0377; */
					ADD_TOKEN_UINT32 (awk, tok, c_acc);
					escaped = 0;
				}
				continue;
			}
			else
			{
				ADD_TOKEN_UINT32 (awk, tok, c_acc);
				escaped = 0;
			}
		}
		else if (escaped == HEX_DIGIT_LIMIT_FOR_X || escaped == 4 || escaped == 8)
		{
			if (c >= HAWK_T('0') && c <= HAWK_T('9'))
			{
				c_acc = c_acc * 16 + c - HAWK_T('0');
				digit_count++;
				if (digit_count >= escaped) 
				{
					ADD_TOKEN_UINT32 (awk, tok, c_acc);
					escaped = 0;
				}
				continue;
			}
			else if (c >= HAWK_T('A') && c <= HAWK_T('F'))
			{
				c_acc = c_acc * 16 + c - HAWK_T('A') + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					ADD_TOKEN_UINT32 (awk, tok, c_acc);
					escaped = 0;
				}
				continue;
			}
			else if (c >= HAWK_T('a') && c <= HAWK_T('f'))
			{
				c_acc = c_acc * 16 + c - HAWK_T('a') + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					ADD_TOKEN_UINT32 (awk, tok, c_acc);
					escaped = 0;
				}
				continue;
			}
			else
			{
				hawk_ooch_t rc;

				rc = (escaped == HEX_DIGIT_LIMIT_FOR_X)? HAWK_T('x'):
				     (escaped == 4)? HAWK_T('u'): HAWK_T('U');
				if (digit_count == 0) 
				{
					/* no valid character after the escaper.
					 * keep the escaper as it is. consider this input:
					 *   \xGG
					 * 'c' is at the first G. this part is to restore the
					 * \x part. since \x is not followed by any hexadecimal
					 * digits, it's literally 'x' */
					ADD_TOKEN_CHAR (awk, tok, rc);
				}
				else ADD_TOKEN_UINT32 (awk, tok, c_acc);

				escaped = 0;
			}
		}

		if (escaped == 0 && c == end_char)
		{
			/* terminating quote */
			/*GET_CHAR_TO (awk, c);*/
			GET_CHAR (awk);
			break;
		}

		if (escaped == 0 && c == esc_char)
		{
			escaped = 1;
			continue;
		}

		if (escaped == 1)
		{
			if (c == HAWK_T('n')) c = HAWK_T('\n');
			else if (c == HAWK_T('r')) c = HAWK_T('\r');
			else if (c == HAWK_T('t')) c = HAWK_T('\t');
			else if (c == HAWK_T('f')) c = HAWK_T('\f');
			else if (c == HAWK_T('b')) c = HAWK_T('\b');
			else if (c == HAWK_T('v')) c = HAWK_T('\v');
			else if (c == HAWK_T('a')) c = HAWK_T('\a');
			else if (c >= HAWK_T('0') && c <= HAWK_T('7') && end_char != HAWK_T('/')) 
			{
				/* i don't support the octal notation for a regular expression.
				 * it conflicts with the backreference notation between \1 and \7 inclusive. */
				escaped = 3;
				digit_count = 1;
				c_acc = c - HAWK_T('0');
				continue;
			}
			else if (c == HAWK_T('x')) 
			{
				escaped = HEX_DIGIT_LIMIT_FOR_X;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
			else if (!byte_only && c == HAWK_T('u')) 
			{
				/* in the MCHAR mode, the \u letter will get converted to UTF-8 sequences.
				 * see ADD_TOKEN_UINT32(). */
				escaped = 4;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
			else if (!byte_only && c == HAWK_T('U')) 
			{
				/* in the MCHAR mode, the \u letter will get converted to UTF-8 sequences 
				 * see ADD_TOKEN_UINT32(). */
				escaped = 8;
				digit_count = 0;
				c_acc = 0;
				continue;
			}
			else if (keep_esc_char) 
			{
				/* if the following character doesn't compose a proper
				 * escape sequence, keep the escape character. 
				 * an unhandled escape sequence can be handled 
				 * outside this function since the escape character 
				 * is preserved.*/
				ADD_TOKEN_CHAR (awk, tok, esc_char);
			}

			escaped = 0;
		}

		ADD_TOKEN_CHAR (awk, tok, c);
	}

	return 0;
}

static int get_rexstr (hawk_t* awk, hawk_tok_t* tok)
{
	if (awk->sio.last.c == HAWK_T('/'))
	{
		/* handle an empty regular expression.
		 *
		 * this condition is met when the input is //.
		 * the first / has been tokenized to TOK_DIV already.
		 * if TOK_DIV is seen as a primary, this function is called.
		 * as the token buffer has been cleared by the caller and
		 * the token type is set to TOK_REX, this function can
		 * just return after reading the next character.
		 * see parse_primary_rex(). */
		GET_CHAR (awk);
		return 0;
	}
	else
	{
		hawk_oow_t preescaped = 0;
		if (awk->sio.last.c == HAWK_T('\\'))
		{
			/* for input like /\//, this condition is met. 
			 * the initial escape character is added when the
			 * second charater is handled in get_string() */
			preescaped = 1;
		}
		else
		{
			/* add other initial characters here as get_string()
			 * begins with reading the next character */
			ADD_TOKEN_CHAR (awk, tok, awk->sio.last.c);
		}
		return get_string(awk, HAWK_T('/'), HAWK_T('\\'), 1, 0, preescaped, tok);
	}
}


static int get_single_quoted_string (hawk_t* awk, int byte_only, hawk_tok_t* tok)
{
	hawk_ooci_t c;

	while (1)
	{
		GET_CHAR_TO (awk, c);

		if (c == HAWK_OOCI_EOF)
		{
			SETERR_LOC (awk, HAWK_ESTRNC, &awk->tok.loc);
			return -1;
		}

	#if defined(HAWK_OOCH_IS_BCH)
		/* nothing extra to handle byte_only */
	#else
		if (byte_only && c != HAWK_T('\\') && !HAWK_BYTE_PRINTABLE(c))
		{
			hawk_ooch_t wc = c;
			SETERR_ARG_LOC (awk, HAWK_EMBSCHR, &wc, 1, &awk->tok.loc);
			return -1;
		}
	#endif

		if (c == HAWK_T('\''))
		{
			/* terminating quote */
			GET_CHAR (awk);
			break;
		}

		ADD_TOKEN_CHAR (awk, tok, c);
	}

	return 0;
}

static int skip_spaces (hawk_t* awk)
{
	hawk_ooci_t c = awk->sio.last.c;

	if (awk->opt.trait & HAWK_NEWLINE)
	{
		do 
		{
			while (c != HAWK_T('\n') && hawk_is_ooch_space(c))
				GET_CHAR_TO (awk, c);

			if (c == HAWK_T('\\'))
			{
				hawk_sio_lxc_t bs;
				hawk_sio_lxc_t cr;
				int hascr = 0;

				bs = awk->sio.last;
				GET_CHAR_TO (awk, c);
				if (c == HAWK_T('\r')) 
				{
					hascr = 1;
					cr = awk->sio.last;
					GET_CHAR_TO (awk, c);
				}

				if (c == HAWK_T('\n'))
				{
					GET_CHAR_TO (awk, c);
					continue;
				}
				else
				{
					/* push back the last character */
					unget_char (awk, &awk->sio.last);
					/* push CR if any */
					if (hascr) unget_char (awk, &cr);
					/* restore the orginal backslash */
					awk->sio.last = bs;
				}
			}

			break;
		}
		while (1);
	}
	else
	{
		while (hawk_is_ooch_space(c)) GET_CHAR_TO (awk, c);
	}

	return 0;
}

static int skip_comment (hawk_t* awk)
{
	hawk_ooci_t c = awk->sio.last.c;
	hawk_sio_lxc_t lc;

	if (c == HAWK_T('#'))
	{
		/* skip up to \n */
		do { GET_CHAR_TO (awk, c); }
		while (c != HAWK_T('\n') && c != HAWK_OOCI_EOF);

		if (!(awk->opt.trait & HAWK_NEWLINE)) GET_CHAR (awk);
		return 1; /* comment by # */
	}

	/* handle c-style comment */
	if (c != HAWK_T('/')) return 0; /* not a comment */

	/* save the last character */
	lc = awk->sio.last;
	/* read a new character */
	GET_CHAR_TO (awk, c);

	if (c == HAWK_T('*')) 
	{
		do 
		{
			GET_CHAR_TO (awk, c);
			if (c == HAWK_OOCI_EOF)
			{
				hawk_loc_t loc;
				loc.line = awk->sio.inp->line;
				loc.colm = awk->sio.inp->colm;
				loc.file = awk->sio.inp->name;
				SETERR_LOC (awk, HAWK_ECMTNC, &loc);
				return -1;
			}

			if (c == HAWK_T('*')) 
			{
				GET_CHAR_TO (awk, c);
				if (c == HAWK_OOCI_EOF)
				{
					hawk_loc_t loc;
					loc.line = awk->sio.inp->line;
					loc.colm = awk->sio.inp->colm;
					loc.file = awk->sio.inp->name;
					SETERR_LOC (awk, HAWK_ECMTNC, &loc);
					return -1;
				}

				if (c == HAWK_T('/')) 
				{
					/*GET_CHAR_TO (awk, c);*/
					GET_CHAR (awk);
					break;
				}
			}
		} 
		while (1);

		return 1; /* c-style comment */
	}

	/* unget '*' */
	unget_char (awk, &awk->sio.last);
	/* restore the previous state */
	awk->sio.last = lc;

	return 0;
}

static int get_symbols (hawk_t* awk, hawk_ooci_t c, hawk_tok_t* tok)
{
	struct ops_t
	{
		const hawk_ooch_t* str;
		hawk_oow_t len;
		int tid;
		int trait;
	};

	static struct ops_t ops[] = 
	{
		{ HAWK_T("==="), 3, TOK_TEQ,          0 },
		{ HAWK_T("=="),  2, TOK_EQ,           0 },
		{ HAWK_T("="),   1, TOK_ASSN,         0 },
		{ HAWK_T("!=="), 3, TOK_TNE,          0 },
		{ HAWK_T("!="),  2, TOK_NE,           0 },
		{ HAWK_T("!~"),  2, TOK_NM,           0 },
		{ HAWK_T("!"),   1, TOK_LNOT,         0 },
		{ HAWK_T(">>="), 3, TOK_RS_ASSN,      0 },
		{ HAWK_T(">>"),  2, TOK_RS,           0 },
		{ HAWK_T(">="),  2, TOK_GE,           0 },
		{ HAWK_T(">"),   1, TOK_GT,           0 },
		{ HAWK_T("<<="), 3, TOK_LS_ASSN,      0 },
		{ HAWK_T("<<"),  2, TOK_LS,           0 },
		{ HAWK_T("<="),  2, TOK_LE,           0 },
		{ HAWK_T("<"),   1, TOK_LT,           0 },
		{ HAWK_T("||"),  2, TOK_LOR,          0 },
		{ HAWK_T("|="),  2, TOK_BOR_ASSN,     0 },
		{ HAWK_T("|"),   1, TOK_BOR,          0 },
		{ HAWK_T("&&"),  2, TOK_LAND,         0 },
		{ HAWK_T("&="),  2, TOK_BAND_ASSN,    0 },
		{ HAWK_T("&"),   1, TOK_BAND,         0 },
		{ HAWK_T("^^="), 3, TOK_BXOR_ASSN,    0 },
		{ HAWK_T("^^"),  2, TOK_BXOR,         0 },
		{ HAWK_T("^="),  2, TOK_EXP_ASSN,     0 },
		{ HAWK_T("^"),   1, TOK_EXP,          0 },
		{ HAWK_T("++"),  2, TOK_PLUSPLUS,     0 },
		{ HAWK_T("+="),  2, TOK_PLUS_ASSN,    0 },
		{ HAWK_T("+"),   1, TOK_PLUS,         0 },
		{ HAWK_T("--"),  2, TOK_MINUSMINUS,   0 },
		{ HAWK_T("-="),  2, TOK_MINUS_ASSN,   0 },
		{ HAWK_T("-"),   1, TOK_MINUS,        0 },
		{ HAWK_T("**="), 3, TOK_EXP_ASSN,     0 },
		{ HAWK_T("**"),  2, TOK_EXP,          0 },
		{ HAWK_T("*="),  2, TOK_MUL_ASSN,     0 },
		{ HAWK_T("*"),   1, TOK_MUL,          0 },
		{ HAWK_T("/="),  2, TOK_DIV_ASSN,     0 },
		{ HAWK_T("/"),   1, TOK_DIV,          0 },
		{ HAWK_T("\\="), 2, TOK_IDIV_ASSN,    0 },
		{ HAWK_T("\\"),  1, TOK_IDIV,         0 },
		{ HAWK_T("%%="), 3, TOK_CONCAT_ASSN,  0 },
		{ HAWK_T("%%"),  2, TOK_CONCAT,       0 },
		{ HAWK_T("%="),  2, TOK_MOD_ASSN,     0 },
		{ HAWK_T("%"),   1, TOK_MOD,          0 },
		{ HAWK_T("~~"),  2, TOK_BNOT,         0 },
		{ HAWK_T("~"),   1, TOK_MA,           0 },
		{ HAWK_T("("),   1, TOK_LPAREN,       0 },
		{ HAWK_T(")"),   1, TOK_RPAREN,       0 },
		{ HAWK_T("{"),   1, TOK_LBRACE,       0 },
		{ HAWK_T("}"),   1, TOK_RBRACE,       0 },
		{ HAWK_T("["),   1, TOK_LBRACK,       0 },
		{ HAWK_T("]"),   1, TOK_RBRACK,       0 },
		{ HAWK_T("$"),   1, TOK_DOLLAR,       0 },
		{ HAWK_T(","),   1, TOK_COMMA,        0 },
		{ HAWK_T(";"),   1, TOK_SEMICOLON,    0 },
		{ HAWK_T("::"),  2, TOK_DBLCOLON,     0 },
		{ HAWK_T(":"),   1, TOK_COLON,        0 },
		{ HAWK_T("?"),   1, TOK_QUEST,        0 },
		{ HAWK_NULL,     0, 0,                0 }
	};

	struct ops_t* p;
	int idx = 0;

	/* note that the loop below is not generaic enough.
	 * you must keep the operators strings in a particular order */


	for (p = ops; p->str != HAWK_NULL; )
	{
		if (p->trait == 0 || (awk->opt.trait & p->trait))
		{
			if (p->str[idx] == HAWK_T('\0'))
			{
				ADD_TOKEN_STR (awk, tok, p->str, p->len);
				SET_TOKEN_TYPE (awk, tok, p->tid);
				return 1;
			}

			if (c == p->str[idx])
			{
				idx++;
				GET_CHAR_TO (awk, c);
				continue;
			}
		}

		p++;
	}

	return 0;
}

static int get_token_into (hawk_t* awk, hawk_tok_t* tok)
{
	hawk_ooci_t c;
	int n;
	int skip_semicolon_after_include = 0;

retry:
	do 
	{
		if (skip_spaces(awk) <= -1) return -1;
		if ((n = skip_comment(awk)) <= -1) return -1;
	} 
	while (n >= 1);

	hawk_ooecs_clear (tok->name);
	tok->loc.file = awk->sio.last.file;
	tok->loc.line = awk->sio.last.line;
	tok->loc.colm = awk->sio.last.colm;

	c = awk->sio.last.c;

	if (c == HAWK_OOCI_EOF) 
	{
		n = end_include(awk);
		if (n <= -1) return -1;
		if (n >= 1) 
		{
			/*awk->sio.last = awk->sio.inp->last;*/
			/* mark that i'm retrying after end of an included file */
			skip_semicolon_after_include = 1; 
			goto retry;
		}

		ADD_TOKEN_STR (awk, tok, HAWK_T("<EOF>"), 5);
		SET_TOKEN_TYPE (awk, tok, TOK_EOF);
	}
	else if (c == HAWK_T('\n')) 
	{
		/*ADD_TOKEN_CHAR (awk, tok, HAWK_T('\n'));*/
		ADD_TOKEN_STR (awk, tok, HAWK_T("<NL>"), 4);
		SET_TOKEN_TYPE (awk, tok, TOK_NEWLINE);
		GET_CHAR (awk);
	}
	else if (hawk_is_ooch_digit(c)/*|| c == HAWK_T('.')*/)
	{
		if (get_number (awk, tok) <= -1) return -1;
	}
	else if (c == HAWK_T('.'))
	{
		hawk_sio_lxc_t lc;

		lc = awk->sio.last;
		GET_CHAR_TO (awk, c);

		unget_char (awk, &awk->sio.last);
		awk->sio.last = lc;

		if (hawk_is_ooch_digit(c))
		{
			/* for a token such as .123 */
			if (get_number(awk, tok) <= -1) return -1;
		}
		else 
		{
			c = HAWK_T('.');
			goto try_get_symbols;
		}
	}
	else if (c == HAWK_T('@'))
	{
		int type;

		ADD_TOKEN_CHAR (awk, tok, c);
		GET_CHAR_TO (awk, c);

		/*
		if (c == HAWK_T('@'))
		{
			SET_TOKEN_TYPE (awk, tok, TOK_DBLAT);
			GET_CHAR (awk);
		}*/
		/* other special extended operators here 
		else if (c == HAWK_T('*'))
		{
		}*/
		/*else*/ if (c != HAWK_T('_') && !hawk_is_ooch_alpha(c))
		{
			/* this extended keyword is empty, 
			 * not followed by a valid word */
			SETERR_LOC (awk, HAWK_EXKWEM, &(awk)->tok.loc);
			return -1;
		}
		else
		{
			/* expect normal identifier starting with an alphabet */
			do 
			{
				ADD_TOKEN_CHAR (awk, tok, c);
				GET_CHAR_TO (awk, c);
			} 
			while (c == HAWK_T('_') || hawk_is_ooch_alpha(c) || hawk_is_ooch_digit(c));

			type = classify_ident(awk, HAWK_OOECS_OOCS(tok->name));
			if (type == TOK_IDENT)
			{
				SETERR_TOK (awk, HAWK_EXKWNR);
				return -1;
			}
			SET_TOKEN_TYPE (awk, tok, type);
		}
	}
	else if (c == HAWK_T('B'))
	{
		GET_CHAR_TO (awk, c);
		if (c == HAWK_T('\"'))
		{
			/* multi-byte string/byte array */
			SET_TOKEN_TYPE (awk, tok, TOK_MBS);
			if (get_string(awk, c, HAWK_T('\\'), 0, 1, 0, tok) <= -1) return -1;
		}
		else if (c == HAWK_T('\''))
		{
			SET_TOKEN_TYPE (awk, tok, TOK_MBS);
			if (get_single_quoted_string(awk, 1, tok) <= -1) return -1;
		}
		else
		{
			ADD_TOKEN_CHAR (awk, tok, HAWK_T('B'));
			goto process_identifier;
		}
	}
	else if (c == HAWK_T('_') || hawk_is_ooch_alpha(c))
	{
		int type;

	process_identifier:
		/* identifier */
		do 
		{
			ADD_TOKEN_CHAR (awk, tok, c);
			GET_CHAR_TO (awk, c);
		} 
		while (c == HAWK_T('_') || hawk_is_ooch_alpha(c) || hawk_is_ooch_digit(c));

		type = classify_ident(awk, HAWK_OOECS_OOCS(tok->name));
		SET_TOKEN_TYPE (awk, tok, type);
	}
	else if (c == HAWK_T('\"'))
	{
		/* double-quoted string */
		SET_TOKEN_TYPE (awk, tok, TOK_STR);
		if (get_string(awk, c, HAWK_T('\\'), 0, 0, 0, tok) <= -1) return -1;
	}
	else if (c == HAWK_T('\''))
	{
		/* single-quoted string - no escaping */
		SET_TOKEN_TYPE (awk, tok, TOK_STR);
		if (get_single_quoted_string(awk, 0, tok) <= -1) return -1;
	}
	else
	{
	try_get_symbols:
		n = get_symbols(awk, c, tok);
		if (n <= -1) return -1;
		if (n == 0)
		{
			/* not handled yet */
			if (c == HAWK_T('\0'))
			{
				SETERR_ARG_LOC (
					awk, HAWK_ELXCHR,
					HAWK_T("<NUL>"), 5, &tok->loc);
			}
			else
			{
				hawk_ooch_t cc = (hawk_ooch_t)c;
				SETERR_ARG_LOC (awk, HAWK_ELXCHR, &cc, 1, &tok->loc);
			}
			return -1;
		}

		if (skip_semicolon_after_include && (tok->type == TOK_SEMICOLON || tok->type == TOK_NEWLINE))
		{
			/* this handles the optional semicolon after the 
			 * included file named as in @include "file-name"; */
			skip_semicolon_after_include = 0;
			goto retry;
		}
	}

	if (skip_semicolon_after_include && !(awk->opt.trait & HAWK_NEWLINE))
	{
		/* semiclon has not been skipped yet and the 
		 * newline option is not set. */
		hawk_seterror (awk, HAWK_ESCOLON, HAWK_OOECS_OOCS(tok->name), &tok->loc);
		return -1;
	}

	return 0;
}

static int get_token (hawk_t* awk)
{
	awk->ptok.type = awk->tok.type;
	awk->ptok.loc.file = awk->tok.loc.file;
	awk->ptok.loc.line = awk->tok.loc.line;
	awk->ptok.loc.colm = awk->tok.loc.colm;
	hawk_ooecs_swap (awk->ptok.name, awk->tok.name);

	if (HAWK_OOECS_LEN(awk->ntok.name) > 0)
	{
		awk->tok.type = awk->ntok.type;
		awk->tok.loc.file = awk->ntok.loc.file;
		awk->tok.loc.line = awk->ntok.loc.line;
		awk->tok.loc.colm = awk->ntok.loc.colm;	

		hawk_ooecs_swap (awk->tok.name, awk->ntok.name);
		hawk_ooecs_clear (awk->ntok.name);

		return 0;
	}

	return get_token_into (awk, &awk->tok);
}

static int preget_token (hawk_t* awk)
{
	/* LIMITATION: no more than one token can be pre-read in a row 
	               without consumption. */
	
	if (HAWK_OOECS_LEN(awk->ntok.name) > 0)
	{
		/* you can't read more than 1 token in advance. 
		 * 
		 * if there is a token already read in, it is just
		 * retained. 
		 * 
		 * parsing an expression like '$0 | a' causes this 
		 * funtion to be called before get_token() consumes the
		 * pre-read token. 
		 *
		 * Because the expression like this 
		 *    print $1 | getline x;
		 * must be parsed as
		 *    print $(1 | getline x);
		 * preget_token() is called from parse_primary().
		 *
		 * For the expression '$0 | $2',
		 * 1) parse_primary() calls parse_primary_positional() if $ is encountered.
		 * 2) parse_primary_positional() calls parse_primary() recursively for the positional part after $.
		 * 3) parse_primary() in #2 calls preget_token()
		 * 4) parse_primary() in #1 also calls preget_token().
		 *
		 * this block is reached because no token is consumed between #3 and #4.
		 *
		 * in short, it happens if getline doesn't doesn't follow | after the positional. 
		 *   $1 | $2
		 *   $1 | abc + 20
	 	 */
		return 0;
	}
	else
	{
		/* if there is no token pre-read, we get a new
		 * token and place it to awk->ntok. */ 
		return get_token_into (awk, &awk->ntok);
	}
}

static int classify_ident (hawk_t* awk, const hawk_oocs_t* name)
{
	/* perform binary search */

	/* declaring left, right, mid to be the int type is ok
	 * because we know kwtab is small enough. */
	int left = 0, right = HAWK_COUNTOF(kwtab) - 1, mid;

	while (left <= right)
	{
		int n;
		kwent_t* kwp;

		/*mid = (left + right) / 2;*/
		mid = left + (right - left) / 2;
		kwp = &kwtab[mid];

		n = hawk_comp_oochars(kwp->name.ptr, kwp->name.len, name->ptr, name->len, 0);
		if (n > 0) 
		{
			/* if left, right, mid were of hawk_oow_t,
			 * you would need the following line. 
			if (mid == 0) break;
			 */
			right = mid - 1;
		}
		else if (n < 0) left = mid + 1;
		else
		{
			if ((awk->opt.trait & kwp->trait) != kwp->trait) break;
			return kwp->type;
		}
	}

	return TOK_IDENT;
}

struct deparse_func_t 
{
	hawk_t* awk;
	hawk_ooch_t* tmp;
	hawk_oow_t tmp_len;
	int ret;
};

static int deparse (hawk_t* awk)
{
	hawk_nde_t* nde;
	hawk_chain_t* chain;
	hawk_ooch_t tmp[HAWK_SIZEOF(hawk_oow_t)*8 + 32];
	struct deparse_func_t df;
	int n = 0; 
	hawk_ooi_t op;
	hawk_oocs_t kw;

	HAWK_ASSERT (awk, awk->sio.outf != HAWK_NULL);

	HAWK_MEMSET (&awk->sio.arg, 0, HAWK_SIZEOF(awk->sio.arg));

	CLRERR (awk);
	op = awk->sio.outf(awk, HAWK_SIO_CMD_OPEN, &awk->sio.arg, HAWK_NULL, 0);
	if (op <= -1)
	{
		if (ISNOERR(awk)) SETERR_ARG (awk, HAWK_EOPEN, HAWK_T("<SOUT>"), 6);
		return -1;
	}

#define EXIT_DEPARSE() do { n = -1; goto exit_deparse; } while(0)

	if (awk->parse.pragma.rtx_stack_limit > 0 && awk->parse.pragma.rtx_stack_limit != awk->opt.rtx_stack_limit)
	{
		hawk_oow_t len;

		len = hawk_int_to_oocstr((hawk_int_t)awk->parse.pragma.rtx_stack_limit, 10, HAWK_NULL, tmp, HAWK_COUNTOF(tmp));
		if (hawk_putsroocs(awk, HAWK_T("@pragma stack_limit ")) <= -1 ||
		    hawk_putsroocsn (awk, tmp, len) <= -1 ||
		    hawk_putsroocs(awk, HAWK_T(";\n")) <= -1) EXIT_DEPARSE ();
	}

	if (awk->tree.ngbls > awk->tree.ngbls_base) 
	{
		hawk_oow_t i, len;

		HAWK_ASSERT (awk, awk->tree.ngbls > 0);

		hawk_getkwname (awk, HAWK_KWID_XGLOBAL, &kw);
		if (hawk_putsroocsn(awk, kw.ptr, kw.len) <= -1 || hawk_putsroocs (awk, HAWK_T(" ")) <= -1) EXIT_DEPARSE ();

		for (i = awk->tree.ngbls_base; i < awk->tree.ngbls - 1; i++) 
		{
			if (!(awk->opt.trait & HAWK_IMPLICIT))
			{
				/* use the actual name if no named variable is allowed */
				if (hawk_putsroocsn (awk, HAWK_ARR_DPTR(awk->parse.gbls, i), HAWK_ARR_DLEN(awk->parse.gbls, i)) <= -1) EXIT_DEPARSE ();
			}
			else
			{
				len = hawk_int_to_oocstr((hawk_int_t)i, 10, HAWK_T("__g"), tmp, HAWK_COUNTOF(tmp));
				HAWK_ASSERT (awk, len != (hawk_oow_t)-1);
				if (hawk_putsroocsn (awk, tmp, len) <= -1) EXIT_DEPARSE ();
			}

			if (hawk_putsroocs (awk, HAWK_T(", ")) <= -1) EXIT_DEPARSE ();
		}

		if (!(awk->opt.trait & HAWK_IMPLICIT))
		{
			/* use the actual name if no named variable is allowed */
			if (hawk_putsroocsn(awk, HAWK_ARR_DPTR(awk->parse.gbls,i), HAWK_ARR_DLEN(awk->parse.gbls,i)) <= -1) EXIT_DEPARSE ();
		}
		else
		{
			len = hawk_int_to_oocstr((hawk_int_t)i, 10, HAWK_T("__g"), tmp, HAWK_COUNTOF(tmp));
			HAWK_ASSERT (awk, len != (hawk_oow_t)-1);
			if (hawk_putsroocsn (awk, tmp, len) <= -1) EXIT_DEPARSE ();
		}

		if (awk->opt.trait & HAWK_CRLF)
		{
			if (hawk_putsroocs(awk, HAWK_T(";\r\n\r\n")) <= -1) EXIT_DEPARSE ();
		}
		else
		{
			if (hawk_putsroocs(awk, HAWK_T(";\n\n")) <= -1) EXIT_DEPARSE ();
		}
	}

	df.awk = awk;
	df.tmp = tmp;
	df.tmp_len = HAWK_COUNTOF(tmp);
	df.ret = 0;

	hawk_htb_walk (awk->tree.funs, deparse_func, &df);
	if (df.ret <= -1) EXIT_DEPARSE ();

	for (nde = awk->tree.begin; nde != HAWK_NULL; nde = nde->next)
	{
		hawk_oocs_t kw;

		hawk_getkwname (awk, HAWK_KWID_BEGIN, &kw);

		if (hawk_putsroocsn (awk, kw.ptr, kw.len) <= -1) EXIT_DEPARSE ();
		if (hawk_putsroocs (awk, HAWK_T(" ")) <= -1) EXIT_DEPARSE ();
		if (hawk_prnnde (awk, nde) <= -1) EXIT_DEPARSE ();

		if (awk->opt.trait & HAWK_CRLF)
		{
			if (put_char(awk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
		}

		if (put_char(awk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();
	}

	chain = awk->tree.chain;
	while (chain != HAWK_NULL) 
	{
		if (chain->pattern != HAWK_NULL) 
		{
			if (hawk_prnptnpt(awk, chain->pattern) <= -1) EXIT_DEPARSE ();
		}

		if (chain->action == HAWK_NULL) 
		{
			/* blockless pattern */
			if (awk->opt.trait & HAWK_CRLF)
			{
				if (put_char(awk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
			}

			if (put_char(awk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();
		}
		else 
		{
			if (chain->pattern != HAWK_NULL)
			{
				if (put_char(awk, HAWK_T(' ')) <= -1) EXIT_DEPARSE ();
			}
			if (hawk_prnpt(awk, chain->action) <= -1) EXIT_DEPARSE ();
		}

		if (awk->opt.trait & HAWK_CRLF)
		{
			if (put_char(awk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
		}

		if (put_char(awk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();

		chain = chain->next;
	}

	for (nde = awk->tree.end; nde != HAWK_NULL; nde = nde->next)
	{
		hawk_oocs_t kw;

		hawk_getkwname (awk, HAWK_KWID_END, &kw);

		if (hawk_putsroocsn(awk, kw.ptr, kw.len) <= -1) EXIT_DEPARSE ();
		if (hawk_putsroocs(awk, HAWK_T(" ")) <= -1) EXIT_DEPARSE ();
		if (hawk_prnnde(awk, nde) <= -1) EXIT_DEPARSE ();
		
		/*
		if (awk->opt.trait & HAWK_CRLF)
		{
			if (put_char (awk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
		}

		if (put_char (awk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();
		*/
	}

	if (flush_out (awk) <= -1) EXIT_DEPARSE ();

exit_deparse:
	if (n == 0) CLRERR (awk);
	if (awk->sio.outf(awk, HAWK_SIO_CMD_CLOSE, &awk->sio.arg, HAWK_NULL, 0) != 0)
	{
		if (n == 0)
		{
			if (ISNOERR(awk)) SETERR_ARG (awk, HAWK_ECLOSE, HAWK_T("<SOUT>"), 6);
			n = -1;
		}
	}

	return n;
}

static hawk_htb_walk_t deparse_func (hawk_htb_t* map, hawk_htb_pair_t* pair, void* arg)
{
	struct deparse_func_t* df = (struct deparse_func_t*)arg;
	hawk_fun_t* fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
	hawk_oow_t i, n;
	hawk_oocs_t kw;

	HAWK_ASSERT (awk, hawk_comp_oochars(HAWK_HTB_KPTR(pair), HAWK_HTB_KLEN(pair), fun->name.ptr, fun->name.len) == 0);

#define PUT_C(x,c) \
	if (put_char(x->awk,c)==-1) { \
		x->ret = -1; return HAWK_HTB_WALK_STOP; \
	}

#define PUT_S(x,str) \
	if (hawk_putsroocs(x->awk,str) <= -1) { \
		x->ret = -1; return HAWK_HTB_WALK_STOP; \
	}

#define PUT_SX(x,str,len) \
	if (hawk_putsroocsn (x->awk, str, len) <= -1) { \
		x->ret = -1; return HAWK_HTB_WALK_STOP; \
	}

	hawk_getkwname (df->awk, HAWK_KWID_FUNCTION, &kw);
	PUT_SX (df, kw.ptr, kw.len);

	PUT_C (df, HAWK_T(' '));
	PUT_SX (df, fun->name.ptr, fun->name.len);
	PUT_S (df, HAWK_T(" ("));

	for (i = 0; i < fun->nargs; ) 
	{
		if (fun->argspec && fun->argspec[i] == 'r') PUT_S (df, HAWK_T("&"));
		n = hawk_int_to_oocstr (i++, 10, HAWK_T("__p"), df->tmp, df->tmp_len);
		HAWK_ASSERT (awk, n != (hawk_oow_t)-1);
		PUT_SX (df, df->tmp, n);

		if (i >= fun->nargs) break;
		PUT_S (df, HAWK_T(", "));
	}

	PUT_S (df, HAWK_T(")"));
	if (df->awk->opt.trait & HAWK_CRLF) PUT_C (df, HAWK_T('\r'));

	PUT_C (df, HAWK_T('\n'));

	if (hawk_prnpt(df->awk, fun->body) <= -1) return -1;
	if (df->awk->opt.trait & HAWK_CRLF)
	{
		PUT_C (df, HAWK_T('\r'));
	}
	PUT_C (df, HAWK_T('\n'));

	return HAWK_HTB_WALK_FORWARD;

#undef PUT_C
#undef PUT_S
#undef PUT_SX
}

static int put_char (hawk_t* awk, hawk_ooch_t c)
{
	awk->sio.arg.b.buf[awk->sio.arg.b.len++] = c;
	if (awk->sio.arg.b.len >= HAWK_COUNTOF(awk->sio.arg.b.buf))
	{
		if (flush_out (awk) <= -1) return -1;
	}
	return 0;
}

static int flush_out (hawk_t* awk)
{
	hawk_ooi_t n;

	while (awk->sio.arg.b.pos < awk->sio.arg.b.len)
	{
		CLRERR (awk);
		n = awk->sio.outf (
			awk, HAWK_SIO_CMD_WRITE, &awk->sio.arg,
			&awk->sio.arg.b.buf[awk->sio.arg.b.pos], 
			awk->sio.arg.b.len - awk->sio.arg.b.pos
		);
		if (n <= 0) 
		{
			if (ISNOERR(awk)) 
				SETERR_ARG (awk, HAWK_EWRITE, HAWK_T("<SOUT>"), 6);
			return -1;
		}

		awk->sio.arg.b.pos += n;
	}

	awk->sio.arg.b.pos = 0;
	awk->sio.arg.b.len = 0;
	return 0;
}

int hawk_putsroocs (hawk_t* awk, const hawk_ooch_t* str)
{
	while (*str != HAWK_T('\0'))
	{
		if (put_char (awk, *str) <= -1) return -1;
		str++;
	}

	return 0;
}

int hawk_putsroocsn (
	hawk_t* awk, const hawk_ooch_t* str, hawk_oow_t len)
{
	const hawk_ooch_t* end = str + len;

	while (str < end)
	{
		if (put_char (awk, *str) <= -1) return -1;
		str++;
	}

	return 0;
}

#if defined(HAWK_ENABLE_STATIC_MODULE)

/* let's hardcode module information */
//#include "mod-dir.h"
#include "mod-math.h"
#include "mod-str.h"
//#include "mod-sys.h"

#if defined(HAWK_ENABLE_MOD_MPI)
#include "../awkmod/mod-mpi.h"
#endif

#if defined(HAWK_ENABLE_MOD_MYSQL)
#include "../awkmod/mod-mysql.h"
#endif

#if defined(HAWK_ENABLE_MOD_SED)
#include "../awkmod/mod-sed.h"
#endif

#if defined(HAWK_ENABLE_MOD_UCI)
#include "../awkmod/mod-uci.h"
#endif

/* 
 * if modules are linked statically into the main awk module,
 * this table is used to find the entry point of the modules.
 * you must update this table if you add more modules 
 */

static struct
{
	hawk_ooch_t* modname;
	int (*modload) (hawk_mod_t* mod, hawk_t* awk);
} static_modtab[] = 
{
//	{ HAWK_T("dir"),    hawk_mod_dir },
	{ HAWK_T("math"),   hawk_mod_math },
#if defined(HAWK_ENABLE_MOD_MPI)
	{ HAWK_T("mpi"),    hawk_mod_mpi },
#endif
#if defined(HAWK_ENABLE_MOD_MYSQL)
	{ HAWK_T("mysql"),  hawk_mod_mysql },
#endif
#if defined(HAWK_ENABLE_MOD_SED)
	{ HAWK_T("sed"),    hawk_mod_sed },
#endif
	{ HAWK_T("str"),    hawk_mod_str },
//	{ HAWK_T("sys"),    hawk_mod_sys },
#if defined(HAWK_ENABLE_MOD_UCI)
	{ HAWK_T("uci"),    hawk_mod_uci }
#endif
};
#endif

static hawk_mod_t* query_module (hawk_t* awk, const hawk_oocs_t segs[], int nsegs, hawk_mod_sym_t* sym)
{

	hawk_rbt_pair_t* pair;
	hawk_mod_data_t* mdp;
	hawk_oocs_t ea;
	int n;

	HAWK_ASSERT (awk, nsegs == 2);

	pair = hawk_rbt_search (awk->modtab, segs[0].ptr, segs[0].len);
	if (pair)
	{
		mdp = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
	}
	else
	{
		hawk_mod_data_t md;
		hawk_mod_load_t load = HAWK_NULL;
		hawk_mod_spec_t spec;
		hawk_oow_t buflen;
		/*hawk_ooch_t buf[64 + 12] = HAWK_T("_hawk_mod_");*/

		/* maximum module name length is 64. 15 is decomposed to 13 + 1 + 1.
		 * 10 for _hawk_mod_
		 * 1 for _ at the end when hawk_mod_xxx_ is attempted.
		 * 1 for the terminating '\0'
		 */
		hawk_ooch_t buf[64 + 12]; 

		/* the terminating null isn't needed in buf here */
		HAWK_MEMCPY (buf, HAWK_T("_hawk_mod_"), HAWK_SIZEOF(hawk_ooch_t) * 10); 
		if (segs[0].len > HAWK_COUNTOF(buf) - 15)
		{
			/* module name too long  */
			ea.ptr = segs[0].ptr;
			ea.len = segs[0].len;
			hawk_seterror (awk, HAWK_ESEGTL, &ea, HAWK_NULL);
			return HAWK_NULL;
		}

#if defined(HAWK_ENABLE_STATIC_MODULE)
		/* attempt to find a statically linked module */

		/* TODO: binary search ... */
		for (n = 0; n < HAWK_COUNTOF(static_modtab); n++)
		{
			if (hawk_comp_oocstr(static_modtab[n].modname, segs[0].ptr, 0) == 0) 
			{
				load = static_modtab[n].modload;
				break;
			}
		}

		/*if (n >= HAWK_COUNTOF(static_modtab))
		{

			ea.ptr = segs[0].ptr;
			ea.len = segs[0].len;
			hawk_seterror (awk, HAWK_ENOENT, &ea, HAWK_NULL);
			return HAWK_NULL;
		}*/

		if (load)
		{
			/* found the module in the staic module table */

			HAWK_MEMSET (&md, 0, HAWK_SIZEOF(md));
			/* Note md.handle is HAWK_NULL for a static module */

			/* i copy-insert 'md' into the table before calling 'load'.
			 * to pass the same address to load(), query(), etc */
			pair = hawk_rbt_insert(awk->modtab, segs[0].ptr, segs[0].len, &md, HAWK_SIZEOF(md));
			if (pair == HAWK_NULL)
			{
				hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
				return HAWK_NULL;
			}

			mdp = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
			if (load (&mdp->mod, awk) <= -1)
			{
				hawk_rbt_delete (awk->modtab, segs[0].ptr, segs[0].len);
				return HAWK_NULL;
			}

			goto done;
		}
#endif
		hawk_seterrnum (awk, HAWK_ENOERR, HAWK_NULL);

		/* attempt to find an external module */
		HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
		if (awk->opt.mod[0].len > 0)
			spec.prefix = awk->opt.mod[0].ptr;
		else spec.prefix = HAWK_T(HAWK_DEFAULT_MODPREFIX);

		if (awk->opt.mod[1].len > 0)
			spec.postfix = awk->opt.mod[1].ptr;
		else spec.postfix = HAWK_T(HAWK_DEFAULT_MODPOSTFIX);

		HAWK_MEMSET (&md, 0, HAWK_SIZEOF(md));
		if (awk->prm.modopen && awk->prm.modgetsym && awk->prm.modclose)
		{
			spec.name = segs[0].ptr;
			md.handle = awk->prm.modopen (awk, &spec);
		}
		else md.handle = HAWK_NULL;

		if (md.handle == HAWK_NULL) 
		{
			if (hawk_geterrnum(awk) == HAWK_ENOERR)
			{
				hawk_seterrfmt (awk, HAWK_ENOENT, HAWK_NULL, HAWK_T("module '%.*js' not found"), (int)segs[0].len, segs[0].ptr);
			}
			else
			{
				const hawk_ooch_t* olderrmsg = hawk_backuperrmsg(awk);
				hawk_seterrfmt (awk, HAWK_ENOENT, HAWK_NULL, HAWK_T("module '%.*js' not found - %js"), (int)segs[0].len, segs[0].ptr, olderrmsg);
			}
			return HAWK_NULL;
		}

		buflen = hawk_copy_oocstr_unlimited(&buf[13], segs[0].ptr);
		/* attempt hawk_mod_xxx */
		load = awk->prm.modgetsym(awk, md.handle, &buf[1]);
		if (!load) 
		{
			/* attempt _hawk_mod_xxx */
			load = awk->prm.modgetsym(awk, md.handle, &buf[0]);
			if (!load)
			{
				hawk_seterrnum (awk, HAWK_ENOERR, HAWK_NULL);

				/* attempt hawk_mod_xxx_ */
				buf[13 + buflen] = HAWK_T('_');
				buf[13 + buflen + 1] = HAWK_T('\0');
				load = awk->prm.modgetsym(awk, md.handle, &buf[1]);
				if (!load)
				{
					if (hawk_geterrnum(awk) == HAWK_ENOERR)
					{
						hawk_seterrfmt (awk, HAWK_ENOENT, HAWK_NULL, HAWK_T("module '%.*js' not found"), (int)(12 + buflen));
					}
					else
					{
						const hawk_ooch_t* olderrmsg = hawk_backuperrmsg(awk);
						hawk_seterrfmt (awk, HAWK_ENOENT, HAWK_NULL, HAWK_T("module '%.*js' not found - %js"), (int)(12 + buflen), &buf[1], olderrmsg);
					}
					awk->prm.modclose (awk, md.handle);
					return HAWK_NULL;
				}
			}
		}

		/* i copy-insert 'md' into the table before calling 'load'.
		 * to pass the same address to load(), query(), etc */
		pair = hawk_rbt_insert (awk->modtab, segs[0].ptr, segs[0].len, &md, HAWK_SIZEOF(md));
		if (pair == HAWK_NULL)
		{
			hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
			awk->prm.modclose (awk, md.handle);
			return HAWK_NULL;
		}

		mdp = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
		if (load(&mdp->mod, awk) <= -1)
		{
			hawk_rbt_delete (awk->modtab, segs[0].ptr, segs[0].len);
			awk->prm.modclose (awk, mdp->handle);
			return HAWK_NULL;
		}
	}

done:
	hawk_seterrnum (awk, HAWK_ENOERR, HAWK_NULL);
	n = mdp->mod.query(&mdp->mod, awk, segs[1].ptr, sym);
	if (n <= -1)
	{
		if (hawk_geterrnum(awk) == HAWK_ENOERR)
		{
			hawk_seterrfmt (awk, HAWK_ENOENT, HAWK_NULL, HAWK_T("unable to find '%.*js' in module '%.*js'"), (int)segs[1].len, segs[1].ptr, (int)segs[0].len, segs[0].ptr);
		}
		else
		{
			const hawk_ooch_t* olderrmsg = hawk_backuperrmsg(awk);
			hawk_seterrfmt (awk, HAWK_ENOENT, HAWK_NULL, HAWK_T("unable to find '%.*js' in module '%.*js' - %js"), (int)segs[1].len, segs[1].ptr, (int)segs[0].len, segs[0].ptr, olderrmsg);
		}
		return HAWK_NULL;
	}
	return &mdp->mod;
}
