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

#include <hawk-chr.h>
#include "hawk-prv.h"

/* ---------------------------------------------------------- */
#include "uch-prop.h"
#include "uch-case.h"
/* ---------------------------------------------------------- */

#define UCH_PROP_MAP_INDEX(c) ((c) / HAWK_COUNTOF(uch_prop_page_0000))
#define UCH_PROP_PAGE_INDEX(c) ((c) % HAWK_COUNTOF(uch_prop_page_0000))

#define UCH_CASE_MAP_INDEX(c) ((c) / HAWK_COUNTOF(uch_case_page_0000))
#define UCH_CASE_PAGE_INDEX(c) ((c) % HAWK_COUNTOF(uch_case_page_0000))

#define UCH_IS_TYPE(c,type) \
	((c) >= 0 && (c) <= UCH_PROP_MAX && \
	 (uch_prop_map[UCH_PROP_MAP_INDEX(c)][UCH_PROP_PAGE_INDEX(c)] & (type)) != 0)

int hawk_is_uch_type (hawk_uch_t c, hawk_uch_prop_t type)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, type);
}

int hawk_is_uch_upper (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_UPPER);
}

int hawk_is_uch_lower (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_LOWER);
}

int hawk_is_uch_alpha (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_ALPHA);
}

int hawk_is_uch_digit (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_DIGIT);
}

int hawk_is_uch_xdigit (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_XDIGIT);
}

int hawk_is_uch_alnum (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_ALNUM);
}

int hawk_is_uch_space (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_SPACE);
}

int hawk_is_uch_print (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_PRINT);
}

int hawk_is_uch_graph (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_GRAPH);
}

int hawk_is_uch_cntrl (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_CNTRL);
}

int hawk_is_uch_punct (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_PUNCT);
}

int hawk_is_uch_blank (hawk_uch_t c)
{
	return UCH_IS_TYPE((hawk_uchu_t)c, HAWK_UCH_PROP_BLANK);
}

hawk_uch_t hawk_to_uch_upper (hawk_uch_t c)
{
	hawk_uchu_t uc = (hawk_uchu_t)c;
	if (uc >= 0 && uc <= UCH_CASE_MAX) 
	{
	 	uch_case_page_t* page;
		page = uch_case_map[UCH_CASE_MAP_INDEX(uc)];
		return uc - page[UCH_CASE_PAGE_INDEX(uc)].upper;
	}
	return c;
}

hawk_uch_t hawk_to_uch_lower (hawk_uch_t c)
{
	hawk_uchu_t uc = (hawk_uchu_t)c;
	if (uc >= 0 && uc <= UCH_CASE_MAX) 
	{
	 	uch_case_page_t* page;
		page = uch_case_map[UCH_CASE_MAP_INDEX(uc)];
		return uc + page[UCH_CASE_PAGE_INDEX(uc)].lower;
	}
	return c;
}

/* ---------------------------------------------------------- */

int hawk_is_bch_type (hawk_bch_t c, hawk_bch_prop_t type)
{
	switch (type)
	{
		case HAWK_OOCH_PROP_UPPER:
			return hawk_is_bch_upper(c);
		case HAWK_OOCH_PROP_LOWER:
			return hawk_is_bch_lower(c);
		case HAWK_OOCH_PROP_ALPHA:
			return hawk_is_bch_alpha(c);
		case HAWK_OOCH_PROP_DIGIT:
			return hawk_is_bch_digit(c);
		case HAWK_OOCH_PROP_XDIGIT:
			return hawk_is_bch_xdigit(c);
		case HAWK_OOCH_PROP_ALNUM:
			return hawk_is_bch_alnum(c);
		case HAWK_OOCH_PROP_SPACE:
			return hawk_is_bch_space(c);
		case HAWK_OOCH_PROP_PRINT:
			return hawk_is_bch_print(c);
		case HAWK_OOCH_PROP_GRAPH:
			return hawk_is_bch_graph(c);
		case HAWK_OOCH_PROP_CNTRL:
			return hawk_is_bch_cntrl(c);
		case HAWK_OOCH_PROP_PUNCT:
			return hawk_is_bch_punct(c);
		case HAWK_OOCH_PROP_BLANK:
			return hawk_is_bch_blank(c);
	}

	/* must not reach here */
	return 0;
}

#if !defined(hawk_to_bch_upper)
hawk_bch_t hawk_to_bch_upper (hawk_bch_t c)
{
	if(hawk_is_bch_lower(c)) return c & 95;
	return c;
}
#endif

#if !defined(hawk_to_bch_lower)
hawk_bch_t hawk_to_bch_lower (hawk_bch_t c)
{
	if(hawk_is_bch_upper(c)) return c | 32;
	return c;
}
#endif


/* ---------------------------------------------------------- */

