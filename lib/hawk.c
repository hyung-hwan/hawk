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

static void free_fun (hawk_htb_t* map, void* vptr, hawk_oow_t vlen)
{
	hawk_t* hawk = *(hawk_t**)(map + 1);
	hawk_fun_t* f = (hawk_fun_t*)vptr;

	/* f->name doesn't have to be freed */
	/*hawk_freemem (hawk, f->name);*/

	if (f->argspec) hawk_freemem (hawk, f->argspec);
	hawk_clrpt (hawk, f->body);
	hawk_freemem (hawk, f);
}

static void free_fnc (hawk_htb_t* map, void* vptr, hawk_oow_t vlen)
{
	hawk_t* hawk = *(hawk_t**)(map + 1);
	hawk_fnc_t* f = (hawk_fnc_t*)vptr;
	hawk_freemem (hawk, f);
}

static int init_token (hawk_t* hawk, hawk_tok_t* tok)
{
	tok->name = hawk_ooecs_open(hawk_getgem(hawk), 0, 128);
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
	hawk_t* hawk;

	hawk = (hawk_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(hawk_t) + xtnsize);
	if (HAWK_LIKELY(hawk))
	{
		int xret;

		xret = hawk_init(hawk, mmgr, cmgr, prm);
		if (xret <= -1)
		{
			if (errnum) *errnum = hawk_geterrnum(hawk);
			HAWK_MMGR_FREE (mmgr, hawk);
			hawk = HAWK_NULL;
		}
		else HAWK_MEMSET (hawk + 1, 0, xtnsize);
	}
	else if (errnum) *errnum = HAWK_ENOMEM;

	return hawk;
}

void hawk_close (hawk_t* hawk)
{
	hawk_fini (hawk);
	HAWK_MMGR_FREE (hawk_getmmgr(hawk), hawk);
}

int hawk_init (hawk_t* hawk, hawk_mmgr_t* mmgr, hawk_cmgr_t* cmgr, const hawk_prm_t* prm)
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
	HAWK_MEMSET (hawk, 0, HAWK_SIZEOF(*hawk));

	/* remember the memory manager */
	hawk->_instsize = HAWK_SIZEOF(*hawk);
	hawk->_gem.mmgr = mmgr;
	hawk->_gem.cmgr = cmgr;

	/* initialize error handling fields */
	hawk->_gem.errnum = HAWK_ENOERR;
	hawk->_gem.errmsg[0] = '\0';
	hawk->_gem.errloc.line = 0;
	hawk->_gem.errloc.colm = 0;
	hawk->_gem.errloc.file = HAWK_NULL;
	hawk->_gem.errstr = hawk_dfl_errstr;
	hawk->haltall = 0;
	hawk->ecb = (hawk_ecb_t*)hawk; /* use this as a sentinel node instead of HAWK_NULL */

	/* progagate the primitive functions */
	HAWK_ASSERT (prm           != HAWK_NULL);
	HAWK_ASSERT (prm->math.pow != HAWK_NULL);
	HAWK_ASSERT (prm->math.mod != HAWK_NULL);
	if (prm           == HAWK_NULL ||
	    prm->math.pow == HAWK_NULL ||
	    prm->math.mod == HAWK_NULL)
	{
		hawk_seterrnum (hawk, HAWK_NULL, HAWK_EINVAL);
		goto oops;
	}
	hawk->prm = *prm;

	if (init_token(hawk, &hawk->ptok) <= -1 ||
	    init_token(hawk, &hawk->tok) <= -1 ||
	    init_token(hawk, &hawk->ntok) <= -1) goto oops;

	hawk->opt.trait = HAWK_MODERN;
#if defined(__WIN32__) || defined(_OS2) || defined(__DOS__)
	hawk->opt.trait |= HAWK_CRLF;
