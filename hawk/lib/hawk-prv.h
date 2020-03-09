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

#ifndef _HAWK_PRV_H_
#define _HAWK_PRV_H_

#include <hawk-arr.h>
#include <hawk-chr.h>
#include <hawk-ecs.h>
#include <hawk-fmt.h>
#include <hawk-htb.h>
#include <hawk-rbt.h>
#include <hawk-utl.h>

typedef struct hawk_chain_t hawk_chain_t;
typedef struct hawk_tree_t hawk_tree_t;

#include <hawk.h>
#include "tree-prv.h"
#include "fnc-prv.h"
#include "parse-prv.h"
#include "run-prv.h"
#include "rio-prv.h"
#include "val-prv.h"
#include "err-prv.h"
#include "misc-prv.h"

#define ENABLE_FEATURE_SCACHE
#define FEATURE_SCACHE_NUM_BLOCKS 16
#define FEATURE_SCACHE_BLOCK_UNIT 16
#define FEATURE_SCACHE_BLOCK_SIZE 128

/* [NOTE] the function value support implemented is very limited.
 *        it supports very primitive way to call a function via a variable.
 *        only user-defined functions are supported. neither builtin functions
 *        nor module functions are not supported yet.
 *   -----------------------------------------------------
 *   function x(a,b,c) { print a, b, c; }
 *   BEGIN { q = x; q(1, 2, 3); } # this works
 *   BEGIN { q[1]=x; q[1](1,2,3); } # this doesn't work. same as q[1] %% (1, 2, 3) or q[1] %% 3
 *   BEGIN { q[1]=x; y=q[1]; y(1,2,3); } # this works.
 *   -----------------------------------------------------
 *   function __printer(a,b,c) { print a, b, c; }
 *   function show(printer, a,b,c) { printer(a, b, c); }  
 *   BEGIN { show(__printer, 10, 20, 30); } ## passing the function value as an argumnet is ok.
 */
#define ENABLE_FEATURE_FUN_AS_VALUE

#define HAWK_MAX_GBLS 9999
#define HAWK_MAX_LCLS  9999
#define HAWK_MAX_PARAMS  9999

#define HAWK_DFL_RTX_STACK_LIMIT 5120
#define HAWK_MIN_RTX_STACK_LIMIT 512
#if (HAWK_SIZEOF_VOID_P <= 4)
#	define HAWK_MAX_RTX_STACK_LIMIT ((hawk_oow_t)1 << (HAWK_SIZEOF_VOID_P * 4 + 1))
#else
#	define HAWK_MAX_RTX_STACK_LIMIT ((hawk_oow_t)1 << (HAWK_SIZEOF_VOID_P * 4))
#endif

#define HAWK_BYTE_PRINTABLE(x) ((x) <= 0x7F && (x) != '\\' && hawk_is_bch_print(x))

#if defined(__has_builtin)

#	if (!__has_builtin(__builtin_memset) || !__has_builtin(__builtin_memcpy) || !__has_builtin(__builtin_memmove) || !__has_builtin(__builtin_memcmp))
#	include <string.h>
#	endif

#	if __has_builtin(__builtin_memset)
#		define HAWK_MEMSET(dst,src,size)  __builtin_memset(dst,src,size)
#	else
#		define HAWK_MEMSET(dst,src,size)  memset(dst,src,size)
#	endif
#	if __has_builtin(__builtin_memcpy)
#		define HAWK_MEMCPY(dst,src,size)  __builtin_memcpy(dst,src,size)
#	else
#		define HAWK_MEMCPY(dst,src,size)  memcpy(dst,src,size)
#	endif
#	if __has_builtin(__builtin_memmove)
#		define HAWK_MEMMOVE(dst,src,size)  __builtin_memmove(dst,src,size)
#	else
#		define HAWK_MEMMOVE(dst,src,size)  memmove(dst,src,size)
#	endif
#	if __has_builtin(__builtin_memcmp)
#		define HAWK_MEMCMP(dst,src,size)  __builtin_memcmp(dst,src,size)
#	else
#		define HAWK_MEMCMP(dst,src,size)  memcmp(dst,src,size)
#	endif

