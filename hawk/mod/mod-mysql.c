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

#include "mod-mysql.h"
#include <mysql/mysql.h>
#include "../cmn/mem-prv.h"
#include <hawk/cmn/main.h>
#include <hawk/cmn/rbt.h>

#if !defined(MYSQL_OPT_RECONNECT)
#	define DUMMY_OPT_RECONNECT 31231 /* randomly chosen */
#endif

#define __IMAP_NODE_T_DATA  MYSQL* mysql; int connect_ever_attempted;
#define __IMAP_LIST_T_DATA  int errnum; hawk_ooch_t errmsg[256];
#define __IMAP_LIST_T sql_list_t
#define __IMAP_NODE_T sql_node_t
#define __MAKE_IMAP_NODE __new_sql_node
#define __FREE_IMAP_NODE __free_sql_node
#include "../lib/awk/imap-imp.h"

#undef __IMAP_NODE_T_DATA
#undef __IMAP_LIST_T_DATA
#undef __IMAP_LIST_T
#undef __IMAP_NODE_T
#undef __MAKE_IMAP_NODE
#undef __FREE_IMAP_NODE

#define __IMAP_NODE_T_DATA  MYSQL_RES* res; unsigned int num_fields;
#define __IMAP_LIST_T_DATA  /* int errnum; */
#define __IMAP_LIST_T res_list_t
#define __IMAP_NODE_T res_node_t
#define __MAKE_IMAP_NODE __new_res_node
#define __FREE_IMAP_NODE __free_res_node
#include "../lib/awk/imap-imp.h"

struct rtx_data_t
{
	sql_list_t sql_list;
	res_list_t res_list;
};
typedef struct rtx_data_t rtx_data_t;

static sql_node_t* new_sql_node (hawk_rtx_t* rtx, sql_list_t* sql_list)
{
	sql_node_t* sql_node;

	sql_node = __new_sql_node(rtx, sql_list);
	if (!sql_node) return HAWK_NULL;

	sql_node->mysql = mysql_init(HAWK_NULL);
	if (!sql_node->mysql)
	{
		hawk_rtx_seterrfmt (rtx, HAWK_ENOMEM, HAWK_NULL, HAWK_T("unable to allocate a mysql object"));
		return HAWK_NULL;
	}

	return sql_node;
}

static void free_sql_node (hawk_rtx_t* rtx, sql_list_t* sql_list, sql_node_t* sql_node)
{
	mysql_close (sql_node->mysql);
	sql_node->mysql = HAWK_NULL;
	__free_sql_node (rtx, sql_list, sql_node);
}


static res_node_t* new_res_node (hawk_rtx_t* rtx, res_list_t* res_list, MYSQL_RES* res)
{
	res_node_t* res_node;

	res_node = __new_res_node(rtx, res_list);
	if (!res_node) return HAWK_NULL;

	res_node->res = res;
	res_node->num_fields = mysql_num_fields(res);

	return res_node;
}

static void free_res_node (hawk_rtx_t* rtx, res_list_t* res_list, res_node_t* res_node)
{
	mysql_free_result (res_node->res);
	res_node->res = HAWK_NULL;
	__free_res_node (rtx, res_list, res_node);
}

/* ------------------------------------------------------------------------ */

static HAWK_INLINE sql_list_t* rtx_to_sql_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	data = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	return &data->sql_list;
}

static HAWK_INLINE sql_node_t* get_sql_list_node (sql_list_t* sql_list, hawk_int_t id)
{
	if (id < 0 || id >= sql_list->map.high || !sql_list->map.tab[id]) return HAWK_NULL;
	return sql_list->map.tab[id];
}

static HAWK_INLINE res_list_t* rtx_to_res_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	data = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	return &data->res_list;
}

static HAWK_INLINE res_node_t* get_res_list_node (res_list_t* res_list, hawk_int_t id)
{
	if (id < 0 || id >= res_list->map.high || !res_list->map.tab[id]) return HAWK_NULL;
	return res_list->map.tab[id];
}