#endif
	hawk->opt.rtx_stack_limit = HAWK_DFL_RTX_STACK_LIMIT;
	hawk->opt.log_mask = HAWK_LOG_ALL_LEVELS  | HAWK_LOG_ALL_TYPES;
	hawk->opt.log_maxcapa = HAWK_DFL_LOG_MAXCAPA;

	hawk->log.capa = HAWK_ALIGN_POW2(1, HAWK_LOG_CAPA_ALIGN);
	hawk->log.ptr = hawk_allocmem(hawk, (hawk->log.capa + 1) * HAWK_SIZEOF(*hawk->log.ptr));
	if (!hawk->log.ptr) goto oops;

	hawk->tree.ngbls = 0;
	hawk->tree.ngbls_base = 0;
	hawk->tree.begin = HAWK_NULL;
	hawk->tree.begin_tail = HAWK_NULL;
	hawk->tree.end = HAWK_NULL;
	hawk->tree.end_tail = HAWK_NULL;
	hawk->tree.chain = HAWK_NULL;
	hawk->tree.chain_tail = HAWK_NULL;
	hawk->tree.chain_size = 0;

	/* TODO: initial map size?? */
	hawk->tree.funs = hawk_htb_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 512, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	hawk->parse.funs = hawk_htb_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	hawk->parse.named = hawk_htb_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1);

	hawk->parse.gbls = hawk_arr_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 128);
	hawk->parse.lcls = hawk_arr_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 64);
	hawk->parse.params = hawk_arr_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 32);

	hawk->fnc.sys = HAWK_NULL;
	hawk->fnc.user = hawk_htb_open(hawk_getgem(hawk), HAWK_SIZEOF(hawk), 512, 70, HAWK_SIZEOF(hawk_ooch_t), 1);
	hawk->modtab = hawk_rbt_open(hawk_getgem(hawk), 0, HAWK_SIZEOF(hawk_ooch_t), 1);

	if (hawk->tree.funs == HAWK_NULL ||
	    hawk->parse.funs == HAWK_NULL ||
	    hawk->parse.named == HAWK_NULL ||
	    hawk->parse.gbls == HAWK_NULL ||
	    hawk->parse.lcls == HAWK_NULL ||
	    hawk->parse.params == HAWK_NULL ||
	    hawk->fnc.user == HAWK_NULL ||
	    hawk->modtab == HAWK_NULL)
	{
		hawk_seterrnum (hawk, HAWK_NULL, HAWK_ENOMEM);
		goto oops;
	}

	*(hawk_t**)(hawk->tree.funs + 1) = hawk;
	hawk_htb_setstyle (hawk->tree.funs, &treefuncbs);

	*(hawk_t**)(hawk->parse.funs + 1) = hawk;
	hawk_htb_setstyle (hawk->parse.funs, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_KEY_COPIER));

	*(hawk_t**)(hawk->parse.named + 1) = hawk;
	hawk_htb_setstyle (hawk->parse.named, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_KEY_COPIER));

	*(hawk_t**)(hawk->parse.gbls + 1) = hawk;
	hawk_arr_setscale (hawk->parse.gbls, HAWK_SIZEOF(hawk_ooch_t));
	hawk_arr_setstyle (hawk->parse.gbls, hawk_get_arr_style(HAWK_ARR_STYLE_INLINE_COPIER));

	*(hawk_t**)(hawk->parse.lcls + 1) = hawk;
	hawk_arr_setscale (hawk->parse.lcls, HAWK_SIZEOF(hawk_ooch_t));
	hawk_arr_setstyle (hawk->parse.lcls, hawk_get_arr_style(HAWK_ARR_STYLE_INLINE_COPIER));

	*(hawk_t**)(hawk->parse.params + 1) = hawk;
	hawk_arr_setscale (hawk->parse.params, HAWK_SIZEOF(hawk_ooch_t));
	hawk_arr_setstyle (hawk->parse.params, hawk_get_arr_style(HAWK_ARR_STYLE_INLINE_COPIER));

	*(hawk_t**)(hawk->fnc.user + 1) = hawk;
	hawk_htb_setstyle (hawk->fnc.user, &fncusercbs);

	hawk_rbt_setstyle (hawk->modtab, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	if (hawk_initgbls(hawk) <= -1) goto oops;
	return 0;

oops:
	if (hawk->modtab) hawk_rbt_close (hawk->modtab);
	if (hawk->fnc.user) hawk_htb_close (hawk->fnc.user);
	if (hawk->parse.params) hawk_arr_close (hawk->parse.params);
	if (hawk->parse.lcls) hawk_arr_close (hawk->parse.lcls);
	if (hawk->parse.gbls) hawk_arr_close (hawk->parse.gbls);
	if (hawk->parse.named) hawk_htb_close (hawk->parse.named);
	if (hawk->parse.funs) hawk_htb_close (hawk->parse.funs);
	if (hawk->tree.funs) hawk_htb_close (hawk->tree.funs);
	fini_token (&hawk->ntok);
	fini_token (&hawk->tok);
	fini_token (&hawk->ptok);
	if (hawk->log.ptr) hawk_freemem (hawk, hawk->log.ptr);
	hawk->log.capa = 0;

	return -1;
}

