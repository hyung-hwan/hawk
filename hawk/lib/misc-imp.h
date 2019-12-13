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

/* this file is supposed to be included by misc.c */


hawk_int_t awk_strxtoint (hawk_t* awk, const char_t* str, hawk_oow_t len, int base, const char_t** endptr)
{
	hawk_int_t n = 0;
	const char_t* p;
	const char_t* end;
	hawk_oow_t rem;
	int digit, negative = 0;

	HAWK_ASSERT (awk, base < 37); 

	p = str; 
	end = str + len;
	
	if (awk->opt.trait & HAWK_STRIPSTRSPC)
	{
		/* strip off leading spaces */
		while (p < end && AWK_IS_SPACE(*p)) p++;
	}

	/* check for a sign */
	while (p < end)
	{
		if (*p == _T('-')) 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == _T('+')) p++;
		else break;
	}

	/* check for a binary/octal/hexadecimal notation */
	rem = end - p;
	if (base == 0) 
	{
		if (rem >= 1 && *p == _T('0')) 
		{
			p++;

			if (rem == 1) base = 8;
			else if (*p == _T('x') || *p == _T('X'))
			{
				p++; base = 16;
			} 
			else if (*p == _T('b') || *p == _T('B'))
			{
				p++; base = 2;
			}
			else base = 8;
		}
		else base = 10;
	} 
	else if (rem >= 2 && base == 16)
	{
		if (*p == _T('0') && 
		    (*(p+1) == _T('x') || *(p+1) == _T('X'))) p += 2; 
	}
	else if (rem >= 2 && base == 2)
	{
		if (*p == _T('0') && 
		    (*(p+1) == _T('b') || *(p+1) == _T('B'))) p += 2; 
	}

	/* process the digits */
	while (p < end)
	{
		if (*p >= _T('0') && *p <= _T('9'))
			digit = *p - _T('0');
		else if (*p >= _T('A') && *p <= _T('Z'))
			digit = *p - _T('A') + 10;
		else if (*p >= _T('a') && *p <= _T('z'))
			digit = *p - _T('a') + 10;
		else break;

		if (digit >= base) break;
		n = n * base + digit;

		p++;
	}

	if (endptr) *endptr = p;
	return (negative)? -n: n;
}



/*
 * hawk_strtoflt is almost a replica of strtod.
 *
 * strtod.c --
 *
 *      Source code for the "strtod" library procedure.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/*
 *                double(64bits)    extended(80-bits)    quadruple(128-bits)
 *  exponent      11 bits           15 bits              15 bits
 *  fraction      52 bits           63 bits              112 bits
 *  sign          1 bit             1 bit                1 bit
 *  integer                         1 bit
 */         
#define MAX_EXPONENT 511