/* ------------------------------------------------------------------------ */

static void set_error_on_sql_list (hawk_rtx_t* rtx, sql_list_t* sql_list, const hawk_ooch_t* errfmt, ...)
{
	if (errfmt)
	{
		va_list ap;
		va_start (ap, errfmt);
		hawk_strxvfmt (sql_list->errmsg, HAWK_COUNTOF(sql_list->errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_strxcpy (sql_list->errmsg, HAWK_COUNTOF(sql_list->errmsg), hawk_rtx_geterrmsg(rtx));
	}
}

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	hawk_val_t* retv;

	sql_list = rtx_to_sql_list(rtx, fi);
	retv = hawk_rtx_makestrvalwithstr(rtx, sql_list->errmsg);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */

static sql_node_t* get_sql_list_node_with_arg (hawk_rtx_t* rtx, sql_list_t* sql_list, hawk_val_t* arg)
{
	hawk_int_t id;
	sql_node_t* sql_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list (rtx, sql_list, HAWK_T("illegal instance id"));
		return HAWK_NULL;
	}
	else if (!(sql_node = get_sql_list_node(sql_list, id)))
	{
		set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid instance id - %zd"), (hawk_size_t)id);
		return HAWK_NULL;
	}

	return sql_node;
}

static res_node_t* get_res_list_node_with_arg (hawk_rtx_t* rtx, sql_list_t* sql_list, res_list_t* res_list, hawk_val_t* arg)
{
	hawk_int_t id;
	res_node_t* res_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list (rtx, sql_list, HAWK_T("illegal result id"));
		return HAWK_NULL;
	}
	else if (!(res_node = get_res_list_node(res_list, id)))
	{
		set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid result id - %zd"), (hawk_size_t)id);
		return HAWK_NULL;
	}

	return res_node;
}

/* ------------------------------------------------------------------------ */
static int fnc_open (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node = HAWK_NULL;
	hawk_int_t ret = -1;
	hawk_val_t* retv;

	sql_list = rtx_to_sql_list(rtx, fi);

	sql_node = new_sql_node(rtx, sql_list);
	if (sql_node) ret = sql_node->id;
	else set_error_on_sql_list (rtx, sql_list, HAWK_NULL);

	/* ret may not be a statically managed number. 
	 * error checking is required */
	retv = hawk_rtx_makeintval(rtx, ret);
	if (retv == HAWK_NULL)
	{
		if (sql_node) free_sql_node (rtx, sql_list, sql_node);
		return -1;
	}

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		free_sql_node (rtx, sql_list, sql_node);
		ret = 0;
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

#if 0
static int fnc_get_option (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_int_t id, tv;
		union
		{
			unsigned int ui;
			int b;
		} v;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &id) <= -1)
		{
			take_rtx_err = 1;
			goto oops;
		}

		switch (id)
		{
			case MYSQL_OPT_CONNECT_TIMEOUT:
			case MYSQL_OPT_READ_TIMEOUT:
			case MYSQL_OPT_WRITE_TIMEOUT:
				/* unsigned int * */
				if (mysql_get_option(sql_node->mysql, id, &v.ui) != 0)
				{
					set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_errstr(sql_node->mysql));
					goto done;
				}

				retv = hawk_rtx_makeintval(rtx, v.ui);
				if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), retv) <= -1)
				{
					hawk_rtx_refupval(rtx, retv);
					hawk_rtx_refdownval(rtx, retv);
					goto oops;
				}
				break;

		#if defined(MYSQL_OPT_RECONNECT)
			case MYSQL_OPT_RECONNECT:
				/* bool * */
				if (mysql_get_option(sql_node->mysql, id, &v.b) != 0)
				{
					set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_errstr(sql_node->mysql));
					goto done;
				}

				retv = hawk_rtx_makeintval(rtx, v.b);
				if (!retv) goto oops;

				if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), retv) <= -1) 
				{
					hawk_rtx_refupval(rtx, retv);
					hawk_rtx_refdownval(rtx, retv);
					goto oops;
				}
				break;
		#else
				/* the system without MYSQL_OPT_RECONNECT available. return 1 all the time */
				if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), hawk_rtx_makeintval(rtx, 1)) <= -1) 
				{
					hawk_rtx_refupval(rtx, retv);
					hawk_rtx_refdownval(rtx, retv);
					goto oops;
				}
				break;
		#endif

			default:
				set_error_on_sql_list (rtx, sql_list, HAWK_T("unsupported option id - %zd"), (hawk_size_t)id);
				goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}
