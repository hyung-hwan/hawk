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

#define HAWK_ENABLE_GC
#define HAWK_ENABLE_STR_CACHE
#define HAWK_ENABLE_MBS_CACHE
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
#define HAWK_ENABLE_FUN_AS_VALUE

/* ------------------------------------------------------------------------ */

/* gc configuration */
#define HAWK_GC_NUM_GENS (3)

/* string cache configuration */
#define HAWK_STR_CACHE_NUM_BLOCKS (16)
#define HAWK_STR_CACHE_BLOCK_UNIT (16)
#define HAWK_STR_CACHE_BLOCK_SIZE (128)

/* byte string cache configuration */
#define HAWK_MBS_CACHE_NUM_BLOCKS (16)
#define HAWK_MBS_CACHE_BLOCK_UNIT (16)
#define HAWK_MBS_CACHE_BLOCK_SIZE (128)

/* maximum number of globals, locals, parameters allowed in parsing */
#define HAWK_MAX_GBLS    (9999)
#define HAWK_MAX_LCLS    (9999)
#define HAWK_MAX_PARAMS  (9999)


/* runtime stack limit */
#define HAWK_DFL_RTX_STACK_LIMIT (5120)
#define HAWK_MIN_RTX_STACK_LIMIT (512)
#if (HAWK_SIZEOF_VOID_P <= 4)
#	define HAWK_MAX_RTX_STACK_LIMIT ((hawk_oow_t)1 << (HAWK_SIZEOF_VOID_P * 4 + 1))
#else
#	define HAWK_MAX_RTX_STACK_LIMIT ((hawk_oow_t)1 << (HAWK_SIZEOF_VOID_P * 4))
#endif

/* Don't forget to grow HAWK_IDX_BUF_SIZE if hawk_int_t is very large */
#if (HAWK_SIZEOF_INT_T <= 16) /* 128 bits */
#	define HAWK_IDX_BUF_SIZE 64
#elif (HAWK_SIZEOF_INT_T <= 32) /* 256 bits */
#	define HAWK_IDX_BUF_SIZE 128
#elif (HAWK_SIZEOF_INT_T <= 64) /* 512 bits */
#	define HAWK_IDX_BUF_SIZE 192
#elif (HAWK_SIZEOF_INT_T <= 128) /* 1024 bits */
#	define HAWK_IDX_BUF_SIZE 384
#elif (HAWK_SIZEOF_INT_T <= 256) /* 2048 bits */
#	define HAWK_IDX_BUF_SIZE 640
#else
#	error unsupported. hawk_int_t too big
#endif

/* ------------------------------------------------------------------------ */


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

	/* g++ 2.95 had a problem with __builtin_memxxx functions:
	 * implicit declaration of function `int HAWK::__builtin_memset(...)' */
#	if defined(__cplusplus) && defined(__GNUC__) && (__GNUC__ <= 2)
#		undef HAVE___BUILTIN_MEMSET
#		undef HAVE___BUILTIN_MEMCPY
#		undef HAVE___BUILTIN_MEMMOVE
#		undef HAVE___BUILTIN_MEMCMP
#	endif

#	if !defined(HAVE___BUILTIN_MEMSET) || \
	   !defined(HAVE___BUILTIN_MEMCPY) || \
	   !defined(HAVE___BUILTIN_MEMMOVE) || \
	   !defined(HAVE___BUILTIN_MEMCMP) 
#		include <string.h>
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


struct hawk_tree_t
{
	hawk_oow_t ngbls; /* total number of globals */
	hawk_oow_t ngbls_base; /* number of intrinsic globals */
	hawk_oocs_t cur_fun;
	hawk_htb_t* funs; /* hawk function map */

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
	int           flags;
	hawk_ooecs_t* name;
	hawk_loc_t    loc;
};

