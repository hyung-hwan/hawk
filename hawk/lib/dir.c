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

#include <hawk-dir.h>
#include "hawk-prv.h"

#if defined(_WIN32) 
#	include <windows.h>
#elif defined(__OS2__) 
#	define INCL_DOSFILEMGR
#	define INCL_ERRORS
#	include <os2.h>
#elif defined(__DOS__) 
#	include <dos.h>
#	include <errno.h>
#else
#	include "syscall.h"
#endif


#define STATUS_OPENED    (1 << 0)
#define STATUS_DONE      (1 << 1)
#define STATUS_DONE_ERR  (1 << 2)
#define STATUS_POPHEAP   (1 << 3)
#define STATUS_SORT_ERR  (1 << 4)

#define IS_CURDIR(x) ((x)[0] == '.' && (x)[1] == '\0')
#define IS_PREVDIR(x) ((x)[0] == '.' && (x)[1] == '.' && (x)[2] == '\0')


struct hawk_dir_t
{
	hawk_gem_t* gem;
	int flags;

	hawk_uecs_t wbuf;
	hawk_becs_t mbuf;

	hawk_arr_t* stab;
	int status;

#if defined(_WIN32)
	HANDLE h;
	WIN32_FIND_DATA wfd;
#elif defined(__OS2__)
	HDIR h;
	#if defined(FIL_STANDARDL) 
	FILEFINDBUF3L ffb;
	#else
	FILEFINDBUF3 ffb;
	#endif
	ULONG count;
#elif defined(__DOS__)
	struct find_t f;
#else
	HAWK_DIR* dp;
#endif
};

int hawk_dir_init (hawk_dir_t* dir, hawk_gem_t* gem, const hawk_ooch_t* path, int flags);
void hawk_dir_fini (hawk_dir_t* dir);

static void close_dir_safely (hawk_dir_t* dir);
static int reset_to_path (hawk_dir_t* dir, const hawk_ooch_t* path);
static int read_ahead_and_sort (hawk_dir_t* dir, const hawk_ooch_t* path);

hawk_dir_t* hawk_dir_open (hawk_gem_t* gem, hawk_oow_t xtnsize, const hawk_ooch_t* path, int flags)
{
	hawk_dir_t* dir;

	dir = hawk_gem_allocmem(gem, HAWK_SIZEOF(*dir) + xtnsize);
	if (dir)
	{
		if (hawk_dir_init(dir, gem, path, flags) <= -1)
		{
			hawk_gem_freemem (gem, dir);
			dir = HAWK_NULL;
		}
		else HAWK_MEMSET (dir + 1, 0, xtnsize);
	}

	return dir;
}

void hawk_dir_close (hawk_dir_t* dir)
{
	hawk_dir_fini (dir);
	hawk_gem_freemem (dir->gem, dir);
}

void* hawk_dir_getxtn (hawk_dir_t* dir)
{
	return (void*)(dir + 1);
}

static int compare_dirent (hawk_arr_t* arr, const void* dptr1, hawk_oow_t dlen1, const void* dptr2, hawk_oow_t dlen2)
{
	int n = HAWK_MEMCMP(dptr1, dptr2, ((dlen1 < dlen2)? dlen1: dlen2));
	if (n == 0 && dlen1 != dlen2) n = (dlen1 > dlen2)? 1: -1;
	return -n;
}

