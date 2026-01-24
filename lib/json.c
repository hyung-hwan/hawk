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
#include <hawk-chr.h>
#include "hawk-prv.h"
#include "json-prv.h"

#define HAWK_JSON_TOKEN_NAME_ALIGN (64)

/* ========================================================================= */

static void clear_token (hawk_json_t* json)
{
	json->tok.len = 0;
	if (json->tok_capa > 0) json->tok.ptr[json->tok.len] = HAWK_T('\0');
}

static int add_char_to_token (hawk_json_t* json, hawk_ooch_t ch)
{
	if (json->tok.len >= json->tok_capa)
	{
		hawk_ooch_t* tmp;
		hawk_oow_t newcapa;

		newcapa = HAWK_ALIGN_POW2(json->tok.len + 2, HAWK_JSON_TOKEN_NAME_ALIGN); /* +2 here because of -1 when setting newcapa */
		tmp = (hawk_ooch_t*)hawk_json_reallocmem(json, json->tok.ptr, newcapa * HAWK_SIZEOF(*tmp));
		if (!tmp) return -1;

		json->tok_capa = newcapa - 1; /* -1 to secure space for terminating null */
		json->tok.ptr = tmp;
	}

	json->tok.ptr[json->tok.len++] = ch;
	json->tok.ptr[json->tok.len] = HAWK_T('\0');
	return 0;
}

static int add_chars_to_token (hawk_json_t* json, const hawk_ooch_t* ptr, hawk_oow_t len)
{
	hawk_oow_t i;

	if (json->tok_capa - json->tok.len > len)
	{
		hawk_ooch_t* tmp;
		hawk_oow_t newcapa;

		newcapa = HAWK_ALIGN_POW2(json->tok.len + len + 1, HAWK_JSON_TOKEN_NAME_ALIGN);
		tmp = (hawk_ooch_t*)hawk_json_reallocmem(json, json->tok.ptr, newcapa * HAWK_SIZEOF(*tmp));
		if (!tmp) return -1;

		json->tok_capa = newcapa - 1;
		json->tok.ptr = tmp;
	}

	for (i = 0; i < len; i++)
		json->tok.ptr[json->tok.len++] = ptr[i];
	json->tok.ptr[json->tok.len] = HAWK_T('\0');
	return 0;
}

static HAWK_INLINE hawk_ooch_t unescape (hawk_ooch_t c)
{
	switch (c)
	{
		case HAWK_T('a'): return HAWK_T('\a');
		case HAWK_T('b'): return HAWK_T('\b');
		case HAWK_T('f'): return HAWK_T('\f');
		case HAWK_T('n'): return HAWK_T('\n');
		case HAWK_T('r'): return HAWK_T('\r');
		case HAWK_T('t'): return HAWK_T('\t');
		case HAWK_T('v'): return HAWK_T('\v');
		default: return c;
	}
}

/* ========================================================================= */

static int push_state (hawk_json_t* json, hawk_json_state_t state)
{
	hawk_json_state_node_t* ss;

	ss = (hawk_json_state_node_t*)hawk_json_callocmem(json, HAWK_SIZEOF(*ss));
	if (!ss) return -1;

	ss->state = state;
	ss->next = json->state_stack;

	json->state_stack = ss;
	return 0;
}

static void pop_state (hawk_json_t* json)
{
	hawk_json_state_node_t* ss;

	ss = json->state_stack;
	HAWK_ASSERT(ss != HAWK_NULL && ss != &json->state_top);
	json->state_stack = ss->next;

	if (json->state_stack->state == HAWK_JSON_STATE_IN_ARRAY)
	{
		json->state_stack->u.ia.got_value = 1;
	}
	else if (json->state_stack->state == HAWK_JSON_STATE_IN_DIC)
	{
		json->state_stack->u.id.state++;
	}

/* TODO: don't free this. move it to the free list? */
	hawk_json_freemem(json, ss);
}

static void pop_all_states (hawk_json_t* json)
{
	while (json->state_stack != &json->state_top) pop_state(json);
}

/* ========================================================================= */