struct hawk_sbuf_t
{
	hawk_ooch_t* ptr;
	hawk_oow_t   len;
	hawk_oow_t   capa;
};
typedef struct hawk_sbuf_t hawk_sbuf_t;

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
		hawk_oocs_t mod[3];
		hawk_oocs_t includedirs;

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

	/* some temporary workspace */
	hawk_sbuf_t sbuf[HAWK_SBUF_COUNT];	

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
	hawk_ooch_t   errmsg_backup[HAWK_ERRMSG_CAPA];

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

typedef struct hawk_ctos_b_t hawk_ctos_b_t;
struct hawk_ctos_b_t
{
	hawk_oochu_t c[2]; /* ensure the unsigned type to hold not only a character but also a free slot index */
};

typedef struct hawk_bctos_b_t hawk_bctos_b_t;
struct hawk_bctos_b_t
{
	hawk_bchu_t c[2]; /* ensure the unsigned type to hold not only a byte character but also a free slot index */
};

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

#if defined(HAWK_ENABLE_STR_CACHE)
	hawk_val_str_t* str_cache[HAWK_STR_CACHE_NUM_BLOCKS][HAWK_STR_CACHE_BLOCK_SIZE];
	hawk_oow_t str_cache_count[HAWK_STR_CACHE_NUM_BLOCKS];
#endif

#if defined(HAWK_ENABLE_MBS_CACHE)
	hawk_val_mbs_t* mbs_cache[HAWK_MBS_CACHE_NUM_BLOCKS][HAWK_MBS_CACHE_BLOCK_SIZE];
	hawk_oow_t mbs_cache_count[HAWK_MBS_CACHE_NUM_BLOCKS];
#endif

	struct
	{
		hawk_val_int_t* ifree;
		hawk_val_chunk_t* ichunk;
		hawk_val_flt_t* rfree;
		hawk_val_chunk_t* rchunk;
	} vmgr;

	struct
	{
#if defined(HAWK_OOCH_IS_UCH)
		hawk_ctos_b_t b[512];
#else
		hawk_ctos_b_t b[256]; /* it must not be larger than 256 */
#endif
		hawk_oow_t fi;
	} ctos; /* char/nil to string conversion */

	struct
	{
		hawk_bctos_b_t b[256];
		hawk_oow_t fi;
	} bctos;

	struct
	{
		/* lists of values under gc management */
		hawk_gch_t g[HAWK_GC_NUM_GENS]; 

		/* 
		 * Pressure imposed on each generation before gc is triggered
		 *  pressure[0] - number of allocation attempt since the last gc
		 *  pressure[N] - nubmer of collections performed for generation N - 1. 
		 */
		hawk_oow_t pressure[HAWK_GC_NUM_GENS + 1];

		/* threshold to trigger generational collection. */
		hawk_oow_t threshold[HAWK_GC_NUM_GENS];
	} gc;

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
		int numstrdetect;

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
		hawk_becs_t bout;
		hawk_ooecs_t oout;
	} fnc; /* output buffer simple functions like gsub, sub, match*/

	struct
	{
		hawk_oow_t block;
		hawk_oow_t expr; /* expression */
	} depth;

	struct
	{
		hawk_val_t** ptr;
		hawk_oow_t size;
		hawk_oow_t capa;
	} forin; /* keys for for (x in y) ... */

	hawk_ooch_t errmsg_backup[HAWK_ERRMSG_CAPA];
	hawk_rtx_ecb_t* ecb;
};

typedef struct hawk_mod_data_t hawk_mod_data_t;
struct hawk_mod_data_t
{
	void* handle;
	hawk_mod_t mod;
};


