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

#ifndef _HAWK_STD_H_
#define _HAWK_STD_H_

#include <hawk.h>

/** \file
 * This file defines functions and data types that help you create
 * an hawk interpreter with less effort. It is designed to be as close
 * to conventional hawk implementations as possible.
 *
 * The source script handler does not evaluate a file name of the "var=val"
 * form as an assignment expression. Instead, it just treats it as a
 * normal file name.
 */

/**
 * The hawk_parsestd_type_t type defines standard source I/O types.
 */
enum hawk_parsestd_type_t
{
	HAWK_PARSESTD_NULL   = 0, /**< pseudo-value to indicate no script */
	HAWK_PARSESTD_FILE   = 1, /**< file path */
	HAWK_PARSESTD_FILEB  = 2, /**< file path */
	HAWK_PARSESTD_FILEU  = 3, /**< file path */
	HAWK_PARSESTD_OOCS   = 4, /**< length-bounded string */
	HAWK_PARSESTD_BCS    = 5,
	HAWK_PARSESTD_UCS    = 6

};
typedef enum hawk_parsestd_type_t hawk_parsestd_type_t;

/**
 * The hawk_parsestd_t type defines a source I/O.
 */
struct hawk_parsestd_t
{
	hawk_parsestd_type_t type;

	union
	{
		struct
		{
			/** file path to open. #HAWK_NULL or '-' for stdin/stdout. */
			const hawk_ooch_t* path;

			/** a stream created with the file path is set with this
			 * cmgr if it is not #HAWK_NULL. */
			hawk_cmgr_t*  cmgr;
		} file;

		struct
		{
			/** file path to open. #HAWK_NULL or '-' for stdin/stdout. */
			const hawk_bch_t* path;

			/** a stream created with the file path is set with this
			 * cmgr if it is not #HAWK_NULL. */
			hawk_cmgr_t*  cmgr;
		} fileb;

		struct
		{
			/** file path to open. #HAWK_NULL or '-' for stdin/stdout. */
			const hawk_uch_t* path;

			/** a stream created with the file path is set with this
			 * cmgr if it is not #HAWK_NULL. */
			hawk_cmgr_t*  cmgr;
		} fileu;

		/**
		 * input string or dynamically allocated output string
		 *
		 * For input, the ptr and the len field of str indicates the
		 * pointer and the length of a string to read. You must set
		 * these fields before calling hawk_parsestd().
		 *
		 * For output, the ptr and the len field of str indicates the
		 * pointer and the length of a deparsed source string. The output
		 * string is dynamically allocated. You don't need to set these
		 * fields before calling hawk_parsestd() because they are set
		 * by hawk_parsestd() and valid while the relevant hawk object
		 * is alive. You must free the memory chunk pointed to by the
		 * ptr field with hawk_freemem() once you're done with it to
		 * avoid memory leaks.
		 */
		hawk_oocs_t oocs;
		hawk_bcs_t bcs;
		hawk_ucs_t ucs;
	} u;
};

typedef struct hawk_parsestd_t hawk_parsestd_t;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_openstd() function creates an hawk object using the default
 * memory manager and primitive functions. Besides, it adds a set of
 * standard intrinsic functions like atan, system, etc. Use this function
 * over hawk_open() if you don't need finer-grained customization.
 */
HAWK_EXPORT hawk_t* hawk_openstd (
	hawk_oow_t     xtnsize,  /**< extension size in bytes */
	hawk_errnum_t* errnum    /**< pointer to an error number variable */
);

/**
 * The hawk_openstdwithmmgr() function creates an hawk object with a
 * user-defined memory manager. It is equivalent to hawk_openstd(),
 * except that you can specify your own memory manager.
 */
HAWK_EXPORT hawk_t* hawk_openstdwithmmgr (
	hawk_mmgr_t*   mmgr,     /**< memory manager */
	hawk_oow_t     xtnsize,  /**< extension size in bytes */
	hawk_cmgr_t*   cmgr,     /**< character encoding manager */
	hawk_errnum_t* errnum    /**< pointer to an error number variable */
);

