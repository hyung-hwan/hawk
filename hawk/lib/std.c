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

#include "hawk-prv.h"
#include "std-prv.h"

#include <hawk-pio.h>
#include <hawk-sio.h>

#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#if defined(HAVE_QUADMATH_H)
#	include <quadmath.h>
#endif

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	if defined(HAWK_HAVE_CONFIG_H) && defined(HAWK_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	endif
#elif defined(__OS2__)
#	define INCL_DOSMODULEMGR
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__DOS__)
	/* nothing to include */
#else
#	include "syscall.h"
#	if defined(HAWK_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	elif defined(HAVE_DLFCN_H)
#		include <dlfcn.h>
#		define USE_DLFCN
#	else
#		error UNSUPPORTED DYNAMIC LINKER
#	endif
#endif

#if !defined(HAWK_HAVE_CONFIG_H)
#	if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#		define HAVE_POW
#		define HAVE_FMOD
#	endif
#endif

typedef struct xtn_t
{
	struct
	{
		struct 
		{
			hawk_parsestd_t* x;
			hawk_oow_t       xindex;

			union
			{
				/* nothing to maintain here for file */

				struct 
				{
					const hawk_ooch_t* ptr;
					const hawk_ooch_t* end;
				} oocs;
				struct
				{
					const hawk_bch_t* ptr;
					const hawk_bch_t* end;
				} bcs;
				struct
				{
					const hawk_uch_t* ptr;
					const hawk_uch_t* end;
				} ucs;
			} u;
		} in;

		struct
		{
			hawk_parsestd_t* x;
			union
			{
				struct
				{
					hawk_sio_t* sio;
				} file;
				struct 
				{
					hawk_ooecs_t* buf;
				} oocs;
				struct 
				{
					hawk_becs_t* buf;
				} bcs;
				struct 
				{
					hawk_uecs_t* buf;
				} ucs;
			} u;
		} out;
	} s; /* script/source handling */

	int gbl_argc;
	int gbl_argv;
	int gbl_environ;

	hawk_ecb_t ecb;
	int stdmod_up;
} xtn_t;

typedef struct rxtn_t
{
	struct
	{
		struct {
			const hawk_ooch_t*const* files;
			hawk_oow_t index;
			hawk_oow_t count;
		} in; 

		struct 
		{
			const hawk_ooch_t*const* files;
			hawk_oow_t index;
			hawk_oow_t count;
		} out;

		hawk_cmgr_t* cmgr;
	} c;  /* console */

	int cmgrtab_inited;
	hawk_htb_t cmgrtab;

	hawk_rtx_ecb_t ecb;
} rxtn_t;

typedef struct ioattr_t
{
	hawk_cmgr_t* cmgr;
	hawk_ooch_t cmgr_name[64]; /* i assume that the cmgr name never exceeds this length */
	hawk_ntime_t tmout[4];
} ioattr_t;

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE xtn_t* GET_XTN(hawk_t* awk) { return (xtn_t*)((hawk_uint8_t*)hawk_getxtn(awk) - HAWK_SIZEOF(xtn_t)); }
static HAWK_INLINE rxtn_t* GET_RXTN(hawk_rtx_t* rtx) { return (rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)); }
#else
#define GET_XTN(awk) ((xtn_t*)((hawk_uint8_t*)hawk_getxtn(awk) - HAWK_SIZEOF(xtn_t)))
#define GET_RXTN(rtx) ((rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)))
#endif


/* ========================================================================= */

static void* sys_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	return malloc(size);
}

static void* sys_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	return realloc(ptr, size);
}

static void sys_free (hawk_mmgr_t* mmgr, void* ptr)
{
	free (ptr);
}

static hawk_mmgr_t sys_mmgr =
{
	sys_alloc,
	sys_realloc,
	sys_free,
	HAWK_NULL
};

/* ========================================================================= */

static ioattr_t* get_ioattr (hawk_htb_t* tab, const hawk_ooch_t* ptr, hawk_oow_t len);

hawk_flt_t hawk_stdmathpow (hawk_t* awk, hawk_flt_t x, hawk_flt_t y)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_POWQ)
	return powq(x, y);
#elif defined(HAVE_POWL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return powl(x, y);
#elif defined(HAVE_POW)
	return pow(x, y);
#elif defined(HAVE_POWF)
	return powf(x, y);
#else
	#error ### no pow function available ###
#endif
}

hawk_flt_t hawk_stdmathmod (hawk_t* awk, hawk_flt_t x, hawk_flt_t y)
{
#if defined(HAWK_USE_AWK_FLTMAX) && defined(HAVE_FMODQ)
	return fmodq(x, y);
#elif defined(HAVE_FMODL) && (HAWK_SIZEOF_LONG_DOUBLE > HAWK_SIZEOF_DOUBLE)
	return fmodl(x, y);
#elif defined(HAVE_FMOD)
	return fmod(x, y);
#elif defined(HAVE_FMODF)
	return fmodf(x, y);
#else
	#error ### no fmod function available ###
#endif
}

/* [IMPORTANT]
 * hawk_stdmodXXXX() functions must not access the extension
 * area of 'awk'. they are used in StdAwk.cpp which instantiates
 * an awk object with hawk_open() instead of hawk_openstd(). */

int hawk_stdmodstartup (hawk_t* awk)
{
#if defined(USE_LTDL)

	/* lt_dlinit() can be called more than once and 
	 * lt_dlexit() shuts down libltdl if it's called as many times as
	 * corresponding lt_dlinit(). so it's safe to call lt_dlinit()
	 * and lt_dlexit() at the library level. */
	return lt_dlinit() != 0? -1: 0;

#else
	return 0;
#endif

}

void hawk_stdmodshutdown (hawk_t* awk)
{
#if defined(USE_LTDL)
	lt_dlexit ();
#else
	/* do nothing */
#endif
}

static void* std_mod_open_checked (hawk_t* awk, const hawk_mod_spec_t* spec)
{
	xtn_t* xtn = GET_XTN(awk);

	if (!xtn->stdmod_up)
	{
		/* hawk_stdmodstartup() must have failed upon start-up.
		 * return failure immediately */
		hawk_seterrnum (awk, HAWK_ENOIMPL, HAWK_NULL);
		return HAWK_NULL;
	}

	return hawk_stdmodopen(awk, spec);
}

void* hawk_stdmodopen (hawk_t* awk, const hawk_mod_spec_t* spec)
{
#if defined(USE_LTDL)
	void* h;
	lt_dladvise adv;
	hawk_bch_t* modpath;
	const hawk_ooch_t* tmp[4];
	int count;

	count = 0;
	if (spec->prefix) tmp[count++] = spec->prefix;
	tmp[count++] = spec->name;
	if (spec->postfix) tmp[count++] = spec->postfix;
	tmp[count] = HAWK_NULL;

	#if defined(HAWK_OOCH_IS_BCH)
	modpath = hawk_dupbcstrarr(awk, tmp, HAWK_NULL);
	#else
	modpath = hawk_dupucstrarrtobcstr(awk, tmp, HAWK_NULL)
	#endif
	if (!modpath) return HAWK_NULL;

	if (lt_dladvise_init(&adv) != 0)
	{
		/* the only failure of lt_dladvise_init() seems to be caused
		 * by memory allocation failured */
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		return HAWK_NULL;
	}

	lt_dladvise_ext (&adv);
	/*lt_dladvise_resident (&adv); useful for debugging with valgrind */

	h = lt_dlopenadvise (modpath, adv);

	lt_dladvise_destroy (&adv);

	HAWK_MMGR_FREE (hawk_getmmgr(awk), modpath);

	return h;

#elif defined(_WIN32)

	HMODULE h;
	hawk_ooch_t* modpath;
	const hawk_ooch_t* tmp[4];
	int count;

	count = 0;
	if (spec->prefix) tmp[count++] = spec->prefix;
	tmp[count++] = spec->name;
	if (spec->postfix) tmp[count++] = spec->postfix;
	tmp[count] = HAWK_NULL;

	modpath = hawk_stradup (tmp, HAWK_NULL, hawk_getmmgr(awk));
	if (!modpath)
	{
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		return HAWK_NULL;
	}

	h = LoadLibrary (modpath);

	hawk_freemem (awk, modpath);

	HAWK_ASSERT (awk, HAWK_SIZEOF(h) <= HAWK_SIZEOF(void*));
	return h;

#elif defined(__OS2__)

	HMODULE h;
	hawk_bch_t* modpath;
	const hawk_ooch_t* tmp[4];
	int count;
	char errbuf[CCHMAXPATH];
	APIRET rc;

	count = 0;
	if (spec->prefix) tmp[count++] = spec->prefix;
	tmp[count++] = spec->name;
	if (spec->postfix) tmp[count++] = spec->postfix;
	tmp[count] = HAWK_NULL;

	#if defined(HAWK_OOCH_IS_BCH)
	modpath = hawk_dupbcstrarr(awk, tmp, HAWK_NULL);
	#else
	modpath = hawk_dupucstrarrtobcstr(awk, tmp, HAWK_NULL);
	#endif
	if (!modpath) return HAWK_NULL;

	/* DosLoadModule() seems to have severe limitation on 
	 * the file name it can load (max-8-letters.xxx) */
	rc = DosLoadModule (errbuf, HAWK_COUNTOF(errbuf) - 1, modpath, &h);
	if (rc != NO_ERROR) h = HAWK_NULL;

	hawk_freemem (awk, modpath);

	HAWK_ASSERT (awk, HAWK_SIZEOF(h) <= HAWK_SIZEOF(void*));
	return h;

#elif defined(__DOS__) && defined(HAWK_ENABLE_DOS_DYNAMIC_MODULE)

	/* the DOS code here is not generic enough. it's for a specific
	 * dos-extender only. the best is not to use dynamic loading
	 * when building for DOS. */
	void* h;
	hawk_bch_t* modpath;
	const hawk_ooch_t* tmp[4];
	int count;

	count = 0;
	if (spec->prefix) tmp[count++] = spec->prefix;
	tmp[count++] = spec->name;
	if (spec->postfix) tmp[count++] = spec->postfix;
	tmp[count] = HAWK_NULL;

	#if defined(HAWK_OOCH_IS_BCH)
	modpath = hawk_dupbcstrarr(awk, tmp, HAWK_NULL);
	#else
	modpath = hawk_dupucstrarrtobcstr(awk, tmp, HAWK_NULL);
	#endif
	if (!modpath) return HAWK_NULL;

	h = LoadModule (modpath);

	hawk_freemem (awk, modpath);
	
	HAWK_ASSERT (awk, HAWK_SIZEOF(h) <= HAWK_SIZEOF(void*));
	return h;

#elif defined(USE_DLFCN)
	void* h;
	hawk_bch_t* modpath;
	const hawk_ooch_t* tmp[4];
	int count;

	count = 0;
	if (spec->prefix) tmp[count++] = spec->prefix;
	tmp[count++] = spec->name;
	if (spec->postfix) tmp[count++] = spec->postfix;
	tmp[count] = HAWK_NULL;

	#if defined(HAWK_OOCH_IS_BCH)
	modpath = hawk_dupbcstrarr(awk, tmp, HAWK_NULL);
	#else
	modpath = hawk_dupucstrarrtobcstr(awk, tmp, HAWK_NULL);
	#endif
	if (!modpath)
	{
		hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
		return HAWK_NULL;
	}

	h = dlopen(modpath, RTLD_NOW);
	if (!h)
	{
		hawk_seterrfmt (awk, HAWK_ESYSERR, HAWK_NULL, HAWK_T("%hs"), dlerror());
	}

	hawk_freemem (awk, modpath);

	return h;

#else
	hawk_seterrnum (awk, HAWK_ENOIMPL, HAWK_NULL);
	return HAWK_NULL;
#endif
}

