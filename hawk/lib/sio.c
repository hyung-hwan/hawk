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

#include "syserr.h"
IMPLEMENT_SYSERR_TO_ERRNUM (hawk, HAWK)

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

hawk_sio_t* hawk_sio_open (hawk_gem_t* gem, hawk_oow_t xtnsize, const hawk_ooch_t* file, int flags)
{
	hawk_sio_t* sio;

	sio = (hawk_sio_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_sio_t) + xtnsize);
	if (sio)
	{
		if (hawk_sio_init(sio, gem, file, flags) <= -1)
		{
			hawk_gem_freemem (gem, sio);
			return HAWK_NULL;
		}
		else HAWK_MEMSET (sio + 1, 0, xtnsize);
	}
	return sio;
}

hawk_sio_t* hawk_sio_openstd (hawk_gem_t* gem, hawk_oow_t xtnsize, hawk_sio_std_t std, int flags)
{
	hawk_sio_t* sio;
	hawk_fio_hnd_t hnd;

	/* Is this necessary?
	if (flags & HAWK_SIO_KEEPATH)
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_EINVAL);
		return HAWK_NULL;
	}
	*/

	if (hawk_get_std_fio_handle(std, &hnd) <= -1) return HAWK_NULL;

	sio = hawk_sio_open(gem, xtnsize, (const hawk_ooch_t*)&hnd, flags | HAWK_SIO_HANDLE | HAWK_SIO_NOCLOSE);

#if defined(_WIN32)
	if (sio) 
	{
		DWORD mode;
		if (GetConsoleMode(sio->file.handle, &mode) == TRUE && GetConsoleOutputCP() == CP_UTF8)
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
	hawk_gem_freemem (sio->gem, sio);
}

int hawk_sio_init (hawk_sio_t* sio, hawk_gem_t* gem, const hawk_ooch_t* path, int flags)
{
	int mode;
	int topt = 0;

	HAWK_MEMSET (sio, 0, HAWK_SIZEOF(*sio));
	sio->gem = gem;

	mode = HAWK_FIO_RUSR | HAWK_FIO_WUSR | HAWK_FIO_RGRP | HAWK_FIO_ROTH;

	if (flags & HAWK_SIO_REENTRANT)
	{
		sio->mtx = hawk_mtx_open(gem, 0);
		if (!sio->mtx) goto oops00;
	}
	/* sio flag enumerators redefines most fio flag enumerators and 
	 * compose a superset of fio flag enumerators. when a user calls 
	 * this function, a user can specify a sio flag enumerator not 
	 * present in the fio flag enumerator. mask off such an enumerator. */
	if (hawk_fio_init(&sio->file, gem, path, (flags & ~HAWK_FIO_RESERVED), mode) <= -1) goto oops01;

	if (flags & HAWK_SIO_IGNOREECERR) topt |= HAWK_TIO_IGNOREECERR;
	if (flags & HAWK_SIO_NOAUTOFLUSH) topt |= HAWK_TIO_NOAUTOFLUSH;

	if ((flags & HAWK_SIO_KEEPPATH) && !(flags & HAWK_SIO_HANDLE))
	{
		sio->path = hawk_gem_dupoocstr(gem, path, HAWK_NULL);
		if (sio->path == HAWK_NULL) goto oops02;
	}

	if (hawk_tio_init(&sio->tio.io, gem, topt) <= -1) goto oops03;

	/* store the back-reference to sio in the extension area.*/
	/*HAWK_ASSERT (hawk, (&sio->tio.io + 1) == &sio->tio.xtn);*/
	*(hawk_sio_t**)(&sio->tio.io + 1) = sio;

	if (hawk_tio_attachin(&sio->tio.io, file_input, sio->inbuf, HAWK_COUNTOF(sio->inbuf)) <= -1 ||
	    hawk_tio_attachout(&sio->tio.io, file_output, sio->outbuf, HAWK_COUNTOF(sio->outbuf)) <= -1)
	{
		goto oops04;
	}

#if defined(__OS2__)
	if (flags & HAWK_SIO_LINEBREAK) sio->status |= STATUS_LINE_BREAK;
#endif
	return 0;

oops04:
	hawk_tio_fini (&sio->tio.io);
oops03:
	if (sio->path) hawk_gem_freemem (sio->gem, sio->path);
oops02:
	hawk_fio_fini (&sio->file);
oops01:
	if (sio->mtx) hawk_mtx_close (sio->mtx);
oops00:
	return -1;
}

int hawk_sio_initstd (hawk_sio_t* sio, hawk_gem_t* gem, hawk_sio_std_t std, int flags)
{
	int n;
	hawk_fio_hnd_t hnd;

	if (hawk_get_std_fio_handle (std, &hnd) <= -1) return -1;

	n = hawk_sio_init(sio, gem, (const hawk_ooch_t*)&hnd, flags | HAWK_SIO_HANDLE | HAWK_SIO_NOCLOSE);

#if defined(_WIN32)
	if (n >= 0) 
	{
		DWORD mode;
		if (GetConsoleMode (sio->file.handle, &mode) == TRUE && GetConsoleOutputCP() == CP_UTF8)
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
	if (sio->path) hawk_gem_freemem (sio->gem, sio->path);
	if (sio->mtx) hawk_mtx_close (sio->mtx);
}

hawk_cmgr_t* hawk_sio_getcmgr (hawk_sio_t* sio)
{
	return hawk_tio_getcmgr(&sio->tio.io);
}

void hawk_sio_setcmgr (hawk_sio_t* sio, hawk_cmgr_t* cmgr)
{
	hawk_tio_setcmgr (&sio->tio.io, cmgr);
}

hawk_sio_hnd_t hawk_sio_gethnd (const hawk_sio_t* sio)
{
	/*return hawk_fio_gethnd(&sio->file);*/
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
	n = hawk_tio_flush(&sio->tio.io);
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
	n = hawk_tio_readbchars(&sio->tio.io, c, 1);
	UNLOCK_INPUT (sio);
	return n;
}

hawk_ooi_t hawk_sio_getuchar (hawk_sio_t* sio, hawk_uch_t* c)
{
	hawk_ooi_t n;
	LOCK_INPUT (sio);
	n = hawk_tio_readuchars(&sio->tio.io, c, 1);
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
	n = hawk_tio_readbchars(&sio->tio.io, buf, size - 1);
	if (n <= -1) return -1;
	UNLOCK_INPUT (sio);

	buf[n] = '\0';
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
	n = hawk_tio_readbchars(&sio->tio.io, buf, size);
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
	n = hawk_tio_readuchars(&sio->tio.io, buf, size - 1);
	UNLOCK_INPUT (sio);

	buf[n] = '\0';
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
	n = hawk_tio_readuchars (&sio->tio.io, buf, size);
	UNLOCK_INPUT (sio);

	return n;
}

static hawk_ooi_t putbc_no_mutex (hawk_sio_t* sio, hawk_bch_t c)
{
	hawk_ooi_t n;

#if defined(__OS2__)
	if (c == '\n' && (sio->status & STATUS_LINE_BREAK))
		n = hawk_tio_writebchars(&sio->tio.io, "\r\n", 2);
	else
		n = hawk_tio_writebchars(&sio->tio.io, &c, 1);
#else
	n = hawk_tio_writebchars(&sio->tio.io, &c, 1);
#endif

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

#if defined(__OS2__)
	if (c == '\n' && (sio->status & STATUS_LINE_BREAK))
	{
		static hawk_uch_t crnl[2] = { '\r', '\n' };
		n = hawk_tio_writeuchars(&sio->tio.io, crnl, 2);
	}
	else
		n = hawk_tio_writeuchars(&sio->tio.io, &c, 1);
#else
	n = hawk_tio_writeuchars(&sio->tio.io, &c, 1);
#endif

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
		for (n = 0; n < HAWK_TYPE_MAX(hawk_ooi_t) && str[n] != '\0'; n++)
		{
			if ((n = putbc_no_mutex(sio, str[n])) <= -1) 
			{
				n = -1;
				break;
			}
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);
	n = hawk_tio_writebchars(&sio->tio.io, str, (hawk_oow_t)-1);
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
			if (putbc_no_mutex(sio, str[n]) <= -1)
			{
				n = -1;
				break;
			}
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);
	n = hawk_tio_writebchars (&sio->tio.io, str, size);
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

		for (cur = str, left = hawk_count_ucstr(str); left > 0; cur += count, left -= count)
		{
			if (WriteConsoleW(sio->file.handle, cur, left, &count, HAWK_NULL) == FALSE) 
			{
				hawk_gem_seterrnum (sio->gem, HAWK_NULL, syserr_to_errnum(GetLastError()));
				return -1;
			}
			if (count == 0) break;

			if (count > left) 
			{
				hawk_gem_seterrnum (sio->gem, HAWK_NULL, HAWK_ESYSERR);
				return -1;
			}
		}
		return cur - str;
	}
#elif defined(__OS2__)
	if (sio->status & STATUS_LINE_BREAK)
	{
		LOCK_OUTPUT (sio);
		for (n = 0; n < HAWK_TYPE_MAX(hawk_ooi_t) && str[n] != '\0'; n++)
		{
			if (put_uchar_no_mutex(sio, str[n]) <= -1)
			{
				n = -1;
				break;
			}
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);
	n = hawk_tio_writeuchars(&sio->tio.io, str, (hawk_oow_t)-1);
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
				hawk_gem_seterrnum (sio->gem, HAWK_NULL, syserr_to_errnum(GetLastError()));
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
				hawk_gem_seterrnum (sio->gem, HAWK_NULL, HAWK_ESYSERR);
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
			if (put_uchar_no_mutex(sio, str[n]) <= -1) 
			{
				n = -1;
				break;
			}
		}
		UNLOCK_OUTPUT (sio);
		return n;
	}
