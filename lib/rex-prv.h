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

#ifndef _HAWK_REX_PRV_H_
#define _HAWK_REX_PRV_H_

#include "hawk-rex.h"

int hawk_rex_buildpair (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm,
	const hawk_ooch_t* pattern, hawk_oow_t len, int flags,
	void** code, void** icode
);

int hawk_rex_matchucs (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm, const void* code,
	const hawk_ucs_t* str, const hawk_ucs_t* substr, int flags,
	hawk_ucs_t* match, hawk_ucs_t submat[9]
);

int hawk_rex_matchbcs (
	hawk_gem_t* gem, const hawk_rex_prm_t* prm, const void* code,
	const hawk_bcs_t* str, const hawk_bcs_t* substr, int flags,
	hawk_bcs_t* match, hawk_bcs_t submat[9]
);

#if defined(HAWK_OOCH_IS_UCH)
#define hawk_rex_matchoocs hawk_rex_matchucs
#else
#define hawk_rex_matchoocs hawk_rex_matchbcs
#endif

#endif
