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

static void free_fun (hawk_htb_t* map, void* vptr, hawk_oow_t vlen)
{
	hawk_t* awk = *(hawk_t**)(map + 1);
	hawk_fun_t* f = (hawk_fun_t*)vptr;

	/* f->name doesn't have to be freed */
	/*hawk_freemem (awk, f->name);*/

	if (f->argspec) hawk_freemem (awk, f->argspec);
	hawk_clrpt (awk, f->body);
	hawk_freemem (awk, f);
}

static void free_fnc (hawk_htb_t* map, void* vptr, hawk_oow_t vlen)
{
	hawk_t* awk = *(hawk_t**)(map + 1);
	hawk_fnc_t* f = (hawk_fnc_t*)vptr;
	hawk_freemem (awk, f);
}

static int init_token (hawk_t* awk, hawk_tok_t* tok)
{
	tok->name = hawk_ooecs_open(awk, 0, 128);
	if (!tok->name) return -1;
	
	tok->type = 0;
	tok->loc.file = HAWK_NULL;
	tok->loc.line = 0;
	tok->loc.colm = 0;

	return 0;
}

static void fini_token (hawk_tok_t* tok)
{
	if (tok->name)
	{
		hawk_ooecs_close (tok->name);
		tok->name = HAWK_NULL;
	}
}

static void clear_token (hawk_tok_t* tok)
{
	if (tok->name) hawk_ooecs_clear (tok->name);
	tok->type = 0;
	tok->loc.file = HAWK_NULL;
	tok->loc.line = 0;
	tok->loc.colm = 0;
}

hawk_t* hawk_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, const hawk_prm_t* prm, hawk_errnum_t* errnum)
{
	hawk_t* awk;

	awk = (hawk_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(hawk_t) + xtnsize);
	if (awk)
	{
		int xret;

		xret = hawk_init(awk, mmgr, cmgr, prm);
		if (xret <= -1)
		{
			if (errnum) *errnum = hawk_geterrnum(awk);
			HAWK_MMGR_FREE (mmgr, awk);
			awk = HAWK_NULL;
		}
		else HAWK_MEMSET (awk + 1, 0, xtnsize);
	}
	else if (errnum) *errnum = HAWK_ENOMEM;

	return awk;
}

void hawk_close (hawk_t* hawk)
{
	hawk_fini (hawk);
	HAWK_MMGR_FREE (hawk_getmmgr(hawk), hawk);
}

