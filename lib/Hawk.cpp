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
#include "hawk-prv.h"

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

//////////////////////////////////////////////////////////////////
// Mmged
//////////////////////////////////////////////////////////////////

Mmged::Mmged (Mmgr* mmgr)
{
	if (!mmgr) mmgr = Mmgr::getDFL();
	this->_mmgr = mmgr;
}

void Mmged::setMmgr (Mmgr* mmgr)
{
	if (!mmgr) mmgr = Mmgr::getDFL();
	this->_mmgr = mmgr;
}

//////////////////////////////////////////////////////////////////
// Mmgr
//////////////////////////////////////////////////////////////////


void* Mmgr::alloc_mem (mmgr_t* mmgr, hawk_oow_t n) HAWK_CXX_NOEXCEPT
{
	return ((Mmgr*)mmgr->ctx)->allocMem (n);
}

void* Mmgr::realloc_mem (mmgr_t* mmgr, void* ptr, hawk_oow_t n) HAWK_CXX_NOEXCEPT
{
	return ((Mmgr*)mmgr->ctx)->reallocMem (ptr, n);
}

void Mmgr::free_mem (mmgr_t* mmgr, void* ptr) HAWK_CXX_NOEXCEPT
{
	((Mmgr*)mmgr->ctx)->freeMem (ptr);
}

void* Mmgr::callocate (hawk_oow_t n, bool raise_exception) /*HAWK_CXX_THREXCEPT1(MemoryError)*/
{
	void* ptr = this->allocate(n, raise_exception);
	HAWK_MEMSET (ptr, 0, n);
	return ptr;
}

#if 0
#if defined(__GNUC__)
static MmgrStd __attribute__((init_priority(101))) std_dfl_mmgr; //<- this solved the problem
#else
static MmgrStd std_dfl_mmgr; //<-- has an issue for undefined initialization order
#endif
Mmgr* Mmgr::dfl_mmgr = &std_dfl_mmgr;
//Mmgr* Mmgr::dfl_mmgr = MmgrStd::getInstance();  //<--- has an issue as well
Mmgr* Mmgr::getDFL () HAWK_CXX_NOEXCEPT
{
	return Mmgr::dfl_mmgr;
}
#else
// C++ initialization order across translation units are not defined.
// so it's tricky to work around this... the init_priority attribute
// can solve this problem. but it's compiler dependent.
// Mmgr::getDFL() resets dfl_mmgr to &std_dfl_mmgr if it's NULL.
Mmgr* Mmgr::dfl_mmgr = HAWK_NULL;

Mmgr* Mmgr::getDFL () HAWK_CXX_NOEXCEPT
{
	static MmgrStd std_dfl_mmgr;
	Mmgr* mmgr = Mmgr::dfl_mmgr;
	return mmgr? mmgr: &std_dfl_mmgr;
}
#endif

void Mmgr::setDFL (Mmgr* mmgr) HAWK_CXX_NOEXCEPT
{
	Mmgr::dfl_mmgr = mmgr;
}


//////////////////////////////////////////////////////////////////
// Hawk::Source
//////////////////////////////////////////////////////////////////

struct xtn_t
{
	Hawk* hawk;
	hawk_ecb_t ecb;
};

struct rxtn_t
{
	Hawk::Run* run;
};

Hawk::NoSource Hawk::Source::NONE;

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE xtn_t* GET_XTN(hawk_t* hawk) { return (xtn_t*)((hawk_uint8_t*)hawk_getxtn(hawk) - HAWK_SIZEOF(xtn_t)); }
static HAWK_INLINE rxtn_t* GET_RXTN(hawk_rtx_t* rtx) { return (rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)); }
#else
#define GET_XTN(hawk) ((xtn_t*)((hawk_uint8_t*)hawk_getxtn(hawk) - HAWK_SIZEOF(xtn_t)))
#define GET_RXTN(rtx) ((rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)))
#endif

//////////////////////////////////////////////////////////////////
// Hawk::Pipe
//////////////////////////////////////////////////////////////////

Hawk::Pipe::Pipe (Run* run, hawk_rio_arg_t* riod): RIOBase (run, riod)
{
}

Hawk::Pipe::Mode Hawk::Pipe::getMode () const
{
	return (Mode)riod->mode;
}

Hawk::Pipe::CloseMode Hawk::Pipe::getCloseMode () const
{
	return (CloseMode)riod->rwcmode;
}

//////////////////////////////////////////////////////////////////
// Hawk::File
//////////////////////////////////////////////////////////////////

Hawk::File::File (Run* run, hawk_rio_arg_t* riod): RIOBase (run, riod)
{
}

Hawk::File::Mode Hawk::File::getMode () const
{
	return (Mode)riod->mode;
}

//////////////////////////////////////////////////////////////////
// Hawk::Console
//////////////////////////////////////////////////////////////////

Hawk::Console::Console (Run* run, hawk_rio_arg_t* riod):
	RIOBase (run, riod), filename (HAWK_NULL)
{
}

Hawk::Console::~Console ()
{
	if (filename != HAWK_NULL)
	{
		hawk_freemem((hawk_t*)this, filename);
	}
}

int Hawk::Console::setFileName (const hawk_ooch_t* name)
{
	if (this->getMode() == READ)
	{
		return hawk_rtx_setfilenamewithoochars(this->run->rtx, name, hawk_count_oocstr(name));
	}
	else
	{
		return hawk_rtx_setofilenamewithoochars(this->run->rtx, name, hawk_count_oocstr(name));
	}
}

int Hawk::Console::setFNR (hawk_int_t fnr)
{
	hawk_val_t* tmp;
	int n;

	tmp = hawk_rtx_makeintval(this->run->rtx, fnr);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval(this->run->rtx, tmp);
	n = hawk_rtx_setgbl(this->run->rtx, HAWK_GBL_FNR, tmp);
	hawk_rtx_refdownval(this->run->rtx, tmp);

	return n;
}

Hawk::Console::Mode Hawk::Console::getMode () const
{
	return (Mode)riod->mode;
}

//////////////////////////////////////////////////////////////////
// Hawk::Value
//////////////////////////////////////////////////////////////////

Hawk::Value::IndexIterator Hawk::Value::IndexIterator::END;

const hawk_ooch_t* Hawk::Value::getEmptyStr()
{
	static const hawk_ooch_t* EMPTY_STRING = HAWK_T("");
	return EMPTY_STRING;
}
const hawk_bch_t* Hawk::Value::getEmptyMbs()
{
	static const hawk_bch_t* EMPTY_STRING = HAWK_BT("");
	return EMPTY_STRING;
}

const void* Hawk::Value::getEmptyBob()
{
	static const hawk_uint8_t EMPTY_BOB = 0;
	return &EMPTY_BOB;
}

Hawk::Value::IntIndex::IntIndex (hawk_int_t x)
{
	ptr = buf;
	len = 0;

#define NTOC(n) (HAWK_T("0123456789")[n])

	int base = 10;
	hawk_int_t last = x % base;
	hawk_int_t y = 0;
	int dig = 0;

	if (x < 0) buf[len++] = HAWK_T('-');

	x = x / base;
	if (x < 0) x = -x;

	while (x > 0)
	{
		y = y * base + (x % base);
		x = x / base;
		dig++;
	}

        while (y > 0)
        {
		buf[len++] = NTOC (y % base);
		y = y / base;
		dig--;
	}

	while (dig > 0)
	{
		dig--;
		buf[len++] = HAWK_T('0');
	}
	if (last < 0) last = -last;
	buf[len++] = NTOC(last);

	buf[len] = HAWK_T('\0');

#undef NTOC
}

#if defined(HAWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW)
void* Hawk::Value::operator new (hawk_oow_t n, Run* run) throw ()
{
	void* ptr = hawk_rtx_allocmem(run->rtx, HAWK_SIZEOF(run) + n);
	if (HAWK_UNLIKELY(!ptr)) return HAWK_NULL;

	*(Run**)ptr = run;
	return (char*)ptr + HAWK_SIZEOF(run);
}

void* Hawk::Value::operator new[] (hawk_oow_t n, Run* run) throw ()
{
	void* ptr = hawk_rtx_allocmem(run->rtx, HAWK_SIZEOF(run) + n);
	if (HAWK_UNLIKELY(!ptr)) return HAWK_NULL;

	*(Run**)ptr = run;
	return (char*)ptr + HAWK_SIZEOF(run);
}

#if !defined(__BORLANDC__) && !defined(__WATCOMC__)
void Hawk::Value::operator delete (void* ptr, Run* run)
{
	// this placement delete is to be called when the constructor
	// throws an exception and it's caught by the caller.
	hawk_rtx_freemem(run->rtx, (char*)ptr - HAWK_SIZEOF(run));
}

void Hawk::Value::operator delete[] (void* ptr, Run* run)
{
	// this placement delete is to be called when the constructor
	// throws an exception and it's caught by the caller.
	hawk_rtx_freemem(run->rtx, (char*)ptr - HAWK_SIZEOF(run));
}
#endif

void Hawk::Value::operator delete (void* ptr)
{
	// this delete is to be called for normal destruction.
	void* p = (char*)ptr - HAWK_SIZEOF(Run*);
	hawk_rtx_freemem((*(Run**)p)->rtx, p);
}

void Hawk::Value::operator delete[] (void* ptr)
{
	// this delete is to be called for normal destruction.
	void* p = (char*)ptr - HAWK_SIZEOF(Run*);
	hawk_rtx_freemem((*(Run**)p)->rtx, p);
}
#endif

Hawk::Value::Value (): run(HAWK_NULL), val(hawk_get_nil_val())
{
	this->cached.str.ptr = HAWK_NULL;
	this->cached.str.len = 0;
	this->cached.mbs.ptr = HAWK_NULL;
	this->cached.mbs.len = 0;
}