#endif

	LOCK_OUTPUT (sio);
	n = hawk_tio_writeuchars(&sio->tio.io, str, size);
	UNLOCK_OUTPUT (sio);
	return n;
}

int hawk_sio_getpos (hawk_sio_t* sio, hawk_sio_pos_t* pos)
{
	hawk_fio_off_t off;

	if (hawk_sio_flush(sio) <= -1) return -1;

	off = hawk_fio_seek(&sio->file, 0, HAWK_FIO_CURRENT);
	if (off == (hawk_fio_off_t)-1) return -1;

	*pos = off;
	return 0;
}

int hawk_sio_setpos (hawk_sio_t* sio, hawk_sio_pos_t pos)
{
	hawk_fio_off_t off;

	if (hawk_sio_flush(sio) <= -1) return -1;

	off = hawk_fio_seek(&sio->file, pos, HAWK_FIO_BEGIN);
	if (off == (hawk_fio_off_t)-1) return -1;

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
		hawk_sio_t* sio;

		sio = *(hawk_sio_t**)(tio + 1);
		/*HAWK_ASSERT (tio->hawk, sio != HAWK_NULL);
		HAWK_ASSERT (tio->hawk, tio->hawk == sio->hawk);*/

		return hawk_fio_read(&sio->file, buf, size);
	}

	return 0;
}

static hawk_ooi_t file_output (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size)
{
	if (cmd == HAWK_TIO_DATA) 
	{
		hawk_sio_t* sio;

		sio = *(hawk_sio_t**)(tio + 1);
		/*HAWK_ASSERT (tio->hawk, sio != HAWK_NULL);
		HAWK_ASSERT (tio->hawk, tio->hawk == sio->hawk);*/

		return hawk_fio_write(&sio->file, buf, size);
	}

	return 0;
}
