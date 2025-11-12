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

#include <hawk-rbt.h>
#include "hawk-prv.h"

#define copier_t        hawk_rbt_copier_t
#define freeer_t        hawk_rbt_freeer_t
#define comper_t        hawk_rbt_comper_t
#define keeper_t        hawk_rbt_keeper_t
#define walker_t        hawk_rbt_walker_t
#define cbserter_t      hawk_rbt_cbserter_t

#define KPTR(p)  HAWK_RBT_KPTR(p)
#define KLEN(p)  HAWK_RBT_KLEN(p)
#define VPTR(p)  HAWK_RBT_VPTR(p)
#define VLEN(p)  HAWK_RBT_VLEN(p)

#define KTOB(rbt,len) ((len)*(rbt)->scale[HAWK_RBT_KEY])
#define VTOB(rbt,len) ((len)*(rbt)->scale[HAWK_RBT_VAL])

#define UPSERT 1
#define UPDATE 2
#define ENSERT 3
#define INSERT 4

#define IS_NIL(rbt,x) ((x) == &((rbt)->xnil))
#define LEFT 0
#define RIGHT 1
#define left child[LEFT]
#define right child[RIGHT]
#define rotate_left(rbt,pivot) rotate(rbt,pivot,1);
#define rotate_right(rbt,pivot) rotate(rbt,pivot,0);

HAWK_INLINE hawk_rbt_pair_t* hawk_rbt_allocpair (
	hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	hawk_rbt_pair_t* pair;

	copier_t kcop = rbt->style->copier[HAWK_RBT_KEY];
	copier_t vcop = rbt->style->copier[HAWK_RBT_VAL];

	hawk_oow_t as = HAWK_SIZEOF(hawk_rbt_pair_t);
	if (kcop == HAWK_RBT_COPIER_INLINE) as += HAWK_ALIGN_POW2(KTOB(rbt,klen), HAWK_SIZEOF_VOID_P);
	if (vcop == HAWK_RBT_COPIER_INLINE) as += VTOB(rbt,vlen);

	pair = (hawk_rbt_pair_t*)hawk_gem_allocmem(rbt->gem, as);
	if (HAWK_UNLIKELY(!pair)) return HAWK_NULL;

	pair->color = HAWK_RBT_RED;
	pair->parent = HAWK_NULL;
	pair->child[LEFT] = &rbt->xnil;
	pair->child[RIGHT] = &rbt->xnil;

	KLEN(pair) = klen;
	if (kcop == HAWK_RBT_COPIER_SIMPLE)
	{
		KPTR(pair) = kptr;
	}
	else if (kcop == HAWK_RBT_COPIER_INLINE)
	{
		KPTR(pair) = pair + 1;
		if (kptr) HAWK_MEMCPY(KPTR(pair), kptr, KTOB(rbt,klen));
	}
	else
	{
		KPTR(pair) = kcop(rbt, kptr, klen);
		if (KPTR(pair) == HAWK_NULL)
		{
			hawk_gem_freemem(rbt->gem, pair);
			return HAWK_NULL;
		}
	}

	VLEN(pair) = vlen;
	if (vcop == HAWK_RBT_COPIER_SIMPLE)
	{
		VPTR(pair) = vptr;
	}
	else if (vcop == HAWK_RBT_COPIER_INLINE)
	{
		VPTR(pair) = pair + 1;
		if (kcop == HAWK_RBT_COPIER_INLINE)
			VPTR(pair) = (hawk_oob_t*)VPTR(pair) + HAWK_ALIGN_POW2(KTOB(rbt,klen), HAWK_SIZEOF_VOID_P);
		if (vptr) HAWK_MEMCPY(VPTR(pair), vptr, VTOB(rbt,vlen));
	}
	else
	{
		VPTR(pair) = vcop(rbt, vptr, vlen);
		if (VPTR(pair) != HAWK_NULL)
		{
			if (rbt->style->freeer[HAWK_RBT_KEY])
				rbt->style->freeer[HAWK_RBT_KEY](rbt, KPTR(pair), KLEN(pair));
			hawk_gem_freemem(rbt->gem, pair);
			return HAWK_NULL;
		}
	}

	return pair;
}

