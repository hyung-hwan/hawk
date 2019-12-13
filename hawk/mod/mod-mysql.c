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
#include <qse/cmn/main.h>
#include <qse/cmn/rbt.h>

#if !defined(MYSQL_OPT_RECONNECT)
#	define DUMMY_OPT_RECONNECT 31231 /* randomly chosen */
#endif

#define __IMAP_NODE_T_DATA  MYSQL* mysql; int connect_ever_attempted;
#define __IMAP_LIST_T_DATA  int errnum; qse_char_t errmsg[256];
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

static sql_node_t* new_sql_node (qse_awk_rtx_t* rtx, sql_list_t* sql_list)
{
	sql_node_t* sql_node;

	sql_node = __new_sql_node(rtx, sql_list);
	if (!sql_node) return QSE_NULL;

	sql_node->mysql = mysql_init(QSE_NULL);
	if (!sql_node->mysql)
	{
		qse_awk_rtx_seterrfmt (rtx, QSE_AWK_ENOMEM, QSE_NULL, QSE_T("unable to allocate a mysql object"));
		return QSE_NULL;
	}

	return sql_node;
}

static void free_sql_node (qse_awk_rtx_t* rtx, sql_list_t* sql_list, sql_node_t* sql_node)
{
	mysql_close (sql_node->mysql);
	sql_node->mysql = QSE_NULL;
	__free_sql_node (rtx, sql_list, sql_node);
}


static res_node_t* new_res_node (qse_awk_rtx_t* rtx, res_list_t* res_list, MYSQL_RES* res)
{
	res_node_t* res_node;

	res_node = __new_res_node(rtx, res_list);
	if (!res_node) return QSE_NULL;

	res_node->res = res;
	res_node->num_fields = mysql_num_fields(res);

	return res_node;
}

static void free_res_node (qse_awk_rtx_t* rtx, res_list_t* res_list, res_node_t* res_node)
{
	mysql_free_result (res_node->res);
	res_node->res = QSE_NULL;
	__free_res_node (rtx, res_list, res_node);
}

/* ------------------------------------------------------------------------ */

static QSE_INLINE sql_list_t* rtx_to_sql_list (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	qse_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = qse_rbt_search((qse_rbt_t*)fi->mod->ctx, &rtx, QSE_SIZEOF(rtx));
	QSE_ASSERT (pair != QSE_NULL);
	data = (rtx_data_t*)QSE_RBT_VPTR(pair);
	return &data->sql_list;
}

static QSE_INLINE sql_node_t* get_sql_list_node (sql_list_t* sql_list, qse_awk_int_t id)
{
	if (id < 0 || id >= sql_list->map.high || !sql_list->map.tab[id]) return QSE_NULL;
	return sql_list->map.tab[id];
}

static QSE_INLINE res_list_t* rtx_to_res_list (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	qse_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = qse_rbt_search((qse_rbt_t*)fi->mod->ctx, &rtx, QSE_SIZEOF(rtx));
	QSE_ASSERT (pair != QSE_NULL);
	data = (rtx_data_t*)QSE_RBT_VPTR(pair);
	return &data->res_list;
}

static QSE_INLINE res_node_t* get_res_list_node (res_list_t* res_list, qse_awk_int_t id)
{
	if (id < 0 || id >= res_list->map.high || !res_list->map.tab[id]) return QSE_NULL;
	return res_list->map.tab[id];
}

/* ------------------------------------------------------------------------ */

