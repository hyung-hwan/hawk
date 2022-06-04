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
	hawk_errnum_t errnum;
	this->sed = hawk_sed_open(this->getMmgr(), HAWK_SIZEOF(xtn_t), this->getCmgr(), &errnum);
	if (!this->sed) 
	{
		this->setError (errnum);
		return -1;
	}

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
		hawk_sed_close (this->sed);
		this->sed = HAWK_NULL;
	}
}

int Sed::compile (Stream& sstream)
{
	HAWK_ASSERT (this->sed != HAWK_NULL);

	this->sstream = &sstream;
	return hawk_sed_comp(this->sed, Sed::sin);
}

int Sed::execute (Stream& iostream)
{
	HAWK_ASSERT (this->sed != HAWK_NULL);

	this->iostream = &iostream;
	return hawk_sed_exec(this->sed, Sed::xin, Sed::xout);
}

void Sed::halt ()
{
	HAWK_ASSERT (this->sed != HAWK_NULL);
	hawk_sed_halt (this->sed);
}

bool Sed::isHalt () const
{
	HAWK_ASSERT (this->sed != HAWK_NULL);
	return hawk_sed_ishalt(this->sed);
}

int Sed::getTrait () const
{
	HAWK_ASSERT (this->sed != HAWK_NULL);
	int val;
	hawk_sed_getopt (this->sed, HAWK_SED_TRAIT, &val);
	return val;
}

void Sed::setTrait (int trait)
{
	HAWK_ASSERT (this->sed != HAWK_NULL);
	hawk_sed_setopt (this->sed, HAWK_SED_TRAIT, &trait);
}

const hawk_ooch_t* Sed::getErrorMessage () const
{
	return (this->sed == HAWK_NULL)? HAWK_T(""): hawk_sed_geterrmsg(this->sed);
}

const hawk_uch_t* Sed::getErrorMessageU () const
{
	return (this->sed == HAWK_NULL)? HAWK_UT(""): hawk_sed_geterrumsg(this->sed);
}

const hawk_bch_t* Sed::getErrorMessageB () const
{
	return (this->sed == HAWK_NULL)? HAWK_BT(""): hawk_sed_geterrbmsg(this->sed);
}

hawk_loc_t Sed::getErrorLocation () const
{
	if (this->sed == HAWK_NULL) 
	{
		hawk_loc_t loc;
		loc.line = 0; 
		loc.colm = 0;
		return loc;
	}
	return *hawk_sed_geterrloc(this->sed);
}

hawk_errnum_t Sed::getErrorNumber () const
{
	return (this->sed == HAWK_NULL)? HAWK_ENOERR: hawk_sed_geterrnum(this->sed);
}

void Sed::setError (hawk_errnum_t err, const hawk_oocs_t* args, const hawk_loc_t* loc)
{
	HAWK_ASSERT (this->sed != HAWK_NULL);
	hawk_sed_seterror (this->sed, loc, err, args);
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
	HAWK_ASSERT (this->sed != HAWK_NULL);
	return hawk_sed_getlinenum(this->sed);
}

void Sed::setConsoleLine (hawk_oow_t num)
{
	HAWK_ASSERT (this->sed != HAWK_NULL);
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
				return xtn->sed->iostream->open(iodata);
			case HAWK_SED_IO_CLOSE:
				return xtn->sed->iostream->close(iodata);
			case HAWK_SED_IO_READ:
				return xtn->sed->iostream->read(iodata, buf, len);
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
				return xtn->sed->iostream->open(iodata);
			case HAWK_SED_IO_CLOSE:
				return xtn->sed->iostream->close(iodata);
			case HAWK_SED_IO_WRITE:
				return xtn->sed->iostream->write(iodata, dat, len);
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
	HAWK_ASSERT (this->dflerrstr != HAWK_NULL);
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
