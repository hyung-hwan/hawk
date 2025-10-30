/*
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

#define FMT_EBADVAR       HAWK_T("'%.*js' not a valid variable name")
#define FMT_ECOMMA        HAWK_T("comma expected in place of '%.*js'")
#define FMT_ECOLON        HAWK_T("colon expected in place of '%.*js'")
#define FMT_EDUPLCL       HAWK_T("duplicate local variable name - %.*js")
#define FMT_EFUNNF        HAWK_T("function '%.*js' not defined")
#define FMT_EIDENT        HAWK_T("identifier expected in place of '%.*js'")
#define FMT_EINTLIT       HAWK_T("integer literal expected in place of '%.*js'")
#define FMT_EIONMNL       HAWK_T("invalid I/O name of length %zu containing '\\0'")
#define FMT_EKWFNC        HAWK_T("keyword 'function' expected in place of '%.*js'")
#define FMT_EKWIN         HAWK_T("keyword 'in' expected in place of '%.*js'")
#define FMT_EKWWHL        HAWK_T("keyword 'while' expected in place of '%.*js'")
#define FMT_EKWCASE       HAWK_T("keyword 'case' expected in place of '%.*js'")
#define FMT_EMULDFL       HAWK_T("multiple '%.*js' labels")
#define FMT_ELBRACE       HAWK_T("left brace expected in place of '%.*js'")
#define FMT_ELPAREN       HAWK_T("left parenthesis expected in place of '%.*js'")
#define FMT_ENOENT_GBL_HS HAWK_T("no such global variable - %.*hs")
#define FMT_ENOENT_GBL_LS HAWK_T("no such global variable - %.*ls")
#define FMT_ERBRACE       HAWK_T("right brace expected in place of '%.*js'")
#define FMT_ERBRACK       HAWK_T("right bracket expected in place of '%.*js'")
#define FMT_ERPAREN       HAWK_T("right parenthesis expected in place of '%.*js'")
#define FMT_ESCOLON       HAWK_T("semicolon expected in place of '%.*js'")
#define FMT_ESEGTL        HAWK_T("segment '%.*js' too long")
#define FMT_EUNDEF        HAWK_T("undefined identifier '%.*js'")
#define FMT_EXKWNR        HAWK_T("'%.*js' not recognized")

#define TOK_FLAGS_LPAREN_CLOSER (1 << 0)

#define PARSE_BLOCK_FLAG_IS_TOP (1 << 0)

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
	TOK_RS_ASSN, /* >> */
	TOK_LS_ASSN, /* << */
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
	TOK_TILDE,   /* ~ - match or bitwise negation */
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
	TOK_LOR, /* || */
	TOK_LAND, /* && */
	TOK_BOR, /* | */
	TOK_PIPE_BD, /* |& - bidirectional pipe */
	TOK_BXOR, /* ^^ - bitwise-xor */
	TOK_BAND, /* & */
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
	TOK_ELLIPSIS,
	TOK_DBLPERIOD, /* not used yet */
	TOK_PERIOD, /* not used yet */

	/* ==  begin reserved words == */
	/* === extended reserved words === */
	TOK_XARGC,
	TOK_XARGV,
	TOK_XABORT,
	TOK_XGLOBAL,
	TOK_XINCLUDE,
	TOK_XINCLUDE_ONCE,
	TOK_XLOCAL,
	TOK_XPRAGMA,
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
	TOK_SWITCH,
	TOK_CASE,
	TOK_DEFAULT,
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
	TOK_GETBLINE,
	TOK_GETLINE,
	/* ==  end reserved words == */

	TOK_IDENT,
	TOK_CHAR,
	TOK_BCHR,
	TOK_INT,
	TOK_FLT,
	TOK_STR,
	TOK_MBS,
	TOK_REX,
	TOK_XNIL,

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

static int parse_progunit (hawk_t* hawk);
static hawk_t* collect_globals (hawk_t* hawk);
static void adjust_static_globals (hawk_t* hawk);
static hawk_oow_t find_global (hawk_t* hawk, const hawk_oocs_t* name);
static hawk_t* collect_locals (hawk_t* hawk, hawk_oow_t nlcls, int flags);

static hawk_nde_t* parse_function (hawk_t* hawk);
static hawk_nde_t* parse_begin (hawk_t* hawk);
static hawk_nde_t* parse_end (hawk_t* hawk);
static hawk_chain_t* parse_action_block (hawk_t* hawk, hawk_nde_t* ptn, int blockless);

static hawk_nde_t* parse_block_dc (hawk_t* hawk, const hawk_loc_t* xloc, int flags);
static hawk_nde_t* parse_statement (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_expr_withdc (hawk_t* hawk, const hawk_loc_t* xloc);

static hawk_nde_t* parse_logical_or (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_logical_and (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_in (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_regex_match (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_bitwise_or (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_bitwise_xor (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_bitwise_and (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_equality (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_relational (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_shift (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_concat (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_additive (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_multiplicative (hawk_t* hawk, const hawk_loc_t* xloc);

static hawk_nde_t* parse_unary (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_exponent (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_unary_exp (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_increment (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_primary (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_primary_ident (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_primary_literal (hawk_t* hawk, const hawk_loc_t* xloc);
static hawk_nde_t* parse_hashidx (hawk_t* hawk, const hawk_oocs_t* name, const hawk_loc_t* xloc);

#define FNCALL_FLAG_NOARG (1 << 0) /* no argument */
#define FNCALL_FLAG_VAR   (1 << 1)
static hawk_nde_t* parse_fncall (hawk_t* hawk, const hawk_oocs_t* name, hawk_fnc_t* fnc, const hawk_loc_t* xloc, int flags);

static hawk_nde_t* parse_primary_ident_segs (hawk_t* hawk, const hawk_loc_t* xloc, const hawk_oocs_t* full, const hawk_oocs_t segs[], int nsegs);

static int get_token (hawk_t* hawk);
static int preget_token (hawk_t* hawk);
static int get_rexstr (hawk_t* hawk, hawk_tok_t* tok);

static int skip_spaces (hawk_t* hawk);
static int skip_comment (hawk_t* hawk);
static int classify_ident (hawk_t* hawk, const hawk_oocs_t* name);

static int deparse (hawk_t* hawk);
static hawk_htb_walk_t deparse_func (hawk_htb_t* map, hawk_htb_pair_t* pair, void* arg);
static int put_char (hawk_t* hawk, hawk_ooch_t c);
static int flush_out (hawk_t* hawk);

static hawk_mod_t* query_module (hawk_t* hawk, const hawk_oocs_t segs[], int nsegs, hawk_mod_sym_t* sym);

typedef struct kwent_t kwent_t;
struct kwent_t
{
	hawk_oocs_t name;
	int type;
	int trait; /* the entry is valid when this option is set */
};

static kwent_t kwtab[] =
{
	/* keep this table in sync with the kw_t enums in "parse-prv.h".
	 * also keep it sorted by the first field for binary search */
	{ { HAWK_T("@abort"),         6 }, TOK_XABORT,        0 },
	{ { HAWK_T("@argc"),          5 }, TOK_XARGC,         0 },
	{ { HAWK_T("@argv"),          5 }, TOK_XARGV,         0 },
	{ { HAWK_T("@global"),        7 }, TOK_XGLOBAL,       0 },
	{ { HAWK_T("@include"),       8 }, TOK_XINCLUDE,      0 },
	{ { HAWK_T("@include_once"), 13 }, TOK_XINCLUDE_ONCE, 0 },
	{ { HAWK_T("@local"),         6 }, TOK_XLOCAL,        0 },
	{ { HAWK_T("@nil"),           4 }, TOK_XNIL,          0 },
	{ { HAWK_T("@pragma"),        7 }, TOK_XPRAGMA,       0 },
	{ { HAWK_T("@reset"),         6 }, TOK_XRESET,        0 },
	{ { HAWK_T("BEGIN"),          5 }, TOK_BEGIN,         HAWK_PABLOCK },
	{ { HAWK_T("END"),            3 }, TOK_END,           HAWK_PABLOCK },
	{ { HAWK_T("break"),          5 }, TOK_BREAK,         0 },
	{ { HAWK_T("case"),           4 }, TOK_CASE,          0 },
	{ { HAWK_T("continue"),       8 }, TOK_CONTINUE,      0 },
	{ { HAWK_T("default"),        7 }, TOK_DEFAULT,       0 },
	{ { HAWK_T("delete"),         6 }, TOK_DELETE,        0 },
	{ { HAWK_T("do"),             2 }, TOK_DO,            0 },
	{ { HAWK_T("else"),           4 }, TOK_ELSE,          0 },
	{ { HAWK_T("exit"),           4 }, TOK_EXIT,          0 },
	{ { HAWK_T("for"),            3 }, TOK_FOR,           0 },
	{ { HAWK_T("function"),       8 }, TOK_FUNCTION,      0 },
	{ { HAWK_T("getbline"),       8 }, TOK_GETBLINE,      HAWK_RIO },
	{ { HAWK_T("getline"),        7 }, TOK_GETLINE,       HAWK_RIO },
	{ { HAWK_T("if"),             2 }, TOK_IF,            0 },
	{ { HAWK_T("in"),             2 }, TOK_IN,            0 },
	{ { HAWK_T("next"),           4 }, TOK_NEXT,          HAWK_PABLOCK },
	{ { HAWK_T("nextfile"),       8 }, TOK_NEXTFILE,      HAWK_PABLOCK },
	{ { HAWK_T("nextofile"),      9 }, TOK_NEXTOFILE,     HAWK_PABLOCK | HAWK_NEXTOFILE },
	{ { HAWK_T("print"),          5 }, TOK_PRINT,         HAWK_RIO },
	{ { HAWK_T("printf"),         6 }, TOK_PRINTF,        HAWK_RIO },
	{ { HAWK_T("return"),         6 }, TOK_RETURN,        0 },
	{ { HAWK_T("switch"),         6 }, TOK_SWITCH,        0 },
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

	/* detect a numeric string */
	{ HAWK_T("NUMSTRDETECT"), 12, 0 },

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

	{ HAWK_T("SCRIPTNAME"),   10, 0 },
	/* it decides the field construction behavior when FS is a regular expression and
	 * the field splitter is composed of whitespaces only. e.g) FS="[ \t]*";
	 * if set to a non-zero value, remove leading spaces and trailing spaces off a record
	 * before field splitting.
	 * if set to zero, leading spaces and trailing spaces result in 1 empty field respectively.
	 * if not set, the behavior is dependent on the hawk->opt.trait & HAWK_STRIPRECSPC */
	{ HAWK_T("STRIPRECSPC"),  11, 0 },

	{ HAWK_T("STRIPSTRSPC"),  11, 0 },

	{ HAWK_T("SUBSEP"),       6,  0 }
};

#define GET_CHAR(hawk) \
	do { if (get_char(hawk) <= -1) return -1; } while(0)

#define GET_CHAR_TO(hawk,c) \
	do { \
		if (get_char(hawk) <= -1) return -1; \
		c = (hawk)->sio.last.c; \
	} while(0)

#define SET_TOKEN_TYPE(hawk,tok,code) \
	do { (tok)->type = (code); } while (0)

#define ADD_TOKEN_CHAR(hawk,tok,c) \
	do { \
		if (HAWK_UNLIKELY(hawk_ooecs_ccat((tok)->name,(c)) == (hawk_oow_t)-1)) \
		{ \
			ADJERR_LOC(hawk, &(tok)->loc); \
			return -1; \
		} \
	} while (0)

#define ADD_TOKEN_STR(hawk,tok,s,l) \
	do { \
		if (HAWK_UNLIKELY(hawk_ooecs_ncat((tok)->name,(s),(l)) == (hawk_oow_t)-1)) \
		{ \
			ADJERR_LOC(hawk, &(tok)->loc); \
			return -1; \
		} \
	} while (0)

#if defined(HAWK_OOCH_IS_BCH)

#	define ADD_TOKEN_UINT32(hawk,tok,c) \
	do { \
		if (c <= 0xFF) ADD_TOKEN_CHAR(hawk, tok, c); \
		else  \
		{ \
			hawk_bch_t __xbuf[HAWK_MBLEN_MAX + 1]; \
			hawk_oow_t __len, __i; \
			__len = uc_to_utf8(c, __xbuf, HAWK_COUNTOF(__xbuf)); /* use utf8 all the time */ \
			for (__i = 0; __i < __len; __i++) ADD_TOKEN_CHAR(hawk, tok, __xbuf[__i]); \
		} \
	} while (0)
#else
#	define ADD_TOKEN_UINT32(hawk,tok,c) ADD_TOKEN_CHAR(hawk,tok,c);
#endif

#define MATCH(hawk,tok_type) ((hawk)->tok.type == (tok_type))
#define MATCH_RANGE(hawk,tok_type_start,tok_type_end) ((hawk)->tok.type >= (tok_type_start) && (hawk)->tok.type <= (tok_type_end))

#define MATCH_TERMINATOR_NORMAL(hawk) \
	(MATCH((hawk),TOK_SEMICOLON) || MATCH((hawk),TOK_NEWLINE))

#define MATCH_TERMINATOR_RBRACE(hawk) \
	((hawk->opt.trait & HAWK_NEWLINE) && MATCH((hawk),TOK_RBRACE))

#define MATCH_TERMINATOR(hawk) \
	(MATCH_TERMINATOR_NORMAL(hawk) || MATCH_TERMINATOR_RBRACE(hawk))

#define ADJERR_LOC(hawk,l) do { if (l) (hawk)->_gem.errloc = *(l); } while (0)

#if defined(HAWK_OOCH_IS_BCH)
static HAWK_INLINE hawk_oow_t uc_to_utf8 (hawk_uch_t uc, hawk_bch_t* buf, hawk_oow_t bsz)
{
	static hawk_cmgr_t* utf8_cmgr = HAWK_NULL;
	if (!utf8_cmgr) utf8_cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
	return utf8_cmgr->uctobc(uc, buf, bsz);
}
#endif

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

static int get_char (hawk_t* hawk)
{
	hawk_ooi_t n;

	if (hawk->sio.nungots > 0)
	{
		/* there are something in the unget buffer */
		hawk->sio.last = hawk->sio.ungot[--hawk->sio.nungots];
		return 0;
	}

	if (hawk->sio.inp->b.pos >= hawk->sio.inp->b.len)
	{
		n = hawk->sio.inf(
			hawk, HAWK_SIO_CMD_READ, hawk->sio.inp,
			hawk->sio.inp->b.buf, HAWK_COUNTOF(hawk->sio.inp->b.buf)
		);
		if (n <= -1) return -1;

		if (n == 0)
		{
			hawk->sio.inp->last.c = HAWK_OOCI_EOF;
			hawk->sio.inp->last.line = hawk->sio.inp->line;
			hawk->sio.inp->last.colm = hawk->sio.inp->colm;
			hawk->sio.inp->last.file = hawk->sio.inp->path;
			hawk->sio.last = hawk->sio.inp->last;
			return 0;
		}

		hawk->sio.inp->b.pos = 0;
		hawk->sio.inp->b.len = n;
	}

	if (hawk->sio.inp->last.c == HAWK_T('\n'))
	{
		/* if the previous charater was a newline,
		 * increment the line counter and reset column to 1.
		 * incrementing it line number here instead of
		 * updating inp->last causes the line number for
		 * TOK_EOF to be the same line as the last newline. */
		hawk->sio.inp->line++;
		hawk->sio.inp->colm = 1;
	}

	hawk->sio.inp->last.c = hawk->sio.inp->b.buf[hawk->sio.inp->b.pos++];
	hawk->sio.inp->last.line = hawk->sio.inp->line;
	hawk->sio.inp->last.colm = hawk->sio.inp->colm++;
	hawk->sio.inp->last.file = hawk->sio.inp->path;
	hawk->sio.last = hawk->sio.inp->last;
	return 0;
}

static void unget_char (hawk_t* hawk, const hawk_sio_lxc_t* c)
{
	/* Make sure that the unget buffer is large enough */
	HAWK_ASSERT(hawk->sio.nungots < HAWK_COUNTOF(hawk->sio.ungot));
	hawk->sio.ungot[hawk->sio.nungots++] = *c;
}

const hawk_ooch_t* hawk_getgblname (hawk_t* hawk, hawk_oow_t idx, hawk_oow_t* len)
{
	HAWK_ASSERT(idx < HAWK_ARR_SIZE(hawk->parse.gbls));

	*len = HAWK_ARR_DLEN(hawk->parse.gbls,idx);
	return HAWK_ARR_DPTR(hawk->parse.gbls,idx);
}

void hawk_getkwname (hawk_t* hawk, hawk_kwid_t id, hawk_oocs_t* s)
{
	*s = kwtab[id].name;
}

static int parse (hawk_t* hawk)
{
	int ret = -1;
	hawk_ooi_t op;

	HAWK_ASSERT(hawk->sio.inf != HAWK_NULL);

	op = hawk->sio.inf(hawk, HAWK_SIO_CMD_OPEN, hawk->sio.inp, HAWK_NULL, 0);
	if (op <= -1)
	{
		/* cannot open the source file.
		 * it doesn't even have to call CLOSE */
		return -1;
	}

	adjust_static_globals (hawk);

	/* get the first character and the first token */
	if (get_char(hawk) <= -1 || get_token(hawk)) goto oops;

	while (1)
	{
		while (MATCH(hawk,TOK_NEWLINE))
		{
			if (get_token(hawk) <= -1) goto oops;
		}
		if (MATCH(hawk,TOK_EOF)) break;

		if (parse_progunit(hawk) <= -1) goto oops;
	}

	if (!(hawk->opt.trait & HAWK_IMPLICIT))
	{
		/* ensure that all functions called are defined in the EXPLICIT-only mode.
		 * otherwise, the error detection will get delay until run-time. */

		hawk_htb_pair_t* p;
		hawk_htb_itr_t itr;

		p = hawk_htb_getfirstpair(hawk->parse.funs, &itr);
		while (p)
		{
			if (hawk_htb_search(hawk->tree.funs, HAWK_HTB_KPTR(p), HAWK_HTB_KLEN(p)) == HAWK_NULL)
			{
				hawk_nde_t* nde;

				/* see parse_fncall() for what is
				 * stored into hawk->tree.funs */
				nde = (hawk_nde_t*)HAWK_HTB_VPTR(p);
				hawk_seterrfmt(hawk, &nde->loc, HAWK_EFUNNF, FMT_EFUNNF, HAWK_HTB_KLEN(p), HAWK_HTB_KPTR(p));
				goto oops;
			}

			p = hawk_htb_getnextpair(hawk->parse.funs, &itr);
		}
	}

	HAWK_ASSERT(hawk->tree.ngbls == HAWK_ARR_SIZE(hawk->parse.gbls));
	HAWK_ASSERT(hawk->sio.inp == &hawk->sio.arg);
	ret = 0;

oops:
	if (ret <= -1)
	{
		/* an error occurred and control has reached here
		 * probably, some included files might not have been
		 * closed. close them */
		while (hawk->sio.inp != &hawk->sio.arg)
		{
			hawk_sio_arg_t* prev;

			/* nothing much to do about a close error */
			hawk->sio.inf(hawk, HAWK_SIO_CMD_CLOSE, hawk->sio.inp, HAWK_NULL, 0);

			prev = hawk->sio.inp->prev;

			HAWK_ASSERT(hawk->sio.inp->name != HAWK_NULL);
			hawk_freemem(hawk, hawk->sio.inp);

			hawk->sio.inp = prev;
		}
	}

	if (hawk->sio.inf(hawk, HAWK_SIO_CMD_CLOSE, hawk->sio.inp, HAWK_NULL, 0) != 0 && ret == 0) ret = -1;

	/* clear the parse tree partially constructed on error */
	if (ret <= -1) hawk_clear(hawk);

	return ret;
}

hawk_ooch_t* hawk_addsionamewithuchars (hawk_t* hawk, const hawk_uch_t* ptr, hawk_oow_t len)
{
	hawk_link_t* link;

	/* TODO: duplication check? */

#if defined(HAWK_OOCH_IS_UCH)
	link = (hawk_link_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_uch_t) * (len + 1));
	if (HAWK_UNLIKELY(!link)) return HAWK_NULL;

	hawk_copy_uchars_to_ucstr_unlimited ((hawk_uch_t*)(link + 1), ptr, len);
#else
	hawk_oow_t bcslen, ucslen;

	ucslen = len;
	if (hawk_convutobchars(hawk, ptr, &ucslen, HAWK_NULL, &bcslen) <= -1) return HAWK_NULL;

	link = (hawk_link_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_bch_t) * (bcslen + 1));
	if (HAWK_UNLIKELY(!link)) return HAWK_NULL;

	ucslen = len;
	bcslen = bcslen + 1;
	hawk_convutobchars(hawk, ptr, &ucslen, (hawk_bch_t*)(link + 1), &bcslen);
	((hawk_bch_t*)(link + 1))[bcslen] = '\0';
#endif

	link->link = hawk->sio_names;
	hawk->sio_names = link;

	return (hawk_ooch_t*)(link + 1);
}

hawk_ooch_t* hawk_addsionamewithbchars (hawk_t* hawk, const hawk_bch_t* ptr, hawk_oow_t len)
{
	hawk_link_t* link;

	/* TODO: duplication check? */

#if defined(HAWK_OOCH_IS_UCH)
	hawk_oow_t bcslen, ucslen;

	bcslen = len;
	if (hawk_convbtouchars(hawk, ptr, &bcslen, HAWK_NULL, &ucslen, 0) <= -1) return HAWK_NULL;

	link = (hawk_link_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_uch_t) * (ucslen + 1));
	if (HAWK_UNLIKELY(!link)) return HAWK_NULL;

	bcslen = len;
	ucslen = ucslen + 1;
	hawk_convbtouchars(hawk, ptr, &bcslen, (hawk_uch_t*)(link + 1), &ucslen, 0);
	((hawk_uch_t*)(link + 1))[ucslen] = '\0';

#else
	link = (hawk_link_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_bch_t) * (len + 1));
	if (HAWK_UNLIKELY(!link)) return HAWK_NULL;

	hawk_copy_bchars_to_bcstr_unlimited ((hawk_bch_t*)(link + 1), ptr, len);
#endif

	link->link = hawk->sio_names;
	hawk->sio_names = link;

	return (hawk_ooch_t*)(link + 1);
}

void hawk_clearsionames (hawk_t* hawk)
{
	hawk_link_t* cur;
	while (hawk->sio_names)
	{
		cur = hawk->sio_names;
		hawk->sio_names = cur->link;
		hawk_freemem(hawk, cur);
	}
}

int hawk_parse (hawk_t* hawk, hawk_sio_cbs_t* sio)
{
	int n;

	/* the source code istream must be provided */
	HAWK_ASSERT(sio != HAWK_NULL);

	/* the source code input stream must be provided at least */
	HAWK_ASSERT(sio->in != HAWK_NULL);

	if (!sio || !sio->in)
	{
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	HAWK_ASSERT(hawk->parse.depth.loop == 0);
	HAWK_ASSERT(hawk->parse.depth.expr == 0);

	hawk_clear(hawk);
	hawk_clearsionames(hawk);

	HAWK_MEMSET (&hawk->sio, 0, HAWK_SIZEOF(hawk->sio));
	hawk->sio.inf = sio->in;
	hawk->sio.outf = sio->out;
	hawk->sio.last.c = HAWK_OOCI_EOF;
	/*hawk->sio.arg.name = HAWK_NULL;
	hawk->sio.arg.handle = HAWK_NULL;
	hawk->sio.arg.path = HAWK_NULL;*/
	hawk->sio.arg.line = 1;
	hawk->sio.arg.colm = 1;
	hawk->sio.arg.pragma_trait = 0;
	hawk->sio.inp = &hawk->sio.arg;

	n = parse(hawk);
	if (n == 0  && hawk->sio.outf != HAWK_NULL) n = deparse(hawk);

	HAWK_ASSERT(hawk->parse.depth.loop == 0);
	HAWK_ASSERT(hawk->parse.depth.expr == 0);

	return n;
}

static int end_include (hawk_t* hawk)
{
	int x;
	hawk_sio_arg_t* cur;

	if (hawk->sio.inp == &hawk->sio.arg) return 0; /* no include */

	/* if it is an included file, close it and
	 * retry to read a character from an outer file */

	x = hawk->sio.inf(hawk, HAWK_SIO_CMD_CLOSE, hawk->sio.inp, HAWK_NULL, 0);

	/* if closing has failed, still destroy the
	 * sio structure first as normal and return
	 * the failure below. this way, the caller
	 * does not call HAWK_SIO_CMD_CLOSE on
	 * hawk->sio.inp again. */

	cur = hawk->sio.inp;
	hawk->sio.inp = hawk->sio.inp->prev;

	HAWK_ASSERT(cur->name != HAWK_NULL);
	/* restore the pragma values */
	hawk->parse.pragma.trait = cur->pragma_trait;
	hawk_freemem(hawk, cur);
	hawk->parse.depth.incl--;

	if (x != 0)
	{
		/* the failure mentioned above is returned here */
		return -1;
	}

	hawk->sio.last = hawk->sio.inp->last;
	return 1; /* ended the included file successfully */
}

static int ever_included (hawk_t* hawk, hawk_sio_arg_t* arg)
{
	hawk_oow_t i;
	for (i = 0; i < hawk->parse.incl_hist.count; i++)
	{
		if (HAWK_MEMCMP(&hawk->parse.incl_hist.ptr[i * HAWK_SIZEOF(arg->unique_id)], arg->unique_id, HAWK_SIZEOF(arg->unique_id)) == 0) return 1;
	}
	return 0;
}

static int record_ever_included (hawk_t* hawk, hawk_sio_arg_t* arg)
{
	if (hawk->parse.incl_hist.count >= hawk->parse.incl_hist.capa)
	{
		hawk_uint8_t* tmp;
		hawk_oow_t newcapa;

		newcapa = hawk->parse.incl_hist.capa + 128;
		tmp = (hawk_uint8_t*)hawk_reallocmem(hawk, hawk->parse.incl_hist.ptr, newcapa * HAWK_SIZEOF(arg->unique_id));
		if (!tmp) return -1;

		hawk->parse.incl_hist.ptr = tmp;
		hawk->parse.incl_hist.capa = newcapa;
	}

	HAWK_MEMCPY (&hawk->parse.incl_hist.ptr[hawk->parse.incl_hist.count * HAWK_SIZEOF(arg->unique_id)], arg->unique_id, HAWK_SIZEOF(arg->unique_id));
	hawk->parse.incl_hist.count++;
	return 0;
}

static int begin_include (hawk_t* hawk, int once)
{
	hawk_sio_arg_t* arg = HAWK_NULL;
	hawk_ooch_t* sio_name;

	if (hawk_count_oocstr(HAWK_OOECS_PTR(hawk->tok.name)) != HAWK_OOECS_LEN(hawk->tok.name))
	{
		/* a '\0' character included in the include file name.
		 * we don't support such a file name */
		hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EIONMNL, FMT_EIONMNL, HAWK_OOECS_LEN(hawk->tok.name));
		return -1;
	}

	if (hawk->opt.includedirs.ptr)
	{
		/* include directory is set... */
/* TODO: search target files in these directories */
	}

	/* store the include-file name into a list
	 * and this list is not deleted after hawk_parse.
	 * the errinfo.loc.file can point to the file name here. */
	sio_name = hawk_addsionamewithoochars(hawk, HAWK_OOECS_PTR(hawk->tok.name), HAWK_OOECS_LEN(hawk->tok.name));
	if (HAWK_UNLIKELY(!sio_name))
	{
		ADJERR_LOC(hawk, &hawk->ptok.loc);
		goto oops;
	}

	arg = (hawk_sio_arg_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*arg));
	if (HAWK_UNLIKELY(!arg))
	{
		ADJERR_LOC(hawk, &hawk->ptok.loc);
		goto oops;
	}

	arg->name = sio_name;
	arg->line = 1;
	arg->colm = 1;

	/* let the argument's prev field point to the current */
	arg->prev = hawk->sio.inp;

	if (hawk->sio.inf(hawk, HAWK_SIO_CMD_OPEN, arg, HAWK_NULL, 0) <= -1)
	{
		ADJERR_LOC(hawk, &hawk->tok.loc);
		goto oops;
	}

	/* store the pragma value */
	arg->pragma_trait = hawk->parse.pragma.trait;
	/* but don't change hawk->parse.pragma.trait. it means the included file inherits
	 * the existing progma values.
	hawk->parse.pragma.trait = (hawk->opt.trait & (HAWK_IMPLICIT | HAWK_MULTILINESTR | HAWK_STRIPRECSPC | HAWK_STRIPSTRSPC));
	*/

	/* update the current pointer after opening is successful */
	hawk->sio.inp = arg;
	hawk->parse.depth.incl++;

	if (once && ever_included(hawk, arg))
	{
		end_include (hawk);
		/* it has been included previously. don't include this file again. */
		if (get_token(hawk) <= -1) return -1; /* skip the include file name */
		if (MATCH(hawk, TOK_SEMICOLON) || MATCH(hawk, TOK_NEWLINE))
		{
			if (get_token(hawk) <= -1) return -1; /* skip the semicolon */
		}
	}
	else
	{
		/* read in the first character in the included file.
		 * so the next call to get_token() sees the character read
		 * from this file. */
		if (record_ever_included(hawk, arg) <= -1 || get_char(hawk) <= -1 || get_token(hawk) <= -1)
		{
			end_include (hawk);
			/* i don't jump to oops since i've called
			 * end_include() where hawk->sio.inp/arg is freed. */
			return -1;
		}
	}

	return 0;

oops:
	/* i don't need to free 'link'  here since it's linked to hawk->sio_names
	 * that's freed at the beginning of hawk_parse() or by hawk_close(). */
	if (arg) hawk_freemem(hawk, arg);
	return -1;
}

static int parse_progunit (hawk_t* hawk)
{
	/*
	@pragma ....
	@include "xxxx"
	@global xxx, xxxx;
	BEGIN { action }
	END { action }
	pattern { action }
	function name (parameter-list) { statement }
	*/

	HAWK_ASSERT(hawk->parse.depth.loop == 0);

	if (MATCH(hawk, TOK_XGLOBAL))
	{
		hawk_oow_t ngbls;

		hawk->parse.id.block = PARSE_GBL;

		if (get_token(hawk) <= -1) return -1;

		HAWK_ASSERT(hawk->tree.ngbls == HAWK_ARR_SIZE(hawk->parse.gbls));
		ngbls = hawk->tree.ngbls;
		if (collect_globals(hawk) == HAWK_NULL)
		{
			hawk_arr_delete(hawk->parse.gbls, ngbls, HAWK_ARR_SIZE(hawk->parse.gbls) - ngbls);
			hawk->tree.ngbls = ngbls;
			return -1;
		}
	}
	else if (MATCH(hawk, TOK_XINCLUDE) || MATCH(hawk, TOK_XINCLUDE_ONCE))
	{
		int once;

		if (hawk->opt.depth.s.incl > 0 &&
		    hawk->parse.depth.incl >=  hawk->opt.depth.s.incl)
		{
			hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EINCLTD);
			return -1;
		}

		once = MATCH(hawk, TOK_XINCLUDE_ONCE);
		if (get_token(hawk) <= -1) return -1;

		if (!MATCH(hawk, TOK_STR))
		{
			hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EINCLSTR);
			return -1;
		}

		if (begin_include(hawk, once) <= -1) return -1;

		/* i just return without doing anything special
		 * after having setting up the environment for file
		 * inclusion. the loop in parse() proceeds to call
		 * parse_progunit() */
	}
	else if (MATCH(hawk, TOK_XPRAGMA))
	{
		hawk_oocs_t name;
		int trait;

		if (get_token(hawk) <= -1) return -1;
		if (!MATCH(hawk, TOK_IDENT))
		{
			hawk_seterrfmt(hawk, &hawk->ptok.loc, HAWK_EIDENT, HAWK_T("identifier expected for '@pragma'"));
			return -1;
		}

		name.len = HAWK_OOECS_LEN(hawk->tok.name);
		name.ptr = HAWK_OOECS_PTR(hawk->tok.name);

		if (hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("entry"), 0) == 0)
		{
			if (get_token(hawk) <= -1) return -1;
			if (!MATCH(hawk, TOK_IDENT))
			{
				hawk_seterrfmt(hawk, &hawk->ptok.loc, HAWK_EIDENT, HAWK_T("function name expected for 'entry'"));
				return -1;
			}

			if (HAWK_OOECS_LEN(hawk->tok.name) >= HAWK_COUNTOF(hawk->parse.pragma.entry))
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EFUNNAM, HAWK_T("entry function name too long"));
				return -1;
			}

			if (hawk->sio.inp == &hawk->sio.arg)
			{
				/* only the top level source */
				if (hawk->parse.pragma.entry[0] != '\0')
				{
					hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EEXIST, HAWK_T("@pragma entry already set"));
					return -1;
				}

				hawk_copy_oochars_to_oocstr(hawk->parse.pragma.entry, HAWK_COUNTOF(hawk->parse.pragma.entry), HAWK_OOECS_PTR(hawk->tok.name), HAWK_OOECS_LEN(hawk->tok.name));
			}
		}
		/* NOTE: trait = is an intended assignment */
		else if (((trait = HAWK_IMPLICIT) && hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("implicit"), 0) == 0) ||
		         ((trait = HAWK_MULTILINESTR) && hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("multilinestr"), 0) == 0) ||
		         ((trait = HAWK_RWPIPE) && hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("rwpipe"), 0) == 0))
		{
			/* @pragma implicit on
			 * @pragma implicit off
			 * @pragma multilinestr on
			 * @pragma multilinestr off
			 * @pragma rwpipe on
			 * @pragma rwpipe off
			 *
			 * The initial values of these pragmas are set in hawk_clear()
			 */
			hawk_oocs_t value;

			if (get_token(hawk) <= -1) return -1;
			if (!MATCH(hawk, TOK_IDENT))
			{
			error_ident_on_off_expected_for_implicit:
				hawk_seterrfmt(hawk, &hawk->ptok.loc, HAWK_EIDENT, HAWK_T("identifier 'on' or 'off' expected for '%.*js'"), name.len, name.ptr);
				return -1;
			}

			value.len = HAWK_OOECS_LEN(hawk->tok.name);
			value.ptr = HAWK_OOECS_PTR(hawk->tok.name);
			if (hawk_comp_oochars_oocstr(value.ptr, value.len, HAWK_T("on"), 0) == 0)
			{
				hawk->parse.pragma.trait |= trait;
			}
			else if (hawk_comp_oochars_oocstr(value.ptr, value.len, HAWK_T("off"), 0) == 0)
			{
				hawk->parse.pragma.trait &= ~trait;
			}
			else
			{
				goto error_ident_on_off_expected_for_implicit;
			}
		}
		/* NOTE: trait = is an intended assignment */
		else if (((trait = HAWK_STRIPRECSPC) && hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("striprecspc"), 0) == 0) ||
		         ((trait = HAWK_STRIPSTRSPC) && hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("stripstrspc"), 0) == 0) ||
		         ((trait = HAWK_NUMSTRDETECT) && hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("numstrdetect"), 0) == 0))
		{
			/* @pragma striprecspc on
			 * @pragma striprecspc off
			 * @pragma stripstrspc on
			 * @pragma stripstrspc off
			 * @pragma numstrdetect on
			 * @pragma numstrdetect off
			 *
			 * The initial values of these pragmas are set in hawk_clear()
			 *
			 * Take note the global STRIPRECSPC is available for context based change.
			 * STRIPRECSPC takes precedence over this pragma.
			 */
			int is_on;
			hawk_oocs_t value;

			if (get_token(hawk) <= -1) return -1;
			if (!MATCH(hawk, TOK_IDENT))
			{
			error_ident_on_off_expected_for_striprecspc:
				hawk_seterrfmt(hawk, &hawk->ptok.loc, HAWK_EIDENT, HAWK_T("identifier 'on' or 'off' expected for '%.*js'"), name.len, name.ptr);
				return -1;
			}

			value.len = HAWK_OOECS_LEN(hawk->tok.name);
			value.ptr = HAWK_OOECS_PTR(hawk->tok.name);
			if (hawk_comp_oochars_oocstr(value.ptr, value.len, HAWK_T("on"), 0) == 0) is_on = 1;
			else if (hawk_comp_oochars_oocstr(value.ptr, value.len, HAWK_T("off"), 0) == 0) is_on = 0;
			else goto error_ident_on_off_expected_for_striprecspc;

			if (hawk->sio.inp == &hawk->sio.arg)
			{
				/* only the top level source. ignore the specified pragma in other levels */
				if (is_on)
					hawk->parse.pragma.trait |= trait;
				else
					hawk->parse.pragma.trait &= ~trait;
			}
		}
		/* ---------------------------------------------------------------------
		 * the pragmas up to this point affect the parser
		 * the following pragmas affect runtime
		 * --------------------------------------------------------------------- */
		else if (hawk_comp_oochars_oocstr(name.ptr, name.len, HAWK_T("stack_limit"), 0) == 0)
		{
			hawk_int_t sl;

			/* @pragma stack_limit 99999 */
			if (get_token(hawk) <= -1) return -1;
			if (!MATCH(hawk, TOK_INT))
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EINTLIT, FMT_EINTLIT, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				return -1;
			}

			sl = hawk_oochars_to_int(HAWK_OOECS_PTR(hawk->tok.name), HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOCHARS_TO_INT_MAKE_OPTION(0, 0, 0), HAWK_NULL, HAWK_NULL);
			if (sl < HAWK_MIN_RTX_STACK_LIMIT) sl = HAWK_MIN_RTX_STACK_LIMIT;
			else if (sl > HAWK_MAX_RTX_STACK_LIMIT) sl = HAWK_MAX_RTX_STACK_LIMIT;
			/* take the specified value if it's greater than the existing value */
			if (sl > hawk->parse.pragma.rtx_stack_limit) hawk->parse.pragma.rtx_stack_limit = sl;
		}
		else
		{
			hawk_seterrfmt(hawk, &hawk->ptok.loc, HAWK_EIDENT, HAWK_T("unknown @pragma identifier - %.*js"), name.len, name.ptr);
			return -1;
		}

		if (get_token(hawk) <= -1) return -1;
		if (MATCH(hawk,TOK_SEMICOLON) && get_token(hawk) <= -1) return -1;
	}
	else if (MATCH(hawk, TOK_FUNCTION))
	{
		hawk->parse.id.block = PARSE_FUNCTION;
		if (parse_function(hawk) == HAWK_NULL) return -1;
	}
	else if (MATCH(hawk, TOK_BEGIN))
	{
		if (!(hawk->opt.trait & HAWK_PABLOCK)) /* pattern action block not allowed */
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EKWFNC, FMT_EKWFNC, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return -1;
		}

		hawk->parse.id.block = PARSE_BEGIN;
		if (get_token(hawk) <= -1) return -1;

		if (MATCH(hawk, TOK_NEWLINE) || MATCH(hawk, TOK_EOF))
		{
			/* when HAWK_NEWLINE is set,
	   		 * BEGIN and { should be located on the same line */
			hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EBLKBEG);
			return -1;
		}

		if (!MATCH(hawk, TOK_LBRACE))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELBRACE, FMT_ELBRACE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return -1;
		}

		hawk->parse.id.block = PARSE_BEGIN_BLOCK;
		if (parse_begin(hawk) == HAWK_NULL) return -1;

		/* skip a semicolon after an action block if any */
		if (MATCH(hawk, TOK_SEMICOLON) && get_token(hawk) <= -1) return -1;
	}
	else if (MATCH(hawk, TOK_END))
	{
		if (!(hawk->opt.trait & HAWK_PABLOCK))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EKWFNC, FMT_EKWFNC, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return -1;
		}

		hawk->parse.id.block = PARSE_END;
		if (get_token(hawk) <= -1) return -1;

		if (MATCH(hawk, TOK_NEWLINE) || MATCH(hawk, TOK_EOF))
		{
			/* when HAWK_NEWLINE is set,
	   		 * END and { should be located on the same line */
			hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EBLKEND);
			return -1;
		}

		if (!MATCH(hawk, TOK_LBRACE))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELBRACE, FMT_ELBRACE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return -1;
		}

		hawk->parse.id.block = PARSE_END_BLOCK;
		if (parse_end(hawk) == HAWK_NULL) return -1;

		/* skip a semicolon after an action block if any */
		if (MATCH(hawk,TOK_SEMICOLON) && get_token(hawk) <= -1) return -1;
	}
	else if (MATCH(hawk, TOK_LBRACE))
	{
		/* patternless block */
		if (!(hawk->opt.trait & HAWK_PABLOCK))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EKWFNC, FMT_EKWFNC, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return -1;
		}

		hawk->parse.id.block = PARSE_ACTION_BLOCK;
		if (parse_action_block(hawk, HAWK_NULL, 0) == HAWK_NULL) return -1;

		/* skip a semicolon after an action block if any */
		if (MATCH(hawk, TOK_SEMICOLON) && get_token(hawk) <= -1) return -1;
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

		if (!(hawk->opt.trait & HAWK_PABLOCK))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EKWFNC, FMT_EKWFNC, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return -1;
		}

		hawk->parse.id.block = PARSE_PATTERN;

		eloc = hawk->tok.loc;
		ptn = parse_expr_withdc(hawk, &eloc);
		if (ptn == HAWK_NULL) return -1;

		HAWK_ASSERT(ptn->next == HAWK_NULL);

		if (MATCH(hawk,TOK_COMMA))
		{
			if (get_token(hawk) <= -1)
			{
				hawk_clrpt(hawk, ptn);
				return -1;
			}

			eloc = hawk->tok.loc;
			ptn->next = parse_expr_withdc(hawk, &eloc);

			if (ptn->next == HAWK_NULL)
			{
				hawk_clrpt(hawk, ptn);
				return -1;
			}
		}

		if (MATCH(hawk,TOK_NEWLINE) || MATCH(hawk,TOK_SEMICOLON) || MATCH(hawk,TOK_EOF))
		{
			/* blockless pattern */
			int eof;
			hawk_loc_t ploc;


			eof = MATCH(hawk,TOK_EOF);
			ploc  = hawk->ptok.loc;

			hawk->parse.id.block = PARSE_ACTION_BLOCK;
			if (parse_action_block(hawk, ptn, 1) == HAWK_NULL)
			{
				hawk_clrpt(hawk, ptn);
				return -1;
			}

			if (!eof)
			{
				if (get_token(hawk) <= -1)
				{
					/* 'ptn' has been added to the chain.
					 * it doesn't have to be cleared here
					 * as hawk_clear() does it */
					/*hawk_clrpt(hawk, ptn);*/
					return -1;
				}
			}

			if ((hawk->opt.trait & HAWK_RIO) != HAWK_RIO)
			{
				/* blockless pattern requires HAWK_RIO
				 * to be ON because the implicit block is
				 * "print $0" */
				hawk_seterrnum(hawk, &ploc, HAWK_ENOSUP);
				return -1;
			}
		}
		else
		{
			/* parse the action block */
			if (!MATCH(hawk,TOK_LBRACE))
			{
				hawk_clrpt(hawk, ptn);
				hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELBRACE, FMT_ELBRACE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				return -1;
			}

			hawk->parse.id.block = PARSE_ACTION_BLOCK;
			if (parse_action_block(hawk, ptn, 0) == HAWK_NULL)
			{
				hawk_clrpt(hawk, ptn);
				return -1;
			}

			/* skip a semicolon after an action block if any */
			if (MATCH(hawk,TOK_SEMICOLON) && get_token(hawk) <= -1) return -1;
		}
	}

	return 0;
}