#endif

static int fnc_set_option (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_int_t id, tv;
		void* vptr;
		union
		{
			unsigned int ui;
			int b;
		} v;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &id) <= -1)
		{
			take_rtx_err = 1;
			goto oops;
		}

		/* this function must not check sql_node->connect_ever_attempted */

		switch (id)
		{
			case MYSQL_OPT_CONNECT_TIMEOUT:
			case MYSQL_OPT_READ_TIMEOUT:
			case MYSQL_OPT_WRITE_TIMEOUT:
				/* unsigned int * */
				if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &tv) <= -1)
				{
					take_rtx_err = 1;
					goto oops;
				}

				v.ui = tv;
				vptr = &v.ui;
				break;

		#if defined(MYSQL_OPT_RECONNECT)
			case MYSQL_OPT_RECONNECT:
				/* bool * */
				if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &tv) <= -1)
				{
					take_rtx_err = 1;
					goto oops;
				}

				v.b = tv;
				vptr = &v.b;
				break;
		#else
			case DUMMY_OPT_RECONNECT:
				ret = 0;
				goto done;		
		#endif

			default:
				set_error_on_sql_list (rtx, sql_list, HAWK_T("unsupported option id - %zd"), (hawk_size_t)id);
				goto done;
		}

		if (mysql_options(sql_node->mysql, id, vptr) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}

static int fnc_connect (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	hawk_val_t* a1, * a2, * a3, * a4, * a6;
	hawk_mchar_t* host = HAWK_NULL;
	hawk_mchar_t* user = HAWK_NULL;
	hawk_mchar_t* pass = HAWK_NULL;
	hawk_mchar_t* db = HAWK_NULL;
	hawk_int_t port = 0;
	hawk_mchar_t* usck = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_size_t nargs;

		nargs = hawk_rtx_getnargs(rtx);

		a1 = hawk_rtx_getarg(rtx, 1);
		a2 = hawk_rtx_getarg(rtx, 2);
		a3 = hawk_rtx_getarg(rtx, 3);

		if (!(host = hawk_rtx_getvalmbs(rtx, a1, HAWK_NULL)) ||
		    !(user = hawk_rtx_getvalmbs(rtx, a2, HAWK_NULL)) ||
		    !(pass = hawk_rtx_getvalmbs(rtx, a3, HAWK_NULL)))
		{
		arg_fail:
			take_rtx_err = 1;
			goto done; 
		}

		if (nargs >= 5)
		{
			a4 = hawk_rtx_getarg(rtx, 4);
			if (!(db = hawk_rtx_getvalmbs(rtx, a4, HAWK_NULL))) goto arg_fail;
			if (nargs >= 6 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 5), &port) <= -1) goto arg_fail;

			if (nargs >= 7)
			{
				a6 = hawk_rtx_getarg(rtx, 6);
				if (!(usck = hawk_rtx_getvalmbs(rtx, a6, HAWK_NULL))) goto arg_fail;
			}
		}

		if (!mysql_real_connect(sql_node->mysql, host, user, pass, db, port, usck, 0))
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			sql_node->connect_ever_attempted = 1; /* doesn't matter if mysql_real_connect() failed */
			goto done;
		}

		sql_node->connect_ever_attempted = 1;
		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (usck) hawk_rtx_freevalmbs (rtx, a6, usck);
	if (db) hawk_rtx_freevalmbs (rtx, a4, db);
	if (pass) hawk_rtx_freevalmbs (rtx, a3, pass);
	if (user) hawk_rtx_freevalmbs (rtx, a2, user);
	if (host) hawk_rtx_freevalmbs (rtx, a1, host);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

