/*
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

#include <hawk-utl.h>
#include <hawk-chr.h>

void hawk_unescape_ucstr (hawk_uch_t* str)
{
	hawk_uch_t c, * p1, * p2;
	hawk_uint32_t c_acc;
	int escaped = 0, digit_count;

	p1 = str;
	p2 = str;
	while ((c = *p1++) != '\0')
	{
		if (escaped == 3)
		{
			/* octal */
			if (c >= '0' && c <= '7')
			{
				c_acc = c_acc * 8 + c - '0';
				digit_count++;
				
				if (digit_count >= escaped) 
				{
					/* should i limit the max to 0xFF/0377? 
					 if (c_acc > 0377) c_acc = 0377; */
					escaped = 0;
					*p2++ = c_acc;
				}
				continue;
			}
			else
			{
				escaped = 0;
				*p2++ = c_acc;
				goto normal_char;
			}
		}
		else if (escaped == 2 || escaped == 4 || escaped == 8)
		{
			/* hexadecimal */
			if (c >= '0' && c <= '9')
			{
				c_acc = c_acc * 16 + c - '0';
				digit_count++;
				if (digit_count >= escaped) 
				{
					*p2++ = c_acc;
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'A' && c <= 'F')
			{
				c_acc = c_acc * 16 + c - 'A' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					*p2++ = c_acc;
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'a' && c <= 'f')
			{
				c_acc = c_acc * 16 + c - 'a' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					*p2++ = c_acc;
					escaped = 0;
				}
				continue;
			}
			else
			{
				if (digit_count == 0) 
				{
					/* no valid character after the escaper.
					 * keep the escaper as it is. consider this input:
					 *   \xGG
					 * 'c' is at the first G. this part is to restore the
					 * \x part. since \x is not followed by any hexadecimal
					 * digits, it's literally 'x' */
					*p2++ = (escaped == 2)? 'x':
				             (escaped == 4)? 'u': 'U';
				}
				else *p2++ = c_acc;

				escaped = 0;
				goto normal_char;
			}
		}

		if (escaped == 1)
		{
			switch (c) 
			{
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'f': c = '\f'; break;
				case 'b': c = '\b'; break;
				case 'v': c = '\v'; break;
				case 'a': c = '\a'; break;

				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					escaped = 3;
					digit_count = 1;
					c_acc = c - '0';
					continue;

				case 'x':
					escaped = 2;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'u':
					escaped = 4;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'U':
					escaped = 8;
					digit_count = 0;
					c_acc = 0;
					continue;
				}

			*p2++ = c;
			escaped = 0;
			continue;
		}

	normal_char:
		if (c == '\\') 
		{
			escaped = 1;
			continue;
		}

		*p2++ = c;
	}

	*p2 = '\0';
}

/* ------------------------------------------------------------------------ */

