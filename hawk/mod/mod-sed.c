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

#include "mod-sed.h"
#include "../lib/sed-prv.h"

#if 0
static int fnc_errno (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	uctx_list_t* list;
	hawk_val_t* retv;

	list = rtx_to_list (rtx, fi);

	retv = hawk_rtx_makeintval (rtx, list->errnum);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static hawk_ooch_t* errmsg[] =
{
	HAWK_T("no error"),
	HAWK_T("out of memory"),
	HAWK_T("invalid data"),
	HAWK_T("not found"),
	HAWK_T("I/O error"),
	HAWK_T("parse error"),
	HAWK_T("duplicate data"),
	HAWK_T("unknown error")
};

static int fnc_errstr (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* retv;
	hawk_int_t errnum;

/*
	list = rtx_to_list (rtx, fi);

	if (hawk_rtx_getnargs (rtx) <= 0 ||
	    hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &errnum) <= -1)
	{
		errnum = list->errnum;
	}
*/

	ret = hawk_rtx_valtoint (rtx, hawk_rtx_getarg (rtx, 0), &id);
	if (ret <= -1) errnum = -1;


	if (errnum < 0 || errnum >= HAWK_COUNTOF(errmsg)) errnum = HAWK_COUNTOF(errmsg) - 1;

	retv = hawk_rtx_makestrvalwithstr (rtx, errmsg[errnum]);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}
#endif

static int fnc_file_to_file (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_sed_t* sed = HAWK_NULL;
	hawk_val_t* retv;
	hawk_val_t* a[3];
	hawk_oocs_t xstr[3];
	int i = 0, ret = 0;

	/* result = sed::file_to_file ("s/ABC/123/g", input_file, output_file [, option_string]) */

	sed = hawk_sed_openstdwithmmgr(hawk_rtx_getmmgr(rtx), 0, hawk_rtx_getcmgr(rtx), HAWK_NULL);
	if (sed == HAWK_NULL) 
	{
		ret = -2;
		goto oops;
	}

/* TODO hawk_set_opt (TRAIT) using the optional parameter */

	for (i = 0; i < 3; i++)
	{
		a[i] = hawk_rtx_getarg(rtx, i);
		xstr[i].ptr = hawk_rtx_getvaloocstr(rtx, a[i], &xstr[i].len);
		if (xstr[i].ptr == HAWK_NULL) 
		{
			ret = -2;
			goto oops;
		}
	}

	if (hawk_sed_compstdoocs(sed, &xstr[0]) <= -1) 
	{
		ret = -3; /* compile error */
		goto oops;
	}

	if (hawk_sed_execstdfile(sed, xstr[1].ptr, xstr[2].ptr, HAWK_NULL) <= -1) 
	{
		ret = -4;
		goto oops;
	}

oops:
	retv = hawk_rtx_makeintval(rtx, ret);
	if (retv == HAWK_NULL) retv = hawk_rtx_makeintval(rtx, -1);

	while (i > 0)
	{
		--i;
		hawk_rtx_freevaloocstr (rtx, a[i], xstr[i].ptr);
	}

	if (sed) hawk_sed_close (sed);
	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_str_to_str (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_sed_t* sed = HAWK_NULL;
	hawk_val_t* retv;
	hawk_val_t* a[2], * tmp;
	hawk_oocs_t xstr[2];
	hawk_oocs_t outstr;
	int i = 0, ret = 0, n;

	sed = hawk_sed_openstdwithmmgr (hawk_rtx_getmmgr(rtx), 0, hawk_rtx_getcmgr(rtx), HAWK_NULL);
	if (sed == HAWK_NULL) 
	{
		ret = -2;
		goto oops;
	}

/* TODO hawk_set_opt (TRAIT) using the optional parameter */
	for (i = 0; i < 2; i++)
	{
		a[i] = hawk_rtx_getarg(rtx, i);
		xstr[i].ptr = hawk_rtx_getvaloocstr(rtx, a[i], &xstr[i].len);
		if (xstr[i].ptr == HAWK_NULL) 
		{
			ret = -2;
			goto oops;
		}
	}

	if (hawk_sed_compstdoocs(sed, &xstr[0]) <= -1) 
	{
		ret = -3; /* compile error */
		goto oops;
	}

	if (hawk_sed_execstdxstr (sed, &xstr[1], &outstr, HAWK_NULL) <= -1) 
	{
		ret = -4;
		goto oops;
	}

	tmp = hawk_rtx_makestrvalwithoocs(rtx, &outstr);
	hawk_sed_freemem (sed, outstr.ptr);

	if (!tmp)
	{
		ret = -1;
		goto oops;
	}

	hawk_rtx_refupval (rtx, tmp);
	n = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), tmp);
	hawk_rtx_refdownval (rtx, tmp);
	if (n <= -1)
	{
		ret = -5;
		goto oops;
	}

oops:
	retv = hawk_rtx_makeintval(rtx, ret);
	if (retv == HAWK_NULL) retv = hawk_rtx_makeintval(rtx, -1);

	while (i > 0)
	{
		--i;
		hawk_rtx_freevaloocstr (rtx, a[i], xstr[i].ptr);
	}

	if (sed) hawk_sed_close (sed);
	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("file_to_file"),  { { 3, 3, HAWK_NULL },    fnc_file_to_file,  0 } },
	{ HAWK_T("str_to_str"),    { { 3, 3, HAWK_T("vvr")}, fnc_str_to_str,    0 } }
};

/* ------------------------------------------------------------------------ */

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{	
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

/* TODO: proper resource management */

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* TODO: anything? */
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	/* TODO: anything? */
}

int hawk_mod_sed (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_ntime_t tv;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;
	mod->ctx = HAWK_NULL;

	return 0;
}

