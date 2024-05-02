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

/*
 * Do NOT edit glob.c. Edit glob.c.m4 instead.
 *
 * Generate gem-glob.c with m4
 *   $ m4 gem-glob.c.m4 > gem-glob.c
 */

#include "hawk-prv.h"
#include <hawk-chr.h>
#include <hawk-ecs.h>
#include <hawk-dir.h>
#include <hawk-glob.h>

#include "syscall.h"

#define NO_RECURSION 1

enum segment_type_t
{
        NONE,
        ROOT,
        NORMAL
};
typedef enum segment_type_t segment_type_t;


#define IS_ESC(c) HAWK_FNMAT_IS_ESC(c)
#define IS_SEP(c) HAWK_FNMAT_IS_SEP(c)
#define IS_NIL(c) ((c) == '\0')
#define IS_SEP_OR_NIL(c) (IS_SEP(c) || IS_NIL(c))

/* this macro only checks for top-level wild-cards among these.
 *  *, ?, [], !, -
 * see str-fnmat.c for more wild-card letters
 */
#define IS_WILD(c) ((c) == '*' || (c) == '?' || (c) == '[')


static hawk_bch_t* wcs_to_mbuf (hawk_gem_t* g, const hawk_uch_t* wcs, hawk_becs_t* mbuf)
{
	hawk_oow_t ml, wl;

	if (hawk_gem_convutobcstr(g, wcs, &wl, HAWK_NULL, &ml) <= -1 ||
	    hawk_becs_setlen(mbuf, ml) == (hawk_oow_t)-1) return HAWK_NULL;

	hawk_gem_convutobcstr (g, wcs, &wl, HAWK_BECS_PTR(mbuf), &ml);
	return HAWK_BECS_PTR(mbuf);
}

#if defined(_WIN32) && !defined(INVALID_FILE_ATTRIBUTES)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

static int upath_exists (hawk_gem_t* g, const hawk_uch_t* name, hawk_becs_t* mbuf)
{
#if defined(_WIN32)
	return (GetFileAttributesW(name) != INVALID_FILE_ATTRIBUTES)? 1: 0;

#elif defined(__OS2__)
	FILESTATUS3 fs;
	APIRET rc;
	const hawk_bch_t* mptr;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	rc = DosQueryPathInfo(mptr, FIL_STANDARD, &fs, HAWK_SIZEOF(fs));
	return (rc == NO_ERROR)? 1: ((rc == ERROR_PATH_NOT_FOUND)? 0: -1);

#elif defined(__DOS__)
	unsigned int x, attr;
	const hawk_bch_t* mptr;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	x = _dos_getfileattr (mptr, &attr);
	return (x == 0)? 1: ((errno == ENOENT)? 0: -1);

#elif defined(macintosh)
	HFileInfo fpb;
	const hawk_bch_t* mptr;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	HAWK_MEMSET (&fpb, 0, HAWK_SIZEOF(fpb));
	fpb.ioNamePtr = (unsigned char*)mptr;

	return (PBGetCatInfoSync ((CInfoPBRec*)&fpb) == noErr)? 1: 0;
#else
	struct stat st;
	const hawk_bch_t* mptr;
	int t;

	mptr = wcs_to_mbuf(g, name, mbuf);
	if (HAWK_UNLIKELY(!mptr)) return -1;

	#if defined(HAVE_LSTAT)
	t = HAWK_LSTAT(mptr, &st);
	#else
	t = HAWK_STAT(mptr, &st);/* use stat() if no lstat() is available. */
	#endif
	return (t == 0);
#endif
}

static int bpath_exists (hawk_gem_t* g, const hawk_bch_t* name, hawk_becs_t* mbuf)
{
#if defined(_WIN32)
	return (GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES)? 1: 0;
#elif defined(__OS2__)

	FILESTATUS3 fs;
	APIRET rc;
	rc = DosQueryPathInfo(name, FIL_STANDARD, &fs, HAWK_SIZEOF(fs));
	return (rc == NO_ERROR)? 1: ((rc == ERROR_PATH_NOT_FOUND)? 0: -1);

#elif defined(__DOS__)
	unsigned int x, attr;
	x = _dos_getfileattr(name, &attr);
	return (x == 0)? 1: ((errno == ENOENT)? 0: -1);

#elif defined(macintosh)
	HFileInfo fpb;
	HAWK_MEMSET (&fpb, 0, HAWK_SIZEOF(fpb));
	fpb.ioNamePtr = (unsigned char*)name;
	return (PBGetCatInfoSync((CInfoPBRec*)&fpb) == noErr)? 1: 0;
#else
	struct stat st;
	int t;
	#if defined(HAVE_LSTAT)
	t = HAWK_LSTAT(name, &st);
	#else
	t = HAWK_STAT(name, &st); /* use stat() if no lstat() is available. */
	#endif
	return (t == 0);
#endif
}