int hawk_init (hawk_t* awk, hawk_mmgr_t* mmgr, hawk_cmgr_t* cmgr, const hawk_prm_t* prm)
{
	static hawk_htb_style_t treefuncbs =
	{
		{
			HAWK_HTB_COPIER_INLINE,
			HAWK_HTB_COPIER_DEFAULT 
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			free_fun
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	};

	static hawk_htb_style_t fncusercbs =
	{
		{
			HAWK_HTB_COPIER_INLINE,
			HAWK_HTB_COPIER_DEFAULT 
		},
		{
			HAWK_HTB_FREEER_DEFAULT,
			free_fnc
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	};

	/* zero out the object */
	HAWK_MEMSET (awk, 0, HAWK_SIZEOF(*awk));

	/* remember the memory manager */
	awk->_instsize = HAWK_SIZEOF(*awk);
	awk->_gem.mmgr = mmgr;
	awk->_gem.cmgr = cmgr;

	/* initialize error handling fields */
	awk->_gem.errnum = HAWK_ENOERR;
	awk->_gem.errmsg[0] = '\0';
	awk->_gem.errloc.line = 0;
	awk->_gem.errloc.colm = 0;
	awk->_gem.errloc.file = HAWK_NULL;
	awk->errstr = hawk_dflerrstr;
	awk->haltall = 0;

	/* progagate the primitive functions */
	HAWK_ASSERT (awk, prm           != HAWK_NULL);
	HAWK_ASSERT (awk, prm->math.pow != HAWK_NULL);
	HAWK_ASSERT (awk, prm->math.mod != HAWK_NULL);
	if (prm           == HAWK_NULL || 
	    prm->math.pow == HAWK_NULL ||
	    prm->math.mod == HAWK_NULL)
	{
		hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
		goto oops;
	}
	awk->prm = *prm;

	if (init_token(awk, &awk->ptok) <= -1 ||
	    init_token(awk, &awk->tok) <= -1 ||
	    init_token(awk, &awk->ntok) <= -1) 
	{
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		goto oops;
	}

	awk->opt.trait = HAWK_MODERN;
#if defined(__OS2__) || defined(_WIN32) || defined(__DOS__)
	awk->opt.trait |= HAWK_CRLF;
#endif
	awk->opt.rtx_stack_limit = HAWK_DFL_RTX_STACK_LIMIT;

	awk->tree.ngbls = 0;
	awk->tree.ngbls_base = 0;
	awk->tree.begin = HAWK_NULL;
	awk->tree.begin_tail = HAWK_NULL;
	awk->tree.end = HAWK_NULL;
	awk->tree.end_tail = HAWK_NULL;
	awk->tree.chain = HAWK_NULL;
	awk->tree.chain_tail = HAWK_NULL;
	awk->tree.chain_size = 0;

	/* TODO: initial map size?? */
	awk->tree.funs = hawk_htb_open(awk, HAWK_SIZEOF(awk), 512, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	awk->parse.funs = hawk_htb_open(awk, HAWK_SIZEOF(awk), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	awk->parse.named = hawk_htb_open(awk, HAWK_SIZEOF(awk), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1);

	awk->parse.gbls = hawk_arr_open(awk, HAWK_SIZEOF(awk), 128);
	awk->parse.lcls = hawk_arr_open(awk, HAWK_SIZEOF(awk), 64);
	awk->parse.params = hawk_arr_open(awk, HAWK_SIZEOF(awk), 32);

	awk->fnc.sys = HAWK_NULL;
	awk->fnc.user = hawk_htb_open(awk, HAWK_SIZEOF(awk), 512, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	awk->modtab = hawk_rbt_open(awk, 0, HAWK_SIZEOF(hawk_ooch_t), 1);

	if (awk->tree.funs == HAWK_NULL ||
	    awk->parse.funs == HAWK_NULL ||
	    awk->parse.named == HAWK_NULL ||
	    awk->parse.gbls == HAWK_NULL ||
	    awk->parse.lcls == HAWK_NULL ||
	    awk->parse.params == HAWK_NULL ||
	    awk->fnc.user == HAWK_NULL ||
	    awk->modtab == HAWK_NULL) 
	{
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		goto oops;
	}

	*(hawk_t**)(awk->tree.funs + 1) = awk;
	hawk_htb_setstyle (awk->tree.funs, &treefuncbs);

	*(hawk_t**)(awk->parse.funs + 1) = awk;
	hawk_htb_setstyle (awk->parse.funs, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_KEY_COPIER));

	*(hawk_t**)(awk->parse.named + 1) = awk;
	hawk_htb_setstyle (awk->parse.named, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_KEY_COPIER));

	*(hawk_t**)(awk->parse.gbls + 1) = awk;
	hawk_arr_setscale (awk->parse.gbls, HAWK_SIZEOF(hawk_ooch_t));
	hawk_arr_setcopier (awk->parse.gbls, HAWK_ARR_COPIER_INLINE);

	*(hawk_t**)(awk->parse.lcls + 1) = awk;
	hawk_arr_setscale (awk->parse.lcls, HAWK_SIZEOF(hawk_ooch_t));
	hawk_arr_setcopier (awk->parse.lcls, HAWK_ARR_COPIER_INLINE);

	*(hawk_t**)(awk->parse.params + 1) = awk;
	hawk_arr_setscale (awk->parse.params, HAWK_SIZEOF(hawk_ooch_t));
	hawk_arr_setcopier (awk->parse.params, HAWK_ARR_COPIER_INLINE);

	*(hawk_t**)(awk->fnc.user + 1) = awk;
	hawk_htb_setstyle (awk->fnc.user, &fncusercbs);

	hawk_rbt_setstyle (awk->modtab, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	if (hawk_initgbls(awk) <= -1) 
	{
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		goto oops;
	}

	return 0;

oops:
	if (awk->modtab) hawk_rbt_close (awk->modtab);
	if (awk->fnc.user) hawk_htb_close (awk->fnc.user);
	if (awk->parse.params) hawk_arr_close (awk->parse.params);
	if (awk->parse.lcls) hawk_arr_close (awk->parse.lcls);
	if (awk->parse.gbls) hawk_arr_close (awk->parse.gbls);
	if (awk->parse.named) hawk_htb_close (awk->parse.named);
	if (awk->parse.funs) hawk_htb_close (awk->parse.funs);
	if (awk->tree.funs) hawk_htb_close (awk->tree.funs);
	fini_token (&awk->ntok);
	fini_token (&awk->tok);
	fini_token (&awk->ptok);

	return -1;
}

void hawk_fini (hawk_t* awk)
{
	hawk_ecb_t* ecb;
	int i;

	hawk_clear (awk);
	/*hawk_clrfnc (awk);*/

	for (ecb = awk->ecb; ecb; ecb = ecb->next)
		if (ecb->close) ecb->close (awk);

	hawk_rbt_close (awk->modtab);
	hawk_htb_close (awk->fnc.user);

	hawk_arr_close (awk->parse.params);
	hawk_arr_close (awk->parse.lcls);
	hawk_arr_close (awk->parse.gbls);
	hawk_htb_close (awk->parse.named);
	hawk_htb_close (awk->parse.funs);

	hawk_htb_close (awk->tree.funs);

	fini_token (&awk->ntok);
	fini_token (&awk->tok);
	fini_token (&awk->ptok);

	if (awk->parse.incl_hist.ptr) hawk_freemem (awk, awk->parse.incl_hist.ptr);
	hawk_clearsionames (awk);

	/* destroy dynamically allocated options */
	for (i = 0; i < HAWK_COUNTOF(awk->opt.mod); i++)
	{
		if (awk->opt.mod[i].ptr) hawk_freemem (awk, awk->opt.mod[i].ptr);
	}
}

static hawk_rbt_walk_t unload_module (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, void* ctx)
{
	hawk_t* awk = (hawk_t*)ctx;
	hawk_mod_data_t* md;

	md = HAWK_RBT_VPTR(pair);	
	if (md->mod.unload) md->mod.unload (&md->mod, awk);
	if (md->handle) awk->prm.modclose (awk, md->handle);

	return HAWK_RBT_WALK_FORWARD;
}

void hawk_clear (hawk_t* awk)
{
	hawk_ecb_t* ecb;

	for (ecb = awk->ecb; ecb; ecb = ecb->next)
	{
		if (ecb->clear) ecb->clear (awk);
	}

	awk->haltall = 0;

	clear_token (&awk->tok);
	clear_token (&awk->ntok);
	clear_token (&awk->ptok);

	/* clear all loaded modules */
	hawk_rbt_walk (awk->modtab, unload_module, awk);
	hawk_rbt_clear (awk->modtab);

	HAWK_ASSERT (awk, HAWK_ARR_SIZE(awk->parse.gbls) == awk->tree.ngbls);
	/* delete all non-builtin global variables */
	hawk_arr_delete (
		awk->parse.gbls, awk->tree.ngbls_base, 
		HAWK_ARR_SIZE(awk->parse.gbls) - awk->tree.ngbls_base);

	hawk_arr_clear (awk->parse.lcls);
	hawk_arr_clear (awk->parse.params);
	hawk_htb_clear (awk->parse.named);
	hawk_htb_clear (awk->parse.funs);

	awk->parse.nlcls_max = 0; 
	awk->parse.depth.block = 0;
	awk->parse.depth.loop = 0;
	awk->parse.depth.expr = 0;
	awk->parse.depth.incl = 0;
	awk->parse.pragma.trait = (awk->opt.trait & HAWK_IMPLICIT); 
	awk->parse.pragma.rtx_stack_limit = 0;

	awk->parse.incl_hist.count =0;

	/* clear parse trees */
	/*awk->tree.ngbls_base = 0;
	awk->tree.ngbls = 0; */
	awk->tree.ngbls = awk->tree.ngbls_base;

	awk->tree.cur_fun.ptr = HAWK_NULL;
	awk->tree.cur_fun.len = 0;
	hawk_htb_clear (awk->tree.funs);

	if (awk->tree.begin) 
	{
		/*HAWK_ASSERT (awk, awk->tree.begin->next == HAWK_NULL);*/
		hawk_clrpt (awk, awk->tree.begin);
		awk->tree.begin = HAWK_NULL;
		awk->tree.begin_tail = HAWK_NULL;
	}

	if (awk->tree.end) 
	{
		/*HAWK_ASSERT (awk, awk->tree.end->next == HAWK_NULL);*/
		hawk_clrpt (awk, awk->tree.end);
		awk->tree.end = HAWK_NULL;
		awk->tree.end_tail = HAWK_NULL;
	}

	while (awk->tree.chain) 
	{
		hawk_chain_t* next = awk->tree.chain->next;
		if (awk->tree.chain->pattern) hawk_clrpt (awk, awk->tree.chain->pattern);
		if (awk->tree.chain->action) hawk_clrpt (awk, awk->tree.chain->action);
		hawk_freemem (awk, awk->tree.chain);
		awk->tree.chain = next;
	}

	awk->tree.chain_tail = HAWK_NULL;
	awk->tree.chain_size = 0;

	/* this table must not be cleared here as there can be a reference
	 * to an entry of this table from errinf.loc.file when hawk_parse() 
	 * failed. this table is cleared in hawk_parse().
	 * hawk_claersionames (awk);
	 */
}

void hawk_getprm (hawk_t* awk, hawk_prm_t* prm)
{
	*prm = awk->prm;
}

void hawk_setprm (hawk_t* awk, const hawk_prm_t* prm)
{
	awk->prm = *prm;
}

static int dup_str_opt (hawk_t* awk, const void* value, hawk_oocs_t* tmp)
{
	if (value)
	{
		tmp->ptr = hawk_dupoocstr(awk, value, &tmp->len);
		if (!tmp->ptr) return -1;
	}
	else
	{
		tmp->ptr = HAWK_NULL;
		tmp->len = 0;
	}

	return 0;
}

int hawk_setopt (hawk_t* awk, hawk_opt_t id, const void* value)
{
	switch (id)
	{
		case HAWK_TRAIT:
			awk->opt.trait = *(const int*)value;
			return 0;

		case HAWK_MODPREFIX:
		case HAWK_MODPOSTFIX:
		{
			hawk_oocs_t tmp;
			int idx;

			if (dup_str_opt (awk, value, &tmp) <= -1) return -1;

			idx = id - HAWK_MODPREFIX;
			if (awk->opt.mod[idx].ptr) hawk_freemem (awk, awk->opt.mod[idx].ptr);

			awk->opt.mod[idx] = tmp;
			return 0;
		}

		case HAWK_INCLUDEDIRS:
		{
			hawk_oocs_t tmp;
			if (dup_str_opt (awk, value, &tmp) <= -1) return -1;
			if (awk->opt.incldirs.ptr) hawk_freemem (awk, awk->opt.incldirs.ptr);
			awk->opt.incldirs = tmp;
			return 0;
		}

		case HAWK_DEPTH_INCLUDE:
		case HAWK_DEPTH_BLOCK_PARSE:
		case HAWK_DEPTH_BLOCK_RUN:
		case HAWK_DEPTH_EXPR_PARSE:
		case HAWK_DEPTH_EXPR_RUN:
		case HAWK_DEPTH_REX_BUILD:
		case HAWK_DEPTH_REX_MATCH:
			awk->opt.depth.a[id - HAWK_DEPTH_INCLUDE] = *(const hawk_oow_t*)value;
			return 0;

		case HAWK_RTX_STACK_LIMIT:
			awk->opt.rtx_stack_limit = *(const hawk_oow_t*)value;
			if (awk->opt.rtx_stack_limit < HAWK_MIN_RTX_STACK_LIMIT) awk->opt.rtx_stack_limit = HAWK_MIN_RTX_STACK_LIMIT;
			else if (awk->opt.rtx_stack_limit > HAWK_MAX_RTX_STACK_LIMIT) awk->opt.rtx_stack_limit = HAWK_MAX_RTX_STACK_LIMIT;
			return 0;
	}

	hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
	return -1;
}

int hawk_getopt (hawk_t* awk, hawk_opt_t id, void* value)
{
	switch  (id)
	{
		case HAWK_TRAIT:
			*(int*)value = awk->opt.trait;
			return 0;

		case HAWK_MODPREFIX:
		case HAWK_MODPOSTFIX:
			*(const hawk_ooch_t**)value = awk->opt.mod[id - HAWK_MODPREFIX].ptr;
			return 0;

		case HAWK_INCLUDEDIRS:
			*(const hawk_ooch_t**)value = awk->opt.incldirs.ptr;
			return 0;

		case HAWK_DEPTH_INCLUDE:
		case HAWK_DEPTH_BLOCK_PARSE:
		case HAWK_DEPTH_BLOCK_RUN:
		case HAWK_DEPTH_EXPR_PARSE:
		case HAWK_DEPTH_EXPR_RUN:
		case HAWK_DEPTH_REX_BUILD:
		case HAWK_DEPTH_REX_MATCH:
			*(hawk_oow_t*)value = awk->opt.depth.a[id - HAWK_DEPTH_INCLUDE];
			return 0;

		case HAWK_RTX_STACK_LIMIT:
			*(hawk_oow_t*)value = awk->opt.rtx_stack_limit;
			return 0;
	};

	hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
	return -1;
}

void hawk_haltall (hawk_t* awk)
{
	awk->haltall = 1;
	hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
}

hawk_ecb_t* hawk_popecb (hawk_t* awk)
{
	hawk_ecb_t* top = awk->ecb;
	if (top) awk->ecb = top->next;
	return top;
}

void hawk_pushecb (hawk_t* awk, hawk_ecb_t* ecb)
{
	ecb->next = awk->ecb;
	awk->ecb = ecb;
}

/* ----------------------------------------------------------------------- */

int hawk_convbtouchars (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all)
{
	/* length bound */
	int n;
	n = hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, hawk_getcmgr(hawk), all);
	/* -1: illegal character, -2: buffer too small, -3: incomplete sequence */
	if (n <= -1) hawk_seterrnum (hawk, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR, HAWK_NULL);
	return n;
}

int hawk_convutobchars (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	int n;
	n = hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, hawk_getcmgr(hawk));
	if (n <= -1) hawk_seterrnum (hawk, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR, HAWK_NULL);
	return n;
}

int hawk_convbtoucstr (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all)
{
	/* null-terminated. */
	int n;
	n = hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, hawk_getcmgr(hawk), all);
	if (n <= -1) hawk_seterrnum (hawk, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR, HAWK_NULL);
	return n;
}

int hawk_convutobcstr (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	int n;
	n = hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, hawk_getcmgr(hawk));
	if (n <= -1) hawk_seterrnum (hawk, (n == -2)? HAWK_EBUFFULL: HAWK_EECERR, HAWK_NULL);
	return n;
}