void hawk_stdmodclose (hawk_t* awk, void* handle)
{
#if defined(USE_LTDL)
	lt_dlclose (handle);
#elif defined(_WIN32)
	FreeLibrary ((HMODULE)handle);
#elif defined(__OS2__)
	DosFreeModule ((HMODULE)handle);
#elif defined(__DOS__) && defined(HAWK_ENABLE_DOS_DYNAMIC_MODULE)
	FreeModule (handle);
#elif defined(USE_DLFCN)
	dlclose (handle);
#else
	/* nothing to do */
#endif
}

void* hawk_stdmodgetsym (hawk_t* awk, void* handle, const hawk_ooch_t* name)
{
	void* s;
	hawk_bch_t* mname;

#if defined(HAWK_OOCH_IS_BCH)
	mname = (hawk_bch_t*)name;
#else
	mname = hawk_duputobcstr(awk, name, HAWK_NULL);
	if (!mname) return HAWK_NULL;
#endif

#if defined(USE_LTDL)
	s = lt_dlsym (handle, mname);

#elif defined(_WIN32)
	s = GetProcAddress ((HMODULE)handle, mname);

#elif defined(__OS2__)
	if (DosQueryProcAddr ((HMODULE)handle, 0, mname, (PFN*)&s) != NO_ERROR) s = HAWK_NULL;

#elif defined(__DOS__) && defined(HAWK_ENABLE_DOS_DYNAMIC_MODULE)
	s = GetProcAddress (handle, mname);

#elif defined(USE_DLFCN)
	s = dlsym (handle, mname);

#else
	s = HAWK_NULL;
#endif

#if defined(HAWK_OOCH_IS_BCH)
	/* nothing to do */
#else
	HAWK_MMGR_FREE (hawk_getmmgr(awk), mname);
#endif

	return s;
}

static int add_globals (hawk_t* awk);
static int add_functions (hawk_t* awk);

hawk_t* hawk_openstd (hawk_oow_t xtnsize, hawk_errnum_t* errnum)
{
	return hawk_openstdwithmmgr(&sys_mmgr, xtnsize, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), errnum);
}

static void fini_xtn (hawk_t* awk)
{
	xtn_t* xtn = GET_XTN(awk);
	if (xtn->stdmod_up)
	{
		hawk_stdmodshutdown (awk);
		xtn->stdmod_up = 0;
	}
}

static void clear_xtn (hawk_t* awk)
{
	/* nothing to do */
}

hawk_t* hawk_openstdwithmmgr (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errnum_t* errnum)
{
	hawk_t* awk;
	hawk_prm_t prm;
	xtn_t* xtn;

	prm.math.pow = hawk_stdmathpow;
	prm.math.mod = hawk_stdmathmod;

	prm.modopen = std_mod_open_checked;
	prm.modclose = hawk_stdmodclose;
	prm.modgetsym = hawk_stdmodgetsym;

	/* create an object */
	awk = hawk_open(mmgr, HAWK_SIZEOF(xtn_t) + xtnsize, cmgr, &prm, errnum);
	if (!awk) return HAWK_NULL;

	/* adjust the object size by the sizeof xtn_t so that hawk_getxtn() returns the right pointer. */
	awk->_instsize += HAWK_SIZEOF(xtn_t);

#if defined(USE_DLFCN)
	if (hawk_setopt(awk, HAWK_MODPOSTFIX, HAWK_T(".so")) <= -1) 
	{
		if (errnum) *errnum = hawk_geterrnum(awk);
		goto oops;
	}
#endif

	/* initialize extension */
	xtn = GET_XTN(awk);
	/* the extension area has been cleared in hawk_open().
	 * HAWK_MEMSET (xtn, 0, HAWK_SIZEOF(*xtn));*/

	/* add intrinsic global variables and functions */
	if (add_globals(awk) <= -1 || add_functions(awk) <= -1) 
	{
		if (errnum) *errnum = hawk_geterrnum(awk);
		goto oops;
	}

	if (hawk_stdmodstartup(awk) <= -1) 
	{
		xtn->stdmod_up = 0;
		/* carry on regardless of failure */
	}
	else
	{
		xtn->stdmod_up = 1;
	}

	xtn->ecb.close = fini_xtn;
	xtn->ecb.clear = clear_xtn;
	hawk_pushecb (awk, &xtn->ecb);

	return awk;

oops:
	if (awk) hawk_close (awk);
	return HAWK_NULL;
}

static hawk_sio_t* open_sio (hawk_t* awk, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;
	sio = hawk_sio_open(awk, 0, file, flags);
	if (sio == HAWK_NULL)
	{
		hawk_oocs_t errarg;
		errarg.ptr = (hawk_ooch_t*)file;
		errarg.len = hawk_count_oocstr(file);
		hawk_seterrnum (awk, HAWK_EOPEN, &errarg);
	}
	return sio;
}

static hawk_sio_t* open_sio_rtx (hawk_rtx_t* rtx, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;
	sio = hawk_sio_open(hawk_rtx_getawk(rtx), 0, file, flags);
	if (sio == HAWK_NULL)
	{
		hawk_oocs_t errarg;
		errarg.ptr = (hawk_ooch_t*)file;
		errarg.len = hawk_count_oocstr(file);
		hawk_rtx_seterrnum (rtx, HAWK_EOPEN, &errarg);
	}
	return sio;
}

static hawk_oocs_t sio_std_names[] =
{
	{ HAWK_T("stdin"),   5 },
	{ HAWK_T("stdout"),  6 },
	{ HAWK_T("stderr"),  6 }
};

static hawk_sio_t* open_sio_std (hawk_t* awk, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	sio = hawk_sio_openstd(awk, 0, std, flags);
	if (sio == HAWK_NULL) hawk_seterrnum (awk, HAWK_EOPEN, &sio_std_names[std]);
	return sio;
}

static hawk_sio_t* open_sio_std_rtx (hawk_rtx_t* rtx, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;

	sio = hawk_sio_openstd(hawk_rtx_getawk(rtx), 0, std, flags);
	if (sio == HAWK_NULL) hawk_rtx_seterrnum (rtx, HAWK_EOPEN, &sio_std_names[std]);
	return sio;
}

/*** PARSESTD ***/

static int is_psin_file (hawk_parsestd_t* psin)
{
	return psin->type == HAWK_PARSESTD_FILE ||
	       psin->type == HAWK_PARSESTD_FILEB ||
	       psin->type == HAWK_PARSESTD_FILEU;
}

static int open_parsestd (hawk_t* awk, hawk_sio_arg_t* arg, xtn_t* xtn, hawk_oow_t index)
{
	hawk_parsestd_t* psin = &xtn->s.in.x[index];

	switch (psin->type)
	{
		/* normal source files */
	#if defined(HAWK_OOCH_IS_BCH)
		case HAWK_PARSESTD_FILE:
			HAWK_ASSERT (moo, &psin->u.fileb.path == &psin->u.file.path);
			HAWK_ASSERT (moo, &psin->u.fileb.cmgr == &psin->u.file.cmgr);
	#endif
		case HAWK_PARSESTD_FILEB:
		{
			hawk_sio_t* tmp;
			if (psin->u.fileb.path == HAWK_NULL || (psin->u.fileb.path[0] == '-' && psin->u.fileb.path[1] == '\0'))
			{
				/* no path name or - -> stdin */
				tmp = open_sio_std(awk, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			else
			{
			#if defined(HAWK_OOCH_IS_UCH)
				hawk_uch_t* upath;
				upath = hawk_dupbtoucstr(awk, psin->u.fileb.path, HAWK_NULL, 1);
				if (!upath) return -1;
				tmp = open_sio(awk, upath, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
				hawk_freemem (awk, upath);
			#else
				tmp = open_sio(awk, psin->u.fileb.path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
			#endif
			}
			if (tmp == HAWK_NULL) return -1;

			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1])) hawk_sio_close (arg->handle);

			arg->handle = tmp;
			if (psin->u.fileb.cmgr) hawk_sio_setcmgr (arg->handle, psin->u.fileb.cmgr);
			return 0;
		}

	#if defined(HAWK_OOCH_IS_UCH)
		case HAWK_PARSESTD_FILE:
			HAWK_ASSERT (moo, &psin->u.fileu.path == &psin->u.file.path);
			HAWK_ASSERT (moo, &psin->u.fileu.cmgr == &psin->u.file.cmgr);
	#endif
		case HAWK_PARSESTD_FILEU:
		{
			hawk_sio_t* tmp;
			if (psin->u.fileu.path == HAWK_NULL || (psin->u.fileu.path[0] == '-' && psin->u.fileu.path[1] == '\0'))
			{
				/* no path name or - -> stdin */
				tmp = open_sio_std(awk, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			else
			{
			#if defined(HAWK_OOCH_IS_UCH)
				tmp = open_sio(awk, psin->u.fileu.path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
			#else
				hawk_bch_t* bpath;
				bpath = hawk_duputobcstr(awk, psin->u.fileu.path, HAWK_NULL);
				if (!upath) return -1;
				tmp = open_sio(awk, bpath, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
				hawk_freemem (hawk, bpath);
			#endif
			}
			if (tmp == HAWK_NULL) return -1;

			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1]))
				hawk_sio_close (arg->handle);

			arg->handle = tmp;
			if (psin->u.fileu.cmgr) hawk_sio_setcmgr (arg->handle, psin->u.fileu.cmgr);
			return 0;
		}

		case HAWK_PARSESTD_OOCS:
			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1])) hawk_sio_close (arg->handle);
			xtn->s.in.u.oocs.ptr = psin->u.oocs.ptr;
			xtn->s.in.u.oocs.end = psin->u.oocs.ptr + psin->u.oocs.len;
			return 0;

		case HAWK_PARSESTD_BCS:
			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1])) hawk_sio_close (arg->handle);
			xtn->s.in.u.bcs.ptr = psin->u.bcs.ptr;
			xtn->s.in.u.bcs.end = psin->u.bcs.ptr + psin->u.bcs.len;
			return 0;

		case HAWK_PARSESTD_UCS:
			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1])) hawk_sio_close (arg->handle);
			xtn->s.in.u.ucs.ptr = psin->u.ucs.ptr;
			xtn->s.in.u.ucs.end = psin->u.ucs.ptr + psin->u.ucs.len;
			return 0;

		default:
			hawk_seterrnum (awk, HAWK_EINTERN, HAWK_NULL);
			return -1;
	}
}