HAWK_INLINE void hawk_rbt_freepair (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair)
{
	if (rbt->style->freeer[HAWK_RBT_KEY])
		rbt->style->freeer[HAWK_RBT_KEY](rbt, KPTR(pair), KLEN(pair));
	if (rbt->style->freeer[HAWK_RBT_VAL])
		rbt->style->freeer[HAWK_RBT_VAL](rbt, VPTR(pair), VLEN(pair));
	hawk_gem_freemem(rbt->gem, pair);
}

static hawk_rbt_style_t style[] =
{
	{
		{
			HAWK_RBT_COPIER_DEFAULT,
			HAWK_RBT_COPIER_DEFAULT
		},
		{
			HAWK_RBT_FREEER_DEFAULT,
			HAWK_RBT_FREEER_DEFAULT
		},
		HAWK_RBT_COMPER_DEFAULT,
		HAWK_RBT_KEEPER_DEFAULT
	},

	{
		{
			HAWK_RBT_COPIER_INLINE,
			HAWK_RBT_COPIER_INLINE
		},
		{
			HAWK_RBT_FREEER_DEFAULT,
			HAWK_RBT_FREEER_DEFAULT
		},
		HAWK_RBT_COMPER_DEFAULT,
		HAWK_RBT_KEEPER_DEFAULT
	},

	{
		{
			HAWK_RBT_COPIER_INLINE,
			HAWK_RBT_COPIER_DEFAULT
		},
		{
			HAWK_RBT_FREEER_DEFAULT,
			HAWK_RBT_FREEER_DEFAULT
		},
		HAWK_RBT_COMPER_DEFAULT,
		HAWK_RBT_KEEPER_DEFAULT
	},

	{
		{
			HAWK_RBT_COPIER_DEFAULT,
			HAWK_RBT_COPIER_INLINE
		},
		{
			HAWK_RBT_FREEER_DEFAULT,
			HAWK_RBT_FREEER_DEFAULT
		},
		HAWK_RBT_COMPER_DEFAULT,
		HAWK_RBT_KEEPER_DEFAULT
	}
};

const hawk_rbt_style_t* hawk_get_rbt_style (hawk_rbt_style_kind_t kind)
{
	return &style[kind];
}

hawk_rbt_t* hawk_rbt_open (hawk_gem_t* gem, hawk_oow_t xtnsize, int kscale, int vscale)
{
	hawk_rbt_t* rbt;

	rbt = (hawk_rbt_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_rbt_t) + xtnsize);
	if (HAWK_UNLIKELY(!rbt)) return HAWK_NULL;

	if (HAWK_UNLIKELY(hawk_rbt_init(rbt, gem, kscale, vscale) <= -1))
	{
		hawk_gem_freemem(gem, rbt);
		return HAWK_NULL;
	}

	HAWK_MEMSET(rbt + 1, 0, xtnsize);
	return rbt;
}

void hawk_rbt_close (hawk_rbt_t* rbt)
{
	hawk_rbt_fini (rbt);
	hawk_gem_freemem(rbt->gem, rbt);
}

int hawk_rbt_init (hawk_rbt_t* rbt, hawk_gem_t* gem, int kscale, int vscale)
{
	/* do not zero out the extension */
	HAWK_MEMSET(rbt, 0, HAWK_SIZEOF(*rbt));
	rbt->gem = gem;

	rbt->scale[HAWK_RBT_KEY] = (kscale < 1)? 1: kscale;
	rbt->scale[HAWK_RBT_VAL] = (vscale < 1)? 1: vscale;
	rbt->size = 0;

	rbt->style = &style[0];

	/* self-initializing nil */
	HAWK_MEMSET(&rbt->xnil, 0, HAWK_SIZEOF(rbt->xnil));
	rbt->xnil.color = HAWK_RBT_BLACK;
	rbt->xnil.left = &rbt->xnil;
	rbt->xnil.right = &rbt->xnil;

	/* root is set to nil initially */
	rbt->root = &rbt->xnil;

#if defined(HAWK_ENABLE_RBT_ITR_PROTECTION)
	rbt->_prot_itr._prot_next = &rbt->_prot_itr;
	rbt->_prot_itr._prot_prev = &rbt->_prot_itr;
#endif

	return 0;
}

void hawk_rbt_fini (hawk_rbt_t* rbt)
{
	hawk_rbt_clear(rbt);
}

const hawk_rbt_style_t* hawk_rbt_getstyle (const hawk_rbt_t* rbt)
{
	return rbt->style;
}

