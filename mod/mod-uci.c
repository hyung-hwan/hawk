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

#include "mod-uci.h"
/*
#include <hawk-str.h>
#include <hawk-rbt.h>
#include <hawk-chr.h>
#include <hawk-fmt.h>
*/
#include "../lib/hawk-prv.h"

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

static uctx_node_t* new_uctx_node (hawk_rtx_t* rtx, uctx_list_t* list)
{
	/* create a new context node and append it to the list tail */
	uctx_node_t* node;

	node = HAWK_NULL;

	if (list->free) node = list->free;
	else
	{
		node = hawk_rtx_callocmem (rtx, HAWK_SIZEOF(*node));
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

			tmp = (uctx_node_t**) hawk_rtx_reallocmem (
				rtx, list->map.tab, HAWK_SIZEOF(*tmp) * newcapa);
			if (!tmp) goto oops;

			HAWK_MEMSET (&tmp[list->map.capa], 0,
				HAWK_SIZEOF(*tmp) * (newcapa - list->map.capa));

			list->map.tab = tmp;
			list->map.capa = newcapa;
		}

		node->id = list->map.high;
		HAWK_ASSERT (list->map.tab[node->id] == HAWK_NULL);
		list->map.tab[node->id] = node;
		list->map.high++;
	}

	/* append it to the tail */
	node->next = HAWK_NULL;
	node->prev = list->tail;
	if (list->tail) list->tail->next = node;
	else list->head = node;
	list->tail = node;

	return node;

oops:
	if (node) hawk_rtx_freemem (rtx, node);
	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOMEM);
	return HAWK_NULL;
}

static void free_uctx_node (hawk_rtx_t* rtx, uctx_list_t* list, uctx_node_t* node)
{
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	if (list->head == node) list->head = node->next;
	if (list->tail == node) list->tail = node->prev;

	list->map.tab[node->id] = HAWK_NULL;

	if (node->ctx)
	{
		uci_free_context (node->ctx);
	}

	if (list->map.high == node->id + 1)
	{
		/* destroy the actual node if the node to be freed has the
		 * highest id */
		hawk_rtx_freemem(rtx, node);
		list->map.high--;
	}
	else
	{
		/* otherwise, chain the node to the free list */
		node->ctx = HAWK_NULL;
		node->next = list->free;
		list->free = node;
	}

	/* however, i destroy the whole free list when all the nodes are
	 * chanined to the free list */
	if (list->head == HAWK_NULL)
	{
		uctx_node_t* curnode;

		while (list->free)
		{
			curnode = list->free;
			list->free = list->free->next;
			HAWK_ASSERT (curnode->ctx == HAWK_NULL);
			hawk_rtx_freemem (rtx, curnode);
		}

		hawk_rtx_freemem (rtx, list->map.tab);
		list->map.high = 0;
		list->map.capa = 0;
		list->map.tab = HAWK_NULL;
	}
}

/* ------------------------------------------------------------------------ */

static int close_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		free_uctx_node (rtx, list, list->map.tab[id]);
		x = UCI_OK;
	}

	return -x;
}

static int load_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		x = uci_load(list->map.tab[id]->ctx, path, HAWK_NULL);
	}

	return -x;
}

static int unload_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		x = uci_unload(list->map.tab[id]->ctx, HAWK_NULL);
		return 0;
	}

	return -x;
}

static int save_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
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

static int commit_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
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

static int revert_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
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

static int delete_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
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

static int rename_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
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

static int set_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
		if (x == UCI_OK)
		{
			x = ptr.value? uci_set (list->map.tab[id]->ctx, &ptr): UCI_ERR_INVAL;
		}
	}

	return -x;
}

static int addsection_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item, hawk_bch_t* type)
{
	int x = UCI_ERR_INVAL;

/* TODO: this looks like a wrong implementation */
	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
		if (x == UCI_OK)
		{
			/* add an unnamed section. use set to add a named section */
			struct uci_section* s = HAWK_NULL;
			if (!ptr.value && (ptr.flags & UCI_LOOKUP_COMPLETE) && (ptr.last->type == UCI_TYPE_PACKAGE))
			{
				x = uci_add_section(list->map.tab[id]->ctx, ptr.p, type, &s);
			}
			else x = UCI_ERR_INVAL;
		}
	}

	return -x;
}

