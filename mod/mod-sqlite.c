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

#include "mod-sqlite.h"

#include <sqlite3.h>

#include "../lib/hawk-prv.h"

#define __IDMAP_NODE_T_DATA  sqlite3* db; int connect_ever_attempted;
#define __IDMAP_LIST_T_DATA  hawk_ooch_t errmsg[256];
#define __IDMAP_LIST_T sql_list_t
#define __IDMAP_NODE_T sql_node_t
#define __INIT_IDMAP_LIST __init_sql_list
#define __FINI_IDMAP_LIST __fini_sql_list
#define __MAKE_IDMAP_NODE __new_sql_node
#define __FREE_IDMAP_NODE __free_sql_node
#include "../lib/idmap-imp.h"

#undef __IDMAP_NODE_T_DATA
#undef __IDMAP_LIST_T_DATA
#undef __IDMAP_LIST_T
#undef __IDMAP_NODE_T
#undef __INIT_IDMAP_LIST
#undef __FINI_IDMAP_LIST
#undef __MAKE_IDMAP_NODE
#undef __FREE_IDMAP_NODE

#define __IDMAP_NODE_T_DATA  sqlite3_stmt* stmt; sqlite3* db; int col_count;
#define __IDMAP_LIST_T_DATA  /* none */
#define __IDMAP_LIST_T stmt_list_t
#define __IDMAP_NODE_T stmt_node_t
#define __INIT_IDMAP_LIST __init_stmt_list
#define __FINI_IDMAP_LIST __fini_stmt_list
#define __MAKE_IDMAP_NODE __new_stmt_node
#define __FREE_IDMAP_NODE __free_stmt_node
#include "../lib/idmap-imp.h"

#undef __IDMAP_NODE_T_DATA
#undef __IDMAP_LIST_T_DATA
#undef __IDMAP_LIST_T
#undef __IDMAP_NODE_T
#undef __INIT_IDMAP_LIST
#undef __FINI_IDMAP_LIST
#undef __MAKE_IDMAP_NODE
#undef __FREE_IDMAP_NODE

struct rtx_data_t
{
	sql_list_t sql_list;
	stmt_list_t stmt_list;
};
typedef struct rtx_data_t rtx_data_t;

static sql_node_t* new_sql_node (hawk_rtx_t* rtx, sql_list_t* sql_list)
{
	sql_node_t* sql_node;

	sql_node = __new_sql_node(rtx, sql_list);
	if (HAWK_UNLIKELY(!sql_node)) return HAWK_NULL;

	sql_node->db = HAWK_NULL;
	sql_node->connect_ever_attempted = 0;
	return sql_node;
}

static void free_sql_node (hawk_rtx_t* rtx, sql_list_t* sql_list, sql_node_t* sql_node)
{
	HAWK_ASSERT(sql_node->db != HAWK_NULL);
	sqlite3_close(sql_node->db);
	sql_node->db = HAWK_NULL;
	__free_sql_node(rtx, sql_list, sql_node);
}

static stmt_node_t* new_stmt_node (hawk_rtx_t* rtx, stmt_list_t* stmt_list, sqlite3_stmt* stmt, sqlite3* db)
{
	stmt_node_t* stmt_node;

	stmt_node = __new_stmt_node(rtx, stmt_list);
	if (HAWK_UNLIKELY(!stmt_node)) return HAWK_NULL;

	stmt_node->stmt = stmt;
	stmt_node->db = db;
	stmt_node->col_count = sqlite3_column_count(stmt);
	return stmt_node;
}

static void free_stmt_node (hawk_rtx_t* rtx, stmt_list_t* stmt_list, stmt_node_t* stmt_node)
{
	HAWK_ASSERT(stmt_node->stmt != HAWK_NULL);
	sqlite3_finalize(stmt_node->stmt);
	stmt_node->stmt = HAWK_NULL;
	stmt_node->db = HAWK_NULL;
	__free_stmt_node(rtx, stmt_list, stmt_node);
}

/* ------------------------------------------------------------------------ */

