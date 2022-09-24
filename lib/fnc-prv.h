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

#ifndef _HAWK_FNC_PRV_H_
#define _HAWK_FNC_PRV_H_

struct hawk_fnc_t
{
	struct
	{
		hawk_ooch_t* ptr;
		hawk_oow_t  len;
	} name;

	int dfl0; /* if set, ($0) is assumed if () is missing. 
	           * this ia mainly for the weird length() function */

	hawk_fnc_spec_t spec;
	const hawk_ooch_t* owner; /* set this to a module name if a built-in function is located in a module */

	hawk_mod_t* mod; /* set by the engine to a valid pointer if it's associated to a module */
};

#if defined(__cplusplus)
extern "C" {
#endif

hawk_fnc_t* hawk_findfncwithbcs (hawk_t* hawk, const hawk_bcs_t* name);
hawk_fnc_t* hawk_findfncwithucs (hawk_t* hawk, const hawk_ucs_t* name);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_findfncwithoocs hawk_findfncwithbcs
#else
#	define hawk_findfncwithoocs hawk_findfncwithucs
#endif

/* EXPORT is required for linking on windows as they are referenced by mod-str.c */
HAWK_EXPORT int hawk_fnc_gsub    (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_index   (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_length  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_match   (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_rindex  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_split   (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_splita  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_sprintf (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_sub     (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_substr  (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_tolower (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);
HAWK_EXPORT int hawk_fnc_toupper (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi);

#if defined(__cplusplus)
}
#endif

#endif