int hawk_dir_init (hawk_dir_t* dir, hawk_gem_t* gem, const hawk_ooch_t* path, int flags)
{
	static hawk_arr_style_t stab_style =
	{
		HAWK_ARR_COPIER_INLINE,
		HAWK_ARR_FREEER_DEFAULT,
		compare_dirent,
		HAWK_ARR_KEEPER_DEFAULT,
		HAWK_ARR_SIZER_DEFAULT
	};

	int n;
	int path_flags;

	path_flags = flags & (HAWK_DIR_MBSPATH | HAWK_DIR_WCSPATH);
	if (path_flags == (HAWK_DIR_MBSPATH | HAWK_DIR_WCSPATH) || path_flags == 0)
	{
		/* if both are set or none are set, force it to the default */
	#if defined(HAWK_OOCH_IS_BCH)
		flags |= HAWK_DIR_MBSPATH;
		flags &= ~HAWK_DIR_WCSPATH;
	#else
		flags |= HAWK_DIR_WCSPATH;
		flags &= ~HAWK_DIR_MBSPATH;
	#endif
	}

	HAWK_MEMSET (dir, 0, HAWK_SIZEOF(*dir));

	dir->gem = gem;
	dir->flags = flags;

	if (hawk_uecs_init(&dir->wbuf, gem, 256) <= -1) goto oops_0;
	if (hawk_becs_init(&dir->mbuf, gem, 256) <= -1) goto oops_1;

#if defined(_WIN32)
	dir->h = INVALID_HANDLE_VALUE;
#endif

	n = reset_to_path(dir, path);
	if (n <= -1) goto oops_2;

	if (dir->flags & HAWK_DIR_SORT)
	{
		dir->stab = hawk_arr_open(gem, 0, 128);
		if (HAWK_UNLIKELY(!dir->stab)) goto oops_3;

		/*hawk_arr_setscale (dir->stab, 1);*/
		hawk_arr_setstyle (dir->stab, &stab_style);

		if (read_ahead_and_sort(dir, path) <= -1) goto oops_4;
	}

	return n;

oops_4:
	hawk_arr_close (dir->stab);
oops_3:
	close_dir_safely (dir);
oops_2:
	hawk_becs_fini (&dir->mbuf);
oops_1:
	hawk_uecs_fini (&dir->wbuf);
oops_0:
	return -1;
}

static void close_dir_safely (hawk_dir_t* dir)
{
#if defined(_WIN32)
	if (dir->h != INVALID_HANDLE_VALUE)
	{
		FindClose (dir->h);
		dir->h = INVALID_HANDLE_VALUE;
	}
#elif defined(__OS2__)
	if (dir->status & STATUS_OPENED)
	{
		DosFindClose (dir->h);
		dir->status &= ~STATUS_OPENED;
	}
#elif defined(__DOS__)
	if (dir->status & STATUS_OPENED)
	{
		_dos_findclose (&dir->f);
		dir->status &= ~STATUS_OPENED;
	}
#else
	if (dir->dp)
	{
		HAWK_CLOSEDIR (dir->dp);
		dir->dp = HAWK_NULL;
	}
#endif
}

void hawk_dir_fini (hawk_dir_t* dir)
{
	close_dir_safely (dir);

	hawk_becs_fini (&dir->mbuf);
	hawk_uecs_fini (&dir->wbuf);

	if (dir->stab) hawk_arr_close (dir->stab);
}

static hawk_bch_t* wcs_to_mbuf (hawk_dir_t* dir, const hawk_uch_t* wcs, hawk_becs_t* mbuf)
{
	hawk_becs_clear (mbuf);
	if (hawk_becs_ncatuchars(mbuf, wcs, hawk_count_ucstr(wcs), dir->gem->cmgr) == (hawk_oow_t)-1) return HAWK_NULL;
	return HAWK_BECS_PTR(mbuf);
}

static hawk_uch_t* mbs_to_wbuf (hawk_dir_t* dir, const hawk_bch_t* mbs, hawk_uecs_t* wbuf)
{
	/* convert all regardless of encoding failure */
	hawk_uecs_clear (wbuf);
	if (hawk_uecs_ncatbchars(wbuf, mbs, hawk_count_bcstr(mbs), dir->gem->cmgr, 1) == (hawk_oow_t)-1) return HAWK_NULL;
	return HAWK_UECS_PTR(wbuf);
}

static hawk_uch_t* wcs_to_wbuf (hawk_dir_t* dir, const hawk_uch_t* wcs, hawk_uecs_t* wbuf)
{
	if (hawk_uecs_cpy(&dir->wbuf, wcs) == (hawk_oow_t)-1) return HAWK_NULL;
	return HAWK_UECS_PTR(wbuf);
}

static hawk_bch_t* mbs_to_mbuf (hawk_dir_t* dir, const hawk_bch_t* mbs, hawk_becs_t* mbuf)
{
	if (hawk_becs_cpy(&dir->mbuf, mbs) == (hawk_oow_t)-1) return HAWK_NULL;
	return HAWK_BECS_PTR(mbuf);
}