void hawk_unescape_bcstr (hawk_bch_t* str)
{
	hawk_bch_t c, * p1, * p2;
	hawk_uint32_t c_acc;
	int escaped = 0, digit_count;
	hawk_cmgr_t* utf8_cmgr;

	utf8_cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	p1 = str;
	p2 = str;
	while ((c = *p1++) != '\0')
	{
		if (escaped == 3)
		{
			/* octal */
			if (c >= '0' && c <= '7')
			{
				c_acc = c_acc * 8 + c - '0';
				digit_count++;
				
				if (digit_count >= escaped) 
				{
					/* should i limit the max to 0xFF/0377? 
					 if (c_acc > 0377) c_acc = 0377; */
					escaped = 0;
					*p2++ = c_acc;
				}
				continue;
			}
			else
			{
				escaped = 0;
				*p2++ = c_acc;
				goto normal_char;
			}
		}
		else if (escaped == 2 || escaped == 4 || escaped == 8)
		{
			/* hexadecimal */
			if (c >= '0' && c <= '9')
			{
				c_acc = c_acc * 16 + c - '0';
				digit_count++;
				if (digit_count >= escaped) 
				{
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'A' && c <= 'F')
			{
				c_acc = c_acc * 16 + c - 'A' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
					escaped = 0;
				}
				continue;
			}
			else if (c >= 'a' && c <= 'f')
			{
				c_acc = c_acc * 16 + c - 'a' + 10;
				digit_count++;
				if (digit_count >= escaped) 
				{
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
					escaped = 0;
				}
				continue;
			}
			else
			{
				/* non digit or xdigit */
				
				if (digit_count == 0) 
				{
					/* no valid character after the escaper.
					 * keep the escaper as it is. consider this input:
					 *   \xGG
					 * 'c' is at the first G. this part is to restore the
					 * \x part. since \x is not followed by any hexadecimal
					 * digits, it's literally 'x' */
					*p2++ = (escaped == 2)? 'x':
					        (escaped == 4)? 'u': 'U';
				}
				else 
				{
					/* for a unicode, the utf8 conversion can never outgrow the input string of the hexadecimal notation with an escaper.
					 * so it must be safe to specify a very large buffer size to uctobc() */
					if (escaped == 2) *p2++ = c_acc;
					else p2 += utf8_cmgr->uctobc(c_acc, p2, HAWK_TYPE_MAX(hawk_oow_t));
				}

				escaped = 0;
				goto normal_char;
			}
		}

		if (escaped == 1)
		{
			switch (c) 
			{
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'f': c = '\f'; break;
				case 'b': c = '\b'; break;
				case 'v': c = '\v'; break;
				case 'a': c = '\a'; break;

				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					escaped = 3;
					digit_count = 1;
					c_acc = c - '0';
					continue;

				case 'x':
					escaped = 2;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'u':
					escaped = 4;
					digit_count = 0;
					c_acc = 0;
					continue;

				case 'U':
					escaped = 8;
					digit_count = 0;
					c_acc = 0;
					continue;
				}

			*p2++ = c;
			escaped = 0;
			continue;
		}

	normal_char:
		if (c == '\\') 
		{
			escaped = 1;
			continue;
		}

		*p2++ = c;
	}

	*p2 = '\0';
}


/* ----------------------------------------------------------------------- */

static const hawk_uch_t* scan_dollar_for_subst_u (const hawk_uch_t* f, hawk_oow_t l, hawk_ucs_t* ident, hawk_ucs_t* dfl, int depth)
{
	const hawk_uch_t* end = f + l;

	HAWK_ASSERT (l >= 2);
	
	f += 2; /* skip ${ */ 
	if (ident) ident->ptr = (hawk_uch_t*)f;

	while (1)
	{
		if (f >= end) return HAWK_NULL;
		if (*f == '}' || *f == ':') break;
		f++;
	}

	if (*f == ':')
	{
		if (f >= end || *(f + 1) != '=') 
		{
			/* not := */
			return HAWK_NULL; 
		}

		if (ident) ident->len = f - ident->ptr;

		f += 2; /* skip := */

		if (dfl) dfl->ptr = (hawk_uch_t*)f;
		while (1)
		{
			if (f >= end) return HAWK_NULL;

			else if (*f == '$' && *(f + 1) == '{')
			{
				if (depth >= 64) return HAWK_NULL; /* depth too deep */

				/* TODO: remove recursion */
				f = scan_dollar_for_subst_u(f, end - f, HAWK_NULL, HAWK_NULL, depth + 1);
				if (!f) return HAWK_NULL;
			}
			else if (*f == '}') 
			{
				/* ending bracket  */
				if (dfl) dfl->len = f - dfl->ptr;
				return f + 1;
			}
			else f++;
		}
	}
	else if (*f == '}') 
	{
		if (ident) ident->len = f - ident->ptr;
		if (dfl) 
		{
			dfl->ptr = HAWK_NULL;
			dfl->len = 0;
		}
		return f + 1;
	}

	/* this part must not be reached */
	return HAWK_NULL;
}

static hawk_uch_t* exapnd_dollar_for_subst_u (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_ucs_t* ident, const hawk_ucs_t* dfl, hawk_subst_for_ucs_t subst, void* ctx)
{
	hawk_uch_t* tmp;

	tmp = subst(buf, bsz, ident, ctx);
	if (tmp == HAWK_NULL)
	{
		/* substitution failed */
		if (dfl->len > 0)
		{
			/* take the default value */
			hawk_oow_t len;

			/* TODO: remove recursion */
			len = hawk_subst_for_uchars_to_ucstr(buf, bsz, dfl->ptr, dfl->len, subst, ctx);
			tmp = buf + len;
		}
		else tmp = buf;
	}

	return tmp;
}

