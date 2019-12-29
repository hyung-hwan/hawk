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

#ifndef _HAWK_TIO_H_
#define _HAWK_TIO_H_

#include <hawk-cmn.h>

enum hawk_tio_cmd_t
{
	HAWK_TIO_OPEN,
	HAWK_TIO_CLOSE,
	HAWK_TIO_DATA
};
typedef enum hawk_tio_cmd_t hawk_tio_cmd_t;

enum hawk_tio_flag_t
{
	/**< ignore multibyte/wide-character conversion error by
	 *   inserting a question mark for each error occurrence */
	HAWK_TIO_IGNOREECERR = (1 << 0),

	/**< do not flush data in the buffer until the buffer gets full. */
	HAWK_TIO_NOAUTOFLUSH   = (1 << 1)
};

enum hawk_tio_misc_t
{
	HAWK_TIO_MININBUFCAPA  = 32,
	HAWK_TIO_MINOUTBUFCAPA = 32
};

typedef struct hawk_tio_t hawk_tio_t;

/**
 * The hawk_tio_io_impl_t types define a text I/O handler.
 */
typedef hawk_ooi_t (*hawk_tio_io_impl_t) (
	hawk_tio_t*    tio,
	hawk_tio_cmd_t cmd, 
	void*         data, 
	hawk_oow_t    size
);

struct hawk_tio_io_t
{
	hawk_tio_io_impl_t fun;
	struct
	{
		hawk_oow_t   capa;
		hawk_bch_t* ptr;
	} buf;
};

typedef struct hawk_tio_io_t hawk_tio_io_t;

/**
 * The hawk_tio_t type defines a generic type for text IO. If #hawk_ooch_t is
 * #hawk_bch_t, it handles any byte streams. If hawk_ooch_t is #hawk_uch_t,
 * it handles a multi-byte stream converted to a wide character stream.
 */
struct hawk_tio_t
{
	hawk_gem_t*       gem;
	int               flags;
	hawk_cmgr_t*      cmgr;

	hawk_tio_io_t     in;
	hawk_tio_io_t     out;

	/* for house keeping from here */
	int               status;
	hawk_oow_t       inbuf_cur;
	hawk_oow_t       inbuf_len;
	hawk_oow_t       outbuf_len;
};

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_tio_open() function creates an text stream processoor.
 */
HAWK_EXPORT hawk_tio_t* hawk_tio_open (
	hawk_gem_t* gem,
	hawk_oow_t  xtnsize, /**< extension size in bytes */
	int         flags    /**< ORed of hawk_tio_flag_t enumerators */
);

/**
 * The hawk_tio_close() function destroys an text stream processor.
 */
HAWK_EXPORT int hawk_tio_close (
	hawk_tio_t* tio
);

/**
 * The hawk_tio_init() function  initialize a statically declared 
 * text stream processor.
 */
HAWK_EXPORT int hawk_tio_init (
	hawk_tio_t*  tio,
	hawk_gem_t*  gem,
	int          flags
);

/**
 * The hawk_tio_fini() function finalizes a text stream processor
 */
HAWK_EXPORT int hawk_tio_fini (
	hawk_tio_t* tio
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_tio_getxtn (hawk_tio_t* tio) { return (void*)(tio + 1); }
#else
#define hawk_tio_getxtn(awk) ((void*)((hawk_tio_t*)(tio) + 1))
#endif

/**
 * The hawk_tio_getcmgr() function returns the character manager.
 */
HAWK_EXPORT hawk_cmgr_t* hawk_tio_getcmgr (
	hawk_tio_t* tio
);

/**
 * The hawk_tio_setcmgr() function changes the character manager.
 */
HAWK_EXPORT void hawk_tio_setcmgr (
	hawk_tio_t*  tio,
	hawk_cmgr_t* cmgr
);

/**
 * The hawk_tio_attachin() function attachs an input handler .
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_tio_attachin (
	hawk_tio_t*        tio,
	hawk_tio_io_impl_t input,
	hawk_bch_t*        bufptr,
	hawk_oow_t         bufcapa
);

/**
 * The hawk_tio_detachin() function detaches an input handler .
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_tio_detachin (
	hawk_tio_t* tio
);

/**
 * The hawk_tio_attachout() function attaches an output handler.
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_tio_attachout (
	hawk_tio_t*        tio,
	hawk_tio_io_impl_t output,
	hawk_bch_t*        bufptr,
	hawk_oow_t         bufcapa
);

/**
 * The hawk_tio_detachout() function detaches an output handler .
 * @return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_tio_detachout (
	hawk_tio_t* tio
);

/**
 * The hawk_tio_flush() function flushes the output buffer. It returns the 
 * number of bytes written on success, -1 on failure.
 */
HAWK_EXPORT hawk_ooi_t hawk_tio_flush (
	hawk_tio_t* tio
);

/**
 * The hawk_tio_drain() function empties input and output buffers.
 */
HAWK_EXPORT void hawk_tio_drain (
	hawk_tio_t* tio
);

HAWK_EXPORT hawk_ooi_t hawk_tio_readbchars (
	hawk_tio_t*   tio, 
	hawk_bch_t* buf, 
	hawk_oow_t   size
);

HAWK_EXPORT hawk_ooi_t hawk_tio_readuchars (
	hawk_tio_t*   tio, 
	hawk_uch_t* buf, 
	hawk_oow_t   size
);

/**
 * The hawk_tio_read() macro is character-type neutral. It maps
 * to hawk_tio_readbchars() or hawk_tio_readuchars().
 */
#ifdef HAWK_OOCH_IS_BCH
#	define hawk_tio_read(tio,buf,size) hawk_tio_readbchars(tio,buf,size)
#else
#	define hawk_tio_read(tio,buf,size) hawk_tio_readuchars(tio,buf,size)
#endif

/**
 * The hawk_tio_writebchars() function writes the @a size characters 
 * from a multibyte string @a str. If @a size is (hawk_oow_t)-1,
 * it writes on until a terminating null is found. It doesn't 
 * write more than HAWK_TYPE_MAX(hawk_ooi_t) characters.
 * @return number of characters written on success, -1 on failure.
 */
HAWK_EXPORT hawk_ooi_t hawk_tio_writebchars (
	hawk_tio_t*        tio,
	const hawk_bch_t*  str,
	hawk_oow_t         size
);

/**
 * The hawk_tio_writebchars() function writes the @a size characters 
 * from a wide-character string @a str. If @a size is (hawk_oow_t)-1,
 * it writes on until a terminating null is found. It doesn't write 
 * more than HAWK_TYPE_MAX(hawk_ooi_t) characters.
 * @return number of characters written on success, -1 on failure.
 */
HAWK_EXPORT hawk_ooi_t hawk_tio_writeuchars (
	hawk_tio_t*        tio,
	const hawk_uch_t*  str,
	hawk_oow_t         size
);

/**
 * The hawk_tio_write() macro is character-type neutral. It maps
 * to hawk_tio_writebchars() or hawk_tio_writeuchars().
 */
#ifdef HAWK_OOCH_IS_BCH
#	define hawk_tio_write(tio,str,size) hawk_tio_writebchars(tio,str,size)
#else
#	define hawk_tio_write(tio,str,size) hawk_tio_writeuchars(tio,str,size)
#endif

#if defined(__cplusplus)
}
#endif

#endif