void hawk_fini (hawk_t* hawk)
{
	hawk_ecb_t* ecb, * ecb_next;
	int i;

	hawk_clear (hawk);
	/*hawk_clrfnc (hawk);*/

	if (hawk->log.len >  0)
        {
		int shuterr = hawk->shuterr;
		hawk->shuterr = 1;
		hawk->prm.logwrite (hawk, hawk->log.last_mask, hawk->log.ptr, hawk->log.len);
		hawk->shuterr = shuterr;
	}

	for (ecb = hawk->ecb; ecb != (hawk_ecb_t*)hawk; ecb = ecb_next)
	{
		ecb_next = ecb->next;
		if (ecb->close) ecb->close (hawk, ecb->ctx);
	}

	do { ecb = hawk_popecb(hawk); } while (ecb);
	HAWK_ASSERT (hawk->ecb == (hawk_ecb_t*)hawk);


	hawk_rbt_close (hawk->modtab);
	hawk_htb_close (hawk->fnc.user);

	hawk_arr_close (hawk->parse.params);
	hawk_arr_close (hawk->parse.lcls);
	hawk_arr_close (hawk->parse.gbls);
	hawk_htb_close (hawk->parse.named);
	hawk_htb_close (hawk->parse.funs);

	hawk_htb_close (hawk->tree.funs);

	fini_token (&hawk->ntok);
	fini_token (&hawk->tok);
	fini_token (&hawk->ptok);

	if (hawk->parse.incl_hist.ptr) hawk_freemem (hawk, hawk->parse.incl_hist.ptr);
	hawk_clearsionames (hawk);

	/* destroy dynamically allocated options */
	for (i = 0; i < HAWK_COUNTOF(hawk->opt.mod); i++)
	{
		if (hawk->opt.mod[i].ptr) hawk_freemem (hawk, hawk->opt.mod[i].ptr);
	}

	if (hawk->opt.includedirs.ptr) hawk_freemem (hawk, hawk->opt.includedirs.ptr);


	for (i = 0; i < HAWK_COUNTOF(hawk->sbuf); i++)
	{
		if (hawk->sbuf[i].ptr)
		{
			hawk_freemem (hawk, hawk->sbuf[i].ptr);
			hawk->sbuf[i].ptr = HAWK_NULL;
			hawk->sbuf[i].len = 0;
			hawk->sbuf[i].capa = 0;
		}
	}

	if (hawk->log.len > 0)
	{
		/* flush pending log message that could be generated by the fini
		 * callbacks. however, the actual logging might not be produced at
		 * this point because one of the callbacks could arrange to stop
		 * logging */
		int shuterr = hawk->shuterr;
		hawk->shuterr = 1;
		hawk->prm.logwrite (hawk, hawk->log.last_mask, hawk->log.ptr, hawk->log.len);
		hawk->shuterr = shuterr;
	}

	if (hawk->log.ptr)
	{
		hawk_freemem (hawk, hawk->log.ptr);
		hawk->log.capa = 0;
		hawk->log.len = 0;
	}
}

static hawk_rbt_walk_t unload_module (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, void* ctx)
{
	hawk_t* hawk = (hawk_t*)ctx;
	hawk_mod_data_t* md;

	md = HAWK_RBT_VPTR(pair);
	if (md->mod.unload) md->mod.unload (&md->mod, hawk);
	if (md->handle) hawk->prm.modclose (hawk, md->handle);

	return HAWK_RBT_WALK_FORWARD;
}

