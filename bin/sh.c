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
#include <hawk-sh.h>
#include <stdio.h>

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#	include <unistd.h>
#endif

int main_sh (int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0)
{
	hawk_sh_t* sh;
	hawk_errinf_t errinf;
	const hawk_bch_t* command = HAWK_NULL;
	const hawk_bch_t* path = HAWK_NULL;
	int i, interactive = 0, registered = 0, status;

	for (i = 1; i < argc; i++)
	{
		if (hawk_comp_bcstr(argv[i], "--help", 0) == 0 || hawk_comp_bcstr(argv[i], "-h", 0) == 0)
		{
			printf("Usage: %s --sh [-i] [--registered|--path] [-c command | file]\n", real_argv0? real_argv0: argv[0]);
			printf("Builtins: mode addcmd delcmd echo cd pwd export unset jobs wait fg bg kill exit\n");
			return 0;
		}
		if (hawk_comp_bcstr(argv[i], "-i", 0) == 0) interactive = 1;
		else if (hawk_comp_bcstr(argv[i], "--registered", 0) == 0) registered = 1;
		else if (hawk_comp_bcstr(argv[i], "--path", 0) == 0) registered = 0;
		else if (hawk_comp_bcstr(argv[i], "-c", 0) == 0 && i + 1 < argc && !path && !command) command = argv[++i];
		else if (argv[i][0] != '-' && !path && !command) path = argv[i];
		else
		{
			hawk_main_print_error("invalid shell option or operand: %s\n", argv[i]);
			return 2;
		}
	}

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
	if (!path && !command && isatty(0)) interactive = 1;
#endif

	sh = hawk_sh_openstd(HAWK_NULL, 0, HAWK_NULL, &errinf);
	if (!sh)
	{
		hawk_main_print_error("cannot open shell (error %d)\n", (int)errinf.num);
		return 125;
	}

	hawk_sh_setmode(sh, registered? HAWK_SH_REGISTERED: HAWK_SH_PATH);
	if (interactive && hawk_sh_setinteractivestd(sh, 1) <= -1) status = -1;
	else if (command) status = hawk_sh_execbcs(sh, command);
	else
	{
#if defined(HAWK_OOCH_IS_UCH)
		hawk_ooch_t* converted = path? hawk_gem_dupbtoucstr(hawk_sh_getgem(sh), path, HAWK_NULL, 0): HAWK_NULL;

		status = path && !converted? -1: hawk_sh_execstd(sh, converted);
		if (converted) hawk_gem_freemem(hawk_sh_getgem(sh), converted);
#else
		status = hawk_sh_execstd(sh, path);
#endif
	}

	if (status <= -1)
	{
		const hawk_loc_t* loc = hawk_sh_geterrloc(sh);

		if (loc->line > 0)
		{
			hawk_main_print_error("line %lu: %s\n", (unsigned long)loc->line, hawk_sh_geterrbmsg(sh));
		}
		else hawk_main_print_error("%s\n", hawk_sh_geterrbmsg(sh));
	}

	hawk_sh_close(sh);
	return status <= -1? 2: status;
}
