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

#ifndef _HAWK_STD_PRV_H_
#define _HAWK_STD_PRV_H_

#include <hawk-std.h>

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_flt_t hawk_stdmathpow (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y);
HAWK_EXPORT hawk_flt_t hawk_stdmathmod (hawk_t* hawk, hawk_flt_t x, hawk_flt_t y);

HAWK_EXPORT int hawk_stdmodstartup (hawk_t* hawk);
HAWK_EXPORT void hawk_stdmodshutdown (hawk_t* hawk);

HAWK_EXPORT void* hawk_stdmodopen (hawk_t* hawk, const hawk_mod_spec_t* spec);
HAWK_EXPORT void hawk_stdmodclose (hawk_t* hawk, void* handle);
HAWK_EXPORT void* hawk_stdmodgetsym (hawk_t* hawk, void* handle, const hawk_ooch_t* name);

#if defined(__cplusplus)
}
#endif


#endif