#if defined(NO_RECURSION)
typedef struct __u_stack_node_t __u_stack_node_t;
#endif

struct __u_glob_t
{
	hawk_gem_uglob_cb_t cbimpl;
	void* cbctx;

	hawk_gem_t* gem;
	int flags;

	hawk_uecs_t path;
	hawk_uecs_t tbuf; /* temporary buffer */

	hawk_becs_t mbuf; /* not used if the base character type is hawk_bch_t */

	int expanded;
	int fnmat_flags;

#if defined(NO_RECURSION)
	__u_stack_node_t* stack;
	__u_stack_node_t* free;
#endif
};

typedef struct __u_glob_t __u_glob_t;

struct __u_segment_t
{
	segment_type_t type;

	const hawk_uch_t* ptr;
	hawk_oow_t    len;

	hawk_uch_t sep; /* preceeding separator */
	unsigned int wild: 1;  /* indicate that it contains wildcards */
	unsigned int esc: 1;  /* indicate that it contains escaped letters */
	unsigned int next: 1;  /* indicate that it has the following segment */
};
typedef struct __u_segment_t __u_segment_t;

#if defined(NO_RECURSION)
struct __u_stack_node_t
{
	hawk_oow_t tmp;
	hawk_oow_t tmp2;
	hawk_dir_t* dp;
	__u_segment_t seg;
	__u_stack_node_t* next;
};
#endif

static int __u_get_next_segment (__u_glob_t* g, __u_segment_t* seg)
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

static int __u_handle_non_wild_segments (__u_glob_t* g, __u_segment_t* seg)
{
	while (__u_get_next_segment(g, seg) != NONE && !seg->wild)
	{
		HAWK_ASSERT (seg->type != NONE && !seg->wild);

		if (seg->sep && hawk_uecs_ccat (&g->path, seg->sep) == (hawk_oow_t)-1) return -1;
		if (seg->esc)
		{
			/* if the segment contains escape sequences,
			 * strip the escape letters off the segment */


			hawk_ucs_t tmp;
			hawk_oow_t i;
			int escaped = 0;

			if (HAWK_UECS_CAPA(&g->tbuf) < seg->len &&
			    hawk_uecs_setcapa (&g->tbuf, seg->len) == (hawk_oow_t)-1) return -1;

			tmp.ptr = HAWK_UECS_PTR(&g->tbuf);
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

			if (hawk_uecs_ncat (&g->path, tmp.ptr, tmp.len) == (hawk_oow_t)-1) return -1;
		}
		else
		{
			/* if the segment doesn't contain escape sequences,
			 * append the segment to the path without special handling */
			if (hawk_uecs_ncat (&g->path, seg->ptr, seg->len) == (hawk_oow_t)-1) return -1;
		}

		if (!seg->next && upath_exists(g->gem, HAWK_UECS_PTR(&g->path), &g->mbuf) > 0)
		{
			/* reached the last segment. match if the path exists */
			if (g->cbimpl(HAWK_UECS_CS(&g->path), g->cbctx) <= -1) return -1;
			g->expanded = 1;
		}
	}

	return 0;
}

