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

/*
 * This file contains a formatted output routine derived from kvprintf() 
 * of FreeBSD. It has been heavily modified and bug-fixed.
 */

/*
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static char_t* sprintn (char_t* nbuf, hawk_uintmax_t num, int base, int *lenp, int upper)
{
	char_t *p, c;

	p = nbuf;
	*p = T('\0');
	do 
	{
		c = hex2ascii(num % base);
		*++p = upper ? toupper(c) : c;
	} 
	while (num /= base);

	if (lenp) *lenp = p - nbuf;
	return p;
}

/* NOTE: data output is aborted if the data limit is reached or 
 *       I/O error occurs  */

#undef PUT_CHAR
#undef PUT_BYTE_IN_HEX

#define PUT_CHAR(c) do { \
	int xx; \
	if (data->count >= data->limit) goto done; \
	if ((xx = data->put(c, data->ctx)) <= -1) goto oops; \
	if (xx == 0) goto done; \
	data->count++; \
} while (0)

#define PUT_BYTE_IN_HEX(byte,extra_flags) do { \
	hawk_bch_t __xbuf[3]; \
	hawk_bytetombs ((byte), __xbuf, HAWK_COUNTOF(__xbuf), (16 | (extra_flags)), '0'); \
	PUT_CHAR(__xbuf[0]); \
	PUT_CHAR(__xbuf[1]); \
} while (0)

#define BYTE_PRINTABLE(x) ((x) <= 0x7F && (x) != '\\' && HAWK_ISMPRINT(x))

