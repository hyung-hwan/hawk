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

#include <HawkStd.hpp>
#include <hawk-utl.h>
#include <hawk-fmt.h>
#include <hawk-cli.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <locale.h>

#include <cstring>

#if defined(_WIN32)
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_DOSPROCESS
#	include <os2.h>
#else
#	include <unistd.h>
#	include <signal.h>
#	include <errno.h>
#endif

/* these three definitions for doxygen cross-reference */
typedef HAWK::HawkStd HawkStd;
typedef HAWK::HawkStd::Run Run;
typedef HAWK::HawkStd::Value Value;

class MyHawk: public HawkStd
{
public:
	MyHawk (HAWK::Mmgr* mmgr = HAWK_NULL): HawkStd(mmgr) { }
	~MyHawk () { this->close (); }

	int open ()
	{
		if (HawkStd::open () <= -1) return -1;

		idLastSleep = this->addGlobal(HAWK_T("LAST_SLEEP"));
		if (idLastSleep <= -1) goto oops;

		/* this is for demonstration only. 
		 * you can use sys::sleep() instead */
		if (this->addFunction(HAWK_T("sleep"), 1, 1, HAWK_NULL, (FunctionHandler)&MyHawk::sleep) <= -1 ||
		    this->addFunction (HAWK_T("sumintarray"), 1, 1, HAWK_NULL, (FunctionHandler)&MyHawk::sumintarray) <= -1 ||
		    this->addFunction (HAWK_T("arrayindices"), 1, 1, HAWK_NULL, (FunctionHandler)&MyHawk::arrayindices) <= -1) goto oops;

		return 0;

	oops:
		HawkStd::close ();
		return -1;
	}

	int sleep (Run& run, Value& ret, Value* args, hawk_oow_t nargs, const hawk_ooch_t* name, hawk_oow_t len)
	{
		if (args[0].isIndexed()) 
		{
			run.setError (HAWK_EINVAL);
			return -1;
		}

		hawk_int_t x = args[0].toInt();

		/*Value arg;
		if (run.getGlobal(idLastSleep, arg) == 0)
			hawk_printf (HAWK_T("GOOD: [%d]\n"), (int)arg.toInt());
		else { hawk_printf (HAWK_T("BAD:\n")); }
		*/

		if (run.setGlobal (idLastSleep, x) <= -1) return -1;

	#if defined(_WIN32)
		::Sleep ((DWORD)(x * 1000));
		return ret.setInt (0);
	#elif defined(__OS2__)
		::DosSleep ((ULONG)(x * 1000));
		return ret.setInt (0);
	#else
		return ret.setInt (::sleep (x));
	#endif
	}

	int sumintarray (Run& run, Value& ret, Value* args, hawk_oow_t nargs, const hawk_ooch_t* name, hawk_oow_t len)
	{
		// BEGIN { 
		//   for(i=0;i<=10;i++) x[i]=i; 
		//   print sumintarray(x);
		// }
		hawk_int_t x = 0;

		if (args[0].isIndexed()) 
		{
			Value val(run);
			Value::Index idx;
			Value::IndexIterator ii;

			ii = args[0].getFirstIndex(&idx);
			while (ii != ii.END)
			{
				if (args[0].getIndexed(idx, &val) <= -1) return -1;
				x += val.toInt ();

				ii = args[0].getNextIndex(&idx, ii);
			}
		}
		else x += args[0].toInt();

		return ret.setInt(x);
	}

	int arrayindices (Run& run, Value& ret, Value* args, hawk_oow_t nargs, const hawk_ooch_t* name, hawk_oow_t len)
	{
		// create another array composed of array indices
		// BEGIN { 
		//   for(i=0;i<=10;i++) x[i]=i; 
		//   y=arrayindices(x); 
		//   for (i in y) print y[i]; 
		// }
		if (!args[0].isIndexed()) return 0;

		Value::Index idx;
		Value::IndexIterator ii;
		hawk_int_t i;

		ii = args[0].getFirstIndex (&idx);
		for (i = 0; ii != ii.END ; i++)
		{
			Value::IntIndex iidx (i);
			if (ret.setIndexedStr (
				iidx, idx.pointer(), idx.length()) <= -1) return -1;
			ii = args[0].getNextIndex (&idx, ii);
		}
	
		return 0;
	}
	
private:
	int idLastSleep;
};