/* ----------------------------------------------------------------------- */

hawk_uch_t* hawk_dupbtoucharswithheadroom (hawk_t* hawk, hawk_oow_t headroom_bytes, const hawk_bch_t* bcs, hawk_oow_t bcslen, hawk_oow_t* ucslen, int all)
{
	hawk_oow_t inlen, outlen;
	hawk_uch_t* ptr;

	inlen = bcslen;
	if (hawk_convbtouchars(hawk, bcs, &inlen, HAWK_NULL, &outlen, all) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HAWK_NULL;
	}

	ptr = (hawk_uch_t*)hawk_allocmem(hawk, headroom_bytes + ((outlen + 1) * HAWK_SIZEOF(*ptr)));
	if (!ptr) return HAWK_NULL;

	inlen = bcslen;

	ptr = (hawk_uch_t*)((hawk_oob_t*)ptr + headroom_bytes);
	hawk_convbtouchars (hawk, bcs, &inlen, ptr, &outlen, all);

	/* hawk_convbtouchars() doesn't null-terminate the target. 
	 * but in hawk_dupbtouchars(), i allocate space. so i don't mind
	 * null-terminating it with 1 extra character overhead */
	ptr[outlen] = '\0'; 
	if (ucslen) *ucslen = outlen;
	return ptr;
}