int fmtout (const char_t* fmt, fmtout_t* data, va_list ap)
{
	char_t nbuf[MAXNBUF];
	const char_t* p, * percent;
	int n, base, tmp, width, neg, sign, precision, upper;
	uchar_t ch; 
	char_t ach, padc, * sp;
	ochar_t oach, * osp;
	hawk_oow_t oslen, slen;
	hawk_byte_t* bytep;
	int lm_flag, lm_dflag, flagc, numlen;
	hawk_uintmax_t num = 0;
	int stop = 0;

	struct
	{
		hawk_bch_t  sbuf[32];
		hawk_bch_t* ptr;
		hawk_oow_t   capa;
	} fltfmt;

	struct
	{
		char_t       sbuf[96];
		char_t*      ptr;
		hawk_oow_t   capa;
	} fltout;

	data->count = 0;

	fltfmt.ptr  = fltfmt.sbuf;
	fltfmt.capa = HAWK_COUNTOF(fltfmt.sbuf) - 1;

	fltout.ptr  = fltout.sbuf;
	fltout.capa = HAWK_COUNTOF(fltout.sbuf) - 1;

	while (1)
	{
		while ((ch = (uchar_t)*fmt++) != T('%') || stop) 
		{
			if (ch == T('\0')) goto done;
			PUT_CHAR (ch);
		}
		percent = fmt - 1;

		padc = T(' '); 
		width = 0; precision = 0;
		neg = 0; sign = 0; upper = 0;

		lm_flag = 0; lm_dflag = 0; flagc = 0; 

reswitch:	
		switch (ch = (uchar_t)*fmt++) 
		{
		case T('%'): /* %% */
			ach = ch;
			goto print_lowercase_c;
			break;

		/* flag characters */
		case T('.'):
			if (flagc & FLAGC_DOT) goto invalid_format;
			flagc |= FLAGC_DOT;
			goto reswitch;

		case T('#'): 
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SHARP;
			goto reswitch;

		case T(' '):
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SPACE;
			goto reswitch;

		case T('+'): /* place sign for signed conversion */
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			flagc |= FLAGC_SIGN;
			goto reswitch;

		case T('-'): /* left adjusted */
			if (flagc & (FLAGC_WIDTH | FLAGC_DOT | FLAGC_LENMOD)) goto invalid_format;
			if (flagc & FLAGC_DOT)
			{
				goto invalid_format;
			}
			else
			{
				flagc |= FLAGC_LEFTADJ;
				if (flagc & FLAGC_ZEROPAD)
				{
					padc = T(' ');
					flagc &= ~FLAGC_ZEROPAD;
				}
			}
			
			goto reswitch;

		case T('*'): /* take the length from the parameter */
			if (flagc & FLAGC_DOT) 
			{
				if (flagc & (FLAGC_STAR2 | FLAGC_PRECISION)) goto invalid_format;
				flagc |= FLAGC_STAR2;

				precision = va_arg(ap, int);
				if (precision < 0) 
				{
					/* if precision is less than 0, 
					 * treat it as if no .precision is specified */
					flagc &= ~FLAGC_DOT;
					precision = 0;
				}
			} 
			else 
			{
				if (flagc & (FLAGC_STAR1 | FLAGC_WIDTH)) goto invalid_format;
				flagc |= FLAGC_STAR1;

				width = va_arg(ap, int);
				if (width < 0) 
				{
					/*
					if (flagc & FLAGC_LEFTADJ) 
						flagc  &= ~FLAGC_LEFTADJ;
					else
					*/
						flagc |= FLAGC_LEFTADJ;
					width = -width;
				}
			}
			goto reswitch;

		case T('0'): /* zero pad */
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			if (!(flagc & (FLAGC_DOT | FLAGC_LEFTADJ)))
			{
				padc = T('0');
				flagc |= FLAGC_ZEROPAD;
				goto reswitch;
			}
		/* end of flags characters */

		case T('1'): case T('2'): case T('3'): case T('4'):
		case T('5'): case T('6'): case T('7'): case T('8'): case T('9'):
			if (flagc & FLAGC_LENMOD) goto invalid_format;
			for (n = 0;; ++fmt) 
			{
				n = n * 10 + ch - T('0');
				ch = *fmt;
				if (ch < T('0') || ch > T('9')) break;
			}
			if (flagc & FLAGC_DOT) 
			{
				if (flagc & FLAGC_STAR2) goto invalid_format;
				precision = n;
				flagc |= FLAGC_PRECISION;
			}
			else 
			{
				if (flagc & FLAGC_STAR1) goto invalid_format;
				width = n;
				flagc |= FLAGC_WIDTH;
			}
			goto reswitch;

		/* length modifiers */
		case T('h'): /* short int */
		case T('l'): /* long int */
		case T('q'): /* long long int */
		case T('j'): /* uintmax_t */
		case T('z'): /* size_t */
		case T('t'): /* ptrdiff_t */
			if (lm_flag & (LF_LD | LF_QD)) goto invalid_format;

			flagc |= FLAGC_LENMOD;
			if (lm_dflag)
			{
				/* error */
				goto invalid_format;
			}
			else if (lm_flag)
			{
				if (lm_tab[ch - T('a')].dflag && lm_flag == lm_tab[ch - T('a')].flag)
				{
					lm_flag &= ~lm_tab[ch - T('a')].flag;
					lm_flag |= lm_tab[ch - T('a')].dflag;
					lm_dflag |= lm_flag;
					goto reswitch;
				}
				else
				{
					/* error */
					goto invalid_format;
				}
			}
			else 
			{
				lm_flag |= lm_tab[ch - T('a')].flag;
				goto reswitch;
			}
			break;

		case T('L'): /* long double */
			if (flagc & FLAGC_LENMOD) 
			{
				/* conflict with other length modifier */
				goto invalid_format; 
			}
			flagc |= FLAGC_LENMOD;
			lm_flag |= LF_LD;
			goto reswitch;

		case T('Q'): /* __float128 */
			if (flagc & FLAGC_LENMOD)
			{
				/* conflict with other length modifier */
				goto invalid_format; 
			}
			flagc |= FLAGC_LENMOD;
			lm_flag |= LF_QD;
			goto reswitch;

		case T('I'):
		{
			int save_lm_flag = lm_flag;
			if (fmt[0] == T('8'))
			{
				lm_flag |= LF_I8;
				fmt += 1;
			}
			else if (fmt[0] == T('1') && fmt[1] == T('6'))
			{
				lm_flag |= LF_I16;
				fmt += 2;
			}
		#if defined(HAWK_HAVE_INT128_T)
			else if (fmt[0] == T('1') && fmt[1] == T('2') && fmt[2] == T('8'))
			{
				lm_flag |= LF_I128;
				fmt += 3;
			}
		#endif
			else if (fmt[0] == T('3') && fmt[1] == T('2'))
			{
				lm_flag |= LF_I32;
				fmt += 2;
			}
		#if defined(HAWK_HAVE_INT64_T)
			else if (fmt[0] == T('6') && fmt[1] == T('4'))
			{
				lm_flag |= LF_I64;
				fmt += 2;
			}
		#endif
			else
			{
				goto invalid_format; 
			}

			if (flagc & FLAGC_LENMOD)
			{
				/* conflict with other length modifier */
				lm_flag = save_lm_flag;
				goto invalid_format; 
			}
			flagc |= FLAGC_LENMOD;
			goto reswitch;
		}

		/* end of length modifiers */

		case T('n'):
			if (lm_flag & LF_J) /* j */
				*(va_arg(ap, hawk_intmax_t*)) = data->count;
			else if (lm_flag & LF_Z) /* z */
				*(va_arg(ap, hawk_oow_t*)) = data->count;
		#if (HAWK_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q) /* ll */
				*(va_arg(ap, long long int*)) = data->count;
		#endif
			else if (lm_flag & LF_L) /* l */
				*(va_arg(ap, long int*)) = data->count;
			else if (lm_flag & LF_H) /* h */
				*(va_arg(ap, short int*)) = data->count;
			else if (lm_flag & LF_C) /* hh */
				*(va_arg(ap, char*)) = data->count;
			else if (flagc & FLAGC_LENMOD)
				goto oops;
			else
				*(va_arg(ap, int*)) = data->count;
			break;


		/* signed integer conversions */
		case T('d'):
		case T('i'): /* signed conversion */
			base = 10;
			sign = 1;
			goto handle_sign;
		/* end of signed integer conversions */

		/* unsigned integer conversions */
		case T('o'): 
			base = 8;
			goto handle_nosign;
		case T('u'):
			base = 10;
			goto handle_nosign;
		case T('X'):
			upper = 1;
		case T('x'):
			base = 16;
			goto handle_nosign;
		case T('b'):
			base = 2;
			goto handle_nosign;
		/* end of unsigned integer conversions */

		case T('p'): /* pointer */
			base = 16;

			if (width == 0) flagc |= FLAGC_SHARP;
			else flagc &= ~FLAGC_SHARP;

			num = (hawk_uintptr_t)va_arg(ap, void*);
			goto number;

		case T('c'):
			/* zeropad must not take effect for 'c' */
			if (flagc & FLAGC_ZEROPAD) padc = T(' '); 
			if (((lm_flag & LF_H) && (HAWK_SIZEOF(char_t) > HAWK_SIZEOF(ochar_t))) ||
			    ((lm_flag & LF_L) && (HAWK_SIZEOF(char_t) < HAWK_SIZEOF(ochar_t)))) goto uppercase_c;
		lowercase_c:
			ach = HAWK_SIZEOF(char_t) < HAWK_SIZEOF(int)? va_arg(ap, int): va_arg(ap, char_t);

		print_lowercase_c:
			/* precision 0 doesn't kill the letter */
			width--;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR (padc);
			}
			PUT_CHAR (ach);
			if ((flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR (padc);
			}
			break;

		case T('C'):
			/* zeropad must not take effect for 'C' */
			if (flagc & FLAGC_ZEROPAD) padc = T(' ');
			if (((lm_flag & LF_H) && (HAWK_SIZEOF(char_t) < HAWK_SIZEOF(ochar_t))) ||
			    ((lm_flag & LF_L) && (HAWK_SIZEOF(char_t) > HAWK_SIZEOF(ochar_t)))) goto lowercase_c;
		uppercase_c:
			oach = HAWK_SIZEOF(ochar_t) < HAWK_SIZEOF(int)? va_arg(ap, int): va_arg(ap, ochar_t);

			oslen = 1;
			if (data->conv (&oach, &oslen, HAWK_NULL, &slen, data->ctx) <= -1)
			{
				/* conversion error */
				goto oops;
			}

			/* precision 0 doesn't kill the letter */
			width -= slen;
			if (!(flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR (padc);
			}

			{
				char_t conv_buf[CONV_MAX]; 
				hawk_oow_t i, conv_len;

				oslen = 1;
				conv_len = HAWK_COUNTOF(conv_buf);

				/* this must not fail since the dry-run above was successful */
				data->conv (&oach, &oslen, conv_buf, &conv_len, data->ctx);

				for (i = 0; i < conv_len; i++)
				{
					PUT_CHAR (conv_buf[i]);
				}
			}

			if ((flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR (padc);
			}
			break;

		case T('s'):
			/* zeropad must not take effect for 's' */
			if (flagc & FLAGC_ZEROPAD) padc = T(' ');
			if (((lm_flag & LF_H) && (HAWK_SIZEOF(char_t) > HAWK_SIZEOF(ochar_t))) ||
			    ((lm_flag & LF_L) && (HAWK_SIZEOF(char_t) < HAWK_SIZEOF(ochar_t)))) goto uppercase_s;
		lowercase_s:
			sp = va_arg (ap, char_t*);
			if (sp == HAWK_NULL) p = T("(null)");

		print_lowercase_s:
			if (flagc & FLAGC_DOT)
			{
				for (n = 0; n < precision && sp[n]; n++);
			}
			else
			{
				for (n = 0; sp[n]; n++);
			}

			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR(padc);
			}
			while (n--) PUT_CHAR(*sp++);
			if ((flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR(padc);
			}
			break;

		case T('S'):
			/* zeropad must not take effect for 'S' */
			if (flagc & FLAGC_ZEROPAD) padc = T(' ');
			if (((lm_flag & LF_H) && (HAWK_SIZEOF(char_t) < HAWK_SIZEOF(ochar_t))) ||
			    ((lm_flag & LF_L) && (HAWK_SIZEOF(char_t) > HAWK_SIZEOF(ochar_t)))) goto lowercase_s;
		uppercase_s:

			osp = va_arg (ap, ochar_t*);
			if (osp == HAWK_NULL) osp = OT("(null)");

			/* get the length */
			for (oslen = 0; osp[oslen]; oslen++);

			if (data->conv(osp, &oslen, HAWK_NULL, &slen, data->ctx) <= -1)
			{
				/* conversion error */
				goto oops;
			}

			/* slen hold the length after conversion */
			n = slen;
			if ((flagc & FLAGC_DOT) && precision < slen) n = precision;
			width -= n;

			if (!(flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR (padc);
			}

			{
				char_t conv_buf[CONV_MAX]; 
				hawk_oow_t i, conv_len, src_len, tot_len = 0;
				while (n > 0)
				{
					HAWK_ASSERT (oslen > tot_len);

				#if CONV_MAX == 1
					src_len = oslen - tot_len;
				#else
					src_len = 1;
				#endif
					conv_len = HAWK_COUNTOF(conv_buf);

					/* this must not fail since the dry-run above was successful */
					data->conv (&osp[tot_len], &src_len, conv_buf, &conv_len, data->ctx);

					tot_len += src_len;

					/* stop outputting if a converted character can't be printed 
					 * in its entirety (limited by precision). but this is not an error */
					if (n < conv_len) break; 

					for (i = 0; i < conv_len; i++)
					{
						PUT_CHAR (conv_buf[i]);
					}

					n -= conv_len;
				}
			}
			
			if ((flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR (padc);
			}
			break;

		case T('k'):
		case T('K'):
		{
			int k_hex_width;
			/* zeropad must not take effect for 's'. 
			 * 'h' & 'l' is not used to differentiate hawk_bch_t and hawk_uch_t
			 * because 'k' means hawk_byte_t. 
			 * 'l', results in uppercase hexadecimal letters. 
			 * 'h' drops the leading \x in the output 
			 * --------------------------------------------------------
			 * hk -> \x + non-printable in lowercase hex
			 * k -> all in lowercase hex
			 * lk -> \x +  all in lowercase hex
			 * --------------------------------------------------------
			 * hK -> \x + non-printable in uppercase hex
			 * K -> all in uppercase hex
			 * lK -> \x +  all in uppercase hex
			 * --------------------------------------------------------
			 * with 'k' or 'K', i don't substitute "(null)" for the NULL pointer
			 */
			if (flagc & FLAGC_ZEROPAD) padc = T(' ');

			bytep = va_arg(ap, hawk_byte_t*);
			k_hex_width = (lm_flag & (LF_H | LF_L))? 4: 2;

			if (lm_flag & LF_H)
			{
				/* to print non-printables in hex. i don't use ismprint() to avoid escaping a backslash itself. */
				if (flagc & FLAGC_DOT)
				{
					/* if precision is specifed, it doesn't stop at the value of zero unlike 's' or 'S' */
					for (n = 0; n < precision; n++) width -= BYTE_PRINTABLE(bytep[n])? 1: k_hex_width;
				}
				else
				{
					for (n = 0; bytep[n]; n++) width -= BYTE_PRINTABLE(bytep[n])? 1: k_hex_width;
				}
			}
			else
			{
				/* to print all in hex */
				if (flagc & FLAGC_DOT)
				{
					/* if precision is specifed, it doesn't stop at the value of zero unlike 's' or 'S' */
					for (n = 0; n < precision; n++) /* nothing */;
				}
				else
				{
					for (n = 0; bytep[n]; n++) /* nothing */;
				}
				width -= (n * k_hex_width);
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR(padc);
			}

			while (n--) 
			{
				if ((lm_flag & LF_H) && BYTE_PRINTABLE(*bytep)) 
				{
					PUT_CHAR(*bytep);
				}
				else
				{
					hawk_bch_t xbuf[3];
					hawk_bytetombs (*bytep, xbuf, HAWK_COUNTOF(xbuf), (16 | (ch == T('k')? HAWK_BYTETOSTR_LOWERCASE: 0)), HAWK_BT('0'));
					if (lm_flag & (LF_H | LF_L))
					{
						PUT_CHAR('\\');
						PUT_CHAR('x');
					}
					PUT_CHAR(xbuf[0]);
					PUT_CHAR(xbuf[1]);
				}
				bytep++;
			}

			if ((flagc & FLAGC_LEFTADJ) && width > 0)
			{
				while (width--) PUT_CHAR(padc);
			}
			break;
		}

		case T('w'):
		case T('W'):
		{
			/* unicode string in unicode escape sequence.
			 * 
			 * hw -> \uXXXX, \UXXXXXXXX, printable-byte(only in ascii range)
			 * w -> \uXXXX, \UXXXXXXXX
			 * lw -> all in \UXXXXXXXX
			 */
			const hawk_uch_t* usp;
			hawk_oow_t uwid;

			if (flagc & FLAGC_ZEROPAD) padc = T(' ');
			usp = va_arg(ap, hawk_uch_t*);

			if (flagc & FLAGC_DOT)
			{
				/* if precision is specifed, it doesn't stop at the value of zero unlike 's' or 'S' */
				for (n = 0; n < precision; n++) 
				{
					if ((lm_flag & LF_H) && BYTE_PRINTABLE(usp[n])) uwid = 1;
					else if (!(lm_flag & LF_L) && usp[n] <= 0xFFFF) uwid = 6;
					else uwid = 10;
					width -= uwid;
				}
			}
			else
			{
				for (n = 0; usp[n]; n++)
				{
					if ((lm_flag & LF_H) && BYTE_PRINTABLE(usp[n])) uwid = 1;
					else if (!(lm_flag & LF_L) && usp[n] <= 0xFFFF) uwid = 6;
					else uwid = 10;
					width -= uwid;
				}
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0) 
			{
				while (width--) PUT_CHAR(padc);
			}

			while (n--) 
			{
				if ((lm_flag & LF_H) && BYTE_PRINTABLE(*usp)) 
				{
					PUT_CHAR(*usp);
				}
				else if (!(lm_flag & LF_L) && *usp <= 0xFFFF) 
				{
					hawk_uint16_t u16 = *usp;
					int extra_flags = ((ch) == 'w'? HAWK_BYTETOSTR_LOWERCASE: 0);
					PUT_CHAR('\\');
					PUT_CHAR('u');
					PUT_BYTE_IN_HEX((u16 >> 8) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX(u16 & 0xFF, extra_flags);
				}
				else
				{
					hawk_uint32_t u32 = *usp;
					int extra_flags = ((ch) == 'w'? HAWK_BYTETOSTR_LOWERCASE: 0);
					PUT_CHAR('\\');
					PUT_CHAR('U');
					PUT_BYTE_IN_HEX((u32 >> 24) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX((u32 >> 16) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX((u32 >> 8) & 0xFF, extra_flags);
					PUT_BYTE_IN_HEX(u32 & 0xFF, extra_flags);
				}
				usp++;
			}

			if ((flagc & FLAGC_LEFTADJ) && width > 0) 
			{
				while (width--) PUT_CHAR(padc);
			}
			break;
		}

		case T('e'):
		case T('E'):
		case T('f'):
		case T('F'):
		case T('g'):
		case T('G'):
		/*
		case T('a'):
		case T('A'):
		*/
		{
			/* let me rely on snprintf until i implement float-point to string conversion */
			int q;
			hawk_oow_t fmtlen;
		#if (HAWK_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
			__float128 v_qd;
		#endif
			long double v_ld;
			double v_d;
			int dtype = 0;
			hawk_oow_t newcapa;

			if (lm_flag & LF_J)
			{
			#if (HAWK_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF) && (HAWK_SIZEOF_FLTMAX_T == HAWK_SIZEOF___FLOAT128)
				v_qd = va_arg (ap, hawk_fltmax_t);
				dtype = LF_QD;
			#elif HAWK_SIZEOF_FLTMAX_T == HAWK_SIZEOF_DOUBLE
				v_d = va_arg (ap, hawk_fltmax_t);
			#elif HAWK_SIZEOF_FLTMAX_T == HAWK_SIZEOF_LONG_DOUBLE
				v_ld = va_arg (ap, hawk_fltmax_t);
				dtype = LF_LD;
			#else
				#error Unsupported hawk_flt_t
			#endif
			}
			else if (lm_flag & LF_Z)
			{
				/* hawk_flt_t is limited to double or long double */

				/* precedence goes to double if sizeof(double) == sizeof(long double) 
				 * for example, %Lf didn't work on some old platforms.
				 * so i prefer the format specifier with no modifier.
				 */
			#if HAWK_SIZEOF_FLT_T == HAWK_SIZEOF_DOUBLE
				v_d = va_arg (ap, hawk_flt_t);
			#elif HAWK_SIZEOF_FLT_T == HAWK_SIZEOF_LONG_DOUBLE
				v_ld = va_arg (ap, hawk_flt_t);
				dtype = LF_LD;
			#else
				#error Unsupported hawk_flt_t
			#endif
			}
			else if (lm_flag & (LF_LD | LF_L))
			{
				v_ld = va_arg (ap, long double);
				dtype = LF_LD;
			}
		#if (HAWK_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
			else if (lm_flag & (LF_QD | LF_Q))
			{
				v_qd = va_arg(ap, __float128);
				dtype = LF_QD;
			}
		#endif
			else if (flagc & FLAGC_LENMOD)
			{
				goto oops;
			}
			else
			{
				v_d = va_arg (ap, double);
			}

			fmtlen = fmt - percent;
			if (fmtlen > fltfmt.capa)
			{
				if (fltfmt.ptr == fltfmt.sbuf)
				{
					fltfmt.ptr = HAWK_MMGR_ALLOC (HAWK_MMGR_GETDFL(), HAWK_SIZEOF(*fltfmt.ptr) * (fmtlen + 1));
					if (fltfmt.ptr == HAWK_NULL) goto oops;
				}
				else
				{
					hawk_bch_t* tmpptr;

					tmpptr = HAWK_MMGR_REALLOC (HAWK_MMGR_GETDFL(), fltfmt.ptr, HAWK_SIZEOF(*fltfmt.ptr) * (fmtlen + 1));
					if (tmpptr == HAWK_NULL) goto oops;
					fltfmt.ptr = tmpptr;
				}

				fltfmt.capa = fmtlen;
			}

			/* compose back the format specifier */
			fmtlen = 0;
			fltfmt.ptr[fmtlen++] = HAWK_BT('%');
			if (flagc & FLAGC_SPACE) fltfmt.ptr[fmtlen++] = HAWK_BT(' ');
			if (flagc & FLAGC_SHARP) fltfmt.ptr[fmtlen++] = HAWK_BT('#');
			if (flagc & FLAGC_SIGN) fltfmt.ptr[fmtlen++] = HAWK_BT('+');
			if (flagc & FLAGC_LEFTADJ) fltfmt.ptr[fmtlen++] = HAWK_BT('-');
			if (flagc & FLAGC_ZEROPAD) fltfmt.ptr[fmtlen++] = HAWK_BT('0');

			if (flagc & FLAGC_STAR1) fltfmt.ptr[fmtlen++] = HAWK_BT('*');
			else if (flagc & FLAGC_WIDTH) 
			{
				fmtlen += hawk_fmtuintmaxtombs (
					&fltfmt.ptr[fmtlen], fltfmt.capa - fmtlen, 
					width, 10, -1, HAWK_BT('\0'), HAWK_NULL);
			}
			if (flagc & FLAGC_DOT) fltfmt.ptr[fmtlen++] = HAWK_BT('.');
			if (flagc & FLAGC_STAR2) fltfmt.ptr[fmtlen++] = HAWK_BT('*');
			else if (flagc & FLAGC_PRECISION) 
			{
				fmtlen += hawk_fmtuintmaxtombs (
					&fltfmt.ptr[fmtlen], fltfmt.capa - fmtlen, 
					precision, 10, -1, HAWK_BT('\0'), HAWK_NULL);
			}

			if (dtype == LF_LD)
				fltfmt.ptr[fmtlen++] = HAWK_BT('L');
		#if (HAWK_SIZEOF___FLOAT128 > 0)
			else if (dtype == LF_QD)
				fltfmt.ptr[fmtlen++] = HAWK_BT('Q');
		#endif

			fltfmt.ptr[fmtlen++] = ch;
			fltfmt.ptr[fmtlen] = HAWK_BT('\0');

		#if defined(HAVE_SNPRINTF)
			/* nothing special here */
		#else
			/* best effort to avoid buffer overflow when no snprintf is available. 
			 * i really can't do much if it happens. */
			newcapa = precision + width + 32;
			if (fltout.capa < newcapa)
			{
				HAWK_ASSERT (fltout.ptr == fltout.sbuf);

				fltout.ptr = HAWK_MMGR_ALLOC (HAWK_MMGR_GETDFL(), HAWK_SIZEOF(char_t) * (newcapa + 1));
				if (fltout.ptr == HAWK_NULL) goto oops;
				fltout.capa = newcapa;
			}
		#endif

			while (1)
			{

				if (dtype == LF_LD)
				{
				#if defined(HAVE_SNPRINTF)
					q = snprintf ((hawk_bch_t*)fltout.ptr, fltout.capa + 1, fltfmt.ptr, v_ld);
				#else
					q = sprintf ((hawk_bch_t*)fltout.ptr, fltfmt.ptr, v_ld);
				#endif
				}
			#if (HAWK_SIZEOF___FLOAT128 > 0) && defined(HAVE_QUADMATH_SNPRINTF)
				else if (dtype == LF_QD)
				{
					q = quadmath_snprintf ((hawk_bch_t*)fltout.ptr, fltout.capa + 1, fltfmt.ptr, v_qd);
				}
			#endif
				else
				{
				#if defined(HAVE_SNPRINTF)
					q = snprintf ((hawk_bch_t*)fltout.ptr, fltout.capa + 1, fltfmt.ptr, v_d);
				#else
					q = sprintf ((hawk_bch_t*)fltout.ptr, fltfmt.ptr, v_d);
				#endif
				}
				if (q <= -1) goto oops;
				if (q <= fltout.capa) break;

				newcapa = fltout.capa * 2;
				if (newcapa < q) newcapa = q;

				if (fltout.ptr == fltout.sbuf)
				{
					fltout.ptr = HAWK_MMGR_ALLOC (HAWK_MMGR_GETDFL(), HAWK_SIZEOF(char_t) * (newcapa + 1));
					if (fltout.ptr == HAWK_NULL) goto oops;
				}
				else
				{
					char_t* tmpptr;

					tmpptr = HAWK_MMGR_REALLOC (HAWK_MMGR_GETDFL(), fltout.ptr, HAWK_SIZEOF(char_t) * (newcapa + 1));
					if (tmpptr == HAWK_NULL) goto oops;
					fltout.ptr = tmpptr;
				}
				fltout.capa = newcapa;
			}

			if (HAWK_SIZEOF(char_t) != HAWK_SIZEOF(hawk_bch_t))
			{
				fltout.ptr[q] = T('\0');	
				while (q > 0)
				{
					q--;
					fltout.ptr[q] = ((hawk_bch_t*)fltout.ptr)[q];	
				}
			}

			sp = fltout.ptr;
			flagc &= ~FLAGC_DOT;
			width = 0;
			precision = 0;
			goto print_lowercase_s;
		}

		handle_nosign:
			sign = 0;
			if (lm_flag & LF_J)
			{
			#if defined(__GNUC__) && \
			    (HAWK_SIZEOF_UINTMAX_T > HAWK_SIZEOF_SIZE_T) && \
			    (HAWK_SIZEOF_UINTMAX_T != HAWK_SIZEOF_LONG_LONG) && \
			    (HAWK_SIZEOF_UINTMAX_T != HAWK_SIZEOF_LONG)
				/* GCC-compiled binaries crashed when getting hawk_uintmax_t with va_arg.
				 * This is just a work-around for it */
				int i;
				for (i = 0, num = 0; i < HAWK_SIZEOF(hawk_uintmax_t) / HAWK_SIZEOF(hawk_oow_t); i++)
				{	
				#if defined(HAWK_ENDIAN_BIG)
					num = num << (8 * HAWK_SIZEOF(hawk_oow_t)) | (va_arg (ap, hawk_oow_t));
				#else
					register int shift = i * HAWK_SIZEOF(hawk_oow_t);
					hawk_oow_t x = va_arg (ap, hawk_oow_t);
					num |= (hawk_uintmax_t)x << (shift * 8);
				#endif
				}
			#else
				num = va_arg (ap, hawk_uintmax_t);
			#endif
			}
			else if (lm_flag & LF_T)
				num = va_arg (ap, hawk_ptrdiff_t);
			else if (lm_flag & LF_Z)
				num = va_arg (ap, hawk_oow_t);
			#if (HAWK_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q)
				num = va_arg (ap, unsigned long long int);
			#endif
			else if (lm_flag & (LF_L | LF_LD))
				num = va_arg (ap, unsigned long int);
			else if (lm_flag & LF_H)
				num = (unsigned short int)va_arg (ap, int);
			else if (lm_flag & LF_C)
				num = (unsigned char)va_arg (ap, int);

			else if (lm_flag & LF_I8)
			{
			#if (HAWK_SIZEOF_UINT8_T < HAWK_SIZEOF_INT)
				num = (hawk_uint8_t)va_arg (ap, unsigned int);
			#else
				num = va_arg (ap, hawk_uint8_t);
			#endif
			}
			else if (lm_flag & LF_I16)
			{
			#if (HAWK_SIZEOF_UINT16_T < HAWK_SIZEOF_INT)
				num = (hawk_uint16_t)va_arg (ap, unsigned int);
			#else
				num = va_arg (ap, hawk_uint16_t);
			#endif
			}
			else if (lm_flag & LF_I32)
			{	
			#if (HAWK_SIZEOF_UINT32_T < HAWK_SIZEOF_INT)
				num = (hawk_uint32_t)va_arg (ap, unsigned int);
			#else
				num = va_arg (ap, hawk_uint32_t);
			#endif
			}
		#if defined(HAWK_HAVE_UINT64_T)
			else if (lm_flag & LF_I64)
			{
			#if (HAWK_SIZEOF_UINT64_T < HAWK_SIZEOF_INT)
				num = (hawk_uint64_t)va_arg (ap, unsigned int);
			#else
				num = va_arg (ap, hawk_uint64_t);
			#endif
			}
		#endif
		#if defined(HAWK_HAVE_UINT128_T)
			else if (lm_flag & LF_I128)
			{
			#if (HAWK_SIZEOF_UINT128_T < HAWK_SIZEOF_INT)
				num = (hawk_uint128_t)va_arg (ap, unsigned int);
			#else
				num = va_arg (ap, hawk_uint128_t);
			#endif
			}
		#endif
			else
			{
				num = va_arg (ap, unsigned int);
			}
			goto number;

handle_sign:
			if (lm_flag & LF_J)
			{
			#if defined(__GNUC__) && \
			    (HAWK_SIZEOF_INTMAX_T > HAWK_SIZEOF_SIZE_T) && \
			    (HAWK_SIZEOF_UINTMAX_T != HAWK_SIZEOF_LONG_LONG) && \
			    (HAWK_SIZEOF_UINTMAX_T != HAWK_SIZEOF_LONG)
				/* GCC-compiled binraries crashed when getting hawk_uintmax_t with va_arg.
				 * This is just a work-around for it */
				int i;
				for (i = 0, num = 0; i < HAWK_SIZEOF(hawk_intmax_t) / HAWK_SIZEOF(hawk_oow_t); i++)
				{
				#if defined(HAWK_ENDIAN_BIG)
					num = num << (8 * HAWK_SIZEOF(hawk_oow_t)) | (va_arg (ap, hawk_oow_t));
				#else
					register int shift = i * HAWK_SIZEOF(hawk_oow_t);
					hawk_oow_t x = va_arg (ap, hawk_oow_t);
					num |= (hawk_uintmax_t)x << (shift * 8);
				#endif
				}
			#else
				num = va_arg (ap, hawk_intmax_t);
			#endif
			}

			else if (lm_flag & LF_T)
				num = va_arg(ap, hawk_ptrdiff_t);
			else if (lm_flag & LF_Z)
				num = va_arg (ap, hawk_ssize_t);
			#if (HAWK_SIZEOF_LONG_LONG > 0)
			else if (lm_flag & LF_Q)
				num = va_arg (ap, long long int);
			#endif
			else if (lm_flag & (LF_L | LF_LD))
				num = va_arg (ap, long int);
			else if (lm_flag & LF_H)
				num = (short int)va_arg (ap, int);
			else if (lm_flag & LF_C)
				num = (char)va_arg (ap, int);

			else if (lm_flag & LF_I8)
			{
			#if (HAWK_SIZEOF_INT8_T < HAWK_SIZEOF_INT)
				num = (hawk_int8_t)va_arg (ap, int);
			#else
				num = va_arg (ap, hawk_int8_t);
			#endif
			}
			else if (lm_flag & LF_I16)
			{
			#if (HAWK_SIZEOF_INT16_T < HAWK_SIZEOF_INT)
				num = (hawk_int16_t)va_arg (ap, int);
			#else
				num = va_arg (ap, hawk_int16_t);
			#endif
			}
			else if (lm_flag & LF_I32)
			{
			#if (HAWK_SIZEOF_INT32_T < HAWK_SIZEOF_INT)
				num = (hawk_int32_t)va_arg (ap, int);
			#else
				num = va_arg (ap, hawk_int32_t);
			#endif
			}
		#if defined(HAWK_HAVE_INT64_T)
			else if (lm_flag & LF_I64)
			{
			#if (HAWK_SIZEOF_INT64_T < HAWK_SIZEOF_INT)
				num = (hawk_int64_t)va_arg (ap, int);
			#else
				num = va_arg (ap, hawk_int64_t);
			#endif
			}
		#endif
		#if defined(HAWK_HAVE_INT128_T)
			else if (lm_flag & LF_I128)
			{
			#if (HAWK_SIZEOF_INT128_T < HAWK_SIZEOF_INT)
				num = (hawk_int128_t)va_arg (ap, int);
			#else
				num = va_arg (ap, hawk_int128_t);
			#endif
			}
		#endif
			else
			{
				num = va_arg (ap, int);
			}

number:
			if (sign && (hawk_intmax_t)num < 0) 
			{
				neg = 1;
				num = -(hawk_intmax_t)num;
			}
			p = sprintn (nbuf, num, base, &tmp, upper);
			if ((flagc & FLAGC_SHARP) && num != 0) 
			{
				if (base == 8) tmp++;
				else if (base == 16) tmp += 2;
			}
			if (neg) tmp++;
			else if (flagc & FLAGC_SIGN) tmp++;
			else if (flagc & FLAGC_SPACE) tmp++;

			numlen = p - nbuf;
			if ((flagc & FLAGC_DOT) && precision > numlen) 
			{
				/* extra zeros fro precision specified */
				tmp += (precision - numlen);
			}

			if (!(flagc & FLAGC_LEFTADJ) && !(flagc & FLAGC_ZEROPAD) && width > 0 && (width -= tmp) > 0)
			{
				while (width--) PUT_CHAR(padc);
			}

			if (neg) PUT_CHAR(T('-'));
			else if (flagc & FLAGC_SIGN) PUT_CHAR(T('+'));
			else if (flagc & FLAGC_SPACE) PUT_CHAR(T(' '));

			if ((flagc & FLAGC_SHARP) && num != 0) 
			{
				if (base == 2) 
				{
					PUT_CHAR(T('0'));
					PUT_CHAR(T('b'));
				} 
				else if (base == 8) 
				{
					PUT_CHAR(T('0'));
				} 
				else if (base == 16) 
				{
					PUT_CHAR(T('0'));
					PUT_CHAR(T('x'));
				}
			}

			if ((flagc & FLAGC_DOT) && precision > numlen)
			{
				/* extra zeros for precision specified */
				while (numlen < precision) 
				{
					PUT_CHAR (T('0'));
					numlen++;
				}
			}

			if (!(flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				while (width-- > 0) PUT_CHAR (padc);
			}

			while (*p) PUT_CHAR(*p--); /* output actual digits */

			if ((flagc & FLAGC_LEFTADJ) && width > 0 && (width -= tmp) > 0)
			{
				while (width-- > 0) PUT_CHAR (padc);
			}
			break;

invalid_format:
			while (percent < fmt) PUT_CHAR(*percent++);
			break;

		default:
			while (percent < fmt) PUT_CHAR(*percent++);
			/*
			 * Since we ignore an formatting argument it is no
			 * longer safe to obey the remaining formatting
			 * arguments as the arguments will no longer match
			 * the format specs.
			 */
			stop = 1;
			break;
		}
	}

done:
	if (fltfmt.ptr != fltfmt.sbuf)
		HAWK_MMGR_FREE (HAWK_MMGR_GETDFL(), fltfmt.ptr);
	if (fltout.ptr != fltout.sbuf)
		HAWK_MMGR_FREE (HAWK_MMGR_GETDFL(), fltout.ptr);
	return 0;

oops:
	if (fltfmt.ptr != fltfmt.sbuf)
		HAWK_MMGR_FREE (HAWK_MMGR_GETDFL(), fltfmt.ptr);
	if (fltout.ptr != fltout.sbuf)
		HAWK_MMGR_FREE (HAWK_MMGR_GETDFL(), fltout.ptr);
	return (hawk_ssize_t)-1;
}
#undef PUT_CHAR

