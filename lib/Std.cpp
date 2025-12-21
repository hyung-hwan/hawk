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

#include <Hawk.hpp>
#include <hawk-sio.h>
#include <hawk-pio.h>
#include "hawk-prv.h"
#include <stdlib.h>
#include <sys/stat.h>

// TODO: remove the following definitions and find a way to share the similar definitions in std.c
#if defined(HAWK_ENABLE_LIBLTDL)
#	define USE_LTDL
#elif defined(HAVE_DLFCN_H)
#	define USE_DLFCN
#else
#	error UNSUPPORTED DYNAMIC LINKER
#endif

#if defined(HAVE_CRT_EXTERNS_H)
#	include <crt_externs.h> /* MacOSX/darwin. _NSGetEnviron() */
#	define SYSTEM_ENVIRON (*(_NSGetEnviron()))
#else
	extern char** environ;
#	define SYSTEM_ENVIRON ::environ
#endif

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// MmgrStd
//////////////////////////////////////////////////////////////////////////////
void* MmgrStd::allocMem (hawk_oow_t n) HAWK_CXX_NOEXCEPT
{
	return ::malloc(n);
}

void* MmgrStd::reallocMem (void* ptr, hawk_oow_t n) HAWK_CXX_NOEXCEPT
{
	return ::realloc(ptr, n);
}

void MmgrStd::freeMem (void* ptr) HAWK_CXX_NOEXCEPT
{
	::free (ptr);
}

//////////////////////////////////////////////////////////////////////////////
// HawkStd
//////////////////////////////////////////////////////////////////////////////

HawkStd::ioattr_t HawkStd::default_ioattr;

static hawk_sio_t* open_sio (Hawk* hawk, HawkStd::Run* run, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;

	//sio = hawk_sio_open ((run? ((Hawk::hawk_t*)*(Hawk*)*run)->mmgr: hawk->getMmgr()), 0, file, flags);
	sio = hawk_sio_open((run? (hawk_gem_t*)*run: (hawk_gem_t*)*hawk), 0, file, flags);
	if (sio == HAWK_NULL)
	{
		if (run)
		{
			const hawk_ooch_t* bem = hawk_rtx_backuperrmsg((hawk_rtx_t*)*run);
			//run->formatError (HAWK_EOPEN, HAWK_NULL, HAWK_T("unable to open %js - %js"), file, bem);
			hawk_rtx_seterrfmt((hawk_rtx_t*)*run, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), file, bem);
		}
		else
		{
			const hawk_ooch_t* bem = hawk_backuperrmsg((hawk_t*)*hawk);
			//hawk->formatError (HAWK_EOPEN, HAWK_NULL, HAWK_T("unable to open %js - %js"), file, bem);
			hawk_seterrfmt((hawk_t*)*hawk, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), file, bem);
		}
	}
	return sio;
}

static hawk_sio_t* open_sio_std (Hawk* hawk, HawkStd::Run* run, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	static const hawk_ooch_t* std_names[] =
	{
		HAWK_T("stdin"),
		HAWK_T("stdout"),
		HAWK_T("stderr"),
	};

	// at least either hawk or run must be non null

	//sio = hawk_sio_openstd ((run? ((Hawk::hawk_t*)*(Hawk*)*run)->mmgr: hawk->getMmgr()), 0, std, flags);
	sio = hawk_sio_openstd((run? (hawk_gem_t*)*run: (hawk_gem_t*)*hawk), 0, std, flags);
	if (sio == HAWK_NULL)
	{
		if (run)
		{
			const hawk_ooch_t* bem = hawk_rtx_backuperrmsg((hawk_rtx_t*)*run);
			//run->formatError (HAWK_EOPEN, HAWK_NULL, HAWK_T("unable to open %js - %js"), std_names[std], bem);
			hawk_rtx_seterrfmt((hawk_rtx_t*)*run, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), std_names[std], bem);
		}
		else
		{
			const hawk_ooch_t* bem = hawk_backuperrmsg((hawk_t*)*hawk);
			//hawk->formatError (HAWK_EOPEN, HAWK_NULL, HAWK_T("unable to open %js - %js"), std_names[std], bem);
			hawk_seterrfmt((hawk_t*)*hawk, HAWK_NULL, HAWK_EOPEN, HAWK_T("unable to open %js - %js"), std_names[std], bem);
		}
	}
	return sio;
}

HawkStd::~HawkStd ()
{
	this->close();
}

int HawkStd::open ()
{
	int n = Hawk::open();
	if (HAWK_UNLIKELY(n <= -1)) return n;

	this->gbl_argc = this->addGlobal(HAWK_T("ARGC"));
	this->gbl_argv = this->addGlobal(HAWK_T("ARGV"));
	this->gbl_environ = this->addGlobal(HAWK_T("ENVIRON"));
	if (this->gbl_argc <= -1 || this->gbl_argv <= -1 ||  this->gbl_environ <= -1) goto oops;

	if (this->addFunction(HAWK_T("rand"),       1, 0, HAWK_T("math"),   HAWK_NULL,                            0) <= -1 ||
	    this->addFunction(HAWK_T("srand"),      1, 0, HAWK_T("math"),   HAWK_NULL,                            0) <= -1 ||
	    this->addFunction(HAWK_T("system"),     1, 0, HAWK_T("sys"),    HAWK_NULL,                            0) <= -1 ||
	    this->addFunction(HAWK_T("setioattr"),  3, 3, HAWK_NULL,        (FunctionHandler)&HawkStd::setioattr, HAWK_RIO) <= -1 ||
	    this->addFunction(HAWK_T("getioattr"),  3, 3, HAWK_T("vvr"),    (FunctionHandler)&HawkStd::getioattr, HAWK_RIO) <= -1)
	{
		goto oops;
	}

	if (!this->stdmod_up)
	{
	/*
	#if defined(USE_DLFCN)
		if (hawk_setopt(this->hawk, HAWK_OPT_MODPOSTFIX, HAWK_T(".so")) <= -1) goto oops;
	#endif
	*/

		if (hawk_stdmodstartup(this->hawk) <= -1) goto oops;
		this->stdmod_up = true;
	}

	this->cmgrtab_inited = false;
	return 0;

oops:
	Hawk::close ();
	return -1;
}