/**
 * The hawk_parsestd() functions parses source script.
 * The code below shows how to parse a literal string 'BEGIN { print 10; }'
 * and deparses it out to a buffer 'buf'.
 * \code
 * int n;
 * hawk_parsestd_t in[2];
 * hawk_parsestd_t out;
 *
 * in[0].type = HAWK_PARSESTD_OOCS;
 * in[0].u.str.ptr = HAWK_T("BEGIN { print 10; }");
 * in[0].u.str.len = hawk_count_oocstr(in.u.str.ptr);
 * in[1].type = HAWK_PARSESTD_NULL;
 * out.type = HAWK_PARSESTD_OOCS;
 * n = hawk_parsestd(hawk, in, &out);
 * if (n >= 0)
 * {
 *   hawk_printf (HAWK_T("%s\n"), out.u.str.ptr);
 *   HAWK_MMGR_FREE (out.u.str.ptr);
 * }
 * \endcode
 */
HAWK_EXPORT int hawk_parsestd (
	hawk_t*          hawk,
	hawk_parsestd_t  in[],
	hawk_parsestd_t* out
);

/**
 * The hawk_rtx_openstdwithbcstr() function creates a standard runtime context.
 * The caller should keep the contents of \a icf and \a ocf valid throughout
 * the lifetime of the runtime context created. The \a cmgr is set to the
 * streams created with \a icf and \a ocf if it is not #HAWK_NULL.
 */
HAWK_EXPORT hawk_rtx_t* hawk_rtx_openstdwithbcstr (
	hawk_t*           hawk,
	hawk_oow_t        xtnsize,
	const hawk_bch_t* id,
	hawk_bch_t*       icf[],
	hawk_bch_t*       ocf[],
	hawk_cmgr_t*      cmgr
);

/**
 * The hawk_rtx_openstdwithucstr() function creates a standard runtime context.
 * The caller should keep the contents of \a icf and \a ocf valid throughout
 * the lifetime of the runtime context created. The \a cmgr is set to the
 * streams created with \a icf and \a ocf if it is not #HAWK_NULL.
 */
HAWK_EXPORT hawk_rtx_t* hawk_rtx_openstdwithucstr (
	hawk_t*           hawk,
	hawk_oow_t        xtnsize,
	const hawk_uch_t* id,
	hawk_uch_t*       icf[],
	hawk_uch_t*       ocf[],
	hawk_cmgr_t*      cmgr
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_rtx_openstd hawk_rtx_openstdwithbcstr
#else
#	define hawk_rtx_openstd hawk_rtx_openstdwithucstr
#endif

/**
 * The hawk_rtx_getiocmgrstd() function gets the current character
 * manager associated with a particular I/O target indicated by the name
 * \a ioname if #HAWK_OOCH_IS_UCH is defined. It always returns #HAWK_NULL
 * if #HAWK_OOCH_IS_BCH is defined.
 */
HAWK_EXPORT hawk_cmgr_t* hawk_rtx_getiocmgrstd (
	hawk_rtx_t*         rtx,
	const hawk_ooch_t* ioname
);


/* ------------------------------------------------------------------------- */


HAWK_EXPORT hawk_flt_t hawk_stdmathpow (
	hawk_t*    hawk,
	hawk_flt_t x,
	hawk_flt_t y
);

HAWK_EXPORT hawk_flt_t hawk_stdmathmod (
	hawk_t*    hawk,
	hawk_flt_t x,
	hawk_flt_t y
);

HAWK_EXPORT int hawk_stdmodstartup (
	hawk_t* hawk
);

HAWK_EXPORT void hawk_stdmodshutdown (
	hawk_t* hawk
);

HAWK_EXPORT void* hawk_stdmodopen (
	hawk_t*                hawk,
	const hawk_mod_spec_t* spec
);

HAWK_EXPORT void hawk_stdmodclose (
	hawk_t*                hawk,
	void*                  handle
);

HAWK_EXPORT void* hawk_stdmodgetsym (
	hawk_t*                hawk,
	void*                  handle,
	const hawk_ooch_t*     name
);


HAWK_EXPORT int hawk_stdplainfileexists (
	hawk_t*                hawk,
     const hawk_ooch_t*     file
);

HAWK_EXPORT const hawk_ooch_t* hawk_stdgetfileindirs (
	hawk_t*                hawk,
	const hawk_oocs_t*     dirs,
	const hawk_ooch_t*     file
);

/* ------------------------------------------------------------------------- */

HAWK_EXPORT hawk_mmgr_t* hawk_get_sys_mmgr (
	void
);

#if defined(__cplusplus)
}
#endif


#endif
