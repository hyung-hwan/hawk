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

#include <hawk-std.h>
#include <hawk-utl.h>
#include <hawk-fmt.h>
#include <hawk-cli.h>
#include <hawk-xma.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <locale.h>

#define HAWK_MEMSET memset

// #define ENABLE_CALLBACK  TODO: enable this back
#define ABORT(label) goto label

#if defined(_WIN32)
#	include <winsock2.h>
#	include <windows.h>
#elif defined(__OS2__)
#	define INCL_ERRORS
#	include <os2.h>
#elif defined(__DOS__)
#	include <dos.h>
#	include <tcp.h> /* watt-32 */
#else
#	include <unistd.h>
#	include <errno.h>
#	if defined(HAWK_ENABLE_LIBLTDL)
#		include <ltdl.h>
#		define USE_LTDL
#	elif defined(HAVE_DLFCN_H)
#		include <dlfcn.h>
#		define USE_DLFCN
#	else
#		error UNSUPPORTED DYNAMIC LINKER
#	endif

#endif

static hawk_rtx_t* app_rtx = HAWK_NULL;
static int app_debug = 0;

typedef struct gv_t gv_t;
typedef struct gvm_t gvm_t;
typedef struct arg_t arg_t;
typedef struct xarg_t xarg_t;

struct gv_t
{
	int        idx;
	int        uc; /* if 0, the name and the value are bchs. otherwise, uchs */
	void*      name; /* hawk_bch_t* or hawk_uch_t* */
	hawk_ptl_t value;  /* hawk_bcs_t or hawk_ucs_t */
};

struct gvm_t
{
	gv_t*      ptr;
	hawk_oow_t size;
	hawk_oow_t capa;
};

struct xarg_t
{
	hawk_bch_t** ptr;
	hawk_oow_t   size;
	hawk_oow_t   capa;
};

struct arg_t
{
	int incl_conv;
	hawk_parsestd_t* psin; /* input source streams */
	hawk_bch_t*   osf;  /* output source file */
	xarg_t        icf; /* input console files */
	xarg_t        ocf; /* output console files */
	gvm_t         gvm; /* global variable map */
	hawk_bch_t*   fs;   /* field separator */
	hawk_bch_t*   call; /* function to call */
	hawk_cmgr_t*  script_cmgr;
	hawk_cmgr_t*  console_cmgr;
	hawk_bch_t*   includedirs;
	hawk_bch_t*   modlibdirs;

	unsigned int modern: 1;
	unsigned int classic: 1;
	int          opton;
	int          optoff;

	hawk_uintptr_t  memlimit;
#if defined(HAWK_BUILD_DEBUG)
	hawk_uintptr_t  failmalloc;
#endif
};


/* ========================================================================= */

/* ---------------------------------------------------------------------- */

typedef struct sig_state_t sig_state_t;
struct sig_state_t
{
	hawk_uintptr_t handler;
	hawk_uintptr_t old_handler;
#if defined(HAVE_SIGACTION)
	sigset_t  old_sa_mask;
	int       old_sa_flags;
#endif
};

typedef void (*sig_handler_t) (int sig);

static sig_state_t g_sig_state[HAWK_NSIG];

static int is_signal_handler_set (int sig)
{
	return !!g_sig_state[sig].handler;
}

#if defined(HAVE_SIGACTION)
static void dispatch_siginfo (int sig, siginfo_t* si, void* ctx)
{
	if (g_sig_state[sig].handler != (hawk_uintptr_t)SIG_IGN &&
	    g_sig_state[sig].handler != (hawk_uintptr_t)SIG_DFL)
	{
		((sig_handler_t)g_sig_state[sig].handler) (sig);
	}

	if (g_sig_state[sig].old_handler && 
	    g_sig_state[sig].old_handler != (hawk_uintptr_t)SIG_IGN &&
	    g_sig_state[sig].old_handler != (hawk_uintptr_t)SIG_DFL)
	{
		((void(*)(int, siginfo_t*, void*))g_sig_state[sig].old_handler) (sig, si, ctx);
	}
}
#endif

static void dispatch_signal (int sig)
{
	if (g_sig_state[sig].handler != (hawk_uintptr_t)SIG_IGN &&
	    g_sig_state[sig].handler != (hawk_uintptr_t)SIG_DFL)
	{
		((sig_handler_t)g_sig_state[sig].handler) (sig);
	}

	if (g_sig_state[sig].old_handler && 
	    g_sig_state[sig].old_handler != (hawk_uintptr_t)SIG_IGN &&
	    g_sig_state[sig].old_handler != (hawk_uintptr_t)SIG_DFL)
	{
		((sig_handler_t)g_sig_state[sig].old_handler) (sig);
	}
}

