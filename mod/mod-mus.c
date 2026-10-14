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

#include "mod-mus.h"
#include <hawk-mus.h>
#include "../lib/hawk-prv.h"

enum io_mode_t
{
	IO_STR_TO_STR,
	IO_FILE_TO_STR,
	IO_STR_TO_FILE,
	IO_FILE_TO_FILE
};

typedef struct rtx_data_t rtx_data_t;
struct rtx_data_t
{
	hawk_ooch_t errmsg[HAWK_ERRMSG_CAPA];
};

typedef struct array_t array_t;
struct array_t
{
	array_t* next;
	hawk_val_t* value;
	hawk_oow_t count;
	hawk_val_t** items;
};

typedef struct data_ctx_t data_ctx_t;
struct data_ctx_t
{
	hawk_rtx_t* rtx;
	hawk_ooch_t* text;
	array_t* arrs;
};

static rtx_data_t* get_rtx_data (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_rbt_pair_t* pair;
	hawk_errinf_t error;

	/* module init/fini modify this registry under the module mutex. The
	 * returned record belongs only to this runtime, whose calls are serial. */
	if (hawk_mtx_lock(rtx->hawk->modmtx, HAWK_NULL) <= -1)
	{
		hawk_geterrinf(rtx->hawk, &error);
		hawk_rtx_seterrinf(rtx, &error);
		return HAWK_NULL;
	}
	pair = hawk_rbt_search(fi->mod->ctx, &rtx, HAWK_SIZEOF(rtx));
	hawk_mtx_unlock(rtx->hawk->modmtx);
	HAWK_ASSERT(pair != HAWK_NULL);
	return HAWK_RBT_VPTR(pair);
}

static hawk_val_t* deref_val (hawk_rtx_t* rtx, hawk_val_t* val)
{
	while (HAWK_RTX_GETVALTYPE(rtx, val) == HAWK_VAL_REF)
		val = hawk_rtx_getrefval(rtx, (hawk_val_ref_t*)val);
	return val;
}

static int copy_rtx_error (hawk_mus_t* mus, hawk_rtx_t* rtx)
{
	hawk_errinf_t error;
	hawk_rtx_geterrinf(rtx, &error);
	hawk_gem_seterrinf(hawk_mus_getgem(mus), &error);
	return -1;
}

static int data_get (hawk_mus_t* mus, void* value, const hawk_ooch_t* name, hawk_oow_t len, void** result, void* ctx)
{
	data_ctx_t* dc = ctx;
	hawk_val_t* val = deref_val(dc->rtx, value);

	*result = HAWK_NULL;
	if (HAWK_RTX_GETVALTYPE(dc->rtx, val) != HAWK_VAL_MAP) return 0;
	*result = hawk_rtx_getmapvalfld(dc->rtx, val, name, len);
	return *result? 1: 0;
}

static int data_type (hawk_mus_t* mus, void* value, void* ctx)
{
	data_ctx_t* dc = ctx;
	hawk_val_t* val = deref_val(dc->rtx, value);

	switch (HAWK_RTX_GETVALTYPE(dc->rtx, val))
	{
		case HAWK_VAL_NIL:
			return HAWK_MUS_VAL_NIL;

		case HAWK_VAL_BOOL:
			return hawk_rtx_valtobool(dc->rtx, val)? HAWK_MUS_VAL_TRUE: HAWK_MUS_VAL_FALSE;

		case HAWK_VAL_MAP:
			return HAWK_MUS_VAL_MAP;

		case HAWK_VAL_ARR:
			return HAWK_MUS_VAL_ARR;

		case HAWK_VAL_CHAR:
		case HAWK_VAL_BCHR:
		case HAWK_VAL_INT:
		case HAWK_VAL_FLT:
		case HAWK_VAL_STR:
		case HAWK_VAL_MBS:
			return HAWK_MUS_VAL_SCALAR;

		default:
			hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EINVAL, "unsupported mustache data value type");
			return -1;
	}
}