hawk_bch_t* hawk_duputobcharswithheadroom (hawk_t* hawk, hawk_oow_t headroom_bytes, const hawk_uch_t* ucs, hawk_oow_t ucslen, hawk_oow_t* bcslen)
{
	hawk_oow_t inlen, outlen;
	hawk_bch_t* ptr;

	inlen = ucslen;
	if (hawk_convutobchars(hawk, ucs, &inlen, HAWK_NULL, &outlen) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HAWK_NULL;
	}

	ptr = (hawk_bch_t*)hawk_allocmem(hawk, headroom_bytes + ((outlen + 1) * HAWK_SIZEOF(*ptr)));
	if (!ptr) return HAWK_NULL;

	inlen = ucslen;
	ptr = (hawk_bch_t*)((hawk_oob_t*)ptr + headroom_bytes);
	hawk_convutobchars (hawk, ucs, &inlen, ptr, &outlen);

	ptr[outlen] = '\0';
	if (bcslen) *bcslen = outlen;
	return ptr;
}

/* ----------------------------------------------------------------------- */

hawk_uch_t* hawk_dupbtoucstrwithheadroom (hawk_t* hawk, hawk_oow_t headroom_bytes, const hawk_bch_t* bcs, hawk_oow_t* ucslen, int all)
{
	hawk_oow_t inlen, outlen;
	hawk_uch_t* ptr;

	if (hawk_convbtoucstr(hawk, bcs, &inlen, HAWK_NULL, &outlen, all) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HAWK_NULL;
	}

	outlen++;
	ptr = (hawk_uch_t*)hawk_allocmem(hawk, headroom_bytes + (outlen * HAWK_SIZEOF(hawk_uch_t)));
	if (!ptr) return HAWK_NULL;

	hawk_convbtoucstr (hawk, bcs, &inlen, ptr, &outlen, all);
	if (ucslen) *ucslen = outlen;
	return ptr;
}

