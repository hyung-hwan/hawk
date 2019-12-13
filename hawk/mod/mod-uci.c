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

#include "mod-uci.h"
#include <qse/cmn/str.h>
#include <qse/cmn/rbt.h>
#include <qse/cmn/mbwc.h>
#include <qse/cmn/fmt.h>
#include "../cmn/mem-prv.h"

#if defined(HAVE_UCI_H)
#	include <uci.h>
#else
#	error this module needs uci.h
#endif

typedef struct uctx_list_t uctx_list_t;
typedef struct uctx_node_t uctx_node_t;

struct uctx_node_t
{
	int id;
	struct uci_context* ctx;	
	uctx_node_t* prev;
	uctx_node_t* next;
};

struct uctx_list_t
{
	uctx_node_t* head;
	uctx_node_t* tail;
	uctx_node_t* free;
	
	/* mapping table to map 'id' to 'node' */
	struct
	{
		uctx_node_t** tab;
		int capa;
		int high;
	} map;

	int errnum;
};

static uctx_node_t* new_uctx_node (qse_awk_rtx_t* rtx, uctx_list_t* list)
{
	/* create a new context node and append it to the list tail */
	uctx_node_t* node;

	node = QSE_NULL;

	if (list->free) node = list->free;
	else
	{
		node = qse_awk_rtx_callocmem (rtx, QSE_SIZEOF(*node));
		if (!node) goto oops;
	}

	node->ctx = uci_alloc_context();
	if (!node->ctx) goto oops;

	if (node == list->free) list->free = node->next;
	else
	{
		if (list->map.high <= list->map.capa)
		{
			int newcapa;
			uctx_node_t** tmp;

			newcapa = list->map.capa + 64;
			if (newcapa < list->map.capa) goto oops; /* overflow */

			tmp = (uctx_node_t**) qse_awk_rtx_reallocmem (
				rtx, list->map.tab, QSE_SIZEOF(*tmp) * newcapa);
			if (!tmp) goto oops;

			QSE_MEMSET (&tmp[list->map.capa], 0, 
				QSE_SIZEOF(*tmp) * (newcapa - list->map.capa));

			list->map.tab = tmp;
			list->map.capa = newcapa;
		}

		node->id = list->map.high;
		QSE_ASSERT (list->map.tab[node->id] == QSE_NULL);
		list->map.tab[node->id] = node;
		list->map.high++;
	}

	/* append it to the tail */
	node->next = QSE_NULL;
	node->prev = list->tail;
	if (list->tail) list->tail->next = node;
	else list->head = node;
	list->tail = node;

	return node;

oops:
	if (node) qse_awk_rtx_freemem (rtx, node);
	qse_awk_rtx_seterrnum (rtx, QSE_AWK_ENOMEM, QSE_NULL);
	return QSE_NULL;
}

static void free_uctx_node (qse_awk_rtx_t* rtx, uctx_list_t* list, uctx_node_t* node)
{
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	if (list->head == node) list->head = node->next;
	if (list->tail == node) list->tail = node->prev;

	list->map.tab[node->id] = QSE_NULL;

	if (node->ctx) 
	{
		uci_free_context (node->ctx);
	}

	if (list->map.high == node->id + 1)
	{
		/* destroy the actual node if the node to be freed has the
		 * highest id */
		QSE_MMGR_FREE (qse_awk_rtx_getmmgr(rtx), node);
		list->map.high--;
	}
	else
	{
		/* otherwise, chain the node to the free list */
		node->ctx = QSE_NULL;
		node->next = list->free;
		list->free = node;
	}

	/* however, i destroy the whole free list when all the nodes are
	 * chanined to the free list */
	if (list->head == QSE_NULL) 
	{
		qse_mmgr_t* mmgr;
		uctx_node_t* curnode;

		mmgr = qse_awk_rtx_getmmgr(rtx);

		while (list->free)
		{
			curnode = list->free;
			list->free = list->free->next;
			QSE_ASSERT (curnode->ctx == QSE_NULL);
			QSE_MMGR_FREE (mmgr, curnode);
		}

		QSE_MMGR_FREE (mmgr, list->map.tab);
		list->map.high = 0;
		list->map.capa = 0;
		list->map.tab = QSE_NULL;
	}
}

/* ------------------------------------------------------------------------ */

static int close_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		free_uctx_node (rtx, list, list->map.tab[id]);
		x = UCI_OK;
	}
	
	return -x;
}

