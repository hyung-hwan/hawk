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

#include <hawk-fmt.h>
#include <hawk-utl.h>

#undef T
#undef char_t
#undef fmt_uintmax
#undef strlen

#define T(x) HAWK_BT(x)
#define char_t hawk_bch_t
#define fmt_uintmax fmt_unsigned_to_mbs
#define strlen(x) hawk_count_bcs(x)
#include "fmt-intmax.h"

#undef T
#undef char_t
#undef fmt_uintmax
#undef strlen

#define T(x) HAWK_UT(x)
#define char_t hawk_uch_t
#define fmt_uintmax fmt_unsigned_to_wcs
#define strlen(x) hawk_count_ucs(x)
#include "fmt-intmax.h"

/* ==================== multibyte ===================================== */

int hawk_fmtintmaxtombs (
	hawk_bch_t* buf, int size, 
	hawk_intmax_t value, int base_and_flags, int prec,
	hawk_bch_t fillchar, const hawk_bch_t* prefix)
{
	hawk_bch_t signchar;
	hawk_uintmax_t absvalue;

	if (value < 0)
	{
		signchar = HAWK_BT('-');
		absvalue = -value;
	}
	else if (base_and_flags & HAWK_FMTINTMAXTOMBS_PLUSSIGN)
	{
		signchar = HAWK_BT('+');
		absvalue = value;
	}
	else if (base_and_flags & HAWK_FMTINTMAXTOMBS_EMPTYSIGN)
	{
		signchar = HAWK_BT(' ');
		absvalue = value;
	}
	else
	{
		signchar = HAWK_BT('\0');
		absvalue = value;
	}

	return fmt_unsigned_to_mbs(buf, size, absvalue, base_and_flags, prec, fillchar, signchar, prefix);
}

int hawk_fmtuintmaxtombs (
	hawk_bch_t* buf, int size, 
	hawk_uintmax_t value, int base_and_flags, int prec,
	hawk_bch_t fillchar, const hawk_bch_t* prefix)
{
	hawk_bch_t signchar;

	/* determine if a sign character is needed */
	if (base_and_flags & HAWK_FMTINTMAXTOMBS_PLUSSIGN)
	{
		signchar = HAWK_BT('+');
	}
	else if (base_and_flags & HAWK_FMTINTMAXTOMBS_EMPTYSIGN)
	{
		signchar = HAWK_BT(' ');
	}
	else
	{
		signchar = HAWK_BT('\0');
	}

	return fmt_unsigned_to_mbs(buf, size, value, base_and_flags, prec, fillchar, signchar, prefix);
}

/* ==================== wide-char ===================================== */

int hawk_fmtintmaxtowcs (
	hawk_uch_t* buf, int size, 
	hawk_intmax_t value, int base_and_flags, int prec,
	hawk_uch_t fillchar, const hawk_uch_t* prefix)
{
	hawk_uch_t signchar;
	hawk_uintmax_t absvalue;

	if (value < 0)
	{
		signchar = HAWK_UT('-');
		absvalue = -value;
	}
	else if (base_and_flags & HAWK_FMTINTMAXTOWCS_PLUSSIGN)
	{
		signchar = HAWK_UT('+');
		absvalue = value;
	}
	else if (base_and_flags & HAWK_FMTINTMAXTOMBS_EMPTYSIGN)
	{
		signchar = HAWK_UT(' ');
		absvalue = value;
	}
	else
	{
		signchar = HAWK_UT('\0');
		absvalue = value;
	}

	return fmt_unsigned_to_wcs(buf, size, absvalue, base_and_flags, prec, fillchar, signchar, prefix);
}

int hawk_fmtuintmaxtowcs (
	hawk_uch_t* buf, int size, 
	hawk_uintmax_t value, int base_and_flags, int prec,
	hawk_uch_t fillchar, const hawk_uch_t* prefix)
{
	hawk_uch_t signchar;

	/* determine if a sign character is needed */
	if (base_and_flags & HAWK_FMTINTMAXTOWCS_PLUSSIGN)
	{
		signchar = HAWK_UT('+');
	}
	else if (base_and_flags & HAWK_FMTINTMAXTOMBS_EMPTYSIGN)
	{
		signchar = HAWK_UT(' ');
	}
	else
	{
		signchar = HAWK_UT('\0');
	}

	return fmt_unsigned_to_wcs(buf, size, value, base_and_flags, prec, fillchar, signchar, prefix);
}


/* ==================== floating-point number =========================== */

/* TODO: finish this function */
int hawk_fmtfltmaxtombs (hawk_bch_t* buf, hawk_oow_t bufsize, hawk_fltmax_t f, hawk_bch_t point, int digits)
{
	hawk_oow_t len;
	hawk_uintmax_t v;

	/*if (bufsize <= 0) return -reqlen; TODO: */

	if (f < 0) 
	{
		f *= -1;
		v = (hawk_uintmax_t)f;  
		len = hawk_fmtuintmaxtombs (buf, bufsize, v, 10, 0, HAWK_BT('\0'), HAWK_BT("-"));
	}
	else
	{
		v = (hawk_uintmax_t)f;  
		len = hawk_fmtuintmaxtombs (buf, bufsize, v, 10, 0, HAWK_BT('\0'), HAWK_NULL);
	}
 
	if (len + 1 < bufsize)
	{
		buf[len++] = point;  /* add decimal point to string */
		buf[len] = HAWK_BT('\0');
	}
        
	while (len + 1 < bufsize && digits-- > 0)
	{
		f = (f - (hawk_fltmax_t)v) * 10;
		v = (hawk_uintmax_t)f;
		len += hawk_fmtuintmaxtombs(&buf[len], bufsize - len, v, 10, 0, HAWK_BT('\0'), HAWK_NULL);
	}

	return (int)len;
}