static HAWK_INLINE sql_list_t* rtx_to_sql_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT(pair != HAWK_NULL);
	data = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	return &data->sql_list;
}

static HAWK_INLINE sql_node_t* get_sql_list_node (sql_list_t* sql_list, hawk_int_t id)
{
	if (id < 0 || id >= sql_list->map.high || !sql_list->map.tab[id]) return HAWK_NULL;
	return sql_list->map.tab[id];
}

static HAWK_INLINE stmt_list_t* rtx_to_stmt_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	rtx_data_t* data;
	pair = hawk_rbt_search((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT(pair != HAWK_NULL);
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
		va_start(ap, errfmt);
		hawk_rtx_vfmttooocstr(rtx, sql_list->errmsg, HAWK_COUNTOF(sql_list->errmsg), errfmt, ap);
		va_end(ap);
	}
	else
	{
		hawk_copy_oocstr(sql_list->errmsg, HAWK_COUNTOF(sql_list->errmsg), hawk_rtx_geterrmsg(rtx));
	}
}

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	hawk_val_t* retv;

	sql_list = rtx_to_sql_list(rtx, fi);
	retv = hawk_rtx_makestrvalwithoocstr(rtx, sql_list->errmsg);
	if (HAWK_UNLIKELY(!retv)) return -1;

	hawk_rtx_setretval(rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */

static sql_node_t* get_sql_list_node_with_arg (hawk_rtx_t* rtx, sql_list_t* sql_list, hawk_val_t* arg)
{
	hawk_int_t id;
	sql_node_t* sql_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list(rtx, sql_list, HAWK_T("illegal instance id"));
		return HAWK_NULL;
	}
	else if (!(sql_node = get_sql_list_node(sql_list, id)))
	{
		set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid instance id - %zd"), (hawk_oow_t)id);
		return HAWK_NULL;
	}

	return sql_node;
}

static stmt_node_t* get_stmt_list_node_with_arg (hawk_rtx_t* rtx, sql_list_t* sql_list, stmt_list_t* stmt_list, hawk_val_t* arg)
{
	hawk_int_t id;
	stmt_node_t* stmt_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		set_error_on_sql_list(rtx, sql_list, HAWK_T("illegal statement id"));
		return HAWK_NULL;
	}
	else if (!(stmt_node = get_stmt_list_node(stmt_list, id)))
	{
		set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid statement id - %zd"), (hawk_oow_t)id);
		return HAWK_NULL;
	}

	return stmt_node;
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
	else set_error_on_sql_list(rtx, sql_list, HAWK_NULL);

	retv = hawk_rtx_makeintval(rtx, ret);
	if (HAWK_UNLIKELY(!retv))
	{
		if (sql_node) free_sql_node(rtx, sql_list, sql_node);
		return -1;
	}

	hawk_rtx_setretval(rtx, retv);
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
		free_sql_node(rtx, sql_list, sql_node);
		ret = 0;
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_connect (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	hawk_val_t* a1 = HAWK_NULL;
	hawk_val_t* a3 = HAWK_NULL;
	hawk_bch_t* path = HAWK_NULL;
	hawk_bch_t* vfs = HAWK_NULL;
	hawk_int_t flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	int ret = -1;
	int take_rtx_err = 0;
	int rc;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		hawk_oow_t nargs;

		nargs = hawk_rtx_getnargs(rtx);

		a1 = hawk_rtx_getarg(rtx, 1);
		path = hawk_rtx_getvalbcstr(rtx, a1, HAWK_NULL);
		if (!path)
		{
			take_rtx_err = 1;
			goto done;
		}

		if (nargs >= 3)
		{
			hawk_val_t* a2 = hawk_rtx_getarg(rtx, 2);
			if (hawk_rtx_valtoint(rtx, a2, &flags) <= -1) /* flags */
			{
				take_rtx_err = 1;
				goto done;
			}
		}

		if (nargs >= 4)
		{
			a3 = hawk_rtx_getarg(rtx, 3);
			vfs = hawk_rtx_getvalbcstr(rtx, a3, HAWK_NULL);
			if (HAWK_UNLIKELY(!vfs))
			{
				take_rtx_err = 1;
				goto done;
			}
		}

		if (sql_node->db)
		{
			sqlite3_close(sql_node->db);
			sql_node->db = HAWK_NULL;
		}

		rc = sqlite3_open_v2(path, &sql_node->db, (int)flags, vfs);
		sql_node->connect_ever_attempted = 1;
		if (rc != SQLITE_OK)
		{
			if (sql_node->db)
			{
				set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(sql_node->db));
				sqlite3_close(sql_node->db);
				sql_node->db = HAWK_NULL;
			}
			else
			{
				set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errstr(rc));
			}
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list(rtx, sql_list, HAWK_NULL);
	if (vfs) hawk_rtx_freevalbcstr(rtx, a3, vfs);
	if (path) hawk_rtx_freevalbcstr(rtx, a1, path);

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

