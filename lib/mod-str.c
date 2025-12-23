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

#include "mod-str.h"
#include "hawk-prv.h"

static int fnc_length (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return hawk_fnc_length(rtx, fi, 0);
}

static int fnc_normspace (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* normalize spaces
	 * - trim leading and trailing spaces
	 * - replace a series of spaces to a single space
	 */
	hawk_val_t* retv;
	hawk_val_t* a0;

	a0 = hawk_rtx_getarg(rtx, 0);
	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bch_t* str0;
			hawk_oow_t len0;

			str0 = hawk_rtx_valtobcstrdup(rtx, a0, &len0);
			if (HAWK_UNLIKELY(!str0)) return -1;
			len0 = hawk_compact_bchars(str0, len0);
			retv = hawk_rtx_makembsvalwithbchars(rtx, str0, len0);
			hawk_rtx_freemem (rtx, str0);
			break;
		}

		default:
		{
			hawk_ooch_t* str0;
			hawk_oow_t len0;

			str0 = hawk_rtx_valtooocstrdup(rtx, a0, &len0);
			if (HAWK_UNLIKELY(!str0)) return -1;
			len0 = hawk_compact_oochars(str0, len0);
			retv = hawk_rtx_makestrvalwithoochars(rtx, str0, len0);
			hawk_rtx_freemem (rtx, str0);
		}
	}

	if (HAWK_UNLIKELY(!retv)) return -1;
	hawk_rtx_setretval (rtx, retv);

	return 0;
}

static int trim (hawk_rtx_t* rtx, int flags)
{
	hawk_val_t* retv;
	hawk_val_t* a0;

	a0 = hawk_rtx_getarg(rtx, 0);

	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bcs_t str;
			hawk_bch_t* nstr;
			str.ptr = hawk_rtx_getvalbcstr(rtx, a0, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;
			nstr = hawk_trim_bchars(str.ptr, &str.len, flags);
			retv = hawk_rtx_makembsvalwithbchars(rtx, nstr, str.len);
			hawk_rtx_freevalbcstr(rtx, a0, str.ptr);
			break;
		}

		default:
		{
			hawk_oocs_t str;
			hawk_ooch_t* nstr;
			str.ptr = hawk_rtx_getvaloocstr(rtx, a0, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;
			/* because hawk_trim_oochars() returns the pointer and the length without
			 * affecting the string given, it's safe to pass the original value.
			 * hawk_rtx_getvaloocstr() doesn't duplicate the value if it's of
			 * the string type. */
			nstr = hawk_trim_oochars(str.ptr, &str.len, flags);
			retv = hawk_rtx_makestrvalwithoochars(rtx, nstr, str.len);
			hawk_rtx_freevaloocstr(rtx, a0, str.ptr);
			break;
		}
	}

	if (HAWK_UNLIKELY(!retv)) return -1;
	hawk_rtx_setretval (rtx, retv);
	return 0;
}

#define TRIM_FLAG_PAC_SPACES (1 << 0)

static int fnc_trim (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* str::trim(" test string ");
	   str::trim("   test   string  ", str::TRIM_PAC_SPACES); */
	if (hawk_rtx_getnargs(rtx) >= 2)
	{
		hawk_int_t iv;
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &iv) <= -1) return -1;
		if (iv & TRIM_FLAG_PAC_SPACES) return fnc_normspace(rtx, fi);
	}

	return trim(rtx, HAWK_TRIM_OOCHARS_LEFT | HAWK_TRIM_OOCHARS_RIGHT);
}
static int fnc_ltrim (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return trim(rtx, HAWK_TRIM_OOCHARS_LEFT);
}
static int fnc_rtrim (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return trim(rtx, HAWK_TRIM_OOCHARS_RIGHT);
}