static int data_str (hawk_mus_t* mus, void* value, const hawk_ooch_t** ptr, hawk_oow_t* len, void* ctx)
{
	data_ctx_t* dc = ctx;
	hawk_val_t* val = deref_val(dc->rtx, value);

	hawk_rtx_valtostr_out_t out;

	if (HAWK_RTX_GETVALTYPE(dc->rtx, val) == HAWK_VAL_MAP || HAWK_RTX_GETVALTYPE(dc->rtx, val) == HAWK_VAL_ARR)
	{
		hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EINVAL, "cannot interpolate a mustache map or array");
		return -1;
	}

	if (dc->text)
	{
		hawk_rtx_freemem(dc->rtx, dc->text);
		dc->text = HAWK_NULL;
	}

	out.type = HAWK_RTX_VALTOSTR_CPLDUP;
	if (hawk_rtx_valtostr(dc->rtx, val, &out) <= -1) return copy_rtx_error(mus, dc->rtx);
	dc->text = out.u.cpldup.ptr;

	*ptr = dc->text;
	*len = out.u.cpldup.len;
	return 0;
}

static int data_count (hawk_mus_t* mus, void* value, hawk_oow_t* count, void* ctx)
{
	data_ctx_t* dc = ctx;
	*count = hawk_rtx_getarrvaltally(dc->rtx, deref_val(dc->rtx, value));
	return 0;
}

static int data_item (hawk_mus_t* mus, void* value, hawk_oow_t index, void** result, void* ctx)
{
	data_ctx_t* dc = ctx;
	hawk_val_t* val;
	array_t* arr;

	val = deref_val(dc->rtx, value);

	for (arr = dc->arrs; arr; arr = arr->next)
	{
		if (arr->value == val) break;
	}

	if (!arr)
	{
		hawk_val_arr_itr_t itr;
		hawk_val_arr_itr_t* iptr;
		hawk_oow_t count, n;

		/* use occupied slots in ascending index order, like json::stringify().
		 * a view avoids scanning sparse arrays afresh for every rendered item.
		 * the root argument keeps all borrowed values alive during rendering. */
		count = hawk_rtx_getarrvaltally(dc->rtx, val);
		if (count > (HAWK_TYPE_MAX(hawk_oow_t) - HAWK_SIZEOF(*arr)) / HAWK_SIZEOF(*arr->items))
		{
			hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_ENOMEM, "mustache array too large");
			return -1;
		}

		arr = hawk_mus_allocmem(mus, HAWK_SIZEOF(*arr) + count * HAWK_SIZEOF(*arr->items));
		if (HAWK_UNLIKELY(!arr)) return -1;

		arr->value = val;
		arr->count = count;
		arr->items = (hawk_val_t**)(arr + 1);

		iptr = hawk_rtx_getfirstarrvalitr(dc->rtx, val, &itr);
		for (n = 0; iptr; n++)
		{
			HAWK_ASSERT(n < count);
			arr->items[n] = iptr->elem;
			iptr = hawk_rtx_getnextarrvalitr(dc->rtx, val, &itr);
		}
		HAWK_ASSERT(n == count);
		arr->next = dc->arrs;
		dc->arrs = arr;
	}
	if (index >= arr->count)
	{
		hawk_mus_seterrbfmt(mus, HAWK_NULL, HAWK_EINVAL, "invalid mustache array index");
		return -1;
	}
	*result = arr->items[index];
	return 0;
}

static const hawk_mus_data_t data_callbacks =
{
	data_get,
	data_type,
	data_str,
	data_count,
	data_item
};