void hawk_rbt_setstyle (hawk_rbt_t* rbt, const hawk_rbt_style_t* style)
{
	HAWK_ASSERT (style != HAWK_NULL);
	rbt->style = style;
}

hawk_oow_t hawk_rbt_getsize (const hawk_rbt_t* rbt)
{
	return rbt->size;
}

hawk_rbt_pair_t* hawk_rbt_search (const hawk_rbt_t* rbt, const void* kptr, hawk_oow_t klen)
{
	hawk_rbt_pair_t* pair = rbt->root;

	while (!IS_NIL(rbt,pair))
	{
		int n = rbt->style->comper(rbt, kptr, klen, KPTR(pair), KLEN(pair));
		if (n == 0) return pair;

		if (n > 0) pair = pair->right;
		else /* if (n < 0) */ pair = pair->left;
	}

	hawk_gem_seterrnum (rbt->gem, HAWK_NULL, HAWK_ENOENT);
	return HAWK_NULL;
}

static void rotate (hawk_rbt_t* rbt, hawk_rbt_pair_t* pivot, int leftwise)
{
	/*
	 * == leftwise rotation
	 * move the pivot pair down to the poistion of the pivot's original
	 * left child(x). move the pivot's right child(y) to the pivot's original
	 * position. as 'c1' is between 'y' and 'pivot', move it to the right
	 * of the new pivot position.
	 *       parent                   parent
	 *        | | (left or right?)      | |
	 *       pivot                      y
	 *       /  \                     /  \
	 *     x     y    =====>      pivot   c2
	 *          / \               /  \
	 *         c1  c2            x   c1
	 *
	 * == rightwise rotation
	 * move the pivot pair down to the poistion of the pivot's original
	 * right child(y). move the pivot's left child(x) to the pivot's original
	 * position. as 'c2' is between 'x' and 'pivot', move it to the left
	 * of the new pivot position.
	 *
	 *       parent                   parent
	 *        | | (left or right?)      | |
	 *       pivot                      x
	 *       /  \                     /  \
	 *     x     y    =====>        c1   pivot
	 *    / \                            /  \
	 *   c1  c2                         c2   y
	 *
	 *
	 * the actual implementation here resolves the pivot's relationship to
	 * its parent by comparaing pointers as it is not known if the pivot pair
	 * is the left child or the right child of its parent,
	 */

	hawk_rbt_pair_t* parent, * z, * c;
	int cid1, cid2;

	HAWK_ASSERT (pivot != HAWK_NULL);

	if (leftwise)
	{
		cid1 = RIGHT;
		cid2 = LEFT;
	}
	else
	{
		cid1 = LEFT;
		cid2 = RIGHT;
	}

	parent = pivot->parent;
	/* y for leftwise rotation, x for rightwise rotation */
	z = pivot->child[cid1];
	/* c1 for leftwise rotation, c2 for rightwise rotation */
	c = z->child[cid2];

	z->parent = parent;
	if (parent)
	{
		if (parent->left == pivot)
		{
			parent->left = z;
		}
		else
		{
			HAWK_ASSERT (parent->right == pivot);
			parent->right = z;
		}
	}
	else
	{
		HAWK_ASSERT (rbt->root == pivot);
		rbt->root = z;
	}

	z->child[cid2] = pivot;
	if (!IS_NIL(rbt,pivot)) pivot->parent = z;

	pivot->child[cid1] = c;
	if (!IS_NIL(rbt,c)) c->parent = pivot;
}

static void adjust (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair)
{
	while (pair != rbt->root)
	{
		hawk_rbt_pair_t* tmp, * tmp2, * x_par;
		int leftwise;

		x_par = pair->parent;
		if (x_par->color == HAWK_RBT_BLACK) break;

		HAWK_ASSERT (x_par->parent != HAWK_NULL);

		if (x_par == x_par->parent->child[LEFT])
		{
			tmp = x_par->parent->child[RIGHT];
			tmp2 = x_par->child[RIGHT];
			leftwise = 1;
		}
		else
		{
			tmp = x_par->parent->child[LEFT];
			tmp2 = x_par->child[LEFT];
			leftwise = 0;
		}

		if (tmp->color == HAWK_RBT_RED)
		{
			x_par->color = HAWK_RBT_BLACK;
			tmp->color = HAWK_RBT_BLACK;
			x_par->parent->color = HAWK_RBT_RED;
			pair = x_par->parent;
		}
		else
		{
			if (pair == tmp2)
			{
				pair = x_par;
				rotate(rbt, pair, leftwise);
				x_par = pair->parent;
			}

			x_par->color = HAWK_RBT_BLACK;
			x_par->parent->color = HAWK_RBT_RED;
			rotate(rbt, x_par->parent, !leftwise);
		}
	}
}

