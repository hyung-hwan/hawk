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
#include "../lib/hawk-prv.h"

#if !defined(MYSQL_OPT_RECONNECT)
#	define DUMMY_OPT_RECONNECT 31231 /* randomly chosen */
#endif

struct param_data_t
{
	my_bool is_null;
	union
	{
		my_ulonglong llv;
		double dv;
		struct
		{
			char* ptr;
			unsigned long len;
		} sv;
	} u;
};
typedef struct param_data_t param_data_t;
typedef struct param_data_t res_data_t;

#define __IMAP_NODE_T_DATA  MYSQL* mysql; int connect_ever_attempted;
#define __IMAP_LIST_T_DATA  int errnum; hawk_ooch_t errmsg[256];
#define __IMAP_LIST_T sql_list_t
#define __IMAP_NODE_T sql_node_t
#define __MAKE_IMAP_NODE __new_sql_node
#define __FREE_IMAP_NODE __free_sql_node
#include "../lib/imap-imp.h"

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
#include "../lib/imap-imp.h"

#undef __IMAP_NODE_T_DATA
#undef __IMAP_LIST_T_DATA
#undef __IMAP_LIST_T
#undef __IMAP_NODE_T
#undef __MAKE_IMAP_NODE
#undef __FREE_IMAP_NODE

#define __IMAP_NODE_T_DATA  MYSQL_STMT* stmt; MYSQL_BIND* param_binds; param_data_t* param_data; hawk_oow_t param_capa; MYSQL_BIND* res_binds; res_data_t* res_data; hawk_oow_t res_capa;
#define __IMAP_LIST_T_DATA  /* int errnum; */
#define __IMAP_LIST_T stmt_list_t
#define __IMAP_NODE_T stmt_node_t
#define __MAKE_IMAP_NODE __new_stmt_node
#define __FREE_IMAP_NODE __free_stmt_node
#include "../lib/imap-imp.h"

struct rtx_data_t
{
	sql_list_t sql_list;
	res_list_t res_list;
	stmt_list_t stmt_list;
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
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_ENOMEM, HAWK_T("unable to allocate a mysql object"));
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

static stmt_node_t* new_stmt_node (hawk_rtx_t* rtx, stmt_list_t* stmt_list, MYSQL_STMT* stmt)
{
	stmt_node_t* stmt_node;

	stmt_node = __new_stmt_node(rtx, stmt_list);
	if (!stmt_node) return HAWK_NULL;

	stmt_node->stmt = stmt;

	return stmt_node;
}

static void free_stmt_node (hawk_rtx_t* rtx, stmt_list_t* stmt_list, stmt_node_t* stmt_node)
{
	mysql_stmt_close (stmt_node->stmt);
	stmt_node->stmt = HAWK_NULL;
	__free_stmt_node (rtx, stmt_list, stmt_node);
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


static HAWK_INLINE stmt_list_t* rtx_to_stmt_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	data = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	return &data->stmt_list;
}

static HAWK_INLINE stmt_node_t* get_stmt_list_node (stmt_list_t* stmt_list, hawk_int_t id)
{
	if (id < 0 || id >= stmt_list->map.high || !stmt_list->map.tab[id]) return HAWK_NULL;
	return stmt_list->map.tab[id];
}
/* ------------------------------------------------------------------------ */

static void set_error_on_sql_list (hawk_rtx_t* rtx, sql_list_t* sql_list, const hawk_ooch_t* errfmt, ...)
{
	if (errfmt)
	{
		va_list ap;
		va_start (ap, errfmt);
		hawk_rtx_vfmttooocstr (rtx, sql_list->errmsg, HAWK_COUNTOF(sql_list->errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_copy_oocstr (sql_list->errmsg, HAWK_COUNTOF(sql_list->errmsg), hawk_rtx_geterrmsg(rtx));
	}
}

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	hawk_val_t* retv;

	sql_list = rtx_to_sql_list(rtx, fi);
	retv = hawk_rtx_makestrvalwithoocstr(rtx, sql_list->errmsg);
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
		set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid instance id - %zd"), (hawk_oow_t)id);
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
		set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid result id - %zd"), (hawk_oow_t)id);
		return HAWK_NULL;
	}

	return res_node;
}