#define ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node) \
	do { \
		if (!(sql_node)->connect_ever_attempted || !(sql_node)->db) \
		{ \
			set_error_on_sql_list(rtx, sql_list, HAWK_T("not connected")); \
			goto done; \
		} \
	} while (0)

static int fnc_exec (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	hawk_val_t* a1;
	hawk_bch_t* sql = HAWK_NULL;
	int ret = -1;
	int take_rtx_err = 0;
	int rc;
	char* err = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		a1 = hawk_rtx_getarg(rtx, 1);
		sql = hawk_rtx_getvalbcstr(rtx, a1, HAWK_NULL);
		if (!sql)
		{
			take_rtx_err = 1;
			goto done;
		}

		rc = sqlite3_exec(sql_node->db, sql, HAWK_NULL, HAWK_NULL, &err);
		if (rc != SQLITE_OK)
		{
			if (err)
			{
				set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), err);
				sqlite3_free(err);
			}
			else
			{
				set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(sql_node->db));
			}
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list(rtx, sql_list, HAWK_NULL);
	if (sql) hawk_rtx_freevalbcstr(rtx, a1, sql);

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int bind_text (hawk_rtx_t* rtx, sql_list_t* sql_list, stmt_node_t* stmt_node, int index, hawk_val_t* val)
{
	hawk_bch_t* text = HAWK_NULL;
	hawk_oow_t len = 0;
	int rc;

	text = hawk_rtx_getvalbcstr(rtx, val, &len);
	if (!text) return -1;

	rc = sqlite3_bind_text(stmt_node->stmt, index, text, (int)len, SQLITE_TRANSIENT);
	hawk_rtx_freevalbcstr(rtx, val, text);

	if (rc != SQLITE_OK)
	{
		set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
		return -1;
	}

	return 0;
}

static int bind_blob (hawk_rtx_t* rtx, sql_list_t* sql_list, stmt_node_t* stmt_node, int index, hawk_val_t* val)
{
	hawk_bch_t* blob = HAWK_NULL;
	hawk_oow_t len = 0;
	int rc;

	blob = hawk_rtx_getvalbcstr(rtx, val, &len);
	if (!blob) return -1;

	rc = sqlite3_bind_blob(stmt_node->stmt, index, blob, (int)len, SQLITE_TRANSIENT);
	hawk_rtx_freevalbcstr(rtx, val, blob);

	if (rc != SQLITE_OK)
	{
		set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
		return -1;
	}

	return 0;
}