#define ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node) \
	do { \
		if (!(sql_node)->connect_ever_attempted) \
		{ \
			set_error_on_sql_list (rtx, sql_list, HAWK_T("not connected")); \
			goto done; \
		} \
	} while(0)

static int fnc_ping (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_ping(sql_node->mysql) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}

static int fnc_select_db (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	hawk_val_t* a1;
	hawk_mchar_t* db = HAWK_NULL;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		a1 = hawk_rtx_getarg(rtx, 1);

		if (!(db = hawk_rtx_getvalmbs(rtx, a1, HAWK_NULL))) { take_rtx_err = 1; goto oops; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_select_db(sql_node->mysql, db) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (db) hawk_rtx_freevalmbs (rtx, a1, db);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}


static int fnc_autocommit (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int v;

		v = hawk_rtx_valtobool(rtx, hawk_rtx_getarg(rtx, 1));

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_autocommit(sql_node->mysql, v) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}

static int fnc_commit (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int v;

		v = hawk_rtx_valtobool(rtx, hawk_rtx_getarg(rtx, 1));

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_commit(sql_node->mysql) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}

static int fnc_rollback (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int v;

		v = hawk_rtx_valtobool(rtx, hawk_rtx_getarg(rtx, 1));

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_rollback(sql_node->mysql) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}

static int fnc_affected_rows (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		my_ulonglong nrows;
		hawk_val_t* vrows;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		nrows = mysql_affected_rows(sql_node->mysql);
		if (nrows == (my_ulonglong)-1)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		vrows = hawk_rtx_makeintval(rtx, nrows);
		if (!vrows)
		{
			take_rtx_err = 1;
			goto oops;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), vrows) <= -1)
		{
			hawk_rtx_refupval (rtx, vrows);
			hawk_rtx_refdownval (rtx, vrows);
			take_rtx_err = 1;
			goto oops;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	return -1;
}

static int fnc_escape_string (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	hawk_val_t* a1, *retv;
	hawk_mchar_t* qstr = HAWK_NULL;
	hawk_mchar_t* ebuf = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_size_t qlen;

		a1 = hawk_rtx_getarg(rtx, 1);

		qstr = hawk_rtx_getvalmbs(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto oops; }

		ebuf = hawk_rtx_allocmem(rtx, (qlen * 2 + 1) * HAWK_SIZEOF(*qstr));
		if (!ebuf) { take_rtx_err = 1; goto oops; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);
		mysql_real_escape_string(sql_node->mysql, ebuf, qstr, qlen);

		retv = hawk_rtx_makestrvalwithmbs(rtx, ebuf);
		if (!retv)
		{
			take_rtx_err = 1;
			goto oops;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), retv) <= -1)
		{
			hawk_rtx_refupval (rtx, retv);
			hawk_rtx_refdownval (rtx, retv);
			take_rtx_err = 1;
			goto oops;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (ebuf) hawk_rtx_freemem (rtx, ebuf);
	if (qstr) hawk_rtx_freevalmbs (rtx, a1, qstr);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (ebuf) hawk_rtx_freemem (rtx, ebuf);
	if (qstr) hawk_rtx_freevalmbs (rtx, a1, qstr);
	return -1;
}

