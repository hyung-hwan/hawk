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

#include "cut-prv.h"
#include "hawk-prv.h"
#include "hawk-std.h"

typedef struct xtn_in_t xtn_in_t;
struct xtn_in_t
{
	hawk_cut_iostd_t* ptr;
	hawk_cut_iostd_t* cur;
	hawk_oow_t        mempos;
};

typedef struct xtn_out_t xtn_out_t;
struct xtn_out_t
{
	hawk_cut_iostd_t* ptr;
	struct
	{
		hawk_becs_t* be;
		hawk_uecs_t* ue;
	} memstr;
};

typedef struct xtn_t xtn_t;
struct xtn_t
{
	struct
	{
		xtn_in_t in;
		hawk_ooch_t last;
		int newline_squeezed;
	} s;
	struct
	{
		xtn_in_t  in;
		xtn_out_t out;
	} e;

	hawk_link_t* sio_names;
};

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE xtn_t* GET_XTN(hawk_cut_t* cut) { return (xtn_t*)((hawk_uint8_t*)hawk_cut_getxtn(cut) - HAWK_SIZEOF(xtn_t)); }
#else
#define GET_XTN(cut) ((xtn_t*)((hawk_uint8_t*)hawk_cut_getxtn(cut) - HAWK_SIZEOF(xtn_t)))
#endif


hawk_cut_t* hawk_cut_openstd (hawk_oow_t xtnsize, hawk_errnum_t* errnum)
{
	return hawk_cut_openstdwithmmgr(hawk_get_sys_mmgr(), xtnsize, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), errnum);
}

hawk_cut_t* hawk_cut_openstdwithmmgr (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errnum_t* errnum)
{
	hawk_cut_t* cut;
	xtn_t* xtn;

	if (!mmgr) mmgr = hawk_get_sys_mmgr();
	if (!cmgr) cmgr = hawk_get_cmgr_by_id(HAWK_CMGR_UTF8);

	/* create an object */
	cut = hawk_cut_open(mmgr, HAWK_SIZEOF(xtn_t) + xtnsize, cmgr, errnum);
	if (HAWK_UNLIKELY(!cut)) return HAWK_NULL;

	cut->_instsize += HAWK_SIZEOF(xtn_t);

	return cut;
}

#if 0

static hawk_ooi_t xin (hawk_cut_t* cut, hawk_cut_io_cmd_t cmd, hawk_cut_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len)
{
	hawk_sio_t* sio;
	xtn_t* xtn = (xtn_t*)GET_XTN(cut);

	switch (cmd)
	{
		case HAWK_CUT_IO_OPEN:
		{
			/* main data stream */
			if (xtn->infile == HAWK_NULL) sio = hawk_sio_in;
			else
			{
				sio = hawk_sio_open (
					cut->mmgr,
					0,
					xtn->infile,
					HAWK_SIO_READ
				);
				if (sio == HAWK_NULL)
				{
					/* set the error message explicitly
					 * as the file name is different from
					 * the standard console name (NULL) */
					hawk_oocs_t ea;
					ea.ptr = xtn->infile;
					ea.len = hawk_strlen (xtn->infile);
					hawk_cut_seterrnum (cut, HAWK_CUT_EIOFIL, &ea);
					return -1;
				}
			}

			if (sio == HAWK_NULL) return -1;
			arg->handle = sio;
			return 1;
		}

		case HAWK_CUT_IO_CLOSE:
		{
			sio = (hawk_sio_t*)arg->handle;
			if (sio != hawk_sio_in && sio != hawk_sio_out && sio != hawk_sio_err) hawk_sio_close (sio);
			return 0;
		}

		case HAWK_CUT_IO_READ:
		{
			hawk_ooi_t n = hawk_sio_getsn(arg->handle, buf, len);

			if (n == -1)
			{
				if (xtn->infile != HAWK_NULL)
				{
					hawk_oocs_t ea;
					ea.ptr = xtn->infile;
					ea.len = hawk_strlen (xtn->infile);
					hawk_cut_seterrnum (cut, HAWK_CUT_EIOFIL, &ea);
				}
			}

			return n;
		}

		default:
			return -1;
	}
}

