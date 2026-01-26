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

#include "mod-json.h"

#include <hawk-json.h>
#include <hawk-ecs.h>

#include "../lib/hawk-prv.h"

#define JSON_MAX_DEPTH 64

typedef struct json_stack_t json_stack_t;
struct json_stack_t
{
	hawk_val_t* container;
	hawk_ooch_t* key_ptr;
	hawk_oow_t key_len;
	hawk_ooi_t next_index;
	int is_map;
	json_stack_t* prev;
};

typedef struct json_parse_ctx_t json_parse_ctx_t;
struct json_parse_ctx_t
{
	hawk_rtx_t* rtx;
	hawk_val_t* root;
	json_stack_t* top;
};

static void free_stack_node (hawk_rtx_t* rtx, hawk_json_t* json, json_stack_t* node)
{
	if (node->key_ptr) hawk_rtx_freemem(rtx, node->key_ptr);
	hawk_json_freemem(json, node);
}

static int push_container (hawk_json_t* json, json_parse_ctx_t* ctx, hawk_val_t* container, int is_map)
{
	json_stack_t* node;

	node = (json_stack_t*)hawk_json_callocmem(json, HAWK_SIZEOF(*node));
	if (!node) return -1;

	node->container = container;
	node->is_map = is_map;
	node->next_index = 1;
	node->prev = ctx->top;
	ctx->top = node;
	return 0;
}

static int pop_container (hawk_json_t* json, json_parse_ctx_t* ctx)
{
	json_stack_t* node;

	node = ctx->top;
	if (!node) return -1;
	ctx->top = node->prev;
	free_stack_node(ctx->rtx, json, node);
	return 0;
}

static int attach_value (hawk_json_t* json, json_parse_ctx_t* ctx, hawk_val_t* val)
{
	hawk_rtx_t* rtx = ctx->rtx;

	if (!ctx->top)
	{
		if (ctx->root)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "multiple root values");
			return -1;
		}
		ctx->root = val;
		return 0;
	}

	if (ctx->top->is_map)
	{
		if (!ctx->top->key_ptr)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "missing object key");
			return -1;
		}

		if (!hawk_rtx_setmapvalfld(rtx, ctx->top->container, ctx->top->key_ptr, ctx->top->key_len, val))
		{
			hawk_json_seterrnum(json, HAWK_NULL, hawk_rtx_geterrnum(rtx));
			return -1;
		}

		hawk_rtx_freemem(rtx, ctx->top->key_ptr);
		ctx->top->key_ptr = HAWK_NULL;
		ctx->top->key_len = 0;
		return 0;
	}
	else
	{
		if (!hawk_rtx_setarrvalfld(rtx, ctx->top->container, ctx->top->next_index, val))
		{
			hawk_json_seterrnum(json, HAWK_NULL, hawk_rtx_geterrnum(rtx));
			return -1;
		}

		ctx->top->next_index++;
		return 0;
	}
}

static int build_value (hawk_json_t* json, hawk_json_inst_t inst, const hawk_oocs_t* str)
{
	json_parse_ctx_t* ctx = (json_parse_ctx_t*)hawk_json_getxtn(json);
	hawk_rtx_t* rtx = ctx->rtx;
	hawk_val_t* val;

	switch (inst)
	{
		case HAWK_JSON_INST_STRING:
		case HAWK_JSON_INST_CHARACTER:
			val = hawk_rtx_makestrvalwithoochars(rtx, str->ptr, str->len);
			if (!val) goto oops;
			break;

		case HAWK_JSON_INST_NUMBER:
			val = hawk_rtx_makenumorstrvalwithoochars(rtx, str->ptr, str->len, 1);
			if (!val) goto oops;
			break;

		case HAWK_JSON_INST_NIL:
			val = hawk_val_nil;
			break;

		case HAWK_JSON_INST_TRUE:
			val = hawk_val_true;
			break;

		case HAWK_JSON_INST_FALSE:
			val = hawk_val_false;
			break;

		default:
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "unexpected instruction %d", (int)inst);
			return -1;
	}

	return attach_value(json, ctx, val);

oops:
	hawk_json_seterrnum(json, HAWK_NULL, hawk_rtx_geterrnum(rtx));
	return -1;
}