static int invoke_data_inst (hawk_json_t* json, hawk_json_inst_t inst)
{
	if (json->state_stack->state == HAWK_JSON_STATE_IN_DIC && json->state_stack->u.id.state == 1)
	{
		if (inst != HAWK_JSON_INST_STRING)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "dictionary key not a string - %.*js", json->tok.len, json->tok.ptr);
			return -1;
		}

		inst = HAWK_JSON_INST_KEY;
	}

	if (json->prim.instcb(json, inst, &json->tok) <= -1) return -1;
	return 0;
}

static int handle_string_value_char (hawk_json_t* json, hawk_ooci_t c)
{
	int ret = 1;

	if (json->state_stack->u.sv.escaped == 3)
	{
		if (c >= '0' && c <= '7')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 8 + c - '0';
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else
		{
			ret = 0;
			goto add_sv_acc;
		}
	}
	else if (json->state_stack->u.sv.escaped >= 2)
	{
		if (c >= '0' && c <= '9')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 16 + c - '0';
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else if (c >= 'a' && c <= 'f')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 16 + c - 'a' + 10;
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else if (c >= 'A' && c <= 'F')
		{
			json->state_stack->u.sv.acc = json->state_stack->u.sv.acc * 16 + c - 'A' + 10;
			json->state_stack->u.sv.digit_count++;
			if (json->state_stack->u.sv.digit_count >= json->state_stack->u.sv.escaped) goto add_sv_acc;
		}
		else
		{
			ret = 0;
		add_sv_acc:
		#if defined(HAWK_OOCH_IS_UCH)
			if (add_char_to_token(json, json->state_stack->u.sv.acc) <= -1) return -1;
		#else
			/* convert the character to utf8 */
			{
				hawk_bch_t bcsbuf[HAWK_MBLEN_MAX];
				hawk_oow_t n;

				n = json->gem_.cmgr->uctobc(json->state_stack->u.sv.acc, bcsbuf, HAWK_COUNTOF(bcsbuf));
				if (n == 0 || n > HAWK_COUNTOF(bcsbuf))
				{
					/* illegal character or buffer to small */
					hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EECERR, "unable to convert %jc", json->state_stack->u.sv.acc);
					return -1;
				}

				if (add_chars_to_token(json, bcsbuf, n) <= -1) return -1;
			}
		#endif
			json->state_stack->u.sv.escaped = 0;
		}
	}
	else if (json->state_stack->u.sv.escaped == 1)
	{
		if (c >= '0' && c <= '8')
		{
			json->state_stack->u.sv.escaped = 3;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = c - '0';
		}
		else if (c == 'x')
		{
			json->state_stack->u.sv.escaped = 2;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = 0;
		}
		else if (c == 'u')
		{
			json->state_stack->u.sv.escaped = 4;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = 0;
		}
		else if (c == 'U')
		{
			json->state_stack->u.sv.escaped = 8;
			json->state_stack->u.sv.digit_count = 0;
			json->state_stack->u.sv.acc = 0;
		}
		else
		{
			json->state_stack->u.sv.escaped = 0;
			if (add_char_to_token(json, unescape(c)) <= -1) return -1;
		}
	}
	else if (c == '\\')
	{
		json->state_stack->u.sv.escaped = 1;
	}
	else if (c == '\"')
	{
		pop_state(json);
		if (invoke_data_inst(json, HAWK_JSON_INST_STRING) <= -1) return -1;
	}
	else
	{
		if (add_char_to_token(json, c) <= -1) return -1;
	}

	return ret;
}

