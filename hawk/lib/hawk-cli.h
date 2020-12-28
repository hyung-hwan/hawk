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

#ifndef _HAWK_CLI_H_
#define _HAWK_CLI_H_

#include <hawk-cmn.h>

/** \file
 * This file defines functions and data structures to process 
 * command-line arguments. 
 */

typedef struct hawk_ucli_t hawk_ucli_t;
typedef struct hawk_ucli_lng_t hawk_ucli_lng_t;

struct hawk_ucli_lng_t
{
	const hawk_uch_t* str;
	hawk_uci_t        val;
};

struct hawk_ucli_t
{
	/* input */
	const hawk_uch_t* str; /* option string  */
	hawk_ucli_lng_t*  lng; /* long options */

	/* output */
	hawk_uci_t        opt; /* character checked for validity */
	hawk_uch_t*       arg; /* argument associated with an option */

	/* output */
	const hawk_uch_t* lngopt; 

	/* input + output */
	int              ind; /* index into parent argv vector */

	/* input + output - internal*/
	hawk_uch_t*       cur;
};

typedef struct hawk_bcli_t hawk_bcli_t;
typedef struct hawk_bcli_lng_t hawk_bcli_lng_t;

struct hawk_bcli_lng_t
{
	const hawk_bch_t* str;
	hawk_bci_t        val;
};

struct hawk_bcli_t
{
	/* input */
	const hawk_bch_t* str; /* option string  */
	hawk_bcli_lng_t*  lng; /* long options */

	/* output */
	hawk_bci_t        opt; /* character checked for validity */
	hawk_bch_t*       arg; /* argument associated with an option */

	/* output */
	const hawk_bch_t* lngopt; 

	/* input + output */
	int              ind; /* index into parent argv vector */

	/* input + output - internal*/
	hawk_bch_t*       cur;
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_get_cli() function processes the \a argc command-line arguments
 * pointed to by \a argv as configured in \a opt. It can process two 
 * different option styles: a single character starting with '-', and a 
 * long name starting with '--'. 
 *
 * A character in \a opt.str is treated as a single character option. Should
 * it require a parameter, specify ':' after it.
 *
 * Two special returning option characters indicate special error conditions. 
 * - \b ? indicates a bad option stored in the \a opt->opt field.
 * - \b : indicates a bad parameter for an option stored in the \a opt->opt field.
 *
 * @return an option character on success, HAWK_CHAR_EOF on no more options.
 */
HAWK_EXPORT hawk_uci_t hawk_get_ucli (
	int                 argc, /* argument count */ 
	hawk_uch_t* const*  argv, /* argument array */
	hawk_ucli_t*        opt   /* option configuration */
);

HAWK_EXPORT hawk_bci_t hawk_get_bcli (
	int                 argc, /* argument count */ 
	hawk_bch_t* const*  argv, /* argument array */
	hawk_bcli_t*        opt   /* option configuration */
);


#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_cli_t hawk_ucli_t
#	define hawk_cli_lng_t hawk_ucli_lng_t
#	define hawk_get_cli(argc,argv,opt) hawk_get_ucli(argc,argv,opt)
#else
#	define hawk_cli_t hawk_bcli_t
#	define hawk_cli_lng_t hawk_bcli_lng_t
#	define hawk_get_cli(argc,argv,opt) hawk_get_bcli(argc,argv,opt)
#endif

#if defined(__cplusplus)
}
#endif

#endif
