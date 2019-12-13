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

#include <hawk-sio.h>
#include "hawk-prv.h"

#if defined(_WIN32)
#	include <windows.h> /* for the UGLY hack */
#elif defined(__OS2__)
	/* nothing */
#elif defined(__DOS__)
	/* nothing */
#else
#	include "syscall.h"
#endif


#define LOCK_OUTPUT(sio) do { if ((sio)->mtx) hawk_mtx_lock ((sio)->mtx, HAWK_NULL); } while(0)
#define UNLOCK_OUTPUT(sio) do { if ((sio)->mtx) hawk_mtx_unlock ((sio)->mtx); } while(0)

/* TODO: currently, LOCK_INPUT and LOCK_OUTPUT don't have difference.
 *       can i just use two difference mutex objects to differentiate? */
#define LOCK_INPUT(sio) do { if ((sio)->mtx) hawk_mtx_lock ((sio)->mtx, HAWK_NULL); } while(0)
#define UNLOCK_INPUT(sio) do { if ((sio)->mtx) hawk_mtx_unlock ((sio)->mtx); } while(0)

/* internal status codes */
enum
{
	STATUS_UTF8_CONSOLE = (1 << 0),
	STATUS_LINE_BREAK   = (1 << 1)
};

static hawk_ooi_t file_input (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size);
static hawk_ooi_t file_output (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size);

static hawk_sio_errnum_t fio_errnum_to_sio_errnum (hawk_fio_t* fio)
{
	switch (fio->errnum)
	{
		case HAWK_FIO_ENOMEM:
			return HAWK_SIO_ENOMEM;
		case HAWK_FIO_EINVAL:
			return HAWK_SIO_EINVAL;
		case HAWK_FIO_EACCES:
			return HAWK_SIO_EACCES;
		case HAWK_FIO_ENOENT:
			return HAWK_SIO_ENOENT;
		case HAWK_FIO_EEXIST:
			return HAWK_SIO_EEXIST;
		case HAWK_FIO_EINTR:
			return HAWK_SIO_EINTR;
		case HAWK_FIO_EPIPE:
			return HAWK_SIO_EPIPE;
		case HAWK_FIO_EAGAIN:
			return HAWK_SIO_EAGAIN;
		case HAWK_FIO_ESYSERR:
			return HAWK_SIO_ESYSERR;
		case HAWK_FIO_ENOIMPL:
			return HAWK_SIO_ENOIMPL;
		default:
			return HAWK_SIO_EOTHER;
	}
}

static hawk_sio_errnum_t tio_errnum_to_sio_errnum (hawk_tio_t* tio)
{
	switch (tio->errnum)
	{
		case HAWK_TIO_ENOMEM:
			return HAWK_SIO_ENOMEM;
		case HAWK_TIO_EINVAL:
			return HAWK_SIO_EINVAL;
		case HAWK_TIO_EACCES:
			return HAWK_SIO_EACCES;
		case HAWK_TIO_ENOENT:
			return HAWK_SIO_ENOENT;
		case HAWK_TIO_EILSEQ:
			return HAWK_SIO_EILSEQ;
		case HAWK_TIO_EICSEQ:
			return HAWK_SIO_EICSEQ;
		case HAWK_TIO_EILCHR:
			return HAWK_SIO_EILCHR;
		default:
			return HAWK_SIO_EOTHER;
	}
}

hawk_sio_t* hawk_sio_open (hawk_t* hawk, hawk_oow_t xtnsize, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;

	sio = (hawk_sio_t*)hawk_allocmem(hawk, HAWK_SIZEOF(hawk_sio_t) + xtnsize);
	if (sio)
	{
		if (hawk_sio_init(sio, hawk, file, flags) <= -1)
		{
			hawk_freemem (hawk, sio);
			return HAWK_NULL;
		}
		else HAWK_MEMSET (sio + 1, 0, xtnsize);
	}
	return sio;
}

