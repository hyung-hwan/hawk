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

#ifndef _HAWK_DIR_H_
#define _HAWK_DIR_H_

#include <hawk-cmn.h>

typedef struct hawk_dir_t hawk_dir_t;
typedef struct hawk_dir_ent_t hawk_dir_ent_t;

enum hawk_dir_flag_t
{
	HAWK_DIR_MBSPATH    = (1 << 0),
	HAWK_DIR_WCSPATH    = (1 << 1),
	HAWK_DIR_SORT       = (1 << 2),
	HAWK_DIR_SKIPSPCDIR = (1 << 3)  /**< limited to normal entries excluding . and .. */
};
typedef enum hawk_dir_flag_t hawk_dir_flag_t;

struct hawk_dir_ent_t
{
	const hawk_ooch_t* name;
};

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_dir_t* hawk_dir_open (
	hawk_gem_t*        gem,
	hawk_oow_t         xtnsize,
	const hawk_ooch_t* path, 
	int                flags
);

HAWK_EXPORT void hawk_dir_close (
	hawk_dir_t* dir
);

HAWK_EXPORT void* hawk_dir_getxtn (
	hawk_dir_t* dir
);

HAWK_EXPORT int hawk_dir_reset (
	hawk_dir_t*        dir,
	const hawk_ooch_t* path
);

/**
 * The hawk_dir_read() function reads a directory entry and
 * stores it in memory pointed to by \a ent.
 * \return -1 on failure, 0 upon no more entry, 1 on success
 */
HAWK_EXPORT int hawk_dir_read (
	hawk_dir_t*     dir,
	hawk_dir_ent_t* ent
);

#if defined(__cplusplus)
}
#endif

#endif