static int handle_character_value_char (hawk_json_t* json, hawk_ooci_t c)
{
	/* The real JSON dones't support character literal. this is HCL's own extension. */
	int ret = 1;

	if (json->state_stack->u.cv.escaped == 3)
	{
		if (c >= '0' && c <= '7')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 8 + c - '0';
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else
		{
			ret = 0;
			goto add_cv_acc;
		}
	}
	if (json->state_stack->u.cv.escaped >= 2)
	{
		if (c >= '0' && c <= '9')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 16 + c - '0';
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else if (c >= 'a' && c <= 'f')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 16 + c - 'a' + 10;
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else if (c >= 'A' && c <= 'F')
		{
			json->state_stack->u.cv.acc = json->state_stack->u.cv.acc * 16 + c - 'A' + 10;
			json->state_stack->u.cv.digit_count++;
			if (json->state_stack->u.cv.digit_count >= json->state_stack->u.cv.escaped) goto add_cv_acc;
		}
		else
		{
			ret = 0;
		add_cv_acc:
			if (add_char_to_token(json, json->state_stack->u.cv.acc) <= -1) return -1;
			json->state_stack->u.cv.escaped = 0;
		}
	}
	else if (json->state_stack->u.cv.escaped == 1)
	{
		if (c >= '0' && c <= '8')
		{
			json->state_stack->u.cv.escaped = 3;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = c - '0';
		}
		else if (c == 'x')
		{
			json->state_stack->u.cv.escaped = 2;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = 0;
		}
		else if (c == 'u')
		{
			json->state_stack->u.cv.escaped = 4;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = 0;
		}
		else if (c == 'U')
		{
			json->state_stack->u.cv.escaped = 8;
			json->state_stack->u.cv.digit_count = 0;
			json->state_stack->u.cv.acc = 0;
		}
		else
		{
			json->state_stack->u.cv.escaped = 0;
			if (add_char_to_token(json, unescape(c)) <= -1) return -1;
		}
	}
	else if (c == '\\')
	{
		json->state_stack->u.cv.escaped = 1;
	}
	else if (c == '\'')
	{
		pop_state(json);

		if (json->tok.len < 1)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "no character in a character literal");
			return -1;
		}
		if (invoke_data_inst(json, HAWK_JSON_INST_CHARACTER) <= -1) return -1;
	}
	else
	{
		if (add_char_to_token(json, c) <= -1) return -1;
	}

	if (json->tok.len > 1)
	{
		hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "too many characters in a character literal - %.*js", (int)json->tok.len, json->tok.ptr);
		return -1;
	}

	return ret;
}

static int handle_numeric_value_char (hawk_json_t* json, hawk_ooci_t c)
{
	if (hawk_is_ooch_digit(c) || (json->tok.len == 0 && (c == '+' || c == '-')))
	{
		if (add_char_to_token(json, c) <= -1) return -1;
		return 1;
	}
	else if (!json->state_stack->u.nv.dotted && c == '.' &&
	         json->tok.len > 0 && hawk_is_ooch_digit(json->tok.ptr[json->tok.len - 1]))
	{
		if (add_char_to_token(json, c) <= -1) return -1;
		json->state_stack->u.nv.dotted = 1;
		return 1;
	}

	pop_state(json);

	HAWK_ASSERT(json->tok.len > 0);
	if (!hawk_is_ooch_digit(json->tok.ptr[json->tok.len - 1]))
	{
		hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "invalid numeric value - %.*js", (int)json->tok.len, json->tok.ptr);
		return -1;
	}
	if (invoke_data_inst(json, HAWK_JSON_INST_NUMBER) <= -1) return -1;
	return 0; /* start over */
}

static int handle_word_value_char (hawk_json_t* json, hawk_ooci_t c)
{
	hawk_json_inst_t inst;

	if (hawk_is_ooch_alpha(c))
	{
		if (add_char_to_token(json, c) <= -1) return -1;
		return 1;
	}

	pop_state(json);

	if (hawk_comp_oochars_bcstr(json->tok.ptr, json->tok.len, "null", 0) == 0) inst = HAWK_JSON_INST_NIL;
	else if (hawk_comp_oochars_bcstr(json->tok.ptr, json->tok.len, "true", 0) == 0) inst = HAWK_JSON_INST_TRUE;
	else if (hawk_comp_oochars_bcstr(json->tok.ptr, json->tok.len, "false", 0) == 0) inst = HAWK_JSON_INST_FALSE;
	else
	{
		hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "invalid word value - %.*js", json->tok.len, json->tok.ptr);
		return -1;
	}

	if (invoke_data_inst(json, inst) <= -1) return -1;
	return 0; /* start over */
}

/* ========================================================================= */