static stmt_node_t* get_stmt_list_node_with_arg (hawk_rtx_t* rtx, sql_list_t* sql_list, stmt_list_t* stmt_list, hawk_val_t* arg)
{
	hawk_int_t id;
	stmt_node_t* stmt_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list (rtx, sql_list, HAWK_T("illegal statement id"));
		return HAWK_NULL;
	}
	else if (!(stmt_node = get_stmt_list_node(stmt_list, id)))
	{
		set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid statement id - %zd"), (hawk_oow_t)id);
		return HAWK_NULL;
	}

	return stmt_node;
}

/* ------------------------------------------------------------------------ */
/*
	mysql = mysql::open();

	if (mysql::connect(mysql, "localhost", "username", "password", "database") <= -1)
	{
		print "connect error -", mysql::errmsg();
	}


	if (mysql::query(mysql, "select * from mytable") <= -1)
	{
		print "query error -", mysql::errmsg();
	}

	result = mysql::store_result(mysql);
	if (result <= -1)
	{
		print "store result error - ", mysql::errmsg();
	}

	while (mysql::fetch_row(result, row) > 0)
	{
		ncols = length(row);
		for (i = 0; i < ncols; i++) print row[i];
		print "----";
	}

	mysql::free_result(result);

	mysql::close(mysql);
*/

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
					take_rtx_err = 1;
					goto done;
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
					take_rtx_err = 1;
					goto done;
				}
				break;
		#else
				/* the system without MYSQL_OPT_RECONNECT available. return 1 all the time */
				if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), hawk_rtx_makeintval(rtx, 1)) <= -1) 
				{
					hawk_rtx_refupval(rtx, retv);
					hawk_rtx_refdownval(rtx, retv);
					take_rtx_err = 1;
					goto done;
				}
				break;
		#endif

			default:
				set_error_on_sql_list (rtx, sql_list, HAWK_T("unsupported option id - %zd"), (hawk_oow_t)id);
				goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
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
			goto done;
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
					goto done;
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
					goto done;
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
				set_error_on_sql_list (rtx, sql_list, HAWK_T("unsupported option id - %zd"), (hawk_oow_t)id);
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
}

static int fnc_connect (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	hawk_val_t* a1, * a2, * a3, * a4, * a6;
	hawk_bch_t* host = HAWK_NULL;
	hawk_bch_t* user = HAWK_NULL;
	hawk_bch_t* pass = HAWK_NULL;
	hawk_bch_t* db = HAWK_NULL;
	hawk_int_t port = 0;
	hawk_bch_t* usck = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_oow_t nargs;

		nargs = hawk_rtx_getnargs(rtx);

		a1 = hawk_rtx_getarg(rtx, 1);
		a2 = hawk_rtx_getarg(rtx, 2);
		a3 = hawk_rtx_getarg(rtx, 3);

		if (!(host = hawk_rtx_getvalbcstr(rtx, a1, HAWK_NULL)) ||
		    !(user = hawk_rtx_getvalbcstr(rtx, a2, HAWK_NULL)) ||
		    !(pass = hawk_rtx_getvalbcstr(rtx, a3, HAWK_NULL)))
		{
		arg_fail:
			take_rtx_err = 1;
			goto done; 
		}

		if (nargs >= 5)
		{
			a4 = hawk_rtx_getarg(rtx, 4);
			if (!(db = hawk_rtx_getvalbcstr(rtx, a4, HAWK_NULL))) goto arg_fail;
			if (nargs >= 6 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 5), &port) <= -1) goto arg_fail;

			if (nargs >= 7)
			{
				a6 = hawk_rtx_getarg(rtx, 6);
				if (!(usck = hawk_rtx_getvalbcstr(rtx, a6, HAWK_NULL))) goto arg_fail;
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
	if (usck) hawk_rtx_freevalbcstr (rtx, a6, usck);
	if (db) hawk_rtx_freevalbcstr (rtx, a4, db);
	if (pass) hawk_rtx_freevalbcstr (rtx, a3, pass);
	if (user) hawk_rtx_freevalbcstr (rtx, a2, user);
	if (host) hawk_rtx_freevalbcstr (rtx, a1, host);

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
}