static int addlist_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* item)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, item, 1);
		if (x == UCI_OK)
		{
			x = ptr.value? uci_add_list (list->map.tab[id]->ctx, &ptr): UCI_ERR_INVAL;
		}
	}

	return -x;
}

static int setconfdir_byid (
	hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		x = uci_set_confdir (list->map.tab[id]->ctx, path);
	}

	return -x;
}

static int setsavedir_byid (
	hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		x = uci_set_savedir (list->map.tab[id]->ctx, path);
	}

	return -x;
}

static int adddeltapath_byid (
	hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* path)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		x = uci_add_delta_path (list->map.tab[id]->ctx, path);
	}

	return -x;
}

static int getsection_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* tuple, hawk_val_ref_t* ref)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, tuple, 1);
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
					hawk_val_map_data_t md[3];
					hawk_int_t lv;
					hawk_val_t* tmp;

					HAWK_MEMSET (md, 0, HAWK_SIZEOF(md));

					md[0].key.ptr = HAWK_T("type");
					md[0].key.len = 4;
					md[0].type = HAWK_VAL_MAP_DATA_BCSTR;
					md[0].vptr = ptr.s->type;

					md[1].key.ptr = HAWK_T("name");
					md[1].key.len = 4;
					md[1].type = HAWK_VAL_MAP_DATA_BCSTR;
					md[1].vptr = ptr.s->e.name; /* e->name == ptr.s->e.name */

					md[2].key.ptr = HAWK_T("anon");
					md[2].key.len = 4;
					md[2].vptr = HAWK_VAL_MAP_DATA_INT;
					lv = ptr.s->anonymous;
					md[2].vptr = &lv;

					tmp = hawk_rtx_makemapvalwithdata(rtx, md, 3);
					if (tmp)
					{
						int n;
						hawk_rtx_refupval (rtx, tmp);
						n = hawk_rtx_setrefval (rtx, ref, tmp);
						hawk_rtx_refdownval (rtx, tmp);
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

static int getoption_byid (hawk_rtx_t* rtx, uctx_list_t* list, hawk_int_t id, hawk_bch_t* tuple, hawk_val_ref_t* ref)
{
	int x = UCI_ERR_INVAL;

	if (id >= 0 && id < list->map.high && list->map.tab[id])
	{
		struct uci_ptr ptr;

		x = uci_lookup_ptr(list->map.tab[id]->ctx, &ptr, tuple, 1);
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
						hawk_val_t* map;
						hawk_val_map_data_t md[2];

						HAWK_MEMSET (md, 0, HAWK_SIZEOF(md));

						md[0].key.ptr = HAWK_T("type");
						md[0].key.len = 4;
						md[0].type = HAWK_VAL_MAP_DATA_OOCSTR;
						md[0].vptr = HAWK_T("string");

						md[1].key.ptr = HAWK_T("value");
						md[1].key.len = 5;
						md[1].type = HAWK_VAL_MAP_DATA_BCSTR;
						md[1].vptr = uo->v.string;

						map = hawk_rtx_makemapvalwithdata(rtx, md, 2);
						if (map)
						{
							int n;
							hawk_rtx_refupval (rtx, map);
							n = hawk_rtx_setrefval (rtx, ref, map);
							hawk_rtx_refdownval (rtx, map);
							if (n <= -1)
							{
								map = HAWK_NULL;
								return -9999;
							}
						}
						else x = UCI_ERR_MEM;
					}
					else if (uo->type == UCI_TYPE_LIST)
					{
						hawk_val_t* map, * fld;
						hawk_val_map_data_t md[2];
						struct uci_element* tmp;
						hawk_int_t count;

						count = 0;
						uci_foreach_element(&uo->v.list, tmp) { count++; }

						HAWK_MEMSET (md, 0, HAWK_SIZEOF(md));

						md[0].key.ptr = HAWK_T("type");
						md[0].key.len = 4;
						md[0].type = HAWK_VAL_MAP_DATA_OOCSTR;
						md[0].vptr = HAWK_T("list");

						md[1].key.ptr = HAWK_T("count");
						md[1].key.len = 5;
						md[1].type = HAWK_VAL_MAP_DATA_INT;
						md[1].vptr = &count;

						map = hawk_rtx_makemapvalwithdata(rtx, md, 2);
						if (map)
						{
							count = 1;
							hawk_rtx_refupval (rtx, map);

							uci_foreach_element(&uo->v.list, tmp)
							{
								const hawk_oocs_t* subsep;
								hawk_oocs_t k[4];
								hawk_ooch_t idxbuf[64];
								hawk_ooch_t* kp;
								hawk_oow_t kl;

								fld = hawk_rtx_makestrvalwithbcstr(rtx, tmp->name);
								if (!fld)
								{
									hawk_rtx_refdownval (rtx, map);
									map = HAWK_NULL;
									x = UCI_ERR_MEM;
									break;
								}

								hawk_rtx_refupval (rtx, fld);
								subsep = hawk_rtx_getsubsep(rtx);

								k[0].ptr = HAWK_T("value");
								k[0].len = 5;
								k[1].ptr = subsep->ptr;
								k[1].len = subsep->len;
								k[2].ptr = idxbuf;
								k[2].len = hawk_fmt_uintmax_to_oocstr(idxbuf, HAWK_COUNTOF(idxbuf), count, 10, -1, HAWK_T('\0'), HAWK_NULL);
								k[3].ptr = HAWK_NULL;
								k[3].len = 0;

								kp = hawk_rtx_dupoocsarr(rtx, k, &kl);
								if (!kp || hawk_rtx_setmapvalfld(rtx, map, kp, kl, fld) == HAWK_NULL)
								{
									if (kp) hawk_rtx_freemem (rtx, kp);
									hawk_rtx_refdownval (rtx, fld);
									hawk_rtx_refdownval (rtx, map);
									map = HAWK_NULL;
									x = UCI_ERR_MEM;
									break;
								}

								hawk_rtx_freemem (rtx, kp);
								hawk_rtx_refdownval (rtx, fld);
								count++;
							}

							if (map)
							{
								int n;
								n = hawk_rtx_setrefval(rtx, ref, map);
								hawk_rtx_refdownval (rtx, map);
								if (n <= -1)
								{
									map = HAWK_NULL;
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

static HAWK_INLINE uctx_list_t* rtx_to_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	pair = hawk_rbt_search ((hawk_rbt_t*)fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	return (uctx_list_t*)HAWK_RBT_VPTR(pair);
}

static int fnc_uci_errno (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;

	list = rtx_to_list (rtx, fi);

	retv = hawk_rtx_makeintval(rtx, list->errnum);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static hawk_ooch_t* errmsg[] =
{
	HAWK_T("no error"),
	HAWK_T("out of memory"),
	HAWK_T("invalid data"),
	HAWK_T("not found"),
	HAWK_T("I/O error"),
	HAWK_T("parse error"),
	HAWK_T("duplicate data"),
	HAWK_T("unknown error")
};

static int fnc_uci_errstr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t errnum;

	list = rtx_to_list (rtx, fi);

	if (hawk_rtx_getnargs(rtx) <= 0 ||
	    hawk_rtx_valtoint(rtx, hawk_rtx_getarg (rtx, 0), &errnum) <= -1)
	{
		errnum = list->errnum;
	}

	if (errnum < 0 || errnum >= HAWK_COUNTOF(errmsg)) errnum = HAWK_COUNTOF(errmsg) - 1;

	retv = hawk_rtx_makestrvalwithoocstr(rtx, errmsg[errnum]);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_open (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	uctx_node_t* node;
	hawk_int_t ret;
	hawk_val_t* retv;

	list = rtx_to_list (rtx, fi);
	node = new_uctx_node (rtx, list);
	ret = node? node->id: -UCI_ERR_MEM;

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint(rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else ret = close_byid(rtx, list, id);

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_load  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = load_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_unload (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else ret = unload_byid(rtx, list, id);

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_save  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = save_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_commit (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = commit_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_revert (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = revert_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_delete (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = delete_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_rename (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = rename_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_set (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = set_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_addsection (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item, * type;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		type = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 2), HAWK_NULL);
		if (item && type)
		{
			ret = addsection_byid(rtx, list, id, item, type);
		}
		else ret = -UCI_ERR_MEM;

		if (type) hawk_rtx_freemem (rtx, type);
		if (item) hawk_rtx_freemem (rtx, item);
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_addlist (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = addlist_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_setconfdir  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = setconfdir_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}


static int fnc_uci_setsavedir  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = setsavedir_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_adddeltapath  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = adddeltapath_byid(rtx, list, id, item);
			hawk_rtx_freemem (rtx, item);
		}
		else ret = -UCI_ERR_MEM;
	}

	if (ret <= -1)
	{
		list->errnum = -ret;
		ret = -1;
	}

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_uci_getoption (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = getoption_byid(rtx, list, id, item, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2));
			hawk_rtx_freemem (rtx, item);
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

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

static int fnc_uci_getsection (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_int_t id;
	int ret;

	list = rtx_to_list (rtx, fi);

	ret = hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &id);
	if (ret <= -1) ret = -UCI_ERR_INVAL;
	else
	{
		hawk_bch_t* item;

		item = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 1), HAWK_NULL);
		if (item)
		{
			ret = getsection_byid(rtx, list, id, item, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2));
			hawk_rtx_freemem (rtx, item);
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

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ret));
	return 0;
}

/* ------------------------------------------------------------------------ */

static hawk_mod_fnc_tab_t fnctab[] =
{
	{ HAWK_T("adddeltapath"), { { 2, 2, HAWK_NULL    }, fnc_uci_adddeltapath, 0 } },
	{ HAWK_T("addlist"),      { { 2, 2, HAWK_NULL    }, fnc_uci_addlist,      0 } },
	{ HAWK_T("addsection"),   { { 3, 3, HAWK_NULL    }, fnc_uci_addsection,   0 } },
	{ HAWK_T("close"),        { { 1, 1, HAWK_NULL    }, fnc_uci_close,        0 } },
	{ HAWK_T("commit"),       { { 2, 2, HAWK_NULL    }, fnc_uci_commit,       0 } },
	{ HAWK_T("delete"),       { { 2, 2, HAWK_NULL    }, fnc_uci_delete,       0 } },
	{ HAWK_T("errno"),        { { 0, 0, HAWK_NULL    }, fnc_uci_errno,        0 } },
	{ HAWK_T("errstr"),       { { 0, 1, HAWK_NULL    }, fnc_uci_errstr,       0 } },
	{ HAWK_T("getoption"),    { { 3, 3, HAWK_T("vvr")}, fnc_uci_getoption,    0 } },
	{ HAWK_T("getsection"),   { { 3, 3, HAWK_T("vvr")}, fnc_uci_getsection,   0 } },
	{ HAWK_T("load"),         { { 2, 2, HAWK_NULL    }, fnc_uci_load,         0 } },
	{ HAWK_T("open"),         { { 0, 0, HAWK_NULL    }, fnc_uci_open,         0 } },
	{ HAWK_T("rename"),       { { 2, 2, HAWK_NULL    }, fnc_uci_rename,       0 } },
	{ HAWK_T("revert"),       { { 2, 2, HAWK_NULL    }, fnc_uci_revert,       0 } },
	{ HAWK_T("save"),         { { 2, 2, HAWK_NULL    }, fnc_uci_save,         0 } },
	{ HAWK_T("set"),          { { 2, 2, HAWK_NULL    }, fnc_uci_set,          0 } },
	{ HAWK_T("setconfdir"),   { { 2, 2, HAWK_NULL    }, fnc_uci_setconfdir,   0 } },
	{ HAWK_T("setsavedir"),   { { 2, 2, HAWK_NULL    }, fnc_uci_setsavedir,   0 } },
	{ HAWK_T("unload"),       { { 1, 1, HAWK_NULL    }, fnc_uci_unload,       0 } }
};

/* ------------------------------------------------------------------------ */

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_t* rbt;
	uctx_list_t list;

	rbt = (hawk_rbt_t*)mod->ctx;

	HAWK_MEMSET (&list, 0, HAWK_SIZEOF(list));
	if (hawk_rbt_insert(rbt, &rtx, HAWK_SIZEOF(rtx), &list, HAWK_SIZEOF(list)) == HAWK_NULL)
	{
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_ENOMEM);
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
	pair = hawk_rbt_search  (rbt, &rtx, HAWK_SIZEOF(rtx));
	if (pair)
	{
		uctx_list_t* list;
		uctx_node_t* node, * next;

		list = HAWK_RBT_VPTR(pair);
		node = list->head;
		while (node)
		{
			next = node->next;
			free_uctx_node (rtx, list, node);
			node = next;
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
}

int hawk_mod_uci (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (!rbt) return -1;

	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	mod->ctx = rbt;
	return 0;
}