static MyHawk* app_hawk = HAWK_NULL;

static void print_error (const hawk_bch_t* fmt, ...)
{
	va_list va;

	fprintf (stderr, "ERROR: ");

	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);
}

static void print_error (MyHawk& hawk)
{
	hawk_errnum_t code = hawk.getErrorNumber();
	hawk_loc_t loc = hawk.getErrorLocation();

	if (loc.file)
	{
		print_error ("code %d line %lu at %s - %s\n", (int)code, (unsigned long int)loc.line, hawk.getErrorLocationFileB(), hawk.getErrorMessageB());
	}
	else
	{
		print_error ("code %d line %lu - %s\n", (int)code, (unsigned long int)loc.line, hawk.getErrorMessageB());
	}
}

#ifdef _WIN32
static BOOL WINAPI stop_run (DWORD ctrl_type)
{
	if (ctrl_type == CTRL_C_EVENT ||
	    ctrl_type == CTRL_CLOSE_EVENT)
	{
		if (app_hawk) app_hawk->stop ();
		return TRUE;
	}

	return FALSE;
}
#else

static int setsignal (int sig, void(*handler)(int), int restart)
{
	struct sigaction sa_int;

	sa_int.sa_handler = handler;
	sigemptyset (&sa_int.sa_mask);
	
	sa_int.sa_flags = 0;

	if (restart)
	{
	#ifdef SA_RESTART
		sa_int.sa_flags |= SA_RESTART;
	#endif
	}
	else
	{
	#ifdef SA_INTERRUPT
		sa_int.sa_flags |= SA_INTERRUPT;
	#endif
	}
	return sigaction (sig, &sa_int, NULL);
}

static void stop_run (int sig)
{
	int e = errno;
	if (app_hawk) app_hawk->halt ();
	errno = e;
}
#endif

static void set_signal (void)
{
#ifdef _WIN32
	SetConsoleCtrlHandler (stop_run, TRUE);
#else
	/*setsignal (SIGINT, stop_run, 1); TO BE MORE COMPATIBLE WITH WIN32*/
	setsignal (SIGINT, stop_run, 0);
#endif
}

static void unset_signal (void)
{
#ifdef _WIN32
	SetConsoleCtrlHandler (stop_run, FALSE);
#else
	setsignal (SIGINT, SIG_DFL, 1);
#endif
}

static void print_usage (FILE* out, const hawk_bch_t* argv0)
{
	fprintf (out, "USAGE: %s [options] -f sourcefile [ -- ] [datafile]*\n", argv0);
	fprintf (out, "       %s [options] [ -- ] sourcestring [datafile]*\n", argv0);
	fprintf (out, "Where options are:\n");
	fprintf (out, " -h                print this message\n");
	fprintf (out, " -f sourcefile     set the source script file\n");
	fprintf (out, " -d deparsedfile   set the deparsing output file\n");
	fprintf (out, " -o outputfile     set the console output file\n");
	fprintf (out, " -F string         set a field separator(FS)\n");
}

struct cmdline_t
{
	hawk_bch_t* ins;
	hawk_bch_t* inf;
	hawk_bch_t* outf;
	hawk_bch_t* outc;
	hawk_bch_t* fs;
};

static int handle_cmdline (MyHawk& awk, int argc, hawk_bch_t* argv[], cmdline_t* cmdline)
{
	static hawk_bcli_t opt =
	{
		"hF:f:d:o:",
		HAWK_NULL
	};
	hawk_bci_t c;

	std::memset (cmdline, 0, HAWK_SIZEOF(*cmdline));
	while ((c = hawk_get_bcli(argc, argv, &opt)) != HAWK_BCI_EOF)
	{
		switch (c)
		{
			case 'h':
				print_usage (stdout, argv[0]);
				return 0;

			case 'F':
				cmdline->fs = opt.arg;
				break;

			case 'f':
				cmdline->inf = opt.arg;
				break;

			case 'd':
				cmdline->outf = opt.arg;
				break;

			case 'o':
				cmdline->outc = opt.arg;
				break;

			case '?':
				print_error ("illegal option - '%c'\n", opt.opt);
				return -1;

			case ':':
				print_error ("bad argument for '%c'\n", opt.opt);
				return -1;

			default:
				print_usage (stderr, argv[0]);
				return -1;
		}
	}

	if (opt.ind < argc && !cmdline->inf) cmdline->ins = argv[opt.ind++];

	while (opt.ind < argc)
	{
		if (awk.addArgument(argv[opt.ind++]) <= -1) 
		{
			print_error (awk);
			return -1;
		}
	}

	if (!cmdline->ins && !cmdline->inf)
	{
		print_usage (stderr, argv[0]);
		return -1;
	}

	return 1;
}