static int fnc_query (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	hawk_val_t* a1;
	hawk_mchar_t* qstr = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_size_t qlen;
		a1 = hawk_rtx_getarg(rtx, 1);

		qstr = hawk_rtx_getvalmbs(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto oops; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_real_query(sql_node->mysql, qstr, qlen) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (qstr) hawk_rtx_freevalmbs (rtx, a1, qstr);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (qstr) hawk_rtx_freevalmbs (rtx, a1, qstr);
	return -1;
}

static int fnc_store_result (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		res_list_t* res_list;
		res_node_t* res_node;
		MYSQL_RES* res;

		res_list = rtx_to_res_list(rtx, fi);

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		res = mysql_store_result(sql_node->mysql);
		if (!res)
		{
			if (mysql_errno(sql_node->mysql) == 0)
				set_error_on_sql_list (rtx, sql_list, HAWK_T("no result"));
			else
				set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		res_node = new_res_node(rtx, res_list, res);
		if (!res_node)	
		{
			mysql_free_result (res);
			take_rtx_err = 1;
			goto done;
		}

		ret = res_node->id;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_free_result (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	res_list_t* res_list;
	res_node_t* res_node;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	res_list = rtx_to_res_list(rtx, fi);
	res_node = get_res_list_node_with_arg(rtx, sql_list, res_list, hawk_rtx_getarg(rtx, 0));
	if (res_node)
	{
		free_res_node (rtx, res_list, res_node);
		ret = 0;
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_fetch_row (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	res_list_t* res_list;
	res_node_t* res_node;
	int ret = -1, take_rtx_err = 0;
	hawk_val_t* row_map = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	res_list = rtx_to_res_list(rtx, fi);
	res_node = get_res_list_node_with_arg(rtx, sql_list, res_list, hawk_rtx_getarg(rtx, 0));
	if (res_node)
	{
		MYSQL_ROW row;
		unsigned int i;
		hawk_val_t* row_val;
		int x;

		row = mysql_fetch_row(res_node->res);
		if (!row)
		{
			/* no more row in the result */
			ret = 0;
			goto done;
		}

		row_map = hawk_rtx_makemapval(rtx);
		if (!row_map) goto oops;

		hawk_rtx_refupval (rtx, row_map);

		for (i = 0; i < res_node->num_fields; )
		{
			hawk_ooch_t key_buf[HAWK_SIZEOF(hawk_int_t) * 8 + 2];
			hawk_size_t key_len;

			if (row[i])
			{
/* TODO: consider using make multi byte string - hawk_rtx_makembsstr */
				row_val = hawk_rtx_makestrvalwithmbs(rtx, row[i]);
				if (!row_val) goto oops;
			}
			else
			{
				row_val = hawk_rtx_makenilval(rtx);
			}

			++i;

			/* put it into the map */
			key_len = hawk_inttostr(hawk_rtx_getawk(rtx), i, 10, HAWK_NULL, key_buf, HAWK_COUNTOF(key_buf));
			HAWK_ASSERT (key_len != (hawk_size_t)-1);

			if (hawk_rtx_setmapvalfld(rtx, row_map, key_buf, key_len, row_val) == HAWK_NULL)
			{
				hawk_rtx_refupval (rtx, row_val);
				hawk_rtx_refdownval (rtx, row_val);
				goto oops;
			}
		}

		x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), row_map);

		hawk_rtx_refdownval (rtx, row_map);
		row_map = HAWK_NULL;

		if (x <= -1) goto oops;
		ret = 1; /* indicate that there is a row */
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;


oops:
	if (row_map) hawk_rtx_refdownval (rtx, row_map);
	return -1;
}

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


#define A_MAX HAWK_TYPE_MAX(int)

static fnctab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("affected_rows"),     { { 2, 2, HAWK_T("vr") },  fnc_affected_rows,  0 } },
	{ HAWK_T("autocommit"),        { { 2, 2, HAWK_NULL },     fnc_autocommit,     0 } },
	{ HAWK_T("close"),             { { 1, 1, HAWK_NULL },     fnc_close,          0 } },
	{ HAWK_T("commit"),            { { 1, 1, HAWK_NULL },     fnc_commit,         0 } },
	{ HAWK_T("connect"),           { { 4, 7, HAWK_NULL },     fnc_connect,        0 } },
	{ HAWK_T("errmsg"),            { { 0, 0, HAWK_NULL },     fnc_errmsg,         0 } },
	{ HAWK_T("escape_string"),     { { 3, 3, HAWK_T("vvr") }, fnc_escape_string,  0 } },
	{ HAWK_T("fetch_row"),         { { 2, 2, HAWK_T("vr") },  fnc_fetch_row,      0 } },
	{ HAWK_T("free_result"),       { { 1, 1, HAWK_NULL },     fnc_free_result,    0 } },
	/*{ HAWK_T("get_option"),       { { 3, 3, HAWK_T("vr") },  fnc_get_option,     0 } },*/
	{ HAWK_T("open"),              { { 0, 0, HAWK_NULL },     fnc_open,           0 } },
	{ HAWK_T("ping"),              { { 1, 1, HAWK_NULL },     fnc_ping,           0 } },
	{ HAWK_T("query"),             { { 2, 2, HAWK_NULL },     fnc_query,          0 } },
	{ HAWK_T("rollback"),          { { 1, 1, HAWK_NULL },     fnc_rollback,       0 } },
	{ HAWK_T("select_db"),         { { 2, 2, HAWK_NULL },     fnc_select_db,     0 } },
	{ HAWK_T("set_option"),        { { 3, 3, HAWK_NULL },     fnc_set_option,     0 } },
	{ HAWK_T("store_result"),      { { 1, 1, HAWK_NULL },     fnc_store_result,   0 } }
};

static inttab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("OPT_CONNECT_TIMEOUT"), { MYSQL_OPT_CONNECT_TIMEOUT } },
	{ HAWK_T("OPT_READ_TIMEOUT"),    { MYSQL_OPT_READ_TIMEOUT    } },
#if defined(MYSQL_OPT_RECONNECT)
	{ HAWK_T("OPT_RECONNECT"),       { MYSQL_OPT_RECONNECT       } },
#else
	{ HAWK_T("OPT_RECONNECT"),       { DUMMY_OPT_RECONNECT       } },
#endif
	{ HAWK_T("OPT_WRITE_TIMEOUT"),   { MYSQL_OPT_WRITE_TIMEOUT   } }
};

