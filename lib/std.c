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

#include "hawk-prv.h"
#include <hawk-std.h>
#include <hawk-pio.h>
#include <hawk-sio.h>
#include <hawk-xma.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#if defined(HAVE_QUADMATH_H)
#	include <quadmath.h>
#elif defined(HAWK_USE_FLTMAX) && (HAWK_SIZEOF_FLT_T == 16) && defined(HAWK_FLTMAX_REQUIRE_QUADMATH)
	/* the header file doesn't exist while the library is available */
	extern __float128 powq (__float128, __float128);
	extern __float128 fmodq (__float128, __float128);
#endif

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#	if defined(HAWK_HAVE_CFG_H) && defined(HAWK_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	endif
#	include <io.h>
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

#	if defined(HAVE_CRT_EXTERNS_H)
#		include <crt_externs.h> /* MacOSX/darwin. _NSGetEnviron() */
#	endif
#endif

#if !defined(HAWK_HAVE_CFG_H)
#	if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#		define HAVE_POW
#		define HAVE_FMOD
#	endif
#endif

enum logfd_flag_t
{
	LOGFD_TTY = (1 << 0),
	LOGFD_OPENED_HERE = (1 << 1)
};

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

	struct
	{
		int fd;
		int fd_flag; /* bitwise OR'ed fo logfd_flag_t bits */

		struct
		{
			hawk_bch_t buf[4096];
 			hawk_oow_t len;
		} out;
	} log;
} xtn_t;

typedef struct rxtn_t
{
	struct
	{
		struct
		{
			hawk_ooch_t** files;
			hawk_oow_t index;
			hawk_oow_t count; /* number of files opened so far */
		} in;

		struct
		{
			hawk_ooch_t** files;
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
static HAWK_INLINE xtn_t* GET_XTN(hawk_t* hawk) { return (xtn_t*)((hawk_uint8_t*)hawk_getxtn(hawk) - HAWK_SIZEOF(xtn_t)); }
static HAWK_INLINE rxtn_t* GET_RXTN(hawk_rtx_t* rtx) { return (rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)); }
#else
#define GET_XTN(hawk) ((xtn_t*)((hawk_uint8_t*)hawk_getxtn(hawk) - HAWK_SIZEOF(xtn_t)))
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


hawk_mmgr_t* hawk_get_sys_mmgr (void)
{
	return &sys_mmgr;
}

/* ----------------------------------------------------------------------- */


static void* xma_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	return hawk_xma_alloc(mmgr->ctx, size);
}

static void* xma_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	return hawk_xma_realloc(mmgr->ctx, ptr, size);
}

static void xma_free (hawk_mmgr_t* mmgr, void* ptr)
{
	hawk_xma_free(mmgr->ctx, ptr);
}

int hawk_init_xma_mmgr (hawk_mmgr_t* mmgr, hawk_oow_t memlimit)
{
	hawk_xma_t* xma;

	xma = hawk_xma_open(hawk_get_sys_mmgr(), 0, HAWK_NULL, memlimit);
	if (HAWK_UNLIKELY(!xma)) return -1;

	HAWK_MEMSET(mmgr, 0, HAWK_SIZEOF(*mmgr));
	mmgr->alloc = xma_alloc;
	mmgr->realloc = xma_realloc;
	mmgr->free = xma_free;
	mmgr->ctx = xma;

	return 0;
}

void hawk_fini_xma_mmgr (hawk_mmgr_t* mmgr)
{
	if (mmgr->ctx) hawk_xma_close(mmgr->ctx);
	mmgr->ctx = HAWK_NULL;
}

/* ----------------------------------------------------------------------- */

hawk_flt_t hawk_stdmathpow (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y)
{
#if defined(HAWK_USE_FLTMAX) && defined(HAVE_POWQ)
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

hawk_flt_t hawk_stdmathmod (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y)
{
#if defined(HAWK_USE_FLTMAX) && defined(HAVE_FMODQ)
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
 * area of 'hawk'. they are used in StdAwk.cpp which instantiates
 * an hawk object with hawk_open() instead of hawk_openstd(). */

int hawk_stdmodstartup (hawk_t* hawk)
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

void hawk_stdmodshutdown (hawk_t* hawk)
{
#if defined(USE_LTDL)
	lt_dlexit ();
#else
	/* do nothing */
#endif
}

static void* std_mod_open_checked (hawk_t* hawk, const hawk_mod_spec_t* spec)
{
	xtn_t* xtn = GET_XTN(hawk);

	if (!xtn->stdmod_up)
	{
		/* hawk_stdmodstartup() must have failed upon start-up.
		 * return failure immediately */
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOIMPL);
		return HAWK_NULL;
	}

	return hawk_stdmodopen(hawk, spec);
}

void* hawk_stdmodopen (hawk_t* hawk, const hawk_mod_spec_t* spec)
{
#if defined(USE_LTDL)
	if (spec)
	{
		void* h;
		lt_dladvise adv;
		hawk_bch_t* modpath;
		const hawk_ooch_t* tmp[6];
		int count;
		static hawk_ooch_t ds[] = { '/', '\0' };

		count = 0;
		if (spec->libdir)
		{
			hawk_oow_t len;

			tmp[count++] = spec->libdir;
			len = hawk_count_oocstr(spec->libdir);
			if (len > 0 && spec->libdir[len - 1] != '/') tmp[count++] = ds;
		}
		if (spec->prefix) tmp[count++] = spec->prefix;
		tmp[count++] = spec->name;
		if (spec->postfix) tmp[count++] = spec->postfix;
		tmp[count] = HAWK_NULL;

		#if defined(HAWK_OOCH_IS_BCH)
		modpath = hawk_dupbcstrarr(hawk, tmp, HAWK_NULL);
		#else
		modpath = hawk_dupucstrarrtobcstr(hawk, tmp, HAWK_NULL)
		#endif
		if (!modpath) return HAWK_NULL;

		if (lt_dladvise_init(&adv) != 0)
		{
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), lt_dlerror());
			return HAWK_NULL;
		}

		lt_dladvise_ext (&adv);
		/*lt_dladvise_resident (&adv); useful for debugging with valgrind */

		h = lt_dlopenadvise(modpath, adv);
		if (!h) hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), lt_dlerror());

		lt_dladvise_destroy (&adv);

		if (modpath) hawk_freemem(hawk, modpath);
		return h;
	}
	else
	{
		void* h;
		h = lt_dlopen(HAWK_NULL);
		if (HAWK_UNLIKELY(!h)) hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), lt_dlerror());
		return h;
	}

#elif defined(_WIN32)

	if (spec)
	{
		HMODULE h;
		hawk_ooch_t* modpath;
		const hawk_ooch_t* tmp[6];
		int count;
		static hawk_ooch_t ds[][2] = { { '\\', '\0' }, { '/', '\0' } };

		count = 0;
		if (spec->libdir)
		{
			hawk_oow_t len;

			tmp[count++] = spec->libdir;
			len = hawk_count_oocstr(spec->libdir);
			if (len > 0 && (spec->libdir[len - 1] != '/' && spec->libdir[len - 1] != '\\'))
			{
				tmp[count++] = ds[hawk_find_oochar_in_oocstr(spec->libdir, '/') != HAWK_NULL];
			}
		}
		if (spec->prefix) tmp[count++] = spec->prefix;
		tmp[count++] = spec->name;
		if (spec->postfix) tmp[count++] = spec->postfix;
		tmp[count] = HAWK_NULL;

		modpath = hawk_dupoocstrarr(hawk, tmp, HAWK_NULL);
		if (!modpath) return HAWK_NULL;

		h = LoadLibrary(modpath);
		if (!h) hawk_seterrnum(hawk, HAWK_NULL, hawk_syserr_to_errnum(GetLastError());

		hawk_freemem(hawk, modpath);

		HAWK_ASSERT (HAWK_SIZEOF(h) <= HAWK_SIZEOF(void*));
		return h;
	}
	else
	{
		HMODULE h;
		h = GetModuleHandle(HAWK_NULL);
		if (!h) hawk_seterrnum(hawk, HAWK_NULL, hawk_syserr_to_errnum(GetLastError());
		return h;
	}

#elif defined(__OS2__)

	if (spec)
	{
		HMODULE h;
		hawk_bch_t* modpath;
		const hawk_ooch_t* tmp[6];
		int count;
		char errbuf[CCHMAXPATH];
		APIRET rc;
		static hawk_ooch_t ds[] = { '\\', '\0' };

		count = 0;
		if (spec->libdir)
		{
			hawk_oow_t len;

			tmp[count++] = spec->libdir;
			len = hawk_count_oocstr(spec->libdir);
			if (len > 0 && spec->libdir[len - 1] != '\\') tmp[count++] = ds;
		}
		if (spec->prefix) tmp[count++] = spec->prefix;
		tmp[count++] = spec->name;
		if (spec->postfix) tmp[count++] = spec->postfix;
		tmp[count] = HAWK_NULL;

		#if defined(HAWK_OOCH_IS_BCH)
		modpath = hawk_dupbcstrarr(hawk, tmp, HAWK_NULL);
		#else
		modpath = hawk_dupucstrarrtobcstr(hawk, tmp, HAWK_NULL);
		#endif
		if (HAWK_UNLIKELY(!modpath)) return HAWK_NULL;

		/* DosLoadModule() seems to have severe limitation on
		 * the file name it can load (max-8-letters.xxx) */
		rc = DosLoadModule(errbuf, HAWK_COUNTOF(errbuf) - 1, modpath, &h);
		if (rc != NO_ERROR)
		{
			h = HAWK_NULL;
			hawk_seterrnum(hawk, HAWK_NULL, hawk_syserr_to_errnum(rc));
		}

		hawk_freemem(hawk, modpath);

		HAWK_ASSERT (HAWK_SIZEOF(h) <= HAWK_SIZEOF(void*));
		return h;
	}
	else
	{
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOIMPL);
		return HAWK_NULL;
	}

#elif defined(__DOS__) && defined(HAWK_ENABLE_DOS_DYNAMIC_MODULE)

	if (spec)
	{
		/* the DOS code here is not generic enough. it's for a specific
		 * dos-extender only. the best is not to use dynamic loading
		 * when building for DOS. */
		void* h;
		hawk_bch_t* modpath;
		const hawk_ooch_t* tmp[6];
		int count;
		static hawk_ooch_t ds[] = { '\\', '\0' };

		count = 0;
		if (spec->libdir)
		{
			hawk_oow_t len;

			tmp[count++] = spec->libdir;
			len = hawk_count_oocstr(spec->libdir);
			if (len > 0 && spec->libdir[len - 1] != '/') tmp[count++] = ds;
		}
		if (spec->prefix) tmp[count++] = spec->prefix;
		tmp[count++] = spec->name;
		if (spec->postfix) tmp[count++] = spec->postfix;
		tmp[count] = HAWK_NULL;

		#if defined(HAWK_OOCH_IS_BCH)
		modpath = hawk_dupbcstrarr(hawk, tmp, HAWK_NULL);
		#else
		modpath = hawk_dupucstrarrtobcstr(hawk, tmp, HAWK_NULL);
		#endif
		if (!modpath) return HAWK_NULL;

		h = LoadModule(modpath);
		if (!h) hawk_seterrnum(hawk, HAWK_NULL, HAWK_ESYSERR);

		hawk_freemem(hawk, modpath);

		HAWK_ASSERT (HAWK_SIZEOF(h) <= HAWK_SIZEOF(void*));
		return h;
	}
	else
	{
		void* h;
		h = GetModuleHandle(HAWK_NULL);
		if (!h) hawk_seterrnum(hawk, HAWK_NULL, HAWK_ESYSERR);
		return h;
	}

#elif defined(USE_DLFCN)
	if (spec)
	{
		void* h;
		hawk_bch_t* modpath;
		const hawk_ooch_t* tmp[6];
		int count;
		static hawk_ooch_t ds[] = { '/', '\0' };

		count = 0;
		if (spec->libdir)
		{
			hawk_oow_t len;

			tmp[count++] = spec->libdir;
			len = hawk_count_oocstr(spec->libdir);
			if (len > 0 && spec->libdir[len - 1] != '/') tmp[count++] = ds;
		}
		if (spec->prefix) tmp[count++] = spec->prefix;
		tmp[count++] = spec->name;
		if (spec->postfix) tmp[count++] = spec->postfix;
		tmp[count] = HAWK_NULL;

		#if defined(HAWK_OOCH_IS_BCH)
		modpath = hawk_dupbcstrarr(hawk, tmp, HAWK_NULL);
		#else
		modpath = hawk_dupucstrarrtobcstr(hawk, tmp, HAWK_NULL);
		#endif
		if (!modpath) return HAWK_NULL;

		h = dlopen(modpath, RTLD_NOW | RTLD_LOCAL);
		if (!h) hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), dlerror());

		hawk_freemem(hawk, modpath);

		return h;
	}
	else
	{
		void* h;
		h = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
		if (!h) hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), dlerror());
		return h;
	}