hawk_bch_t* hawk_duputobcstrwithheadroom (hawk_t* hawk, hawk_oow_t headroom_bytes, const hawk_uch_t* ucs, hawk_oow_t* bcslen)
{
	hawk_oow_t inlen, outlen;
	hawk_bch_t* ptr;

	if (hawk_convutobcstr(hawk, ucs, &inlen, HAWK_NULL, &outlen) <= -1) 
	{
		/* note it's also an error if no full conversion is made in this function */
		return HAWK_NULL;
	}

	outlen++;
	ptr = (hawk_bch_t*)hawk_allocmem(hawk, headroom_bytes + (outlen * HAWK_SIZEOF(hawk_bch_t)));
	if (!ptr) return HAWK_NULL;

	ptr = (hawk_bch_t*)((hawk_oob_t*)ptr + headroom_bytes);

	hawk_convutobcstr (hawk, ucs, &inlen, ptr, &outlen);
	if (bcslen) *bcslen = outlen;
	return ptr;
}

/* ----------------------------------------------------------------------- */

hawk_bch_t* hawk_dupucstrarrtobcstr (hawk_t* hawk, const hawk_uch_t* ucs[], hawk_oow_t* bcslen)
{
	hawk_oow_t wl, ml, capa, pos, i;
	hawk_bch_t* bcs;

	for (capa = 0, i = 0; ucs[i]; i++)
	{
		if (hawk_convutobcstr(hawk, ucs[i], &wl, HAWK_NULL, &ml) <=  -1)  return HAWK_NULL;
		capa += ml;
	}

	bcs = (hawk_bch_t*)hawk_allocmem(hawk, (capa + 1) * HAWK_SIZEOF(*bcs));
	if (!bcs) return HAWK_NULL;

	for (pos = 0, i = 0; ucs[i]; i++)
	{
		ml = capa - pos + 1;
		hawk_convutobcstr (hawk, ucs[i], &wl, &bcs[pos], &ml);
		pos += ml;
	}

	if (bcslen) *bcslen = capa;
	return bcs;
}

