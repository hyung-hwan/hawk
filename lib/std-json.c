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

#include "json-prv.h"
#include <hawk-std.h>

typedef struct xtn_t xtn_t;
struct xtn_t
{
	/* empty. nothing at the moment */
};

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE xtn_t* GET_XTN(hawk_json_t* json) { return (xtn_t*)((hawk_uint8_t*)hawk_json_getxtn(json) - HAWK_SIZEOF(xtn_t)); }
#else
#define GET_XTN(json) ((xtn_t*)((hawk_uint8_t*)hawk_json_getxtn(json) - HAWK_SIZEOF(xtn_t)))
#endif

hawk_json_t* hawk_json_openstd (hawk_oow_t xtnsize, hawk_json_prim_t* prim, hawk_errnum_t* errnum)
{
	return hawk_json_openstdwithmmgr (hawk_get_sys_mmgr(), xtnsize, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), prim, errnum);
}

hawk_json_t* hawk_json_openstdwithmmgr (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_json_prim_t* prim, hawk_errnum_t* errnum)
{
	hawk_json_t* json;

	if (!mmgr) mmgr = hawk_get_sys_mmgr();
	if (!cmgr) cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	json = hawk_json_open(mmgr, HAWK_SIZEOF(xtn_t) + xtnsize, cmgr, prim, errnum);
	if (!json) return HAWK_NULL;

	json->_instsize += HAWK_SIZEOF(xtn_t);

	return json;
}