static int fnc_select_db (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	hawk_val_t* a1;
	hawk_bch_t* db = HAWK_NULL;
	int ret = -1, take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		a1 = hawk_rtx_getarg(rtx, 1);

		if (!(db = hawk_rtx_getvalbcstr(rtx, a1, HAWK_NULL))) { take_rtx_err = 1; goto done; }

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
	if (db) hawk_rtx_freevalbcstr (rtx, a1, db);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
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
			goto done;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), vrows) <= -1)
		{
			hawk_rtx_refupval (rtx, vrows);
			hawk_rtx_refdownval (rtx, vrows);
			take_rtx_err = 1;
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_insert_id (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
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

		nrows = mysql_insert_id(sql_node->mysql);
		if (nrows == (my_ulonglong)-1)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		vrows = hawk_rtx_makeintval(rtx, nrows);
		if (!vrows)
		{
			take_rtx_err = 1;
			goto done;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), vrows) <= -1)
		{
			hawk_rtx_refupval (rtx, vrows);
			hawk_rtx_refdownval (rtx, vrows);
			take_rtx_err = 1;
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_escape_string (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	hawk_val_t* a1, *retv;
	hawk_bch_t* qstr = HAWK_NULL;
	hawk_bch_t* ebuf = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_oow_t qlen;

		a1 = hawk_rtx_getarg(rtx, 1);

		qstr = hawk_rtx_getvalbcstr(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto done; }

		ebuf = hawk_rtx_allocmem(rtx, (qlen * 2 + 1) * HAWK_SIZEOF(*qstr));
		if (!ebuf) { take_rtx_err = 1; goto done; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);
		mysql_real_escape_string(sql_node->mysql, ebuf, qstr, qlen);

		retv = hawk_rtx_makestrvalwithbcstr(rtx, ebuf);
		if (!retv)
		{
			take_rtx_err = 1;
			goto done;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), retv) <= -1)
		{
			hawk_rtx_refupval (rtx, retv);
			hawk_rtx_refdownval (rtx, retv);
			take_rtx_err = 1;
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (ebuf) hawk_rtx_freemem (rtx, ebuf);
	if (qstr) hawk_rtx_freevalbcstr (rtx, a1, qstr);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_query (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1, take_rtx_err = 0;

	hawk_val_t* a1;
	hawk_bch_t* qstr = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_oow_t qlen;
		a1 = hawk_rtx_getarg(rtx, 1);

		qstr = hawk_rtx_getvalbcstr(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto done; }

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
	if (qstr) hawk_rtx_freevalbcstr (rtx, a1, qstr);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
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
		if (!row_map)
		{
			take_rtx_err = 1;
			goto done;
		}

		hawk_rtx_refupval (rtx, row_map);

		for (i = 0; i < res_node->num_fields; )
		{
			hawk_ooch_t key_buf[HAWK_SIZEOF(hawk_int_t) * 8 + 2];
			hawk_oow_t key_len;

			if (row[i])
			{
/* TODO: consider using make multi byte string - hawk_rtx_makembsstr depending on user options or depending on column types */
				row_val = hawk_rtx_makestrvalwithbcstr(rtx, row[i]);
				if (!row_val) 
				{
					take_rtx_err = 1;
					goto done;
				}
			}
			else
			{
				row_val = hawk_rtx_makenilval(rtx);
			}

			++i;

			/* put it into the map */ 
			key_len = hawk_int_to_oocstr(i, 10, HAWK_NULL, key_buf, HAWK_COUNTOF(key_buf)); /* TOOD: change this function to hawk_rtx_intxxxxx */
			HAWK_ASSERT (key_len != (hawk_oow_t)-1);

			if (hawk_rtx_setmapvalfld(rtx, row_map, key_buf, key_len, row_val) == HAWK_NULL)
			{
				hawk_rtx_refupval (rtx, row_val);
				hawk_rtx_refdownval (rtx, row_val);
				take_rtx_err = 1;
				goto done;
			}
		}

		x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), row_map);

		hawk_rtx_refdownval (rtx, row_map);
		row_map = HAWK_NULL;

		if (x <= -1)
		{
			take_rtx_err = 1;
			goto done;
		}

		ret = 1; /* indicate that there is a row */
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (row_map) hawk_rtx_refdownval (rtx, row_map);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

