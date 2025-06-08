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

#ifndef _HAWK_CUT_H_
#define _HAWK_CUT_H_

#include <hawk-cmn.h>
#include <hawk-gem.h>
#include <hawk-sio.h>

/** @file
 * This file defines a text cutter utility.
 *
 * @todo HAWK_CUT_ORDEREDSEL - A selector 5,3,1 is ordered to 1,3,5
 */

/**
 * @example cut.c
 * This example implements a simple cut utility.
 */

/** @struct hawk_cut_t
 * The hawk_cut_t type defines a text cutter. The details are hidden as it is
 * a large complex structure vulnerable to unintended changes.
 */
typedef struct hawk_cut_t hawk_cut_t;

#define HAWK_CUT_HDR \
	hawk_oow_t _instsize; \
	hawk_gem_t _gem

typedef struct hawk_cut_alt_t hawk_cut_alt_t;
struct hawk_cut_alt_t
{
	/* ensure that hawk_cut_alt_t matches the beginning part of hawk_cut_t */
	HAWK_CUT_HDR;
};

/**
 * The hawk_cut_option_t type defines various option codes for a text cutter.
 * Options can be OR'ed with each other and be passed to a text cutter with
 * the hawk_cut_setoption() function.
 */
enum hawk_cut_option_t
{
	/** show delimited line only. if not set, undelimited lines are
	 *  shown in its entirety */
	HAWK_CUT_DELIMONLY    = (1 << 0),

	/** treat any whitespaces as an input delimiter */
	HAWK_CUT_WHITESPACE   = (1 << 2),

	/** fold adjacent delimiters */
	HAWK_CUT_FOLDDELIMS   = (1 << 3),

	/** trim leading and trailing whitespaces off the input line */
	HAWK_CUT_TRIMSPACE    = (1 << 4),

	/** normalize whitespaces in the input line */
	HAWK_CUT_NORMSPACE    = (1 << 5)
};
typedef enum hawk_cut_option_t hawk_cut_option_t;

/**
 * The hawk_cut_io_cmd_t type defines I/O command codes. The code indicates
 * the action to take in an I/O handler.
 */
enum hawk_cut_io_cmd_t
{
	HAWK_CUT_IO_OPEN  = 0,
	HAWK_CUT_IO_CLOSE = 1,
	HAWK_CUT_IO_READ  = 2,
	HAWK_CUT_IO_WRITE = 3
};
typedef enum hawk_cut_io_cmd_t hawk_cut_io_cmd_t;

/**
 * The hawk_cut_io_arg_t type defines a data structure required by
 * an I/O handler.
 */
struct hawk_cut_io_arg_t
{
	void* handle; /**< I/O handle */
	const hawk_ooch_t* path;   /**< file path. HAWK_NULL for a console */
};
typedef struct hawk_cut_io_arg_t hawk_cut_io_arg_t;

/**
 * The hawk_cut_io_impl_t type defines an I/O handler. hawk_cut_exec() calls
 * I/O handlers to read from and write to a text stream.
 */
typedef hawk_ooi_t (*hawk_cut_io_impl_t) (
	hawk_cut_t*        cut,
	hawk_cut_io_cmd_t  cmd,
	hawk_cut_io_arg_t* arg,
	hawk_ooch_t*       data,
	hawk_oow_t         count
);

/* ------------------------------------------------------------------------ */

/**
 * The hawk_cut_iostd_type_t type defines types of standard
 * I/O resources.
 */
enum hawk_cut_iostd_type_t
{
	HAWK_CUT_IOSTD_NULL,  /**< null resource type */
	HAWK_CUT_IOSTD_FILE,  /**< file */
	HAWK_CUT_IOSTD_FILEB, /**< file */
	HAWK_CUT_IOSTD_FILEU, /**< file */
	HAWK_CUT_IOSTD_OOCS,  /**< string  */
	HAWK_CUT_IOSTD_BCS,
	HAWK_CUT_IOSTD_UCS,
	HAWK_CUT_IOSTD_SIO    /**< sio */
};
typedef enum hawk_cut_iostd_type_t hawk_cut_iostd_type_t;

/**
 * The hawk_cut_iostd_t type defines a standard I/O resource.
 */
struct hawk_cut_iostd_t
{
	/** resource type */
	hawk_cut_iostd_type_t type;

	/** union describing the resource of the specified type */
	union
	{
		/** file path with character encoding */
		struct
		{
			/** file path to open. #HAWK_NULL or '-' for stdin/stdout. */
			const hawk_ooch_t* path;
			/** a stream created with the file path is set with this
			 * cmgr if it is not #HAWK_NULL. */
			hawk_cmgr_t* cmgr;
		} file;