#define HAWK_RTX_STACK_AT(rtx,n) ((rtx)->stack[(rtx)->stack_base+(n)])
#define HAWK_RTX_STACK_NARGS(rtx) HAWK_RTX_STACK_AT(rtx,3)
#define HAWK_RTX_STACK_ARG(rtx,n) HAWK_RTX_STACK_AT(rtx,3+1+(n))
#define HAWK_RTX_STACK_LCL(rtx,n) HAWK_RTX_STACK_AT(rtx,3+(hawk_oow_t)HAWK_RTX_STACK_NARGS(rtx)+1+(n))
#define HAWK_RTX_STACK_RETVAL(rtx) HAWK_RTX_STACK_AT(rtx,2)
#define HAWK_RTX_STACK_GBL(rtx,n) ((rtx)->stack[(n)])
#define HAWK_RTX_STACK_RETVAL_GBL(rtx) ((rtx)->stack[(rtx)->hawk->tree.ngbls+2])

#define HAWK_RTX_STACK_AVAIL(rtx) ((rtx)->stack_limit - (rtx)->stack_top)

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void HAWK_RTX_STACK_PUSH (hawk_rtx_t* rtx, hawk_val_t* val)
{
	/*HAWK_ASSERT (rtx->stack_top < rtx->stack_limit);*/
	rtx->stack[rtx->stack_top++] = val;
}

static HAWK_INLINE void HAWK_RTX_STACK_POP (hawk_rtx_t* rtx)
{
	/*HAWK_ASSERT (rtx->stack_top > rtx->stack_base);*/
	rtx->stack_top--;
}
#else
#define HAWK_RTX_STACK_PUSH(rtx,val) ((rtx)->stack[(rtx)->stack_top++] = val)
#define HAWK_RTX_STACK_POP(rtx) ((rtx)->stack_top--)
#endif


#define HAWK_RTX_INIT_REF_VAL(refval, _id, _adr, _nrefs) \
	do { \
		(refval)->v_type = HAWK_VAL_REF; \
		(refval)->v_refs = (_nrefs); \
		(refval)->v_static = 0; \
		(refval)->v_nstr = 0; \
		(refval)->v_gc = 0; \
		(refval)->id = (_id); \
		(refval)->adr = (_adr); \
	} while(0);


#define HAWK_RTX_IS_STRIPRECSPC_ON(rtx) ((rtx)->gbl.striprecspc > 0 || ((rtx)->gbl.striprecspc < 0 && ((rtx)->hawk->parse.pragma.trait & HAWK_STRIPRECSPC)))
#define HAWK_RTX_IS_STRIPSTRSPC_ON(rtx) ((rtx)->gbl.stripstrspc > 0 || ((rtx)->gbl.stripstrspc < 0 && ((rtx)->hawk->parse.pragma.trait & HAWK_STRIPSTRSPC)))
#define HAWK_RTX_IS_NUMSTRDETECT_ON(rtx) ((rtx)->gbl.numstrdetect > 0 || ((rtx)->gbl.stripstrspc < 0 && ((rtx)->hawk->parse.pragma.trait & HAWK_NUMSTRDETECT)))

#if !defined(HAWK_DEFAULT_MODLIBDIRS)
#	define HAWK_DEFAULT_MODLIBDIRS ""
#endif

#if !defined(HAWK_DEFAULT_MODPREFIX)
#	if defined(_WIN32)
#		define HAWK_DEFAULT_MODPREFIX "hawk-"
#	elif defined(__OS2__)
#		define HAWK_DEFAULT_MODPREFIX "hawk"
#	elif defined(__DOS__)
#		define HAWK_DEFAULT_MODPREFIX "hawk"
#	else
#		define HAWK_DEFAULT_MODPREFIX "libhawk-"
#	endif
#endif

#if !defined(HAWK_DEFAULT_MODPOSTFIX)
#	define HAWK_DEFAULT_MODPOSTFIX ""
#endif



#if defined(__cplusplus)
extern "C" {
#endif

int hawk_init (hawk_t* hawk, hawk_mmgr_t* mmgr, hawk_cmgr_t* cmgr, const hawk_prm_t* prm);
void hawk_fini (hawk_t* hawk);

#if defined(__cplusplus)
}
#endif

#endif
