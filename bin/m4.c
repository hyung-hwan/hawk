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

#include "main.h"
#include <hawk-m4.h>
#include <stdio.h>
#include <string.h>

static void print_usage (FILE* out, const hawk_bch_t* argv0, const hawk_bch_t* real_argv0)
{
	const hawk_bch_t* b1 = hawk_get_base_name_bcstr(real_argv0? real_argv0: argv0);
	const hawk_bch_t* b2 = real_argv0? " ": "";
	const hawk_bch_t* b3 = real_argv0? argv0: "";
	fprintf(out, "USAGE: %s%s%s [file ...]\n", b1, b2, b3);
	fprintf(out, "       %s%s%s --help\n", b1, b2, b3);
}

int main_m4 (int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0)
{
	hawk_m4_t* m4 = HAWK_NULL;
	const hawk_ooch_t** input = HAWK_NULL;
	hawk_errinf_t errinf;
	int i, ret = -1;

	if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
	{
		print_usage(stdout, argv[0], real_argv0);
		return 0;
	}

	m4 = hawk_m4_openstd(0, &errinf);
	if (!m4)
	{
	#if defined(HAWK_OOCH_IS_UCH)
		hawk_bch_t msgbuf[HAWK_ERRMSG_CAPA];
		hawk_oow_t ucslen = hawk_count_ucstr(errinf.msg), bcslen = HAWK_COUNTOF(msgbuf) - 1;
		hawk_conv_uchars_to_bchars_with_cmgr(errinf.msg, &ucslen, msgbuf, &bcslen, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8));
		msgbuf[bcslen] = '\0';
		hawk_main_print_error("cannot open m4 processor - %s\n", msgbuf);
	#else
		hawk_main_print_error("cannot open m4 processor - %s\n", errinf.msg);
	#endif
		return -1;
	}

	if (argc > 1)
	{
		input = (const hawk_ooch_t**)hawk_m4_callocmem(m4, HAWK_SIZEOF(*input) * argc);
		if (!input) goto oops;
		for (i = 1; i < argc; i++)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			input[i - 1] = argv[i];
		#else
			input[i - 1] = hawk_gem_dupbtoucstr(hawk_m4_getgem(m4), argv[i], HAWK_NULL, 1);
			if (!input[i - 1]) goto oops;
		#endif
		}
	}

	if (hawk_m4_execstd(m4, input) <= -1)
	{
		const hawk_loc_t* loc = hawk_m4_geterrloc(m4);
		if (loc->line > 0)
			hawk_main_print_error("cannot process m4 input - %s at line %lu\n", hawk_m4_geterrbmsg(m4), (unsigned long)loc->line);
		else
			hawk_main_print_error("cannot process m4 input - %s\n", hawk_m4_geterrbmsg(m4));
		goto oops;
	}
	ret = 0;

oops:
#if defined(HAWK_OOCH_IS_UCH)
	if (input) for (i = 0; i < argc - 1; i++) if (input[i]) hawk_m4_freemem(m4, (void*)input[i]);
#endif
	if (input) hawk_m4_freemem(m4, input);
	hawk_m4_close(m4);
	return ret;
}