hawk_sio_t* hawk_sio_openstd (hawk_t* hawk, hawk_oow_t xtnsize, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	hawk_fio_hnd_t hnd;

	/* Is this necessary?
	if (flags & HAWK_SIO_KEEPATH)
	{
		sio->errnum = HAWK_SIO_EINVAL;
		return HAWK_NULL;
	}
	*/

	if (hawk_get_std_fio_handle(std, &hnd) <= -1) return HAWK_NULL;

	sio = hawk_sio_open(hawk, xtnsize, (const hawk_ooch_t*)&hnd, flags | HAWK_SIO_HANDLE | HAWK_SIO_NOCLOSE);

#if defined(_WIN32)
	if (sio) 
	{
		DWORD mode;
		if (GetConsoleMode (sio->file.handle, &mode) == TRUE &&
		    GetConsoleOutputCP() == CP_UTF8)
		{
			sio->status |= STATUS_UTF8_CONSOLE;
		}
	}
#endif

	return sio;
}

void hawk_sio_close (hawk_sio_t* sio)
{
	hawk_sio_fini (sio);
	hawk_freemem (sio->hawk, sio);
}

int hawk_sio_init (hawk_sio_t* sio, hawk_t* hawk, const hawk_ooch_t* path, int flags)
{
	int mode;
	int topt = 0;

	HAWK_MEMSET (sio, 0, HAWK_SIZEOF(*sio));
	sio->hawk = hawk;

	mode = HAWK_FIO_RUSR | HAWK_FIO_WUSR | HAWK_FIO_RGRP | HAWK_FIO_ROTH;

	if (flags & HAWK_SIO_REENTRANT)
	{
		sio->mtx = hawk_mtx_open(hawk, 0);
		if (!sio->mtx) goto oops00;
	}
	/* sio flag enumerators redefines most fio flag enumerators and 
	 * compose a superset of fio flag enumerators. when a user calls 
	 * this function, a user can specify a sio flag enumerator not 
	 * present in the fio flag enumerator. mask off such an enumerator. */
	if (hawk_fio_init(&sio->file, hawk, path, (flags & ~HAWK_FIO_RESERVED), mode) <= -1) 
	{
		sio->errnum = fio_errnum_to_sio_errnum (&sio->file);
		goto oops01;
	}

	if (flags & HAWK_SIO_IGNOREECERR) topt |= HAWK_TIO_IGNOREECERR;
	if (flags & HAWK_SIO_NOAUTOFLUSH) topt |= HAWK_TIO_NOAUTOFLUSH;

	if ((flags & HAWK_SIO_KEEPPATH) && !(flags & HAWK_SIO_HANDLE))
	{
		sio->path = hawk_dupoocstr(hawk, path, HAWK_NULL);
		if (sio->path == HAWK_NULL)
		{
			sio->errnum = HAWK_SIO_ENOMEM;
			goto oops02;
		}
	}

	if (hawk_tio_init(&sio->tio.io, hawk, topt) <= -1)
	{
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
		goto oops03;
	}

	/* store the back-reference to sio in the extension area.*/
	HAWK_ASSERT (hawk, (&sio->tio.io + 1) == &sio->tio.xtn);
	*(hawk_sio_t**)(&sio->tio.io + 1) = sio;

	if (hawk_tio_attachin(&sio->tio.io, file_input, sio->inbuf, HAWK_COUNTOF(sio->inbuf)) <= -1 ||
	    hawk_tio_attachout(&sio->tio.io, file_output, sio->outbuf, HAWK_COUNTOF(sio->outbuf)) <= -1)
	{
		if (sio->errnum == HAWK_SIO_ENOERR) 
			sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
		goto oops04;
	}

#if defined(__OS2__)
	if (flags & HAWK_SIO_LINEBREAK) sio->status |= STATUS_LINE_BREAK;
#endif
	return 0;

oops04:
	hawk_tio_fini (&sio->tio.io);
oops03:
	if (sio->path) hawk_freemem (sio->hawk, sio->path);
oops02:
	hawk_fio_fini (&sio->file);
oops01:
	if (sio->mtx) hawk_mtx_close (sio->mtx);
oops00:
	return -1;
}