static int is_class (hawk_rtx_t* rtx, hawk_ooch_prop_t ctype)
{
	hawk_val_t* a0;
	int tmp;

	a0 = hawk_rtx_getarg(rtx, 0);

	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bch_t* str0;
			hawk_oow_t len0;

			str0 = hawk_rtx_getvalbcstr(rtx, a0, &len0);
			if (HAWK_UNLIKELY(!str0)) return -1;

			if (len0 <= 0) tmp = 0;
			else
			{
				tmp = 1;
				do
				{
					len0--;
					if (!hawk_is_bch_type(str0[len0], ctype))
					{
						tmp = 0;
						break;
					}
				}
				while (len0 > 0);
			}

			hawk_rtx_freevalbcstr (rtx, a0, str0);
			break;
		}

		default:
		{
			hawk_ooch_t* str0;
			hawk_oow_t len0;

			str0 = hawk_rtx_getvaloocstr(rtx, a0, &len0);
			if (HAWK_UNLIKELY(!str0)) return -1;

			if (len0 <= 0) tmp = 0;
			else
			{
				tmp = 1;
				do
				{
					len0--;
					if (!hawk_is_ooch_type(str0[len0], ctype))
					{
						tmp = 0;
						break;
					}
				}
				while (len0 > 0);
			}
			hawk_rtx_freevaloocstr (rtx, a0, str0);
			break;
		}
	}

	a0 = hawk_rtx_makeintval (rtx, tmp);
	if (HAWK_UNLIKELY(!a0)) return -1;

	hawk_rtx_setretval (rtx, a0);
	return 0;
}

static int fnc_isalnum (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_ALNUM);
}

static int fnc_isalpha (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_ALPHA);
}

static int fnc_isblank (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_BLANK);
}

static int fnc_iscntrl (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_CNTRL);
}

static int fnc_isdigit (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_DIGIT);
}

static int fnc_isgraph (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_GRAPH);
}

static int fnc_islower (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_LOWER);
}

static int fnc_isprint (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_PRINT);
}

static int fnc_ispunct (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_PUNCT);
}

static int fnc_isspace (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_SPACE);
}

static int fnc_isupper (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_UPPER);
}

static int fnc_isxdigit (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	return is_class(rtx, HAWK_OOCH_PROP_XDIGIT);
}

