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

static int fmt_uintmax (
	char_t* buf, int size, 
	hawk_uintmax_t value, int base_and_flags, int prec,
	char_t fillchar, char_t signchar, const char_t* prefix)
{
	char_t tmp[(HAWK_SIZEOF(hawk_uintmax_t) * 8)];
	int reslen, base, fillsize, reqlen, pflen, preczero;
	char_t* p, * bp, * be;
	const hawk_bch_t* xbasestr;

	base = base_and_flags & 0x3F;
	if (base < 2 || base > 36) return -1;

	xbasestr = (base_and_flags & HAWK_FMT_INTMAX_UPPERCASE)?
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ":
		"0123456789abcdefghijklmnopqrstuvwxyz";

	if ((base_and_flags & HAWK_FMT_INTMAX_NOZERO) && value == 0) 
	{
		p = tmp; 
		if (base_and_flags & HAWK_FMT_INTMAX_ZEROLEAD) 
		{
			/* NOZERO emits no digit, ZEROLEAD emits 1 digit.
			 * so it emits '0' */
			reslen = 1;
			preczero = 1;
		}
		else
		{
			/* since the value is zero, emit no digits */
			reslen = 0;
			preczero = 0;
		}
	}
	else
	{
		hawk_uintmax_t v = value;

		/* store the resulting numeric string into 'tmp' first */
		p = tmp; 
		do
		{
			*p++ = xbasestr[v % base];
			v /= base;
		}
		while (v > 0);

		/* reslen is the length of the resulting string without padding. */
		reslen = (int)(p - tmp);
	
		/* precision specified the minum number of digits to produce.
		 * so if the precision is larger that the digits produced, 
		 * reslen should be adjusted to precision */
		if (prec > reslen) 
		{
			/* if the precision is greater than the actual digits
			 * made from the value, 0 is inserted in front.
			 * ZEROLEAD doesn't have to be handled explicitly
			 * since it's achieved effortlessly */
			preczero = prec - reslen;
			reslen = prec;
		}
		else 
		{
			preczero = 0;
			if ((base_and_flags & HAWK_FMT_INTMAX_ZEROLEAD) && value != 0) 
			{
				/* if value is zero, 0 is emitted from it. 
				 * so ZEROLEAD don't need to add another 0. */
				preczero++;
				reslen++;
			}
		}
	}

	if (signchar) reslen++; /* increment reslen for the sign character */
	if (prefix)
	{
		/* since the length can be truncated for different type sizes,
		 * don't pass in a very long prefix. */
		const char_t* pp;
		for (pp = prefix; *pp != '\0'; pp++) ;
		pflen = pp - prefix;
		reslen += pflen;
	}
	else pflen = 0;

	/* get the required buffer size for lossless formatting */
	reqlen = (base_and_flags & HAWK_FMT_INTMAX_NONULL)? reslen: (reslen + 1);

	if (size <= 0 ||
	    ((base_and_flags & HAWK_FMT_INTMAX_NOTRUNC) && size < reqlen))
	{
		return -reqlen;
	}

	/* get the size to fill with fill characters */
	fillsize = (base_and_flags & HAWK_FMT_INTMAX_NONULL)? size: (size - 1);
	bp = buf;
	be = buf + fillsize;

	/* fill space */
	if (fillchar != '\0')
	{
		if (base_and_flags & HAWK_FMT_INTMAX_FILLRIGHT)
		{
			/* emit sign */
			if (signchar && bp < be) *bp++ = signchar;

			/* copy prefix if necessary */
			if (prefix) while (*prefix && bp < be) *bp++ = *prefix++;

			/* add 0s for precision */
			while (preczero > 0 && bp < be) 
			{ 
				*bp++ = '0';
				preczero--; 
			}

			/* copy the numeric string to the destination buffer */
			while (p > tmp && bp < be) *bp++ = *--p;

			/* fill the right side */
			while (fillsize > reslen)
			{
				*bp++ = fillchar;
				fillsize--;
			}
		}
		else if (base_and_flags & HAWK_FMT_INTMAX_FILLCENTER)
		{
			/* emit sign */
			if (signchar && bp < be) *bp++ = signchar;

			/* fill the left side */
			while (fillsize > reslen)
			{
				*bp++ = fillchar;
				fillsize--;
			}

			/* copy prefix if necessary */
			if (prefix) while (*prefix && bp < be) *bp++ = *prefix++;

			/* add 0s for precision */
			while (preczero > 0 && bp < be) 
			{ 
				*bp++ = '0';
				preczero--; 
			}

			/* copy the numeric string to the destination buffer */
			while (p > tmp && bp < be) *bp++ = *--p;
		}
		else
		{
			/* fill the left side */
			while (fillsize > reslen)
			{
				*bp++ = fillchar;
				fillsize--;
			}

			/* emit sign */
			if (signchar && bp < be) *bp++ = signchar;

			/* copy prefix if necessary */
			if (prefix) while (*prefix && bp < be) *bp++ = *prefix++;

			/* add 0s for precision */
			while (preczero > 0 && bp < be) 
			{ 
				*bp++ = '0';
				preczero--; 
			}

			/* copy the numeric string to the destination buffer */
			while (p > tmp && bp < be) *bp++ = *--p;
		}
	}
	else
	{
		/* emit sign */
		if (signchar && bp < be) *bp++ = signchar;

		/* copy prefix if necessary */
		if (prefix) while (*prefix && bp < be) *bp++ = *prefix++;

		/* add 0s for precision */
		while (preczero > 0 && bp < be) 
		{ 
			*bp++ = '0';
			preczero--; 
		}

		/* copy the numeric string to the destination buffer */
		while (p > tmp && bp < be) *bp++ = *--p;
	}

	if (!(base_and_flags & HAWK_FMT_INTMAX_NONULL)) *bp = '\0';
	return bp - buf;
}