static int instcb (hawk_json_t* json, hawk_json_inst_t inst, const hawk_oocs_t* str)
{
	json_parse_ctx_t* ctx = (json_parse_ctx_t*)hawk_json_getxtn(json);
	hawk_rtx_t* rtx = ctx->rtx;
	hawk_val_t* val;

	switch (inst)
	{
		case HAWK_JSON_INST_START_ARRAY:
			val = hawk_rtx_makearrval(rtx, 0);
			if (!val) goto oops;
			if (attach_value(json, ctx, val) <= -1) return -1;
			if (push_container(json, ctx, val, 0) <= -1) goto oops;
			return 0;

		case HAWK_JSON_INST_END_ARRAY:
			if (!ctx->top || ctx->top->is_map) goto invalid_state;
			return pop_container(json, ctx);

		case HAWK_JSON_INST_START_DIC:
			val = hawk_rtx_makemapval(rtx);
			if (!val) goto oops;
			if (attach_value(json, ctx, val) <= -1) return -1;
			if (push_container(json, ctx, val, 1) <= -1) goto oops;
			return 0;

		case HAWK_JSON_INST_END_DIC:
			if (!ctx->top || !ctx->top->is_map) goto invalid_state;
			if (ctx->top->key_ptr)
			{
				hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "dangling object key");
				return -1;
			}
			return pop_container(json, ctx);

		case HAWK_JSON_INST_KEY:
			if (!ctx->top || !ctx->top->is_map) goto invalid_state;
			if (ctx->top->key_ptr)
			{
				hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "duplicate object key");
				return -1;
			}
			ctx->top->key_ptr = hawk_rtx_dupoochars(rtx, str->ptr, str->len);
			if (!ctx->top->key_ptr) goto oops;
			ctx->top->key_len = str->len;
			return 0;

		case HAWK_JSON_INST_STRING:
		case HAWK_JSON_INST_CHARACTER:
		case HAWK_JSON_INST_NUMBER:
		case HAWK_JSON_INST_NIL:
		case HAWK_JSON_INST_TRUE:
		case HAWK_JSON_INST_FALSE:
			return build_value(json, inst, str);

		default:
			break;
	}

invalid_state:
	hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "invalid parser state");
	return -1;

oops:
	hawk_json_seterrnum(json, HAWK_NULL, hawk_rtx_geterrnum(rtx));
	return -1;
}

static int fnc_parse (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_json_t* json = HAWK_NULL;
	hawk_val_t* arg;
	hawk_ooch_t* ptr;
	hawk_oow_t len;
	hawk_oow_t xlen;
	hawk_val_t* retv;
	hawk_json_prim_t prim;
	json_parse_ctx_t* ctx;
	int x;

	arg = hawk_rtx_getarg(rtx, 0);
	ptr = hawk_rtx_getvaloocstr(rtx, arg, &len);
	if (!ptr) return -1;

	prim.instcb = instcb;
	json = hawk_json_openstdwithmmgr(hawk_rtx_getmmgr(rtx), HAWK_SIZEOF(*ctx), hawk_rtx_getcmgr(rtx), &prim, HAWK_NULL);
	if (!json)
	{
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_ENOMEM);
		hawk_rtx_freevaloocstr(rtx, arg, ptr);
		return -1;
	}

	ctx = (json_parse_ctx_t*)hawk_json_getxtn(json);
	ctx->rtx = rtx;
	ctx->root = HAWK_NULL;
	ctx->top = HAWK_NULL;

	x = hawk_json_feed(json, ptr, len, &xlen);
	hawk_rtx_freevaloocstr(rtx, arg, ptr);

	if (x <= -1 || xlen != len || hawk_json_getstate(json) != HAWK_JSON_STATE_START || ctx->top)
	{
		hawk_errnum_t errnum;
		const hawk_ooch_t* errmsg;

		if (hawk_json_geterrnum(json) == HAWK_ENOERR)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "incomplete json input");
		}

		hawk_json_geterror(json, &errnum, &errmsg, HAWK_NULL);
		hawk_rtx_seterrfmt(rtx, HAWK_NULL, errnum, HAWK_T("json parse error: %js"), (errmsg? errmsg: HAWK_T("")));
		goto oops;
	}

	retv = ctx->root? ctx->root: hawk_val_nil;
	hawk_rtx_setretval(rtx, retv);
	hawk_json_close(json);
	return 0;

