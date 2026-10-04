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

#ifndef _HAWK_M4_H_
#define _HAWK_M4_H_

#include <hawk-cmn.h>
#include <hawk-gem.h>

typedef struct hawk_m4_t hawk_m4_t;

/* All M4 text and paths use hawk_ooch_t, following the library's BCH/UCH build mode. */

#define HAWK_M4_HDR \
	hawk_oow_t instsize_; \
	hawk_gem_t gem_

typedef struct hawk_m4_alt_t hawk_m4_alt_t;
struct hawk_m4_alt_t
{
	HAWK_M4_HDR;
};

enum hawk_m4_io_cmd_t
{
	HAWK_M4_IO_OPEN,
	HAWK_M4_IO_CLOSE,
	HAWK_M4_IO_READ,
	HAWK_M4_IO_WRITE,
	HAWK_M4_IO_SYSCMD
};
typedef enum hawk_m4_io_cmd_t hawk_m4_io_cmd_t;

enum hawk_m4_io_kind_t
{
	HAWK_M4_IO_INPUT,
	HAWK_M4_IO_OUTPUT,
	HAWK_M4_IO_ERROR
};
typedef enum hawk_m4_io_kind_t hawk_m4_io_kind_t;

struct hawk_m4_io_arg_t
{
	hawk_m4_io_kind_t kind;
	void* handle;
	const hawk_ooch_t* path;
};
typedef struct hawk_m4_io_arg_t hawk_m4_io_arg_t;

/**
 * An I/O callback returns zero or a positive value for OPEN and CLOSE,
 * the number of characters processed for READ and WRITE, and a negative
 * value on error. For SYSCMD, data points to the command and count is its
 * length; zero means that the command was accepted.
 */
typedef hawk_ooi_t (*hawk_m4_io_impl_t) (
	hawk_m4_t*        m4,
	hawk_m4_io_cmd_t  cmd,
	hawk_m4_io_arg_t* arg,
	hawk_ooch_t*      data,
	hawk_oow_t        count
);

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_m4_t* hawk_m4_open (
	hawk_mmgr_t*   mmgr,
	hawk_oow_t     xtnsize,
	hawk_cmgr_t*   cmgr,
	hawk_errinf_t* errinf
);

HAWK_EXPORT void hawk_m4_close (hawk_m4_t* m4);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_m4_getxtn (hawk_m4_t* m4) { return (void*)((hawk_uint8_t*)m4 + ((hawk_m4_alt_t*)m4)->instsize_); }
static HAWK_INLINE hawk_gem_t* hawk_m4_getgem (hawk_m4_t* m4) { return &((hawk_m4_alt_t*)m4)->gem_; }
static HAWK_INLINE hawk_mmgr_t* hawk_m4_getmmgr (hawk_m4_t* m4) { return ((hawk_m4_alt_t*)m4)->gem_.mmgr; }
static HAWK_INLINE hawk_cmgr_t* hawk_m4_getcmgr (hawk_m4_t* m4) { return ((hawk_m4_alt_t*)m4)->gem_.cmgr; }
#else
#define hawk_m4_getxtn(m4) ((void*)((hawk_uint8_t*)(m4) + ((hawk_m4_alt_t*)(m4))->instsize_))
#define hawk_m4_getgem(m4) (&((hawk_m4_alt_t*)(m4))->gem_)
#define hawk_m4_getmmgr(m4) (((hawk_m4_alt_t*)(m4))->gem_.mmgr)
#define hawk_m4_getcmgr(m4) (((hawk_m4_alt_t*)(m4))->gem_.cmgr)
#endif

#define hawk_m4_geterrnum(m4) hawk_gem_geterrnum(hawk_m4_getgem(m4))
#define hawk_m4_geterrloc(m4) hawk_gem_geterrloc(hawk_m4_getgem(m4))
#define hawk_m4_geterrbmsg(m4) hawk_gem_geterrbmsg(hawk_m4_getgem(m4))
#define hawk_m4_geterrumsg(m4) hawk_gem_geterrumsg(hawk_m4_getgem(m4))
#define hawk_m4_geterrbinf(m4,ei) hawk_gem_geterrbinf(hawk_m4_getgem(m4),ei)
#define hawk_m4_geterruinf(m4,ei) hawk_gem_geterruinf(hawk_m4_getgem(m4),ei)

