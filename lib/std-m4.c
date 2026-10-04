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

#include "hawk-prv.h"
#include <hawk-m4.h>
#include <hawk-sio.h>
#include <stdlib.h>

static int is_dash (const hawk_ooch_t* path)
{
	return path && path[0] == HAWK_T('-') && path[1] == HAWK_T('\0');
}

static int is_absolute_path (const hawk_ooch_t* path)
{
	if (HAWK_IS_PATH_SEP(path[0])) return 1;
#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
	if (HAWK_IS_PATH_DRIVE(path)) return 1;
#endif
	return 0;
}

static hawk_sio_t* open_input_file (hawk_m4_t* m4, const hawk_ooch_t* path)
{
	hawk_sio_t* sio;
	const hawk_ooch_t* dirs;
	const hawk_ooch_t* ptr;
	hawk_oow_t plen;

	sio = hawk_sio_open(hawk_m4_getgem(m4), 0, path, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
	if (sio || is_absolute_path(path)) return sio;

	if (hawk_m4_getopt(m4, HAWK_M4_OPT_INCDIRS, &dirs) <= -1 || !dirs || dirs[0] == HAWK_T('\0')) return HAWK_NULL;
	plen = hawk_count_oocstr(path);
	ptr = dirs;
	while (1)
	{
		const hawk_ooch_t* sep;
		hawk_oow_t dlen, len, pos;
		hawk_ooch_t* xpath;
		int need_sep;

		sep = hawk_find_oochar_in_oocstr(ptr, HAWK_DFL_PATH_LIST_SEP);
		dlen = sep? (hawk_oow_t)(sep - ptr): hawk_count_oocstr(ptr);
		if (dlen > 0)
		{
			need_sep = !HAWK_IS_PATH_SEP(ptr[dlen - 1]);
			len = dlen + need_sep + plen;
			xpath = (hawk_ooch_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*xpath) * (len + 1));
			if (!xpath) return HAWK_NULL;
			HAWK_MEMCPY(xpath, ptr, HAWK_SIZEOF(*xpath) * dlen);
			pos = dlen;
			if (need_sep) xpath[pos++] = HAWK_T('/');
			HAWK_MEMCPY(&xpath[pos], path, HAWK_SIZEOF(*xpath) * (plen + 1));

			sio = hawk_sio_open(hawk_m4_getgem(m4), 0, xpath, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
			hawk_m4_freemem(m4, xpath);
			if (sio)
			{
				hawk_m4_seterrnum(m4, HAWK_NULL, HAWK_ENOERR);
				return sio;
			}
		}

		if (!sep) break;
		ptr = sep + 1;
	}

	return HAWK_NULL;
}

static hawk_ooi_t std_io (hawk_m4_t* m4, hawk_m4_io_cmd_t cmd, hawk_m4_io_arg_t* arg, hawk_ooch_t* data, hawk_oow_t count)
{
	hawk_sio_t* sio;

	switch (cmd)
	{
		case HAWK_M4_IO_OPEN:
			if (arg->kind == HAWK_M4_IO_INPUT)
			{
				if (!arg->path || is_dash(arg->path)) sio = hawk_sio_openstd(hawk_m4_getgem(m4), 0, HAWK_SIO_STDIN, HAWK_SIO_READ | HAWK_SIO_IGNOREECERR);
				else sio = open_input_file(m4, arg->path);
			}
			else if (arg->kind == HAWK_M4_IO_OUTPUT)
				sio = hawk_sio_openstd(hawk_m4_getgem(m4), 0, HAWK_SIO_STDOUT, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			else
				sio = hawk_sio_openstd(hawk_m4_getgem(m4), 0, HAWK_SIO_STDERR, HAWK_SIO_WRITE | HAWK_SIO_IGNOREECERR | HAWK_SIO_LINEBREAK);
			if (!sio)
			{
				const hawk_ooch_t* bem = hawk_gem_backuperrmsg(hawk_m4_getgem(m4));
				if (arg->path) hawk_m4_seterrbfmt(m4, HAWK_NULL, HAWK_EOPEN, "unable to open %js - %js", arg->path, bem);
				else hawk_m4_seterrbfmt(m4, HAWK_NULL, HAWK_EOPEN, "unable to open standard stream - %js", bem);
				return -1;
			}
			arg->handle = sio;
			return 0;

		case HAWK_M4_IO_CLOSE:
			sio = (hawk_sio_t*)arg->handle;
			if (sio)
			{
				if (arg->kind != HAWK_M4_IO_INPUT) hawk_sio_flush(sio);
				hawk_sio_close(sio);
				arg->handle = HAWK_NULL;
			}
			return 0;

		case HAWK_M4_IO_READ:
			return hawk_sio_getoochars((hawk_sio_t*)arg->handle, data, count);

		case HAWK_M4_IO_WRITE:
			return hawk_sio_putoochars((hawk_sio_t*)arg->handle, data, count);

		case HAWK_M4_IO_SYSCMD:
		{
			hawk_ooch_t* oocmd;
			hawk_bch_t* bcmd;
			int n;

			oocmd = (hawk_ooch_t*)hawk_m4_allocmem(m4, HAWK_SIZEOF(*oocmd) * (count + 1));
			if (!oocmd) return -1;
			HAWK_MEMCPY(oocmd, data, HAWK_SIZEOF(*oocmd) * count);
			oocmd[count] = HAWK_T('\0');
		#if defined(HAWK_OOCH_IS_BCH)
			bcmd = oocmd;
		#else
			bcmd = hawk_gem_duputobcstr(hawk_m4_getgem(m4), oocmd, HAWK_NULL);
			hawk_m4_freemem(m4, oocmd);
			if (!bcmd) return -1;
		#endif
			n = system(bcmd);
			hawk_m4_freemem(m4, bcmd);
			return n == -1? -1: 0;
		}
	}

	hawk_m4_seterrbfmt(m4, HAWK_NULL, HAWK_EINVAL, "invalid m4 I/O command");
	return -1;
}

hawk_m4_t* hawk_m4_openstd (hawk_oow_t xtnsize, hawk_errinf_t* errinf)
{
	return hawk_m4_openstdwithmmgr(hawk_get_sys_mmgr(), xtnsize, hawk_get_cmgr_by_id(HAWK_CMGR_UTF8), errinf);
}

hawk_m4_t* hawk_m4_openstdwithmmgr (hawk_mmgr_t* mmgr, hawk_oow_t xtnsize, hawk_cmgr_t* cmgr, hawk_errinf_t* errinf)
{
	return hawk_m4_open(mmgr, xtnsize, cmgr, errinf);
}

int hawk_m4_execstd (hawk_m4_t* m4, const hawk_ooch_t* const input[])
{
	return hawk_m4_exec(m4, input, std_io);
}
