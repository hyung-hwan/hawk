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
		hawk_oocs_t ea;
		ea.ptr = (hawk_ooch_t*)file;
		ea.len = hawk_count_oocstr(file);
		((Sed*)io)->setError (HAWK_SED_EIOFIL, &ea);
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
		hawk_oocs_t ea;
		ea.ptr = (hawk_ooch_t*)std_names[std];
		ea.len = hawk_count_oocstr(std_names[std]);
		((Sed*)io)->setError (HAWK_SED_EIOFIL, &ea);
	}
	return sio;
}

int SedStd::FileStream::open (Data& io)
{
	hawk_sio_t* sio;
	const hawk_ooch_t* ioname = io.getName();
	int oflags;

	if (io.getMode() == READ)
		oflags = HAWK_SIO_READ | HAWK_SIO_IGNOREECERR;
	else
		oflags = HAWK_SIO_WRITE | HAWK_SIO_CREATE | HAWK_SIO_TRUNCATE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK;

	if (ioname == HAWK_NULL)
	{
		//
		// a normal console is indicated by a null name or a dash
		//
		if (io.getMode() == READ)
		{
			sio = (infile == HAWK_NULL || (infile[0] == HAWK_T('-') && infile[1] == HAWK_T('\0')))?
				open_sio_std (io, HAWK_SIO_STDIN, oflags):
				open_sio (io, infile, oflags);
		}
		else
		{
			sio = (outfile == HAWK_NULL || (outfile[0] == HAWK_T('-') && outfile[1] == HAWK_T('\0')))?
				open_sio_std (io, HAWK_SIO_STDOUT, oflags):
				open_sio (io, outfile, oflags);
		}
	}
	else
	{
		//
		// if ioname is not empty, it is a file name
		//
		sio = open_sio (io, ioname, oflags);
	}
	if (sio == HAWK_NULL) return -1;

	if (this->cmgr) hawk_sio_setcmgr (sio, this->cmgr);
	io.setHandle (sio);
	return 1;
}

int SedStd::FileStream::close (Data& io)
{
	hawk_sio_close ((hawk_sio_t*)io.getHandle());
	return 0;
}

hawk_ooi_t SedStd::FileStream::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	hawk_ooi_t n = hawk_sio_getoochars((hawk_sio_t*)io.getHandle(), buf, len);

	if (n == -1)
	{
		if (io.getName() == HAWK_NULL && infile != HAWK_NULL) 
		{
			// if writing to outfile, set the error message
			// explicitly. other cases are handled by 
			// the caller in sed.c.
			hawk_oocs_t ea;
			ea.ptr = (hawk_ooch_t*)infile;
			ea.len = hawk_count_oocstr(infile);
			((Sed*)io)->setError (HAWK_SED_EIOFIL, &ea);
		}
	}

	return n;
}

hawk_ooi_t SedStd::FileStream::write (Data& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	hawk_ooi_t n = hawk_sio_putoochars((hawk_sio_t*)io.getHandle(), buf, len);

	if (n == -1)
	{
		if (io.getName() == HAWK_NULL && outfile != HAWK_NULL) 
		{
			// if writing to outfile, set the error message
			// explicitly. other cases are handled by 
			// the caller in sed.c.
			hawk_oocs_t ea;
			ea.ptr = (hawk_ooch_t*)outfile;
			ea.len = hawk_count_oocstr(outfile);
			((Sed*)io)->setError (HAWK_SED_EIOFIL, &ea);
		}
	}

	return n;
}

SedStd::StringStream::StringStream (const hawk_ooch_t* in)
{
	this->in.ptr = in; 
	this->in.end = in + hawk_count_oocstr(in);
	this->out.inited = false;
}

SedStd::StringStream::StringStream (const hawk_ooch_t* in, hawk_oow_t len)
{
	this->in.ptr = in; 
	this->in.end = in + len;
	this->out.inited = false;
}

SedStd::StringStream::~StringStream ()
{
	if (out.inited) hawk_ooecs_fini (&out.buf);
}
	
int SedStd::StringStream::open (Data& io)
{
	const hawk_ooch_t* ioname = io.getName ();

	if (ioname == HAWK_NULL)
	{
		// open a main data stream
		if (io.getMode() == READ) 
		{
			in.cur = in.ptr;
			io.setHandle ((void*)in.ptr);
		}
		else
		{
			if (!out.inited)
			{
				if (hawk_ooecs_init(&out.buf, (hawk_gem_t*)io, 256) <= -1)
				{
					((Sed*)io)->setError (HAWK_ENOMEM);
					return -1;
				}

				out.inited = true;
			}

			hawk_ooecs_clear (&out.buf);
			io.setHandle (&out.buf);
		}
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
	if (handle != in.ptr && handle != &out.buf)
		hawk_sio_close ((hawk_sio_t*)handle);
	return 0;
}

hawk_ooi_t SedStd::StringStream::read (Data& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	const void* handle = io.getHandle();

	if (len == (hawk_oow_t)-1) len--; // shrink buffer if too long
	if (handle == in.ptr)
	{
		hawk_oow_t n = 0;
		while (in.cur < in.end && n < len)
			buf[n++] = *in.cur++;
		return (hawk_ooi_t)n;
	}
	else
	{
		HAWK_ASSERT (handle != &out.buf);
		return hawk_sio_getoochars((hawk_sio_t*)handle, buf, len);
	}
}

hawk_ooi_t SedStd::StringStream::write (Data& io, const hawk_ooch_t* data, hawk_oow_t len)
{
	const void* handle = io.getHandle();

	if (len == (hawk_oow_t)-1) len--; // shrink data if too long

	if (handle == &out.buf)
	{
		if (hawk_ooecs_ncat(&out.buf, data, len) == (hawk_oow_t)-1)
		{
			((Sed*)io)->setError (HAWK_ENOMEM);
			return -1;
		}

		return len;
	}
	else
	{
		HAWK_ASSERT (handle != in.ptr);
		return hawk_sio_putoochars((hawk_sio_t*)handle, data, len);
	}
}

const hawk_ooch_t* SedStd::StringStream::getInput (hawk_oow_t* len) const
{
	if (len) *len = in.end - in.ptr;
	return in.ptr;
}

const hawk_ooch_t* SedStd::StringStream::getOutput (hawk_oow_t* len) const
{
	if (out.inited)
	{
		if (len) *len = HAWK_OOECS_LEN(&out.buf);
		return HAWK_OOECS_PTR(&out.buf);
	}
	else
	{
		if (len) *len = 0;
		return HAWK_T("");
	}
}

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////
