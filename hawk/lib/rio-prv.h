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

#ifndef _HAWK_RIO_PRV_H_
#define _HAWK_RIO_PRV_H_

#if defined(__cplusplus)
extern "C" {
#endif

int hawk_rtx_readio (
	hawk_rtx_t* run, int in_type, 
	const hawk_ooch_t* name, hawk_ooecs_t* buf);

int hawk_rtx_writeioval (
	hawk_rtx_t* run, int out_type, 
	const hawk_ooch_t* name, hawk_val_t* v);

int hawk_rtx_writeiostr (
	hawk_rtx_t* run, int out_type, 
	const hawk_ooch_t* name, hawk_ooch_t* str, hawk_oow_t len);

int hawk_rtx_writeiobytes (
	hawk_rtx_t* run, int out_type, 
	const hawk_ooch_t* name, hawk_bch_t* str, hawk_oow_t len);

int hawk_rtx_flushio (
	hawk_rtx_t* run, int out_type, const hawk_ooch_t* name);

int hawk_rtx_nextio_read (
	hawk_rtx_t* run, int in_type, const hawk_ooch_t* name);

int hawk_rtx_nextio_write (
	hawk_rtx_t* run, int out_type, const hawk_ooch_t* name);

int hawk_rtx_closeio (
	hawk_rtx_t*    run,
	const hawk_ooch_t* name,
	const hawk_ooch_t* opt
);

void hawk_rtx_cleario (hawk_rtx_t* run);

#if defined(__cplusplus)
}
#endif

#endif