#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
static hawk_bch_t* make_mbsdos_path (hawk_dir_t* dir, const hawk_bch_t* mpath)
{
	if (mpath[0] == '\0')
	{
		if (hawk_becs_cpy(&dir->mbuf, "*.*") == (hawk_oow_t)-1) return HAWK_NULL;
	}
	else
	{
		hawk_oow_t len;
		if ((len = hawk_becs_cpy(&dir->mbuf, mpath)) == (hawk_oow_t)-1 ||
		    (!HAWK_ISPATHMBSEP(mpath[len - 1]) && 
		     !hawk_ismbsdrivecurpath(mpath) &&
		     hawk_becs_ccat(&dir->mbuf, '\\') == (hawk_oow_t)-1) ||
		    hawk_becs_cat(&dir->mbuf, "*.*") == (hawk_oow_t)-1) return HAWK_NULL;
	}

	return HAWK_BECS_PTR(&dir->mbuf);
}

static hawk_uch_t* make_wcsdos_path (hawk_dir_t* dir, const hawk_uch_t* wpath)
{
	if (wpath[0] == HAWK_UT('\0'))
	{
		if (hawk_uecs_cpy (&dir->wbuf, HAWK_UT("*.*")) == (hawk_oow_t)-1) return HAWK_NULL;
	}
	else
	{
		hawk_oow_t len;
		if ((len = hawk_uecs_cpy (&dir->wbuf, wpath)) == (hawk_oow_t)-1 ||
		    (!HAWK_ISPATHWCSEP(wpath[len - 1]) && 
		     !hawk_iswcsdrivecurpath(wpath) &&
		     hawk_uecs_ccat (&dir->wbuf, HAWK_UT('\\')) == (hawk_oow_t)-1) ||
		    hawk_uecs_cat (&dir->wbuf, HAWK_UT("*.*")) == (hawk_oow_t)-1) return HAWK_NULL;
	}

	return HAWK_UECS_PTR(&dir->wbuf);
}
#endif

/*
static hawk_ooch_t* make_dos_path (hawk_dir_t* dir, const hawk_ooch_t* path)
{
	if (path[0] == HAWK_T('\0'))
	{
		if (hawk_str_cpy (&dir->tbuf, HAWK_T("*.*")) == (hawk_oow_t)-1) 
		{
			dir->errnum = HAWK_DIR_ENOMEM;
			return HAWK_NULL;
		}
	}
	else
	{
		hawk_oow_t len;
		if ((len = hawk_str_cpy (&dir->tbuf, path)) == (hawk_oow_t)-1 ||
		    (!HAWK_ISPATHSEP(path[len - 1]) && 
		     !hawk_isdrivecurpath(path) &&
		     hawk_str_ccat (&dir->tbuf, HAWK_T('\\')) == (hawk_oow_t)-1) ||
		    hawk_str_cat (&dir->tbuf, HAWK_T("*.*")) == (hawk_oow_t)-1)
		{
			dir->errnum = HAWK_DIR_ENOMEM;
			return HAWK_NULL;
		}
	}

	return HAWK_STR_PTR(&dir->tbuf);
}

static hawk_bch_t* mkdospath (hawk_dir_t* dir, const hawk_ooch_t* path)
{

#if defined(HAWK_OOCH_IS_BCH)
	return make_dos_path (dir, path);
#else
	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		return make_mbsdos_path (dir, (const hawk_bch_t*) path);
	}
	else
	{
		hawk_ooch_t* tptr;
		hawk_bch_t* mptr;

		tptr = make_dos_path (dir, path);
		if (tptr == HAWK_NULL) return HAWK_NULL;

		mptr = wcs_to_mbuf (dir, HAWK_STR_PTR(&dir->tbuf), &dir->mbuf);
		if (mptr == HAWK_NULL) return HAWK_NULL;

		return mptr;
	}
#endif

}
*/

