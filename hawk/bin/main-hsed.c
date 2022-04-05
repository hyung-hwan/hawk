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

#include <hawk-sed.h>
#include <hawk-xma.h>
#include <hawk-cli.h>
#include <hawk-fmt.h>
#include <hawk-utl.h>
#include <hawk-std.h>

#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static struct
{
	hawk_sed_iostd_t* io;
	hawk_oow_t capa;
	hawk_oow_t size;
} g_script = 
{
	HAWK_NULL,
	0,
	0
};


static hawk_sed_t* g_sed = HAWK_NULL;
static hawk_cmgr_t* g_script_cmgr = HAWK_NULL;
static hawk_cmgr_t* g_infile_cmgr = HAWK_NULL;
static hawk_cmgr_t* g_outfile_cmgr = HAWK_NULL;

/* ------------------------------------------------------------------- */

struct arg_t
{
	hawk_bch_t* output_file;
	int infile_pos;
	int option;
	int separate;
	int inplace;
	int wildcard;

#if defined(HAWK_ENABLE_SEDTRACER)
	int trace;
#endif

	hawk_uintptr_t  memlimit;
#if defined(HAWK_BUILD_DEBUG)
	hawk_uintptr_t  failmalloc;
#endif
};

/* ------------------------------------------------------------------- */
static void* xma_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	return hawk_xma_alloc (mmgr->ctx, size);
}

static void* xma_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	return hawk_xma_realloc (mmgr->ctx, ptr, size);
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

/* ------------------------------------------------------------------- */

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
	ptr = malloc(size);
	if (ptr) debug_mmgr_alloc_count++;
	return ptr;
}

static void* debug_mmgr_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	void* rptr;
	struct arg_t* arg = (struct arg_t*)mmgr->ctx;
	debug_mmgr_count++;
	if (debug_mmgr_count % arg->failmalloc == 0) return HAWK_NULL;
	rptr = realloc(ptr, size);
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

/* ------------------------------------------------------------------- */