		struct
		{
			const hawk_bch_t* path;
			hawk_cmgr_t* cmgr;
		} fileb;

		struct
		{
			const hawk_uch_t* path;
			hawk_cmgr_t* cmgr;
		} fileu;

		/**
		 * input string or dynamically allocated output string
		 *
		 * For input, the ptr and the len field of str indicates the
		 * pointer and the length of a string to read. You must set
		 * these two fields before calling hawk_cut_execstd().
		 *
		 * For output, the ptr and the len field of str indicates the
		 * pointer and the length of produced output. The output
		 * string is dynamically allocated. You don't need to set these
		 * fields before calling hawk_cut_execstd() because they are
		 * set by hawk_cut_execstd() and valid while the relevant cut
		 * object is alive. You must free the memory chunk pointed to by
		 * the ptr field with hawk_cut_freemem() once you're done with it
		 * to avoid memory leaks.
		 */
		hawk_oocs_t        oocs;
		hawk_bcs_t         bcs;
		hawk_ucs_t         ucs;

		/** pre-opened sio stream */
		hawk_sio_t*        sio;
	} u;
};

typedef struct hawk_cut_iostd_t hawk_cut_iostd_t;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_cut_open() function creates a text cutter.
 * @return A pointer to a text cutter on success, #HAWK_NULL on failure
 */

HAWK_EXPORT hawk_cut_t* hawk_cut_open (
	hawk_mmgr_t*   mmgr,   /**< memory manager */
	hawk_oow_t     xtnsize, /**< extension size in bytes */
	hawk_cmgr_t*   cmgr,
        hawk_errnum_t* errnum
);

/**
 * The hawk_cut_close() function destroys a text cutter.
 */
HAWK_EXPORT void hawk_cut_close (
	hawk_cut_t* cut /**< text cutter */
);

#if defined(HAWK_HAVE_INLINE)
/**
 * The hawk_cut_getxtn() function returns the pointer to the extension area
 * placed behind the actual cut object.
 */
static HAWK_INLINE void* hawk_cut_getxtn (hawk_cut_t* cut) { return (void*)((hawk_uint8_t*)cut + ((hawk_cut_alt_t*)cut)->_instsize); }

/**
 * The hawk_cut_getgem() function gets the pointer to the gem structure of the
 * cut object.
 */
static HAWK_INLINE hawk_gem_t* hawk_cut_getgem (hawk_cut_t* cut) { return &((hawk_cut_alt_t*)cut)->_gem; }

/**
 * The hawk_cut_getmmgr() function gets the memory manager ucut in
 * hawk_cut_open().
 */
static HAWK_INLINE hawk_mmgr_t* hawk_cut_getmmgr (hawk_cut_t* cut) { return ((hawk_cut_alt_t*)cut)->_gem.mmgr; }
static HAWK_INLINE hawk_cmgr_t* hawk_cut_getcmgr (hawk_cut_t* cut) { return ((hawk_cut_alt_t*)cut)->_gem.cmgr; }
static HAWK_INLINE void hawk_cut_setcmgr (hawk_cut_t* cut, hawk_cmgr_t* cmgr) { ((hawk_cut_alt_t*)cut)->_gem.cmgr = cmgr; }
#else
#define hawk_cut_getxtn(cut) ((void*)((hawk_uint8_t*)cut + ((hawk_cut_alt_t*)cut)->_instsize))
#define hawk_cut_getgem(cut) (&((hawk_cut_alt_t*)(cut))->_gem)
#define hawk_cut_getmmgr(cut) (((hawk_cut_alt_t*)(cut))->_gem.mmgr)
#define hawk_cut_getcmgr(cut) (((hawk_cut_alt_t*)(cut))->_gem.cmgr)
#define hawk_cut_setcmgr(cut,_cmgr) (((hawk_cut_alt_t*)(cut))->_gem.cmgr = (_cmgr))
#endif /* HAWK_HAVE_INLINE */

/**
 * The hawk_cut_getoption() function retrieves the current options set in
 * a text cutter.
 * @return 0 or a number OR'ed of #hawk_cut_option_t values
 */
HAWK_EXPORT int hawk_cut_getoption (
	hawk_cut_t* cut /**< text cutter */
);

/**
 * The hawk_cut_setoption() function sets the option code.
 */
HAWK_EXPORT void hawk_cut_setoption (
	hawk_cut_t* cut, /**< text cutter */
	int        opt  /**< 0 or a number OR'ed of #hawk_cut_option_t values */
);

HAWK_EXPORT hawk_errstr_t hawk_cut_geterrstr (
	hawk_cut_t* cut
);

