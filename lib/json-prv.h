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

#ifndef _HAWK_JSON_PRV_H_
#define _HAWK_JSON_PRV_H_

#include <hawk-json.h>

typedef struct hawk_json_state_node_t hawk_json_state_node_t;
struct hawk_json_state_node_t
{
	hawk_json_state_t state;
	union
	{
		struct
		{
			int got_value;
		} ia; /* in array */

		struct
		{
			/* 0: ready to get key (at the beginning or got comma), 
			 * 1: got key, 2: got colon, 3: got value */
			int state; 
		} id; /* in dictionary */
		struct
		{
			int escaped;
			int digit_count;
			/* acc is always of unicode type to handle \u and \U. 
			 * in the bch mode, it will get converted to a utf8 stream. */
			hawk_uch_t acc;
		} sv;
		struct
		{
			int escaped;
			int digit_count;
			/* for a character, no way to support the unicode character
			 * in the bch mode */
			hawk_ooch_t acc; 
		} cv;
		struct
		{
			int dotted;
		} nv;
	} u;
	hawk_json_state_node_t* next;
};

struct hawk_json_t
{
	HAWK_JSON_HDR;

	hawk_errnum_t errnum;
	struct
	{
		hawk_ooch_t backup[256];
		hawk_ooch_t buf[256];
		/*hawk_oow_t len;*/
	} errmsg;

	hawk_json_prim_t prim;

	struct
	{
		int trait;
	} cfg;

	hawk_json_state_node_t state_top;
	hawk_json_state_node_t* state_stack;

	hawk_oocs_t tok;
	hawk_oow_t tok_capa;
};

#endif