Hawk::Value::Value (Run& run): run(&run), val(hawk_get_nil_val())
{
	this->cached.str.ptr = HAWK_NULL;
	this->cached.str.len = 0;
	this->cached.mbs.ptr = HAWK_NULL;
	this->cached.mbs.len = 0;
}

Hawk::Value::Value (Run* run): run(run), val(hawk_get_nil_val())
{
	this->cached.str.ptr = HAWK_NULL;
	this->cached.str.len = 0;
	this->cached.mbs.ptr = HAWK_NULL;
	this->cached.mbs.len = 0;
}

Hawk::Value::Value (const Value& v): run(v.run), val(v.val)
{
	if (this->run) hawk_rtx_refupval(this->run->rtx, this->val);

	this->cached.str.ptr = HAWK_NULL;
	this->cached.str.len = 0;
	this->cached.mbs.ptr = HAWK_NULL;
	this->cached.mbs.len = 0;
}

Hawk::Value::~Value ()
{
	if (this->run)
	{
		hawk_rtx_refdownval(this->run->rtx, val);
		if (this->cached.str.ptr) hawk_rtx_freemem(this->run->rtx, cached.str.ptr);
		if (this->cached.mbs.ptr) hawk_rtx_freemem(this->run->rtx, cached.mbs.ptr);
	}
}

Hawk::Value& Hawk::Value::operator= (const Value& v)
{
	if (this == &v) return *this;

	if (this->run)
	{
		hawk_rtx_refdownval(this->run->rtx, this->val);
		if (this->cached.str.ptr)
		{
			hawk_rtx_freemem(this->run->rtx, this->cached.str.ptr);
			this->cached.str.ptr = HAWK_NULL;
			this->cached.str.len = 0;
		}
		if (this->cached.mbs.ptr)
		{
			hawk_rtx_freemem(this->run->rtx, this->cached.mbs.ptr);
			this->cached.mbs.ptr = HAWK_NULL;
			this->cached.mbs.len = 0;
		}
	}

	this->run = v.run;
	this->val = v.val;

	if (this->run) hawk_rtx_refupval(this->run->rtx, this->val);

	return *this;
}

void Hawk::Value::clear ()
{
	if (this->run)
	{
		hawk_rtx_refdownval(this->run->rtx, this->val);

		if (this->cached.str.ptr)
		{
			hawk_rtx_freemem(this->run->rtx, this->cached.str.ptr);
			this->cached.str.ptr = HAWK_NULL;
			this->cached.str.len = 0;
		}
		if (this->cached.mbs.ptr)
		{
			hawk_rtx_freemem(this->run->rtx, this->cached.mbs.ptr);
			this->cached.mbs.ptr = HAWK_NULL;
			this->cached.mbs.len = 0;
		}

		this->run = HAWK_NULL;
		this->val = hawk_get_nil_val();
	}
}

Hawk::Value::operator hawk_int_t () const
{
	hawk_int_t v;
	if (this->getInt(&v) <= -1) v = 0;
	return v;
}

Hawk::Value::operator hawk_flt_t () const
{
	hawk_flt_t v;
	if (this->getFlt(&v) <= -1) v = 0.0;
	return v;
}

Hawk::Value::operator const hawk_ooch_t* () const
{
	const hawk_ooch_t* ptr;
	hawk_oow_t len;
	if (Hawk::Value::getStr(&ptr, &len) <= -1) ptr = getEmptyStr();
	return ptr;
}

#if defined(HAWK_OOCH_IS_UCH)
Hawk::Value::operator const hawk_bch_t* () const
{
	const hawk_bch_t* ptr;
	hawk_oow_t len;
	if (Hawk::Value::getMbs(&ptr, &len) <= -1) ptr = getEmptyMbs();
	return ptr;
}
#endif

int Hawk::Value::getInt (hawk_int_t* v) const
{
	hawk_int_t lv = 0;

	HAWK_ASSERT(this->val != HAWK_NULL);

	if (this->run)
	{
		int n = hawk_rtx_valtoint(this->run->rtx, this->val, &lv);
		if (n <= -1)
		{
			run->hawk->retrieveError(this->run);
			return -1;
		}
	}

	*v = lv;
	return 0;
}

int Hawk::Value::getFlt (hawk_flt_t* v) const
{
	hawk_flt_t rv = 0;

	HAWK_ASSERT(this->val != HAWK_NULL);

	if (this->run)
	{
		int n = hawk_rtx_valtoflt(this->run->rtx, this->val, &rv);
		if (n <= -1)
		{
			run->hawk->retrieveError(this->run);
			return -1;
		}
	}

	*v = rv;
	return 0;
}

int Hawk::Value::getNum (hawk_int_t* lv, hawk_flt_t* fv) const
{
	HAWK_ASSERT(this->val != HAWK_NULL);

	if (this->run)
	{
		int n = hawk_rtx_valtonum(this->run->rtx, this->val, lv, fv);
		if (n <= -1)
		{
			run->hawk->retrieveError(this->run);
			return -1;
		}
		return n;
	}

	*lv = 0;
	return 0;
}

int Hawk::Value::getStr (const hawk_ooch_t** str, hawk_oow_t* len) const
{
	const hawk_ooch_t* p = getEmptyStr();
	hawk_oow_t l = 0;

	HAWK_ASSERT(this->val != HAWK_NULL);

	if (this->run)
	{
		if (HAWK_RTX_GETVALTYPE(this->run->rtx, this->val) == HAWK_VAL_STR)
		{
			p = ((hawk_val_str_t*)this->val)->val.ptr;
			l = ((hawk_val_str_t*)this->val)->val.len;
		}
		else
		{
			if (!cached.str.ptr)
			{
				hawk_rtx_valtostr_out_t out;
				out.type = HAWK_RTX_VALTOSTR_CPLDUP;
				if (hawk_rtx_valtostr(this->run->rtx, this->val, &out) <= -1)
				{
					run->hawk->retrieveError(this->run);
					return -1;
				}

				p = out.u.cpldup.ptr;
				l = out.u.cpldup.len;

				cached.str.ptr = out.u.cpldup.ptr;
				cached.str.len = out.u.cpldup.len;
			}
			else
			{
				p = cached.str.ptr;
				l = cached.str.len;
			}
		}
	}

	*str = p;
	*len = l;

	return 0;
}

int Hawk::Value::getMbs (const hawk_bch_t** str, hawk_oow_t* len) const
{
	const hawk_bch_t* p = getEmptyMbs();
	hawk_oow_t l = 0;

	HAWK_ASSERT(this->val != HAWK_NULL);

	if (this->run)
	{
		if (HAWK_RTX_GETVALTYPE(this->run->rtx, this->val) == HAWK_VAL_MBS)
		{
			p = ((hawk_val_mbs_t*)this->val)->val.ptr;
			l = ((hawk_val_mbs_t*)this->val)->val.len;
		}
		else
		{
			if (!cached.mbs.ptr)
			{
				p = hawk_rtx_valtobcstrdup(this->run->rtx, this->val, &l);
				if (!p)
				{
					run->hawk->retrieveError(this->run);
					return -1;
				}

				cached.mbs.ptr = (hawk_bch_t*)p;
				cached.mbs.len = l;
			}
			else
			{
				p = cached.mbs.ptr;
				l = cached.mbs.len;
			}
		}
	}

	*str = p;
	*len = l;

	return 0;
}

int Hawk::Value::getBob (const void** ptr, hawk_oow_t* len) const
{
	const void* p = getEmptyBob();
	hawk_oow_t l = 0;

	HAWK_ASSERT(this->val != HAWK_NULL);

	if (this->run)
	{
		if (HAWK_RTX_GETVALTYPE(this->run->rtx, this->val) == HAWK_VAL_BOB)
		{
			p = ((hawk_val_bob_t*)this->val)->val.ptr;
			l = ((hawk_val_bob_t*)this->val)->val.len;
		}
	}

	*ptr = p;
	*len = l;

	return 0;
}

//////////////////////////////////////////////////////////////////

int Hawk::Value::setVal (hawk_val_t* v)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setVal(this->run, v);
}

int Hawk::Value::setVal (Run* r, hawk_val_t* v)
{
	if (this->run != HAWK_NULL)
	{
		hawk_rtx_refdownval(this->run->rtx, val);
		if (cached.str.ptr)
		{
			hawk_rtx_freemem(this->run->rtx, cached.str.ptr);
			cached.str.ptr = HAWK_NULL;
			cached.str.len = 0;
		}
		if (cached.mbs.ptr)
		{
			hawk_rtx_freemem(this->run->rtx, cached.mbs.ptr);
			cached.mbs.ptr = HAWK_NULL;
			cached.mbs.len = 0;
		}
	}

	HAWK_ASSERT(cached.str.ptr == HAWK_NULL);
	HAWK_ASSERT(cached.mbs.ptr == HAWK_NULL);
	hawk_rtx_refupval(r->rtx, v);

	this->run = r;
	this->val = v;

	return 0;
}

int Hawk::Value::setInt (hawk_int_t v)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setInt(this->run, v);
}

int Hawk::Value::setInt (Run* r, hawk_int_t v)
{
	hawk_val_t* tmp;
	tmp = hawk_rtx_makeintval(r->rtx, v);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

int Hawk::Value::setFlt (hawk_flt_t v)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setFlt(this->run, v);
}

int Hawk::Value::setFlt (Run* r, hawk_flt_t v)
{
	hawk_val_t* tmp;
	tmp = hawk_rtx_makefltval(r->rtx, v);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

//////////////////////////////////////////////////////////////////

int Hawk::Value::setStr (const hawk_uch_t* str, hawk_oow_t len, bool numeric)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setStr(this->run, str, len, numeric);
}