/**
 * The hawk_cut_geterrnum() function returns the number of the last error
 * occurred.
 * \return error number
 */

/**
 * The hawk_cut_geterror() function gets an error number, an error location,
 * and an error message. The information is set to the memory area pointed
 * to by each parameter.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_errnum_t hawk_cut_geterrnum (hawk_cut_t* cut) { return hawk_gem_geterrnum(hawk_cut_getgem(cut)); }
static HAWK_INLINE const hawk_loc_t* hawk_cut_geterrloc (hawk_cut_t* cut) { return hawk_gem_geterrloc(hawk_cut_getgem(cut)); }
static HAWK_INLINE const hawk_bch_t* hawk_cut_geterrbmsg (hawk_cut_t* cut) { return hawk_gem_geterrbmsg(hawk_cut_getgem(cut)); }
static HAWK_INLINE const hawk_uch_t* hawk_cut_geterrumsg (hawk_cut_t* cut) { return hawk_gem_geterrumsg(hawk_cut_getgem(cut)); }
static HAWK_INLINE void hawk_cut_geterrbinf (hawk_cut_t* cut, hawk_errbinf_t* errinf) { return hawk_gem_geterrbinf(hawk_cut_getgem(cut), errinf); }
static HAWK_INLINE void hawk_cut_geterruinf (hawk_cut_t* cut, hawk_erruinf_t* errinf) { return hawk_gem_geterruinf(hawk_cut_getgem(cut), errinf); }
static HAWK_INLINE void hawk_cut_geterror (hawk_cut_t* cut, hawk_errnum_t* errnum, const hawk_ooch_t** errmsg, hawk_loc_t* errloc) { return hawk_gem_geterror(hawk_cut_getgem(cut), errnum, errmsg, errloc); }
#else
#define hawk_cut_geterrnum(cut) hawk_gem_geterrnum(hawk_cut_getgem(cut))
#define hawk_cut_geterrloc(cut) hawk_gem_geterrloc(hawk_cut_getgem(cut))
#define hawk_cut_geterrbmsg(cut) hawk_gem_geterrbmsg(hawk_cut_getgem(cut))
#define hawk_cut_geterrumsg(cut) hawk_gem_geterrumsg(hawk_cut_getgem(cut))
#define hawk_cut_geterrbinf(cut, errinf) (hawk_gem_geterrbinf(hawk_cut_getgem(cut), errinf))
#define hawk_cut_geterruinf(cut, errinf) (hawk_gem_geterruinf(hawk_cut_getgem(cut), errinf))
#define hawk_cut_geterror(cut, errnum, errmsg, errloc) (hawk_gem_geterror(hawk_cut_getgem(cut), errnum, errmsg, errloc))
#endif

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_cut_geterrmsg hawk_cut_geterrbmsg
#	define hawk_cut_geterrinf hawk_cut_geterrbinf
#else
#	define hawk_cut_geterrmsg hawk_cut_geterrumsg
#	define hawk_cut_geterrinf hawk_cut_geterruinf
#endif


/**
 * The hawk_cut_seterrnum() function sets the error information omitting
 * error location. You must pass a non-NULL for \a errarg if the specified
 * error number \a errnum requires one or more arguments to format an
 * error message.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_cut_seterrnum (hawk_cut_t* cut, const hawk_loc_t* errloc, hawk_errnum_t errnum) { hawk_gem_seterrnum (hawk_cut_getgem(cut), errloc, errnum); }
static HAWK_INLINE void hawk_cut_seterrinf (hawk_cut_t* cut, const hawk_errinf_t* errinf) { hawk_gem_seterrinf (hawk_cut_getgem(cut), errinf); }
static HAWK_INLINE void hawk_cut_seterror (hawk_cut_t* cut, const hawk_loc_t*  errloc, hawk_errnum_t errnum, const hawk_oocs_t* errarg) { hawk_gem_seterror(hawk_cut_getgem(cut), errloc, errnum, errarg); }
static HAWK_INLINE const hawk_ooch_t* hawk_cut_backuperrmsg (hawk_cut_t* cut) { return hawk_gem_backuperrmsg(hawk_cut_getgem(cut)); }
#else
#define hawk_cut_seterrnum(cut, errloc, errnum) hawk_gem_seterrnum(hawk_cut_getgem(cut), errloc, errnum)
#define hawk_cut_seterrinf(cut, errinf) hawk_gem_seterrinf(hawk_cut_getgem(cut), errinf)
#define hawk_cut_seterror(cut, errloc, errnum, errarg) hawk_gem_seterror(hawk_cut_getgem(cut), errloc, errnum, errarg)
#define hawk_cut_backuperrmsg(cut) hawk_gem_backuperrmsg(hawk_cut_getgem(cut))
#endif


HAWK_EXPORT void hawk_cut_seterrbfmt (
	hawk_cut_t*       cut,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_bch_t* fmt,
	...
);

HAWK_EXPORT void hawk_cut_seterrufmt (
	hawk_cut_t*       cut,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_uch_t* fmt,
	...
);


HAWK_EXPORT void hawk_cut_seterrbvfmt (
	hawk_cut_t*         cut,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_bch_t*   errfmt,
	va_list             ap
);

HAWK_EXPORT void hawk_cut_seterruvfmt (
	hawk_cut_t*         cut,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_uch_t*   errfmt,
	va_list             ap
);

/**
 * The hawk_cut_clear() function clears memory buffers internally allocated.
 */
