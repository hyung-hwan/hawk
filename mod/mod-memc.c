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

#include "mod-memc.h"
#include "../lib/hawk-prv.h"

#include <libmemcached/memcached.h>

#define __IDMAP_NODE_T_DATA  memcached_st *memc;
#define __IDMAP_LIST_T_DATA  memcached_return_t rc; hawk_ooch_t errmsg[256];
#define __IDMAP_LIST_T memc_list_t
#define __IDMAP_NODE_T memc_node_t
#define __INIT_IDMAP_LIST __init_memc_list
#define __FINI_IDMAP_LIST __fini_memc_list
#define __MAKE_IDMAP_NODE __new_memc_node
#define __FREE_IDMAP_NODE __free_memc_node
#include "../lib/idmap-imp.h"

struct rtx_data_t
{
	memc_list_t memc_list;
};
typedef struct rtx_data_t rtx_data_t;

/* ------------------------------------------------------------------------ */

static memc_node_t* new_memc_node (hawk_rtx_t* rtx, memc_list_t* memc_list)
{
	memc_node_t* memc_node;

	memc_node = __new_memc_node(rtx, memc_list);
	if (!memc_node) return HAWK_NULL;

	memc_node->memc = HAWK_NULL;
	return memc_node;
}

static void free_memc_node (hawk_rtx_t* rtx, memc_list_t* memc_list, memc_node_t* memc_node)
{
	if (memc_node->memc != HAWK_NULL) {
		memcached_free (memc_node->memc);
		memc_node->memc = HAWK_NULL;
	}

	__free_memc_node (rtx, memc_list, memc_node);
}

static HAWK_INLINE memc_list_t* rtx_to_memc_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	rtx_data_t* data;

	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	data = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	return &data->memc_list;
}