hawk_flt_t awk_strtoflt (hawk_t* awk, const char_t* str)
{
	/* 
	 * Table giving binary powers of 10. Entry is 10^2^i.  
	 * Used to convert decimal exponents into floating-point numbers.
	 */ 
	static hawk_flt_t powers_of_10[] = 
	{
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	hawk_flt_t fraction, dbl_exp, * d;
	const char_t* p;
	cint_t c;
	int exp = 0;		/* Exponent read from "EX" field */

	/* 
	 * Exponent that derives from the fractional part.  Under normal 
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However, if I is very long, the last digits of I get dropped 
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, frac_exp is 
	 * incremented one for each dropped digit. 
	 */

	int frac_exp;
	int mant_size; /* Number of digits in mantissa. */
	int dec_pt;    /* Number of mantissa digits BEFORE decimal point */
	const char_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;

	if (awk->opt.trait & HAWK_STRIPSTRSPC)
	{
		/* strip off leading spaces */ 
		while (AWK_IS_SPACE(*p)) p++;
	}

	/* check for a sign */
	while (*p != _T('\0')) 
	{
		if (*p == _T('-')) 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == _T('+')) p++;
		else break;
	}

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	for (mant_size = 0; ; mant_size++) 
	{
		c = *p;
		if (!AWK_IS_DIGIT(c)) 
		{
			if ((c != _T('.')) || (dec_pt >= 0)) break;
			dec_pt = mant_size;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mant_size;
	if (dec_pt < 0) 
	{
		dec_pt = mant_size;
	} 
	else 
	{
		mant_size--;	/* One of the digits was the point */
	}

	if (mant_size > 18) 
	{
		frac_exp = dec_pt - 18;
		mant_size = 18;
	} 
	else 
	{
		frac_exp = dec_pt - mant_size;
	}

	if (mant_size == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		p = pexp;
		goto done;
	} 
	else 
	{
		int frac1, frac2;
		frac1 = 0;
		for ( ; mant_size > 9; mant_size--) 
		{
			c = *p;
			p++;
			if (c == _T('.')) 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - _T('0'));
		}
		frac2 = 0;
		for (; mant_size > 0; mant_size--) {
			c = *p;
			p++;
			if (c == _T('.')) 
			{
				c = *p;
				p++;
			}
			frac2 = 10*frac2 + (c - _T('0'));
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if ((*p == _T('E')) || (*p == _T('e'))) 
	{
		p++;
		if (*p == _T('-')) 
		{
			exp_negative = 1;
			p++;
		} 
		else 
		{
			if (*p == _T('+')) p++;
			exp_negative = 0;
		}
		if (!AWK_IS_DIGIT(*p)) 
		{
			/* p = pexp; */
			/* goto done; */
			goto no_exp;
		}
		while (AWK_IS_DIGIT(*p)) 
		{
			exp = exp * 10 + (*p - _T('0'));
			p++;
		}
	}

no_exp:
	if (exp_negative) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		exp_negative = 1;
		exp = -exp;
	} 
	else exp_negative = 0;

	if (exp > MAX_EXPONENT) exp = MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	return (negative)? -fraction: fraction;
}

hawk_flt_t awk_strxtoflt (hawk_t* awk, const char_t* str, hawk_oow_t len, const char_t** endptr)
{
	/* 
	 * Table giving binary powers of 10. Entry is 10^2^i.  
	 * Used to convert decimal exponents into floating-point numbers.
	 */ 
	static hawk_flt_t powers_of_10[] = 
	{
		10.,    100.,   1.0e4,   1.0e8,   1.0e16,
		1.0e32, 1.0e64, 1.0e128, 1.0e256
	};

	hawk_flt_t fraction, dbl_exp, * d;
	const char_t* p, * end;
	cint_t c;
	int exp = 0; /* Exponent read from "EX" field */

	/* 
	 * Exponent that derives from the fractional part.  Under normal 
	 * circumstatnces, it is the negative of the number of digits in F.
	 * However, if I is very long, the last digits of I get dropped 
	 * (otherwise a long I with a large negative exponent could cause an
	 * unnecessary overflow on I alone).  In this case, frac_exp is 
	 * incremented one for each dropped digit. 
	 */

	int frac_exp;
	int mant_size; /* Number of digits in mantissa. */
	int dec_pt;    /* Number of mantissa digits BEFORE decimal point */
	const char_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;
	end = str + len;

	/* Strip off leading blanks and check for a sign */
	/*while (AWK_IS_SPACE(*p)) p++;*/

	/*while (*p != _T('\0')) */
	while (p < end)
	{
		if (*p == _T('-')) 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == _T('+')) p++;
		else break;
	}

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	/*for (mant_size = 0; ; mant_size++) */
	for (mant_size = 0; p < end; mant_size++) 
	{
		c = *p;
		if (!AWK_IS_DIGIT(c)) 
		{
			if (c != _T('.') || dec_pt >= 0) break;
			dec_pt = mant_size;
		}
		p++;
	}

	/*
	 * Now suck up the digits in the mantissa.  Use two integers to
	 * collect 9 digits each (this is faster than using floating-point).
	 * If the mantissa has more than 18 digits, ignore the extras, since
	 * they can't affect the value anyway.
	 */
	pexp = p;
	p -= mant_size;
	if (dec_pt < 0) 
	{
		dec_pt = mant_size;
	} 
	else 
	{
		mant_size--;	/* One of the digits was the point */
	}

	if (mant_size > 18)  /* TODO: is 18 correct for hawk_flt_t??? */
	{
		frac_exp = dec_pt - 18;
		mant_size = 18;
	} 
	else 
	{
		frac_exp = dec_pt - mant_size;
	}

	if (mant_size == 0) 
	{
		fraction = 0.0;
		/*p = str;*/
		p = pexp;
		goto done;
	} 
	else 
	{
		int frac1, frac2;

		frac1 = 0;
		for ( ; mant_size > 9; mant_size--) 
		{
			c = *p;
			p++;
			if (c == _T('.')) 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - _T('0'));
		}

		frac2 = 0;
		for (; mant_size > 0; mant_size--) {
			c = *p++;
			if (c == _T('.')) 
			{
				c = *p;
				p++;
			}
			frac2 = 10 * frac2 + (c - _T('0'));
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (p < end && (*p == _T('E') || *p == _T('e'))) 
	{
		p++;

		if (p < end) 
		{
			if (*p == _T('-')) 
			{
				exp_negative = 1;
				p++;
			} 
			else 
			{
				if (*p == _T('+')) p++;
				exp_negative = 0;
			}
		}
		else exp_negative = 0;

		if (!(p < end && AWK_IS_DIGIT(*p))) 
		{
			/*p = pexp;*/
			/*goto done;*/
			goto no_exp;
		}

		while (p < end && AWK_IS_DIGIT(*p)) 
		{
			exp = exp * 10 + (*p - _T('0'));
			p++;
		}
	}

no_exp:
	if (exp_negative) exp = frac_exp - exp;
	else exp = frac_exp + exp;

	/*
	 * Generate a floating-point number that represents the exponent.
	 * Do this by processing the exponent one bit at a time to combine
	 * many powers of 2 of 10. Then combine the exponent with the
	 * fraction.
	 */
	if (exp < 0) 
	{
		exp_negative = 1;
		exp = -exp;
	} 
	else exp_negative = 0;

	if (exp > MAX_EXPONENT) exp = MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	if (endptr != HAWK_NULL) *endptr = p;
	return (negative)? -fraction: fraction;
}
