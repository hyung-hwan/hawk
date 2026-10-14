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

#ifndef _HAWK_MUS_H_
#define _HAWK_MUS_H_

#include <hawk-cmn.h>
#include <hawk-gem.h>

typedef struct hawk_mus_t hawk_mus_t;

#define HAWK_MUS_HDR \
	hawk_oow_t instsize_; \
	hawk_gem_t gem_

struct hawk_mus_alt_t
{
	HAWK_MUS_HDR;
};
typedef struct hawk_mus_alt_t hawk_mus_alt_t;

enum hawk_mus_opt_t
{
	HAWK_MUS_OPT_MAX_DEPTH, /**< Positive hawk_oow_t; default 256. Bounds parser
	                           * section nesting and render frames (root included). */
	HAWK_MUS_OPT_PARTIALDIRS /**< Copied path list for stock partial-file I/O. */
};
typedef enum hawk_mus_opt_t hawk_mus_opt_t;

enum hawk_mus_val_type_t
{
	HAWK_MUS_VAL_NIL,
	HAWK_MUS_VAL_FALSE,
	HAWK_MUS_VAL_TRUE,
	HAWK_MUS_VAL_SCALAR,
	HAWK_MUS_VAL_MAP,
	HAWK_MUS_VAL_ARR
};
typedef enum hawk_mus_val_type_t hawk_mus_val_type_t;

/** Values are opaque borrowed handles, valid throughout a render. NULL is nil.
 * get returns 1 for a present member (including a nil member), 0 for absence,
 * or -1 for an error. The engine implements dotted names and parent lookup.
 * type returns a hawk_mus_val_type_t code, or -1 for an error. NIL, FALSE, and
 * empty arrays are false; other values, including 0 and empty strings, are true.
 * str returns 0/-1 and borrowed text valid until the next data callback.
 * count and item return 0/-1; array indices start at 0. They may be NULL if
 * the model has no arrays. Callbacks set a diagnostic on error. No JSON or
 * Hawk runtime is required. The callback table and context are borrowed only
 * for the duration of hawk_mus_render(). Serialize access to each instance. */
struct hawk_mus_data_t
{
	int (*get) (
		hawk_mus_t*       mus,
		void*             value,
		const hawk_ooch_t* name,
		hawk_oow_t        len,
		void**            result,
		void*             ctx
	);
	int (*type) (
		hawk_mus_t* mus,
		void*       value,
		void*       ctx
	);
	int (*str) (
		hawk_mus_t*        mus,
		void*              value,
		const hawk_ooch_t** ptr,
		hawk_oow_t*        len,
		void*              ctx
	);
	int (*count) (
		hawk_mus_t* mus,
		void*       value,
		hawk_oow_t* count,
		void*       ctx
	);
	int (*item) (
		hawk_mus_t* mus,
		void*       value,
		hawk_oow_t  index,
		void**      result,
		void*       ctx
	);
};
typedef struct hawk_mus_data_t hawk_mus_data_t;

enum hawk_mus_io_cmd_t
{
	HAWK_MUS_IO_OPEN,
	HAWK_MUS_IO_CLOSE,
	HAWK_MUS_IO_READ,
	HAWK_MUS_IO_WRITE
};
typedef enum hawk_mus_io_cmd_t hawk_mus_io_cmd_t;

enum hawk_mus_io_kind_t
{
	HAWK_MUS_IO_TEMPLATE,
	HAWK_MUS_IO_PARTIAL,
	HAWK_MUS_IO_OUTPUT
};
typedef enum hawk_mus_io_kind_t hawk_mus_io_kind_t;

struct hawk_mus_io_arg_t
{
	hawk_mus_io_kind_t kind;
	const hawk_ooch_t* name; /**< Requested, null-terminated name; NULL for output. */
	const hawk_ooch_t* parent; /**< Calling template path, or NULL. */
	const hawk_ooch_t* path; /**< OPEN may supply a resolved path, valid until CLOSE. */
	void* handle;
};
typedef struct hawk_mus_io_arg_t hawk_mus_io_arg_t;

