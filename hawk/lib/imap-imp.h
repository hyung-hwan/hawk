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

/* THIS FILE IS SUPPOSED TO BE INCLUDED BY MODULE SOURCE THAT MAINTAINS MAPPING BETWEEN ID AND DATA */

typedef struct __IMAP_NODE_T __IMAP_NODE_T;
struct __IMAP_NODE_T
{
	__IMAP_NODE_T* prev;
	__IMAP_NODE_T* next;
	int id;

	__IMAP_NODE_T_DATA
};

typedef struct __IMAP_LIST_T __IMAP_LIST_T;
struct __IMAP_LIST_T
{
	__IMAP_NODE_T* head;
	__IMAP_NODE_T* tail;
	__IMAP_NODE_T* free;

	/* mapping table to map 'id' to 'node' */
	struct
	{
		__IMAP_NODE_T** tab;
		int capa;
		int high;
	} map;

	__IMAP_LIST_T_DATA
};

static __IMAP_NODE_T* __MAKE_IMAP_NODE (hawk_rtx_t* rtx, __IMAP_LIST_T* list)
{
	/* create a new context node and append it to the list tail */
	__IMAP_NODE_T* node;

	node = HAWK_NULL;

	if (list->free) 
	{
		node = list->free;
		list->free = node->next;
	}
	else
	{
		node = hawk_rtx_callocmem(rtx, HAWK_SIZEOF(*node));
		if (!node) goto oops;
		
		if (list->map.high <= list->map.capa)
		{
			hawk_oow_t newcapa, inc;
			__IMAP_NODE_T** tmp;

			inc = HAWK_TYPE_MAX(int) - list->map.capa;
			if (inc == 0) goto oops; /* too many nodes */

			if (inc > 64) inc = 64;
			newcapa = (hawk_oow_t)list->map.capa + inc;

			tmp = (__IMAP_NODE_T**)hawk_rtx_reallocmem(rtx, list->map.tab, HAWK_SIZEOF(*tmp) * newcapa);
			if (!tmp) goto oops;

			HAWK_MEMSET (&tmp[list->map.capa], 0, HAWK_SIZEOF(*tmp) * (newcapa - list->map.capa));

			list->map.tab = tmp;
			list->map.capa = newcapa;
		}

		node->id = list->map.high;
		list->map.high++;
	}

	HAWK_ASSERT (hawk_rtx_getawk(rtx), list->map.tab[node->id] == HAWK_NULL);
	list->map.tab[node->id] = node;

	/* append it to the tail */
	node->next = HAWK_NULL;
	node->prev = list->tail;
	if (list->tail) list->tail->next = node;
	else list->head = node;
	list->tail = node;

	return node;

oops:
	if (node) hawk_rtx_freemem (rtx, node);
	return HAWK_NULL;
}

static void __FREE_IMAP_NODE (hawk_rtx_t* rtx, __IMAP_LIST_T* list, __IMAP_NODE_T* node)
{
	if (node->prev) node->prev->next = node->next;
	if (node->next) node->next->prev = node->prev;
	if (list->head == node) list->head = node->next;
	if (list->tail == node) list->tail = node->prev;

	list->map.tab[node->id] = HAWK_NULL;

	if (list->map.high == node->id + 1)
	{
		/* destroy the actual node if the node to be freed has the
		 * highest id */
		hawk_rtx_freemem (rtx, node);
		list->map.high--;
	}
	else
	{
		/* otherwise, chain the node to the free list */
		node->next = list->free;
		list->free = node;
	}

	/* however, i destroy the whole free list when all the nodes are
	 * chanined to the free list */
	if (list->head == HAWK_NULL) 
	{
		__IMAP_NODE_T* curnode;

		while (list->free)
		{
			curnode = list->free;
			list->free = list->free->next;
			hawk_rtx_freemem (rtx, curnode);
		}

		hawk_rtx_freemem (rtx, list->map.tab);
		list->map.high = 0;
		list->map.capa = 0;
		list->map.tab = HAWK_NULL;
	}
}