static int set_signal_handler (int sig, sig_handler_t handler, int extra_flags)
{
	if (g_sig_state[sig].handler)
	{
		/* already set - allow handler change. ignore extra_flags. */
		if (g_sig_state[sig].handler == (hawk_uintptr_t)handler) return -1;
		g_sig_state[sig].handler = (hawk_uintptr_t)handler;
	}
	else
	{
	#if defined(HAVE_SIGACTION)
		struct sigaction sa, oldsa;

		if (sigaction(sig, HAWK_NULL, &oldsa) == -1) return -1;

		HAWK_MEMSET (&sa, 0, HAWK_SIZEOF(sa));
		if (oldsa.sa_flags & SA_SIGINFO)
		{
			sa.sa_sigaction = dispatch_siginfo;
			sa.sa_flags = SA_SIGINFO;
		}
		else
		{
			sa.sa_handler = dispatch_signal;
			sa.sa_flags = 0;
		}
		sa.sa_flags |= extra_flags;
		/*sa.sa_flags |= SA_INTERUPT;
		sa.sa_flags |= SA_RESTART;*/
		sigfillset (&sa.sa_mask); /* block all signals while the handler is being executed */

		if (sigaction(sig, &sa, HAWK_NULL) == -1) return -1;

		g_sig_state[sig].handler = (hawk_uintptr_t)handler;
		if (oldsa.sa_flags & SA_SIGINFO)
			g_sig_state[sig].old_handler = (hawk_uintptr_t)oldsa.sa_sigaction;
		else
			g_sig_state[sig].old_handler = (hawk_uintptr_t)oldsa.sa_handler;

		g_sig_state[sig].old_sa_mask = oldsa.sa_mask;
		g_sig_state[sig].old_sa_flags = oldsa.sa_flags;
	#else
		g_sig_state[sig].old_handler = (hawk_uintptr_t)signal(sig, handler);
		g_sig_state[sig].handler = (hawk_uintptr_t)dispatch_signal;
	#endif
	}

	return 0;
}

static int unset_signal_handler (int sig)
{
#if defined(HAVE_SIGACTION)
	struct sigaction sa;
#endif

	if (!g_sig_state[sig].handler) return -1; /* not set */

#if defined(HAVE_SIGACTION)
	HAWK_MEMSET (&sa, 0, HAWK_SIZEOF(sa));
	sa.sa_mask = g_sig_state[sig].old_sa_mask;
	sa.sa_flags = g_sig_state[sig].old_sa_flags;

	if (sa.sa_flags & SA_SIGINFO)
	{
		sa.sa_sigaction = (void(*)(int,siginfo_t*,void*))g_sig_state[sig].old_handler;
	}
	else
	{
		sa.sa_handler = (sig_handler_t)g_sig_state[sig].old_handler;
	}

	if (sigaction(sig, &sa, HAWK_NULL) == -1) return -1;
#else
	signal (sig, (sig_handler_t)g_sig_state[sig].old_handler);
#endif

	g_sig_state[sig].handler = 0;
	/* keep other fields untouched */

	return 0;
}

/* ---------------------------------------------------------------------- */

static void stop_run (int signo)
{
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
	int e = errno;
#endif

	/*hawk_haltall(hawk_rtx_gethawk(app_rtx));*/
	hawk_rtx_halt (app_rtx);

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
	errno = e;
#endif
}

static void do_nothing (int unused)
{
}

static void set_intr_run (void)
{
#if defined(SIGTERM)
	set_signal_handler (SIGTERM, stop_run, 0);
#endif
#if defined(SIGHUP)
	set_signal_handler (SIGHUP, stop_run, 0);
#endif
#if defined(SIGINT)
	set_signal_handler (SIGINT, stop_run, 0);
#endif
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(SIGPIPE)
	set_signal_handler (SIGPIPE, do_nothing, 0);
#endif
}

static void unset_intr_run (void)
{
#if defined(SIGTERM)
	unset_signal_handler (SIGTERM);
#endif
#if defined(SIGHUP)
	unset_signal_handler (SIGHUP);
#endif
#if defined(SIGINT)
	unset_signal_handler (SIGINT);
#endif
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(SIGPIPE)
	unset_signal_handler (SIGPIPE);
#endif
}

static hawk_htb_walk_t print_awk_value (hawk_htb_t* map, hawk_htb_pair_t* pair, void* arg)
{
	hawk_rtx_t* rtx = (hawk_rtx_t*)arg;
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_errinf_t oerrinf;

	hawk_rtx_geterrinf (rtx, &oerrinf);

	str = hawk_rtx_getvaloocstr(rtx, HAWK_HTB_VPTR(pair), &len);
	if (HAWK_UNLIKELY(!str))
	{
		if (hawk_rtx_geterrnum(rtx) == HAWK_EVALTOSTR)
		{
			hawk_logfmt (hawk, HAWK_LOG_STDERR, HAWK_T("%.*js = [not printable]\n"), HAWK_HTB_KLEN(pair), HAWK_HTB_KPTR(pair));
			hawk_rtx_seterrinf (rtx, &oerrinf);
		}
		else
		{
			hawk_logfmt (hawk, HAWK_LOG_STDERR, HAWK_T("***OUT OF MEMORY***\n"));
		}
	}
	else
	{
		hawk_logfmt (hawk, HAWK_LOG_STDERR, HAWK_T("%.*js = %.*js\n"), HAWK_HTB_KLEN(pair), HAWK_HTB_KPTR(pair), len, str);
		hawk_rtx_freevaloocstr (rtx, HAWK_HTB_VPTR(pair), str);
	}

	return HAWK_HTB_WALK_FORWARD;
}

static int add_gvs_to_hawk (hawk_t* hawk, arg_t* arg)
{
	if (arg->gvm.size > 0)
	{
		hawk_oow_t i;

		for (i = 0; i < arg->gvm.size; i++)
		{
			arg->gvm.ptr[i].idx = arg->gvm.ptr[i].uc? hawk_addgblwithucstr(hawk, arg->gvm.ptr[i].name):
			                                          hawk_addgblwithbcstr(hawk, arg->gvm.ptr[i].name);
		}
	}

	return 0;
}