int Hawk::Value::setStr (Run* r, const hawk_uch_t* str, hawk_oow_t len, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithuchars(r->rtx, str, len):
	               hawk_rtx_makestrvalwithuchars(r->rtx, str, len);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

int Hawk::Value::setStr (const hawk_uch_t* str, bool numeric)
{
	if (HAWK_UNLIKELY(!this->run)) return -1;
	return this->setStr(this->run, str, numeric);
}

int Hawk::Value::setStr (Run* r, const hawk_uch_t* str, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithuchars(r->rtx, str, hawk_count_ucstr(str)):
	               hawk_rtx_makestrvalwithucstr(r->rtx, str);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

//////////////////////////////////////////////////////////////////

int Hawk::Value::setStr (const hawk_bch_t* str, hawk_oow_t len, bool numeric)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setStr(this->run, str, len, numeric);
}

int Hawk::Value::setStr (Run* r, const hawk_bch_t* str, hawk_oow_t len, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithbchars(r->rtx, str, len):
	               hawk_rtx_makestrvalwithbchars(r->rtx, str, len);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

int Hawk::Value::setStr (const hawk_bch_t* str, bool numeric)
{
	if (HAWK_UNLIKELY(!this->run)) return -1;
	return this->setStr(this->run, str, numeric);
}

int Hawk::Value::setStr (Run* r, const hawk_bch_t* str, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithbchars(r->rtx, str, hawk_count_bcstr(str)):
	               hawk_rtx_makestrvalwithbcstr(r->rtx, str);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

//////////////////////////////////////////////////////////////////
int Hawk::Value::setMbs (const hawk_bch_t* str, hawk_oow_t len)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setMbs(this->run, str, len);
}

int Hawk::Value::setMbs (Run* r, const hawk_bch_t* str, hawk_oow_t len)
{
	hawk_val_t* tmp;

	hawk_bcs_t oocs;
	oocs.ptr = (hawk_bch_t*)str;
	oocs.len = len;

	tmp = hawk_rtx_makembsvalwithbcs(r->rtx, &oocs);
	if (!tmp)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

int Hawk::Value::setMbs (const hawk_bch_t* str)
{
	if (HAWK_UNLIKELY(!this->run)) return -1;
	return this->setMbs(this->run, str);
}

int Hawk::Value::setMbs (Run* r, const hawk_bch_t* str)
{
	hawk_val_t* tmp;
	tmp = hawk_rtx_makembsvalwithbchars(r->rtx, str, hawk_count_bcstr(str));
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

///////////////////////////////////////////////////////////////////

int Hawk::Value::setBob (const void* ptr, hawk_oow_t len)
{
	if (this->run == HAWK_NULL)
	{
		/* no runtime context assoicated. unfortunately, i can't
		 * set an error number for the same reason */
		return -1;
	}
	return this->setBob(this->run, ptr, len);
}

int Hawk::Value::setBob (Run* r, const void* ptr, hawk_oow_t len)
{
	hawk_val_t* tmp;

	tmp = hawk_rtx_makebobval(r->rtx, ptr, len);
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT(n == 0);
	return n;
}

///////////////////////////////////////////////////////////////////
int Hawk::Value::scaleArrayed (hawk_ooi_t size)
{
	if (HAWK_UNLIKELY(!this->run)) return -1;
	return this->scaleArrayed(this->run, size);
}

int Hawk::Value::scaleArrayed (Run* r, hawk_ooi_t size)
{
	HAWK_ASSERT(r != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_ARR)
	{
		// the previous value is not a arr.
		// a new arr value needs to be created first.
		hawk_val_t* arr = hawk_rtx_makearrval(r->rtx, size);
		if (arr == HAWK_NULL)
		{
			r->hawk->retrieveError(r);
			return -1;
		}

		hawk_rtx_refupval(r->rtx, arr);

		// free the previous value
		if (this->run)
		{
			// if val is not nil, this->run can't be NULL
			hawk_rtx_refdownval(this->run->rtx, this->val);
		}

		this->run = r;
		this->val = arr;
	}
	else
	{
		HAWK_ASSERT(run != HAWK_NULL);

		// if the previous value is a arr, things are a bit simpler
		// however it needs to check if the runtime context matches
		// with the previous one.
		if (this->run != r)
		{
			// it can't span across multiple runtime contexts
			this->run->setError(HAWK_EINVAL);
			this->run->hawk->retrieveError(run);
			return -1;
		}

		// update the capacity of an array
		if (hawk_rtx_scalearrval(r->rtx, this->val, size) <= -1)
		{
			r->hawk->retrieveError(r);
			return -1;
		}
	}

	return 0;
}

hawk_ooi_t Hawk::Value::getArrayedLength () const
{
	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_ARR) return -1;
	return (hawk_ooi_t)HAWK_ARR_TALLY(((hawk_val_arr_t*)this->val)->arr);
}

hawk_ooi_t Hawk::Value::getArrayedSize () const
{
	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_ARR) return -1;
	return (hawk_ooi_t)HAWK_ARR_SIZE(((hawk_val_arr_t*)this->val)->arr);
}

hawk_ooi_t Hawk::Value::getArrayedCapa () const
{
	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_ARR) return -1;
	return (hawk_ooi_t)HAWK_ARR_CAPA(((hawk_val_arr_t*)this->val)->arr);
}

int Hawk::Value::setArrayedVal (hawk_ooi_t idx, hawk_val_t* v)
{
	if (HAWK_UNLIKELY(!this->run)) return -1;
	return this->setArrayedVal(this->run, idx, v);
}

int Hawk::Value::setArrayedVal (Run* r, hawk_ooi_t idx, hawk_val_t* v)
{
	HAWK_ASSERT(r != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_ARR)
	{
		// the previous value is not a arr.
		// a new arr value needs to be created first.
		hawk_val_t* arr = hawk_rtx_makearrval(r->rtx, -1); // TODO: can we change this to accept the initial value in the constructor
		if (arr == HAWK_NULL)
		{
			r->hawk->retrieveError(r);
			return -1;
		}

		hawk_rtx_refupval(r->rtx, arr);

		// update the arr with a given value
		if (hawk_rtx_setarrvalfld(r->rtx, arr, idx, v) == HAWK_NULL)
		{
			hawk_rtx_refdownval(r->rtx, arr);
			r->hawk->retrieveError(r);
			return -1;
		}

		// free the previous value
		if (this->run)
		{
			// if val is not nil, this->run can't be NULL
			hawk_rtx_refdownval(this->run->rtx, this->val);
		}

		this->run = r;
		this->val = arr;
	}
	else
	{
		HAWK_ASSERT(run != HAWK_NULL);

		// if the previous value is a arr, things are a bit simpler
		// however it needs to check if the runtime context matches
		// with the previous one.
		if (this->run != r)
		{
			// it can't span across multiple runtime contexts
			this->run->setError(HAWK_EINVAL);
			this->run->hawk->retrieveError(run);
			return -1;
		}

		// update the arr with a given value
		if (hawk_rtx_setarrvalfld(r->rtx, val, idx, v) == HAWK_NULL)
		{
			r->hawk->retrieveError(r);
			return -1;
		}
	}

	return 0;
}

bool Hawk::Value::isArrayed () const
{
	HAWK_ASSERT(val != HAWK_NULL);
	return HAWK_RTX_GETVALTYPE(this->run->rtx, val) == HAWK_VAL_ARR;
}


int Hawk::Value::getArrayed (hawk_ooi_t idx, Value* v) const
{
	HAWK_ASSERT(val != HAWK_NULL);

	// not a map. v is just nil. not an error
	if (HAWK_RTX_GETVALTYPE(this->run->rtx, val) != HAWK_VAL_ARR)
	{
		v->clear ();
		return 0;
	}

	// get the value from the map.
	hawk_val_t* fv = hawk_rtx_getarrvalfld(this->run->rtx, val, idx);

	// the key is not found. it is not an error. v is just nil
	if (fv == HAWK_NULL)
	{
		v->clear ();
		return 0;
	}

	// if v.set fails, it should return an error
	return v->setVal(this->run, fv);
}

///////////////////////////////////////////////////////////////////

hawk_ooi_t Hawk::Value::getIndexedLength () const
{
	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_MAP) return -1;
	return (hawk_ooi_t)HAWK_MAP_SIZE(((hawk_val_map_t*)this->val)->map);
}

hawk_ooi_t Hawk::Value::getIndexedSize () const
{
	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_MAP) return -1;
	return (hawk_ooi_t)HAWK_MAP_SIZE(((hawk_val_map_t*)this->val)->map);
}

hawk_ooi_t Hawk::Value::getIndexedCapa () const
{
	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_MAP) return -1;
	return (hawk_ooi_t)hawk_map_getcapa(((hawk_val_map_t*)this->val)->map);
}

int Hawk::Value::setIndexedVal (const Index& idx, hawk_val_t* v)
{
	if (HAWK_UNLIKELY(!this->run)) return -1;
	return this->setIndexedVal(this->run, idx, v);
}

int Hawk::Value::setIndexedVal (Run* r, const Index& idx, hawk_val_t* v)
{
	HAWK_ASSERT(r != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE(r->rtx, this->val) != HAWK_VAL_MAP)
	{
		// the previous value is not a map.
		// a new map value needs to be created first.
		hawk_val_t* map = hawk_rtx_makemapval(r->rtx);
		if (map == HAWK_NULL)
		{
			r->hawk->retrieveError(r);
			return -1;
		}

		hawk_rtx_refupval(r->rtx, map);

		// update the map with a given value
		if (hawk_rtx_setmapvalfld(r->rtx, map, idx.ptr, idx.len, v) == HAWK_NULL)
		{
			hawk_rtx_refdownval(r->rtx, map);
			r->hawk->retrieveError(r);
			return -1;
		}

		// free the previous value
		if (this->run)
		{
			// if val is not nil, this->run can't be NULL
			hawk_rtx_refdownval(this->run->rtx, this->val);
		}

		this->run = r;
		this->val = map;
	}
	else
	{
		HAWK_ASSERT(run != HAWK_NULL);

		// if the previous value is a map, things are a bit simpler
		// however it needs to check if the runtime context matches
		// with the previous one.
		if (this->run != r)
		{
			// it can't span across multiple runtime contexts
			this->run->setError(HAWK_EINVAL);
			this->run->hawk->retrieveError(run);
			return -1;
		}

		// update the map with a given value
		if (hawk_rtx_setmapvalfld(r->rtx, val, idx.ptr, idx.len, v) == HAWK_NULL)
		{
			r->hawk->retrieveError(r);
			return -1;
		}
	}

	return 0;
}