/* -------------------------------------------------------------------------- */
static int fnc_stmt_init (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;

	int ret = -1;
	int take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));

	if (sql_list)
	{
		stmt_list_t* stmt_list;
		stmt_node_t* stmt_node;
		MYSQL_STMT* stmt;

		stmt_list = rtx_to_stmt_list(rtx, fi);

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		stmt = mysql_stmt_init(sql_node->mysql);
		if (!stmt)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_error(sql_node->mysql));
			goto done;
		}

		stmt_node = new_stmt_node(rtx, stmt_list, stmt);
		if (!stmt_node)
		{
			mysql_stmt_close (stmt);
			take_rtx_err = 1;
			goto done;
		}

		ret = stmt_node->id;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_stmt_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		if (stmt_node->res_data) 
		{
			if (stmt_node->res_binds)
			{
				/* the program guarantees thats res_binds is non-null if res_data is to have 
				 * some valid data. so i do this clearance if res_binds is non-null */
				hawk_oow_t i;
				for (i = 0; i < stmt_node->res_capa; i++)
				{
					res_data_t* data = &stmt_node->res_data[i];
					MYSQL_BIND* bind = &stmt_node->res_binds[i];
					if ((bind->buffer_type == MYSQL_TYPE_STRING || bind->buffer_type == MYSQL_TYPE_BLOB) && data->u.sv.ptr)
					{
						hawk_rtx_freemem (rtx, data->u.sv.ptr);
						data->u.sv.ptr = HAWK_NULL;
						data->u.sv.len = 0;
					}
				}
			}
			hawk_rtx_freemem (rtx, stmt_node->res_data);
		}
		if (stmt_node->res_binds) hawk_rtx_freemem (rtx, stmt_node->res_binds);


		if (stmt_node->param_data) 
		{
			if (stmt_node->param_binds)
			{
				/* the program guarantees thats param_binds is non-null if param_data is to have 
				 * some valid data. so i do this clearance if param_binds is non-null */
				hawk_oow_t i;
				for (i = 0; i < stmt_node->param_capa; i++)
				{
					param_data_t* data = &stmt_node->param_data[i];
					MYSQL_BIND* bind = &stmt_node->param_binds[i];
					if ((bind->buffer_type == MYSQL_TYPE_STRING || bind->buffer_type == MYSQL_TYPE_BLOB) && data->u.sv.ptr)
					{
						hawk_rtx_freemem (rtx, data->u.sv.ptr);
						data->u.sv.ptr = HAWK_NULL;
						data->u.sv.len = 0;
					}
				}
			}
			hawk_rtx_freemem (rtx, stmt_node->param_data);
		}
		if (stmt_node->param_binds) hawk_rtx_freemem (rtx, stmt_node->param_binds);


		free_stmt_node (rtx, stmt_list, stmt_node);
		ret = 0;
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_stmt_prepare (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int take_rtx_err = 0;
	hawk_val_t* a1;
	hawk_bch_t* qstr = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		hawk_oow_t qlen;

		a1 = hawk_rtx_getarg(rtx, 1);

		qstr = hawk_rtx_getvalbcstr(rtx, a1, &qlen);
		if (!qstr) { take_rtx_err = 1; goto done; }

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		if (mysql_stmt_prepare(stmt_node->stmt, qstr, qlen) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_stmt_error(stmt_node->stmt));
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	if (qstr) hawk_rtx_freevalbcstr (rtx, a1, qstr);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_stmt_execute (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int take_rtx_err = 0;
	MYSQL_RES* res_meta = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		hawk_oow_t nargs, nparams, i;
		hawk_oow_t param_count;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		nargs = hawk_rtx_getnargs(rtx);
		nparams = (nargs - 1) / 2;
		param_count = mysql_stmt_param_count(stmt_node->stmt);
		if (nparams != param_count)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid number of pramaters"));
			goto done;
		}

		for (i = 0; i < stmt_node->param_capa; i++)
		{
			MYSQL_BIND* bind;
			param_data_t* data;

			bind = &stmt_node->param_binds[i];
			data = &stmt_node->param_data[i];

			if ((bind->buffer_type == MYSQL_TYPE_STRING || bind->buffer_type == MYSQL_TYPE_BLOB) && data->u.sv.ptr)
			{
				hawk_rtx_freemem (rtx, data->u.sv.ptr);
				data->u.sv.ptr = HAWK_NULL;
				data->u.sv.len = 0;
			}
		}

		if (nparams > stmt_node->param_capa)
		{
			MYSQL_BIND* binds;
			param_data_t* data;

			binds = hawk_rtx_reallocmem(rtx, stmt_node->param_binds, HAWK_SIZEOF(MYSQL_BIND) * nparams);
			if (!binds) { take_rtx_err = 1; goto done; }
			stmt_node->param_binds = binds;

			data = hawk_rtx_reallocmem(rtx, stmt_node->param_data, HAWK_SIZEOF(param_data_t) * nparams);
			if (!data) { take_rtx_err = 1; goto done; }
			stmt_node->param_data = data;

			/* update the capacity only if both (re)allocations gets successful */
			stmt_node->param_capa = nparams;
		}

		HAWK_MEMSET (stmt_node->param_binds, 0, HAWK_SIZEOF(MYSQL_BIND)* stmt_node->param_capa);
		HAWK_MEMSET (stmt_node->param_data, 0, HAWK_SIZEOF(param_data_t)* stmt_node->param_capa);

		for (i = 1; i < nargs; i += 2)
		{
			hawk_val_t* ta, * va;
			hawk_oow_t j;
			hawk_int_t type;
			MYSQL_BIND* bind;
			param_data_t* data;

			ta = hawk_rtx_getarg(rtx, i);
			va = hawk_rtx_getarg(rtx, i + 1);
			j = (i >> 1);

			if (hawk_rtx_valtoint(rtx, ta, &type) <= -1) { take_rtx_err = 1; goto done; }

			bind = &stmt_node->param_binds[j];
			data = &stmt_node->param_data[j];

			switch (type)
			{
				case MYSQL_TYPE_LONG:
				{
					hawk_int_t iv;

					if (hawk_rtx_valtoint(rtx, va, &iv) <= -1) { take_rtx_err = 1; goto done; }
					data->u.llv = iv;
					data->is_null = hawk_rtx_isnilval(rtx, va);

					bind->buffer_type = MYSQL_TYPE_LONGLONG;
					bind->buffer = &data->u.llv;
					bind->is_null = &data->is_null;
					break;
				}

				case MYSQL_TYPE_FLOAT:
				{
					hawk_flt_t fv;

					if (hawk_rtx_valtoflt(rtx, va, &fv) <= -1) { take_rtx_err = 1; goto done; }
					data->u.dv = fv;
					data->is_null = hawk_rtx_isnilval(rtx, va);

					bind->buffer_type = MYSQL_TYPE_DOUBLE;
					bind->buffer = &data->u.dv;
					bind->is_null = &data->is_null;
					break;
				}

				case MYSQL_TYPE_STRING:
				case MYSQL_TYPE_BLOB:
				{
					hawk_bch_t* ptr;
					hawk_oow_t len;

					ptr = hawk_rtx_valtobcstrdup(rtx, va, &len);
					if (!ptr) { take_rtx_err = 1; goto done; }

					data->u.sv.ptr = ptr;
					data->u.sv.len = len;
					data->is_null = hawk_rtx_isnilval(rtx, va);

					bind->buffer_type = type;
					bind->buffer = data->u.sv.ptr;
					bind->length = &data->u.sv.len;
					bind->is_null = &data->is_null;
					break;
				}

				default:
					set_error_on_sql_list (rtx, sql_list, HAWK_T("invalid value type"));
					goto done;
			}

		}

		if (mysql_stmt_bind_param(stmt_node->stmt, stmt_node->param_binds) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_stmt_error(stmt_node->stmt));
			goto done;
		}

		if (mysql_stmt_execute(stmt_node->stmt) != 0)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_stmt_error(stmt_node->stmt));
			goto done;
		}

		res_meta = mysql_stmt_result_metadata(stmt_node->stmt);
		if (res_meta)
		{
			/* the statement will return results */
			unsigned int ncols;
			MYSQL_FIELD* cinfo;

			MYSQL_BIND* bind;
			res_data_t* data;

			ncols = mysql_num_fields(res_meta);
			cinfo = mysql_fetch_fields(res_meta);

			for (i = 0; i < stmt_node->res_capa; i++)
			{
				MYSQL_BIND* bind;
				res_data_t* data;

				bind = &stmt_node->res_binds[i];
				data = &stmt_node->res_data[i];

				if ((bind->buffer_type == MYSQL_TYPE_STRING || bind->buffer_type == MYSQL_TYPE_BLOB) && data->u.sv.ptr)
				{
					hawk_rtx_freemem (rtx, data->u.sv.ptr);
					data->u.sv.ptr = HAWK_NULL;
					data->u.sv.len = 0;
				}
			}

			if (ncols > stmt_node->res_capa)
			{
				MYSQL_BIND* binds;
				res_data_t* data;

				binds = hawk_rtx_reallocmem(rtx, stmt_node->res_binds, HAWK_SIZEOF(MYSQL_BIND) * ncols);
				if (!binds) { take_rtx_err = 1; goto done; }
				stmt_node->res_binds = binds;

				data = hawk_rtx_reallocmem(rtx, stmt_node->res_data, HAWK_SIZEOF(res_data_t) * ncols);
				if (!data) { take_rtx_err = 1; goto done; }
				stmt_node->res_data = data;

				/* update the capacity only if both (re)allocations gets successful */
				stmt_node->res_capa = ncols;
			}

			HAWK_MEMSET (stmt_node->res_binds, 0, HAWK_SIZEOF(MYSQL_BIND)* stmt_node->res_capa);
			HAWK_MEMSET (stmt_node->res_data, 0, HAWK_SIZEOF(res_data_t)* stmt_node->res_capa);

			
			for (i = 0; i < ncols; i++)
			{
/* TOOD: more types... */
				bind = &stmt_node->res_binds[i];
				data = &stmt_node->res_data[i];

				switch(cinfo[i].type)
				{
					case MYSQL_TYPE_LONGLONG:
					case MYSQL_TYPE_LONG:
					case MYSQL_TYPE_SHORT:
					case MYSQL_TYPE_TINY:
					case MYSQL_TYPE_BIT:
					case MYSQL_TYPE_INT24:
						bind->buffer_type = MYSQL_TYPE_LONGLONG;
						bind->buffer = &data->u.llv;
						break;

					case MYSQL_TYPE_FLOAT:
					case MYSQL_TYPE_DOUBLE:
						bind->buffer_type = MYSQL_TYPE_DOUBLE;
						bind->buffer = &data->u.dv;
						break;

					case MYSQL_TYPE_STRING:
					case MYSQL_TYPE_VARCHAR:
					case MYSQL_TYPE_VAR_STRING:
						bind->buffer_type = MYSQL_TYPE_STRING;

					res_string:
						data->u.sv.ptr = hawk_rtx_allocmem(rtx, cinfo[i].length + 1);
						if (!data->u.sv.ptr) { take_rtx_err = 1; goto done; }
						data->u.sv.len = 0;

						bind->buffer = data->u.sv.ptr;
						bind->buffer_length = cinfo[i].length + 1;
						bind->length = &data->u.sv.len;
						break;

					case MYSQL_TYPE_BLOB:
					case MYSQL_TYPE_TINY_BLOB:
					case MYSQL_TYPE_MEDIUM_BLOB:
					case MYSQL_TYPE_LONG_BLOB:
					default: /* TOOD: i must not do this.... treat others properly */
						bind->buffer_type = MYSQL_TYPE_BLOB;
						goto res_string;
				}

				bind->is_null = &data->is_null;
			}

			if (mysql_stmt_bind_result(stmt_node->stmt, stmt_node->res_binds) != 0)
			{
				set_error_on_sql_list (rtx, sql_list, HAWK_T("%hs"), mysql_stmt_error(stmt_node->stmt));
				goto done;
			}
		}

		ret = 0;
	}