static int handle_start_char (hawk_json_t* json, hawk_ooci_t c)
{
	if (c == '[')
	{
		if (push_state(json, HAWK_JSON_STATE_IN_ARRAY) <= -1) return -1;
		json->state_stack->u.ia.got_value = 0;
		if (json->prim.instcb(json, HAWK_JSON_INST_START_ARRAY, HAWK_NULL) <= -1) return -1;
		return 1;
	}
	else if (c == '{')
	{
		if (push_state(json, HAWK_JSON_STATE_IN_DIC) <= -1) return -1;
		json->state_stack->u.id.state = 0;
		if (json->prim.instcb(json, HAWK_JSON_INST_START_DIC, HAWK_NULL) <= -1) return -1;
		return 1;
	}
	else if (hawk_is_ooch_space(c))
	{
		/* do nothing */
		return 1;
	}
	else
	{
		hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "not starting with [ or { - %jc", (hawk_ooch_t)c);
		return -1;
	}
}

static int handle_char_in_array (hawk_json_t* json, hawk_ooci_t c)
{
	if (c == ']')
	{
		if (json->prim.instcb(json, HAWK_JSON_INST_END_ARRAY, HAWK_NULL) <= -1) return -1;
		pop_state(json);
		return 1;
	}
	else if (c == ',')
	{
		if (!json->state_stack->u.ia.got_value)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "redundant comma in array - %jc", (hawk_ooch_t)c);
			return -1;
		}
		json->state_stack->u.ia.got_value = 0;
		return 1;
	}
	else if (hawk_is_ooch_space(c))
	{
		/* do nothing */
		return 1;
	}
	else
	{
		if (json->state_stack->u.ia.got_value)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "comma required in array - %jc", (hawk_ooch_t)c);
			return -1;
		}

		if (c == '\"')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_STRING_VALUE) <= -1) return -1;
			clear_token(json);
			return 1;
		}
		else if (c == '\'')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_CHARACTER_VALUE) <= -1) return -1;
			clear_token(json);
			return 1;
		}
		/* TOOD: else if (c == '#') HCL radixed number
		 */
		else if (hawk_is_ooch_digit(c) || c == '+' || c == '-')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_NUMERIC_VALUE) <= -1) return -1;
			clear_token(json);
			json->state_stack->u.nv.dotted = 0;
			return 0; /* start over */
		}
		else if (hawk_is_ooch_alpha(c))
		{
			if (push_state(json, HAWK_JSON_STATE_IN_WORD_VALUE) <= -1) return -1;
			clear_token(json);
			return 0; /* start over */
		}
		else if (c == '[')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_ARRAY) <= -1) return -1;
			json->state_stack->u.ia.got_value = 0;
			if (json->prim.instcb(json, HAWK_JSON_INST_START_ARRAY, HAWK_NULL) <= -1) return -1;
			return 1;
		}
		else if (c == '{')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_DIC) <= -1) return -1;
			json->state_stack->u.id.state = 0;
			if (json->prim.instcb(json, HAWK_JSON_INST_START_DIC, HAWK_NULL) <= -1) return -1;
			return 1;
		}
		else
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "wrong character inside array - %jc[%d]", (hawk_ooch_t)c, (int)c);
			return -1;
		}
	}
}