static hawk_ooi_t xout (hawk_cut_t* cut, hawk_cut_io_cmd_t cmd, hawk_cut_io_arg_t* arg, hawk_ooch_t* dat, hawk_oow_t len)
{
	hawk_sio_t* sio;
	xtn_t* xtn = (xtn_t*)GET_XTN(cut);

	switch (cmd)
	{
		case HAWK_CUT_IO_OPEN:
		{
			if (xtn->outfile == HAWK_NULL) sio = hawk_sio_out;
			else
			{
				sio = hawk_sio_open (
					cut->mmgr,
					0,
					xtn->outfile,
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE
				);
				if (sio == HAWK_NULL)
				{
					/* set the error message explicitly
					 * as the file name is different from
					 * the standard console name (NULL) */
					hawk_oocs_t ea;
					ea.ptr = xtn->outfile;
					ea.len = hawk_strlen (xtn->outfile);
					hawk_cut_seterrnum (cut, HAWK_CUT_EIOFIL, &ea);
					return -1;
				}
			}

			if (sio == HAWK_NULL) return -1;
			arg->handle = sio;
			return 1;
		}

		case HAWK_CUT_IO_CLOSE:
		{
			sio = (hawk_sio_t*)arg->handle;
			hawk_sio_flush (sio);
			if (sio != hawk_sio_in && sio != hawk_sio_out && sio != hawk_sio_err)
				hawk_sio_close (sio);
			return 0;
		}

		case HAWK_CUT_IO_WRITE:
		{
			hawk_ooi_t n = hawk_sio_putsn (arg->handle, dat, len);

			if (n == -1)
			{
				if (xtn->infile != HAWK_NULL)
				{
					hawk_oocs_t ea;
					ea.ptr = xtn->infile;
					ea.len = hawk_strlen (xtn->infile);
					hawk_cut_seterrnum (cut, HAWK_CUT_EIOFIL, &ea);
				}
			}

			return n;
		}

		default:
			return -1;
	}
}
#else

static int verify_iostd_in (hawk_cut_t* cut, hawk_cut_iostd_t in[])
{
	hawk_oow_t i;

	if (in[0].type == HAWK_CUT_IOSTD_NULL)
	{
		/* if 'in' is specified, it must contains at least one
		 * valid entry */
		hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_EINVAL, "no input handler provided");
		return -1;
	}

	for (i = 0; in[i].type != HAWK_CUT_IOSTD_NULL; i++)
	{
		if (in[i].type != HAWK_CUT_IOSTD_FILE &&
		    in[i].type != HAWK_CUT_IOSTD_FILEB &&
		    in[i].type != HAWK_CUT_IOSTD_FILEU &&
		    in[i].type != HAWK_CUT_IOSTD_OOCS &&
		    in[i].type != HAWK_CUT_IOSTD_BCS &&
		    in[i].type != HAWK_CUT_IOSTD_UCS &&
		    in[i].type != HAWK_CUT_IOSTD_SIO)
		{
			hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_EINVAL, "unsupported input type provided - handler %d type %d", i, in[i].type);
			return -1;
		}
	}

	return 0;
}

static hawk_sio_t* open_sio_file (hawk_cut_t* cut, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;

	sio = hawk_sio_open(hawk_cut_getgem(cut), 0, file, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* bem = hawk_cut_backuperrmsg(cut);
		hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "unable to open %js - %js", file, bem);
	}
	return sio;
}

static hawk_oocs_t sio_std_names[] =
{
	{ (hawk_ooch_t*)HAWK_T("stdin"),   5 },
	{ (hawk_ooch_t*)HAWK_T("stdout"),  6 },
	{ (hawk_ooch_t*)HAWK_T("stderr"),  6 }
};

static hawk_ooch_t* add_sio_name_with_uchars (hawk_cut_t* cut, const hawk_uch_t* ptr, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(cut);
	hawk_link_t* link;

	/* TODO: duplication check? */

#if defined(HAWK_OOCH_IS_UCH)
	link = (hawk_link_t*)hawk_cut_callocmem(cut, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_uch_t) * (len + 1));
	if (!link) return HAWK_NULL;

	hawk_copy_uchars_to_ucstr_unlimited((hawk_uch_t*)(link + 1), ptr, len);
#else
	hawk_oow_t bcslen, ucslen;

	ucslen = len;
	if (hawk_gem_convutobchars(hawk_cut_getgem(cut), ptr, &ucslen, HAWK_NULL, &bcslen) <= -1) return HAWK_NULL;

	link = (hawk_link_t*)hawk_cut_callocmem(cut, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_bch_t) * (bcslen + 1));
	if (!link) return HAWK_NULL;

	ucslen = len;
	bcslen = bcslen + 1;
	hawk_gem_convutobchars(hawk_cut_getgem(cut), ptr, &ucslen, (hawk_bch_t*)(link + 1), &bcslen);
	((hawk_bch_t*)(link + 1))[bcslen] = '\0';
#endif

	link->link = xtn->sio_names;
	xtn->sio_names = link;

	return (hawk_ooch_t*)(link + 1);
}

static hawk_ooch_t* add_sio_name_with_bchars (hawk_cut_t* cut, const hawk_bch_t* ptr, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(cut);
	hawk_link_t* link;

	/* TODO: duplication check? */

#if defined(HAWK_OOCH_IS_UCH)
	hawk_oow_t bcslen, ucslen;

	bcslen = len;
	if (hawk_gem_convbtouchars(hawk_cut_getgem(cut), ptr, &bcslen, HAWK_NULL, &ucslen, 0) <= -1) return HAWK_NULL;

	link = (hawk_link_t*)hawk_cut_callocmem(cut, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_uch_t) * (ucslen + 1));
	if (!link) return HAWK_NULL;

	bcslen = len;
	ucslen = ucslen + 1;
	hawk_gem_convbtouchars (hawk_cut_getgem(cut), ptr, &bcslen, (hawk_uch_t*)(link + 1), &ucslen, 0);
	((hawk_uch_t*)(link + 1))[ucslen] = '\0';