int Hawk::Value::setIndexedInt (const Index& idx, hawk_int_t v)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedInt (run, idx, v);
}

int Hawk::Value::setIndexedInt (Run* r, const Index& idx, hawk_int_t v)
{
	hawk_val_t* tmp = hawk_rtx_makeintval (r->rtx, v);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedFlt (const Index& idx, hawk_flt_t v)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedFlt(run, idx, v);
}

int Hawk::Value::setIndexedFlt (Run* r, const Index& idx, hawk_flt_t v)
{
	hawk_val_t* tmp = hawk_rtx_makefltval(r->rtx, v);
	if (tmp == HAWK_NULL)
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

///////////////////////////////////////////////////////////////////

int Hawk::Value::setIndexedStr (const Index& idx, const hawk_uch_t* str, hawk_oow_t len, bool numeric)
{
	if (HAWK_UNLIKELY(!this->run)) return -1; // NOTE: this->run isn't available. neither is this->run->hawk. unable to set an error code
	return this->setIndexedStr(this->run, idx, str, len, numeric);
}

int Hawk::Value::setIndexedStr (Run* r, const Index& idx, const hawk_uch_t* str, hawk_oow_t len, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithuchars(r->rtx, str, len):
	               hawk_rtx_makestrvalwithuchars(r->rtx, str, len);
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedStr (const Index& idx, const hawk_uch_t* str, bool numeric)
{
	if (run == HAWK_NULL) return -1; // NOTE: this->run isn't available. neither is this->run->hawk. unable to set an error code
	return this->setIndexedStr(run, idx, str, numeric);
}

int Hawk::Value::setIndexedStr (Run* r, const Index& idx, const hawk_uch_t* str, bool numeric)
{
	hawk_val_t* tmp;
	tmp = numeric? hawk_rtx_makenumorstrvalwithuchars(r->rtx, str, hawk_count_ucstr(str)):
	               hawk_rtx_makestrvalwithucstr(r->rtx, str);
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedStr (const Index& idx, const hawk_bch_t* str, hawk_oow_t len, bool numeric)
{
	if (HAWK_UNLIKELY(!this->run)) return -1; // NOTE: this->run isn't available. neither is this->run->hawk. unable to set an error code
	return this->setIndexedStr(this->run, idx, str, len, numeric);
}

int Hawk::Value::setIndexedStr (Run* r, const Index& idx, const hawk_bch_t* str, hawk_oow_t len, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithbchars(r->rtx, str, len):
	               hawk_rtx_makestrvalwithbchars(r->rtx, str, len);
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedStr (const Index& idx, const hawk_bch_t* str, bool numeric)
{
	if (HAWK_UNLIKELY(!this->run)) return -1; // NOTE: this->run isn't available. neither is this->run->hawk. unable to set an error code
	return this->setIndexedStr(run, idx, str, numeric);
}

int Hawk::Value::setIndexedStr (Run* r, const Index& idx, const hawk_bch_t* str, bool numeric)
{
	hawk_val_t* tmp;

	tmp = numeric? hawk_rtx_makenumorstrvalwithbchars(r->rtx, str, hawk_count_bcstr(str)):
	               hawk_rtx_makestrvalwithbcstr(r->rtx, str);
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

///////////////////////////////////////////////////////////////////

int Hawk::Value::setIndexedMbs (const Index& idx, const hawk_bch_t* str, hawk_oow_t len)
{
	if (HAWK_UNLIKELY(!this->run)) return -1; // NOTE: this->run isn't available. neither is this->run->hawk. unable to set an error code
	return this->setIndexedMbs(this->run, idx, str, len);
}

int Hawk::Value::setIndexedMbs (Run* r, const Index& idx, const hawk_bch_t* str, hawk_oow_t len)
{
	hawk_val_t* tmp;

	hawk_bcs_t oocs;
	oocs.ptr = (hawk_bch_t*)str;
	oocs.len = len;

	tmp = hawk_rtx_makembsvalwithbcs(r->rtx, &oocs);
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedMbs (const Index& idx, const hawk_bch_t* str)
{
	if (HAWK_UNLIKELY(!this->run)) return -1; // NOTE: this->run isn't available. neither is this->run->hawk. unable to set an error code
	return this->setIndexedMbs(run, idx, str);
}

int Hawk::Value::setIndexedMbs (Run* r, const Index& idx, const hawk_bch_t* str)
{
	hawk_val_t* tmp;
	tmp = hawk_rtx_makembsvalwithbchars(r->rtx, str, hawk_count_bcstr(str));
	if (HAWK_UNLIKELY(!tmp))
	{
		r->hawk->retrieveError(r);
		return -1;
	}

	hawk_rtx_refupval(r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval(r->rtx, tmp);

	return n;
}


bool Hawk::Value::isIndexed () const
{
	HAWK_ASSERT(val != HAWK_NULL);
	return HAWK_RTX_GETVALTYPE(this->run->rtx, val) == HAWK_VAL_MAP;
}

int Hawk::Value::getIndexed (const Index& idx, Value* v) const
{
	HAWK_ASSERT(val != HAWK_NULL);

	// not a map. v is just nil. not an error
	if (HAWK_RTX_GETVALTYPE(this->run->rtx, val) != HAWK_VAL_MAP)
	{
		v->clear ();
		return 0;
	}

	// get the value from the map.
	hawk_val_t* fv = hawk_rtx_getmapvalfld(this->run->rtx, val, (hawk_ooch_t*)idx.ptr, idx.len);

	// the key is not found. it is not an error. v is just nil
	if (fv == HAWK_NULL)
	{
		v->clear ();
		return 0;
	}

	// if v.set fails, it should return an error
	return v->setVal(this->run, fv);
}

Hawk::Value::IndexIterator Hawk::Value::getFirstIndex (Index* idx) const
{
	HAWK_ASSERT(this->val != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE(this->run->rtx, this->val) != HAWK_VAL_MAP) return IndexIterator::END;

	HAWK_ASSERT(this->run != HAWK_NULL);

	Hawk::Value::IndexIterator itr;
	hawk_val_map_itr_t* iptr;

	iptr = hawk_rtx_getfirstmapvalitr(this->run->rtx, this->val, &itr);
	if (iptr == HAWK_NULL) return IndexIterator::END; // no more key

	idx->set (HAWK_VAL_MAP_ITR_KEY(iptr));

	return itr;
}

Hawk::Value::IndexIterator Hawk::Value::getNextIndex (Index* idx, const IndexIterator& curitr) const
{
	HAWK_ASSERT(val != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE(this->run->rtx, val) != HAWK_VAL_MAP) return IndexIterator::END;

	HAWK_ASSERT(this->run != HAWK_NULL);

	Hawk::Value::IndexIterator itr(curitr);
	hawk_val_map_itr_t* iptr;

	iptr = hawk_rtx_getnextmapvalitr(this->run->rtx, this->val, &itr);
	if (iptr == HAWK_NULL) return IndexIterator::END; // no more key

	idx->set (HAWK_VAL_MAP_ITR_KEY(iptr));

	return itr;
}

//////////////////////////////////////////////////////////////////
// Hawk::Run
//////////////////////////////////////////////////////////////////

Hawk::Run::Run (Hawk* hawk): hawk(hawk), rtx (HAWK_NULL)
{
}

Hawk::Run::Run (Hawk* hawk, hawk_rtx_t* rtx): hawk(hawk), rtx (rtx)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
}

Hawk::Run::~Run ()
{
}

void Hawk::Run::halt () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	hawk_rtx_halt (this->rtx);
}

bool Hawk::Run::isHalt () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return !!hawk_rtx_ishalt(this->rtx);
}

hawk_errnum_t Hawk::Run::getErrorNumber () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return hawk_rtx_geterrnum(this->rtx);
}

hawk_loc_t Hawk::Run::getErrorLocation () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return *hawk_rtx_geterrloc(this->rtx);
}

const hawk_ooch_t* Hawk::Run::getErrorMessage () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return hawk_rtx_geterrmsg(this->rtx);
}

const hawk_uch_t* Hawk::Run::getErrorMessageU () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return hawk_rtx_geterrumsg(this->rtx);
}

const hawk_bch_t* Hawk::Run::getErrorMessageB () const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return hawk_rtx_geterrbmsg(this->rtx);
}

void Hawk::Run::setError (hawk_errnum_t code, const hawk_loc_t* loc)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	hawk_rtx_seterrnum(this->rtx, loc, code);
}

void Hawk::Run::formatError (hawk_errnum_t code, const hawk_loc_t* loc, const hawk_bch_t* fmt, ...)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	va_list ap;
	va_start (ap, fmt);
	hawk_rtx_seterrbvfmt (this->rtx, loc, code, fmt, ap);
	va_end (ap);
}

void Hawk::Run::formatError (hawk_errnum_t code, const hawk_loc_t* loc, const hawk_uch_t* fmt, ...)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	va_list ap;
	va_start (ap, fmt);
	hawk_rtx_seterruvfmt (this->rtx, loc, code, fmt, ap);
	va_end (ap);
}