static int load_byid (
	qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		x = uci_load (list->map.tab[id]->ctx, path, QSE_NULL);
	}
	
	return -x;
}

static int unload_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		x = uci_unload (list->map.tab[id]->ctx, QSE_NULL);
		return 0;
	}
	
	return -x;
}

static int save_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{

			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE) && (ptr.last->type == UCI_TYPE_PACKAGE))
				x = uci_save (list->map.tab[id]->ctx, ptr.p);
			else
				x = UCI_ERR_NOTFOUND;
		}
	}
	
	return -x;
}

static int commit_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE) && (ptr.last->type == UCI_TYPE_PACKAGE))
				x = uci_commit (list->map.tab[id]->ctx, &ptr.p, 0);
			else
				x = UCI_ERR_NOTFOUND;
		}
	}
	
	return -x;
}

static int revert_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE))
				x = uci_revert (list->map.tab[id]->ctx, &ptr);
			else
				x = UCI_ERR_NOTFOUND;
		}
	}
	
	return -x;
}

static int delete_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE))
				x = uci_delete (list->map.tab[id]->ctx, &ptr);
			else
				x = UCI_ERR_NOTFOUND;
		}
	}
	
	return -x;
}

static int rename_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			if (ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE))
				x = uci_rename (list->map.tab[id]->ctx, &ptr);
			else
				x = UCI_ERR_NOTFOUND;
		}
	}
	
	return -x;
}

static int set_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			x = ptr.value? uci_set (list->map.tab[id]->ctx, &ptr): UCI_ERR_INVAL;
		}
	}
	
	return -x;
}

static int addsection_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item, qse_mchar_t* type)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			/* add an unnamed section. use set to add a named section */
			struct uci_section* s = QSE_NULL;
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE) && (ptr.last->type == UCI_TYPE_PACKAGE))
				x = uci_add_section (list->map.tab[id]->ctx, ptr.p, type, &s);
			else x = UCI_ERR_INVAL;
		}
	}
	
	return -x;
}

static int addlist_byid (qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, item, 1);

		if (x == UCI_OK) 
		{
			x = ptr.value? uci_add_list (list->map.tab[id]->ctx, &ptr): UCI_ERR_INVAL;
		}
	}
	
	return -x;
}

static int setconfdir_byid (
	qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		x = uci_set_confdir (list->map.tab[id]->ctx, path);
	}
	
	return -x;
}

static int setsavedir_byid (
	qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		x = uci_set_savedir (list->map.tab[id]->ctx, path);
	}
	
	return -x;
}

static int adddeltapath_byid (
	qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id, qse_mchar_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		x = uci_add_delta_path (list->map.tab[id]->ctx, path);
	}
	
	return -x;
}

static int getsection_byid (
	qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id,
	qse_mchar_t* tuple, qse_awk_val_ref_t* ref)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, tuple, 1);
		if (x == UCI_OK)
		{
			/* ptr.value is not null if the tuple specified contains
			   assignment like network.wan.ifname=eth0 */
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE))
			{
				struct uci_element* e;

				e = ptr.last;	
				if (e->type == UCI_TYPE_SECTION)
				{
					qse_awk_val_map_data_t md[4];
					qse_awk_int_t lv;
					qse_awk_val_t* tmp;

					QSE_MEMSET (md, 0, QSE_SIZEOF(md));
		
					md[0].key.ptr = QSE_T("type");
					md[0].key.len = 4;
					md[0].type = QSE_AWK_VAL_MAP_DATA_MBS;
					md[0].vptr = ptr.s->type;

					md[1].key.ptr = QSE_T("name");
					md[1].key.len = 4;
					md[1].type = QSE_AWK_VAL_MAP_DATA_MBS;
					md[1].vptr = ptr.s->e.name; /* e->name == ptr.s->e.name */

					md[2].key.ptr = QSE_T("anon");
					md[2].key.len = 4;
					md[2].vptr = QSE_AWK_VAL_MAP_DATA_INT;
					lv = ptr.s->anonymous;
					md[2].vptr = &lv;

					tmp = qse_awk_rtx_makemapvalwithdata (rtx, md);
					if (tmp) 
					{
						int n;
						qse_awk_rtx_refupval (rtx, tmp);
						n = qse_awk_rtx_setrefval (rtx, ref, tmp);
						qse_awk_rtx_refdownval (rtx, tmp);
						if (n <= -1) return -9999;
					}
					else x = UCI_ERR_MEM;
				}
				else x = UCI_ERR_NOTFOUND;
				
			}
			else x = UCI_ERR_NOTFOUND;
		}
	}

	return -x;
}