#if defined(HAWK_OOCH_IS_BCH)
#define hawk_m4_geterrmsg hawk_m4_geterrbmsg
#define hawk_m4_geterrinf hawk_m4_geterrbinf
#else
#define hawk_m4_geterrmsg hawk_m4_geterrumsg
#define hawk_m4_geterrinf hawk_m4_geterruinf
#endif

/**
 * The hawk_m4_seterrnum() function sets the error information omitting
 * error location. You must pass a non-NULL for \a errarg if the specified
 * error number \a errnum requires one or more arguments to format an
 * error message.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_m4_seterrnum (hawk_m4_t* m4, const hawk_loc_t* errloc, hawk_errnum_t errnum) { hawk_gem_seterrnum (hawk_m4_getgem(m4), errloc, errnum); }
static HAWK_INLINE void hawk_m4_seterrinf (hawk_m4_t* m4, const hawk_errinf_t* errinf) { hawk_gem_seterrinf (hawk_m4_getgem(m4), errinf); }
static HAWK_INLINE void hawk_m4_seterror (hawk_m4_t* m4, const hawk_loc_t*  errloc, hawk_errnum_t errnum, const hawk_oocs_t* errarg) { hawk_gem_seterror(hawk_m4_getgem(m4), errloc, errnum, errarg); }
static HAWK_INLINE const hawk_ooch_t* hawk_m4_backuperrmsg (hawk_m4_t* m4) { return hawk_gem_backuperrmsg(hawk_m4_getgem(m4)); }
#else
#define hawk_m4_seterrnum(m4, errloc, errnum) hawk_gem_seterrnum(hawk_m4_getgem(m4), errloc, errnum)
#define hawk_m4_seterrinf(m4, errinf) hawk_gem_seterrinf(hawk_m4_getgem(m4), errinf)
#define hawk_m4_seterror(m4, errloc, errnum, errarg) hawk_gem_seterror(hawk_m4_getgem(m4), errloc, errnum, errarg)
#define hawk_m4_backuperrmsg(m4) hawk_gem_backuperrmsg(hawk_m4_getgem(m4))
#endif

HAWK_EXPORT void hawk_m4_seterrbfmt (
	hawk_m4_t*        m4,
	const hawk_loc_t* loc,
	hawk_errnum_t     num,
	const hawk_bch_t* fmt,
	...
);

HAWK_EXPORT void hawk_m4_seterrufmt (
	hawk_m4_t*        m4,
	const hawk_loc_t* loc,
	hawk_errnum_t     num,
	const hawk_uch_t* fmt,
	...
);

/**
 * Process the null-terminated input path array. A null array means standard
 * input. The same callback is used for included files and output streams.
 */
HAWK_EXPORT int hawk_m4_exec (
	hawk_m4_t*               m4,
	const hawk_ooch_t* const input[],
	hawk_m4_io_impl_t         io
);

/** Standard file/console wrapper implemented separately in std-m4.c. */
HAWK_EXPORT hawk_m4_t* hawk_m4_openstd (
	hawk_oow_t     xtnsize,
	hawk_errinf_t* errinf
);

HAWK_EXPORT hawk_m4_t* hawk_m4_openstdwithmmgr (
	hawk_mmgr_t*   mmgr,
	hawk_oow_t     xtnsize,
	hawk_cmgr_t*   cmgr,
	hawk_errinf_t* errinf
);

HAWK_EXPORT int hawk_m4_execstd (
	hawk_m4_t*               m4,
	const hawk_ooch_t* const input[]
);

HAWK_EXPORT void* hawk_m4_allocmem (
	hawk_m4_t* m4,
	hawk_oow_t size
);

HAWK_EXPORT void* hawk_m4_callocmem (
	hawk_m4_t* m4,
	hawk_oow_t size
);

HAWK_EXPORT void* hawk_m4_reallocmem (
	hawk_m4_t* m4,
	void*      ptr,
	hawk_oow_t size
);

HAWK_EXPORT void hawk_m4_freemem (
	hawk_m4_t* m4,
	void*      ptr
);

#if defined(__cplusplus)
}
#endif

#endif
