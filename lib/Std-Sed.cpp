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

#include <Hawk-Sed.hpp>
#include <hawk-fio.h>
#include <hawk-sio.h>
#include "sed-prv.h"

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

static hawk_sio_t* open_sio (SedStd::Stream::Data& io, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;

	sio = hawk_sio_open((hawk_gem_t*)io, 0, file, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* old_errmsg = hawk_sed_backuperrmsg((hawk_sed_t*)io);
		((Sed*)io)->formatError(HAWK_SED_EIOFIL, HAWK_NULL, "unable to open %js - %js", file, old_errmsg);
	}
	return sio;
}

static hawk_sio_t* open_sio_std (SedStd::Stream::Data& io, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	static const hawk_ooch_t* std_names[] =
	{
		HAWK_T("stdin"),
		HAWK_T("stdout"),
		HAWK_T("stderr"),
	};

	sio = hawk_sio_openstd((hawk_gem_t*)io, 0, std, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* old_errmsg = hawk_sed_backuperrmsg((hawk_sed_t*)io);
		((Sed*)io)->formatError(HAWK_SED_EIOFIL, HAWK_NULL, "unable to open %js - %js", std_names[std], old_errmsg);
	}
	return sio;
}

SedStd::FileStream::FileStream (const hawk_uch_t* file, hawk_cmgr_t* cmgr):
	_type(NAME_UCH), _file(file), file(HAWK_NULL), cmgr(cmgr)
{
}

SedStd::FileStream::FileStream (const hawk_bch_t* file, hawk_cmgr_t* cmgr ):
	_type(NAME_BCH), _file(file), file(HAWK_NULL), cmgr(cmgr)
{
}
SedStd::FileStream::~FileStream ()
{
	HAWK_ASSERT (this->file == HAWK_NULL);
}

int SedStd::FileStream::open (Data& io)
{
	hawk_sio_t* sio;
	const hawk_ooch_t* ioname = io.getName();
	hawk_sio_std_t std_sio;
	int oflags;
	hawk_gem_t* gem = (hawk_gem_t*)io;

	if (io.getMode() == READ)
	{
		oflags = HAWK_SIO_READ | HAWK_SIO_IGNOREECERR;
		std_sio = HAWK_SIO_STDIN;
	}
	else
	{
		oflags = HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK;
		std_sio = HAWK_SIO_STDOUT;
	}

	if (ioname == HAWK_NULL)
	{
		//
		// a normal console is indicated by a null name or a dash
		//

		if (this->_file)
		{
			if (this->_type == NAME_UCH)
			{
				const hawk_uch_t* tmp = (const hawk_uch_t*)this->_file;
				if (tmp[0] == '-' || tmp[1] == '\0') goto sio_stdio;
			#if defined(HAWK_OOCH_IS_UCH)
				this->file = hawk_gem_dupucstr(gem, tmp, HAWK_NULL);
			#else
				this->file = hawk_gem_duputobcstr(gem, tmp, HAWK_NULL);
			#endif
			}
			else
			{
				const hawk_bch_t* tmp = (const hawk_bch_t*)this->_file;
				if (tmp[0] == '-' || tmp[1] == '\0') goto sio_stdio;
			#if defined(HAWK_OOCH_IS_UCH)
				this->file = hawk_gem_dupbtoucstr(gem, tmp, HAWK_NULL, 0);
			#else
				this->file = hawk_gem_dupbcstr(gem, tmp, HAWK_NULL);
			#endif
			}
			if (!this->file) return -1;
			sio = open_sio(io, this->file, oflags);
			if (!sio) hawk_gem_freemem (gem, this->file);
		}
		else
		{
		sio_stdio:
			sio = open_sio_std(io, std_sio, oflags);
		}
	}
	else
	{
		//
		// if ioname is not empty, it is a file name
		//
		sio = open_sio(io, ioname, oflags);
	}
	if (sio == HAWK_NULL) return -1;

	if (this->cmgr) hawk_sio_setcmgr (sio, this->cmgr);
	io.setHandle (sio);
	return 1;
}

int SedStd::FileStream::close (Data& io)
{
	hawk_sio_close ((hawk_sio_t*)io.getHandle());

	// this stream object may get called more than once and is merely a proxy
	// object that has its own lifespan. while io.getHandle() returns a unique
	// handle value as set by io.setHandle() in the open method, this object
	// is resued for calls over multiple I/O objects created. When releasing
	// extra resources stored in the object, we must take extra care to know
	// the context of this close method.

	if (!io.getName())
	{
		// for a master stream that's not started by r or w
		if (this->file)
		{
			hawk_gem_freemem ((hawk_gem_t*)io, this->file);
			this->file = HAWK_NULL;
		}
	}

	return 0;
}