static hawk_nde_t* parse_function (hawk_t* hawk)
{
	hawk_oocs_t name;
	hawk_nde_t* body = HAWK_NULL;
	hawk_fun_t* fun = HAWK_NULL;
	hawk_ooch_t* argspec = HAWK_NULL;
	hawk_oow_t argspeccapa = 0;
	hawk_oow_t argspeclen = 0;
	hawk_oow_t nargs, g;
	int variadic = 0;
	hawk_htb_pair_t* pair;
	hawk_loc_t xloc;
	int rederr;
	const hawk_ooch_t* redobj;

	/* eat up the keyword 'function' and get the next token */
	HAWK_ASSERT(MATCH(hawk,TOK_FUNCTION));
	if (get_token(hawk) <= -1) return HAWK_NULL;

	/* check if an identifier is in place */
	if (!MATCH(hawk,TOK_IDENT))
	{
		/* cannot find a valid identifier for a function name */
		hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EFUNNAM, HAWK_T("'%.*js' not a valid function name"), HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		return HAWK_NULL;
	}

	name.len = HAWK_OOECS_LEN(hawk->tok.name);
	name.ptr = HAWK_OOECS_PTR(hawk->tok.name);

	/* note that i'm assigning to rederr in the 'if' conditions below.
 	 * i'm not checking equality */
	    /* check if it is a builtin function */
	if ((hawk_findfncwithoocs(hawk, &name) != HAWK_NULL && (rederr = HAWK_EFNCRED, redobj = HAWK_T("intrinsic function"))) ||
	    /* check if it has already been defined as a function */
	    (hawk_htb_search(hawk->tree.funs, name.ptr, name.len) != HAWK_NULL && (rederr = HAWK_EFUNRED, redobj = HAWK_T("function"))) ||
	    /* check if it conflicts with a named variable */
	    (hawk_htb_search(hawk->parse.named, name.ptr, name.len) != HAWK_NULL && (rederr = HAWK_EVARRED, redobj = HAWK_T("variable"))) ||
	    /* check if it coincides to be a global variable name */
	    (((g = find_global(hawk, &name)) != HAWK_ARR_NIL) && (rederr = HAWK_EGBLRED, redobj = HAWK_T("global variable"))))
	{
		hawk_seterrfmt(hawk, &hawk->tok.loc, rederr, HAWK_T("%js '%.*js' redefined"), redobj, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		return HAWK_NULL;
	}

	/* duplicate the name before it's overridden by get_token() */
	name.ptr = hawk_dupoochars(hawk, name.ptr, name.len);
	if (HAWK_UNLIKELY(!name.ptr))
	{
		ADJERR_LOC(hawk, &hawk->tok.loc);
		return HAWK_NULL;
	}
	/* == from this point, failure must jump to oops to free name.ptr == */

	/* get the next token */
	if (get_token(hawk) <= -1) goto oops;

	/* match a left parenthesis */
	if (!MATCH(hawk,TOK_LPAREN))
	{
		/* a function name is not followed by a left parenthesis */
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	/* get the next token */
	if (get_token(hawk) <= -1) goto oops;

	/* make sure that parameter table is empty */
	HAWK_ASSERT(HAWK_ARR_SIZE(hawk->parse.params) == 0);

	/* read parameter list */
	if (MATCH(hawk,TOK_RPAREN))
	{
		/* no function parameter found. get the next token */
		if (get_token(hawk) <= -1) goto oops;
	}
	else
	{
		while (1)
		{
			hawk_ooch_t* pa;
			hawk_oow_t pal;

			if (MATCH(hawk, TOK_ELLIPSIS))
			{
				/* this must be the last parameter. the variadic part doesn't support reference */
				/* this must be the last token before the parenthesis if given.
				 *   function xxx (...)
				 *   function xxx (a ...)
				 *   function xxx (a, b ...)  */
				if (get_token(hawk) <= -1) goto oops;

				if (!MATCH(hawk,TOK_RPAREN))
				{
					hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
					goto oops;
				}

				variadic = 1;
				break;
			}

			if (MATCH(hawk, TOK_BAND)) /* &arg */
			{
				/* pass-by-reference argument */
				nargs = HAWK_ARR_SIZE(hawk->parse.params);
				if (nargs >= argspeccapa)
				{
					hawk_oow_t i, newcapa = HAWK_ALIGN_POW2(nargs + 2, 64);
					argspec = hawk_reallocmem(hawk, argspec, newcapa * HAWK_SIZEOF(*argspec));
					if (HAWK_UNLIKELY(!argspec)) goto oops;
					for (i = argspeccapa; i < newcapa; i++) argspec[i] = HAWK_T(' ');
					argspeccapa = newcapa;
				}
				argspec[nargs] = 'r';
				if (get_token(hawk) <= -1) goto oops;
			}

			if (!MATCH(hawk,TOK_IDENT))
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EBADPAR, HAWK_T("'%.*js' not a valid parameter name"), HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}

			pa = HAWK_OOECS_PTR(hawk->tok.name);
			pal = HAWK_OOECS_LEN(hawk->tok.name);

			/* NOTE: the following is not a conflict.
			 *       so the parameter is not checked against
			 *       global variables.
			 *  global x;
			 *  function f (x) { print x; }
			 *  x in print x is a parameter
			 */

			/* check if a parameter conflicts with the function
			 * name or other parameters */
			if (((hawk->opt.trait & HAWK_STRICTNAMING) &&
			     hawk_comp_oochars(pa, pal, name.ptr, name.len, 0) == 0) ||
			    hawk_arr_search(hawk->parse.params, 0, pa, pal) != HAWK_ARR_NIL)
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EDUPPAR, HAWK_T("duplicate parameter name '%.*js'"), pal, pa);
				goto oops;
			}

			/* push the parameter to the parameter list */
			if (HAWK_ARR_SIZE(hawk->parse.params) >= HAWK_MAX_PARAMS)
			{
				hawk_seterrnum(hawk, &hawk->tok.loc, HAWK_EPARTM);
				goto oops;
			}

			if (hawk_arr_insert(hawk->parse.params, HAWK_ARR_SIZE(hawk->parse.params), pa, pal) == HAWK_ARR_NIL)
			{
				ADJERR_LOC(hawk, &hawk->tok.loc);
				goto oops;
			}

			if (get_token(hawk) <= -1) goto oops;

			/* no ... present after the variable name */
			if (MATCH(hawk,TOK_RPAREN)) break;

			if (!MATCH(hawk,TOK_COMMA))
			{
				hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ECOMMA, FMT_ECOMMA, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}

			do
			{
				if (get_token(hawk) <= -1) goto oops;
			}
			while (MATCH(hawk,TOK_NEWLINE));
		}

		if (argspec)
		{
			/* nargs is the number taken before the current argument word was added to the parse.params array.
			 * so the actual number of arguments is nargs + 1 */
			argspeclen = nargs + 1;
			argspec[argspeclen] = '\0';
		}
		if (get_token(hawk) <= -1) goto oops;
	}

	/* function body can be placed on a different line
	 * from a function name and the parameters even if
	 * HAWK_NEWLINE is set. note TOK_NEWLINE is
	 * available only when the option is set. */
	while (MATCH(hawk,TOK_NEWLINE))
	{
		if (get_token(hawk) <= -1) goto oops;
	}

	/* check if the function body starts with a left brace */
	if (!MATCH(hawk,TOK_LBRACE))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELBRACE, FMT_ELBRACE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}
	if (get_token(hawk) <= -1) goto oops;

	/* remember the current function name so that the body parser
	 * can know the name of the current function being parsed */
	hawk->tree.cur_fun.ptr = name.ptr;
	hawk->tree.cur_fun.len = name.len;

	/* actual function body */
	xloc = hawk->ptok.loc;
	body = parse_block_dc(hawk, &xloc, 1);

	/* clear the current function name remembered */
	hawk->tree.cur_fun.ptr = HAWK_NULL;
	hawk->tree.cur_fun.len = 0;

	if (!body) goto oops;

	/* TODO: study furthur if the parameter names should be saved
	 *       for some reasons - might be needed for better deparsing output */
	nargs = HAWK_ARR_SIZE(hawk->parse.params);
	/* parameter names are not required anymore. clear them */
	hawk_arr_clear(hawk->parse.params);

	fun = (hawk_fun_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*fun));
	if (HAWK_UNLIKELY(!fun))
	{
		ADJERR_LOC(hawk, &hawk->tok.loc);
		goto oops;
	}

	/*fun->name.ptr = HAWK_NULL;*/ /* function name is set below */
	fun->name.len = 0;
	fun->nargs = nargs;
	fun->variadic = variadic;
	fun->argspec = argspec;
	fun->argspeclen = argspeclen;
	fun->body = body;

	pair = hawk_htb_insert(hawk->tree.funs, name.ptr, name.len, fun, 0);
	if (HAWK_UNLIKELY(!pair))
	{
		/* if hawk_htb_insert() fails for other reasons than memory
		 * shortage, there should be implementaion errors as duplicate
		 * functions are detected earlier in this function */
		ADJERR_LOC(hawk, &hawk->tok.loc);
		goto oops;
	}

	/* do some trick to save a string. make it back-point at the key part
	 * of the pair */
	fun->name.ptr = HAWK_HTB_KPTR(pair);
	fun->name.len = HAWK_HTB_KLEN(pair);
	hawk_freemem(hawk, name.ptr);

	/* remove an undefined function call entry from the parse.fun table */
	hawk_htb_delete(hawk->parse.funs, fun->name.ptr, name.len);
	return body;

oops:
	if (body) hawk_clrpt(hawk, body);
	if (argspec) hawk_freemem(hawk, argspec);
	if (fun) hawk_freemem(hawk, fun);
	hawk_freemem(hawk, name.ptr);
	hawk_arr_clear(hawk->parse.params);
	return HAWK_NULL;
}

static hawk_nde_t* parse_begin (hawk_t* hawk)
{
	hawk_nde_t* nde;
	hawk_loc_t xloc;

	xloc = hawk->tok.loc;
	HAWK_ASSERT(MATCH(hawk,TOK_LBRACE));

	if (get_token(hawk) <= -1) return HAWK_NULL;
	nde = parse_block_dc(hawk, &xloc, 1);
	if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;

	if (hawk->tree.begin == HAWK_NULL)
	{
		hawk->tree.begin = nde;
		hawk->tree.begin_tail = nde;
	}
	else
	{
		hawk->tree.begin_tail->next = nde;
		hawk->tree.begin_tail = nde;
	}

	return nde;
}

static hawk_nde_t* parse_end (hawk_t* hawk)
{
	hawk_nde_t* nde;
	hawk_loc_t xloc;

	xloc = hawk->tok.loc;
	HAWK_ASSERT(MATCH(hawk,TOK_LBRACE));

	if (get_token(hawk) <= -1) return HAWK_NULL;
	nde = parse_block_dc(hawk, &xloc, 1);
	if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;

	if (hawk->tree.end == HAWK_NULL)
	{
		hawk->tree.end = nde;
		hawk->tree.end_tail = nde;
	}
	else
	{
		hawk->tree.end_tail->next = nde;
		hawk->tree.end_tail = nde;
	}
	return nde;
}

static hawk_chain_t* parse_action_block (hawk_t* hawk, hawk_nde_t* ptn, int blockless)
{
	hawk_nde_t* nde;
	hawk_chain_t* chain;
	hawk_loc_t xloc;

	xloc = hawk->tok.loc;
	if (blockless) nde = HAWK_NULL;
	else
	{
		HAWK_ASSERT(MATCH(hawk,TOK_LBRACE));
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_block_dc(hawk, &xloc, 1);
		if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;
	}

	chain = (hawk_chain_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*chain));
	if (HAWK_UNLIKELY(!chain))
	{
		hawk_clrpt(hawk, nde);
		ADJERR_LOC(hawk, &xloc);
		return HAWK_NULL;
	}

	chain->pattern = ptn;
	chain->action = nde;
	chain->next = HAWK_NULL;

	if (hawk->tree.chain == HAWK_NULL)
	{
		hawk->tree.chain = chain;
		hawk->tree.chain_tail = chain;
		hawk->tree.chain_size++;
	}
	else
	{
		hawk->tree.chain_tail->next = chain;
		hawk->tree.chain_tail = chain;
		hawk->tree.chain_size++;
	}

	return chain;
}