static struct prop_tab_t
{
	const hawk_bch_t* name;
	int class;
} prop_tab[] =
{
	{ "alnum",  HAWK_OOCH_PROP_ALNUM },
	{ "alpha",  HAWK_OOCH_PROP_ALPHA },
	{ "blank",  HAWK_OOCH_PROP_BLANK },
	{ "cntrl",  HAWK_OOCH_PROP_CNTRL },
	{ "digit",  HAWK_OOCH_PROP_DIGIT },
	{ "graph",  HAWK_OOCH_PROP_GRAPH },
	{ "lower",  HAWK_OOCH_PROP_LOWER },
	{ "print",  HAWK_OOCH_PROP_PRINT },
	{ "punct",  HAWK_OOCH_PROP_PUNCT },
	{ "space",  HAWK_OOCH_PROP_SPACE },
	{ "upper",  HAWK_OOCH_PROP_UPPER },
	{ "xdigit", HAWK_OOCH_PROP_XDIGIT }
};

/* ---------------------------------------------------------- */

int hawk_ucstr_to_uch_prop (const hawk_uch_t* name, hawk_uch_prop_t* id)
{
	int left = 0, right = HAWK_COUNTOF(prop_tab) - 1, mid;
	while (left <= right)
	{
		int n;
		struct prop_tab_t* kwp;

		/*mid = (left + right) / 2;*/
		mid = left + (right - left) / 2;
		kwp = &prop_tab[mid];

		n = hawk_comp_ucstr_bcstr(name, kwp->name);
		if (n > 0) 
		{
			/* if left, right, mid were of hawk_oow_t,
			 * you would need the following line. 
			if (mid == 0) break;
			 */
			right = mid - 1;
		}
		else if (n < 0) left = mid + 1;
		else
		{ 
			*id = kwp->class;
			return 0;
		}
	}

	return -1;
}

int hawk_uchars_to_uch_prop (const hawk_uch_t* name, hawk_oow_t len, hawk_uch_prop_t* id)
{
	int left = 0, right = HAWK_COUNTOF(prop_tab) - 1, mid;
	while (left <= right)
	{
		int n;
		struct prop_tab_t* kwp;

		/*mid = (left + right) / 2;*/
		mid = left + (right - left) / 2;
		kwp = &prop_tab[mid];

		n = hawk_comp_uchars_bcstr(name, len, kwp->name);
		if (n < 0) 
		{
			/* if left, right, mid were of hawk_oow_t,
			 * you would need the following line. 
			if (mid == 0) break;
			 */
			right = mid - 1;
		}
		else if (n > 0) left = mid + 1;
		else 
		{ 
			*id = kwp->class;
			return 0;
		}
	}

	return -1;
}

/* ---------------------------------------------------------- */

int hawk_bcstr_to_bch_prop (const hawk_bch_t* name, hawk_bch_prop_t* id)
{
	int left = 0, right = HAWK_COUNTOF(prop_tab) - 1, mid;
	while (left <= right)
	{
		int n;
		struct prop_tab_t* kwp;

		/*mid = (left + right) / 2;*/
		mid = left + (right - left) / 2;
		kwp = &prop_tab[mid];

		n = hawk_comp_bcstr(name, kwp->name, 0);
		if (n > 0) 
		{
			/* if left, right, mid were of hawk_oow_t,
			 * you would need the following line. 
			if (mid == 0) break;
			 */
			right = mid - 1;
		}
		else if (n < 0) left = mid + 1;
		else
		{ 
			*id = kwp->class;
			return 0;
		}
	}

	return -1;
}

int hawk_bchars_to_bch_prop (const hawk_bch_t* name, hawk_oow_t len, hawk_bch_prop_t* id)
{
	int left = 0, right = HAWK_COUNTOF(prop_tab) - 1, mid;
	while (left <= right)
	{
		int n;
		struct prop_tab_t* kwp;

		/*mid = (left + right) / 2;*/
		mid = left + (right - left) / 2;
		kwp = &prop_tab[mid];

		n = hawk_comp_bchars_bcstr(name, len, kwp->name);
		if (n < 0) 
		{
			/* if left, right, mid were of hawk_oow_t,
			 * you would need the following line. 
			if (mid == 0) break;
			 */
			right = mid - 1;
		}
		else if (n > 0) left = mid + 1;
		else
		{ 
			*id = kwp->class;
			return 0;
		}
	}

	return -1;
}

/* ----------------------------------------------------------------------- */

/*
 * See http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c 
 */
struct interval 
{
	int first;
	int last;
};

/* auxiliary function for binary search in interval table */
static int bisearch(hawk_uch_t ucs, const struct interval *table, int max) 
{
	int min = 0;
	int mid;

	if (ucs < table[0].first || ucs > table[max].last) return 0;
	while (max >= min)
	{
		mid = (min + max) / 2;
		if (ucs > table[mid].last) min = mid + 1;
		else if (ucs < table[mid].first) max = mid - 1;
		else return 1;
	}

	return 0;
}