static int apply_fs_and_gvs_to_rtx (hawk_rtx_t* rtx, arg_t* arg)
{
	hawk_oow_t i;

	if (arg->fs)
	{
		hawk_val_t* fs;

		/* compose a string value to use to set FS to */
		fs = hawk_rtx_makestrvalwithbcstr(rtx, arg->fs);
		if (HAWK_UNLIKELY(!fs)) return -1;

		/* change FS according to the command line argument */
		hawk_rtx_refupval (rtx, fs);
		hawk_rtx_setgbl (rtx, HAWK_GBL_FS, fs);
		hawk_rtx_refdownval (rtx, fs);
	}

	if (arg->gvm.capa > 0)
	{
		/* set the value of user-defined global variables 
		 * to a runtime context */
		hawk_oow_t i;

		for (i = 0; i < arg->gvm.size; i++)
		{
			hawk_val_t* v;

			v = (arg->gvm.ptr[i].uc)? hawk_rtx_makenumorstrvalwithuchars(rtx, arg->gvm.ptr[i].value.ptr, arg->gvm.ptr[i].value.len):
			                          hawk_rtx_makenumorstrvalwithbchars(rtx, arg->gvm.ptr[i].value.ptr, arg->gvm.ptr[i].value.len);
			if (HAWK_UNLIKELY(!v)) return -1;

			hawk_rtx_refupval (rtx, v);
			hawk_rtx_setgbl (rtx, arg->gvm.ptr[i].idx, v);
			hawk_rtx_refdownval (rtx, v);
		}
	}

	for (i = 0; arg->psin[i].type != HAWK_PARSESTD_NULL; i++) 
	{
		if (arg->psin[i].type == HAWK_PARSESTD_FILE) 
		{
			if (hawk_rtx_setscriptnamewithoochars(rtx, arg->psin[i].u.file.path, hawk_count_oocstr(arg->psin[i].u.file.path)) <= -1) return -1;
			break;
		}
		else if (arg->psin[i].type == HAWK_PARSESTD_FILEB) 
		{
			if (hawk_rtx_setscriptnamewithbchars(rtx, arg->psin[i].u.fileb.path, hawk_count_bcstr(arg->psin[i].u.fileb.path)) <= -1) return -1;
			break;
		}
		else if (arg->psin[i].type == HAWK_PARSESTD_FILEU) 
		{
			if (hawk_rtx_setscriptnamewithuchars(rtx, arg->psin[i].u.fileu.path, hawk_count_ucstr(arg->psin[i].u.fileu.path)) <= -1) return -1;
			break;
		}
	}

	return 0;
}

static void dprint_return (hawk_rtx_t* rtx, hawk_val_t* ret)
{
	hawk_t* hawk = hawk_rtx_gethawk(rtx);
	hawk_oow_t len;
	hawk_ooch_t* str;
	

	if (hawk_rtx_isnilval(rtx, ret))
	{
		hawk_logfmt (hawk, HAWK_LOG_STDERR,HAWK_T("[RETURN] - ***nil***\n"));
	}
	else
	{
		str = hawk_rtx_valtooocstrdup (rtx, ret, &len);
		if (str == HAWK_NULL)
		{
			hawk_logfmt (hawk, HAWK_LOG_STDERR,HAWK_T("[RETURN] - ***OUT OF MEMORY***\n"));
		}
		else
		{
			hawk_logfmt (hawk, HAWK_LOG_STDERR, HAWK_T("[RETURN] - [%.*js]\n"), len, str);
			hawk_freemem (hawk_rtx_gethawk(rtx), str);
		}
	}

	hawk_logfmt (hawk, HAWK_LOG_STDERR, HAWK_T("[NAMED VARIABLES]\n"));
	hawk_htb_walk (hawk_rtx_getnvmap(rtx), print_awk_value, rtx);
	hawk_logfmt (hawk, HAWK_LOG_STDERR, HAWK_T("[END NAMED VARIABLES]\n"));
}

#if defined(ENABLE_CALLBACK)
static void on_statement (hawk_rtx_t* rtx, hawk_nde_t* nde)
{
	
	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR, HAWK_T("running %d at line %zu\n"), (int)nde->type, (hawk_oow_t)nde->loc.line);
}
#endif

static void print_version (const hawk_bch_t* argv0)
{
	printf ("%s %s\n", hawk_get_base_name_bcstr(argv0), HAWK_PACKAGE_VERSION);
	printf ("Copyright 2006-2022 Chung, Hyung-Hwan\n");
}

static void print_error (const hawk_bch_t* fmt, ...)
{
	va_list va;
	fprintf (stderr, "ERROR: ");
	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);
}

static void print_warning (const hawk_bch_t* fmt, ...)
{
	va_list va;
	fprintf (stderr, "WARNING: ");
	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);
}

