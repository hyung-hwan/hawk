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
#include "sed-prv.h"
#include "hawk-prv.h"

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

struct xtn_t
{
	Sed* sed;
};

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE xtn_t* GET_XTN(hawk_sed_t* sed) { return (xtn_t*)((hawk_uint8_t*)hawk_sed_getxtn(sed) - HAWK_SIZEOF(xtn_t)); }
#else
#define GET_XTN(sed) ((xtn_t*)((hawk_uint8_t*)hawk_sed_getxtn(sed) - HAWK_SIZEOF(xtn_t)))
#endif

Sed::Sed (Mmgr* mmgr): Mmged(mmgr), sed(HAWK_NULL), dflerrstr(HAWK_NULL)
{
	HAWK_MEMSET (&this->errinf, 0, HAWK_SIZEOF(this->errinf));
	this->errinf.num = HAWK_ENOERR;
	this->_cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
}

hawk_cmgr_t* Sed::getCmgr () const
{
	return this->sed? hawk_sed_getcmgr(this->sed): this->_cmgr;
}

void Sed::setCmgr (hawk_cmgr_t* cmgr)
{
	if (this->sed) hawk_sed_setcmgr(this->sed, cmgr);
	this->_cmgr = cmgr;
}

int Sed::open ()
{
	this->sed = hawk_sed_open(this->getMmgr(), HAWK_SIZEOF(xtn_t), this->getCmgr(), &this->errinf);
	if (HAWK_UNLIKELY(!this->sed)) return -1;

	this->sed->_instsize += HAWK_SIZEOF(xtn_t);

	xtn_t* xtn = GET_XTN(this->sed);
	xtn->sed = this;

	this->dflerrstr = hawk_sed_geterrstr(this->sed);
// TODO: revive this too when hawk_sed_seterrstr is revived()
//	hawk_sed_seterrstr (this->sed, Sed::xerrstr);

	return 0;
}

void Sed::close ()
{
	if (this->sed)
	{
		hawk_sed_close(this->sed);
		this->sed = HAWK_NULL;
	}
}

int Sed::compile (Stream& sstream)
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	this->sstream = &sstream;
	int n = hawk_sed_comp(this->sed, Sed::sin);
	if (n <= -1) this->retrieveError();
	return n;
}

int Sed::execute (Stream& istream, Stream& ostream)
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	this->istream = &istream;
	this->ostream = &ostream;
	int n = hawk_sed_exec(this->sed, Sed::xin, Sed::xout);
	if (n <= -1) this->retrieveError();
	return n;
}

void Sed::halt ()
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	hawk_sed_halt(this->sed);
}

bool Sed::isHalt () const
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	return hawk_sed_ishalt(this->sed);
}

int Sed::getTrait () const
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	int val;
	hawk_sed_getopt(this->sed, HAWK_SED_TRAIT, &val);
	return val;
}

void Sed::setTrait (int trait)
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	hawk_sed_setopt(this->sed, HAWK_SED_TRAIT, &trait);
}

const hawk_ooch_t* Sed::getErrorMessage () const
{
	return this->errinf.msg;
}

const hawk_uch_t* Sed::getErrorMessageU () const
{
#if defined(HAWK_OOCH_IS_UCH)
	return this->errinf.msg;
#else
	hawk_oow_t wcslen, mbslen;
	wcslen = HAWK_COUNTOF(this->xerrmsg);
	hawk_conv_bcstr_to_ucstr_with_cmgr(this->errinf.msg, &mbslen, this->xerrmsg, &wcslen, this->getCmgr(), 1);
	return this->xerrmsg;
#endif
}

const hawk_bch_t* Sed::getErrorMessageB () const
{
#if defined(HAWK_OOCH_IS_UCH)
	hawk_oow_t wcslen, mbslen;
	mbslen = HAWK_COUNTOF(this->xerrmsg);
	hawk_conv_ucstr_to_bcstr_with_cmgr(this->errinf.msg, &wcslen, this->xerrmsg, &mbslen, this->getCmgr());
	return this->xerrmsg;
#else
	return this->errinf.msg;
#endif
}

hawk_loc_t Sed::getErrorLocation () const
{
	return this->errinf.loc;
}

const hawk_uch_t* Sed::getErrorLocationFileU () const
{
#if defined(HAWK_OOCH_IS_UCH)
	return this->errinf.loc.file;
#else
	if (!this->errinf.loc.file) return HAWK_NULL;
	hawk_oow_t wcslen, mbslen;
	wcslen = HAWK_COUNTOF(this->xerrlocfile);
	hawk_conv_bcstr_to_ucstr_with_cmgr(this->errinf.loc.file, &mbslen, this->xerrlocfile, &wcslen, this->getCmgr(), 1);
	return this->xerrlocfile;
#endif
}

const hawk_bch_t* Sed::getErrorLocationFileB () const
{
#if defined(HAWK_OOCH_IS_UCH)
	if (!this->errinf.loc.file) return HAWK_NULL;
	hawk_oow_t wcslen, mbslen;
	mbslen = HAWK_COUNTOF(this->xerrlocfile);
	hawk_conv_ucstr_to_bcstr_with_cmgr(this->errinf.loc.file, &wcslen, this->xerrlocfile, &mbslen, this->getCmgr());
	return this->xerrlocfile;
#else
	return this->errinf.loc.file;
#endif
}

hawk_errnum_t Sed::getErrorNumber () const
{
	return this->errinf.num;
}