static int fnc_process (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	rtx_data_t* state;
	hawk_mus_t* mus = HAWK_NULL;
	hawk_mus_iostd_t input, output;
	hawk_val_t* args[3];
	hawk_oocs_t strings[3];
	hawk_val_t* retv;
	hawk_int_t mode;
	hawk_flt_t fmode;
	hawk_errinf_t error;
	data_ctx_t dc;
	array_t* array;
	int n, i, output_is_file;
	int result = -1;

	state = get_rtx_data(rtx, fi);
	if (!state) return -1;

	state->errmsg[0] = '\0';
	HAWK_MEMSET(strings, 0, HAWK_SIZEOF(strings));
	HAWK_MEMSET(&input, 0, HAWK_SIZEOF(input));
	HAWK_MEMSET(&output, 0, HAWK_SIZEOF(output));
	HAWK_MEMSET(&dc, 0, HAWK_SIZEOF(dc));

	dc.rtx = rtx;

	/* check a floating mode before converting to integer: NaN, infinity,
	 * out-of-range numbers and fractional values must not reach file I/O. */
	n = hawk_rtx_valtonum(rtx, hawk_rtx_getarg(rtx, 3), &mode, &fmode);
	if (n <= -1) goto rtx_error;

	if (n > 0)
	{
		if (!(fmode >= IO_STR_TO_STR && fmode <= IO_FILE_TO_FILE)) goto invalid_mode;
		mode = (hawk_int_t)fmode;
		if ((hawk_flt_t)mode != fmode) goto invalid_mode;
	}

	if (mode < IO_STR_TO_STR || mode > IO_FILE_TO_FILE) goto invalid_mode;

	output_is_file = (mode == IO_STR_TO_FILE || mode == IO_FILE_TO_FILE);
	if (!output_is_file && HAWK_RTX_GETVALTYPE(rtx, hawk_rtx_getarg(rtx, 2)) != HAWK_VAL_REF)
	{
		hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "mustache string output requires a reference");
		goto rtx_error;
	}

	args[0] = hawk_rtx_getarg(rtx, 0);
	args[1] = deref_val(rtx, hawk_rtx_getarg(rtx, 2));
	args[2] = hawk_rtx_getnargs(rtx) >= 5? hawk_rtx_getarg(rtx, 4): HAWK_NULL;
	for (i = 0; i < 3; i++)
	{
		if (i == 1 && !output_is_file) continue;
		if (i == 2 && !args[i]) continue;

		strings[i].ptr = hawk_rtx_getvaloocstr(rtx, args[i], &strings[i].len);
		if (HAWK_UNLIKELY(!strings[i].ptr)) goto rtx_error;

		if ((i == 0 && (mode == IO_FILE_TO_STR || mode == IO_FILE_TO_FILE)) || i > 0)
		{
			if (hawk_find_oochar_in_oochars(strings[i].ptr, strings[i].len, '\0'))
			{
				hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "NUL in mustache file path or partial directory list");
				goto rtx_error;
			}
		}
	}

	mus = hawk_mus_openstdwithmmgr(hawk_rtx_getmmgr(rtx), 0, hawk_rtx_getcmgr(rtx), &error);
	if (HAWK_UNLIKELY(!mus))
	{
		hawk_copy_oocstr(state->errmsg, HAWK_COUNTOF(state->errmsg), error.msg);
		goto done;
	}

	if (strings[2].ptr && hawk_mus_setopt(mus, HAWK_MUS_OPT_PARTIALDIRS, strings[2].ptr) <= -1) goto mus_error;

	input.type = (mode == IO_FILE_TO_STR || mode == IO_FILE_TO_FILE? HAWK_MUS_IOSTD_FILE: HAWK_MUS_IOSTD_OOCS);

	if (input.type == HAWK_MUS_IOSTD_FILE) input.u.file.path = strings[0].ptr;
	else input.u.oocs = strings[0];

	output.type = output_is_file? HAWK_MUS_IOSTD_FILE: HAWK_MUS_IOSTD_OOCS;
	if (output_is_file) output.u.file.path = strings[1].ptr;

	if (hawk_mus_compilestd(mus, &input) <= -1) goto mus_error;
	if (hawk_mus_renderstd(mus, hawk_rtx_getarg(rtx, 1), &data_callbacks, &dc, &output) <= -1) goto mus_error;

	if (!output_is_file)
	{
		hawk_val_t* tmp;

		tmp = hawk_rtx_makestrvalwithoocs(rtx, &output.u.oocs);
		hawk_mus_freemem(mus, output.u.oocs.ptr);
		if (HAWK_UNLIKELY(!tmp)) goto rtx_error;

		hawk_rtx_refupval(rtx, tmp);
		n = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), tmp);
		hawk_rtx_refdownval(rtx, tmp);
		if (n <= -1) goto rtx_error;
	}
	result = 0;
	goto done;

