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

#include <Hawk.hpp>
#include "hawk-prv.h"

/////////////////////////////////
HAWK_BEGIN_NAMESPACE(HAWK)
/////////////////////////////////

//////////////////////////////////////////////////////////////////
// Hawk::Source
//////////////////////////////////////////////////////////////////

struct xtn_t
{
	Hawk* awk;
	hawk_ecb_t ecb;
};

struct rxtn_t
{
	Hawk::Run* run;
};

Hawk::NoSource Hawk::Source::NONE;

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE xtn_t* GET_XTN(hawk_t* awk) { return (xtn_t*)((hawk_uint8_t*)hawk_getxtn(awk) - HAWK_SIZEOF(xtn_t)); }
static HAWK_INLINE rxtn_t* GET_RXTN(hawk_rtx_t* rtx) { return (rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)); }
#else
#define GET_XTN(awk) ((xtn_t*)((hawk_uint8_t*)hawk_getxtn(awk) - HAWK_SIZEOF(xtn_t)))
#define GET_RXTN(rtx) ((rxtn_t*)((hawk_uint8_t*)hawk_rtx_getxtn(rtx) - HAWK_SIZEOF(rxtn_t)))
#endif

//////////////////////////////////////////////////////////////////
// Hawk::RIO
//////////////////////////////////////////////////////////////////

Hawk::RIOBase::RIOBase (Run* run, rio_arg_t* riod): run (run), riod (riod)
{
}

const Hawk::char_t* Hawk::RIOBase::getName () const
{
	return this->riod->name;
}

const void* Hawk::RIOBase::getHandle () const
{
	return this->riod->handle;
}

void Hawk::RIOBase::setHandle (void* handle)
{
	this->riod->handle = handle;
}

int Hawk::RIOBase::getUflags () const
{
	return this->riod->uflags;
}

void Hawk::RIOBase::setUflags (int uflags)
{
	this->riod->uflags = uflags;
}

Hawk::RIOBase::operator Hawk* () const 
{
	return this->run->awk;
}

Hawk::RIOBase::operator Hawk::awk_t* () const 
{
	HAWK_ASSERT (hawk_rtx_getawk(this->run->rtx) == this->run->awk->getHandle());
	return this->run->awk->getHandle();
}

Hawk::RIOBase::operator Hawk::rio_arg_t* () const
{
	return this->riod;
}

Hawk::RIOBase::operator Hawk::Run* () const
{
	return this->run;
}

Hawk::RIOBase::operator Hawk::rtx_t* () const
{
	return this->run->rtx;
}

//////////////////////////////////////////////////////////////////
// Hawk::Pipe
//////////////////////////////////////////////////////////////////

Hawk::Pipe::Pipe (Run* run, rio_arg_t* riod): RIOBase (run, riod)
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

Hawk::File::File (Run* run, rio_arg_t* riod): RIOBase (run, riod)
{
}

Hawk::File::Mode Hawk::File::getMode () const
{
	return (Mode)riod->mode;
}

//////////////////////////////////////////////////////////////////
// Hawk::Console
//////////////////////////////////////////////////////////////////

Hawk::Console::Console (Run* run, rio_arg_t* riod): 
	RIOBase (run, riod), filename (HAWK_NULL)
{
}

Hawk::Console::~Console ()
{
	if (filename != HAWK_NULL)
	{
		hawk_freemem ((awk_t*)this, filename);
	}
}

int Hawk::Console::setFileName (const char_t* name)
{
	if (this->getMode() == READ)
	{
		return hawk_rtx_setfilename (
			this->run->rtx, name, hawk_strlen(name));
	}
	else
	{
		return hawk_rtx_setofilename (
			this->run->rtx, name, hawk_strlen(name));
	}
}