static void set_error_on_sql_list (qse_awk_rtx_t* rtx, sql_list_t* sql_list, const qse_char_t* errfmt, ...)
{
	if (errfmt)
	{
		va_list ap;
		va_start (ap, errfmt);
		qse_strxvfmt (sql_list->errmsg, QSE_COUNTOF(sql_list->errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		qse_strxcpy (sql_list->errmsg, QSE_COUNTOF(sql_list->errmsg), qse_awk_rtx_geterrmsg(rtx));
	}
}

static int fnc_errmsg (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	qse_awk_val_t* retv;

	sql_list = rtx_to_sql_list(rtx, fi);
	retv = qse_awk_rtx_makestrvalwithstr(rtx, sql_list->errmsg);
	if (!retv) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */

static sql_node_t* get_sql_list_node_with_arg (qse_awk_rtx_t* rtx, sql_list_t* sql_list, qse_awk_val_t* arg)
{
	qse_awk_int_t id;
	sql_node_t* sql_node;

	if (qse_awk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list (rtx, sql_list, QSE_T("illegal instance id"));
		return QSE_NULL;
	}
	else if (!(sql_node = get_sql_list_node(sql_list, id)))
	{
		set_error_on_sql_list (rtx, sql_list, QSE_T("invalid instance id - %zd"), (qse_size_t)id);
		return QSE_NULL;
	}

	return sql_node;
}

static res_node_t* get_res_list_node_with_arg (qse_awk_rtx_t* rtx, sql_list_t* sql_list, res_list_t* res_list, qse_awk_val_t* arg)
{
	qse_awk_int_t id;
	res_node_t* res_node;

	if (qse_awk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list (rtx, sql_list, QSE_T("illegal result id"));
		return QSE_NULL;
	}
	else if (!(res_node = get_res_list_node(res_list, id)))
	{
		set_error_on_sql_list (rtx, sql_list, QSE_T("invalid result id - %zd"), (qse_size_t)id);
		return QSE_NULL;
	}

	return res_node;
}

/* ------------------------------------------------------------------------ */
static int fnc_open (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node = QSE_NULL;
	qse_awk_int_t ret = -1;
	qse_awk_val_t* retv;

	sql_list = rtx_to_sql_list(rtx, fi);

	sql_node = new_sql_node(rtx, sql_list);
	if (sql_node) ret = sql_node->id;
	else set_error_on_sql_list (rtx, sql_list, QSE_NULL);

	/* ret may not be a statically managed number. 
	 * error checking is required */
	retv = qse_awk_rtx_makeintval(rtx, ret);
	if (retv == QSE_NULL)
	{
		if (sql_node) free_sql_node (rtx, sql_list, sql_node);
		return -1;
	}

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_close (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		free_sql_node (rtx, sql_list, sql_node);
		ret = 0;
	}

	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;
}

#if 0
static int fnc_get_option (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		qse_awk_int_t id, tv;
		union
		{
			unsigned int ui;
			int b;
		} v;

		if (qse_awk_rtx_valtoint(rtx, qse_awk_rtx_getarg(rtx, 1), &id) <= -1)
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
					set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_errstr(sql_node->mysql));
					goto done;
				}

				retv = qse_awk_rtx_makeintval(rtx, v.ui);
				if (qse_awk_rtx_setrefval(rtx, (qse_awk_val_ref_t*)qse_awk_rtx_getarg(rtx, 2), retv) <= -1)
				{
					qse_awk_rtx_refupval(rtx, retv);
					qse_awk_rtx_refdownval(rtx, retv);
					goto oops;
				}
				break;

		#if defined(MYSQL_OPT_RECONNECT)
			case MYSQL_OPT_RECONNECT:
				/* bool * */
				if (mysql_get_option(sql_node->mysql, id, &v.b) != 0)
				{
					set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_errstr(sql_node->mysql));
					goto done;
				}

				retv = qse_awk_rtx_makeintval(rtx, v.b);
				if (!retv) goto oops;

				if (qse_awk_rtx_setrefval(rtx, (qse_awk_val_ref_t*)qse_awk_rtx_getarg(rtx, 2), retv) <= -1) 
				{
					qse_awk_rtx_refupval(rtx, retv);
					qse_awk_rtx_refdownval(rtx, retv);
					goto oops;
				}
				break;
		#else
				/* the system without MYSQL_OPT_RECONNECT available. return 1 all the time */
				if (qse_awk_rtx_setrefval(rtx, (qse_awk_val_ref_t*)qse_awk_rtx_getarg(rtx, 2), qse_awk_rtx_makeintval(rtx, 1)) <= -1) 
				{
					qse_awk_rtx_refupval(rtx, retv);
					qse_awk_rtx_refdownval(rtx, retv);
					goto oops;
				}
				break;
		#endif

			default:
				set_error_on_sql_list (rtx, sql_list, QSE_T("unsupported option id - %zd"), (qse_size_t)id);
				goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}
#endif