static int fnc_bind (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_val_t* a1;
	hawk_val_t* a2;
	hawk_int_t index;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		a1 = hawk_rtx_getarg(rtx, 1);
		if (hawk_rtx_valtoint(rtx, a1, &index) <= -1)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid bind index"));
			goto done;
		}

		a2 = hawk_rtx_getarg(rtx, 2);
		switch (HAWK_RTX_GETVALTYPE(rtx, a2))
		{
			case HAWK_VAL_INT:
			{
				hawk_int_t iv;
				if (hawk_rtx_valtoint(rtx, a2, &iv) <= -1) goto done;
				if (sqlite3_bind_int64(stmt_node->stmt, (int)index, (sqlite3_int64)iv) != SQLITE_OK)
				{
					set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
					goto done;
				}
				break;
			}

			case HAWK_VAL_FLT:
			{
				hawk_flt_t fv;
				if (hawk_rtx_valtoflt(rtx, a2, &fv) <= -1) goto done;
				if (sqlite3_bind_double(stmt_node->stmt, (int)index, (double)fv) != SQLITE_OK)
				{
					set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
					goto done;
				}
				break;
			}

			case HAWK_VAL_BCHR:
			case HAWK_VAL_MBS:
			case HAWK_VAL_BOB:
				if (bind_blob(rtx, sql_list, stmt_node, (int)index, a2) <= -1) goto done;
				break;

			case HAWK_VAL_NIL:
				if (sqlite3_bind_null(stmt_node->stmt, (int)index) != SQLITE_OK)
				{
					set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
					goto done;
				}
				break;

			default:
				if (bind_text(rtx, sql_list, stmt_node, (int)index, a2) <= -1) goto done;
				break;
		}

		ret = 0;
	}

done:
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_bind_int (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_int_t index;
	hawk_int_t val;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &index) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &val) <= -1)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid bind args"));
			goto done;
		}

		if (sqlite3_bind_int64(stmt_node->stmt, (int)index, (sqlite3_int64)val) != SQLITE_OK)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
			goto done;
		}

		ret = 0;
	}

done:
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_bind_flt (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_int_t index;
	hawk_flt_t val;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &index) <= -1 ||
		    hawk_rtx_valtoflt(rtx, hawk_rtx_getarg(rtx, 2), &val) <= -1)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid bind args"));
			goto done;
		}

		if (sqlite3_bind_double(stmt_node->stmt, (int)index, (double)val) != SQLITE_OK)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
			goto done;
		}

		ret = 0;
	}

done:
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_bind_str (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_int_t index;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &index) <= -1)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid bind args"));
			goto done;
		}

		if (bind_text(rtx, sql_list, stmt_node, (int)index, hawk_rtx_getarg(rtx, 2)) <= -1) goto done;
		ret = 0;
	}

done:
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_bind_blob (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_int_t index;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &index) <= -1)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid bind args"));
			goto done;
		}

		if (bind_blob(rtx, sql_list, stmt_node, (int)index, hawk_rtx_getarg(rtx, 2)) <= -1) goto done;
		ret = 0;
	}

done:
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_bind_null (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_int_t index;
	int ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &index) <= -1)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("invalid bind args"));
			goto done;
		}

		if (sqlite3_bind_null(stmt_node->stmt, (int)index) != SQLITE_OK)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
			goto done;
		}

		ret = 0;
	}

done:
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_escape_string (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	int ret = -1;
	int take_rtx_err = 0;

	hawk_val_t* a1, * retv;
	hawk_bch_t* qstr = HAWK_NULL;
	char* ebuf = HAWK_NULL;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		int n;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		a1 = hawk_rtx_getarg(rtx, 1);
		qstr = hawk_rtx_getvalbcstr(rtx, a1, HAWK_NULL);
		if (!qstr) { take_rtx_err = 1; goto done; }

		ebuf = sqlite3_mprintf("%q", qstr);
		if (HAWK_UNLIKELY(!ebuf))
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("unable to allocate escaped string"));
			goto done;
		}

		retv = hawk_rtx_makestrvalwithbcstr(rtx, (hawk_bch_t*)ebuf);
		if (HAWK_UNLIKELY(!retv))
		{
			take_rtx_err = 1;
			goto done;
		}

		hawk_rtx_refupval(rtx, retv);
		n = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), retv);
		hawk_rtx_refdownval(rtx, retv);
		if (n <= -1)
		{
			take_rtx_err = 1;
			goto done;
		}

		ret = 0;
	}