static int handle_char_in_dic (hawk_json_t* json, hawk_ooci_t c)
{
	if (c == '}')
	{
		if (json->prim.instcb(json, HAWK_JSON_INST_END_DIC, HAWK_NULL) <= -1) return -1;
		pop_state(json);
		return 1;
	}
	else if (c == ':')
	{
		if (json->state_stack->u.id.state != 1)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "redundant colon in dictionary - %jc", (hawk_ooch_t)c);
			return -1;
		}
		json->state_stack->u.id.state++;
		return 1;
	}
	else if (c == ',')
	{
		if (json->state_stack->u.id.state != 3)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "redundant comma in dicitonary - %jc", (hawk_ooch_t)c);
			return -1;
		}
		json->state_stack->u.id.state = 0;
		return 1;
	}
	else if (hawk_is_ooch_space(c))
	{
		/* do nothing */
		return 1;
	}
	else
	{
		if (json->state_stack->u.id.state == 1)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "colon required in dicitonary - %jc", (hawk_ooch_t)c);
			return -1;
		}
		else if (json->state_stack->u.id.state == 3)
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "comma required in dicitonary - %jc", (hawk_ooch_t)c);
			return -1;
		}

		if (c == '\"')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_STRING_VALUE) <= -1) return -1;
			clear_token(json);
			return 1;
		}
		else if (c == '\'')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_CHARACTER_VALUE) <= -1) return -1;
			clear_token(json);
			return 1;
		}
		/* TOOD: else if (c == '#') HCL radixed number
		 */
		else if (hawk_is_ooch_digit(c) || c == '+' || c == '-')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_NUMERIC_VALUE) <= -1) return -1;
			clear_token(json);
			json->state_stack->u.nv.dotted = 0;
			return 0; /* start over */
		}
		else if (hawk_is_ooch_alpha(c))
		{
			if (push_state(json, HAWK_JSON_STATE_IN_WORD_VALUE) <= -1) return -1;
			clear_token(json);
			return 0; /* start over */
		}
		else if (c == '[')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_ARRAY) <= -1) return -1;
			json->state_stack->u.ia.got_value = 0;
			if (json->prim.instcb(json, HAWK_JSON_INST_START_ARRAY, HAWK_NULL) <= -1) return -1;
			return 1;
		}
		else if (c == '{')
		{
			if (push_state(json, HAWK_JSON_STATE_IN_DIC) <= -1) return -1;
			json->state_stack->u.id.state = 0;
			if (json->prim.instcb(json, HAWK_JSON_INST_START_DIC, HAWK_NULL) <= -1) return -1;
			return 1;
		}
		else
		{
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINVAL, "wrong character inside dictionary - %jc[%d]", (hawk_ooch_t)c, (int)c);
			return -1;
		}
	}
}

/* ========================================================================= */

static int handle_char (hawk_json_t* json, hawk_ooci_t c)
{
	int x;

start_over:
	if (c == HAWK_OOCI_EOF)
	{
		if (json->state_stack->state == HAWK_JSON_STATE_START)
		{
			/* no input data */
			return 0;
		}
		else
		{
			hawk_json_seterrnum(json, HAWK_NULL, HAWK_EEOF);
			return -1;
		}
	}

	switch (json->state_stack->state)
	{
		case HAWK_JSON_STATE_START:
			x = handle_start_char(json, c);
			break;

		case HAWK_JSON_STATE_IN_ARRAY:
			x = handle_char_in_array(json, c);
			break;

		case HAWK_JSON_STATE_IN_DIC:
			x = handle_char_in_dic(json, c);
			break;

		case HAWK_JSON_STATE_IN_WORD_VALUE:
			x = handle_word_value_char(json, c);
			break;

		case HAWK_JSON_STATE_IN_STRING_VALUE:
			x = handle_string_value_char(json, c);
			break;

		case HAWK_JSON_STATE_IN_CHARACTER_VALUE:
			x = handle_character_value_char(json, c);
			break;

		case HAWK_JSON_STATE_IN_NUMERIC_VALUE:
			x = handle_numeric_value_char(json, c);
			break;

		default:
			hawk_json_seterrbfmt(json, HAWK_NULL, HAWK_EINTERN, "internal error - must not be called for state %d", (int)json->state_stack->state);
			return -1;
	}

	if (x <= -1) return -1;
	if (x == 0) goto start_over;

	return 0;
}

/* ========================================================================= */

