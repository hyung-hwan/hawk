/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#ifndef _HAWK_MISC_PRV_H_
#define _HAWK_MISC_PRV_H_

#include <hawk-tre.h>

#if defined(__cplusplus)
extern "C" {
#endif

hawk_ooch_t* hawk_rtx_strtok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, 
	const hawk_ooch_t* delim, hawk_oocs_t* tok);

hawk_ooch_t* hawk_rtx_strxtok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, hawk_oow_t len,
	const hawk_ooch_t* delim, hawk_oocs_t* tok);

hawk_ooch_t* hawk_rtx_strntok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, 
	const hawk_ooch_t* delim, hawk_oow_t delim_len, hawk_oocs_t* tok);

hawk_ooch_t* hawk_rtx_strxntok (
	hawk_rtx_t* rtx, const hawk_ooch_t* s, hawk_oow_t len,
	const hawk_ooch_t* delim, hawk_oow_t delim_len, hawk_oocs_t* tok);

hawk_ooch_t* hawk_rtx_strxntokbyrex (
	hawk_rtx_t*        rtx, 
	const hawk_ooch_t* str,
	hawk_oow_t         len,
	const hawk_ooch_t* substr,
	hawk_oow_t         sublen,
	hawk_tre_t*        rex,
	hawk_oocs_t*       tok
);

hawk_ooch_t* hawk_rtx_strxnfld (
	hawk_rtx_t*     rtx,
	hawk_ooch_t*    str,
	hawk_oow_t      len,
	hawk_ooch_t     fs,
	hawk_ooch_t     lq,
	hawk_ooch_t     rq,
	hawk_ooch_t     ec,
	hawk_oocs_t*    tok
);

int hawk_rtx_matchvalwithucs (
	hawk_rtx_t* rtx, hawk_val_t* val,
	const hawk_ucs_t* str, const hawk_ucs_t* substr,
	hawk_ucs_t* match, hawk_ucs_t submat[9]
);

int hawk_rtx_matchvalwithbcs (
	hawk_rtx_t* rtx, hawk_val_t* val,
	const hawk_bcs_t* str, const hawk_bcs_t* substr,
	hawk_bcs_t* match, hawk_bcs_t submat[9]
);

int hawk_rtx_matchrexwithucs (
	hawk_rtx_t* rtx, hawk_tre_t* code, 
	const hawk_ucs_t* str, const hawk_ucs_t* substr,
	hawk_ucs_t* match, hawk_ucs_t submat[9]
);

int hawk_rtx_matchrexwithbcs (
	hawk_rtx_t* rtx, hawk_tre_t* code, 
	const hawk_bcs_t* str, const hawk_bcs_t* substr,
	hawk_bcs_t* match, hawk_bcs_t submat[9]
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_rtx_matchvalwithoocs hawk_rtx_matchvalwithucs
#	define hawk_rtx_matchrexwithoocs hawk_rtx_matchrexwithucs
#else
#	define hawk_rtx_matchvalwithoocs hawk_rtx_matchvalwithbcs
#	define hawk_rtx_matchrexwithoocs hawk_rtx_matchrexwithbcs
#endif

#if defined(__cplusplus)
}
#endif

#endif