struct opttab_t
{
	const hawk_bch_t* name;
	int opt;
	const hawk_bch_t* desc;
} opttab[] =
{
	{ "implicit",     HAWK_IMPLICIT,       "allow undeclared variables" },
	{ "multilinestr", HAWK_MULTILINESTR,   "allow raw multiline string and regular expression literals" },
	{ "nextofile",    HAWK_NEXTOFILE,      "enable nextofile & OFILENAME" },
	{ "rio",          HAWK_RIO,            "enable builtin I/O including getline & print" },
	{ "rwpipe",       HAWK_RWPIPE,         "allow a dual-directional pipe" },
	{ "newline",      HAWK_NEWLINE,        "enable a newline to terminate a statement" },
	{ "striprecspc",  HAWK_STRIPRECSPC,    "strip spaces in splitting a record" },
	{ "stripstrspc",  HAWK_STRIPSTRSPC,    "strip spaces in string-to-number conversion" },
	{ "blankconcat",  HAWK_BLANKCONCAT,    "enable concatenation by blanks" },
	{ "crlf",         HAWK_CRLF,           "use CRLF for a newline" },
	{ "flexmap",      HAWK_FLEXMAP,        "allow a map to be assigned or returned" },
	{ "pablock",      HAWK_PABLOCK,        "enable pattern-action loop" },
	{ "rexbound",     HAWK_REXBOUND,       "enable {n,m} in a regular expression" },
	{ "ncmponstr",    HAWK_NCMPONSTR,      "perform numeric comparsion on numeric strings" },
	{ "numstrdetect", HAWK_NUMSTRDETECT,   "detect a numeric string and convert it to a number" },
	{ "strictnaming", HAWK_STRICTNAMING,   "enable the strict naming rule" },
	{ "tolerant",     HAWK_TOLERANT,       "make more fault-tolerant" },
	{ HAWK_NULL,      0,                   HAWK_NULL }
};

static void print_usage (FILE* out, const hawk_bch_t* argv0)
{
	int j;
	const hawk_bch_t* b = hawk_get_base_name_bcstr(argv0);

	fprintf (out, "USAGE: %s [options] -f sourcefile [ -- ] [datafile]*\n", b);
	fprintf (out, "       %s [options] [ -- ] sourcestring [datafile]*\n", b);
	fprintf (out, "Where options are:\n");
	fprintf (out, " -h/--help                         print this message\n");
	fprintf (out, " --version                         print version\n");
	fprintf (out, " -D                                show extra information\n");
	fprintf (out, " -c/--call            name         call a function instead of entering\n");
	fprintf (out, "                                   the pattern-action loop. [datafile]* is\n");
	fprintf (out, "                                   passed to the function as parameters\n");
	fprintf (out, " -f/--file            file         set the source script file\n");
	fprintf (out, " -d/--deparsed-file   file         set the deparsed script file to produce\n");
	fprintf (out, " -t/--console-output  file         set the console output file\n");
	fprintf (out, "                                   multiple -t options are allowed\n");
	fprintf (out, " -F/--field-separator string       set a field separator(FS)\n");
	fprintf (out, " -v/--assign          var=value    add a global variable with a value\n");
	fprintf (out, " -m/--memory-limit    number       limit the memory usage (bytes)\n");
	fprintf (out, " -w                                expand datafile wildcards\n");
	
#if defined(HAWK_BUILD_DEBUG)
	fprintf (out, " -X                   number       fail the number'th memory allocation\n");
#endif
#if defined(HAWK_OOCH_IS_UCH)
	fprintf (out, " --script-encoding    string       specify script file encoding name\n");
	fprintf (out, " --console-encoding   string       specify console encoding name\n");
#endif

	fprintf (out, " -I/--includedirs     string       specify directories to look for includes files in\n");
	fprintf (out, " --modlibdirs         string       specify directories to look for module files in\n");
	fprintf (out, " --modern                          run in the modern mode(default)\n");
	fprintf (out, " --classic                         run in the classic mode\n");

	for (j = 0; opttab[j].name; j++)
	{
		fprintf (out, " --%-18s on/off       %s\n", opttab[j].name, opttab[j].desc);
	}
}

/* ---------------------------------------------------------------------- */
static int collect_into_xarg (const hawk_bcs_t* path, void* ctx)
{
	xarg_t* xarg = (xarg_t*)ctx;

	if (xarg->size <= xarg->capa)
	{
		hawk_bch_t** tmp;

		tmp = (hawk_bch_t**)realloc(xarg->ptr, HAWK_SIZEOF(*tmp) * (xarg->capa + 128 + 1));
		if (tmp == HAWK_NULL) return -1;

		xarg->ptr = tmp;
		xarg->capa += 128;
	}

	xarg->ptr[xarg->size] = strdup(path->ptr);
	if (xarg->ptr[xarg->size] == HAWK_NULL) return -1;
	xarg->size++;

	return 0;
}

static void purge_xarg (xarg_t* xarg)
{
	if (xarg->ptr)
	{
		hawk_oow_t i;

		for (i = 0; i < xarg->size; i++) free (xarg->ptr[i]);
		free (xarg->ptr);

		xarg->size = 0;
		xarg->capa = 0;
		xarg->ptr = HAWK_NULL;
	}
}

static int expand_wildcard (int argc, hawk_bch_t* argv[], int glob, xarg_t* xarg)
{
	int i;
	hawk_bcs_t tmp;

	for (i = 0; i < argc; i++)
	{
		int x;

#if 0
// TOOD: revive this part
		if (glob)
		{
		#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
			int glob_flags = HAWK_GLOB_TOLERANT | HAWK_GLOB_PERIOD | HAWK_GLOB_NOESCAPE | HAWK_GLOB_IGNORECASE;
		#else
			int glob_flags = HAWK_GLOB_TOLERANT | HAWK_GLOB_PERIOD;
		#endif

			x = hawk_glob(argv[i], collect_into_xarg, xarg, glob_flags, xarg->mmgr, hawk_getdflcmgr());
			if (x <= -1) return -1;
		}
		else x = 0;
#else
		x = 0;
#endif

		if (x == 0)
		{
			/* not expanded. just use it as is */
			tmp.ptr = argv[i];
			tmp.len = hawk_count_bcstr(argv[i]);
			if (collect_into_xarg(&tmp, xarg) <= -1) return -1;
		}
	}

	xarg->ptr[xarg->size] = HAWK_NULL;
	return 0;
}

