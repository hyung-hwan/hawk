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

#ifndef _HAWK_CUT_PRV_H_
#define _HAWK_CUT_PRV_H_

#include <hawk-cut.h>
#include <hawk-ecs.h>

typedef struct hawk_cut_sel_blk_t hawk_cut_sel_blk_t;

struct hawk_cut_sel_blk_t
{
	hawk_oow_t len;
	struct
	{
		enum
		{
			HAWK_SED_SEL_CHAR = HAWK_T('c'),
			HAWK_SED_SEL_FIELD = HAWK_T('f')
		} id;
		hawk_oow_t start;
		hawk_oow_t end;
	} range[128];
	hawk_cut_sel_blk_t* next;
};

struct hawk_cut_t
{
	HAWK_CUT_HDR;

#if 0
	hawk_cut_errstr_t errstr; /**< error string getter */
	hawk_cut_errnum_t errnum; /**< stores an error number */
	hawk_ooch_t errmsg[128];  /**< error message holder */
#endif

	int option;              /**< stores options */

	struct
	{
		hawk_cut_sel_blk_t  fb; /**< the first block is static */
		hawk_cut_sel_blk_t* lb; /**< points to the last block */

		hawk_ooch_t         din; /**< input field delimiter */
		hawk_ooch_t         dout; /**< output field delimiter */

		hawk_oow_t         count; 
		hawk_oow_t         fcount; 
		hawk_oow_t         ccount; 
        } sel;

	struct
	{
		/** data needed for output streams */
		struct
		{
			hawk_cut_io_impl_t fun; /**< an output handler */
			hawk_cut_io_arg_t arg; /**< output handling data */

			hawk_ooch_t buf[2048];
			hawk_oow_t len;
			int        eof;
		} out;
		
		/** data needed for input streams */
		struct
		{
			hawk_cut_io_impl_t fun; /**< an input handler */
			hawk_cut_io_arg_t arg; /**< input handling data */

			hawk_ooch_t xbuf[1]; /**< a read-ahead buffer */
			int xbuf_len; /**< data length in the buffer */

			hawk_ooch_t buf[2048]; /**< input buffer */
			hawk_oow_t len; /**< data length in the buffer */
			hawk_oow_t pos; /**< current position in the buffer */
			int        eof; /**< EOF indicator */

			hawk_ooecs_t line; /**< pattern space */
			hawk_oow_t num; /**< current line number */

			hawk_oow_t  nflds; /**< the number of fields */
			hawk_oow_t  cflds; /**< capacity of flds field */
			hawk_oocs_t  sflds[128]; /**< static field buffer */
			hawk_oocs_t* flds;
			int delimited;
		} in;
	} e;
};

#ifdef __cplusplus
extern "C" {
#endif


/* NOTHING */

#ifdef __cplusplus
}
#endif

#endif