int Hawk::Run::setGlobal (int id, hawk_int_t v)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);

	hawk_val_t* tmp = hawk_rtx_makeintval(this->rtx, v);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval(this->rtx, tmp);
	int n = hawk_rtx_setgbl(this->rtx, id, tmp);
	hawk_rtx_refdownval(this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, hawk_flt_t v)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);

	hawk_val_t* tmp = hawk_rtx_makefltval (this->rtx, v);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval(this->rtx, tmp);
	int n = hawk_rtx_setgbl(this->rtx, id, tmp);
	hawk_rtx_refdownval(this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const hawk_uch_t* ptr, hawk_oow_t len, bool mbs)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);

	hawk_val_t* tmp = mbs? hawk_rtx_makembsvalwithuchars(this->rtx, ptr, len):
	                       hawk_rtx_makestrvalwithuchars(this->rtx, ptr, len);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval(this->rtx, tmp);
	int n = hawk_rtx_setgbl(this->rtx, id, tmp);
	hawk_rtx_refdownval(this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const hawk_bch_t* ptr, hawk_oow_t len, bool mbs)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);

	hawk_val_t* tmp = mbs? hawk_rtx_makembsvalwithbchars(this->rtx, ptr, len):
	                       hawk_rtx_makestrvalwithbchars(this->rtx, ptr, len);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval(this->rtx, tmp);
	int n = hawk_rtx_setgbl(this->rtx, id, tmp);
	hawk_rtx_refdownval(this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const hawk_uch_t* ptr, bool mbs)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);

	hawk_val_t* tmp = mbs? hawk_rtx_makembsvalwithucstr(this->rtx, ptr):
	                       hawk_rtx_makestrvalwithucstr(this->rtx, ptr);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval(this->rtx, tmp);
	int n = hawk_rtx_setgbl(this->rtx, id, tmp);
	hawk_rtx_refdownval(this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const hawk_bch_t* ptr, bool mbs)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);

	hawk_val_t* tmp = mbs? hawk_rtx_makembsvalwithbcstr(this->rtx, ptr):
	                       hawk_rtx_makestrvalwithbcstr(this->rtx, ptr);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	hawk_rtx_refupval(this->rtx, tmp);
	int n = hawk_rtx_setgbl(this->rtx, id, tmp);
	hawk_rtx_refdownval(this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const Value& gbl)
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return hawk_rtx_setgbl(this->rtx, id, (hawk_val_t*)gbl);
}

int Hawk::Run::getGlobal (int id, Value& g) const
{
	HAWK_ASSERT(this->rtx != HAWK_NULL);
	return g.setVal ((Run*)this, hawk_rtx_getgbl (this->rtx, id));
}

//////////////////////////////////////////////////////////////////
// Hawk
//////////////////////////////////////////////////////////////////

Hawk::Hawk (Mmgr* mmgr):
	Mmged(mmgr), hawk(HAWK_NULL),
#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	functionMap(HAWK_NULL),
#else
	functionMap(this),
#endif
	source_reader(HAWK_NULL), source_writer(HAWK_NULL),
	pipe_handler(HAWK_NULL), file_handler(HAWK_NULL),
	console_handler(HAWK_NULL), runctx(this)

{
	HAWK_MEMSET (&this->errinf, 0, HAWK_SIZEOF(this->errinf));
	this->errinf.num = HAWK_ENOERR;

	this->_cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
}

Hawk::~Hawk()
{
	this->close();
}

const hawk_ooch_t* Hawk::getErrorString (hawk_errnum_t num) const
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	HAWK_ASSERT(this->dflerrstr != HAWK_NULL);
	return this->dflerrstr(num);
}

const hawk_ooch_t* Hawk::xerrstr (hawk_t* a, hawk_errnum_t num)
{
	Hawk* hawk = *(Hawk**)GET_XTN(a);
	return hawk->getErrorString(num);
}

hawk_errnum_t Hawk::getErrorNumber () const
{
	return this->errinf.num;
}

hawk_loc_t Hawk::getErrorLocation () const
{
	return this->errinf.loc;
}

const hawk_ooch_t* Hawk::getErrorLocationFile () const
{
	return this->errinf.loc.file;
}

const hawk_uch_t* Hawk::getErrorLocationFileU () const
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

const hawk_bch_t* Hawk::getErrorLocationFileB () const
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

const hawk_ooch_t* Hawk::getErrorMessage () const
{
	return this->errinf.msg;
}

const hawk_uch_t* Hawk::getErrorMessageU () const
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

const hawk_bch_t* Hawk::getErrorMessageB () const
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

void Hawk::setError (hawk_errnum_t code, const hawk_loc_t* loc)
{
	if (this->hawk)
	{
		hawk_seterrnum(this->hawk, loc, code);
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

void Hawk::formatError (hawk_errnum_t code, const hawk_loc_t* loc, const hawk_bch_t* fmt, ...)
{
	if (this->hawk)
	{
		va_list ap;
		va_start(ap, fmt);
		hawk_seterrbvfmt(this->hawk, loc, code, fmt, ap);
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

void Hawk::formatError (hawk_errnum_t code, const hawk_loc_t* loc, const hawk_uch_t* fmt, ...)
{
	if  (this->hawk)
	{
		va_list ap;
		va_start(ap, fmt);
		hawk_seterruvfmt(this->hawk, loc, code, fmt, ap);
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

void Hawk::clearError ()
{
	HAWK_MEMSET(&this->errinf, 0, HAWK_SIZEOF(this->errinf));
	this->errinf.num = HAWK_ENOERR;
}

void Hawk::retrieveError()
{
	if (this->hawk == HAWK_NULL)
	{
		this->clearError();
	}
	else
	{
		hawk_geterrinf(this->hawk, &this->errinf);
	}
}

void Hawk::retrieveError(Run* run)
{
	HAWK_ASSERT(run != HAWK_NULL);
	if (run->rtx == HAWK_NULL) return;
	hawk_rtx_geterrinf(run->rtx, &this->errinf);
}

static void fini_xtn (hawk_t* hawk, void* ctx)
{
	xtn_t* xtn = GET_XTN(hawk);
	xtn->hawk->uponClosing();
}

static void clear_xtn (hawk_t* hawk, void* ctx)
{
	xtn_t* xtn = GET_XTN(hawk);
	xtn->hawk->uponClearing();
}

int Hawk::open ()
{
	HAWK_ASSERT(this->hawk == HAWK_NULL);
#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	HAWK_ASSERT(this->functionMap == HAWK_NULL);
#endif

	hawk_prm_t prm;

	HAWK_MEMSET (&prm, 0, HAWK_SIZEOF(prm));
	prm.math.pow  = &Hawk::pow;
	prm.math.mod  = &Hawk::mod;
	prm.modopen   = &Hawk::modopen;
	prm.modclose  = &Hawk::modclose;
	prm.modgetsym = &Hawk::modgetsym;

	this->hawk = hawk_open(this->getMmgr(), HAWK_SIZEOF(xtn_t), this->getCmgr(), &prm, &this->errinf);
	if (HAWK_UNLIKELY(!this->hawk)) return -1;

	this->hawk->instsize_ += HAWK_SIZEOF(xtn_t);

	// associate this Hawk object with the underlying hawk object
	xtn_t* xtn = (xtn_t*)GET_XTN(this->hawk);
	xtn->hawk = this;
	xtn->ecb.close = fini_xtn;
	xtn->ecb.clear = clear_xtn;

	this->dflerrstr = hawk_geterrstr(this->hawk);
// TODO: revive this too when hawk_seterrstr is revived()
//	hawk_seterrstr (this->hawk, Hawk::xerrstr);

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	this->functionMap = hawk_htb_open(
		hawk_getgem(this->hawk), HAWK_SIZEOF(this), 512, 70,
		HAWK_SIZEOF(hawk_ooch_t), 1
	);
	if (this->functionMap == HAWK_NULL)
	{
		hawk_close(this->hawk);
		this->hawk = HAWK_NULL;

		this->setError(HAWK_ENOMEM);
		return -1;
	}

	*(Hawk**)hawk_htb_getxtn(this->functionMap) = this;

	static hawk_htb_style_t style =
	{
		{
			HAWK_HTB_COPIER_DEFAULT, // keep the key pointer only
			HAWK_HTB_COPIER_INLINE   // copy the value into the pair
		},
		{
			HAWK_HTB_FREEER_DEFAULT, // free nothing
			HAWK_HTB_FREEER_DEFAULT  // free nothing
		},
		HAWK_HTB_COMPER_DEFAULT,
		HAWK_HTB_KEEPER_DEFAULT,
		HAWK_HTB_SIZER_DEFAULT,
		HAWK_HTB_HASHER_DEFAULT
	};
	hawk_htb_setstyle (this->functionMap, &style);

#endif

	// push the call back after everything else is ok.
	hawk_pushecb(this->hawk, &xtn->ecb);
	return 0;
}

void Hawk::close ()
{
	this->fini_runctx();
	this->clearArguments();

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	if (this->functionMap)
	{
		hawk_htb_close(this->functionMap);
		this->functionMap = HAWK_NULL;
	}
#else
	this->functionMap.clear();
#endif

	if (this->hawk)
	{
		hawk_close(this->hawk);
		this->hawk = HAWK_NULL;
	}

	this->clearError();
}

hawk_cmgr_t* Hawk::getCmgr () const
{
	return this->hawk? hawk_getcmgr(this->hawk): this->_cmgr;
}

void Hawk::setCmgr (hawk_cmgr_t* cmgr)
{
	if (this->hawk) hawk_setcmgr(this->hawk, cmgr);
	this->_cmgr = cmgr;
}

void Hawk::uponClosing ()
{
	// nothing to do
}

void Hawk::uponClearing ()
{
	// nothing to do
}

Hawk::Run* Hawk::parse (Source& in, Source& out)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);

	if (&in == &Source::NONE)
	{
		this->setError(HAWK_EINVAL);
		return HAWK_NULL;
	}

	this->fini_runctx();

	source_reader = &in;
	source_writer = (&out == &Source::NONE)? HAWK_NULL: &out;

	hawk_sio_cbs_t sio;
	sio.in = Hawk::readSource;
	sio.out = (source_writer == HAWK_NULL)? HAWK_NULL: Hawk::writeSource;

	int n = hawk_parse(this->hawk, &sio);
	if (n <= -1)
	{
		this->retrieveError();
		return HAWK_NULL;
	}

	if (this->init_runctx() <= -1) return HAWK_NULL;
	return &this->runctx;
}

Hawk::Run* Hawk::resetRunContext ()
{
	if (this->runctx.rtx)
	{
		this->fini_runctx();
		if (this->init_runctx() <= -1) return HAWK_NULL;
		return &this->runctx;
	}
	else return HAWK_NULL;
}

int Hawk::loop (Value* ret)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	HAWK_ASSERT(this->runctx.rtx != HAWK_NULL);

	hawk_val_t* rv = hawk_rtx_loop (this->runctx.rtx);
	if (rv == HAWK_NULL)
	{
		this->retrieveError(&this->runctx);
		return -1;
	}

	ret->setVal(&this->runctx, rv);
	hawk_rtx_refdownval(this->runctx.rtx, rv);

	return 0;
}

int Hawk::call (const hawk_bch_t* name, Value* ret, const Value* args, hawk_oow_t nargs)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	HAWK_ASSERT(this->runctx.rtx != HAWK_NULL);

	hawk_val_t* buf[16];
	hawk_val_t** ptr = HAWK_NULL;

	if (args != HAWK_NULL)
	{
		if (nargs <= HAWK_COUNTOF(buf)) ptr = buf;
		else
		{
			ptr = (hawk_val_t**)hawk_allocmem(this->hawk, HAWK_SIZEOF(hawk_val_t*) * nargs);
			if (ptr == HAWK_NULL)
			{
				this->runctx.setError (HAWK_ENOMEM);
				this->retrieveError(&this->runctx);
				return -1;
			}
		}

		for (hawk_oow_t i = 0; i < nargs; i++) ptr[i] = (hawk_val_t*)args[i];
	}

	hawk_val_t* rv = hawk_rtx_callwithbcstr(this->runctx.rtx, name, ptr, nargs);

	if (ptr != HAWK_NULL && ptr != buf) hawk_freemem(this->hawk, ptr);

	if (rv == HAWK_NULL)
	{
		this->retrieveError(&this->runctx);
		return -1;
	}

	ret->setVal(&this->runctx, rv);

	hawk_rtx_refdownval(this->runctx.rtx, rv);
	return 0;
}

int Hawk::call (const hawk_uch_t* name, Value* ret, const Value* args, hawk_oow_t nargs)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	HAWK_ASSERT(this->runctx.rtx != HAWK_NULL);

	hawk_val_t* buf[16];
	hawk_val_t** ptr = HAWK_NULL;

	if (args != HAWK_NULL)
	{
		if (nargs <= HAWK_COUNTOF(buf)) ptr = buf;
		else
		{
			ptr = (hawk_val_t**)hawk_allocmem(this->hawk, HAWK_SIZEOF(hawk_val_t*) * nargs);
			if (ptr == HAWK_NULL)
			{
				this->runctx.setError(HAWK_ENOMEM);
				this->retrieveError(&this->runctx);
				return -1;
			}
		}

		for (hawk_oow_t i = 0; i < nargs; i++) ptr[i] = (hawk_val_t*)args[i];
	}

	hawk_val_t* rv = hawk_rtx_callwithucstr(this->runctx.rtx, name, ptr, nargs);

	if (ptr != HAWK_NULL && ptr != buf) hawk_freemem(this->hawk, ptr);

	if (rv == HAWK_NULL)
	{
		this->retrieveError(&this->runctx);
		return -1;
	}

	ret->setVal(&this->runctx, rv);

	hawk_rtx_refdownval(this->runctx.rtx, rv);
	return 0;
}

int Hawk::exec (Value* ret, const Value* args, hawk_oow_t nargs)
{
	int n = (this->runctx.rtx->hawk->parse.pragma.entry[0] != '\0')?
		this->call(this->runctx.rtx->hawk->parse.pragma.entry, ret, args, nargs): this->loop(ret);

#if defined(HAWK_ENABLE_GC)
        /* i assume this function is a usual hawk program starter.
         * call garbage collection after a whole program finishes */
        hawk_rtx_gc(this->runctx.rtx, HAWK_RTX_GC_GEN_FULL);
#endif

	return n;
}

void Hawk::halt ()
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	hawk_haltall (this->hawk);
}