static hawk_rbt_pair_t* change_pair_val (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, void* vptr, hawk_oow_t vlen)
{
	if (VPTR(pair) == vptr && VLEN(pair) == vlen)
	{
		/* if the old value and the new value are the same,
		 * it just calls the handler for this condition.
		 * No value replacement occurs. */
		if (rbt->style->keeper != HAWK_NULL)
		{
			rbt->style->keeper(rbt, vptr, vlen);
		}
	}
	else
	{
		copier_t vcop = rbt->style->copier[HAWK_RBT_VAL];
		void* ovptr = VPTR(pair);
		hawk_oow_t ovlen = VLEN(pair);

		/* place the new value according to the copier */
		if (vcop == HAWK_RBT_COPIER_SIMPLE)
		{
			VPTR(pair) = vptr;
			VLEN(pair) = vlen;
		}
		else if (vcop == HAWK_RBT_COPIER_INLINE)
		{
			if (ovlen == vlen)
			{
				if (vptr) HAWK_MEMCPY(VPTR(pair), vptr, VTOB(rbt,vlen));
			}
			else
			{
				/* need to reconstruct the pair */
				hawk_rbt_pair_t* p = hawk_rbt_allocpair(rbt, KPTR(pair), KLEN(pair), vptr, vlen);
				if (HAWK_UNLIKELY(!p)) return HAWK_NULL;

				p->color = pair->color;
				p->left = pair->left;
				p->right = pair->right;
				p->parent = pair->parent;

				if (pair->parent)
				{
					if (pair->parent->left == pair)
					{
						pair->parent->left = p;
					}
					else
					{
						HAWK_ASSERT (pair->parent->right == pair);
						pair->parent->right = p;
					}
				}
				if (!IS_NIL(rbt,pair->left)) pair->left->parent = p;
				if (!IS_NIL(rbt,pair->right)) pair->right->parent = p;

				if (pair == rbt->root) rbt->root = p;

				hawk_rbt_freepair(rbt, pair);
				return p;
			}
		}
		else
		{
			void* nvptr = vcop(rbt, vptr, vlen);
			if (HAWK_UNLIKELY(!nvptr)) return HAWK_NULL;
			VPTR(pair) = nvptr;
			VLEN(pair) = vlen;
		}

		/* free up the old value */
		if (rbt->style->freeer[HAWK_RBT_VAL])
		{
			rbt->style->freeer[HAWK_RBT_VAL](rbt, ovptr, ovlen);
		}
	}

	return pair;
}

static hawk_rbt_pair_t* insert (hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen, int opt)
{
	hawk_rbt_pair_t* x_cur = rbt->root;
	hawk_rbt_pair_t* x_par = HAWK_NULL;
	hawk_rbt_pair_t* x_new;

	while (!IS_NIL(rbt,x_cur))
	{
		int n = rbt->style->comper(rbt, kptr, klen, KPTR(x_cur), KLEN(x_cur));
		if (n == 0)
		{
			switch (opt)
			{
				case UPSERT:
				case UPDATE:
					return change_pair_val(rbt, x_cur, vptr, vlen);

				case ENSERT:
					/* return existing pair */
					return x_cur;

				case INSERT:
					/* return failure */
					hawk_gem_seterrnum (rbt->gem, HAWK_NULL, HAWK_EEXIST);
					return HAWK_NULL;
			}
		}

		x_par = x_cur;

		if (n > 0) x_cur = x_cur->right;
		else /* if (n < 0) */ x_cur = x_cur->left;
	}

	if (opt == UPDATE)
	{
		hawk_gem_seterrnum (rbt->gem, HAWK_NULL, HAWK_ENOENT);
		return HAWK_NULL;
	}

	x_new = hawk_rbt_allocpair(rbt, kptr, klen, vptr, vlen);
	if (HAWK_UNLIKELY(!x_new)) return HAWK_NULL;

	if (x_par == HAWK_NULL)
	{
		/* the tree contains no pair */
		HAWK_ASSERT (rbt->root == &rbt->xnil);
		rbt->root = x_new;
	}
	else
	{
		/* perform normal binary insert */
		int n = rbt->style->comper(rbt, kptr, klen, KPTR(x_par), KLEN(x_par));
		if (n > 0)
		{
			HAWK_ASSERT (x_par->right == &rbt->xnil);
			x_par->right = x_new;
		}
		else
		{
			HAWK_ASSERT (x_par->left == &rbt->xnil);
			x_par->left = x_new;
		}

		x_new->parent = x_par;
		adjust(rbt, x_new);
	}

	rbt->root->color = HAWK_RBT_BLACK;
	rbt->size++;
	return x_new;
}