#else
	hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOIMPL);
	return HAWK_NULL;
#endif
}

void hawk_stdmodclose (hawk_t* hawk, void* handle)
{
#if defined(USE_LTDL)
	lt_dlclose (handle);
#elif defined(_WIN32)
	if (handle != GetModuleHandle(HAWK_NULL)) FreeLibrary ((HMODULE)handle);
#elif defined(__OS2__)
	DosFreeModule ((HMODULE)handle);
#elif defined(__DOS__) && defined(HAWK_ENABLE_DOS_DYNAMIC_MODULE)
	if (handle != GetModuleHandle(HAWK_NULL))  FreeModule (handle);
#elif defined(USE_DLFCN)
	dlclose (handle);
#else
	/* nothing to do */
#endif
}

void* hawk_stdmodgetsym (hawk_t* hawk, void* handle, const hawk_ooch_t* name)
{
	void* s;
	hawk_bch_t* mname;

#if defined(HAWK_OOCH_IS_BCH)
	mname = (hawk_bch_t*)name;
#else
	mname = hawk_duputobcstr(hawk, name, HAWK_NULL);
	if (!mname) return HAWK_NULL;
#endif

#if defined(USE_LTDL)
	s = lt_dlsym(handle, mname);
	if (!s) hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), lt_dlerror());

#elif defined(_WIN32)
	s = GetProcAddress((HMODULE)handle, mname);
	if (!s) hawk_seterrnum(hawk, HAWK_NULL, hawk_syserr_to_errnum(GetLastError());

#elif defined(__OS2__)
	{
		APIRET rc;
		rc = DosQueryProcAddr((HMODULE)handle, 0, mname, (PFN*)&s);
		if (rc != NO_ERROR)
		{
			s = HAWK_NULL;
			hawk_seterrnum(hawk, HAWK_NULL, hawk_syserr_to_errnum(rc));
		}
	}

#elif defined(__DOS__) && defined(HAWK_ENABLE_DOS_DYNAMIC_MODULE)
	s = GetProcAddress(handle, mname);
	if (!s) hawk_seterrnum(hawk, HAWK_NULL, HAWK_ESYSERR);

#elif defined(USE_DLFCN)
	s = dlsym(handle, mname);
	if (!s) hawk_seterrfmt(hawk, HAWK_NULL, HAWK_ESYSERR, HAWK_T("%hs"), dlerror());

#else
	s = HAWK_NULL;
#endif

#if defined(HAWK_OOCH_IS_BCH)
	/* nothing to do */
#else
	hawk_freemem(hawk, mname);
#endif

	return s;
}

/* ----------------------------------------------------------------------- */

static HAWK_INLINE void reset_log_to_default (xtn_t* xtn)
{
#if defined(ENABLE_LOG_INITIALLY)
	xtn->log.fd = 2;
	xtn->log.fd_flag = 0;
	#if defined(HAVE_ISATTY)
	if (isatty(xtn->log.fd)) xtn->log.fd_flag |= LOGFD_TTY;
	#endif
#else
	xtn->log.fd = -1;
	xtn->log.fd_flag = 0;
#endif
}

#if defined(EMSCRIPTEN)
EM_JS(int, write_all, (int, const hawk_bch_t* ptr, hawk_oow_t len), {
	// UTF8ToString() doesn't handle a null byte in the middle of an array.
	// Use the heap memory and pass the right portion to UTF8Decoder.
	//console.log ("%s", UTF8ToString(ptr, len));
	console.log ("%s", UTF8Decoder.decode(HEAPU8.subarray(ptr, ptr + len)));
	return 0;
});

#else

static int write_all (int fd, const hawk_bch_t* ptr, hawk_oow_t len)
{
#if defined(EMSCRIPTEN)
	EM_ASM_ ({
		// UTF8ToString() doesn't handle a null byte in the middle of an array.
		// Use the heap memory and pass the right portion to UTF8Decoder.
		//console.log ("%s", UTF8ToString($0, $1));
		console.log ("%s", UTF8Decoder.decode(HEAPU8.subarray($0, $0 + $1)));
	}, ptr, len);
#else
	while (len > 0)
	{
		hawk_ooi_t wr;

		wr = write(fd, ptr, len);

		if (wr <= -1)
		{
		#if defined(EAGAIN) && defined(EWOULDBLOCK) && (EAGAIN == EWOULDBLOCK)
			if (errno == EAGAIN) continue;
		#else
			#if defined(EAGAIN)
			if (errno == EAGAIN) continue;
			#elif defined(EWOULDBLOCK)
			if (errno == EWOULDBLOCK) continue;
			#endif
		#endif

		#if defined(EINTR)
			/* TODO: would this interfere with non-blocking nature of this VM? */
			if (errno == EINTR) continue;
		#endif
			return -1;
		}

		ptr += wr;
		len -= wr;
	}
#endif

	return 0;
}
#endif

static int write_log (hawk_t* hawk, int fd, const hawk_bch_t* ptr, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(hawk);

	while (len > 0)
	{
		if (xtn->log.out.len > 0)
		{
			hawk_oow_t rcapa, cplen;

			rcapa = HAWK_COUNTOF(xtn->log.out.buf) - xtn->log.out.len;
			cplen = (len >= rcapa)? rcapa: len;

			HAWK_MEMCPY(&xtn->log.out.buf[xtn->log.out.len], ptr, cplen);
			xtn->log.out.len += cplen;
			ptr += cplen;
			len -= cplen;

			if (xtn->log.out.len >= HAWK_COUNTOF(xtn->log.out.buf))
			{
				int n;
				n = write_all(fd, xtn->log.out.buf, xtn->log.out.len);
				xtn->log.out.len = 0;
				if (n <= -1) return -1;
			}
		}
		else
		{
			hawk_oow_t rcapa;

			rcapa = HAWK_COUNTOF(xtn->log.out.buf);
			if (len >= rcapa)
			{
				if (write_all(fd, ptr, rcapa) <= -1) return -1;
				ptr += rcapa;
				len -= rcapa;
			}
			else
			{
				HAWK_MEMCPY(xtn->log.out.buf, ptr, len);
				xtn->log.out.len += len;
				ptr += len;
				len -= len;
			}
		}
	}

	return 0;
}

static void flush_log (hawk_t* hawk, int fd)
{
	xtn_t* xtn = GET_XTN(hawk);
	if (xtn->log.out.len > 0)
	{
		write_all (fd, xtn->log.out.buf, xtn->log.out.len);
		xtn->log.out.len = 0;
	}
}