done:
	if (take_rtx_err) set_error_on_sql_list(rtx, sql_list, HAWK_NULL);
	if (ebuf) sqlite3_free(ebuf);
	if (qstr) hawk_rtx_freevalbcstr(rtx, a1, qstr);
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_prepare (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	sql_node_t* sql_node;
	hawk_val_t* a1;
	hawk_bch_t* sql = HAWK_NULL;
	int ret = -1;
	int take_rtx_err = 0;
	sqlite3_stmt* stmt = HAWK_NULL;
	int rc;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		stmt_node_t* stmt_node;

		ENSURE_CONNECT_EVER_ATTEMPTED(rtx, sql_list, sql_node);

		a1 = hawk_rtx_getarg(rtx, 1);
		sql = hawk_rtx_getvalbcstr(rtx, a1, HAWK_NULL);
		if (!sql)
		{
			take_rtx_err = 1;
			goto done;
		}

		rc = sqlite3_prepare_v2(sql_node->db, sql, -1, &stmt, HAWK_NULL);
		if (rc != SQLITE_OK)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(sql_node->db));
			goto done;
		}

		stmt_node = new_stmt_node(rtx, stmt_list, stmt, sql_node->db);
		if (!stmt_node)
		{
			sqlite3_finalize(stmt);
			take_rtx_err = 1;
			goto done;
		}

		ret = stmt_node->id;
	}

done:
	if (take_rtx_err) set_error_on_sql_list(rtx, sql_list, HAWK_NULL);
	if (sql) hawk_rtx_freevalbcstr(rtx, a1, sql);

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_finalize (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
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
		free_stmt_node(rtx, stmt_list, stmt_node);
		ret = 0;
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_reset (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int rc;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		rc = sqlite3_reset(stmt_node->stmt);
		if (rc != SQLITE_OK)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
		}
		else ret = 0;
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_column_count (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	hawk_int_t ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node) ret = stmt_node->col_count;

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

#define FETCH_ROW_ARRAY (1)
#define FETCH_ROW_MAP (2)

static int fnc_fetch_row (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	stmt_list_t* stmt_list;
	stmt_node_t* stmt_node;
	int ret = -1;
	int take_rtx_err = 0;
	hawk_val_t* row_map = HAWK_NULL;
	int rc;
	hawk_int_t mode = FETCH_ROW_MAP;

	sql_list = rtx_to_sql_list(rtx, fi);
	stmt_list = rtx_to_stmt_list(rtx, fi);
	stmt_node = get_stmt_list_node_with_arg(rtx, sql_list, stmt_list, hawk_rtx_getarg(rtx, 0));
	if (stmt_node)
	{
		hawk_oow_t nargs;
		hawk_val_t* row_val, * tmp;
		int i;

		nargs = hawk_rtx_getnargs(rtx);

		rc = sqlite3_step(stmt_node->stmt);
		if (rc == SQLITE_DONE)
		{
			ret = 0;
			goto done;
		}
		if (rc != SQLITE_ROW)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("%hs"), sqlite3_errmsg(stmt_node->db));
			goto done;
		}

		if (nargs >= 3)
		{
			if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &mode) <= -1)
			{
				set_error_on_sql_list(rtx, sql_list, HAWK_T("illegal mode"));
				goto done;
			}
		}

		row_map = (mode == FETCH_ROW_MAP? hawk_rtx_makemapval(rtx): hawk_rtx_makearrval(rtx, -1));
		if (HAWK_UNLIKELY(!row_map))
		{
			take_rtx_err = 1;
			goto done;
		}

		hawk_rtx_refupval(rtx, row_map);

		for (i = 0; i < stmt_node->col_count; )
		{
			int col_type;

			col_type = sqlite3_column_type(stmt_node->stmt, i);
			switch (col_type)
			{
				case SQLITE_INTEGER:
					row_val = hawk_rtx_makeintval(rtx, (hawk_int_t)sqlite3_column_int64(stmt_node->stmt, i));
					break;
				case SQLITE_FLOAT:
					row_val = hawk_rtx_makefltval(rtx, (hawk_flt_t)sqlite3_column_double(stmt_node->stmt, i));
					break;
				case SQLITE_BLOB:
				{
					const void* blob = sqlite3_column_blob(stmt_node->stmt, i);
					int blen = sqlite3_column_bytes(stmt_node->stmt, i);
					row_val = hawk_rtx_makembsvalwithbchars(rtx, (const hawk_bch_t*)blob, (hawk_oow_t)blen);
					break;
				}
				case SQLITE_TEXT:
				{
					const hawk_bch_t* text = (const hawk_bch_t*)sqlite3_column_text(stmt_node->stmt, i);
					int tlen = sqlite3_column_bytes(stmt_node->stmt, i);
					row_val = hawk_rtx_makestrvalwithbchars(rtx, text, (hawk_oow_t)tlen);
					break;
				}
				case SQLITE_NULL:
				default:
					row_val = hawk_rtx_makenilval(rtx);
					break;
			}

			if (HAWK_UNLIKELY(!row_val))
			{
				take_rtx_err = 1;
				goto done;
			}

			++i;

			if (mode == FETCH_ROW_MAP)
			{
				hawk_ooch_t key_buf[HAWK_SIZEOF(hawk_int_t) * 8 + 2];
				hawk_oow_t key_len;

				key_len = hawk_int_to_oocstr(i, 10, HAWK_NULL, key_buf, HAWK_COUNTOF(key_buf));
				HAWK_ASSERT(key_len != (hawk_oow_t)-1);

				hawk_rtx_refupval(rtx, row_val);
				tmp = hawk_rtx_setmapvalfld(rtx, row_map, key_buf, key_len, row_val);
				hawk_rtx_refdownval(rtx, row_val);
			}
			else
			{
				hawk_rtx_refupval(rtx, row_val);
				tmp = hawk_rtx_setarrvalfld(rtx, row_map, i, row_val);
				hawk_rtx_refdownval(rtx, row_val);
			}
			if (HAWK_UNLIKELY(!tmp))
			{
				take_rtx_err = 1;
				goto done;
			}
		}

		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), row_map) <= -1)
		{
			take_rtx_err = 1;
			goto done;
		}

		hawk_rtx_refdownval(rtx, row_map);
		row_map = HAWK_NULL;
		ret = 1;
	}