void HawkStd::close ()
{
	if (this->cmgrtab_inited)
	{
		hawk_htb_fini (&this->cmgrtab);
		this->cmgrtab_inited = false;
	}

	clearConsoleOutputs ();

	//
	// HawkStd called hawk_stdmodstartup() after Hawk::open().
	// It's logical to call hawk_stdmodshutdown() in Hawk::close().
	// but Hawk::close() still needs to call some module's fini and
	// unload functions. So it must be done in HawkStd::uponClosing()
	// which is called after modules have been unloaded but while
	// the underlying hawk object is still alive.
	//
	// See HawkStd::uponClosing() below.
	//
	//if (this->stdmod_up)
	//{
	//	hawk_stdmodshutdown (this->hawk);
	//	this->stdmod_up = false;
	//}
	//

	Hawk::close ();
}

void HawkStd::uponClosing ()
{
	if (this->stdmod_up)
	{
		hawk_stdmodshutdown (this->hawk);
		this->stdmod_up = false;
	}

	// chain up
	Hawk::uponClosing ();
}

HawkStd::Run* HawkStd::parse (Source& in, Source& out)
{
	Run* run = Hawk::parse(in, out);

	if (this->cmgrtab_inited)
	{
		// if cmgrtab has already been initialized,
		// just clear the contents regardless of
		// parse() result.
		hawk_htb_clear (&this->cmgrtab);
	}
	else if (run && (this->getTrait() & HAWK_RIO))
	{
		// it initialized cmgrtab only if HAWK_RIO is set.
		// but if you call parse() multiple times while
		// setting and unsetting HAWK_RIO in-between,
		// cmgrtab can still be initialized when HAWK_RIO is not set.
		if (hawk_htb_init(&this->cmgrtab, *this, 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1)
		{
			this->setError (HAWK_ENOMEM);
			return HAWK_NULL;
		}
		hawk_htb_setstyle (&this->cmgrtab, hawk_get_htb_style(HAWK_HTB_STYLE_INLINE_KEY_COPIER));
		this->cmgrtab_inited = true;
	}

	if (run && make_additional_globals(run) <= -1) return HAWK_NULL;

	return run;
}

int HawkStd::build_argcv (Run* run)
{
	Value argv(run);

	for (hawk_oow_t i = 0; i < this->runarg.len; i++)
	{
		if (argv.setIndexedStr(Value::IntIndex(i), this->runarg.ptr[i].ptr, this->runarg.ptr[i].len, true) <= -1) return -1;
	}

	run->setGlobal(this->gbl_argc, (hawk_int_t)this->runarg.len);
	run->setGlobal(this->gbl_argv, argv);
	return 0;
}


int HawkStd::build_environ (Run* run, env_char_t* envarr[])
{
	Value v_env (run);

	if (envarr)
	{
		env_char_t* eq;
		hawk_ooch_t* kptr, * vptr;
		hawk_oow_t klen, count;
		hawk_rtx_t* rtx = *run;

		for (count = 0; envarr[count]; count++)
		{
		#if ((defined(HAWK_STD_ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(HAWK_STD_ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
			eq = hawk_find_oochar_in_oocstr(envarr[count], HAWK_T('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			kptr = envarr[count];
			klen = eq - envarr[count];
			vptr = eq + 1;
		#elif defined(HAWK_STD_ENV_CHAR_IS_BCH)
			eq = hawk_find_bchar_in_bcstr(envarr[count], HAWK_BT('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			*eq = HAWK_BT('\0');

			kptr = hawk_rtx_dupbtoucstr(rtx, envarr[count], &klen, 1);
			vptr = hawk_rtx_dupbtoucstr(rtx, eq + 1, HAWK_NULL, 1);
			if (kptr == HAWK_NULL || vptr == HAWK_NULL)
			{
				if (kptr) hawk_rtx_freemem(rtx, kptr);
				if (vptr) hawk_rtx_freemem(rtx, kptr);

				/* mbstowcsdup() may fail for invalid encoding.
				 * so setting the error code to ENOMEM may not
				 * be really accurate */
				this->setError (HAWK_ENOMEM);
				return -1;
			}

			*eq = HAWK_BT('=');
		#else
			eq = hawk_find_uchar_in_ucstr(envarr[count], HAWK_UT('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			*eq = HAWK_UT('\0');

			kptr = hawk_rtx_duputobcstr(rtx, envarr[count], &klen);
			vptr = hawk_rtx_duputobcstr(rtx, eq + 1, HAWK_NULL);
			if (kptr == HAWK_NULL || vptr == HAWK_NULL)
			{
		#if ((defined(HAWK_STD_ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(HAWK_STD_ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
				/* nothing to do */
		#else
				if (vptr) hawk_rtx_freemem(rtx, vptr);
				if (kptr) hawk_rtx_freemem(rtx, kptr);
		#endif

				this->setError (HAWK_ENOMEM);
				return -1;
			}

			*eq = HAWK_UT('=');
		#endif

			// numeric string
			v_env.setIndexedStr (Value::Index(kptr, klen), vptr, true);

		#if ((defined(HAWK_STD_ENV_CHAR_IS_BCH) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(HAWK_STD_ENV_CHAR_IS_UCH) && defined(HAWK_OOCH_IS_UCH)))
				/* nothing to do */
		#else
			if (vptr) hawk_rtx_freemem(rtx, vptr);
			if (kptr) hawk_rtx_freemem(rtx, kptr);
		#endif
		}
	}

	return run->setGlobal(this->gbl_environ, v_env);
}

int HawkStd::make_additional_globals (Run* run)
{
	/* TODO: use wenviron where it's available */

	if (this->build_argcv(run) <= -1 || this->build_environ(run, SYSTEM_ENVIRON) <= -1) return -1;

	return 0;
}

hawk_cmgr_t* HawkStd::getiocmgr (const hawk_ooch_t* ioname)
{
	HAWK_ASSERT(this->cmgrtab_inited == true);

#if defined(HAWK_OOCH_IS_UCH)
	ioattr_t* ioattr = get_ioattr(ioname, hawk_count_oocstr(ioname));
	if (ioattr) return ioattr->cmgr;
#endif
	return HAWK_NULL;
}

HawkStd::ioattr_t* HawkStd::get_ioattr (const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search (&this->cmgrtab, ptr, len);
	if (pair == HAWK_NULL) return HAWK_NULL;

	return (ioattr_t*)HAWK_HTB_VPTR(pair);
}

HawkStd::ioattr_t* HawkStd::find_or_make_ioattr (const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search (&this->cmgrtab, ptr, len);
	if (pair == HAWK_NULL)
	{
		pair = hawk_htb_insert (
			&this->cmgrtab, (void*)ptr, len,
			(void*)&HawkStd::default_ioattr,
			HAWK_SIZEOF(HawkStd::default_ioattr));
		if (pair == HAWK_NULL)
		{
			this->setError (HAWK_ENOMEM);
			return HAWK_NULL;
		}
	}

	return (ioattr_t*)HAWK_HTB_VPTR(pair);
}

static int timeout_code (const hawk_ooch_t* name)
{
	if (hawk_comp_oocstr(name, HAWK_T("rtimeout"), 1) == 0) return 0;
	if (hawk_comp_oocstr(name, HAWK_T("wtimeout"), 1) == 0) return 1;
	if (hawk_comp_oocstr(name, HAWK_T("ctimeout"), 1) == 0) return 2;
	if (hawk_comp_oocstr(name, HAWK_T("atimeout"), 1) == 0) return 3;
	return -1;
}

int HawkStd::setioattr (
	Run& run, Value& ret, Value* args, hawk_oow_t nargs,
	const hawk_ooch_t* name, hawk_oow_t len)
{
	HAWK_ASSERT(this->cmgrtab_inited == true);
	hawk_oow_t l[3];
	const hawk_ooch_t* ptr[3];

	ptr[0] = args[0].toStr(&l[0]);
	ptr[1] = args[1].toStr(&l[1]);
	ptr[2] = args[2].toStr(&l[2]);

	if (hawk_find_oochar_in_oochars(ptr[0], l[0], '\0') ||
	    hawk_find_oochar_in_oochars(ptr[1], l[1], '\0') ||
	    hawk_find_oochar_in_oochars(ptr[2], l[2], '\0'))
	{
		return ret.setInt((hawk_int_t)-1);
	}

	int tmout;
	if ((tmout = timeout_code (ptr[1])) >= 0)
	{
		hawk_int_t lv;
		hawk_flt_t fv;
		int n;

		n = args[2].getNum(&lv, &fv);
		if (n <= -1) return -1;

		ioattr_t* ioattr = find_or_make_ioattr (ptr[0], l[0]);
		if (ioattr == HAWK_NULL) return -1;

		if (n == 0)
		{
			ioattr->tmout[tmout].sec = lv;
			ioattr->tmout[tmout].nsec = 0;
		}
		else
		{
			hawk_flt_t nsec;
			ioattr->tmout[tmout].sec = (hawk_int_t)fv;
			nsec = fv - ioattr->tmout[tmout].sec;
			ioattr->tmout[tmout].nsec = HAWK_SEC_TO_NSEC(nsec);
		}
		return ret.setInt((hawk_int_t)0);
	}
#if defined(HAWK_OOCH_IS_UCH)
	else if (hawk_comp_oocstr(ptr[1], HAWK_T("codepage"), 1) == 0 ||
	         hawk_comp_oocstr(ptr[1], HAWK_T("encoding"), 1) == 0)
	{
		ioattr_t* ioattr;
		hawk_cmgr_t* cmgr;

		if (ptr[2][0] == HAWK_T('\0')) cmgr = HAWK_NULL;
		else
		{
			cmgr = hawk_get_cmgr_by_name(ptr[2]);
			if (cmgr == HAWK_NULL) return ret.setInt((hawk_int_t)-1);
		}

		ioattr = find_or_make_ioattr(ptr[0], l[0]);
		if (ioattr == HAWK_NULL) return -1;

		ioattr->cmgr = cmgr;
		hawk_copy_oocstr (ioattr->cmgr_name, HAWK_COUNTOF(ioattr->cmgr_name), ptr[2]);
		return 0;
	}
#endif
	else
	{
		// unknown attribute name
		return ret.setInt((hawk_int_t)-1);
	}
}

int HawkStd::getioattr (
	Run& run, Value& ret, Value* args, hawk_oow_t nargs,
	const hawk_ooch_t* name, hawk_oow_t len)
{
	HAWK_ASSERT(this->cmgrtab_inited == true);
	hawk_oow_t l[2];
	const hawk_ooch_t* ptr[2];

	ptr[0] = args[0].toStr(&l[0]);
	ptr[1] = args[1].toStr(&l[1]);

	int xx = -1;

	/* ionames must not contains a null character */
	if (hawk_find_oochar_in_oochars(ptr[0], l[0], '\0') == HAWK_NULL &&
	    hawk_find_oochar_in_oochars(ptr[1], l[1], '\0') == HAWK_NULL)
	{
		ioattr_t* ioattr = get_ioattr(ptr[0], l[0]);
		if (ioattr == HAWK_NULL) ioattr = &HawkStd::default_ioattr;

		int tmout;
		if ((tmout = timeout_code(ptr[1])) >= 0)
		{
			if (ioattr->tmout[tmout].nsec == 0)
				xx = args[2].setInt((hawk_int_t)ioattr->tmout[tmout].sec);
			else
				xx = args[2].setFlt((hawk_flt_t)ioattr->tmout[tmout].sec + HAWK_NSEC_TO_SEC((hawk_flt_t)ioattr->tmout[tmout].nsec));
		}
	#if defined(HAWK_OOCH_IS_UCH)
		else if (hawk_comp_oocstr(ptr[1], HAWK_T("codepage"), 1) == 0 ||
		         hawk_comp_oocstr(ptr[1], HAWK_T("encoding"), 1) == 0)
		{
			xx = args[2].setStr(ioattr->cmgr_name);
		}
	#endif
	}

	// unknown attribute name or errors
	return ret.setInt((hawk_int_t)xx);
}

#if defined(ENABLE_NWIO)
int HawkStd::open_nwio (Pipe& io, int flags, void* nwad)
{
	hawk_nwio_tmout_t tmout_buf;
	hawk_nwio_tmout_t* tmout = HAWK_NULL;

	const hawk_ooch_t* name = io.getName();
	ioattr_t* ioattr = get_ioattr(name, hawk_count_oocstr(name));
	if (ioattr)
	{
		tmout = &tmout_buf;
		tmout->r = ioattr->tmout[0];
		tmout->w = ioattr->tmout[1];
		tmout->c = ioattr->tmout[2];
		tmout->a = ioattr->tmout[3];
	}

	hawk_nwio_t* handle = hawk_nwio_open (
		this->getMmgr(), 0, (hawk_nwad_t*)nwad,
		flags | HAWK_NWIO_TEXT | HAWK_NWIO_IGNOREECERR |
		HAWK_NWIO_REUSEADDR | HAWK_NWIO_READNORETRY | HAWK_NWIO_WRITENORETRY,
		tmout
	);
	if (handle == HAWK_NULL) return -1;

#if defined(HAWK_OOCH_IS_UCH)
	hawk_cmgr_t* cmgr = this->getiocmgr(io.getName());
	if (cmgr) hawk_nwio_setcmgr(handle, cmgr);
#endif

	io.setHandle ((void*)handle);
	io.setUflags (1);

	return 1;
}
#endif

int HawkStd::open_pio (Pipe& io)
{
	Hawk::Pipe::Mode mode = io.getMode();
	hawk_pio_t* pio = HAWK_NULL;
	int flags = HAWK_PIO_TEXT | HAWK_PIO_SHELL | HAWK_PIO_IGNOREECERR;

	switch (mode)
	{
		case Hawk::Pipe::READ:
			/* TODO: should we specify ERRTOOUT? */
			flags |= HAWK_PIO_READOUT | HAWK_PIO_ERRTOOUT;
			break;

		case Hawk::Pipe::WRITE:
			flags |= HAWK_PIO_WRITEIN;
			break;

		case Hawk::Pipe::RW:
			flags |= HAWK_PIO_READOUT | HAWK_PIO_ERRTOOUT | HAWK_PIO_WRITEIN;
			break;
	}

	pio = hawk_pio_open((hawk_gem_t*)*this, 0, io.getName(), flags);
	if (!pio) return -1;

#if defined(HAWK_OOCH_IS_UCH)
	hawk_cmgr_t* cmgr = this->getiocmgr(io.getName());
	if (cmgr)
	{
		hawk_pio_setcmgr(pio, HAWK_PIO_IN, cmgr);
		hawk_pio_setcmgr(pio, HAWK_PIO_OUT, cmgr);
		hawk_pio_setcmgr(pio, HAWK_PIO_ERR, cmgr);
	}
#endif
	io.setHandle (pio);
	io.setUflags (0);
	return 1;
}

#if defined(ENABLE_NWIO)
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
	hawk_oow_t i;

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

int HawkStd::openPipe (Pipe& io)
{
#if defined(ENABLE_NWIO)
	int flags;
	hawk_nwad_t nwad;

	if (io.getMode() != Hawk::Pipe::RW ||
	    parse_rwpipe_uri(io.getName(), &flags, &nwad) <= -1)
	{
		return open_pio(io);
	}
	else
	{
		return open_nwio(io, flags, &nwad);
	}
#else
	return this->open_pio(io);
#endif
}

int HawkStd::closePipe (Pipe& io)
{
#if defined(ENABLE_NWIO)
	if (io.getUflags() > 0)
	{
		/* nwio can't honor partical close */
		hawk_nwio_close ((hawk_nwio_t*)io.getHandle());
	}
	else
	{
#endif
		hawk_pio_t* pio = (hawk_pio_t*)io.getHandle();
		if (io.getMode() == Hawk::Pipe::RW)
		{
			Pipe::CloseMode rwcopt = io.getCloseMode();
			if (rwcopt == Hawk::Pipe::CLOSE_READ)
			{
				hawk_pio_end (pio, HAWK_PIO_IN);
				return 0;
			}
			else if (rwcopt == Hawk::Pipe::CLOSE_WRITE)
			{
				hawk_pio_end (pio, HAWK_PIO_OUT);
				return 0;
			}
		}

		hawk_pio_close (pio);
#if defined(ENABLE_NWIO)
	}
#endif
	return 0;
}

hawk_ooi_t HawkStd::readPipe (Pipe& io, hawk_ooch_t* buf, hawk_oow_t len)
{
#if defined(ENABLE_NWIO)
	return (io.getUflags() > 0)?
		hawk_nwio_read((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_read((hawk_pio_t*)io.getHandle(), HAWK_PIO_OUT, buf, len);
#else
	return hawk_pio_read((hawk_pio_t*)io.getHandle(), HAWK_PIO_OUT, buf, len);
#endif
}

hawk_ooi_t HawkStd::readPipeBytes (Pipe& io, hawk_bch_t* buf, hawk_oow_t len)
{
#if defined(ENABLE_NWIO)
	return (io.getUflags() > 0)?
		hawk_nwio_readbytes((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_readbytes((hawk_pio_t*)io.getHandle(), HAWK_PIO_OUT, buf, len);
#else
	return hawk_pio_readbytes((hawk_pio_t*)io.getHandle(), HAWK_PIO_OUT, buf, len);
#endif
}

hawk_ooi_t HawkStd::writePipe (Pipe& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
#if defined(ENABLE_NWIO)
	return (io.getUflags() > 0)?
		hawk_nwio_write((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_write((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN, buf, len);
#else
	return hawk_pio_write((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN, buf, len);
#endif
}

hawk_ooi_t HawkStd::writePipeBytes (Pipe& io, const hawk_bch_t* buf, hawk_oow_t len)
{
#if defined(ENABLE_NWIO)
	return (io.getUflags() > 0)?
		hawk_nwio_writebytes((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_writebytes((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN, buf, len);
#else
	return hawk_pio_writebytes((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN, buf, len);
#endif
}

int HawkStd::flushPipe (Pipe& io)
{
#if defined(ENABLE_NWIO)
	return (io.getUflags() > 0)?
		hawk_nwio_flush((hawk_nwio_t*)io.getHandle()):
		hawk_pio_flush((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN);
#else
	return hawk_pio_flush((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN);
#endif
}

int HawkStd::openFile (File& io)
{
	Hawk::File::Mode mode = io.getMode();
	hawk_sio_t* sio = HAWK_NULL;
	int flags = HAWK_SIO_IGNOREECERR;

	switch (mode)
	{
		case Hawk::File::READ:
			flags |= HAWK_SIO_READ;
			break;
		case Hawk::File::WRITE:
			flags |= HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE;
			break;
		case Hawk::File::APPEND:
			flags |= HAWK_SIO_APPEND | HAWK_SIO_CREATE;
			break;
	}

	const hawk_ooch_t* ioname = io.getName();
	if (ioname[0] == '-' && ioname[1] == '\0')
	{
		if (mode == Hawk::File::READ)
			sio = open_sio_std(HAWK_NULL, io, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
		else
			sio = open_sio_std(HAWK_NULL, io, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
	}
	else
	{
		sio = hawk_sio_open((hawk_gem_t*)*this, 0, ioname, flags);
	}
	if (!sio) return -1;
#if defined(HAWK_OOCH_IS_UCH)
	hawk_cmgr_t* cmgr = this->getiocmgr(ioname);
	if (cmgr) hawk_sio_setcmgr(sio, cmgr);
#endif

	io.setHandle (sio);
	return 1;
}

int HawkStd::closeFile (File& io)
{
	hawk_sio_close((hawk_sio_t*)io.getHandle());
	return 0;
}

hawk_ooi_t HawkStd::readFile (File& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t HawkStd::readFileBytes (File& io, hawk_bch_t* buf, hawk_oow_t len)
{
	return hawk_sio_getbchars((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t HawkStd::writeFile (File& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t HawkStd::writeFileBytes (File& io, const hawk_bch_t* buf, hawk_oow_t len)
{
	return hawk_sio_putbchars((hawk_sio_t*)io.getHandle(), buf, len);
}

int HawkStd::flushFile (File& io)
{
	return hawk_sio_flush((hawk_sio_t*)io.getHandle());
}

void HawkStd::setConsoleCmgr (const hawk_cmgr_t* cmgr)
{
	this->console_cmgr = (hawk_cmgr_t*)cmgr;
}

const hawk_cmgr_t* HawkStd::getConsoleCmgr () const
{
	return this->console_cmgr;
}

int HawkStd::addConsoleOutput (const hawk_uch_t* arg, hawk_oow_t len)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = this->ofile.add(this->hawk, arg, len);
	if (n <= -1) this->setError (HAWK_ENOMEM);
	return n;
}

int HawkStd::addConsoleOutput (const hawk_uch_t* arg)
{
	return this->addConsoleOutput(arg, hawk_count_ucstr(arg));
}

int HawkStd::addConsoleOutput (const hawk_bch_t* arg, hawk_oow_t len)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = this->ofile.add(this->hawk, arg, len);
	if (n <= -1) this->setError (HAWK_ENOMEM);
	return n;
}

int HawkStd::addConsoleOutput (const hawk_bch_t* arg)
{
	return this->addConsoleOutput(arg, hawk_count_bcstr(arg));
}


void HawkStd::clearConsoleOutputs ()
{
	this->ofile.clear (this->hawk);
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

	hawk_rtx_freemem(rtx, dstr);
	return n;
}

int HawkStd::open_console_in (Console& io)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)io;
	hawk_sio_t* sio;
	hawk_val_t* v_argc, * v_argv, * v_pair;
	hawk_int_t i_argc;
	const hawk_ooch_t* file;
	hawk_map_t* map;
	hawk_map_pair_t* pair;
	hawk_ooch_t ibuf[128];
	hawk_oow_t ibuflen;
	hawk_oocs_t as;
	int x;

	v_argc = hawk_rtx_getgbl(rtx, this->gbl_argc);
	HAWK_ASSERT(v_argc != HAWK_NULL);
	if (hawk_rtx_valtoint(rtx, v_argc, &i_argc) <= -1) return -1;

	/* handle special case when ARGV[x] has been altered.
	 * so from here down, the file name gotten from
	 * rxtn->c.in.files is not important and is overridden
	 * from ARGV.
	 * 'BEGIN { ARGV[1]="file3"; }
	 *        { print $0; }' file1 file2
	 */
	v_argv = hawk_rtx_getgbl(rtx, this->gbl_argv);
	HAWK_ASSERT(v_argv != HAWK_NULL);
	if (HAWK_RTX_GETVALTYPE(rtx, v_argv) != HAWK_VAL_MAP)
	{
		/* with flexmap on, you can change ARGV to a scalar.
		 *   BEGIN { ARGV="xxx"; }
		 * you must not do this. */
		hawk_rtx_seterrfmt(rtx, HAWK_NULL, HAWK_EINVAL, HAWK_T("phony value in ARGV"));
		return -1;
	}

	map = ((hawk_val_map_t*)v_argv)->map;
	HAWK_ASSERT(map != HAWK_NULL);

nextfile:
	if ((hawk_int_t)this->runarg_index >= (i_argc - 1))  /* ARGV is a kind of 0-based array unlike other normal arrays or substring indexing scheme */
	{
		/* reached the last ARGV */

		if (this->runarg_count <= 0) /* but no file has been ever opened */
		{
		console_open_stdin:
			/* open stdin */
			sio = open_sio_std(HAWK_NULL, io, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			if (sio == HAWK_NULL) return -1;

			if (this->console_cmgr) hawk_sio_setcmgr(sio, this->console_cmgr);

			io.setHandle (sio);
			this->runarg_count++;
			return 1;
		}

		return 0;
	}

	ibuflen = hawk_int_to_oocstr(this->runarg_index + 1, 10, HAWK_NULL, ibuf, HAWK_COUNTOF(ibuf));
	pair = hawk_map_search(map, ibuf, ibuflen);
	if (!pair)
	{
		if (this->runarg_count < i_argc)
		{
			this->runarg_index++;
			goto nextfile;
		}
		if (this->runarg_count <= 0) goto console_open_stdin;
		return 0;
	}

	v_pair = (hawk_val_t*)HAWK_HTB_VPTR(pair);
	HAWK_ASSERT(v_pair != HAWK_NULL);

	as.ptr = hawk_rtx_getvaloocstr(rtx, v_pair, &as.len);
	if (HAWK_UNLIKELY(!as.ptr)) return -1;

	if (as.len == 0)
	{
		/* the name is empty */
		hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
		this->runarg_index++;
		goto nextfile;
	}

	if (hawk_count_oocstr(as.ptr) < as.len)
	{
		/* the name contains one or more '\0' */
		((Run*)io)->formatError (HAWK_EIONMNL, HAWK_NULL, HAWK_T("invalid I/O name of length %zu containing '\\0'"), as.len);
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
		this->runarg_index++;
		goto nextfile;
	}

	if (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))
		sio = open_sio_std(HAWK_NULL, io, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
	else
		sio = open_sio(HAWK_NULL, io, file, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
	if (sio == HAWK_NULL)
	{
		hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
		return -1;
	}

	if (hawk_rtx_setfilenamewithoochars(rtx, file, hawk_count_oocstr(file)) <= -1)
	{
		hawk_sio_close(sio);
		hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);
		return -1;
	}

	hawk_rtx_freevaloocstr (rtx, v_pair, as.ptr);

	if (this->console_cmgr) hawk_sio_setcmgr(sio, this->console_cmgr);

	io.setHandle (sio);

	/* increment the counter of files successfully opened */
	this->runarg_count++;
	this->runarg_index++;
	return 1;
}

int HawkStd::open_console_out (Console& io)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)io;

	if (this->ofile.ptr == HAWK_NULL)
	{
		HAWK_ASSERT(this->ofile.len == 0 && this->ofile.capa == 0);

		if (this->ofile_count == 0)
		{
			hawk_sio_t* sio;
			sio = open_sio_std(
				HAWK_NULL, io, HAWK_SIO_STDOUT,
				HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			if (sio == HAWK_NULL) return -1;

			if (this->console_cmgr) hawk_sio_setcmgr(sio, this->console_cmgr);

			io.setHandle (sio);
			this->ofile_count++;
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

		file = this->ofile.ptr[this->ofile_index].ptr;

		if (file == HAWK_NULL)
		{
			/* no more input file */
			return 0;
		}

		if (hawk_count_oocstr(file) != this->ofile.ptr[this->ofile_index].len)
		{
			((Run*)io)->formatError (HAWK_EIONMNL, HAWK_NULL, HAWK_T("invalid I/O name of length %zu containing '\\0'"), this->ofile.ptr[this->ofile_index].len);
			return -1;
		}

		if (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))
			sio = open_sio_std(HAWK_NULL, io, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
		else
			sio = open_sio(HAWK_NULL, io, file, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
		if (sio == HAWK_NULL) return -1;

		if (hawk_rtx_setofilenamewithoochars(rtx, file, hawk_count_oocstr(file)) == -1)
		{
			hawk_sio_close(sio);
			return -1;
		}

		if (this->console_cmgr) hawk_sio_setcmgr(sio, this->console_cmgr);
		io.setHandle (sio);

		this->ofile_index++;
		this->ofile_count++;
		return 1;
	}
}

int HawkStd::openConsole (Console& io)
{
	Console::Mode mode = io.getMode();

	if (mode == Console::READ)
	{
		this->runarg_count = 0;
		this->runarg_index = 0;
		return open_console_in (io);
	}
	else
	{
		HAWK_ASSERT(mode == Console::WRITE);

		this->ofile_count = 0;
		this->ofile_index = 0;
		return open_console_out (io);
	}
}

int HawkStd::closeConsole (Console& io)
{
	hawk_sio_close((hawk_sio_t*)io.getHandle());
	return 0;
}

hawk_ooi_t HawkStd::readConsole (Console& io, hawk_ooch_t* data, hawk_oow_t size)
{
	hawk_ooi_t nn;

	while ((nn = hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), data, size)) == 0)
	{
		int n;
		hawk_sio_t* sio = (hawk_sio_t*)io.getHandle();

		n = open_console_in(io);
		if (n == -1) return -1;

		if (n == 0)
		{
			/* no more input console */
			return 0;
		}

		if (sio) hawk_sio_close(sio);
		((Run*)io)->setGlobal(HAWK_GBL_FNR, (hawk_int_t)0);
		io.setSwitched(true); // indicates that the console medium switched
	}

	return nn;
}

hawk_ooi_t HawkStd::readConsoleBytes (Console& io, hawk_bch_t* data, hawk_oow_t size)
{
	hawk_ooi_t nn;

	while ((nn = hawk_sio_getbchars((hawk_sio_t*)io.getHandle(), data, size)) == 0)
	{
		int n;
		hawk_sio_t* sio = (hawk_sio_t*)io.getHandle();

		n = open_console_in(io);
		if (n == -1) return -1;

		if (n == 0)
		{
			/* no more input console */
			return 0;
		}

		if (sio) hawk_sio_close(sio);
		((Run*)io)->setGlobal(HAWK_GBL_FNR, (hawk_int_t)0);
		io.setSwitched(true); // indicates that the data comes from a new medium
	}

	return nn;
}

hawk_ooi_t HawkStd::writeConsole (Console& io, const hawk_ooch_t* data, hawk_oow_t size)
{
	return hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), data, size);
}

hawk_ooi_t HawkStd::writeConsoleBytes (Console& io, const hawk_bch_t* data, hawk_oow_t size)
{
	return hawk_sio_putbchars((hawk_sio_t*)io.getHandle(), data, size);
}

int HawkStd::flushConsole (Console& io)
{
	return hawk_sio_flush ((hawk_sio_t*)io.getHandle());
}

int HawkStd::nextConsole (Console& io)
{
	int n;
	hawk_sio_t* sio = (hawk_sio_t*)io.getHandle();

	n = (io.getMode() == Console::READ)?
		open_console_in(io): open_console_out(io);
	if (n == -1) return -1;

	if (n == 0)
	{
		/* if there is no more file, keep the previous handle */
		return 0;
	}

	if (sio) hawk_sio_close(sio);
	return n;
}

// memory allocation primitives
void* HawkStd::allocMem (hawk_oow_t n)
{
	return ::malloc (n);
}

void* HawkStd::reallocMem (void* ptr, hawk_oow_t n)
{
	return ::realloc (ptr, n);
}

void  HawkStd::freeMem (void* ptr)
{
	::free (ptr);
}

// miscellaneous primitive

hawk_flt_t HawkStd::pow (hawk_flt_t x, hawk_flt_t y)
{
	return hawk_stdmathpow(this->hawk, x, y);
}

hawk_flt_t HawkStd::mod (hawk_flt_t x, hawk_flt_t y)
{
	return hawk_stdmathmod(this->hawk, x, y);
}

void* HawkStd::modopen (const hawk_mod_spec_t* spec)
{
	void* h;
	h = hawk_stdmodopen(this->hawk, spec);
	if (!h) this->retrieveError ();
	return h;
}

void HawkStd::modclose (void* handle)
{
	hawk_stdmodclose (this->hawk, handle);
}

void* HawkStd::modgetsym (void* handle, const hawk_ooch_t* name)
{
	void* s;
	s = hawk_stdmodgetsym(this->hawk, handle, name);
	if (!s) this->retrieveError ();
	return s;
}

int HawkStd::SourceFile::open (Data& io)
{
	hawk_sio_t* sio;

	if (io.isMaster())
	{
		hawk_ooch_t*  xpath;

		xpath = (this->_type == NAME_UCH)?
			hawk_addsionamewithuchars((hawk_t*)io, (const hawk_uch_t*)this->_name, hawk_count_ucstr((const hawk_uch_t*)this->_name)):
			hawk_addsionamewithbchars((hawk_t*)io, (const hawk_bch_t*)this->_name, hawk_count_bcstr((const hawk_bch_t*)this->_name));
		if (HAWK_UNLIKELY(!xpath)) return -1;

		if (xpath[0] == HAWK_T('-') && xpath[1] == HAWK_T('\0'))
		{
			if (io.getMode() == READ)
				sio = open_sio_std(
					io, HAWK_NULL, HAWK_SIO_STDIN,
					HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			else
				sio = open_sio_std(
					io, HAWK_NULL, HAWK_SIO_STDOUT,
					HAWK_SIO_WRITE | HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			if (sio == HAWK_NULL) return -1;
		}
		else
		{
			sio = open_sio(
				io, HAWK_NULL, xpath,
				(io.getMode() == READ?
					(HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH):
					(HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR))
			);
			if (sio == HAWK_NULL) return -1;
		}

		if (this->cmgr) hawk_sio_setcmgr(sio, this->cmgr);
		io.setName (xpath);
		io.setPath (xpath); // let the parser use this path, especially upon an error
	}
	else
	{
		// open an included file
		const hawk_ooch_t* ioname;
		hawk_ooch_t* xpath;

		ioname = io.getName();
		HAWK_ASSERT(ioname != HAWK_NULL);

		if (io.getPrevHandle())
		{
			const hawk_ooch_t* outer, * path;
			hawk_ooch_t fbuf[64];
			hawk_ooch_t* dbuf = HAWK_NULL;

			path = ioname;
			outer = hawk_sio_getpath((hawk_sio_t*)io.getPrevHandle());
			if (outer)
			{
				const hawk_ooch_t* base;

				base = hawk_get_base_name_oocstr(outer);
				if (base != outer && ioname[0] != HAWK_T('/'))
				{
					hawk_oow_t tmplen, totlen, dirlen;

					dirlen = base - outer;
					totlen = hawk_count_oocstr(ioname) + dirlen;
					if (totlen >= HAWK_COUNTOF(fbuf))
					{
						dbuf = (hawk_ooch_t*)hawk_allocmem((hawk_t*)io, HAWK_SIZEOF(hawk_ooch_t) * (totlen + 1));
						if (!dbuf) return -1;
						path = dbuf;
					}
					else path = fbuf;

					tmplen = hawk_copy_oochars_to_oocstr_unlimited((hawk_ooch_t*)path, outer, dirlen);
					hawk_copy_oocstr_unlimited ((hawk_ooch_t*)path + tmplen, ioname);
				}
			}

			if (!hawk_stdplainfileexists((hawk_t*)io, path) && ((hawk_t*)io)->opt.includedirs.len > 0 && ioname[0] != '/')
			{
				const hawk_ooch_t* tmp;
				tmp = hawk_stdgetfileindirs((hawk_t*)io, &((hawk_t*)io)->opt.includedirs, ioname);
				if (tmp) path = tmp;
			}

			xpath = hawk_addsionamewithoochars((hawk_t*)io, path, hawk_count_oocstr(path));
			if (dbuf) hawk_freemem((hawk_t*)io, dbuf);
		}
		else
		{
			const hawk_ooch_t* path;

			path = ioname;
			if (!hawk_stdplainfileexists((hawk_t*)io, path) && ((hawk_t*)io)->opt.includedirs.len > 0 && ioname[0] != '/')
			{
				const hawk_ooch_t* tmp;
				tmp = hawk_stdgetfileindirs((hawk_t*)io, &((hawk_t*)io)->opt.includedirs, ioname);
				if (tmp) path = tmp;
			}

			xpath = hawk_addsionamewithoochars((hawk_t*)io, path, hawk_count_oocstr(path));
		}
		if (!xpath) return -1;

		sio = open_sio(
			io, HAWK_NULL, xpath,
			(io.getMode() == READ?
				(HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH):
				(HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR))
		);
		if (!sio) return -1;

		io.setPath (xpath);
		io.setHandle (sio);
		if (this->cmgr) hawk_sio_setcmgr(sio, this->cmgr);
	}

	io.setHandle (sio);
	return 1;
}

int HawkStd::SourceFile::close (Data& io)
{
	hawk_sio_t* sio = (hawk_sio_t*)io.getHandle();
	hawk_sio_flush (sio);
	hawk_sio_close(sio);
	return 0;
}

hawk_ooi_t HawkStd::SourceFile::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t HawkStd::SourceFile::write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), buf, len);
}

HawkStd::SourceString::~SourceString ()
{
	HAWK_ASSERT(this->_hawk == HAWK_NULL);
	HAWK_ASSERT(this->str == HAWK_NULL);
}

int HawkStd::SourceString::open (Data& io)
{
	hawk_sio_t* sio;

	if (io.getName() == HAWK_NULL)
	{
		// open the main source file.
		if (io.getMode() == WRITE)
		{
			// SourceString does not support writing.
			((Hawk*)io)->setError (HAWK_ENOIMPL);
			return -1;
		}

		if (!this->str)
		{
			this->_hawk = (hawk_t*)io;
			if (this->_type == STR_UCH)
			{
			#if defined(HAWK_OOCH_IS_UCH)
				this->str = hawk_dupucstr(this->_hawk, (const hawk_uch_t*)this->_str, HAWK_NULL);
			#else
				this->str = hawk_duputobcstr(this->_hawk, (const hawk_uch_t*)this->_str, HAWK_NULL);
			#endif
			}
			else
			{
				HAWK_ASSERT(this->_type == STR_BCH);
			#if defined(HAWK_OOCH_IS_UCH)
				this->str = hawk_dupbtoucstr(this->_hawk, (const hawk_bch_t*)this->_str, HAWK_NULL, 0);
			#else
				this->str = hawk_dupbcstr(this->_hawk, (const hawk_bch_t*)this->_str, HAWK_NULL);
			#endif
			}
			if (HAWK_UNLIKELY(!this->str))
			{
				this->_hawk = HAWK_NULL;
				return -1;
			}
		}

		this->ptr = this->str;
	}
	else
	{
		// open an included file
		const hawk_ooch_t* ioname;
		hawk_ooch_t* xpath;

		ioname = io.getName();
		HAWK_ASSERT(ioname != HAWK_NULL);

		if (io.getPrevHandle())
		{
			const hawk_ooch_t* outer, * path;
			hawk_ooch_t fbuf[64];
			hawk_ooch_t* dbuf = HAWK_NULL;

			path = ioname;
			outer = hawk_sio_getpath((hawk_sio_t*)io.getPrevHandle());
			if (outer)
			{
				const hawk_ooch_t* base;

				base = hawk_get_base_name_oocstr(outer);
				if (base != outer && ioname[0] != HAWK_T('/'))
				{
					hawk_oow_t tmplen, totlen, dirlen;

					dirlen = base - outer;
					totlen = hawk_count_oocstr(ioname) + dirlen;
					if (totlen >= HAWK_COUNTOF(fbuf))
					{
						dbuf = (hawk_ooch_t*)hawk_allocmem((hawk_t*)io, HAWK_SIZEOF(hawk_ooch_t) * (totlen + 1));
						if (HAWK_UNLIKELY(!dbuf)) return -1;
						path = dbuf;
					}
					else path = fbuf;

					tmplen = hawk_copy_oochars_to_oocstr_unlimited((hawk_ooch_t*)path, outer, dirlen);
					hawk_copy_oocstr_unlimited ((hawk_ooch_t*)path + tmplen, ioname);
				}
			}
			xpath = hawk_addsionamewithoochars((hawk_t*)io, path, hawk_count_oocstr(path));
			if (dbuf) hawk_freemem((hawk_t*)io, dbuf);
		}
		else
		{
			xpath = hawk_addsionamewithoochars((hawk_t*)io, ioname, hawk_count_oocstr(ioname));
		}
		if (HAWK_UNLIKELY(!xpath)) return -1;

		sio = open_sio(
			io, HAWK_NULL, xpath,
			(io.getMode() == READ?
				(HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH):
				(HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR))
		);
		if (!sio) return -1;

		io.setPath(xpath);
		io.setHandle(sio);
		if (this->cmgr) hawk_sio_setcmgr(sio, this->cmgr);
	}

	return 1;
}

int HawkStd::SourceString::close (Data& io)
{
	if (io.isMaster())
	{
		// free the resources and nullify this->_hawk in particular
		// to prevent this object from outliving the hawk instance pointed to
		// by this->_hawk.
		HAWK_ASSERT(this->_hawk != HAWK_NULL);
		HAWK_ASSERT(this->str != HAWK_NULL);
		hawk_freemem(this->_hawk, this->str);
		this->str = HAWK_NULL;
		this->ptr = HAWK_NULL;
		this->_hawk = HAWK_NULL;
	}
	else
	{
		hawk_sio_close((hawk_sio_t*)io.getHandle());
	}
	return 0;
}

hawk_ooi_t HawkStd::SourceString::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	if (io.isMaster())
	{
		hawk_oow_t n = 0;
		while (*this->ptr != '\0' && n < len) buf[n++] = *this->ptr++;
		return n;
	}
	else
	{
		return hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), buf, len);
	}
}

hawk_ooi_t HawkStd::SourceString::write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	if (io.isMaster())
	{
		return -1;
	}
	else
	{
		// in fact, this block will never be reached as
		// there is no included file concept for deparsing
		return hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), buf, len);
	}
}

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////