static int hawk_main (MyHawk& hawk, int argc, hawk_bch_t* argv[])
{
	MyHawk::Run* run;
	cmdline_t cmdline;
	int n;

	hawk.setTrait (hawk.getTrait() | HAWK_FLEXMAP | HAWK_RWPIPE | HAWK_NEXTOFILE);

	// ARGV[0]
	if (hawk.addArgument(HAWK_T("hawk51")) <= -1)
	{
		print_error (hawk); 
		return -1; 
	}

	if ((n = handle_cmdline(hawk, argc, argv, &cmdline)) <= 0) return n;

	MyHawk::Source* in, * out;
	MyHawk::SourceString in_str(cmdline.ins);
	MyHawk::SourceFile in_file(cmdline.inf); 
	MyHawk::SourceFile out_file(cmdline.outf);

	in = (cmdline.ins)? (MyHawk::Source*)&in_str: (MyHawk::Source*)&in_file;
	out = (cmdline.outf)? (MyHawk::Source*)&out_file: &MyHawk::Source::NONE;
	run = hawk.parse (*in, *out);
	if (run == HAWK_NULL) 
	{
		print_error (hawk); 
		return -1; 
	}

	if (cmdline.fs)
	{
		MyHawk::Value fs (run);
		if (fs.setStr(cmdline.fs) <= -1) 
		{
			print_error (hawk); 
			return -1; 
		}
		if (hawk.setGlobal(HAWK_GBL_FS, fs) <= -1) 
		{
			print_error (hawk); 
			return -1; 
		}
	}

	if (cmdline.outc) 
	{
		if (hawk.addConsoleOutput(cmdline.outc) <= -1)
		{
			print_error (hawk); 
			return -1; 
		}
	}

	MyHawk::Value ret;
	if (hawk.loop(&ret) <= -1) 
	{ 
		print_error (hawk); 
		return -1; 
	}

	return 0;
}

static HAWK_INLINE int execute_hawk (int argc, hawk_bch_t* argv[])
{
	//HAWK::HeapMmgr hm (1000000);
	//MyHawk awk (&hm);
	MyHawk hawk;

	if (hawk.open() <= -1)
	{
		print_error (hawk);
		return -1;
	}
	app_hawk = &hawk;

	set_signal ();
	int n = hawk_main(hawk, argc, argv);
	unset_signal ();

	app_hawk = HAWK_NULL;
	hawk.close ();
	return n;
}

/* ---------------------------------------------------------------------- */

int main (int argc, hawk_bch_t* argv[])
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
		/*SetConsoleOUtputCP (CP_UTF8);*/
		/*hawk_setdflcmgrbyid (HAWK_CMGR_UTF8);*/
	}
	else
	{
		/* .codepage */
		hawk_fmt_uintmax_to_bcstr (locale, HAWK_COUNTOF(locale), codepage, 10, -1, '\0', ".");
		setlocale (LC_ALL, locale);
		/* hawk_setdflcmgrbyid (HAWK_CMGR_SLMB); */
	}

#else
	setlocale (LC_ALL, "");
	/* hawk_setdflcmgrbyid (HAWK_CMGR_SLMB); */
#endif

#if defined(_WIN32)
	if (WSAStartup(MAKEWORD(2,0), &wsadata) != 0)
		print_warning ("Failed to start up winsock\n");
	else sock_inited = 1;
#elif defined(__DOS__)
	/* TODO: add an option to skip watt-32 */
	_watt_do_exit = 0; /* prevent sock_init from exiting upon failure */
	if (sock_init() != 0) print_warning ("Failed to initialize watt-32\n");
	else sock_inited = 1;
#endif

	ret = execute_hawk(argc, argv);

#if defined(_WIN32)
	if (sock_inited) WSACleanup ();
#elif defined(__DOS__)
	if (sock_inited) sock_exit ();
#endif

	return ret;
}