hawk_oow_t hawk_subst_for_uchars_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, hawk_oow_t fsz, hawk_subst_for_ucs_t subst, void* ctx)
{
	hawk_uch_t* b = buf;
	hawk_uch_t* end = buf + bsz - 1;
	const hawk_uch_t* f = fmt;
	const hawk_uch_t* fend = fmt + fsz;

	if (buf != HAWK_SUBST_NOBUF && bsz <= 0) return 0;

	while (f < fend)
	{
		if (*f == '$' && f < fend - 1)
		{
			if (*(f + 1) == '{')
			{
				const hawk_uch_t* tmp;
				hawk_ucs_t ident, dfl;

				tmp = scan_dollar_for_subst_u(f, fend - f, &ident, &dfl, 0);
				if (!tmp || ident.len <= 0) goto normal;
				f = tmp;

				if (buf != HAWK_SUBST_NOBUF)
				{
					b = exapnd_dollar_for_subst_u(b, end - b + 1, &ident, &dfl, subst, ctx);
					if (b >= end) goto fini;
				}
				else
				{
					/* the buffer points to HAWK_SUBST_NOBUF. */
					tmp = exapnd_dollar_for_subst_u(buf, bsz, &ident, &dfl, subst, ctx);
					/* increment b by the length of the expanded string */
					b += (tmp - buf);
				}

				continue;
			}
			else if (*(f + 1) == '$') 
			{
				/* $$ -> $. */
				f++;
			}
		}

	normal:
		if (buf != HAWK_SUBST_NOBUF)
		{
			if (b >= end) break;
			*b = *f;
		}
		b++; f++;
	}

fini:
	if (buf != HAWK_SUBST_NOBUF) *b = '\0';
	return b - buf;
}

hawk_oow_t hawk_subst_for_ucstr_to_ucstr (hawk_uch_t* buf, hawk_oow_t bsz, const hawk_uch_t* fmt, hawk_subst_for_ucs_t subst, void* ctx)
{
	return hawk_subst_for_uchars_to_ucstr(buf, bsz, fmt, hawk_count_ucstr(fmt), subst, ctx);
}

/* ------------------------------------------------------------------------ */

static const hawk_bch_t* scan_dollar_for_subst_b (const hawk_bch_t* f, hawk_oow_t l, hawk_bcs_t* ident, hawk_bcs_t* dfl, int depth)
{
	const hawk_bch_t* end = f + l;

	HAWK_ASSERT (l >= 2);
	
	f += 2; /* skip ${ */ 
	if (ident) ident->ptr = (hawk_bch_t*)f;

	while (1)
	{
		if (f >= end) return HAWK_NULL;
		if (*f == '}' || *f == ':') break;
		f++;
	}

	if (*f == ':')
	{
		if (f >= end || *(f + 1) != '=') 
		{
			/* not := */
			return HAWK_NULL; 
		}

		if (ident) ident->len = f - ident->ptr;

		f += 2; /* skip := */

		if (dfl) dfl->ptr = (hawk_bch_t*)f;
		while (1)
		{
			if (f >= end) return HAWK_NULL;

			else if (*f == '$' && *(f + 1) == '{')
			{
				if (depth >= 64) return HAWK_NULL; /* depth too deep */

				/* TODO: remove recursion */
				f = scan_dollar_for_subst_b(f, end - f, HAWK_NULL, HAWK_NULL, depth + 1);
				if (!f) return HAWK_NULL;
			}
			else if (*f == '}') 
			{
				/* ending bracket  */
				if (dfl) dfl->len = f - dfl->ptr;
				return f + 1;
			}
			else f++;
		}
	}
	else if (*f == '}') 
	{
		if (ident) ident->len = f - ident->ptr;
		if (dfl) 
		{
			dfl->ptr = HAWK_NULL;
			dfl->len = 0;
		}
		return f + 1;
	}

	/* this part must not be reached */
	return HAWK_NULL;
}

static hawk_bch_t* exapnd_dollar_for_subst_b (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bcs_t* ident, const hawk_bcs_t* dfl, hawk_subst_for_bcs_t subst, void* ctx)
{
	hawk_bch_t* tmp;

	tmp = subst(buf, bsz, ident, ctx);
	if (tmp == HAWK_NULL)
	{
		/* substitution failed */
		if (dfl->len > 0)
		{
			/* take the default value */
			hawk_oow_t len;

			/* TODO: remove recursion */
			len = hawk_subst_for_bchars_to_bcstr(buf, bsz, dfl->ptr, dfl->len, subst, ctx);
			tmp = buf + len;
		}
		else tmp = buf;
	}

	return tmp;
}