#else
	link = (hawk_link_t*)hawk_cut_callocmem(cut, HAWK_SIZEOF(*link) + HAWK_SIZEOF(hawk_bch_t) * (len + 1));
	if (!link) return HAWK_NULL;

	hawk_copy_bchars_to_bcstr_unlimited((hawk_bch_t*)(link + 1), ptr, len);
#endif

	link->link = xtn->sio_names;
	xtn->sio_names = link;

	return (hawk_ooch_t*)(link + 1);
}

static void clear_sio_names (hawk_cut_t* cut)
{
	xtn_t* xtn = GET_XTN(cut);
	hawk_link_t* cur;

	while (xtn->sio_names)
	{
		cur = xtn->sio_names;
		xtn->sio_names = cur->link;
		hawk_cut_freemem (cut, cur);
	}
}

static hawk_sio_t* open_sio_std (hawk_cut_t* cut, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;

	sio = hawk_sio_openstd(hawk_cut_getgem(cut), 0, std, flags);
	if (sio == HAWK_NULL)
	{
		const hawk_ooch_t* bem = hawk_cut_backuperrmsg(cut);
		hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "unable to open %js - %js", &sio_std_names[std], bem);
	}
	return sio;
}

static void set_eiofil_for_iostd (hawk_cut_t* cut, hawk_cut_iostd_t* io)
{
	const hawk_ooch_t* bem = hawk_cut_backuperrmsg(cut);

	HAWK_ASSERT (io->type == HAWK_CUT_IOSTD_FILE || io->type == HAWK_CUT_IOSTD_FILEB || io->type == HAWK_CUT_IOSTD_FILEU);

	if (io->u.file.path) /* file, fileb, fileu are union members. checking file.path regardless of io->type must be safe */
	{
		switch (io->type)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			case HAWK_CUT_IOSTD_FILE:
		#endif
			case HAWK_CUT_IOSTD_FILEB:
				hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "I/O error with file '%hs' - %js", io->u.fileb.path, bem);
				break;

		#if defined(HAWK_OOCH_IS_UCH)
			case HAWK_CUT_IOSTD_FILE:
		#endif
			case HAWK_CUT_IOSTD_FILEU:
				hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "I/O error with file '%ls' - %js", io->u.fileu.path, bem);
				break;

			default:
				HAWK_ASSERT (!"should never happen - unknown file I/O type");
				hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINVAL);
				break;
		}
	}
	else
	{
		hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "I/O error with file '%js' - %js", &sio_std_names[HAWK_SIO_STDIN], bem);
	}
}


static void close_main_stream (hawk_cut_t* cut, hawk_cut_io_arg_t* arg, hawk_cut_iostd_t* io)
{
	switch (io->type)
	{
		case HAWK_CUT_IOSTD_FILE:
		case HAWK_CUT_IOSTD_FILEB:
		case HAWK_CUT_IOSTD_FILEU:
			hawk_sio_close(arg->handle);
			break;

		case HAWK_CUT_IOSTD_OOCS:
		case HAWK_CUT_IOSTD_BCS:
		case HAWK_CUT_IOSTD_UCS:
			/* for input, there is nothing to do here.
			 * for output, closing xtn->e.out.memstr is required. but put it to hawk_awk_execstd().
			 */
			break;

		case HAWK_CUT_IOSTD_SIO:
			/* nothing to do */
			break;

		default:
			/* do nothing */
			break;
	}

}

