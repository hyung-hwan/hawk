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
 
#ifndef _HAWK_SIO_H_
#define _HAWK_SIO_H_

#include <hawk-cmn.h>
#include <hawk-fio.h>
#include <hawk-tio.h>
#include <hawk-mtx.h>
#include <stdarg.h>

enum hawk_sio_flag_t
{
	/* ensure that these enumerators never overlap with
	 * hawk_fio_flag_t enumerators. you can use values between
	 * (1<<0) and (1<<7) inclusive reserved in hawk_fio_flag_t.
	 * the range is represented by HAWK_FIO_RESERVED. */
	HAWK_SIO_LINEBREAK     = (1 << 0), /* expand \n to a system line-break convention if necessary */
	HAWK_SIO_IGNOREECERR   = (1 << 1),
	HAWK_SIO_NOAUTOFLUSH   = (1 << 2),
	HAWK_SIO_KEEPPATH      = (1 << 3),
	HAWK_SIO_REENTRANT     = (1 << 4),

	/* ensure that the following enumerators are one of
	 * hawk_fio_flags_t enumerators */
	HAWK_SIO_HANDLE        = HAWK_FIO_HANDLE,
	HAWK_SIO_NOCLOSE       = HAWK_FIO_NOCLOSE,
	HAWK_SIO_READ          = HAWK_FIO_READ,
	HAWK_SIO_WRITE         = HAWK_FIO_WRITE,
	HAWK_SIO_APPEND        = HAWK_FIO_APPEND,
	HAWK_SIO_CREATE        = HAWK_FIO_CREATE,
	HAWK_SIO_TRUNCATE      = HAWK_FIO_TRUNCATE,
	HAWK_SIO_EXCLUSIVE     = HAWK_FIO_EXCLUSIVE,
	HAWK_SIO_SYNC          = HAWK_FIO_SYNC,
	HAWK_SIO_NOFOLLOW      = HAWK_FIO_NOFOLLOW,
	HAWK_SIO_NOSHREAD      = HAWK_FIO_NOSHREAD,
	HAWK_SIO_NOSHWRITE     = HAWK_FIO_NOSHWRITE,
	HAWK_SIO_NOSHDELETE    = HAWK_FIO_NOSHDELETE,
	HAWK_SIO_RANDOM        = HAWK_FIO_RANDOM,
	HAWK_SIO_SEQUENTIAL    = HAWK_FIO_SEQUENTIAL
};

typedef hawk_fio_off_t hawk_sio_pos_t;
typedef hawk_fio_hnd_t hawk_sio_hnd_t;
typedef hawk_fio_std_t hawk_sio_std_t;
typedef hawk_fio_ori_t hawk_sio_ori_t;

#define HAWK_SIO_STDIN  HAWK_FIO_STDIN
#define HAWK_SIO_STDOUT HAWK_FIO_STDOUT
#define HAWK_SIO_STDERR HAWK_FIO_STDERR

#define HAWK_SIO_BEGIN   HAWK_FIO_BEGIN
#define HAWK_SIO_CURRENT HAWK_FIO_CURRENT
#define HAWK_SIO_END     HAWK_FIO_END

/**
 * The hawk_sio_t type defines a simple text stream over a file. It also
 * provides predefined streams for standard input, output, and error.
 */
typedef struct hawk_sio_t hawk_sio_t;

struct hawk_sio_t
{
	hawk_gem_t* gem;

	hawk_fio_t file;

	struct
	{
		hawk_tio_t  io;
		hawk_sio_t* xtn; /* static extension for tio */
	} tio;

	hawk_bch_t inbuf[2048];
	hawk_bch_t outbuf[2048];

	hawk_ooch_t* path;
	hawk_mtx_t* mtx;

#if defined(_WIN32) || defined(__OS2__)
	int status;
#endif
};

/** access the @a errnum field of the #hawk_sio_t structure */
#define HAWK_SIO_ERRNUM(sio)    ((sio)->errnum)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_sio_open() fucntion creates a stream object.
 */
HAWK_EXPORT hawk_sio_t* hawk_sio_open (
	hawk_gem_t*        gem,
	hawk_oow_t         xtnsize, /**< extension size in bytes */
	const hawk_ooch_t* file,    /**< file name */
	int                flags    /**< number OR'ed of #hawk_sio_flag_t */
);

HAWK_EXPORT hawk_sio_t* hawk_sio_openstd (
	hawk_gem_t*        gem,
	hawk_oow_t         xtnsize, /**< extension size in bytes */
	hawk_sio_std_t     std,     /**< standard I/O identifier */
	int                flags    /**< number OR'ed of #hawk_sio_flag_t */
);

/**
 * The hawk_sio_close() function destroys a stream object.
 */
HAWK_EXPORT void hawk_sio_close (
	hawk_sio_t* sio  /**< stream */
);

HAWK_EXPORT int hawk_sio_init (
	hawk_sio_t*        sio,
	hawk_gem_t*        gem,
	const hawk_ooch_t* file,
	int                flags
);

HAWK_EXPORT int hawk_sio_initstd (
	hawk_sio_t*        sio,
	hawk_gem_t*        gem,
	hawk_sio_std_t     std,
	int                flags
);