void hawk_clear (hawk_t* hawk)
{
	hawk_ecb_t* ecb, * ecb_next;

	for (ecb = hawk->ecb; ecb != (hawk_ecb_t*)hawk; ecb = ecb_next)
	{
		ecb_next = ecb->next;
		if (ecb->clear) ecb->clear (hawk, ecb->ctx);
	}
	/* hawk_clear() this doesn't pop event callbacks */

	hawk->haltall = 0;

	clear_token (&hawk->tok);
	clear_token (&hawk->ntok);
	clear_token (&hawk->ptok);

	/* clear all loaded modules */
	hawk_rbt_walk (hawk->modtab, unload_module, hawk);
	hawk_rbt_clear (hawk->modtab);

	HAWK_ASSERT (HAWK_ARR_SIZE(hawk->parse.gbls) == hawk->tree.ngbls);
	/* delete all non-builtin global variables */
	hawk_arr_delete (
		hawk->parse.gbls, hawk->tree.ngbls_base,
		HAWK_ARR_SIZE(hawk->parse.gbls) - hawk->tree.ngbls_base);

	hawk_arr_clear (hawk->parse.lcls);
	hawk_arr_clear (hawk->parse.params);
	hawk_htb_clear (hawk->parse.named);
	hawk_htb_clear (hawk->parse.funs);

	hawk->parse.nlcls_max = 0;
	hawk->parse.depth.block = 0;
	hawk->parse.depth.loop = 0;
	hawk->parse.depth.expr = 0;
	hawk->parse.depth.incl = 0;
	hawk->parse.pragma.trait = (hawk->opt.trait & (HAWK_IMPLICIT | HAWK_MULTILINESTR | HAWK_STRIPRECSPC | HAWK_STRIPSTRSPC)); /* implicit on if you didn't mask it off in hawk->opt.trait with hawk_setopt */
	hawk->parse.pragma.rtx_stack_limit = 0;
	hawk->parse.pragma.entry[0] = '\0';

	hawk->parse.incl_hist.count =0;

	/* clear parse trees */
	/*hawk->tree.ngbls_base = 0;
	hawk->tree.ngbls = 0; */
	hawk->tree.ngbls = hawk->tree.ngbls_base;

	hawk->tree.cur_fun.ptr = HAWK_NULL;
	hawk->tree.cur_fun.len = 0;
	hawk_htb_clear (hawk->tree.funs);

	if (hawk->tree.begin)
	{
		hawk_clrpt (hawk, hawk->tree.begin);
		hawk->tree.begin = HAWK_NULL;
		hawk->tree.begin_tail = HAWK_NULL;
	}

	if (hawk->tree.end)
	{
		hawk_clrpt (hawk, hawk->tree.end);
		hawk->tree.end = HAWK_NULL;
		hawk->tree.end_tail = HAWK_NULL;
	}

	while (hawk->tree.chain)
	{
		hawk_chain_t* next = hawk->tree.chain->next;
		if (hawk->tree.chain->pattern) hawk_clrpt (hawk, hawk->tree.chain->pattern);
		if (hawk->tree.chain->action) hawk_clrpt (hawk, hawk->tree.chain->action);
		hawk_freemem (hawk, hawk->tree.chain);
		hawk->tree.chain = next;
	}

	hawk->tree.chain_tail = HAWK_NULL;
	hawk->tree.chain_size = 0;

	/* this table must not be cleared here as there can be a reference
	 * to an entry of this table from errinf.loc.file when hawk_parse()
	 * failed. this table is cleared in hawk_parse().
	 * hawk_claersionames (hawk);
	 */
}

void hawk_getprm (hawk_t* hawk, hawk_prm_t* prm)
{
	*prm = hawk->prm;
}

void hawk_setprm (hawk_t* hawk, const hawk_prm_t* prm)
{
	hawk->prm = *prm;
}

static int dup_str_opt (hawk_t* hawk, const void* value, hawk_oocs_t* tmp)
{
	if (value)
	{
		tmp->ptr = hawk_dupoocstr(hawk, value, &tmp->len);
		if (HAWK_UNLIKELY(!tmp->ptr)) return -1;
	}
	else
	{
		tmp->ptr = HAWK_NULL;
		tmp->len = 0;
	}

	return 0;
}

