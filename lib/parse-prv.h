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

#ifndef _HAWK_PARSE_PRV_H_
#define _HAWK_PARSE_PRV_H_

/* these enums should match kwtab in parse.c */
enum hawk_kwid_t
{
	HAWK_KWID_XABORT,
	HAWK_KWID_XARGC,
	HAWK_KWID_XARGV,
	HAWK_KWID_XGLOBAL,
	HAWK_KWID_XINCLUDE,
	HAWK_KWID_XINCLUDE_ONCE,
	HAWK_KWID_XLOCAL,
	HAWK_KWID_XNIL,
	HAWK_KWID_XPRAGMA,
	HAWK_KWID_XRESET,
	HAWK_KWID_BEGIN,
	HAWK_KWID_END,
	HAWK_KWID_BREAK,
	HAWK_KWID_CONTINUE,
	HAWK_KWID_DELETE,
	HAWK_KWID_DO,
	HAWK_KWID_ELSE,
	HAWK_KWID_EXIT,
	HAWK_KWID_FOR,
	HAWK_KWID_FUNCTION,
	HAWK_KWID_GETBLINE,
	HAWK_KWID_GETLINE,
	HAWK_KWID_IF,
	HAWK_KWID_IN,
	HAWK_KWID_NEXT,
	HAWK_KWID_NEXTFILE,
	HAWK_KWID_NEXTOFILE,
	HAWK_KWID_PRINT,
	HAWK_KWID_PRINTF,
	HAWK_KWID_RETURN,
	HAWK_KWID_WHILE
};

typedef enum hawk_kwid_t hawk_kwid_t;

#if defined(__cplusplus)
extern "C" {
#endif

int hawk_putsrcoocstr (
	hawk_t*            hawk,
	const hawk_ooch_t* str
);

int hawk_putsrcoochars (
	hawk_t*            hawk,
	const hawk_ooch_t* str,
	hawk_oow_t         len
);

const hawk_ooch_t* hawk_getgblname (
	hawk_t*     hawk,
	hawk_oow_t  idx,
	hawk_oow_t* len
);

void hawk_getkwname (
	hawk_t*       hawk,
	hawk_kwid_t   id,
	hawk_oocs_t*  s
);

int hawk_initgbls (
	hawk_t* hawk
);

hawk_ooch_t* hawk_addsionamewithuchars (
	hawk_t*            hawk,
	const hawk_uch_t* ptr,
	hawk_oow_t         len
);

hawk_ooch_t* hawk_addsionamewithbchars (
	hawk_t*            hawk,
	const hawk_bch_t* ptr,
	hawk_oow_t         len
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_addsionamewithoochars hawk_addsionamewithuchars
#else
#	define hawk_addsionamewithoochars hawk_addsionamewithbchars
#endif

void hawk_clearsionames (
	hawk_t* hawk
);


hawk_mod_t* hawk_querymodulewithname (
	hawk_t*          hawk,
	hawk_ooch_t*     name, /* this must be a mutable null-terminated string */
	hawk_mod_sym_t*  sym
);

#if defined(__cplusplus)
}
#endif

#endif
