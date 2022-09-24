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

#ifndef _HAWK_RIO_PRV_H_
#define _HAWK_RIO_PRV_H_

enum hawk_rio_type_t
{
	/* rio types available */
	HAWK_RIO_PIPE = 0,
	HAWK_RIO_FILE = 1,
	HAWK_RIO_CONSOLE = 2,

	/* reserved for internal use only */
	HAWK_RIO_NUM
};
typedef enum hawk_rio_type_t hawk_rio_type_t;

#if defined(__cplusplus)
extern "C" {
#endif

hawk_rio_type_t hawk_rtx_intoriotype (hawk_rtx_t* rtx, hawk_in_type_t in_type);
hawk_rio_type_t hawk_rtx_outtoriotype (hawk_rtx_t* rtx, hawk_out_type_t out_type);

int hawk_rtx_readio (
	hawk_rtx_t* rtx, hawk_in_type_t in_type, 
	const hawk_ooch_t* name, hawk_ooecs_t* buf);

int hawk_rtx_readiobytes (
	hawk_rtx_t* rtx, hawk_in_type_t in_type, 
	const hawk_ooch_t* name, hawk_becs_t* buf);

int hawk_rtx_writeioval (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, 
	const hawk_ooch_t* name, hawk_val_t* v);

int hawk_rtx_writeiostr (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, 
	const hawk_ooch_t* name, hawk_ooch_t* str, hawk_oow_t len);

int hawk_rtx_writeiobytes (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, 
	const hawk_ooch_t* name, hawk_bch_t* str, hawk_oow_t len);

int hawk_rtx_flushio (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name);

int hawk_rtx_nextio_read (
	hawk_rtx_t* rtx, hawk_in_type_t in_type, const hawk_ooch_t* name);

int hawk_rtx_nextio_write (
	hawk_rtx_t* rtx, hawk_out_type_t out_type, const hawk_ooch_t* name);

int hawk_rtx_closeio (
	hawk_rtx_t*        rtx,
	const hawk_ooch_t* name,
	const hawk_ooch_t* opt
);

void hawk_rtx_flushallios (hawk_rtx_t* rtx);
void hawk_rtx_clearallios (hawk_rtx_t* rtx);

#if defined(__cplusplus)
}
#endif

#endif