done:
	if (take_rtx_err) set_error_on_sql_list(rtx, sql_list, HAWK_NULL);
	if (row_map) hawk_rtx_refdownval(rtx, row_map);
	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_changes (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	hawk_int_t ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		if (!sql_node->connect_ever_attempted || !sql_node->db)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("not connected"));
		}
		else
		{
			ret = sqlite3_changes(sql_node->db);
		}
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_last_insert_rowid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sql_list_t* sql_list;
	sql_node_t* sql_node;
	hawk_int_t ret = -1;

	sql_list = rtx_to_sql_list(rtx, fi);
	sql_node = get_sql_list_node_with_arg(rtx, sql_list, hawk_rtx_getarg(rtx, 0));
	if (sql_node)
	{
		if (!sql_node->connect_ever_attempted || !sql_node->db)
		{
			set_error_on_sql_list(rtx, sql_list, HAWK_T("not connected"));
		}
		else
		{
			ret = (hawk_int_t)sqlite3_last_insert_rowid(sql_node->db);
		}
	}

	hawk_rtx_setretval(rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

/* ----------------------------------------------------------------------- */

#define A_MAX HAWK_TYPE_MAX(int)

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("bind"),             { { 3, 3, HAWK_NULL },     fnc_bind,             0 } },
	{ HAWK_T("bind_blob"),        { { 3, 3, HAWK_NULL },     fnc_bind_blob,        0 } },
	{ HAWK_T("bind_flt"),         { { 3, 3, HAWK_NULL },     fnc_bind_flt,         0 } },
	{ HAWK_T("bind_int"),         { { 3, 3, HAWK_NULL },     fnc_bind_int,         0 } },
	{ HAWK_T("bind_null"),        { { 2, 2, HAWK_NULL },     fnc_bind_null,        0 } },
	{ HAWK_T("bind_str"),         { { 3, 3, HAWK_NULL },     fnc_bind_str,         0 } },
	{ HAWK_T("changes"),          { { 1, 1, HAWK_NULL },     fnc_changes,          0 } },
	{ HAWK_T("close"),            { { 1, 1, HAWK_NULL },     fnc_close,            0 } },
	{ HAWK_T("column_count"),     { { 1, 1, HAWK_NULL },     fnc_column_count,     0 } },
	{ HAWK_T("connect"),          { { 2, 4, HAWK_NULL },     fnc_connect,          0 } },
	{ HAWK_T("errmsg"),           { { 0, 0, HAWK_NULL },     fnc_errmsg,           0 } },
	{ HAWK_T("escape_string"),    { { 3, 3, HAWK_T("vvr") }, fnc_escape_string,    0 } },
	{ HAWK_T("exec"),             { { 2, 2, HAWK_NULL },     fnc_exec,             0 } },
	{ HAWK_T("fetch_row"),        { { 2, 3, HAWK_T("vrv") }, fnc_fetch_row,        0 } },
	{ HAWK_T("finalize"),         { { 1, 1, HAWK_NULL },     fnc_finalize,         0 } },
	{ HAWK_T("last_insert_rowid"),{ { 1, 1, HAWK_NULL },     fnc_last_insert_rowid,0 } },
	{ HAWK_T("open"),             { { 0, 0, HAWK_NULL },     fnc_open,             0 } },
	{ HAWK_T("prepare"),          { { 2, 2, HAWK_NULL },     fnc_prepare,          0 } },
	{ HAWK_T("reset"),            { { 1, 1, HAWK_NULL },     fnc_reset,            0 } }
};

