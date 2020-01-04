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

#include <StdHawk.hpp>
#include "hawk-prv.h"
#include <stdlib.h>

#if !defined(HAWK_HAVE_CONFIG_H)
#	if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#		define HAVE_POW
#		define HAVE_FMOD
#	endif
#endif


// TODO: remove the following definitions and find a way to share the similar definitions in std.c 
#if defined(HAWK_ENABLE_LIBLTDL)
#	define USE_LTDL
#elif defined(HAVE_DLFCN_H)
#	define USE_DLFCN
#else
#	error UNSUPPORTED DYNAMIC LINKER
#endif


/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

StdHawk::ioattr_t StdHawk::default_ioattr;

static hawk_sio_t* open_sio (Hawk* awk, StdHawk::Run* run, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;

	//sio = hawk_sio_open ((run? ((Hawk::awk_t*)*(Hawk*)*run)->mmgr: awk->getMmgr()), 0, file, flags);
	sio = hawk_sio_open ((run? ((Hawk*)*run)->getMmgr(): awk->getMmgr()), 0, file, flags);
	if (sio == HAWK_NULL)
	{
		hawk_oocs_t ea;
		ea.ptr = (hawk_ooch_t*)file;
		ea.len = hawk_count_oocstr (file);
		if (run) run->setError (HAWK_EOPEN, &ea);
		else awk->setError (HAWK_EOPEN, &ea);
	}
	return sio;
}

static hawk_sio_t* open_sio_std (Hawk* awk, StdHawk::Run* run, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	static const hawk_ooch_t* std_names[] =
	{
		HAWK_T("stdin"),
		HAWK_T("stdout"),
		HAWK_T("stderr"),
	};

	//sio = hawk_sio_openstd ((run? ((Hawk::awk_t*)*(Hawk*)*run)->mmgr: awk->getMmgr()), 0, std, flags);
	sio = hawk_sio_openstd ((run? ((Hawk*)*run)->getMmgr(): awk->getMmgr()), 0, std, flags);
	if (sio == HAWK_NULL)
	{
		hawk_oocs_t ea;
		ea.ptr = (hawk_ooch_t*)std_names[std];
		ea.len = hawk_count_oocstr (std_names[std]);
		if (run) run->setError (HAWK_EOPEN, &ea);
		else awk->setError (HAWK_EOPEN, &ea);
	}
	return sio;
}

int StdHawk::open () 
{
	int n = Hawk::open ();
	if (n == -1) return n;

	this->gbl_argc = addGlobal (HAWK_T("ARGC"));
	this->gbl_argv = addGlobal (HAWK_T("ARGV"));
	this->gbl_environ = addGlobal (HAWK_T("ENVIRON"));
	if (this->gbl_argc <= -1 ||
	    this->gbl_argv <= -1 ||
	    this->gbl_environ <= -1)
	{
		goto oops;
	}

	if (addFunction (HAWK_T("rand"),       1, 0, HAWK_T("math"),   HAWK_NULL,                            0) <= -1 ||
	    addFunction (HAWK_T("srand"),      1, 0, HAWK_T("math"),   HAWK_NULL,                            0) <= -1 ||
	    addFunction (HAWK_T("system"),     1, 0, HAWK_T("sys"),    HAWK_NULL,                            0) <= -1 ||
	    addFunction (HAWK_T("setioattr"),  3, 3, HAWK_NULL,        (FunctionHandler)&StdHawk::setioattr, HAWK_RIO) <= -1 ||
	    addFunction (HAWK_T("getioattr"),  3, 3, HAWK_T("vvr"),    (FunctionHandler)&StdHawk::getioattr, HAWK_RIO) <= -1)
	{
		goto oops;
	}

	if (!this->stdmod_up)
	{
	#if defined(USE_DLFCN)
		if (hawk_setopt(awk, HAWK_MODPOSTFIX, HAWK_T(".so")) <= -1) goto oops;
	#endif

		if (hawk_stdmodstartup (this->awk) <= -1) goto oops;
		this->stdmod_up = true;
	}

	this->cmgrtab_inited = false;
	return 0;

oops:
	Hawk::close ();
	return -1;
}

void StdHawk::close () 
{
	if (this->cmgrtab_inited) 
	{
		hawk_htb_fini (&this->cmgrtab);
		this->cmgrtab_inited = false;
	}

	clearConsoleOutputs ();

	//
	// StdHawk called hawk_stdmodstartup() after Hawk::open().
	// It's logical to call hawk_stdmodshutdown() Hawk::close().
	// but Hawk::close() still needs to call some module's fini and
	// unload functions. So it must be done in StdHawk::uponClosing()
	// which is called after modules have been unloaded but while
	// the underlying awk object is still alive. 
	//
	// See StdHawk::uponClosing() below.
	//
	//if (this->stdmod_up)
	//{
	//	hawk_stdmodshutdown (this->awk);
	//	this->stdmod_up = false;
	//}
	//

	Hawk::close ();
}