static int feed_json_data_b (hawk_json_t* json, const hawk_bch_t* data, hawk_oow_t len, hawk_oow_t* xlen)
{
	const hawk_bch_t* ptr;
	const hawk_bch_t* end;

	ptr = data;
	end = ptr + len;

	while (ptr < end)
	{
		hawk_ooci_t c;

	#if defined(HAWK_OOCH_IS_UCH)
		hawk_ooch_t uc;
		hawk_oow_t bcslen;
		hawk_oow_t n;

		bcslen = end - ptr;
		n = json->gem_.cmgr->bctouc(ptr, bcslen, &uc);
		if (n == 0)
		{
			/* invalid sequence */
			uc = *ptr;
			n = 1;
		}
		else if (n > bcslen)
		{
			/* incomplete sequence */
			*xlen = ptr - data; /* didn't manage to process in full */
			return 0; /* feed more for incomplete sequence */
		}

		ptr += n;
		c = uc;
	#else
		c = *ptr++;
	#endif

		/* handle a single character */
		if (handle_char(json, c) <= -1) goto oops;
	}

	*xlen = ptr - data;
	return 1;

oops:
	/* TODO: compute the number of processed bytes so far and return it via a parameter??? */
/*printf ("feed oops....\n");*/
	return -1;
}

/* ========================================================================= */

static int feed_json_data_u (hawk_json_t* json, const hawk_uch_t* data, hawk_oow_t len, hawk_oow_t* xlen)
{
	const hawk_uch_t* ptr;
	const hawk_uch_t* end;

	ptr = data;
	end = ptr + len;

	while (ptr < end)
	{
		hawk_ooci_t c;

	#if defined(HAWK_OOCH_IS_UCH)
		c = *ptr++;
		/* handle a single character */
		if (handle_char(json, c) <= -1) goto oops;
	#else
		hawk_bch_t bcsbuf[HAWK_BCSIZE_MAX];
		hawk_oow_t mlen = 0;
		hawk_oow_t n, i;

		n = json->gem_.cmgr->uctobc(*ptr++, bcsbuf, HAWK_COUNTOF(bcsbuf));
		if (n == 0) goto oops; // illegal character

		for (i = 0; i < n; i++)
		{
			if (handle_char(json, bcsbuf[i]) <= -1) goto oops;
		}
	#endif
	}

	*xlen = ptr - data;
	return 1;

oops:
	/* TODO: compute the number of processed bytes so far and return it via a parameter??? */
/*printf ("feed oops....\n");*/
	return -1;
}

/* ========================================================================= */


hawk_json_t* hawk_json_open (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_json_prim_t* prim, hawk_errinf_t* errinf)
{
	hawk_json_t* json;

	json = (hawk_json_t*)HAWK_MMGR_ALLOC(mmgr, HAWK_SIZEOF(hawk_json_t) + xtnsize);
	if (!json)
	{
		if (errinf)
		{
			HAWK_MEMSET(errinf, 0, HAWK_SIZEOF(*errinf));
			errinf->num = HAWK_ENOMEM;
			hawk_copy_oocstr(errinf->msg, HAWK_COUNTOF(errinf->msg), hawk_dfl_errstr(errinf->num));
		}
		return HAWK_NULL;
	}

	HAWK_MEMSET(json, 0, HAWK_SIZEOF(*json) + xtnsize);
	json->instsize_ = HAWK_SIZEOF(*json);
	json->gem_.mmgr = mmgr;
	json->gem_.cmgr = cmgr;

	/* initialize error handling fields */
	json->gem_.errnum = HAWK_ENOERR;
	json->gem_.errmsg[0] = '\0';
	json->gem_.errloc.line = 0;
	json->gem_.errloc.colm = 0;
	json->gem_.errloc.file = HAWK_NULL;
	json->gem_.errstr = hawk_dfl_errstr;

	json->prim = *prim;
	json->state_top.state = HAWK_JSON_STATE_START;
	json->state_top.next = HAWK_NULL;
	json->state_stack = &json->state_top;

	return json;
}

void hawk_json_close (hawk_json_t* json)
{
	pop_all_states(json);
	if (json->tok.ptr) hawk_json_freemem(json, json->tok.ptr);
	HAWK_MMGR_FREE(hawk_json_getmmgr(json), json);
}