static hawk_nde_t* parse_block (hawk_t* hawk, const hawk_loc_t* xloc, int flags)
{
	hawk_nde_t* head, * curr, * nde;
	hawk_nde_blk_t* block;
	hawk_oow_t nlcls_outer, nlcls_max, tmp;

	nlcls_outer = HAWK_ARR_SIZE(hawk->parse.lcls);
	nlcls_max = hawk->parse.nlcls_max;

	/* local variable declarations */
	while (1)
	{
		/* skip new lines before local declaration in a block*/
		while (MATCH(hawk,TOK_NEWLINE))
		{
			if (get_token(hawk) <= -1) return HAWK_NULL;
		}

		if (MATCH(hawk,TOK_XINCLUDE) || MATCH(hawk, TOK_XINCLUDE_ONCE))
		{
			/* @include ... */
			int once;

			if (hawk->opt.depth.s.incl > 0 &&
			    hawk->parse.depth.incl >=  hawk->opt.depth.s.incl)
			{
				hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EINCLTD);
				return HAWK_NULL;
			}

			once = MATCH(hawk, TOK_XINCLUDE_ONCE);
			if (get_token(hawk) <= -1) return HAWK_NULL;

			if (!MATCH(hawk,TOK_STR))
			{
				hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EINCLSTR);
				return HAWK_NULL;
			}

			if (begin_include(hawk, once) <= -1) return HAWK_NULL;
		}
		else if (MATCH(hawk,TOK_XLOCAL))
		{
			/* @local ... */
			if (get_token(hawk) <= -1)
			{
				hawk_arr_delete(hawk->parse.lcls, nlcls_outer, HAWK_ARR_SIZE(hawk->parse.lcls) - nlcls_outer);
				return HAWK_NULL;
			}

			if (collect_locals(hawk, nlcls_outer, flags) == HAWK_NULL)
			{
				hawk_arr_delete(hawk->parse.lcls, nlcls_outer, HAWK_ARR_SIZE(hawk->parse.lcls) - nlcls_outer);
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
		while (MATCH(hawk,TOK_NEWLINE))
		{
			if (get_token(hawk) <= -1) return HAWK_NULL;
		}

		/* if EOF is met before the right brace, this is an error */
		if (MATCH(hawk,TOK_EOF))
		{
			hawk_arr_delete(hawk->parse.lcls, nlcls_outer, HAWK_ARR_SIZE(hawk->parse.lcls) - nlcls_outer);
			if (head) hawk_clrpt(hawk, head);
			hawk_seterrnum(hawk, &hawk->tok.loc, HAWK_EEOF);
			return HAWK_NULL;
		}

		/* end the block when the right brace is met */
		if (MATCH(hawk,TOK_RBRACE))
		{
			if (get_token(hawk) <= -1)
			{
				hawk_arr_delete(hawk->parse.lcls, nlcls_outer, HAWK_ARR_SIZE(hawk->parse.lcls) - nlcls_outer);
				if (head) hawk_clrpt(hawk, head);
				return HAWK_NULL;
			}

			break;
		}

		else if (MATCH(hawk, TOK_XINCLUDE) || MATCH(hawk, TOK_XINCLUDE_ONCE))
		{
			int once;

			if (hawk->opt.depth.s.incl > 0 && hawk->parse.depth.incl >=  hawk->opt.depth.s.incl)
			{
				hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EINCLTD);
				return HAWK_NULL;
			}

			once = MATCH(hawk, TOK_XINCLUDE_ONCE);
			if (get_token(hawk) <= -1) return HAWK_NULL;

			if (!MATCH(hawk,TOK_STR))
			{
				hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_EINCLSTR);
				return HAWK_NULL;
			}

			if (begin_include(hawk, once) <= -1) return HAWK_NULL;
		}
		else
		{
			/* parse an actual statement in a block */
			{
				hawk_loc_t sloc;
				sloc = hawk->tok.loc;
				nde = parse_statement(hawk, &sloc);
			}

			if (HAWK_UNLIKELY(!nde))
			{
				hawk_arr_delete(hawk->parse.lcls, nlcls_outer, HAWK_ARR_SIZE(hawk->parse.lcls) - nlcls_outer);
				if (head) hawk_clrpt(hawk, head);
				return HAWK_NULL;
			}

			/* remove unnecessary statements such as adjacent
			 * null statements */
			if (nde->type == HAWK_NDE_NULL)
			{
				hawk_clrpt(hawk, nde);
				continue;
			}
			if (nde->type == HAWK_NDE_BLK && ((hawk_nde_blk_t*)nde)->body == HAWK_NULL)
			{
				hawk_clrpt(hawk, nde);
				continue;
			}

			if (curr == HAWK_NULL) head = nde;
			else curr->next = nde;
			curr = nde;
		}
	}

	block = (hawk_nde_blk_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*block));
	if (HAWK_UNLIKELY(!block))
	{
		hawk_arr_delete(hawk->parse.lcls, nlcls_outer, HAWK_ARR_SIZE(hawk->parse.lcls) - nlcls_outer);
		hawk_clrpt(hawk, head);
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	tmp = HAWK_ARR_SIZE(hawk->parse.lcls);
	if (tmp > hawk->parse.nlcls_max) hawk->parse.nlcls_max = tmp;

	/* remove all lcls to move them up to the top level */
	hawk_arr_delete(hawk->parse.lcls, nlcls_outer, tmp - nlcls_outer);

	/* adjust the number of lcls for a block without any statements */
	/* if (head == HAWK_NULL) tmp = 0; */

	block->type = HAWK_NDE_BLK;
	block->loc = *xloc;
	block->body = head;

	block->org_nlcls = tmp - nlcls_outer; /* number of locals defined in this block */
	block->outer_nlcls = nlcls_outer; /* number of locals defined in outer blocks */

#if 1
	/* TODO: not only local variables but also nested blocks,
	unless it is part of other constructs such as if, can be promoted
	and merged to top-level block */

	/* migrate all block-local variables to the outermost block */
	if (flags & PARSE_BLOCK_FLAG_IS_TOP)
	{
		HAWK_ASSERT(nlcls_outer == 0 && nlcls_max == 0);
		block->nlcls = hawk->parse.nlcls_max - nlcls_outer;
		hawk->parse.nlcls_max = nlcls_max; /* restore */
	}
	else
	{
		block->nlcls = 0;
	}
#else
	/* no migration */
	block->nlcls = block->org_nlcls;
#endif

	return (hawk_nde_t*)block;
}

static hawk_nde_t* parse_block_dc (hawk_t* hawk, const hawk_loc_t* xloc, int flags)
{
	hawk_nde_t* nde;

	/* perform the depth check before calling parse_block() */
	if (hawk->opt.depth.s.block_parse > 0 &&
	    hawk->parse.depth.block >= hawk->opt.depth.s.block_parse)
	{
		hawk_seterrnum(hawk, xloc, HAWK_EBLKNST);
		return HAWK_NULL;
	}

	hawk->parse.depth.block++;
	nde = parse_block(hawk, xloc, flags);
	hawk->parse.depth.block--;

	return nde;
}

int hawk_initgbls (hawk_t* hawk)
{
	int id;

	/* hawk_initgbls is not generic-purpose. call this from
	 * hawk_open only. */
	HAWK_ASSERT(hawk->tree.ngbls_base == 0 && hawk->tree.ngbls == 0);

	hawk->tree.ngbls_base = 0;
	hawk->tree.ngbls = 0;

	for (id = HAWK_MIN_GBL_ID; id <= HAWK_MAX_GBL_ID; id++)
	{
		hawk_oow_t g;

		g = hawk_arr_insert(hawk->parse.gbls, HAWK_ARR_SIZE(hawk->parse.gbls), (hawk_ooch_t*)gtab[id].name, gtab[id].namelen);
		if (g == HAWK_ARR_NIL) return -1;

		HAWK_ASSERT((int)g == id);

		hawk->tree.ngbls_base++;
		hawk->tree.ngbls++;
	}

	HAWK_ASSERT(hawk->tree.ngbls_base == HAWK_MAX_GBL_ID-HAWK_MIN_GBL_ID+1);
	return 0;
}

static void adjust_static_globals (hawk_t* hawk)
{
	int id;

	HAWK_ASSERT(hawk->tree.ngbls_base >= HAWK_MAX_GBL_ID - HAWK_MAX_GBL_ID + 1);

	for (id = HAWK_MIN_GBL_ID; id <= HAWK_MAX_GBL_ID; id++)
	{
		if ((hawk->opt.trait & gtab[id].trait) != gtab[id].trait)
		{
			HAWK_ARR_DLEN(hawk->parse.gbls,id) = 0;
		}
		else
		{
			HAWK_ARR_DLEN(hawk->parse.gbls,id) = gtab[id].namelen;
		}
	}
}

static hawk_oow_t get_global (hawk_t* hawk, const hawk_oocs_t* name)
{
	hawk_oow_t i;
	hawk_arr_t* gbls = hawk->parse.gbls;

	for (i = HAWK_ARR_SIZE(gbls); i > 0; )
	{
		i--;
		if (hawk_comp_oochars(HAWK_ARR_DPTR(gbls,i), HAWK_ARR_DLEN(gbls,i), name->ptr, name->len, 0) == 0) return i;
	}

	return HAWK_ARR_NIL;
}

static hawk_oow_t find_global (hawk_t* hawk, const hawk_oocs_t* name)
{
	hawk_oow_t i;
	hawk_arr_t* gbls = hawk->parse.gbls;

	for (i = 0; i < HAWK_ARR_SIZE(gbls); i++)
	{
		if (hawk_comp_oochars(HAWK_ARR_DPTR(gbls,i), HAWK_ARR_DLEN(gbls,i), name->ptr, name->len, 0) == 0) return i;
	}

	return HAWK_ARR_NIL;
}

static int add_global (hawk_t* hawk, const hawk_oocs_t* name, hawk_loc_t* xloc, int disabled)
{
	hawk_oow_t ngbls;

	/* check if it is a keyword */
	if (classify_ident(hawk, name) != TOK_IDENT)
	{
		hawk_seterrfmt(hawk, xloc, HAWK_EKWRED,
			HAWK_T("unable to add global '%.*js' - keyword '%.*js' redefined"),
			name->len, name->ptr, name->len, name->ptr);
		return -1;
	}

	/* check if it conflicts with a builtin function name */
	if (hawk_findfncwithoocs(hawk, name) != HAWK_NULL)
	{
		hawk_seterrfmt(hawk, xloc, HAWK_EFNCRED,
			HAWK_T("unable to add global '%.*js' - intrinsic function '%.*js' redefined"),
			name->len, name->ptr, name->len, name->ptr);
		return -1;
	}

	/* check if it conflicts with a function name */
	if (hawk_htb_search(hawk->tree.funs, name->ptr, name->len) != HAWK_NULL ||
	/* check if it conflicts with a function name caught in the function call table */
         hawk_htb_search(hawk->parse.funs, name->ptr, name->len) != HAWK_NULL)
	{
		hawk_seterrfmt(hawk, xloc, HAWK_EFUNRED,
			HAWK_T("unable to add global '%.*js' - function '%.*js' redefined"),
			name->len, name->ptr, name->len, name->ptr);
		return -1;
	}

	/* check if it conflicts with other global variable names */
	if (find_global(hawk, name) != HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, xloc, HAWK_EDUPGBL,
			HAWK_T("unable to add global '%.*js' - duplicate global variable name '%.*js'"),
			name->len, name->ptr, name->len, name->ptr);
		return -1;
	}

#if 0
	/* TODO: need to check if it conflicts with a named variable to
	 * disallow such a program shown below (IMPLICIT & EXPLICIT on)
	 *  BEGIN {X=20; x(); x(); x(); print X}
	 *  @global X;
	 *  function x() { print X++; }
	 */
	if (hawk_htb_search(hawk->parse.named, name, len) != HAWK_NULL)
	{
		hawk_seterrfmt(hawk, xloc, HAWK_EVARRED, HAWK_T("variable '%.*js' redefined"), len, name);
		return -1;
	}
#endif

	ngbls = HAWK_ARR_SIZE(hawk->parse.gbls);
	if (ngbls >= HAWK_MAX_GBLS)
	{
		hawk_seterrfmt(hawk, xloc, HAWK_EGBLTM,
			HAWK_T("unable to add global '%.*js' - too many globals"),
			name->len, name->ptr);
		return -1;
	}

	if (hawk_arr_insert(hawk->parse.gbls, HAWK_ARR_SIZE(hawk->parse.gbls), (hawk_ooch_t*)name->ptr, name->len) == HAWK_ARR_NIL)
	{
		const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
		hawk_seterrfmt(hawk, xloc, hawk_geterrnum(hawk),
			HAWK_T("unable to add global '%.*js' - %js"),
			name->len, name->ptr, bem);
		return -1;
	}

	HAWK_ASSERT(ngbls == HAWK_ARR_SIZE(hawk->parse.gbls) - 1);

	/* the disabled item is inserted normally but
	 * the name length is reset to zero. */
	if (disabled) HAWK_ARR_DLEN(hawk->parse.gbls,ngbls) = 0;

	hawk->tree.ngbls = HAWK_ARR_SIZE (hawk->parse.gbls);
	HAWK_ASSERT(ngbls == hawk->tree.ngbls - 1);

	/* return the id which is the index to the gbl table. */
	return (int)ngbls;
}

int hawk_addgblwithbcstr (hawk_t* hawk, const hawk_bch_t* name)
{
	int n;
	hawk_bcs_t ncs;

	if (hawk->tree.ngbls > hawk->tree.ngbls_base)
	{
		/* this function is not allowed after hawk_parse() is called */
		hawk_seterrbfmt(hawk, HAWK_NULL, HAWK_EPERM, "not permitted to add global '%hs'", name);
		return -1;
	}

	ncs.ptr = (hawk_bch_t*)name;
	ncs.len = hawk_count_bcstr(name);
	if (ncs.len <= 0)
	{
		hawk_seterrbfmt(hawk, HAWK_NULL, HAWK_EINVAL, "blank global name not permitted");
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	n = add_global(hawk, &ncs, HAWK_NULL, 0);
#else
	{
		hawk_ucs_t wcs;
		wcs.ptr = hawk_dupbtoucstr(hawk, ncs.ptr, &wcs.len, 0);
		if (HAWK_UNLIKELY(!wcs.ptr))
		{
			const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
			hawk_seterrfmt(hawk, HAWK_NULL, hawk_geterrnum(hawk),
				HAWK_T("unable to add global '%hs' - %js"), name, bem);
			return -1;
		}
		n = add_global(hawk, &wcs, HAWK_NULL, 0);
		hawk_freemem(hawk, wcs.ptr);
	}
#endif

	/* update the count of the static globals.
	 * the total global count has been updated inside add_global. */
	if (n >= 0) hawk->tree.ngbls_base++;

	return n;
}

int hawk_addgblwithucstr (hawk_t* hawk, const hawk_uch_t* name)
{
	int n;
	hawk_ucs_t ncs;

	if (hawk->tree.ngbls > hawk->tree.ngbls_base)
	{
		/* this function is not allowed after hawk_parse is called */
		hawk_seterrbfmt(hawk, HAWK_NULL, HAWK_EPERM, "not permitted to add global '%ls'", name);
		return -1;
	}

	ncs.ptr = (hawk_uch_t*)name;
	ncs.len = hawk_count_ucstr(name);
	if (ncs.len <= 0)
	{
		hawk_seterrbfmt(hawk, HAWK_NULL, HAWK_EINVAL, "blank global name not permitted");
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	{
		hawk_bcs_t mbs;
		mbs.ptr = hawk_duputobcstr(hawk, ncs.ptr, &mbs.len);
		if (HAWK_UNLIKELY(!mbs.ptr))
		{
			const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
			hawk_seterrfmt(hawk, HAWK_NULL, hawk_geterrnum(hawk),
				HAWK_T("unable to add global '%ls' - %js"), name, bem);
			return -1;
		}
		n = add_global(hawk, &mbs, HAWK_NULL, 0);
		hawk_freemem(hawk, mbs.ptr);
	}
#else
	n = add_global(hawk, &ncs, HAWK_NULL, 0);
#endif

	/* update the count of the static globals.
	 * the total global count has been updated inside add_global. */
	if (n >= 0) hawk->tree.ngbls_base++;

	return n;
}

#define HAWK_NUM_STATIC_GBLS \
	(HAWK_MAX_GBL_ID-HAWK_MIN_GBL_ID+1)

int hawk_delgblwithbcstr (hawk_t* hawk, const hawk_bch_t* name)
{
	hawk_oow_t n;
	hawk_bcs_t ncs;
	hawk_ucs_t wcs;

	ncs.ptr = (hawk_bch_t*)name;
	ncs.len = hawk_count_bcstr(name);

	if (hawk->tree.ngbls > hawk->tree.ngbls_base)
	{
		/* this function is not allow after hawk_parse is called */
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EPERM);
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	n = hawk_arr_search(hawk->parse.gbls, HAWK_NUM_STATIC_GBLS, ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_HS, ncs.len, ncs.ptr);
		return -1;
	}
#else
	wcs.ptr = hawk_dupbtoucstr(hawk, ncs.ptr, &wcs.len, 0);
	if (HAWK_UNLIKELY(!wcs.ptr)) return -1;
	n = hawk_arr_search(hawk->parse.gbls, HAWK_NUM_STATIC_GBLS, wcs.ptr, wcs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_LS, wcs.len, wcs.ptr);
		hawk_freemem(hawk, wcs.ptr);
		return -1;
	}
	hawk_freemem(hawk, wcs.ptr);
#endif

	/* invalidate the name if deletion is requested.
	 * this approach does not delete the entry.
	 * if hawk_delgbl() is called with the same name
	 * again, the entry will be appended again.
	 * never call this funciton unless it is really required. */
	/*
	hawk->parse.gbls.buf[n].name.ptr[0] = HAWK_T('\0');
	hawk->parse.gbls.buf[n].name.len = 0;
	*/
	n = hawk_arr_uplete(hawk->parse.gbls, n, 1);
	HAWK_ASSERT(n == 1);

	return 0;
}

int hawk_delgblwithucstr (hawk_t* hawk, const hawk_uch_t* name)
{
	hawk_oow_t n;
	hawk_ucs_t ncs;
	hawk_bcs_t mbs;

	ncs.ptr = (hawk_uch_t*)name;
	ncs.len = hawk_count_ucstr(name);

	if (hawk->tree.ngbls > hawk->tree.ngbls_base)
	{
		/* this function is not allow after hawk_parse is called */
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EPERM);
		return -1;
	}

#if defined(HAWK_OOCH_IS_BCH)
	mbs.ptr = hawk_duputobcstr(hawk, ncs.ptr, &mbs.len);
	if (HAWK_UNLIKELY(!mbs.ptr)) return -1;
	n = hawk_arr_search(hawk->parse.gbls, HAWK_NUM_STATIC_GBLS, mbs.ptr, mbs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_HS, mbs.len, mbs.ptr);
		hawk_freemem(hawk, mbs.ptr);
		return -1;
	}
	hawk_freemem(hawk, mbs.ptr);
#else
	n = hawk_arr_search(hawk->parse.gbls, HAWK_NUM_STATIC_GBLS, ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_LS, ncs.len, ncs.ptr);
		return -1;
	}
#endif

	/* invalidate the name if deletion is requested.
	 * this approach does not delete the entry.
	 * if hawk_delgbl() is called with the same name
	 * again, the entry will be appended again.
	 * never call this funciton unless it is really required. */
	/*
	hawk->parse.gbls.buf[n].name.ptr[0] = HAWK_T('\0');
	hawk->parse.gbls.buf[n].name.len = 0;
	*/
	n = hawk_arr_uplete(hawk->parse.gbls, n, 1);
	HAWK_ASSERT(n == 1);

	return 0;
}

int hawk_findgblwithbcstr (hawk_t* hawk, const hawk_bch_t* name, int inc_builtins)
{
	hawk_oow_t n;
	hawk_bcs_t ncs;
	hawk_ucs_t wcs;

	ncs.ptr = (hawk_bch_t*)name;
	ncs.len = hawk_count_bcstr(name);

#if defined(HAWK_OOCH_IS_BCH)
	n = hawk_arr_search(hawk->parse.gbls, (inc_builtins? 0: HAWK_NUM_STATIC_GBLS), ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_HS, ncs.len, ncs.ptr);
		return -1;
	}
#else
	wcs.ptr = hawk_dupbtoucstr(hawk, ncs.ptr, &wcs.len, 0);
	if (HAWK_UNLIKELY(!wcs.ptr)) return -1;
	n = hawk_arr_search(hawk->parse.gbls, (inc_builtins? 0: HAWK_NUM_STATIC_GBLS), wcs.ptr, wcs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_LS, wcs.len, wcs.ptr);
		hawk_freemem(hawk, wcs.ptr);
		return -1;
	}
	hawk_freemem(hawk, wcs.ptr);
#endif

	return (int)n;
}

int hawk_findgblwithucstr (hawk_t* hawk, const hawk_uch_t* name, int inc_builtins)
{
	hawk_oow_t n;
	hawk_ucs_t ncs;
	hawk_bcs_t mbs;

	ncs.ptr = (hawk_uch_t*)name;
	ncs.len = hawk_count_ucstr(name);

#if defined(HAWK_OOCH_IS_BCH)
	mbs.ptr = hawk_duputobcstr(hawk, ncs.ptr, &mbs.len);
	if (HAWK_UNLIKELY(!mbs.ptr)) return -1;
	n = hawk_arr_search(hawk->parse.gbls, (inc_builtins? 0: HAWK_NUM_STATIC_GBLS), mbs.ptr, mbs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_HS, mbs.len, mbs.ptr);
		hawk_freemem(hawk, mbs.ptr);
		return -1;
	}
	hawk_freemem(hawk, mbs.ptr);
#else
	n = hawk_arr_search(hawk->parse.gbls, (inc_builtins? 0: HAWK_NUM_STATIC_GBLS), ncs.ptr, ncs.len);
	if (n == HAWK_ARR_NIL)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, FMT_ENOENT_GBL_LS, ncs.len, ncs.ptr);
		return -1;
	}
#endif

	return (int)n;
}


static hawk_t* collect_globals (hawk_t* hawk)
{
	if (MATCH(hawk,TOK_NEWLINE))
	{
		/* special check if the first name is on the
		 * same line when HAWK_NEWLINE is on */
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EVARMS);
		return HAWK_NULL;
	}

	while (1)
	{
		if (!MATCH(hawk,TOK_IDENT))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EBADVAR, FMT_EBADVAR, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return HAWK_NULL;
		}

		if (add_global(hawk, HAWK_OOECS_OOCS(hawk->tok.name), &hawk->tok.loc, 0) <= -1) return HAWK_NULL;

		if (get_token(hawk) <= -1) return HAWK_NULL;

		if (MATCH_TERMINATOR_NORMAL(hawk))
		{
			/* skip a terminator (;, <NL>) */
			if (get_token(hawk) <= -1) return HAWK_NULL;
			break;
		}

		/*
		 * unlike collect_locals(), the right brace cannot
		 * terminate a global declaration as it can never be
		 * placed within a block.
		 * so do not perform MATCH_TERMINATOR_RBRACE(hawk))
		 */

		if (!MATCH(hawk,TOK_COMMA))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ECOMMA, FMT_ECOMMA, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return HAWK_NULL;
		}

		do
		{
			if (get_token(hawk) <= -1) return HAWK_NULL;
		}
		while (MATCH(hawk,TOK_NEWLINE));
	}

	return hawk;
}

static hawk_t* collect_locals (hawk_t* hawk, hawk_oow_t nlcls, int flags)
{
	if (MATCH(hawk,TOK_NEWLINE))
	{
		/* special check if the first name is on the
		 * same line when HAWK_NEWLINE is on */
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EVARMS);
		return HAWK_NULL;
	}

	while (1)
	{
		hawk_oocs_t lcl;
		hawk_oow_t n;

		if (!MATCH(hawk,TOK_IDENT))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EBADVAR, FMT_EBADVAR, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return HAWK_NULL;
		}

		lcl = *HAWK_OOECS_OOCS(hawk->tok.name);

		/* check if it conflicts with a builtin function name
		 * function f() { local length; } */
		if (hawk_findfncwithoocs(hawk, &lcl) != HAWK_NULL)
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EFNCRED, HAWK_T("intrinsic function '%.*js' redefined"), lcl.len, lcl.ptr);
			return HAWK_NULL;
		}

		if (flags & PARSE_BLOCK_FLAG_IS_TOP)
		{
			/* check if it conflicts with a parameter name.
			 * the first level declaration is treated as the same
			 * scope as the parameter list */
			n = hawk_arr_search(hawk->parse.params, 0, lcl.ptr, lcl.len);
			if (n != HAWK_ARR_NIL)
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EPARRED, HAWK_T("parameter '%.*js' redefined"), lcl.len, lcl.ptr);
				return HAWK_NULL;
			}
		}

		if (hawk->opt.trait & HAWK_STRICTNAMING)
		{
			/* check if it conflicts with the owning function */
			if (hawk->tree.cur_fun.ptr != HAWK_NULL)
			{
				if (hawk_comp_oochars(lcl.ptr, lcl.len, hawk->tree.cur_fun.ptr, hawk->tree.cur_fun.len, 0) == 0)
				{
					hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EFUNRED, HAWK_T("function '%.*js' redefined"), lcl.len, lcl.ptr);
					return HAWK_NULL;
				}
			}
		}

		/* check if it conflicts with other local variable names */
		n = hawk_arr_search(hawk->parse.lcls, nlcls, lcl.ptr, lcl.len);
		if (n != HAWK_ARR_NIL)
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EDUPLCL, FMT_EDUPLCL, lcl.len, lcl.ptr);
			return HAWK_NULL;
		}

		/* check if it conflicts with global variable names */
		n = find_global(hawk, &lcl);
		if (n != HAWK_ARR_NIL)
		{
			if (n < hawk->tree.ngbls_base)
			{
				/* it is a conflict only if it is one of a static global variable */
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EDUPLCL, FMT_EDUPLCL, lcl.len, lcl.ptr);
				return HAWK_NULL;
			}
		}

		if (HAWK_ARR_SIZE(hawk->parse.lcls) >= HAWK_MAX_LCLS)
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_ELCLTM, HAWK_T("too many local variables defined - %.*js"), lcl.len, lcl.ptr);
			return HAWK_NULL;
		}

		if (hawk_arr_insert(hawk->parse.lcls, HAWK_ARR_SIZE(hawk->parse.lcls), lcl.ptr, lcl.len) == HAWK_ARR_NIL)
		{
			ADJERR_LOC(hawk, &hawk->tok.loc);
			return HAWK_NULL;
		}

		if (get_token(hawk) <= -1) return HAWK_NULL;

		if (MATCH_TERMINATOR_NORMAL(hawk))
		{
			/* skip the terminator (;, <NL>) */
			if (get_token(hawk) <= -1) return HAWK_NULL;
			break;
		}

		if (MATCH_TERMINATOR_RBRACE(hawk))
		{
			/* should not skip } */
			break;
		}

		if (!MATCH(hawk,TOK_COMMA))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ECOMMA, FMT_ECOMMA, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return HAWK_NULL;
		}

		do
		{
			if (get_token(hawk) <= -1) return HAWK_NULL;
		}
		while (MATCH(hawk,TOK_NEWLINE));
	}

	return hawk;
}