/** OPEN returns 1 when opened, 0 when absent (empty partial), or -1 on error.
 * A successful OPEN is paired with CLOSE, including on failure. CLOSE returns
 * 0/-1. READ/WRITE return character counts or -1; READ zero is EOF, WRITE zero
 * is an error. Counts are in hawk_ooch_t units, including in UCH builds.
 * Template input is buffered for parsing; rendered output is streamed.
 * Callback arguments and ctx are borrowed for the duration of the operation. */
typedef hawk_ooi_t (*hawk_mus_io_impl_t) (
	hawk_mus_t*        mus,
	hawk_mus_io_cmd_t  cmd,
	hawk_mus_io_arg_t* arg,
	hawk_ooch_t*       data,
	hawk_oow_t        count,
	void*             ctx
);

enum hawk_mus_iostd_type_t
{
	HAWK_MUS_IOSTD_FILE,
	HAWK_MUS_IOSTD_OOCS
};
typedef enum hawk_mus_iostd_type_t hawk_mus_iostd_type_t;

struct hawk_mus_iostd_t
{
	hawk_mus_iostd_type_t type;
	union
	{
		struct
		{
			const hawk_ooch_t* path; /**< NULL or "-" selects stdin/stdout. */
			hawk_cmgr_t* cmgr; /**< NULL selects the engine's character manager. */
		} file;
		hawk_oocs_t oocs;
	} u;
};
typedef struct hawk_mus_iostd_t hawk_mus_iostd_t;

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_mus_t* hawk_mus_open (
	hawk_mmgr_t*   mmgr, /**< Required memory manager. */
	hawk_oow_t     xtnsize,
	hawk_cmgr_t*   cmgr,
	hawk_errinf_t* errinf
);

/** Do not close or reset an instance from its callbacks. */
HAWK_EXPORT void hawk_mus_close (
	hawk_mus_t* mus
);

/** Discard the parsed template. Options remain unchanged. */
HAWK_EXPORT void hawk_mus_reset (
	hawk_mus_t* mus
);

HAWK_EXPORT int hawk_mus_setopt (
	hawk_mus_t*    mus,
	hawk_mus_opt_t id,
	const void*    value
);

HAWK_EXPORT int hawk_mus_getopt (
	hawk_mus_t*    mus,
	hawk_mus_opt_t id,
	void*         value
);

/** Parse a copy of the length-bearing source. Failure preserves the previous
 * template. A zero-length source is valid. Optional lambdas, inheritance, and
 * dynamic partial names are not implemented by this core renderer. */
HAWK_EXPORT int hawk_mus_compilewithoochars (
	hawk_mus_t*       mus,
	const hawk_ooch_t* ptr,
	hawk_oow_t        len
);

/** Convert text with the instance's character manager when necessary. */
HAWK_EXPORT int hawk_mus_compilewithbchars (
	hawk_mus_t*      mus,
	const hawk_bch_t* ptr,
	hawk_oow_t       len
);

HAWK_EXPORT int hawk_mus_compilewithuchars (
	hawk_mus_t*      mus,
	const hawk_uch_t* ptr,
	hawk_oow_t       len
);

HAWK_EXPORT int hawk_mus_compile (
	hawk_mus_t*       mus,
	const hawk_ooch_t* name,
	hawk_mus_io_impl_t io,
	void*             ctx
);

/** Render the parsed template with new data. Uses heap-backed frames for
 * sections and partials. Missing variables/partials render nothing. Output
 * already written is retained on error. The instance is reusable after error. */
HAWK_EXPORT int hawk_mus_render (
	hawk_mus_t*           mus,
	void*                 root,
	const hawk_mus_data_t* data,
	void*                 data_ctx,
	hawk_mus_io_impl_t     io,
	void*                 io_ctx
);