int hawk_sio_initstd (hawk_sio_t* sio, hawk_t* hawk, hawk_sio_std_t std, int flags)
{
	int n;
	hawk_fio_hnd_t hnd;

	if (hawk_get_std_fio_handle (std, &hnd) <= -1) return -1;

	n = hawk_sio_init(sio, hawk, (const hawk_ooch_t*)&hnd, flags | HAWK_SIO_HANDLE | HAWK_SIO_NOCLOSE);

#if defined(_WIN32)
	if (n >= 0) 
	{
		DWORD mode;
		if (GetConsoleMode (sio->file.handle, &mode) == TRUE &&
		    GetConsoleOutputCP() == CP_UTF8)
		{
			sio->status |= STATUS_UTF8_CONSOLE;
		}
	}
#endif

	return n;
}

void hawk_sio_fini (hawk_sio_t* sio)
{
	/*if (hawk_sio_flush (sio) <= -1) return -1;*/
	hawk_sio_flush (sio);
	hawk_tio_fini (&sio->tio.io);
	hawk_fio_fini (&sio->file);
	if (sio->path) hawk_freemem (sio->hawk, sio->path);
	if (sio->mtx) hawk_mtx_close (sio->mtx);
}
hawk_sio_errnum_t hawk_sio_geterrnum (const hawk_sio_t* sio)
{
	return sio->errnum;
}

hawk_cmgr_t* hawk_sio_getcmgr (hawk_sio_t* sio)
{
	return hawk_tio_getcmgr (&sio->tio.io);
}

void hawk_sio_setcmgr (hawk_sio_t* sio, hawk_cmgr_t* cmgr)
{
	hawk_tio_setcmgr (&sio->tio.io, cmgr);
}

hawk_sio_hnd_t hawk_sio_gethnd (const hawk_sio_t* sio)
{
	/*return hawk_fio_gethnd (&sio->file);*/
	return HAWK_FIO_HANDLE(&sio->file);
}

const hawk_ooch_t* hawk_sio_getpath (hawk_sio_t* sio)
{
	/* this path is valid if HAWK_SIO_HANDLE is off and HAWK_SIO_KEEPPATH is on */
	return sio->path;
}

hawk_ooi_t hawk_sio_flush (hawk_sio_t* sio)
{
	hawk_ooi_t n;

	LOCK_OUTPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_flush(&sio->tio.io);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
	UNLOCK_OUTPUT (sio);
	return n;
}

void hawk_sio_drain (hawk_sio_t* sio)
{
	LOCK_OUTPUT (sio);
	hawk_tio_drain (&sio->tio.io);
	UNLOCK_OUTPUT (sio);
}

hawk_ooi_t hawk_sio_getbchar (hawk_sio_t* sio, hawk_bch_t* c)
{
	hawk_ooi_t n;

	LOCK_INPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_readbchars(&sio->tio.io, c, 1);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
	UNLOCK_INPUT (sio);

	return n;
}

hawk_ooi_t hawk_sio_getuchar (hawk_sio_t* sio, hawk_uch_t* c)
{
	hawk_ooi_t n;

	LOCK_INPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_readuchars(&sio->tio.io, c, 1);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
	UNLOCK_INPUT (sio);

	return n;
}

hawk_ooi_t hawk_sio_getbcstr (hawk_sio_t* sio, hawk_bch_t* buf, hawk_oow_t size)
{
	hawk_ooi_t n;

	if (size <= 0) return 0;

#if defined(_WIN32)
	/* Using ReadConsoleA() didn't help at all.
	 * so I don't implement any hack here */
#endif

	LOCK_INPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_readbchars(&sio->tio.io, buf, size - 1);
	if (n <= -1) 
	{
		if (sio->errnum == HAWK_SIO_ENOERR)
			sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
		return -1;
	}
	UNLOCK_INPUT (sio);

	buf[n] = HAWK_BT('\0');
	return n;
}