static int fnc_frombcharcode (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* create a byte-string from a series of character codes.
	 * create a byte-character from a single character code.
	 *  - str::frombcharcode(65) for 'A'
	 *  - str::frombcharcode(65, 66, 67) for "ABC" */

	hawk_val_t* retv;
	hawk_oow_t nargs, i;
	hawk_bchu_t* ptr0; /* to guarantee the unsigned code conversion */

	nargs = hawk_rtx_getnargs(rtx);

	if (nargs == 1)
	{
		hawk_val_t* a0;
		hawk_int_t cc;

		a0 = hawk_rtx_getarg(rtx, 0);
		if (hawk_rtx_valtoint(rtx, a0, &cc) <= -1) return -1;

		retv = hawk_rtx_makebchrval(rtx, (hawk_bch_t)cc);
		if (HAWK_UNLIKELY(!retv)) return -1;
	}
	else
	{
		retv = hawk_rtx_makembsvalwithbchars(rtx, HAWK_NULL, nargs);
		if (HAWK_UNLIKELY(!retv)) return -1;

		ptr0 = (hawk_bchu_t*)((hawk_val_mbs_t*)retv)->val.ptr;

		for (i = 0; i < nargs; i++)
		{
			hawk_val_t* a0;
			hawk_int_t cc;

			a0 = hawk_rtx_getarg(rtx, i);
			if (hawk_rtx_valtoint(rtx, a0, &cc) <= -1)
			{
				hawk_rtx_freeval (rtx, retv, 0);
				return -1;
			}

			ptr0[i] = cc;
		}
	}

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_fromcharcode (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* create a string from a series of character codes.
	 * create a character from a single character code.
	 *  - str::fromcharcode(65) for 'A'
	 *  - str::fromcharcode(65, 66, 67) for "ABC" */

	hawk_val_t* retv;
	hawk_oow_t nargs, i;
	hawk_oochu_t* ptr0; /* to guarantee the unsigned code conversion */

	nargs = hawk_rtx_getnargs(rtx);

	if (nargs == 1)
	{
		hawk_val_t* a0;
		hawk_int_t cc;

		a0 = hawk_rtx_getarg(rtx, 0);
		if (hawk_rtx_valtoint(rtx, a0, &cc) <= -1) return -1;

		retv = hawk_rtx_makecharval(rtx, (hawk_ooch_t)cc);
		if (HAWK_UNLIKELY(!retv)) return -1;
	}
	else
	{
		retv = hawk_rtx_makestrvalwithoochars(rtx, HAWK_NULL, nargs);
		if (HAWK_UNLIKELY(!retv)) return -1;

		ptr0 = (hawk_oochu_t*)((hawk_val_str_t*)retv)->val.ptr;

		for (i = 0; i < nargs; i++)
		{
			hawk_val_t* a0;
			hawk_int_t cc;

			a0 = hawk_rtx_getarg(rtx, i);
			if (hawk_rtx_valtoint(rtx, a0, &cc) <= -1)
			{
				hawk_rtx_freeval (rtx, retv, 0);
				return -1;
			}

			ptr0[i] = cc;
		}
	}

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_tocharcode (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* return the numeric value for the first character or the character of the given position.
	 * you can use sprintf("%c", num_val) or str::fromcharcode(num_val) for reverse conversion. */
	hawk_val_t* retv;
	hawk_val_t* a0;
	hawk_int_t iv = -1;
	hawk_int_t pos = 0;

	a0 = hawk_rtx_getarg(rtx, 0);
	if  (hawk_rtx_getnargs(rtx) >= 2)
	{
		/* optional index. must be between 1 and the string length inclusive */
		hawk_val_t* a1;
		a1 = hawk_rtx_getarg(rtx, 1);
		if (hawk_rtx_valtoint(rtx, a1, &pos) <= -1) return -1;
		pos--; /* 1 based indexing. range check to be done before accessing below */
	}

	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bch_t* str0;
			hawk_oow_t len0;

			str0 = hawk_rtx_getvalbcstr(rtx, a0, &len0);
			if (HAWK_UNLIKELY(!str0)) return -1;

			if (pos >= 0 && pos < len0)
			{
				/* typecasting in case hawk_bch_t is signed */
				iv = (hawk_bchu_t)str0[pos];
			}

			hawk_rtx_freevalbcstr(rtx, a0, str0);
			break;
		}

		default:
		{
			hawk_ooch_t* str0;
			hawk_oow_t len0;

			str0 = hawk_rtx_getvaloocstr(rtx, a0, &len0);
			if (HAWK_UNLIKELY(!str0)) return -1;

			if (pos >= 0 && pos < len0)
			{
				/* typecasting in case hawk_bch_t is signed */
				iv = (hawk_oochu_t)str0[pos];
			}

			hawk_rtx_freevaloocstr(rtx, a0, str0);
			break;
		}
	}

	if (iv >= 0)
	{
		retv = hawk_rtx_makeintval(rtx, iv);
		if (HAWK_UNLIKELY(!retv)) return -1;
		hawk_rtx_setretval (rtx, retv);
	}

	return 0;
}

