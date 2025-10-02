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

#include <hawk-cut.h>
#include <hawk-cli.h>
#include <hawk-fmt.h>
#include <hawk-utl.h>
#include <hawk-xma.h>

#if !defined(_GNU_SOURCE)
#	define _GNU_SOURCE
#endif
#if !defined(_XOPEN_SOURCE)
#	define _XOPEN_SOURCE 700
#endif

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
	hawk_cut_iostd_t* io;
	hawk_oow_t capa;
	hawk_oow_t size;
} g_script =
{
	HAWK_NULL,
	0,
	0
};


static hawk_cut_t* g_cut = HAWK_NULL;
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
	int debug;
	hawk_uintptr_t  memlimit;
};

/* ------------------------------------------------------------------- */

static void print_usage (FILE* out, const hawk_bch_t* argv0, const hawk_bch_t* real_argv0)
{
	const hawk_bch_t* b1 = hawk_get_base_name_bcstr(real_argv0? real_argv0: argv0);
	const hawk_bch_t* b2 = (real_argv0? " ": "");
	const hawk_bch_t* b3 = (real_argv0? argv0: "");

	fprintf (out, "USAGE: %s%s%s [options] selector [file]\n", b1, b2, b3);
	fprintf (out, "       %s%s%s [options] -f selector-file [file]\n", b1, b2, b3);

	fprintf (out, "Selector as follows:\n");
	fprintf (out, " 'd' followed by a delimiter character\n");
	fprintf (out, " 'D' followed by an input delimiter character and an output delimiter character\n");
	fprintf (out, " 'f' followed by a field number or a field number range\n");
	fprintf (out, " 'c' followed by a character position or a character position range\n");
	fprintf (out, " for example, 'f3-5 c1-2 f4 D:,'\n");
	fprintf (out, "Options as follows:\n");
	fprintf (out, " -h/--help                  show this message\n");
	fprintf (out, " -D                         show extra information\n");
	fprintf (out, " -e                 script  specify a script\n");
	fprintf (out, " -f                 file    specify a script file\n");
	fprintf (out, " -o                 file    specify an output file\n");
	fprintf (out, " -m/--memory-limit number   specify the maximum amount of memory to use in bytes\n");
	fprintf (out, " -w                         expand file wildcards\n");
#if defined(HAWK_OOCH_IS_UCH)
	fprintf (out, " --script-encoding  string  specify script file encoding name\n");
	fprintf (out, " --infile-encoding  string  specify input file encoding name\n");
	fprintf (out, " --outfile-encoding string  specify output file encoding name\n");
#endif
}