static int fill_sio_arg_unique_id (hawk_t* hawk, hawk_sio_arg_t* arg, const hawk_ooch_t* path)
{
#if defined(_WIN32)
	return -1;

#elif defined(__OS2__)
	return -1;

#elif defined(__DOS__)
	return -1;

#else
	hawk_stat_t st;
	int x;
	struct
	{
		hawk_uintptr_t ino;
		hawk_uintptr_t dev;
	} tmp;

#if defined(HAWK_OOCH_IS_BCH)
	x = HAWK_STAT(path, &st);
	if (x <= -1) return -1;
#else
	hawk_bch_t* bpath;

	bpath= hawk_duputobcstr(hawk, path, HAWK_NULL);
	if (!bpath) return -1;

	x = HAWK_STAT(bpath, &st);
	hawk_freemem (hawk, bpath);
	if (x <= -1) return -1;
#endif

	tmp.ino = st.st_ino;
	tmp.dev = st.st_dev;

	HAWK_MEMCPY (arg->unique_id, &tmp, (HAWK_SIZEOF(tmp) > HAWK_SIZEOF(arg->unique_id)? HAWK_SIZEOF(arg->unique_id): HAWK_SIZEOF(tmp)));
	return 0;
#endif
}

static hawk_ooi_t sf_in_open (hawk_t* awk, hawk_sio_arg_t* arg, xtn_t* xtn)
{
	if (arg->prev == HAWK_NULL)
	{
		/* handle normal source input streams specified 
		 * to hawk_parsestd() */

		hawk_ooi_t x;

		HAWK_ASSERT (awk, arg == &awk->sio.arg);

		x = open_parsestd(awk, arg, xtn, 0);
		if (x >= 0) 
		{
			xtn->s.in.xindex = 0; /* update the current stream index */
			/* perform some manipulation about the top-level input information */
			if (is_psin_file(&xtn->s.in.x[0]))
			{
				arg->name = hawk_sio_getpath(arg->handle);
				if (arg->name == HAWK_NULL) arg->name = sio_std_names[HAWK_SIO_STDIN].ptr;
			}
			else arg->name = HAWK_NULL;
		}

		return x;
	}
	else
	{
		/* handle the included source file - @include */
		const hawk_ooch_t* path;
		hawk_ooch_t fbuf[64];
		hawk_ooch_t* dbuf = HAWK_NULL;

		HAWK_ASSERT (awk, arg->name != HAWK_NULL);

		path = arg->name;
		if (arg->prev->handle)
		{
			const hawk_ooch_t* outer;

			outer = hawk_sio_getpath(arg->prev->handle);
			if (outer)
			{
				const hawk_ooch_t* base;

				base = hawk_get_base_name_oocstr(outer);
				if (base != outer && arg->name[0] != HAWK_T('/'))
				{
					hawk_oow_t tmplen, totlen, dirlen;

					dirlen = base - outer;
					totlen = hawk_count_oocstr(arg->name) + dirlen;
					if (totlen >= HAWK_COUNTOF(fbuf))
					{
						dbuf = hawk_allocmem(awk, HAWK_SIZEOF(hawk_ooch_t) * (totlen + 1));
						if (!dbuf) return -1;
						path = dbuf;
					}
					else path = fbuf;

					tmplen = hawk_copy_oochars_to_oocstr_unlimited((hawk_ooch_t*)path, outer, dirlen);
					hawk_copy_oocstr_unlimited ((hawk_ooch_t*)path + tmplen, arg->name);
				}
			}
		}

		arg->handle = hawk_sio_open(awk, 0, path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);

		if (dbuf) hawk_freemem (awk, dbuf);
		if (!arg->handle)
		{
			hawk_oocs_t ea;
			ea.ptr = (hawk_ooch_t*)arg->name;
			ea.len = hawk_count_oocstr(ea.ptr);
			hawk_seterrnum (awk, HAWK_EOPEN, &ea);
			return -1;
		}

		/* TODO: use the system handle(file descriptor) instead of the path? */
		/*syshnd = hawk_sio_gethnd(arg->handle);*/
		fill_sio_arg_unique_id(awk, arg, path); /* ignore failure */

		return 0;
	}
}

static hawk_ooi_t sf_in_close (hawk_t* awk, hawk_sio_arg_t* arg, xtn_t* xtn)
{
	if (arg->prev == HAWK_NULL)
	{
		switch (xtn->s.in.x[xtn->s.in.xindex].type)
		{
			case HAWK_PARSESTD_FILE:
			case HAWK_PARSESTD_FILEB:
			case HAWK_PARSESTD_FILEU:
				HAWK_ASSERT (awk, arg->handle != HAWK_NULL);
				hawk_sio_close (arg->handle);
				break;

			case HAWK_PARSESTD_OOCS:
			case HAWK_PARSESTD_BCS:
			case HAWK_PARSESTD_UCS:
				/* nothing to close */
				break;

			default:
				/* nothing to close */
				break;
		}
	}
	else
	{
		/* handle the included source file - @include */
		HAWK_ASSERT (awk, arg->handle != HAWK_NULL);
		hawk_sio_close (arg->handle);
	}

	return 0;
}

static hawk_ooi_t sf_in_read (hawk_t* awk, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size, xtn_t* xtn)
{
	if (arg->prev == HAWK_NULL)
	{
		hawk_ooi_t n;

		HAWK_ASSERT (awk, arg == &awk->sio.arg);

	again:
		switch (xtn->s.in.x[xtn->s.in.xindex].type)
		{
			case HAWK_PARSESTD_FILE:
			case HAWK_PARSESTD_FILEB:
			case HAWK_PARSESTD_FILEU:
				HAWK_ASSERT (awk, arg->handle != HAWK_NULL);
				n = hawk_sio_getoochars (arg->handle, data, size);
				if (n <= -1)
				{
					hawk_oocs_t ea;
					ea.ptr = (hawk_ooch_t*)xtn->s.in.x[xtn->s.in.xindex].u.file.path;
					if (ea.ptr == HAWK_NULL) ea.ptr = sio_std_names[HAWK_SIO_STDIN].ptr;
					ea.len = hawk_count_oocstr(ea.ptr);
					hawk_seterrnum (awk, HAWK_EREAD, &ea);
				}
				break;

			case HAWK_PARSESTD_OOCS:
			parsestd_str:
				n = 0;
				while (n < size && xtn->s.in.u.oocs.ptr < xtn->s.in.u.oocs.end)
				{
					data[n++] = *xtn->s.in.u.oocs.ptr++;
				}
				break;

			case HAWK_PARSESTD_BCS:
			#if defined(HAWK_OOCH_IS_BCH)
				goto parsestd_str;
			#else
			{
				int m;
				hawk_oow_t mbslen, wcslen;

				mbslen = xtn->s.in.u.bcs.end - xtn->s.in.u.bcs.ptr;
				wcslen = size;
				if ((m = hawk_conv_bchars_to_uchars_with_cmgr(xtn->s.in.u.bcs.ptr, &mbslen, data, &wcslen, hawk_getcmgr(awk), 0)) <= -1 && m != -2)
				{
					hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
					n = -1;
				}
				else
				{
					xtn->s.in.u.bcs.ptr += mbslen;
					n = wcslen;
				}
				break;
			}
			#endif

			case HAWK_PARSESTD_UCS:
			#if defined(HAWK_OOCH_IS_BCH)
			{
				int m;
				hawk_oow_t mbslen, wcslen;

				wcslen = xtn->s.in.u.ucs.end - xtn->s.in.u.ucs.ptr;
				mbslen = size;
				if ((m = hawk_conv_uchars_to_bchars_with_cmgr(xtn->s.in.u.ucs.ptr, &wcslen, data, &mbslen, hawk_getcmgr(awk))) <= -1 && m != -2)
				{
					hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
					n = -1;
				}
				else
				{
					xtn->s.in.u.ucs.ptr += wcslen;
					n = mbslen;
				}
				break;
			}
			#else
				goto parsestd_str;
			#endif

			default:
				/* this should never happen */
				hawk_seterrnum (awk, HAWK_EINTERN, HAWK_NULL);
				n = -1;
				break;
		}

		if (n == 0)
		{
		 	/* reached end of the current stream. */
			hawk_oow_t next = xtn->s.in.xindex + 1;
			if (xtn->s.in.x[next].type != HAWK_PARSESTD_NULL)
			{
				/* open the next stream if available. */
				if (open_parsestd (awk, arg, xtn, next) <= -1) n = -1;
				else 
				{
					xtn->s.in.xindex = next; /* update the next to the current */

					/* update the I/O object name */
					if (is_psin_file(&xtn->s.in.x[next]))
					{
						arg->name = hawk_sio_getpath(arg->handle);
						if (arg->name == HAWK_NULL) arg->name = sio_std_names[HAWK_SIO_STDIN].ptr;
					}
					else
					{
						arg->name = HAWK_NULL;
					}

					arg->line = 0; /* reset the line number */
					arg->colm = 0;
					goto again;
				}
			}
		}

		return n;
	}
	else
	{
		/* handle the included source file - @include */
		hawk_ooi_t n;

		HAWK_ASSERT (awk, arg->name != HAWK_NULL);
		HAWK_ASSERT (awk, arg->handle != HAWK_NULL);

		n = hawk_sio_getoochars(arg->handle, data, size);
		if (n <= -1)
		{
			hawk_oocs_t ea;
			ea.ptr = (hawk_ooch_t*)arg->name;
			ea.len = hawk_count_oocstr(ea.ptr);
			hawk_seterrnum (awk, HAWK_EREAD, &ea);
		}
		return n;
	}
}

static hawk_ooi_t sf_in (hawk_t* awk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size)
{
	xtn_t* xtn = GET_XTN(awk);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			return sf_in_open(awk, arg, xtn);

		case HAWK_SIO_CMD_CLOSE:
			return sf_in_close(awk, arg, xtn);

		case HAWK_SIO_CMD_READ:
			return sf_in_read(awk, arg, data, size, xtn);

		default:
			hawk_seterrnum (awk, HAWK_EINTERN, HAWK_NULL);
			return -1;
	}
}