hawk_rbt_pair_t* hawk_rbt_upsert (hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert(rbt, kptr, klen, vptr, vlen, UPSERT);
}

hawk_rbt_pair_t* hawk_rbt_ensert (hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert(rbt, kptr, klen, vptr, vlen, ENSERT);
}

hawk_rbt_pair_t* hawk_rbt_insert (hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert(rbt, kptr, klen, vptr, vlen, INSERT);
}


hawk_rbt_pair_t* hawk_rbt_update (hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, void* vptr, hawk_oow_t vlen)
{
	return insert(rbt, kptr, klen, vptr, vlen, UPDATE);
}

hawk_rbt_pair_t* hawk_rbt_cbsert (hawk_rbt_t* rbt, void* kptr, hawk_oow_t klen, cbserter_t cbserter, void* ctx)
{
	hawk_rbt_pair_t* x_cur = rbt->root;
	hawk_rbt_pair_t* x_par = HAWK_NULL;
	hawk_rbt_pair_t* x_new;

	while (!IS_NIL(rbt,x_cur))
	{
		int n = rbt->style->comper(rbt, kptr, klen, KPTR(x_cur), KLEN(x_cur));
		if (n == 0)
		{
			/* back up the contents of the current pair
			 * in case it is reallocated */
			hawk_rbt_pair_t tmp;

			tmp = *x_cur;

			/* call the callback function to manipulate the pair */
			x_new = cbserter(rbt, x_cur, kptr, klen, ctx);
			if (x_new == HAWK_NULL)
			{
				/* error returned by the callback function */
				return HAWK_NULL;
			}

			if (x_new != x_cur)
			{
				/* the current pair has been reallocated, which implicitly
				 * means the previous contents were wiped out. so the contents
				 * backed up will be used for restoration/migration */

				x_new->color = tmp.color;
				x_new->left = tmp.left;
				x_new->right = tmp.right;
				x_new->parent = tmp.parent;

				if (tmp.parent)
				{
					if (tmp.parent->left == x_cur)
					{
						tmp.parent->left = x_new;
					}
					else
					{
						HAWK_ASSERT (tmp.parent->right == x_cur);
						tmp.parent->right = x_new;
					}
				}
				if (!IS_NIL(rbt,tmp.left)) tmp.left->parent = x_new;
				if (!IS_NIL(rbt,tmp.right)) tmp.right->parent = x_new;

				if (x_cur == rbt->root) rbt->root = x_new;
			}

			return x_new;
		}

		x_par = x_cur;

		if (n > 0) x_cur = x_cur->right;
		else /* if (n < 0) */ x_cur = x_cur->left;
	}

	x_new = cbserter(rbt, HAWK_NULL, kptr, klen, ctx);
	if (x_new == HAWK_NULL) return HAWK_NULL;

	if (x_par == HAWK_NULL)
	{
		/* the tree contains no pair */
		HAWK_ASSERT (rbt->root == &rbt->xnil);
		rbt->root = x_new;
	}
	else
	{
		/* perform normal binary insert */
		int n = rbt->style->comper(rbt, kptr, klen, KPTR(x_par), KLEN(x_par));
		if (n > 0)
		{
			HAWK_ASSERT (x_par->right == &rbt->xnil);
			x_par->right = x_new;
		}
		else
		{
			HAWK_ASSERT (x_par->left == &rbt->xnil);
			x_par->left = x_new;
		}

		x_new->parent = x_par;
		adjust(rbt, x_new);
	}

	rbt->root->color = HAWK_RBT_BLACK;
	rbt->size++;
	return x_new;
}