hawk_ooi_t hawk_sio_getbchars (hawk_sio_t* sio, hawk_bch_t* buf, hawk_oow_t size)
{
	hawk_ooi_t n;
#if defined(_WIN32)
	/* Using ReadConsoleA() didn't help at all.
	 * so I don't implement any hack here */
#endif

	LOCK_INPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_readbchars(&sio->tio.io, buf, size);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
	UNLOCK_INPUT (sio);

	return n;
}

hawk_ooi_t hawk_sio_getucstr (hawk_sio_t* sio, hawk_uch_t* buf, hawk_oow_t size)
{
	hawk_ooi_t n;

	if (size <= 0) return 0;

#if defined(_WIN32)
	/* Using ReadConsoleA() didn't help at all.
	 * so I don't implement any hack here */
#endif

	LOCK_INPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_readuchars(&sio->tio.io, buf, size - 1);
	if (n <= -1) 
	{
		if (sio->errnum == HAWK_SIO_ENOERR)
			sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
		return -1;
	}
	UNLOCK_INPUT (sio);

	buf[n] = HAWK_UT('\0');
	return n;
}

hawk_ooi_t hawk_sio_getuchars(hawk_sio_t* sio, hawk_uch_t* buf, hawk_oow_t size)
{
	hawk_ooi_t n;

#if defined(_WIN32)
	/* Using ReadConsoleW() didn't help at all.
	 * so I don't implement any hack here */
#endif

	LOCK_INPUT (sio);
	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_readuchars (&sio->tio.io, buf, size);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);
	UNLOCK_INPUT (sio);

	return n;
}

static hawk_ooi_t putbc_no_mutex (hawk_sio_t* sio, hawk_bch_t c)
{
	hawk_ooi_t n;

	sio->errnum = HAWK_SIO_ENOERR;

#if defined(__OS2__)
	if (c == HAWK_BT('\n') && (sio->status & STATUS_LINE_BREAK))
		n = hawk_tio_writebchars (&sio->tio.io, HAWK_BT("\r\n"), 2);
	else
		n = hawk_tio_writebchars (&sio->tio.io, &c, 1);
#else
	n = hawk_tio_writebchars (&sio->tio.io, &c, 1);
#endif

	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);

	return n;
}

hawk_ooi_t hawk_sio_putbchar (hawk_sio_t* sio, hawk_bch_t c)
{
	hawk_ooi_t n;
	LOCK_OUTPUT (sio);
	n = putbc_no_mutex(sio, c);
	UNLOCK_OUTPUT (sio);
	return n;
}

static hawk_ooi_t put_uchar_no_mutex (hawk_sio_t* sio, hawk_uch_t c)
{
	hawk_ooi_t n;

	sio->errnum = HAWK_SIO_ENOERR;
#if defined(__OS2__)
	if (c == HAWK_UT('\n') && (sio->status & STATUS_LINE_BREAK))
		n = hawk_tio_writeuchars(&sio->tio.io, HAWK_UT("\r\n"), 2);
	else
		n = hawk_tio_writeuchars(&sio->tio.io, &c, 1);
#else
	n = hawk_tio_writeuchars(&sio->tio.io, &c, 1);
#endif
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);

	return n;
}

hawk_ooi_t hawk_sio_putuchar (hawk_sio_t* sio, hawk_uch_t c)
{
	hawk_ooi_t n;
	LOCK_OUTPUT (sio);
	n = put_uchar_no_mutex(sio, c);
	UNLOCK_OUTPUT (sio);
	return n;
}