hawk_ooi_t SedStd::FileStream::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	hawk_ooi_t n = hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), buf, len);

	if (n <= -1)
	{
		if (!io.getName() && this->file)  // io.getMode() must be READ
		{
			const hawk_ooch_t* old_errmsg = hawk_sed_backuperrmsg((hawk_sed_t*)io);
			((Sed*)io)->formatError(HAWK_SED_EIOFIL, HAWK_NULL, "unable to read %js - %js", this->file, old_errmsg);
		}
	}

	return n;
}

hawk_ooi_t SedStd::FileStream::write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	hawk_ooi_t n = hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), buf, len);

	if (n <= -1)
	{
		if (!io.getName() && this->file) // io.getMode() must be WRITE
		{
			const hawk_ooch_t* old_errmsg = hawk_sed_backuperrmsg((hawk_sed_t*)io);
			((Sed*)io)->formatError(HAWK_SED_EIOFIL, HAWK_NULL, "unable to read %js - %js", this->file, old_errmsg);
		}
	}

	return n;
}

SedStd::StringStream::StringStream (hawk_cmgr_t* cmgr): _type(STR_UCH) // this type isn't import for this
{
	this->cmgr = cmgr;

	this->in._sed = HAWK_NULL;
	this->in._str = HAWK_NULL;
	this->in._end = HAWK_NULL;
	this->in.str = HAWK_NULL;
	this->in.end = HAWK_NULL;
	this->in.ptr = HAWK_NULL;

	this->out._sed = HAWK_NULL;
	this->out.sed_ecb.next_ = HAWK_NULL;
	this->out.inited = false;
	this->out.alt_buf = HAWK_NULL;
	this->out.alt_sed = HAWK_NULL;
}

SedStd::StringStream::StringStream (const hawk_uch_t* in, hawk_cmgr_t* cmgr): _type(STR_UCH)
{
	this->cmgr = cmgr;

	this->in._sed = HAWK_NULL;
	this->in._str = in;
	this->in._end = in + hawk_count_ucstr(in);
	this->in.str = HAWK_NULL;
	this->in.end = HAWK_NULL;
	this->in.ptr = HAWK_NULL;

	this->out._sed = HAWK_NULL;
	this->out.sed_ecb.next_ = HAWK_NULL;
	this->out.inited = false;
	this->out.alt_buf = HAWK_NULL;
	this->out.alt_sed = HAWK_NULL;
}

SedStd::StringStream::StringStream (const hawk_uch_t* in, hawk_oow_t len, hawk_cmgr_t* cmgr): _type(STR_UCH)
{
	this->cmgr = cmgr;

	this->in._sed = HAWK_NULL;
	this->in._str = in;
	this->in._end = in + len;
	this->in.str = HAWK_NULL;
	this->in.end = HAWK_NULL;
	this->in.ptr = HAWK_NULL;

	this->out._sed = HAWK_NULL;
	this->out.sed_ecb.next_ = HAWK_NULL;
	this->out.inited = false;
	this->out.alt_buf = HAWK_NULL;
	this->out.alt_sed = HAWK_NULL;

}

SedStd::StringStream::StringStream (const hawk_bch_t* in, hawk_cmgr_t* cmgr): _type(STR_BCH)
{
	this->cmgr = cmgr;

	this->in._sed = HAWK_NULL;
	this->in._str = in;
	this->in._end = in + hawk_count_bcstr(in);
	this->in.str = HAWK_NULL;
	this->in.end = HAWK_NULL;
	this->in.ptr = HAWK_NULL;

	this->out._sed = HAWK_NULL;
	this->out.sed_ecb.next_ = HAWK_NULL;
	this->out.inited = false;
	this->out.alt_buf = HAWK_NULL;
	this->out.alt_sed = HAWK_NULL;
}

SedStd::StringStream::StringStream (const hawk_bch_t* in, hawk_oow_t len, hawk_cmgr_t* cmgr): _type(STR_BCH)
{
	this->cmgr = cmgr;

	this->in._sed = HAWK_NULL;
	this->in._str = in;
	this->in._end = in + len;
	this->in.str = HAWK_NULL;
	this->in.end = HAWK_NULL;
	this->in.ptr = HAWK_NULL;

	this->out._sed = HAWK_NULL;
	this->out.inited = false;
	this->out.alt_buf = HAWK_NULL;
	this->out.alt_sed = HAWK_NULL;

	this->out.sed_ecb.next_ = HAWK_NULL;
}