static int open_input_stream (hawk_cut_t* cut, hawk_cut_io_arg_t* arg, hawk_cut_iostd_t* io, xtn_in_t* base)
{
	xtn_t* xtn = GET_XTN(cut);

	HAWK_ASSERT (io != HAWK_NULL);
	switch (io->type)
	{
	#if defined(HAWK_OOCH_IS_BCH)
		case HAWK_CUT_IOSTD_FILE:
			HAWK_ASSERT (&io->u.fileb.path == &io->u.file.path);
			HAWK_ASSERT (&io->u.fileb.cmgr == &io->u.file.cmgr);
	#endif
		case HAWK_CUT_IOSTD_FILEB:
		{
			hawk_sio_t* sio;
			hawk_ooch_t* path;

			if (io->u.fileb.path == HAWK_NULL ||
			    (io->u.fileb.path[0] == '-' && io->u.fileb.path[1] == '\0'))
			{
				sio = open_sio_std(cut, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			else
			{
				path = add_sio_name_with_bchars(cut, io->u.fileb.path, hawk_count_bcstr(io->u.fileb.path));
				if (path == HAWK_NULL) return -1;
				sio = open_sio_file(cut, path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			if (sio == HAWK_NULL) return -1;

			if (io->u.fileb.cmgr) hawk_sio_setcmgr (sio, io->u.fileb.cmgr);
			arg->handle = sio;
			break;
		}

	#if defined(HAWK_OOCH_IS_UCH)
		case HAWK_CUT_IOSTD_FILE:
			HAWK_ASSERT (&io->u.fileu.path == &io->u.file.path);
			HAWK_ASSERT (&io->u.fileu.cmgr == &io->u.file.cmgr);
	#endif
		case HAWK_CUT_IOSTD_FILEU:
		{
			hawk_sio_t* sio;
			hawk_ooch_t* path;

			if (io->u.fileu.path == HAWK_NULL ||
			    (io->u.fileu.path[0] == '-' && io->u.fileu.path[1] == '\0'))
			{
				sio = open_sio_std(cut, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			else
			{
				path = add_sio_name_with_uchars(cut, io->u.fileu.path, hawk_count_ucstr(io->u.fileu.path));
				if (path == HAWK_NULL) return -1;
				sio = open_sio_file(cut, path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			}
			if (sio == HAWK_NULL) return -1;

			if (io->u.fileu.cmgr) hawk_sio_setcmgr (sio, io->u.fileu.cmgr);
			arg->handle = sio;
			break;
		}

		case HAWK_CUT_IOSTD_OOCS:
		case HAWK_CUT_IOSTD_BCS:
		case HAWK_CUT_IOSTD_UCS:
			/* don't store anything to arg->handle */
			base->mempos = 0;
			break;

		case HAWK_CUT_IOSTD_SIO:
			arg->handle = io->u.sio;
			break;

		default:
			HAWK_ASSERT (!"should never happen - io-type must be one of SIO,FILE,FILEB,FILEU,STR");
			hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}

	if (base == &xtn->s.in)
	{
#if 0
		/* reset script location */
		switch (io->type)
		{
			case HAWK_CUT_IOSTD_FILE:
				if (io->u.fileb.path)
				{
					hawk_cut_setcompid (cut, io->u.file.path);
				}
				else
				{
				compid_stdin:
					hawk_cut_setcompid (cut, sio_std_names[HAWK_SIO_STDIN].ptr);
				}
				break;

			case HAWK_CUT_IOSTD_FILEB:
				if (!io->u.fileb.path) goto compid_stdin;
				hawk_cut_setcompidwithbcstr (cut, io->u.fileb.path);
				break;

			case HAWK_CUT_IOSTD_FILEU:
				if (!io->u.fileu.path) goto compid_stdin;
				hawk_cut_setcompid (cut, sio_std_names[HAWK_SIO_STDIN].ptr);
				break;

			default:
			{
				hawk_ooch_t buf[64];

				/* format an identifier to be something like M#1, S#5 */
				buf[0] = (io->type == HAWK_CUT_IOSTD_OOCS ||
						io->type == HAWK_CUT_IOSTD_BCS ||
						io->type == HAWK_CUT_IOSTD_UCS)? HAWK_T('M'): HAWK_T('S');
				buf[1] = HAWK_T('#');
				int_to_str (io - xtn->s.in.ptr, &buf[2], HAWK_COUNTOF(buf) - 2);

				/* don't care about failure int_to_str() though it's not
				 * likely to happen */
				hawk_cut_setcompid (cut, buf);
				break;
			}
		}
		cut->src.loc.line = 1;
		cut->src.loc.colm = 1;
#endif
	}
	return 0;
}

static int open_output_stream (hawk_cut_t* cut, hawk_cut_io_arg_t* arg, hawk_cut_iostd_t* io)
{
	xtn_t* xtn = GET_XTN(cut);

	HAWK_ASSERT (io != HAWK_NULL);
	switch (io->type)
	{
	#if defined(HAWK_OOCH_IS_BCH)
		case HAWK_CUT_IOSTD_FILE:
			HAWK_ASSERT (&io->u.fileb.path == &io->u.file.path);
			HAWK_ASSERT (&io->u.fileb.cmgr == &io->u.file.cmgr);
	#endif
		case HAWK_CUT_IOSTD_FILEB:
		{
			hawk_sio_t* sio;
			hawk_ooch_t* path;

			if (io->u.fileb.path == HAWK_NULL ||
			    (io->u.fileb.path[0] == HAWK_T('-') && io->u.fileb.path[1] == HAWK_T('\0')))
			{
				sio = open_sio_std(
					cut, HAWK_SIO_STDOUT,
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE |
					HAWK_SIO_IGNOREECERR |
					HAWK_SIO_LINEBREAK
				);
			}
			else
			{
				path = add_sio_name_with_bchars(cut, io->u.fileb.path, hawk_count_bcstr(io->u.fileb.path));
				if (path == HAWK_NULL) return -1;
				sio = open_sio_file(
					cut, path,
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE |
					HAWK_SIO_IGNOREECERR
				);
			}
			if (sio == HAWK_NULL) return -1;
			if (io->u.fileb.cmgr) hawk_sio_setcmgr (sio, io->u.fileb.cmgr);
			arg->handle = sio;
			break;
		}

	#if defined(HAWK_OOCH_IS_UCH)
		case HAWK_CUT_IOSTD_FILE:
			HAWK_ASSERT (&io->u.fileu.path == &io->u.file.path);
			HAWK_ASSERT (&io->u.fileu.cmgr == &io->u.file.cmgr);
	#endif
		case HAWK_CUT_IOSTD_FILEU:
		{
			hawk_sio_t* sio;
			hawk_ooch_t* path;

			if (io->u.fileu.path == HAWK_NULL ||
			    (io->u.fileu.path[0] == HAWK_T('-') && io->u.fileu.path[1] == HAWK_T('\0')))
			{
				sio = open_sio_std(
					cut, HAWK_SIO_STDOUT,
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE |
					HAWK_SIO_IGNOREECERR |
					HAWK_SIO_LINEBREAK
				);
			}
			else
			{
				path = add_sio_name_with_uchars(cut, io->u.fileu.path, hawk_count_ucstr(io->u.fileu.path));
				if (path == HAWK_NULL) return -1;
				sio = open_sio_file(
					cut, path,
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE |
					HAWK_SIO_IGNOREECERR
				);
			}
			if (sio == HAWK_NULL) return -1;
			if (io->u.fileu.cmgr) hawk_sio_setcmgr (sio, io->u.fileu.cmgr);
			arg->handle = sio;
			break;
		}

	#if defined(HAWK_OOCH_IS_BCH)
		case HAWK_CUT_IOSTD_OOCS:
	#endif
		case HAWK_CUT_IOSTD_BCS:
			xtn->e.out.memstr.be = hawk_becs_open(hawk_cut_getgem(cut), 0, 512);
			if (xtn->e.out.memstr.be == HAWK_NULL) return -1;
			break;

	#if defined(HAWK_OOCH_IS_UCH)
		case HAWK_CUT_IOSTD_OOCS:
	#endif
		case HAWK_CUT_IOSTD_UCS:
			/* don't store anything to arg->handle */
			xtn->e.out.memstr.ue = hawk_uecs_open(hawk_cut_getgem(cut), 0, 512);
			if (xtn->e.out.memstr.ue == HAWK_NULL) return -1;
			break;

		case HAWK_CUT_IOSTD_SIO:
			arg->handle = io->u.sio;
			break;

		default:
			HAWK_ASSERT (!"should never happen - io-type must be one of SIO,FILE,FILEB,FILEU,STR");
			hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}

	return 0;
}


static hawk_ooi_t read_input_stream (hawk_cut_t* cut, hawk_cut_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len, xtn_in_t* base)
{
	xtn_t* xtn = GET_XTN(cut);
	hawk_cut_iostd_t* io, * next;
	void* old, * new;
	hawk_ooi_t n = 0;

	if (len > HAWK_TYPE_MAX(hawk_ooi_t)) len = HAWK_TYPE_MAX(hawk_ooi_t);

	do
	{
		io = base->cur;

		if (base == &xtn->s.in && xtn->s.newline_squeezed)
		{
			xtn->s.newline_squeezed = 0;
			goto open_next;
		}

		HAWK_ASSERT (io != HAWK_NULL);

		if (io->type == HAWK_CUT_IOSTD_OOCS)
		{
		iostd_oocs:
			n = 0;
			while (base->mempos < io->u.oocs.len && n < len)
				buf[n++] = io->u.oocs.ptr[base->mempos++];
		}
		else if (io->type == HAWK_CUT_IOSTD_BCS)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			goto iostd_oocs;
		#else
			int m;
			hawk_oow_t mbslen, wcslen;

			mbslen = io->u.bcs.len - base->mempos;
			wcslen = len;
			if ((m = hawk_conv_bchars_to_uchars_with_cmgr(&io->u.bcs.ptr[base->mempos], &mbslen, buf, &wcslen, hawk_cut_getcmgr(cut), 0)) <= -1 && m != -2)
			{
				hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EECERR);
				n = -1;
			}
			else
			{
				base->mempos += mbslen;
				n = wcslen;
			}
		#endif
		}
		else if (io->type == HAWK_CUT_IOSTD_UCS)
		{
		#if defined(HAWK_OOCH_IS_UCH)
			goto iostd_oocs;
		#else
			int m;
			hawk_oow_t mbslen, wcslen;

			wcslen = io->u.ucs.len - base->mempos;
			mbslen = len;
			if ((m = hawk_conv_uchars_to_bchars_with_cmgr(&io->u.ucs.ptr[base->mempos], &wcslen, buf, &mbslen, hawk_cut_getcmgr(cut))) <= -1 && m != -2)
			{
				hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EECERR);
				n = -1;
			}
			else
			{
				base->mempos += mbslen;
				n = mbslen;
			}
		#endif
		}
		else
		{
			n = hawk_sio_getoochars(arg->handle, buf, len);
			if (n <= -1)
			{
				set_eiofil_for_iostd(cut, io);
				break;
			}
		}

		if (n != 0)
		{
			if (base == &xtn->s.in)
			{
				xtn->s.last = buf[n - 1];
			}

			break;
		}

		/* ============================================= */
		/* == end of file on the current input stream == */
		/* ============================================= */

#if 0
		if (base == &xtn->s.in && xtn->s.last != HAWK_T('\n'))
		{
			/* TODO: different line termination convension */
			buf[0] = HAWK_T('\n');
			n = 1;
			xtn->s.newline_squeezed = 1;
			break;
		}
#endif

	open_next:
		next = base->cur + 1;
		if (next->type == HAWK_CUT_IOSTD_NULL)
		{
			/* no next stream available - return 0 */
			break;
		}

		old = arg->handle;

		/* try to open the next input stream */
		if (open_input_stream(cut, arg, next, base) <= -1)
		{
			/* failed to open the next input stream */
			set_eiofil_for_iostd(cut, next);
			n = -1;
			break;
		}

		/* successfuly opened the next input stream */
		new = arg->handle;

		arg->handle = old;

		/* close the previous stream */
		close_main_stream(cut, arg, io);

		arg->handle = new;

		base->cur++;
	}
	while (1);

	return n;
}

static hawk_ooi_t s_in (hawk_cut_t* cut, hawk_cut_io_cmd_t cmd, hawk_cut_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(cut);

	switch (cmd)
	{
		case HAWK_CUT_IO_OPEN:
		{
			if (open_input_stream(cut, arg, xtn->s.in.cur, &xtn->s.in) <= -1) return -1;
			return 1;
		}

		case HAWK_CUT_IO_CLOSE:
		{
			close_main_stream(cut, arg, xtn->s.in.cur);
			return 0;
		}

		case HAWK_CUT_IO_READ:
		{
			return read_input_stream(cut, arg, buf, len, &xtn->s.in);
		}

		default:
		{
			HAWK_ASSERT (!"should never happen - cmd must be one of OPEN,CLOSE,READ");
			hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINTERN);
			return -1;
		}
	}
}

static hawk_ooi_t x_in (hawk_cut_t* cut, hawk_cut_io_cmd_t cmd, hawk_cut_io_arg_t* arg, hawk_ooch_t* buf, hawk_oow_t len)
{
	hawk_sio_t* sio;
	xtn_t* xtn = GET_XTN(cut);

	switch (cmd)
	{
		case HAWK_CUT_IO_OPEN:
		{
			if (arg->path == HAWK_NULL)
			{
				/* main data stream */
				if (xtn->e.in.ptr == HAWK_NULL)
				{
					/* HAWK_NULL pascut into hawk_cut_exec() for input. open stdin */
					sio = open_sio_std(cut, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
					if (sio == HAWK_NULL) return -1;
					arg->handle = sio;
				}
				else
				{
					/* use the input stream handlers pascut to hawk_cut_exec() */
					if (open_input_stream(cut, arg, xtn->e.in.cur, &xtn->e.in) <= -1) return -1;
				}
			}
			else
			{
				/* sub-stream */
				sio = open_sio_file(cut, arg->path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
				if (sio == HAWK_NULL) return -1;
				arg->handle = sio;
			}

			return 1;
		}

		case HAWK_CUT_IO_CLOSE:
		{
			if (arg->path == HAWK_NULL)
			{
				/* main data stream */
				if (xtn->e.in.ptr == HAWK_NULL)
					hawk_sio_close(arg->handle);
				else
					close_main_stream(cut, arg, xtn->e.in.cur);
			}
			else
			{
				hawk_sio_close(arg->handle);
			}

			return 0;
		}

		case HAWK_CUT_IO_READ:
		{
			if (arg->path == HAWK_NULL)
			{
				/* main data stream */
				if (xtn->e.in.ptr == HAWK_NULL)
				{
					hawk_ooi_t n;
					n = hawk_sio_getoochars(arg->handle, buf, len);
					if (n <= -1)
					{
						const hawk_ooch_t* bem = hawk_cut_backuperrmsg(cut);
						hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "unable to read '%js' - %js", &sio_std_names[HAWK_SIO_STDIN], bem);
					}
					return n;
				}
				else
				{
					return read_input_stream(cut, arg, buf, len, &xtn->e.in);
				}
			}
			else
			{
				hawk_ooi_t n;
				n = hawk_sio_getoochars(arg->handle, buf, len);
				if (n <= -1)
				{
					const hawk_ooch_t* bem = hawk_cut_backuperrmsg(cut);
					hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "unable to read '%js' - %js", arg->path, bem);
				}
				return n;
			}
		}

		default:
			HAWK_ASSERT (!"should never happen - cmd must be one of OPEN,CLOSE,READ");
			hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}
}

static hawk_ooi_t x_out (
	hawk_cut_t* cut, hawk_cut_io_cmd_t cmd, hawk_cut_io_arg_t* arg,
	hawk_ooch_t* dat, hawk_oow_t len)
{
	xtn_t* xtn = GET_XTN(cut);
	hawk_sio_t* sio;

	switch (cmd)
	{
		case HAWK_CUT_IO_OPEN:
		{
			if (arg->path == HAWK_NULL)
			{
				/* main data stream */

				if (xtn->e.out.ptr == HAWK_NULL)
				{
					/* HAWK_NULL pascut into hawk_cut_execstd() for output */
					sio = open_sio_std(
						cut, HAWK_SIO_STDOUT,
						HAWK_SIO_WRITE |
						HAWK_SIO_CREATE |
						HAWK_SIO_TRUNCATE |
						HAWK_SIO_IGNOREECERR |
						HAWK_SIO_LINEBREAK
					);
					if (sio == HAWK_NULL) return -1;
					arg->handle = sio;
				}
				else
				{
					if (open_output_stream(cut, arg, xtn->e.out.ptr) <= -1) return -1;
				}
			}
			else
			{

				sio = open_sio_file(
					cut, arg->path,
					HAWK_SIO_WRITE |
					HAWK_SIO_CREATE |
					HAWK_SIO_TRUNCATE |
					HAWK_SIO_IGNOREECERR
				);
				if (sio == HAWK_NULL) return -1;
				arg->handle = sio;
			}

			return 1;
		}

		case HAWK_CUT_IO_CLOSE:
		{
			if (arg->path == HAWK_NULL)
			{
				if (xtn->e.out.ptr == HAWK_NULL)
					hawk_sio_close(arg->handle);
				else
					close_main_stream(cut, arg, xtn->e.out.ptr);
			}
			else
			{
				hawk_sio_close(arg->handle);
			}
			return 0;
		}

		case HAWK_CUT_IO_WRITE:
		{
			if (arg->path == HAWK_NULL)
			{
				/* main data stream */
				if (xtn->e.out.ptr == HAWK_NULL)
				{
					hawk_ooi_t n;
					n = hawk_sio_putoochars(arg->handle, dat, len);
					if (n <= -1) hawk_cut_seterror (cut, HAWK_NULL, HAWK_CUT_EIOFIL, &sio_std_names[HAWK_SIO_STDOUT]);
					return n;
				}
				else
				{
					hawk_cut_iostd_t* io = xtn->e.out.ptr;
					switch (io->type)
					{
					#if defined(HAWK_OOCH_IS_BCH)
						case HAWK_CUT_IOSTD_OOCS:
					#endif
						case HAWK_CUT_IOSTD_BCS:
							if (len > HAWK_TYPE_MAX(hawk_ooi_t)) len = HAWK_TYPE_MAX(hawk_ooi_t);
					#if defined(HAWK_OOCH_IS_BCH)
							if (hawk_becs_ncat(xtn->e.out.memstr.be, dat, len) == (hawk_oow_t)-1) return -1;
					#else
							if (hawk_becs_ncatuchars(xtn->e.out.memstr.be, dat, len, HAWK_NULL) == (hawk_oow_t)-1) return -1;
					#endif
							return len;

					#if defined(HAWK_OOCH_IS_UCH)
						case HAWK_CUT_IOSTD_OOCS:
					#endif
						case HAWK_CUT_IOSTD_UCS:
							if (len > HAWK_TYPE_MAX(hawk_ooi_t)) len = HAWK_TYPE_MAX(hawk_ooi_t);
					#if defined(HAWK_OOCH_IS_UCH)
							if (hawk_uecs_ncat(xtn->e.out.memstr.ue, dat, len) == (hawk_oow_t)-1) return -1;
					#else
							if (hawk_uecs_ncatbchars(xtn->e.out.memstr.ue, dat, len, HAWK_NULL, 1) == (hawk_oow_t)-1) return -1;
					#endif
							return len;

						default:
						{
							hawk_ooi_t n;
							n = hawk_sio_putoochars(arg->handle, dat, len);
							if (n <= -1) set_eiofil_for_iostd(cut, io);
							return n;
						}
					}
				}
			}
			else
			{
				hawk_ooi_t n;
				n = hawk_sio_putoochars(arg->handle, dat, len);
				if (n <= -1)
				{
					const hawk_ooch_t* bem = hawk_cut_backuperrmsg(cut);
					hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_CUT_EIOFIL, "unable to write '%js' - %js", arg->path, bem);
				}
				return n;
			}
		}

		default:
			HAWK_ASSERT (!"should never happen - cmd must be one of OPEN,CLOSE,WRITE");
			hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}
}

#endif

/* ------------------------------------------------------------- */

int hawk_cut_compstd (hawk_cut_t* cut, hawk_cut_iostd_t in[], hawk_oow_t* count)
{
	xtn_t* xtn = GET_XTN(cut);
	int ret;

	if (in == HAWK_NULL)
	{
		/* it requires a valid array unlike hawk_cut_execstd(). */
		hawk_cut_seterrbfmt(cut, HAWK_NULL, HAWK_EINVAL, "no input handler provided");
		if (count) *count = 0;
		return -1;
	}
	if (verify_iostd_in(cut, in) <= -1)
	{
		if (count) *count = 0;
		return -1;
	}

	HAWK_MEMSET(&xtn->s, 0, HAWK_SIZEOF(xtn->s));
	xtn->s.in.ptr = in;
	xtn->s.in.cur = in;

	ret = hawk_cut_comp(cut, s_in);

	if (count) *count = xtn->s.in.cur - xtn->s.in.ptr;

	clear_sio_names(cut);
	return ret;
}

int hawk_cut_compstdoocstr (hawk_cut_t* cut, const hawk_ooch_t* script)
{
	hawk_cut_iostd_t in[2];

	in[0].type = HAWK_CUT_IOSTD_OOCS;
	in[0].u.oocs.ptr = (hawk_ooch_t*)script;
	in[0].u.oocs.len = hawk_count_oocstr(script);
	in[1].type = HAWK_CUT_IOSTD_NULL;

	return hawk_cut_compstd(cut, in, HAWK_NULL);
}

int hawk_cut_compstdoocs (hawk_cut_t* cut, const hawk_oocs_t* script)
{
	hawk_cut_iostd_t in[2];

	in[0].type = HAWK_CUT_IOSTD_OOCS;
	in[0].u.oocs = *script;
	in[1].type = HAWK_CUT_IOSTD_NULL;

	return hawk_cut_compstd(cut, in, HAWK_NULL);
}

/* ------------------------------------------------------------- */

int hawk_cut_execstd (hawk_cut_t* cut, hawk_cut_iostd_t in[], hawk_cut_iostd_t* out)
{
	int n;
	xtn_t* xtn = GET_XTN(cut);

	if (in && verify_iostd_in(cut, in) <= -1) return -1;

	if (out)
	{
		if (out->type != HAWK_CUT_IOSTD_FILE &&
		    out->type != HAWK_CUT_IOSTD_FILEB &&
		    out->type != HAWK_CUT_IOSTD_FILEU &&
		    out->type != HAWK_CUT_IOSTD_OOCS &&
		    out->type != HAWK_CUT_IOSTD_SIO)
		{
			hawk_cut_seterrnum(cut, HAWK_NULL, HAWK_EINVAL);
			return -1;
		}
	}

	HAWK_MEMSET(&xtn->e, 0, HAWK_SIZEOF(xtn->e));
	xtn->e.in.ptr = in;
	xtn->e.in.cur = in;
	xtn->e.out.ptr = out;

	n = hawk_cut_exec(cut, x_in, x_out);

	if (out && out->type == HAWK_CUT_IOSTD_OOCS)
	{
		switch (out->type)
		{
		#if defined(HAWK_OOCH_IS_BCH)
			case HAWK_CUT_IOSTD_OOCS:
		#endif
			case HAWK_CUT_IOSTD_BCS:
				if (n >= 0)
				{
					HAWK_ASSERT (xtn->e.out.memstr.be != HAWK_NULL);
					hawk_becs_yield(xtn->e.out.memstr.be, &out->u.bcs, 0);
				}
				if (xtn->e.out.memstr.be) hawk_becs_close(xtn->e.out.memstr.be);
				break;

		#if defined(HAWK_OOCH_IS_UCH)
			case HAWK_CUT_IOSTD_OOCS:
		#endif
			case HAWK_CUT_IOSTD_UCS:
				if (n >= 0)
				{
					HAWK_ASSERT (xtn->e.out.memstr.ue != HAWK_NULL);
					hawk_uecs_yield(xtn->e.out.memstr.ue, &out->u.ucs, 0);
				}
				if (xtn->e.out.memstr.ue) hawk_uecs_close(xtn->e.out.memstr.ue);
				break;

			default:
				/* don't handle other types */
				break;
		}

	}

	clear_sio_names(cut);
	return n;
}

int hawk_cut_execstdfile (hawk_cut_t* cut, const hawk_ooch_t* infile, const hawk_ooch_t* outfile, hawk_cmgr_t* cmgr)
{
#if 0
	xtn_t* xtn = (xtn_t*)GET_XTN(cut);
	xtn->infile = infile;
	xtn->outfile = outfile;
	return hawk_cut_exec(cut, xin, xout);
#else
	hawk_cut_iostd_t in[2];
	hawk_cut_iostd_t out;
	hawk_cut_iostd_t* pin = HAWK_NULL, * pout = HAWK_NULL;

	if (infile)
	{
		in[0].type = HAWK_CUT_IOSTD_FILE;
		in[0].u.file.path = infile;
		in[0].u.file.cmgr = cmgr;
		in[1].type = HAWK_CUT_IOSTD_NULL;
		pin = in;
	}

	if (outfile)
	{
		out.type = HAWK_CUT_IOSTD_FILE;
		out.u.file.path = outfile;
		out.u.file.cmgr = cmgr;
		pout = &out;
	}

	return hawk_cut_execstd(cut, pin, pout);
#endif
}