static int getoption_byid (
	qse_awk_rtx_t* rtx, uctx_list_t* list, qse_awk_int_t id,
	qse_mchar_t* tuple, qse_awk_val_ref_t* ref)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id]) 
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr (list->map.tab[id]->ctx, &ptr, tuple, 1);
		if (x == UCI_OK)
		{
			/* ptr.value is not null if the tuple specified contains
			   assignment like network.wan.ifname=eth0 */
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE))
			{
				struct uci_element* e;

				e = ptr.last;	
				if (e->type == UCI_TYPE_OPTION)
				{
					struct uci_option* uo = ptr.o;

					if (uo->type == UCI_TYPE_STRING)
					{
						qse_awk_val_t* map;
						qse_awk_val_map_data_t md[3];

						QSE_MEMSET (md, 0, QSE_SIZEOF(md));

						md[0].key.ptr = QSE_T("type");
						md[0].key.len = 4;
						md[0].type = QSE_AWK_VAL_MAP_DATA_STR;
						md[0].vptr = QSE_T("string");

						md[1].key.ptr = QSE_T("value");
						md[1].key.len = 5;
						md[1].type = QSE_AWK_VAL_MAP_DATA_MBS;
						md[1].vptr = uo->v.string;

						map = qse_awk_rtx_makemapvalwithdata (rtx, md);
						if (map) 
						{
							int n;
							qse_awk_rtx_refupval (rtx, map);
							n = qse_awk_rtx_setrefval (rtx, ref, map);
							qse_awk_rtx_refdownval (rtx, map);
							if (n <= -1)
							{
								map = QSE_NULL;
								return -9999;
							}
						}
						else x = UCI_ERR_MEM;
					}
					else if (uo->type == UCI_TYPE_LIST)
					{
						qse_awk_val_t* map, * fld;
						qse_awk_val_map_data_t md[3];
						struct uci_element* tmp;
						qse_awk_int_t count;

						count = 0;
						uci_foreach_element(&uo->v.list, tmp) { count++; }

						QSE_MEMSET (md, 0, QSE_SIZEOF(md));

						md[0].key.ptr = QSE_T("type");
						md[0].key.len = 4;
						md[0].type = QSE_AWK_VAL_MAP_DATA_STR;
						md[0].vptr = QSE_T("list");

						md[1].key.ptr = QSE_T("count");
						md[1].key.len = 5;
						md[1].type = QSE_AWK_VAL_MAP_DATA_INT;
						md[1].vptr = &count;

						map = qse_awk_rtx_makemapvalwithdata (rtx, md);
						if (map)	
						{
							count = 1;
							uci_foreach_element(&uo->v.list, tmp) 
							{
								const qse_cstr_t* subsep;
								qse_cstr_t k[4];
								qse_char_t idxbuf[64];
								qse_char_t* kp;
								qse_size_t kl;

								fld = qse_awk_rtx_makestrvalwithmbs (rtx, tmp->name);
								if (!fld)
								{
									qse_awk_rtx_refupval (rtx, map);
									qse_awk_rtx_refdownval (rtx, map);
									map = QSE_NULL;
									x = UCI_ERR_MEM;
									break;
								}

								subsep = qse_awk_rtx_getsubsep (rtx);

								k[0].ptr = QSE_T("value");
								k[0].len = 5;
								k[1].ptr = subsep->ptr;
								k[1].len = subsep->len;
								k[2].ptr = idxbuf;
								k[2].len = qse_fmtuintmax (idxbuf, QSE_COUNTOF(idxbuf), count, 10, -1, QSE_T('\0'), QSE_NULL);
								k[3].ptr = QSE_NULL;
								k[3].len = 0;
							
								kp = qse_wcstradup (k, &kl, qse_awk_rtx_getmmgr(rtx));
								if (kp == QSE_NULL || qse_awk_rtx_setmapvalfld (rtx, map, kp, kl, fld) == QSE_NULL)
								{
									if (kp) QSE_MMGR_FREE (qse_awk_rtx_getmmgr(rtx), kp);
									qse_awk_rtx_refupval (rtx, fld);
									qse_awk_rtx_refdownval (rtx, fld);
									qse_awk_rtx_refupval (rtx, map);
									qse_awk_rtx_refdownval (rtx, map);
									map = QSE_NULL;
									x = UCI_ERR_MEM;
									break;
								}

								QSE_MMGR_FREE (qse_awk_rtx_getmmgr(rtx), kp);
								count++;
							}
							
							if (map) 
							{
								if (qse_awk_rtx_setrefval (rtx, ref, map) <= -1)
								{
									qse_awk_rtx_refupval (rtx, map);
									qse_awk_rtx_refdownval (rtx, map);
									map = QSE_NULL;
									return -9999;
								}
							}
						}
						else x = UCI_ERR_MEM;
					} 
					else x = UCI_ERR_INVAL; /* uo->type */
				}
				else x = UCI_ERR_NOTFOUND; /* e->type */
			}
			else x = UCI_ERR_NOTFOUND;
		}
	}

	return -x;
}
/* ------------------------------------------------------------------------ */