static int fnc_set_option (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		qse_awk_int_t id, tv;
		void* vptr;
		union
		{
			unsigned int ui;
			int b;
		} v;

		if (qse_awk_rtx_valtoint(rtx, qse_awk_rtx_getarg(rtx, 1), &id) <= -1)
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
				if (qse_awk_rtx_valtoint(rtx, qse_awk_rtx_getarg(rtx, 2), &tv) <= -1)
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
				if (qse_awk_rtx_valtoint(rtx, qse_awk_rtx_getarg(rtx, 2), &tv) <= -1)
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
				set_error_on_sql_list (rtx, sql_list, QSE_T("unsupported option id - %zd"), (qse_size_t)id);
				goto done;
		}

		if (mysql_options(sql_node->mysql, id, vptr) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}

static int fnc_connect (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	qse_awk_val_t* a1, * a2, * a3, * a4, * a6;
	qse_mchar_t* host = QSE_NULL;
	qse_mchar_t* user = QSE_NULL;
	qse_mchar_t* pass = QSE_NULL;
	qse_mchar_t* db = QSE_NULL;
	qse_awk_int_t port = 0;
	qse_mchar_t* usck = QSE_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		qse_size_t nargs;

		nargs = qse_awk_rtx_getnargs(rtx);

		a1 = qse_awk_rtx_getarg(rtx, 1);
		a2 = qse_awk_rtx_getarg(rtx, 2);
		a3 = qse_awk_rtx_getarg(rtx, 3);

		if (!(host = qse_awk_rtx_getvalmbs(rtx, a1, QSE_NULL)) ||
		    !(user = qse_awk_rtx_getvalmbs(rtx, a2, QSE_NULL)) ||
		    !(pass = qse_awk_rtx_getvalmbs(rtx, a3, QSE_NULL)))
		{
		arg_fail:
			take_rtx_err = 1;
			goto done; 
		}

		if (nargs >= 5)
		{
			a4 = qse_awk_rtx_getarg(rtx, 4);
			if (!(db = qse_awk_rtx_getvalmbs(rtx, a4, QSE_NULL))) goto arg_fail;
			if (nargs >= 6 && qse_awk_rtx_valtoint(rtx, qse_awk_rtx_getarg(rtx, 5), &port) <= -1) goto arg_fail;

			if (nargs >= 7)
			{
				a6 = qse_awk_rtx_getarg(rtx, 6);
				if (!(usck = qse_awk_rtx_getvalmbs(rtx, a6, QSE_NULL))) goto arg_fail;
			}
		}

		if (!mysql_real_connect(sql_node->mysql, host, user, pass, db, port, usck, 0))
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			sql_node->connect_ever_attempted = 1; /* doesn't matter if mysql_real_connect() failed */
			goto done;
		}

		sql_node->connect_ever_attempted = 1;
		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	if (usck) qse_awk_rtx_freevalmbs (rtx, a6, usck);
	if (db) qse_awk_rtx_freevalmbs (rtx, a4, db);
	if (pass) qse_awk_rtx_freevalmbs (rtx, a3, pass);
	if (user) qse_awk_rtx_freevalmbs (rtx, a2, user);
	if (host) qse_awk_rtx_freevalmbs (rtx, a1, host);

	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;
}

#define ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node) \
	do { \
		if (!(sql_node)->connect_ever_attempted) \
		{ \
			set_error_on_sql_list (rtx, sql_list, QSE_T("not connected")); \
			goto done; \
		} \
	} while(0)

static int fnc_ping (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_ping(sql_node->mysql) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}

