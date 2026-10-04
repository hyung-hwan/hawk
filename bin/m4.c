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
#include <hawk-cli.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum macro_action_type_t
{
	MACRO_ACTION_DEFINE,
	MACRO_ACTION_UNDEFINE
};

struct macro_action_t
{
	int type;
	hawk_bch_t* spec;
};

static void print_usage (FILE* out, const hawk_bch_t* argv0, const hawk_bch_t* real_argv0)
{
	const hawk_bch_t* b1 = hawk_get_base_name_bcstr(real_argv0? real_argv0: argv0);
	const hawk_bch_t* b2 = real_argv0? " ": "";
	const hawk_bch_t* b3 = real_argv0? argv0: "";
	fprintf(out, "USAGE: %s%s%s [options] [file ...]\n", b1, b2, b3);
	fprintf(out, "       %s%s%s --help\n", b1, b2, b3);
	fprintf(out, "Options as follows:\n");
	fprintf(out, " -h/--help                  print this message\n");
	fprintf(out, " -D/--define name[=value]   define a macro\n");
	fprintf(out, " -U/--undefine name         undefine a macro\n");
	fprintf(out, " -I/--incdirs directory     append an include directory\n");
}

static int append_incdir (hawk_bch_t** list, hawk_oow_t* len, const hawk_bch_t* dir)
{
	hawk_oow_t dlen = hawk_count_bcstr(dir);
	hawk_oow_t nlen;
	hawk_bch_t* tmp;

	if (dlen == 0) return 0;
	nlen = *len + (*len > 0) + dlen;
	tmp = (hawk_bch_t*)realloc(*list, HAWK_SIZEOF(*tmp) * (nlen + 1));
	if (!tmp) return -1;
	if (*len > 0) tmp[(*len)++] = HAWK_DFL_PATH_LIST_SEP;
	memcpy(&tmp[*len], dir, HAWK_SIZEOF(*tmp) * (dlen + 1));
	*len = nlen;
	*list = tmp;
	return 0;
}

static void print_m4_error (hawk_m4_t* m4, const hawk_bch_t* prefix)
{
	const hawk_loc_t* loc = hawk_m4_geterrloc(m4);
	if (loc->line > 0)
		hawk_main_print_error("%s - %s at line %lu\n", prefix, hawk_m4_geterrbmsg(m4), (unsigned long)loc->line);
	else
		hawk_main_print_error("%s - %s\n", prefix, hawk_m4_geterrbmsg(m4));
}

