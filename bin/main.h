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

#ifndef _MAIN_H_
#define _MAIN_H_

#include <hawk.h>

typedef void (*hawk_main_sig_handler_t) (int sig);


typedef struct hawk_main_xarg_t hawk_main_xarg_t;

struct hawk_main_xarg_t
{
	hawk_bch_t** ptr;
	hawk_oow_t   size;
	hawk_oow_t   capa;
};

#if defined(__cplusplus)
extern "C" {
#endif

int main_cut(int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0);
int main_hawk(int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0);
int main_sed(int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0);

void hawk_main_print_xma (void* ctx, const hawk_bch_t* fmt, ...);
void hawk_main_print_error (const hawk_bch_t* fmt, ...);
void hawk_main_print_warning (const hawk_bch_t* fmt, ...);

int hawk_main_set_signal_handler (int sig, hawk_main_sig_handler_t handler, int extra_flags);
int hawk_main_unset_signal_handler (int sig);
void hawk_main_do_nothing_on_signal (int sig);

int hawk_main_collect_into_xarg (const hawk_bcs_t* path, hawk_main_xarg_t* xarg);
void hawk_main_purge_xarg (hawk_main_xarg_t* xarg);
int hawk_main_expand_wildcard (int argc, hawk_bch_t* argv[], int do_glob, hawk_main_xarg_t* xarg);

#if defined(__cplusplus)
}
#endif

#endif