static void adjust_for_delete (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair, hawk_rbt_pair_t* par)
{
	while (pair != rbt->root && pair->color == HAWK_RBT_BLACK)
	{
		hawk_rbt_pair_t* tmp;

		if (pair == par->left)
		{
			tmp = par->right;
			if (tmp->color == HAWK_RBT_RED)
			{
				tmp->color = HAWK_RBT_BLACK;
				par->color = HAWK_RBT_RED;
				rotate_left(rbt, par);
				tmp = par->right;
			}

			if (tmp->left->color == HAWK_RBT_BLACK &&
			    tmp->right->color == HAWK_RBT_BLACK)
			{
				if (!IS_NIL(rbt,tmp)) tmp->color = HAWK_RBT_RED;
				pair = par;
				par = pair->parent;
			}
			else
			{
				if (tmp->right->color == HAWK_RBT_BLACK)
				{
					if (!IS_NIL(rbt,tmp->left))
						tmp->left->color = HAWK_RBT_BLACK;
					tmp->color = HAWK_RBT_RED;
					rotate_right(rbt, tmp);
					tmp = par->right;
				}

				tmp->color = par->color;
				if (!IS_NIL(rbt,par)) par->color = HAWK_RBT_BLACK;
				if (tmp->right->color == HAWK_RBT_RED)
					tmp->right->color = HAWK_RBT_BLACK;

				rotate_left(rbt, par);
				pair = rbt->root;
			}
		}
		else
		{
			HAWK_ASSERT (pair == par->right);
			tmp = par->left;
			if (tmp->color == HAWK_RBT_RED)
			{
				tmp->color = HAWK_RBT_BLACK;
				par->color = HAWK_RBT_RED;
				rotate_right(rbt, par);
				tmp = par->left;
			}

			if (tmp->left->color == HAWK_RBT_BLACK &&
			    tmp->right->color == HAWK_RBT_BLACK)
			{
				if (!IS_NIL(rbt,tmp)) tmp->color = HAWK_RBT_RED;
				pair = par;
				par = pair->parent;
			}
			else
			{
				if (tmp->left->color == HAWK_RBT_BLACK)
				{
					if (!IS_NIL(rbt,tmp->right))
						tmp->right->color = HAWK_RBT_BLACK;
					tmp->color = HAWK_RBT_RED;
					rotate_left(rbt, tmp);
					tmp = par->left;
				}
				tmp->color = par->color;
				if (!IS_NIL(rbt,par)) par->color = HAWK_RBT_BLACK;
				if (tmp->left->color == HAWK_RBT_RED)
					tmp->left->color = HAWK_RBT_BLACK;

				rotate_right(rbt, par);
				pair = rbt->root;
			}
		}
	}

	pair->color = HAWK_RBT_BLACK;
}

static void delete_pair (hawk_rbt_t* rbt, hawk_rbt_pair_t* pair)
{
	hawk_rbt_pair_t* x, * y, * parent;

	HAWK_ASSERT (pair && !IS_NIL(rbt,pair));

	if (IS_NIL(rbt,pair->left) || IS_NIL(rbt,pair->right))
	{
		y = pair;
	}
	else
	{
		/* find a successor with NIL as a child */
		y = pair->right;
		while (!IS_NIL(rbt,y->left)) y = y->left;
	}

	x = IS_NIL(rbt,y->left)? y->right: y->left;

	parent = y->parent;
	if (!IS_NIL(rbt,x)) x->parent = parent;

	if (parent)
	{
		if (y == parent->left)
			parent->left = x;
		else
			parent->right = x;
	}
	else
	{
		rbt->root = x;
	}

	if (y == pair)
	{
		if (y->color == HAWK_RBT_BLACK && !IS_NIL(rbt,x))
			adjust_for_delete(rbt, x, parent);

		hawk_rbt_freepair(rbt, y);
	}
	else
	{
		if (y->color == HAWK_RBT_BLACK && !IS_NIL(rbt,x))
			adjust_for_delete(rbt, x, parent);

		if (pair->parent)
		{
			if (pair->parent->left == pair) pair->parent->left = y;
			if (pair->parent->right == pair) pair->parent->right = y;
		}
		else
		{
			rbt->root = y;
		}

		y->parent = pair->parent;
		y->left = pair->left;
		y->right = pair->right;
		y->color = pair->color;

		if (pair->left->parent == pair) pair->left->parent = y;
		if (pair->right->parent == pair) pair->right->parent = y;

		hawk_rbt_freepair(rbt, pair);
	}

	rbt->size--;

#if defined(HAWK_ENABLE_RBT_ITR_PROTECTION)
	/* an iterator set by hawk_rbt_getfirstpair() or hawk_rbt_getnextpair(), if deleted, gets invalidated.
	 * this protection updates registered iterators to the next valid pair if they gets deleted.
	 * the caller may reuse the iterator if _prot_updated is set to a non-zero value */
	if (rbt->_prot_itr._prot_next != &rbt->_prot_itr)
	{
		/* there are protected iterators */
		hawk_rbt_itr_t* itr = rbt->_prot_itr._prot_next;
		do
		{
			if (itr->pair == pair)
			{
				hawk_oow_t seqno = itr->_prot_seqno;

				/* TODO: this is slow. devise a way to get the next pair safely without traversal */
				hawk_rbt_getfirstpair(rbt, itr);
				while (itr->pair && itr->_prot_seqno < seqno)
				{
					hawk_rbt_getnextpair(rbt, itr);
				}

				itr->_prot_updated = 1;
			}
			itr = itr->_prot_next;
		}
		while (itr != &rbt->_prot_itr);
	}
#endif
}