static hawk_nde_t* parse_if (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* test = HAWK_NULL;
	hawk_nde_t* then_part = HAWK_NULL;
	hawk_nde_t* else_part = HAWK_NULL;
	hawk_nde_if_t* nde;
	hawk_loc_t eloc, tloc;

	if (!MATCH(hawk,TOK_LPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		return HAWK_NULL;
	}
	if (get_token(hawk) <= -1) return HAWK_NULL;

	eloc = hawk->tok.loc;
	test = parse_expr_withdc(hawk, &eloc);
	if (test == HAWK_NULL) goto oops;

	if (!MATCH(hawk,TOK_RPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

/* TODO: optimization. if you know 'test' evaluates to true or false,
 *       you can drop the 'if' statement and take either the 'then_part'
 *       or 'else_part'. */

	if (get_token(hawk) <= -1) goto oops;

	tloc = hawk->tok.loc;
	then_part = parse_statement(hawk, &tloc);
	if (then_part == HAWK_NULL) goto oops;

	/* skip any new lines before the else block */
	while (MATCH(hawk,TOK_NEWLINE))
	{
		if (get_token(hawk) <= -1) goto oops;
	}

	if (MATCH(hawk,TOK_ELSE))
	{
		if (get_token(hawk) <= -1) goto oops;

		{
			hawk_loc_t eloc;
			eloc = hawk->tok.loc;
			else_part = parse_statement(hawk, &eloc);
			if (else_part == HAWK_NULL) goto oops;
		}
	}

	nde = (hawk_nde_if_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_IF;
	nde->loc = *xloc;
	nde->test = test;
	nde->then_part = then_part;
	nde->else_part = else_part;

	return (hawk_nde_t*)nde;

oops:
	if (else_part) hawk_clrpt(hawk, else_part);
	if (then_part) hawk_clrpt(hawk, then_part);
	if (test) hawk_clrpt(hawk, test);
	return HAWK_NULL;
}

static hawk_nde_case_t* alloc_nde_case (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_case_t* nde;

	nde = (hawk_nde_case_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_CASE;
	nde->loc = *xloc;
	nde->val = HAWK_NULL;
	nde->action = HAWK_NULL;

	return nde;

oops:
	return HAWK_NULL;
}

static hawk_nde_t* parse_switch (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_switch_t* nde;
	hawk_nde_t* test = HAWK_NULL; /* test to switch */
	hawk_nde_t* case_val = HAWK_NULL; /* value after case */
	hawk_nde_t* case_first = HAWK_NULL;
	hawk_nde_t* case_last = HAWK_NULL;
	hawk_nde_t* default_part = HAWK_NULL;
	hawk_loc_t eloc;

	if (!MATCH(hawk,TOK_LPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		return HAWK_NULL;
	}
	if (get_token(hawk) <= -1) return HAWK_NULL;

	eloc = hawk->tok.loc;
	test = parse_expr_withdc(hawk, &eloc);
	if (HAWK_UNLIKELY(!test)) goto oops;

	if (!MATCH(hawk,TOK_RPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1) goto oops;

	while (MATCH(hawk,TOK_NEWLINE))
	{
		if (get_token(hawk) <= -1) goto oops;
	}
	if (!MATCH(hawk,TOK_LBRACE))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELBRACE, FMT_ELBRACE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1) goto oops;

	while (!MATCH(hawk, TOK_RBRACE))
	{
		while (MATCH(hawk,TOK_NEWLINE))
		{
			if (get_token(hawk) <= -1) goto oops;
		}

		if (MATCH(hawk, TOK_CASE) || MATCH(hawk, TOK_DEFAULT))
		{
			hawk_nde_case_t* vv;
			hawk_nde_t* action_first = HAWK_NULL;
			hawk_nde_t* action_last = HAWK_NULL;

			if (MATCH(hawk, TOK_CASE))
			{
				if (get_token(hawk) <= -1) goto oops;
				eloc = hawk->tok.loc;
				case_val = parse_primary_literal(hawk, &eloc);
				if (HAWK_UNLIKELY(!case_val)) goto oops;
			}
			else /* MATCH(hawk, TOK_DEFAULT) */
			{
				if (default_part)
				{
					hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EMULDFL, FMT_EMULDFL, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
					goto oops;
				}
				case_val = HAWK_NULL;
				if (get_token(hawk) <= -1) goto oops;
			}

			if (!MATCH(hawk, TOK_COLON))
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_ECOLON, FMT_ECOLON, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}
			if (get_token(hawk) <= -1) goto oops;

			while (MATCH(hawk,TOK_NEWLINE))
			{
				if (get_token(hawk) <= -1) goto oops;
			}

			while (!MATCH(hawk, TOK_CASE) && !MATCH(hawk, TOK_DEFAULT) && !MATCH(hawk, TOK_RBRACE))
			{
				hawk_nde_t* v;

				eloc = hawk->tok.loc;
				hawk->parse.depth.swtch++;
				v = parse_statement(hawk, &eloc);
				hawk->parse.depth.swtch--;
				if (!v) {
					if (action_first) hawk_clrpt(hawk, action_first);
					goto oops;
				}

				if (!action_first)
				{
					action_first = v;
					action_last = v;
				}
				else if (v->type == HAWK_NDE_NULL && v->type == action_last->type)
				{
					/* skip a successive null statement */
					hawk_freemem(hawk, v);
				}
				else
				{
					action_last->next = v;
					action_last = v;
				}

				while (MATCH(hawk,TOK_NEWLINE))
				{
					if (get_token(hawk) <= -1) goto oops;
				}
			}

			vv = alloc_nde_case(hawk, &eloc);
			if (!vv)
			{
				if (action_first) hawk_clrpt(hawk, action_first);
				goto oops;
			}

			vv->val = case_val;
			vv->action = action_first;

			if (!case_val) default_part = (hawk_nde_t*)vv;
			else case_val = HAWK_NULL;

			if (!case_first)
			{
				case_first = (hawk_nde_t*)vv;
				case_last = (hawk_nde_t*)vv;
			}
			else
			{
				case_last->next = (hawk_nde_t*)vv;
				case_last = (hawk_nde_t*)vv;
			}
		}
		else
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EKWCASE, FMT_EKWCASE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			goto oops;
		}
	}

	while (MATCH(hawk,TOK_NEWLINE))
	{
		if (get_token(hawk) <= -1) goto oops;
	}
	if (!MATCH(hawk, TOK_RBRACE))
	{
		hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_ERBRACE, FMT_ERBRACE, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1) goto oops;


	nde = (hawk_nde_switch_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_SWITCH;
	nde->loc = *xloc;
	nde->test = test;
	nde->case_part = case_first;
	nde->default_part = default_part;

	return (hawk_nde_t*)nde;

oops:
	if (case_val) hawk_clrpt(hawk, case_val);
	if (case_first) hawk_clrpt(hawk, case_first);
	/*if (default_part) hawk_clrpt(hawk, default_part);  no need to crear it as it is in the case_first chanin */
	if (test) hawk_clrpt(hawk, test);
	return HAWK_NULL;
}

static hawk_nde_t* parse_while (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* test = HAWK_NULL;
	hawk_nde_t* body = HAWK_NULL;
	hawk_nde_while_t* nde;
	hawk_loc_t ploc;

	if (!MATCH(hawk,TOK_LPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}
	if (get_token(hawk) <= -1) goto oops;

	ploc = hawk->tok.loc;
	test = parse_expr_withdc(hawk, &ploc);
	if (HAWK_UNLIKELY(!test)) goto oops;

	if (!MATCH(hawk,TOK_RPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1)  goto oops;

	ploc = hawk->tok.loc;
	body = parse_statement(hawk, &ploc);
	if (HAWK_UNLIKELY(!body)) goto oops;

	nde = (hawk_nde_while_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_WHILE;
	nde->loc = *xloc;
	nde->test = test;
	nde->body = body;

	return (hawk_nde_t*)nde;

oops:
	if (body) hawk_clrpt(hawk, body);
	if (test) hawk_clrpt(hawk, test);
	return HAWK_NULL;
}

static hawk_nde_t* parse_for (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* init = HAWK_NULL, * test = HAWK_NULL;
	hawk_nde_t* incr = HAWK_NULL, * body = HAWK_NULL;
	hawk_nde_for_t* nde_for;
	hawk_nde_forin_t* nde_forin;
	hawk_loc_t ploc;

	if (!MATCH(hawk,TOK_LPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		return HAWK_NULL;
	}
	if (get_token(hawk) <= -1) return HAWK_NULL;

	if (!MATCH(hawk,TOK_SEMICOLON))
	{
		/* this line is very ugly. it checks the entire next
		 * expression or the first element in the expression
		 * is wrapped by a parenthesis */
		int no_forin = MATCH(hawk,TOK_LPAREN);

		ploc = hawk->tok.loc;
		init = parse_expr_withdc(hawk, &ploc);
		if (HAWK_UNLIKELY(!init)) goto oops;

		if (!no_forin && init->type == HAWK_NDE_EXP_BIN &&
		    ((hawk_nde_exp_t*)init)->opcode == HAWK_BINOP_IN &&
		#if defined(HAWK_ENABLE_GC)
		    is_var(((hawk_nde_exp_t*)init)->left))
		#else
		    is_plain_var(((hawk_nde_exp_t*)init)->left))
		#endif
		{
			/* switch to forin - for (x in y) */

			if (!MATCH(hawk,TOK_RPAREN))
			{
				hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}

			if (get_token(hawk) <= -1) goto oops;

			ploc = hawk->tok.loc;
			body = parse_statement(hawk, &ploc);
			if (HAWK_UNLIKELY(!body)) goto oops;

			nde_forin = (hawk_nde_forin_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde_forin));
			if (HAWK_UNLIKELY(!nde_forin))
			{
				ADJERR_LOC(hawk, xloc);
				goto oops;
			}

			nde_forin->type = HAWK_NDE_FORIN;
			nde_forin->loc = *xloc;
			nde_forin->test = init;
			nde_forin->body = body;

			return (hawk_nde_t*)nde_forin;
		}

		if (!MATCH(hawk,TOK_SEMICOLON))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ESCOLON, FMT_ESCOLON, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			goto oops;
		}
	}

	do
	{
		if (get_token(hawk) <= -1)  goto oops;
		/* skip new lines after the first semicolon */
	}
	while (MATCH(hawk,TOK_NEWLINE));

	if (!MATCH(hawk,TOK_SEMICOLON))
	{
		ploc = hawk->tok.loc;
		test = parse_expr_withdc(hawk, &ploc);
		if (HAWK_UNLIKELY(!test)) goto oops;

		if (!MATCH(hawk,TOK_SEMICOLON))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ESCOLON, FMT_ESCOLON, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			goto oops;
		}
	}

	do
	{
		if (get_token(hawk) <= -1) goto oops;
		/* skip new lines after the second semicolon */
	}
	while (MATCH(hawk,TOK_NEWLINE));

	if (!MATCH(hawk,TOK_RPAREN))
	{
		{
			hawk_loc_t eloc;
			eloc = hawk->tok.loc;
			incr = parse_expr_withdc(hawk, &eloc);
			if (HAWK_UNLIKELY(!incr)) goto oops;
		}

		if (!MATCH(hawk,TOK_RPAREN))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			goto oops;
		}
	}

	if (get_token(hawk) <= -1) goto oops;

	ploc = hawk->tok.loc;
	body = parse_statement(hawk, &ploc);
	if (body == HAWK_NULL) goto oops;

	nde_for = (hawk_nde_for_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde_for));
	if (HAWK_UNLIKELY(!nde_for))
	{
		ADJERR_LOC(hawk, xloc);
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
	if (init) hawk_clrpt(hawk, init);
	if (test) hawk_clrpt(hawk, test);
	if (incr) hawk_clrpt(hawk, incr);
	if (body) hawk_clrpt(hawk, body);
	return HAWK_NULL;
}

static hawk_nde_t* parse_dowhile (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* test = HAWK_NULL;
	hawk_nde_t* body = HAWK_NULL;
	hawk_nde_while_t* nde;
	hawk_loc_t ploc;

	HAWK_ASSERT(hawk->ptok.type == TOK_DO);

	ploc = hawk->tok.loc;
	body = parse_statement(hawk, &ploc);
	if (HAWK_UNLIKELY(!body)) goto oops;

	while (MATCH(hawk,TOK_NEWLINE))
	{
		if (get_token(hawk) <= -1) goto oops;
	}

	if (!MATCH(hawk,TOK_WHILE))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_EKWWHL, FMT_EKWWHL, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1) goto oops;

	if (!MATCH(hawk,TOK_LPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1) goto oops;

	ploc = hawk->tok.loc;
	test = parse_expr_withdc(hawk, &ploc);
	if (HAWK_UNLIKELY(!test)) goto oops;

	if (!MATCH(hawk,TOK_RPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	if (get_token(hawk) <= -1)  goto oops;

	nde = (hawk_nde_while_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_DOWHILE;
	nde->loc = *xloc;
	nde->test = test;
	nde->body = body;

	return (hawk_nde_t*)nde;

oops:
	if (body) hawk_clrpt(hawk, body);
	if (test) hawk_clrpt(hawk, test);
	HAWK_ASSERT(nde == HAWK_NULL);
	return HAWK_NULL;
}

static hawk_nde_t* parse_break (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_break_t* nde;

	HAWK_ASSERT(hawk->ptok.type == TOK_BREAK);
	if (hawk->parse.depth.loop <= 0 && hawk->parse.depth.swtch <= 0)
	{
		hawk_seterrnum(hawk, xloc, HAWK_EBREAK);
		return HAWK_NULL;
	}

	nde = (hawk_nde_break_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_BREAK;
	nde->loc = *xloc;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_continue (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_continue_t* nde;

	HAWK_ASSERT(hawk->ptok.type == TOK_CONTINUE);
	if (hawk->parse.depth.loop <= 0)
	{
		hawk_seterrnum(hawk, xloc, HAWK_ECONTINUE);
		return HAWK_NULL;
	}

	nde = (hawk_nde_continue_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_CONTINUE;
	nde->loc = *xloc;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_return (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_return_t* nde;
	hawk_nde_t* val;

	HAWK_ASSERT(hawk->ptok.type == TOK_RETURN);

	nde = (hawk_nde_return_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_RETURN;
	nde->loc = *xloc;

	if (MATCH_TERMINATOR(hawk))
	{
		/* no return value */
		val = HAWK_NULL;
	}
	else
	{
		hawk_loc_t eloc;

		eloc = hawk->tok.loc;
		val = parse_expr_withdc(hawk, &eloc);
		if (HAWK_UNLIKELY(!val))
		{
			hawk_freemem(hawk, nde);
			return HAWK_NULL;
		}
	}

	nde->val = val;
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_exit (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_exit_t* nde;
	hawk_nde_t* val;

	HAWK_ASSERT(hawk->ptok.type == TOK_EXIT || hawk->ptok.type == TOK_XABORT);

	nde = (hawk_nde_exit_t*) hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_EXIT;
	nde->loc = *xloc;
	nde->abort = (hawk->ptok.type == TOK_XABORT);

	if (MATCH_TERMINATOR(hawk))
	{
		/* no exit code */
		val = HAWK_NULL;
	}
	else
	{
		hawk_loc_t eloc;

		eloc = hawk->tok.loc;
		val = parse_expr_withdc(hawk, &eloc);
		if (val == HAWK_NULL)
		{
			hawk_freemem(hawk, nde);
			return HAWK_NULL;
		}
	}

	nde->val = val;
	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_next (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_next_t* nde;

	HAWK_ASSERT(hawk->ptok.type == TOK_NEXT);

	if (hawk->parse.id.block == PARSE_BEGIN_BLOCK)
	{
		hawk_seterrnum(hawk, xloc, HAWK_ENEXTBEG);
		return HAWK_NULL;
	}
	if (hawk->parse.id.block == PARSE_END_BLOCK)
	{
		hawk_seterrnum(hawk, xloc, HAWK_ENEXTEND);
		return HAWK_NULL;
	}

	nde = (hawk_nde_next_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}
	nde->type = HAWK_NDE_NEXT;
	nde->loc = *xloc;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_nextfile (hawk_t* hawk, const hawk_loc_t* xloc, int out)
{
	hawk_nde_nextfile_t* nde;

	if (!out && hawk->parse.id.block == PARSE_BEGIN_BLOCK)
	{
		hawk_seterrnum(hawk, xloc, HAWK_ENEXTFBEG);
		return HAWK_NULL;
	}
	if (!out && hawk->parse.id.block == PARSE_END_BLOCK)
	{
		hawk_seterrnum(hawk, xloc, HAWK_ENEXTFEND);
		return HAWK_NULL;
	}

	nde = (hawk_nde_nextfile_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_NEXTFILE;
	nde->loc = *xloc;
	nde->out = out;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_delete (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_delete_t* nde;
	hawk_nde_t* var = HAWK_NULL;
	hawk_loc_t dloc;
	hawk_nde_type_t type;
	int inparen = 0;

	HAWK_ASSERT(hawk->ptok.type == TOK_DELETE || hawk->ptok.type == TOK_XRESET);

	type = (hawk->ptok.type == TOK_DELETE)?  HAWK_NDE_DELETE: HAWK_NDE_RESET;

	if (MATCH(hawk,TOK_LPAREN))
	{
		if (get_token(hawk) <= -1) goto oops;
		inparen = 1;
	}

	if (!MATCH(hawk,TOK_IDENT))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_EIDENT, FMT_EIDENT, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	dloc = hawk->tok.loc;
	var = parse_primary_ident(hawk, &dloc);
	if (HAWK_UNLIKELY(!var)) goto oops;

	if ((type == HAWK_NDE_DELETE && !is_var(var)) ||
	    (type == HAWK_NDE_RESET && !is_plain_var(var)))
	{
		hawk_seterrnum(hawk, &dloc, HAWK_EBADARG);
		goto oops;
	}

	if (inparen)
	{
		if (!MATCH(hawk,TOK_RPAREN))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			goto oops;
		}

		if (get_token(hawk) <= -1) goto oops;
	}

	nde = (hawk_nde_delete_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = type;
	nde->loc = *xloc;
	nde->var = var;

	return (hawk_nde_t*)nde;

oops:
	if (var) hawk_clrpt(hawk, var);
	return HAWK_NULL;
}

static hawk_nde_t* parse_print (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_print_t* nde;
	hawk_nde_t* args = HAWK_NULL;
	hawk_nde_t* out = HAWK_NULL;
	hawk_out_type_t out_type;
	hawk_nde_type_t type;
	hawk_loc_t eloc;

	HAWK_ASSERT(hawk->ptok.type == TOK_PRINT || hawk->ptok.type == TOK_PRINTF);

	type = (hawk->ptok.type == TOK_PRINT)? HAWK_NDE_PRINT: HAWK_NDE_PRINTF;

	if (!MATCH_TERMINATOR(hawk) && !MATCH(hawk,TOK_GT) &&
	    !MATCH(hawk,TOK_RS) && !MATCH(hawk,TOK_BOR) && !MATCH(hawk,TOK_PIPE_BD))
	{
		hawk_nde_t* args_tail;
		hawk_nde_t* tail_prev;

		int in_parens = 0, gm_in_parens = 0;
		hawk_oow_t opening_lparen_seq;

		if (MATCH(hawk,TOK_LPAREN))
		{
			/* just remember the sequence number of the left
			 * parenthesis before calling parse_expr_withdc()
			 * that eventually calls parse_primary_lparen() */
			opening_lparen_seq = hawk->parse.lparen_seq;
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

		eloc = hawk->tok.loc;
		args = parse_expr_withdc(hawk, &eloc);
		if (args == HAWK_NULL) goto oops;

		args_tail = args;
		tail_prev = HAWK_NULL;

		if (args->type != HAWK_NDE_GRP)
		{
			/* args->type == HAWK_NDE_GRP when print (a, b, c)
			 * args->type != HAWK_NDE_GRP when print a, b, c */
			hawk_oow_t group_opening_lparen_seq;

			while (MATCH(hawk,TOK_COMMA))
			{
				do
				{
					if (get_token(hawk) <= -1) goto oops;
				}
				while (MATCH(hawk,TOK_NEWLINE));

				/* if it's grouped, i must check if the last group member
				 * is enclosed in parentheses.
				 *
				 * i set the condition to false whenever i see
				 * a new group member. */
				gm_in_parens = 0;
				if (MATCH(hawk,TOK_LPAREN))
				{
					group_opening_lparen_seq = hawk->parse.lparen_seq;
					gm_in_parens = 1; /* maybe */
				}

				eloc = hawk->tok.loc;
				args_tail->next = parse_expr_withdc(hawk, &eloc);
				if (args_tail->next == HAWK_NULL) goto oops;

				tail_prev = args_tail;
				args_tail = args_tail->next;

				if (gm_in_parens == 1 && hawk->ptok.type == TOK_RPAREN &&
				    hawk->parse.lparen_last_closed == group_opening_lparen_seq)
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
		if (in_parens == 1 && hawk->ptok.type == TOK_RPAREN && (hawk->ptok.flags & TOK_FLAGS_LPAREN_CLOSER) &&
		    hawk->parse.lparen_last_closed == opening_lparen_seq)
		{
			in_parens = 2; /* confirmed */
		}

		if (in_parens != 2 && gm_in_parens != 2 && args_tail->type == HAWK_NDE_EXP_BIN)
		{
			/* check if the expression ends with an output redirector
			 * and take out the part after the redirector and make it
			 * the output target */

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
				/* |& doesn't oeverlap with other binary operators.
				{
					HAWK_BINOP_PIPE_BD,
					HAWK_OUT_RWPIPE,
					HAWK_RWPIPE
				}
				*/
			};

			for (i = 0; i < HAWK_COUNTOF(tab); i++)
			{
				if (ep->opcode == tab[i].opc)
				{
					hawk_nde_t* tmp;

					if (tab[i].opt && !(hawk->opt.trait & tab[i].opt)) break;

					tmp = args_tail;

					if (tail_prev) tail_prev->next = ep->left;
					else args = ep->left;

					out = ep->right;
					out_type = tab[i].out;
					hawk_freemem(hawk, tmp);
					break;
				}
			}
		}
	}

	if (!out)
	{
		out_type = MATCH(hawk,TOK_GT)?        HAWK_OUT_FILE:
		           MATCH(hawk,TOK_RS)?        HAWK_OUT_APFILE:
		           MATCH(hawk,TOK_BOR)?       HAWK_OUT_PIPE:
		           ((hawk->opt.trait & HAWK_RWPIPE) &&
		            MATCH(hawk,TOK_PIPE_BD))? HAWK_OUT_RWPIPE:
		                                      HAWK_OUT_CONSOLE;

		if (out_type != HAWK_OUT_CONSOLE)
		{
			if (get_token(hawk) <= -1) goto oops;

			eloc = hawk->tok.loc;
			out = parse_expr_withdc(hawk, &eloc);
			if (out == HAWK_NULL) goto oops;
		}
	}

	if (type == HAWK_NDE_PRINTF && !args)
	{
		hawk_seterrnum(hawk, xloc, HAWK_ENOARG);
		goto oops;
	}

	nde = (hawk_nde_print_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = type;
	nde->loc = *xloc;
	nde->args = args;
	nde->out_type = out_type;
	nde->out = out;

	return (hawk_nde_t*)nde;

oops:
	if (args) hawk_clrpt(hawk, args);
	if (out) hawk_clrpt(hawk, out);
	return HAWK_NULL;
}

static hawk_nde_t* parse_statement_nb (hawk_t* hawk, const hawk_loc_t* xloc)
{
	/* parse a non-block statement */
	hawk_nde_t* nde;

	/* keywords that don't require any terminating semicolon */
	if (MATCH(hawk,TOK_IF))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		return parse_if(hawk, xloc);
	}
	else if (MATCH(hawk, TOK_SWITCH))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		return parse_switch(hawk, xloc);
	}
	else if (MATCH(hawk,TOK_WHILE))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;

		hawk->parse.depth.loop++;
		nde = parse_while(hawk, xloc);
		hawk->parse.depth.loop--;

		return nde;
	}
	else if (MATCH(hawk,TOK_FOR))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;

		hawk->parse.depth.loop++;
		nde = parse_for(hawk, xloc);
		hawk->parse.depth.loop--;

		return nde;
	}

	/* keywords that require a terminating semicolon */
	if (MATCH(hawk,TOK_DO))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;

		hawk->parse.depth.loop++;
		nde = parse_dowhile(hawk, xloc);
		hawk->parse.depth.loop--;

		return nde;
	}
	else if (MATCH(hawk,TOK_BREAK))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_break(hawk, xloc);
	}
	else if (MATCH(hawk,TOK_CONTINUE))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_continue(hawk, xloc);
	}
	else if (MATCH(hawk,TOK_RETURN))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_return(hawk, xloc);
	}
	else if (MATCH(hawk,TOK_EXIT) || MATCH(hawk,TOK_XABORT))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_exit(hawk, xloc);
	}
	else if (MATCH(hawk,TOK_NEXT))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_next(hawk, xloc);
	}
	else if (MATCH(hawk,TOK_NEXTFILE))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_nextfile(hawk, xloc, 0);
	}
	else if (MATCH(hawk,TOK_NEXTOFILE))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_nextfile(hawk, xloc, 1);
	}
	else if (MATCH(hawk,TOK_DELETE) || MATCH(hawk,TOK_XRESET))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_delete(hawk, xloc);
	}
	else if (!(hawk->opt.trait & HAWK_TOLERANT))
	{
		/* in the non-tolerant mode, we treat print and printf
		 * as a separate statement */
		if (MATCH(hawk,TOK_PRINT) || MATCH(hawk,TOK_PRINTF))
		{
			if (get_token(hawk) <= -1) return HAWK_NULL;
			nde = parse_print(hawk, xloc);
		}
		else nde = parse_expr_withdc(hawk, xloc);
	}
	else
	{
		nde = parse_expr_withdc(hawk, xloc);
	}

	if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;

	if (MATCH_TERMINATOR_NORMAL(hawk))
	{
		/* check if a statement ends with a semicolon or <NL> */
		if (get_token(hawk) <= -1)
		{
			if (nde) hawk_clrpt(hawk, nde);
			return HAWK_NULL;
		}
	}
	else if (MATCH_TERMINATOR_RBRACE(hawk))
	{
		/* do not skip the right brace as a statement terminator.
		 * is there anything to do here? */
	}
	else
	{
		if (nde) hawk_clrpt(hawk, nde);
		hawk_seterrnum(hawk, &hawk->ptok.loc, HAWK_ESTMEND);
		return HAWK_NULL;
	}

	return nde;
}

static hawk_nde_t* parse_statement (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde;

	/* skip new lines before a statement */
	while (MATCH(hawk,TOK_NEWLINE))
	{
		if (get_token(hawk) <= -1) return HAWK_NULL;
	}

	if (MATCH(hawk,TOK_SEMICOLON))
	{
		/* null statement */
		nde = (hawk_nde_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
		if (HAWK_UNLIKELY(!nde))
		{
			ADJERR_LOC(hawk, xloc);
			return HAWK_NULL;
		}

		nde->type = HAWK_NDE_NULL;
		nde->loc = *xloc;
		nde->next = HAWK_NULL;

		if (get_token(hawk) <= -1)
		{
			hawk_freemem(hawk, nde);
			return HAWK_NULL;
		}
	}
	else if (MATCH(hawk,TOK_LBRACE))
	{
		/* a block statemnt { ... } */
		hawk_loc_t tloc;

		tloc = hawk->ptok.loc;
		if (get_token(hawk) <= -1) return HAWK_NULL;
		nde = parse_block_dc(hawk, &tloc, 0);
	}
	else
	{
		/* the statement id held in hawk->parse.id.stmt denotes
		 * the token id of the statement currently being parsed.
		 * the current statement id is saved here because the
		 * statement id can be changed in parse_statement_nb.
		 * it will, in turn, call parse_statement which will
		 * eventually change the statement id. */
		int old_id;
		hawk_loc_t tloc;

		old_id = hawk->parse.id.stmt;
		tloc = hawk->tok.loc;

		/* set the current statement id */
		hawk->parse.id.stmt = hawk->tok.type;

		/* proceed parsing the statement */
		nde = parse_statement_nb(hawk, &tloc);

		/* restore the statement id saved previously */
		hawk->parse.id.stmt = old_id;
	}

	return nde;
}

static int assign_to_opcode (hawk_t* hawk)
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

	if (hawk->tok.type >= TOK_ASSN &&
	    hawk->tok.type <= TOK_BOR_ASSN)
	{
		return assop[hawk->tok.type - TOK_ASSN];
	}

	return -1;
}

static hawk_nde_t* parse_expr_basic (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde, * n1, * n2;

	nde = parse_logical_or(hawk, xloc);
	if (nde == HAWK_NULL) return HAWK_NULL;

	if (MATCH(hawk,TOK_QUEST))
	{
		hawk_loc_t eloc;
		hawk_nde_cnd_t* cnd;

		if (get_token(hawk) <= -1)
		{
			hawk_clrpt(hawk, nde);
			return HAWK_NULL;
		}

		eloc = hawk->tok.loc;
		n1 = parse_expr_withdc(hawk, &eloc);
		if (n1 == HAWK_NULL)
		{
			hawk_clrpt(hawk, nde);
			return HAWK_NULL;
		}

		if (!MATCH(hawk,TOK_COLON))
		{
			hawk_clrpt(hawk, nde);
			hawk_clrpt(hawk, n1);
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ECOLON, FMT_ECOLON, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			return HAWK_NULL;
		}
		if (get_token(hawk) <= -1)
		{
			hawk_clrpt(hawk, nde);
			hawk_clrpt(hawk, n1);
			return HAWK_NULL;
		}

		eloc = hawk->tok.loc;
		n2 = parse_expr_withdc(hawk, &eloc);
		if (n2 == HAWK_NULL)
		{
			hawk_clrpt(hawk, nde);
			hawk_clrpt(hawk, n1);
			return HAWK_NULL;
		}

		cnd = (hawk_nde_cnd_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*cnd));
		if (cnd == HAWK_NULL)
		{
			hawk_clrpt(hawk, nde);
			hawk_clrpt(hawk, n1);
			hawk_clrpt(hawk, n2);
			ADJERR_LOC(hawk, xloc);
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

static hawk_nde_t* parse_expr (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* x, * y;
	hawk_nde_ass_t* nde;
	int opcode;

	x = parse_expr_basic(hawk, xloc);
	if (x == HAWK_NULL) return HAWK_NULL;

	opcode = assign_to_opcode(hawk);
	if (opcode <= -1)
	{
		/* no assignment operator found. */
		return x;
	}

	HAWK_ASSERT(x->next == HAWK_NULL);
	if (!is_var(x) && x->type != HAWK_NDE_POS)
	{
		hawk_clrpt(hawk, x);
		hawk_seterrnum(hawk, xloc, HAWK_EASSIGN);
		return HAWK_NULL;
	}

	if (get_token(hawk) <= -1)
	{
		hawk_clrpt(hawk, x);
		return HAWK_NULL;
	}

	{
		hawk_loc_t eloc;
		eloc = hawk->tok.loc;
		y = parse_expr_withdc(hawk, &eloc);
	}
	if (y == HAWK_NULL)
	{
		hawk_clrpt(hawk, x);
		return HAWK_NULL;
	}

	nde = (hawk_nde_ass_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		hawk_clrpt(hawk, x);
		hawk_clrpt(hawk, y);
		ADJERR_LOC(hawk, xloc);
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

static hawk_nde_t* parse_expr_withdc (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde;

	/* perform depth check before parsing expression */

	if (hawk->opt.depth.s.expr_parse > 0 &&
	    hawk->parse.depth.expr >= hawk->opt.depth.s.expr_parse)
	{
		hawk_seterrnum(hawk, xloc, HAWK_EEXPRNST);
		return HAWK_NULL;
	}

	hawk->parse.depth.expr++;
	nde = parse_expr(hawk, xloc);
	hawk->parse.depth.expr--;

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
	hawk_t* hawk, hawk_nde_t* left, hawk_nde_t* right,
	int opcode, folded_t* folded)
{
	int fold = -1;

	/* TODO: can i shorten various comparisons below?
 	 *       i hate to repeat similar code just for type difference */

	if (left->type == HAWK_NDE_INT && right->type == HAWK_NDE_INT)
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
					hawk_seterrnum(hawk, HAWK_NULL, HAWK_EDIVBY0);
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
					hawk_seterrnum(hawk, HAWK_NULL, HAWK_EDIVBY0);
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
	else if (left->type == HAWK_NDE_FLT && right->type == HAWK_NDE_FLT)
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
				folded->r = hawk->prm.math.mod(
					hawk,
					((hawk_nde_flt_t*)left)->val,
					((hawk_nde_flt_t*)right)->val
				);
				break;

			default:
				fold = -1;
				break;
		}
	}
	else if (left->type == HAWK_NDE_INT && right->type == HAWK_NDE_FLT)
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
				folded->r = hawk->prm.math.mod(
					hawk,
					(hawk_flt_t)((hawk_nde_int_t*)left)->val,
					((hawk_nde_flt_t*)right)->val
				);
				break;

			default:
				fold = -1;
				break;
		}
	}
	else if (left->type == HAWK_NDE_FLT && right->type == HAWK_NDE_INT)
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
				folded->r = hawk->prm.math.mod(
					hawk,
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
	hawk_t* hawk, const hawk_loc_t* loc,
	int opcode, hawk_nde_t* left, hawk_nde_t* right)
{
	hawk_nde_exp_t* tmp;

	tmp = (hawk_nde_exp_t*) hawk_callocmem(hawk, HAWK_SIZEOF(*tmp));
	if (tmp)
	{
		tmp->type = HAWK_NDE_EXP_BIN;
		tmp->loc = *loc;
		tmp->opcode = opcode;
		tmp->left = left;
		tmp->right = right;
	}
	else ADJERR_LOC(hawk, loc);

	return (hawk_nde_t*)tmp;
}

static hawk_nde_t* new_int_node (hawk_t* hawk, hawk_int_t lv, const hawk_loc_t* loc)
{
	hawk_nde_int_t* tmp;

	tmp = (hawk_nde_int_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*tmp));
	if (HAWK_LIKELY(tmp))
	{
		tmp->type = HAWK_NDE_INT;
		tmp->loc = *loc;
		tmp->val = lv;
	}
	else ADJERR_LOC(hawk, loc);

	return (hawk_nde_t*)tmp;
}

static hawk_nde_t* new_flt_node (hawk_t* hawk, hawk_flt_t rv, const hawk_loc_t* loc)
{
	hawk_nde_flt_t* tmp;

	tmp = (hawk_nde_flt_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*tmp));
	if (HAWK_LIKELY(tmp))
	{
		tmp->type = HAWK_NDE_FLT;
		tmp->loc = *loc;
		tmp->val = rv;
	}
	else ADJERR_LOC(hawk, loc);

	return (hawk_nde_t*)tmp;
}

static HAWK_INLINE void update_int_node (hawk_t* hawk, hawk_nde_int_t* node, hawk_int_t lv)
{
	node->val = lv;
	if (node->str)
	{
		hawk_freemem(hawk, node->str);
		node->str = HAWK_NULL;
		node->len = 0;
	}
}

static HAWK_INLINE void update_flt_node (hawk_t* hawk, hawk_nde_flt_t* node, hawk_flt_t rv)
{
	node->val = rv;
	if (node->str)
	{
		hawk_freemem(hawk, node->str);
		node->str = HAWK_NULL;
		node->len = 0;
	}
}

static hawk_nde_t* parse_binary (
	hawk_t* hawk, const hawk_loc_t* xloc,
	int skipnl, const binmap_t* binmap,
	hawk_nde_t*(*next_level_func)(hawk_t*,const hawk_loc_t*))
{
	hawk_nde_t* left = HAWK_NULL;
	hawk_nde_t* right = HAWK_NULL;
	hawk_loc_t rloc;

	left = next_level_func(hawk, xloc);
	if (HAWK_UNLIKELY(!left)) goto oops;

	do
	{
		const binmap_t* p = binmap;
		int matched = 0;
		int opcode, fold;
		folded_t folded;

		while (p->token != TOK_EOF)
		{
			if (MATCH(hawk,p->token))
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
			if (get_token(hawk) <= -1) goto oops;
		}
		while (skipnl && MATCH(hawk,TOK_NEWLINE));

		rloc = hawk->tok.loc;
		right = next_level_func(hawk, &rloc);
		if (right == HAWK_NULL) goto oops;

		fold = fold_constants_for_binop(hawk, left, right, opcode, &folded);
		switch (fold)
		{
			case HAWK_NDE_INT:
				if (fold == left->type)
				{
					hawk_clrpt(hawk, right);
					right = HAWK_NULL;
					update_int_node(hawk, (hawk_nde_int_t*)left, folded.l);
				}
				else if (fold == right->type)
				{
					hawk_clrpt(hawk, left);
					update_int_node(hawk, (hawk_nde_int_t*)right, folded.l);
					left = right;
					right = HAWK_NULL;
				}
				else
				{
					hawk_clrpt(hawk, right); right = HAWK_NULL;
					hawk_clrpt(hawk, left); left = HAWK_NULL;

					left = new_int_node(hawk, folded.l, xloc);
					if (left == HAWK_NULL) goto oops;
				}

				break;

			case HAWK_NDE_FLT:
				if (fold == left->type)
				{
					hawk_clrpt(hawk, right);
					right = HAWK_NULL;
					update_flt_node(hawk, (hawk_nde_flt_t*)left, folded.r);
				}
				else if (fold == right->type)
				{
					hawk_clrpt(hawk, left);
					update_flt_node(hawk, (hawk_nde_flt_t*)right, folded.r);
					left = right;
					right = HAWK_NULL;
				}
				else
				{
					hawk_clrpt(hawk, right); right = HAWK_NULL;
					hawk_clrpt(hawk, left); left = HAWK_NULL;

					left = new_flt_node(hawk, folded.r, xloc);
					if (left == HAWK_NULL) goto oops;
				}

				break;

			case -2:
				goto oops;

			default:
			{
				hawk_nde_t* tmp;

				tmp = new_exp_bin_node(hawk, xloc, opcode, left, right);
				if (tmp == HAWK_NULL) goto oops;
				left = tmp; right = HAWK_NULL;
				break;
			}
		}
	}
	while (1);

	return left;

oops:
	if (right) hawk_clrpt(hawk, right);
	if (left) hawk_clrpt(hawk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_logical_or (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_LOR, HAWK_BINOP_LOR },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 1, map, parse_logical_and);
}

static hawk_nde_t* parse_logical_and (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_LAND, HAWK_BINOP_LAND },
		{ TOK_EOF,  0 }
	};

	return parse_binary(hawk, xloc, 1, map, parse_in);
}

static hawk_nde_t* parse_in (hawk_t* hawk, const hawk_loc_t* xloc)
{
	/*
	static binmap_t map[] =
	{
		{ TOK_IN, HAWK_BINOP_IN },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_regex_match);
	*/

	hawk_nde_t* left = HAWK_NULL;
	hawk_nde_t* right = HAWK_NULL;
	hawk_loc_t rloc;

	left = parse_regex_match(hawk, xloc);
	if (left == HAWK_NULL) goto oops;

	do
	{
		hawk_nde_t* tmp;

		if (!MATCH(hawk,TOK_IN)) break;

		if (get_token(hawk) <= -1) goto oops;

		rloc = hawk->tok.loc;
		right = parse_regex_match(hawk, &rloc);
		if (HAWK_UNLIKELY(!right))  goto oops;

	#if defined(HAWK_ENABLE_GC)
		if (!is_var(right) && right->type != HAWK_NDE_XARGV)
	#else
		if (!is_plain_var(right) && right->type != HAWK_NDE_XARGV)
	#endif
		{
			hawk_seterrnum(hawk, &rloc, HAWK_ENOTVAR);
			goto oops;
		}

		tmp = new_exp_bin_node(hawk, xloc, HAWK_BINOP_IN, left, right);
		if (HAWK_UNLIKELY(!left)) goto oops;

		left = tmp;
		right = HAWK_NULL;
	}
	while (1);

	return left;

oops:
	if (right) hawk_clrpt(hawk, right);
	if (left) hawk_clrpt(hawk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_regex_match (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_TILDE,  HAWK_BINOP_MA },
		{ TOK_NM,  HAWK_BINOP_NM },
		{ TOK_EOF, 0 },
	};

	return parse_binary(hawk, xloc, 0, map, parse_bitwise_or);
}

static hawk_nde_t* parse_bitwise_or (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_BOR, HAWK_BINOP_BOR },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_bitwise_xor);
}

static hawk_nde_t* parse_bitwise_xor (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_BXOR, HAWK_BINOP_BXOR },
		{ TOK_EOF,  0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_bitwise_and);
}

static hawk_nde_t* parse_bitwise_and (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_BAND, HAWK_BINOP_BAND },
		{ TOK_EOF,  0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_equality);
}