static int fnc_select_db (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	qse_awk_val_t* a1;
	qse_mchar_t* db = QSE_NULL;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		a1 = qse_awk_rtx_getarg(rtx, 1);

		if (!(db = qse_awk_rtx_getvalmbs(rtx, a1, QSE_NULL))) { take_rtx_err = 1; goto oops; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_select_db(sql_node->mysql, db) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	if (db) qse_awk_rtx_freevalmbs (rtx, a1, db);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}


static int fnc_autocommit (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int v;

		v = qse_awk_rtx_valtobool(rtx, qse_awk_rtx_getarg(rtx, 1));

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_autocommit(sql_node->mysql, v) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}

static int fnc_commit (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int v;

		v = qse_awk_rtx_valtobool(rtx, qse_awk_rtx_getarg(rtx, 1));

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_commit(sql_node->mysql) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}

static int fnc_rollback (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int v;

		v = qse_awk_rtx_valtobool(rtx, qse_awk_rtx_getarg(rtx, 1));

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_rollback(sql_node->mysql) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}

static int fnc_affected_rows (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		my_ulonglong nrows;
		qse_awk_val_t* vrows;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		nrows = mysql_affected_rows(sql_node->mysql);
		if (nrows == (my_ulonglong)-1)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		vrows = qse_awk_rtx_makeintval(rtx, nrows);
		if (!vrows)
		{
			take_rtx_err = 1;
			goto oops;
		}

		if (qse_awk_rtx_setrefval(rtx, (qse_awk_val_ref_t*)qse_awk_rtx_getarg(rtx, 1), vrows) <= -1)
		{
			qse_awk_rtx_refupval (rtx, vrows);
			qse_awk_rtx_refdownval (rtx, vrows);
			take_rtx_err = 1;
			goto oops;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	return -1;
}

static int fnc_escape_string (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	qse_awk_val_t* a1, *retv;
	qse_mchar_t* qstr = QSE_NULL;
	qse_mchar_t* ebuf = QSE_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		qse_size_t qlen;

		a1 = qse_awk_rtx_getarg(rtx, 1);

		qstr = qse_awk_rtx_getvalmbs(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto oops; }

		ebuf = qse_awk_rtx_allocmem(rtx, (qlen * 2 + 1) * QSE_SIZEOF(*qstr));
		if (!ebuf) { take_rtx_err = 1; goto oops; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);
		mysql_real_escape_string(sql_node->mysql, ebuf, qstr, qlen);

		retv = qse_awk_rtx_makestrvalwithmbs(rtx, ebuf);
		if (!retv)
		{
			take_rtx_err = 1;
			goto oops;
		}

		if (qse_awk_rtx_setrefval(rtx, (qse_awk_val_ref_t*)qse_awk_rtx_getarg(rtx, 2), retv) <= -1)
		{
			qse_awk_rtx_refupval (rtx, retv);
			qse_awk_rtx_refdownval (rtx, retv);
			take_rtx_err = 1;
			goto oops;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	if (ebuf) qse_awk_rtx_freemem (rtx, ebuf);
	if (qstr) qse_awk_rtx_freevalmbs (rtx, a1, qstr);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (ebuf) qse_awk_rtx_freemem (rtx, ebuf);
	if (qstr) qse_awk_rtx_freevalmbs (rtx, a1, qstr);
	return -1;
}

static int fnc_query (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	qse_awk_val_t* a1;
	qse_mchar_t* qstr = QSE_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		qse_size_t qlen;
		a1 = qse_awk_rtx_getarg(rtx, 1);

		qstr = qse_awk_rtx_getvalmbs(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto oops; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_real_query(sql_node->mysql, qstr, qlen) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	if (qstr) qse_awk_rtx_freevalmbs (rtx, a1, qstr);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;

oops:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	if (qstr) qse_awk_rtx_freevalmbs (rtx, a1, qstr);
	return -1;
}

static int fnc_store_result (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, qse_awk_rtx_getarg(rtx, 0));
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
				set_error_on_sql_list (rtx, sql_list, QSE_T("no result"));
			else
				set_error_on_sql_list (rtx, sql_list, QSE_T("%hs"), mysql_error(sql_node->mysql));
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
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_free_result (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	res_list_t* res_list;
	res_node_t* res_node;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	res_list = rtx_to_res_list(rtx, fi);
	res_node = get_res_list_node_with_arg(rtx, sql_list, res_list, qse_awk_rtx_getarg(rtx, 0));
	if (res_node)
	{
		free_res_node (rtx, res_list, res_node);
		ret = 0;
	}

	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_fetch_row (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	res_list_t* res_list;
	res_node_t* res_node;
	int ret = -1, take_rtx_err = 0;
	qse_awk_val_t* row_map = QSE_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	res_list = rtx_to_res_list(rtx, fi);
	res_node = get_res_list_node_with_arg(rtx, sql_list, res_list, qse_awk_rtx_getarg(rtx, 0));
	if (res_node)
	{
		MYSQL_ROW row;
		unsigned int i;
		qse_awk_val_t* row_val;
		int x;

		row = mysql_fetch_row(res_node->res);
		if (!row)
		{
			/* no more row in the result */
			ret = 0;
			goto done;
		}

		row_map = qse_awk_rtx_makemapval(rtx);
		if (!row_map) goto oops;

		qse_awk_rtx_refupval (rtx, row_map);

		for (i = 0; i < res_node->num_fields; )
		{
			qse_char_t key_buf[QSE_SIZEOF(qse_awk_int_t) * 8 + 2];
			qse_size_t key_len;

			if (row[i])
			{
/* TODO: consider using make multi byte string - qse_awk_rtx_makembsstr */
				row_val = qse_awk_rtx_makestrvalwithmbs(rtx, row[i]);
				if (!row_val) goto oops;
			}
			else
			{
				row_val = qse_awk_rtx_makenilval(rtx);
			}

			++i;

			/* put it into the map */
			key_len = qse_awk_inttostr(qse_awk_rtx_getawk(rtx), i, 10, QSE_NULL, key_buf, QSE_COUNTOF(key_buf));
			QSE_ASSERT (key_len != (qse_size_t)-1);

			if (qse_awk_rtx_setmapvalfld(rtx, row_map, key_buf, key_len, row_val) == QSE_NULL)
			{
				qse_awk_rtx_refupval (rtx, row_val);
				qse_awk_rtx_refdownval (rtx, row_val);
				goto oops;
			}
		}

		x = qse_awk_rtx_setrefval(rtx, (qse_awk_val_ref_t*)qse_awk_rtx_getarg(rtx, 1), row_map);

		qse_awk_rtx_refdownval (rtx, row_map);
		row_map = QSE_NULL;

		if (x <= -1) goto oops;
		ret = 1; /* indicate that there is a row */
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, QSE_NULL);
	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval(rtx, ret));
	return 0;


oops:
	if (row_map) qse_awk_rtx_refdownval (rtx, row_map);
	return -1;
}

typedef struct fnctab_t fnctab_t;
struct fnctab_t
{
	const qse_char_t* name;
	qse_awk_mod_sym_fnc_t info;
};

typedef struct inttab_t inttab_t;
struct inttab_t
{
	const qse_char_t* name;
	qse_awk_mod_sym_int_t info;
};


#define A_MAX QSE_TYPE_MAX(int)

static fnctab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ QSE_T("affected_rows"),     { { 2, 2, QSE_T("vr") },  fnc_affected_rows,  0 } },
	{ QSE_T("autocommit"),        { { 2, 2, QSE_NULL },     fnc_autocommit,     0 } },
	{ QSE_T("close"),             { { 1, 1, QSE_NULL },     fnc_close,          0 } },
	{ QSE_T("commit"),            { { 1, 1, QSE_NULL },     fnc_commit,         0 } },
	{ QSE_T("connect"),           { { 4, 7, QSE_NULL },     fnc_connect,        0 } },
	{ QSE_T("errmsg"),            { { 0, 0, QSE_NULL },     fnc_errmsg,         0 } },
	{ QSE_T("escape_string"),     { { 3, 3, QSE_T("vvr") }, fnc_escape_string,  0 } },
	{ QSE_T("fetch_row"),         { { 2, 2, QSE_T("vr") },  fnc_fetch_row,      0 } },
	{ QSE_T("free_result"),       { { 1, 1, QSE_NULL },     fnc_free_result,    0 } },
	/*{ QSE_T("get_option"),       { { 3, 3, QSE_T("vr") },  fnc_get_option,     0 } },*/
	{ QSE_T("open"),              { { 0, 0, QSE_NULL },     fnc_open,           0 } },
	{ QSE_T("ping"),              { { 1, 1, QSE_NULL },     fnc_ping,           0 } },
	{ QSE_T("query"),             { { 2, 2, QSE_NULL },     fnc_query,          0 } },
	{ QSE_T("rollback"),          { { 1, 1, QSE_NULL },     fnc_rollback,       0 } },
	{ QSE_T("select_db"),         { { 2, 2, QSE_NULL },     fnc_select_db,     0 } },
	{ QSE_T("set_option"),        { { 3, 3, QSE_NULL },     fnc_set_option,     0 } },
	{ QSE_T("store_result"),      { { 1, 1, QSE_NULL },     fnc_store_result,   0 } }
};

static inttab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ QSE_T("OPT_CONNECT_TIMEOUT"), { MYSQL_OPT_CONNECT_TIMEOUT } },
	{ QSE_T("OPT_READ_TIMEOUT"),    { MYSQL_OPT_READ_TIMEOUT    } },
#if defined(MYSQL_OPT_RECONNECT)
	{ QSE_T("OPT_RECONNECT"),       { MYSQL_OPT_RECONNECT       } },
#else
	{ QSE_T("OPT_RECONNECT"),       { DUMMY_OPT_RECONNECT       } },
#endif
	{ QSE_T("OPT_WRITE_TIMEOUT"),   { MYSQL_OPT_WRITE_TIMEOUT   } }
};