static int fnc_frommbs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* str::frommbs(@b"byte-string" [, "encoding-name"])
	 *
	 * if you use a supported encoding name, it may look like this:
	 *   a = str::frommbs(@b"\xC7\xD1\xB1\xDB", "cp949");
	 *   a = str::frommbs(@b"\xEB\x8F\x99\xEA\xB7\xB8\xEB\x9D\xBC\xEB\xAF\xB8", "utf8");
	 *   printf ("%K\n", a);
	 */
	hawk_val_t* a0, * r;
	hawk_cmgr_t* cmgr = hawk_rtx_getcmgr(rtx);

	if (hawk_rtx_getnargs(rtx) >= 2)
	{
		hawk_val_t* a1;
		hawk_oocs_t enc;

		a1 = hawk_rtx_getarg(rtx, 1);
		enc.ptr = hawk_rtx_getvaloocstr(rtx, a1, &enc.len);
		if (HAWK_UNLIKELY(!enc.ptr)) return -1;
		/* if encoding name is an empty string, hawk_findcmgr() returns the default cmgr.
		 * i don't want that behavior. */
		cmgr = (enc.len > 0 && enc.len == hawk_count_oocstr(enc.ptr))? hawk_get_cmgr_by_name(enc.ptr): HAWK_NULL;
		hawk_rtx_freevaloocstr (rtx, a1, enc.ptr);

		if (!cmgr)
		{
			/* if the encoding name is not known, return a zero-length string */
			r = hawk_rtx_makestrvalwithoochars(rtx, HAWK_NULL, 0); /* this never fails for length 0 */
			goto done;
		}
	}

	a0 = hawk_rtx_getarg(rtx, 0);
	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_STR:
			r = a0;
			break;

		default:
		{
			hawk_oocs_t str;
			str.ptr = hawk_rtx_getvaloocstrwithcmgr(rtx, a0, &str.len, cmgr);
			if (!str.ptr) return -1;
			r = hawk_rtx_makestrvalwithoocs(rtx, &str);
			hawk_rtx_freevaloocstr (rtx, a0, str.ptr);
			if (!r) return -1;
			break;
		}
	}

done:
	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_tombs (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* str::tombs("string", [, "encoding-name"])
	 *
	 * if you use a supported encoding name, it may look like this:
	 *   a = str::tombs("\uD55C\uAE00", "cp949");
	 *   printf (@b"%K\n", a);
	 */

	hawk_val_t* a0, * r;
	hawk_cmgr_t* cmgr = hawk_rtx_getcmgr(rtx);

	if (hawk_rtx_getnargs(rtx) >= 2)
	{
		hawk_val_t* a1;
		hawk_oocs_t enc;
		a1 = hawk_rtx_getarg(rtx, 1);
		enc.ptr = hawk_rtx_getvaloocstr(rtx, a1, &enc.len);
		if (!enc.ptr) return -1;
		/* if encoding name is an empty string, hawk_findcmgr() returns the default cmgr.
		 * i don't want that behavior. */
		cmgr = (enc.len > 0 && enc.len == hawk_count_oocstr(enc.ptr))? hawk_get_cmgr_by_name(enc.ptr): HAWK_NULL;
		hawk_rtx_freevaloocstr (rtx, a1, enc.ptr);

		if (!cmgr)
		{
			/* if the encoding name is not known, return a zero-length string */
			r = hawk_rtx_makembsvalwithbchars(rtx, HAWK_NULL, 0); /* this never fails for length 0 */
			goto done;
		}
	}

	a0 = hawk_rtx_getarg(rtx, 0);
	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		{
			/* no conversion, but make it a byte string */
			hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, a0);
			r = hawk_rtx_makembsvalwithbchars(rtx, &tmp, 1);
			if (HAWK_UNLIKELY(!r)) return -1;
			break;
		}

		case HAWK_VAL_MBS:
			/* no conversion */
			r = a0;
			break;

		case HAWK_VAL_BOB: /* BOB must fall thru and reach below */
		default:
		{
			hawk_bcs_t str;
			str.ptr = hawk_rtx_getvalbcstrwithcmgr(rtx, a0, &str.len, cmgr);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;
			r = hawk_rtx_makembsvalwithbcs(rtx, &str);
			hawk_rtx_freevalbcstr (rtx, a0, str.ptr);
			if (HAWK_UNLIKELY(!r)) return -1;
			break;
		}
	}

done:
	hawk_rtx_setretval (rtx, r);
	return 0;
}