static void set_error_on_memc_list (hawk_rtx_t* rtx, memc_list_t* memc_list, const hawk_ooch_t* errfmt, ...)
{
	if (errfmt)
	{
		va_list ap;
		va_start (ap, errfmt);
		hawk_rtx_vfmttooocstr (rtx, memc_list->errmsg, HAWK_COUNTOF(memc_list->errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_copy_oocstr (memc_list->errmsg, HAWK_COUNTOF(memc_list->errmsg), hawk_rtx_geterrmsg(rtx));
	}
}

static HAWK_INLINE memc_node_t* get_memc_list_node (memc_list_t* memc_list, hawk_int_t id)
{
	if (id < 0 || id >= memc_list->map.high || !memc_list->map.tab[id]) return HAWK_NULL;
	return memc_list->map.tab[id];
}

static memc_node_t* get_memc_list_node_with_arg (hawk_rtx_t* rtx, memc_list_t* memc_list, hawk_val_t* arg)
{
	hawk_int_t id;
	memc_node_t* memc_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_memc_list (rtx, memc_list, HAWK_T("illegal instance id"));
		return HAWK_NULL;
	}
	else if (!(memc_node = get_memc_list_node(memc_list, id)))
	{
		set_error_on_memc_list (rtx, memc_list, HAWK_T("invalid instance id - %zd"), (hawk_oow_t)id);
		return HAWK_NULL;
	}

	return memc_node;
}

/* ------------------------------------------------------------------------ */

static int fnc_new (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	memc_list_t* memc_list;
	memc_node_t* memc_node = HAWK_NULL;
	hawk_int_t ret = -1;
	hawk_val_t* retv;

	memc_list = rtx_to_memc_list(rtx, fi);

	memc_node = new_memc_node(rtx, memc_list);
	if (memc_node) ret = memc_node->id;
	else set_error_on_memc_list (rtx, memc_list, HAWK_NULL);

	/* ret may not be a statically managed number.
	 * error checking is required */
	retv = hawk_rtx_makeintval(rtx, ret);
	if (retv == HAWK_NULL)
	{
		if (memc_node) free_memc_node (rtx, memc_list, memc_node);
		return -1;
	}

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_connect (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	memc_list_t* memc_list;
	memc_node_t* memc_node;
	hawk_val_t* a0;
	hawk_bch_t* conf = HAWK_NULL;
	hawk_oow_t conf_len = 0;
	int ret = -1, take_rtx_err = 0;

	memc_list = rtx_to_memc_list(rtx, fi);
	memc_node = get_memc_list_node_with_arg(rtx, memc_list, hawk_rtx_getarg(rtx, 0));
	if (memc_node)
	{
		a0 = hawk_rtx_getarg(rtx, 0);
		if (!(conf = hawk_rtx_getvalbcstr(rtx, a0, &conf_len)))
		{
			take_rtx_err = 1;
			goto done;
		}

		memc_node->memc = memcached (conf, conf_len);
		if (memc_node->memc == HAWK_NULL)
		{
			set_error_on_memc_list (rtx, memc_list, HAWK_T("unable to connect to %hs"), conf);
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_memc_list (rtx, memc_list, HAWK_NULL);
	if (conf) hawk_rtx_freevalbcstr (rtx, a0, conf);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	memc_list_t* memc_list;
	memc_node_t* memc_node;
	int ret = -1;

	memc_list = rtx_to_memc_list(rtx, fi);
	memc_node = get_memc_list_node_with_arg(rtx, memc_list, hawk_rtx_getarg(rtx, 0));
	if (memc_node)
	{
		free_memc_node (rtx, memc_list, memc_node);
		ret = 0;
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	memc_list_t* memc_list;
	hawk_val_t* retv;

	memc_list = rtx_to_memc_list(rtx, fi);
	retv = hawk_rtx_makestrvalwithoocstr(rtx, memc_list->errmsg);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_get (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	memc_list_t* memc_list;
	memc_node_t* memc_node;
	int take_rtx_err = 0;

	hawk_val_t* a1, * a2;
	hawk_bch_t* key = HAWK_NULL;
	hawk_bch_t* def_val = HAWK_NULL;
	hawk_bch_t* rv = HAWK_NULL;
	hawk_oow_t key_len = 0;
	hawk_oow_t def_val_len = 0;
	hawk_oow_t rv_len = 0;
	char* val = HAWK_NULL;
	size_t val_len = 0;

	memc_list = rtx_to_memc_list(rtx, fi);
	memc_node = get_memc_list_node_with_arg(rtx, memc_list, hawk_rtx_getarg(rtx, 0));
	if (!memc_node)
	{
		goto done;
	}
	else
	{
		uint32_t flags;
		memcached_return_t rc;

		hawk_oow_t nargs;
		nargs = hawk_rtx_getnargs(rtx);

		a1 = hawk_rtx_getarg(rtx, 1);
		if (!(key = hawk_rtx_getvalbcstr(rtx, a1, &key_len)))
		{
			take_rtx_err = 1;
			goto done;
		}

		if (nargs >= 3)
		{
			a2 = hawk_rtx_getarg(rtx, 2);
			if (!(def_val = hawk_rtx_getvalbcstr(rtx, a2, &def_val_len)))
			{
				take_rtx_err = 1;
				goto done;
			}
		}

		val = memcached_get(memc_node->memc, key, key_len, &val_len, &flags, &rc);
		if (val == HAWK_NULL) goto done;

		rv = val;
		rv_len = val_len;
	}

done:
	if (rv == HAWK_NULL)
	{
		if (def_val != HAWK_NULL) rv = "";
		else
		{
			rv = def_val;
			rv_len = def_val_len;
		}
	}
	hawk_rtx_setretval (rtx, hawk_rtx_makestrvalwithbchars(rtx, rv, rv_len));

	if (take_rtx_err) set_error_on_memc_list (rtx, memc_list, HAWK_NULL);
	if (key) hawk_rtx_freevalbcstr (rtx, a1, key);
	if (def_val) hawk_rtx_freevalbcstr (rtx, a2, def_val);
	if (val) free(val);

	return 0;
}

static int fnc_set (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	memc_list_t* memc_list;
	memc_node_t* memc_node;
	int ret = -1;
	int take_rtx_err = 0;

	hawk_val_t* a1, * a2, * a3, * a4;
	hawk_bch_t* key = HAWK_NULL;
	hawk_bch_t* val = HAWK_NULL;
	hawk_oow_t key_len = 0;
	hawk_oow_t val_len = 0;
	hawk_int_t ttl = 0;
	hawk_int_t flag = 0;

	memc_list = rtx_to_memc_list(rtx, fi);
	memc_node = get_memc_list_node_with_arg(rtx, memc_list, hawk_rtx_getarg(rtx, 0));
	if (!memc_node)
	{
		goto done;
	}
	else
	{
		hawk_oow_t nargs;
		memcached_return_t rc;

		a1 = hawk_rtx_getarg(rtx, 1);
		a2 = hawk_rtx_getarg(rtx, 2);
		a3 = hawk_rtx_getarg(rtx, 3);

		if (!(key = hawk_rtx_getvalbcstr(rtx, a1, &key_len)) ||
		    !(val = hawk_rtx_getvalbcstr(rtx, a2, &val_len)) ||
		    (hawk_rtx_valtoint(rtx, a3, &ttl) <= -1))
		{
			take_rtx_err = 1;
			goto done;
		}

		nargs = hawk_rtx_getnargs(rtx);
		if (nargs >= 5) {
			a4 = hawk_rtx_getarg(rtx, 4);
			if (hawk_rtx_valtoint(rtx, a4, &flag) <= -1)
			{
				take_rtx_err = 1;
				goto done;
			}
		}

		rc = memcached_set(memc_node->memc, key, key_len, val, val_len, ttl, flag);
		if (rc != MEMCACHED_SUCCESS) {
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_memc_list (rtx, memc_list, HAWK_NULL);
	if (key) hawk_rtx_freevalbcstr (rtx, a1, key);
	if (val) hawk_rtx_freevalbcstr (rtx, a2, val);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

/* ------------------------------------------------------------------------ */

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("close"),   { { 1, 1, HAWK_NULL }, fnc_close,   0 } },
	{ HAWK_T("connect"), { { 1, 1, HAWK_NULL }, fnc_connect, 0 } },
	{ HAWK_T("errmsg"),  { { 0, 0, HAWK_NULL }, fnc_errmsg,  0 } },
	{ HAWK_T("get"),     { { 2, 3, HAWK_NULL }, fnc_get,     0 } },
	{ HAWK_T("new"),     { { 0, 0, HAWK_NULL }, fnc_new,     0 } },
	{ HAWK_T("set"),     { { 4, 5, HAWK_NULL }, fnc_set,     0 } },
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	rtx_data_t data, * datap;
	hawk_rbt_pair_t* pair;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_MEMSET (&data, 0, HAWK_SIZEOF(data));
	pair = hawk_rbt_insert(rbt, &rtx, HAWK_SIZEOF(rtx), &data, HAWK_SIZEOF(data));
	if (HAWK_UNLIKELY(!pair)) return -1;

	datap = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	__init_memc_list (rtx, &datap->memc_list);

	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	hawk_rbt_pair_t* pair;

	rbt = (hawk_rbt_t*)mod->ctx;

	/* garbage clean-up */
	pair = hawk_rbt_search(rbt, &rtx, HAWK_SIZEOF(rtx));
	if (pair)
	{
		rtx_data_t* data;

		data = (rtx_data_t*)HAWK_RBT_VPTR(pair);
		__fini_memc_list (rtx, &data->memc_list);
		hawk_rbt_delete (rbt, &rtx, HAWK_SIZEOF(rtx));
	}
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_ASSERT (HAWK_RBT_SIZE(rbt) == 0);
	hawk_rbt_close (rbt);
}

int hawk_mod_memc (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (HAWK_UNLIKELY(!rbt)) return -1;

	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));
	mod->ctx = rbt;

	return 0;
}

/* ------------------------------------------------------------------------ */
/*
BEGIN {
	conn = memc::new();
	if (memc::connect("--SERVER=localhost") <= -1)
	{
		print "connect error -", memc::errmsg();
        return -1;
	}

	rc = memc::set(conn, "key", "val", 900);
	if (rc <= -1) {
		print "store result error - ", memc::errmsg();
	}

	val = memc::get(conn, "key");            print val;
	val = memc::get(conn, "key", "default"); print val;

	memc::close(conn);
}
*/