/* ---------------------------------------------------------------------- */

static int process_argv (int argc, hawk_bch_t* argv[], struct arg_t* arg)
{
	static hawk_bcli_lng_t lng[] = 
	{
		{ ":implicit",         '\0' },
		{ ":multilinestr",     '\0' },
		{ ":nextofile",        '\0' },
		{ ":rio",              '\0' },
		{ ":rwpipe",           '\0' },
		{ ":newline",          '\0' },
		{ ":striprecspc",      '\0' },
		{ ":stripstrspc",      '\0' },
		{ ":blankconcat",      '\0' },
		{ ":crlf",             '\0' },
		{ ":flexmap",          '\0' },
		{ ":pablock",          '\0' },
		{ ":rexbound",         '\0' },
		{ ":ncmponstr",        '\0' },
		{ ":numstrdetect",     '\0' },
		{ ":strictnaming",     '\0' },
		{ ":tolerant",         '\0' },

		{ ":call",             'c' },
		{ ":file",             'f' },
		{ ":deparsed-file",    'd' },
		{ ":console-output",   't' },
		{ ":field-separator",  'F' },
		{ ":assign",           'v' },
		{ ":memory-limit",     'm' },

		{ ":script-encoding",  '\0' },
		{ ":console-encoding", '\0' },
		{ ":includedirs",      'I' },
		{ ":modlibdirs",       '\0' },

		{ "modern",            '\0' },
		{ "classic",           '\0' },

		{ "version",           '\0' },
		{ "help",              'h' },
		{ HAWK_NULL,           '\0' }                  
	};

	static hawk_bcli_t opt = 
	{
#if defined(HAWK_BUILD_DEBUG)
		"hDc:f:d:t:F:v:m:I:wX:",
#else
		"hDc:f:d:t:F:v:m:I:w",
#endif
		lng
	};

	hawk_bci_t c;

	hawk_oow_t i;
	hawk_oow_t isfc = 16; /* the capacity of isf */
	hawk_oow_t isfl = 0; /* number of input source files */
	hawk_parsestd_t* isf = HAWK_NULL; /* input source files */

	int oops_ret = -1;
	int do_glob = 0;

	HAWK_MEMSET (arg, 0, HAWK_SIZEOF(*arg));

	isf = (hawk_parsestd_t*)malloc(HAWK_SIZEOF(*isf) * isfc);
	if (!isf)
	{
		print_error ("out of memory\n");
		goto oops;
	}

	while ((c = hawk_get_bcli(argc, argv, &opt)) != HAWK_BCI_EOF)
	{
		switch (c)
		{
			case 'h':
				oops_ret = 0;
				goto oops;

			case 'D':
				app_debug = 1;
				break;

			case 'c':
				arg->call = opt.arg; /* function name to call */
				break;

			case 'f':
			{
				if (isfl >= isfc - 1) /* -1 for last HAWK_NULL */
				{
					hawk_parsestd_t* tmp;
					tmp = (hawk_parsestd_t*)realloc(isf, HAWK_SIZEOF(*isf) * (isfc + 16));
					if (tmp == HAWK_NULL)
					{
						print_error ("out of memory\n");
						goto oops;
					}

					isf = tmp;
					isfc = isfc + 16;
				}

				isf[isfl].type = HAWK_PARSESTD_FILEB;
				isf[isfl].u.fileb.path = opt.arg;
				isf[isfl].u.fileb.cmgr = HAWK_NULL;
				isfl++;
				break;
			}

			case 'd':
			{
				arg->osf = opt.arg; /* output source file */
				break;
			}

			case 't':
			{
				hawk_bcs_t tmp;
				tmp.ptr = opt.arg;
				tmp.len = hawk_count_bcstr(opt.arg);
				if (collect_into_xarg(&tmp, &arg->ocf) <= -1) 
				{
					print_error ("out of memory\n");
					goto oops;
				}
				arg->ocf.ptr[arg->ocf.size] = HAWK_NULL;
				break;
			}

			case 'F':
			{
				arg->fs = opt.arg; /* field seperator */
				break;
			}

			case 'v':
			{
				hawk_bch_t* eq;

				eq = hawk_find_bchar_in_bcstr(opt.arg, HAWK_T('='));
				if (eq == HAWK_NULL) 
				{
					if (opt.lngopt)
						print_error ("no value for '%s' in '%s'\n", opt.arg, opt.lngopt);
					else
						print_error ("no value for '%s' in '%c'\n", opt.arg, opt.opt);
					goto oops;
				}

				*eq = '\0';

				if (arg->gvm.size <= arg->gvm.capa)
				{
					gv_t* tmp;
					hawk_oow_t newcapa;

					newcapa = arg->gvm.capa + argc;
					tmp = realloc(arg->gvm.ptr, HAWK_SIZEOF(*tmp) * newcapa);
					if (!tmp)
					{
						print_error ("out of memory\n");
						goto oops;
					}

					arg->gvm.capa = newcapa;
					arg->gvm.ptr = tmp;
				}

				arg->gvm.ptr[arg->gvm.size].idx = -1;
				arg->gvm.ptr[arg->gvm.size].uc = 0;
				arg->gvm.ptr[arg->gvm.size].name = opt.arg;
				arg->gvm.ptr[arg->gvm.size].value.ptr = ++eq;
				arg->gvm.ptr[arg->gvm.size].value.len = hawk_count_bcstr(eq);
				arg->gvm.size++;
				break;
			}

			case 'm':
			{
				arg->memlimit = strtoul(opt.arg, HAWK_NULL, 10);
				break;
			}

			case 'w':
			{
				do_glob = 1;
				break;
			}


			case 'I':
			{
				arg->includedirs = opt.arg;
				break;
			}


#if defined(HAWK_BUILD_DEBUG)
			case 'X':
			{
				arg->failmalloc = strtoul(opt.arg, HAWK_NULL, 10);
				break;
			}
#endif

			case '\0':
			{
				/* a long option with no corresponding short option */
				hawk_oow_t i;

				if (hawk_comp_bcstr(opt.lngopt, "version", 0) == 0)
				{
					print_version (argv[0]);
					oops_ret = 2;
					goto oops;
				}
				else if (hawk_comp_bcstr(opt.lngopt, "script-encoding", 0) == 0)
				{
					arg->script_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (arg->script_cmgr == HAWK_NULL)
					{
						print_error ("unknown script encoding - %s\n", opt.arg);
						oops_ret = 3;
						goto oops;
					}
				}
				else if (hawk_comp_bcstr(opt.lngopt, "console-encoding", 0) == 0)
				{
					arg->console_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (arg->console_cmgr == HAWK_NULL)
					{
						print_error ("unknown console encoding - %s\n", opt.arg);
						oops_ret = 3;
						goto oops;
					}
				}
				else if (hawk_comp_bcstr(opt.lngopt, "modern", 0) == 0)
				{
					arg->modern = 1;
				}
				else if (hawk_comp_bcstr(opt.lngopt, "classic", 0) == 0)
				{
					arg->classic = 1;
				}
				else if (hawk_comp_bcstr(opt.lngopt, "modlibdirs", 0) == 0)
				{
					arg->modlibdirs = opt.arg;
				}
				else
				{
					for (i = 0; opttab[i].name; i++)
					{
						if (hawk_comp_bcstr(opt.lngopt, opttab[i].name, 0) == 0)
						{
							if (hawk_comp_bcstr(opt.arg, "off", 1) == 0)
								arg->optoff |= opttab[i].opt;
							else if (hawk_comp_bcstr(opt.arg, "on", 1) == 0)
								arg->opton |= opttab[i].opt;
							else
							{
								print_error ("invalid value '%s' for '%s' - use 'on' or 'off'\n", opt.arg, opt.lngopt);
								oops_ret = 3;
								goto oops;
							}
							break;
						}
					}

				}
				break;
			}
			
			case '?':
			{
				if (opt.lngopt)
					print_error ("illegal option - '%s'\n", opt.lngopt);
				else
					print_error ("illegal option - '%c'\n", opt.opt);

				goto oops;
			}

			case ':':
			{
				if (opt.lngopt)
					print_error ("bad argument for '%s'\n", opt.lngopt);
				else
					print_error ("bad argument for '%c'\n", opt.opt);

				goto oops;
			}

			default:
				goto oops;
		}
	}

	if (isfl <= 0)
	{
		/* no -f specified */
		if (opt.ind >= argc)
		{
			/* no source code specified */
			goto oops;
		}

		/* the source code is the string, not from the file */
		isf[isfl].type = HAWK_PARSESTD_BCS;
		isf[isfl].u.bcs.ptr = argv[opt.ind++];
		isf[isfl].u.bcs.len = hawk_count_bcstr(isf[isfl].u.bcs.ptr);

		isfl++;
	}

	for (i = 0; i < isfl ; i++) 
	{
		if (isf[i].type == HAWK_PARSESTD_FILE) isf[i].u.file.cmgr = arg->script_cmgr;
	}

	isf[isfl].type = HAWK_PARSESTD_NULL;
	arg->psin = isf;

	if (opt.ind < argc) 
	{
		/* the remaining arguments are input console file names */
		if (expand_wildcard(argc - opt.ind,  &argv[opt.ind], do_glob, &arg->icf) <= -1)
		{
			print_error ("failed to expand wildcard\n");
			goto oops;
		}
	}

	return 1;

oops:
	if (arg->gvm.ptr) free (arg->gvm.ptr);
	purge_xarg (&arg->icf);
	purge_xarg (&arg->ocf);
	if (isf) 
	{
		if (arg->incl_conv) free (isf[0].u.bcs.ptr);
		free (isf);
	}
	return oops_ret;
}

