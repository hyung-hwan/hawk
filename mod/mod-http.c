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

#include "mod-http.h"
#include "../lib/hawk-prv.h"

static hawk_val_t* deref (hawk_rtx_t* rtx, hawk_val_t* value)
{
	while (HAWK_RTX_GETVALTYPE(rtx, value) == HAWK_VAL_REF)
	{
		value = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)value);
		if (!value) return HAWK_NULL;
	}
	return value;
}

static int get_text (hawk_rtx_t* rtx, hawk_oow_t index, hawk_val_t** value, hawk_oocs_t* text)
{
	*value = deref(rtx, hawk_rtx_getarg(rtx, index));
	if (!*value) return -1;
	if (HAWK_RTX_GETVALTYPE(rtx, *value) != HAWK_VAL_STR && HAWK_RTX_GETVALTYPE(rtx, *value) != HAWK_VAL_MBS)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "HTTP header arguments must be strings");
		return -1;
	}
	text->ptr = hawk_rtx_getvaloocstr(rtx, *value, &text->len);
	return text->ptr? 0: -1;
}

static int valid_name (const hawk_oocs_t* text)
{
	hawk_oow_t i;
	hawk_ooch_t c;

	if (text->len == 0) return 0;
	for (i = 0; i < text->len; i++)
	{
		c = text->ptr[i];
		if (c <= 32 || c >= 127 || hawk_find_oochar_in_oocstr(HAWK_T("()<>@,;:\\\"/[]?={}"), c)) return 0;
	}
	return 1;
}

static int valid_value (const hawk_oocs_t* text)
{
	hawk_oow_t i;
	hawk_ooch_t c;

	for (i = 0; i < text->len; i++)
	{
		c = text->ptr[i];
		if ((c >= 0 && c < 32 && c != '\t') || c == 127) return 0;
	}
	return 1;
}

static int get_integer (hawk_rtx_t* rtx, hawk_oow_t index, hawk_int_t* result)
{
	hawk_val_t* value = deref(rtx, hawk_rtx_getarg(rtx, index));

	if (!value) return -1;
	if (HAWK_RTX_GETVALTYPE(rtx, value) != HAWK_VAL_INT)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "HTTP numeric arguments must be integers");
		return -1;
	}
	return hawk_rtx_valtoint(rtx, value, result);
}

static int dispatch (hawk_rtx_t* rtx, hawk_http_cmd_t cmd)
{
	hawk_http_arg_t arg;
	hawk_val_t* name = HAWK_NULL;
	hawk_val_t* value = HAWK_NULL;
	hawk_val_t* zero;
	hawk_int_t number;
	hawk_uint16_t id;
	int n = -1;

	HAWK_MEMSET(&arg, 0, HAWK_SIZEOF(arg));
	arg.index = 1;

	if (cmd == HAWK_HTTP_GETHEADER || cmd == HAWK_HTTP_SETHEADER || cmd == HAWK_HTTP_ADDHEADER || cmd == HAWK_HTTP_DELHEADER)
	{
		if (get_text(rtx, 0, &name, &arg.name) <= -1) goto done;
		if (!valid_name(&arg.name))
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "invalid HTTP header name");
			goto done;
		}
	}
	if (cmd == HAWK_HTTP_SETHEADER || cmd == HAWK_HTTP_ADDHEADER)
	{
		if (get_text(rtx, 1, &value, &arg.value) <= -1) goto done;
		if (!valid_value(&arg.value))
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "invalid HTTP header value");
			goto done;
		}
	}
	if (cmd == HAWK_HTTP_GETHEADER && hawk_rtx_getnargs(rtx) >= 2)
	{
		if (get_integer(rtx, 1, &number) <= -1) goto done;
		if (number <= 0 || (hawk_uint_t)number > HAWK_TYPE_MAX(hawk_oow_t))
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "HTTP header index must be positive and representable");
			goto done;
		}
		arg.index = (hawk_oow_t)number;
	}
	if (cmd == HAWK_HTTP_SETSTATUS)
	{
		if (get_integer(rtx, 0, &number) <= -1) goto done;
		if (number < 200 || number > 599)
		{
			hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "HTTP response status is outside 200..599");
			goto done;
		}
		arg.status = (int)number;
	}

	/* prepare the setter result before invoking a callback that may mutate or
	 * commit the response. getter callbacks supply their own return values. */
	zero = hawk_rtx_makeintval(rtx, 0);
	if (HAWK_UNLIKELY(!zero)) goto done;

	if (hawk_rtx_getauxid(rtx, HAWK_HTTP_AUX_NAME, &id) <= -1) goto done;

	if (hawk_rtx_getauxctx(rtx, id, HAWK_NULL, HAWK_NULL) == 0)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EPERM, "HTTP request context is not attached");
		goto done;
	}

	/* dispatch to the actual handler */
	n = hawk_rtx_auxcall(rtx, id, (hawk_oow_t)cmd, &arg);

	if (n >= 0 && cmd >= HAWK_HTTP_SETSTATUS && cmd != HAWK_HTTP_GETSTATUS)
	{
		hawk_rtx_setretval(rtx, zero);
	}