static hawk_nde_t* parse_equality (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_TEQ, HAWK_BINOP_TEQ },
		{ TOK_TNE, HAWK_BINOP_TNE },
		{ TOK_EQ, HAWK_BINOP_EQ },
		{ TOK_NE, HAWK_BINOP_NE },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_relational);
}

static hawk_nde_t* parse_relational (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_GT, HAWK_BINOP_GT },
		{ TOK_GE, HAWK_BINOP_GE },
		{ TOK_LT, HAWK_BINOP_LT },
		{ TOK_LE, HAWK_BINOP_LE },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_shift);
}

static hawk_nde_t* parse_shift (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_LS, HAWK_BINOP_LS },
		{ TOK_RS, HAWK_BINOP_RS },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_concat);
}

static hawk_nde_t* parse_concat (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* left = HAWK_NULL;
	hawk_nde_t* right = HAWK_NULL;
	hawk_loc_t rloc;

	left = parse_additive(hawk, xloc);
	if (HAWK_UNLIKELY(!left)) goto oops;

	do
	{
		hawk_nde_t* tmp;

		if (MATCH(hawk,TOK_CONCAT))
		{
			if (get_token(hawk) <= -1) goto oops;
		}
		else if (hawk->opt.trait & HAWK_BLANKCONCAT)
		{
			/*
			 * [NOTE]
			 * TOK_TILDE has been commented out in the if condition below because
			 * BINOP_MA has lower precedence than concatenation and it is not certain
			 * the tilde is an unary bitwise negation operator at this phase.
			 * You may use (~10) rather than ~10 after concatenation to avoid confusion.
			 */
			if (MATCH(hawk,TOK_LPAREN) || MATCH(hawk,TOK_DOLLAR) ||
			   /* unary operators */
			   MATCH(hawk,TOK_PLUS) || MATCH(hawk,TOK_MINUS) ||
			   MATCH(hawk,TOK_LNOT) ||/* MATCH(hawk,TOK_TILDE) ||*/
			   /* increment operators */
			   MATCH(hawk,TOK_PLUSPLUS) || MATCH(hawk,TOK_MINUSMINUS) ||
			   ((hawk->opt.trait & HAWK_TOLERANT) &&
			    (hawk->tok.type == TOK_PRINT || hawk->tok.type == TOK_PRINTF)) ||
			   hawk->tok.type >= TOK_GETLINE)
			{
				/* proceed to handle concatenation expression */
				/* nothing to to here. just fall through */
			}
			else break;
		}
		else break;

		rloc = hawk->tok.loc;
		right = parse_additive(hawk, &rloc);
		if (HAWK_UNLIKELY(!right)) goto oops;

		tmp = new_exp_bin_node(hawk, xloc, HAWK_BINOP_CONCAT, left, right);
		if (HAWK_UNLIKELY(!tmp)) goto oops;
		left = tmp; right = HAWK_NULL;
	}
	while (1);

	return left;

oops:
	if (right) hawk_clrpt(hawk, right);
	if (left) hawk_clrpt(hawk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_additive (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_PLUS, HAWK_BINOP_PLUS },
		{ TOK_MINUS, HAWK_BINOP_MINUS },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_multiplicative);
}

static hawk_nde_t* parse_multiplicative (hawk_t* hawk, const hawk_loc_t* xloc)
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

	return parse_binary(hawk, xloc, 0, map, parse_unary);
}