static hawk_mod_int_tab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("CONNECT_CREATE"),       { SQLITE_OPEN_CREATE       } },
	{ HAWK_T("CONNECT_FULLMUTEX"),    { SQLITE_OPEN_FULLMUTEX    } },
	{ HAWK_T("CONNECT_NOMUTEX"),      { SQLITE_OPEN_NOMUTEX      } },
	{ HAWK_T("CONNECT_PRIVATECACHE"), { SQLITE_OPEN_PRIVATECACHE } },
	{ HAWK_T("CONNECT_READONLY"),     { SQLITE_OPEN_READONLY     } },
	{ HAWK_T("CONNECT_READWRITE"),    { SQLITE_OPEN_READWRITE    } },
	{ HAWK_T("CONNECT_SHAREDCACHE"),  { SQLITE_OPEN_SHAREDCACHE  } },
	{ HAWK_T("CONNECT_URI"),          { SQLITE_OPEN_URI          } },
	{ HAWK_T("FETCH_ROW_ARRAY"),      { FETCH_ROW_ARRAY          } },
	{ HAWK_T("FETCH_ROW_MAP"),        { FETCH_ROW_MAP            } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	if (hawk_findmodsymfnc_noseterr(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym) >= 0) return 0;
	return hawk_findmodsymint(hawk, inttab, HAWK_COUNTOF(inttab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	rtx_data_t data, * datap;
	hawk_rbt_pair_t* pair;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_MEMSET(&data, 0, HAWK_SIZEOF(data));
	pair = hawk_rbt_insert(rbt, &rtx, HAWK_SIZEOF(rtx), &data, HAWK_SIZEOF(data));
	if (HAWK_UNLIKELY(!pair)) return -1;

	datap = (rtx_data_t*)HAWK_RBT_VPTR(pair);
	__init_sql_list(rtx, &datap->sql_list);
	__init_stmt_list(rtx, &datap->stmt_list);

	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	hawk_rbt_pair_t* pair;

	rbt = (hawk_rbt_t*)mod->ctx;

	pair = hawk_rbt_search(rbt, &rtx, HAWK_SIZEOF(rtx));
	if (pair)
	{
		rtx_data_t* data;

		data = (rtx_data_t*)HAWK_RBT_VPTR(pair);

		__fini_stmt_list(rtx, &data->stmt_list);
		__fini_sql_list(rtx, &data->sql_list);

		hawk_rbt_delete(rbt, &rtx, HAWK_SIZEOF(rtx));
	}
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_ASSERT(HAWK_RBT_SIZE(rbt) == 0);
	hawk_rbt_close(rbt);
}

int hawk_mod_sqlite (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (HAWK_UNLIKELY(!rbt)) return -1;

	hawk_rbt_setstyle(rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}