#else

#	if !defined(HAVE___BUILTIN_MEMSET) || !defined(HAVE___BUILTIN_MEMCPY) || !defined(HAVE___BUILTIN_MEMMOVE) || !defined(HAVE___BUILTIN_MEMCMP)
#	include <string.h>
#	endif

#	if defined(HAVE___BUILTIN_MEMSET)
#		define HAWK_MEMSET(dst,src,size)  __builtin_memset(dst,src,size)
#	else
#		define HAWK_MEMSET(dst,src,size)  memset(dst,src,size)
#	endif
#	if defined(HAVE___BUILTIN_MEMCPY)
#		define HAWK_MEMCPY(dst,src,size)  __builtin_memcpy(dst,src,size)
#	else
#		define HAWK_MEMCPY(dst,src,size)  memcpy(dst,src,size)
#	endif
#	if defined(HAVE___BUILTIN_MEMMOVE)
#		define HAWK_MEMMOVE(dst,src,size)  __builtin_memmove(dst,src,size)
#	else
#		define HAWK_MEMMOVE(dst,src,size)  memmove(dst,src,size)
#	endif
#	if defined(HAVE___BUILTIN_MEMCMP)
#		define HAWK_MEMCMP(dst,src,size)  __builtin_memcmp(dst,src,size)
#	else
#		define HAWK_MEMCMP(dst,src,size)  memcmp(dst,src,size)
#	endif

#endif

enum hawk_rio_type_t
{
	/* rio types available */
	HAWK_RIO_PIPE,
	HAWK_RIO_FILE,
	HAWK_RIO_CONSOLE,

	/* reserved for internal use only */
	HAWK_RIO_NUM
};

struct hawk_tree_t
{
	hawk_oow_t ngbls; /* total number of globals */
	hawk_oow_t ngbls_base; /* number of intrinsic globals */
	hawk_oocs_t cur_fun;
	hawk_htb_t* funs; /* awk function map */

	hawk_nde_t* begin;
	hawk_nde_t* begin_tail;

	hawk_nde_t* end;
	hawk_nde_t* end_tail;

	hawk_chain_t* chain;
	hawk_chain_t* chain_tail;
	hawk_oow_t chain_size; /* number of nodes in the chain */

	int ok;
};

typedef struct hawk_tok_t hawk_tok_t;
struct hawk_tok_t
{
	int           type;
	hawk_ooecs_t* name;
	hawk_loc_t    loc;
};

struct hawk_t
{
	/* exposed fields via hawk_alt_t */
	HAWK_HDR;

	/* primitive functions */
	hawk_prm_t  prm;

	/* options */
	struct
	{
		int trait;
		hawk_oocs_t mod[2];
		hawk_oocs_t incldirs;

		union
		{
			hawk_oow_t a[7];
			struct
			{
				hawk_oow_t incl;
				hawk_oow_t block_parse;
				hawk_oow_t block_run;
				hawk_oow_t expr_parse;
				hawk_oow_t expr_run;
				hawk_oow_t rex_build;
				hawk_oow_t rex_match;
			} s;
		} depth;

		hawk_oow_t rtx_stack_limit;
		hawk_oow_t log_mask;
		hawk_oow_t log_maxcapa;
	} opt;

	/* parse tree */
	hawk_tree_t tree;

	/* temporary information that the parser needs */
	struct
	{
		struct
		{
			int block;
			int loop;
			int stmt; /* statement */
		} id;

		struct
		{
			hawk_oow_t block;
			hawk_oow_t loop;
			hawk_oow_t expr; /* expression */
			hawk_oow_t incl;
		} depth;

		/* current pragma values */
		struct
		{
			int trait;
			hawk_oow_t rtx_stack_limit;
			hawk_ooch_t entry[128];
		} pragma;

		/* function calls */
		hawk_htb_t* funs;

		/* named variables */
		hawk_htb_t* named;

		/* global variables */
		hawk_arr_t* gbls;

		/* local variables */
		hawk_arr_t* lcls;

		/* parameters to a function */
		hawk_arr_t* params;