SedStd::StringStream::~StringStream ()
{
	HAWK_ASSERT (this->in._sed == HAWK_NULL);
	HAWK_ASSERT (this->in.str == HAWK_NULL);

	this->clearOutputData (true);
	HAWK_ASSERT (this->out._sed == HAWK_NULL);
	HAWK_ASSERT (this->out.inited == false);
}

int SedStd::StringStream::open (Data& io)
{
	const hawk_ooch_t* ioname = io.getName ();

	if (ioname == HAWK_NULL)
	{
		// open a main data stream
		if (io.getMode() == READ)
		{
			HAWK_ASSERT (this->in.str == HAWK_NULL);

			if (this->in._str == HAWK_NULL)
			{
				// no input data was passed to this object for construction

				if (this->out.inited)
				{
					// this object is being reused for input after output
					// use the output data as input
					hawk_oocs_t out;
					hawk_ooecs_yield (&this->out.buf, &out, 0);
					this->in.str = out.ptr;
					this->in.ptr = out.ptr;
					this->in.end = this->in.str + out.len;
					this->clearOutputData (true);
				}
				else
				{
					((Sed*)io)->formatError(HAWK_EINVAL, HAWK_NULL, "no input data available to open");
					return -1;
				}
			}
			else
			{
				hawk_oow_t len;
				hawk_gem_t* gem = hawk_sed_getgem((hawk_sed_t*)io);

				if (this->_type == STR_UCH)
				{
				#if defined(HAWK_OOCH_IS_UCH)
					this->in.str = hawk_gem_dupucstr(gem, (const hawk_uch_t*)this->in._str, &len);
				#else
					this->in.str = hawk_gem_duputobcstr(gem, (const hawk_uch_t*)this->in._str, &len);
				#endif
				}
				else
				{
					HAWK_ASSERT (this->_type == STR_BCH);
				#if defined(HAWK_OOCH_IS_UCH)
					this->in.str = hawk_gem_dupbtoucstr(gem, (const hawk_bch_t*)this->in._str, &len, 0);
				#else
					this->in.str = hawk_gem_dupbcstr(gem, (const hawk_bch_t*)this->in._str, &len);
				#endif
				}
				if (HAWK_UNLIKELY(!this->in.str)) return -1;

				this->in.end = this->in.str + len;
			}

			this->in.ptr = this->in.str;
			this->in._sed = (hawk_sed_t*)io;
		}
		else
		{
			this->clearOutputData (true);
			// preserving this previous output data is a bit tricky when hawk_ooecs_init() fails.
			// let's not try to preserve the old output data for now.
			// full clearing is needed because this object may get passed to different sed objects
			// in succession.

			if (hawk_ooecs_init(&this->out.buf, (hawk_gem_t*)io, 256) <= -1) return -1;
			this->out.inited = true;
			this->out._sed = (hawk_sed_t*)io;

			// only if it's not already pushed
			this->out.sed_ecb.close = this->on_sed_close;
			this->out.sed_ecb.ctx = this;
			hawk_sed_pushecb((hawk_sed_t*)io, &this->out.sed_ecb);
		}

		io.setHandle (this);
	}
	else
	{
		// open files for a r or w command
		hawk_sio_t* sio;
		int mode = (io.getMode() == READ)?
			HAWK_SIO_READ:
			(HAWK_SIO_WRITE|HAWK_SIO_CREATE|HAWK_SIO_TRUNCATE);

		sio = hawk_sio_open((hawk_gem_t*)io, 0, ioname, mode);
		if (sio == HAWK_NULL) return -1;

		io.setHandle (sio);
	}

	return 1;
}

int SedStd::StringStream::close (Data& io)
{
	const void* handle = io.getHandle();
	if (handle == this)
	{
		if (io.getMode() == READ)
		{
			this->clearInputData();
		}
		else if (io.getMode() == WRITE && this->out.inited)
		{
			// don't clear anyting here as some output fields are required
			// until this object is destroyed or the associated sed object is closed.
		}
	}
	else
	{
		hawk_sio_close ((hawk_sio_t*)handle);
	}

	return 0;
}

