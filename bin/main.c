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

#include "main.h"
#include <hawk.h>
#include <hawk-xma.h>
#include <stdio.h>
#include <locale.h>

#if defined(_WIN32)
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_ERRORS
#	include <os2.h>
#	include <signal.h>
#elif defined(__DOS__)
#	include <dos.h>
#	include <signal.h>
#else
#	include <unistd.h>
#	include <errno.h>
#	include <signal.h>
#endif

/* -------------------------------------------------------- */

void hawk_main_print_xma (void* ctx, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
}
/* -------------------------------------------------------- */

void hawk_main_print_error (const hawk_bch_t* fmt, ...)
{
	va_list va;
	fprintf (stderr, "ERROR: ");
	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);
}

void hawk_main_print_warning (const hawk_bch_t* fmt, ...)
{
	va_list va;
	fprintf (stderr, "WARNING: ");
	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);
}

/* -------------------------------------------------------- */

static int main_version(int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0)
{
	printf ("%s %s\n", hawk_get_base_name_bcstr(real_argv0), HAWK_PACKAGE_VERSION);
	printf ("Copyright 2006-2022 Chung, Hyung-Hwan\n");
	return 0;
}

static void print_usage(FILE* out, const hawk_bch_t* real_argv0)
{
	const hawk_bch_t* b1 = hawk_get_base_name_bcstr(real_argv0);

	fprintf (out, "USAGE: %s [options] [mode specific options and parameters]\n", b1);
	fprintf (out, "Options as follows:\n");
	fprintf (out, " --usage                          print this message\n");
	fprintf (out, " --version                        print version\n");
	fprintf (out, " --awk/--hawk                     switch to the awk mode(default)\n");
	fprintf (out, " --sed                            switch to the sed mode\n");
}

static int main_usage(int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0)
{
	print_usage(stdout, real_argv0);
	return 0;
}

static struct {
	const hawk_bch_t* name;
	int (*main) (int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0);
} entry_funcs[] = {
	{ "awk",     main_hawk },
	{ "cut",     main_cut },
	{ "hawk",    main_hawk },
	{ "sed",     main_sed },
	{ "usage",   main_usage },
	{ "version", main_version }
};

int main(int argc, hawk_bch_t* argv[])
{
	int ret;

#if defined(_WIN32)
	char locale[100];
	UINT codepage;
	WSADATA wsadata;
	int sock_inited = 0;
#elif defined(__DOS__)
	extern BOOL _watt_do_exit;
	int sock_inited = 0;
#else
	/* nothing special */
#endif

#if defined(_WIN32)
	codepage = GetConsoleOutputCP();
	if (codepage == CP_UTF8)
	{
		/*SetConsoleOUtputCP(CP_UTF8);*/
		/*hawk_setdflcmgrbyid(HAWK_CMGR_UTF8);*/
	}
	else
	{
		/* .codepage */
		hawk_fmt_uintmax_to_bcstr(locale, HAWK_COUNTOF(locale), codepage, 10, -1, '\0', ".");
		setlocale(LC_ALL, locale);
		/* hawk_setdflcmgrbyid(HAWK_CMGR_SLMB); */
	}

#else
	setlocale(LC_ALL, "");
	/* hawk_setdflcmgrbyid(HAWK_CMGR_SLMB); */
#endif

#if defined(_WIN32)
	if (WSAStartup(MAKEWORD(2,0), &wsadata) != 0)
		hawk_main_print_warning("Failed to start up winsock\n");
	else sock_inited = 1;
#elif defined(__DOS__)
	/* TODO: add an option to skip watt-32 */
	_watt_do_exit = 0; /* prevent sock_init from exiting upon failure */
	if (sock_init() != 0) hawk_main_print_warning("Failed to initialize watt-32\n");
	else sock_inited = 1;
#endif

	if (argc >= 2)
	{
		hawk_oow_t i;
		const hawk_bch_t* first_opt = argv[1];
		for (i = 0; i < HAWK_COUNTOF(entry_funcs); i++) {
			if (first_opt[0] == '-' && first_opt[1] == '-' && hawk_comp_bcstr(&first_opt[2], entry_funcs[i].name, 0) == 0) {
				/* [NOTE]
				 *  if hawk is invoked via 'hawk --awk' or 'hawk --hawk',
				 *  the value ARGV[0] inside a hawk script is "--awk" or "--hawk" */
				ret = entry_funcs[i].main(argc -1, &argv[1], argv[0]);
				goto done;
			}
		}
	}

	ret = main_hawk(argc, argv, HAWK_NULL);

done:
#if defined(_WIN32)
	if (sock_inited) WSACleanup();
#elif defined(__DOS__)
	if (sock_inited) sock_exit();
#endif

	return ret;
}

/* ---------------------------------------------------------------------- */

#if defined(FAKE_SOCKET)
socket () {}
listen () {}
accept () {}
recvfrom () {}
connect () {}
getsockopt () {}
recv      () {}
setsockopt () {}
send      () {}
bind     () {}
shutdown  () {}

void* memmove (void* x, void* y, size_t z) {}
#endif
