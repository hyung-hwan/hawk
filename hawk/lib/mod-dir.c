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


/*
BEGIN {
	x = dir::open ("/etc", dir::SORT); # dir::open ("/etc", 0)
	if (x <= -1) {
		print "cannot open";
		return -1;
	}

	while (dir::read(x, q) > 0) {
		print q;
	}
	dir::close (x);
}
*/

#include "mod-dir.h"
#include "hawk-prv.h"

enum
{
	DIR_ENOERR,
	DIR_EOTHER,
	DIR_ESYSERR,
	DIR_ENOMEM,
	DIR_EINVAL,
	DIR_EACCES,
	DIR_EPERM,
	DIR_ENOENT,
	DIR_EMAPTOSCALAR
};

static int dir_err_to_errnum (qse_dir_errnum_t num)
{
	switch (num)
	{
		case HAWK_DIR_ESYSERR:
			return DIR_ESYSERR;
		case HAWK_DIR_ENOMEM:
			return DIR_ENOMEM;
		case HAWK_DIR_EINVAL:
			return DIR_EINVAL;
		case HAWK_DIR_EACCES:
			return DIR_EACCES;
		case HAWK_DIR_EPERM:
			return DIR_EPERM;
		case HAWK_DIR_ENOENT:
			return DIR_ENOENT;
		default:
			return DIR_EOTHER;
	}
}

static int awk_err_to_errnum (hawk_errnum_t num)
{
	switch (num)
	{
		case HAWK_ESYSERR:
			return DIR_ESYSERR;
		case HAWK_ENOMEM:
			return DIR_ENOMEM;
		case HAWK_EINVAL:
			return DIR_EINVAL;
		case HAWK_EACCES:
			return DIR_EACCES;
		case HAWK_EPERM:
			return DIR_EPERM;
		case HAWK_ENOENT:
			return DIR_ENOENT;
		case HAWK_EMAPTOSCALAR:
			return DIR_EMAPTOSCALAR;
		default:
			return DIR_EOTHER;
	}
}

#define __IMAP_NODE_T_DATA qse_dir_t* ctx;
#define __IMAP_LIST_T_DATA int errnum;
#define __IMAP_LIST_T dir_list_t
#define __IMAP_NODE_T dir_node_t
#define __MAKE_IMAP_NODE __new_dir_node
#define __FREE_IMAP_NODE __free_dir_node
#include "imap-imp.h"

static dir_node_t* new_dir_node (hawk_rtx_t* rtx, dir_list_t* list, const hawk_ooch_t* path, hawk_int_t flags)
{
	dir_node_t* node;
	qse_dir_errnum_t oe;

	node = __new_dir_node(rtx, list);
	if (!node) 
	{
		list->errnum = DIR_ENOMEM;
		return HAWK_NULL;
	}

	node->ctx = qse_dir_open(hawk_rtx_getmmgr(rtx), 0, path, flags, &oe);
	if (!node->ctx) 
	{
		list->errnum = dir_err_to_errnum(oe);
		__free_dir_node (rtx, list, node);
		return HAWK_NULL;
	}

	return node;
}

static void free_dir_node (hawk_rtx_t* rtx, dir_list_t* list, dir_node_t* node)
{
	if (node->ctx) 
	{
		qse_dir_close(node->ctx);
		node->ctx = HAWK_NULL;
	}
	__free_dir_node (rtx, list, node);
}
/* ------------------------------------------------------------------------ */

static int close_byid (hawk_rtx_t* rtx, dir_list_t* list, hawk_int_t id)
{
	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		free_dir_node (rtx, list, list->map.tab[id]);
		return 0;
	}
	else
	{
		list->errnum = DIR_EINVAL;
		return -1;
	}
}

static int reset_byid (hawk_rtx_t* rtx, dir_list_t* list, hawk_int_t id, const hawk_ooch_t* path)
{
	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		if (qse_dir_reset(list->map.tab[id]->ctx, path) <= -1)
		{
			list->errnum = dir_err_to_errnum (qse_dir_geterrnum (list->map.tab[id]->ctx));
			return -1;
		}
		return 0;
	}
	else
	{
		list->errnum = DIR_EINVAL;
		return -1;
	}
}