/* The following two functions define the column width of an ISO 10646
 * character as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - SOFT HYPHEN (U+00AD) has a column width of 1.
 *
 *    - Other format characters (general category code Cf in the Unicode
 *      database) and ZERO WIDTH SPACE (U+200B) have a column width of 0.
 *
 *    - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *      have a column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      Full-width (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

int hawk_get_ucwidth (hawk_uch_t uc)
{
	/* sorted list of non-overlapping intervals of non-spacing characters */
	/* generated by "uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B c" */
	static const struct interval combining[] = {
		{ 0x0300, 0x036F }, { 0x0483, 0x0486 }, { 0x0488, 0x0489 },
		{ 0x0591, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
		{ 0x05C4, 0x05C5 }, { 0x05C7, 0x05C7 }, { 0x0600, 0x0603 },
		{ 0x0610, 0x0615 }, { 0x064B, 0x065E }, { 0x0670, 0x0670 },
		{ 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
		{ 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
		{ 0x07A6, 0x07B0 }, { 0x07EB, 0x07F3 }, { 0x0901, 0x0902 },
		{ 0x093C, 0x093C }, { 0x0941, 0x0948 }, { 0x094D, 0x094D },
		{ 0x0951, 0x0954 }, { 0x0962, 0x0963 }, { 0x0981, 0x0981 },
		{ 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD },
		{ 0x09E2, 0x09E3 }, { 0x0A01, 0x0A02 }, { 0x0A3C, 0x0A3C },
		{ 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D },
		{ 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC },
		{ 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD },
		{ 0x0AE2, 0x0AE3 }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
		{ 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
		{ 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
		{ 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
		{ 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBC, 0x0CBC },
		{ 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
		{ 0x0CE2, 0x0CE3 }, { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D },
		{ 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 },
		{ 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E },
		{ 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC },
		{ 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 },
		{ 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E },
		{ 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 },
		{ 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 },
		{ 0x1032, 0x1032 }, { 0x1036, 0x1037 }, { 0x1039, 0x1039 },
		{ 0x1058, 0x1059 }, { 0x1160, 0x11FF }, { 0x135F, 0x135F },
		{ 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
		{ 0x1772, 0x1773 }, { 0x17B4, 0x17B5 }, { 0x17B7, 0x17BD },
		{ 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x17DD, 0x17DD },
		{ 0x180B, 0x180D }, { 0x18A9, 0x18A9 }, { 0x1920, 0x1922 },
		{ 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193B },
		{ 0x1A17, 0x1A18 }, { 0x1B00, 0x1B03 }, { 0x1B34, 0x1B34 },
		{ 0x1B36, 0x1B3A }, { 0x1B3C, 0x1B3C }, { 0x1B42, 0x1B42 },
		{ 0x1B6B, 0x1B73 }, { 0x1DC0, 0x1DCA }, { 0x1DFE, 0x1DFF },
		{ 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x2063 },
		{ 0x206A, 0x206F }, { 0x20D0, 0x20EF }, { 0x302A, 0x302F },
		{ 0x3099, 0x309A }, { 0xA806, 0xA806 }, { 0xA80B, 0xA80B },
		{ 0xA825, 0xA826 }, { 0xFB1E, 0xFB1E }, { 0xFE00, 0xFE0F },
		{ 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
		{ 0x10A01, 0x10A03 }, { 0x10A05, 0x10A06 }, { 0x10A0C, 0x10A0F },
		{ 0x10A38, 0x10A3A }, { 0x10A3F, 0x10A3F }, { 0x1D167, 0x1D169 },
		{ 0x1D173, 0x1D182 }, { 0x1D185, 0x1D18B }, { 0x1D1AA, 0x1D1AD },
		{ 0x1D242, 0x1D244 }, { 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F },
		{ 0xE0100, 0xE01EF }
	};

	/* test for 8-bit control characters */
	if (uc == 0) return 0;
	if (uc < 32 || (uc >= 0x7f && uc < 0xa0)) return -1;

	/* binary search in table of non-spacing characters */
	if (bisearch(uc, combining, sizeof(combining) / sizeof(struct interval) - 1)) return 0;

	/* if we arrive here, uc is not a combining or C0/C1 control character */

	if (uc >= 0x1100)
	{
		if (uc <= 0x115f || /* Hangul Jamo init. consonants */
		    uc == 0x2329 || uc == 0x232a ||
		    (uc >= 0x2e80 && uc <= 0xa4cf && uc != 0x303f) || /* CJK ... Yi */
		    (uc >= 0xac00 && uc <= 0xd7a3) || /* Hangul Syllables */
		    (uc >= 0xf900 && uc <= 0xfaff) || /* CJK Compatibility Ideographs */
		    (uc >= 0xfe10 && uc <= 0xfe19) || /* Vertical forms */
		    (uc >= 0xfe30 && uc <= 0xfe6f) || /* CJK Compatibility Forms */
		    (uc >= 0xff00 && uc <= 0xff60) || /* Fullwidth Forms */
		    (uc >= 0xffe0 && uc <= 0xffe6)
		#if (HAWK_SIZEOF_UCH_T  > 2)
		    || 
		    (uc >= 0x20000 && uc <= 0x2fffd) ||
		    (uc >= 0x30000 && uc <= 0x3fffd)
		#endif
		   )
		{
			return 2;
		}
	}

	return 1; 
}
