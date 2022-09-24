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

#ifndef _HAWK_FIO_H_
#define _HAWK_FIO_H_

#include <hawk-cmn.h>

enum hawk_fio_flag_t
{
	/* (1 << 0) to (1 << 7) reserved for hawk_sio_flag_t. 
	 * see <hawk/cmn/sio.h>. nerver use this value. */
	HAWK_FIO_RESERVED      = 0xFF,

	/** treat the file name pointer as a handle pointer */
	HAWK_FIO_HANDLE        = (1 << 8),

	/** treat the file name pointer as a pointer to file name
	 *  template to use when making a temporary file name */
	HAWK_FIO_TEMPORARY     = (1 << 9),

	/** don't close an I/O handle in hawk_fio_fini() and hawk_fio_close() */
	HAWK_FIO_NOCLOSE       = (1 << 10),

	/** treat the path name as a multi-byte string */
	HAWK_FIO_BCSTRPATH    = (1 << 11), 

	/* normal open flags */
	HAWK_FIO_READ          = (1 << 14),
	HAWK_FIO_WRITE         = (1 << 15),
	HAWK_FIO_APPEND        = (1 << 16),

	HAWK_FIO_CREATE        = (1 << 17),
	HAWK_FIO_TRUNCATE      = (1 << 18),
	HAWK_FIO_EXCLUSIVE     = (1 << 19),
	HAWK_FIO_SYNC          = (1 << 20),
	
	/* do not follow a symbolic link, only on a supported platform */
	HAWK_FIO_NOFOLLOW      = (1 << 23),

	/* for WIN32 only. harmless(no effect) when used on other platforms */
	HAWK_FIO_NOSHREAD      = (1 << 24),
	HAWK_FIO_NOSHWRITE     = (1 << 25),
	HAWK_FIO_NOSHDELETE    = (1 << 26),

	/* hints to OS. harmless(no effect) when used on unsupported platforms */
	HAWK_FIO_RANDOM        = (1 << 27), /* hint that access be random */
	HAWK_FIO_SEQUENTIAL    = (1 << 28)  /* hint that access is sequential */
};

enum hawk_fio_std_t
{
	HAWK_FIO_STDIN  = 0,
	HAWK_FIO_STDOUT = 1,
	HAWK_FIO_STDERR = 2
};
typedef enum hawk_fio_std_t hawk_fio_std_t;

/* seek origin */
enum hawk_fio_ori_t
{
	HAWK_FIO_BEGIN   = 0,
	HAWK_FIO_CURRENT = 1,
	HAWK_FIO_END     = 2
};
/* file origin for seek */
typedef enum hawk_fio_ori_t hawk_fio_ori_t;

enum hawk_fio_mode_t
{
	HAWK_FIO_SUID = 04000, /* set UID */
	HAWK_FIO_SGID = 02000, /* set GID */
	HAWK_FIO_SVTX = 01000, /* sticky bit */
	HAWK_FIO_RUSR = 00400, /* can be read by owner */
	HAWK_FIO_WUSR = 00200, /* can be written by owner */
	HAWK_FIO_XUSR = 00100, /* can be executed by owner */
	HAWK_FIO_RGRP = 00040, /* can be read by group */
	HAWK_FIO_WGRP = 00020, /* can be written by group */
	HAWK_FIO_XGRP = 00010, /* can be executed by group */
	HAWK_FIO_ROTH = 00004, /* can be read by others */
	HAWK_FIO_WOTH = 00002, /* can be written by others */
	HAWK_FIO_XOTH = 00001  /* can be executed by others */
};

#if defined(_WIN32)
	/* <winnt.h> => typedef PVOID HANDLE; */
	typedef void* hawk_fio_hnd_t;
#elif defined(__OS2__)
	/* <os2def.h> => typedef LHANDLE HFILE;
	                 typedef unsigned long LHANDLE; */
	typedef unsigned long hawk_fio_hnd_t;
#elif defined(__DOS__)
	typedef int hawk_fio_hnd_t;
#elif defined(vms) || defined(__vms)
	typedef void* hawk_fio_hnd_t; /* struct FAB*, struct RAB* */
#else
	typedef int hawk_fio_hnd_t;
#endif

/* file offset */
typedef hawk_foff_t hawk_fio_off_t;

typedef struct hawk_fio_t hawk_fio_t;
typedef struct hawk_fio_lck_t hawk_fio_lck_t;

struct hawk_fio_t
{
	hawk_gem_t*       gem;
	hawk_fio_hnd_t    handle;
	int               status; 
};