static int read_byid (hawk_rtx_t* rtx, dir_list_t* list, hawk_int_t id, hawk_val_ref_t* ref) 
{
	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		int y;
		qse_dir_ent_t ent;	
		hawk_val_t* tmp;

		y = qse_dir_read(list->map.tab[id]->ctx, &ent);
		if (y <= -1) 
		{
			list->errnum = dir_err_to_errnum(qse_dir_geterrnum (list->map.tab[id]->ctx));
			return -1;
		}

		if (y == 0) return 0; /* no more entry */

		tmp = hawk_rtx_makestrvalwithoocstr(rtx, ent.name);
		if (!tmp)
		{
			list->errnum = awk_err_to_errnum(hawk_rtx_geterrnum (rtx));
			return -1;
		}
		else
		{
			int n;
			hawk_rtx_refupval (rtx, tmp);
			n = hawk_rtx_setrefval (rtx, ref, tmp);
			hawk_rtx_refdownval (rtx, tmp);
			if (n <= -1) return -9999;
		}

		return 1; /* has entry */
	}
	else
	{
		list->errnum = DIR_EINVAL;
		return -1;
	}
}

/* ------------------------------------------------------------------------ */

static HAWK_INLINE dir_list_t* rtx_to_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (hawk_rtx_gethawk(rtx), pair != HAWK_NULL);
	return (dir_list_t*)HAWK_RBT_VPTR(pair);
}

static int fnc_dir_errno (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	dir_list_t* list;
	hawk_val_t* retv;

	list = rtx_to_list(rtx, fi);

	retv = hawk_rtx_makeintval (rtx, list->errnum);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static hawk_ooch_t* errmsg[] =
{
	HAWK_T("no error"),
	HAWK_T("other error"),
	HAWK_T("system error"),
	HAWK_T("insufficient memory"),
	HAWK_T("invalid data"),
	HAWK_T("access denied"),
	HAWK_T("operation not permitted"),
	HAWK_T("no entry"),
	HAWK_T("cannot change a map to a scalar"),
	HAWK_T("unknown error")
};

static int fnc_dir_errstr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	dir_list_t* list;
	hawk_val_t* retv;
	hawk_int_t errnum;

	list = rtx_to_list(rtx, fi);

	if (hawk_rtx_getnargs(rtx) <= 0 ||
	    hawk_rtx_valtoint(rtx, hawk_rtx_getarg (rtx, 0), &errnum) <= -1)
	{
		errnum = list->errnum;
	}

	if (errnum < 0 || errnum >= HAWK_COUNTOF(errmsg)) errnum = HAWK_COUNTOF(errmsg) - 1;

	retv = hawk_rtx_makestrvalwithoocstr(rtx, errmsg[errnum]);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_dir_open (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	dir_list_t* list;
	dir_node_t* node = HAWK_NULL;
	hawk_int_t ret;
	hawk_ooch_t* path;
	hawk_val_t* retv;
	hawk_val_t* a0;
	hawk_int_t flags = 0;

	list = rtx_to_list(rtx, fi);

	a0 = hawk_rtx_getarg(rtx, 0);
	path = hawk_rtx_getvaloocstr(rtx, a0, HAWK_NULL);
	if (path)
	{
		if (hawk_rtx_getnargs(rtx) >= 2 && 
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &flags) <= -1)
		{
			hawk_rtx_freevaloocstr (rtx, a0, path);
			goto oops;
		}

		node = new_dir_node(rtx, list, path, flags);
		if (node) ret = node->id;
		else ret = -1;
		hawk_rtx_freevaloocstr (rtx, a0, path);
	}
	else
	{
	oops:
		list->errnum = awk_err_to_errnum(hawk_rtx_geterrnum(rtx));
		ret = -1;
	}

	/* ret may not be a statically managed number. 
	 * error checking is required */
	retv = hawk_rtx_makeintval(rtx, ret);
	if (retv == HAWK_NULL)
	{
		if (node) free_dir_node (rtx, list, node);
		return -1;
	}

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_dir_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	dir_list_t* list;
	hawk_int_t id;
	int ret;

	list = rtx_to_list(rtx, fi);

	ret = hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &id);
	if (ret <= -1)
	{
		list->errnum = awk_err_to_errnum(hawk_rtx_geterrnum(rtx));
		ret = -1;
	}
	else
	{
		ret = close_byid(rtx, list, id);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_dir_reset (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	dir_list_t* list;
	hawk_int_t id;
	int ret;
	hawk_ooch_t* path;
	
	list = rtx_to_list(rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1)
	{
		list->errnum = awk_err_to_errnum (hawk_rtx_geterrnum (rtx));
	}
	else
	{
		hawk_val_t* a1;

		a1 = hawk_rtx_getarg (rtx, 1);
		path = hawk_rtx_getvaloocstr (rtx, a1, HAWK_NULL);
		if (path)
		{
			ret = reset_byid (rtx, list, id, path);
			hawk_rtx_freevaloocstr (rtx, a1, path);
		}
		else
		{
			list->errnum = awk_err_to_errnum (hawk_rtx_geterrnum (rtx));
			ret = -1;
		}
	}

	/* no error check for hawk_rtx_makeintval() here since ret 
	 * is 0 or -1. it will never fail for those numbers */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval (rtx, ret));
	return 0;
}