int Hawk::Console::setFNR (int_t fnr)
{
	val_t* tmp;
	int n;

	tmp = hawk_rtx_makeintval (this->run->rtx, fnr);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (this->run->rtx, tmp);
	n = hawk_rtx_setgbl (this->run->rtx, HAWK_GBL_FNR, tmp);
	hawk_rtx_refdownval (this->run->rtx, tmp);

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

const Hawk::char_t* Hawk::Value::getEmptyStr()
{
	static const Hawk::char_t* EMPTY_STRING = HAWK_T("");
	return EMPTY_STRING;
}
const hawk_bch_t* Hawk::Value::getEmptyMbs()
{
	static const hawk_bch_t* EMPTY_STRING = HAWK_BT("");
	return EMPTY_STRING;
}

Hawk::Value::IntIndex::IntIndex (int_t x)
{
	ptr = buf;
	len = 0;

#define NTOC(n) (HAWK_T("0123456789")[n])

	int base = 10;
	int_t last = x % base;
	int_t y = 0;
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
void* Hawk::Value::operator new (size_t n, Run* run) throw ()
{
	void* ptr = hawk_rtx_allocmem (run->rtx, HAWK_SIZEOF(run) + n);
	if (ptr == HAWK_NULL) return HAWK_NULL;

	*(Run**)ptr = run;
	return (char*)ptr + HAWK_SIZEOF(run);
}

void* Hawk::Value::operator new[] (size_t n, Run* run) throw () 
{
	void* ptr = hawk_rtx_allocmem (run->rtx, HAWK_SIZEOF(run) + n);
	if (ptr == HAWK_NULL) return HAWK_NULL;

	*(Run**)ptr = run;
	return (char*)ptr + HAWK_SIZEOF(run);
}

#if !defined(__BORLANDC__) && !defined(__WATCOMC__)
void Hawk::Value::operator delete (void* ptr, Run* run) 
{
	// this placement delete is to be called when the constructor 
	// throws an exception and it's caught by the caller.
	hawk_rtx_freemem (run->rtx, (char*)ptr - HAWK_SIZEOF(run));
}

void Hawk::Value::operator delete[] (void* ptr, Run* run) 
{
	// this placement delete is to be called when the constructor 
	// throws an exception and it's caught by the caller.
	hawk_rtx_freemem (run->rtx, (char*)ptr - HAWK_SIZEOF(run));
}
#endif

void Hawk::Value::operator delete (void* ptr) 
{
	// this delete is to be called for normal destruction.
	void* p = (char*)ptr - HAWK_SIZEOF(Run*);
	hawk_rtx_freemem ((*(Run**)p)->rtx, p);
}

void Hawk::Value::operator delete[] (void* ptr) 
{
	// this delete is to be called for normal destruction.
	void* p = (char*)ptr - HAWK_SIZEOF(Run*);
	hawk_rtx_freemem ((*(Run**)p)->rtx, p);
}
#endif

Hawk::Value::Value (): run (HAWK_NULL), val (hawk_get_awk_nil_val()) 
{
	cached.str.ptr = HAWK_NULL;
	cached.str.len = 0;
	cached.mbs.ptr = HAWK_NULL;
	cached.mbs.len = 0;
}

Hawk::Value::Value (Run& run): run (&run), val (hawk_get_awk_nil_val()) 
{
	cached.str.ptr = HAWK_NULL;
	cached.str.len = 0;
	cached.mbs.ptr = HAWK_NULL;
	cached.mbs.len = 0;
}

Hawk::Value::Value (Run* run): run (run), val (hawk_get_awk_nil_val()) 
{
	cached.str.ptr = HAWK_NULL;
	cached.str.len = 0;
	cached.mbs.ptr = HAWK_NULL;
	cached.mbs.len = 0;
}

Hawk::Value::Value (const Value& v): run(v.run), val(v.val)
{
	if (run) hawk_rtx_refupval (run->rtx, val);

	cached.str.ptr = HAWK_NULL;
	cached.str.len = 0;
	cached.mbs.ptr = HAWK_NULL;
	cached.mbs.len = 0;
}

Hawk::Value::~Value ()
{
	if (run != HAWK_NULL)
	{
		hawk_rtx_refdownval (run->rtx, val);
		if (cached.str.ptr) hawk_rtx_freemem (run->rtx, cached.str.ptr);
		if (cached.mbs.ptr) hawk_rtx_freemem (run->rtx, cached.mbs.ptr);
	}
}

Hawk::Value& Hawk::Value::operator= (const Value& v)
{
	if (this == &v) return *this;

	if (run)
	{
		hawk_rtx_refdownval (run->rtx, val);
		if (cached.str.ptr)
		{
			hawk_rtx_freemem (run->rtx, cached.str.ptr);
			cached.str.ptr = HAWK_NULL;
			cached.str.len = 0;
		}
		if (cached.mbs.ptr)
		{
			hawk_rtx_freemem (run->rtx, cached.mbs.ptr);
			cached.mbs.ptr = HAWK_NULL;
			cached.mbs.len = 0;
		}
	}

	run = v.run;
	val = v.val;

	if (run) hawk_rtx_refupval (run->rtx, val);

	return *this;
}

void Hawk::Value::clear ()
{
	if (run)
	{
		hawk_rtx_refdownval (run->rtx, val);

		if (cached.str.ptr)
		{
			hawk_rtx_freemem (run->rtx, cached.str.ptr);
			cached.str.ptr = HAWK_NULL;
			cached.str.len = 0;
		}
		if (cached.mbs.ptr)
		{
			hawk_rtx_freemem (run->rtx, cached.mbs.ptr);
			cached.mbs.ptr = HAWK_NULL;
			cached.mbs.len = 0;
		}

		run = HAWK_NULL;
		val = hawk_get_awk_nil_val();
	}
}

Hawk::Value::operator Hawk::int_t () const
{
	int_t v;
	if (this->getInt(&v) <= -1) v = 0;
	return v;
}

Hawk::Value::operator Hawk::flt_t () const
{
	flt_t v;
	if (this->getFlt(&v) <= -1) v = 0.0;
	return v;
}

Hawk::Value::operator const Hawk::char_t* () const
{
	const Hawk::char_t* ptr;
	size_t len;
	if (Hawk::Value::getStr(&ptr, &len) <= -1) ptr = getEmptyStr();
	return ptr;
}

#if defined(HAWK_OOCH_IS_UCH)
Hawk::Value::operator const hawk_bch_t* () const
{
	const hawk_bch_t* ptr;
	size_t len;
	if (Hawk::Value::getMbs(&ptr, &len) <= -1) ptr = getEmptyMbs();
	return ptr;
}
#endif

int Hawk::Value::getInt (int_t* v) const
{
	int_t lv = 0;

	HAWK_ASSERT (this->val != HAWK_NULL);

	if (this->run)
	{
		int n = hawk_rtx_valtoint(this->run->rtx, this->val, &lv);
		if (n <= -1) 
		{
			run->awk->retrieveError (this->run);
			return -1;
		}
	}

	*v = lv;
	return 0;
}

int Hawk::Value::getFlt (flt_t* v) const
{
	flt_t rv = 0;

	HAWK_ASSERT (this->val != HAWK_NULL);

	if (this->run)
	{
		int n = hawk_rtx_valtoflt(this->run->rtx, this->val, &rv);
		if (n <= -1)
		{
			run->awk->retrieveError (this->run);
			return -1;
		}
	}

	*v = rv;
	return 0;
}

int Hawk::Value::getNum (int_t* lv, flt_t* fv) const
{
	HAWK_ASSERT (this->val != HAWK_NULL);

	if (this->run)
	{
		int n = hawk_rtx_valtonum(this->run->rtx, this->val, lv, fv);
		if (n <= -1)
		{
			run->awk->retrieveError (this->run);
			return -1;
		}
		return n;
	}

	*lv = 0;
	return 0;
}

int Hawk::Value::getStr (const char_t** str, size_t* len) const
{
	const char_t* p = getEmptyStr();
	size_t l = 0;

	HAWK_ASSERT (this->val != HAWK_NULL);

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
					run->awk->retrieveError (this->run);
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

int Hawk::Value::getMbs (const hawk_bch_t** str, size_t* len) const
{
	const hawk_bch_t* p = getEmptyMbs();
	size_t l = 0;

	HAWK_ASSERT (this->val != HAWK_NULL);

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
					run->awk->retrieveError (this->run);
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

int Hawk::Value::setVal (val_t* v)
{
	if (this->run == HAWK_NULL) 
	{
		/* no runtime context assoicated. unfortunately, i can't 
		 * set an error number for the same reason */
		return -1;
	}
	return this->setVal(this->run, v);
}

int Hawk::Value::setVal (Run* r, val_t* v)
{
	if (this->run != HAWK_NULL)
	{
		hawk_rtx_refdownval (this->run->rtx, val);
		if (cached.str.ptr)
		{
			hawk_rtx_freemem (this->run->rtx, cached.str.ptr);
			cached.str.ptr = HAWK_NULL;
			cached.str.len = 0;
		}
		if (cached.mbs.ptr)
		{
			hawk_rtx_freemem (this->run->rtx, cached.mbs.ptr);
			cached.mbs.ptr = HAWK_NULL;
			cached.mbs.len = 0;
		}
	}

	HAWK_ASSERT (cached.str.ptr == HAWK_NULL);
	HAWK_ASSERT (cached.mbs.ptr == HAWK_NULL);
	hawk_rtx_refupval (r->rtx, v);

	this->run = r;
	this->val = v;

	return 0;
}

int Hawk::Value::setInt (int_t v)
{
	if (this->run == HAWK_NULL) 
	{
		/* no runtime context assoicated. unfortunately, i can't 
		 * set an error number for the same reason */
		return -1;
	}
	return this->setInt(this->run, v);
}

int Hawk::Value::setInt (Run* r, int_t v)
{
	val_t* tmp;
	tmp = hawk_rtx_makeintval(r->rtx, v);
	if (tmp == HAWK_NULL) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT (n == 0);
	return n;
}

int Hawk::Value::setFlt (flt_t v)
{
	if (this->run == HAWK_NULL) 
	{
		/* no runtime context assoicated. unfortunately, i can't 
		 * set an error number for the same reason */
		return -1;
	}
	return this->setFlt(this->run, v);
}

int Hawk::Value::setFlt (Run* r, flt_t v)
{
	val_t* tmp;
	tmp = hawk_rtx_makefltval(r->rtx, v);
	if (tmp == HAWK_NULL)
	{
		r->awk->retrieveError (r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT (n == 0);
	return n;
}

int Hawk::Value::setStr (const char_t* str, size_t len, bool numeric)
{
	if (this->run == HAWK_NULL) 
	{
		/* no runtime context assoicated. unfortunately, i can't 
		 * set an error number for the same reason */
		return -1; 
	}
	return this->setStr(this->run, str, len, numeric);
}

int Hawk::Value::setStr (Run* r, const char_t* str, size_t len, bool numeric)
{
	val_t* tmp;

	oocs_t oocs;
	oocs.ptr = (char_t*)str;
	oocs.len = len;

	tmp = numeric? hawk_rtx_makenstrvalwithoocs(r->rtx, &oocs):
	               hawk_rtx_makestrvalwithoocs(r->rtx, &oocs);
	if (tmp == HAWK_NULL)
	{
		r->awk->retrieveError (r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT (n == 0);
	return n;
}

int Hawk::Value::setStr (const char_t* str, bool numeric)
{
	if (this->run == HAWK_NULL) return -1;
	return this->setStr(this->run, str, numeric);
}

int Hawk::Value::setStr (Run* r, const char_t* str, bool numeric)
{
	val_t* tmp;
	tmp = numeric? hawk_rtx_makenstrvalwithoocstr(r->rtx, str):
	               hawk_rtx_makestrvalwithoocstr(r->rtx, str);
	if (tmp == HAWK_NULL) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT (n == 0);
	return n;
}


int Hawk::Value::setMbs (const hawk_bch_t* str, size_t len)
{
	if (this->run == HAWK_NULL) 
	{
		/* no runtime context assoicated. unfortunately, i can't 
		 * set an error number for the same reason */
		return -1; 
	}
	return this->setMbs(this->run, str, len);
}

int Hawk::Value::setMbs (Run* r, const hawk_bch_t* str, size_t len)
{
	val_t* tmp;

	hawk_bcs_t oocs;
	oocs.ptr = (hawk_bch_t*)str;
	oocs.len = len;

	tmp = hawk_rtx_makembsvalwithbcs(r->rtx, &oocs);
	if (!tmp)
	{
		r->awk->retrieveError (r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT (n == 0);
	return n;
}

int Hawk::Value::setMbs (const hawk_bch_t* str)
{
	if (this->run == HAWK_NULL) return -1;
	return this->setMbs(this->run, str);
}

int Hawk::Value::setMbs (Run* r, const hawk_bch_t* str)
{
	val_t* tmp;
	tmp = hawk_rtx_makembsval(r->rtx, str, hawk_mbslen(str));
	if (!tmp) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	int n = this->setVal(r, tmp);
	HAWK_ASSERT (n == 0);
	return n;
}

int Hawk::Value::setIndexedVal (const Index& idx, val_t* v)
{
	if (this->run == HAWK_NULL) return -1;
	return this->setIndexedVal (this->run, idx, v);
}

int Hawk::Value::setIndexedVal (Run* r, const Index& idx, val_t* v)
{
	HAWK_ASSERT (r != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE (r->rtx, this->val) != HAWK_VAL_MAP)
	{
		// the previous value is not a map. 
		// a new map value needs to be created first.
		val_t* map = hawk_rtx_makemapval(r->rtx);
		if (map == HAWK_NULL) 
		{
			r->awk->retrieveError (r);
			return -1;
		}

		hawk_rtx_refupval (r->rtx, map);

		// update the map with a given value 
		if (hawk_rtx_setmapvalfld(r->rtx, map, idx.ptr, idx.len, v) == HAWK_NULL)
		{
			hawk_rtx_refdownval (r->rtx, map);
			r->awk->retrieveError (r);
			return -1;
		}

		// free the previous value
		if (this->run) 
		{
			// if val is not nil, this->run can't be NULL
			hawk_rtx_refdownval (this->run->rtx, this->val);
		}

		this->run = r;
		this->val = map;
	}
	else
	{
		HAWK_ASSERT (run != HAWK_NULL);

		// if the previous value is a map, things are a bit simpler 
		// however it needs to check if the runtime context matches
		// with the previous one.
		if (this->run != r) 
		{
			// it can't span across multiple runtime contexts
			this->run->setError (HAWK_EINVAL);
			this->run->awk->retrieveError (run);
			return -1;
		}

		// update the map with a given value 
		if (hawk_rtx_setmapvalfld(r->rtx, val, idx.ptr, idx.len, v) == HAWK_NULL)
		{
			r->awk->retrieveError (r);
			return -1;
		}
	}

	return 0;
}

int Hawk::Value::setIndexedInt (const Index& idx, int_t v)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedInt (run, idx, v);
}

int Hawk::Value::setIndexedInt (Run* r, const Index& idx, int_t v)
{
	val_t* tmp = hawk_rtx_makeintval (r->rtx, v);
	if (tmp == HAWK_NULL) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	hawk_rtx_refupval (r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval (r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedFlt (const Index& idx, flt_t v)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedFlt(run, idx, v);
}

int Hawk::Value::setIndexedFlt (Run* r, const Index& idx, flt_t v)
{
	val_t* tmp = hawk_rtx_makefltval(r->rtx, v);
	if (tmp == HAWK_NULL) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	hawk_rtx_refupval (r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval (r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedStr (const Index& idx, const char_t* str, size_t len, bool numeric)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedStr(run, idx, str, len, numeric);
}

int Hawk::Value::setIndexedStr (
	Run* r, const Index& idx, const char_t* str, size_t len, bool numeric)
{
	val_t* tmp;

	oocs_t oocs;
	oocs.ptr = (char_t*)str;
	oocs.len = len;

	tmp = numeric? hawk_rtx_makenstrvalwithoocs(r->rtx, &oocs):
	               hawk_rtx_makestrvalwithoocs(r->rtx, &oocs);
	if (tmp == HAWK_NULL) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	hawk_rtx_refupval (r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval (r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedStr (const Index& idx, const char_t* str, bool numeric)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedStr(run, idx, str, numeric);
}

int Hawk::Value::setIndexedStr (Run* r, const Index& idx, const char_t* str, bool numeric)
{
	val_t* tmp;
	tmp = numeric? hawk_rtx_makenstrvalwithoocstr(r->rtx, str):
	               hawk_rtx_makestrvalwithoocstr(r->rtx, str);
	if (tmp == HAWK_NULL)
	{
		r->awk->retrieveError (r);
		return -1;
	}

	hawk_rtx_refupval (r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval (r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedMbs (const Index& idx, const hawk_bch_t* str, size_t len)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedMbs(run, idx, str, len);
}

int Hawk::Value::setIndexedMbs (Run* r, const Index& idx, const hawk_bch_t* str, size_t len)
{
	val_t* tmp;

	hawk_bcs_t oocs;
	oocs.ptr = (hawk_bch_t*)str;
	oocs.len = len;

	tmp = hawk_rtx_makembsvalwithbcs(r->rtx, &oocs);
	if (!tmp) 
	{
		r->awk->retrieveError (r);
		return -1;
	}

	hawk_rtx_refupval (r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval (r->rtx, tmp);

	return n;
}

int Hawk::Value::setIndexedMbs (const Index& idx, const hawk_bch_t* str)
{
	if (run == HAWK_NULL) return -1;
	return this->setIndexedMbs(run, idx, str);
}

int Hawk::Value::setIndexedMbs (Run* r, const Index& idx, const hawk_bch_t* str)
{
	val_t* tmp;
	tmp = hawk_rtx_makembsval(r->rtx, str, hawk_mbslen(str));
	if (tmp == HAWK_NULL)
	{
		r->awk->retrieveError (r);
		return -1;
	}

	hawk_rtx_refupval (r->rtx, tmp);
	int n = this->setIndexedVal(r, idx, tmp);
	hawk_rtx_refdownval (r->rtx, tmp);

	return n;
}


bool Hawk::Value::isIndexed () const
{
	HAWK_ASSERT (val != HAWK_NULL);
	return HAWK_RTX_GETVALTYPE(this->run->rtx, val) == HAWK_VAL_MAP;
}

int Hawk::Value::getIndexed (const Index& idx, Value* v) const
{
	HAWK_ASSERT (val != HAWK_NULL);

	// not a map. v is just nil. not an error 
	if (HAWK_RTX_GETVALTYPE(this->run->rtx, val) != HAWK_VAL_MAP) 
	{
		v->clear ();
		return 0;
	}

	// get the value from the map.
	val_t* fv = hawk_rtx_getmapvalfld(this->run->rtx, val, (char_t*)idx.ptr, idx.len);

	// the key is not found. it is not an error. v is just nil 
	if (fv == HAWK_NULL)
	{
		v->clear ();
		return 0; 
	}

	// if v.set fails, it should return an error 
	return v->setVal (this->run, fv);
}

Hawk::Value::IndexIterator Hawk::Value::getFirstIndex (Index* idx) const
{
	HAWK_ASSERT (this->val != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE (this->run->rtx, this->val) != HAWK_VAL_MAP) return IndexIterator::END;

	HAWK_ASSERT (this->run != HAWK_NULL);

	Hawk::Value::IndexIterator itr;
	hawk_val_map_itr_t* iptr;

	iptr = hawk_rtx_getfirstmapvalitr(this->run->rtx, this->val, &itr);
	if (iptr == HAWK_NULL) return IndexIterator::END; // no more key

	idx->set (HAWK_VAL_MAP_ITR_KEY(iptr));

	return itr;
}

Hawk::Value::IndexIterator Hawk::Value::getNextIndex (
	Index* idx, const IndexIterator& curitr) const
{
	HAWK_ASSERT (val != HAWK_NULL);

	if (HAWK_RTX_GETVALTYPE (this->run->rtx, val) != HAWK_VAL_MAP) return IndexIterator::END;

	HAWK_ASSERT (this->run != HAWK_NULL);

	Hawk::Value::IndexIterator itr (curitr);
	hawk_val_map_itr_t* iptr;

	iptr = hawk_rtx_getnextmapvalitr(this->run->rtx, this->val, &itr);
	if (iptr == HAWK_NULL) return IndexIterator::END; // no more key

	idx->set (HAWK_VAL_MAP_ITR_KEY(iptr));

	return itr;
}

//////////////////////////////////////////////////////////////////
// Hawk::Run
//////////////////////////////////////////////////////////////////

Hawk::Run::Run (Hawk* awk): awk (awk), rtx (HAWK_NULL)
{
}

Hawk::Run::Run (Hawk* awk, rtx_t* rtx): awk (awk), rtx (rtx)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
}

Hawk::Run::~Run ()
{
}

Hawk::Run::operator Hawk* () const 
{
	return this->awk;
}

Hawk::Run::operator Hawk::rtx_t* () const 
{
	return this->rtx;
}

void Hawk::Run::halt () const 
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	hawk_rtx_halt (this->rtx);
}

bool Hawk::Run::isHalt () const
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	return hawk_rtx_ishalt (this->rtx)? true: false;
}

Hawk::errnum_t Hawk::Run::getErrorNumber () const 
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	return hawk_rtx_geterrnum (this->rtx);
}

Hawk::loc_t Hawk::Run::getErrorLocation () const 
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	return *hawk_rtx_geterrloc (this->rtx);
}

const Hawk::char_t* Hawk::Run::getErrorMessage () const  
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	return hawk_rtx_geterrmsg (this->rtx);
}

void Hawk::Run::setError (errnum_t code, const oocs_t* args, const loc_t* loc)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	hawk_rtx_seterror (this->rtx, code, args, loc);
}

void Hawk::Run::setErrorWithMessage (
	errnum_t code, const char_t* msg, const loc_t* loc)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);

	errinf_t errinf;

	HAWK_MEMSET (&errinf, 0, HAWK_SIZEOF(errinf));
	errinf.num = code;
	if (loc == HAWK_NULL) errinf.loc = *loc;
	hawk_strxcpy (errinf.msg, HAWK_COUNTOF(errinf.msg), msg);

	hawk_rtx_seterrinf (this->rtx, &errinf);
}

int Hawk::Run::setGlobal (int id, int_t v)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);

	val_t* tmp = hawk_rtx_makeintval (this->rtx, v);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (this->rtx, tmp);
	int n = hawk_rtx_setgbl (this->rtx, id, tmp);
	hawk_rtx_refdownval (this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, flt_t v)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);

	val_t* tmp = hawk_rtx_makefltval (this->rtx, v);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (this->rtx, tmp);
	int n = hawk_rtx_setgbl (this->rtx, id, tmp);
	hawk_rtx_refdownval (this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const char_t* ptr, size_t len)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);

	val_t* tmp = hawk_rtx_makestrvalwithoochars(this->rtx, ptr, len);
	if (tmp == HAWK_NULL) return -1;

	hawk_rtx_refupval (this->rtx, tmp);
	int n = hawk_rtx_setgbl (this->rtx, id, tmp);
	hawk_rtx_refdownval (this->rtx, tmp);
	return n;
}

int Hawk::Run::setGlobal (int id, const Value& gbl)
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	return hawk_rtx_setgbl (this->rtx, id, (val_t*)gbl);
}

int Hawk::Run::getGlobal (int id, Value& g) const
{
	HAWK_ASSERT (this->rtx != HAWK_NULL);
	return g.setVal ((Run*)this, hawk_rtx_getgbl (this->rtx, id));
}

//////////////////////////////////////////////////////////////////
// Hawk
//////////////////////////////////////////////////////////////////

Hawk::Hawk (Mmgr* mmgr): 
	Mmged (mmgr), awk (HAWK_NULL), 
#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	functionMap (HAWK_NULL), 
#else
	functionMap (this), 
#endif
	source_reader (HAWK_NULL), source_writer (HAWK_NULL),
	pipe_handler (HAWK_NULL), file_handler (HAWK_NULL), 
	console_handler (HAWK_NULL), runctx (this)

{
	HAWK_MEMSET (&errinf, 0, HAWK_SIZEOF(errinf));
	errinf.num = HAWK_ENOERR;
}

Hawk::operator Hawk::awk_t* () const 
{
	return this->awk;
}

const Hawk::char_t* Hawk::getErrorString (errnum_t num) const 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	HAWK_ASSERT (dflerrstr != HAWK_NULL);
	return dflerrstr (awk, num);
}

const Hawk::char_t* Hawk::xerrstr (awk_t* a, errnum_t num) 
{
	Hawk* awk = *(Hawk**)GET_XTN(a);
	return awk->getErrorString (num);
}


Hawk::errnum_t Hawk::getErrorNumber () const 
{
	return this->errinf.num;
}

Hawk::loc_t Hawk::getErrorLocation () const 
{
	return this->errinf.loc;
}

const Hawk::char_t* Hawk::getErrorMessage () const 
{
	return this->errinf.msg;
}

void Hawk::setError (errnum_t code, const oocs_t* args, const loc_t* loc)
{
	if (awk != HAWK_NULL)
	{
		hawk_seterror (awk, code, args, loc);
		this->retrieveError ();
	}
	else
	{
		HAWK_MEMSET (&errinf, 0, HAWK_SIZEOF(errinf));
		errinf.num = code;
		if (loc != HAWK_NULL) errinf.loc = *loc;
		hawk_strxcpy (errinf.msg, HAWK_COUNTOF(errinf.msg), 
			HAWK_T("not ready to set an error message"));
	}
}

void Hawk::setErrorWithMessage (errnum_t code, const char_t* msg, const loc_t* loc)
{
	HAWK_MEMSET (&errinf, 0, HAWK_SIZEOF(errinf));

	errinf.num = code;
	if (loc != HAWK_NULL) errinf.loc = *loc;
	hawk_strxcpy (errinf.msg, HAWK_COUNTOF(errinf.msg), msg);

	if (awk != HAWK_NULL) hawk_seterrinf (awk, &errinf);
}

void Hawk::clearError ()
{
	HAWK_MEMSET (&errinf, 0, HAWK_SIZEOF(errinf));
	errinf.num = HAWK_ENOERR;
}

void Hawk::retrieveError ()
{
	if (this->awk == HAWK_NULL) 
	{
		clearError ();
	}
	else
	{
		hawk_geterrinf (this->awk, &errinf);
	}
}

void Hawk::retrieveError (Run* run)
{
	HAWK_ASSERT (run != HAWK_NULL);
	if (run->rtx == HAWK_NULL) return;
	hawk_rtx_geterrinf (run->rtx, &errinf);
}

static void fini_xtn (hawk_t* awk)
{
	xtn_t* xtn = GET_XTN(awk);
	xtn->awk->uponClosing ();
}

static void clear_xtn (hawk_t* awk)
{
	xtn_t* xtn = GET_XTN(awk);
	xtn->awk->uponClearing ();
}

int Hawk::open () 
{
	HAWK_ASSERT (this->awk == HAWK_NULL);
#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	HAWK_ASSERT (this->functionMap == HAWK_NULL);
#endif

	hawk_prm_t prm;

	HAWK_MEMSET (&prm, 0, HAWK_SIZEOF(prm));
	prm.math.pow = pow;
	prm.math.mod = mod;
	prm.modopen  = modopen;
	prm.modclose = modclose;
	prm.modsym   = modsym;

	hawk_errnum_t errnum;
	this->awk = hawk_open(this->getMmgr(), HAWK_SIZEOF(xtn_t), &prm, &errnum);
	if (!this->awk)
	{
		this->setError (errnum);
		return -1;
	}

	this->awk->_instsize += HAWK_SIZEOF(xtn_t);

	// associate this Hawk object with the underlying awk object
	xtn_t* xtn = (xtn_t*)GET_XTN(this->awk);
	xtn->awk = this;
	xtn->ecb.close = fini_xtn;
	xtn->ecb.clear = clear_xtn;

	dflerrstr = hawk_geterrstr (this->awk);
	hawk_seterrstr (this->awk, xerrstr);

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	this->functionMap = hawk_htb_open(
		hawk_getgem(this->awk), HAWK_SIZEOF(this), 512, 70,
		HAWK_SIZEOF(hawk_ooch_t), 1
	);
	if (this->functionMap == HAWK_NULL)
	{
		hawk_close (this->awk);
		this->awk = HAWK_NULL;

		this->setError (HAWK_ENOMEM);
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
	hawk_pushecb (this->awk, &xtn->ecb);
	return 0;
}

void Hawk::close () 
{
	this->fini_runctx ();
	this->clearArguments ();

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	if (this->functionMap)
	{
		hawk_htb_close (this->functionMap);
		this->functionMap = HAWK_NULL;
	}
#else
	this->functionMap.clear ();
#endif

	if (this->awk) 
	{
		hawk_close (this->awk);
		this->awk = HAWK_NULL;
	}

	this->clearError ();
}

hawk_cmgr_t* Hawk::getCmgr () const
{
	if (!this->awk) return HAWK_NULL;
	return hawk_getcmgr(this->awk);
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
	HAWK_ASSERT (awk != HAWK_NULL);

	if (&in == &Source::NONE) 
	{
		this->setError (HAWK_EINVAL);
		return HAWK_NULL;
	}

	this->fini_runctx ();

	source_reader = &in;
	source_writer = (&out == &Source::NONE)? HAWK_NULL: &out;

	hawk_sio_cbs_t sio;
	sio.in = readSource;
	sio.out = (source_writer == HAWK_NULL)? HAWK_NULL: writeSource;

	int n = hawk_parse(awk, &sio);
	if (n <= -1) 
	{
		this->retrieveError ();
		return HAWK_NULL;
	}

	if (this->init_runctx() <= -1) return HAWK_NULL;
	return &this->runctx;
}

Hawk::Run* Hawk::resetRunContext ()
{
	if (this->runctx.rtx)
	{
		this->fini_runctx ();
		if (this->init_runctx() <= -1) return HAWK_NULL;
		return &this->runctx;
	}
	else return HAWK_NULL;
}

int Hawk::loop (Value* ret)
{
	HAWK_ASSERT (this->awk != HAWK_NULL);
	HAWK_ASSERT (this->runctx.rtx != HAWK_NULL);

	val_t* rv = hawk_rtx_loop (this->runctx.rtx);
	if (rv == HAWK_NULL) 
	{
		this->retrieveError (&this->runctx);
		return -1;
	}

	ret->setVal (&this->runctx, rv);
	hawk_rtx_refdownval (this->runctx.rtx, rv);
	
	return 0;
}

int Hawk::call (const hawk_bch_t* name, Value* ret, const Value* args, size_t nargs)
{
	HAWK_ASSERT (this->awk != HAWK_NULL);
	HAWK_ASSERT (this->runctx.rtx != HAWK_NULL);

	val_t* buf[16];
	val_t** ptr = HAWK_NULL;

	if (args != HAWK_NULL)
	{
		if (nargs <= HAWK_COUNTOF(buf)) ptr = buf;
		else
		{
			ptr = (val_t**)hawk_allocmem(awk, HAWK_SIZEOF(val_t*) * nargs);
			if (ptr == HAWK_NULL)
			{
				this->runctx.setError (HAWK_ENOMEM);
				this->retrieveError (&this->runctx);
				return -1;
			}
		}

		for (size_t i = 0; i < nargs; i++) ptr[i] = (val_t*)args[i];
	}

	val_t* rv = hawk_rtx_callwithbcstr(this->runctx.rtx, name, ptr, nargs);

	if (ptr != HAWK_NULL && ptr != buf) hawk_freemem (awk, ptr);

	if (rv == HAWK_NULL) 
	{
		this->retrieveError (&this->runctx);
		return -1;
	}

	ret->setVal (&this->runctx, rv);

	hawk_rtx_refdownval (this->runctx.rtx, rv);
	return 0;
}

int Hawk::call (const hawk_uch_t* name, Value* ret, const Value* args, size_t nargs)
{
	HAWK_ASSERT (this->awk != HAWK_NULL);
	HAWK_ASSERT (this->runctx.rtx != HAWK_NULL);

	val_t* buf[16];
	val_t** ptr = HAWK_NULL;

	if (args != HAWK_NULL)
	{
		if (nargs <= HAWK_COUNTOF(buf)) ptr = buf;
		else
		{
			ptr = (val_t**)hawk_allocmem(awk, HAWK_SIZEOF(val_t*) * nargs);
			if (ptr == HAWK_NULL)
			{
				this->runctx.setError (HAWK_ENOMEM);
				this->retrieveError (&this->runctx);
				return -1;
			}
		}

		for (size_t i = 0; i < nargs; i++) ptr[i] = (val_t*)args[i];
	}

	val_t* rv = hawk_rtx_callwithucstr(this->runctx.rtx, name, ptr, nargs);

	if (ptr != HAWK_NULL && ptr != buf) hawk_freemem (awk, ptr);

	if (rv == HAWK_NULL) 
	{
		this->retrieveError (&this->runctx);
		return -1;
	}

	ret->setVal (&this->runctx, rv);

	hawk_rtx_refdownval (this->runctx.rtx, rv);
	return 0;
}

void Hawk::halt () 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	hawk_haltall (awk);
}

int Hawk::init_runctx () 
{
	if (this->runctx.rtx) return 0;

	hawk_rio_t rio;

	rio.pipe    = pipeHandler;
	rio.file    = fileHandler;
	rio.console = consoleHandler;

	rtx_t* rtx = hawk_rtx_open(awk, HAWK_SIZEOF(rxtn_t), &rio);
	if (rtx == HAWK_NULL) 
	{
		this->retrieveError();
		return -1;
	}

	rtx->_instsize += HAWK_SIZEOF(rxtn_t);
	runctx.rtx = rtx;

	rxtn_t* rxtn = GET_RXTN(rtx);
	rxtn->run = &runctx;

	return 0;
}

void Hawk::fini_runctx () 
{
	if (this->runctx.rtx)
	{
		hawk_rtx_close (this->runctx.rtx);
		this->runctx.rtx = HAWK_NULL;
	}
}

int Hawk::getTrait () const 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int val;
	hawk_getopt (awk, HAWK_TRAIT, &val);
	return val;
}

void Hawk::setTrait (int trait) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	hawk_setopt (awk, HAWK_TRAIT, &trait);
}

Hawk::size_t Hawk::getMaxDepth (depth_t id) const 
{
	HAWK_ASSERT (awk != HAWK_NULL);

	size_t depth;
	hawk_getopt (awk, (hawk_opt_t)id, &depth);
	return depth;
}

void Hawk::setMaxDepth (depth_t id, size_t depth) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	hawk_setopt (awk, (hawk_opt_t)id, &depth);
}