static hawk_nde_t* parse_unary (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* left;
	hawk_loc_t uloc;
	int opcode;
	int fold;
	folded_t folded;

	opcode = (MATCH(hawk,TOK_PLUS))?  HAWK_UNROP_PLUS:
	         (MATCH(hawk,TOK_MINUS))? HAWK_UNROP_MINUS:
	         (MATCH(hawk,TOK_LNOT))?  HAWK_UNROP_LNOT:
	         (MATCH(hawk,TOK_TILDE))? HAWK_UNROP_BNOT: -1;

	/*if (opcode <= -1) return parse_increment(hawk);*/
	if (opcode <= -1) return parse_exponent(hawk, xloc);

	if (hawk->opt.depth.s.expr_parse > 0 &&
	    hawk->parse.depth.expr >= hawk->opt.depth.s.expr_parse)
	{
		hawk_seterrnum(hawk, xloc, HAWK_EEXPRNST);
		return HAWK_NULL;
	}

	if (get_token(hawk) <= -1) return HAWK_NULL;

	hawk->parse.depth.expr++;
	uloc = hawk->tok.loc;
	left = parse_unary(hawk, &uloc);
	hawk->parse.depth.expr--;
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
				update_int_node(hawk, (hawk_nde_int_t*)left, folded.l);
				return left;
			}
			else
			{
				HAWK_ASSERT(left->type == HAWK_NDE_FLT);
				hawk_clrpt(hawk, left);
				return new_int_node(hawk, folded.l, xloc);
			}

		case HAWK_NDE_FLT:
			if (left->type == fold)
			{
				update_flt_node(hawk, (hawk_nde_flt_t*)left, folded.r);
				return left;
			}
			else
			{
				HAWK_ASSERT(left->type == HAWK_NDE_INT);
				hawk_clrpt(hawk, left);
				return new_flt_node(hawk, folded.r, xloc);
			}

		default:
		{
			hawk_nde_exp_t* nde;

			nde = (hawk_nde_exp_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
			if (nde == HAWK_NULL)
			{
				hawk_clrpt(hawk, left);
				ADJERR_LOC(hawk, xloc);
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

static hawk_nde_t* parse_exponent (hawk_t* hawk, const hawk_loc_t* xloc)
{
	static binmap_t map[] =
	{
		{ TOK_EXP, HAWK_BINOP_EXP },
		{ TOK_EOF, 0 }
	};

	return parse_binary(hawk, xloc, 0, map, parse_unary_exp);
}

static hawk_nde_t* parse_unary_exp (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_exp_t* nde;
	hawk_nde_t* left;
	hawk_loc_t uloc;
	int opcode;

	opcode = (MATCH(hawk,TOK_PLUS))?  HAWK_UNROP_PLUS:
	         (MATCH(hawk,TOK_MINUS))? HAWK_UNROP_MINUS:
	         (MATCH(hawk,TOK_LNOT))?  HAWK_UNROP_LNOT:
	         (MATCH(hawk,TOK_TILDE))? HAWK_UNROP_BNOT: -1; /* ~ in the unary context is a bitwise-not operator */

	if (opcode <= -1) return parse_increment(hawk, xloc);

	if (hawk->opt.depth.s.expr_parse > 0 &&
	    hawk->parse.depth.expr >= hawk->opt.depth.s.expr_parse)
	{
		hawk_seterrnum(hawk, xloc, HAWK_EEXPRNST);
		return HAWK_NULL;
	}

	if (get_token(hawk) <= -1) return HAWK_NULL;

	hawk->parse.depth.expr++;
	uloc = hawk->tok.loc;
	left = parse_unary(hawk, &uloc);
	hawk->parse.depth.expr--;
	if (left == HAWK_NULL) return HAWK_NULL;

	nde = (hawk_nde_exp_t*) hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		hawk_clrpt(hawk, left);
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_EXP_UNR;
	nde->loc = *xloc;
	nde->opcode = opcode;
	nde->left = left;
	nde->right = HAWK_NULL;

	return (hawk_nde_t*)nde;
}

static hawk_nde_t* parse_increment (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_exp_t* nde;
	hawk_nde_t* left;
	int type, opcode, opcode1, opcode2;
	hawk_loc_t ploc;

	/* check for prefix increment operator */
	opcode1 = MATCH(hawk,TOK_PLUSPLUS)? HAWK_INCOP_PLUS:
	          MATCH(hawk,TOK_MINUSMINUS)? HAWK_INCOP_MINUS: -1;

	if (opcode1 != -1)
	{
		/* there is a prefix increment operator */
		if (get_token(hawk) <= -1) return HAWK_NULL;
	}

	ploc = hawk->tok.loc;
	left = parse_primary(hawk, &ploc);
	if (HAWK_UNLIKELY(!left)) return HAWK_NULL;

	/* check for postfix increment operator */
	opcode2 = MATCH(hawk,TOK_PLUSPLUS)? HAWK_INCOP_PLUS:
	          MATCH(hawk,TOK_MINUSMINUS)? HAWK_INCOP_MINUS: -1;

	if (!(hawk->opt.trait & HAWK_BLANKCONCAT))
	{
		if (opcode1 != -1 && opcode2 != -1)
		{
			/* both prefix and postfix increment operator.
			 * not allowed */
			hawk_clrpt(hawk, left);
			hawk_seterrnum(hawk, xloc, HAWK_EPREPST);
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
		HAWK_ASSERT(opcode2 != -1);

		/* postfix increment operator */
		type = HAWK_NDE_EXP_INCPST;
		opcode = opcode2;

		/* let's do it later
		if (get_token(hawk) <= -1)
		{
			hawk_clrpt(hawk, left);
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
			hawk_clrpt(hawk, left);
			hawk_seterrnum(hawk, xloc, HAWK_EINCDECOPR);
			return HAWK_NULL;
		}
	}

	if (type == HAWK_NDE_EXP_INCPST)
	{
		/* consume the postfix operator */
		if (get_token(hawk) <= -1)
		{
			hawk_clrpt(hawk, left);
			return HAWK_NULL;
		}
	}

	nde = (hawk_nde_exp_t*) hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		hawk_clrpt(hawk, left);
		ADJERR_LOC(hawk, xloc);
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

static HAWK_INLINE int isfunname (hawk_t* hawk, const hawk_oocs_t* name, hawk_fun_t** fun)
{
	hawk_htb_pair_t* pair;

	/* check if it is an hawk function being processed currently */
	if (hawk->tree.cur_fun.ptr)
	{
		if (hawk_comp_oochars(hawk->tree.cur_fun.ptr, hawk->tree.cur_fun.len, name->ptr, name->len, 0) == 0)
		{
			/* the current function begin parsed */
			return FNTYPE_FUN;
		}
	}

	/* check the funtion name in the function table */
	pair = hawk_htb_search(hawk->tree.funs, name->ptr, name->len);
	if (pair)
	{
		/* one of the functions defined previously */
		if (fun)
		{
			*fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
			HAWK_ASSERT(*fun != HAWK_NULL);
		}
		return FNTYPE_FUN;
	}

	/* check if it is a function not resolved so far */
	if (hawk_htb_search(hawk->parse.funs, name->ptr, name->len))
	{
		/* one of the function calls not resolved so far. */
		return FNTYPE_FUN;
	}

	return FNTYPE_UNKNOWN;
}

static HAWK_INLINE int isfnname (hawk_t* hawk, const hawk_oocs_t* name)
{
	if (hawk_findfncwithoocs(hawk, name) != HAWK_NULL)
	{
		/* implicit function */
		return FNTYPE_FNC;
	}

	return isfunname(hawk, name, HAWK_NULL);
}

static hawk_nde_t* parse_primary_char  (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_char_t* nde;

	nde = (hawk_nde_char_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_CHAR;
	nde->loc = *xloc;
	nde->val = HAWK_OOECS_CHAR(hawk->tok.name, 0);

	if (get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_bchr  (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_bchr_t* nde;

	nde = (hawk_nde_bchr_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_BCHR;
	nde->loc = *xloc;
	nde->val = HAWK_OOECS_CHAR(hawk->tok.name, 0);

	if (get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_int (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_int_t* nde;

	/* create the node for the literal */
	nde = (hawk_nde_int_t*)new_int_node(
		hawk,
		hawk_oochars_to_int (HAWK_OOECS_PTR(hawk->tok.name), HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOCHARS_TO_INT_MAKE_OPTION(0, 0, 0), HAWK_NULL, HAWK_NULL),
		xloc
	);
	if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;

	HAWK_ASSERT(HAWK_OOECS_LEN(hawk->tok.name) == hawk_count_oocstr(HAWK_OOECS_PTR(hawk->tok.name)));

	/* remember the literal in the original form */
	nde->len = HAWK_OOECS_LEN(hawk->tok.name);
	nde->str = hawk_dupoocs(hawk, HAWK_OOECS_OOCS(hawk->tok.name));
	if (!nde->str || get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	if (nde->str) hawk_freemem(hawk, nde->str);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_flt (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_flt_t* nde;

	/* create the node for the literal */
	nde = (hawk_nde_flt_t*)new_flt_node(
		hawk,
		hawk_oochars_to_flt(HAWK_OOECS_PTR(hawk->tok.name), HAWK_OOECS_LEN(hawk->tok.name), HAWK_NULL, 0),
		xloc
	);
	if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;

	HAWK_ASSERT(
		HAWK_OOECS_LEN(hawk->tok.name) ==
		hawk_count_oocstr(HAWK_OOECS_PTR(hawk->tok.name)));

	/* remember the literal in the original form */
	nde->len = HAWK_OOECS_LEN(hawk->tok.name);
	nde->str = hawk_dupoocs(hawk, HAWK_OOECS_OOCS(hawk->tok.name));
	if (!nde->str || get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	if (nde->str) hawk_freemem(hawk, nde->str);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_str  (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_str_t* nde;

	nde = (hawk_nde_str_t*) hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_STR;
	nde->loc = *xloc;
	nde->len = HAWK_OOECS_LEN(hawk->tok.name);
	nde->ptr = hawk_dupoocs(hawk, HAWK_OOECS_OOCS(hawk->tok.name));
	if (nde->ptr == HAWK_NULL || get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	if (nde->ptr) hawk_freemem(hawk, nde->ptr);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_mbs (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_mbs_t* nde;

	nde = (hawk_nde_mbs_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_MBS;
	nde->loc = *xloc;

#if defined(HAWK_OOCH_IS_BCH)
	nde->len = HAWK_OOECS_LEN(hawk->tok.name);
	nde->ptr = hawk_dupoocs(hawk, HAWK_OOECS_OOCS(hawk->tok.name));
	if (!nde->ptr)
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}
#else
	{
		/* the MBS token doesn't include a character greater than 0xFF in hawk->tok.name
		 * even though it is a unicode string. simply copy over without conversion */
		nde->len = HAWK_OOECS_LEN(hawk->tok.name);
		nde->ptr = hawk_allocmem(hawk, (nde->len + 1) * HAWK_SIZEOF(*nde->ptr));
		if (!nde->ptr)
		{
			ADJERR_LOC(hawk, xloc);
			goto oops;
		}

		hawk_copy_uchars_to_bchars (nde->ptr, HAWK_OOECS_PTR(hawk->tok.name), nde->len);
		nde->ptr[nde->len] = '\0';
	}
#endif

	if (get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	if (nde->ptr) hawk_freemem(hawk, nde->ptr);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_rex (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_rex_t* nde;

	/* the regular expression is tokenized here because
	 * of the context-sensitivity of the slash symbol.
	 * if TOK_DIV is seen as a primary, it tries to compile
	 * it as a regular expression */
	hawk_ooecs_clear(hawk->tok.name);

	if (MATCH(hawk,TOK_DIV_ASSN) && hawk_ooecs_ccat(hawk->tok.name, HAWK_T('=')) == (hawk_oow_t)-1)
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	SET_TOKEN_TYPE(hawk, &hawk->tok, TOK_REX);
	if (get_rexstr(hawk, &hawk->tok) <= -1) return HAWK_NULL;

	HAWK_ASSERT(MATCH(hawk,TOK_REX));

	nde = (hawk_nde_rex_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (nde == HAWK_NULL)
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_REX;
	nde->loc = *xloc;
	nde->str.len = HAWK_OOECS_LEN(hawk->tok.name);
	nde->str.ptr = hawk_dupoocs(hawk, HAWK_OOECS_OOCS(hawk->tok.name));
	if (nde->str.ptr == HAWK_NULL) goto oops;

	if (hawk_buildrex(hawk, HAWK_OOECS_PTR(hawk->tok.name), HAWK_OOECS_LEN(hawk->tok.name), &nde->code[0], &nde->code[1]) <= -1)
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	if (get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	if (nde->code[0]) hawk_freerex(hawk, nde->code[0], nde->code[1]);
	if (nde->str.ptr) hawk_freemem(hawk, nde->str.ptr);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_positional (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_pos_t* nde;
	hawk_loc_t ploc;

	nde = (hawk_nde_pos_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		goto oops;
	}

	nde->type = HAWK_NDE_POS;
	nde->loc = *xloc;

	if (get_token(hawk) <= -1) return HAWK_NULL;

	ploc = hawk->tok.loc;
	nde->val = parse_primary(hawk, &ploc);
	if (HAWK_UNLIKELY(!nde->val)) goto oops;

	return (hawk_nde_t*)nde;

oops:
	if (nde)
	{
		if (nde->val) hawk_clrpt(hawk, nde->val);
		hawk_freemem(hawk, nde);
	}
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_lparen (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde;
	hawk_nde_t* last;
	hawk_loc_t eloc;
	hawk_oow_t opening_lparen_seq;

	opening_lparen_seq = hawk->parse.lparen_seq++;

	/* eat up the left parenthesis */
	if (get_token(hawk) <= -1) return HAWK_NULL;

	/* parse the sub-expression inside the parentheses */
	eloc = hawk->tok.loc;
	nde = parse_expr_withdc(hawk, &eloc);
	if (HAWK_UNLIKELY(!nde)) return HAWK_NULL;

	/* parse subsequent expressions separated by a comma, if any */
	last = nde;
	HAWK_ASSERT(last->next == HAWK_NULL);

	while (MATCH(hawk,TOK_COMMA))
	{
		hawk_nde_t* tmp;

		do
		{
			if (get_token(hawk) <= -1) goto oops;
		}
		while (MATCH(hawk,TOK_NEWLINE));

		eloc = hawk->tok.loc;
		tmp = parse_expr_withdc(hawk, &eloc);
		if (HAWK_UNLIKELY(!tmp)) goto oops;

		HAWK_ASSERT(tmp->next == HAWK_NULL);
		last->next = tmp;
		last = tmp;
	}
	/* ----------------- */

	/* check for the closing parenthesis */
	if (!MATCH(hawk,TOK_RPAREN))
	{
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERPAREN, FMT_ERPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		goto oops;
	}

	/* remember the sequence number of the left parenthesis
	 * that's been just closed by the matching right parenthesis */
	hawk->parse.lparen_last_closed = opening_lparen_seq;
	hawk->tok.flags |= TOK_FLAGS_LPAREN_CLOSER; /* indicate that this RPAREN is closing LPAREN */

	if (get_token(hawk) <= -1) goto oops;

	/* check if it is a chained node */
	if (nde->next)
	{
		/* if so, it is an expression group */
		/* (expr1, expr2, expr2) */

		hawk_nde_grp_t* tmp;

		if ((hawk->parse.id.stmt != TOK_PRINT && hawk->parse.id.stmt != TOK_PRINTF) || hawk->parse.depth.expr != 1)
		{
			if (!(hawk->opt.trait & HAWK_TOLERANT) && !MATCH(hawk,TOK_IN))
			{
				hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_EKWIN, FMT_EKWIN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}
		}

		tmp = (hawk_nde_grp_t*)hawk_callocmem(hawk, HAWK_SIZEOF(hawk_nde_grp_t));
		if (HAWK_UNLIKELY(!tmp))
		{
			ADJERR_LOC(hawk, xloc);
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
	if (nde) hawk_clrpt(hawk, nde);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_getline (hawk_t* hawk, const hawk_loc_t* xloc, int mbs)
{
	/* parse the statement-level getline.
	 * getline after the pipe symbols(|,||) is parsed
	 * by parse_primary().
	 */

	hawk_nde_getline_t* nde;
	hawk_loc_t ploc;

	nde = (hawk_nde_getline_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde)) goto oops;

	nde->type = HAWK_NDE_GETLINE;
	nde->mbs = mbs;
	nde->loc = *xloc;
	nde->in_type = HAWK_IN_CONSOLE;

	if (get_token(hawk) <= -1) goto oops;

	if (MATCH(hawk,TOK_IDENT) || MATCH(hawk,TOK_DOLLAR))
	{
		/* getline var
		 * getline $XXX */

		if ((hawk->opt.trait & HAWK_BLANKCONCAT) && MATCH(hawk,TOK_IDENT))
		{
			/* i need to perform some precheck on if the identifier is
			 * really a variable */
			if (preget_token(hawk) <= -1) goto oops;

			if (hawk->ntok.type == TOK_DBLCOLON) goto novar;
			if (hawk->ntok.type == TOK_LPAREN)
			{
				if (hawk->ntok.loc.line == hawk->tok.loc.line &&
				    hawk->ntok.loc.colm == hawk->tok.loc.colm + HAWK_OOECS_LEN(hawk->tok.name))
				{
					/* it's in the form of a function call since
					 * there is no spaces between the identifier
					 * and the left parenthesis. */
					goto novar;
				}
			}

			if (isfnname(hawk, HAWK_OOECS_OOCS(hawk->tok.name)) != FNTYPE_UNKNOWN) goto novar;
		}

		ploc = hawk->tok.loc;
		nde->var = parse_primary(hawk, &ploc);
		if (HAWK_UNLIKELY(!nde->var)) goto oops;

		if (!is_var(nde->var) && nde->var->type != HAWK_NDE_POS)
		{
			/* this is 'getline' followed by an expression probably.
			 *    getline a()
			 *    getline sys::WNOHANG
			 */
			hawk_seterrnum(hawk, &ploc, HAWK_EBADARG);
			goto oops;
		}
	}

novar:
	if (MATCH(hawk, TOK_LT))
	{
		/* getline [var] < file */
		if (get_token(hawk) <= -1) goto oops;

		ploc = hawk->tok.loc;
		/* TODO: is this correct? */
		/*nde->in = parse_expr_withdc(hawk, &ploc);*/
		nde->in = parse_primary(hawk, &ploc);
		if (HAWK_UNLIKELY(!nde->in)) goto oops;

		nde->in_type = HAWK_IN_FILE;
	}

	return (hawk_nde_t*)nde;

oops:
	if (nde)
	{
		if (nde->in) hawk_clrpt(hawk, nde->in);
		if (nde->var) hawk_clrpt(hawk, nde->var);
		hawk_freemem(hawk, nde);
	}
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_xnil (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_xnil_t* nde;

	nde = (hawk_nde_xnil_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = HAWK_NDE_XNIL;
	nde->loc = *xloc;

	if (get_token(hawk) <= -1) goto oops;

	return (hawk_nde_t*)nde;

oops:
	HAWK_ASSERT(nde != HAWK_NULL);
	hawk_freemem(hawk, nde);
	return HAWK_NULL;
}


static hawk_nde_t* parse_primary_xarg (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde = HAWK_NULL;
	hawk_nde_t* pos = HAWK_NULL;
	int nde_type;

	/* @argc, @argv, @argv[] look like the standard ARGC and ARGV but form special constructs
	 * for function arguments. this is useful when combined with the variadic function arguments.
	 * it can't be used in assignment, delete, reset, in many other context, however.
	 * when used in the main entry point function, @argc is the number of arguments to the
	 * function but ARGC is the number of arguments including the program name so ARGC is greater
	 * than @argc by 1. */

	HAWK_ASSERT(hawk->tok.type == TOK_XARGV || hawk->tok.type == TOK_XARGC);

	if (hawk->tok.type == TOK_XARGV)
	{
		/* @argv */
		nde_type = HAWK_NDE_XARGV;
		if (get_token(hawk) <= -1) goto oops;

		if (MATCH(hawk,TOK_LBRACK)) /* @argv[, expecting to see @argv[expr] */
		{
			hawk_loc_t eloc;

			if (get_token(hawk) <= -1) goto oops;

			eloc = hawk->tok.loc;
			pos = parse_expr_withdc(hawk, &eloc);
			if (HAWK_UNLIKELY(!pos)) goto oops;

			if (!MATCH(hawk,TOK_RBRACK))
			{
				hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERBRACK, FMT_ERBRACK, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}

			if (get_token(hawk) <= -1) goto oops;
		}
	}
	else
	{
		nde_type = HAWK_NDE_XARGC;
		if (get_token(hawk) <= -1) goto oops;
	}

	if (pos)
	{
		hawk_nde_xargvidx_t* xargvidx;

		xargvidx = (hawk_nde_xargvidx_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*xargvidx));
		if (HAWK_UNLIKELY(!xargvidx))
		{
			ADJERR_LOC(hawk, xloc);
			return HAWK_NULL;
		}

		xargvidx->type = HAWK_NDE_XARGVIDX;
		xargvidx->loc = *xloc;
		xargvidx->pos = pos;

		nde = (hawk_nde_t*)xargvidx;
	}
	else
	{
		nde = hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
		if (HAWK_UNLIKELY(!nde))
		{
			ADJERR_LOC(hawk, xloc);
			return HAWK_NULL;
		}

		nde->type = nde_type;
		nde->loc = *xloc;
	}

	return (hawk_nde_t*)nde;

oops:
	if (nde) hawk_freemem(hawk, nde); /* tricky to call hawk_clrpt() on nde due to pos */
	if (pos) hawk_clrpt(hawk, pos);
	return HAWK_NULL;
}

static hawk_nde_t* parse_primary_literal (hawk_t* hawk, const hawk_loc_t* xloc)
{
	switch (hawk->tok.type)
	{
		case TOK_CHAR:
			return parse_primary_char(hawk, xloc);

		case TOK_BCHR:
			return parse_primary_bchr(hawk, xloc);

		case TOK_INT:
			return parse_primary_int(hawk, xloc);

		case TOK_FLT:
			return parse_primary_flt(hawk, xloc);

		case TOK_STR:
			return parse_primary_str(hawk, xloc);

		case TOK_MBS:
			return parse_primary_mbs(hawk, xloc);

		default:
		{
			hawk_tok_t* xtok;

			xtok = MATCH(hawk,TOK_NEWLINE)? &hawk->ptok: &hawk->tok;
			hawk_seterrfmt(hawk, &xtok->loc, HAWK_EEXPRNR, HAWK_T("literal expression not recognized around '%.*js'"), HAWK_OOECS_LEN(xtok->name),  HAWK_OOECS_PTR(xtok->name));
			return HAWK_NULL;
		}
	}
}

static hawk_nde_t* parse_primary_nopipe (hawk_t* hawk, const hawk_loc_t* xloc)
{
	switch (hawk->tok.type)
	{
		case TOK_IDENT:
			return parse_primary_ident(hawk, xloc);

		case TOK_CHAR:
			return parse_primary_char(hawk, xloc);

		case TOK_BCHR:
			return parse_primary_bchr(hawk, xloc);

		case TOK_INT:
			return parse_primary_int(hawk, xloc);

		case TOK_FLT:
			return parse_primary_flt(hawk, xloc);

		case TOK_STR:
			return parse_primary_str(hawk, xloc);

		case TOK_MBS:
			return parse_primary_mbs(hawk, xloc);

		case TOK_DIV:
		case TOK_DIV_ASSN:
			return parse_primary_rex(hawk, xloc);

		case TOK_DOLLAR:
			return parse_primary_positional(hawk, xloc);

		case TOK_LPAREN:
			return parse_primary_lparen(hawk, xloc);

		case TOK_GETBLINE:
			return parse_primary_getline(hawk, xloc, 1);

		case TOK_GETLINE:
			return parse_primary_getline(hawk, xloc, 0);

		case TOK_XNIL:
			return parse_primary_xnil(hawk, xloc);

		case TOK_XARGV:
		case TOK_XARGC:
			return parse_primary_xarg(hawk, xloc);

		default:
		{
			hawk_tok_t* xtok;

			/* in the tolerant mode, we treat print and printf
			 * as a function like getline */
			if ((hawk->opt.trait & HAWK_TOLERANT) &&
			    (MATCH(hawk,TOK_PRINT) || MATCH(hawk,TOK_PRINTF)))
			{
				if (get_token(hawk) <= -1) return HAWK_NULL;
				return parse_print(hawk, xloc);
			}

			/* valid expression introducer is expected */
			xtok = MATCH(hawk,TOK_NEWLINE)? &hawk->ptok: &hawk->tok;
			hawk_seterrfmt(hawk, &xtok->loc, HAWK_EEXPRNR, HAWK_T("expression not recognized around '%.*js'"), HAWK_OOECS_LEN(xtok->name),  HAWK_OOECS_PTR(xtok->name));
			return HAWK_NULL;
		}
	}
}

static hawk_nde_t* parse_primary (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* left;
	hawk_nde_getline_t* nde;
	hawk_nde_t* var = HAWK_NULL;
	hawk_loc_t ploc;

	left = parse_primary_nopipe(hawk, xloc);
	if (!left) goto oops;

	/* handle the piping part */
	do
	{
		int intype = -1;
		int mbs;

		if (hawk->opt.trait & HAWK_RIO)
		{
			if (MATCH(hawk,TOK_BOR))
			{
				intype = HAWK_IN_PIPE;
			}
			else if (MATCH(hawk,TOK_PIPE_BD) && (hawk->parse.pragma.trait & HAWK_RWPIPE))
			{
				intype = HAWK_IN_RWPIPE;
			}
		}

		if (intype == -1) break;

		if (preget_token(hawk) <= -1) goto oops;
		if (hawk->ntok.type == TOK_GETBLINE) mbs = 1;
		else if (hawk->ntok.type == TOK_GETLINE) mbs = 0;
		else break;

		/* consume ntok('getline') */
		get_token(hawk); /* no error check needed as it's guaranteeded to succeed for preget_token() above */

		/* get the next token */
		if (get_token(hawk) <= -1) goto oops;

		/* TODO: is this correct? */
		if (MATCH(hawk,TOK_IDENT) || MATCH(hawk,TOK_DOLLAR))
		{
			/* command | getline var
			 * command || getline var */

			if ((hawk->opt.trait & HAWK_BLANKCONCAT) && MATCH(hawk,TOK_IDENT))
			{
				/* i need to perform some precheck on if the identifier is
				 * really a variable */
				if (preget_token(hawk) <= -1) goto oops;

				if (hawk->ntok.type == TOK_DBLCOLON) goto novar;
				if (hawk->ntok.type == TOK_LPAREN)
				{
					if (hawk->ntok.loc.line == hawk->tok.loc.line &&
					    hawk->ntok.loc.colm == hawk->tok.loc.colm + HAWK_OOECS_LEN(hawk->tok.name))
					{
						/* it's in the form of a function call since
						 * there is no spaces between the identifier
						 * and the left parenthesis. */
						goto novar;
					}
				}

				if (isfnname(hawk, HAWK_OOECS_OOCS(hawk->tok.name)) != FNTYPE_UNKNOWN) goto novar;
			}

			ploc = hawk->tok.loc;
			var = parse_primary(hawk, &ploc);
			if (!var) goto oops;

			if (!is_var(var) && var->type != HAWK_NDE_POS)
			{
				/* fucntion a() {}
				 * print ("ls -laF" | getline a()) */
				hawk_seterrnum(hawk, &ploc, HAWK_EBADARG);
				goto oops;
			}
		}

	novar:
		nde = (hawk_nde_getline_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
		if (HAWK_UNLIKELY(!nde))
		{
			ADJERR_LOC(hawk, xloc);
			goto oops;
		}

		nde->type = HAWK_NDE_GETLINE;
		nde->loc = *xloc;
		nde->var = var;
		nde->mbs = mbs;
		nde->in_type = intype;
		nde->in = left;

		left = (hawk_nde_t*)nde;
		var = HAWK_NULL;
	}
	while (1);

	return left;

oops:
	if (var) hawk_clrpt(hawk, var);
	hawk_clrpt(hawk, left);
	return HAWK_NULL;
}

static hawk_nde_t* parse_variable (hawk_t* hawk, const hawk_loc_t* xloc, hawk_nde_type_t type, const hawk_oocs_t* name, hawk_oow_t idxa)
{
	hawk_nde_var_t* nde;
	int is_fcv = 0;

	if (MATCH(hawk,TOK_LPAREN))
	{
	#if defined(HAWK_ENABLE_FUN_AS_VALUE)
/*
	if (MATCH(hawk,TOK_LPAREN) &&
	    (!(hawk->opt.trait & HAWK_BLANKCONCAT) ||
		(hawk->tok.loc.line == xloc->line &&
		 hawk->tok.loc.colm == xloc->colm + name->len)))
*/
		if (hawk->tok.loc.line == xloc->line && hawk->tok.loc.colm == xloc->colm + name->len)
		{
			is_fcv = 1;
		}
		else
	#endif
		if (!(hawk->opt.trait & HAWK_BLANKCONCAT))
		{
			/* if concatenation by blanks is not allowed, the explicit
			 * concatenation operator(%%) must be used. so it is obvious
			 * that it is a function call, which is illegal for a variable.
			 * if implicit, "var_xxx (1)" may be concatenation of
			 * the value of var_xxx and 1.
			 */
			/* a variable is not a function */
			hawk_seterrfmt(hawk, xloc, HAWK_EFUNNAM, HAWK_T("'%.*js' not a valid function name"), name->len, name->ptr);
			return HAWK_NULL;
		}
	}

	nde = (hawk_nde_var_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	nde->type = type;
	nde->loc = *xloc;
	/*nde->id.name.ptr = HAWK_NULL;*/
	nde->id.name.ptr = name->ptr;
	nde->id.name.len = name->len;
	nde->id.idxa = idxa;
	nde->idx = HAWK_NULL;

#if defined(HAWK_ENABLE_FUN_AS_VALUE)
	if (!is_fcv) return (hawk_nde_t*)nde;
	return parse_fncall(hawk, (const hawk_oocs_t*)nde, HAWK_NULL, xloc, FNCALL_FLAG_VAR);
#else
	return (hawk_nde_t*)nde;
#endif
}

static int dup_ident_and_get_next (hawk_t* hawk, const hawk_loc_t* xloc, hawk_oocs_t* name, int max)
{
	int nsegs = 0;

	HAWK_ASSERT(MATCH(hawk,TOK_IDENT));

	do
	{
		name[nsegs].ptr = HAWK_OOECS_PTR(hawk->tok.name);
		name[nsegs].len = HAWK_OOECS_LEN(hawk->tok.name);

		/* duplicate the identifier */
		name[nsegs].ptr = hawk_dupoochars(hawk, name[nsegs].ptr, name[nsegs].len);
		if (!name[nsegs].ptr)
		{
			ADJERR_LOC(hawk, xloc);
			goto oops;
		}

		nsegs++;

		if (get_token(hawk) <= -1) goto oops;

		if (!MATCH(hawk,TOK_DBLCOLON)) break;

		if (get_token(hawk) <= -1) goto oops;

		/* the identifier after ::
		 * allow reserved words as well since i view the whole name(mod::ident)
		 * as one segment. however, i don't want the identifier part to begin
		 * with @. some extended keywords begin with @ like @include.
		 * TOK_XGLOBAL to TOK_XRESET are excuded from the check for that reason. */
		if (!MATCH(hawk, TOK_IDENT) && !(MATCH_RANGE(hawk, TOK_BEGIN, TOK_GETLINE)))
		{
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_EIDENT, FMT_EIDENT, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
			goto oops;
		}

		if (nsegs >= max)
		{
			hawk_seterrnum(hawk, xloc, HAWK_ESEGTM);
			goto oops;
		}
	}
	while (1);

	return nsegs;

oops:
	while (nsegs > 0) hawk_freemem(hawk, name[--nsegs].ptr);
	return -1;
}

#if defined(HAWK_ENABLE_FUN_AS_VALUE)
static hawk_nde_t* parse_fun_as_value  (hawk_t* hawk, const hawk_oocs_t* name, const hawk_loc_t* xloc, hawk_fun_t* funptr)
{
	hawk_nde_fun_t* nde;

	/* create the node for the literal */
	nde = (hawk_nde_fun_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		ADJERR_LOC(hawk, xloc);
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

static hawk_nde_t* parse_primary_ident_noseg (hawk_t* hawk, const hawk_loc_t* xloc, const hawk_oocs_t* name)
{
	hawk_fnc_t* fnc;
	hawk_oow_t idxa;
	hawk_nde_t* nde = HAWK_NULL;

	/* check if name is an intrinsic function name */
	fnc = hawk_findfncwithoocs(hawk, name);
	if (fnc)
	{
		if (MATCH(hawk,TOK_LPAREN) || fnc->dfl0)
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

				HAWK_ASSERT(fnc->spec.arg.spec != HAWK_NULL);

				segs[0].ptr = (hawk_ooch_t*)fnc->spec.arg.spec;
				segs[0].len = hawk_count_oocstr(fnc->spec.arg.spec);
				segs[1] = *name;

				return parse_primary_ident_segs(hawk, xloc, name, segs, 2);
			}

			/* fnc->dfl0 means that the function can be called without ().
			 * i.e. length */
			nde = parse_fncall(hawk, name, fnc, xloc, ((!MATCH(hawk,TOK_LPAREN) && fnc->dfl0)? FNCALL_FLAG_NOARG: 0));
		}
		else
		{
			/* an intrinsic function should be in the form of the function call */
			hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		}
	}
	/* now we know that name is a normal identifier. */
	else if (MATCH(hawk,TOK_LBRACK))
	{
		nde = parse_hashidx(hawk, name, xloc);
	}
	else if ((idxa = hawk_arr_rsearch(hawk->parse.lcls, HAWK_ARR_SIZE(hawk->parse.lcls), name->ptr, name->len)) != HAWK_ARR_NIL)
	{
		/* local variable */
		nde = parse_variable(hawk, xloc, HAWK_NDE_LCL, name, idxa);
	}
	else if ((idxa = hawk_arr_search(hawk->parse.params, 0, name->ptr, name->len)) != HAWK_ARR_NIL)
	{
		/* parameter */
		nde = parse_variable(hawk, xloc, HAWK_NDE_ARG, name, idxa);
	}
	else if ((idxa = get_global(hawk, name)) != HAWK_ARR_NIL)
	{
		/* global variable */
		nde = parse_variable(hawk, xloc, HAWK_NDE_GBL, name, idxa);
	}
	else
	{
		int fntype;
		hawk_fun_t* funptr = HAWK_NULL;

		fntype = isfunname(hawk, name, &funptr);

		if (fntype)
		{
			HAWK_ASSERT(fntype == FNTYPE_FUN);

			if (MATCH(hawk,TOK_LPAREN))
			{
				/* must be a function name */
				HAWK_ASSERT(hawk_htb_search(hawk->parse.named, name->ptr, name->len) == HAWK_NULL);
				nde = parse_fncall(hawk, name, HAWK_NULL, xloc, 0);
			}
			else
			{
				/* function name appeared without (). used as a value without invocation */
			#if defined(HAWK_ENABLE_FUN_AS_VALUE)
				nde = parse_fun_as_value(hawk, name, xloc, funptr);
			#else
				hawk_seterrfmt(hawk, xloc, HAWK_EFUNRED, HAWK_T("function '%.*js' redefined"), name->len, name->ptr);
			#endif
			}
		}
		/*else if (hawk->opt.trait & HAWK_IMPLICIT) */
		else if (hawk->parse.pragma.trait & HAWK_IMPLICIT)
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

			if (MATCH(hawk,TOK_LPAREN) &&
			    (!(hawk->opt.trait & HAWK_BLANKCONCAT) ||
			     (hawk->tok.loc.line == xloc->line &&
			      hawk->tok.loc.colm == xloc->colm + name->len)))
			{
				/* it is a function call to an undefined function yet */
				if (hawk_htb_search(hawk->parse.named, name->ptr, name->len) != HAWK_NULL)
				{
					/* the function call conflicts with a named variable */
			#if defined(HAWK_ENABLE_FUN_AS_VALUE)
					is_fncall_var = 1;
					goto named_var;
			#else
					hawk_seterrfmt(hawk, xloc, HAWK_EVARRED, HAWK_T("variable '%.*js' redefined"), name->len, name->ptr);
			#endif
				}
				else
				{
					nde = parse_fncall(hawk, name, HAWK_NULL, xloc, 0);
				}
			}
			else
			{
				hawk_nde_var_t* tmp;

			#if defined(HAWK_ENABLE_FUN_AS_VALUE)
			named_var:
			#endif

				/* if there is a space between the name and the left parenthesis
				 * while the name is not resolved to anything, we treat the space
				 * as concatention by blanks. so we handle the name as a named
				 * variable. */
				tmp = (hawk_nde_var_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*tmp));
				if (HAWK_UNLIKELY(!tmp)) ADJERR_LOC(hawk, xloc);
				else
				{
					/* collect unique instances of a named variable
					 * for reference */
					if (hawk_htb_upsert(hawk->parse.named, name->ptr, name->len, HAWK_NULL, 0) == HAWK_NULL)
					{
						ADJERR_LOC(hawk, xloc);
						hawk_freemem(hawk, tmp);
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

			#if defined(HAWK_ENABLE_FUN_AS_VALUE)
						if (is_fncall_var)
							nde = parse_fncall(hawk, (const hawk_oocs_t*)nde, HAWK_NULL, xloc, FNCALL_FLAG_VAR);
			#endif
					}
				}
			}
		}
		else
		{
			if (MATCH(hawk,TOK_LPAREN))
			{
				/* it is a function call as the name is followed
				 * by ( with/without spaces and implicit variables are disabled. */
				nde = parse_fncall(hawk, name, HAWK_NULL, xloc, 0);
			}
			else
			{
				/* undefined variable */
				hawk_seterrfmt(hawk, xloc, HAWK_EUNDEF, FMT_EUNDEF, name->len, name->ptr);
			}
		}
	}

	return nde;
}

static hawk_nde_t* parse_primary_ident_segs (hawk_t* hawk, const hawk_loc_t* xloc, const hawk_oocs_t* full, const hawk_oocs_t segs[], int nsegs)
{
	/* parse xxx::yyy */

	hawk_nde_t* nde = HAWK_NULL;
	hawk_mod_t* mod;
	hawk_mod_sym_t sym;
	hawk_fnc_t fnc;

	mod = query_module(hawk, segs, nsegs, &sym);
	if (!mod)
	{
		ADJERR_LOC(hawk, xloc);
	}
	else
	{
		switch (sym.type)
		{
			case HAWK_MOD_FNC:
				if ((hawk->opt.trait & sym.u.fnc_.trait) != sym.u.fnc_.trait)
				{
					hawk_seterrfmt(hawk, xloc, HAWK_EUNDEF, FMT_EUNDEF, full->len, full->ptr);
					break;
				}

				if (MATCH(hawk,TOK_LPAREN))
				{
					HAWK_MEMSET (&fnc, 0, HAWK_SIZEOF(fnc));
					fnc.name.ptr = full->ptr;
					fnc.name.len = full->len;
					fnc.spec = sym.u.fnc_;
					fnc.mod = mod;
					nde = parse_fncall(hawk, full, &fnc, xloc, 0);
				}
				else
				{
					hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ELPAREN, FMT_ELPAREN, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				}
				break;

			case HAWK_MOD_INT:
				if ((hawk->opt.trait & sym.u.int_.trait) != sym.u.int_.trait)
				{
					hawk_seterrfmt(hawk, xloc, HAWK_EUNDEF, FMT_EUNDEF, full->len, full->ptr);
					break;
				}

				nde = new_int_node(hawk, sym.u.int_.val, xloc);
				/* i don't remember the symbol in the original form */
				break;

			case HAWK_MOD_FLT:
				if ((hawk->opt.trait & sym.u.flt_.trait) != sym.u.flt_.trait)
				{
					hawk_seterrfmt(hawk, xloc, HAWK_EUNDEF, FMT_EUNDEF, full->len, full->ptr);
					break;
				}

				nde = new_flt_node(hawk, sym.u.flt_.val, xloc);
				/* i don't remember the symbol in the original form */
				break;

			default:
				/* TODO: support MOD_VAR */
				hawk_seterrfmt(hawk, xloc, HAWK_EUNDEF, FMT_EUNDEF, full->len, full->ptr);
				break;
		}
	}

	return nde;
}

static hawk_nde_t* parse_primary_ident (hawk_t* hawk, const hawk_loc_t* xloc)
{
	hawk_nde_t* nde = HAWK_NULL;
	hawk_oocs_t name[2]; /* TODO: support more than 2 segments??? */
	int nsegs;

	HAWK_ASSERT(MATCH(hawk,TOK_IDENT));

	nsegs = dup_ident_and_get_next(hawk, xloc, name, HAWK_COUNTOF(name));
	if (nsegs <= -1) return HAWK_NULL;

	if (nsegs <= 1)
	{
		nde = parse_primary_ident_noseg(hawk, xloc, &name[0]);
		if (HAWK_UNLIKELY(!nde)) hawk_freemem(hawk, name[0].ptr);
	}
	else
	{
		hawk_oocs_t full; /* full name including :: */
		hawk_oow_t capa;
		int i;

		for (capa = 0, i = 0; i < nsegs; i++) capa += name[i].len + 2; /* +2 for :: */
		full.ptr = hawk_allocmem(hawk, HAWK_SIZEOF(*full.ptr) * (capa + 1));
		if (HAWK_LIKELY(full.ptr))
		{
			capa = hawk_copy_oochars_to_oocstr_unlimited(&full.ptr[0], name[0].ptr, name[0].len);
			for (i = 1; i < nsegs; i++)
			{
				capa += hawk_copy_oocstr_unlimited(&full.ptr[capa], HAWK_T("::"));
				capa += hawk_copy_oochars_to_oocstr_unlimited(&full.ptr[capa], name[i].ptr, name[i].len);
			}
			full.ptr[capa] = HAWK_T('\0');
			full.len = capa;

			nde = parse_primary_ident_segs(hawk, xloc, &full, name, nsegs);
			if (!nde || nde->type != HAWK_NDE_FNCALL_FNC)
			{
				/* the FNC node takes the full name but other
				 * nodes don't. so i need to free it. i know it's ugly. */
				hawk_freemem(hawk, full.ptr);
			}
		}
		else
		{
			/* error number is set in hawk_allocmem */
			ADJERR_LOC(hawk, xloc);
		}

		/* i don't need the name segments */
		while (nsegs > 0) hawk_freemem(hawk, name[--nsegs].ptr);
	}

	return nde;
}

static hawk_nde_t* parse_hashidx (hawk_t* hawk, const hawk_oocs_t* name, const hawk_loc_t* xloc)
{
	hawk_nde_t* idx, * tmp, * last;
	hawk_nde_var_t* nde;
	hawk_oow_t idxa;

	idx = HAWK_NULL;
	last = HAWK_NULL;

#if defined(HAWK_ENABLE_GC)
more_idx:
#endif
	do
	{
		if (get_token(hawk) <= -1)
		{
			if (idx) hawk_clrpt(hawk, idx);
			return HAWK_NULL;
		}

		{
			hawk_loc_t eloc;
			eloc = hawk->tok.loc;
			tmp = parse_expr_withdc(hawk, &eloc);
		}
		if (HAWK_UNLIKELY(!tmp))
		{
			if (idx) hawk_clrpt(hawk, idx);
			return HAWK_NULL;
		}

		if (!idx)
		{
			/* this is the first item */
			HAWK_ASSERT(last == HAWK_NULL);
			idx = tmp;
			last = tmp;
		}
		else
		{
			/* not the first item. append it */
			last->next = tmp;
			last = tmp;
		}
	}
	while (MATCH(hawk,TOK_COMMA));

	HAWK_ASSERT(idx != HAWK_NULL);

	if (!MATCH(hawk,TOK_RBRACK))
	{
		hawk_clrpt(hawk, idx);
		hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ERBRACK, FMT_ERBRACK, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
		return HAWK_NULL;
	}

	if (get_token(hawk) <= -1)
	{
		hawk_clrpt(hawk, idx);
		return HAWK_NULL;
	}

#if defined(HAWK_ENABLE_GC)
	if (MATCH(hawk,TOK_LBRACK))
	{
		/* additional index - a[10][20] ...
		 * use the NULL node as a splitter */
		tmp = (hawk_nde_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*tmp));
		if (HAWK_UNLIKELY(!tmp))
		{
			hawk_clrpt(hawk, idx);
			ADJERR_LOC(hawk, xloc);
			return HAWK_NULL;
		}
		tmp->type = HAWK_NDE_NULL;

		HAWK_ASSERT(idx != HAWK_NULL);
		HAWK_ASSERT(last != HAWK_NULL);

		last->next = tmp;
		last = tmp;
		goto more_idx;
	}
#endif

	nde = (hawk_nde_var_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*nde));
	if (HAWK_UNLIKELY(!nde))
	{
		hawk_clrpt(hawk, idx);
		ADJERR_LOC(hawk, xloc);
		return HAWK_NULL;
	}

	/* search the local variable list */
	idxa = hawk_arr_rsearch(hawk->parse.lcls, HAWK_ARR_SIZE(hawk->parse.lcls), name->ptr, name->len);
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
	idxa = hawk_arr_search(hawk->parse.params, 0, name->ptr, name->len);
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
	idxa = get_global(hawk, name);
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

	/*if (hawk->opt.trait & HAWK_IMPLICIT) */
	if (hawk->parse.pragma.trait & HAWK_IMPLICIT)
	{
		int fnname = isfnname(hawk, name);
		switch (fnname)
		{
			case FNTYPE_FNC:
				hawk_seterrfmt(hawk, xloc, HAWK_EFNCRED, HAWK_T("intrinsic function '%.*js' redefined"), name->len, name->ptr);
				goto exit_func;

			case FNTYPE_FUN:
				hawk_seterrfmt(hawk, xloc, HAWK_EFUNRED, HAWK_T("function '%.*js' redefined"), name->len, name->ptr);
				goto exit_func;
		}

		HAWK_ASSERT(fnname == 0);

		nde->type = HAWK_NDE_NAMEDIDX;
		nde->loc = *xloc;
		nde->id.name.ptr = name->ptr;
		nde->id.name.len = name->len;
		nde->id.idxa = (hawk_oow_t)-1;
		nde->idx = idx;

		return (hawk_nde_t*)nde;
	}

	/* undefined variable */
	hawk_seterrfmt(hawk, xloc, HAWK_EUNDEF, FMT_EUNDEF, name->len, name->ptr);

exit_func:
	hawk_clrpt(hawk, idx);
	hawk_freemem(hawk, nde);

	return HAWK_NULL;
}

static hawk_nde_t* parse_fncall (hawk_t* hawk, const hawk_oocs_t* name, hawk_fnc_t* fnc, const hawk_loc_t* xloc, int flags)
{
	hawk_nde_t* head, * curr, * nde;
	hawk_nde_fncall_t* call;
	hawk_oow_t nargs;
	hawk_loc_t eloc;

	head = curr = HAWK_NULL;
	call = HAWK_NULL;
	nargs = 0;

	if (flags & FNCALL_FLAG_NOARG) goto make_node;
	if (get_token(hawk) <= -1) goto oops;

	if (MATCH(hawk,TOK_RPAREN))
	{
		/* no parameters to the function call */
		if (get_token(hawk) <= -1) goto oops;
	}
	else
	{
		/* parse function parameters */
		while (1)
		{
			eloc = hawk->tok.loc;
			nde = parse_expr_withdc(hawk, &eloc);
			if (!nde) goto oops;

			if (!head) head = nde;
			else curr->next = nde;
			curr = nde;

			nargs++;

			if (MATCH(hawk,TOK_RPAREN))
			{
				if (get_token(hawk) <= -1) goto oops;
				break;
			}

			if (!MATCH(hawk,TOK_COMMA))
			{
				hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_ECOMMA, FMT_ECOMMA, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
				goto oops;
			}

			do
			{
				if (get_token(hawk) <= -1) goto oops;
			}
			while (MATCH(hawk,TOK_NEWLINE));
		}

	}

make_node:
	call = (hawk_nde_fncall_t*)hawk_callocmem(hawk, HAWK_SIZEOF(*call));
	if (HAWK_UNLIKELY(!call))
	{
		ADJERR_LOC(hawk, xloc);
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
			hawk_seterrnum(hawk, xloc, HAWK_EARGTM);
			goto oops;
		}
		else if (nargs < call->u.fnc.spec.arg.min)
		{
			hawk_seterrnum(hawk, xloc, HAWK_EARGTF);
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

		/* store a non-builtin function call into the hawk->parse.funs table */
		if (!hawk_htb_upsert(hawk->parse.funs, name->ptr, name->len, call, 0))
		{
			ADJERR_LOC(hawk, xloc);
			goto oops;
		}
	}

	return (hawk_nde_t*)call;

oops:
	if (call) hawk_freemem(hawk, call);
	if (head) hawk_clrpt(hawk, head);
	return HAWK_NULL;
}

static int get_number (hawk_t* hawk, hawk_tok_t* tok)
{
	hawk_ooci_t c;

	HAWK_ASSERT(HAWK_OOECS_LEN(tok->name) == 0);
	SET_TOKEN_TYPE(hawk, tok, TOK_INT);

	c = hawk->sio.last.c;

	if (c == '0')
	{
		ADD_TOKEN_CHAR(hawk, tok, c);
		GET_CHAR_TO(hawk, c);

		if (c == 'x' || c == 'X')
		{
			/* hexadecimal number */
			do
			{
				ADD_TOKEN_CHAR(hawk, tok, c);
				GET_CHAR_TO(hawk, c);
			}
			while (hawk_is_ooch_xdigit(c));

			return 0;
		}
		else if (c == 'b' || c == 'B')
		{
			/* binary number */
			do
			{
				ADD_TOKEN_CHAR(hawk, tok, c);
				GET_CHAR_TO(hawk, c);
			}
			while (c == '0' || c == '1');

			return 0;
		}
		else if (c != '.')
		{
			/* octal number */
			while (c >= '0' && c <= '7')
			{
				ADD_TOKEN_CHAR(hawk, tok, c);
				GET_CHAR_TO(hawk, c);
			}

			if (c == '8' || c == '9')
			{
				hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_ELXCHR, HAWK_T("invalid digit '%jc'"), (hawk_ooch_t)c);
				return -1;
			}

			return 0;
		}
	}

	while (hawk_is_ooch_digit(c))
	{
		ADD_TOKEN_CHAR(hawk, tok, c);
		GET_CHAR_TO(hawk, c);
	}

	if (c == '.')
	{
		/* floating-point number */
		SET_TOKEN_TYPE(hawk, tok, TOK_FLT);

		ADD_TOKEN_CHAR(hawk, tok, c);
		GET_CHAR_TO(hawk, c);

		while (hawk_is_ooch_digit(c))
		{
			ADD_TOKEN_CHAR(hawk, tok, c);
			GET_CHAR_TO(hawk, c);
		}
	}

	if (c == 'E' || c == 'e')
	{
		SET_TOKEN_TYPE(hawk, tok, TOK_FLT);

		ADD_TOKEN_CHAR(hawk, tok, c);
		GET_CHAR_TO(hawk, c);

		if (c == '+' || c == '-')
		{
			ADD_TOKEN_CHAR(hawk, tok, c);
			GET_CHAR_TO(hawk, c);
		}

		while (hawk_is_ooch_digit(c))
		{
			ADD_TOKEN_CHAR(hawk, tok, c);
			GET_CHAR_TO(hawk, c);
		}
	}

	return 0;
}

/* i think allowing only up to 2 hexadigits is more useful though it
 * may break compatibilty with other hawk implementations. If you want
 * more than 2, define HEX_DIGIT_LIMIT_FOR_X to HAWK_TYPE_MAX(hawk_oow_t). */
/*#define HEX_DIGIT_LIMIT_FOR_X (HAWK_TYPE_MAX(hawk_oow_t))*/
#define HEX_DIGIT_LIMIT_FOR_X (2)

static int get_string (
	hawk_t* hawk, hawk_ooch_t end_char,
	hawk_ooch_t esc_char, int keep_esc_char, int byte_only,
	hawk_oow_t preescaped, hawk_tok_t* tok)
{
	hawk_ooci_t c;
	hawk_oow_t escaped = preescaped;
	hawk_oow_t digit_count = 0;
	hawk_uint32_t c_acc = 0;

	while (1)
	{
		GET_CHAR_TO(hawk, c);

		if (c == HAWK_OOCI_EOF)
		{
			hawk_seterrnum(hawk, &hawk->tok.loc, HAWK_ESTRNC);
			return -1;
		}

	#if defined(HAWK_OOCH_IS_BCH)
		/* nothing extra to handle byte_only */
	#else
		if (byte_only && c != '\\' && !HAWK_BYTE_PRINTABLE(c))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EMBSCHR, HAWK_T("invalid mbs character '%jc'"), (hawk_ooch_t)c);
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
					ADD_TOKEN_UINT32(hawk, tok, c_acc);
					escaped = 0;
				}
				continue;
			}
			else
			{
				if (digit_count == 1 && end_char == HAWK_T('/'))
				{
					/* inside a regular expression, it's likely a backreference */
					hawk_ooch_t oc = c_acc + HAWK_T('0');
					ADD_TOKEN_CHAR(hawk, tok, esc_char);
					ADD_TOKEN_CHAR(hawk, tok, oc);
				}
				else
				{
					ADD_TOKEN_UINT32(hawk, tok, c_acc);
				}
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
					ADD_TOKEN_UINT32(hawk, tok, c_acc);
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
					ADD_TOKEN_UINT32(hawk, tok, c_acc);
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
					ADD_TOKEN_UINT32(hawk, tok, c_acc);
					escaped = 0;
				}
				continue;
			}
			else
			{
				if (digit_count == 0)
				{
					hawk_ooch_t ec;

					ec = (escaped == HEX_DIGIT_LIMIT_FOR_X)? HAWK_T('x'):
					     (escaped == 4)? HAWK_T('u'): HAWK_T('U');

					/* no valid character after the escaper.
					 * keep the escaper as it is. consider this input:
					 *   \xGG
					 * 'c' is at the first G. this part is to restore the
					 * \x part. since \x is not followed by any hexadecimal
					 * digits, it's literally 'x' */
					ADD_TOKEN_CHAR(hawk, tok, ec);
				}
				else ADD_TOKEN_UINT32(hawk, tok, c_acc);

				escaped = 0;
				/* carry on to handle the current character  */
			}
		}
		else if (escaped == 99)
		{
			escaped = 0;
			if (c == '\n') continue; /* backslash \r \n */
		}

		/* -------------------------------------- */

		if (escaped == 0)
		{
			if (c == end_char)
			{
				/* terminating quote */
				/*GET_CHAR_TO(hawk, c);*/
				GET_CHAR (hawk);
				break;
			}
			else if (c == esc_char)
			{
				escaped = 1;
				continue;
			}
			else if (!(hawk->parse.pragma.trait & HAWK_MULTILINESTR) && (c == '\n' || c == '\r'))
			{
				hawk_seterrnum(hawk, &hawk->tok.loc, HAWK_ESTRNC);
				return -1;
			}
		}
		else if (escaped == 1)
		{
			if (c == '\n')
			{
				/* line continuation - a backslash at the end of line */
				escaped = 0;
				continue;
			}
			else if (c == '\r')
			{
				escaped = 99;
				continue;
			}

			if (c == HAWK_T('n')) c = HAWK_T('\n');
			else if (c == HAWK_T('r')) c = HAWK_T('\r');
			else if (c == HAWK_T('t')) c = HAWK_T('\t');
			else if (c == HAWK_T('f')) c = HAWK_T('\f');
			else if (c == HAWK_T('b')) c = HAWK_T('\b');
			else if (c == HAWK_T('v')) c = HAWK_T('\v');
			else if (c == HAWK_T('a')) c = HAWK_T('\a');
			else if (c >= HAWK_T('0') && c <= HAWK_T('7'))
			{
				/* treat it as an octal notation first and
				 * check if it's a backreference between \1 and \7 inclusive
				 * in the `if (escaped == 3)` block. */
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
				ADD_TOKEN_CHAR(hawk, tok, esc_char);
			}

			escaped = 0;
		}

		ADD_TOKEN_CHAR(hawk, tok, c);
	}

	return 0;
}

static int get_rexstr (hawk_t* hawk, hawk_tok_t* tok)
{
	if (hawk->sio.last.c == HAWK_T('/'))
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
		GET_CHAR (hawk);
		return 0;
	}
	else
	{
		hawk_oow_t preescaped = 0;
		if (hawk->sio.last.c == HAWK_T('\\'))
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
			ADD_TOKEN_CHAR(hawk, tok, hawk->sio.last.c);
		}
		return get_string(hawk, HAWK_T('/'), HAWK_T('\\'), 1, 0, preescaped, tok);
	}
}


static int get_raw_string (hawk_t* hawk, hawk_ooch_t end_char, int byte_only, hawk_tok_t* tok)
{
	hawk_ooci_t c;

	while (1)
	{
		GET_CHAR_TO(hawk, c);

		if (c == HAWK_OOCI_EOF)
		{
			hawk_seterrnum(hawk, &hawk->tok.loc, HAWK_ESTRNC);
			return -1;
		}

	#if defined(HAWK_OOCH_IS_BCH)
		/* nothing extra to handle byte_only */
	#else
		if (byte_only && c != '\\' && !HAWK_BYTE_PRINTABLE(c))
		{
			hawk_seterrfmt(hawk, &hawk->tok.loc, HAWK_EMBSCHR, HAWK_T("invalid mbs character '%jc'"), (hawk_ooch_t)c);
			return -1;
		}
	#endif

		if (c == end_char)
		{
			/* terminating quote */
			GET_CHAR (hawk);
			break;
		}

		ADD_TOKEN_CHAR(hawk, tok, c);
	}

	return 0;
}

static int skip_spaces (hawk_t* hawk)
{
	hawk_ooci_t c = hawk->sio.last.c;

	if (hawk->opt.trait & HAWK_NEWLINE)
	{
		do
		{
			while (c != HAWK_T('\n') && hawk_is_ooch_space(c))
				GET_CHAR_TO(hawk, c);

			if (c == HAWK_T('\\'))
			{
				hawk_sio_lxc_t bs;
				hawk_sio_lxc_t cr;
				int hascr = 0;

				bs = hawk->sio.last;
				GET_CHAR_TO(hawk, c);
				if (c == HAWK_T('\r'))
				{
					hascr = 1;
					cr = hawk->sio.last;
					GET_CHAR_TO(hawk, c);
				}

				if (c == HAWK_T('\n'))
				{
					GET_CHAR_TO(hawk, c);
					continue;
				}
				else
				{
					/* push back the last character */
					unget_char(hawk, &hawk->sio.last);
					/* push CR if any */
					if (hascr) unget_char(hawk, &cr);
					/* restore the orginal backslash */
					hawk->sio.last = bs;
				}
			}

			break;
		}
		while (1);
	}
	else
	{
		while (hawk_is_ooch_space(c)) GET_CHAR_TO(hawk, c);
	}

	return 0;
}

static int skip_comment (hawk_t* hawk)
{
	hawk_ooci_t c = hawk->sio.last.c;
	hawk_sio_lxc_t lc;

	if (c == HAWK_T('#'))
	{
		/* skip up to \n */
		do { GET_CHAR_TO(hawk, c); }
		while (c != HAWK_T('\n') && c != HAWK_OOCI_EOF);

		if (!(hawk->opt.trait & HAWK_NEWLINE)) GET_CHAR (hawk);
		return 1; /* comment by # */
	}

	/* handle c-style comment */
	if (c != HAWK_T('/')) return 0; /* not a comment */

	/* save the last character */
	lc = hawk->sio.last;
	/* read a new character */
	GET_CHAR_TO(hawk, c);

	if (c == HAWK_T('*'))
	{
		do
		{
			GET_CHAR_TO(hawk, c);
			if (c == HAWK_OOCI_EOF)
			{
				hawk_loc_t loc;
				loc.line = hawk->sio.inp->line;
				loc.colm = hawk->sio.inp->colm;
				loc.file = hawk->sio.inp->path;
				hawk_seterrnum(hawk, &loc, HAWK_ECMTNC);
				return -1;
			}

			if (c == HAWK_T('*'))
			{
				GET_CHAR_TO(hawk, c);
				if (c == HAWK_OOCI_EOF)
				{
					hawk_loc_t loc;
					loc.line = hawk->sio.inp->line;
					loc.colm = hawk->sio.inp->colm;
					loc.file = hawk->sio.inp->path;
					hawk_seterrnum(hawk, &loc, HAWK_ECMTNC);
					return -1;
				}

				if (c == HAWK_T('/'))
				{
					/*GET_CHAR_TO(hawk, c);*/
					GET_CHAR (hawk);
					break;
				}
			}
		}
		while (1);

		return 1; /* c-style comment */
	}

	/* unget '*' */
	unget_char(hawk, &hawk->sio.last);
	/* restore the previous state */
	hawk->sio.last = lc;

	return 0;
}

static int get_symbols (hawk_t* hawk, hawk_ooci_t c, hawk_tok_t* tok)
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
		{ HAWK_T("|&"),  2, TOK_PIPE_BD,      0 },
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
		{ HAWK_T("~"),   1, TOK_TILDE,        0 },
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
		{ HAWK_T("..."), 3, TOK_ELLIPSIS,     0 },
		{ HAWK_T(".."),  2, TOK_DBLPERIOD,    0 },
		{ HAWK_T("."),   1, TOK_PERIOD,       0 },
		{ HAWK_NULL,     0, 0,                0 }
	};

	struct ops_t* p;
	int idx = 0;

	/* note that the loop below is not generaic enough.
	 * you must keep the operators strings in a particular order */


	for (p = ops; p->str != HAWK_NULL; )
	{
		if (p->trait == 0 || (hawk->opt.trait & p->trait))
		{
			if (p->str[idx] == HAWK_T('\0'))
			{
				ADD_TOKEN_STR(hawk, tok, p->str, p->len);
				SET_TOKEN_TYPE(hawk, tok, p->tid);
				return 1;
			}

			if (c == p->str[idx])
			{
				idx++;
				GET_CHAR_TO(hawk, c);
				continue;
			}
		}

		p++;
	}

	return 0;
}

static int get_token_into (hawk_t* hawk, hawk_tok_t* tok)
{
	hawk_ooci_t c;
	int n;
	int skip_semicolon_after_include = 0;

retry:
	do
	{
		if (skip_spaces(hawk) <= -1) return -1;
		if ((n = skip_comment(hawk)) <= -1) return -1;
	}
	while (n >= 1);

	hawk_ooecs_clear(tok->name);
	tok->flags = 0;
	tok->loc.file = hawk->sio.last.file;
	tok->loc.line = hawk->sio.last.line;
	tok->loc.colm = hawk->sio.last.colm;

	c = hawk->sio.last.c;

	if (c == HAWK_OOCI_EOF)
	{
		n = end_include(hawk);
		if (n <= -1) return -1;
		if (n >= 1)
		{
			/*hawk->sio.last = hawk->sio.inp->last;*/
			/* mark that i'm retrying after end of an included file */
			skip_semicolon_after_include = 1;
			goto retry;
		}

		ADD_TOKEN_STR(hawk, tok, HAWK_T("<EOF>"), 5);
		SET_TOKEN_TYPE(hawk, tok, TOK_EOF);
	}
	else if (c == HAWK_T('\n'))
	{
		/*ADD_TOKEN_CHAR(hawk, tok, HAWK_T('\n'));*/
		ADD_TOKEN_STR(hawk, tok, HAWK_T("<NL>"), 4);
		SET_TOKEN_TYPE(hawk, tok, TOK_NEWLINE);
		GET_CHAR (hawk);
	}
	else if (hawk_is_ooch_digit(c)/*|| c == HAWK_T('.')*/)
	{
		if (get_number(hawk, tok) <= -1) return -1;
	}
	else if (c == HAWK_T('.'))
	{
		hawk_sio_lxc_t lc;

		lc = hawk->sio.last;
		GET_CHAR_TO(hawk, c);

		unget_char(hawk, &hawk->sio.last);
		hawk->sio.last = lc;

		if (hawk_is_ooch_digit(c))
		{
			/* for a token such as .123 */
			if (get_number(hawk, tok) <= -1) return -1;
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

		GET_CHAR_TO(hawk, c);

		if (c != HAWK_T('_') && !hawk_is_ooch_alpha(c))
		{
			/* this extended keyword is empty,
			 * not followed by a valid word */
			hawk_seterrnum(hawk, &(hawk)->tok.loc, HAWK_EXKWEM);
			return -1;
		}

		if (c == 'B' || c == 'b')
		{
			hawk_sio_lxc_t pc1 = hawk->sio.last;
			GET_CHAR_TO(hawk, c);
			if (c == 'R' || c == 'r')
			{
				hawk_sio_lxc_t pc2 = hawk->sio.last;
				GET_CHAR_TO(hawk, c);
				if (c == '\"')
				{
					/* raw byte string */
					SET_TOKEN_TYPE(hawk, tok, TOK_MBS);
					if (get_raw_string(hawk, c, 1, tok) <= -1) return -1;
				}
				else
				{
					unget_char(hawk, &hawk->sio.last);
					unget_char(hawk, &pc2);
					hawk->sio.last = pc1;
					c = pc1.c;
					goto process_at_identifier;
				}
			}
			else if (c == '\"')
			{
				/* @B"XXX", @b"XXX" - byte string */
				SET_TOKEN_TYPE(hawk, tok, TOK_MBS);
				if (get_string(hawk, c, HAWK_T('\\'), 0, 1, 0, tok) <= -1) return -1;
			}
			else if (c == '\'')
			{
				/* @B'X' @b'x' - byte character */
				SET_TOKEN_TYPE(hawk, tok, TOK_BCHR);
				if (get_string(hawk, c, '\\', 0, 1, 0, tok) <= -1) return -1;
				if (HAWK_OOECS_LEN(tok->name) != 1)
				{
					hawk_seterrfmt(hawk, &tok->loc, HAWK_ELXCHR, HAWK_T("invalid byte-character token"));
					return -1;
				}
			}
			else
			{
				unget_char(hawk, &hawk->sio.last);
				hawk->sio.last = pc1;
				c = pc1.c;
				goto process_at_identifier;
			}
		}
		else if (c == 'R' || c == 'r')
		{
			hawk_sio_lxc_t pc1 = hawk->sio.last;
			GET_CHAR_TO(hawk, c);
			if (c == 'B' || c == 'b')
			{
				hawk_sio_lxc_t pc2 = hawk->sio.last;
				GET_CHAR_TO(hawk, c);
				if (c == '\"')
				{
					/* raw byte string */
					SET_TOKEN_TYPE(hawk, tok, TOK_MBS);
					if (get_raw_string(hawk, c, 1, tok) <= -1) return -1;
				}
				else
				{
					unget_char(hawk, &hawk->sio.last);
					unget_char(hawk, &pc2);
					hawk->sio.last = pc1;
					c = pc1.c;
					goto process_at_identifier;
				}
			}
			else if (c == '\"')
			{
				/* R, r - raw string */
				SET_TOKEN_TYPE(hawk, tok, TOK_STR);
				if (get_raw_string(hawk, c, 0, tok) <= -1) return -1;
			}
	#if 0

			else if (c == '\'')
			{
				/* TODO: character literal when I add a character type?? */
			}
	#endif
			else
			{
				unget_char(hawk, &hawk->sio.last);
				hawk->sio.last = pc1;
				c = pc1.c;
				goto process_at_identifier;
			}
		}
		else
		{
		process_at_identifier:
			ADD_TOKEN_CHAR(hawk, tok, HAWK_T('@'));

			/* expect normal identifier starting with an alphabet */
			do
			{
				ADD_TOKEN_CHAR(hawk, tok, c);
				GET_CHAR_TO(hawk, c);
			}
			while (c == HAWK_T('_') || hawk_is_ooch_alpha(c) || hawk_is_ooch_digit(c));

			if (hawk_comp_oochars_bcstr(HAWK_OOECS_PTR(tok->name), HAWK_OOECS_LEN(tok->name), "@SCRIPTNAME", 0) == 0)
			{
				/* special parser-level word @SCRIPTNAME. substitute an actual value for it */
				if (HAWK_UNLIKELY(hawk_ooecs_cpy(tok->name, (tok->loc.file? (const hawk_ooch_t*)tok->loc.file: (const hawk_ooch_t*)HAWK_T(""))) == (hawk_oow_t)-1)) return -1;
				SET_TOKEN_TYPE(hawk, tok, TOK_STR);
			}
			else if (hawk_comp_oochars_bcstr(HAWK_OOECS_PTR(tok->name), HAWK_OOECS_LEN(tok->name), "@SCRIPTLINE", 0) == 0)
			{
				/* special parser-level word @SCRIPTLINE. subsitute an actual value for it */
				if (HAWK_UNLIKELY(hawk_ooecs_fmt(tok->name, HAWK_T("%zu"), tok->loc.line) == (hawk_oow_t)-1)) return -1;
				SET_TOKEN_TYPE(hawk, tok, TOK_INT);
			}
			else if (hawk_comp_oochars_bcstr(HAWK_OOECS_PTR(tok->name), HAWK_OOECS_LEN(tok->name), "@UCH_ON", 0) == 0)
			{
				/* special parser-level word @SCRIPTLINE. subsitute an actual value for it */
			#if defined(HAWK_OOCH_IS_UCH)
				if (HAWK_UNLIKELY(hawk_ooecs_fmt(tok->name, HAWK_T("%zu"), 1) == (hawk_oow_t)-1)) return -1;
			#else
				if (HAWK_UNLIKELY(hawk_ooecs_fmt(tok->name, HAWK_T("%zu"), 0) == (hawk_oow_t)-1)) return -1;
			#endif
				SET_TOKEN_TYPE(hawk, tok, TOK_INT);
			}
			else if (hawk_comp_oochars_bcstr(HAWK_OOECS_PTR(tok->name), HAWK_OOECS_LEN(tok->name), "@BCH_ON", 0) == 0)
			{
				/* special parser-level word @SCRIPTLINE. subsitute an actual value for it */
			#if defined(HAWK_OOCH_IS_UCH)
				if (HAWK_UNLIKELY(hawk_ooecs_fmt(tok->name, HAWK_T("%zu"), 0) == (hawk_oow_t)-1)) return -1;
			#else
				if (HAWK_UNLIKELY(hawk_ooecs_fmt(tok->name, HAWK_T("%zu"), 1) == (hawk_oow_t)-1)) return -1;
			#endif
				SET_TOKEN_TYPE(hawk, tok, TOK_INT);
			}
			else
			{
				type = classify_ident(hawk, HAWK_OOECS_OOCS(tok->name));
				if (type == TOK_IDENT)
				{
					hawk_seterrfmt(hawk,  &hawk->tok.loc, HAWK_EXKWNR, FMT_EXKWNR, HAWK_OOECS_LEN(hawk->tok.name), HAWK_OOECS_PTR(hawk->tok.name));
					return -1;
				}
				SET_TOKEN_TYPE(hawk, tok, type);
			}
		}
	}
	else if (c == '_' || hawk_is_ooch_alpha(c))
	{
		int type;

		/* identifier */
		do
		{
			ADD_TOKEN_CHAR(hawk, tok, c);
			GET_CHAR_TO(hawk, c);
		}
		while (c == '_' || hawk_is_ooch_alpha(c) || hawk_is_ooch_digit(c));

		type = classify_ident(hawk, HAWK_OOECS_OOCS(tok->name));
		SET_TOKEN_TYPE(hawk, tok, type);
	}
	else if (c == '\"')
	{
		/* double-quoted string */
		SET_TOKEN_TYPE(hawk, tok, TOK_STR);
		if (get_string(hawk, c, '\\', 0, 0, 0, tok) <= -1) return -1;
	}
	else if (c == '\'')
	{
		SET_TOKEN_TYPE(hawk, tok, TOK_CHAR);
		if (get_string(hawk, c, '\\', 0, 0, 0, tok) <= -1) return -1;
		if (HAWK_OOECS_LEN(tok->name) != 1)
		{
			hawk_seterrfmt(hawk, &tok->loc, HAWK_ELXCHR, HAWK_T("invalid character token"));
			return -1;
		}
	}
	else
	{
	try_get_symbols:
		n = get_symbols(hawk, c, tok);
		if (n <= -1) return -1;
		if (n == 0)
		{
			/* not handled yet */
			if (c == HAWK_T('\0'))
			{
				hawk_seterrfmt(hawk, &tok->loc, HAWK_ELXCHR, HAWK_T("invalid character '\\0'"));
			}
			else
			{
				hawk_seterrfmt(hawk, &tok->loc, HAWK_ELXCHR, HAWK_T("invalid character '%jc'"), (hawk_ooch_t)c);
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

	if (skip_semicolon_after_include && !(hawk->opt.trait & HAWK_NEWLINE))
	{
		/* semiclon has not been skipped yet and the newline option is not set. */
		hawk_seterrfmt(hawk, &tok->loc, HAWK_ESCOLON, FMT_ESCOLON, HAWK_OOECS_LEN(tok->name), HAWK_OOECS_PTR(tok->name));
		return -1;
	}

	return 0;
}

static int get_token (hawk_t* hawk)
{
	hawk->ptok.type = hawk->tok.type;
	hawk->ptok.flags = hawk->tok.flags;
	hawk->ptok.loc.file = hawk->tok.loc.file;
	hawk->ptok.loc.line = hawk->tok.loc.line;
	hawk->ptok.loc.colm = hawk->tok.loc.colm;
	hawk_ooecs_swap(hawk->ptok.name, hawk->tok.name);

	if (HAWK_OOECS_LEN(hawk->ntok.name) > 0)
	{
		hawk->tok.type = hawk->ntok.type;
		hawk->tok.flags = hawk->ntok.flags;
		hawk->tok.loc.file = hawk->ntok.loc.file;
		hawk->tok.loc.line = hawk->ntok.loc.line;
		hawk->tok.loc.colm = hawk->ntok.loc.colm;

		hawk_ooecs_swap(hawk->tok.name, hawk->ntok.name);
		hawk_ooecs_clear(hawk->ntok.name);

		return 0;
	}

	return get_token_into(hawk, &hawk->tok);
}

static int preget_token (hawk_t* hawk)
{
	/* LIMITATION: no more than one token can be pre-read in a row
	               without consumption. */

	if (HAWK_OOECS_LEN(hawk->ntok.name) > 0)
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
		 * token and place it to hawk->ntok. */
		return get_token_into(hawk, &hawk->ntok);
	}
}

static int classify_ident (hawk_t* hawk, const hawk_oocs_t* name)
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
			if ((hawk->opt.trait & kwp->trait) != kwp->trait) break;
			return kwp->type;
		}
	}

	return TOK_IDENT;
}

int hawk_isvalidident (hawk_t* hawk, const hawk_ooch_t* name)
{
	hawk_ooch_t c;
	hawk_oocs_t cs;

	cs.ptr = (hawk_ooch_t*)name;

	if ((c = *name) == '_' || hawk_is_ooch_alpha(c))
	{
		do
		{
			c = *++name;
		}
		while (c == '_' || hawk_is_ooch_alpha(c) || hawk_is_ooch_digit(c));
		if (c != '\0') return 0;

		cs.len = name - cs.ptr;
		return classify_ident(hawk, &cs) == TOK_IDENT;
	}

	return 0;
}

struct deparse_func_t
{
	hawk_t* hawk;
	hawk_ooch_t* tmp;
	hawk_oow_t tmp_len;
	int ret;
};

static int deparse (hawk_t* hawk)
{
	hawk_nde_t* nde;
	hawk_chain_t* chain;
	hawk_ooch_t tmp[HAWK_SIZEOF(hawk_oow_t)*8 + 32];
	struct deparse_func_t df;
	int n = 0;
	hawk_ooi_t op;
	hawk_oocs_t kw;

	HAWK_ASSERT(hawk->sio.outf != HAWK_NULL);

	HAWK_MEMSET (&hawk->sio.arg, 0, HAWK_SIZEOF(hawk->sio.arg));

	op = hawk->sio.outf(hawk, HAWK_SIO_CMD_OPEN, &hawk->sio.arg, HAWK_NULL, 0);
	if (op <= -1) return -1;

#define EXIT_DEPARSE() do { n = -1; goto exit_deparse; } while(0)

	if (hawk->parse.pragma.rtx_stack_limit > 0 && hawk->parse.pragma.rtx_stack_limit != hawk->opt.rtx_stack_limit)
	{
		hawk_oow_t len;

		len = hawk_int_to_oocstr((hawk_int_t)hawk->parse.pragma.rtx_stack_limit, 10, HAWK_NULL, tmp, HAWK_COUNTOF(tmp));
		if (hawk_putsrcoocstr(hawk, HAWK_T("@pragma stack_limit ")) <= -1 ||
		    hawk_putsrcoochars(hawk, tmp, len) <= -1 ||
		    hawk_putsrcoocstr(hawk, HAWK_T(";\n")) <= -1) EXIT_DEPARSE ();
	}

	if (hawk->tree.ngbls > hawk->tree.ngbls_base)
	{
		hawk_oow_t i, len;

		HAWK_ASSERT(hawk->tree.ngbls > 0);

		hawk_getkwname(hawk, HAWK_KWID_XGLOBAL, &kw);
		if (hawk_putsrcoochars(hawk, kw.ptr, kw.len) <= -1 || hawk_putsrcoocstr(hawk, HAWK_T(" ")) <= -1) EXIT_DEPARSE ();

		for (i = hawk->tree.ngbls_base; i < hawk->tree.ngbls - 1; i++)
		{
			if (!(hawk->opt.trait & HAWK_IMPLICIT))
			{
				/* use the actual name if no named variable is allowed */
				if (hawk_putsrcoochars(hawk, HAWK_ARR_DPTR(hawk->parse.gbls, i), HAWK_ARR_DLEN(hawk->parse.gbls, i)) <= -1) EXIT_DEPARSE ();
			}
			else
			{
				len = hawk_int_to_oocstr((hawk_int_t)i, 10, HAWK_T("__g"), tmp, HAWK_COUNTOF(tmp));
				HAWK_ASSERT(len != (hawk_oow_t)-1);
				if (hawk_putsrcoochars(hawk, tmp, len) <= -1) EXIT_DEPARSE ();
			}

			if (hawk_putsrcoocstr(hawk, HAWK_T(", ")) <= -1) EXIT_DEPARSE ();
		}

		if (!(hawk->opt.trait & HAWK_IMPLICIT))
		{
			/* use the actual name if no named variable is allowed */
			if (hawk_putsrcoochars(hawk, HAWK_ARR_DPTR(hawk->parse.gbls,i), HAWK_ARR_DLEN(hawk->parse.gbls,i)) <= -1) EXIT_DEPARSE ();
		}
		else
		{
			len = hawk_int_to_oocstr((hawk_int_t)i, 10, HAWK_T("__g"), tmp, HAWK_COUNTOF(tmp));
			HAWK_ASSERT(len != (hawk_oow_t)-1);
			if (hawk_putsrcoochars(hawk, tmp, len) <= -1) EXIT_DEPARSE ();
		}

		if (hawk->opt.trait & HAWK_CRLF)
		{
			if (hawk_putsrcoocstr(hawk, HAWK_T(";\r\n\r\n")) <= -1) EXIT_DEPARSE ();
		}
		else
		{
			if (hawk_putsrcoocstr(hawk, HAWK_T(";\n\n")) <= -1) EXIT_DEPARSE ();
		}
	}

	df.hawk = hawk;
	df.tmp = tmp;
	df.tmp_len = HAWK_COUNTOF(tmp);
	df.ret = 0;

	hawk_htb_walk(hawk->tree.funs, deparse_func, &df);
	if (df.ret <= -1) EXIT_DEPARSE ();

	for (nde = hawk->tree.begin; nde != HAWK_NULL; nde = nde->next)
	{
		hawk_oocs_t kw;

		hawk_getkwname(hawk, HAWK_KWID_BEGIN, &kw);

		if (hawk_putsrcoochars(hawk, kw.ptr, kw.len) <= -1) EXIT_DEPARSE ();
		if (hawk_putsrcoocstr(hawk, HAWK_T(" ")) <= -1) EXIT_DEPARSE ();

		if (hawk_prnnde(hawk, nde) <= -1) EXIT_DEPARSE ();

		if (hawk->opt.trait & HAWK_CRLF)
		{
			if (put_char(hawk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
		}

		if (put_char(hawk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();
	}

	chain = hawk->tree.chain;
	while (chain)
	{
		if (chain->pattern != HAWK_NULL)
		{
			if (hawk_prnptnpt(hawk, chain->pattern) <= -1) EXIT_DEPARSE ();
		}

		if (chain->action == HAWK_NULL)
		{
			/* blockless pattern */
			if (hawk->opt.trait & HAWK_CRLF)
			{
				if (put_char(hawk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
			}

			if (put_char(hawk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();
		}
		else
		{
			if (chain->pattern)
			{
				if (put_char(hawk, HAWK_T(' ')) <= -1) EXIT_DEPARSE ();
			}
			if (hawk_prnpt(hawk, chain->action) <= -1) EXIT_DEPARSE ();
		}

		if (hawk->opt.trait & HAWK_CRLF)
		{
			if (put_char(hawk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
		}

		if (put_char(hawk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();

		chain = chain->next;
	}

	for (nde = hawk->tree.end; nde != HAWK_NULL; nde = nde->next)
	{
		hawk_oocs_t kw;

		hawk_getkwname(hawk, HAWK_KWID_END, &kw);

		if (hawk_putsrcoochars(hawk, kw.ptr, kw.len) <= -1) EXIT_DEPARSE ();
		if (hawk_putsrcoocstr(hawk, HAWK_T(" ")) <= -1) EXIT_DEPARSE ();

		if (hawk_prnnde(hawk, nde) <= -1) EXIT_DEPARSE ();

		/*
		if (hawk->opt.trait & HAWK_CRLF)
		{
			if (put_char(hawk, HAWK_T('\r')) <= -1) EXIT_DEPARSE ();
		}

		if (put_char(hawk, HAWK_T('\n')) <= -1) EXIT_DEPARSE ();
		*/
	}

	if (flush_out(hawk) <= -1) EXIT_DEPARSE ();

exit_deparse:
	if (hawk->sio.outf(hawk, HAWK_SIO_CMD_CLOSE, &hawk->sio.arg, HAWK_NULL, 0) != 0 && n == 0) n = -1;
	return n;
}

static hawk_htb_walk_t deparse_func (hawk_htb_t* map, hawk_htb_pair_t* pair, void* arg)
{
	struct deparse_func_t* df = (struct deparse_func_t*)arg;
	hawk_fun_t* fun = (hawk_fun_t*)HAWK_HTB_VPTR(pair);
	hawk_oow_t i, n;
	hawk_oocs_t kw;

	HAWK_ASSERT(hawk_comp_oochars(HAWK_HTB_KPTR(pair), HAWK_HTB_KLEN(pair), fun->name.ptr, fun->name.len, 0) == 0);

#define PUT_C(x,c) \
	if (put_char(x->hawk,c)==-1) { \
		x->ret = -1; return HAWK_HTB_WALK_STOP; \
	}

#define PUT_S(x,str) \
	if (hawk_putsrcoocstr(x->hawk,str) <= -1) { \
		x->ret = -1; return HAWK_HTB_WALK_STOP; \
	}

#define PUT_SX(x,str,len) \
	if (hawk_putsrcoochars(x->hawk, str, len) <= -1) { \
		x->ret = -1; return HAWK_HTB_WALK_STOP; \
	}

	hawk_getkwname (df->hawk, HAWK_KWID_FUNCTION, &kw);
	PUT_SX(df, kw.ptr, kw.len);

	PUT_C(df, HAWK_T(' '));
	PUT_SX(df, fun->name.ptr, fun->name.len);
	PUT_S(df, HAWK_T(" ("));

	for (i = 0; i < fun->nargs; )
	{
		if (fun->argspec && i < fun->argspeclen && fun->argspec[i] == 'r') PUT_S(df, HAWK_T("&"));
		n = hawk_int_to_oocstr(i++, 10, HAWK_T("__p"), df->tmp, df->tmp_len);
		HAWK_ASSERT(n != (hawk_oow_t)-1);
		PUT_SX (df, df->tmp, n);

		if (i >= fun->nargs) break;
		PUT_S(df, HAWK_T(", "));
	}

	if (fun->variadic)
	{
		if (fun->nargs > 0) PUT_S(df, HAWK_T(", "));
		PUT_S(df, HAWK_T("..."));
	}

	PUT_S(df, HAWK_T(")"));
	if (df->hawk->opt.trait & HAWK_CRLF) PUT_C(df, HAWK_T('\r'));

	PUT_C(df, HAWK_T('\n'));

	if (hawk_prnpt(df->hawk, fun->body) <= -1) return -1;
	if (df->hawk->opt.trait & HAWK_CRLF)
	{
		PUT_C(df, HAWK_T('\r'));
	}
	PUT_C(df, HAWK_T('\n'));

	return HAWK_HTB_WALK_FORWARD;

#undef PUT_C
#undef PUT_S
#undef PUT_SX
}

static int put_char (hawk_t* hawk, hawk_ooch_t c)
{
	hawk->sio.arg.b.buf[hawk->sio.arg.b.len++] = c;
	if (hawk->sio.arg.b.len >= HAWK_COUNTOF(hawk->sio.arg.b.buf))
	{
		if (flush_out(hawk) <= -1) return -1;
	}
	return 0;
}

static int flush_out (hawk_t* hawk)
{
	hawk_ooi_t n;

	while (hawk->sio.arg.b.pos < hawk->sio.arg.b.len)
	{
		n = hawk->sio.outf(
			hawk, HAWK_SIO_CMD_WRITE, &hawk->sio.arg,
			&hawk->sio.arg.b.buf[hawk->sio.arg.b.pos],
			hawk->sio.arg.b.len - hawk->sio.arg.b.pos
		);
		if (n <= 0) return -1;
		hawk->sio.arg.b.pos += n;
	}

	hawk->sio.arg.b.pos = 0;
	hawk->sio.arg.b.len = 0;
	return 0;
}

int hawk_putsrcoocstr (hawk_t* hawk, const hawk_ooch_t* str)
{
	while (*str != '\0')
	{
		if (put_char(hawk, *str) <= -1) return -1;
		str++;
	}

	return 0;
}

int hawk_putsrcoochars (hawk_t* hawk, const hawk_ooch_t* str, hawk_oow_t len)
{
	const hawk_ooch_t* end = str + len;

	while (str < end)
	{
		if (put_char(hawk, *str) <= -1) return -1;
		str++;
	}

	return 0;
}

#if defined(HAWK_ENABLE_STATIC_MODULE)

/* let's hardcode module information */
#include "mod-hawk.h"
#include "mod-math.h"
#include "mod-str.h"
#include "mod-sys.h"

#if defined(HAWK_ENABLE_MOD_FFI_STATIC)
#include "../mod/mod-ffi.h"
#endif

#if defined(HAWK_ENABLE_MOD_MYSQL_STATIC)
#include "../mod/mod-mysql.h"
#endif

#if defined(HAWK_ENABLE_MOD_SED_STATIC)
#include "../mod/mod-sed.h"
#endif

#if defined(HAWK_ENABLE_MOD_UCI_STATIC)
#include "../mod/mod-uci.h"
#endif

#if defined(HAWK_ENABLE_MOD_MEMC_STATIC)
#include "../mod/mod-memc.h"
#endif

/*
 * if modules are linked statically into the main hawk module,
 * this table is used to find the entry point of the modules.
 * you must update this table if you add more modules
 */

static struct
{
	hawk_ooch_t* modname;
	int (*modload) (hawk_mod_t* mod, hawk_t* hawk);
} static_modtab[] =
{
#if defined(HAWK_ENABLE_MOD_FFI_STATIC)
	{ HAWK_T("ffi"),    hawk_mod_ffi },
#endif
	{ HAWK_T("hawk"),   hawk_mod_hawk },
	{ HAWK_T("math"),   hawk_mod_math },
#if defined(HAWK_ENABLE_MOD_MYSQL_STATIC)
	{ HAWK_T("mysql"),  hawk_mod_mysql },
#endif
#if defined(HAWK_ENABLE_MOD_SED_STATIC)
	{ HAWK_T("sed"),    hawk_mod_sed },
#endif
	{ HAWK_T("str"),    hawk_mod_str },
	{ HAWK_T("sys"),    hawk_mod_sys },
#if defined(HAWK_ENABLE_MOD_UCI_STATIC)
	{ HAWK_T("uci"),    hawk_mod_uci },
#endif
#if defined(HAWK_ENABLE_MOD_MEMC_STATIC)
	{ HAWK_T("memc"),   hawk_mod_memc },
#endif
};
#endif

static hawk_mod_t* query_module (hawk_t* hawk, const hawk_oocs_t segs[], int nsegs, hawk_mod_sym_t* sym)
{
	hawk_rbt_pair_t* pair;
	hawk_mod_data_t* mdp;
	int n;

	HAWK_ASSERT(nsegs == 2);

	pair = hawk_rbt_search(hawk->modtab, segs[0].ptr, segs[0].len);
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
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESEGTL, FMT_ESEGTL, segs[0].len, segs[0].ptr);
			return HAWK_NULL;
		}

#if defined(HAWK_ENABLE_STATIC_MODULE)
		/* attempt to find a statically linked module */

		/* TODO: binary search ... */
		for (n = 0; n < HAWK_COUNTOF(static_modtab); n++)
		{
			if (hawk_comp_oochars_oocstr(segs[0].ptr, segs[0].len, static_modtab[n].modname, 0) == 0)
			{
				load = static_modtab[n].modload;
				break;
			}
		}

		/*if (n >= HAWK_COUNTOF(static_modtab))
		{
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%.*js' not found"), segs[0].len, segs[0].ptr);
			return HAWK_NULL;
		}*/

		if (load)
		{
			/* found the module in the staic module table */

			HAWK_MEMSET (&md, 0, HAWK_SIZEOF(md));
			/* Note md.handle is HAWK_NULL for a static module */

			/* i copy-insert 'md' into the table before calling 'load'.
			 * to pass the same address to load(), query(), etc */
			pair = hawk_rbt_insert(hawk->modtab, segs[0].ptr, segs[0].len, &md, HAWK_SIZEOF(md));
			if (!pair) return HAWK_NULL;

			mdp = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
			if (load(&mdp->mod, hawk) <= -1)
			{
				hawk_rbt_delete(hawk->modtab, segs[0].ptr, segs[0].len);
				return HAWK_NULL;
			}

			goto done;
		}
#endif
		/* attempt to find an external module */
		HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
		spec.prefix = (hawk->opt.mod[1].len > 0)? (const hawk_ooch_t*)hawk->opt.mod[1].ptr: (const hawk_ooch_t*)HAWK_T(HAWK_DEFAULT_MODPREFIX);
		spec.postfix = (hawk->opt.mod[2].len > 0)? (const hawk_ooch_t*)hawk->opt.mod[2].ptr: (const hawk_ooch_t*)HAWK_T(HAWK_DEFAULT_MODPOSTFIX);
		spec.name = segs[0].ptr; /* the caller must ensure that this segment is null-terminated */

		if (!hawk->prm.modopen || !hawk->prm.modgetsym || !hawk->prm.modclose)
		{
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EINVAL, HAWK_T("module callbacks not set properly"));
			goto open_fail;
		}

		spec.libdir = (hawk->opt.mod[0].len > 0)? (const hawk_ooch_t*)hawk->opt.mod[0].ptr: (const hawk_ooch_t*)HAWK_T(HAWK_DEFAULT_MODLIBDIRS);
		do
		{
#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	define LIBDIR_SEPARATOR ';'
#else
#	define LIBDIR_SEPARATOR ':'
#endif
			hawk_ooch_t* colon;
			colon = hawk_find_oochar_in_oocstr(spec.libdir, LIBDIR_SEPARATOR);
			if (colon) *colon = '\0';

			HAWK_MEMSET (&md, 0, HAWK_SIZEOF(md));
			md.handle = hawk->prm.modopen(hawk, &spec);
			if (!colon) break;

			*colon = LIBDIR_SEPARATOR;
			spec.libdir = colon + 1;
#undef LIBDIR_SEPARATOR
		}
		while (!md.handle);

		if (!md.handle)
		{
			const hawk_ooch_t* bem;
		open_fail:
			bem = hawk_backuperrmsg(hawk);
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js%js%js' for module '%js' not found - %js"),
				(spec.prefix? (const hawk_ooch_t*)spec.prefix: (const hawk_ooch_t*)HAWK_T("")),
				spec.name,
				(spec.postfix? (const hawk_ooch_t*)spec.postfix: (const hawk_ooch_t*)HAWK_T("")),
				spec.name, bem);
			return HAWK_NULL;
		}

		buflen = hawk_copy_oocstr_unlimited(&buf[10], segs[0].ptr);
		/* attempt hawk_mod_xxx */
		load = hawk->prm.modgetsym(hawk, md.handle, &buf[1]);
		if (!load)
		{
			/* attempt _hawk_mod_xxx */
			load = hawk->prm.modgetsym(hawk, md.handle, &buf[0]);
			if (!load)
			{
				hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOERR);

				/* attempt hawk_mod_xxx_ */
				buf[10 + buflen] = HAWK_T('_');
				buf[10 + buflen + 1] = HAWK_T('\0');
				load = hawk->prm.modgetsym(hawk, md.handle, &buf[1]);
				if (!load)
				{
					const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
					hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("module symbol '%.*js' not found - %js"), (10 + buflen), &buf[1], bem);

					hawk->prm.modclose(hawk, md.handle);
					return HAWK_NULL;
				}
			}
		}

		/* i copy-insert 'md' into the table before calling 'load'.
		 * to pass the same address to load(), query(), etc */
		pair = hawk_rbt_insert(hawk->modtab, segs[0].ptr, segs[0].len, &md, HAWK_SIZEOF(md));
		if (pair == HAWK_NULL)
		{
			hawk->prm.modclose(hawk, md.handle);
			return HAWK_NULL;
		}

		mdp = (hawk_mod_data_t*)HAWK_RBT_VPTR(pair);
		if (load(&mdp->mod, hawk) <= -1)
		{
			hawk_rbt_delete(hawk->modtab, segs[0].ptr, segs[0].len);
			hawk->prm.modclose(hawk, mdp->handle);
			return HAWK_NULL;
		}
	}

done:
	hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOERR);
	n = mdp->mod.query(&mdp->mod, hawk, segs[1].ptr, sym);
	if (n <= -1)
	{
		const hawk_ooch_t* olderrmsg = hawk_backuperrmsg(hawk);
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("unable to find '%.*js' in module '%.*js' - %js"), segs[1].len, segs[1].ptr, segs[0].len, segs[0].ptr, olderrmsg);
		return HAWK_NULL;
	}
	return &mdp->mod;
}

hawk_mod_t* hawk_querymodulewithname (hawk_t* hawk, hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	const hawk_ooch_t* dc;
	hawk_oocs_t segs[2];
	hawk_mod_t* mod;
	hawk_oow_t name_len;
	hawk_ooch_t tmp;

/*TOOD: non-module builtin function? fnc? */
	name_len = hawk_count_oocstr(name);
	dc = hawk_find_oochars_in_oochars(name, name_len, HAWK_T("::"), 2, 0);
	if (!dc)
	{
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EINVAL, HAWK_T("invalid module name -  %js"), name);
		return HAWK_NULL;
	}

	segs[0].len = dc - name;
	segs[0].ptr = name;
	tmp = name[segs[0].len];
	name[segs[0].len] = '\0';

	segs[1].len = name_len - segs[0].len - 2;
	segs[1].ptr = (hawk_ooch_t*)name + segs[0].len + 2;

	mod = query_module(hawk, segs, 2, sym);

	name[segs[0].len] = tmp;
	return mod;
}