done:
	if (arg.value.ptr) hawk_rtx_freevaloocstr(rtx, value, arg.value.ptr);
	if (arg.name.ptr) hawk_rtx_freevaloocstr(rtx, name, arg.name.ptr);
	return n;
}

#define DEFINE_HANDLER(name,cmd) \
static int fnc_##name (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi) \
{ \
	return dispatch(rtx, cmd); \
}

DEFINE_HANDLER(addheader,  HAWK_HTTP_ADDHEADER)
DEFINE_HANDLER(delheader,  HAWK_HTTP_DELHEADER)
DEFINE_HANDLER(flush,      HAWK_HTTP_FLUSH)
DEFINE_HANDLER(getheader,  HAWK_HTTP_GETHEADER)
DEFINE_HANDLER(getheaders, HAWK_HTTP_GETHEADERS)
DEFINE_HANDLER(getmethod,  HAWK_HTTP_GETMETHOD)
DEFINE_HANDLER(getpath,    HAWK_HTTP_GETPATH)
DEFINE_HANDLER(getstatus,  HAWK_HTTP_GETSTATUS)
DEFINE_HANDLER(setheader,  HAWK_HTTP_SETHEADER)
DEFINE_HANDLER(setstatus,  HAWK_HTTP_SETSTATUS)

static hawk_mod_fnc_tab_t fnctab[] =
{
	{ HAWK_T("addheader"),  { { 2, 2, HAWK_NULL }, fnc_addheader, 0 } },
	{ HAWK_T("delheader"),  { { 1, 1, HAWK_NULL }, fnc_delheader, 0 } },
	{ HAWK_T("flush"),      { { 0, 0, HAWK_NULL }, fnc_flush, 0 } },
	{ HAWK_T("getheader"),  { { 1, 2, HAWK_NULL }, fnc_getheader, 0 } },
	{ HAWK_T("getheaders"), { { 0, 0, HAWK_NULL }, fnc_getheaders, 0 } },
	{ HAWK_T("getmethod"),  { { 0, 0, HAWK_NULL }, fnc_getmethod, 0 } },
	{ HAWK_T("getpath"),    { { 0, 0, HAWK_NULL }, fnc_getpath, 0 } },
	{ HAWK_T("getstatus"),  { { 0, 0, HAWK_NULL }, fnc_getstatus, 0 } },
	{ HAWK_T("setheader"),  { { 2, 2, HAWK_NULL }, fnc_setheader, 0 } },
	{ HAWK_T("setstatus"),  { { 1, 1, HAWK_NULL }, fnc_setstatus, 0 } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

int hawk_mod_http (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = query;
	mod->unload = HAWK_NULL;
	mod->init = HAWK_NULL;
	mod->fini = HAWK_NULL;
	mod->ctx = HAWK_NULL;
	return 0;
}