struct hawk_fio_lck_t
{
	int             type;   /* READ, WRITE */
	hawk_fio_off_t  offset; /* starting offset */
	hawk_fio_off_t  length; /* length */
	hawk_fio_ori_t  origin; /* origin */
};

#define HAWK_FIO_HANDLE(fio) ((fio)->handle)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_fio_open() function opens a file.
 * To open a file, you should set the flags with at least one of
 * HAWK_FIO_READ, HAWK_FIO_WRITE, HAWK_FIO_APPEND.
 *
 * If the #HAWK_FIO_HANDLE flag is set, the @a path parameter is interpreted
 * as a pointer to hawk_fio_hnd_t.
 *
 * If the #HAWK_FIO_TEMPORARY flag is set, the @a path parameter is 
 * interpreted as a path name template and an actual file name to open
 * is internally generated using the template. The @a path parameter 
 * is filled with the last actual path name attempted when the function
 * returns. So, you must not pass a constant string to the @a path 
 * parameter when #HAWK_FIO_TEMPORARY is set.
 */
HAWK_EXPORT hawk_fio_t* hawk_fio_open (
	hawk_gem_t*        gem,
	hawk_oow_t         xtnsize,
	const hawk_ooch_t* path,
	int                flags,
	int                mode
);

/**
 * The hawk_fio_close() function closes a file.
 */
HAWK_EXPORT void hawk_fio_close (
	hawk_fio_t* fio
);

/**
 * The hawk_fio_close() function opens a file into @a fio.
 */
HAWK_EXPORT int hawk_fio_init (
	hawk_fio_t*        fio,
	hawk_gem_t*        gem,
	const hawk_ooch_t* path,
	int                flags,
	int                mode
);

/**
 * The hawk_fio_close() function finalizes a file by closing the handle 
 * stored in @a fio.
 */
HAWK_EXPORT void hawk_fio_fini (
	hawk_fio_t* fio
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_fio_getxtn (hawk_fio_t* fio) { return (void*)(fio + 1); }
#else
#define hawk_fio_getxtn(fio) ((void*)((hawk_fio_t*)(fio) + 1))
#endif

/**
 * The hawk_fio_gethnd() function returns the native file handle.
 */
HAWK_EXPORT hawk_fio_hnd_t hawk_fio_gethnd (
	const hawk_fio_t* fio
);


/**
 * The hawk_fio_seek() function changes the current file position.
 */
HAWK_EXPORT hawk_fio_off_t hawk_fio_seek (
	hawk_fio_t*    fio,
	hawk_fio_off_t offset,
	hawk_fio_ori_t origin
);

/**
 * The hawk_fio_truncate() function truncates a file to @a size.
 */
HAWK_EXPORT int hawk_fio_truncate (
	hawk_fio_t*    fio,
	hawk_fio_off_t size
);

/**
 * The hawk_fio_read() function reads data.
 */
HAWK_EXPORT hawk_ooi_t hawk_fio_read (
	hawk_fio_t*  fio,
	void*        buf,
	hawk_oow_t   size
);

/**
 * The hawk_fio_write() function writes data.
 */
HAWK_EXPORT hawk_ooi_t hawk_fio_write (
	hawk_fio_t*  fio,
	const void*  data,
	hawk_oow_t   size
);

/**
 * The hawk_fio_chmod() function changes the file mode.
 *
 * @note
 * On _WIN32, this function is implemented on the best-effort basis and 
 * returns an error on the following conditions:
 * - The file size is 0.
 * - The file is opened without #HAWK_FIO_READ.
 */
HAWK_EXPORT int hawk_fio_chmod (
	hawk_fio_t* fio,
	int         mode
);

/**
 * The hawk_fio_sync() function synchronizes file contents into storage media
 * It is useful in determining the media error, without which hawk_fio_close() 
 * may succeed despite such an error.
 */
HAWK_EXPORT int hawk_fio_sync (
	hawk_fio_t* fio
);

HAWK_EXPORT int hawk_fio_lock ( 
	hawk_fio_t*     fio, 
	hawk_fio_lck_t* lck,
	int             flags
);

HAWK_EXPORT int hawk_fio_unlock (
	hawk_fio_t*     fio,
	hawk_fio_lck_t* lck,
	int             flags
);

/**
 * The hawk_get_std_fio_handle() returns a low-level system handle to
 * commonly used I/O channels.
 */
HAWK_EXPORT int hawk_get_std_fio_handle (
	hawk_fio_std_t  std,
	hawk_fio_hnd_t* hnd
);

#if defined(__cplusplus)
}
#endif

#endif