static int fnc_dir_read  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	dir_list_t* list;
	hawk_int_t id;
	int ret;

	list = rtx_to_list(rtx, fi);

	ret = hawk_rtx_valtoint(rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) 
	{
		list->errnum = awk_err_to_errnum(hawk_rtx_geterrnum (rtx));
	}
	else
	{
		ret = read_byid(rtx, list, id, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1));
		if (ret == -9999) return -1;
	}

	/* no error check for hawk_rtx_makeintval() here since ret 
	 * is 0, 1, -1. it will never fail for those numbers */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

/* ------------------------------------------------------------------------ */

typedef struct fnctab_t fnctab_t;
struct fnctab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_fnc_t info;
};

typedef struct inttab_t inttab_t;
struct inttab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_int_t info;
};


static fnctab_t fnctab[] =
{
	{ HAWK_T("close"),       { { 1, 1, HAWK_NULL    }, fnc_dir_close,      0 } },
	{ HAWK_T("errno"),       { { 0, 0, HAWK_NULL    }, fnc_dir_errno,      0 } },
	{ HAWK_T("errstr"),      { { 0, 1, HAWK_NULL    }, fnc_dir_errstr,     0 } },
	{ HAWK_T("open"),        { { 1, 2, HAWK_NULL    }, fnc_dir_open,       0 } },
	{ HAWK_T("read"),        { { 2, 2, HAWK_T("vr") }, fnc_dir_read,       0 } },
	{ HAWK_T("reset"),       { { 2, 2, HAWK_NULL    }, fnc_dir_reset,      0 } },
};

static inttab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("SORT"), { HAWK_DIR_SORT } }
};

/* ------------------------------------------------------------------------ */

static int query (hawk_mod_t* mod, hawk_t* awk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	hawk_oocs_t ea;
	int left, right, mid, n;

	left = 0; right = HAWK_COUNTOF(fnctab) - 1;

	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = qse_strcmp (fnctab[mid].name, name);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_FNC;
			sym->u.fnc = fnctab[mid].info;
			return 0;
		}
	}

	left = 0; right = HAWK_COUNTOF(inttab) - 1;
	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = qse_strcmp (inttab[mid].name, name);
		if (n > 0) right = mid - 1;
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_INT;
			sym->u.in = inttab[mid].info;
			return 0;
		}
	}

	ea.ptr = (hawk_ooch_t*)name;
	ea.len = hawk_count_oocstr(name);
	hawk_seterror (awk, HAWK_ENOENT, &ea, HAWK_NULL);
	return -1;
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	dir_list_t list;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_MEMSET (&list, 0, HAWK_SIZEOF(list));
	if (hawk_rbt_insert (rbt, &rtx, HAWK_SIZEOF(rtx), &list, HAWK_SIZEOF(list)) == HAWK_NULL) 
	{
		hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
		return -1;
	}

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
		dir_list_t* list;
		dir_node_t* node, * next;

		list = HAWK_RBT_VPTR(pair);
		node = list->head;
		while (node)
		{
			next = node->next;
			free_dir_node (rtx, list, node);
			node = next;
		}

		hawk_rbt_delete (rbt, &rtx, HAWK_SIZEOF(rtx));
	}
}

static void unload (hawk_mod_t* mod, hawk_t* awk)
{
	hawk_rbt_t* rbt;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_ASSERT (awk, HAWK_RBT_SIZE(rbt) == 0);
	hawk_rbt_close (rbt);
}

int hawk_mod_dir (hawk_mod_t* mod, hawk_t* awk) 
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = hawk_rbt_open(hawk_getmmgr(awk), 0, 1, 1);
	if (rbt == HAWK_NULL) 
	{
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		return -1;
	}
	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}