static int query (hawk_mod_t* mod, hawk_t* awk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int left, right, mid, n;

	left = 0; right = HAWK_COUNTOF(fnctab) - 1;

	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = hawk_strcmp (fnctab[mid].name, name);
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

		n = hawk_strcmp (inttab[mid].name, name);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_INT;
			sym->u.in = inttab[mid].info;
			return 0;
		}
	}


	hawk_seterrfmt (awk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js' not found"), name);
	return -1;
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	rtx_data_t data;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_MEMSET (&data, 0, HAWK_SIZEOF(data));
	if (hawk_rbt_insert(rbt, &rtx, HAWK_SIZEOF(rtx), &data, HAWK_SIZEOF(data)) == HAWK_NULL) 
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
		rtx_data_t* data;
		sql_node_t* sql_node, * sql_next;
		res_node_t* res_node, * res_next;

		data = (rtx_data_t*)HAWK_RBT_VPTR(pair);

		res_node = data->res_list.head;
		while (res_node)
		{
			res_next = res_node->next;
			free_res_node (rtx, &data->res_list, res_node);
			res_node = res_next;
		}

		sql_node = data->sql_list.head;
		while (sql_node)
		{
			sql_next = sql_node->next;
			free_sql_node (rtx, &data->sql_list, sql_node);
			sql_node = sql_next;
		}

		hawk_rbt_delete (rbt, &rtx, HAWK_SIZEOF(rtx));
	}
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_ASSERT (HAWK_RBT_SIZE(rbt) == 0);
	hawk_rbt_close (rbt);
//mysql_library_end ();
}

int hawk_mod_mysql (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = hawk_rbt_open(hawk_getgem(awk), 0, 1, 1);
	if (rbt == HAWK_NULL) return -1;

	hawk_rbt_setstyle (rbt, hawk_getrbtstyle(HAWK_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}


HAWK_EXPORT int hawk_mod_mysql_init (int argc, hawk_achar_t* argv[])
{
	if (mysql_library_init(argc, argv, HAWK_NULL) != 0) return -1;
	return 0;
}

HAWK_EXPORT void hawk_mod_mysql_fini (void)
{
	mysql_library_end ();
}