hawk_oow_t hawk_subst_for_bchars_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, hawk_oow_t fsz, hawk_subst_for_bcs_t subst, void* ctx)
{
	hawk_bch_t* b = buf;
	hawk_bch_t* end = buf + bsz - 1;
	const hawk_bch_t* f = fmt;
	const hawk_bch_t* fend = fmt + fsz;

	if (buf != HAWK_SUBST_NOBUF && bsz <= 0) return 0;

	while (f < fend)
	{
		if (*f == '$' && f < fend - 1)
		{
			if (*(f + 1) == '{')
			{
				const hawk_bch_t* tmp;
				hawk_bcs_t ident, dfl;

				tmp = scan_dollar_for_subst_b(f, fend - f, &ident, &dfl, 0);
				if (!tmp || ident.len <= 0) goto normal;
				f = tmp;

				if (buf != HAWK_SUBST_NOBUF)
				{
					b = exapnd_dollar_for_subst_b(b, end - b + 1, &ident, &dfl, subst, ctx);
					if (b >= end) goto fini;
				}
				else
				{
					/* the buffer points to HAWK_SUBST_NOBUF. */
					tmp = exapnd_dollar_for_subst_b(buf, bsz, &ident, &dfl, subst, ctx);
					/* increment b by the length of the expanded string */
					b += (tmp - buf);
				}

				continue;
			}
			else if (*(f + 1) == '$') 
			{
				/* $$ -> $. */
				f++;
			}
		}

	normal:
		if (buf != HAWK_SUBST_NOBUF)
		{
			if (b >= end) break;
			*b = *f;
		}
		b++; f++;
	}

fini:
	if (buf != HAWK_SUBST_NOBUF) *b = '\0';
	return b - buf;
}

hawk_oow_t hawk_subst_for_bcstr_to_bcstr (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bch_t* fmt, hawk_subst_for_bcs_t subst, void* ctx)
{
	return hawk_subst_for_bchars_to_bcstr(buf, bsz, fmt, hawk_count_bcstr(fmt), subst, ctx);
}

















/*
 * hawk_uchars_to_flt/hawk_bchars_to_flt is almost a replica of strtod.
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
#define FLT_MAX_EXPONENT 511

hawk_flt_t hawk_uchars_to_flt (const hawk_uch_t* str, hawk_oow_t len, const hawk_uch_t** endptr, int stripspc)
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
	const hawk_uch_t* p, * end;
	hawk_uci_t c;
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
	const hawk_uch_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;
	end = str + len;

	/* Strip off leading blanks and check for a sign */
	/*while (AWK_IS_SPACE(*p)) p++;*/
	if (stripspc)
	{
		/* strip off leading spaces */ 
		while (p < end && hawk_is_uch_space(*p)) p++;
	}

	/*while (*p != _T('\0')) */
	while (p < end)
	{
		if (*p == '-') 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == '+') p++;
		else break;
	}

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	/*for (mant_size = 0; ; mant_size++) */
	for (mant_size = 0; p < end; mant_size++) 
	{
		c = *p;
		if (!hawk_is_uch_digit(c)) 
		{
			if (c != '.' || dec_pt >= 0) break;
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
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - '0');
		}

		frac2 = 0;
		for (; mant_size > 0; mant_size--) 
		{
			c = *p++;
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (p < end && (*p == 'E' || *p == 'e')) 
	{
		p++;

		if (p < end) 
		{
			if (*p == '-') 
			{
				exp_negative = 1;
				p++;
			} 
			else 
			{
				if (*p == '+') p++;
				exp_negative = 0;
			}
		}
		else exp_negative = 0;

		if (!(p < end && hawk_is_uch_digit(*p))) 
		{
			/*p = pexp;*/
			/*goto done;*/
			goto no_exp;
		}

		while (p < end && hawk_is_uch_digit(*p)) 
		{
			exp = exp * 10 + (*p - '0');
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

	if (exp > FLT_MAX_EXPONENT) exp = FLT_MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	if (stripspc)
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_uch_space(*p)) p++;
	}
	if (endptr) *endptr = p;
	return (negative)? -fraction: fraction;
}

