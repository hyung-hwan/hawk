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

#ifndef _HAWK_SH_PRV_H_
#define _HAWK_SH_PRV_H_

#include <hawk-sh.h>

typedef struct hawk_sh_creg_t hawk_sh_creg_t;

/* command registration */
struct hawk_sh_creg_t
{
	hawk_sh_creg_t* next;
	hawk_ooch_t* name;
	hawk_ooch_t* path;
	hawk_sh_cmd_impl_t impl;
	void* ctx;
};

struct hawk_sh_t
{
	HAWK_SH_HDR;
	hawk_sh_prm_t prm;
	void* prm_ctx;
	void* std_ctx;
	void (*std_close) (hawk_sh_t*);
	hawk_sh_creg_t* creg;
	hawk_sh_mode_t mode;
	int status;
	int exiting;
	int running;
	int reading_heredoc;
	hawk_oow_t line;
};

#endif