int hawk_setopt (hawk_t* hawk, hawk_opt_t id, const void* value)
{
	switch (id)
	{
		case HAWK_OPT_TRAIT:
			hawk->opt.trait = *(const int*)value;
			return 0;

		case HAWK_OPT_MODLIBDIRS:
		case HAWK_OPT_MODPREFIX:
		case HAWK_OPT_MODPOSTFIX:
		{
			hawk_oocs_t tmp;
			int idx;

			if (dup_str_opt(hawk, value, &tmp) <= -1) return -1;

			idx = id - HAWK_OPT_MODLIBDIRS;
			if (hawk->opt.mod[idx].ptr) hawk_freemem (hawk, hawk->opt.mod[idx].ptr);

			hawk->opt.mod[idx] = tmp;
			return 0;
		}

		case HAWK_OPT_INCLUDEDIRS:
		{
			hawk_oocs_t tmp;
			if (dup_str_opt(hawk, value, &tmp) <= -1) return -1;
			if (hawk->opt.includedirs.ptr) hawk_freemem (hawk, hawk->opt.includedirs.ptr);
			hawk->opt.includedirs = tmp;
			return 0;
		}

		case HAWK_OPT_DEPTH_INCLUDE:
		case HAWK_OPT_DEPTH_BLOCK_PARSE:
		case HAWK_OPT_DEPTH_BLOCK_RUN:
		case HAWK_OPT_DEPTH_EXPR_PARSE:
		case HAWK_OPT_DEPTH_EXPR_RUN:
		case HAWK_OPT_DEPTH_REX_BUILD:
		case HAWK_OPT_DEPTH_REX_MATCH:
			hawk->opt.depth.a[id - HAWK_OPT_DEPTH_INCLUDE] = *(const hawk_oow_t*)value;
			return 0;

		case HAWK_OPT_RTX_STACK_LIMIT:
			hawk->opt.rtx_stack_limit = *(const hawk_oow_t*)value;
			if (hawk->opt.rtx_stack_limit < HAWK_MIN_RTX_STACK_LIMIT) hawk->opt.rtx_stack_limit = HAWK_MIN_RTX_STACK_LIMIT;
			else if (hawk->opt.rtx_stack_limit > HAWK_MAX_RTX_STACK_LIMIT) hawk->opt.rtx_stack_limit = HAWK_MAX_RTX_STACK_LIMIT;
			return 0;


		case HAWK_OPT_LOG_MASK:
			hawk->opt.log_mask = *(hawk_bitmask_t*)value;
			return 0;

		case HAWK_OPT_LOG_MAXCAPA:
			hawk->opt.log_maxcapa = *(hawk_oow_t*)value;
			return 0;

	}

	hawk_seterrnum (hawk, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

int hawk_getopt (hawk_t* hawk, hawk_opt_t id, void* value)
{
	switch  (id)
	{
		case HAWK_OPT_TRAIT:
			*(int*)value = hawk->opt.trait;
			return 0;

		case HAWK_OPT_MODLIBDIRS:
		case HAWK_OPT_MODPREFIX:
		case HAWK_OPT_MODPOSTFIX:
			*(const hawk_ooch_t**)value = hawk->opt.mod[id - HAWK_OPT_MODLIBDIRS].ptr;
			return 0;

		case HAWK_OPT_INCLUDEDIRS:
			*(const hawk_ooch_t**)value = hawk->opt.includedirs.ptr;
			return 0;

		case HAWK_OPT_DEPTH_INCLUDE:
		case HAWK_OPT_DEPTH_BLOCK_PARSE:
		case HAWK_OPT_DEPTH_BLOCK_RUN:
		case HAWK_OPT_DEPTH_EXPR_PARSE:
		case HAWK_OPT_DEPTH_EXPR_RUN:
		case HAWK_OPT_DEPTH_REX_BUILD:
		case HAWK_OPT_DEPTH_REX_MATCH:
			*(hawk_oow_t*)value = hawk->opt.depth.a[id - HAWK_OPT_DEPTH_INCLUDE];
			return 0;

		case HAWK_OPT_RTX_STACK_LIMIT:
			*(hawk_oow_t*)value = hawk->opt.rtx_stack_limit;
			return 0;

		case HAWK_OPT_LOG_MASK:
			*(hawk_bitmask_t*)value = hawk->opt.log_mask;
			return 0;

		case HAWK_OPT_LOG_MAXCAPA:
			*(hawk_oow_t*)value = hawk->opt.log_maxcapa;
			return 0;

	};

	hawk_seterrnum (hawk, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

void hawk_haltall (hawk_t* hawk)
{
	hawk->haltall = 1;
}

void hawk_killecb (hawk_t* hawk, hawk_ecb_t* ecb)
{
	hawk_ecb_t* prev, * cur;
	for (cur = hawk->ecb, prev = HAWK_NULL; cur != (hawk_ecb_t*)hawk; cur = cur->next)
	{
		if (cur == ecb)
		{
			if (prev) prev->next = cur->next;
			else hawk->ecb = cur->next;
			cur->next = HAWK_NULL;
			break;
		}
	}
}

hawk_ecb_t* hawk_popecb (hawk_t* hawk)
{
	hawk_ecb_t* top = hawk->ecb;
	if (top == (hawk_ecb_t*)hawk) return HAWK_NULL;
	hawk->ecb = top->next;
	top->next = HAWK_NULL;
	return top;
}

void hawk_pushecb (hawk_t* hawk, hawk_ecb_t* ecb)
{
	ecb->next = hawk->ecb;
	hawk->ecb = ecb;
}

/* ------------------------------------------------------------------------ */

hawk_oow_t hawk_fmttoucstr_ (hawk_t* hawk, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, ...)
{
	va_list ap;
	hawk_oow_t n;
	va_start(ap, fmt);
	n = hawk_gem_vfmttoucstr(hawk_getgem(hawk), buf, bufsz, fmt, ap);
	va_end(ap);
	return n;
}

hawk_oow_t hawk_fmttobcstr_ (hawk_t* hawk, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	hawk_oow_t n;
	va_start(ap, fmt);
	n = hawk_gem_vfmttobcstr(hawk_getgem(hawk), buf, bufsz, fmt, ap);
	va_end(ap);
	return n;
}


int hawk_buildrex (hawk_t* hawk, const hawk_ooch_t* ptn, hawk_oow_t len, hawk_tre_t** code, hawk_tre_t** icode)
{
	return hawk_gem_buildrex(hawk_getgem(hawk), ptn, len, !(hawk->opt.trait & HAWK_REXBOUND), code, icode);
}



/* ------------------------------------------------------------------------ */

int hawk_findmodsymfnc_noseterr (hawk_t* hawk, hawk_mod_fnc_tab_t* fnctab, hawk_oow_t count, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int n;

	/* binary search */
	hawk_oow_t base, mid, lim;

	for (base = 0, lim = count; lim > 0; lim >>= 1)
	{
		mid = base + (lim >> 1);
		n = hawk_comp_oocstr(name, fnctab[mid].name, 0);
		if (n == 0)
		{
			sym->type = HAWK_MOD_FNC;
			sym->name = fnctab[mid].name;
			sym->u.fnc_ = fnctab[mid].info;
			return 0;
		}
		if (n > 0) { base = mid + 1; lim--; }
	}

	return -1;
}


int hawk_findmodsymint_noseterr (hawk_t* hawk, hawk_mod_int_tab_t* inttab, hawk_oow_t count, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int n;

	/* binary search */
	hawk_oow_t base, mid, lim;

	for (base = 0, lim = count; lim > 0; lim >>= 1)
	{
		mid = base + (lim >> 1);
		n = hawk_comp_oocstr(name, inttab[mid].name, 0);
		if (n == 0)
		{
			sym->type = HAWK_MOD_INT;
			sym->name = inttab[mid].name;
			sym->u.int_ = inttab[mid].info;
			return 0;
		}
		if (n > 0) { base = mid + 1; lim--; }
	}

	return -1;
}

int hawk_findmodsymflt_noseterr (hawk_t* hawk, hawk_mod_flt_tab_t* flttab, hawk_oow_t count, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int n;

	/* binary search */
	hawk_oow_t base, mid, lim;

	for (base = 0, lim = count; lim > 0; lim >>= 1)
	{
		mid = base + (lim >> 1);
		n = hawk_comp_oocstr(name, flttab[mid].name, 0);
		if (n == 0)
		{
			sym->type = HAWK_MOD_FLT;
			sym->name = flttab[mid].name;
			sym->u.flt_ = flttab[mid].info;
			return 0;
		}
		if (n > 0) { base = mid + 1; lim--; }
	}

	return -1;
}

int hawk_findmodsymfnc (hawk_t* hawk, hawk_mod_fnc_tab_t* fnctab, hawk_oow_t count, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int n = hawk_findmodsymfnc_noseterr(hawk, fnctab, count, name, sym);
	if (n <= -1) hawk_seterrfmt (hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js' not found"), name);
	return n;
}

int hawk_findmodsymint (hawk_t* hawk, hawk_mod_int_tab_t* inttab, hawk_oow_t count, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int n = hawk_findmodsymint_noseterr(hawk, inttab, count, name, sym);
	if (n <= -1) hawk_seterrfmt (hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js' not found"), name);
	return n;
}

int hawk_findmodsymflt (hawk_t* hawk, hawk_mod_flt_tab_t* flttab, hawk_oow_t count, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int n = hawk_findmodsymflt_noseterr(hawk, flttab, count, name, sym);
	if (n <= -1) hawk_seterrfmt (hawk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js' not found"), name);
	return n;
}


/* ------------------------------------------------------------------------ */

static HAWK_INLINE int secure_space_in_sbuf (hawk_t* hawk, hawk_oow_t req, hawk_sbuf_t* p)
{
	if (req > p->capa - p->len)
	{
		hawk_oow_t newcapa;
		hawk_ooch_t* tmp;

		newcapa = p->len + req + 1;
		newcapa = HAWK_ALIGN_POW2(newcapa, 512); /* TODO: adjust this capacity */

		tmp = (hawk_ooch_t*)hawk_reallocmem(hawk, p->ptr, newcapa * HAWK_SIZEOF(*tmp));
		if (!tmp) return -1;

		p->ptr = tmp;
		p->capa = newcapa - 1;
	}

	return 0;
}

int hawk_concatoocstrtosbuf (hawk_t* hawk, const hawk_ooch_t* str, hawk_sbuf_id_t id)
{
	return hawk_concatoocharstosbuf(hawk, str, hawk_count_oocstr(str), id);
}

int hawk_concatoocharstosbuf (hawk_t* hawk, const hawk_ooch_t* ptr, hawk_oow_t len, hawk_sbuf_id_t id)
{
	hawk_sbuf_t* p;

	p = &hawk->sbuf[id];
	if (secure_space_in_sbuf(hawk, len, p) <= -1) return -1;
	hawk_copy_oochars (&p->ptr[p->len], ptr, len);
	p->len += len;
	p->ptr[p->len] = '\0';

	return 0;
}

int hawk_concatoochartosbuf (hawk_t* hawk, hawk_ooch_t ch, hawk_oow_t count, hawk_sbuf_id_t id)
{
	hawk_sbuf_t* p;

	p = &hawk->sbuf[id];
	if (secure_space_in_sbuf(hawk, count, p) <= -1) return -1;
	hawk_fill_oochars (&p->ptr[p->len], ch, count);
	p->len += count;
	p->ptr[p->len] = '\0';

	return 0;
}

int hawk_copyoocstrtosbuf (hawk_t* hawk, const hawk_ooch_t* str, hawk_sbuf_id_t id)
{
	hawk->sbuf[id].len = 0;
	return hawk_concatoocstrtosbuf(hawk, str, id);
}

int hawk_copyoocharstosbuf (hawk_t* hawk, const hawk_ooch_t* ptr, hawk_oow_t len, hawk_sbuf_id_t id)
{
	hawk->sbuf[id].len = 0;
	return hawk_concatoocharstosbuf(hawk, ptr, len, id);
}