static int reset_to_path (hawk_dir_t* dir, const hawk_ooch_t* path)
{
#if defined(_WIN32)
	/* ------------------------------------------------------------------- */
	const hawk_ooch_t* tptr;
	HANDLE dh;
	WIN32_FIND_DATA wfd;

	dir->status &= ~STATUS_DONE;
	dir->status &= ~STATUS_DONE_ERR;

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		hawk_bch_t* mptr;

		mptr = make_mbsdos_path(dir, (const hawk_bch_t*)path);
		if (mptr == HAWK_NULL) return -1;

	#if defined(HAWK_OOCH_IS_BCH)
		tptr = mptr;
	#else
		tptr = mbs_to_wbuf (dir, mptr, &dir->wbuf);
	#endif
	}
	else
	{
		hawk_uch_t* wptr;
		HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);

		wptr = make_wcsdos_path(dir, (const hawk_uch_t*)path);
		if (wptr == HAWK_NULL) return -1;

	#if defined(HAWK_OOCH_IS_BCH)
		tptr = wcs_to_mbuf(dir, wptr, &dir->mbuf);
	#else
		tptr = wptr;
	#endif
	}
	if (tptr == HAWK_NULL) return -1;

	dh = FindFirstFile(tptr, &wfd);
	if (dh == INVALID_HANDLE_VALUE) 
	{
		hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

	close_dir_safely (dir);

	dir->h = dh;
	dir->wfd = wfd;
	return 0;
	/* ------------------------------------------------------------------- */

#elif defined(__OS2__)

	/* ------------------------------------------------------------------- */
	APIRET rc;
	const hawk_bch_t* mptr;
	HDIR h = HDIR_CREATE;
	#if defined(FIL_STANDARDL) 
	FILEFINDBUF3L ffb = { 0 };
	#else
	FILEFINDBUF3 ffb = { 0 };
	#endif
	ULONG count = 1;

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		mptr = make_mbsdos_path (dir, (const hawk_bch_t*)path);
	}
	else
	{
		hawk_uch_t* wptr;
		HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);

		wptr = make_wcsdos_path(dir, (const hawk_uch_t*)path);
		if (wptr == HAWK_NULL) return -1;
		mptr = wcs_to_mbuf(dir, wptr, &dir->mbuf);
	}
	if (mptr == HAWK_NULL) return -1;

	rc = DosFindFirst (
		mptr,
		&h, 
		FILE_DIRECTORY | FILE_READONLY,
		&ffb,
		HAWK_SIZEOF(dir->ffb),
		&count,
	#if defined(FIL_STANDARDL) 
		FIL_STANDARDL
	#else
		FIL_STANDARD
	#endif
	);

	if (rc != NO_ERROR)
	{
		hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

	close_dir_safely (dir);

	dir->h = h;
	dir->ffb = ffb;
	dir->count = count;
	dir->status |= STATUS_OPENED;
	return 0;
	/* ------------------------------------------------------------------- */

#elif defined(__DOS__)

	/* ------------------------------------------------------------------- */
	unsigned int rc;
	const hawk_bch_t* mptr;
	struct find_t f;

	dir->status &= ~STATUS_DONE;
	dir->status &= ~STATUS_DONE_ERR;

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		mptr = make_mbsdos_path(dir, (const hawk_bch_t*)path);
	}
	else
	{
		hawk_uch_t* wptr;

		HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);

		wptr = make_wcsdos_path(dir, (const hawk_uch_t*)path);
		if (wptr == HAWK_NULL) return -1;
		mptr = wcs_to_mbuf(dir, wptr, &dir->mbuf);
	}
	if (mptr == HAWK_NULL) return -1;

	rc = _dos_findfirst(mptr, _A_NORMAL | _A_SUBDIR, &f);
	if (rc != 0) 
	{
		hawk_gem_seterrnum (dir->gem, HAWK_NULL,hawk_syserr_to_errnum(errno));
		return -1;
	}

	close_dir_safely (dir);

	dir->f = f;
	dir->status |= STATUS_OPENED;
	return 0;
	/* ------------------------------------------------------------------- */

#else
	DIR* dp;

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		const hawk_bch_t* mpath = (const hawk_bch_t*)path;
		dp = HAWK_OPENDIR(mpath[0] == '\0'? ".": mpath);
	}
	else
	{
		const hawk_uch_t* wpath;
		/*HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);*/

		wpath = (const hawk_uch_t*)path;
		if (wpath[0] == '\0')
		{
			dp = HAWK_OPENDIR(".");
		}
		else
		{
			hawk_bch_t* mptr;

			mptr = wcs_to_mbuf(dir, wpath, &dir->mbuf);
			if (mptr == HAWK_NULL) return -1;

			dp = HAWK_OPENDIR(mptr);
		}
	}

	if (dp == HAWK_NULL) 
	{
		hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}

	close_dir_safely (dir);

	dir->dp = dp;
	return 0;
#endif 
}