/* ----------------------------------------------------------------------- */

struct fmt_uch_buf_t
{
	hawk_t* hawk;
	hawk_uch_t* ptr;
	hawk_oow_t len;
	hawk_oow_t capa;
};
typedef struct fmt_uch_buf_t fmt_uch_buf_t;

static int fmt_put_bchars_to_uch_buf (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	fmt_uch_buf_t* b = (fmt_uch_buf_t*)fmtout->ctx;
	hawk_oow_t bcslen, ucslen;
	int n;

	bcslen = b->capa - b->len;
	ucslen = len;
	n = hawk_conv_bchars_to_uchars_with_cmgr(ptr, &bcslen, &b->ptr[b->len], &ucslen, hawk_getcmgr(b->hawk), 1);
	if (n <= -1) 
	{
		if (n == -2) 
		{
			return 0; /* buffer full. stop */
		}
		else
		{
			hawk_seterrnum (b->hawk, HAWK_EECERR, HAWK_NULL);
			return -1;
		}
	}

	return 1; /* success. carry on */
}

static int fmt_put_uchars_to_uch_buf (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	fmt_uch_buf_t* b = (fmt_uch_buf_t*)fmtout->ctx;
	hawk_oow_t n;

	/* this function null-terminates the destination. so give the restored buffer size */
	n = hawk_copy_uchars_to_ucstr(&b->ptr[b->len], b->capa - b->len + 1, ptr, len);
	b->len += n;
	if (n < len)
	{
		hawk_seterrnum (b->hawk, HAWK_EBUFFULL, HAWK_NULL);
		return 0; /* stop. insufficient buffer */
	}

	return 1; /* success */
}