void StdHawk::uponClosing ()
{
	if (this->stdmod_up)
	{
		hawk_stdmodshutdown (this->awk);
		this->stdmod_up = false;
	}

	// chain up
	Hawk::uponClosing ();
}

StdHawk::Run* StdHawk::parse (Source& in, Source& out)
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
		if (hawk_htb_init(&this->cmgrtab, this->getMmgr(), 256, 70, HAWK_SIZEOF(hawk_ooch_t), 1) <= -1)
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

int StdHawk::build_argcv (Run* run)
{
	Value argv (run);

	for (hawk_oow_t i = 0; i < this->runarg.len; i++)
	{
		if (argv.setIndexedStr (
			Value::IntIndex(i), 
			this->runarg.ptr[i].ptr, 
			this->runarg.ptr[i].len, true) <= -1) return -1;
	}

	run->setGlobal (this->gbl_argc, (int_t)this->runarg.len);
	run->setGlobal (this->gbl_argv, argv);
	return 0;
}

int StdHawk::__build_environ (Run* run, void* envptr)
{
	hawk_env_hawk_ooch_t** envarr = (hawk_env_hawk_ooch_t**)envptr;
	Value v_env (run);

	if (envarr)
	{
		hawk_env_hawk_ooch_t* eq;
		hawk_ooch_t* kptr, * vptr;
		hawk_oow_t klen, count;
		hawk_mmgr_t* mmgr = ((Hawk*)*run)->getMmgr();
		hawk_cmgr_t* cmgr = ((Hawk*)*run)->getCmgr();

		for (count = 0; envarr[count]; count++)
		{
		#if ((defined(HAWK_ENV_CHAR_IS_MCHAR) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(HAWK_ENV_CHAR_IS_WCHAR) && defined(HAWK_OOCH_IS_UCH)))
			eq = hawk_strchr (envarr[count], HAWK_T('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			kptr = envarr[count];
			klen = eq - envarr[count];
			vptr = eq + 1;
		#elif defined(HAWK_ENV_CHAR_IS_MCHAR)
			eq = hawk_mbschr (envarr[count], HAWK_BT('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			*eq = HAWK_BT('\0');

			kptr = hawk_mbstowcsalldupwithcmgr(envarr[count], &klen, mmgr, cmgr);
			vptr = hawk_mbstowcsalldupwithcmgr(eq + 1, HAWK_NULL, mmgr, cmgr);
			if (kptr == HAWK_NULL || vptr == HAWK_NULL)
			{
				if (kptr) HAWK_MMGR_FREE (mmgr, kptr);
				if (vptr) HAWK_MMGR_FREE (mmgr, vptr);

				/* mbstowcsdup() may fail for invalid encoding.
				 * so setting the error code to ENOMEM may not
				 * be really accurate */
				this->setError (HAWK_ENOMEM);
				return -1;
			}

			*eq = HAWK_BT('=');
		#else
			eq = hawk_wcschr (envarr[count], HAWK_UT('='));
			if (eq == HAWK_NULL || eq == envarr[count]) continue;

			*eq = HAWK_UT('\0');

			kptr = hawk_wcstombsdupwithcmgr(envarr[count], &klen, mmgr, cmgr); 
			vptr = hawk_wcstombsdupwithcmgr(eq + 1, HAWK_NULL, mmgr, cmgr);
			if (kptr == HAWK_NULL || vptr == HAWK_NULL)
			{
				if (kptr) HAWK_MMGR_FREE (mmgr, kptr);
				if (vptr) HAWK_MMGR_FREE (mmgr, vptr);

				/* mbstowcsdup() may fail for invalid encoding.
				 * so setting the error code to ENOMEM may not
				 * be really accurate */
				this->setError (HAWK_ENOMEM);
				return -1;
			}

			*eq = HAWK_UT('=');
		#endif

			// numeric string
			v_env.setIndexedStr (Value::Index (kptr, klen), vptr, true);

		#if ((defined(HAWK_ENV_CHAR_IS_MCHAR) && defined(HAWK_OOCH_IS_BCH)) || \
		     (defined(HAWK_ENV_CHAR_IS_WCHAR) && defined(HAWK_OOCH_IS_UCH)))
				/* nothing to do */
		#else
			if (vptr) HAWK_MMGR_FREE (mmgr, vptr);
			if (kptr) HAWK_MMGR_FREE (mmgr, kptr);
		#endif
		}
	}

	return run->setGlobal (this->gbl_environ, v_env);
}

int StdHawk::build_environ (Run* run)
{
	hawk_env_t env;
	int xret;

	if (hawk_env_init (&env, ((Hawk*)*run)->getMmgr(), 1) <= -1)
	{
		this->setError (HAWK_ENOMEM);
		return -1;
	}

	xret = __build_environ (run, hawk_env_getarr(&env));

	hawk_env_fini (&env);
	return xret;
}

int StdHawk::make_additional_globals (Run* run)
{
	if (build_argcv (run) <= -1 ||
	    build_environ (run) <= -1) return -1;
	    
	return 0;
}

hawk_cmgr_t* StdHawk::getiocmgr (const hawk_ooch_t* ioname)
{
	HAWK_ASSERT (this->cmgrtab_inited == true);

#if defined(HAWK_OOCH_IS_UCH)
	ioattr_t* ioattr = get_ioattr(ioname, hawk_count_oocstr(ioname));
	if (ioattr) return ioattr->cmgr;
#endif
	return HAWK_NULL;
}

StdHawk::ioattr_t* StdHawk::get_ioattr (const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search (&this->cmgrtab, ptr, len);
	if (pair == HAWK_NULL) return HAWK_NULL;

	return (ioattr_t*)HAWK_HTB_VPTR(pair);
}

StdHawk::ioattr_t* StdHawk::find_or_make_ioattr (const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_htb_pair_t* pair;

	pair = hawk_htb_search (&this->cmgrtab, ptr, len);
	if (pair == HAWK_NULL)
	{
		pair = hawk_htb_insert (
			&this->cmgrtab, (void*)ptr, len, 
			(void*)&StdHawk::default_ioattr, 
			HAWK_SIZEOF(StdHawk::default_ioattr));
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
	if (hawk_strcasecmp (name, HAWK_T("rtimeout")) == 0) return 0;
	if (hawk_strcasecmp (name, HAWK_T("wtimeout")) == 0) return 1;
	if (hawk_strcasecmp (name, HAWK_T("ctimeout")) == 0) return 2;
	if (hawk_strcasecmp (name, HAWK_T("atimeout")) == 0) return 3;
	return -1;
}

int StdHawk::setioattr (
	Run& run, Value& ret, Value* args, hawk_oow_t nargs,
	const hawk_ooch_t* name, hawk_oow_t len)
{
	HAWK_ASSERT (this->cmgrtab_inited == true);
	hawk_oow_t l[3];
	const hawk_ooch_t* ptr[3];

	ptr[0] = args[0].toStr(&l[0]);
	ptr[1] = args[1].toStr(&l[1]);
	ptr[2] = args[2].toStr(&l[2]);

	if (hawk_strxchr (ptr[0], l[0], HAWK_T('\0')) ||
	    hawk_strxchr (ptr[1], l[1], HAWK_T('\0')) ||
	    hawk_strxchr (ptr[2], l[2], HAWK_T('\0')))
	{
		return ret.setInt ((int_t)-1);
	}
	
	int tmout;
	if ((tmout = timeout_code (ptr[1])) >= 0)
	{
		int_t lv;
		flt_t fv;
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
			ioattr->tmout[tmout].sec = (hawk_int_awk_t)fv;
			nsec = fv - ioattr->tmout[tmout].sec;
			ioattr->tmout[tmout].nsec = HAWK_SEC_TO_NSEC(nsec);
		}
		return ret.setInt ((int_t)0);
	}
#if defined(HAWK_OOCH_IS_UCH)
	else if (hawk_strcasecmp (ptr[1], HAWK_T("codepage")) == 0 ||
	         hawk_strcasecmp (ptr[1], HAWK_T("encoding")) == 0)
	{
		ioattr_t* ioattr;
		hawk_cmgr_t* cmgr;

		if (ptr[2][0] == HAWK_T('\0')) cmgr = HAWK_NULL;
		else
		{
			cmgr = hawk_findcmgr (ptr[2]);
			if (cmgr == HAWK_NULL) return ret.setInt ((int_t)-1);
		}
		
		ioattr = find_or_make_ioattr (ptr[0], l[0]);
		if (ioattr == HAWK_NULL) return -1;

		ioattr->cmgr = cmgr;
		hawk_copy_oocstr (ioattr->cmgr_name, HAWK_COUNTOF(ioattr->cmgr_name), ptr[2]);
		return 0;
	}
#endif
	else
	{
		// unknown attribute name
		return ret.setInt ((int_t)-1);
	}
}

int StdHawk::getioattr (
	Run& run, Value& ret, Value* args, hawk_oow_t nargs,
	const hawk_ooch_t* name, hawk_oow_t len)
{
	HAWK_ASSERT (this->cmgrtab_inited == true);
	hawk_oow_t l[2];
	const hawk_ooch_t* ptr[2];

	ptr[0] = args[0].toStr(&l[0]);
	ptr[1] = args[1].toStr(&l[1]);

	int xx = -1;

	/* ionames must not contains a null character */
	if (hawk_strxchr (ptr[0], l[0], HAWK_T('\0')) == HAWK_NULL &&
	    hawk_strxchr (ptr[1], l[1], HAWK_T('\0')) == HAWK_NULL)
	{
		ioattr_t* ioattr = get_ioattr (ptr[0], l[0]);
		if (ioattr == HAWK_NULL) ioattr = &StdHawk::default_ioattr;

		int tmout;
		if ((tmout = timeout_code(ptr[1])) >= 0)
		{
			if (ioattr->tmout[tmout].nsec == 0)
				xx = args[2].setInt ((int_t)ioattr->tmout[tmout].sec);
			else
				xx = args[2].setFlt ((hawk_flt_t)ioattr->tmout[tmout].sec + HAWK_NSEC_TO_SEC((hawk_flt_t)ioattr->tmout[tmout].nsec));
		}
	#if defined(HAWK_OOCH_IS_UCH)
		else if (hawk_strcasecmp (ptr[1], HAWK_T("codepage")) == 0 ||
		         hawk_strcasecmp (ptr[1], HAWK_T("encoding")) == 0)
		{
			xx = args[2].setStr (ioattr->cmgr_name);
		}
	#endif
	}

	// unknown attribute name or errors
	return ret.setInt ((int_t)xx);
}

int StdHawk::open_nwio (Pipe& io, int flags, void* nwad)
{
	hawk_nwio_tmout_t tmout_buf;
	hawk_nwio_tmout_t* tmout = HAWK_NULL;

	const hawk_ooch_t* name = io.getName();
	ioattr_t* ioattr = get_ioattr (name, hawk_count_oocstr(name));
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
	if (cmgr) hawk_nwio_setcmgr (handle, cmgr);
#endif

	io.setHandle ((void*)handle);
	io.setUflags (1);

	return 1;
}

int StdHawk::open_pio (Pipe& io) 
{ 
	Hawk::Pipe::Mode mode = io.getMode();
	hawk_pio_t* pio = HAWK_NULL;
	int flags = HAWK_PIO_TEXT | HAWK_PIO_SHELL | HAWK_PIO_IGNOREECERR;

	switch (mode)
	{
		case Hawk::Pipe::READ:
			/* TODO: should we specify ERRTOOUT? */
			flags |= HAWK_PIO_READOUT |
			         HAWK_PIO_ERRTOOUT;
			break;

		case Hawk::Pipe::WRITE:
			flags |= HAWK_PIO_WRITEIN;
			break;

		case Hawk::Pipe::RW:
			flags |= HAWK_PIO_READOUT |
			         HAWK_PIO_ERRTOOUT |
			         HAWK_PIO_WRITEIN;
			break;
	}

	pio = hawk_pio_open (
		this->getMmgr(),
		0, 
		io.getName(), 
		HAWK_NULL,
		flags
	);
	if (pio == HAWK_NULL) return -1;

#if defined(HAWK_OOCH_IS_UCH)
	hawk_cmgr_t* cmgr = this->getiocmgr(io.getName());
	if (cmgr) 
	{
		hawk_pio_setcmgr (pio, HAWK_PIO_IN, cmgr);
		hawk_pio_setcmgr (pio, HAWK_PIO_OUT, cmgr);
		hawk_pio_setcmgr (pio, HAWK_PIO_ERR, cmgr);
	}
#endif
	io.setHandle (pio);
	io.setUflags (0);
	return 1;
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

int StdHawk::openPipe (Pipe& io) 
{ 
	int flags;
	hawk_nwad_t nwad;

	if (io.getMode() != Hawk::Pipe::RW ||
	    parse_rwpipe_uri (io.getName(), &flags, &nwad) <= -1)
	{
		return open_pio (io);
	}
	else
	{
		return open_nwio (io, flags, &nwad);
	}
}

int StdHawk::closePipe (Pipe& io) 
{
	if (io.getUflags() > 0)
	{
		/* nwio can't honor partical close */
		hawk_nwio_close ((hawk_nwio_t*)io.getHandle());
	}
	else
	{
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
	}
	return 0; 
}

hawk_ooi_t StdHawk::readPipe (Pipe& io, hawk_ooch_t* buf, hawk_oow_t len) 
{ 
	return (io.getUflags() > 0)?
		hawk_nwio_read ((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_read ((hawk_pio_t*)io.getHandle(), HAWK_PIO_OUT, buf, len);
}

hawk_ooi_t StdHawk::writePipe (Pipe& io, const hawk_ooch_t* buf, hawk_oow_t len) 
{ 
	return (io.getUflags() > 0)?
		hawk_nwio_write((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_write((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN, buf, len);
}

hawk_ooi_t StdHawk::writePipeBytes (Pipe& io, const hawk_bch_t* buf, hawk_oow_t len) 
{ 
	return (io.getUflags() > 0)?
		hawk_nwio_writebytes((hawk_nwio_t*)io.getHandle(), buf, len):
		hawk_pio_writebytes((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN, buf, len);
}

int StdHawk::flushPipe (Pipe& io) 
{ 
	return (io.getUflags() > 0)?
		hawk_nwio_flush ((hawk_nwio_t*)io.getHandle()):
		hawk_pio_flush ((hawk_pio_t*)io.getHandle(), HAWK_PIO_IN);
}

int StdHawk::openFile (File& io) 
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
			flags |= HAWK_SIO_WRITE | 
			         HAWK_SIO_CREATE | 
			         HAWK_SIO_TRUNCATE;
			break;
		case Hawk::File::APPEND:
			flags |= HAWK_SIO_APPEND |
			         HAWK_SIO_CREATE;
			break;
	}

	sio = hawk_sio_open(this->getMmgr(), 0, io.getName(), flags);
	if (!sio) return -1;
#if defined(HAWK_OOCH_IS_UCH)
	hawk_cmgr_t* cmgr = this->getiocmgr(io.getName());
	if (cmgr) hawk_sio_setcmgr (sio, cmgr);
#endif

	io.setHandle (sio);
	return 1;
}

int StdHawk::closeFile (File& io) 
{ 
	hawk_sio_close ((hawk_sio_t*)io.getHandle());
	return 0; 
}

hawk_ooi_t StdHawk::readFile (File& io, hawk_ooch_t* buf, hawk_oow_t len) 
{
	return hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t StdHawk::writeFile (File& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t StdHawk::writeFileBytes (File& io, const hawk_bch_t* buf, hawk_oow_t len)
{
	return hawk_sio_putbchars((hawk_sio_t*)io.getHandle(), buf, len);
}


int StdHawk::flushFile (File& io) 
{ 
	return hawk_sio_flush((hawk_sio_t*)io.getHandle());
}

void StdHawk::setConsoleCmgr (const hawk_cmgr_t* cmgr)
{
	this->console_cmgr = (hawk_cmgr_t*)cmgr;	
}

const hawk_cmgr_t* StdHawk::getConsoleCmgr () const
{
	return this->console_cmgr;
}

int StdHawk::addConsoleOutput (const hawk_ooch_t* arg, hawk_oow_t len) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = this->ofile.add (awk, arg, len);
	if (n <= -1) setError (HAWK_ENOMEM);
	return n;
}

int StdHawk::addConsoleOutput (const hawk_ooch_t* arg) 
{
	return addConsoleOutput (arg, hawk_count_oocstr(arg));
}

void StdHawk::clearConsoleOutputs () 
{
	this->ofile.clear (awk);
}

int StdHawk::open_console_in (Console& io) 
{ 
	hawk_rtx_t* rtx = (rtx_t*)io;

	if (this->runarg.ptr == HAWK_NULL) 
	{
		HAWK_ASSERT (this->runarg.len == 0 && this->runarg.capa == 0);

		if (this->runarg_count == 0) 
		{
			hawk_sio_t* sio;

			sio = open_sio_std (
				HAWK_NULL, io, HAWK_SIO_STDIN,
				HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			if (sio == HAWK_NULL) return -1;

			if (this->console_cmgr) 
				hawk_sio_setcmgr (sio, this->console_cmgr);

			io.setHandle (sio);
			this->runarg_count++;
			return 1;
		}

		return 0;
	}
	else
	{
		hawk_sio_t* sio;
		const hawk_ooch_t* file;
		hawk_val_t* argv;
		hawk_htb_t* map;
		hawk_htb_pair_t* pair;
		hawk_ooch_t ibuf[128];
		hawk_oow_t ibuflen;
		hawk_val_t* v;
		hawk_oocs_t as;

	nextfile:
		file = this->runarg.ptr[this->runarg_index].ptr;

		if (file == HAWK_NULL)
		{
			/* no more input file */

			if (this->runarg_count == 0)
			{
				/* all ARGVs are empty strings. 
				 * so no console files were opened.
				 * open the standard input here.
				 *
				 * 'BEGIN { ARGV[1]=""; ARGV[2]=""; }
				 *        { print $0; }' file1 file2
				 */
				sio = open_sio_std (
					HAWK_NULL, io, HAWK_SIO_STDIN,
					HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
				if (sio == HAWK_NULL) return -1;

				if (this->console_cmgr)
					hawk_sio_setcmgr (sio, this->console_cmgr);

				io.setHandle (sio);
				this->runarg_count++;
				return 1;
			}

			return 0;
		}

		if (hawk_count_oocstr(file) != this->runarg.ptr[this->runarg_index].len)
		{
			hawk_oocs_t arg;
			arg.ptr = (hawk_ooch_t*)file;
			arg.len = hawk_count_oocstr (arg.ptr);
			((Run*)io)->setError (HAWK_EIONMNL, &arg);
			return -1;
		}

		/* handle special case when ARGV[x] has been altered.
		 * so from here down, the file name gotten from 
		 * rxtn->c.in.files is not important and is overridden 
		 * from ARGV.
		 * 'BEGIN { ARGV[1]="file3"; } 
		 *        { print $0; }' file1 file2
		 */
		argv = hawk_rtx_getgbl (rtx, this->gbl_argv);
		HAWK_ASSERT (argv != HAWK_NULL);
		HAWK_ASSERT (HAWK_RTX_GETVALTYPE (rtx, argv) == HAWK_VAL_MAP);

		map = ((hawk_val_map_t*)argv)->map;
		HAWK_ASSERT (map != HAWK_NULL);
		
		// ok to find ARGV[this->runarg_index] as ARGV[0]
		// has been skipped.
		ibuflen = hawk_int_to_oocstr (this->runarg_index, 10, HAWK_NULL, ibuf, HAWK_COUNTOF(ibuf));

		pair = hawk_htb_search (map, ibuf, ibuflen);
		HAWK_ASSERT (pair != HAWK_NULL);

		v = (hawk_val_t*)HAWK_HTB_VPTR(pair);
		HAWK_ASSERT (v != HAWK_NULL);

		as.ptr = hawk_rtx_getvaloocstr (rtx, v, &as.len);
		if (as.ptr == HAWK_NULL) return -1;

		if (as.len == 0)
		{
			/* the name is empty */
			hawk_rtx_freevaloocstr (rtx, v, as.ptr);
			this->runarg_index++;
			goto nextfile;
		}

		if (hawk_count_oocstr(as.ptr) < as.len)
		{
			/* the name contains one or more '\0' */
			hawk_oocs_t arg;
			arg.ptr = as.ptr;
			arg.len = hawk_count_oocstr (as.ptr);
			((Run*)io)->setError (HAWK_EIONMNL, &arg);
			hawk_rtx_freevaloocstr (rtx, v, as.ptr);
			return -1;
		}

		file = as.ptr;

		if (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))
			sio = open_sio_std(HAWK_NULL, io, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
		else
			sio = open_sio(HAWK_NULL, io, file, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
		if (sio == HAWK_NULL) 
		{
			hawk_rtx_freevaloocstr (rtx, v, as.ptr);
			return -1;
		}
		
		if (hawk_rtx_setfilename (rtx, file, hawk_count_oocstr(file)) <= -1)
		{
			hawk_sio_close (sio);
			hawk_rtx_freevaloocstr (rtx, v, as.ptr);
			return -1;
		}

		hawk_rtx_freevaloocstr (rtx, v, as.ptr);

		if (this->console_cmgr) hawk_sio_setcmgr (sio, this->console_cmgr);

		io.setHandle (sio);

		/* increment the counter of files successfully opened */
		this->runarg_count++;
		this->runarg_index++;
		return 1;
	}

}

int StdHawk::open_console_out (Console& io) 
{
	hawk_rtx_t* rtx = (rtx_t*)io;

	if (this->ofile.ptr == HAWK_NULL)
	{
		HAWK_ASSERT (this->ofile.len == 0 && this->ofile.capa == 0);

		if (this->ofile_count == 0) 
		{
			hawk_sio_t* sio;
			sio = open_sio_std (
				HAWK_NULL, io, HAWK_SIO_STDOUT,
				HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			if (sio == HAWK_NULL) return -1;

			if (this->console_cmgr)
				hawk_sio_setcmgr (sio, this->console_cmgr);

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
			hawk_oocs_t arg;
			arg.ptr = (hawk_ooch_t*)file;
			arg.len = hawk_count_oocstr (arg.ptr);
			((Run*)io)->setError (HAWK_EIONMNL, &arg);
			return -1;
		}

		if (file[0] == HAWK_T('-') && file[1] == HAWK_T('\0'))
			sio = open_sio_std (HAWK_NULL, io, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
		else
			sio = open_sio (HAWK_NULL, io, file, HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR);
		if (sio == HAWK_NULL) return -1;
		
		if (hawk_rtx_setofilename (
			rtx, file, hawk_count_oocstr(file)) == -1)
		{
			hawk_sio_close (sio);
			return -1;
		}

		if (this->console_cmgr) 
			hawk_sio_setcmgr (sio, this->console_cmgr);
		io.setHandle (sio);

		this->ofile_index++;
		this->ofile_count++;
		return 1;
	}
}

int StdHawk::openConsole (Console& io) 
{
	Console::Mode mode = io.getMode();

	if (mode == Console::READ)
	{
		this->runarg_count = 0;
		this->runarg_index = 0;
		if (this->runarg.len > 0) 
		{
			// skip ARGV[0]
			this->runarg_index++;
		}
		return open_console_in (io);
	}
	else
	{
		HAWK_ASSERT (mode == Console::WRITE);

		this->ofile_count = 0;
		this->ofile_index = 0;
		return open_console_out (io);
	}
}

int StdHawk::closeConsole (Console& io) 
{ 
	hawk_sio_close ((hawk_sio_t*)io.getHandle());
	return 0;
}

hawk_ooi_t StdHawk::readConsole (Console& io, hawk_ooch_t* data, hawk_oow_t size) 
{
	hawk_ooi_t nn;

	while ((nn = hawk_sio_getoochars((hawk_sio_t*)io.getHandle(),data,size)) == 0)
	{
		int n;
		hawk_sio_t* sio = (hawk_sio_t*)io.getHandle();

		n = open_console_in (io);
		if (n == -1) return -1;

		if (n == 0) 
		{
			/* no more input console */
			return 0;
		}

		if (sio) hawk_sio_close (sio);
	}

	return nn;
}

hawk_ooi_t StdHawk::writeConsole (Console& io, const hawk_ooch_t* data, hawk_oow_t size) 
{
	return hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), data, size);
}

hawk_ooi_t StdHawk::writeConsoleBytes (Console& io, const hawk_bch_t* data, hawk_oow_t size) 
{
	return hawk_sio_putbchars((hawk_sio_t*)io.getHandle(), data, size);
}

int StdHawk::flushConsole (Console& io) 
{ 
	return hawk_sio_flush ((hawk_sio_t*)io.getHandle());
}

int StdHawk::nextConsole (Console& io) 
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

	if (sio) hawk_sio_close (sio);
	return n;
}

// memory allocation primitives
void* StdHawk::allocMem (hawk_oow_t n) 
{ 
	return ::malloc (n); 
}

void* StdHawk::reallocMem (void* ptr, hawk_oow_t n) 
{ 
	return ::realloc (ptr, n); 
}

void  StdHawk::freeMem (void* ptr) 
{ 
	::free (ptr); 
}

// miscellaneous primitive

StdHawk::flt_t StdHawk::pow (flt_t x, flt_t y) 
{ 
	return hawk_stdmathpow (this->awk, x, y);
}

StdHawk::flt_t StdHawk::mod (flt_t x, flt_t y) 
{ 
	return hawk_stdmathmod (this->awk, x, y);
}

void* StdHawk::modopen (const mod_spec_t* spec)
{
	void* h;
	h = hawk_stdmodopen (this->awk, spec);
	if (!h) this->retrieveError ();
	return h;
}

void StdHawk::modclose (void* handle)
{
	hawk_stdmodclose (this->awk, handle);
}

void* StdHawk::modgetsym (void* handle, const hawk_ooch_t* name)
{
	void* s;
	s = hawk_stdmodgetsym (this->awk, handle, name);
	if (!s) this->retrieveError ();
	return s;
}

int StdHawk::SourceFile::open (Data& io)
{
	hawk_sio_t* sio;

	if (io.isMaster())
	{
		// open the main source file.

		if (this->name[0] == HAWK_T('-') && this->name[1] == HAWK_T('\0'))
		{
			if (io.getMode() == READ)
				sio = open_sio_std (
					io, HAWK_NULL, HAWK_SIO_STDIN, 
					HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			else
				sio = open_sio_std (
					io, HAWK_NULL, HAWK_SIO_STDOUT, 
					HAWK_SIO_WRITE | HAWK_SIO_CREATE | 
					HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			if (sio == HAWK_NULL) return -1;
		}
		else
		{
			sio = open_sio (
				io, HAWK_NULL, this->name,
				(io.getMode() == READ? 
					(HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH): 
					(HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR))
			);
			if (sio == HAWK_NULL) return -1;
		}

		if (this->cmgr) hawk_sio_setcmgr (sio, this->cmgr);
		io.setName (this->name);
	}
	else
	{
		// open an included file
		const hawk_ooch_t* ioname, * file;
		hawk_ooch_t fbuf[64];
		hawk_ooch_t* dbuf = HAWK_NULL;
	
		ioname = io.getName();
		HAWK_ASSERT (ioname != HAWK_NULL);

		file = ioname;
		if (io.getPrevHandle())
		{
			const hawk_ooch_t* outer;

			outer = hawk_sio_getpath ((hawk_sio_t*)io.getPrevHandle());
			if (outer)
			{
				const hawk_ooch_t* base;

				base = hawk_basename(outer);
				if (base != outer && ioname[0] != HAWK_T('/'))
				{
					hawk_oow_t tmplen, totlen, dirlen;
				
					dirlen = base - outer;
					totlen = hawk_count_oocstr(ioname) + dirlen;
					if (totlen >= HAWK_COUNTOF(fbuf))
					{
						dbuf = (hawk_ooch_t*) HAWK_MMGR_ALLOC (
							((Hawk*)io)->getMmgr(),
							HAWK_SIZEOF(hawk_ooch_t) * (totlen + 1)
						);
						if (dbuf == HAWK_NULL)
						{
							((Hawk*)io)->setError (HAWK_ENOMEM);
							return -1;
						}

						file = dbuf;
					}
					else file = fbuf;

					tmplen = hawk_copy_oochars_to_oocstr_unlimited ((hawk_ooch_t*)file, outer, dirlen);
					hawk_copy_oocstr_unlimited ((hawk_ooch_t*)file + tmplen, ioname);
				}
			}
		}

		sio = open_sio (
			io, HAWK_NULL, file,
			(io.getMode() == READ? 
				(HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH): 
				(HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR))
		);
		if (dbuf) HAWK_MMGR_FREE (((Hawk*)io)->getMmgr(), dbuf);
		if (sio == HAWK_NULL) return -1;
		if (this->cmgr) hawk_sio_setcmgr (sio, this->cmgr);
	}

	io.setHandle (sio);
	return 1;
}

int StdHawk::SourceFile::close (Data& io)
{
	hawk_sio_t* sio = (hawk_sio_t*)io.getHandle();
	hawk_sio_flush (sio);
	hawk_sio_close (sio);
	return 0;
}

hawk_ooi_t StdHawk::SourceFile::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_getoochars ((hawk_sio_t*)io.getHandle(), buf, len);
}

hawk_ooi_t StdHawk::SourceFile::write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	return hawk_sio_putoochars ((hawk_sio_t*)io.getHandle(), buf, len);
}

int StdHawk::SourceString::open (Data& io)
{
	hawk_sio_t* sio;

	if (io.getName() == HAWK_NULL)
	{
		// open the main source file.
		// SourceString does not support writing.
		if (io.getMode() == WRITE) return -1;
		ptr = str;
	}
	else
	{
		// open an included file 

		const hawk_ooch_t* ioname, * file;
		hawk_ooch_t fbuf[64];
		hawk_ooch_t* dbuf = HAWK_NULL;
		
		ioname = io.getName();
		HAWK_ASSERT (ioname != HAWK_NULL);

		file = ioname;
		if (io.getPrevHandle())
		{
			const hawk_ooch_t* outer;

			outer = hawk_sio_getpath ((hawk_sio_t*)io.getPrevHandle());
			if (outer)
			{
				const hawk_ooch_t* base;
	
				base = hawk_basename(outer);
				if (base != outer && ioname[0] != HAWK_T('/'))
				{
					hawk_oow_t tmplen, totlen, dirlen;
				
					dirlen = base - outer;
					totlen = hawk_count_oocstr(ioname) + dirlen;
					if (totlen >= HAWK_COUNTOF(fbuf))
					{
						dbuf = (hawk_ooch_t*)HAWK_MMGR_ALLOC(
							((Hawk*)io)->getMmgr(),
							HAWK_SIZEOF(hawk_ooch_t) * (totlen + 1)
						);
						if (dbuf == HAWK_NULL)
						{
							((Hawk*)io)->setError (HAWK_ENOMEM);
							return -1;
						}
	
						file = dbuf;
					}
					else file = fbuf;
	
					tmplen = hawk_copy_oochars_to_oocstr_unlimited ((hawk_ooch_t*)file, outer, dirlen);
					hawk_copy_oocstr_unlimited ((hawk_ooch_t*)file + tmplen, ioname);
				}
			}
		}

		sio = open_sio (
			io, HAWK_NULL, file,
			(io.getMode() == READ? 
				(HAWK_SIO_READ | HAWK_SIO_IGNOREECERR | HAWK_SIO_KEEPPATH): 
				(HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR))
		);
		if (dbuf) HAWK_MMGR_FREE (((Hawk*)io)->getMmgr(), dbuf);
		if (sio == HAWK_NULL) return -1;

		io.setHandle (sio);
	}

	return 1;
}

int StdHawk::SourceString::close (Data& io)
{
	if (!io.isMaster()) hawk_sio_close ((hawk_sio_t*)io.getHandle());
	return 0;
}

hawk_ooi_t StdHawk::SourceString::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	if (io.isMaster())
	{
		hawk_oow_t n = 0;
		while (*ptr != HAWK_T('\0') && n < len) buf[n++] = *ptr++;
		return n;
	}
	else
	{
		return hawk_sio_getoochars ((hawk_sio_t*)io.getHandle(), buf, len);
	}
}

hawk_ooi_t StdHawk::SourceString::write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	if (io.isMaster())
	{
		return -1;
	}
	else
	{
		// in fact, this block will never be reached as
		// there is no included file concept for deparsing 
		return hawk_sio_putoochars ((hawk_sio_t*)io.getHandle(), buf, len);
	}
}

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////