static void freearg (struct arg_t* arg)
{
	if (arg->psin) 
	{
		if (arg->incl_conv) free (arg->psin[0].u.bcs.ptr);
		free (arg->psin);
	}

	/*if (arg->osf != HAWK_NULL) free (arg->osf);*/
	purge_xarg (&arg->icf);
	purge_xarg (&arg->ocf);
	if (arg->gvm.ptr) free (arg->gvm.ptr);
}

static void print_hawk_error (hawk_t* hawk)
{
	const hawk_loc_t* loc = hawk_geterrloc(hawk);

	hawk_logfmt (hawk, HAWK_LOG_STDERR,
		HAWK_T("ERROR: CODE %d LINE %zu COLUMN %zu %js%js%js- %js\n"), 
		(int)hawk_geterrnum(hawk),
		(hawk_oow_t)loc->line,
		(hawk_oow_t)loc->colm,
		((loc->file == HAWK_NULL)? HAWK_T(""): HAWK_T("FILE ")),
		((loc->file == HAWK_NULL)? HAWK_T(""): loc->file),
		((loc->file == HAWK_NULL)? HAWK_T(""): HAWK_T(" ")),
		hawk_geterrmsg(hawk)
	);
}

static void print_hawk_rtx_error (hawk_rtx_t* rtx)
{
	const hawk_loc_t* loc = hawk_rtx_geterrloc(rtx);

	hawk_logfmt (hawk_rtx_gethawk(rtx), HAWK_LOG_STDERR,
		HAWK_T("ERROR: CODE %d LINE %zu COLUMN %zu %js%js%js- %js\n"), 
		(int)hawk_rtx_geterrnum(rtx),
		(hawk_oow_t)loc->line,
		(hawk_oow_t)loc->colm,
		((loc->file == HAWK_NULL)? HAWK_T(""): HAWK_T("FILE ")),
		((loc->file == HAWK_NULL)? HAWK_T(""): loc->file),
		((loc->file == HAWK_NULL)? HAWK_T(""): HAWK_T(" ")),
		hawk_rtx_geterrmsg(rtx)
	);
}