static void log_write (hawk_t* hawk, hawk_bitmask_t mask, const hawk_ooch_t* msg, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(hawk);
	hawk_bch_t buf[256];
	hawk_oow_t ucslen, bcslen, msgidx;
	int n, logfd;

	if (mask & HAWK_LOG_STDERR) logfd = 2;
	else if (mask & HAWK_LOG_STDOUT) logfd = 1;
	else
	{
		logfd = xtn->log.fd;
#if !defined(EMSCRIPTEN)
		if (logfd <= -1)
		{
			return;
		}
#endif
	}

/* TODO: beautify the log message.
 *       do classification based on mask. */
	if (!(mask & (HAWK_LOG_STDOUT | HAWK_LOG_STDERR)))
	{
	#if defined(_WIN32)
		TIME_ZONE_INFORMATION tzi;
		SYSTEMTIME now;
	#else
		time_t now;
		struct tm tm, *tmp;
	#endif
	#if defined(HAWK_OOCH_IS_UCH)
		char ts[32 * HAWK_BCSIZE_MAX];
	#else
		char ts[32];
	#endif
		hawk_oow_t tslen;

	#if defined(_WIN32)
		/* %z for strftime() in win32 seems to produce a long non-numeric timezone name.
		 * i don't use strftime() for time formatting. */
		GetLocalTime (&now);
		if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID)
		{
			tzi.Bias = -tzi.Bias;
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d %+02.2d%02.2d ",
				(int)now.wYear, (int)now.wMonth, (int)now.wDay,
				(int)now.wHour, (int)now.wMinute, (int)now.wSecond,
				(int)(tzi.Bias / 60), (int)(tzi.Bias % 60));
		}
		else
		{
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ",
				(int)now.wYear, (int)now.wMonth, (int)now.wDay,
				(int)now.wHour, (int)now.wMinute, (int)now.wSecond);
		}
	#elif defined(__OS2__)
		now = time(HAWK_NULL);

		#if defined(__WATCOMC__)
		tmp = _localtime(&now, &tm);
		#else
		tmp = localtime(&now);
		#endif

		#if defined(__BORLANDC__)
		/* the borland compiler doesn't handle %z properly - it showed 00 all the time */
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp);
		#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#endif
		if (tslen == 0)
		{
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}

	#elif defined(__DOS__)
		now = time(HAWK_NULL);
		tmp = localtime(&now);
		/* since i know that %z/%Z is not available in strftime, i switch to sprintf immediately */
		tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	#else
		now = time(HAWK_NULL);
		#if defined(HAVE_LOCALTIME_R)
		tmp = localtime_r(&now, &tm);
		#else
		tmp = localtime(&now);
		#endif

		#if defined(HAVE_STRFTIME_SMALL_Z)
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z ", tmp);
		#else
		tslen = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %Z ", tmp);
		#endif
		if (tslen == 0)
		{
			tslen = sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d ", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}
	#endif

		write_log(hawk, logfd, ts, tslen);
	}

	if (logfd == xtn->log.fd && (xtn->log.fd_flag & LOGFD_TTY))
	{
		if (mask & HAWK_LOG_FATAL) write_log(hawk, logfd, "\x1B[1;31m", 7);
		else if (mask & HAWK_LOG_ERROR) write_log(hawk, logfd, "\x1B[1;32m", 7);
		else if (mask & HAWK_LOG_WARN) write_log(hawk, logfd, "\x1B[1;33m", 7);
	}

#if defined(HAWK_OOCH_IS_UCH)
	msgidx = 0;
	while (len > 0)
	{
		ucslen = len;
		bcslen = HAWK_COUNTOF(buf);

		/*n = hawk_convootobchars(hawk, &msg[msgidx], &ucslen, buf, &bcslen);*/
		n = hawk_conv_uchars_to_bchars_with_cmgr(&msg[msgidx], &ucslen, buf, &bcslen, hawk_getcmgr(hawk));
		if (n == 0 || n == -2)
		{
			/* n = 0:
			 *   converted all successfully
			 * n == -2:
			 *    buffer not sufficient. not all got converted yet.
			 *    write what have been converted this round. */

			HAWK_ASSERT (ucslen > 0); /* if this fails, the buffer size must be increased */

			/* attempt to write all converted characters */
			if (write_log(hawk, logfd, buf, bcslen) <= -1) break;

			if (n == 0) break;
			else
			{
				msgidx += ucslen;
				len -= ucslen;
			}
		}
		else if (n <= -1)
		{
			/* conversion error but i just stop here but don't treat it as a hard error. */
			break;
		}
	}
#else
	write_log(hawk, logfd, msg, len);
#endif

	if (logfd == xtn->log.fd && (xtn->log.fd_flag & LOGFD_TTY))
	{
		if (mask & (HAWK_LOG_FATAL | HAWK_LOG_ERROR | HAWK_LOG_WARN)) write_log(hawk, logfd, "\x1B[0m", 4);
	}

	flush_log(hawk, logfd);
}
/* ----------------------------------------------------------------------- */

static int add_globals (hawk_t* hawk);
static int add_functions (hawk_t* hawk);

hawk_t* hawk_openstd (hawk_oow_t xtnsize, hawk_errinf_t* errinf)
{
	return hawk_openstdwithmmgr(&sys_mmgr, xtnsize, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), errinf);
}

static void fini_xtn (hawk_t* hawk, void* ctx)
{
	xtn_t* xtn = GET_XTN(hawk);
	if (xtn->stdmod_up)
	{
		hawk_stdmodshutdown (hawk);
		xtn->stdmod_up = 0;
	}

	if ((xtn->log.fd_flag & LOGFD_OPENED_HERE) && xtn->log.fd >= 0) close (xtn->log.fd);
	reset_log_to_default (xtn);
}

static void clear_xtn (hawk_t* hawk, void* ctx)
{
	/* nothing to do */
}

hawk_t* hawk_openstdwithmmgr (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	hawk_t* hawk;
	hawk_prm_t prm;
	xtn_t* xtn;

	prm.math.pow = hawk_stdmathpow;
	prm.math.mod = hawk_stdmathmod;

	prm.modopen = std_mod_open_checked;
	prm.modclose = hawk_stdmodclose;
	prm.modgetsym = hawk_stdmodgetsym;

	prm.logwrite = log_write;

	if (!mmgr) mmgr = &sys_mmgr;
	if (!cmgr) cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	/* create an object */
	hawk = hawk_open(mmgr, HAWK_SIZEOF(xtn_t) + xtnsize, cmgr, &prm, errinf);
	if (HAWK_UNLIKELY(!hawk)) return HAWK_NULL;

	/* adjust the object size by the sizeof xtn_t so that hawk_getxtn() returns the right pointer. */
	hawk->_instsize += HAWK_SIZEOF(xtn_t);

/*
#if defined(USE_DLFCN)
	if (hawk_setopt(hawk, HAWK_OPT_MODPOSTFIX, HAWK_T(".so")) <= -1)
	{
		if (errinf) hawk_geterrinf(hawk, errinf);
		goto oops;
	}
#endif
*/

	/* initialize extension */
	xtn = GET_XTN(hawk);

	reset_log_to_default (xtn);

	/* add intrinsic global variables and functions */
	if (add_globals(hawk) <= -1 || add_functions(hawk) <= -1)
	{
		if (errinf) hawk_geterrinf(hawk, errinf);
		goto oops;
	}

	if (hawk_stdmodstartup(hawk) <= -1)
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
	xtn->ecb.ctx = HAWK_NULL;
	hawk_pushecb(hawk, &xtn->ecb);

	return hawk;

oops:
	if (hawk) hawk_close(hawk);
	return HAWK_NULL;
}

static hawk_sio_t* open_sio (hawk_t* hawk, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;
	sio = hawk_sio_open(hawk_getgem(hawk), 0, file, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), file, bem);
	}
	return sio;
}

static hawk_sio_t* open_sio_rtx (hawk_rtx_t* rtx, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;
	sio = hawk_sio_open(hawk_rtx_getgem(rtx), 0, file, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* bem = hawk_rtx_backuperrmsg(rtx);
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), file, bem);
	}
	return sio;
}

static hawk_oocs_t sio_std_names[] =
{
	{ HAWK_T("stdin"),   5 },
	{ HAWK_T("stdout"),  6 },
	{ HAWK_T("stderr"),  6 }
};

static hawk_sio_t* open_sio_std (hawk_t* hawk, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	sio = hawk_sio_openstd(hawk_getgem(hawk), 0, std, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
		hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), sio_std_names[std].ptr, bem);
	}
	return sio;
}

static hawk_sio_t* open_sio_std_rtx (hawk_rtx_t* rtx, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;

	sio = hawk_sio_openstd(hawk_rtx_getgem(rtx), 0, std, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* bem = hawk_rtx_backuperrmsg(rtx);
		hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), sio_std_names[std].ptr, bem);
	}
	return sio;
}

/*** PARSESTD ***/

static int is_psin_file (hawk_parsestd_t* psin)
{
	return psin->type == HAWK_PARSESTD_FILE ||
	       psin->type == HAWK_PARSESTD_FILEB ||
	       psin->type == HAWK_PARSESTD_FILEU;
}