int Hawk::dispatch_function (Run* run, const fnc_info_t* fi)
{
	bool has_ref_arg = false;

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	hawk_htb_pair_t* pair;
	pair = hawk_htb_search (this->functionMap, fi->name.ptr, fi->name.len);
	if (pair == HAWK_NULL) 
	{
		run->setError (HAWK_EFUNNF, &fi->name);
		return -1;
	}
	
	FunctionHandler handler;
	handler = *(FunctionHandler*)HAWK_HTB_VPTR(pair);
#else
	FunctionMap::Pair* pair = this->functionMap.search (Cstr(fi->name.ptr, fi->name.len));
	if (pair == HAWK_NULL)
	{
		run->setError (HAWK_EFUNNF, &fi->name);
		return -1;
	}

	FunctionHandler handler;
	handler = pair->value;
#endif

	size_t i, nargs = hawk_rtx_getnargs(run->rtx);

	Value buf[16];
	Value* args;
	if (nargs <= HAWK_COUNTOF(buf)) args = buf;
	else
	{
	#if defined(AWK_VALUE_USE_IN_CLASS_PLACEMENT_NEW)
		args = new(run) Value[nargs];
		if (args == HAWK_NULL) 
		{
			run->setError (HAWK_ENOMEM);
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
			run->setError (HAWK_ENOMEM);
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
		val_t* v = hawk_rtx_getarg (run->rtx, i);

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
						xx = args[i].setStr (run, 
							HAWK_STR_PTR(&run->rtx->inrec.line),
							HAWK_STR_LEN(&run->rtx->inrec.line));
					}
					else if (idx <= run->rtx->inrec.nflds)
					{
						xx = args[i].setStr (run,
							run->rtx->inrec.flds[idx-1].ptr,
							run->rtx->inrec.flds[idx-1].len);
					}
					else
					{
						xx = args[i].setStr (run, HAWK_T(""), (size_t)0);
					}
					break;
				}

				case hawk_val_ref_t::HAWK_VAL_REF_GBL:
				{
					hawk_oow_t idx = (hawk_oow_t)ref->adr;
					hawk_val_t* val = (hawk_val_t*)RTX_STACK_GBL(run->rtx, idx);
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
			run->setError (HAWK_ENOMEM);
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
			HAWK_ASSERTX (args[i].run == run, 
				"Do NOT change the run field from function handler");

			val_t* v = hawk_rtx_getarg(run->rtx, i);
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

int Hawk::xstrs_t::add (awk_t* awk, const char_t* arg, size_t len) 
{
	if (this->len >= this->capa)
	{
		hawk_oocs_t* ptr;
		size_t capa = this->capa;

		capa += 64;
		ptr = (hawk_oocs_t*)hawk_reallocmem(awk, this->ptr, HAWK_SIZEOF(hawk_oocs_t)*(capa+1));
		if (ptr == HAWK_NULL) return -1;

		this->ptr = ptr;
		this->capa = capa;
	}

	this->ptr[this->len].len = len;
	this->ptr[this->len].ptr = hawk_strxdup(awk, arg, len);
	if (this->ptr[this->len].ptr == HAWK_NULL) return -1;

	this->len++;
	this->ptr[this->len].len = 0;
	this->ptr[this->len].ptr = HAWK_NULL;

	return 0;
}

void Hawk::xstrs_t::clear (awk_t* awk) 
{
	if (this->ptr != HAWK_NULL)
	{
		while (this->len > 0)
			hawk_freemem (awk, this->ptr[--this->len].ptr);

		hawk_freemem (awk, this->ptr);
		this->ptr = HAWK_NULL;
		this->capa = 0;
	}
}

int Hawk::addArgument (const char_t* arg, size_t len) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = runarg.add(awk, arg, len);
	if (n <= -1) this->setError (HAWK_ENOMEM);
	return n;
}

int Hawk::addArgument (const char_t* arg) 
{
	return addArgument(arg, hawk_strlen(arg));
}

void Hawk::clearArguments () 
{
	runarg.clear (awk);
}

int Hawk::addGlobal(const hawk_bch_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = hawk_addgblwithbcstr(awk, name);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::addGlobal(const hawk_uch_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = hawk_addgblwithucstr(awk, name);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::deleteGlobal (const hawk_bch_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = hawk_delgblwithbcstr(awk, name);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::deleteGlobal (const hawk_uch_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = hawk_delgblwithucstr(awk, name);
	if (n <= -1) this->retrieveError ();
	return n;
}


int Hawk::findGlobal (const hawk_bch_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = hawk_findgblwithbcstr(awk, name);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::findGlobal (const hawk_uch_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	int n = hawk_findgblwithucstr(awk, name);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::setGlobal (int id, const Value& v) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	HAWK_ASSERT (runctx.rtx != HAWK_NULL);

	if (v.run != &runctx) 
	{
		this->setError (HAWK_EINVAL);
		return -1;
	}

	int n = runctx.setGlobal (id, v);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::getGlobal (int id, Value& v) 
{
	HAWK_ASSERT (awk != HAWK_NULL);
	HAWK_ASSERT (runctx.rtx != HAWK_NULL);

	int n = runctx.getGlobal (id, v);
	if (n <= -1) this->retrieveError ();
	return n;
}

int Hawk::addFunction (
	const hawk_bch_t* name, size_t minArgs, size_t maxArgs, 
	const hawk_bch_t* argSpec, FunctionHandler handler, int validOpts)
{
	HAWK_ASSERT (awk != HAWK_NULL);

	hawk_fnc_mspec_t spec;

	HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
	spec.arg.min = minArgs;
	spec.arg.max = maxArgs;
	spec.arg.spec = argSpec;
	spec.impl = this->functionHandler;
	spec.trait = validOpts;

	hawk_fnc_t* fnc = hawk_addfncwithbcstr(awk, name, &spec);
	if (fnc == HAWK_NULL) 
	{
		this->retrieveError ();
		return -1;
	}

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	// handler is a pointer to a member function. 
	// sizeof(handler) is likely to be greater than sizeof(void*)
	// copy the handler pointer into the table.
	//
	// the function name exists in the underlying function table.
	// use the pointer to the name to maintain the hash table.
	hawk_htb_pair_t* pair = hawk_htb_upsert(this->functionMap, (char_t*)fnc->name.ptr, fnc->name.len, &handler, HAWK_SIZEOF(handler));
#else
	FunctionMap::Pair* pair;
	try { pair = this->functionMap.upsert(Cstr(fnc->name.ptr, fnc->name.len), handler); }
	catch (...) { pair = HAWK_NULL; }
#endif

	if (pair == HAWK_NULL)
	{
		hawk_delfncwithbcstr (awk, name);
		this->setError (HAWK_ENOMEM);
		return -1;
	}

	return 0;
}

int Hawk::addFunction (
	const hawk_uch_t* name, size_t minArgs, size_t maxArgs, 
	const hawk_uch_t* argSpec, FunctionHandler handler, int validOpts)
{
	HAWK_ASSERT (awk != HAWK_NULL);

	hawk_fnc_wspec_t spec;

	HAWK_MEMSET (&spec, 0, HAWK_SIZEOF(spec));
	spec.arg.min = minArgs;
	spec.arg.max = maxArgs;
	spec.arg.spec = argSpec;
	spec.impl = this->functionHandler;
	spec.trait = validOpts;

	hawk_fnc_t* fnc = hawk_addfncwithucstr(awk, name, &spec);
	if (fnc == HAWK_NULL) 
	{
		this->retrieveError ();
		return -1;
	}

#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
	// handler is a pointer to a member function. 
	// sizeof(handler) is likely to be greater than sizeof(void*)
	// copy the handler pointer into the table.
	//
	// the function name exists in the underlying function table.
	// use the pointer to the name to maintain the hash table.
	hawk_htb_pair_t* pair = hawk_htb_upsert(this->functionMap, (char_t*)fnc->name.ptr, fnc->name.len, &handler, HAWK_SIZEOF(handler));
#else
	FunctionMap::Pair* pair;
	try { pair = this->functionMap.upsert(Cstr(fnc->name.ptr, fnc->name.len), handler); }
	catch (...) { pair = HAWK_NULL; }
#endif

	if (pair == HAWK_NULL)
	{
		hawk_delfncwithucstr (awk, name);
		this->setError (HAWK_ENOMEM);
		return -1;
	}

	return 0;
}

int Hawk::deleteFunction (const char_t* name) 
{
	HAWK_ASSERT (awk != HAWK_NULL);

	int n = hawk_delfnc(awk, name);
	if (n == 0) 
	{
#if defined(HAWK_USE_HTB_FOR_FUNCTION_MAP)
		hawk_htb_delete (this->functionMap, name, hawk_strlen(name));
#else
		this->functionMap.remove (Cstr(name));
#endif
	}
	else this->retrieveError ();

	return n;
}

Hawk::ssize_t Hawk::readSource (
	awk_t* awk, sio_cmd_t cmd, sio_arg_t* arg,
	char_t* data, size_t count)
{
	xtn_t* xtn = GET_XTN(awk);
	Source::Data sdat (xtn->awk, Source::READ, arg);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			return xtn->awk->source_reader->open (sdat);
		case HAWK_SIO_CMD_CLOSE:
			return xtn->awk->source_reader->close (sdat);
		case HAWK_SIO_CMD_READ:
			return xtn->awk->source_reader->read (sdat, data, count);
		default:
			return -1;
	}
}

Hawk::ssize_t Hawk::writeSource (
	awk_t* awk, hawk_sio_cmd_t cmd, sio_arg_t* arg,
	char_t* data, size_t count)
{
	xtn_t* xtn = GET_XTN(awk);
	Source::Data sdat (xtn->awk, Source::WRITE, arg);

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			return xtn->awk->source_writer->open (sdat);
		case HAWK_SIO_CMD_CLOSE:
			return xtn->awk->source_writer->close (sdat);
		case HAWK_SIO_CMD_WRITE:
			return xtn->awk->source_writer->write (sdat, data, count);
		default:
			return -1;
	}
}

Hawk::ssize_t Hawk::pipeHandler (rtx_t* rtx, rio_cmd_t cmd, rio_arg_t* riod, void* data, size_t count)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	Hawk* awk = rxtn->run->awk;

	HAWK_ASSERT ((riod->type & 0xFF) == HAWK_RIO_PIPE);

	Pipe pipe (rxtn->run, riod);

	try
	{
		if (awk->pipe_handler)
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return awk->pipe_handler->open(pipe);
				case HAWK_RIO_CMD_CLOSE:
					return awk->pipe_handler->close(pipe);

				case HAWK_RIO_CMD_READ:
					return awk->pipe_handler->read(pipe, (hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return awk->pipe_handler->write(pipe, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return awk->pipe_handler->writeBytes(pipe, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return awk->pipe_handler->flush(pipe);
	
				default:
					return -1;
			}
		}
		else
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return awk->openPipe(pipe);
				case HAWK_RIO_CMD_CLOSE:
					return awk->closePipe(pipe);

				case HAWK_RIO_CMD_READ:
					return awk->readPipe(pipe, (hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return awk->writePipe(pipe, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return awk->writePipeBytes(pipe, (const hawk_bch_t*)data, count);
	
				case HAWK_RIO_CMD_FLUSH:
					return awk->flushPipe(pipe);
	
				default:
					return -1;
			}
		}
	}
	catch (...)
	{
		return -1;
	}
}

Hawk::ssize_t Hawk::fileHandler (rtx_t* rtx, rio_cmd_t cmd, rio_arg_t* riod, void* data, size_t count)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	Hawk* awk = rxtn->run->awk;

	HAWK_ASSERT ((riod->type & 0xFF) == HAWK_RIO_FILE);

	File file (rxtn->run, riod);

	try
	{
		if (awk->file_handler)
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return awk->file_handler->open(file);
				case HAWK_RIO_CMD_CLOSE:
					return awk->file_handler->close(file);

				case HAWK_RIO_CMD_READ:
					return awk->file_handler->read(file, (hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return awk->file_handler->write(file, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return awk->file_handler->writeBytes(file, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return awk->file_handler->flush(file);
	
				default:
					return -1;
			}
		}
		else
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return awk->openFile(file);
				case HAWK_RIO_CMD_CLOSE:
					return awk->closeFile(file);

				case HAWK_RIO_CMD_READ:
					return awk->readFile(file, (hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return awk->writeFile(file, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return awk->writeFileBytes(file, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return awk->flushFile(file);

				default:
					return -1;
			}
		}
	}
	catch (...)
	{
		return -1;
	}
}

Hawk::ssize_t Hawk::consoleHandler (rtx_t* rtx, rio_cmd_t cmd, rio_arg_t* riod, void* data, size_t count)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	Hawk* awk = rxtn->run->awk;

	HAWK_ASSERT ((riod->type & 0xFF) == HAWK_RIO_CONSOLE);

	Console console (rxtn->run, riod);

	try
	{
		if (awk->console_handler)
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return awk->console_handler->open(console);
				case HAWK_RIO_CMD_CLOSE:
					return awk->console_handler->close(console);

				case HAWK_RIO_CMD_READ:
					return awk->console_handler->read(console, (hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return awk->console_handler->write(console, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return awk->console_handler->writeBytes(console, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return awk->console_handler->flush(console);
				case HAWK_RIO_CMD_NEXT:
					return awk->console_handler->next(console);

				default:
					return -1;
			}
		}
		else
		{
			switch (cmd)
			{
				case HAWK_RIO_CMD_OPEN:
					return awk->openConsole(console);
				case HAWK_RIO_CMD_CLOSE:
					return awk->closeConsole(console);

				case HAWK_RIO_CMD_READ:
					return awk->readConsole(console, (hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE:
					return awk->writeConsole(console, (const hawk_ooch_t*)data, count);
				case HAWK_RIO_CMD_WRITE_BYTES:
					return awk->writeConsoleBytes(console, (const hawk_bch_t*)data, count);

				case HAWK_RIO_CMD_FLUSH:
					return awk->flushConsole(console);
				case HAWK_RIO_CMD_NEXT:
					return awk->nextConsole(console);

				default:
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
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::closePipe (Pipe& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::readPipe (Pipe& io, char_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::writePipe (Pipe& io, const char_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::writePipeBytes (Pipe& io, const hawk_bch_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::flushPipe (Pipe& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::openFile  (File& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::closeFile (File& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::readFile (File& io, char_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::writeFile (File& io, const char_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::writeFileBytes (File& io, const hawk_bch_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::flushFile (File& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::openConsole  (Console& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::closeConsole (Console& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::readConsole (Console& io, char_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::writeConsole (Console& io, const char_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

Hawk::ssize_t Hawk::writeConsoleBytes (Console& io, const hawk_bch_t* buf, size_t len)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::flushConsole (Console& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::nextConsole (Console& io)
{
	((Run*)io)->setError (HAWK_ENOIMPL);
	return -1;
}

int Hawk::functionHandler (rtx_t* rtx, const fnc_info_t* fi)
{
	rxtn_t* rxtn = GET_RXTN(rtx);
	return rxtn->run->awk->dispatch_function (rxtn->run, fi);
}	
	
Hawk::flt_t Hawk::pow (awk_t* awk, flt_t x, flt_t y)
{
	xtn_t* xtn = GET_XTN(awk);
	return xtn->awk->pow (x, y);
}

Hawk::flt_t Hawk::mod (awk_t* awk, flt_t x, flt_t y)
{
	xtn_t* xtn = GET_XTN(awk);
	return xtn->awk->mod (x, y);
}

void* Hawk::modopen (awk_t* awk, const mod_spec_t* spec)
{
	xtn_t* xtn = GET_XTN(awk);
	return xtn->awk->modopen (spec);
}

void Hawk::modclose (awk_t* awk, void* handle)
{
	xtn_t* xtn = GET_XTN(awk);
	xtn->awk->modclose (handle);
}

void* Hawk::modsym (awk_t* awk, void* handle, const char_t* name)
{
	xtn_t* xtn = GET_XTN(awk);
	return xtn->awk->modsym (handle, name);
}
/////////////////////////////////
HAWK_END_NAMESPACE(HAWK)
/////////////////////////////////