static int query (qse_awk_mod_t* mod, qse_awk_t* awk, const qse_char_t* name, qse_awk_mod_sym_t* sym)
{
	qse_cstr_t ea;
	int left, right, mid, n;

	left = 0; right = QSE_COUNTOF(fnctab) - 1;

	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = qse_strcmp (fnctab[mid].name, name);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = QSE_AWK_MOD_FNC;
			sym->u.fnc = fnctab[mid].info;
			return 0;
		}
	}

	left = 0; right = QSE_COUNTOF(inttab) - 1;
	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = qse_strcmp (inttab[mid].name, name);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = QSE_AWK_MOD_INT;
			sym->u.in = inttab[mid].info;
			return 0;
		}
	}


	ea.ptr = (qse_char_t*)name;
	ea.len = qse_strlen(name);
	qse_awk_seterror (awk, QSE_AWK_ENOENT, &ea, QSE_NULL);
	return -1;
}

static int init (qse_awk_mod_t* mod, qse_awk_rtx_t* rtx)
{
	qse_rbt_t* rbt;
	rtx_data_t data;

	rbt = (qse_rbt_t*)mod->ctx;

	QSE_MEMSET (&data, 0, QSE_SIZEOF(data));
	if (qse_rbt_insert(rbt, &rtx, QSE_SIZEOF(rtx), &data, QSE_SIZEOF(data)) == QSE_NULL) 
	{
		qse_awk_rtx_seterrnum (rtx, QSE_AWK_ENOMEM, QSE_NULL);
		return -1;
	}

	return 0;
}

