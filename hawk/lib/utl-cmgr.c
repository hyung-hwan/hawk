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

/* ------------------------------------------------------------------------ */

static hawk_cmgr_t builtin_cmgr[] =
{
	/* keep the order aligned with hawk_cmgr_id_t values in <hawk-utl.h> */
	{ hawk_utf8_to_uc,  hawk_uc_to_utf8 },
	{ hawk_utf16_to_uc, hawk_uc_to_utf16 },
	{ hawk_mb8_to_uc,   hawk_uc_to_mb8 }
};

hawk_cmgr_t* hawk_get_cmgr_by_id (hawk_cmgr_id_t id)
{
	return &builtin_cmgr[id];
}

static struct
{
	const hawk_bch_t* name;
	hawk_cmgr_id_t     id;
} builtin_cmgr_tab[] =
{
	{ "utf8",    HAWK_CMGR_UTF8 },
	{ "utf16",   HAWK_CMGR_UTF16 },
	{ "mb8",     HAWK_CMGR_MB8 }
};

hawk_cmgr_t* hawk_get_cmgr_by_bcstr (const hawk_bch_t* name)
{
	if (name)
	{
		hawk_oow_t i;

		for (i = 0; i < HAWK_COUNTOF(builtin_cmgr_tab); i++)
		{
			if (hawk_comp_bcstr(name, builtin_cmgr_tab[i].name, 0) == 0)
			{
				return &builtin_cmgr[builtin_cmgr_tab[i].id];
			}
		 }
	}

	return HAWK_NULL;
}

hawk_cmgr_t* hawk_get_cmgr_by_ucstr (const hawk_uch_t* name)
{
	if (name)
	{
		hawk_oow_t i;

		for (i = 0; i < HAWK_COUNTOF(builtin_cmgr_tab); i++)
		{
			if (hawk_comp_ucstr_bcstr(name, builtin_cmgr_tab[i].name, 0) == 0)
			{
				return &builtin_cmgr[builtin_cmgr_tab[i].id];
			}
		 }
	}

	return HAWK_NULL;
}

/* ------------------------------------------------------------------------ */

int hawk_conv_utf8_to_uchars (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* the source is length bound */
	return hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF8], 0);
}

int hawk_conv_uchars_to_utf8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	return hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF8]);
}

int hawk_conv_utf8_to_ucstr (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* null-terminated. */
	return hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF8], 0);
}

int hawk_conv_ucstr_to_utf8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	return hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF8]);
}

/* ------------------------------------------------------------------------ */

int hawk_conv_utf16_to_uchars (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* the source is length bound */
	return hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF16], 0);
}

int hawk_conv_uchars_to_utf16 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	return hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF16]);
}

int hawk_conv_utf16_to_ucstr (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* null-terminated. */
	return hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_UTF16], 0);
}

int hawk_conv_ucstr_to_utf16 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	return hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_UTF16]);
}

/* ------------------------------------------------------------------------ */

int hawk_conv_mb8_to_uchars (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* the source is length bound */
	return hawk_conv_bchars_to_uchars_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_MB8], 0);
}

int hawk_conv_uchars_to_mb8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* length bound */
	return hawk_conv_uchars_to_bchars_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_MB8]);
}

int hawk_conv_mb8_to_ucstr (const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen)
{
	/* null-terminated. */
	return hawk_conv_bcstr_to_ucstr_with_cmgr(bcs, bcslen, ucs, ucslen, &builtin_cmgr[HAWK_CMGR_MB8], 0);
}

int hawk_conv_ucstr_to_mb8 (const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen)
{
	/* null-terminated */
	return hawk_conv_ucstr_to_bcstr_with_cmgr(ucs, ucslen, bcs, bcslen, &builtin_cmgr[HAWK_CMGR_MB8]);
}

/* ------------------------------------------------------------------------ */