invalid_mode:
	hawk_rtx_seterrbfmt(rtx, HAWK_NULL, HAWK_EINVAL, "invalid mustache I/O mode");
rtx_error:
	hawk_copy_oocstr(state->errmsg, HAWK_COUNTOF(state->errmsg), hawk_rtx_geterrmsg(rtx));
	goto done;
mus_error:
	hawk_copy_oocstr(state->errmsg, HAWK_COUNTOF(state->errmsg), hawk_mus_geterrmsg(mus));
done:
	while ((array = dc.arrs))
	{
		dc.arrs = array->next;
		hawk_mus_freemem(mus, array);
	}

	if (dc.text) hawk_rtx_freemem(rtx, dc.text);
	for (i = 0; i < 3; i++)
	{
		if (strings[i].ptr)
			hawk_rtx_freevaloocstr(rtx, args[i], strings[i].ptr);
	}

	if (mus) hawk_mus_close(mus);

	retv = hawk_rtx_makeintval(rtx, result);
	if (HAWK_UNLIKELY(!retv)) return -1;

	hawk_rtx_setretval(rtx, retv);
	return 0;
}

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	rtx_data_t* state;
	hawk_val_t* retv;

	state = get_rtx_data(rtx, fi);
	if (HAWK_UNLIKELY(!state)) return -1;

	retv = hawk_rtx_makestrvalwithoocstr(rtx, state->errmsg);
	if (HAWK_UNLIKELY(!retv)) return -1;

	hawk_rtx_setretval(rtx, retv);
	return 0;
}

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep these tables sorted for binary search in query(). */
	{ HAWK_T("errmsg"),  { { 0, 0, HAWK_NULL },      fnc_errmsg,  0 } },
	{ HAWK_T("process"), { { 4, 5, HAWK_T("vvRvv") }, fnc_process, 0 } }
};

static hawk_mod_int_tab_t inttab[] =
{
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
	rtx_data_t state;

	HAWK_MEMSET(&state, 0, HAWK_SIZEOF(state));
	if (!hawk_rbt_insert(mod->ctx, &rtx, HAWK_SIZEOF(rtx), &state, HAWK_SIZEOF(state)))
	{
		hawk_errinf_t error;
		hawk_geterrinf(rtx->hawk, &error);
		hawk_rtx_seterrinf(rtx, &error);
		return -1;
	}
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	hawk_rbt_delete(mod->ctx, &rtx, HAWK_SIZEOF(rtx));
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	HAWK_ASSERT(HAWK_RBT_SIZE((hawk_rbt_t*)mod->ctx) == 0);
	hawk_rbt_close(mod->ctx);
}

int hawk_mod_mus (hawk_mod_t* mod, hawk_t* hawk)
{
	hawk_rbt_t* rbt;

	rbt = hawk_rbt_open(hawk_getgem(hawk), 0, 1, 1);
	if (!rbt) return -1;
	hawk_rbt_setstyle(rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));
	mod->query = query;
	mod->unload = unload;
	mod->init = init;
	mod->fini = fini;
	mod->ctx = rbt;
	return 0;
}