static int fnc_fromhex (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* retv;
	hawk_val_t* a0;

	a0 = hawk_rtx_getarg(rtx, 0);

	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bcs_t str;
			hawk_oow_t i;
			hawk_oow_t len;
			hawk_uint8_t v;
			hawk_oow_t x;

			str.ptr = hawk_rtx_getvalbcstr(rtx, a0, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			len = str.len >> 1;

			retv = hawk_rtx_makembsvalwithbchars(rtx, HAWK_NULL, len + (str.len & 1));
			for (i = 0, x = 0; i < len; i++, x++)
			{
				if (str.ptr[x] >= '0' && str.ptr[x] <= '9') v = str.ptr[x] - '0';
				else if (str.ptr[x] >= 'a' && str.ptr[x] <= 'f') v = str.ptr[x] - 'a' + 10;
				else if (str.ptr[x] >= 'A' && str.ptr[x] <= 'F') v = str.ptr[x] - 'A' + 10;
				else v = 0;

				x = x + 1;
				v <<= 4;
				if (str.ptr[x] >= '0' && str.ptr[x] <= '9') v += str.ptr[x] - '0';
				else if (str.ptr[x] >= 'a' && str.ptr[x] <= 'f') v += str.ptr[x] - 'a' + 10;
				else if (str.ptr[x] >= 'A' && str.ptr[x] <= 'F') v += str.ptr[x] - 'A' + 10;

				((hawk_val_mbs_t*)retv)->val.ptr[i] = v;
			}

			if (str.len & 1)
			{
				/* trailing digit */
				if (str.ptr[x] >= '0' && str.ptr[x] <= '9') v = str.ptr[x] - '0';
				else if (str.ptr[x] >= 'a' && str.ptr[x] <= 'f') v = str.ptr[x] - 'a' + 10;
				else if (str.ptr[x] >= 'A' && str.ptr[x] <= 'F') v = str.ptr[x] - 'A' + 10;
				else v = 0;
				((hawk_val_mbs_t*)retv)->val.ptr[i] = v;
			}

			hawk_rtx_freevalbcstr(rtx, a0, str.ptr);
			break;
		}

		default:
		{
			hawk_oocs_t str;
			hawk_oow_t i;
			hawk_oow_t len;
			hawk_uint8_t v;
			hawk_oow_t x;

			str.ptr = hawk_rtx_getvaloocstr(rtx, a0, &str.len);
			if (HAWK_UNLIKELY(!str.ptr)) return -1;

			len = str.len >> 1;

			retv = hawk_rtx_makembsvalwithbchars(rtx, HAWK_NULL, len + (str.len & 1));
			for (i = 0, x = 0; i < len; i++, x++)
			{
				if (str.ptr[x] >= '0' && str.ptr[x] <= '9') v = str.ptr[x] - '0';
				else if (str.ptr[x] >= 'a' && str.ptr[x] <= 'f') v = str.ptr[x] - 'a' + 10;
				else if (str.ptr[x] >= 'A' && str.ptr[x] <= 'F') v = str.ptr[x] - 'A' + 10;
				else v = 0;

				x = x + 1;
				v <<= 4;
				if (str.ptr[x] >= '0' && str.ptr[x] <= '9') v += str.ptr[x] - '0';
				else if (str.ptr[x] >= 'a' && str.ptr[x] <= 'f') v += str.ptr[x] - 'a' + 10;
				else if (str.ptr[x] >= 'A' && str.ptr[x] <= 'F') v += str.ptr[x] - 'A' + 10;

				((hawk_val_mbs_t*)retv)->val.ptr[i] = v;
			}

			if (str.len & 1)
			{
				/* trailing digit */
				if (str.ptr[x] >= '0' && str.ptr[x] <= '9') v = str.ptr[x] - '0';
				else if (str.ptr[x] >= 'a' && str.ptr[x] <= 'f') v = str.ptr[x] - 'a' + 10;
				else if (str.ptr[x] >= 'A' && str.ptr[x] <= 'F') v = str.ptr[x] - 'A' + 10;
				else v = 0;
				((hawk_val_mbs_t*)retv)->val.ptr[i] = v;
			}

			hawk_rtx_freevaloocstr(rtx, a0, str.ptr);
			break;
		}
	}

	if (HAWK_UNLIKELY(!retv)) return -1;
	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_tohex (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* retv;
	hawk_val_t* a0;
	hawk_bcs_t str;
	hawk_oow_t i;

	a0 = hawk_rtx_getarg(rtx, 0);

	/* if the argument is not a multi-byte string, the conversion
	 * will be performed in the default encoding (utf-8). if you
	 * want a different encoding, call str::tombs() first.
	 * - str::tohex(@b"xyz")
	 * - str::tohex("🤩")
	 * - str::tohex(str::tombs("🤩"))
	 * - str::tohex(str::tombs("🤩", "utf8")) */
	str.ptr = hawk_rtx_getvalbcstr(rtx, a0, &str.len);
	if (HAWK_UNLIKELY(!str.ptr)) return -1;

	retv = hawk_rtx_makembsvalwithbchars(rtx, HAWK_NULL, str.len * 2);
	for (i = 0; i < str.len; i++)
	{
		hawk_fmt_uintmax_to_bcstr(
			&((hawk_val_mbs_t*)retv)->val.ptr[i * 2], 2,
			(hawk_uint8_t)str.ptr[i], 16 | HAWK_FMT_UINTMAX_NONULL,
			2, '0', HAWK_NULL);
	}

	hawk_rtx_freevalbcstr(rtx, a0, str.ptr);

	if (HAWK_UNLIKELY(!retv)) return -1;
	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_tonum (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* str::tonum(value) */
	/* str::tonum(string/character, base) */

	/*hawk_t* hawk = hawk_rtx_gethawk(rtx);*/
	hawk_val_t* retv;
	hawk_val_t* a0;
	hawk_int_t lv;
	hawk_flt_t rv;
	int rx;

	a0 = hawk_rtx_getarg(rtx, 0);

	if (hawk_rtx_getnargs(rtx) >= 2)
	{
		switch (HAWK_RTX_GETVALTYPE(rtx, a0))
		{
			case HAWK_VAL_BCHR:
			{
				/* if the value is known to be a string, it supports the optional
				 * base parameter */
				hawk_val_t* a1 = hawk_rtx_getarg(rtx, 1);
				hawk_int_t base;
				hawk_bch_t tmp = HAWK_RTX_GETBCHRFROMVAL(rtx, a0);

				if (hawk_rtx_valtoint(rtx, a1, &base) <= -1) return -1;
				rx = hawk_bchars_to_num(
					HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), base),
					&tmp, 1, &lv, &rv
				);
				break;
			}

			case HAWK_VAL_MBS:
			case HAWK_VAL_BOB: /* assume hawk_val_mbs_t and hawk_val_bob_t are the same */
			{
				/* if the value is known to be a byte string, it supports the optional
				 * base parameter */
				hawk_val_t* a1 = hawk_rtx_getarg(rtx, 1);
				hawk_int_t base;

				if (hawk_rtx_valtoint(rtx, a1, &base) <= -1) return -1;
				rx = hawk_bchars_to_num(
					HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), base),
					((hawk_val_mbs_t*)a0)->val.ptr,
					((hawk_val_mbs_t*)a0)->val.len,
					&lv, &rv
				);
				break;
			}

			case HAWK_VAL_CHAR:
			{
				/* if the value is known to be a string, it supports the optional
				 * base parameter */
				hawk_val_t* a1 = hawk_rtx_getarg(rtx, 1);
				hawk_int_t base;
				hawk_ooch_t tmp = HAWK_RTX_GETCHARFROMVAL(rtx, a0);

				if (hawk_rtx_valtoint(rtx, a1, &base) <= -1) return -1;
				rx = hawk_oochars_to_num(
					HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), base),
					&tmp, 1, &lv, &rv
				);
				break;
			}

			case HAWK_VAL_STR:
			{
				/* if the value is known to be a string, it supports the optional
				 * base parameter */
				hawk_val_t* a1 = hawk_rtx_getarg(rtx, 1);
				hawk_int_t base;

				if (hawk_rtx_valtoint(rtx, a1, &base) <= -1) return -1;
				rx = hawk_oochars_to_num(
					HAWK_OOCHARS_TO_NUM_MAKE_OPTION(0, 0, HAWK_RTX_IS_STRIPSTRSPC_ON(rtx), base),
					((hawk_val_str_t*)a0)->val.ptr,
					((hawk_val_str_t*)a0)->val.len,
					&lv, &rv
				);
				break;
			}

			default:
				goto val_to_num;
		}
	}
	else
	{
	val_to_num:
		rx = hawk_rtx_valtonum(rtx, a0, &lv, &rv);
	}

	if (rx == 0)
	{
		retv = hawk_rtx_makeintval(rtx, lv);
	}
	else if (rx >= 1)
	{
		retv = hawk_rtx_makefltval(rtx, rv);
	}
	else
	{
		retv = hawk_rtx_makeintval(rtx, 0);
	}

	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_subchar (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_oow_t nargs;
	hawk_val_t* a0, * a1, * r;
	hawk_int_t lindex;
	int n;

	nargs = hawk_rtx_getnargs(rtx);
	HAWK_ASSERT (nargs >= 2 && nargs <= 3);

	a0 = hawk_rtx_getarg(rtx, 0);
	a1 = hawk_rtx_getarg(rtx, 1);

	n = hawk_rtx_valtoint(rtx, a1, &lindex);
	if (n <= -1) return -1;

	lindex = lindex - 1;

	switch (HAWK_RTX_GETVALTYPE(rtx, a0))
	{
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		case HAWK_VAL_BOB:
		{
			hawk_bch_t* str;
			hawk_oow_t len;

			str = hawk_rtx_getvalbcstr(rtx, a0, &len);
			if (!str) return -1;

			if (lindex >= 0 && lindex < (hawk_int_t)len)
				r = hawk_rtx_makebchrval(rtx, str[lindex]);
			else
				r = hawk_rtx_makenilval(rtx);

			hawk_rtx_freevalbcstr (rtx, a0, str);
			break;
		}

		default:
		{
			hawk_ooch_t* str;
			hawk_oow_t len;

			str = hawk_rtx_getvaloocstr(rtx, a0, &len);
			if (!str) return -1;

			if (lindex >= 0 && lindex < (hawk_int_t)len)
				r = hawk_rtx_makecharval(rtx, str[lindex]);
			else
				r = hawk_rtx_makenilval(rtx);

			hawk_rtx_freevaloocstr (rtx, a0, str);
			break;
		}
	}

	if (HAWK_UNLIKELY(!r)) return -1;
	hawk_rtx_setretval (rtx, r);
	return 0;
}