int hawk_json_setoption (hawk_json_t* json, hawk_json_option_t id, const void* value)
{
	switch (id)
	{
		case HAWK_JSON_TRAIT:
			json->cfg.trait = *(const int*)value;
			return 0;
	}

	hawk_json_seterrnum(json, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

int hawk_json_getoption (hawk_json_t* json, hawk_json_option_t id, void* value)
{
	switch (id)
	{
		case HAWK_JSON_TRAIT:
			*(int*)value = json->cfg.trait;
			return 0;
	};

	hawk_json_seterrnum(json, HAWK_NULL, HAWK_EINVAL);
	return -1;
}

/* ========================================================================= */

hawk_errstr_t hawk_json_geterrstr (hawk_json_t* json)
{
	return json->gem_.errstr;
}

void hawk_json_seterrbfmt (hawk_json_t* json, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hawk_gem_seterrbvfmt(hawk_json_getgem(json), errloc, errnum, fmt, ap);
	va_end(ap);
}

void hawk_json_seterrufmt (hawk_json_t* json, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	hawk_gem_seterruvfmt(hawk_json_getgem(json), errloc, errnum, fmt, ap);
	va_end(ap);
}


void hawk_json_seterrbvfmt (hawk_json_t* json, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_bch_t* errfmt, va_list ap)
{
	hawk_gem_seterrbvfmt (hawk_json_getgem(json), errloc, errnum, errfmt, ap);
}

void hawk_json_seterruvfmt (hawk_json_t* json, const hawk_loc_t* errloc, hawk_errnum_t errnum, const hawk_uch_t* errfmt, va_list ap)
{
	hawk_gem_seterruvfmt (hawk_json_getgem(json), errloc, errnum, errfmt, ap);
}

/* ========================================================================= */
void* hawk_json_allocmem (hawk_json_t* json, hawk_oow_t size)
{
	void* ptr;

	ptr = HAWK_MMGR_ALLOC(hawk_json_getmmgr(json), size);
	if (!ptr) hawk_json_seterrnum(json, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void* hawk_json_callocmem (hawk_json_t* json, hawk_oow_t size)
{
	void* ptr;

	ptr = HAWK_MMGR_ALLOC(hawk_json_getmmgr(json), size);
	if (!ptr) hawk_json_seterrnum(json, HAWK_NULL, HAWK_ENOMEM);
	else HAWK_MEMSET(ptr, 0, size);
	return ptr;
}

void* hawk_json_reallocmem (hawk_json_t* json, void* ptr, hawk_oow_t size)
{
	ptr = HAWK_MMGR_REALLOC(hawk_json_getmmgr(json), ptr, size);
	if (!ptr) hawk_json_seterrnum(json, HAWK_NULL, HAWK_ENOMEM);
	return ptr;
}

void hawk_json_freemem (hawk_json_t* json, void* ptr)
{
	HAWK_MMGR_FREE(hawk_json_getmmgr(json), ptr);
}

/* ========================================================================= */

hawk_json_state_t hawk_json_getstate (hawk_json_t* json)
{
	return json->state_stack->state;
}

void hawk_json_reset (hawk_json_t* json)
{
	/* TODO: reset XXXXXXXXXXXXXXXXXXXXXXXXXXXxxxxx */
	pop_all_states(json);
	HAWK_ASSERT(json->state_stack == &json->state_top);
	json->state_stack->state = HAWK_JSON_STATE_START;
}

int hawk_json_feedbchars (hawk_json_t* json, const hawk_bch_t* ptr, hawk_oow_t len, hawk_oow_t* xlen)
{
	int x;
	hawk_oow_t total, ylen;

	total = 0;
	while (total < len)
	{
		x = feed_json_data_b(json, &ptr[total], len - total, &ylen);
		if (x <= -1) return -1;

		total += ylen;
		if (x == 0) break; /* incomplete sequence encountered */
	}

	*xlen = total;
	return 0;
}

int hawk_json_feeduchars (hawk_json_t* json, const hawk_uch_t* ptr, hawk_oow_t len, hawk_oow_t* xlen)
{
	int x;
	hawk_oow_t total, ylen;

	total = 0;
	while (total < len)
	{
		x = feed_json_data_u(json, &ptr[total], len - total, &ylen);
		if (x <= -1) return -1;

		total += ylen;
		if (x == 0) break; /* incomplete sequence encountered */
	}

	*xlen = total;
	return 0;
}