static QSE_INLINE uctx_list_t* rtx_to_list (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	qse_rbt_pair_t* pair;
	pair = qse_rbt_search ((qse_rbt_t*)fi->mod->ctx, &rtx, QSE_SIZEOF(rtx));
	QSE_ASSERT (pair != QSE_NULL);
	return (uctx_list_t*)QSE_RBT_VPTR(pair);
}

static int fnc_uci_errno (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;

	list = rtx_to_list (rtx, fi);

	retv = qse_awk_rtx_makeintval (rtx, list->errnum);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static qse_char_t* errmsg[] =
{
	QSE_T("no error"),
	QSE_T("out of memory"),
	QSE_T("invalid data"),
	QSE_T("not found"),
	QSE_T("I/O error"),
	QSE_T("parse error"),
	QSE_T("duplicate data"),
	QSE_T("unknown error")
};

static int fnc_uci_errstr (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t errnum;

	list = rtx_to_list (rtx, fi);

	if (qse_awk_rtx_getnargs (rtx) <= 0 ||
	    qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &errnum) <= -1)
	{
		errnum = list->errnum;
	}

	if (errnum < 0 || errnum >= QSE_COUNTOF(errmsg)) errnum = QSE_COUNTOF(errmsg) - 1;

	retv = qse_awk_rtx_makestrvalwithstr (rtx, errmsg[errnum]);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_open (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	uctx_node_t* node;
	qse_awk_int_t ret;
	qse_awk_val_t* retv;

	list = rtx_to_list (rtx, fi);
	node = new_uctx_node (rtx, list);
	ret = node? node->id: -UCI_ERR_MEM;
	
	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_close (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else ret = close_byid (rtx, list, id);

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_load  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = load_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_unload  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else ret = unload_byid (rtx, list, id);

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_save  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = save_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_commit (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = commit_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_revert (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = revert_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_delete (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = delete_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_rename (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = rename_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_set (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = set_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_addsection (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item, * type;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		type = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 2), QSE_NULL);
		if (item && type)
		{
			ret = addsection_byid (rtx, list, id, item, type);
		}
		else ret = -UCI_ERR_MEM;

		if (type) qse_awk_rtx_freemem (rtx, type);
		if (item) qse_awk_rtx_freemem (rtx, item);
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_addlist (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = addlist_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_setconfdir  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = setconfdir_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}


static int fnc_uci_setsavedir  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = setsavedir_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_adddeltapath  (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_val_t* retv;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = adddeltapath_byid (rtx, list, id, item);
			qse_awk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = qse_awk_rtx_makeintval (rtx, ret);
	if (retv == QSE_NULL) return -1;

	qse_awk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_getoption (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint (rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg(rtx, 1), QSE_NULL);
		if (item)
		{
			ret = getoption_byid (rtx, list, id, item, qse_awk_rtx_getarg (rtx, 2));
			qse_awk_rtx_freemem (rtx, item);
			if (ret == -9999) return -1;
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}
	else ret = 0;

	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval (rtx, ret));
	return 0;
}

static int fnc_uci_getsection (qse_awk_rtx_t* rtx, const qse_awk_fnc_info_t* fi)
{
	uctx_list_t* list;
	qse_awk_int_t id;
	int ret;
	
	list = rtx_to_list (rtx, fi);

	ret = qse_awk_rtx_valtoint(rtx, qse_awk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		qse_mchar_t* item;

		item = qse_awk_rtx_valtombsdup(rtx, qse_awk_rtx_getarg (rtx, 1), QSE_NULL);
		if (item)
		{
			ret = getsection_byid(rtx, list, id, item, qse_awk_rtx_getarg (rtx, 2));
			qse_awk_rtx_freemem (rtx, item);
			if (ret == -9999) return -1;
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1) 
	{
		list->errnum = -ret;
		ret = -1;
	}
	else ret = 0;

	qse_awk_rtx_setretval (rtx, qse_awk_rtx_makeintval (rtx, ret));
	return 0;
}

/* ------------------------------------------------------------------------ */

typedef struct fnctab_t fnctab_t;
struct fnctab_t
{
	const qse_char_t* name;
	qse_awk_mod_sym_fnc_t info;
};

static fnctab_t fnctab[] =
{
	{ QSE_T("adddeltapath"), { { 2, 2, QSE_NULL    }, fnc_uci_adddeltapath, 0 } },
	{ QSE_T("addlist"),      { { 2, 2, QSE_NULL    }, fnc_uci_addlist,      0 } },
	{ QSE_T("addsection"),   { { 3, 3, QSE_NULL    }, fnc_uci_addsection,   0 } },
	{ QSE_T("close"),        { { 1, 1, QSE_NULL    }, fnc_uci_close,        0 } },
	{ QSE_T("commit"),       { { 2, 2, QSE_NULL    }, fnc_uci_commit,       0 } },
	{ QSE_T("delete"),       { { 2, 2, QSE_NULL    }, fnc_uci_delete,       0 } },
	{ QSE_T("errno"),        { { 0, 0, QSE_NULL    }, fnc_uci_errno,        0 } },
	{ QSE_T("errstr"),       { { 0, 1, QSE_NULL    }, fnc_uci_errstr,       0 } },
	{ QSE_T("getoption"),    { { 3, 3, QSE_T("vvr")}, fnc_uci_getoption,    0 } },
	{ QSE_T("getsection"),   { { 3, 3, QSE_T("vvr")}, fnc_uci_getsection,   0 } },
	{ QSE_T("load"),         { { 2, 2, QSE_NULL    }, fnc_uci_load,         0 } },
	{ QSE_T("open"),         { { 0, 0, QSE_NULL    }, fnc_uci_open,         0 } },
	{ QSE_T("rename"),       { { 2, 2, QSE_NULL    }, fnc_uci_rename,       0 } },
	{ QSE_T("revert"),       { { 2, 2, QSE_NULL    }, fnc_uci_revert,       0 } },
	{ QSE_T("save"),         { { 2, 2, QSE_NULL    }, fnc_uci_save,         0 } },
	{ QSE_T("set"),          { { 2, 2, QSE_NULL    }, fnc_uci_set,          0 } }, 
	{ QSE_T("setconfdir"),   { { 2, 2, QSE_NULL    }, fnc_uci_setconfdir,   0 } }, 
	{ QSE_T("setsavedir"),   { { 2, 2, QSE_NULL    }, fnc_uci_setsavedir,   0 } }, 
	{ QSE_T("unload"),       { { 1, 1, QSE_NULL    }, fnc_uci_unload,       0 } }
};

/* ------------------------------------------------------------------------ */

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

	ea.ptr = name;
	ea.len = qse_strlen(name);
	qse_awk_seterror (awk, QSE_AWK_ENOENT, &ea, QSE_NULL);
	return -1;
}

static int init (qse_awk_mod_t* mod, qse_awk_rtx_t* rtx)
{
	qse_rbt_t* rbt;
	uctx_list_t list;

	rbt = (qse_rbt_t*)mod->ctx;

	QSE_MEMSET (&list, 0, QSE_SIZEOF(list));
	if (qse_rbt_insert (rbt, &rtx, QSE_SIZEOF(rtx), &list, QSE_SIZEOF(list)) == QSE_NULL) 
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
	pair = qse_rbt_search  (rbt, &rtx, QSE_SIZEOF(rtx));
	if (pair)
	{
		uctx_list_t* list;
		uctx_node_t* node, * next;

		list = QSE_RBT_VPTR(pair);
		node = list->head;
		while (node)
		{
			next = node->next;
			free_uctx_node (rtx, list, node);
			node = next;
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
}

int qse_awk_mod_uci (qse_awk_mod_t* mod, qse_awk_t* awk)
{
	qse_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = qse_rbt_open (qse_awk_getmmgr(awk), 0, 1, 1);
	if (rbt == QSE_NULL) 
	{
		qse_awk_seterrnum (awk, QSE_AWK_ENOMEM, QSE_NULL);
		return -1;
	}
	qse_rbt_setstyle (rbt, qse_getrbtstyle(QSE_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}