hawk_ooi_t hawk_sio_putbcstr (hawk_sio_t* sio, const hawk_bch_t* str)
{
	hawk_ooi_t n;

#if defined(_WIN32)
	/* Using WriteConsoleA() didn't help at all.
	 * so I don't implement any hacks here */
#elif defined(__OS2__)
	if (sio->status & STATUS_LINE_BREAK)
	{
		LOCK_OUTPUT (sio);
		for (n = 0; n < HAWK_TYPE_MAX(hawk_ooi_t) && str[n] != HAWK_BT('\0'); n++)
		{
			if ((n = putbc_no_mutex(sio, str[n])) <= -1) return n;
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);

	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_writebchars(&sio->tio.io, str, (hawk_oow_t)-1);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum(&sio->tio.io);

	UNLOCK_OUTPUT (sio);
	return n;
}

hawk_ooi_t hawk_sio_putbchars (hawk_sio_t* sio, const hawk_bch_t* str, hawk_oow_t size)
{
	hawk_ooi_t n;

#if defined(_WIN32)
	/* Using WriteConsoleA() didn't help at all.
	 * so I don't implement any hacks here */
#elif defined(__OS2__)
	if (sio->status & STATUS_LINE_BREAK)
	{
		if (size > HAWK_TYPE_MAX(hawk_ooi_t)) size = HAWK_TYPE_MAX(hawk_ooi_t);
		LOCK_OUTPUT (sio);
		for (n = 0; n < size; n++)
		{
			if (putbc_no_mutex(sio, str[n]) <= -1) return -1;
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);

	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_writebchars (&sio->tio.io, str, size);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);

	UNLOCK_OUTPUT (sio);

	return n;
}

hawk_ooi_t hawk_sio_putucstr (hawk_sio_t* sio, const hawk_uch_t* str)
{
	hawk_ooi_t n;

#if defined(_WIN32)
	/* DAMN UGLY: See comment in hawk_sio_putuchars() */
	if (sio->status & STATUS_UTF8_CONSOLE)
	{
		DWORD count, left;
		const hawk_uch_t* cur;

		if (hawk_sio_flush (sio) <= -1) return -1; /* can't do buffering */

		for (cur = str, left = hawk_wcslen(str); left > 0; cur += count, left -= count)
		{
			if (WriteConsoleW(sio->file.handle, cur, left, &count, HAWK_NULL) == FALSE) 
			{
				sio->errnum = HAWK_SIO_ESYSERR;
				return -1;
			}
			if (count == 0) break;

			if (count > left) 
			{
				sio->errnum = HAWK_SIO_ESYSERR;
				return -1;
			}
		}
		return cur - str;
	}
#elif defined(__OS2__)
	if (sio->status & STATUS_LINE_BREAK)
	{
		LOCK_OUTPUT (sio);
		for (n = 0; n < HAWK_TYPE_MAX(hawk_ooi_t) && str[n] != HAWK_UT('\0'); n++)
		{
			if (put_uchar_no_mutex(sio, str[n]) <= -1) return -1;
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);

	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_writeuchars(&sio->tio.io, str, (hawk_oow_t)-1);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);

	UNLOCK_OUTPUT (sio);
	return n;
}

hawk_ooi_t hawk_sio_putuchars (hawk_sio_t* sio, const hawk_uch_t* str, hawk_oow_t size)
{
	hawk_ooi_t n;

#if defined(_WIN32)
	/* DAMN UGLY:
	 *  WriteFile returns wrong number of bytes written if it is 
	 *  requested to write a utf8 string on utf8 console (codepage 65001). 
	 *  it seems to return a number of characters written instead. so 
	 *  i have to use an alternate API for console output for 
	 *  wide-character strings. Conversion to either an OEM codepage or 
	 *  the utf codepage is handled by the API. This hack at least
	 *  lets you do proper utf8 output on utf8 console using wide-character.
	 * 
	 *  Note that the multibyte functions hawk_sio_putbcstr() and
	 *  hawk_sio_putbchars() doesn't handle this. So you may still suffer.
	 */
	if (sio->status & STATUS_UTF8_CONSOLE)
	{
		DWORD count, left;
		const hawk_uch_t* cur;

		if (hawk_sio_flush (sio) <= -1) return -1; /* can't do buffering */

		for (cur = str, left = size; left > 0; cur += count, left -= count)
		{
			if (WriteConsoleW(sio->file.handle, cur, left, &count, HAWK_NULL) == FALSE) 
			{
				sio->errnum = HAWK_SIO_ESYSERR;
				return -1;
			}
			if (count == 0) break;

			/* Note:
			 * WriteConsoleW() in unicosw.dll on win 9x/me returns
			 * the number of bytes via 'count'. If a double byte 
			 * string is given, 'count' can be greater than 'left'.
			 * this case is a miserable failure. however, i don't
			 * think there is CP_UTF8 codepage for console on win9x/me.
			 * so let me make this function fail if that ever happens.
			 */
			if (count > left) 
			{
				sio->errnum = HAWK_SIO_ESYSERR;
				return -1;
			}
		}
		return cur - str;
	}

#elif defined(__OS2__) 
	if (sio->status & STATUS_LINE_BREAK)
	{
		if (size > HAWK_TYPE_MAX(hawk_ooi_t)) size = HAWK_TYPE_MAX(hawk_ooi_t);
		LOCK_OUTPUT (sio);
		for (n = 0; n < size; n++)
		{
			if (put_uchar_no_mutex(sio, str[n]) <= -1) return -1;
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);

	sio->errnum = HAWK_SIO_ENOERR;
	n = hawk_tio_writeuchars(&sio->tio.io, str, size);
	if (n <= -1 && sio->errnum == HAWK_SIO_ENOERR) 
		sio->errnum = tio_errnum_to_sio_errnum (&sio->tio.io);

	UNLOCK_OUTPUT (sio);
	return n;
}

int hawk_sio_getpos (hawk_sio_t* sio, hawk_sio_pos_t* pos)
{
	hawk_fio_off_t off;

	if (hawk_sio_flush(sio) <= -1) return -1;

	off = hawk_fio_seek(&sio->file, 0, HAWK_FIO_CURRENT);
	if (off == (hawk_fio_off_t)-1) 
	{
		sio->errnum = fio_errnum_to_sio_errnum(&sio->file);
		return -1;
	}

	*pos = off;
	return 0;
}

int hawk_sio_setpos (hawk_sio_t* sio, hawk_sio_pos_t pos)
{
	hawk_fio_off_t off;

	if (hawk_sio_flush(sio) <= -1) return -1;

	off = hawk_fio_seek(&sio->file, pos, HAWK_FIO_BEGIN);
	if (off == (hawk_fio_off_t)-1)
	{
		sio->errnum = fio_errnum_to_sio_errnum(&sio->file);
		return -1;
	}

	return 0;
}

int hawk_sio_truncate (hawk_sio_t* sio, hawk_sio_pos_t pos)
{
	if (hawk_sio_flush(sio) <= -1) return -1;
	return hawk_fio_truncate(&sio->file, pos);
}

int hawk_sio_seek (hawk_sio_t* sio, hawk_sio_pos_t* pos, hawk_sio_ori_t origin)
{
	hawk_fio_off_t x;

	if (hawk_sio_flush(sio) <= -1) return -1;
	x = hawk_fio_seek(&sio->file, *pos, origin);
	if (x == (hawk_fio_off_t)-1) return -1;

	*pos = x;
	return 0;
}

static hawk_ooi_t file_input (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size)
{
	if (cmd == HAWK_TIO_DATA) 
	{
		hawk_ooi_t n;
		hawk_sio_t* sio;

		sio = *(hawk_sio_t**)(tio + 1);
		HAWK_ASSERT (tio->hawk, sio != HAWK_NULL);
		HAWK_ASSERT (tio->hawk, tio->hawk == sio->hawk);

		n = hawk_fio_read(&sio->file, buf, size);
		if (n <= -1) sio->errnum = fio_errnum_to_sio_errnum (&sio->file);
		return n;
	}

	return 0;
}

static hawk_ooi_t file_output (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size)
{
	if (cmd == HAWK_TIO_DATA) 
	{
		hawk_ooi_t n;
		hawk_sio_t* sio;

		sio = *(hawk_sio_t**)(tio + 1);
		HAWK_ASSERT (tio->hawk, sio != HAWK_NULL);
		HAWK_ASSERT (tio->hawk, tio->hawk == sio->hawk);

		n = hawk_fio_write(&sio->file, buf, size);
		if (n <= -1) sio->errnum = fio_errnum_to_sio_errnum(&sio->file);
		return n;
	}

	return 0;
}