done:
	if (res_meta) mysql_free_result (res_meta);
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_stmt_fetch (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		int n;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		n = mysql_stmt_fetch(stmt_node->stmt);
		if (n == MYSQL_NO_DATA) ret = 0; /* no more data */
		else if (n == 0)
		{
			hawk_oow_t i, j, nargs;
			MYSQL_BIND* bind;
			res_data_t* data;
			/* there is data */

			nargs = hawk_rtx_getnargs(rtx);
/* TODO: argument count check */

			for (i = 1; i < nargs; i++)
			{
				hawk_val_t* cv;

				j = i - 1;
				bind = &stmt_node->res_binds[j];
				data = &stmt_node->res_data[j];

				if (data->is_null) cv = hawk_rtx_makenilval(rtx);
				else
				{
					switch (bind->buffer_type)
					{
						case MYSQL_TYPE_LONGLONG:
							cv = hawk_rtx_makeintval(rtx, data->u.llv);
							break;
						case MYSQL_TYPE_DOUBLE:
							cv = hawk_rtx_makefltval(rtx, data->u.dv);
							break;
						case MYSQL_TYPE_STRING:
							cv = hawk_rtx_makestrvalwithbchars(rtx, data->u.sv.ptr, data->u.sv.len);
							break;
						case MYSQL_TYPE_BLOB:
							cv = hawk_rtx_makembsvalwithbchars(rtx, data->u.sv.ptr, data->u.sv.len);
							break;

						default:
							/* this must not happen */
							set_error_on_sql_list (rtx, sql_list, HAWK_T("internal error - invalid buffer_type %d"), (int)bind->buffer_type);
							goto done;
					}
				}

				if (!cv || hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, i), cv) <= -1)
				{
					take_rtx_err = 1;
					goto done;
				}
			}

			ret = 1; /* have data */
		}
		else if (n == MYSQL_DATA_TRUNCATED)
		{
			set_error_on_sql_list (rtx, sql_list, HAWK_T("data truncated"));
		}
		else
		{
			take_rtx_err = 1;
			goto done;
		}
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}