static void* xma_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	return hawk_xma_alloc(mmgr->ctx, size);
}

static void* xma_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	return hawk_xma_realloc(mmgr->ctx, ptr, size);
}

static void xma_free (hawk_mmgr_t* mmgr, void* ptr)
{
	hawk_xma_free (mmgr->ctx, ptr);
}

static hawk_mmgr_t xma_mmgr = 
{
	xma_alloc,
	xma_realloc,
	xma_free,
	HAWK_NULL
};

static void xma_dumper_without_hawk (void* ctx, const hawk_bch_t* fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap); 
	va_end (ap);
}

#if defined(HAWK_BUILD_DEBUG)
static hawk_uintptr_t debug_mmgr_count = 0;
static hawk_uintptr_t debug_mmgr_alloc_count = 0;
static hawk_uintptr_t debug_mmgr_realloc_count = 0;
static hawk_uintptr_t debug_mmgr_free_count = 0;

static void* debug_mmgr_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	void* ptr;
	struct arg_t* arg = (struct arg_t*)mmgr->ctx;
	debug_mmgr_count++;
	if (debug_mmgr_count % arg->failmalloc == 0) return HAWK_NULL;
	ptr = malloc (size);
	if (ptr) debug_mmgr_alloc_count++;
	return ptr;
}

static void* debug_mmgr_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	void* rptr;
	struct arg_t* arg = (struct arg_t*)mmgr->ctx;
	debug_mmgr_count++;
	if (debug_mmgr_count % arg->failmalloc == 0) return HAWK_NULL;
	rptr = realloc (ptr, size);
	if (rptr)
	{
		if (ptr) debug_mmgr_realloc_count++;
		else debug_mmgr_alloc_count++;
	}
	return rptr;
}

static void debug_mmgr_free (hawk_mmgr_t* mmgr, void* ptr)
{
	debug_mmgr_free_count++;
	free (ptr);
}

static hawk_mmgr_t debug_mmgr =
{
	debug_mmgr_alloc,
	debug_mmgr_realloc,
	debug_mmgr_free,
	HAWK_NULL
};
#endif