int Hawk::init_runctx ()
{
	if (this->runctx.rtx) return 0;

	hawk_rio_cbs_t rio;

	rio.pipe    = pipeHandler;
	rio.file    = fileHandler;
	rio.console = consoleHandler;

	hawk_rtx_t* rtx = hawk_rtx_open(this->hawk, HAWK_SIZEOF(rxtn_t), &rio);
	if (HAWK_UNLIKELY(!rtx))
	{
		this->retrieveError();
		return -1;
	}

	rtx->instsize_ += HAWK_SIZEOF(rxtn_t);
	this->runctx.rtx = rtx;

	rxtn_t* rxtn = GET_RXTN(rtx);
	rxtn->run = &this->runctx;

	return 0;
}

void Hawk::fini_runctx ()
{
	if (this->runctx.rtx)
	{
		hawk_rtx_close(this->runctx.rtx);
		this->runctx.rtx = HAWK_NULL;
	}
}

int Hawk::getTrait () const
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int val;
	hawk_getopt(this->hawk, HAWK_OPT_TRAIT, &val);
	return val;
}

void Hawk::setTrait (int trait)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	hawk_setopt (this->hawk, HAWK_OPT_TRAIT, &trait);
}

hawk_oow_t Hawk::getMaxDepth (depth_t id) const
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);

	hawk_oow_t depth;
	hawk_getopt(this->hawk, (hawk_opt_t)id, &depth);
	return depth;
}

void Hawk::setMaxDepth (depth_t id, hawk_oow_t depth)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	hawk_setopt (this->hawk, (hawk_opt_t)id, &depth);
}

int Hawk::setIncludeDirs (const hawk_uch_t* dirs)
{
#if defined(HAWK_OOCH_IS_UCH)
	return hawk_setopt(this->hawk, HAWK_OPT_INCLUDEDIRS, dirs);
#else
	hawk_ooch_t* tmp;
	tmp = hawk_duputobcstr(hawk, dirs, HAWK_NULL);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	int n = hawk_setopt(hawk, HAWK_OPT_INCLUDEDIRS, tmp);
	hawk_freemem(hawk, tmp);
	return n;
#endif
}

int Hawk::setIncludeDirs (const hawk_bch_t* dirs)
{
#if defined(HAWK_OOCH_IS_UCH)
	hawk_ooch_t* tmp;
	tmp = hawk_dupbtoucstr(hawk, dirs, HAWK_NULL, 1);
	if (HAWK_UNLIKELY(!tmp)) return -1;

	int n = hawk_setopt(hawk, HAWK_OPT_INCLUDEDIRS, tmp);
	hawk_freemem(hawk, tmp);
	return n;
#else
	return hawk_setopt(this->hawk, HAWK_OPT_INCLUDEDIRS, dirs);
#endif
}