int hawk_rbt_delete (hawk_rbt_t* rbt, const void* kptr, hawk_oow_t klen)
{
	hawk_rbt_pair_t* pair;

	pair = hawk_rbt_search(rbt, kptr, klen);
	if (!pair) return -1;

	delete_pair(rbt, pair);
	return 0;
}

void hawk_rbt_clear (hawk_rbt_t* rbt)
{
	/* TODO: improve this */
	while (!IS_NIL(rbt,rbt->root)) delete_pair(rbt, rbt->root);
}

#if 0
static HAWK_INLINE hawk_rbt_walk_t walk_recursively (
	hawk_rbt_t* rbt, walker_t walker, void* ctx, hawk_rbt_pair_t* pair)
{
	if (!IS_NIL(rbt,pair->left))
	{
		if (walk_recursively(rbt, walker, ctx, pair->left) == HAWK_RBT_WALK_STOP)
			return HAWK_RBT_WALK_STOP;
	}

	if (walker(rbt, pair, ctx) == HAWK_RBT_WALK_STOP) return HAWK_RBT_WALK_STOP;

	if (!IS_NIL(rbt,pair->right))
	{
		if (walk_recursively(rbt, walker, ctx, pair->right) == HAWK_RBT_WALK_STOP)
			return HAWK_RBT_WALK_STOP;
	}

	return HAWK_RBT_WALK_FORWARD;
}
#endif

void hawk_init_rbt_itr (hawk_rbt_itr_t* itr, int dir)
{
	itr->pair = HAWK_NULL;
	itr->_prev = HAWK_NULL;
	itr->_dir = dir;
	itr->_state = 0;
}

static hawk_rbt_pair_t* get_next_pair (hawk_rbt_t* rbt, hawk_rbt_itr_t* itr)
{
	hawk_rbt_pair_t* x_cur = itr->pair;
	hawk_rbt_pair_t* prev = itr->_prev;
	int l = !!itr->_dir;
	int r = !itr->_dir;

	if (itr->_state == 1) goto resume_1;
	if (itr->_state == 2) goto resume_2;

	while (x_cur && !IS_NIL(rbt,x_cur))
	{
		if (prev == x_cur->parent)
		{
			/* the previous node is the parent of the current node.
			 * it indicates that we're going down to the child[l] */
			if (!IS_NIL(rbt,x_cur->child[l]))
			{
				/* go to the child[l] child */
				prev = x_cur;
				x_cur = x_cur->child[l];
			}
			else
			{
				/*if (walker(rbt, x_cur, ctx) == HAWK_RBT_WALK_STOP) break; */
				itr->pair = x_cur;
				itr->_prev = prev;
				itr->_state = 1;
			#if defined(HAWK_ENABLE_RBT_ITR_PROTECTION)
				itr->_prot_seqno++;
			#endif
				return x_cur;

			resume_1:
				if (!IS_NIL(rbt,x_cur->child[r]))
				{
					/* go down to the right node if exists */
					prev = x_cur;
					x_cur = x_cur->child[r];
				}
				else
				{
					/* otherwise, move up to the parent */
					prev = x_cur;
					x_cur = x_cur->parent;
				}
			}
		}
		else if (prev == x_cur->child[l])
		{
			/* the left child has been already traversed */

			/*if (walker(rbt, x_cur, ctx) == HAWK_RBT_WALK_STOP) break;*/
			itr->pair = x_cur;
			itr->_prev = prev;
			itr->_state = 2;
		#if defined(HAWK_ENABLE_RBT_ITR_PROTECTION)
			itr->_prot_seqno++;
		#endif
			return x_cur;

		resume_2:
			if (!IS_NIL(rbt,x_cur->child[r]))
			{
				/* go down to the right node if it exists */
				prev = x_cur;
				x_cur = x_cur->child[r];
			}
			else
			{
				/* otherwise, move up to the parent */
				prev = x_cur;
				x_cur = x_cur->parent;
			}
		}
		else
		{
			/* both the left child and the right child have been traversed */
			HAWK_ASSERT (prev == x_cur->child[r]);
			/* just move up to the parent */
			prev = x_cur;
			x_cur = x_cur->parent;
		}
	}

	itr->pair = HAWK_NULL;
	itr->_prev = HAWK_NULL;
	itr->_state = 0;
	return HAWK_NULL;
}