HAWK_EXPORT void hawk_sio_fini (
	hawk_sio_t* sio
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_sio_getxtn (hawk_sio_t* sio) { return (void*)(sio + 1); }
#else
#define hawk_sio_getxtn(sio) ((void*)((hawk_sio_t*)(sio) + 1))
#endif

HAWK_EXPORT hawk_cmgr_t* hawk_sio_getcmgr (
	hawk_sio_t* sio
);

HAWK_EXPORT void hawk_sio_setcmgr (
	hawk_sio_t*  sio,
	hawk_cmgr_t* cmgr
);

HAWK_EXPORT hawk_sio_hnd_t hawk_sio_gethnd (
	const hawk_sio_t* sio
);

/** 
 * The hawk_sio_getpath() returns the file path used to open the stream.
 * It returns #HAWK_NULL if #HAWK_SIO_HANDLE was on or #HAWK_SIO_KEEPPATH 
 * was off at the time of opening.
 */
HAWK_EXPORT const hawk_ooch_t* hawk_sio_getpath (
	hawk_sio_t* sio
);

HAWK_EXPORT hawk_ooi_t hawk_sio_flush (
	hawk_sio_t* sio
);

/**
 * The hawk_sio_drain() funtion purges all buffered data without writing.
 */
HAWK_EXPORT void hawk_sio_drain (
	hawk_sio_t* sio
);

HAWK_EXPORT hawk_ooi_t hawk_sio_getbchar (
	hawk_sio_t*  sio,
	hawk_bch_t*  c
);

HAWK_EXPORT hawk_ooi_t hawk_sio_getuchar (
	hawk_sio_t*  sio,
	hawk_uch_t*  c
);

HAWK_EXPORT hawk_ooi_t hawk_sio_getbcstr (
	hawk_sio_t*  sio,
	hawk_bch_t*  buf,
	hawk_oow_t   size
);

HAWK_EXPORT hawk_ooi_t hawk_sio_getbchars (
	hawk_sio_t*  sio,
	hawk_bch_t*  buf,
	hawk_oow_t   size
);

/**
 * The hawk_sio_getucstr() function reads at most @a size - 1 characters 
 * from the stream @a sio into the buffer @a buf. If a new line or EOF
 * is encountered, it stops reading from the stream. It null-terminates
 * the buffer if @a size is greater than 0. 
 */
HAWK_EXPORT hawk_ooi_t hawk_sio_getucstr (
	hawk_sio_t*  sio,
	hawk_uch_t*  buf,
	hawk_oow_t   size
);

/**
 * The hawk_sio_getuchars() function reads at most @a size characters 
 * from the stream @a sio into the buffer @a buf. If a new line or EOF
 * is encountered, it stops reading from the stream. 
 */
HAWK_EXPORT hawk_ooi_t hawk_sio_getuchars (
	hawk_sio_t*  sio,
	hawk_uch_t*  buf,
	hawk_oow_t   size
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_sio_getoochar hawk_sio_getbchar
#	define hawk_sio_getoocstr hawk_sio_getbcstr
#	define hawk_sio_getoochars hawk_sio_getbchars
#else
#	define hawk_sio_getoochar hawk_sio_getuchar
#	define hawk_sio_getoocstr hawk_sio_getucstr
#	define hawk_sio_getoochars hawk_sio_getuchars
#endif

HAWK_EXPORT hawk_ooi_t hawk_sio_putbchar (
	hawk_sio_t*  sio, 
	hawk_bch_t   c
);

HAWK_EXPORT hawk_ooi_t hawk_sio_putuchar (
	hawk_sio_t*  sio, 
	hawk_uch_t   c
);

HAWK_EXPORT hawk_ooi_t hawk_sio_putbcstr (
	hawk_sio_t*       sio,
	const hawk_bch_t* str
);

HAWK_EXPORT hawk_ooi_t hawk_sio_putucstr (
	hawk_sio_t*       sio,
	const hawk_uch_t* str
);


HAWK_EXPORT hawk_ooi_t hawk_sio_putbchars (
	hawk_sio_t*       sio, 
	const hawk_bch_t* str,
	hawk_oow_t        size
);

HAWK_EXPORT hawk_ooi_t hawk_sio_putuchars (
	hawk_sio_t*        sio, 
	const hawk_uch_t*  str,
	hawk_oow_t         size
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_sio_putoochar hawk_sio_putbchar
#	define hawk_sio_putoocstr hawk_sio_putbcstr
#	define hawk_sio_putoochars hawk_sio_putbchars
#else
#	define hawk_sio_putoochar hawk_sio_putuchar
#	define hawk_sio_putoocstr hawk_sio_putucstr
#	define hawk_sio_putoochars hawk_sio_putuchars
#endif

/**
 * The hawk_sio_getpos() function gets the current position in a stream.
 * Note that it may not return the desired postion due to buffering.
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_sio_getpos (
	hawk_sio_t*     sio,  /**< stream */
	hawk_sio_pos_t* pos   /**< position */
);

/**
 * The hawk_sio_setpos() function changes the current position in a stream.
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_sio_setpos (
	hawk_sio_t*    sio,   /**< stream */
	hawk_sio_pos_t pos    /**< position */
);

HAWK_EXPORT int hawk_sio_truncate (
	hawk_sio_t*    sio,
	hawk_sio_pos_t pos
);

/**
 * The hawk_sio_seek() function repositions the current file offset.
 * Upon success, \a pos is adjusted to the new offset from the beginning
 * of the file.
 */
HAWK_EXPORT int hawk_sio_seek (
	hawk_sio_t*     sio, 
	hawk_sio_pos_t* pos,
	hawk_sio_ori_t  origin
);

#if defined(__cplusplus)
}
#endif

#endif