void Sed::setError (hawk_errnum_t code, const hawk_oocs_t* args, const hawk_loc_t* loc)
{
	if (this->sed)
	{
		hawk_sed_seterror(this->sed, loc, code, args);
		this->retrieveError();
	}
	else
	{
		HAWK_MEMSET(&this->errinf, 0, HAWK_SIZEOF(this->errinf));
		this->errinf.num = code;
		if (loc) this->errinf.loc = *loc;
		hawk_copy_oocstr(this->errinf.msg, HAWK_COUNTOF(this->errinf.msg), hawk_dfl_errstr(code));
	}
}

void Sed::formatError (hawk_errnum_t code, const hawk_loc_t* loc, const hawk_bch_t* fmt, ...)
{
	if (this->sed)
	{
		va_list ap;
		va_start(ap, fmt);
		hawk_sed_seterrbvfmt(this->sed, loc, code, fmt, ap);
		va_end(ap);
		this->retrieveError();
	}
	else
	{
		HAWK_MEMSET(&this->errinf, 0, HAWK_SIZEOF(this->errinf));
		this->errinf.num = code;
		if (loc) this->errinf.loc = *loc;
		hawk_copy_oocstr(this->errinf.msg, HAWK_COUNTOF(this->errinf.msg), hawk_dfl_errstr(code));
	}
}

void Sed::formatError (hawk_errnum_t code, const hawk_loc_t* loc, const hawk_uch_t* fmt, ...)
{
	if (this->sed)
	{
		va_list ap;
		va_start(ap, fmt);
		hawk_sed_seterruvfmt(this->sed, loc, code, fmt, ap);
		va_end(ap);
		this->retrieveError();
	}
	else
	{
		HAWK_MEMSET(&this->errinf, 0, HAWK_SIZEOF(this->errinf));
		this->errinf.num = code;
		if (loc) this->errinf.loc = *loc;
		hawk_copy_oocstr(this->errinf.msg, HAWK_COUNTOF(this->errinf.msg), hawk_dfl_errstr(code));
	}
}

void Sed::clearError ()
{
	HAWK_MEMSET(&this->errinf, 0, HAWK_SIZEOF(this->errinf));
	this->errinf.num = HAWK_ENOERR;
}

void Sed::retrieveError()
{
	if (this->sed == HAWK_NULL)
	{
		this->clearError();
	}
	else
	{
		hawk_sed_geterrinf(this->sed, &this->errinf);
	}
}

const hawk_ooch_t* Sed::getCompileId () const
{
	return hawk_sed_getcompid(this->sed);
}

const hawk_ooch_t* Sed::setCompileId (const hawk_ooch_t* id)
{
	return hawk_sed_setcompid(this->sed, id);
}

hawk_oow_t Sed::getConsoleLine ()
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	return hawk_sed_getlinenum(this->sed);
}

void Sed::setConsoleLine (hawk_oow_t num)
{
	HAWK_ASSERT(this->sed != HAWK_NULL);
	hawk_sed_setlinenum (this->sed, num);
}

hawk_ooi_t Sed::sin (hawk_sed_t* s, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(s);

	Stream::Data iodata (xtn->sed, Stream::READ, arg);

	try
	{
		switch (cmd)
		{
			case HAWK_SED_IO_OPEN:
				return xtn->sed->sstream->open(iodata);
			case HAWK_SED_IO_CLOSE:
				return xtn->sed->sstream->close(iodata);
			case HAWK_SED_IO_READ:
				return xtn->sed->sstream->read(iodata, buf, len);
			default:
				return -1;
		}
	}
	catch (...)
	{
		return -1;
	}
}

hawk_ooi_t Sed::xin (hawk_sed_t* s, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(s);

	Stream::Data iodata (xtn->sed, Stream::READ, arg);

	try
	{
		switch (cmd)
		{
			case HAWK_SED_IO_OPEN:
				return xtn->sed->istream->open(iodata);
			case HAWK_SED_IO_CLOSE:
				return xtn->sed->istream->close(iodata);
			case HAWK_SED_IO_READ:
				return xtn->sed->istream->read(iodata, buf, len);
			default:
				return -1;
		}
	}
	catch (...)
	{
		return -1;
	}
}

hawk_ooi_t Sed::xout (hawk_sed_t* s, hawk_sed_io_cmd_t cmd, hawk_sed_io_arg_t* arg, hawk_ooch_t* dat, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(s);

	Stream::Data iodata (xtn->sed, Stream::WRITE, arg);

	try
	{
		switch (cmd)
		{
			case HAWK_SED_IO_OPEN:
				return xtn->sed->ostream->open(iodata);
			case HAWK_SED_IO_CLOSE:
				return xtn->sed->ostream->close(iodata);
			case HAWK_SED_IO_WRITE:
				return xtn->sed->ostream->write(iodata, dat, len);
			default:
				return -1;
		}
	}
	catch (...)
	{
		return -1;
	}
}

const hawk_ooch_t* Sed::getErrorString (hawk_errnum_t num) const
{
	HAWK_ASSERT(this->dflerrstr != HAWK_NULL);
	return this->dflerrstr(num);
}

const hawk_ooch_t* Sed::xerrstr (hawk_sed_t* s, hawk_errnum_t num)
{
	xtn_t* xtn = GET_XTN(s);
	return xtn->sed->getErrorString(num);
}

/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////