		/* maximum number of local variables */
		hawk_oow_t nlcls_max;

		/* some data to find if an expression is
		 * enclosed in parentheses or not.
		 * see parse_primary_lparen() and parse_print() in parse.c
		 */
		hawk_oow_t lparen_seq;
		hawk_oow_t lparen_last_closed;

		struct
		{
			hawk_uint8_t* ptr;
			hawk_oow_t count;
			hawk_oow_t capa;
		} incl_hist;
	} parse;

	/* source code management */
	struct
	{
		hawk_sio_impl_t inf;
		hawk_sio_impl_t outf;

		hawk_sio_lxc_t last; 

		hawk_oow_t nungots;
		hawk_sio_lxc_t ungot[5];

		hawk_sio_arg_t arg; /* for the top level source */
		hawk_sio_arg_t* inp; /* current input argument. */
	} sio;
	hawk_link_t* sio_names;

	/* previous token */
	hawk_tok_t ptok;
	/* current token */
	hawk_tok_t tok;
	/* look-ahead token */
	hawk_tok_t ntok;

	/* intrinsic functions */
	struct
	{
		hawk_fnc_t* sys;
		hawk_htb_t* user;
	} fnc;

	struct
	{
		hawk_ooch_t fmt[1024];
	} tmp;

	/* housekeeping */
	hawk_errstr_t errstr;
	hawk_oow_t    errmsg_len; /* used by errbfmt() and errufmt(). don't rely on this. some other funtions don't set this properly */
	hawk_ooch_t   errmsg_backup[HAWK_ERRMSG_CAPA];
#if defined(HAWK_OOCH_IS_BCH)
	hawk_uch_t    xerrmsg[HAWK_ERRMSG_CAPA];
#else
	hawk_bch_t    xerrmsg[HAWK_ERRMSG_CAPA * 2];
#endif

	struct
	{
		hawk_ooch_t* ptr;
		hawk_oow_t len;
		hawk_oow_t capa;
		hawk_bitmask_t last_mask;
		hawk_bitmask_t default_type_mask;
	} log;
	int shuterr;

	int haltall;
	hawk_ecb_t* ecb;

	hawk_rbt_t* modtab;
};

struct hawk_chain_t
{
	hawk_nde_t* pattern;
	hawk_nde_t* action;
	hawk_chain_t* next;
};

#define RTX_STACK_AT(rtx,n) ((rtx)->stack[(rtx)->stack_base+(n)])
#define RTX_STACK_NARGS(rtx) RTX_STACK_AT(rtx,3)
#define RTX_STACK_ARG(rtx,n) RTX_STACK_AT(rtx,3+1+(n))
#define RTX_STACK_LCL(rtx,n) RTX_STACK_AT(rtx,3+(hawk_oow_t)RTX_STACK_NARGS(rtx)+1+(n))
#define RTX_STACK_RETVAL(rtx) RTX_STACK_AT(rtx,2)
#define RTX_STACK_GBL(rtx,n) ((rtx)->stack[(n)])
#define RTX_STACK_RETVAL_GBL(rtx) ((rtx)->stack[(rtx)->hawk->tree.ngbls+2])

struct hawk_rtx_t
{
	HAWK_RTX_HDR;

	hawk_htb_t* named;

	void** stack;
	hawk_oow_t stack_top;
	hawk_oow_t stack_base;
	hawk_oow_t stack_limit;
	int exit_level;

	hawk_val_ref_t* rcache[128];
	hawk_oow_t rcache_count;

#if defined(ENABLE_FEATURE_SCACHE)
	hawk_val_str_t* scache[FEATURE_SCACHE_NUM_BLOCKS][FEATURE_SCACHE_BLOCK_SIZE];
	hawk_oow_t scache_count[FEATURE_SCACHE_NUM_BLOCKS];
#endif

	struct
	{
		hawk_val_int_t* ifree;
		hawk_val_chunk_t* ichunk;
		hawk_val_flt_t* rfree;
		hawk_val_chunk_t* rchunk;
	} vmgr;