HAWK_EXPORT void hawk_cut_clear (
	hawk_cut_t* cut /**< text cutter */
);

/**
 * The hawk_cut_comp() function compiles a selector into an internal form.
 * @return 0 on success, -1 on error
 */
HAWK_EXPORT int hawk_cut_comp (
	hawk_cut_t*        cut, /**< text cutter */
	hawk_cut_io_impl_t inf
);

/**
 * The hawk_cut_exec() function executes the compiled commands.
 * @return 0 on success, -1 on error
 */
HAWK_EXPORT int hawk_cut_exec (
	hawk_cut_t*         cut,  /**< text cutter */
	hawk_cut_io_impl_t  inf,  /**< input text stream */
	hawk_cut_io_impl_t  outf  /**< output text stream */
);

/**
 * The hawk_cut_allocmem() function allocates a chunk of memory using
 * the memory manager of \a cut.
 */
HAWK_EXPORT void* hawk_cut_allocmem (
	hawk_cut_t* cut,
	hawk_oow_t size
);

/**
 * The hawk_cut_callocmem() function allocates a chunk of memory using
 * the memory manager of \a cut and clears it to zeros.
 */
HAWK_EXPORT void* hawk_cut_callocmem (
	hawk_cut_t* cut,
	hawk_oow_t size
);

/**
 * The hawk_cut_reallocmem() function reallocates a chunk of memory using
 * the memory manager of \a cut.
 */
HAWK_EXPORT void* hawk_cut_reallocmem (
	hawk_cut_t* cut,
	void*      ptr,
	hawk_oow_t size
);

/**
 * The hawk_cut_freemem() function frees a chunk of memory using
 * the memory manager of \a cut.
 */
HAWK_EXPORT void hawk_cut_freemem (
	hawk_cut_t* cut,
	void*      ptr
);

/**
 * The hawk_cut_openstd() function creates a text cutter with the default
 * memory manager and initialized it for other hawk_cut_xxxxstd functions.
 * @return pointer to a text cutter on success, QSE_NULL on failure.
 */
HAWK_EXPORT hawk_cut_t* hawk_cut_openstd (
	hawk_oow_t     xtnsize,  /**< extension size in bytes */
	hawk_errnum_t* errnum
);

/**
 * The hawk_cut_openstdwithmmgr() function creates a text cutter with a
 * user-defined memory manager. It is equivalent to hawk_cut_openstd(),
 * except that you can specify your own memory manager.
 * @return pointer to a text cutter on success, QSE_NULL on failure.
 */
HAWK_EXPORT hawk_cut_t* hawk_cut_openstdwithmmgr (
	hawk_mmgr_t*    mmgr,    /**< memory manager */
	hawk_oow_t     xtnsize,  /**< extension size in bytes */
	hawk_cmgr_t*   cmgr,
	hawk_errnum_t* errnum
);

/**
 * The hawk_cut_compstd() function compiles a selector from input streams.
 */
HAWK_EXPORT int hawk_cut_compstd (
	hawk_cut_t*      cut,
	hawk_cut_iostd_t in[],
	hawk_oow_t*      count
);

/**
 * The hawk_cut_execstd() function executes the compiled script
 * over an input file @a infile and an output file @a outfile.
 * If @a infile is QSE_NULL, the standard console input is used.
 * If @a outfile is QSE_NULL, the standard console output is used..
 */
HAWK_EXPORT int hawk_cut_execstd (
	hawk_cut_t*        cut,
	hawk_cut_iostd_t   in[],
	hawk_cut_iostd_t*  out
);

HAWK_EXPORT int hawk_cut_execstdfile (
	hawk_cut_t*        cut,
	const hawk_ooch_t* infile,
	const hawk_ooch_t* outfile,
	hawk_cmgr_t*       cmgr
);

#if defined(__cplusplus)
}
#endif

#endif