int Hawk::dispatch_function (Run* run, const hawk_fnc_info_t* fi)
{
	bool has_ref_arg = false;

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	hawk_htb_pair_t* pair;
	pair = hawk_htb_search(this->functionMap, fi->name.ptr, fi->name.len);
	if (pair == HAWK_NULL)
	{
		run->formatError (HAWK_EFUNNF, HAWK_NULL, HAWK_T("function '%.*js' not defined"), &fi->name.len, &fi->name.ptr);
		return -1;
	}

	FunctionHandler handler;
	handler = *(FunctionHandler*)HAWK_HTB_VPTR(pair);
#else
	FunctionMap::Pair* pair = this->functionMap.search(Cstr(fi->name.ptr, fi->name.len));
	if (pair == HAWK_NULL)
	{
		run->formatError (HAWK_EFUNNF, HAWK_NULL, HAWK_T("function '%.*js' not defined"), &fi->name.len, &fi->name.ptr);
		return -1;
	}

	FunctionHandler handler;
	handler = pair->value;
#endif

	hawk_oow_t i, nargs = hawk_rtx_getnargs(run->rtx);

	Value buf[16];
	Value* args;
	if (nargs <= HAWK_COUNTOF(buf)) args = buf;
	else
	{
	#if defined(AWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW)
		args = new(run) Value[nargs];
		if (args == HAWK_NULL)
		{
			run->setError(HAWK_ENOMEM);
			return -1;
		}
	#else
		try
		{
			//args = (Value*)::operator new (HAWK_SIZEOF(Value) * nargs, this->getMmgr());
			args = (Value*)this->getMmgr()->allocate(HAWK_SIZEOF(Value) * nargs);
		}
		catch (...) { args = HAWK_NULL; }
		if (args == HAWK_NULL)
		{
			run->setError(HAWK_ENOMEM);
			return -1;
		}
		for (i = 0; i < nargs; i++)
		{
			// call the default constructor on the space allocated above.
			// no exception handling is implemented here as i know
			// that Value::Value() doesn't throw an exception
			new((HAWK::Mmgr*)HAWK_NULL, (void*)&args[i]) Value ();
		}
	#endif
	}

	for (i = 0; i < nargs; i++)
	{
		int xx;
		hawk_val_t* v = hawk_rtx_getarg (run->rtx, i);

		if (HAWK_RTX_GETVALTYPE (run->rtx, v) == HAWK_VAL_REF)
		{
			hawk_val_ref_t* ref = (hawk_val_ref_t*)v;

			switch (ref->id)
			{
				case hawk_val_ref_t::HAWK_VAL_REF_POS:
				{
					hawk_oow_t idx = (hawk_oow_t)ref->adr;

					if (idx == 0)
					{
						xx = args[i].setStr(run, HAWK_OOECS_PTR(&run->rtx->inrec.line), HAWK_OOECS_LEN(&run->rtx->inrec.line));
					}
					else if (idx <= run->rtx->inrec.nflds)
					{
						xx = args[i].setStr (run,
							run->rtx->inrec.flds[idx-1].ptr,
							run->rtx->inrec.flds[idx-1].len);
					}
					else
					{
						xx = args[i].setStr (run, HAWK_T(""), (hawk_oow_t)0);
					}
					break;
				}

				case hawk_val_ref_t::HAWK_VAL_REF_GBL:
				{
					hawk_oow_t idx = (hawk_oow_t)ref->adr;
					hawk_val_t* val = (hawk_val_t*)HAWK_RTX_STACK_GBL(run->rtx, idx);
					xx = args[i].setVal (run, val);
					break;
				}

				default:
					xx = args[i].setVal (run, *(ref->adr));
					break;
			}
			has_ref_arg = true;
		}
		else
		{
			xx = args[i].setVal (run, v);
		}

		if (xx <= -1)
		{
			run->setError(HAWK_ENOMEM);
			if (args != buf) delete[] args;
			return -1;
		}
	}

	Value ret (run);

	int n;

	try { n = (this->*handler) (*run, ret, args, nargs, fi); }
	catch (...) { n = -1; }

	if (n >= 0 && has_ref_arg)
	{
		for (i = 0; i < nargs; i++)
		{
			// Do NOT change the run field from function handler
			HAWK_ASSERT(args[i].run == run);

			hawk_val_t* v = hawk_rtx_getarg(run->rtx, i);
			if (HAWK_RTX_GETVALTYPE(run->rtx, v) == HAWK_VAL_REF)
			{
				if (hawk_rtx_setrefval(run->rtx, (hawk_val_ref_t*)v, args[i].toVal()) <= -1)
				{
					n = -1;
					break;
				}
			}
		}
	}

	if (args != buf)
	{
	#if defined(AWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW)
		delete[] args;
	#else
		for (i = nargs; i > 0; )
		{
			--i;
			args[i].Value::~Value ();
		}

		//::operator delete (args, this->getMmgr());
		this->getMmgr()->dispose (args);
	#endif
	}

	if (n <= -1)
	{
		/* this is really the handler error. the underlying engine
		 * will take care of the error code. */
		return -1;
	}

	hawk_rtx_setretval (run->rtx, ret.toVal());
	return 0;
}

int Hawk::xstrs_t::add (hawk_t* hawk, const hawk_uch_t* arg, hawk_oow_t len)
{
	if (this->len >= this->capa)
	{
		hawk_oocs_t* ptr;
		hawk_oow_t capa = this->capa;

		capa += 64;
		ptr = (hawk_oocs_t*)hawk_reallocmem(hawk, this->ptr, HAWK_SIZEOF(hawk_oocs_t)*(capa + 1));
		if (HAWK_UNLIKELY(!ptr)) return -1;

		this->ptr = ptr;
		this->capa = capa;
	}


#if defined(HAWK_OOCH_IS_UCH)
	this->ptr[this->len].len = len;
	this->ptr[this->len].ptr = hawk_dupuchars(hawk, arg, len);
#else
	this->ptr[this->len].ptr = hawk_duputobchars(hawk, arg, len, &this->ptr[this->len].len);
#endif
	if (this->ptr[this->len].ptr == HAWK_NULL) return -1;

	this->len++;
	this->ptr[this->len].len = 0;
	this->ptr[this->len].ptr = HAWK_NULL;

	return 0;
}

int Hawk::xstrs_t::add (hawk_t* hawk, const hawk_bch_t* arg, hawk_oow_t len)
{
	if (this->len >= this->capa)
	{
		hawk_oocs_t* ptr;
		hawk_oow_t capa = this->capa;

		capa += 64;
		ptr = (hawk_oocs_t*)hawk_reallocmem(hawk, this->ptr, HAWK_SIZEOF(hawk_oocs_t)*(capa + 1));
		if (HAWK_UNLIKELY(!ptr)) return -1;

		this->ptr = ptr;
		this->capa = capa;
	}

#if defined(HAWK_OOCH_IS_UCH)
	this->ptr[this->len].ptr = hawk_dupbtouchars(hawk, arg, len, &this->ptr[this->len].len, 0);
#else
	this->ptr[this->len].len = len;
	this->ptr[this->len].ptr = hawk_dupbchars(hawk, arg, len);
#endif
	if (this->ptr[this->len].ptr == HAWK_NULL) return -1;

	this->len++;
	this->ptr[this->len].len = 0;
	this->ptr[this->len].ptr = HAWK_NULL;

	return 0;
}

void Hawk::xstrs_t::clear (hawk_t* hawk)
{
	if (this->ptr != HAWK_NULL)
	{
		while (this->len > 0) hawk_freemem(hawk, this->ptr[--this->len].ptr);

		hawk_freemem(hawk, this->ptr);
		this->ptr = HAWK_NULL;
		this->capa = 0;
	}
}

int Hawk::addArgument (const hawk_uch_t* arg, hawk_oow_t len)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = this->runarg.add(this->hawk, arg, len);
	if (n <= -1) this->setError(HAWK_ENOMEM);
	return n;
}

int Hawk::addArgument (const hawk_uch_t* arg)
{
	return this->addArgument(arg, hawk_count_ucstr(arg));
}

int Hawk::addArgument (const hawk_bch_t* arg, hawk_oow_t len)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = this->runarg.add(this->hawk, arg, len);
	if (n <= -1) this->setError(HAWK_ENOMEM);
	return n;
}

int Hawk::addArgument (const hawk_bch_t* arg)
{
	return this->addArgument(arg, hawk_count_bcstr(arg));
}

void Hawk::clearArguments ()
{
	this->runarg.clear (this->hawk);
}

int Hawk::addGlobal(const hawk_bch_t* name)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = hawk_addgblwithbcstr(this->hawk, name);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::addGlobal(const hawk_uch_t* name)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = hawk_addgblwithucstr(this->hawk, name);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::deleteGlobal (const hawk_bch_t* name)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = hawk_delgblwithbcstr(this->hawk, name);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::deleteGlobal (const hawk_uch_t* name)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = hawk_delgblwithucstr(this->hawk, name);
	if (n <= -1) this->retrieveError();
	return n;
}


int Hawk::findGlobal (const hawk_bch_t* name, bool inc_builtins)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = hawk_findgblwithbcstr(this->hawk, name, inc_builtins);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::findGlobal (const hawk_uch_t* name, bool inc_builtins)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	int n = hawk_findgblwithucstr(this->hawk, name, inc_builtins);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::setGlobal (int id, const Value& v)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	HAWK_ASSERT(this->runctx.rtx != HAWK_NULL);

	if (v.run != &this->runctx)
	{
		this->setError(HAWK_EINVAL);
		return -1;
	}

	int n = this->runctx.setGlobal(id, v);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::getGlobal (int id, Value& v)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);
	HAWK_ASSERT(runctx.rtx != HAWK_NULL);

	int n = runctx.getGlobal (id, v);
	if (n <= -1) this->retrieveError();
	return n;
}

int Hawk::addFunction (
	const hawk_bch_t* name, hawk_oow_t minArgs, hawk_oow_t maxArgs,
	const hawk_bch_t* argSpec, FunctionHandler handler, int validOpts)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);

	hawk_fnc_bspec_t spec;

	HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
	spec.arg.min = minArgs;
	spec.arg.max = maxArgs;
	spec.arg.spec = argSpec;
	spec.impl = this->functionHandler;
	spec.trait = validOpts;

	hawk_fnc_t* fnc = hawk_addfncwithbcstr(this->hawk, name, &spec);
	if (fnc == HAWK_NULL)
	{
		this->retrieveError();
		return -1;
	}

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	// handler is a pointer to a member function.
	// sizeof(handler) is likely to be greater than sizeof(void*)
	// copy the handler pointer into the table.
	//
	// the function name exists in the underlying function table.
	// use the pointer to the name to maintain the hash table.
	hawk_htb_pair_t* pair = hawk_htb_upsert(this->functionMap, (hawk_ooch_t*)fnc->name.ptr, fnc->name.len, &handler, HAWK_SIZEOF(handler));
	if (!pair)
	{
		hawk_delfncwithbcstr (this->hawk, name);
		this->retrieveError();
		return -1;
	}
#else
	FunctionMap::Pair* pair;
	try { pair = this->functionMap.upsert(Cstr(fnc->name.ptr, fnc->name.len), handler); }
	catch (...) { pair = HAWK_NULL; }
	if (!pair)
	{
		hawk_delfncwithbcstr (this->hawk, name);
		this->setError(HAWK_ENOMEM);
		return -1;
	}
#endif

	return 0;
}

int Hawk::addFunction (
	const hawk_uch_t* name, hawk_oow_t minArgs, hawk_oow_t maxArgs,
	const hawk_uch_t* argSpec, FunctionHandler handler, int validOpts)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);

	hawk_fnc_uspec_t spec;

	HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
	spec.arg.min = minArgs;
	spec.arg.max = maxArgs;
	spec.arg.spec = argSpec;
	spec.impl = this->functionHandler;
	spec.trait = validOpts;

	hawk_fnc_t* fnc = hawk_addfncwithucstr(this->hawk, name, &spec);
	if (HAWK_UNLIKELY(!fnc))
	{
		this->retrieveError();
		return -1;
	}

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	// handler is a pointer to a member function.
	// sizeof(handler) is likely to be greater than sizeof(void*)
	// copy the handler pointer into the table.
	//
	// the function name exists in the underlying function table.
	// use the pointer to the name to maintain the hash table.
	hawk_htb_pair_t* pair = hawk_htb_upsert(this->functionMap, (hawk_ooch_t*)fnc->name.ptr, fnc->name.len, &handler, HAWK_SIZEOF(handler));
	if (HAWK_UNLIKELY(!pair))
	{
		hawk_delfncwithucstr (this->hawk, name);
		this->retrieveError();
		return -1;
	}