static int __u_search (__u_glob_t* g, __u_segment_t* seg)
{
	hawk_dir_t* dp;
	hawk_oow_t tmp, tmp2;
	hawk_dir_ent_t ent;
	int x;

#if defined(NO_RECURSION)
	__u_stack_node_t* r;

entry:
#endif

	dp = HAWK_NULL;

	if (__u_handle_non_wild_segments(g, seg) <= -1) goto oops;

	if (seg->wild)
	{
		int dir_flags = 0;
		if (g->flags & HAWK_GLOB_SKIPSPCDIR) dir_flags |= HAWK_DIR_SKIPSPCDIR;
		if (HAWK_SIZEOF(hawk_uch_t) == HAWK_SIZEOF(hawk_bch_t)) dir_flags |= HAWK_DIR_BPATH;

		dp = hawk_dir_open(g->gem, 0, (const hawk_uch_t*)HAWK_UECS_PTR(&g->path), dir_flags);
		if (dp)
		{
			tmp = HAWK_UECS_LEN(&g->path);

			if (seg->sep && hawk_uecs_ccat(&g->path, seg->sep) == (hawk_oow_t)-1) goto oops;
			tmp2 = HAWK_UECS_LEN(&g->path);

			while (1)
			{
				hawk_uecs_setlen (&g->path, tmp2);

				x = hawk_dir_read(dp, &ent);
				if (x <= -1)
				{
					if (g->flags & HAWK_GLOB_TOLERANT) break;
					else goto oops;
				}
				if (x == 0) break;

				if (hawk_uecs_cat(&g->path, (const hawk_uch_t*)ent.name) == (hawk_oow_t)-1) goto oops;

				if (hawk_fnmat_ucstr_uchars(HAWK_UECS_CPTR(&g->path,tmp2), seg->ptr, seg->len, g->fnmat_flags) > 0)
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
						__u_segment_t save;
						int x;

						save = *seg;
						x = __u_search(g, seg);
						*seg = save;
						if (x <= -1) goto oops;
				#endif
					}
					else
					{
						if (g->cbimpl(HAWK_UECS_CS(&g->path), g->cbctx) <= -1) goto oops;
						g->expanded = 1;
					}
				}
			}

			hawk_uecs_setlen (&g->path, tmp);
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

int hawk_gem_uglob (hawk_gem_t* gem, const hawk_uch_t* pattern, hawk_gem_uglob_cb_t cbimpl, void* cbctx, int flags)
{
	__u_segment_t seg;
	__u_glob_t g;
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

	if (hawk_uecs_init(&g.path, g.gem, 512) <= -1) return -1;
	if (hawk_uecs_init(&g.tbuf, g.gem, 256) <= -1)
	{
		hawk_uecs_fini (&g.path);
		return -1;
	}

	if (HAWK_SIZEOF(hawk_uch_t) != HAWK_SIZEOF(hawk_bch_t))
	{
		if (hawk_becs_init(&g.mbuf, g.gem, 512) <= -1)
		{
			hawk_uecs_fini (&g.path);
			hawk_uecs_fini (&g.path);
			return -1;
		}
	}

	HAWK_MEMSET (&seg, 0, HAWK_SIZEOF(seg));
	seg.type = NONE;
	seg.ptr = pattern;
	seg.len = 0;

	x = __u_search(&g, &seg);

	if (HAWK_SIZEOF(hawk_uch_t) != HAWK_SIZEOF(hawk_uch_t)) hawk_becs_fini (&g.mbuf);
	hawk_uecs_fini (&g.tbuf);
	hawk_uecs_fini (&g.path);

	if (x <= -1) return -1;
	return g.expanded;
}




#if defined(NO_RECURSION)
typedef struct __b_stack_node_t __b_stack_node_t;
#endif

struct __b_glob_t
{
	hawk_gem_bglob_cb_t cbimpl;
	void* cbctx;

	hawk_gem_t* gem;
	int flags;

	hawk_becs_t path;
	hawk_becs_t tbuf; /* temporary buffer */

	hawk_becs_t mbuf; /* not used if the base character type is hawk_bch_t */

	int expanded;
	int fnmat_flags;

#if defined(NO_RECURSION)
	__b_stack_node_t* stack;
	__b_stack_node_t* free;
#endif
};

typedef struct __b_glob_t __b_glob_t;

struct __b_segment_t
{
	segment_type_t type;

	const hawk_bch_t* ptr;
	hawk_oow_t    len;

	hawk_bch_t sep; /* preceeding separator */
	unsigned int wild: 1;  /* indicate that it contains wildcards */
	unsigned int esc: 1;  /* indicate that it contains escaped letters */
	unsigned int next: 1;  /* indicate that it has the following segment */
};
typedef struct __b_segment_t __b_segment_t;

#if defined(NO_RECURSION)
struct __b_stack_node_t
{
	hawk_oow_t tmp;
	hawk_oow_t tmp2;
	hawk_dir_t* dp;
	__b_segment_t seg;
	__b_stack_node_t* next;
};
#endif

static int __b_get_next_segment (__b_glob_t* g, __b_segment_t* seg)
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

static int __b_handle_non_wild_segments (__b_glob_t* g, __b_segment_t* seg)
{
	while (__b_get_next_segment(g, seg) != NONE && !seg->wild)
	{
		HAWK_ASSERT (seg->type != NONE && !seg->wild);

		if (seg->sep && hawk_becs_ccat (&g->path, seg->sep) == (hawk_oow_t)-1) return -1;
		if (seg->esc)
		{
			/* if the segment contains escape sequences,
			 * strip the escape letters off the segment */


			hawk_bcs_t tmp;
			hawk_oow_t i;
			int escaped = 0;

			if (HAWK_BECS_CAPA(&g->tbuf) < seg->len &&
			    hawk_becs_setcapa (&g->tbuf, seg->len) == (hawk_oow_t)-1) return -1;

			tmp.ptr = HAWK_BECS_PTR(&g->tbuf);
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

			if (hawk_becs_ncat (&g->path, tmp.ptr, tmp.len) == (hawk_oow_t)-1) return -1;
		}
		else
		{
			/* if the segment doesn't contain escape sequences,
			 * append the segment to the path without special handling */
			if (hawk_becs_ncat (&g->path, seg->ptr, seg->len) == (hawk_oow_t)-1) return -1;
		}

		if (!seg->next && bpath_exists(g->gem, HAWK_BECS_PTR(&g->path), &g->mbuf) > 0)
		{
			/* reached the last segment. match if the path exists */
			if (g->cbimpl(HAWK_BECS_CS(&g->path), g->cbctx) <= -1) return -1;
			g->expanded = 1;
		}
	}

	return 0;
}

static int __b_search (__b_glob_t* g, __b_segment_t* seg)
{
	hawk_dir_t* dp;
	hawk_oow_t tmp, tmp2;
	hawk_dir_ent_t ent;
	int x;

#if defined(NO_RECURSION)
	__b_stack_node_t* r;

entry:
#endif

	dp = HAWK_NULL;

	if (__b_handle_non_wild_segments(g, seg) <= -1) goto oops;

	if (seg->wild)
	{
		int dir_flags = 0;
		if (g->flags & HAWK_GLOB_SKIPSPCDIR) dir_flags |= HAWK_DIR_SKIPSPCDIR;
		if (HAWK_SIZEOF(hawk_bch_t) == HAWK_SIZEOF(hawk_bch_t)) dir_flags |= HAWK_DIR_BPATH;

		dp = hawk_dir_open(g->gem, 0, (const hawk_bch_t*)HAWK_BECS_PTR(&g->path), dir_flags);
		if (dp)
		{
			tmp = HAWK_BECS_LEN(&g->path);

			if (seg->sep && hawk_becs_ccat(&g->path, seg->sep) == (hawk_oow_t)-1) goto oops;
			tmp2 = HAWK_BECS_LEN(&g->path);

			while (1)
			{
				hawk_becs_setlen (&g->path, tmp2);

				x = hawk_dir_read(dp, &ent);
				if (x <= -1)
				{
					if (g->flags & HAWK_GLOB_TOLERANT) break;
					else goto oops;
				}
				if (x == 0) break;

				if (hawk_becs_cat(&g->path, (const hawk_bch_t*)ent.name) == (hawk_oow_t)-1) goto oops;

				if (hawk_fnmat_bcstr_bchars(HAWK_BECS_CPTR(&g->path,tmp2), seg->ptr, seg->len, g->fnmat_flags) > 0)
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
						__b_segment_t save;
						int x;

						save = *seg;
						x = __b_search(g, seg);
						*seg = save;
						if (x <= -1) goto oops;
				#endif
					}
					else
					{
						if (g->cbimpl(HAWK_BECS_CS(&g->path), g->cbctx) <= -1) goto oops;
						g->expanded = 1;
					}
				}
			}

			hawk_becs_setlen (&g->path, tmp);
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

int hawk_gem_bglob (hawk_gem_t* gem, const hawk_bch_t* pattern, hawk_gem_bglob_cb_t cbimpl, void* cbctx, int flags)
{
	__b_segment_t seg;
	__b_glob_t g;
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

	if (hawk_becs_init(&g.path, g.gem, 512) <= -1) return -1;
	if (hawk_becs_init(&g.tbuf, g.gem, 256) <= -1)
	{
		hawk_becs_fini (&g.path);
		return -1;
	}

	if (HAWK_SIZEOF(hawk_bch_t) != HAWK_SIZEOF(hawk_bch_t))
	{
		if (hawk_becs_init(&g.mbuf, g.gem, 512) <= -1)
		{
			hawk_becs_fini (&g.path);
			hawk_becs_fini (&g.path);
			return -1;
		}
	}

	HAWK_MEMSET (&seg, 0, HAWK_SIZEOF(seg));
	seg.type = NONE;
	seg.ptr = pattern;
	seg.len = 0;

	x = __b_search(&g, &seg);

	if (HAWK_SIZEOF(hawk_bch_t) != HAWK_SIZEOF(hawk_uch_t)) hawk_becs_fini (&g.mbuf);
	hawk_becs_fini (&g.tbuf);
	hawk_becs_fini (&g.path);

	if (x <= -1) return -1;
	return g.expanded;
}