static int open_parsestd (hawk_t* hawk, hawk_sio_arg_t* arg, xtn_t* xtn, hawk_oow_t index)
{
	hawk_parsestd_t* psin = &xtn->s.in.x[index];

	switch (psin->type)
	{
		/* normal source files */
	#if defined(HAWK_OOCH_IS_BCH)
		case HAWK_PARSESTD_FILE:
			HAWK_ASSERT (&psin->u.fileb.path == &psin->u.file.path);
			HAWK_ASSERT (&psin->u.fileb.cmgr == &psin->u.file.cmgr);
	#endif
		case HAWK_PARSESTD_FILEB:
		{
			hawk_sio_t* tmp;
			hawk_ooch_t* path = HAWK_NULL;

			if (psin->u.fileb.path == HAWK_NULL || (psin->u.fileb.path[0] == '-' && psin->u.fileb.path[1] == '\0'))
			{
				/* no path name or - -> stdin */
				path = HAWK_NULL;
				tmp = open_sio_std(hawk, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			else
			{
				path = hawk_addsionamewithbchars(hawk, psin->u.fileb.path, hawk_count_bcstr(psin->u.fileb.path));
				if (HAWK_UNLIKELY(!path)) return -1;
				tmp = open_sio(hawk, path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
			}
			if (tmp == HAWK_NULL) return -1;

			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1])) hawk_sio_close (arg->handle);

			arg->handle = tmp;
			arg->path = path;
			if (psin->u.fileb.cmgr) hawk_sio_setcmgr (arg->handle, psin->u.fileb.cmgr);
			return 0;
		}

	#if defined(HAWK_OOCH_IS_UCH)
		case HAWK_PARSESTD_FILE:
			HAWK_ASSERT (&psin->u.fileu.path == &psin->u.file.path);
			HAWK_ASSERT (&psin->u.fileu.cmgr == &psin->u.file.cmgr);
	#endif
		case HAWK_PARSESTD_FILEU:
		{
			hawk_sio_t* tmp;
			hawk_ooch_t* path;

			if (psin->u.fileu.path == HAWK_NULL || (psin->u.fileu.path[0] == '-' && psin->u.fileu.path[1] == '\0'))
			{
				/* no path name or - -> stdin */
				path = HAWK_NULL;
				tmp = open_sio_std(hawk, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			else
			{
				path = hawk_addsionamewithuchars(hawk, psin->u.fileu.path, hawk_count_ucstr(psin->u.fileu.path));
				if (!path) return -1;
				tmp = open_sio(hawk, path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
			}
			if (tmp == HAWK_NULL) return -1;

			if (index >= 1 && is_psin_file(&xtn->s.in.x[index - 1])) hawk_sio_close (arg->handle);

			arg->handle = tmp;
			arg->path = path;
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
			hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
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
	hawk_freemem(hawk, bpath);
	if (x <= -1) return -1;
#endif

	tmp.ino = st.st_ino;
	tmp.dev = st.st_dev;

	HAWK_MEMCPY(arg->unique_id, &tmp, (HAWK_SIZEOF(tmp) > HAWK_SIZEOF(arg->unique_id)? HAWK_SIZEOF(arg->unique_id): HAWK_SIZEOF(tmp)));
	return 0;
#endif
}

int hawk_stdplainfileexists (hawk_t* hawk, const hawk_ooch_t* file)
{
#if defined(_WIN32)
	DWORD attr;

	attr = GetFileAttributes(file);
	if (attr == INVALID_FILE_ATTRIBUTES) return 0;
	if (attr & FILE_ATTRIBUTE_DIRECTORY) return 0; /* not a plain file. it's a directory */
	return 1;
#else

	struct stat st;
#if defined(HAWK_OOCH_IS_UCH)
	hawk_bch_t* tmp;
	int n;
	tmp = hawk_duputobcstr(hawk, file, HAWK_NULL);
	if (!tmp) return 0;
	n = stat(tmp, &st);
	hawk_freemem(hawk, tmp);
	if (n == -1) return 0;
#else
	if (stat(file, &st) == -1) return 0;
#endif
	if (S_ISDIR(st.st_mode)) return 0; /* not a plain file, it's a directory */
	return 1;
#endif
}

const hawk_ooch_t* hawk_stdgetfileindirs (hawk_t* hawk, const hawk_oocs_t* dirs, const hawk_ooch_t* file)
{
	xtn_t* xtn = GET_XTN(hawk);
	const hawk_ooch_t* ptr = dirs->ptr;
	const hawk_ooch_t* dirend = dirs->ptr + dirs->len;
	const hawk_ooch_t* colon, * endptr;

	while (1)
	{
		colon = hawk_find_oochar_in_oocstr(ptr, ':');
		endptr = colon? colon: dirend;

		if (hawk_copyoocharstosbuf(hawk, ptr, endptr - ptr, HAWK_SBUF_ID_TMP_3) <= -1 ||
		    hawk_concatoochartosbuf(hawk, '/', 1, HAWK_SBUF_ID_TMP_3) <= -1 ||
		    hawk_concatoocstrtosbuf(hawk, file, HAWK_SBUF_ID_TMP_3) <= -1) break;

		if (hawk_stdplainfileexists(hawk, hawk->sbuf[HAWK_SBUF_ID_TMP_3].ptr))
		{
			return hawk->sbuf[HAWK_SBUF_ID_TMP_3].ptr;
		}

		if (!colon) break;
		ptr = colon + 1;
	}

	return HAWK_NULL;
}

static hawk_ooi_t sf_in_open (hawk_t* hawk, hawk_sio_arg_t* arg, xtn_t* xtn)
{
	if (arg->prev == HAWK_NULL)
	{
		/* handle top-level source input stream specified to hawk_parsestd() */
		hawk_ooi_t x;

		HAWK_ASSERT (arg == &hawk->sio.arg);

		x = open_parsestd(hawk, arg, xtn, 0);
		if (x >= 0) xtn->s.in.xindex = 0; /* update the current stream index */

		return x;
	}
	else
	{
		/* handle the included source file - @include */
		hawk_ooch_t* xpath;

		HAWK_ASSERT (arg->name != HAWK_NULL);

		if (arg->prev->handle)
		{
			const hawk_ooch_t* outer;
			const hawk_ooch_t* path;
			hawk_ooch_t fbuf[64];
			hawk_ooch_t* dbuf = HAWK_NULL;

			path = arg->name;

			outer = hawk_sio_getpath(arg->prev->handle);
			if (outer)
			{
				const hawk_ooch_t* base;

				base = hawk_get_base_name_oocstr(outer);
				if (base != outer && arg->name[0] != '/')
				{
					hawk_oow_t tmplen, totlen, dirlen;

					dirlen = base - outer;
					totlen = hawk_count_oocstr(arg->name) + dirlen;
					if (totlen >= HAWK_COUNTOF(fbuf))
					{
						dbuf = hawk_allocmem(hawk, HAWK_SIZEOF(hawk_ooch_t) * (totlen + 1));
						if (!dbuf) return -1;
						path = dbuf;
					}
					else path = fbuf;

					tmplen = hawk_copy_oochars_to_oocstr_unlimited((hawk_ooch_t*)path, outer, dirlen);
					hawk_copy_oocstr_unlimited ((hawk_ooch_t*)path + tmplen, arg->name);
				}
			}

			if (!hawk_stdplainfileexists(hawk, path) && hawk->opt.includedirs.len > 0 && arg->name[0] != '/')
			{
				const hawk_ooch_t* tmp;
				tmp = hawk_stdgetfileindirs(hawk, &hawk->opt.includedirs, arg->name);
				if (tmp) path = tmp;
			}

			xpath = hawk_addsionamewithoochars(hawk, path, hawk_count_oocstr(path));
			if (dbuf) hawk_freemem(hawk, dbuf);
		}
		else
		{
			const hawk_ooch_t* path;

			path = arg->name;
			if (!hawk_stdplainfileexists(hawk, path) && hawk->opt.includedirs.len > 0 && arg->name[0] != '/')
			{
				const hawk_ooch_t* tmp;
				tmp = hawk_stdgetfileindirs(hawk, &hawk->opt.includedirs, arg->name);
				if (tmp) path = tmp;
			}

			xpath = hawk_addsionamewithoochars(hawk, path, hawk_count_oocstr(path));
		}
		if (!xpath) goto fail;

		arg->handle = hawk_sio_open(hawk_getgem(hawk), 0, xpath, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
		if (!arg->handle)
		{
			const hawk_ooch_t* bem;
		fail:
			bem = hawk_backuperrmsg(hawk);
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), arg->name, bem);
			return -1;
		}

		arg->path = xpath;
		/* TODO: use the system handle(file descriptor) instead of the path? */
		/*syshnd = hawk_sio_gethnd(arg->handle);*/
		fill_sio_arg_unique_id(hawk, arg, xpath); /* ignore failure */

		return 0;
	}
}

static hawk_ooi_t sf_in_close (hawk_t* hawk, hawk_sio_arg_t* arg, xtn_t* xtn)
{
	if (arg->prev == HAWK_NULL)
	{
		switch (xtn->s.in.x[xtn->s.in.xindex].type)
		{
			case HAWK_PARSESTD_FILE:
			case HAWK_PARSESTD_FILEB:
			case HAWK_PARSESTD_FILEU:
				HAWK_ASSERT (arg->handle != HAWK_NULL);
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
		HAWK_ASSERT (arg->handle != HAWK_NULL);
		hawk_sio_close (arg->handle);
	}

	return 0;
}

static hawk_ooi_t sf_in_read (hawk_t* hawk, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size, xtn_t* xtn)
{
	if (arg->prev == HAWK_NULL)
	{
		hawk_ooi_t n;

		HAWK_ASSERT (arg == &hawk->sio.arg);

	again:
		switch (xtn->s.in.x[xtn->s.in.xindex].type)
		{
		#if defined(HAWK_OOCH_IS_UCH)
			case HAWK_PARSESTD_FILE:
		#endif
			case HAWK_PARSESTD_FILEU:
				HAWK_ASSERT (arg->handle != HAWK_NULL);
				n = hawk_sio_getoochars(arg->handle, data, size);
				if (n <= -1)
				{
					const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
					const hawk_uch_t* path;
					path = xtn->s.in.x[xtn->s.in.xindex].u.fileu.path;
					if (path)
						hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EREAD, HAWK_T("unable to read %ls - %js"), path, bem);
					else
						hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EREAD, HAWK_T("unable to read %js - %js"), sio_std_names[HAWK_SIO_STDIN].ptr, bem);
				}
				break;

		#if defined(HAWK_OOCH_IS_BCH)
			case HAWK_PARSESTD_FILE:
		#endif
			case HAWK_PARSESTD_FILEB:
				HAWK_ASSERT (arg->handle != HAWK_NULL);
				n = hawk_sio_getoochars(arg->handle, data, size);
				if (n <= -1)
				{
					const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
					const hawk_bch_t* path;
					path = xtn->s.in.x[xtn->s.in.xindex].u.fileb.path;
					if (path)
						hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EREAD, HAWK_T("unable to read %hs - %js"), path, bem);
					else
						hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EREAD, HAWK_T("unable to read %js - %js"), sio_std_names[HAWK_SIO_STDIN].ptr, bem);
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
				if ((m = hawk_conv_bchars_to_uchars_with_cmgr(xtn->s.in.u.bcs.ptr, &mbslen, data, &wcslen, hawk_getcmgr(hawk), 0)) <= -1 && m != -2)
				{
					hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINVAL);
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
				if ((m = hawk_conv_uchars_to_bchars_with_cmgr(xtn->s.in.u.ucs.ptr, &wcslen, data, &mbslen, hawk_getcmgr(hawk))) <= -1 && m != -2)
				{
					hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINVAL);
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
				hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
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
				if (open_parsestd(hawk, arg, xtn, next) <= -1) n = -1;
				else
				{
					xtn->s.in.xindex = next; /* update the next to the current */
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

		HAWK_ASSERT (arg->name != HAWK_NULL);
		HAWK_ASSERT (arg->handle != HAWK_NULL);

		n = hawk_sio_getoochars(arg->handle, data, size);

		if (n <= -1)
		{
			const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
			hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EREAD, HAWK_T("unable to read %js - %js"), arg->name, bem);
		}
		return n;
	}
}

static hawk_ooi_t sf_in (hawk_t* hawk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size)
{
	xtn_t* xtn = GET_XTN(hawk);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			return sf_in_open(hawk, arg, xtn);

		case HAWK_SIO_CMD_CLOSE:
			return sf_in_close(hawk, arg, xtn);

		case HAWK_SIO_CMD_READ:
			return sf_in_read(hawk, arg, data, size, xtn);

		default:
			hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}
}

static hawk_ooi_t sf_out (hawk_t* hawk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size)
{
	xtn_t* xtn = GET_XTN(hawk);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
		{
			switch (xtn->s.out.x->type)
			{
			#if defined(HAWK_OOCH_IS_BCH)
				case HAWK_PARSESTD_FILE:
					HAWK_ASSERT (&xtn->s.out.x->u.fileb.path == &xtn->s.out.x->u.file.path);
					HAWK_ASSERT (&xtn->s.out.x->u.fileb.cmgr == &xtn->s.out.x->u.file.cmgr);
			#endif
				case HAWK_PARSESTD_FILEB:
					if (xtn->s.out.x->u.fileb.path == HAWK_NULL || (xtn->s.out.x->u.fileb.path[0] == '-' && xtn->s.out.x->u.fileb.path[1] == '\0'))
					{
						/* no path name or - -> stdout */
						xtn->s.out.u.file.sio = open_sio_std(hawk, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}
					else
					{
					#if defined(HAWK_OOCH_IS_UCH)
						hawk_uch_t* upath;
						upath = hawk_dupbtoucstr(hawk, xtn->s.out.x->u.fileb.path, HAWK_NULL, 1);
						if (!upath) return -1;
						xtn->s.out.u.file.sio = open_sio(hawk, upath, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
						hawk_freemem(hawk, upath);
					#else
						xtn->s.out.u.file.sio = open_sio(hawk, xtn->s.out.x->u.fileb.path, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
					#endif
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}

					if (xtn->s.out.x->u.fileb.cmgr) hawk_sio_setcmgr (xtn->s.out.u.file.sio, xtn->s.out.x->u.fileb.cmgr);
					return 1;

			#if defined(HAWK_OOCH_IS_UCH)
				case HAWK_PARSESTD_FILE:
					HAWK_ASSERT (&xtn->s.out.x->u.fileu.path == &xtn->s.out.x->u.file.path);
					HAWK_ASSERT (&xtn->s.out.x->u.fileu.cmgr == &xtn->s.out.x->u.file.cmgr);
			#endif
				case HAWK_PARSESTD_FILEU:
					if (xtn->s.out.x->u.fileu.path == HAWK_NULL || (xtn->s.out.x->u.fileu.path[0] == '-' && xtn->s.out.x->u.fileu.path[1] == '\0'))
					{
						/* no path name or - -> stdout */
						xtn->s.out.u.file.sio = open_sio_std(hawk, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}
					else
					{
					#if defined(HAWK_OOCH_IS_UCH)
						xtn->s.out.u.file.sio = open_sio(hawk, xtn->s.out.x->u.fileu.path, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
					#else
						hawk_bch_t* bpath;
						bpath = hawk_duputobcstr(hawk, xtn->s.out.x->u.fileu.path, HAWK_NULL);
						if (!bpath) return -1;
						xtn->s.out.u.file.sio = open_sio(hawk, bpath, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
						hawk_freemem(hawk, bpath);
					#endif
						if (xtn->s.out.u.file.sio == HAWK_NULL) return -1;
					}

					if (xtn->s.out.x->u.fileu.cmgr) hawk_sio_setcmgr (xtn->s.out.u.file.sio, xtn->s.out.x->u.fileu.cmgr);
					return 1;

				case HAWK_PARSESTD_OOCS:
					xtn->s.out.u.oocs.buf = hawk_ooecs_open(hawk_getgem(hawk), 0, 512);
					if (xtn->s.out.u.oocs.buf == HAWK_NULL) return -1;
					return 1;

				case HAWK_PARSESTD_BCS:
					xtn->s.out.u.bcs.buf = hawk_becs_open(hawk_getgem(hawk), 0, 512);
					if (xtn->s.out.u.bcs.buf == HAWK_NULL) return -1;
					return 1;

				case HAWK_PARSESTD_UCS:
					xtn->s.out.u.ucs.buf = hawk_uecs_open(hawk_getgem(hawk), 0, 512);
					if (xtn->s.out.u.ucs.buf == HAWK_NULL) return -1;
					return 1;

				default:
					goto internal_error;
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

				default:
					goto internal_error;
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
					HAWK_ASSERT (xtn->s.out.u.file.sio != HAWK_NULL);
					n = hawk_sio_putoochars(xtn->s.out.u.file.sio, data, size);
					if (n <= -1)
					{
						const hawk_ooch_t* ioname;
						const hawk_ooch_t* bem = hawk_backuperrmsg(hawk);
						ioname = xtn->s.out.x->u.file.path;
						if (!ioname) ioname = sio_std_names[HAWK_SIO_STDOUT].ptr;
						hawk_seterrfmt(hawk, HAWK_NULL, HAWK_EWRITE, HAWK_T("unable to write to %js - %js"), ioname, bem);
					}
					return n;
				}

				case HAWK_PARSESTD_OOCS:
				parsestd_str:
					if (size > HAWK_TYPE_MAX(hawk_ooi_t)) size = HAWK_TYPE_MAX(hawk_ooi_t);
					if (hawk_ooecs_ncat(xtn->s.out.u.oocs.buf, data, size) == (hawk_oow_t)-1) return -1;
					return size;

				case HAWK_PARSESTD_BCS:
				#if defined(HAWK_OOCH_IS_BCH)
					goto parsestd_str;
				#else
				{
					hawk_oow_t mbslen, wcslen;
					hawk_oow_t orglen;

					wcslen = size;
					if (hawk_convutobchars(hawk, data, &wcslen, HAWK_NULL, &mbslen) <= -1) return -1;
					if (mbslen > HAWK_TYPE_MAX(hawk_ooi_t)) mbslen = HAWK_TYPE_MAX(hawk_ooi_t);

					orglen = hawk_becs_getlen(xtn->s.out.u.bcs.buf);
					if (hawk_becs_setlen(xtn->s.out.u.bcs.buf, orglen + mbslen) == (hawk_oow_t)-1) return -1;

					wcslen = size;
					hawk_convutobchars(hawk, data, &wcslen, HAWK_BECS_CPTR(xtn->s.out.u.bcs.buf, orglen), &mbslen);
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
					if (hawk_convbtouchars(hawk, data, &mbslen, HAWK_NULL, &wcslen, 0) <= -1) return -1;
					if (wcslen > HAWK_TYPE_MAX(hawk_ooi_t)) wcslen = HAWK_TYPE_MAX(hawk_ooi_t);

					orglen = hawk_uecs_getlen(xtn->s.out.u.ucs.buf);
					if (hawk_uecs_setlen(xtn->s.out.u.ucs.buf, orglen + wcslen) == (hawk_oow_t)-1) return -1;

					mbslen = size;
					hawk_convbtouchars(hawk, data, &mbslen, HAWK_UECS_CPTR(xtn->s.out.u.ucs.buf, orglen), &wcslen, 0);
					size = mbslen;

					return size;
				}
				#else
					goto parsestd_str;
				#endif

				default:
					goto internal_error;
			}

			break;
		}

		default:
			/* other code must not trigger this function */
			break;
	}

internal_error:
	hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
	return -1;
}

int hawk_parsestd (hawk_t* hawk, hawk_parsestd_t in[], hawk_parsestd_t* out)
{
	hawk_sio_cbs_t sio;
	xtn_t* xtn = GET_XTN(hawk);
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
		hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINVAL);
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
			hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINVAL);
			return -1;
		}
		sio.out = sf_out;
		xtn->s.out.x = out;
	}

	n = hawk_parse(hawk, &sio);

	if (out)
	{
		switch (out->type)
		{
			case HAWK_PARSESTD_OOCS:
				if (n >= 0)
				{
					HAWK_ASSERT (xtn->s.out.u.oocs.buf != HAWK_NULL);
					hawk_ooecs_yield (xtn->s.out.u.oocs.buf, &out->u.oocs, 0);
				}
				if (xtn->s.out.u.oocs.buf) hawk_ooecs_close (xtn->s.out.u.oocs.buf);
				break;

			case HAWK_PARSESTD_BCS:
				if (n >= 0)
				{
					HAWK_ASSERT (xtn->s.out.u.bcs.buf != HAWK_NULL);
					hawk_becs_yield (xtn->s.out.u.bcs.buf, &out->u.bcs, 0);
				}
				if (xtn->s.out.u.bcs.buf) hawk_becs_close (xtn->s.out.u.bcs.buf);
				break;

			case HAWK_PARSESTD_UCS:
				if (n >= 0)
				{
					HAWK_ASSERT (xtn->s.out.u.ucs.buf != HAWK_NULL);
					hawk_uecs_yield (xtn->s.out.u.ucs.buf, &out->u.ucs, 0);
				}
				if (xtn->s.out.u.ucs.buf) hawk_uecs_close (xtn->s.out.u.ucs.buf);
				break;

			default:
				/* do nothing */
				break;
		}
	}

	return n;
}

static int check_var_assign (hawk_rtx_t* rtx, const hawk_ooch_t* str)
{
	hawk_ooch_t* eq, * dstr;
	int n;

	eq = hawk_find_oochar_in_oocstr(str, '=');
	if (!eq || eq <= str) return 0; /* not assignment */

	dstr = hawk_rtx_dupoocstr(rtx, str, HAWK_NULL);
	if (HAWK_UNLIKELY(!dstr)) return -1;

	eq = dstr + (eq - str);
	*eq = '\0';

	if (hawk_isvalidident(hawk_rtx_gethawk(rtx), dstr))
	{
		hawk_unescape_oocstr (eq + 1);
		n = (hawk_rtx_setgbltostrbyname(rtx, dstr, eq + 1) <= -1)? -1: 1;
	}
	else
	{
		n = 0;
	}

	hawk_rtx_freemem (rtx, dstr);
	return n;
}

/*** RTX_OPENSTD ***/

static ioattr_t* get_ioattr (hawk_htb_t* tab, const hawk_ooch_t* ptr, hawk_oow_t len);

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
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
			return -1;

		case HAWK_RIO_CMD_CLOSE:
			hawk_nwio_close ((hawk_nwio_t*)riod->handle);
			return 0;

		case HAWK_RIO_CMD_READ:
			return hawk_nwio_read((hawk_nwio_t*)riod->handle, data, size);

		case HAWK_RIO_CMD_READBYTES:
			return hawk_nwio_readbytes(((hawk_nwio_t*)riod->handle, data, size);

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

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
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
		hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
		return -1;
	}

	handle = hawk_pio_open(
		hawk_rtx_getgem(rtx),
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
			hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
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
			return hawk_pio_read((hawk_pio_t*)riod->handle, HAWK_PIO_OUT, data, size);

		case HAWK_RIO_CMD_READ_BYTES:
			return hawk_pio_readbytes((hawk_pio_t*)riod->handle, HAWK_PIO_OUT, data, size);

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

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
	return -1;
}

static hawk_ooi_t hawk_rio_pipe (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
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

static void set_rio_error (hawk_rtx_t* rtx, hawk_errnum_t errnum, const hawk_ooch_t* errmsg, const hawk_ooch_t* path)
{
	const hawk_ooch_t* bem = hawk_rtx_backuperrmsg(rtx);
	hawk_rtx_seterrfmt (rtx, HAWK_NULL, errnum, HAWK_T("%js%js%js - %js"),
		errmsg, (path? HAWK_T(" "): HAWK_T("")), (path? path: HAWK_T("")), bem);
}

static hawk_ooi_t hawk_rio_file (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
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
					hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}

			if (riod->name[0] == '-' && riod->name[1] == '\0')
			{
				if (riod->mode == HAWK_RIO_FILE_READ)
					handle = open_sio_std_rtx(rtx, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
				else
					handle = open_sio_std_rtx(rtx, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			}
			else
			{
				handle = hawk_sio_open(hawk_rtx_getgem(rtx), 0, riod->name, flags);
			}
			if (!handle)
			{
				set_rio_error (rtx, HAWK_EOPEN, HAWK_T("unable to open"), riod->name);
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
		{
			hawk_ooi_t t;
			t = hawk_sio_getoochars((hawk_sio_t*)riod->handle, data, size);
			if (t <= -1) set_rio_error (rtx, HAWK_EOPEN, HAWK_T("unable to read"), riod->name);
			return t;
		}

		case HAWK_RIO_CMD_READ_BYTES:
		{
			hawk_ooi_t t;
			t = hawk_sio_getbchars((hawk_sio_t*)riod->handle, data, size);
			if (t <= -1) set_rio_error (rtx, HAWK_EOPEN, HAWK_T("unable to read"), riod->name);
			return t;
		}

		case HAWK_RIO_CMD_WRITE:
		{
			hawk_ooi_t t;
			t = hawk_sio_putoochars((hawk_sio_t*)riod->handle, data, size);
			if (t <= -1) set_rio_error (rtx, HAWK_EOPEN, HAWK_T("unable to write"), riod->name);
			return t;
		}

		case HAWK_RIO_CMD_WRITE_BYTES:
		{
			hawk_ooi_t t;
			t = hawk_sio_putbchars((hawk_sio_t*)riod->handle, data, size);
			if (t <= -1) set_rio_error (rtx, HAWK_EOPEN, HAWK_T("unable to write"), riod->name);
			return t;
		}

		case HAWK_RIO_CMD_FLUSH:
		{
			int n;

			n = hawk_sio_flush((hawk_sio_t*)riod->handle);
			if (HAWK_UNLIKELY(n <= -1))
			{
				/* if flushing fails, discard the buffered data
				 * keeping the unflushed data causes causes subsequent write or close() to
				 * flush again and again */
				hawk_sio_drain ((hawk_sio_t*)riod->handle);
			}
			return n;
		}

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
	#if 1
		/* this part is more complex than the code in the else part.
		 * it handles simple assignemnt as well as open files
		 * via in ARGV instead of rxtn->c.in.files */
		xtn_t* xtn = (xtn_t*)GET_XTN(hawk_rtx_gethawk(rtx));
		hawk_val_t* v_argc, * v_argv, * v_pair;
		hawk_int_t i_argc;
		const hawk_ooch_t* file;
		hawk_map_t* map;
		hawk_map_pair_t* pair;
		hawk_ooch_t ibuf[128];
		hawk_oow_t ibuflen;
		hawk_oocs_t as;
		int x;

		v_argc = hawk_rtx_getgbl(rtx, xtn->gbl_argc);
		HAWK_ASSERT (v_argc != HAWK_NULL);
		if (hawk_rtx_valtoint(rtx, v_argc, &i_argc) <= -1) return -1;

		/* handle special case when ARGV[x] has been altered.
		 * so from here down, the file name gotten from
		 * rxtn->c.in.files is not important and is overridden
		 * from ARGV.
		 * 'BEGIN { ARGV[1]="file3"; }
		 *        { print $0; }' file1 file2
		 */
		v_argv = hawk_rtx_getgbl(rtx, xtn->gbl_argv);
		HAWK_ASSERT (v_argv != HAWK_NULL);
		if (HAWK_RTX_GETVALTYPE(rtx, v_argv) != HAWK_VAL_MAP)
		{
			/* with flexmap on, you can change ARGV to a scalar.
			 *   BEGIN { ARGV="xxx"; }
			 * you must not do this. */
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EINVAL, HAWK_T("phony value in ARGV"));
			return -1;
		}

		map = ((hawk_val_map_t*)v_argv)->map;
		HAWK_ASSERT (map != HAWK_NULL);

	nextfile:
		if (rxtn->c.in.index >= (i_argc - 1))  /* ARGV is a kind of 0-based array unlike other normal arrays or substring indexing scheme */
		{
			/* reached the last ARGV */

			if (rxtn->c.in.count <= 0) /* but no file has been ever opened */
			{
			console_open_stdin:
				/* open stdin */
				sio = open_sio_std_rtx(rtx, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
				if (HAWK_UNLIKELY(!sio)) return -1;

				if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);

				riod->handle = sio;
				rxtn->c.in.count++;
				return 1;
			}

			return 0;
		}

		ibuflen = hawk_int_to_oocstr(rxtn->c.in.index + 1, 10, HAWK_NULL, ibuf, HAWK_COUNTOF(ibuf));

		pair = hawk_map_search(map, ibuf, ibuflen);
		if (!pair)
		{
			/* the key doesn't exist any more */
			if (rxtn->c.in.count <= 0) goto console_open_stdin;
			return 0; /* end of console */
		}

		v_pair = HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (v_pair != HAWK_NULL);

		as.ptr = hawk_rtx_getvaloocstr(rtx, v_pair, &as.len);
		if (HAWK_UNLIKELY(!as.ptr)) return -1;

		if (as.len == 0)
		{
			/* the name is empty */
			hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
			rxtn->c.in.index++;
			goto nextfile;
		}

		if (hawk_count_oocstr(as.ptr) < as.len)
		{
			/* the name contains one or more '\0' */
			hawk_rtx_seterrfmt (rtx, HAWK_NULL, HAWK_EIONMNL, HAWK_T("invalid I/O name of length %zu containing '\\0'"), as.len);
			hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
			return -1;
		}

		file = as.ptr;

		/*
		 * this is different from the -v option.
		 * if an argument has a special form of var=val, it is treated specially
		 *
		 * on the command-line
		 *   hawk -f a.hawk a=20 /etc/passwd
		 * or via ARGV
		 *    hawk 'BEGIN { ARGV[1] = "a=20"; }
		 *                { print a $1; }' dummy /etc/hosts
		 */
		if ((x = check_var_assign(rtx, file)) != 0)
		{
			hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
			if (HAWK_UNLIKELY(x <= -1)) return -1;
			rxtn->c.in.index++;
			goto nextfile;
		}

		/* a temporary variable sio is used here not to change
		 * any fields of riod when the open operation fails */
		sio = (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))?
			open_sio_std_rtx(rtx, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR):
			open_sio_rtx(rtx, file, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH);
		if (HAWK_UNLIKELY(!sio))
		{
			hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
			return -1;
		}

		if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);

		if (hawk_rtx_setfilenamewithoochars(rtx, file, hawk_count_oocstr(file)) <= -1)
		{
			hawk_sio_close (sio);
			hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
			return -1;
		}

		hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
		riod->handle = sio;

		/* increment the counter of files successfully opened */
		rxtn->c.in.count++;
		rxtn->c.in.index++;
		return 1;
	#else
		/* simple console open implementation.
		 * no var=val handling. no ARGV handling */

		if (!rxtn->c.in.files)
		{
			/* if no input files is specified,
			 * open the standard input */
			HAWK_ASSERT (rxtn->c.in.index == 0);

			if (rxtn->c.in.count == 0)
			{
			console_open_stdin:
				sio = open_sio_std_rtx(rtx, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
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

		nextfile:
			file = rxtn->c.in.files[rxtn->c.in.index];
			if (!file)
			{
				/* no more input file */
				if (rxtn->c.in.count == 0) goto console_open_stdin;
				return 0;
			}

			if (file[0] == '\0')
			{
				rxtn->c.in.index++;
				goto nextfile;
			}

			sio = (file[0] == '-' && file[1] == '\0')?
				open_sio_std_rtx(rtx, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR):
				open_sio_rtx(rtx, file, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			if (HAWK_UNLIKELY(!sio)) return -1;

			if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);

			if (hawk_rtx_setfilenamewithoochars(rtx, file, hawk_count_oocstr(file)) <= -1)
			{
				hawk_sio_close (sio);
				return -1;
			}

			riod->handle = sio;

			/* increment the counter of files successfully opened */
			rxtn->c.in.count++;
			rxtn->c.in.index++;
			return 1;
		}
	#endif
	}
	else if (riod->mode == HAWK_RIO_CONSOLE_WRITE)
	{
		if (rxtn->c.out.files == HAWK_NULL)
		{
			HAWK_ASSERT (rxtn->c.out.index == 0);

			if (rxtn->c.out.count == 0)
			{
				sio = open_sio_std_rtx(
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
				open_sio_std_rtx(rtx, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK):
				open_sio_rtx(rtx, file, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
			if (sio == HAWK_NULL) return -1;

			if (rxtn->c.cmgr) hawk_sio_setcmgr (sio, rxtn->c.cmgr);

			if (hawk_rtx_setofilenamewithoochars(rtx, file, hawk_count_oocstr(file)) <= -1)
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

static hawk_ooi_t hawk_rio_console (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t size)
{
	switch (cmd)
	{
		case HAWK_RIO_CMD_OPEN:
			return open_rio_console(rtx, riod);

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

				/* reset FNR to 0 here since the caller doesn't know that the file has changed. */
				hawk_rtx_setgbl(rtx, HAWK_GBL_FNR, hawk_rtx_makeintval(rtx, 0));
			}

			if (nn <= -1) set_rio_error (rtx, HAWK_EREAD, HAWK_T("unable to read"), hawk_sio_getpath((hawk_sio_t*)riod->handle));
			return nn;
		}

		case HAWK_RIO_CMD_READ_BYTES:
		{
			hawk_ooi_t nn;

			while ((nn = hawk_sio_getbchars((hawk_sio_t*)riod->handle, data, size)) == 0)
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
				hawk_rtx_setgbl(rtx, HAWK_GBL_FNR, hawk_rtx_makeintval(rtx, 0));
			}

			if (nn <= -1) set_rio_error (rtx, HAWK_EREAD, HAWK_T("unable to read"), hawk_sio_getpath((hawk_sio_t*)riod->handle));
			return nn;
		}

		case HAWK_RIO_CMD_WRITE:
		{
			hawk_ooi_t nn;
			nn = hawk_sio_putoochars((hawk_sio_t*)riod->handle, data, size);
			if (nn <= -1) set_rio_error (rtx, HAWK_EREAD, HAWK_T("unable to write"), hawk_sio_getpath((hawk_sio_t*)riod->handle));
			return nn;
		}

		case HAWK_RIO_CMD_WRITE_BYTES:
		{
			hawk_ooi_t nn;
			nn = hawk_sio_putbchars((hawk_sio_t*)riod->handle, data, size);
			if (nn <= -1) set_rio_error (rtx, HAWK_EREAD, HAWK_T("unable to write"), hawk_sio_getpath((hawk_sio_t*)riod->handle));
			return nn;
		}

		case HAWK_RIO_CMD_FLUSH:
		{
			int n;

			n = hawk_sio_flush((hawk_sio_t*)riod->handle);
			if (HAWK_UNLIKELY(n <= -1))
			{
				/* if flushing fails, discard the buffered data
				 * keeping the unflushed data causes causes subsequent write or close() to
				 * flush again and again */
				hawk_sio_drain ((hawk_sio_t*)riod->handle);
			}
			return n;
		}

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

	hawk_rtx_seterrnum (rtx, HAWK_NULL, HAWK_EINTERN);
	return -1;
}

static void fini_rxtn (hawk_rtx_t* rtx, void* ctx)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	/*xtn_t* xtn = (xtn_t*)GET_XTN(hawk_rtx_gethawk(rtx));*/

	if (rxtn->c.in.files)
	{
		hawk_oow_t i;
		for (i = 0; rxtn->c.in.files[i]; i++) hawk_rtx_freemem(rtx, rxtn->c.in.files[i]);
		hawk_rtx_freemem (rtx, rxtn->c.in.files);
		rxtn->c.in.files = HAWK_NULL;
		rxtn->c.in.count = 0;
		rxtn->c.in.index = 0;
	}

	if (rxtn->c.out.files)
	{
		hawk_oow_t i;
		for (i = 0; rxtn->c.out.files[i]; i++) hawk_rtx_freemem(rtx, rxtn->c.out.files[i]);
		hawk_rtx_freemem (rtx, rxtn->c.out.files);
		rxtn->c.out.files = HAWK_NULL;
		rxtn->c.out.count = 0;
		rxtn->c.out.index = 0;
	}

	if (rxtn->cmgrtab_inited)
	{
		hawk_htb_fini (&rxtn->cmgrtab);
		rxtn->cmgrtab_inited = 0;
	}
}

static int build_argcv (hawk_rtx_t* rtx, int argc_id, int argv_id, const hawk_ooch_t* id, hawk_ooch_t* icf[])
{
	hawk_ooch_t** p;
	hawk_oow_t argc;
	hawk_val_t* v_argc;
	hawk_val_t* v_argv;
	hawk_val_t* v_tmp;
	hawk_ooch_t key[HAWK_SIZEOF(hawk_int_t)*8+2];
	hawk_oow_t key_len;

	v_argv = hawk_rtx_makemapval(rtx);
	if (v_argv == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, v_argv);

	/* make ARGV[0] */
	v_tmp = hawk_rtx_makestrvalwithoocstr(rtx, id);
	if (v_tmp == HAWK_NULL)
	{
		hawk_rtx_refdownval (rtx, v_argv);
		return -1;
	}

	/* increment reference count of v_tmp in advance as if
	 * it has successfully been assigned into ARGV. */
	hawk_rtx_refupval (rtx, v_tmp);

	key_len = hawk_copy_oocstr(key, HAWK_COUNTOF(key), HAWK_T("0"));
	if (hawk_map_upsert(((hawk_val_map_t*)v_argv)->map, key, key_len, v_tmp, 0) == HAWK_NULL)
	{
		/* if the assignment operation fails, decrements
		 * the reference of v_tmp to free it */
		hawk_rtx_refdownval (rtx, v_tmp);

		/* the values previously assigned into the
		 * map will be freeed when v_argv is freed */
		hawk_rtx_refdownval (rtx, v_argv);

		return -1;
	}

	if (icf)
	{
		for (argc = 1, p = icf; *p; p++, argc++)
		{
			/* the argument must compose a numeric value if possible */
			/*v_tmp = hawk_rtx_makenstrvalwithoocstr(rtx, *p); */
			v_tmp = hawk_rtx_makenumorstrvalwithoochars(rtx, *p, hawk_count_oocstr(*p));
			if (HAWK_UNLIKELY(!v_tmp))
			{
				hawk_rtx_refdownval (rtx, v_argv);
				return -1;
			}

			key_len = hawk_int_to_oocstr(argc, 10, HAWK_NULL, key, HAWK_COUNTOF(key));
			HAWK_ASSERT (key_len != (hawk_oow_t)-1);

			hawk_rtx_refupval (rtx, v_tmp);

			if (hawk_map_upsert(((hawk_val_map_t*)v_argv)->map, key, key_len, v_tmp, 0) == HAWK_NULL)
			{
				hawk_rtx_refdownval (rtx, v_tmp);
				hawk_rtx_refdownval (rtx, v_argv);
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

#if defined(HAVE_CRT_EXTERNS_H)
#define environ (*(_NSGetEnviron()))
#else
extern char** environ;
#endif

static int build_environ (hawk_rtx_t* rtx, int gbl_id, env_char_t* envarr[])
{
	hawk_val_t* v_env;
	hawk_val_t* v_tmp;

	v_env = hawk_rtx_makemapval(rtx);
	if (v_env == HAWK_NULL) return -1;

	hawk_rtx_refupval (rtx, v_env);

	if (envarr)
	{
		env_char_t* eq;
		hawk_ooch_t* kptr, * vptr;
		hawk_oow_t klen, vlen, count;

		for (count = 0; envarr[count]; count++)
		{
		#if ((defined(ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
			eq = hawk_find_oochar_in_oocstr(envarr[count], '=');
			if (HAWK_UNLIKELY(!eq || eq == envarr[count])) continue;

			kptr = envarr[count];
			klen = eq - envarr[count];
			vptr = eq + 1;
			vlen = hawk_count_oocstr(vptr);
		#elif defined(ENV_CHAR_IS_BCH)
			eq = hawk_find_bchar_in_bcstr(envarr[count], '=');
			if (HAWK_UNLIKELY(!eq || eq == envarr[count])) continue;

			*eq = '\0';

			/* dupbtoucstr() may fail for invalid encoding. as the environment
			 * variaables are not under control, call mbstowcsalldup() instead
			 * to go on despite encoding failure */

			kptr = hawk_rtx_dupbtoucstr(rtx, envarr[count], &klen, 1);
			vptr = hawk_rtx_dupbtoucstr(rtx, eq + 1, &vlen, 1);
			if (HAWK_UNLIKELY(!kptr || !vptr))
			{
				if (kptr) hawk_rtx_freemem (rtx, kptr);
				if (vptr) hawk_rtx_freemem (rtx, vptr);
				hawk_rtx_refdownval (rtx, v_env);
				return -1;
			}

			*eq = '=';
		#else
			eq = hawk_find_uchar_in_ucstr(envarr[count], '=');
			if (HAWK_UNLIKELY(!eq || eq == envarr[count])) continue;

			*eq = '\0';

			kptr = hawk_rtx_duputobcstr(rtx, envarr[count], &klen);
			vptr = hawk_rtx_duputobcstr(rtx, eq + 1, &vlen);
			if (HAWK_UNLIKELY(!kptr || !vptr))
			{
				if (kptr) hawk_rtx_freemem (rtx, kptr);
				if (vptr) hawk_rtx_freeme (rtx, vptr):
				hawk_rtx_refdownval (rtx, v_env);
				return -1;
			}

			*eq = '=';
		#endif

			/* the string in ENVIRON should be a numeric value if
			 * it can be converted to a number.
			 *v_tmp = hawk_rtx_makenstrvalwithoocstr(rtx, vptr);*/
			v_tmp = hawk_rtx_makenumorstrvalwithoochars(rtx, vptr, vlen);
			if (HAWK_UNLIKELY(!v_tmp))
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

			if (hawk_map_upsert(((hawk_val_map_t*)v_env)->map, kptr, klen, v_tmp, 0) == HAWK_NULL)
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

	if (hawk_rtx_setgbl(rtx, gbl_id, v_env) == -1)
	{
		hawk_rtx_refdownval (rtx, v_env);
		return -1;
	}

	hawk_rtx_refdownval (rtx, v_env);
	return 0;
}

static int make_additional_globals (hawk_rtx_t* rtx, xtn_t* xtn, const hawk_ooch_t* id, hawk_ooch_t* icf[])
{
	if (build_argcv(rtx, xtn->gbl_argc, xtn->gbl_argv, id, icf) <= -1 ||
	    build_environ(rtx, xtn->gbl_environ, environ) <= -1) return -1;
	return 0;
}

static hawk_rtx_t* open_rtx_std (
	hawk_t*            hawk,
	hawk_oow_t         xtnsize,
	const hawk_ooch_t* id,
	hawk_ooch_t*       icf[],
	hawk_ooch_t*       ocf[],
	hawk_cmgr_t*       cmgr)
{
	hawk_rtx_t* rtx;
	hawk_rio_cbs_t rio;
	rxtn_t* rxtn;
	xtn_t* xtn;

	xtn = GET_XTN(hawk);

	rio.pipe = hawk_rio_pipe;
	rio.file = hawk_rio_file;
	rio.console = hawk_rio_console;

	rtx = hawk_rtx_open(hawk, HAWK_SIZEOF(rxtn_t) + xtnsize, &rio);
	if (HAWK_UNLIKELY(!rtx)) return HAWK_NULL;

	rtx->_instsize += HAWK_SIZEOF(rxtn_t);

	rxtn = GET_RXTN(rtx);

	if (rtx->hawk->opt.trait & HAWK_RIO)
	{
		if (hawk_htb_init(&rxtn->cmgrtab, hawk_getgem(hawk), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1)
		{
			hawk_rtx_close (rtx);
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
		if (hawk_rtx_setofilenamewithoochars(rtx, ocf[0], hawk_count_oocstr(ocf[0])) <= -1)
		{
			hawk_rtx_errortohawk (rtx, hawk);
			hawk_rtx_close (rtx);
			return HAWK_NULL;
		}
	}

	if (make_additional_globals(rtx, xtn, id, icf) <= -1)
	{
		hawk_rtx_errortohawk (rtx, hawk);
		hawk_rtx_close (rtx);
		return HAWK_NULL;
	}

	return rtx;
}

hawk_rtx_t* hawk_rtx_openstdwithbcstr (
	hawk_t*           hawk,
	hawk_oow_t        xtnsize,
	const hawk_bch_t* id,
	hawk_bch_t*       icf[],
	hawk_bch_t*       ocf[],
	hawk_cmgr_t*      cmgr)
{
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_oow_t wcslen, i;
	hawk_ooch_t* xid, ** xicf = HAWK_NULL, ** xocf = HAWK_NULL;

#if defined(HAWK_OOCH_IS_UCH)
	xid = hawk_dupbtoucstr(hawk, id, &wcslen, 0);
	if (HAWK_UNLIKELY(!xid)) return HAWK_NULL;
#else
	xid = (hawk_ooch_t*)id;
#endif

	if (icf)
	{
		for (i = 0; icf[i]; i++) ;

		xicf = (hawk_ooch_t**)hawk_callocmem(hawk, HAWK_SIZEOF(*xicf) * (i + 1));
		if (!xicf) goto done;

		for (i = 0; icf[i]; i++)
		{
		#if defined(HAWK_OOCH_IS_UCH)
			xicf[i] = hawk_dupbtoucstr(hawk, icf[i], &wcslen, 0);
		#else
			xicf[i] = hawk_dupbcstr(hawk, icf[i], HAWK_NULL);
		#endif
			if (!xicf[i]) goto done;
		}
		xicf[i] = HAWK_NULL;
	}

	if (ocf)
	{
		for (i = 0; ocf[i]; i++) ;

		xocf = (hawk_ooch_t**)hawk_callocmem(hawk, HAWK_SIZEOF(*xocf) * (i + 1));
		if (!xocf) goto done;

		for (i = 0; ocf[i]; i++)
		{
		#if defined(HAWK_OOCH_IS_UCH)
			xocf[i] = hawk_dupbtoucstr(hawk, ocf[i], &wcslen, 0);
		#else
			xocf[i] = hawk_dupbcstr(hawk, ocf[i], HAWK_NULL);
		#endif
			if (!xocf[i]) goto done;
		}
		xocf[i] = HAWK_NULL;
	}

	rtx = open_rtx_std(hawk, xtnsize, xid, xicf, xocf, cmgr);

done:
	if (!rtx)
	{
		if (xocf)
		{
			for (i = 0; xocf[i]; i++) hawk_freemem(hawk, xocf[i]);
			hawk_freemem(hawk, xocf);
		}
		if (xicf)
		{
			for (i = 0; xicf[i]; i++) hawk_freemem(hawk, xicf[i]);
			hawk_freemem(hawk, xicf);
		}
	}

#if defined(HAWK_OOCH_IS_UCH)
	hawk_freemem(hawk, xid);
#endif

	return rtx;
}

hawk_rtx_t* hawk_rtx_openstdwithucstr (
	hawk_t*           hawk,
	hawk_oow_t        xtnsize,
	const hawk_uch_t* id,
	hawk_uch_t*       icf[],
	hawk_uch_t*       ocf[],
	hawk_cmgr_t*      cmgr)
{
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_oow_t mbslen, i;
	hawk_ooch_t* xid, ** xicf = HAWK_NULL, ** xocf = HAWK_NULL;

#if defined(HAWK_OOCH_IS_BCH)
	xid = hawk_duputobcstr(hawk, id, &mbslen);
	if (!xid) return HAWK_NULL;
#else
	xid = (hawk_ooch_t*)id;
#endif

	if (icf)
	{
		for (i = 0; icf[i]; i++) ;

		xicf = (hawk_ooch_t**)hawk_callocmem(hawk, HAWK_SIZEOF(*xicf) * (i + 1));
		if (!xicf) goto done;

		for (i = 0; icf[i]; i++)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			xicf[i] = hawk_duputobcstr(hawk, icf[i], &mbslen);
		#else
			xicf[i] = hawk_dupucstr(hawk, icf[i], HAWK_NULL);
		#endif
			if (HAWK_UNLIKELY(!xicf[i])) goto done;
		}
		xicf[i] = HAWK_NULL;
	}

	if (ocf)
	{
		for (i = 0; ocf[i]; i++) ;

		xocf = (hawk_ooch_t**)hawk_callocmem(hawk, HAWK_SIZEOF(*xocf) * (i + 1));
		if (!xocf) goto done;

		for (i = 0; ocf[i]; i++)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			xocf[i] = hawk_duputobcstr(hawk, ocf[i], &mbslen);
		#else
			xocf[i] = hawk_dupucstr(hawk, ocf[i], HAWK_NULL);
		#endif
			if (!xocf[i]) goto done;
		}
		xocf[i] = HAWK_NULL;
	}

	rtx = open_rtx_std(hawk, xtnsize, xid, xicf, xocf, cmgr);

done:
	if (!rtx)
	{
		if (xocf)
		{
			for (i = 0; xocf[i]; i++) hawk_freemem(hawk, xocf[i]);
			hawk_freemem(hawk, xocf);
		}
		if (xicf)
		{
			for (i = 0; xicf[i]; i++) hawk_freemem(hawk, xicf[i]);
			hawk_freemem(hawk, xicf);
		}
	}

#if defined(HAWK_OOCH_IS_BCH)
	hawk_freemem(hawk, xid);
#endif

	return rtx;
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

static ioattr_t* get_ioattr (hawk_htb_t* tab, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search(tab, ptr, len);
	if (pair) return HAWK_HTB_VPTR(pair);

	return HAWK_NULL;
}

static ioattr_t* find_or_make_ioattr (hawk_rtx_t* rtx, hawk_htb_t* tab, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search(tab, ptr, len);
	if (pair == HAWK_NULL)
	{
		ioattr_t ioattr;

		init_ioattr (&ioattr);

		pair = hawk_htb_insert(tab, (void*)ptr, len, (void*)&ioattr, HAWK_SIZEOF(ioattr));
		if (pair == HAWK_NULL) return HAWK_NULL;
	}

	return (ioattr_t*)HAWK_HTB_VPTR(pair);
}

static int fnc_setioattr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/*hawk_t* hawk = hawk_rtx_gethawk(rtx);*/
	rxtn_t* rxtn;
	hawk_val_t* v[3];
	hawk_ooch_t* ptr[3];
	hawk_oow_t len[3];
	int i, ret = 0, fret = 0;
	int tmout;

	rxtn = GET_RXTN(rtx);
	HAWK_ASSERT (rxtn->cmgrtab_inited == 1);

	for (i = 0; i < 3; i++)
	{
		v[i] = hawk_rtx_getarg(rtx, i);
		ptr[i] = hawk_rtx_getvaloocstr(rtx, v[i], &len[i]);
		if (ptr[i] == HAWK_NULL)
		{
			ret = -1;
			goto done;
		}

		if (hawk_find_oochar_in_oochars(ptr[i], len[i], '\0'))
		{
			fret = -1;
			goto done;
		}
	}

	if ((tmout = timeout_code(ptr[1])) >= 0)
	{
		ioattr_t* ioattr;

		hawk_int_t l;
		hawk_flt_t r;
		int x;

		/* no error is returned by hawk_rtx_strnum() if the strict option
		 * of the second parameter is 0. so i don't check for an error */
		x = hawk_oochars_to_num(HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), 0), ptr[2], len[2], &l, &r);
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
	HAWK_ASSERT (rxtn->cmgrtab_inited == 1);

	for (i = 0; i < 2; i++)
	{
		v[i] = hawk_rtx_getarg (rtx, i);
		ptr[i] = hawk_rtx_getvaloocstr (rtx, v[i], &len[i]);
		if (ptr[i] == HAWK_NULL)
		{
			ret = -1;
			goto done;
		}

		if (hawk_find_oochar_in_oochars(ptr[i], len[i], '\0')) goto done;
	}

	ioattr = get_ioattr(&rxtn->cmgrtab, ptr[0], len[0]);
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
			ret = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), rv);
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

	HAWK_ASSERT (rxtn->cmgrtab_inited == 1);

	ioattr = get_ioattr(&rxtn->cmgrtab, ioname, hawk_count_oocstr(ioname));
	if (ioattr) return ioattr->cmgr;
#endif
	return HAWK_NULL;
}

static int add_globals (hawk_t* hawk)
{
	xtn_t* xtn = GET_XTN(hawk);

	xtn->gbl_argc = hawk_addgblwithoocstr(hawk, HAWK_T("ARGC"));
	xtn->gbl_argv = hawk_addgblwithoocstr(hawk, HAWK_T("ARGV"));
	xtn->gbl_environ = hawk_addgblwithoocstr(hawk, HAWK_T("ENVIRON"));

	return (HAWK_UNLIKELY(xtn->gbl_argc <= -1 || xtn->gbl_argv <= -1 || xtn->gbl_environ <= -1))? -1: 0;
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

static int add_functions (hawk_t* hawk)
{
	int i;

	for (i = 0; i < HAWK_COUNTOF(fnctab); i++)
	{
		if (hawk_addfnc(hawk, fnctab[i].name, &fnctab[i].spec) == HAWK_NULL) return -1;
	}

	return 0;
}