static void fini (qse_awk_mod_t* mod, qse_awk_rtx_t* rtx)
{
	qse_rbt_t* rbt;
	qse_rbt_pair_t* pair;
	
	rbt = (qse_rbt_t*)mod->ctx;

	/* garbage clean-up */
	pair = qse_rbt_search(rbt, &rtx, QSE_SIZEOF(rtx));
	if (pair)
	{
		rtx_data_t* data;
		sql_node_t* sql_node, * sql_next;
		res_node_t* res_node, * res_next;

		data = (rtx_data_t*)QSE_RBT_VPTR(pair);

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

		qse_rbt_delete (rbt, &rtx, QSE_SIZEOF(rtx));
	}
}

static void unload (qse_awk_mod_t* mod, qse_awk_t* awk)
{
	qse_rbt_t* rbt;

	rbt = (qse_rbt_t*)mod->ctx;

	QSE_ASSERT (QSE_RBT_SIZE(rbt) == 0);
	qse_rbt_close (rbt);
//mysql_library_end ();
}

int qse_awk_mod_mysql (qse_awk_mod_t* mod, qse_awk_t* awk)
{
	qse_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = qse_rbt_open(qse_awk_getmmgr(awk), 0, 1, 1);
	if (rbt == QSE_NULL) 
	{
		qse_awk_seterrnum (awk, QSE_AWK_ENOMEM, QSE_NULL);
		return -1;
	}
	qse_rbt_setstyle (rbt, qse_getrbtstyle(QSE_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}


QSE_EXPORT int qse_awk_mod_mysql_init (int argc, qse_achar_t* argv[])
{
	if (mysql_library_init(argc, argv, QSE_NULL) != 0) return -1;
	return 0;
}

QSE_EXPORT void qse_awk_mod_mysql_fini (void)
{
	mysql_library_end ();
}