static int fnc_stmt_affected_rows (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		my_ulonglong nrows;
		hawk_val_t* vrows;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		nrows = mysql_stmt_affected_rows(stmt_node->stmt);

		vrows = hawk_rtx_makeintval(rtx, nrows);
		if (!vrows)
		{
			take_rtx_err = 1;
			goto done;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), vrows) <= -1)
		{
			hawk_rtx_refupval (rtx, vrows);
			hawk_rtx_refdownval (rtx, vrows);
			take_rtx_err = 1;
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}


static int fnc_stmt_insert_id (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int take_rtx_err = 0;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		my_ulonglong nrows;
		hawk_val_t* vrows;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		nrows = mysql_stmt_insert_id(stmt_node->stmt);

		vrows = hawk_rtx_makeintval(rtx, nrows);
		if (!vrows)
		{
			take_rtx_err = 1;
			goto done;
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), vrows) <= -1)
		{
			hawk_rtx_refupval (rtx, vrows);
			hawk_rtx_refdownval (rtx, vrows);
			take_rtx_err = 1;
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list (rtx, sql_list, HAWK_NULL);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
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
	{ HAWK_T("affected_rows"),      { { 2, 2, HAWK_T("vr") },  fnc_affected_rows,   0 } },
	{ HAWK_T("autocommit"),         { { 2, 2, HAWK_NULL },     fnc_autocommit,      0 } },
	{ HAWK_T("close"),              { { 1, 1, HAWK_NULL },     fnc_close,           0 } },
	{ HAWK_T("commit"),             { { 1, 1, HAWK_NULL },     fnc_commit,          0 } },
	{ HAWK_T("connect"),            { { 4, 7, HAWK_NULL },     fnc_connect,         0 } },
	{ HAWK_T("errmsg"),             { { 0, 0, HAWK_NULL },     fnc_errmsg,          0 } },
	{ HAWK_T("escape_string"),      { { 3, 3, HAWK_T("vvr") }, fnc_escape_string,   0 } },
	{ HAWK_T("fetch_row"),          { { 2, 2, HAWK_T("vr") },  fnc_fetch_row,       0 } },
	{ HAWK_T("free_result"),        { { 1, 1, HAWK_NULL },     fnc_free_result,     0 } },
	/*{ HAWK_T("get_option"),        { { 3, 3, HAWK_T("vr") },  fnc_get_option,      0 } },*/
	{ HAWK_T("insert_id"),          { { 2, 2, HAWK_T("vr") },  fnc_insert_id,       0 } },
	{ HAWK_T("open"),               { { 0, 0, HAWK_NULL },     fnc_open,            0 } },
	{ HAWK_T("ping"),               { { 1, 1, HAWK_NULL },     fnc_ping,            0 } },
	{ HAWK_T("query"),              { { 2, 2, HAWK_NULL },     fnc_query,           0 } },
	{ HAWK_T("rollback"),           { { 1, 1, HAWK_NULL },     fnc_rollback,        0 } },
	{ HAWK_T("select_db"),          { { 2, 2, HAWK_NULL },     fnc_select_db,       0 } },
	{ HAWK_T("set_option"),         { { 3, 3, HAWK_NULL },     fnc_set_option,      0 } },

	{ HAWK_T("stmt_affected_rows"), { { 2, 2, HAWK_T("vr") },  fnc_stmt_affected_rows, 0 } },
	{ HAWK_T("stmt_close"),         { { 1, 1, HAWK_NULL },     fnc_stmt_close,      0 } },
	{ HAWK_T("stmt_execute"),       { { 1, A_MAX, HAWK_NULL }, fnc_stmt_execute,    0 } },
	{ HAWK_T("stmt_fetch"),         { { 2, A_MAX, HAWK_T("vr") },  fnc_stmt_fetch, 0 } },
	{ HAWK_T("stmt_init"),          { { 1, 1, HAWK_NULL },     fnc_stmt_init,       0 } },
	{ HAWK_T("stmt_insert_id"),     { { 2, 2, HAWK_T("vr") },  fnc_stmt_insert_id,  0 } },
	{ HAWK_T("stmt_prepare"),       { { 2, 2, HAWK_NULL },     fnc_stmt_prepare,    0 } },

	{ HAWK_T("store_result"),       { { 1, 1, HAWK_NULL },     fnc_store_result,    0 } }
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
	{ HAWK_T("OPT_WRITE_TIMEOUT"),   { MYSQL_OPT_WRITE_TIMEOUT   } },

	{ HAWK_T("TYPE_BIN"),            { MYSQL_TYPE_BLOB           } },
	{ HAWK_T("TYPE_FLT"),            { MYSQL_TYPE_FLOAT          } },
	{ HAWK_T("TYPE_INT"),            { MYSQL_TYPE_LONG           } },
	{ HAWK_T("TYPE_STR"),            { MYSQL_TYPE_STRING         } }
};

static int query (hawk_mod_t* mod, hawk_t* awk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int left, right, mid, n;

	left = 0; right = HAWK_COUNTOF(fnctab) - 1;

	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = hawk_comp_oocstr(fnctab[mid].name, name, 0);
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

		n = hawk_comp_oocstr(inttab[mid].name, name, 0);
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
	if (hawk_rbt_insert(rbt, &rtx, HAWK_SIZEOF(rtx), &data, HAWK_SIZEOF(data)) == HAWK_NULL)  return -1;

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

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (rbt == HAWK_NULL) return -1;

	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}


HAWK_EXPORT int hawk_mod_mysql_init (int argc, char* argv[])
{
	if (mysql_library_init(argc, argv, HAWK_NULL) != 0) return -1;
	return 0;
}

HAWK_EXPORT void hawk_mod_mysql_fini (void)
{
	mysql_library_end ();
}