static void print_version (const hawk_bch_t* argv0)
{
	fprintf (stdout, "%s (HAWK SED) %s\n", hawk_get_base_name_bcstr(argv0), HAWK_PACKAGE_VERSION);
	fprintf (stdout, "Copyright 2006-2022 Chung, Hyung-Hwan\n");
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

static void print_usage (FILE* out, int argc, hawk_bch_t* argv[])
{
	const hawk_bch_t* b = hawk_get_base_name_bcstr(argv[0]);

	fprintf (out, "USAGE: %s [options] script [file]\n", b);
	fprintf (out, "       %s [options] -f script-file [file]\n", b);
	fprintf (out, "       %s [options] -e script [file]\n", b);

	fprintf (out, "options as follows:\n");
	fprintf (out, " -h/--help                 show this message\n");
	fprintf (out, " --version                 show version\n");
	fprintf (out, " -n                        disable auto-print\n");
	fprintf (out, " -e                 script specify a script\n");
	fprintf (out, " -f                 file   specify a script file\n");
	fprintf (out, " -o                 file   specify an output file\n");
	fprintf (out, " -r                        use the extended regular expression\n");
	fprintf (out, " -R                        enable non-standard extensions to the regular\n");
	fprintf (out, "                           expression\n");
	fprintf (out, " -i                        perform in-place editing. imply -s\n");
	fprintf (out, " -s                        process input files separately\n");
	fprintf (out, " -a                        perform strict address and label check\n");
	fprintf (out, " -b                        allow extended address formats\n");
	fprintf (out, "                           <start~step>,<start,+line>,<start,~line>,<0,/regex/>\n");
	fprintf (out, " -x                        allow text on the same line as c, a, i\n");
	fprintf (out, " -y                        ensure a newline at text end\n");
	fprintf (out, " -m                 number specify the maximum amount of memory to use in bytes\n");
	fprintf (out, " -w                        expand file wildcards\n");
#if defined(HAWK_ENABLE_SEDTRACER)
	fprintf (out, " -t                        print command traces\n");
#endif
#if defined(HAWK_BUILD_DEBUG)
	fprintf (out, " -X                 number fail the number'th memory allocation\n");
#endif
#if defined(HAWK_OOCH_IS_UCH)
	fprintf (out, " --script-encoding  string specify script file encoding name\n");
	fprintf (out, " --infile-encoding  string specify input file encoding name\n");
	fprintf (out, " --outfile-encoding string specify output file encoding name\n");
#endif
}

static int add_script (const hawk_bch_t* str, int mem)
{
	if (g_script.size >= g_script.capa)
	{
		hawk_sed_iostd_t* tmp;

		tmp = (hawk_sed_iostd_t*)realloc(g_script.io, HAWK_SIZEOF(*g_script.io) * (g_script.capa + 16 + 1));
		if (tmp == HAWK_NULL) 
		{
			print_error ("out of memory while processing %s\n", str);
			return -1;
		}

		g_script.io = tmp;
		g_script.capa += 16;
	} 

	if (mem)
	{
		/* string */
		g_script.io[g_script.size].type = HAWK_SED_IOSTD_BCS;
		/* though its type is not qualified to be const, 
		 * u.mem.ptr is actually const when used for input */
		g_script.io[g_script.size].u.bcs.ptr = (hawk_bch_t*)str;
		g_script.io[g_script.size].u.bcs.len = hawk_count_bcstr(str);
	}
	else
	{
		/* file name */
		g_script.io[g_script.size].type = HAWK_SED_IOSTD_FILEB;
		g_script.io[g_script.size].u.fileb.path = str;
		g_script.io[g_script.size].u.fileb.cmgr = g_script_cmgr;
	}
	g_script.size++;
	return 0;
}

static void free_scripts (void)
{
	if (g_script.io)
	{
		free (g_script.io);
		g_script.io = HAWK_NULL;
		g_script.capa = 0;
		g_script.size = 0;
	}
}

static int handle_args (int argc, hawk_bch_t* argv[], struct arg_t* arg)
{
	static hawk_bcli_lng_t lng[] = 
	{
#if defined(HAWK_OOCH_IS_UCH)
		{ ":script-encoding",  '\0' },
		{ ":infile-encoding",  '\0' },
		{ ":outfile-encoding", '\0' },
#endif

		{ "version",           '\0' },
		{ "help",              'h' },
		{ HAWK_NULL,           '\0' }                  
	};
	static hawk_bcli_t opt = 
	{
#if defined(HAWK_BUILD_DEBUG)
		"hne:f:o:rRisabxytm:wX:",
#else
		"hne:f:o:rRisabxytm:w",
#endif
		lng
	};
	hawk_bci_t c;

	while ((c = hawk_get_bcli(argc, argv, &opt)) != HAWK_BCI_EOF)
	{
		switch (c)
		{
			default:
				print_usage (stderr, argc, argv);
				goto oops;

			case '?':
				print_error ("bad option - %c\n", opt.opt);
				print_usage (stderr, argc, argv);
				goto oops;

			case ':':
				print_error ("bad parameter for %c\n", opt.opt);
				print_usage (stderr, argc, argv);
				goto oops;

			case 'h':
				print_usage (stdout, argc, argv);
				goto done;

			case 'n':
				arg->option |= HAWK_SED_QUIET;
				break;

			case 'e':
				if (add_script(opt.arg, 1) <= -1) goto oops;
				break;

			case 'f':
				if (add_script(opt.arg, 0) <= -1) goto oops;
				break;

			case 'o':
				arg->output_file = opt.arg;
				break;

			case 'r':
				arg->option |= HAWK_SED_EXTENDEDREX;
				break;

			case 'R':
				arg->option |= HAWK_SED_NONSTDEXTREX;
				break;

			case 'i':
				/* 'i' implies 's'. */
				arg->inplace = 1;

			case 's':
				arg->separate = 1;
				break;

			case 'a':
				arg->option |= HAWK_SED_STRICT;
				break;

			case 'b':
				arg->option |= HAWK_SED_EXTENDEDADR;
				break;

			case 'x':
				arg->option |= HAWK_SED_SAMELINE;
				break;

			case 'y':
				arg->option |= HAWK_SED_ENSURENL;
				break;

			case 't':
			#if defined(HAWK_ENABLE_SEDTRACER)
				arg->trace = 1;
				break;
			#else
				print_usage (stderr, argc, argv);
				goto oops;
			#endif
 
			case 'm':
				arg->memlimit = strtoul(opt.arg, HAWK_NULL, 10);
				break;

			case 'w':
				arg->wildcard = 1;
				break;

			#if defined(HAWK_BUILD_DEBUG)
			case 'X':
				arg->failmalloc = strtoul(opt.arg, HAWK_NULL, 10);
				break;
			#endif

			case '\0':
			{
				if (hawk_comp_bcstr(opt.lngopt, "version", 0) == 0)
				{
					print_version (argv[0]);
					goto done;
				}
				else if (hawk_comp_bcstr(opt.lngopt, "script-encoding", 0) == 0)
				{
					g_script_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (g_script_cmgr == HAWK_NULL)
					{
						print_error ("unknown script encoding - %s\n", opt.arg);
						goto oops;
					}
				}
				else if (hawk_comp_bcstr(opt.lngopt, "infile-encoding", 0) == 0)
				{
					g_infile_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (g_infile_cmgr == HAWK_NULL)
					{
						print_error ("unknown input file encoding - %s\n", opt.arg);
						goto oops;
					}
				}
				else if (hawk_comp_bcstr(opt.lngopt, "outfile-encoding", 0) == 0)
				{
					g_outfile_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (g_outfile_cmgr == HAWK_NULL)
					{
						print_error ("unknown output file encoding - %s\n", opt.arg);
						goto oops;
					}
				}
				break;
			}

		}
	}

	if (opt.ind < argc && g_script.size <= 0) 
	{
		if (add_script(argv[opt.ind++], 1) <= -1) goto oops;
	}
	if (opt.ind < argc) arg->infile_pos = opt.ind;

	if (g_script.size <= 0)
	{
		print_usage (stderr, argc, argv);
		goto oops;
	}

	g_script.io[g_script.size].type = HAWK_SED_IOSTD_NULL;
	return 1;

oops:
	free_scripts ();
	return -1;

done:
	free_scripts ();
	return 0;
}

void print_exec_error (hawk_sed_t* sed)
{
	const hawk_loc_t* errloc = hawk_sed_geterrloc(sed);
	if (errloc->line > 0 || errloc->colm > 0)
	{
		print_error ("cannot execute - %s at line %lu column %lu\n",
			hawk_sed_geterrbmsg(sed), (unsigned long)errloc->line, (unsigned long)errloc->colm);
	}
	else
	{
		print_error ("cannot execute - %s\n", hawk_sed_geterrbmsg(sed));
	}
}

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

		memset (&sa, 0, HAWK_SIZEOF(sa));
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
	memset (&sa, 0, HAWK_SIZEOF(sa));
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

	hawk_sed_halt (g_sed);

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


#if defined(HAWK_ENABLE_SEDTRACER)
static void trace_exec (hawk_sed_t* sed, hawk_sed_tracer_op_t op, const hawk_sed_cmd_t* cmd)
{
	switch (op)
	{
		case HAWK_SED_TRACER_READ:
			/*hawk_fprintf (HAWK_STDERR, HAWK_T("reading...\n"));*/
			break;
		case HAWK_SED_TRACER_WRITE:
			/*hawk_fprintf (HAWK_STDERR, HAWK_T("wrting...\n"));*/
			break;

		/* TODO: use function to get hold space and pattern space and print them */

		case HAWK_SED_TRACER_MATCH:
			hawk_fprintf (HAWK_STDERR, HAWK_T("%s:%lu [%c] MA\n"), 
				((cmd->lid && cmd->lid[0])? cmd->lid: HAWK_T("<<UNKNOWN>>")), 
				(unsigned long)cmd->loc.line,
				cmd->type
			);
			break;

		case HAWK_SED_TRACER_EXEC:
			hawk_fprintf (HAWK_STDERR, HAWK_T("%s:%lu [%c] EC\n"), 
				((cmd->lid && cmd->lid[0])? cmd->lid: HAWK_T("<<UNKNOWN>>")), 
				(unsigned long)cmd->loc.line,
				cmd->type
			);
			break;
	}
}
#endif

struct xarg_t
{
	hawk_mmgr_t*  mmgr;
	hawk_bch_t** ptr;
	hawk_oow_t   size;
	hawk_oow_t   capa;
};

typedef struct xarg_t xarg_t;

static int collect_into_xarg (const hawk_bcs_t* path, void* ctx)
{
	xarg_t* xarg = (xarg_t*)ctx;

	if (xarg->size <= xarg->capa)
	{
		hawk_bch_t** tmp;

		tmp = realloc(xarg->ptr, HAWK_SIZEOF(*tmp) * (xarg->capa + 128));
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

static int expand_wildcards (int argc, hawk_bch_t* argv[], int glob, xarg_t* xarg)
{
	int i;
	hawk_bcs_t tmp;

	for (i = 0; i < argc; i++)
	{
		int x;

#if 0
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

	return 0;
}

static HAWK_INLINE int execute_hawk_sed (int argc, hawk_bch_t* argv[])
{
	hawk_sed_t* sed = HAWK_NULL;
	hawk_oow_t script_count;
	int ret = -1;
	struct arg_t arg;
	xarg_t xarg;
	int xarg_inited = 0;
	hawk_mmgr_t* mmgr = hawk_get_sys_mmgr();
	hawk_cmgr_t* cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	memset (&arg, 0, HAWK_SIZEOF(arg));
	ret = handle_args(argc, argv, &arg);
	if (ret <= -1) return -1;
	if (ret == 0) return 0;

	ret = -1;

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


	sed = hawk_sed_openstdwithmmgr(mmgr, 0, cmgr, HAWK_NULL);
	if (!sed)
	{
		print_error ("cannot open stream editor\n");
		goto oops;
	}

	hawk_sed_setopt (sed, HAWK_SED_TRAIT, &arg.option);

	if (hawk_sed_compstd(sed, g_script.io, &script_count) <= -1)
	{
		const hawk_loc_t* errloc;
		const hawk_bch_t* target;
		hawk_bch_t exprbuf[128];

		errloc = hawk_sed_geterrloc(sed);
	
		if (g_script.io[script_count].type == HAWK_SED_IOSTD_FILEB)
		{
			target = g_script.io[script_count].u.fileb.path;
		}
		else 
		{
			/* i dont' use HAWK_SED_IOSTD_SIO for input */
			HAWK_ASSERT (g_script.io[script_count].type == HAWK_SED_IOSTD_BCS);
			hawk_fmt_uintmax_to_bcstr (exprbuf, HAWK_COUNTOF(exprbuf),
				script_count, 10, -1, '\0', "expression #");
			target = exprbuf;
		}

		if (errloc->line > 0 || errloc->colm > 0)
		{
			print_error ("cannot compile %s - %s at line %lu column %lu\n",
				target, hawk_sed_geterrbmsg(sed), (unsigned long)errloc->line, (unsigned long)errloc->colm
			);
		}
		else
		{
			print_error ("cannot compile %s - %s\n", target, hawk_sed_geterrbmsg(sed));
		}
		goto oops;
	}

#if defined(HAWK_ENABLE_SEDTRACER)
	if (g_trace) hawk_sed_setopt (sed, HAWK_SED_TRACER, trace_exec);
#endif

	memset (&xarg, 0, HAWK_SIZEOF(xarg));
	xarg.mmgr = hawk_sed_getmmgr(sed);
	xarg_inited = 1;

	if (arg.separate && arg.infile_pos > 0)
	{
		/* 's' and input files are specified on the command line */
		hawk_sed_iostd_t out_file;
		hawk_sed_iostd_t out_inplace;
		hawk_sed_iostd_t* output_file = HAWK_NULL;
		hawk_sed_iostd_t* output = HAWK_NULL;
		int inpos;

		/* a dash is treated specially for HAWK_SED_IOSTD_FILE in
		 * hawk_sed_execstd(). so make an exception here */
		if (arg.output_file && hawk_comp_bcstr(arg.output_file, "-", 0) != 0)
		{
			out_file.type = HAWK_SED_IOSTD_SIO;
			out_file.u.sio = hawk_sio_open(
				hawk_sed_getgem(sed),
				0,
				arg.output_file, /* ignore the type mistch error here */
				HAWK_SIO_WRITE |
				HAWK_SIO_CREATE |
				HAWK_SIO_TRUNCATE |
				HAWK_SIO_IGNOREECERR |
				HAWK_SIO_BCSTRPATH
			);
			if (out_file.u.sio == HAWK_NULL)
			{
				print_error ("cannot open %s\n", arg.output_file);
				goto oops;
			}

			output_file = &out_file;
			output = output_file;
		}

		/* perform wild-card expansions for non-unix platforms */
		if (expand_wildcards(argc - arg.infile_pos, &argv[arg.infile_pos], arg.wildcard, &xarg) <= -1)
		{
			print_error ("out of memory\n");
			goto oops;
		}

		for (inpos = 0; inpos < xarg.size; inpos++)
		{
			hawk_sed_iostd_t in[2];
			hawk_bch_t* tmpl_tmpfile;
			
			in[0].type = HAWK_SED_IOSTD_FILEB;
			in[0].u.fileb.path = xarg.ptr[inpos];
			in[0].u.fileb.cmgr = g_infile_cmgr;
			in[1].type = HAWK_SED_IOSTD_NULL;

			tmpl_tmpfile = HAWK_NULL;
			if (arg.inplace && in[0].u.file.path)
			{
				int retried = 0;
				const hawk_bch_t* f[3];

				f[0] = in[0].u.fileb.path;
				f[1] = ".XXXX";
				f[2] = HAWK_NULL;

				tmpl_tmpfile = hawk_gem_dupbcstrarr(hawk_sed_getgem(sed), f, HAWK_NULL);
				if (tmpl_tmpfile == HAWK_NULL)
				{
					print_error ("out of memory\n");
					goto oops;
				}

			open_temp:
				out_inplace.type = HAWK_SED_IOSTD_SIO;
				out_inplace.u.sio = hawk_sio_open(
					hawk_sed_getgem(sed),
					0,
					tmpl_tmpfile, /* expect a type mismatch warning here and note HAWK_SIO_BCSTR_PATH below */
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_IGNOREECERR |
					HAWK_SIO_EXCLUSIVE |
					HAWK_SIO_TEMPORARY |
					HAWK_SIO_BCSTRPATH
				);
				if (out_inplace.u.sio == HAWK_NULL)
				{
					if (retried) 
					{
						print_error ("cannot open %s\n", tmpl_tmpfile);
						hawk_sed_freemem (sed, tmpl_tmpfile);
						goto oops;
					}
					else
					{
						/* retry to open the file with shorter names */
						hawk_sed_freemem (sed, tmpl_tmpfile);
						tmpl_tmpfile = hawk_gem_dupbcstr(hawk_sed_getgem(sed), "TMP-XXXX", HAWK_NULL);
						if (tmpl_tmpfile == HAWK_NULL)
						{
							print_error ("out of memory\n");
							goto oops;
						}
						retried = 1;
						goto open_temp;
					}
				}

				output = &out_inplace;
			}

			if (hawk_sed_execstd(sed, in, output) <= -1)
			{
				if (output) hawk_sio_close (output->u.sio);

				if (tmpl_tmpfile) 
				{
					unlink (tmpl_tmpfile);
					hawk_sed_freemem (sed, tmpl_tmpfile);
				}
				print_exec_error (sed);
				goto oops;
			}

			if (tmpl_tmpfile)
			{
				HAWK_ASSERT (output == &out_inplace);
				hawk_sio_close (output->u.sio);
				output = output_file;

				if (rename(tmpl_tmpfile, in[0].u.fileb.path) <= -1)
				{
					print_error ("cannot rename %s to %s. not deleting %s\n",
						tmpl_tmpfile, in[0].u.fileb.path, tmpl_tmpfile);
					hawk_sed_freemem (sed, tmpl_tmpfile);
					goto oops;
				}

				hawk_sed_freemem (sed, tmpl_tmpfile);
			}

			if (hawk_sed_ishalt(sed)) break;
		}

		if (output) hawk_sio_close (output->u.sio);
	}
	else
	{
		int xx;
		hawk_sed_iostd_t* in = HAWK_NULL;
		hawk_sed_iostd_t out;

		if (arg.infile_pos > 0)
		{
			int i;
			const hawk_bch_t* tmp;

			/* input files are specified on the command line */

			/* perform wild-card expansions for non-unix platforms */
			if (expand_wildcards (argc - arg.infile_pos, &argv[arg.infile_pos], arg.wildcard, &xarg) <= -1)
			{
				print_error ("out of memory\n");
				goto oops;
			}

			in = (hawk_sed_iostd_t*)hawk_sed_allocmem(sed, HAWK_SIZEOF(*in) * (xarg.size + 1));
			if (in == HAWK_NULL)
			{
				print_error ("out of memory\n");
				goto oops;
			}

			for (i = 0; i < xarg.size; i++)
			{
				in[i].type = HAWK_SED_IOSTD_FILEB;
				tmp = xarg.ptr[i];
				in[i].u.fileb.path = tmp;
				in[i].u.fileb.cmgr = g_infile_cmgr;
			}

			in[i].type = HAWK_SED_IOSTD_NULL;
		}

		if (arg.output_file)
		{
			out.type = HAWK_SED_IOSTD_FILEB;
			out.u.fileb.path = arg.output_file;
			out.u.fileb.cmgr = g_outfile_cmgr;
		}
		else
		{
			/* arrange to be able to specify cmgr.
			 * if not for cmgr, i could simply pass HAWK_NULL 
			 * to hawk_sed_execstd() below like
			 *   xx = hawk_sed_execstd (sed, in, HAWK_NULL); */
			out.type = HAWK_SED_IOSTD_FILEB;
			out.u.fileb.path = HAWK_NULL;
			out.u.fileb.cmgr = g_outfile_cmgr;
		}

		g_sed = sed;
		set_intr_run ();
		xx = hawk_sed_execstd(sed, in, &out);
		if (in) hawk_sed_freemem (sed, in);
		unset_intr_run ();
		g_sed = HAWK_NULL;

		if (xx <= -1)
		{
			print_exec_error (sed);
			goto oops;
		}
	}

	ret = 0;

oops:
	if (xarg_inited) purge_xarg (&xarg);
	if (sed) hawk_sed_close (sed);
	if (xma_mmgr.ctx) hawk_xma_close (xma_mmgr.ctx);
	free_scripts ();

#if defined(HAWK_BUILD_DEBUG)
	if (arg.failmalloc > 0)
	{
		fprintf (stderr, "\n");
		fprintf (stderr, "-[MALLOC COUNTS]---------------------------------------\n");
		fprintf (stderr, "ALLOC: %lu FREE: %lu: REALLOC: %lu\n",
			(unsigned long)debug_mmgr_alloc_count,
			(unsigned long)debug_mmgr_free_count,
			(unsigned long)debug_mmgr_realloc_count);
		fprintf (stderr, "-------------------------------------------------------\n");
	}
#endif
	return ret;
}


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

	ret = execute_hawk_sed(argc, argv);

#if defined(_WIN32)
	if (sock_inited) WSACleanup ();
#elif defined(__DOS__)
	if (sock_inited) sock_exit ();
#endif

	return ret;
}