int hawk_dir_reset (hawk_dir_t* dir, const hawk_ooch_t* path)
{
	if (reset_to_path(dir, path) <= -1) return -1;

	if (dir->flags & HAWK_DIR_SORT)
	{
		hawk_arr_clear (dir->stab);
		if (read_ahead_and_sort(dir, path) <= -1) 
		{
			dir->status |= STATUS_SORT_ERR;
			return -1;
		}
		else
		{
			dir->status &= ~STATUS_SORT_ERR;
		}
	}

	return 0;
}

static int read_dir_to_buf (hawk_dir_t* dir, void** name)
{
#if defined(_WIN32)

	/* ------------------------------------------------------------------- */
	if (dir->status & STATUS_DONE) return (dir->status & STATUS_DONE_ERR)? -1: 0;

	if (dir->flags & HAWK_DIR_SKIPSPCDIR)
	{
		/* skip . and .. */
		while (IS_CURDIR(dir->wfd.cFileName) || IS_PREVDIR(dir->wfd.cFileName))
		{
			if (FindNextFile(dir->h, &dir->wfd) == FALSE) 
			{
				DWORD x = GetLastError();
				if (x == ERROR_NO_MORE_FILES) 
				{
					dir->status |= STATUS_DONE;
					return 0;
				}
				else
				{
					hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(x));
					dir->status |= STATUS_DONE;
					dir->status |= STATUS_DONE_ERR;
					return -1;
				}
			}
		}
	}

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
	#if defined(HAWK_OOCH_IS_BCH)
		if (mbs_to_mbuf(dir, dir->wfd.cFileName, &dir->mbuf) == HAWK_NULL) return -1;
	#else
		if (wcs_to_mbuf(dir, dir->wfd.cFileName, &dir->mbuf) == HAWK_NULL) return -1;
	#endif
		*name = HAWK_BECS_PTR(&dir->mbuf);
	}
	else
	{
		HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);
	#if defined(HAWK_OOCH_IS_BCH)
		if (mbs_to_wbuf(dir, dir->wfd.cFileName, &dir->wbuf) == HAWK_NULL) return -1;
	#else
		if (wcs_to_wbuf(dir, dir->wfd.cFileName, &dir->wbuf) == HAWK_NULL) return -1;
	#endif
		*name = HAWK_UECS_PTR(&dir->wbuf);
	}

	if (FindNextFile (dir->h, &dir->wfd) == FALSE) 
	{
		DWORD x = GetLastError();
		if (x == ERROR_NO_MORE_FILES) dir->status |= STATUS_DONE;
		else
		{
			hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(x));
			dir->status |= STATUS_DONE;
			dir->status |= STATUS_DONE_ERR;
		}
	}

	return 1;
	/* ------------------------------------------------------------------- */

#elif defined(__OS2__)

	/* ------------------------------------------------------------------- */
	APIRET rc;

	if (dir->count <= 0) return 0;

	if (dir->flags & HAWK_DIR_SKIPSPCDIR)
	{
		/* skip . and .. */
		while (IS_CURDIR(dir->ffb.achName) || IS_PREVDIR(dir->ffb.achName))
		{
			rc = DosFindNext (dir->h, &dir->ffb, HAWK_SIZEOF(dir->ffb), &dir->count);
			if (rc == ERROR_NO_MORE_FILES) 
			{
				dir->count = 0;
				return 0;
			}
			else if (rc != NO_ERROR)
			{
				hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
				return -1;
			}
		}
	}

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		if (mbs_to_mbuf (dir, dir->ffb.achName, &dir->mbuf) == HAWK_NULL) return -1;
		*name = HAWK_BECS_PTR(&dir->mbuf);
	}
	else
	{
		HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);
		if (mbs_to_wbuf (dir, dir->ffb.achName, &dir->wbuf) == HAWK_NULL) return -1;
		*name = HAWK_UECS_PTR(&dir->wbuf);
	}
	

	rc = DosFindNext (dir->h, &dir->ffb, HAWK_SIZEOF(dir->ffb), &dir->count);
	if (rc == ERROR_NO_MORE_FILES) dir->count = 0;
	else if (rc != NO_ERROR)
	{
		hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

	return 1;
	/* ------------------------------------------------------------------- */

#elif defined(__DOS__)

	/* ------------------------------------------------------------------- */

	if (dir->status & STATUS_DONE) return (dir->status & STATUS_DONE_ERR)? -1: 0;

	if (dir->flags & HAWK_DIR_SKIPSPCDIR)
	{
		/* skip . and .. */
		while (IS_CURDIR(dir->f.name) || IS_PREVDIR(dir->f.name))
		{
			if (_dos_findnext (&dir->f) != 0)
			{
				if (errno == ENOENT) 
				{
					dir->status |= STATUS_DONE;
					return 0;
				}
				else
				{
					hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
					dir->status |= STATUS_DONE;
					dir->status |= STATUS_DONE_ERR;
					return -1;
				}
			}
		}
	}

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		if (mbs_to_mbuf (dir, dir->f.name, &dir->mbuf) == HAWK_NULL) return -1;
		*name = HAWK_BECS_PTR(&dir->mbuf);
	}
	else
	{
		HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);

		if (mbs_to_wbuf (dir, dir->f.name, &dir->wbuf) == HAWK_NULL) return -1;
		*name = HAWK_UECS_PTR(&dir->wbuf);
	}

	if (_dos_findnext (&dir->f) != 0)
	{
		if (errno == ENOENT) dir->status |= STATUS_DONE;
		else
		{
			hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			dir->status |= STATUS_DONE;
			dir->status |= STATUS_DONE_ERR;
		}
	}

	return 1;
	/* ------------------------------------------------------------------- */