hawk_rbt_pair_t* hawk_rbt_getfirstpair (hawk_rbt_t* rbt, hawk_rbt_itr_t* itr)
{
	itr->pair = rbt->root;
	itr->_prev = rbt->root->parent;
	itr->_state = 0;
#if defined(HAWK_ENABLE_RBT_ITR_PROTECTION)
	itr->_prot_seqno = 0;
	/* _prot_prev and _prot_next must be left uninitialized because of the way
	 * this function is called in delete_pair() for protection handling
	 */
#endif
	return get_next_pair(rbt, itr);
}

hawk_rbt_pair_t* hawk_rbt_getnextpair (hawk_rbt_t* rbt, hawk_rbt_itr_t* itr)
{
	return get_next_pair(rbt, itr);
}

#if defined(HAWK_ENABLE_RBT_ITR_PROTECTION)
void hawk_rbt_protectitr (hawk_rbt_t* rbt, hawk_rbt_itr_t* itr)
{
	itr->_prot_next = &rbt->_prot_itr;
	itr->_prot_prev = rbt->_prot_itr._prot_prev;
	itr->_prot_prev->_prot_next = itr;
	rbt->_prot_itr._prot_prev = itr;
}

void hawk_rbt_unprotectitr (hawk_rbt_t* rbt, hawk_rbt_itr_t* itr)
{
	itr->_prot_prev->_prot_next = itr->_prot_next;
	itr->_prot_next->_prot_prev = itr->_prot_prev;
}
#endif

void hawk_rbt_walk (hawk_rbt_t* rbt, walker_t walker, void* ctx)
{
	hawk_rbt_itr_t itr;
	hawk_rbt_pair_t* pair;

	hawk_init_map_itr (&itr, 0);
	pair = hawk_rbt_getfirstpair(rbt, &itr);
	while (pair)
	{
		if (walker(rbt, pair, ctx) == HAWK_RBT_WALK_STOP) break;
		pair = hawk_rbt_getnextpair(rbt, &itr);
	}
}

void hawk_rbt_rwalk (hawk_rbt_t* rbt, walker_t walker, void* ctx)
{
	hawk_rbt_itr_t itr;
	hawk_rbt_pair_t* pair;

	hawk_init_map_itr (&itr, 1);
	pair = hawk_rbt_getfirstpair(rbt, &itr);
	while (pair)
	{
		if (walker(rbt, pair, ctx) == HAWK_RBT_WALK_STOP) break;
		pair = hawk_rbt_getnextpair(rbt, &itr);
	}
}


int hawk_rbt_dflcomp (const hawk_rbt_t* rbt, const void* kptr1, hawk_oow_t klen1, const void* kptr2, hawk_oow_t klen2)
{
	hawk_oow_t min;
	int n, nn;

	if (klen1 < klen2)
	{
		min = klen1;
		nn = -1;
	}
	else
	{
		min = klen2;
		nn = (klen1 == klen2)? 0: 1;
	}

	n = HAWK_MEMCMP(kptr1, kptr2, KTOB(rbt,min));
	if (n == 0) n = nn;
	return n;
}