	hawk_nde_blk_t* active_block;
	hawk_uint8_t* pattern_range_state;

	struct
	{
		hawk_ooch_t buf[1024];
		hawk_oow_t buf_pos;
		hawk_oow_t buf_len;
		int        eof;

		hawk_ooecs_t line; /* entire line */
		hawk_ooecs_t linew; /* line for manipulation, if necessary */
		hawk_ooecs_t lineg; /* line buffer for getline */
		hawk_becs_t linegb; /* line buffer for getline mbs */

		hawk_val_t* d0; /* $0 */

		hawk_oow_t maxflds;
		hawk_oow_t nflds; /* NF */
		struct
		{
			const hawk_ooch_t* ptr;
			hawk_oow_t         len;
			hawk_val_t*        val; /* $1 .. $NF */
		}* flds;
	} inrec;

	hawk_nrflt_t nrflt;

	struct
	{
		void* rs[2];
		void* fs[2]; 
		int ignorecase;
		int striprecspc;
		int stripstrspc;

		hawk_int_t nr;
		hawk_int_t fnr;

		hawk_oocs_t convfmt;
		hawk_oocs_t ofmt;
		hawk_oocs_t ofs;
		hawk_oocs_t ors;
		hawk_oocs_t subsep;
	} gbl;

	/* rio chain */
	struct
	{
		hawk_rio_impl_t handler[HAWK_RIO_NUM];
		hawk_rio_arg_t* chain;
	} rio;

	struct
	{
		hawk_ooecs_t fmt;
		hawk_ooecs_t out;

		struct
		{
			hawk_ooch_t* ptr;
			hawk_oow_t len; /* length */
			hawk_oow_t inc; /* increment */
		} tmp;
	} format;

	struct
	{
		hawk_becs_t fmt;
		hawk_becs_t out;

		struct
		{
			hawk_bch_t* ptr;
			hawk_oow_t len; /* length */
			hawk_oow_t inc; /* increment */
		} tmp;
	} formatmbs;

	struct
	{
		hawk_oow_t block;
		hawk_oow_t expr; /* expression */
	} depth;

	hawk_oow_t  errmsg_len; /* used by errbfmt() and errufmt(). don't rely on this. some other funtions don't set this properly */
	hawk_ooch_t errmsg_backup[HAWK_ERRMSG_CAPA];
#if defined(HAWK_OOCH_IS_BCH)
	hawk_uch_t  xerrmsg[HAWK_ERRMSG_CAPA];
#else
	hawk_bch_t  xerrmsg[HAWK_ERRMSG_CAPA * 2];
#endif

	hawk_rtx_ecb_t* ecb;
};

typedef struct hawk_mod_data_t hawk_mod_data_t;
struct hawk_mod_data_t
{
	void* handle;
	hawk_mod_t mod;
};

#define HAWK_RTX_INIT_REF_VAL(refval, _id, _adr, _nrefs) \
	do { \
		(refval)->v_type = HAWK_VAL_REF; \
		(refval)->ref = (_nrefs); \
		(refval)->stat = 0; \
		(refval)->nstr = 0; \
		(refval)->fcb = 0; \
		(refval)->id = (_id); \
		(refval)->adr = (_adr); \
	} while(0);


#define HAWK_RTX_IS_STRIPRECSPC_ON(rtx) ((rtx)->gbl.striprecspc > 0 || ((rtx)->gbl.striprecspc < 0 && ((rtx)->hawk->parse.pragma.trait & HAWK_STRIPRECSPC)))
#define HAWK_RTX_IS_STRIPSTRSPC_ON(rtx) ((rtx)->gbl.stripstrspc > 0 || ((rtx)->gbl.stripstrspc < 0 && ((rtx)->hawk->parse.pragma.trait & HAWK_STRIPSTRSPC)))

#if defined(__cplusplus)
extern "C" {
#endif

int hawk_init (hawk_t* awk, hawk_mmgr_t* mmgr, hawk_cmgr_t* cmgr, const hawk_prm_t* prm);
void hawk_fini (hawk_t* awk);

#if defined(__cplusplus)
}
#endif

#endif