#else

	/* ------------------------------------------------------------------- */
	hawk_dirent_t* de;

read:
	errno = 0;
	de = HAWK_READDIR(dir->dp);
	if (de == NULL) 
	{
		if (errno == 0) return 0;
		hawk_gem_seterrnum (dir->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}

	if (dir->flags & HAWK_DIR_SKIPSPCDIR)
	{
		/* skip . and .. */
		if (IS_CURDIR(de->d_name) || 
		    IS_PREVDIR(de->d_name)) goto read;
	}

	if (dir->flags & HAWK_DIR_MBSPATH)
	{
		if (mbs_to_mbuf(dir, de->d_name, &dir->mbuf) == HAWK_NULL) return -1;
		*name = HAWK_BECS_PTR(&dir->mbuf);
	}
	else
	{
		/*HAWK_ASSERT (dir->flags & HAWK_DIR_WCSPATH);*/
		if (mbs_to_wbuf(dir, de->d_name, &dir->wbuf) == HAWK_NULL) return -1;
		*name = HAWK_UECS_PTR(&dir->wbuf);
	}

	return 1;
	/* ------------------------------------------------------------------- */

#endif
}

static int read_ahead_and_sort (hawk_dir_t* dir, const hawk_ooch_t* path)
{
	int x;
	void* name;

	while (1)
	{
		x = read_dir_to_buf (dir, &name);
		if (x >= 1)
		{
			hawk_oow_t size;

			if (dir->flags & HAWK_DIR_MBSPATH)
				size = (hawk_count_bcstr(name) + 1) * HAWK_SIZEOF(hawk_bch_t);
			else
				size = (hawk_count_ucstr(name) + 1) * HAWK_SIZEOF(hawk_uch_t);

			if (hawk_arr_pushheap(dir->stab, name, size) == (hawk_oow_t)-1) return -1;
		}
		else if (x == 0) break;
		else return -1;
	}

	dir->status &= ~STATUS_POPHEAP;
	return 0;
}


int hawk_dir_read (hawk_dir_t* dir, hawk_dir_ent_t* ent)
{
	if (dir->flags & HAWK_DIR_SORT)
	{
		if (dir->status & STATUS_SORT_ERR) 
		{
			hawk_gem_seterrnum (dir->gem, HAWK_NULL, HAWK_ESTATE);
			return -1;
		}

		if (dir->status & STATUS_POPHEAP) hawk_arr_popheap (dir->stab);
		else dir->status |= STATUS_POPHEAP;

		if (HAWK_ARR_SIZE(dir->stab) <= 0) return 0; /* no more entry */

		ent->name = HAWK_ARR_DPTR(dir->stab, 0);
		return 1;
	}
	else
	{
		int x;
		void* name;

		x = read_dir_to_buf(dir, &name);
		if (x >= 1)
		{
			HAWK_MEMSET (ent, 0, HAWK_SIZEOF(*ent));
			ent->name = name;
		}

		return x;
	}
}
