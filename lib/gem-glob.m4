dnl ---------------------------------------------------------------------------
changequote(`[[', `]]')dnl
dnl ---------------------------------------------------------------------------

define([[fn_glob]], [[
pushdef([[_fn_name_]], $1)dnl
pushdef([[_char_type_]], $2)dnl
pushdef([[_cs_type_]], $3)dnl
pushdef([[_ecs_prefix_]], $4)dnl
pushdef([[_ECS_PREFIX_]], $5)dnl
pushdef([[_cb_type_]], $6)dnl
pushdef([[_path_exists_]], $7)dnl
pushdef([[_fnmat_]], $8)dnl
pushdef([[_g_prefix_]], $9)dnl

#if defined(NO_RECURSION)
typedef struct _g_prefix_()_stack_node_t _g_prefix_()_stack_node_t;
#endif

struct _g_prefix_()_glob_t
{
	_cb_type_ cbimpl;
	void* cbctx;

	hawk_gem_t* gem;
	int flags;

	_ecs_prefix_()_t path;
	_ecs_prefix_()_t tbuf; /* temporary buffer */

	hawk_becs_t mbuf; /* not used if the base character type is hawk_bch_t */

	int expanded;
	int fnmat_flags;

#if defined(NO_RECURSION)
	_g_prefix_()_stack_node_t* stack;
	_g_prefix_()_stack_node_t* free;
#endif
};

typedef struct _g_prefix_()_glob_t _g_prefix_()_glob_t;

struct _g_prefix_()_segment_t
{
	segment_type_t type;

	const _char_type_* ptr;
	hawk_oow_t    len;

	_char_type_ sep; /* preceeding separator */
	unsigned int wild: 1;  /* indicate that it contains wildcards */
	unsigned int esc: 1;  /* indicate that it contains escaped letters */
	unsigned int next: 1;  /* indicate that it has the following segment */
};
typedef struct _g_prefix_()_segment_t _g_prefix_()_segment_t;

#if defined(NO_RECURSION)
struct _g_prefix_()_stack_node_t
{
	hawk_oow_t tmp;
	hawk_oow_t tmp2;
	hawk_dir_t* dp;
	_g_prefix_()_segment_t seg;
	_g_prefix_()_stack_node_t* next;
};
#endif

static int _g_prefix_()_get_next_segment (_g_prefix_()_glob_t* g, _g_prefix_()_segment_t* seg)
{
	if (seg->type == NONE)
	{
		/* seg->ptr must point to the beginning of the pattern
		 * and seg->len must be zero when seg->type is NONE. */
		if (IS_NIL(seg->ptr[0]))
		{
			/* nothing to do */
		}
		else if (IS_SEP(seg->ptr[0]))
		{
			seg->type = ROOT;
			seg->len = 1;
			seg->next = IS_NIL(seg->ptr[1])? 0: 1;
			seg->sep = '\0';
			seg->wild = 0;
			seg->esc = 0;
		}
	#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
		else if (IS_DRIVE(seg->ptr))
		{
			seg->type = ROOT;
			seg->len = 2;
			if (IS_SEP(seg->ptr[2])) seg->len++;
			seg->next = IS_NIL(seg->ptr[seg->len])? 0: 1;
			seg->sep = '\0';
			seg->wild = 0;
			seg->esc = 0;
		}
	#endif
		else
		{
			int escaped = 0;
			seg->type = NORMAL;
			seg->sep = '\0';
			seg->wild = 0;
			seg->esc = 0;
			do
			{
				if (escaped) escaped = 0;
				else
				{
					if (IS_ESC(seg->ptr[seg->len])) 
					{
						escaped = 1;
						seg->esc = 1;
					}
					else if (IS_WILD(seg->ptr[seg->len])) seg->wild = 1;
				}

				seg->len++;
			}
			while (!IS_SEP_OR_NIL(seg->ptr[seg->len]));
			seg->next = IS_NIL(seg->ptr[seg->len])? 0: 1;
		}
	}
	else if (seg->type == ROOT)
	{
		int escaped = 0;
		seg->type = NORMAL;
		seg->ptr = &seg->ptr[seg->len];
		seg->len = 0;
		seg->sep = '\0';
		seg->wild = 0;
		seg->esc = 0;

		while (!IS_SEP_OR_NIL(seg->ptr[seg->len])) 
		{
			if (escaped) escaped = 0;
			else
			{
				if (IS_ESC(seg->ptr[seg->len])) 
				{
					escaped = 1;
					seg->esc = 1;
				}
				else if (IS_WILD(seg->ptr[seg->len])) seg->wild = 1;
			}
			seg->len++;
		}
		seg->next = IS_NIL(seg->ptr[seg->len])? 0: 1;
	}
	else
	{
		HAWK_ASSERT (seg->type == NORMAL);

		seg->ptr = &seg->ptr[seg->len + 1];
		seg->len = 0;
		seg->wild = 0;
		seg->esc = 0;
		if (IS_NIL(seg->ptr[-1])) 
		{
			seg->type = NONE;
			seg->next = 0;
			seg->sep = '\0';
		}
		else
		{
			int escaped = 0;
			seg->sep = seg->ptr[-1];
			while (!IS_SEP_OR_NIL(seg->ptr[seg->len])) 
			{
				if (escaped) escaped = 0;
				else
				{
					if (IS_ESC(seg->ptr[seg->len])) 
					{
						escaped = 1;
						seg->esc = 1;
					}
					else if (IS_WILD(seg->ptr[seg->len])) seg->wild = 1;
				}
				seg->len++;
			}
			seg->next = IS_NIL(seg->ptr[seg->len])? 0: 1;
		}
	}

	return seg->type;
}

static int _g_prefix_()_handle_non_wild_segments (_g_prefix_()_glob_t* g, _g_prefix_()_segment_t* seg)
{
	while (_g_prefix_()_get_next_segment(g, seg) != NONE && !seg->wild)
	{
		HAWK_ASSERT (seg->type != NONE && !seg->wild);

		if (seg->sep && _ecs_prefix_()_ccat (&g->path, seg->sep) == (hawk_oow_t)-1) return -1;
		if (seg->esc)
		{
			/* if the segment contains escape sequences,
			 * strip the escape letters off the segment */


			_cs_type_ tmp;
			hawk_oow_t i;
			int escaped = 0;

			if (_ECS_PREFIX_()_CAPA(&g->tbuf) < seg->len &&
			    _ecs_prefix_()_setcapa (&g->tbuf, seg->len) == (hawk_oow_t)-1) return -1;

			tmp.ptr = _ECS_PREFIX_()_PTR(&g->tbuf);
			tmp.len = 0;

			/* the following loop drops the last character 
			 * if it is the escape character */
			for (i = 0; i < seg->len; i++)
			{
				if (escaped)
				{
					escaped = 0;
					tmp.ptr[tmp.len++] = seg->ptr[i];
				}
				else
				{
					if (IS_ESC(seg->ptr[i])) 
						escaped = 1;
					else
						tmp.ptr[tmp.len++] = seg->ptr[i];
				}
			}

			if (_ecs_prefix_()_ncat (&g->path, tmp.ptr, tmp.len) == (hawk_oow_t)-1) return -1;
		}
		else
		{
			/* if the segment doesn't contain escape sequences,
			 * append the segment to the path without special handling */
			if (_ecs_prefix_()_ncat (&g->path, seg->ptr, seg->len) == (hawk_oow_t)-1) return -1;
		}

		if (!seg->next && _path_exists_()(g->gem, _ECS_PREFIX_()_PTR(&g->path), &g->mbuf) > 0)
		{
			/* reached the last segment. match if the path exists */
			if (g->cbimpl(_ECS_PREFIX_()_CS(&g->path), g->cbctx) <= -1) return -1;
			g->expanded = 1;
		}
	}

	return 0;
}

static int _g_prefix_()_search (_g_prefix_()_glob_t* g, _g_prefix_()_segment_t* seg)
{
	hawk_dir_t* dp;
	hawk_oow_t tmp, tmp2;
	hawk_dir_ent_t ent;
	int x;

#if defined(NO_RECURSION)
	_g_prefix_()_stack_node_t* r;

entry:
#endif

	dp = HAWK_NULL;

	if (_g_prefix_()_handle_non_wild_segments(g, seg) <= -1) goto oops;

	if (seg->wild)
	{
		int dir_flags = 0;
		if (g->flags & HAWK_GLOB_SKIPSPCDIR) dir_flags |= HAWK_DIR_SKIPSPCDIR;
		if (HAWK_SIZEOF(_char_type_) == HAWK_SIZEOF(hawk_bch_t)) dir_flags |= HAWK_DIR_BPATH;

		dp = hawk_dir_open(g->gem, 0, (const _char_type_*)_ECS_PREFIX_()_PTR(&g->path), dir_flags);
		if (dp)
		{
			tmp = _ECS_PREFIX_()_LEN(&g->path);

			if (seg->sep && _ecs_prefix_()_ccat(&g->path, seg->sep) == (hawk_oow_t)-1) goto oops;
			tmp2 = _ECS_PREFIX_()_LEN(&g->path);

			while (1)
			{
				_ecs_prefix_()_setlen (&g->path, tmp2);

				x = hawk_dir_read(dp, &ent);
				if (x <= -1) 
				{
					if (g->flags & HAWK_GLOB_TOLERANT) break;
					else goto oops;
				}
				if (x == 0) break;

				if (_ecs_prefix_()_cat(&g->path, (const _char_type_*)ent.name) == (hawk_oow_t)-1) goto oops;

				if (_fnmat_()(_ECS_PREFIX_()_CPTR(&g->path,tmp2), seg->ptr, seg->len, g->fnmat_flags) > 0)
				{
					if (seg->next)
					{
				#if defined(NO_RECURSION)
						if (g->free) 
						{
							r = g->free;
							g->free = r->next;
						}
						else
						{
							r = hawk_gem_allocmem(g->gem, HAWK_SIZEOF(*r));
							if (r == HAWK_NULL) goto oops;
						}
						
						/* push key variables that must be restored 
						 * into the stack. */
						r->tmp = tmp;
						r->tmp2 = tmp2;
						r->dp = dp;
						r->seg = *seg;

						r->next = g->stack;
						g->stack = r;

						/* move to the function entry point as if
						 * a recursive call has been made */
						goto entry;

					resume:
						;

				#else
						_g_prefix_()_segment_t save;
						int x;

						save = *seg;
						x = _g_prefix_()_search(g, seg);
						*seg = save;
						if (x <= -1) goto oops;
				#endif
					}
					else
					{
						if (g->cbimpl(_ECS_PREFIX_()_CS(&g->path), g->cbctx) <= -1) goto oops;
						g->expanded = 1;
					}
				}
			}

			_ecs_prefix_()_setlen (&g->path, tmp);
			hawk_dir_close (dp); dp = HAWK_NULL;
		}
	}

	HAWK_ASSERT (dp == HAWK_NULL);

#if defined(NO_RECURSION)
	if (g->stack)
	{
		/* the stack is not empty. the emulated recusive call
		 * must have been made. restore the variables pushed
		 * and jump to the resumption point */
		r = g->stack;
		g->stack = r->next;

		tmp = r->tmp;
		tmp2 = r->tmp2;
		dp = r->dp;
		*seg = r->seg;

		/* link the stack node to the free list 
		 * instead of freeing it here */
		r->next = g->free;
		g->free = r;

		goto resume;
	}

	while (g->free)
	{
		/* destory the free list */
		r = g->free;
		g->free = r->next;
		hawk_gem_freemem (g->gem, r);
	}
#endif

	return 0;

oops:
	if (dp) hawk_dir_close (dp);

#if defined(NO_RECURSION)
	while (g->stack)
	{
		r = g->stack;
		g->stack = r->next;
		hawk_dir_close (r->dp);
		hawk_gem_freemem (g->gem, r);
	}

	while (g->free)
	{
		r = g->stack;
		g->free = r->next;
		hawk_gem_freemem (g->gem, r);
	}
#endif
	return -1;
}

int _fn_name_ (hawk_gem_t* gem, const _char_type_* pattern, _cb_type_ cbimpl, void* cbctx, int flags)
{
	_g_prefix_()_segment_t seg;
	_g_prefix_()_glob_t g;
	int x;

	HAWK_MEMSET (&g, 0, HAWK_SIZEOF(g));
	g.gem = gem;
	g.cbimpl = cbimpl;
	g.cbctx = cbctx;
	g.flags = flags;

#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
	g.fnmat_flags |= HAWK_FNMAT_IGNORECASE;
	g.fnmat_flags |= HAWK_FNMAT_NOESCAPE;
#else
	if (flags & HAWK_GLOB_IGNORECASE) g.fnmat_flags |= HAWK_FNMAT_IGNORECASE;
	if (flags & HAWK_GLOB_NOESCAPE) g.fnmat_flags |= HAWK_FNMAT_NOESCAPE;
#endif
	if (flags & HAWK_GLOB_PERIOD) g.fnmat_flags |= HAWK_FNMAT_PERIOD;

	if (_ecs_prefix_()_init(&g.path, g.gem, 512) <= -1) return -1;
	if (_ecs_prefix_()_init(&g.tbuf, g.gem, 256) <= -1) 
	{
		_ecs_prefix_()_fini (&g.path);
		return -1;
	}

	if (HAWK_SIZEOF(_char_type_) != HAWK_SIZEOF(hawk_bch_t)) 
	{
		if (hawk_becs_init(&g.mbuf, g.gem, 512) <= -1) 
		{
			_ecs_prefix_()_fini (&g.path);
			_ecs_prefix_()_fini (&g.path);
			return -1;
		}
	}

	HAWK_MEMSET (&seg, 0, HAWK_SIZEOF(seg));
	seg.type = NONE;
	seg.ptr = pattern;
	seg.len = 0;

	x = _g_prefix_()_search(&g, &seg);

	if (HAWK_SIZEOF(_char_type_) != HAWK_SIZEOF(hawk_uch_t)) hawk_becs_fini (&g.mbuf);
	_ecs_prefix_()_fini (&g.tbuf);
	_ecs_prefix_()_fini (&g.path);

	if (x <= -1) return -1;
	return g.expanded;
}

popdef([[_g_prefix_]])dnl
popdef([[_fnmat_]])dnl
popdef([[_path_exists_]])dnl
popdef([[_cb_type_]])dnl
popdef([[_ECS_PREFIX_]])dnl
popdef([[_ecs_prefix_]])dnl
popdef([[_cs_type_]])dnl
popdef([[_char_type_]])dnl
popdef([[_fn_name_]])dnl
]])dnl