hawk_flt_t hawk_bchars_to_flt (const hawk_bch_t* str, hawk_oow_t len, const hawk_bch_t** endptr, int stripspc)
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
	const hawk_bch_t* p, * end;
	hawk_bci_t c;
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
	const hawk_bch_t *pexp;  /* Temporarily holds location of exponent in string */
	int negative = 0, exp_negative = 0;

	p = str;
	end = str + len;

	/* Strip off leading blanks and check for a sign */
	/*while (AWK_IS_SPACE(*p)) p++;*/
	if (stripspc)
	{
		/* strip off leading spaces */ 
		while (p < end && hawk_is_bch_space(*p)) p++;
	}

	/*while (*p != _T('\0')) */
	while (p < end)
	{
		if (*p == '-') 
		{
			negative = ~negative;
			p++;
		}
		else if (*p == '+') p++;
		else break;
	}

	/* Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point. */
	dec_pt = -1;
	/*for (mant_size = 0; ; mant_size++) */
	for (mant_size = 0; p < end; mant_size++) 
	{
		c = *p;
		if (!hawk_is_bch_digit(c)) 
		{
			if (c != '.' || dec_pt >= 0) break;
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
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac1 = 10 * frac1 + (c - '0');
		}

		frac2 = 0;
		for (; mant_size > 0; mant_size--) 
		{
			c = *p++;
			if (c == '.') 
			{
				c = *p;
				p++;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/* Skim off the exponent */
	p = pexp;
	if (p < end && (*p == 'E' || *p == 'e')) 
	{
		p++;

		if (p < end) 
		{
			if (*p == '-') 
			{
				exp_negative = 1;
				p++;
			} 
			else 
			{
				if (*p == '+') p++;
				exp_negative = 0;
			}
		}
		else exp_negative = 0;

		if (!(p < end && hawk_is_bch_digit(*p))) 
		{
			/*p = pexp;*/
			/*goto done;*/
			goto no_exp;
		}

		while (p < end && hawk_is_bch_digit(*p)) 
		{
			exp = exp * 10 + (*p - '0');
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

	if (exp > FLT_MAX_EXPONENT) exp = FLT_MAX_EXPONENT;

	dbl_exp = 1.0;

	for (d = powers_of_10; exp != 0; exp >>= 1, d++) 
	{
		if (exp & 01) dbl_exp *= *d;
	}

	if (exp_negative) fraction /= dbl_exp;
	else fraction *= dbl_exp;

done:
	if (stripspc)
	{
		/* consume trailing spaces */
		while (p < end && hawk_is_bch_space(*p)) p++;
	}
	if (endptr) *endptr = p;
	return (negative)? -fraction: fraction;
}

int hawk_uchars_to_num (int option, const hawk_uch_t* ptr, hawk_oow_t len, hawk_int_t* l, hawk_flt_t* r)
{
	const hawk_uch_t* endptr;
	const hawk_uch_t* end;

	int nopartial = HAWK_OOCHARS_TO_NUM_GET_OPTION_NOPARTIAL(option);
	int reqsober = HAWK_OOCHARS_TO_NUM_GET_OPTION_REQSOBER(option);
	int stripspc = HAWK_OOCHARS_TO_NUM_GET_OPTION_STRIPSPC(option);
	int base = HAWK_OOCHARS_TO_NUM_GET_OPTION_BASE(option);
	int is_sober;

	end = ptr + len;
	*l = hawk_uchars_to_int(ptr, len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc,0,base), &endptr, &is_sober);
	if (endptr < end)
	{
		if (*endptr == '.' || *endptr == 'E' || *endptr == 'e')
		{
		handle_float:
			*r = hawk_uchars_to_flt(ptr, len, &endptr, stripspc);
			if (nopartial && endptr < end) return -1;
			return 1; /* flt */
		}
		else if (hawk_is_uch_digit(*endptr))
		{
			const hawk_uch_t* p = endptr;
			do { p++; } while (p < end && hawk_is_uch_digit(*p));
			if (p < end && (*p == '.' || *p == 'E' || *p == 'e')) 
			{
				/* it's probably an floating-point number. 
				 * 
 				 *  BEGIN { b=99; printf "%f\n", (0 b 1.112); }
				 *
				 * for the above code, this function gets '0991.112'.
				 * and endptr points to '9' after hawk_uchars_to_int() as
				 * the numeric string beginning with 0 is treated 
				 * as an octal number. 
				 * 
				 * getting side-tracked, 
				 *   BEGIN { b=99; printf "%f\n", (0 b 1.000); }
				 * the above code cause this function to get 0991, not 0991.000
				 * because of the default CONVFMT '%.6g' which doesn't produce '.000'.
				 * so such a number is not treated as a floating-point number.
				 */
				goto handle_float;
			}
		}

		if (stripspc)
		{
			while (endptr < end && hawk_is_uch_space(*endptr)) endptr++;
		}
	}

	if (reqsober && !is_sober) return -1;
	if (nopartial && endptr < end) return -1;
	return 0; /* int */
}

int hawk_bchars_to_num (int option, const hawk_bch_t* ptr, hawk_oow_t len, hawk_int_t* l, hawk_flt_t* r)
{
	const hawk_bch_t* endptr;
	const hawk_bch_t* end;

	int nopartial = HAWK_OOCHARS_TO_NUM_GET_OPTION_NOPARTIAL(option);
	int reqsober = HAWK_OOCHARS_TO_NUM_GET_OPTION_REQSOBER(option);
	int stripspc = HAWK_OOCHARS_TO_NUM_GET_OPTION_STRIPSPC(option);
	int base = HAWK_OOCHARS_TO_NUM_GET_OPTION_BASE(option);
	int is_sober;

	end = ptr + len;
	*l = hawk_bchars_to_int(ptr, len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(stripspc,0,base), &endptr, &is_sober);
	if (endptr < end)
	{
		if (*endptr == '.' || *endptr == 'E' || *endptr == 'e')
		{
		handle_float:
			*r = hawk_bchars_to_flt(ptr, len, &endptr, stripspc);
			if (nopartial && endptr < end) return -1;
			return 1; /* flt */
		}
		else if (hawk_is_uch_digit(*endptr))
		{
			const hawk_bch_t* p = endptr;
			do { p++; } while (p < end && hawk_is_bch_digit(*p));
			if (p < end && (*p == '.' || *p == 'E' || *p == 'e')) 
			{
				/* it's probably an floating-point number. 
				 * 
 				 *  BEGIN { b=99; printf "%f\n", (0 b 1.112); }
				 *
				 * for the above code, this function gets '0991.112'.
				 * and endptr points to '9' after hawk_bchars_to_int() as
				 * the numeric string beginning with 0 is treated 
				 * as an octal number. 
				 * 
				 * getting side-tracked, 
				 *   BEGIN { b=99; printf "%f\n", (0 b 1.000); }
				 * the above code cause this function to get 0991, not 0991.000
				 * because of the default CONVFMT '%.6g' which doesn't produce '.000'.
				 * so such a number is not treated as a floating-point number.
				 */
				goto handle_float;
			}
		}

		if (stripspc)
		{
			while (endptr < end && hawk_is_bch_space(*endptr)) endptr++;
		}
	}

	if (reqsober && !is_sober) return -1;
	if (nopartial && endptr < end) return -1;
	return 0; /* int */
}

/* ------------------------------------------------------------------------ */

int hawk_uchars_to_bin (const hawk_uch_t* hex, hawk_oow_t hexlen, hawk_uint8_t* buf, hawk_oow_t buflen)
{
	const hawk_uch_t* end = hex + hexlen;
	hawk_oow_t bi = 0;

	while (hex < end && bi < buflen)
	{
		int v;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		if (hex >= end) return -1;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		bi++;
	}

	return 0;
}

int hawk_bchars_to_bin (const hawk_bch_t* hex, hawk_oow_t hexlen, hawk_uint8_t* buf, hawk_oow_t buflen)
{
	const hawk_bch_t* end = hex + hexlen;
	hawk_oow_t bi = 0;

	while (hex < end && bi < buflen)
	{
		int v;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		if (hex >= end) return -1;

		v = HAWK_XDIGIT_TO_NUM(*hex);
		if (v <= -1) return -1;
		buf[bi] = buf[bi] * 16 + v;

		hex++;
		bi++;
	}

	return 0;
}