static int add_script (const hawk_bch_t* str, int mem)
{
	if (g_script.size >= g_script.capa)
	{
		hawk_cut_iostd_t* tmp;

		tmp = (hawk_cut_iostd_t*)realloc(g_script.io, HAWK_SIZEOF(*g_script.io) * (g_script.capa + 16 + 1));
		if (tmp == HAWK_NULL)
		{
			hawk_main_print_error("out of memory while processing %s\n", str);
			return -1;
		}

		g_script.io = tmp;
		g_script.capa += 16;
	}

	if (mem)
	{
		/* string */
		g_script.io[g_script.size].type = HAWK_CUT_IOSTD_BCS;
		/* though its type is not qualified to be const,
		 * u.mem.ptr is actually const when ucut for input */
		g_script.io[g_script.size].u.bcs.ptr = (hawk_bch_t*)str;
		g_script.io[g_script.size].u.bcs.len = hawk_count_bcstr(str);
	}
	else
	{
		/* file name */
		g_script.io[g_script.size].type = HAWK_CUT_IOSTD_FILEB;
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

static int handle_args (int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0, struct arg_t* arg)
{
	static hawk_bcli_lng_t lng[] =
	{
#if defined(HAWK_OOCH_IS_UCH)
		{ ":script-encoding",  '\0' },
		{ ":infile-encoding",  '\0' },
		{ ":outfile-encoding", '\0' },
#endif
		{ ":memory-limit",     'm' },
		{ "help",              'h' },
		{ HAWK_NULL,           '\0' }
	};
	static hawk_bcli_t opt =
	{
		"hDe:f:o:m:w",
		lng
	};
	hawk_bci_t c;

	while ((c = hawk_get_bcli(argc, argv, &opt)) != HAWK_BCI_EOF)
	{
		switch (c)
		{
			default:
				print_usage (stderr, argv[0], real_argv0);
				goto oops;

			case '?':
				hawk_main_print_error("bad option - %c\n", opt.opt);
				print_usage (stderr, argv[0], real_argv0);
				goto oops;

			case ':':
				hawk_main_print_error("bad parameter for %c\n", opt.opt);
				print_usage (stderr, argv[0], real_argv0);
				goto oops;

			case 'h':
				print_usage (stdout, argv[0], real_argv0);
				goto done;

			case 'D':
				arg->debug = 1;
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

			case 'm':
				arg->memlimit = strtoul(opt.arg, HAWK_NULL, 10);
				break;

			case 'w':
				arg->wildcard = 1;
				break;

			case '\0':
			{
				if (hawk_comp_bcstr(opt.lngopt, "script-encoding", 0) == 0)
				{
					g_script_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (g_script_cmgr == HAWK_NULL)
					{
						hawk_main_print_error("unknown script encoding - %s\n", opt.arg);
						goto oops;
					}
				}
				else if (hawk_comp_bcstr(opt.lngopt, "infile-encoding", 0) == 0)
				{
					g_infile_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (g_infile_cmgr == HAWK_NULL)
					{
						hawk_main_print_error("unknown input file encoding - %s\n", opt.arg);
						goto oops;
					}
				}
				else if (hawk_comp_bcstr(opt.lngopt, "outfile-encoding", 0) == 0)
				{
					g_outfile_cmgr = hawk_get_cmgr_by_bcstr(opt.arg);
					if (g_outfile_cmgr == HAWK_NULL)
					{
						hawk_main_print_error("unknown output file encoding - %s\n", opt.arg);
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
		print_usage (stderr, argv[0], real_argv0);
		goto oops;
	}

	g_script.io[g_script.size].type = HAWK_CUT_IOSTD_NULL;
	return 1;

oops:
	free_scripts ();
	return -1;

done:
	free_scripts ();
	return 0;
}

static void print_exec_error (hawk_cut_t* cut)
{
	const hawk_loc_t* errloc = hawk_cut_geterrloc(cut);
	if (errloc->line > 0 || errloc->colm > 0)
	{
		hawk_main_print_error("cannot execute - %s at line %lu column %lu\n",
			hawk_cut_geterrbmsg(cut), (unsigned long)errloc->line, (unsigned long)errloc->colm);
	}
	else
	{
		hawk_main_print_error("cannot execute - %s\n", hawk_cut_geterrbmsg(cut));
	}
}

/* ---------------------------------------------------------------------- */

static void stop_run (int signo)
{
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
	int e = errno;
#endif

	// TODO: hawk_cut_halt(g_cut);

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
	errno = e;
#endif
}

static void set_intr_run (void)
{
#if defined(SIGTERM)
	hawk_main_set_signal_handler(SIGTERM, stop_run, 0);
#endif
#if defined(SIGHUP)
	hawk_main_set_signal_handler(SIGHUP, stop_run, 0);
#endif
#if defined(SIGINT)
	hawk_main_set_signal_handler(SIGINT, stop_run, 0);
#endif
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(SIGPIPE)
	hawk_main_set_signal_handler(SIGPIPE, hawk_main_do_nothing_on_signal, 0);
#endif
}

static void unset_intr_run (void)
{
#if defined(SIGTERM)
	hawk_main_unset_signal_handler(SIGTERM);
#endif
#if defined(SIGHUP)
	hawk_main_unset_signal_handler(SIGHUP);
#endif
#if defined(SIGINT)
	hawk_main_unset_signal_handler(SIGINT);
#endif
#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(SIGPIPE)
	hawk_main_unset_signal_handler(SIGPIPE);
#endif
}

int main_cut(int argc, hawk_bch_t* argv[], const hawk_bch_t* real_argv0)
{
	hawk_cut_t* cut = HAWK_NULL;
	hawk_oow_t script_count;
	int ret = -1;
	struct arg_t arg;
	hawk_main_xarg_t xarg;
	int xarg_inited = 0;
	hawk_mmgr_t* mmgr = hawk_get_sys_mmgr();
	hawk_cmgr_t* cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);
	hawk_mmgr_t xma_mmgr;
	hawk_errinf_t errinf;

	memset (&arg, 0, HAWK_SIZEOF(arg));
	ret = handle_args(argc, argv, real_argv0, &arg);
	if (ret <= -1) return -1;
	if (ret == 0) return 0;

	ret = -1;

	if (arg.memlimit > 0)
	{
		if (hawk_init_xma_mmgr(&xma_mmgr, arg.memlimit) <= -1)
		{
			hawk_main_print_error("cannot open memory heap\n");
			goto oops;
		}
		mmgr = &xma_mmgr;
	}


	cut = hawk_cut_openstdwithmmgr(mmgr, 0, cmgr, &errinf);
	if (!cut)
	{
		const hawk_bch_t* msg;
	#if defined(HAWK_OOCH_IS_UCH)
		hawk_bch_t msgbuf[HAWK_ERRMSG_CAPA];
		hawk_oow_t msglen, wcslen;
		msglen = HAWK_COUNTOF(msgbuf);
		hawk_conv_ucstr_to_bcstr_with_cmgr(errinf.msg, &wcslen, msgbuf, &msglen, cmgr);
		msg = msgbuf;
	#else
		msg = errinf.msg;
	#endif
		hawk_main_print_error("cannot open text cutter - %s\n", msg);
		goto oops;
	}

//TODO	hawk_cut_setopt(cut, HAWK_CUT_TRAIT, &arg.option);

	if (hawk_cut_compstd(cut, g_script.io, &script_count) <= -1)
	{
		const hawk_loc_t* errloc;
		const hawk_bch_t* target;
		hawk_bch_t exprbuf[128];

		errloc = hawk_cut_geterrloc(cut);

		if (g_script.io[script_count].type == HAWK_CUT_IOSTD_FILEB)
		{
			target = g_script.io[script_count].u.fileb.path;
		}
		else
		{
			/* i dont' use HAWK_CUT_IOSTD_SIO for input */
			HAWK_ASSERT(g_script.io[script_count].type == HAWK_CUT_IOSTD_BCS);
			hawk_fmt_uintmax_to_bcstr(exprbuf, HAWK_COUNTOF(exprbuf),
				script_count, 10, -1, '\0', "expression #");
			target = exprbuf;
		}

		if (errloc->line > 0 || errloc->colm > 0)
		{
			hawk_main_print_error("cannot compile %s - %s at line %lu column %lu\n",
				target, hawk_cut_geterrbmsg(cut), (unsigned long)errloc->line, (unsigned long)errloc->colm
			);
		}
		else
		{
			hawk_main_print_error("cannot compile %s - %s\n", target, hawk_cut_geterrbmsg(cut));
		}
		goto oops;
	}

	memset(&xarg, 0, HAWK_SIZEOF(xarg));
	xarg_inited = 1;

	if (arg.separate && arg.infile_pos > 0)
	{
		/* 's' and input files are specified on the command line */
		hawk_cut_iostd_t out_file;
		hawk_cut_iostd_t out_inplace;
		hawk_cut_iostd_t* output_file = HAWK_NULL;
		hawk_cut_iostd_t* output = HAWK_NULL;
		int inpos;

		/* a dash is treated specially for HAWK_CUT_IOSTD_FILE in
		 * hawk_cut_execstd(). so make an exception here */
		if (arg.output_file && hawk_comp_bcstr(arg.output_file, "-", 0) != 0)
		{
			out_file.type = HAWK_CUT_IOSTD_SIO;
			out_file.u.sio = hawk_sio_open(
				hawk_cut_getgem(cut),
				0,
				(const hawk_ooch_t*)arg.output_file, /* fake type casting - note HAWK_SIO_BCSTRPATH below */
				HAWK_SIO_WRITE |
				HAWK_SIO_CREATE |
				HAWK_SIO_TRUNCATE |
				HAWK_SIO_IGNOREECERR |
				HAWK_SIO_BCSTRPATH
			);
			if (out_file.u.sio == HAWK_NULL)
			{
				hawk_main_print_error("cannot open %s\n", arg.output_file);
				goto oops;
			}

			output_file = &out_file;
			output = output_file;
		}

		/* perform wild-card expansions for non-unix platforms */
		if (hawk_main_expand_wildcard(argc - arg.infile_pos, &argv[arg.infile_pos], arg.wildcard, &xarg) <= -1)
		{
			hawk_main_print_error("out of memory\n");
			goto oops;
		}

		for (inpos = 0; inpos < xarg.size; inpos++)
		{
			hawk_cut_iostd_t in[2];
			hawk_bch_t* tmpl_tmpfile;

			in[0].type = HAWK_CUT_IOSTD_FILEB;
			in[0].u.fileb.path = xarg.ptr[inpos];
			in[0].u.fileb.cmgr = g_infile_cmgr;
			in[1].type = HAWK_CUT_IOSTD_NULL;

			tmpl_tmpfile = HAWK_NULL;
			if (arg.inplace && in[0].u.file.path)
			{
				int retried = 0;
				const hawk_bch_t* f[3];

				f[0] = in[0].u.fileb.path;
				f[1] = ".XXXX";
				f[2] = HAWK_NULL;

				tmpl_tmpfile = hawk_gem_dupbcstrarr(hawk_cut_getgem(cut), f, HAWK_NULL);
				if (tmpl_tmpfile == HAWK_NULL)
				{
					hawk_main_print_error("out of memory\n");
					goto oops;
				}

			open_temp:
				out_inplace.type = HAWK_CUT_IOSTD_SIO;
				out_inplace.u.sio = hawk_sio_open(
					hawk_cut_getgem(cut),
					0,
					(const hawk_ooch_t*)tmpl_tmpfile, /* fake type casting - note HAWK_SIO_BCSTRPATH below */
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
						hawk_main_print_error("cannot open %s\n", tmpl_tmpfile);
						hawk_cut_freemem(cut, tmpl_tmpfile);
						goto oops;
					}
					else
					{
						/* retry to open the file with shorter names */
						hawk_cut_freemem(cut, tmpl_tmpfile);
						tmpl_tmpfile = hawk_gem_dupbcstr(hawk_cut_getgem(cut), "TMP-XXXX", HAWK_NULL);
						if (tmpl_tmpfile == HAWK_NULL)
						{
							hawk_main_print_error("out of memory\n");
							goto oops;
						}
						retried = 1;
						goto open_temp;
					}
				}

				output = &out_inplace;
			}

			if (hawk_cut_execstd(cut, in, output) <= -1)
			{
				if (output) hawk_sio_close(output->u.sio);

				if (tmpl_tmpfile)
				{
					unlink(tmpl_tmpfile);
					hawk_cut_freemem(cut, tmpl_tmpfile);
				}
				print_exec_error(cut);
				goto oops;
			}

			if (tmpl_tmpfile)
			{
				HAWK_ASSERT(output == &out_inplace);
				hawk_sio_close(output->u.sio);
				output = output_file;

				if (rename(tmpl_tmpfile, in[0].u.fileb.path) <= -1)
				{
					hawk_main_print_error("cannot rename %s to %s. not deleting %s\n",
						tmpl_tmpfile, in[0].u.fileb.path, tmpl_tmpfile);
					hawk_cut_freemem(cut, tmpl_tmpfile);
					goto oops;
				}

				hawk_cut_freemem(cut, tmpl_tmpfile);
			}

			// TODO: if (hawk_cut_ishalt(cut)) break;
		}

		if (output) hawk_sio_close(output->u.sio);
	}
	else
	{
		int xx;
		hawk_cut_iostd_t* in = HAWK_NULL;
		hawk_cut_iostd_t out;

		if (arg.infile_pos > 0)
		{
			int i;
			const hawk_bch_t* tmp;

			/* input files are specified on the command line */

			/* perform wild-card expansions for non-unix platforms */
			if (hawk_main_expand_wildcard(argc - arg.infile_pos, &argv[arg.infile_pos], arg.wildcard, &xarg) <= -1)
			{
				hawk_main_print_error("out of memory\n");
				goto oops;
			}

			in = (hawk_cut_iostd_t*)hawk_cut_allocmem(cut, HAWK_SIZEOF(*in) * (xarg.size + 1));
			if (in == HAWK_NULL)
			{
				hawk_main_print_error("out of memory\n");
				goto oops;
			}

			for (i = 0; i < xarg.size; i++)
			{
				in[i].type = HAWK_CUT_IOSTD_FILEB;
				tmp = xarg.ptr[i];
				in[i].u.fileb.path = tmp;
				in[i].u.fileb.cmgr = g_infile_cmgr;
			}

			in[i].type = HAWK_CUT_IOSTD_NULL;
		}

		if (arg.output_file)
		{
			out.type = HAWK_CUT_IOSTD_FILEB;
			out.u.fileb.path = arg.output_file;
			out.u.fileb.cmgr = g_outfile_cmgr;
		}
		else
		{
			/* arrange to be able to specify cmgr.
			 * if not for cmgr, i could simply pass HAWK_NULL
			 * to hawk_cut_execstd() below like
			 *   xx = hawk_cut_execstd (cut, in, HAWK_NULL); */
			out.type = HAWK_CUT_IOSTD_FILEB;
			out.u.fileb.path = HAWK_NULL;
			out.u.fileb.cmgr = g_outfile_cmgr;
		}

		g_cut = cut;
		set_intr_run();
		xx = hawk_cut_execstd(cut, in, &out);
		if (in) hawk_cut_freemem(cut, in);
		unset_intr_run();
		g_cut = HAWK_NULL;

		if (xx <= -1)
		{
			print_exec_error(cut);
			goto oops;
		}
	}

	ret = 0;

oops:
	if (xarg_inited) hawk_main_purge_xarg(&xarg);
	if (cut) hawk_cut_close(cut);
	if (arg.memlimit > 0)
	{
		if (arg.debug) hawk_xma_dump(xma_mmgr.ctx, hawk_main_print_xma, HAWK_NULL);
		hawk_fini_xma_mmgr(&xma_mmgr);
	}
	free_scripts();

	return ret;
}