static hawk_ooi_t sf_out (hawk_t* awk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size)
{
	xtn_t* xtn = GET_XTN(awk);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
		{
			switch (xtn->s.out.x->type)
			{
			#if defined(HAWK_OOCH_IS_BCH)
				case HAWK_PARSESTD_FILE:
					HAWK_ASSERT (moo, &xtn->s.out.x->u.fileb.path == &xtn->s.out.x->u.file.path);
					HAWK_ASSERT (moo, &xtn->s.out.x->u.fileb.cmgr == &xtn->s.out.x->u.file.cmgr);
			#endif
				case HAWK_PARSESTD_FILEB:
					if (xtn->s.out.x->u.fileb.path == HAWK_NULL || (xtn->s.out.x->u.fileb.path[0] == '-' && xtn->s.out.x->u.fileb.path[1] == '\0'))
					{
						/* no path name or - -> stdout */
						xtn->s.out.u.file.sio = open_sio_std(awk, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}
					else
					{
					#if defined(HAWK_OOCH_IS_UCH)
						hawk_uch_t* upath;
						upath = hawk_dupbtoucstr(awk, xtn->s.out.x->u.fileb.path, HAWK_NULL, 1);
						if (!upath) return -1;
						xtn->s.out.u.file.sio = open_sio(awk, upath, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
						hawk_freemem (awk, upath);
					#else
						xtn->s.out.u.file.sio = open_sio(awk, xtn->s.out.x->u.fileb.path, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
					#endif
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}

					if (xtn->s.out.x->u.fileb.cmgr) hawk_sio_setcmgr (xtn->s.out.u.file.sio, xtn->s.out.x->u.fileb.cmgr);
					return 1;

			#if defined(HAWK_OOCH_IS_UCH)
				case HAWK_PARSESTD_FILE:
					HAWK_ASSERT (moo, &xtn->s.out.x->u.fileu.path == &xtn->s.out.x->u.file.path);
					HAWK_ASSERT (moo, &xtn->s.out.x->u.fileu.cmgr == &xtn->s.out.x->u.file.cmgr);
			#endif
				case HAWK_PARSESTD_FILEU:
					if (xtn->s.out.x->u.fileu.path == HAWK_NULL || (xtn->s.out.x->u.fileu.path[0] == '-' && xtn->s.out.x->u.fileu.path[1] == '\0'))
					{
						/* no path name or - -> stdout */
						xtn->s.out.u.file.sio = open_sio_std(awk, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}
					else
					{
					#if defined(HAWK_OOCH_IS_UCH)
						xtn->s.out.u.file.sio = open_sio(awk, xtn->s.out.x->u.fileu.path, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
					#else
						hawk_bch_t* bpath;
						bpath = hawk_duputobcstr(awk, xtn->s.out.x->u.fileu.path, HAWK_NULL);
						if (!bpath) return -1;
						xtn->s.out.u.file.sio = open_sio(awk, bpath, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
						hawk_freemem (awk, bpath);
					#endif
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}

					if (xtn->s.out.x->u.fileu.cmgr) hawk_sio_setcmgr (xtn->s.out.u.file.sio, xtn->s.out.x->u.fileu.cmgr);
					return 1;

				case HAWK_PARSESTD_OOCS:
					xtn->s.out.u.oocs.buf = hawk_ooecs_open(awk, 0, 512);
					if (xtn->s.out.u.oocs.buf == HAWK_NULL) return -1;
					return 1;

				case HAWK_PARSESTD_BCS:
					xtn->s.out.u.bcs.buf = hawk_becs_open(awk, 0, 512);
					if (xtn->s.out.u.bcs.buf == HAWK_NULL) return -1;
					return 1;

				case HAWK_PARSESTD_UCS:
					xtn->s.out.u.ucs.buf = hawk_uecs_open(awk, 0, 512);
					if (xtn->s.out.u.ucs.buf == HAWK_NULL) return -1;
					return 1;
			}

			break;
		}


		case HAWK_SIO_CMD_CLOSE:
		{
			switch (xtn->s.out.x->type)
			{
				case HAWK_PARSESTD_FILE:
				case HAWK_PARSESTD_FILEB:
				case HAWK_PARSESTD_FILEU:
					hawk_sio_close (xtn->s.out.u.file.sio);
					return 0;

				case HAWK_PARSESTD_OOCS:
				case HAWK_PARSESTD_BCS:
				case HAWK_PARSESTD_UCS:
					/* i don't close xtn->s.out.u.oocs.buf intentionally here.
					 * it will be closed at the end of hawk_parsestd() */
					return 0;
			}

			break;
		}

		case HAWK_SIO_CMD_WRITE:
		{
			switch (xtn->s.out.x->type)
			{
				case HAWK_PARSESTD_FILE:
				case HAWK_PARSESTD_FILEB:
				case HAWK_PARSESTD_FILEU:
				{
					hawk_ooi_t n;
					HAWK_ASSERT (awk, xtn->s.out.u.file.sio != HAWK_NULL);
					n = hawk_sio_putoochars(xtn->s.out.u.file.sio, data, size);
					if (n <= -1)
					{
						hawk_oocs_t ea;
						ea.ptr = (hawk_ooch_t*)xtn->s.out.x->u.file.path;
						if (ea.ptr == HAWK_NULL) ea.ptr = sio_std_names[HAWK_SIO_STDOUT].ptr;
						ea.len = hawk_count_oocstr(ea.ptr);
						hawk_seterrnum (awk, HAWK_EWRITE, &ea);
					}
					return n;
				}

				case HAWK_PARSESTD_OOCS:
				parsestd_str:
					if (size > HAWK_TYPE_MAX(hawk_ooi_t)) size = HAWK_TYPE_MAX(hawk_ooi_t);
					if (hawk_ooecs_ncat(xtn->s.out.u.oocs.buf, data, size) == (hawk_oow_t)-1)
					{
						hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
						return -1;
					}
					return size;

				case HAWK_PARSESTD_BCS:
				#if defined(HAWK_OOCH_IS_BCH)
					goto parsestd_str;
				#else
				{
					hawk_oow_t mbslen, wcslen;
					hawk_oow_t orglen;

					wcslen = size;
					if (hawk_convutobchars(awk, data, &wcslen, HAWK_NULL, &mbslen) <= -1) return -1;
					if (mbslen > HAWK_TYPE_MAX(hawk_ooi_t)) mbslen = HAWK_TYPE_MAX(hawk_ooi_t);

					orglen = hawk_becs_getlen(xtn->s.out.u.bcs.buf);
					if (hawk_becs_setlen(xtn->s.out.u.bcs.buf, orglen + mbslen) == (hawk_oow_t)-1)
					{
						hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
						return -1;
					}

					wcslen = size;
					hawk_convutobchars (awk, data, &wcslen, HAWK_BECS_CPTR(xtn->s.out.u.bcs.buf, orglen), &mbslen);  
					size = wcslen;

					return size;
				}
				#endif

				case HAWK_PARSESTD_UCS:
				#if defined(HAWK_OOCH_IS_BCH)
				{
					hawk_oow_t mbslen, wcslen;
					hawk_oow_t orglen;

					mbslen = size;
					if (hawk_convbtouchars(awk, data, &mbslen, HAWK_NULL, &wcslen) <= -1) return -1;
					if (wcslen > HAWK_TYPE_MAX(hawk_ooi_t)) wcslen = HAWK_TYPE_MAX(hawk_ooi_t);

					orglen = hawk_becs_getlen(xtn->s.out.u.ucs.buf);
					if (hawk_uecs_setlen(xtn->s.out.u.ucs.buf, orglen + wcslen) == (hawk_oow_t)-1)
					{
						hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
						return -1;
					}

					mbslen = size;
					hawk_convbtouchars (awk, data, &mbslen, HAWK_UECS_CPTR(xtn->s.out.u.ucs.buf, orglen), &wcslen);
					size = mbslen;

					return size;
				}
				#else
					goto parsestd_str;
				#endif
			}

			break;
		}

		default:
			/* other code must not trigger this function */
			break;
	}

	hawk_seterrnum (awk, HAWK_EINTERN, HAWK_NULL);
	return -1;
}

int hawk_parsestd (hawk_t* awk, hawk_parsestd_t in[], hawk_parsestd_t* out)
{
	hawk_sio_cbs_t sio;
	xtn_t* xtn = GET_XTN(awk);
	int n;

	if (in == HAWK_NULL || (in[0].type != HAWK_PARSESTD_FILE && 
	                        in[0].type != HAWK_PARSESTD_FILEB && 
	                        in[0].type != HAWK_PARSESTD_FILEU && 
	                        in[0].type != HAWK_PARSESTD_OOCS &&
	                        in[0].type != HAWK_PARSESTD_BCS &&
	                        in[0].type != HAWK_PARSESTD_UCS))
	{
		/* the input is a must. at least 1 file or 1 string 
		 * must be specified */
		hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
		return -1;
	}

	sio.in = sf_in;
	xtn->s.in.x = in;

	if (out == HAWK_NULL) sio.out = HAWK_NULL;
	else
	{
		if (out->type != HAWK_PARSESTD_FILE &&
		    out->type != HAWK_PARSESTD_FILEB &&
		    out->type != HAWK_PARSESTD_FILEU &&
		    out->type != HAWK_PARSESTD_OOCS &&
		    out->type != HAWK_PARSESTD_BCS &&
		    out->type != HAWK_PARSESTD_UCS)
		{
			hawk_seterrnum (awk, HAWK_EINVAL, HAWK_NULL);
			return -1;
		}
		sio.out = sf_out;
		xtn->s.out.x = out;
	}

	n = hawk_parse(awk, &sio);

	if (out)
	{
		switch (out->type)
		{
			case HAWK_PARSESTD_OOCS:
				if (n >= 0)
				{
					HAWK_ASSERT (awk, xtn->s.out.u.oocs.buf != HAWK_NULL);
					hawk_ooecs_yield (xtn->s.out.u.oocs.buf, &out->u.oocs, 0);
				}
				if (xtn->s.out.u.oocs.buf) hawk_ooecs_close (xtn->s.out.u.oocs.buf);
				break;

			case HAWK_PARSESTD_BCS:
				if (n >= 0)
				{
					HAWK_ASSERT (awk, xtn->s.out.u.bcs.buf != HAWK_NULL);
					hawk_becs_yield (xtn->s.out.u.bcs.buf, &out->u.bcs, 0);
				}
				if (xtn->s.out.u.bcs.buf) hawk_becs_close (xtn->s.out.u.bcs.buf);
				break;

			case HAWK_PARSESTD_UCS:
				if (n >= 0)
				{
					HAWK_ASSERT (awk, xtn->s.out.u.ucs.buf != HAWK_NULL);
					hawk_uecs_yield (xtn->s.out.u.ucs.buf, &out->u.ucs, 0);
				}
				if (xtn->s.out.u.ucs.buf) hawk_uecs_close (xtn->s.out.u.ucs.buf);
				break;
		}
	}

	return n;
}

/*** RTX_OPENSTD ***/

#if defined(ENABLE_NWIO)
static hawk_ooi_t nwio_handler_open (hawk_rtx_t* rtx, hawk_rio_arg_t* riod, int flags, hawk_nwad_t* nwad, hawk_nwio_tmout_t* tmout)
{
	hawk_nwio_t* handle;

	handle = hawk_nwio_open (
		hawk_rtx_getmmgr(rtx), 0, nwad, 
		flags | HAWK_NWIO_TEXT | HAWK_NWIO_IGNOREECERR |
		HAWK_NWIO_REUSEADDR | HAWK_NWIO_READNORETRY | HAWK_NWIO_WRITENORETRY,
		tmout
	);
	if (handle == HAWK_NULL) return -1;

#if defined(HAWK_OOCH_IS_UCH)
	{
		hawk_cmgr_t* cmgr = hawk_rtx_getiocmgrstd(rtx, riod->name);
		if (cmgr) hawk_nwio_setcmgr (handle, cmgr);
	}
#endif

	riod->handle = (void*)handle;
	riod->uflags = 1; /* nwio indicator */
	return 1;
}

static hawk_ooi_t nwio_handler_rest (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
{
	switch (cmd)
	{
		case HAWK_RIO_CMD_OPEN:
			hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
			return -1;

		case HAWK_RIO_CMD_CLOSE:
			hawk_nwio_close ((hawk_nwio_t*)riod->handle);
			return 0;

		case HAWK_RIO_CMD_READ:
			return hawk_nwio_read((hawk_nwio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_WRITE:
			return hawk_nwio_write((hawk_nwio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_WRITE_BYTES:
			return hawk_nwio_writebytes((hawk_nwio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_FLUSH:
			/*if (riod->mode == HAWK_RIO_PIPE_READ) return -1;*/
			return hawk_nwio_flush((hawk_nwio_t*)riod->handle);

		case HAWK_RIO_CMD_NEXT:
			break;
	}

	hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
	return -1;
}

static int parse_rwpipe_uri (const hawk_ooch_t* uri, int* flags, hawk_nwad_t* nwad)
{
	static struct
	{
		const hawk_ooch_t* prefix;
		hawk_oow_t        len;
		int               flags;
	} x[] =
	{
		{ HAWK_T("tcp://"),  6, HAWK_NWIO_TCP },
		{ HAWK_T("udp://"),  6, HAWK_NWIO_UDP },
		{ HAWK_T("tcpd://"), 7, HAWK_NWIO_TCP | HAWK_NWIO_PASSIVE },
		{ HAWK_T("udpd://"), 7, HAWK_NWIO_UDP | HAWK_NWIO_PASSIVE }
	};
	int i;


	for (i = 0; i < HAWK_COUNTOF(x); i++)
	{
		if (hawk_strzcmp (uri, x[i].prefix, x[i].len) == 0)
		{
			if (hawk_strtonwad (uri + x[i].len, nwad) <= -1) return -1;
			*flags = x[i].flags;
			return 0;
		}
	}

	return -1;
}
#endif

static hawk_ooi_t pio_handler_open (hawk_rtx_t* rtx, hawk_rio_arg_t* riod)
{
	hawk_pio_t* handle;
	int flags;

	if (riod->mode == HAWK_RIO_PIPE_READ)
	{
		/* TODO: should ERRTOOUT be unset? */
		flags = HAWK_PIO_READOUT | HAWK_PIO_ERRTOOUT;
	}
	else if (riod->mode == HAWK_RIO_PIPE_WRITE)
	{
		flags = HAWK_PIO_WRITEIN;
	}
	else if (riod->mode == HAWK_RIO_PIPE_RW)
	{
		flags = HAWK_PIO_READOUT | HAWK_PIO_ERRTOOUT | HAWK_PIO_WRITEIN;
	}
	else 
	{
		/* this must not happen */
		hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
		return -1;
	}

	handle = hawk_pio_open (
		hawk_rtx_getawk(rtx),
		0, 
		riod->name, 
		flags | HAWK_PIO_SHELL | HAWK_PIO_TEXT | HAWK_PIO_IGNOREECERR
	);
	if (handle == HAWK_NULL) return -1;

#if defined(HAWK_OOCH_IS_UCH)
	{
		hawk_cmgr_t* cmgr = hawk_rtx_getiocmgrstd(rtx, riod->name);
		if (cmgr)	
		{
			hawk_pio_setcmgr (handle, HAWK_PIO_IN, cmgr);
			hawk_pio_setcmgr (handle, HAWK_PIO_OUT, cmgr);
			hawk_pio_setcmgr (handle, HAWK_PIO_ERR, cmgr);
		}
	}
#endif

	riod->handle = (void*)handle;
	riod->uflags = 0; /* pio indicator */
	return 1;
}

static hawk_ooi_t pio_handler_rest (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
{
	switch (cmd)
	{
		case HAWK_RIO_CMD_OPEN:
			hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
			return -1;

		case HAWK_RIO_CMD_CLOSE:
		{
			hawk_pio_t* pio = (hawk_pio_t*)riod->handle;
			if (riod->mode == HAWK_RIO_PIPE_RW)
			{
				/* specialy treatment is needed for rwpipe.
				 * inspect rwcmode to see if partial closing is
				 * requested. */
				if (riod->rwcmode == HAWK_RIO_CMD_CLOSE_READ)
				{
					hawk_pio_end (pio, HAWK_PIO_IN);
					return 0;
				}
				if (riod->rwcmode == HAWK_RIO_CMD_CLOSE_WRITE)
				{
					hawk_pio_end (pio, HAWK_PIO_OUT);
					return 0;
				}
			}

			hawk_pio_close (pio);
			return 0;
		}

		case HAWK_RIO_CMD_READ:
			return hawk_pio_read ((hawk_pio_t*)riod->handle, HAWK_PIO_OUT, data, size);

		case HAWK_RIO_CMD_WRITE:
			return hawk_pio_write((hawk_pio_t*)riod->handle, HAWK_PIO_IN, data, size);

		case HAWK_RIO_CMD_WRITE_BYTES:
			return hawk_pio_writebytes((hawk_pio_t*)riod->handle, HAWK_PIO_IN, data, size);

		case HAWK_RIO_CMD_FLUSH:
			/*if (riod->mode == HAWK_RIO_PIPE_READ) return -1;*/
			return hawk_pio_flush ((hawk_pio_t*)riod->handle, HAWK_PIO_IN);

		case HAWK_RIO_CMD_NEXT:
			break;
	}

	hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
	return -1;
}

static hawk_ooi_t awk_rio_pipe (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
{
	if (cmd == HAWK_RIO_CMD_OPEN)
	{
	#if defined(ENABLE_NWIO)
		int flags;
		hawk_nwad_t nwad;

		if (riod->mode != HAWK_RIO_PIPE_RW ||
		    parse_rwpipe_uri(riod->name, &flags, &nwad) <= -1) 
		{
			return pio_handler_open (rtx, riod);
		}
		else
		{
			hawk_nwio_tmout_t tmout_buf;
			hawk_nwio_tmout_t* tmout = HAWK_NULL;
			ioattr_t* ioattr;
			rxtn_t* rxtn;

			rxtn = GET_RXTN(rtx);

			ioattr = get_ioattr(&rxtn->cmgrtab, riod->name, hawk_count_oocstr(riod->name));
			if (ioattr)
			{
				tmout = &tmout_buf;
				tmout->r = ioattr->tmout[0];
				tmout->w = ioattr->tmout[1];
				tmout->c = ioattr->tmout[2];
				tmout->a = ioattr->tmout[3];
			}

			return nwio_handler_open(rtx, riod, flags, &nwad, tmout);
		}
	#else
		return pio_handler_open (rtx, riod);
	#endif
	}
	#if defined(ENABLE_NWIO)
	else if (riod->uflags > 0)
		return nwio_handler_rest(rtx, cmd, riod, data, size);
	#endif
	else
		return pio_handler_rest(rtx, cmd, riod, data, size);
}

static hawk_ooi_t awk_rio_file (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
{
	switch (cmd)
	{
		case HAWK_RIO_CMD_OPEN:
		{
			hawk_sio_t* handle;
			int flags = HAWK_SIO_IGNOREECERR;

			switch (riod->mode)
			{
				case HAWK_RIO_FILE_READ: 
					flags |= HAWK_SIO_READ;
					break;
				case HAWK_RIO_FILE_WRITE:
					flags |= HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE;
					break;
				case HAWK_RIO_FILE_APPEND:
					flags |= HAWK_SIO_APPEND | HAWK_SIO_CREATE;
					break;
				default:
					/* this must not happen */
					hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
					return -1; 
			}

			handle = hawk_sio_open(hawk_rtx_getawk(rtx), 0, riod->name, flags);
			if (handle == HAWK_NULL) 
			{
				hawk_oocs_t errarg;
				errarg.ptr = riod->name;
				errarg.len = hawk_count_oocstr(riod->name);
				hawk_rtx_seterrnum (rtx, HAWK_EOPEN, &errarg);
				return -1;
			}

		#if defined(HAWK_OOCH_IS_UCH)
			{
				hawk_cmgr_t* cmgr = hawk_rtx_getiocmgrstd(rtx, riod->name);
				if (cmgr) hawk_sio_setcmgr(handle, cmgr);
			}
		#endif

			riod->handle = (void*)handle;
			return 1;
		}

		case HAWK_RIO_CMD_CLOSE:
			hawk_sio_close ((hawk_sio_t*)riod->handle);
			riod->handle = HAWK_NULL;
			return 0;

		case HAWK_RIO_CMD_READ:
			return hawk_sio_getoochars((hawk_sio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_WRITE:
			return hawk_sio_putoochars((hawk_sio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_WRITE_BYTES:
			return hawk_sio_putbchars((hawk_sio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_FLUSH:
			return hawk_sio_flush((hawk_sio_t*)riod->handle);

		case HAWK_RIO_CMD_NEXT:
			return -1;
	}

	return -1;
}

static int open_rio_console (hawk_rtx_t* rtx, hawk_rio_arg_t* riod)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	hawk_sio_t* sio;

	if (riod->mode == HAWK_RIO_CONSOLE_READ)
	{
		xtn_t* xtn = (xtn_t*)GET_XTN(rtx->awk);

		if (rxtn->c.in.files == HAWK_NULL)
		{
			/* if no input files is specified, 
			 * open the standard input */
			HAWK_ASSERT (hawk_rtx_getawk(rtx), rxtn->c.in.index == 0);

			if (rxtn->c.in.count == 0)
			{
				sio = open_sio_std_rtx (
					rtx, HAWK_SIO_STDIN,
					HAWK_SIO_READ | HAWK_SIO_IGNOREECERR
				);
				if (sio == HAWK_NULL) return -1;

				if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);	

				riod->handle = sio;
				rxtn->c.in.count++;
				return 1;
			}

			return 0;
		}
		else
		{
			/* a temporary variable sio is used here not to change 
			 * any fields of riod when the open operation fails */
			const hawk_ooch_t* file;
			hawk_val_t* argv;
			hawk_htb_t* map;
			hawk_htb_pair_t* pair;
			hawk_ooch_t ibuf[128];
			hawk_oow_t ibuflen;
			hawk_val_t* v;
			hawk_oocs_t as;

		nextfile:
			file = rxtn->c.in.files[rxtn->c.in.index];

			if (file == HAWK_NULL)
			{
				/* no more input file */

				if (rxtn->c.in.count == 0)
				{
					/* all ARGVs are empty strings. 
					 * so no console files were opened.
					 * open the standard input here.
					 *
					 * 'BEGIN { ARGV[1]=""; ARGV[2]=""; }
					 *        { print $0; }' file1 file2
					 */
					sio = open_sio_std_rtx (
						rtx, HAWK_SIO_STDIN,
						HAWK_SIO_READ | HAWK_SIO_IGNOREECERR
					);
					if (sio == HAWK_NULL) return -1;

					if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);	

					riod->handle = sio;
					rxtn->c.in.count++;
					return 1;
				}

				return 0;
			}

			/* handle special case when ARGV[x] has been altered.
			 * so from here down, the file name gotten from 
			 * rxtn->c.in.files is not important and is overridden 
			 * from ARGV.
			 * 'BEGIN { ARGV[1]="file3"; } 
			 *        { print $0; }' file1 file2
			 */
			argv = hawk_rtx_getgbl(rtx, xtn->gbl_argv);
			HAWK_ASSERT (hawk_rtx_getawk(rtx), argv != HAWK_NULL);
			HAWK_ASSERT (hawk_rtx_getawk(rtx), HAWK_RTX_GETVALTYPE (rtx, argv) == HAWK_VAL_MAP);

			map = ((hawk_val_map_t*)argv)->map;
			HAWK_ASSERT (hawk_rtx_getawk(rtx), map != HAWK_NULL);
			
			ibuflen = hawk_int_to_oocstr (rxtn->c.in.index + 1, 10, HAWK_NULL, ibuf, HAWK_COUNTOF(ibuf));

			pair = hawk_htb_search(map, ibuf, ibuflen);
			HAWK_ASSERT (hawk_rtx_getawk(rtx), pair != HAWK_NULL);

			v = HAWK_HTB_VPTR(pair);
			HAWK_ASSERT (hawk_rtx_getawk(rtx), v != HAWK_NULL);

			as.ptr = hawk_rtx_getvaloocstr(rtx, v, &as.len);
			if (as.ptr == HAWK_NULL) return -1;

			if (as.len == 0)
			{
				/* the name is empty */
				hawk_rtx_freevaloocstr (rtx, v, as.ptr);
				rxtn->c.in.index++;
				goto nextfile;
			}

			if (hawk_count_oocstr(as.ptr) < as.len)
			{
				/* the name contains one or more '\0' */
				hawk_oocs_t errarg;

				errarg.ptr = as.ptr;
				/* use this length not to contains '\0'
				 * in an error message */
				errarg.len = hawk_count_oocstr(as.ptr);

				hawk_rtx_seterrnum (rtx, HAWK_EIONMNL, &errarg);

				hawk_rtx_freevaloocstr (rtx, v, as.ptr);
				return -1;
			}

			file = as.ptr;

			sio = (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))?
				open_sio_std_rtx (rtx, HAWK_SIO_STDIN, 
					HAWK_SIO_READ | HAWK_SIO_IGNOREECERR):
				open_sio_rtx (rtx, file,
					HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			if (sio == HAWK_NULL)
			{
				hawk_rtx_freevaloocstr (rtx, v, as.ptr);
				return -1;
			}

			if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);	

			if (hawk_rtx_setfilename (
				rtx, file, hawk_count_oocstr(file)) <= -1)
			{
				hawk_sio_close (sio);
				hawk_rtx_freevaloocstr (rtx, v, as.ptr);
				return -1;
			}

			hawk_rtx_freevaloocstr (rtx, v, as.ptr);
			riod->handle = sio;

			/* increment the counter of files successfully opened */
			rxtn->c.in.count++;
			rxtn->c.in.index++;
			return 1;
		}

	}
	else if (riod->mode == HAWK_RIO_CONSOLE_WRITE)
	{
		if (rxtn->c.out.files == HAWK_NULL)
		{
			HAWK_ASSERT (hawk_rtx_getawk(rtx), rxtn->c.out.index == 0);

			if (rxtn->c.out.count == 0)
			{
				sio = open_sio_std_rtx (
					rtx, HAWK_SIO_STDOUT,
					HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK
				);
				if (sio == HAWK_NULL) return -1;

				if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);	

				riod->handle = sio;
				rxtn->c.out.count++;
				return 1;
			}

			return 0;
		}
		else
		{
			/* a temporary variable sio is used here not to change 
			 * any fields of riod when the open operation fails */
			hawk_sio_t* sio;
			const hawk_ooch_t* file;

			file = rxtn->c.out.files[rxtn->c.out.index];

			if (file == HAWK_NULL)
			{
				/* no more input file */
				return 0;
			}

			sio = (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))?
				open_sio_std_rtx (
					rtx, HAWK_SIO_STDOUT,
					HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK):
				open_sio_rtx (
					rtx, file, 
					HAWK_SIO_WRITE | HAWK_SIO_CREATE | 
					HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
			if (sio == HAWK_NULL) return -1;

			if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);	
			
			if (hawk_rtx_setofilename (
				rtx, file, hawk_count_oocstr(file)) <= -1)
			{
				hawk_sio_close (sio);
				return -1;
			}

			riod->handle = sio;
			rxtn->c.out.index++;
			rxtn->c.out.count++;
			return 1;
		}

	}

	return -1;
}

static hawk_ooi_t awk_rio_console (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
{
	switch (cmd)
	{
		case HAWK_RIO_CMD_OPEN:
			return open_rio_console (rtx, riod);

		case HAWK_RIO_CMD_CLOSE:
			if (riod->handle) hawk_sio_close ((hawk_sio_t*)riod->handle);
			return 0;

		case HAWK_RIO_CMD_READ:
		{
			hawk_ooi_t nn;

			while ((nn = hawk_sio_getoochars((hawk_sio_t*)riod->handle, data, size)) == 0)
			{
				int n;
				hawk_sio_t* sio = (hawk_sio_t*)riod->handle;

				n = open_rio_console(rtx, riod);
				if (n <= -1) return -1;

				if (n == 0) 
				{
					/* no more input console */
					return 0;
				}

				if (sio) hawk_sio_close (sio);
			}
		
			return nn;
		}

		case HAWK_RIO_CMD_WRITE:
			return hawk_sio_putoochars((hawk_sio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_WRITE_BYTES:
			return hawk_sio_putbchars((hawk_sio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_FLUSH:
			return hawk_sio_flush((hawk_sio_t*)riod->handle);

		case HAWK_RIO_CMD_NEXT:
		{
			int n;
			hawk_sio_t* sio = (hawk_sio_t*)riod->handle;

			n = open_rio_console (rtx, riod);
			if (n <= -1) return -1;

			if (n == 0) 
			{
				/* if there is no more file, keep the previous handle */
				return 0;
			}

			if (sio) hawk_sio_close (sio);
			return n;
		}

	}

	hawk_rtx_seterrnum (rtx, HAWK_EINTERN, HAWK_NULL);
	return -1;
}

static void fini_rxtn (hawk_rtx_t* rtx)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	/*xtn_t* xtn = (xtn_t*)GET_XTN(rtx->awk);*/

	if (rxtn->cmgrtab_inited)
	{
		hawk_htb_fini (&rxtn->cmgrtab);
		rxtn->cmgrtab_inited = 0;
	}
}

static int build_argcv (
	hawk_rtx_t* rtx, int argc_id, int argv_id, 
	const hawk_ooch_t* id, const hawk_ooch_t*const icf[])
{
	const hawk_ooch_t*const* p;
	hawk_oow_t argc;
	hawk_val_t* v_argc;
	hawk_val_t* v_argv;
	hawk_val_t* v_tmp;
	hawk_ooch_t key[HAWK_SIZEOF(hawk_int_t)*8+2];
	hawk_oow_t key_len;

	v_argv = hawk_rtx_makemapval (rtx);
	if (v_argv == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, v_argv);

	/* make ARGV[0] */
	v_tmp = hawk_rtx_makestrvalwithoocstr (rtx, id);
	if (v_tmp == HAWK_NULL) 
	{
		hawk_rtx_refdownval (rtx, v_argv);
		return -1;
	}

	/* increment reference count of v_tmp in advance as if 
	 * it has successfully been assigned into ARGV. */
	hawk_rtx_refupval (rtx, v_tmp);

	key_len = hawk_copy_oocstr(key, HAWK_COUNTOF(key), HAWK_T("0"));
	if (hawk_htb_upsert (
		((hawk_val_map_t*)v_argv)->map,
		key, key_len, v_tmp, 0) == HAWK_NULL)
	{
		/* if the assignment operation fails, decrements
		 * the reference of v_tmp to free it */
		hawk_rtx_refdownval (rtx, v_tmp);

		/* the values previously assigned into the
		 * map will be freeed when v_argv is freed */
		hawk_rtx_refdownval (rtx, v_argv);

		hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL); 
		return -1;
	}

	if (icf)
	{
		for (argc = 1, p = icf; *p; p++, argc++) 
		{
			/* the argument must compose a numeric string if it can be.
			 * so call hawk_rtx_makenstrvalwithoocstr() instead of
			 * hawk_rtx_makestrvalwithoocstr(). */
			v_tmp = hawk_rtx_makenstrvalwithoocstr (rtx, *p);
			if (v_tmp == HAWK_NULL)
			{
				hawk_rtx_refdownval (rtx, v_argv);
				return -1;
			}

			key_len = hawk_int_to_oocstr (argc, 10, HAWK_NULL, key, HAWK_COUNTOF(key));
			HAWK_ASSERT (hawk_rtx_getawk(rtx), key_len != (hawk_oow_t)-1);

			hawk_rtx_refupval (rtx, v_tmp);

			if (hawk_htb_upsert (
				((hawk_val_map_t*)v_argv)->map,
				key, key_len, v_tmp, 0) == HAWK_NULL)
			{
				hawk_rtx_refdownval (rtx, v_tmp);
				hawk_rtx_refdownval (rtx, v_argv);
				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL); 
				return -1;
			}
		}
	}
	else argc = 1;

	v_argc = hawk_rtx_makeintval(rtx, (hawk_int_t)argc);
	if (v_argc == HAWK_NULL)
	{
		hawk_rtx_refdownval (rtx, v_argv);
		return -1;
	}

	hawk_rtx_refupval (rtx, v_argc);

	if (hawk_rtx_setgbl(rtx, argc_id, v_argc) <= -1)
	{
		hawk_rtx_refdownval (rtx, v_argc);
		hawk_rtx_refdownval (rtx, v_argv);
		return -1;
	}

	if (hawk_rtx_setgbl(rtx, argv_id, v_argv) <= -1)
	{
		hawk_rtx_refdownval (rtx, v_argc);
		hawk_rtx_refdownval (rtx, v_argv);
		return -1;
	}

	hawk_rtx_refdownval (rtx, v_argc);
	hawk_rtx_refdownval (rtx, v_argv);

	return 0;
}

/* TODO: use wenviron where it's available */
typedef hawk_bch_t env_char_t;
#define ENV_CHAR_IS_BCH
extern char **environ;

static int build_environ (hawk_rtx_t* rtx, int gbl_id, env_char_t* envarr[])
{
	hawk_val_t* v_env;
	hawk_val_t* v_tmp;

	v_env = hawk_rtx_makemapval (rtx);
	if (v_env == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, v_env);

	if (envarr)
	{
		env_char_t* eq;
		hawk_ooch_t* kptr, * vptr;
		hawk_oow_t klen, count;

		for (count = 0; envarr[count]; count++)
		{
		#if ((defined(ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
			eq = hawk_find_oochar_in_oocstr(envarr[count], HAWK_T('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			kptr = envarr[count];
			klen = eq - envarr[count];
			vptr = eq + 1;
		#elif defined(ENV_CHAR_IS_BCH)
			eq = hawk_find_bchar_in_bcstr(envarr[count], HAWK_BT('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			*eq = HAWK_BT('\0');

			/* dupbtoucstr() may fail for invalid encoding. as the environment 
			 * variaables are not under control, call mbstowcsalldup() instead 
			 * to go on despite encoding failure */

			kptr = hawk_dupbtoucstr(hawk_rtx_getawk(rtx), envarr[count], &klen, 1); 
			vptr = hawk_dupbtoucstr(hawk_rtx_getawk(rtx), eq + 1, HAWK_NULL, 1);
			if (kptr == HAWK_NULL || vptr == HAWK_NULL)
			{
				if (kptr) hawk_rtx_freemem (rtx, kptr);
				if (vptr) hawk_rtx_freemem (rtx, vptr);
				hawk_rtx_refdownval (rtx, v_env);

				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL); 
				return -1;
			}

			*eq = HAWK_BT('=');
		#else
			eq = hawk_find_uchar_in_ucstr(envarr[count], HAWK_UT('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			*eq = HAWK_UT('\0');

			kptr = hawk_duputobcstr(hawk_rtx_getawk(rtx), envarr[count], &klen);
			vptr = hawk_duputobcstr(hawk_rtx_getawk(rtx), eq + 1, HAWK_NULL);
			if (kptr == HAWK_NULL || vptr == HAWK_NULL)
			{
				if (kptr) hawk_rtx_freemem (rtx, kptr);
				if (vptr) hawk_rtx_freeme (rtx, vptr):
				hawk_rtx_refdownval (rtx, v_env);

				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}

			*eq = HAWK_UT('=');
		#endif

			/* the string in ENVIRON should be a numeric string if
			 * it can be converted to a number. call makenstrval()
			 * instead of makestrval() */
			v_tmp = hawk_rtx_makenstrvalwithoocstr(rtx, vptr);
			if (v_tmp == HAWK_NULL)
			{
		#if ((defined(ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
				/* nothing to do */
		#else
				if (vptr) hawk_rtx_freemem (rtx, vptr);
				if (kptr) hawk_rtx_freemem (rtx, kptr);
		#endif
				hawk_rtx_refdownval (rtx, v_env);
				return -1;
			}

			/* increment reference count of v_tmp in advance as if 
			 * it has successfully been assigned into ARGV. */
			hawk_rtx_refupval (rtx, v_tmp);

			if (hawk_htb_upsert(((hawk_val_map_t*)v_env)->map, kptr, klen, v_tmp, 0) == HAWK_NULL)
			{
				/* if the assignment operation fails, decrements
				 * the reference of v_tmp to free it */
				hawk_rtx_refdownval (rtx, v_tmp);

		#if ((defined(ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
				/* nothing to do */
		#else
				if (vptr) hawk_rtx_freemem (rtx, vptr);
				if (kptr) hawk_rtx_freemem (rtx, kptr);
		#endif

				/* the values previously assigned into the
				 * map will be freeed when v_env is freed */
				hawk_rtx_refdownval (rtx, v_env);

				hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
				return -1;
			}

		#if ((defined(ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
				/* nothing to do */
		#else
			if (vptr) hawk_rtx_freemem (rtx, vptr);
			if (kptr) hawk_rtx_freemem (rtx, kptr);
		#endif
		}
	}

	if (hawk_rtx_setgbl (rtx, gbl_id, v_env) == -1)
	{
		hawk_rtx_refdownval (rtx, v_env);
		return -1;
	}

	hawk_rtx_refdownval (rtx, v_env);
	return 0;
}

static int make_additional_globals (hawk_rtx_t* rtx, xtn_t* xtn, const hawk_ooch_t* id, const hawk_ooch_t*const icf[])
{
	if (build_argcv (rtx, xtn->gbl_argc, xtn->gbl_argv, id, icf) <= -1 ||
	    build_environ (rtx, xtn->gbl_environ, environ) <= -1) return -1;
	return 0;
}

static hawk_rtx_t* open_rtx_std (
	hawk_t*            awk,
	hawk_oow_t         xtnsize,
	const hawk_ooch_t* id,
	const hawk_ooch_t* icf[],
	const hawk_ooch_t* ocf[],
	hawk_cmgr_t*       cmgr)
{
	hawk_rtx_t* rtx;
	hawk_rio_cbs_t rio;
	rxtn_t* rxtn;
	xtn_t* xtn;

	xtn = GET_XTN(awk);

	rio.pipe = awk_rio_pipe;
	rio.file = awk_rio_file;
	rio.console = awk_rio_console;

	rtx = hawk_rtx_open(awk, HAWK_SIZEOF(rxtn_t) + xtnsize, &rio);
	if (!rtx) return HAWK_NULL;

	rtx->_instsize += HAWK_SIZEOF(rxtn_t);

	rxtn = GET_RXTN(rtx);

	if (rtx->awk->opt.trait & HAWK_RIO)
	{
		if (hawk_htb_init(&rxtn->cmgrtab, awk, 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1)
		{
			hawk_rtx_close (rtx);
			hawk_seterrnum (awk, HAWK_ENOMEM, HAWK_NULL);
			return HAWK_NULL;
		}
		hawk_htb_setstyle (&rxtn->cmgrtab, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_COPIERS));
		rxtn->cmgrtab_inited = 1;
	}

	rxtn->ecb.close = fini_rxtn;
	hawk_rtx_pushecb (rtx, &rxtn->ecb);

	rxtn->c.in.files = icf;
	rxtn->c.in.index = 0;
	rxtn->c.in.count = 0;
	rxtn->c.out.files = ocf;
	rxtn->c.out.index = 0;
	rxtn->c.out.count = 0;
	rxtn->c.cmgr = cmgr;
	
	/* FILENAME can be set when the input console is opened.
	 * so we skip setting it here even if an explicit console file
	 * is specified.  So the value of FILENAME is empty in the 
	 * BEGIN block unless getline is ever met. 
	 *
	 * However, OFILENAME is different. The output
	 * console is treated as if it's already open upon start-up. 
	 * otherwise, 'BEGIN { print OFILENAME; }' prints an empty
	 * string regardless of output console files specified.
	 * That's because OFILENAME is evaluated before the output
	 * console file is opened. 
	 */
	if (ocf && ocf[0])
	{
		if (hawk_rtx_setofilename(rtx, ocf[0], hawk_count_oocstr(ocf[0])) <= -1)
		{
			hawk_rtx_errortohawk (rtx, awk);
			hawk_rtx_close (rtx);
			return HAWK_NULL;
		}
	}

	if (make_additional_globals(rtx, xtn, id, icf) <= -1)
	{
		hawk_rtx_errortohawk (rtx, awk);
		hawk_rtx_close (rtx);
		return HAWK_NULL;
	}

	return rtx;
}

hawk_rtx_t* hawk_rtx_openstdwithbcstr (
	hawk_t*           awk,
	hawk_oow_t        xtnsize,
	const hawk_bch_t* id,
	const hawk_bch_t* icf[],
	const hawk_bch_t* ocf[],
	hawk_cmgr_t*      cmgr)
{
#if defined(HAWK_OOCH_IS_BCH)
	return open_rtx_std(awk, xtnsize, id, icf, ocf, cmgr);
#else
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_oow_t wcslen, i;
	hawk_uch_t* wid = HAWK_NULL, ** wicf = HAWK_NULL, ** wocf = HAWK_NULL;

	wid = hawk_dupbtoucstr(awk, id, &wcslen, 0);
	if (!wid) return HAWK_NULL;

	if (icf)
	{
		for (i = 0; icf[i]; i++) ;

		wicf = (hawk_uch_t**)hawk_callocmem(awk, HAWK_SIZEOF(*wicf) * (i + 1));
		if (!wicf) goto done;

		for (i = 0; icf[i]; i++)
		{
			wicf[i] = hawk_dupbtoucstr(awk, icf[i], &wcslen, 0);
			if (!wicf[i]) goto done;
		}
		wicf[i] = HAWK_NULL;
	}

	if (ocf)
	{
		for (i = 0; ocf[i]; i++) ;

		wocf = (hawk_uch_t**)hawk_callocmem(awk, HAWK_SIZEOF(*wocf) * (i + 1));
		if (!wocf) goto done;

		for (i = 0; ocf[i]; i++)
		{
			wocf[i] = hawk_dupbtoucstr(awk, ocf[i], &wcslen, 0);
			if (!wocf[i]) goto done;
		}
		wocf[i] = HAWK_NULL;
	}

	rtx = open_rtx_std(awk, xtnsize, wid, (const hawk_uch_t**)wicf, (const hawk_uch_t**)wocf, cmgr);

done:
	if (wocf)
	{
		for (i = 0; wocf[i]; i++) hawk_freemem (awk, wocf[i]);
		hawk_freemem (awk, wocf);
	}
	if (wicf)
	{
		for (i = 0; wicf[i]; i++) hawk_freemem (awk, wicf[i]);
		hawk_freemem (awk, wicf);
	}
	if (wid) hawk_freemem (awk, wid);

	return rtx;
#endif
}

hawk_rtx_t* hawk_rtx_openstdwithucstr (
	hawk_t*           awk,
	hawk_oow_t        xtnsize,
	const hawk_uch_t* id,
	const hawk_uch_t* icf[],
	const hawk_uch_t* ocf[],
	hawk_cmgr_t*      cmgr)
{
#if defined(HAWK_OOCH_IS_BCH)
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_oow_t mbslen, i;
	hawk_bch_t* mid = HAWK_NULL, ** micf = HAWK_NULL, ** mocf = HAWK_NULL;

	mid = hawk_duputobcstr(awk, id, &mbslen);
	if (!mid) return HAWK_NULL;

	if (icf)
	{
		for (i = 0; icf[i]; i++) ;

		micf = (hawk_bch_t**)hawk_callocmem(awk, HAWK_SIZEOF(*micf) * (i + 1));
		if (!micf) goto done;

		for (i = 0; icf[i]; i++)
		{
			micf[i] = hawk_duputobcstr(awk, icf[i], &mbslen);
			if (!micf[i]) goto done;
		}
		micf[i] = HAWK_NULL;
	}

	if (ocf)
	{
		for (i = 0; ocf[i]; i++) ;

		mocf = (hawk_uch_t**)hawk_callocmem(awk, HAWK_SIZEOF(*mocf) * (i + 1));
		if (!mocf) goto done;

		for (i = 0; ocf[i]; i++)
		{
			mocf[i] = hawk_dupbtoucstr(awk, ocf[i], &mbslen);
			if (!mocf[i]) goto done;
		}
		mocf[i] = HAWK_NULL;
	}

	rtx = open_rtx_std(awk, xtnsize, mid, (const hawk_bch_t**)micf, (const hawk_bch_t**)mocf, cmgr);

done:
	if (mocf)
	{
		for (i = 0; mocf[i]; i++) hawk_freemem (awk, mocf[i]);
		hawk_freemem (awk, mocf);
	}
	if (micf)
	{
		for (i = 0; micf[i]; i++) hawk_freemem (awk, micf[i]);
		hawk_freemem (awk, micf);
	}
	if (mid) hawk_freemem (awk, mid);

	return rtx;
#else
	return open_rtx_std(awk, xtnsize, id, icf, ocf, cmgr);
#endif
}

static int timeout_code (const hawk_ooch_t* name)
{
	if (hawk_comp_oocstr(name, HAWK_T("rtimeout"), 1) == 0) return 0;
	if (hawk_comp_oocstr(name, HAWK_T("wtimeout"), 1) == 0) return 1;
	if (hawk_comp_oocstr(name, HAWK_T("ctimeout"), 1) == 0) return 2;
	if (hawk_comp_oocstr(name, HAWK_T("atimeout"), 1) == 0) return 3;
	return -1;
}

static HAWK_INLINE void init_ioattr (ioattr_t* ioattr)
{
	int i;
	HAWK_MEMSET (ioattr, 0, HAWK_SIZEOF(*ioattr));
	for (i = 0; i < HAWK_COUNTOF(ioattr->tmout); i++) 
	{
		/* a negative number for no timeout */
		ioattr->tmout[i].sec = -999;
		ioattr->tmout[i].nsec = 0;
	}
}

static ioattr_t* get_ioattr (
	hawk_htb_t* tab, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search (tab, ptr, len);
	if (pair) return HAWK_HTB_VPTR(pair);

	return HAWK_NULL;
}

static ioattr_t* find_or_make_ioattr (
	hawk_rtx_t* rtx, hawk_htb_t* tab, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search (tab, ptr, len);
	if (pair == HAWK_NULL)
	{
		ioattr_t ioattr;

		init_ioattr (&ioattr);

		pair = hawk_htb_insert (tab, (void*)ptr, len, (void*)&ioattr, HAWK_SIZEOF(ioattr));
		if (pair == HAWK_NULL)
		{
			hawk_rtx_seterrnum (rtx, HAWK_ENOMEM, HAWK_NULL);
		}
	}

	return (ioattr_t*)HAWK_HTB_VPTR(pair);
}

static int fnc_setioattr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	rxtn_t* rxtn;
	hawk_val_t* v[3];
	hawk_ooch_t* ptr[3];
	hawk_oow_t len[3];
	int i, ret = 0, fret = 0;
	int tmout;

	rxtn = GET_RXTN(rtx);
	HAWK_ASSERT (hawk_rtx_getawk(rtx), rxtn->cmgrtab_inited == 1);

	for (i = 0; i < 3; i++)
	{
		v[i] = hawk_rtx_getarg (rtx, i);
		ptr[i] = hawk_rtx_getvaloocstr (rtx, v[i], &len[i]);
		if (ptr[i] == HAWK_NULL) 
		{
			ret = -1;
			goto done;
		}

		if (hawk_find_oochar(ptr[i], len[i], HAWK_T('\0')))
		{
			fret = -1;
			goto done;
		}
	}

	if ((tmout = timeout_code (ptr[1])) >= 0)
	{
		ioattr_t* ioattr;

		hawk_int_t l;
		hawk_flt_t r;
		int x;

		/* no error is returned by hawk_rtx_strnum() if the strict option 
		 * of the second parameter is 0. so i don't check for an error */
		x = hawk_rtx_oocharstonum(rtx, HAWK_RTX_OOCSTRTONUM_MAKE_OPTION(0, 0), ptr[2], len[2], &l, &r);
		if (x == 0) r = (hawk_flt_t)l;

		ioattr = find_or_make_ioattr(rtx, &rxtn->cmgrtab, ptr[0], len[0]);
		if (ioattr == HAWK_NULL) 
		{
			ret = -1;
			goto done;
		}

		if (x == 0)
		{
			ioattr->tmout[tmout].sec = l;
			ioattr->tmout[tmout].nsec = 0;
		}
		else if (x >= 1)
		{
			hawk_flt_t nsec;
			ioattr->tmout[tmout].sec = (hawk_int_t)r;
			nsec = r - ioattr->tmout[tmout].sec;
			ioattr->tmout[tmout].nsec = HAWK_SEC_TO_NSEC(nsec);
		}
	}
#if defined(HAWK_OOCH_IS_UCH)
	else if (hawk_comp_oocstr(ptr[1], HAWK_T("codepage"), 1) == 0 ||
	         hawk_comp_oocstr(ptr[1], HAWK_T("encoding"), 1) == 0)
	{
		ioattr_t* ioattr;
		hawk_cmgr_t* cmgr;

		if (ptr[2][0] == HAWK_T('\0')) 
		{
			cmgr = HAWK_NULL;
		}
		else
		{
			cmgr = hawk_get_cmgr_by_name(ptr[2]);
			if (cmgr == HAWK_NULL) 
			{
				fret = -1;
				goto done;
			}
		}

		ioattr = find_or_make_ioattr(rtx, &rxtn->cmgrtab, ptr[0], len[0]);
		if (ioattr == HAWK_NULL) 
		{
			ret = -1;
			goto done;
		}

		ioattr->cmgr = cmgr;
		hawk_copy_oocstr (ioattr->cmgr_name, HAWK_COUNTOF(ioattr->cmgr_name), ptr[2]);
	}
#endif
	else
	{
		/* unknown attribute name */
		fret = -1;
		goto done;
	}

done:
	while (i > 0)
	{
		i--;
		hawk_rtx_freevaloocstr (rtx, v[i], ptr[i]);
	}

	if (ret >= 0)
	{
		v[0] = hawk_rtx_makeintval (rtx, (hawk_int_t)fret);
		if (v[0] == HAWK_NULL) return -1;
		hawk_rtx_setretval (rtx, v[0]);
	}

	return ret;
}

static int fnc_getioattr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	rxtn_t* rxtn;
	hawk_val_t* v[2];
	hawk_ooch_t* ptr[2];
	hawk_oow_t len[2];
	int i, ret = 0;
	int tmout;
	hawk_val_t* rv = HAWK_NULL;

	ioattr_t* ioattr;
	ioattr_t ioattr_buf;

	rxtn = GET_RXTN(rtx);
	HAWK_ASSERT (hawk_rtx_getawk(rtx), rxtn->cmgrtab_inited == 1);

	for (i = 0; i < 2; i++)
	{
		v[i] = hawk_rtx_getarg (rtx, i);
		ptr[i] = hawk_rtx_getvaloocstr (rtx, v[i], &len[i]);
		if (ptr[i] == HAWK_NULL) 
		{
			ret = -1;
			goto done;
		}

		if (hawk_find_oochar(ptr[i], len[i], HAWK_T('\0'))) goto done;
	}

	ioattr = get_ioattr (&rxtn->cmgrtab, ptr[0], len[0]);
	if (ioattr == HAWK_NULL) 
	{
		init_ioattr (&ioattr_buf);
		ioattr = &ioattr_buf;
	}

	if ((tmout = timeout_code (ptr[1])) >= 0)
	{
		if (ioattr->tmout[tmout].nsec == 0)
			rv = hawk_rtx_makeintval(rtx, ioattr->tmout[tmout].sec);
		else
			rv = hawk_rtx_makefltval(rtx, (hawk_flt_t)ioattr->tmout[tmout].sec + HAWK_NSEC_TO_SEC((hawk_flt_t)ioattr->tmout[tmout].nsec));
		if (rv == HAWK_NULL) 
		{
			ret = -1;
			goto done;
		}
	}
#if defined(HAWK_OOCH_IS_UCH)
	else if (hawk_comp_oocstr(ptr[1], HAWK_T("codepage"), 1) == 0 ||
	         hawk_comp_oocstr(ptr[1], HAWK_T("encoding"), 1) == 0)
	{
		rv = hawk_rtx_makestrvalwithoocstr(rtx, ioattr->cmgr_name);
		if (rv == HAWK_NULL)
		{
			ret = -1;
			goto done;
		}
	}
#endif
	else
	{
		/* unknown attribute name */
		goto done;
	}

done:
	while (i > 0)
	{
		i--;
		hawk_rtx_freevaloocstr (rtx, v[i], ptr[i]);
	}

	if (ret >= 0)
	{
		if (rv)
		{
			hawk_rtx_refupval (rtx, rv);
			ret = hawk_rtx_setrefval (rtx, (hawk_val_ref_t*)hawk_rtx_getarg (rtx, 2), rv);
			hawk_rtx_refdownval (rtx, rv);
			if (ret >= 0) hawk_rtx_setretval (rtx, HAWK_VAL_ZERO);
		}
		else 
		{
			hawk_rtx_setretval (rtx, HAWK_VAL_NEGONE);
		}

	}

	return ret;
}

hawk_cmgr_t* hawk_rtx_getiocmgrstd (hawk_rtx_t* rtx, const hawk_ooch_t* ioname)
{
#if defined(HAWK_OOCH_IS_UCH)
	rxtn_t* rxtn = GET_RXTN(rtx);
	ioattr_t* ioattr;

	HAWK_ASSERT (hawk_rtx_getawk(rtx), rxtn->cmgrtab_inited == 1);

	ioattr = get_ioattr(&rxtn->cmgrtab, ioname, hawk_count_oocstr(ioname));
	if (ioattr) return ioattr->cmgr;
#endif
	return HAWK_NULL;
}

static int add_globals (hawk_t* awk)
{
	xtn_t* xtn = GET_XTN(awk);

	xtn->gbl_argc = hawk_addgbl(awk, HAWK_T("ARGC"));
	xtn->gbl_argv = hawk_addgbl(awk, HAWK_T("ARGV"));
	xtn->gbl_environ = hawk_addgbl(awk,  HAWK_T("ENVIRON"));

	return (xtn->gbl_argc <= -1 || 
	        xtn->gbl_argv <= -1 ||
	        xtn->gbl_environ <= -1)? -1: 0;
}

struct fnctab_t 
{
	const hawk_ooch_t* name;
	hawk_fnc_spec_t spec;
};

static struct fnctab_t fnctab[] =
{
	/* additional aliases to module functions */
	{ HAWK_T("rand"),      { {1, 0, HAWK_T("math")},  HAWK_NULL,      0           } },
	{ HAWK_T("srand"),     { {1, 0, HAWK_T("math")},  HAWK_NULL,      0           } },
	{ HAWK_T("system"),    { {1, 0, HAWK_T("sys")},   HAWK_NULL ,     0           } }, 

	/* additional functions */
	{ HAWK_T("setioattr"), { {3, 3, HAWK_NULL},       fnc_setioattr, HAWK_RIO } },
	{ HAWK_T("getioattr"), { {3, 3, HAWK_T("vvr")},   fnc_getioattr, HAWK_RIO } }
};

static int add_functions (hawk_t* awk)
{
	int i;

	for (i = 0; i < HAWK_COUNTOF(fnctab); i++)
	{
		if (hawk_addfnc (awk, fnctab[i].name, &fnctab[i].spec) == HAWK_NULL) return -1;
	}

	return 0;
}