static HAWK_INLINE int execute_hawk (int argc, hawk_bch_t* argv[])
{
	hawk_t* hawk = HAWK_NULL;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_val_t* retv;
	int i;
	struct arg_t arg;
	int ret = -1;

#if defined(ENABLE_CALLBACK)
	static hawk_rtx_ecb_t rtx_ecb =
	{
		HAWK_FV(.close,  HAWK_NULL),
		HAWK_FV(.stmt,   on_statement),
		HAWK_FV(.gblset, HAWK_NULL)
		HAWK_FV(.ctx,    HAWK_NULL)
	};
#endif

	/* TODO: change it to support multiple source files */
	hawk_parsestd_t psout;
	hawk_mmgr_t* mmgr = hawk_get_sys_mmgr();

	i = process_argv(argc, argv, &arg);
	if (i <= 0)
	{
		print_usage (((i == 0)? stdout: stderr), argv[0]);
		return i;
	}
	if (i == 2) return 0;
	if (i == 3) return -1;

	if (arg.osf)
	{
		psout.type = HAWK_PARSESTD_FILEB;
		psout.u.fileb.path = arg.osf;
		psout.u.fileb.cmgr = arg.script_cmgr;
	}

#if defined(HAWK_BUILD_DEBUG)
	if (arg.failmalloc > 0)
	{
		debug_mmgr.ctx = &arg;
		mmgr = &debug_mmgr;
	}
	else 
#endif
	if (arg.memlimit > 0)
	{
		xma_mmgr.ctx = hawk_xma_open(hawk_get_sys_mmgr(), 0, HAWK_NULL, arg.memlimit);
		if (xma_mmgr.ctx == HAWK_NULL)
		{
			print_error ("cannot open memory heap\n");
			goto oops;
		}
		mmgr = &xma_mmgr;
	}

	hawk = hawk_openstdwithmmgr(mmgr, 0, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), HAWK_NULL);
	if (HAWK_UNLIKELY(!hawk))
	{
		print_error ("cannot open hawk\n");
		goto oops;
	}

	if (arg.modern) i = HAWK_MODERN;
	else if (arg.classic) i = HAWK_CLASSIC;
	else hawk_getopt (hawk, HAWK_OPT_TRAIT, &i);
	if (arg.opton) i |= arg.opton;
	if (arg.optoff) i &= ~arg.optoff;
	hawk_setopt (hawk, HAWK_OPT_TRAIT, &i);

	/* TODO: get depth from command line */
	{
		hawk_oow_t tmp;
		tmp = 50;
		hawk_setopt (hawk, HAWK_OPT_DEPTH_BLOCK_PARSE, &tmp);
		hawk_setopt (hawk, HAWK_OPT_DEPTH_EXPR_PARSE, &tmp);
		tmp = 500;
		hawk_setopt (hawk, HAWK_OPT_DEPTH_BLOCK_RUN, &tmp);
		hawk_setopt (hawk, HAWK_OPT_DEPTH_EXPR_RUN, &tmp);
		tmp = 64;
		hawk_setopt (hawk, HAWK_OPT_DEPTH_INCLUDE, &tmp);
	}

	if (arg.includedirs) 
	{
	#if defined(HAWK_OOCH_IS_UCH)
		hawk_ooch_t* tmp;
		tmp = hawk_dupbtoucstr(hawk, arg.includedirs, HAWK_NULL, 1);
		if (HAWK_UNLIKELY(!tmp))
		{
			print_hawk_error (hawk);
			goto oops;
		}

		hawk_setopt (hawk, HAWK_OPT_INCLUDEDIRS, tmp);
		hawk_freemem (hawk, tmp);
	#else
		hawk_setopt (hawk, HAWK_OPT_INCLUDEDIRS, arg.includedirs);
	#endif
	}

	if (arg.modlibdirs) 
	{
	#if defined(HAWK_OOCH_IS_UCH)
		hawk_ooch_t* tmp;
		tmp = hawk_dupbtoucstr(hawk, arg.modlibdirs, HAWK_NULL, 1);
		if (HAWK_UNLIKELY(!tmp))
		{
			print_hawk_error (hawk);
			goto oops;
		}

		hawk_setopt (hawk, HAWK_OPT_MODLIBDIRS, tmp);
		hawk_freemem (hawk, tmp);
	#else
		hawk_setopt (hawk, HAWK_OPT_MODLIBDIRS, arg.modlibdirs);
	#endif
	}

	if (add_gvs_to_hawk(hawk, &arg) <= -1)
	{
		print_hawk_error (hawk);
		goto oops;
	}

	if (hawk_parsestd(hawk, arg.psin, ((arg.osf == HAWK_NULL)? HAWK_NULL: &psout)) <= -1)
	{
		print_hawk_error (hawk);
		goto oops;
	}

	rtx = hawk_rtx_openstdwithbcstr(
		hawk, 0, argv[0],
		(arg.call? HAWK_NULL: arg.icf.ptr), /* console input */
		arg.ocf.ptr, /* console output */
		arg.console_cmgr
	);
	if (HAWK_UNLIKELY(!rtx))
	{
		print_hawk_error (hawk);
		goto oops;
	}

	if (apply_fs_and_gvs_to_rtx(rtx, &arg) <= -1)
	{
		print_hawk_rtx_error (rtx);
		goto oops;
	}
	
	app_rtx = rtx;
#if defined(ENABLE_CALLBACK)
	hawk_rtx_pushecb (rtx, &rtx_ecb);
#endif

	set_intr_run ();

#if 0
	retv = arg.call?
		hawk_rtx_callwithbcstrarr(rtx, arg.call, (const hawk_bch_t**)arg.icf.ptr, arg.icf.size):
		hawk_rtx_loop(rtx); /* this doesn't support @pragma startup ... */
#else
	/* note about @pragma entry ...
	 * hawk_rtx_execwithbcstrarr() invokes the specified function if '@pragma entry' is set 
	 * in the source code. because arg.icf.ptr has been passed to hawk_rtx_openstdwithbcstr() when
	 * arg.call is HAWK_NULL, arg.icf.ptr serves as parameters to the startup function and
	 * affects input consoles */
	retv = arg.call?
		hawk_rtx_callwithbcstrarr(rtx, arg.call, (const hawk_bch_t**)arg.icf.ptr, arg.icf.size):
		hawk_rtx_execwithbcstrarr(rtx, (const hawk_bch_t**)arg.icf.ptr, arg.icf.size);
#endif

	unset_intr_run ();

	if (retv)
	{
		hawk_int_t tmp;

		hawk_rtx_refdownval (rtx, retv);
		if (app_debug) dprint_return (rtx, retv);

		ret = 0;
		if (hawk_rtx_valtoint(rtx, retv, &tmp) >= 0) ret = tmp;
	}
	else
	{
		print_hawk_rtx_error (rtx);
		goto oops;
	}

oops:
	if (rtx) hawk_rtx_close (rtx);
	if (hawk) hawk_close (hawk);

	if (xma_mmgr.ctx) 
	{
		if (app_debug) hawk_xma_dump (xma_mmgr.ctx, xma_dumper_without_hawk, HAWK_NULL);
		hawk_xma_close (xma_mmgr.ctx);
	}
	freearg (&arg);

#if defined(HAWK_BUILD_DEBUG)
	/*
	if (arg.failmalloc > 0)
	{
		hawk_fprintf (HAWK_STDERR, HAWK_T("\n"));
		hawk_fprintf (HAWK_STDERR, HAWK_T("-[MALLOC COUNTS]---------------------------------------\n"));
		hawk_fprintf (HAWK_STDERR, HAWK_T("ALLOC: %lu FREE: %lu: REALLOC: %lu\n"), 
			(unsigned long)debug_mmgr_alloc_count,
			(unsigned long)debug_mmgr_free_count,
			(unsigned long)debug_mmgr_realloc_count);
		hawk_fprintf (HAWK_STDERR, HAWK_T("-------------------------------------------------------\n"));
	}*/
#endif

	return ret;
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