#else
	FunctionMap::Pair* pair;
	try { pair = this->functionMap.upsert(Cstr(fnc->name.ptr, fnc->name.len), handler); }
	catch (...) { pair = HAWK_NULL; }
	if (HAWK_UNLIKELY(!pair))
	{
		hawk_delfncwithucstr (this->hawk, name);
		this->setError(HAWK_ENOMEM);
		return -1;
	}
#endif

	return 0;
}

int Hawk::deleteFunction (const hawk_ooch_t* name)
{
	HAWK_ASSERT(this->hawk != HAWK_NULL);

	int n = hawk_delfnc(this->hawk, name);
	if (n == 0)
	{
#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
		hawk_htb_delete (this->functionMap, name, hawk_count_oocstr(name));
#else
		this->functionMap.remove (Cstr(name));
#endif
	}
	else this->retrieveError();

	return n;
}

hawk_ooi_t Hawk::readSource (hawk_t* hawk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	xtn_t* xtn = GET_XTN(hawk);
	Source::Data sdat(xtn->hawk, Source::READ, arg);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			return xtn->hawk->source_reader->open(sdat);
		case HAWK_SIO_CMD_CLOSE:
			return xtn->hawk->source_reader->close(sdat);
		case HAWK_SIO_CMD_READ:
			return xtn->hawk->source_reader->read(sdat, data, count);
		default:
			hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}
}

hawk_ooi_t Hawk::writeSource (hawk_t* hawk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	xtn_t* xtn = GET_XTN(hawk);
	Source::Data sdat (xtn->hawk, Source::WRITE, arg);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			return xtn->hawk->source_writer->open (sdat);
		case HAWK_SIO_CMD_CLOSE:
			return xtn->hawk->source_writer->close(sdat);
		case HAWK_SIO_CMD_WRITE:
			return xtn->hawk->source_writer->write (sdat, data, count);
		default:
			hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}
}

hawk_ooi_t Hawk::pipeHandler (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t count)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	Hawk* hawk = rxtn->run->hawk;

	HAWK_ASSERT((riod->type & 0xFF) == HAWK_RIO_PIPE);

	Pipe pipe (rxtn->run, riod);

	try
	{
		if (hawk->pipe_handler)
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return hawk->pipe_handler->open(pipe);

				case HAWK_RIO_CMD_CLOSE:
					return hawk->pipe_handler->close(pipe);

				case HAWK_RIO_CMD_READ:
					return hawk->pipe_handler->read(pipe, (hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_READ_BYTES:
					return hawk->pipe_handler->readBytes(pipe, (hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_WRITE:
					return hawk->pipe_handler->write(pipe, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return hawk->pipe_handler->writeBytes(pipe, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return hawk->pipe_handler->flush(pipe);

				default:
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}
		}
		else
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return hawk->openPipe(pipe);

				case HAWK_RIO_CMD_CLOSE:
					return hawk->closePipe(pipe);

				case HAWK_RIO_CMD_READ:
					return hawk->readPipe(pipe, (hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_READ_BYTES:
					return hawk->readPipeBytes(pipe, (hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_WRITE:
					return hawk->writePipe(pipe, (const hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_WRITE_BYTES:
					return hawk->writePipeBytes(pipe, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return hawk->flushPipe(pipe);

				default:
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}
		}
	}
	catch (...)
	{
		return -1;
	}
}

hawk_ooi_t Hawk::fileHandler (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t count)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	Hawk* hawk = rxtn->run->hawk;

	HAWK_ASSERT((riod->type & 0xFF) == HAWK_RIO_FILE);

	File file (rxtn->run, riod);

	try
	{
		if (hawk->file_handler)
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return hawk->file_handler->open(file);

				case HAWK_RIO_CMD_CLOSE:
					return hawk->file_handler->close(file);

				case HAWK_RIO_CMD_READ:
					return hawk->file_handler->read(file, (hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_READ_BYTES:
					return hawk->file_handler->readBytes(file, (hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_WRITE:
					return hawk->file_handler->write(file, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return hawk->file_handler->writeBytes(file, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return hawk->file_handler->flush(file);

				default:
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}
		}
		else
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return hawk->openFile(file);

				case HAWK_RIO_CMD_CLOSE:
					return hawk->closeFile(file);

				case HAWK_RIO_CMD_READ:
					return hawk->readFile(file, (hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_READ_BYTES:
					return hawk->readFileBytes(file, (hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_WRITE:
					return hawk->writeFile(file, (const hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_WRITE_BYTES:
					return hawk->writeFileBytes(file, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return hawk->flushFile(file);

				default:
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}
		}
	}
	catch (...)
	{
		return -1;
	}
}

hawk_ooi_t Hawk::consoleHandler (hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* riod, void* data, hawk_oow_t count)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	Hawk* hawk = rxtn->run->hawk;

	HAWK_ASSERT((riod->type & 0xFF) == HAWK_RIO_CONSOLE);

	Console console(rxtn->run, riod);

	try
	{
		if (hawk->console_handler)
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return hawk->console_handler->open(console);

				case HAWK_RIO_CMD_CLOSE:
					return hawk->console_handler->close(console);

				case HAWK_RIO_CMD_READ:
					return hawk->console_handler->read(console, (hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_READ_BYTES:
					return hawk->console_handler->readBytes(console, (hawk_bch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return hawk->console_handler->write(console, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return hawk->console_handler->writeBytes(console, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return hawk->console_handler->flush(console);

				case HAWK_RIO_CMD_NEXT:
					return hawk->console_handler->next(console);

				default:
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}
		}
		else
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return hawk->openConsole(console);

				case HAWK_RIO_CMD_CLOSE:
					return hawk->closeConsole(console);

				case HAWK_RIO_CMD_READ:
					return hawk->readConsole(console, (hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_READ_BYTES:
					return hawk->readConsoleBytes(console, (hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_WRITE:
					return hawk->writeConsole(console, (const hawk_ooch_t*)data, count);

				case HAWK_RIO_CMD_WRITE_BYTES:
					return hawk->writeConsoleBytes(console, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return hawk->flushConsole(console);

				case HAWK_RIO_CMD_NEXT:
					return hawk->nextConsole(console);

				default:
					hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINTERN);
					return -1;
			}
		}
	}
	catch (...)
	{
		return -1;
	}
}

int Hawk::openPipe (Pipe& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::closePipe (Pipe& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::readPipe (Pipe& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::readPipeBytes (Pipe& io, hawk_bch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::writePipe (Pipe& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::writePipeBytes (Pipe& io, const hawk_bch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::flushPipe (Pipe& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::openFile  (File& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::closeFile (File& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::readFile (File& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::readFileBytes (File& io, hawk_bch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::writeFile (File& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::writeFileBytes (File& io, const hawk_bch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::flushFile (File& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::openConsole  (Console& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::closeConsole (Console& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::readConsole (Console& io, hawk_ooch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::readConsoleBytes (Console& io, hawk_bch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::writeConsole (Console& io, const hawk_ooch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

hawk_ooi_t Hawk::writeConsoleBytes (Console& io, const hawk_bch_t* buf, hawk_oow_t len)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::flushConsole (Console& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::nextConsole (Console& io)
{
	((Run*)io)->setError(HAWK_ENOIMPL);
	return -1;
}

int Hawk::functionHandler (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	return rxtn->run->hawk->dispatch_function(rxtn->run, fi);
}

hawk_flt_t Hawk::pow (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y)
{
	xtn_t* xtn = GET_XTN(hawk);
	return xtn->hawk->pow(x, y);
}

hawk_flt_t Hawk::mod (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y)
{
	xtn_t* xtn = GET_XTN(hawk);
	return xtn->hawk->mod(x, y);
}

void* Hawk::modopen (hawk_t* hawk, const hawk_mod_spec_t* spec)
{
	xtn_t* xtn = GET_XTN(hawk);
	return xtn->hawk->modopen(spec);
}

void Hawk::modclose (hawk_t* hawk, void* handle)
{
	xtn_t* xtn = GET_XTN(hawk);
	xtn->hawk->modclose(handle);
}

void* Hawk::modgetsym (hawk_t* hawk, void* handle, const hawk_ooch_t* name)
{
	xtn_t* xtn = GET_XTN(hawk);
	return xtn->hawk->modgetsym(handle, name);
}
/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////



void* operator new (hawk_oow_t size, HAWK::Mmgr* mmgr) /*HAWK_CXX_THREXCEPT1(HAWK::Mmgr::MemoryError)*/
{
	return mmgr->allocate(size);
}

#if defined(HAWK_CXX_NO_OPERATOR_DELETE_OVERLOADING)
void hawk_operator_delete (void* ptr, HAWK::Mmgr* mmgr)
#else
void operator delete (void* ptr, HAWK::Mmgr* mmgr)
#endif
{
	mmgr->dispose (ptr);
}

void* operator new (hawk_oow_t size, HAWK::Mmgr* mmgr, void* existing_ptr) /*HAWK_CXX_THREXCEPT1(HAWK::Mmgr::MemoryError)*/
{
	// mmgr unused. i put it in the parameter list to make this function
	// less conflicting with the stock ::operator new() that doesn't allocate.
	return existing_ptr;
}

#if 0
void* operator new[] (hawk_oow_t size, HAWK::Mmgr* mmgr)
{
	return mmgr->allocate(size);
}

void operator delete[] (void* ptr, HAWK::Mmgr* mmgr)
{
	mmgr->dispose (ptr);
}
#endif