hawk_ooi_t SedStd::StringStream::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	const void* handle = io.getHandle();

	if (len == (hawk_oow_t)-1) len--; // shrink buffer if too long
	if (handle == this)
	{
		HAWK_ASSERT (this->in.str != HAWK_NULL);
		hawk_oow_t n = 0;
		while (this->in.ptr < this->in.end && n < len)
			buf[n++] = *this->in.ptr++;
		return (hawk_ooi_t)n;
	}
	else
	{
		return hawk_sio_getoochars((hawk_sio_t*)handle, buf, len);
	}
}

hawk_ooi_t SedStd::StringStream::write (Data& io, const hawk_ooch_t* data, hawk_oow_t len)
{
	const void* handle = io.getHandle();

	if (len == (hawk_oow_t)-1) len--; // shrink data if too long

	if (handle == this)
	{
		HAWK_ASSERT (this->out.inited != 0);
		if (hawk_ooecs_ncat(&this->out.buf, data, len) == (hawk_oow_t)-1) return -1;
		return len;
	}
	else
	{
		return hawk_sio_putoochars((hawk_sio_t*)handle, data, len);
	}
}

const hawk_ooch_t* SedStd::StringStream::getOutput (hawk_oow_t* len) const
{
	if (this->out.inited)
	{
		if (len) *len = HAWK_OOECS_LEN(&this->out.buf);
		return HAWK_OOECS_PTR(&this->out.buf);
	}
	else
	{
		if (len) *len = 0;
		return HAWK_T("");
	}
}

const hawk_uch_t* SedStd::StringStream::getOutputU (hawk_oow_t* len)
{
#if defined(HAWK_OOCH_IS_UCH)
	return this->getOutput(len);
#else
	if (this->out.inited)
	{
		hawk_uch_t* tmp = hawk_gem_dupbtoucharswithcmgr(hawk_sed_getgem(this->out._sed), HAWK_OOECS_PTR(&this->out.buf), HAWK_OOECS_LEN(&this->out.buf), len, this->cmgr, 1);
		if (tmp)
		{
			if (this->out.alt_buf) hawk_sed_freemem(this->out._sed, this->out.alt_buf);
			this->out.alt_buf = (void*)tmp;
			this->out.alt_sed = this->out._sed;
		}
		return tmp;
	}
	else
	{
		if (len) *len = 0;
		return HAWK_UT("");
	}
#endif
}

const hawk_bch_t* SedStd::StringStream::getOutputB (hawk_oow_t* len)
{
#if defined(HAWK_OOCH_IS_UCH)
	if (this->out.inited)
	{
		hawk_bch_t* tmp = hawk_gem_duputobcharswithcmgr(hawk_sed_getgem(this->out._sed), HAWK_OOECS_PTR(&this->out.buf), HAWK_OOECS_LEN(&this->out.buf), len, this->cmgr);
		if (tmp)
		{
			if (this->out.alt_buf) hawk_sed_freemem(this->out._sed, this->out.alt_buf);
			this->out.alt_buf = (void*)tmp;
			this->out.alt_sed = this->out._sed;
		}
		return tmp;
	}
	else
	{
		if (len) *len = 0;
		return HAWK_BT("");
	}
#else
	return this->getOutput(len);
#endif
}

void SedStd::StringStream::clearInputData ()
{
	if (this->in.str)
	{
		hawk_sed_freemem (this->in._sed, this->in.str);
		this->in.str = HAWK_NULL;
		this->in.end = HAWK_NULL;
		this->in.ptr = HAWK_NULL;
		this->in._sed = HAWK_NULL;
	}
}

void SedStd::StringStream::clearOutputData (bool kill_ecb)
{
	if (this->out.alt_buf)
	{
		HAWK_ASSERT (this->out.alt_sed != HAWK_NULL);
		hawk_sed_freemem(this->out.alt_sed, this->out.alt_buf);
		this->out.alt_buf = HAWK_NULL;
		this->out.alt_sed = HAWK_NULL;
	}

	if (this->out.inited)
	{
		hawk_ooecs_fini (&this->out.buf);
		if (this->out.alt_buf)
		{
			hawk_sed_freemem (this->out._sed, this->out.alt_buf);
			this->out.alt_buf = HAWK_NULL;
		}
		this->out.inited = false;

		if (kill_ecb && this->out.sed_ecb.next_)
			hawk_sed_killecb (this->out._sed, &this->out.sed_ecb);

		this->out._sed = HAWK_NULL;
	}
}

void SedStd::StringStream::on_sed_close (hawk_sed_t* sed, void* ctx)
{
	SedStd::StringStream* strm = (SedStd::StringStream*)ctx;
	strm->clearOutputData (false);
}

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////