oops:
	if (ctx->root)
	{
		hawk_rtx_refupval(rtx, ctx->root);
		hawk_rtx_refdownval(rtx, ctx->root);
	}

	while (ctx->top)
	{
		json_stack_t* node = ctx->top;
		ctx->top = node->prev;
		free_stack_node(rtx, json, node);
	}

	hawk_json_close(json);
	return -1;
}

static int append_hex2 (hawk_ooecs_t* buf, hawk_uint8_t val)
{
	static const hawk_ooch_t hex[] = HAWK_T("0123456789abcdef");
	hawk_ooch_t tmp[2];

	tmp[0] = hex[(val >> 4) & 0x0F];
	tmp[1] = hex[val & 0x0F];
	return (hawk_ooecs_ncat(buf, tmp, 2) == (hawk_oow_t)-1)? -1: 0;
}

static int append_json_string (hawk_ooecs_t* buf, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t i;

	if (hawk_ooecs_ccat(buf, HAWK_T('"')) == (hawk_oow_t)-1) return -1;

	for (i = 0; i < len; i++)
	{
		hawk_ooch_t c = ptr[i];

		switch (c)
		{
			case HAWK_T('"'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\\"")) == (hawk_oow_t)-1) return -1;
				continue;
			case HAWK_T('\\'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\\\")) == (hawk_oow_t)-1) return -1;
				continue;
			case HAWK_T('\b'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\b")) == (hawk_oow_t)-1) return -1;
				continue;
			case HAWK_T('\f'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\f")) == (hawk_oow_t)-1) return -1;
				continue;
			case HAWK_T('\n'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\n")) == (hawk_oow_t)-1) return -1;
				continue;
			case HAWK_T('\r'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\r")) == (hawk_oow_t)-1) return -1;
				continue;
			case HAWK_T('\t'):
				if (hawk_ooecs_cat(buf, HAWK_T("\\t")) == (hawk_oow_t)-1) return -1;
				continue;
		}

#if defined(HAWK_OOCH_IS_BCH)
		if ((hawk_uint8_t)c < 0x20)
#else
		if (c < 0x20)
#endif
		{
			if (hawk_ooecs_cat(buf, HAWK_T("\\u00")) == (hawk_oow_t)-1) return -1;
			if (append_hex2(buf, (hawk_uint8_t)c) <= -1) return -1;
		}
		else
		{
			if (hawk_ooecs_ccat(buf, c) == (hawk_oow_t)-1) return -1;
		}
	}

	if (hawk_ooecs_ccat(buf, HAWK_T('"')) == (hawk_oow_t)-1) return -1;
	return 0;
}

static int stringify_value (hawk_rtx_t* rtx, hawk_val_t* val, hawk_ooecs_t* out, int depth)
{
	hawk_val_type_t vtype;

	if (depth > JSON_MAX_DEPTH)
	{
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
		return -1;
	}

	vtype = HAWK_RTX_GETVALTYPE(rtx, val);
	switch (vtype)
	{
		case HAWK_VAL_NIL:
			return (hawk_ooecs_cat(out, HAWK_T("null")) == (hawk_oow_t)-1)? -1: 0;

		case HAWK_VAL_INT:
		case HAWK_VAL_FLT:
		{
			hawk_oow_t len;
			hawk_ooch_t* ptr = hawk_rtx_getvaloocstr(rtx, val, &len);
			if (!ptr) return -1;
			if (hawk_ooecs_ncat(out, ptr, len) == (hawk_oow_t)-1)
			{
				hawk_rtx_freevaloocstr(rtx, val, ptr);
				return -1;
			}
			hawk_rtx_freevaloocstr(rtx, val, ptr);
			return 0;
		}

		case HAWK_VAL_STR:
		{
			hawk_val_str_t* sv = (hawk_val_str_t*)val;
			return append_json_string(out, sv->val.ptr, sv->val.len);
		}

		case HAWK_VAL_CHAR:
		case HAWK_VAL_BCHR:
		case HAWK_VAL_MBS:
		{
			hawk_oow_t len;
			hawk_ooch_t* ptr = hawk_rtx_getvaloocstr(rtx, val, &len);
			if (!ptr) return -1;
			if (append_json_string(out, ptr, len) <= -1)
			{
				hawk_rtx_freevaloocstr(rtx, val, ptr);
				return -1;
			}
			hawk_rtx_freevaloocstr(rtx, val, ptr);
			return 0;
		}

		case HAWK_VAL_MAP:
		{
			hawk_val_map_itr_t itr;
			hawk_val_map_itr_t* iptr;
			int first = 1;

			if (hawk_ooecs_ccat(out, HAWK_T('{')) == (hawk_oow_t)-1) return -1;
			iptr = hawk_rtx_getfirstmapvalitr(rtx, val, &itr);
			while (iptr)
			{
				const hawk_oocs_t* key;
				const hawk_val_t* mapval;

				key = HAWK_VAL_MAP_ITR_KEY(iptr);
				mapval = HAWK_VAL_MAP_ITR_VAL(iptr);

				if (!first && hawk_ooecs_ccat(out, HAWK_T(',')) == (hawk_oow_t)-1) return -1;
				first = 0;

				if (append_json_string(out, key->ptr, key->len) <= -1) return -1;
				if (hawk_ooecs_ccat(out, HAWK_T(':')) == (hawk_oow_t)-1) return -1;
				if (stringify_value(rtx, (hawk_val_t*)mapval, out, depth + 1) <= -1) return -1;

				iptr = hawk_rtx_getnextmapvalitr(rtx, val, &itr);
			}
			if (hawk_ooecs_ccat(out, HAWK_T('}')) == (hawk_oow_t)-1) return -1;
			return 0;
		}

		case HAWK_VAL_ARR:
		{
			hawk_val_arr_itr_t itr;
			hawk_val_arr_itr_t* iptr;
			int first = 1;

			if (hawk_ooecs_ccat(out, HAWK_T('[')) == (hawk_oow_t)-1) return -1;
			iptr = hawk_rtx_getfirstarrvalitr(rtx, val, &itr);
			while (iptr)
			{
				if (!first && hawk_ooecs_ccat(out, HAWK_T(',')) == (hawk_oow_t)-1) return -1;
				first = 0;
				if (stringify_value(rtx, iptr->elem, out, depth + 1) <= -1) return -1;
				iptr = hawk_rtx_getnextarrvalitr(rtx, val, &itr);
			}
			if (hawk_ooecs_ccat(out, HAWK_T(']')) == (hawk_oow_t)-1) return -1;
			return 0;
		}

		default:
			hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EINVAL);
			return -1;
	}
}

static int fnc_stringify (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_ooecs_t buf;
	hawk_val_t* arg;
	hawk_val_t* retv;
	int inited = 0;

	arg = hawk_rtx_getarg(rtx, 0);
	if (hawk_ooecs_init(&buf, hawk_rtx_getgem(rtx), 256) <= -1) return -1;
	inited = 1;

	if (stringify_value(rtx, arg, &buf, 0) <= -1) goto oops;

	retv = hawk_rtx_makestrvalwithoocs(rtx, HAWK_OOECS_OOCS(&buf));
	if (!retv) goto oops;

	hawk_ooecs_fini(&buf);
	hawk_rtx_setretval(rtx, retv);
	return 0;

oops:
	if (inited) hawk_ooecs_fini(&buf);
	return -1;
}

static hawk_mod_fnc_tab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */
	{ HAWK_T("parse"),     { { 1, 1, HAWK_NULL }, fnc_parse,     0 } },
	{ HAWK_T("stringify"), { { 1, 1, HAWK_NULL }, fnc_stringify, 0 } }
};

static int query (hawk_mod_t* mod, hawk_t* hawk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	return hawk_findmodsymfnc(hawk, fnctab, HAWK_COUNTOF(fnctab), name, sym);
}

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* nothing to do */
}

static void unload (hawk_mod_t* mod, hawk_t* hawk)
{
	/* nothing to do */
}

int hawk_mod_json (hawk_mod_t* mod, hawk_t* hawk)
{
	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;
	mod->ctx = HAWK_NULL;

	return 0;
}
