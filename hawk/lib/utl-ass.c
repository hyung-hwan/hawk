/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#include <hawk-cmn.h>

#if defined(HAWK_BUILD_RELEASE)

void hawk_assert_fail (const hawk_bch_t* expr, const hawk_bch_t* file, hawk_oow_t line)
{
	/* do nothing */
}

#else /* defined(HAWK_BUILD_RELEASE) */

#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_DOSPROCESS
#	define INCL_DOSFILEMGR
#	include <os2.h>
#elif defined(__DOS__)
#	include <dos.h>
#	include <dosfunc.h>
#elif defined(vms) || defined(__vms)
#	define __NEW_STARLET 1
#	include <starlet.h> /* (SYS$...) */
#	include <ssdef.h> /* (SS$...) */
#	include <lib$routines.h> /* (lib$...) */
#elif defined(macintosh)
#	include <Process.h>
#	include <Dialogs.h>
#	include <TextUtils.h>
#else
#	include "syscall.h"
#endif

#if defined(HAWK_ENABLE_LIBUNWIND)
#include <libunwind.h>
static void backtrace_stack_frames (void)
{
	unw_cursor_t cursor;
	unw_context_t context;
	int n;

	unw_getcontext(&context);
	unw_init_local(&cursor, &context);

	printf ("[BACKTRACE]\n");
	for (n = 0; unw_step(&cursor) > 0; n++) 
	{
		unw_word_t ip, sp, off;
		char symbol[256];

		unw_get_reg (&cursor, UNW_REG_IP, &ip);
		unw_get_reg (&cursor, UNW_REG_SP, &sp);

		if (unw_get_proc_name(&cursor, symbol, HAWK_COUNTOF(symbol), &off)) 
		{
			hawk_copy_bcstr (symbol, HAWK_COUNTOF(symbol), "<unknown>");
		}

		printf (
			"#%02d ip=0x%*p sp=0x%*p %s+0x%lu\n", 
			n, HAWK_SIZEOF(void*) * 2, (void*)ip, HAWK_SIZEOF(void*) * 2, (void*)sp, symbol, (unsigned long int)off);
	}
}
#elif defined(HAVE_BACKTRACE)
#include <execinfo.h>
static void backtrace_stack_frames (void)
{
	void* btarray[128];
	hawk_oow_t btsize;
	char** btsyms;

	btsize = backtrace(btarray, HAWK_COUNTOF(btarray));
	btsyms = backtrace_symbols(btarray, btsize);
	if (btsyms)
	{
		hawk_oow_t i;
		printf ("[BACKTRACE]\n");

		for (i = 0; i < btsize; i++)
		{
			printf ("  %s\n", btsyms[i]);
		}
		free (btsyms);
	}
}
#else
static void backtrace_stack_frames (void)
{
	/* do nothing. not supported */
}
#endif /* defined(HAWK_ENABLE_LIBUNWIND) */

void hawk_assert_fail (const hawk_bch_t* expr, const hawk_bch_t* file, hawk_oow_t line)
{
	printf ("ASSERTION FAILURE: %s at %s:%lu\n", expr, file, (unsigned long int)line);
	backtrace_stack_frames ();

#if defined(_WIN32)
	ExitProcess (249);
#elif defined(__OS2__)
	DosExit (EXIT_PROCESS, 249);
#elif defined(__DOS__)
	{
		union REGS regs;
		regs.h.ah = DOS_EXIT;
		regs.h.al = 249;
		intdos (&regs, &regs);
	}
#elif defined(vms) || defined(__vms)
	lib$stop (SS$_ABORT); /* use SS$_OPCCUS instead? */
	/* this won't be reached since lib$stop() terminates the process */
	sys$exit (SS$_ABORT); /* this condition code can be shown with
	                       * 'show symbol $status' from the command-line. */
#elif defined(macintosh)

	ExitToShell ();

#else

	kill (getpid(), SIGABRT);
	_exit (1);
#endif
}
#endif /* defined(HAWK_BUILD_RELEASE) */