int main_m4 (int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0)
{
	hawk_m4_t* m4 = HAWK_NULL;
	const hawk_ooch_t** input = HAWK_NULL;
	struct macro_action_t* action = HAWK_NULL;
	hawk_oow_t action_count = 0;
	hawk_bch_t* incdirs = HAWK_NULL;
	hawk_oow_t incdirs_len = 0;
	hawk_oow_t input_count = 0;
	hawk_errinf_t errinf;
	hawk_bcli_t opt;
	hawk_bci_t c;
	int i, ret = -1;

	static hawk_bcli_lng_t lng[] =
	{
		{ ":define",       'D' },
		{ ":undefine",     'U' },
		{ ":incdirs",      'I' },
		{ ":includedirs",  'I' },
		{ "help",          'h' },
		{ HAWK_NULL,        '\0' }
	};

	memset(&opt, 0, HAWK_SIZEOF(opt));
	opt.str = "hD:U:I:";
	opt.lng = lng;

	action = (struct macro_action_t*)calloc(argc, HAWK_SIZEOF(*action));
	if (!action)
	{
		hawk_main_print_error("out of memory\n");
		goto oops;
	}

	while ((c = hawk_get_bcli(argc, argv, &opt)) != HAWK_BCI_EOF)
	{
		switch (c)
		{
			case 'h':
				print_usage(stdout, argv[0], real_argv0);
				ret = 0;
				goto oops;

			case 'D':
				action[action_count].type = MACRO_ACTION_DEFINE;
				action[action_count++].spec = opt.arg;
				break;

			case 'U':
				action[action_count].type = MACRO_ACTION_UNDEFINE;
				action[action_count++].spec = opt.arg;
				break;

			case 'I':
				if (append_incdir(&incdirs, &incdirs_len, opt.arg) <= -1)
				{
					hawk_main_print_error("out of memory\n");
					goto oops;
				}
				break;

			case '?':
				if (opt.lngopt) hawk_main_print_error("illegal option - '%s'\n", opt.lngopt);
				else hawk_main_print_error("illegal option - '%c'\n", opt.opt);
				goto oops;

			case ':':
				if (opt.lngopt) hawk_main_print_error("bad argument for '%s'\n", opt.lngopt);
				else hawk_main_print_error("bad argument for '%c'\n", opt.opt);
				goto oops;

			default:
				goto oops;
		}
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
		goto oops;
	}

	if (incdirs)
	{
	#if defined(HAWK_OOCH_IS_BCH)
		if (hawk_m4_setopt(m4, HAWK_M4_OPT_INCDIRS, incdirs) <= -1)
		{
			print_m4_error(m4, "cannot set m4 include directories");
			goto oops;
		}
	#else
		hawk_ooch_t* tmp = hawk_gem_dupbtoucstr(hawk_m4_getgem(m4), incdirs, HAWK_NULL, 1);
		if (!tmp) goto oops;
		if (hawk_m4_setopt(m4, HAWK_M4_OPT_INCDIRS, tmp) <= -1)
		{
			hawk_m4_freemem(m4, tmp);
			print_m4_error(m4, "cannot set m4 include directories");
			goto oops;
		}
		hawk_m4_freemem(m4, tmp);
	#endif
	}

	for (i = 0; i < action_count; i++)
	{
		hawk_bch_t* spec = action[i].spec;
		hawk_bch_t* eq = HAWK_NULL;
		int n;

		if (action[i].type == MACRO_ACTION_DEFINE) eq = hawk_find_bchar_in_bcstr(spec, '=');
		if (eq) *eq = '\0';

	#if defined(HAWK_OOCH_IS_BCH)
		n = action[i].type == MACRO_ACTION_DEFINE?
			hawk_m4_define(m4, spec, eq? eq + 1: HAWK_NULL):
			hawk_m4_undefine(m4, spec);
	#else
		{
			hawk_ooch_t* name = hawk_gem_dupbtoucstr(hawk_m4_getgem(m4), spec, HAWK_NULL, 1);
			hawk_ooch_t* value = HAWK_NULL;
			if (!name)
			{
				if (eq) *eq = '=';
				goto oops;
			}
			if (eq)
			{
				value = hawk_gem_dupbtoucstr(hawk_m4_getgem(m4), eq + 1, HAWK_NULL, 1);
				if (!value)
				{
					hawk_m4_freemem(m4, name);
					*eq = '=';
					goto oops;
				}
			}
			n = action[i].type == MACRO_ACTION_DEFINE?
				hawk_m4_define(m4, name, value):
				hawk_m4_undefine(m4, name);
			if (value) hawk_m4_freemem(m4, value);
			hawk_m4_freemem(m4, name);
		}
	#endif
		if (eq) *eq = '=';
		if (n <= -1)
		{
			print_m4_error(m4, "cannot apply m4 macro option");
			goto oops;
		}
	}

	input_count = argc - opt.ind;
	if (input_count > 0)
	{
		input = (const hawk_ooch_t**)hawk_m4_callocmem(m4, HAWK_SIZEOF(*input) * (input_count + 1));
		if (!input) goto oops;
		for (i = 0; i < input_count; i++)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			input[i] = argv[opt.ind + i];
		#else
			input[i] = hawk_gem_dupbtoucstr(hawk_m4_getgem(m4), argv[opt.ind + i], HAWK_NULL, 1);
			if (!input[i]) goto oops;
		#endif
		}
	}

	if (hawk_m4_execstd(m4, input) <= -1)
	{
		print_m4_error(m4, "cannot process m4 input");
		goto oops;
	}
	ret = 0;

oops:
#if defined(HAWK_OOCH_IS_UCH)
	if (input) for (i = 0; i < input_count; i++) if (input[i]) hawk_m4_freemem(m4, (void*)input[i]);
#endif
	if (input && m4) hawk_m4_freemem(m4, input);
	if (m4) hawk_m4_close(m4);
	if (incdirs) free(incdirs);
	if (action) free(action);
	return ret;
}