/* ----------------------------------------------------------------------- */

#define A_MAX HAWK_TYPE_MAX(hawk_oow_t)

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("frombcharcode"), { { 0, A_MAX, HAWK_NULL },  fnc_frombcharcode,     0 } },
	{ HAWK_T("fromcharcode"),  { { 0, A_MAX, HAWK_NULL },  fnc_fromcharcode,      0 } },
	{ HAWK_T("fromhex"),       { { 1, 1, HAWK_NULL },      fnc_fromhex,           0 } },
	{ HAWK_T("frommbs"),       { { 1, 2, HAWK_NULL },      fnc_frommbs,           0 } },
	{ HAWK_T("gensub"),        { { 3, 4, HAWK_T("xvvv")},  hawk_fnc_gensub,       0 } },
	{ HAWK_T("gsub"),          { { 2, 3, HAWK_T("xvr")},   hawk_fnc_gsub,         0 } },
	{ HAWK_T("index"),         { { 2, 3, HAWK_NULL },      hawk_fnc_index,        0 } },
	{ HAWK_T("isalnum"),       { { 1, 1, HAWK_NULL },      fnc_isalnum,           0 } },
	{ HAWK_T("isalpha"),       { { 1, 1, HAWK_NULL },      fnc_isalpha,           0 } },
	{ HAWK_T("isblank"),       { { 1, 1, HAWK_NULL },      fnc_isblank,           0 } },
	{ HAWK_T("iscntrl"),       { { 1, 1, HAWK_NULL },      fnc_iscntrl,           0 } },
	{ HAWK_T("isdigit"),       { { 1, 1, HAWK_NULL },      fnc_isdigit,           0 } },
	{ HAWK_T("isgraph"),       { { 1, 1, HAWK_NULL },      fnc_isgraph,           0 } },
	{ HAWK_T("islower"),       { { 1, 1, HAWK_NULL },      fnc_islower,           0 } },
	{ HAWK_T("isprint"),       { { 1, 1, HAWK_NULL },      fnc_isprint,           0 } },
	{ HAWK_T("ispunct"),       { { 1, 1, HAWK_NULL },      fnc_ispunct,           0 } },
	{ HAWK_T("isspace"),       { { 1, 1, HAWK_NULL },      fnc_isspace,           0 } },
	{ HAWK_T("isupper"),       { { 1, 1, HAWK_NULL },      fnc_isupper,           0 } },
	{ HAWK_T("isxdigit"),      { { 1, 1, HAWK_NULL },      fnc_isxdigit,          0 } },
	{ HAWK_T("length"),        { { 1, 1, HAWK_NULL },      fnc_length,            0 } },
	{ HAWK_T("ltrim"),         { { 1, 1, HAWK_NULL },      fnc_ltrim,             0 } },
	{ HAWK_T("match"),         { { 2, 4, HAWK_T("vxvr") }, hawk_fnc_match,        0 } },
	{ HAWK_T("normspace"),     { { 1, 1, HAWK_NULL },      fnc_normspace,         0 } }, /* deprecated, use trim("xxx", str::TRIM_PAC_SPACES) */
	{ HAWK_T("printf"),        { { 1, A_MAX, HAWK_NULL },  hawk_fnc_sprintf,      0 } },
	{ HAWK_T("rindex"),        { { 2, 3, HAWK_NULL },      hawk_fnc_rindex,       0 } },
	{ HAWK_T("rtrim"),         { { 1, 1, HAWK_NULL },      fnc_rtrim,             0 } },
	{ HAWK_T("split"),         { { 2, 3, HAWK_T("vrx") },  hawk_fnc_split,        0 } },
	{ HAWK_T("splita"),        { { 2, 3, HAWK_T("vrx") },  hawk_fnc_splita,       0 } }, /* split to array. asplit is not a good name for this */
	{ HAWK_T("sub"),           { { 2, 3, HAWK_T("xvr") },  hawk_fnc_sub,          0 } },
	{ HAWK_T("subchar"),       { { 2, 2, HAWK_NULL },      fnc_subchar,           0 } },
	{ HAWK_T("substr"),        { { 2, 3, HAWK_NULL },      hawk_fnc_substr,       0 } },
	{ HAWK_T("tocharcode"),    { { 1, 2, HAWK_NULL },      fnc_tocharcode,        0 } },
	{ HAWK_T("tohex"),         { { 1, 1, HAWK_NULL },      fnc_tohex,             0 } },
	{ HAWK_T("tolower"),       { { 1, 1, HAWK_NULL },      hawk_fnc_tolower,      0 } },
	{ HAWK_T("tombs"),         { { 1, 2, HAWK_NULL },      fnc_tombs,             0 } },
	{ HAWK_T("tonum"),         { { 1, 2, HAWK_NULL },      fnc_tonum,             0 } },
	{ HAWK_T("toupper"),       { { 1, 1, HAWK_NULL },      hawk_fnc_toupper,      0 } },
	{ HAWK_T("trim"),          { { 1, 2, HAWK_NULL },      fnc_trim,              0 } }
};

static hawk_mod_int_tab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("TRIM_PAC_SPACES"), { TRIM_FLAG_PAC_SPACES } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	if (hawk_findmodsymfnc_noseterr(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym) >= 0) return 0;
	return hawk_findmodsymint(hawk, inttab, HAWK_COUNTOF(inttab), name, sym);
}

/* TODO: proper resource management */

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* TODO: anything */
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	/* TODO: anything */
}

int hawk_mod_str (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;
	/*
	mod->ctx...
	 */

	return 0;
}