hawk_oow_t hawk_vfmttoucstr (hawk_t* hawk, hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, va_list ap)
{
	hawk_fmtout_t fo;
	fmt_uch_buf_t fb;

	if (bsz <= 0) return 0;

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = hawk_getmmgr(hawk);
	fo.putbchars = fmt_put_bchars_to_uch_buf;
	fo.putuchars = fmt_put_uchars_to_uch_buf;
	fo.ctx = &fb;

	HAWK_MEMSET (&fb, 0, HAWK_SIZEOF(fb));
	fb.hawk = hawk;
	fb.ptr = buf;
	fb.capa = bsz - 1;

	if (hawk_ufmt_outv(&fo, fmt, ap) <= -1) return -1;

	buf[fb.len] = '\0';
	return fb.len;
}

hawk_oow_t hawk_fmttoucstr (hawk_t* hawk, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, ...)
{
	hawk_oow_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hawk_vfmttoucstr(hawk, buf, bufsz, fmt, ap);
	va_end (ap);

	return x;
}

/* ----------------------------------------------------------------------- */

struct fmt_bch_buf_t
{
	hawk_t* hawk;
	hawk_bch_t* ptr;
	hawk_oow_t len;
	hawk_oow_t capa;
};
typedef struct fmt_bch_buf_t fmt_bch_buf_t;


static int fmt_put_bchars_to_bch_buf (hawk_fmtout_t* fmtout, const hawk_bch_t* ptr, hawk_oow_t len)
{
	fmt_bch_buf_t* b = (fmt_bch_buf_t*)fmtout->ctx;
	hawk_oow_t n;

	/* this function null-terminates the destination. so give the restored buffer size */
	n = hawk_copy_bchars_to_bcstr(&b->ptr[b->len], b->capa - b->len + 1, ptr, len);
	b->len += n;
	if (n < len)
	{
		hawk_seterrnum (b->hawk, HAWK_EBUFFULL, HAWK_NULL);
		return 0; /* stop. insufficient buffer */
	}

	return 1; /* success */
}


static int fmt_put_uchars_to_bch_buf (hawk_fmtout_t* fmtout, const hawk_uch_t* ptr, hawk_oow_t len)
{
	fmt_bch_buf_t* b = (fmt_bch_buf_t*)fmtout->ctx;
	hawk_oow_t bcslen, ucslen;
	int n;

	bcslen = b->capa - b->len;
	ucslen = len;
	n = hawk_conv_uchars_to_bchars_with_cmgr(ptr, &ucslen, &b->ptr[b->len], &bcslen, hawk_getcmgr(b->hawk));
	b->len += bcslen;
	if (n <= -1)
	{
		if (n == -2)
		{
			return 0; /* buffer full. stop */
		}
		else
		{
			hawk_seterrnum (b->hawk, HAWK_EECERR, HAWK_NULL);
			return -1;
		}
	}

	return 1; /* success. carry on */
}

hawk_oow_t hawk_vfmttobcstr (hawk_t* hawk, hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, va_list ap)
{
	hawk_fmtout_t fo;
	fmt_bch_buf_t fb;

	if (bsz <= 0) return 0;

	HAWK_MEMSET (&fo, 0, HAWK_SIZEOF(fo));
	fo.mmgr = hawk_getmmgr(hawk);
	fo.putbchars = fmt_put_bchars_to_bch_buf;
	fo.putuchars = fmt_put_uchars_to_bch_buf;
	fo.ctx = &fb;

	HAWK_MEMSET (&fb, 0, HAWK_SIZEOF(fb));
	fb.hawk = hawk;
	fb.ptr = buf;
	fb.capa = bsz - 1;

	if (hawk_bfmt_outv(&fo, fmt, ap) <= -1) return -1;

	buf[fb.len] = '\0';
	return fb.len;
}

hawk_oow_t hawk_fmttobcstr (hawk_t* hawk, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, ...)
{
	hawk_oow_t x;
	va_list ap;

	va_start (ap, fmt);
	x = hawk_vfmttobcstr(hawk, buf, bufsz, fmt, ap);
	va_end (ap);

	return x;
}

