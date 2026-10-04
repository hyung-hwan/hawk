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

#include "mod-m4.h"
#include <hawk-m4.h>
#include "../lib/hawk-prv.h"

enum io_mode_t
{
	IO_STR_TO_STR,
	IO_FILE_TO_STR,
	IO_STR_TO_FILE,
	IO_FILE_TO_FILE
};

static hawk_val_t* deref_val (hawk_rtx_t* rtx, hawk_val_t* val)
{
	while (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_REF)
		val = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)val);
	return val;
}

/*
 * BEGIN {m4::process("changequote([,])define([X],hello)X X X", v, m4::STR_TO_STR); print v;}
 * this should print "hello hello hello"
 */
static int fnc_process (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_m4_t* m4 = HAWK_NULL;
	hawk_m4_iostd_t input, output;
	hawk_val_t* retv, * tmp;
	hawk_val_t* a[3];
	hawk_oocs_t xstr[3];
	hawk_int_t mode;
	int i = 0, ret = 0;
	int output_is_file;

	HAWK_MEMSET(xstr, 0, HAWK_SIZEOF(xstr));
	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &mode) <= -1 || mode < IO_STR_TO_STR || mode > IO_FILE_TO_FILE)
	{
		ret = -2;
		goto done;
	}
	output_is_file = (mode == IO_STR_TO_FILE || mode == IO_FILE_TO_FILE);
	if (!output_is_file && HAWK_RTX_GETVALTYPE(rtx, hawk_rtx_getarg(rtx, 1)) != HAWK_VAL_REF)
	{
		ret = -2;
		goto done;
	}

	m4 = hawk_m4_openstdwithmmgr(hawk_rtx_getmmgr(rtx), 0, hawk_rtx_getcmgr(rtx), HAWK_NULL);
	if (!m4)
	{
		ret = -2;
		goto done;
	}

	a[0] = hawk_rtx_getarg(rtx, 0);
	xstr[0].ptr = hawk_rtx_getvaloocstr(rtx, a[0], &xstr[0].len);
	if (!xstr[0].ptr)
	{
		ret = -2;
		goto done;
	}
	i = 1;

	if (output_is_file)
	{
		a[1] = deref_val(rtx, hawk_rtx_getarg(rtx, 1));
		xstr[1].ptr = hawk_rtx_getvaloocstr(rtx, a[1], &xstr[1].len);
		if (!xstr[1].ptr)
		{
			ret = -2;
			goto done;
		}
		i = 2;
	}

	if (hawk_rtx_getnargs(rtx) >= 4)
	{
		a[2] = hawk_rtx_getarg(rtx, 3);
		xstr[2].ptr = hawk_rtx_getvaloocstr(rtx, a[2], &xstr[2].len);
		if (!xstr[2].ptr)
		{
			ret = -2;
			goto done;
		}
		i = 3;
		if (hawk_m4_setopt(m4, HAWK_M4_OPT_INCDIRS, xstr[2].ptr) <= -1)
		{
			ret = -2;
			goto done;
		}
	}

	HAWK_MEMSET(&input, 0, HAWK_SIZEOF(input));
	HAWK_MEMSET(&output, 0, HAWK_SIZEOF(output));
	input.type = (mode == IO_FILE_TO_STR || mode == IO_FILE_TO_FILE)? HAWK_M4_IOSTD_FILE: HAWK_M4_IOSTD_OOCS;
	if (input.type == HAWK_M4_IOSTD_FILE) input.u.file.path = xstr[0].ptr;
	else input.u.oocs = xstr[0];
	output.type = output_is_file? HAWK_M4_IOSTD_FILE: HAWK_M4_IOSTD_OOCS;
	if (output_is_file) output.u.file.path = xstr[1].ptr;

	if (hawk_m4_execstdwithio(m4, &input, &output) <= -1)
	{
		ret = -3;
		goto done;
	}

	if (!output_is_file)
	{
		tmp = hawk_rtx_makestrvalwithoocs(rtx, &output.u.oocs);
		hawk_m4_freemem(m4, output.u.oocs.ptr);
		if (!tmp)
		{
			ret = -1;
			goto done;
		}

		hawk_rtx_refupval(rtx, tmp);
		if (hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), tmp) <= -1) ret = -4;
		hawk_rtx_refdownval(rtx, tmp);
	}

done:
	while (i > 0)
	{
		--i;
		if (xstr[i].ptr) hawk_rtx_freevaloocstr(rtx, a[i], xstr[i].ptr);
	}
	if (m4) hawk_m4_close(m4);

	retv = hawk_rtx_makeintval(rtx, ret);
	if (!retv) return -1;
	hawk_rtx_setretval(rtx, retv);
	return 0;
}

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("process"), { { 3, 4, HAWK_T("vRvv") }, fnc_process, 0 } }
};

static hawk_mod_int_tab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("FILE_TO_FILE"), { IO_FILE_TO_FILE } },
	{ HAWK_T("FILE_TO_STR"),  { IO_FILE_TO_STR  } },
	{ HAWK_T("STR_TO_FILE"),  { IO_STR_TO_FILE  } },
	{ HAWK_T("STR_TO_STR"),   { IO_STR_TO_STR   } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	if (hawk_findmodsymfnc_noseterr(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym) >= 0) return 0;
	return hawk_findmodsymint(hawk, inttab, HAWK_COUNTOF(inttab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
}

int hawk_mod_m4 (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = query;
	mod->unload = unload;
	mod->init = init;
	mod->fini = fini;
	mod->ctx = HAWK_NULL;
	return 0;
}