HAWK_EXPORT hawk_mus_t* hawk_mus_openstd (
	hawk_oow_t     xtnsize,
	hawk_errinf_t* errinf
);

HAWK_EXPORT hawk_mus_t* hawk_mus_openstdwithmmgr (
	hawk_mmgr_t*   mmgr,
	hawk_oow_t     xtnsize,
	hawk_cmgr_t*   cmgr,
	hawk_errinf_t* errinf
);

HAWK_EXPORT int hawk_mus_compilestd (
	hawk_mus_t*             mus,
	const hawk_mus_iostd_t* input
);

/** NULL output selects stdout. An OOCS output receives allocated text on
 * success only; free it with hawk_mus_freemem(). Caller owns any previous text.
 * Stock partials try the name and name.mustache beside the calling template,
 * then in PARTIALDIRS (platform path-list separator), or the working directory
 * when compiling from text. No mutable global state is used. */
HAWK_EXPORT int hawk_mus_renderstd (
	hawk_mus_t*           mus,
	void*                 root,
	const hawk_mus_data_t* data,
	void*                 data_ctx,
	hawk_mus_iostd_t*     output
);

HAWK_EXPORT void hawk_mus_seterrbfmt (
	hawk_mus_t*        mus,
	const hawk_loc_t* loc,
	hawk_errnum_t     num,
	const hawk_bch_t* fmt,
	...
);

HAWK_EXPORT void hawk_mus_seterrufmt (
	hawk_mus_t*        mus,
	const hawk_loc_t* loc,
	hawk_errnum_t     num,
	const hawk_uch_t* fmt,
	...
);

#if defined(__cplusplus)
}
#endif

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_mus_getxtn (hawk_mus_t* mus) { return (hawk_uint8_t*)mus + ((hawk_mus_alt_t*)mus)->instsize_; }
static HAWK_INLINE hawk_gem_t* hawk_mus_getgem (hawk_mus_t* mus) { return &((hawk_mus_alt_t*)mus)->gem_; }
#else
#define hawk_mus_getxtn(mus) ((hawk_uint8_t*)(mus) + ((hawk_mus_alt_t*)(mus))->instsize_)
#define hawk_mus_getgem(mus) (&((hawk_mus_alt_t*)(mus))->gem_)
#endif

#define hawk_mus_getmmgr(mus) (hawk_mus_getgem(mus)->mmgr)
#define hawk_mus_getcmgr(mus) (hawk_mus_getgem(mus)->cmgr)
#define hawk_mus_geterrnum(mus) hawk_gem_geterrnum(hawk_mus_getgem(mus))
#define hawk_mus_geterrloc(mus) hawk_gem_geterrloc(hawk_mus_getgem(mus))
#define hawk_mus_geterrbmsg(mus) hawk_gem_geterrbmsg(hawk_mus_getgem(mus))
#define hawk_mus_geterrumsg(mus) hawk_gem_geterrumsg(hawk_mus_getgem(mus))
#define hawk_mus_geterrinf(mus,ei) hawk_gem_geterrinf(hawk_mus_getgem(mus),ei)
#define hawk_mus_seterrnum(mus,loc,num) hawk_gem_seterrnum(hawk_mus_getgem(mus),loc,num)
#define hawk_mus_allocmem(mus,size) hawk_gem_allocmem(hawk_mus_getgem(mus),size)
#define hawk_mus_reallocmem(mus,ptr,size) hawk_gem_reallocmem(hawk_mus_getgem(mus),ptr,size)
#define hawk_mus_freemem(mus,ptr) hawk_gem_freemem(hawk_mus_getgem(mus),ptr)

#if defined(HAWK_OOCH_IS_UCH)
#define hawk_mus_geterrmsg hawk_mus_geterrumsg
#else
#define hawk_mus_geterrmsg hawk_mus_geterrbmsg
#endif

#endif
