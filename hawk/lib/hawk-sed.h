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

#ifndef _HAWK_SED_H_
#define _HAWK_SED_H_

#include <hawk-cmn.h>
#include <hawk-gem.h>

/** @file
 * This file defines data types and functions to use for creating a custom
 * stream editor commonly available on many platforms. A stream editor is 
 * a non-interactive text editing tool that reads text from an input stream, 
 * stores it to pattern space, manipulates the pattern space by applying a set
 * of editing commands, and writes the pattern space to an output stream. 
 * Typically, the input and output streams are a console or a file.
 *
 * @code
 * sed = hawk_sed_open ();
 * hawk_sed_comp (sed);
 * hawk_sed_exec (sed);
 * hawk_sed_close (sed);
 * @endcode
 */

/** @struct hawk_sed_t
 * The hawk_sed_t type defines a stream editor. The structural details are 
 * hidden as it is a relatively complex data type and fragile to external
 * changes. To use a stream editor, you typically can:
 *
 * - create a stream editor object with hawk_sed_open().
 * - compile stream editor commands with hawk_sed_comp().
 * - execute them over input and output streams with hawk_sed_exec().
 * - destroy it with hawk_sed_close() when done.
 *
 * The input and output streams needed by hawk_sed_exec() are implemented in
 * the form of callback functions. You should implement two functions 
 * conforming to the ::hawk_sed_io_impl_t type.
 */
typedef struct hawk_sed_t hawk_sed_t;

#define HAWK_SED_HDR \
	hawk_oow_t _instsize; \
	hawk_gem_t _gem

typedef struct hawk_sed_alt_t hawk_sed_alt_t;
struct hawk_sed_alt_t
{
	/* ensure that hawk_sed_alt_t matches the beginning part of hawk_sed_t */
	HAWK_SED_HDR;
};

typedef struct hawk_sed_adr_t hawk_sed_adr_t; 
typedef struct hawk_sed_cmd_t hawk_sed_cmd_t;

struct hawk_sed_adr_t
{
	enum
	{
		HAWK_SED_ADR_NONE,     /* no address */
		HAWK_SED_ADR_DOL,      /* $ - last line */
		HAWK_SED_ADR_LINE,     /* specified line */
		HAWK_SED_ADR_REX,      /* lines matching regular expression */
		HAWK_SED_ADR_STEP,     /* line steps - only in the second address */
		HAWK_SED_ADR_RELLINE,  /* relative line - only in second address */
		HAWK_SED_ADR_RELLINEM  /* relative line in the multiples - only in second address */
	} type;

	union 
	{
		hawk_oow_t lno;
		void*      rex;
	} u;
};

typedef struct hawk_sed_cut_sel_t hawk_sed_cut_sel_t;

struct hawk_sed_cut_sel_t
{
	hawk_oow_t len;

	struct
	{
		enum
		{
			HAWK_SED_CUT_SEL_CHAR = HAWK_T('c'),
			HAWK_SED_CUT_SEL_FIELD = HAWK_T('f')
		} id;
		hawk_oow_t start;
		hawk_oow_t end;
	} range[128];

	hawk_sed_cut_sel_t* next;
};

#define HAWK_SED_CMD_NOOP            HAWK_T('\0')
#define HAWK_SED_CMD_QUIT            HAWK_T('q')
#define HAWK_SED_CMD_QUIT_QUIET      HAWK_T('Q')
#define HAWK_SED_CMD_APPEND          HAWK_T('a')
#define HAWK_SED_CMD_INSERT          HAWK_T('i')
#define HAWK_SED_CMD_CHANGE          HAWK_T('c')
#define HAWK_SED_CMD_DELETE          HAWK_T('d')
#define HAWK_SED_CMD_DELETE_FIRSTLN  HAWK_T('D')
#define HAWK_SED_CMD_PRINT_LNNUM     HAWK_T('=')
#define HAWK_SED_CMD_PRINT           HAWK_T('p')
#define HAWK_SED_CMD_PRINT_FIRSTLN   HAWK_T('P')
#define HAWK_SED_CMD_PRINT_CLEARLY   HAWK_T('l')
#define HAWK_SED_CMD_HOLD            HAWK_T('h')
#define HAWK_SED_CMD_HOLD_APPEND     HAWK_T('H')
#define HAWK_SED_CMD_RELEASE         HAWK_T('g')
#define HAWK_SED_CMD_RELEASE_APPEND  HAWK_T('G')
#define HAWK_SED_CMD_EXCHANGE        HAWK_T('x') 
#define HAWK_SED_CMD_NEXT            HAWK_T('n')
#define HAWK_SED_CMD_NEXT_APPEND     HAWK_T('N')
#define HAWK_SED_CMD_READ_FILE       HAWK_T('r')
#define HAWK_SED_CMD_READ_FILELN     HAWK_T('R')
#define HAWK_SED_CMD_WRITE_FILE      HAWK_T('w')
#define HAWK_SED_CMD_WRITE_FILELN    HAWK_T('W')
#define HAWK_SED_CMD_BRANCH          HAWK_T('b') 
#define HAWK_SED_CMD_BRANCH_COND     HAWK_T('t')
#define HAWK_SED_CMD_SUBSTITUTE      HAWK_T('s')
#define HAWK_SED_CMD_TRANSLATE       HAWK_T('y')
#define HAWK_SED_CMD_CLEAR_PATTERN   HAWK_T('z')
#define HAWK_SED_CMD_CUT             HAWK_T('C')

struct hawk_sed_cmd_t
{
	hawk_ooch_t type;

	const hawk_ooch_t* lid;
	hawk_loc_t     loc;

	int negated;

	hawk_sed_adr_t a1; /* optional start address */
	hawk_sed_adr_t a2; /* optional end address */

	union
	{
		/* text for the a, i, c commands */
		hawk_oocs_t text;  

		/* file name for r, w, R, W */
		hawk_oocs_t file;

		/* data for the s command */
		struct
		{
			void* rex; /* regular expression */
			hawk_oocs_t rpl;  /* replacement */

			/* flags */
			hawk_oocs_t file; /* file name for w */
			unsigned short occ;
			unsigned short g: 1; /* global */
			unsigned short p: 1; /* print */
			unsigned short i: 1; /* case insensitive */
			unsigned short k: 1; /* kill unmatched portion */
		} subst;

		/* translation set for the y command */
		hawk_oocs_t transet;

		/* branch target for b and t */
		struct
		{
			hawk_oocs_t label;
			hawk_sed_cmd_t* target;
		} branch;

		/* cut command information */
		struct
		{
			hawk_sed_cut_sel_t* fb;/**< points to the first block */
			hawk_sed_cut_sel_t* lb; /**< points to the last block */

			hawk_ooch_t         delim[2]; /**< input/output field delimiters */
			unsigned short     w: 1; /* whitespace for input delimiters. ignore delim[0]. */
			unsigned short     f: 1; /* fold delimiters */
			unsigned short     d: 1; /* delete if not delimited */

			hawk_oow_t         count;
			hawk_oow_t         fcount;
			hawk_oow_t         ccount;
		} cut;
	} u;	

	struct
	{
		int a1_matched;
		hawk_oow_t a1_match_line;

		int c_ready;

		/* points to the next command for fast traversal and 
		 * fast random jumps */
		hawk_sed_cmd_t* next; 
	} state;
};


/**
 * The hawk_sed_opt_t type defines various option types.
 */
enum hawk_sed_opt_t
{
	HAWK_SED_TRAIT,      /**< trait */
	HAWK_SED_TRACER,     /**< tracer hook */
	HAWK_SED_LFORMATTER, /**< formatter for the 'l' command */

	HAWK_SED_DEPTH_REX_BUILD,
	HAWK_SED_DEPTH_REX_MATCH
};
typedef enum hawk_sed_opt_t hawk_sed_opt_t;

/** 
 * The hawk_sed_trait_t type defines various trait codes for a stream editor.
 * Options can be OR'ed with each other and be passed to a stream editor with
 * the hawk_sed_setopt() function.
 */
enum hawk_sed_trait_t
{
	HAWK_SED_STRIPLS      = (1 << 0), /**< strip leading spaces from text */
	HAWK_SED_KEEPTBS      = (1 << 1), /**< keep an trailing backslash */
	HAWK_SED_ENSURENL     = (1 << 2), /**< ensure NL at the text end */
	HAWK_SED_QUIET        = (1 << 3), /**< do not print pattern space */
	HAWK_SED_STRICT       = (1 << 4), /**< do strict address and label check */
	HAWK_SED_EXTENDEDADR  = (1 << 5), /**< allow start~step , addr1,+line, addr1,~line */
	HAWK_SED_SAMELINE     = (1 << 7), /**< allow text on the same line as c, a, i */
	HAWK_SED_EXTENDEDREX  = (1 << 8), /**< use extended regex */
	HAWK_SED_NONSTDEXTREX = (1 << 9)  /**< enable non-standard extensions to regex */
};
typedef enum hawk_sed_trait_t hawk_sed_trait_t;

/**
 * The hawk_sed_io_cmd_t type defines I/O command codes. The code indicates 
 * the action to take in an I/O handler.
 */
enum hawk_sed_io_cmd_t
{
	HAWK_SED_IO_OPEN  = 0,
	HAWK_SED_IO_CLOSE = 1,
	HAWK_SED_IO_READ  = 2,
	HAWK_SED_IO_WRITE = 3
};
typedef enum hawk_sed_io_cmd_t hawk_sed_io_cmd_t;

/**
 * The hawk_sed_io_arg_t type defines a data structure required by 
 * an I/O handler.
 */
struct hawk_sed_io_arg_t
{
	void*             handle; /**< I/O handle */
	const hawk_ooch_t* path;   /**< file path. HAWK_NULL for a console */
};
typedef struct hawk_sed_io_arg_t hawk_sed_io_arg_t;

/** 
 * The hawk_sed_io_impl_t type defines an I/O handler. I/O handlers are called by
 * hawk_sed_exec().
 */
typedef hawk_ooi_t (*hawk_sed_io_impl_t) (
	hawk_sed_t*        sed,
	hawk_sed_io_cmd_t  cmd,
	hawk_sed_io_arg_t* arg,
	hawk_ooch_t*       data,
	hawk_oow_t        count
);

/**
 * The hawk_sed_lformatter_t type defines a text formatter for the 'l' command.
 */
typedef int (*hawk_sed_lformatter_t) (
	hawk_sed_t*        sed,
	const hawk_ooch_t* str,
	hawk_oow_t        len,
	int (*cwriter) (hawk_sed_t*, hawk_ooch_t)
);

/**
 * The hawk_sed_ecb_close_t type defines the callback function
 * called when an sed object is closed.
 */
typedef void (*hawk_sed_ecb_close_t) (
	hawk_sed_t* sed  /**< sed */
);

/**
 * The hawk_sed_ecb_t type defines an event callback set.
 * You can register a callback function set with
 * hawk_sed_pushecb().  The callback functions in the registered
 * set are called in the reverse order of registration.
 */
typedef struct hawk_sed_ecb_t hawk_sed_ecb_t;
struct hawk_sed_ecb_t
{
	/**
	 * called by hawk_sed_close().
	 */
	hawk_sed_ecb_close_t close;

	/* internal use only. don't touch this field */
	hawk_sed_ecb_t* next;
};

enum hawk_sed_tracer_op_t
{
	HAWK_SED_TRACER_READ,
	HAWK_SED_TRACER_WRITE,
	HAWK_SED_TRACER_MATCH,
	HAWK_SED_TRACER_EXEC
};
typedef enum hawk_sed_tracer_op_t hawk_sed_tracer_op_t;

typedef void (*hawk_sed_tracer_t) (
	hawk_sed_t*           sed,
	hawk_sed_tracer_op_t  op,
	const hawk_sed_cmd_t* cmd
);

/**
 * The hawk_sed_space_t type defines the types of
 * sed bufferspaces. 
 */
enum hawk_sed_space_t
{
	HAWK_SED_SPACE_HOLD,   /**< hold space */
	HAWK_SED_SPACE_PATTERN /**< pattern space */
};
typedef enum hawk_sed_space_t hawk_sed_space_t;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_sed_open() function creates a stream editor object. A memory
 * manager provided is used to allocate and destory the object and any dynamic
 * data through out its lifetime. An extension area is allocated if an
 * extension size greater than 0 is specified. You can access it with the
 * hawk_sed_getxtn() function and use it to store arbitrary data associated
 * with the object.  When done, you should destroy the object with the 
 * hawk_sed_close() function to avoid any resource leaks including memory. 
 * @return pointer to a stream editor on success, HAWK_NULL on failure
 */
HAWK_EXPORT hawk_sed_t* hawk_sed_open (
	hawk_mmgr_t*   mmgr,    /**< memory manager */
	hawk_oow_t     xtnsize, /**< extension size in bytes */
	hawk_errnum_t* errnum
);

/**
 * The hawk_sed_close() function destroys a stream editor.
 */
HAWK_EXPORT void hawk_sed_close (
	hawk_sed_t* sed /**< stream editor */
);



#if defined(HAWK_HAVE_INLINE)
/**
 * The hawk_sed_getxtn() function returns the pointer to the extension area
 * placed behind the actual sed object.
 */
static HAWK_INLINE void* hawk_sed_getxtn (hawk_sed_t* sed) { return (void*)((hawk_uint8_t*)sed + ((hawk_sed_alt_t*)sed)->_instsize); }

/**
 * The hawk_sed_getgem() function gets the pointer to the gem structure of the 
 * sed object. 
 */
static HAWK_INLINE hawk_gem_t* hawk_sed_getgem (hawk_sed_t* sed) { return &((hawk_sed_alt_t*)sed)->_gem; }

/**
 * The hawk_sed_getmmgr() function gets the memory manager used in
 * hawk_sed_open().
 */
static HAWK_INLINE hawk_mmgr_t* hawk_sed_getmmgr (hawk_sed_t* sed) { return ((hawk_sed_alt_t*)sed)->_gem.mmgr; }
#else
#define hawk_sed_getxtn(sed) ((void*)((hawk_uint8_t*)sed + ((hawk_sed_alt_t*)sed)->_instsize))
#define hawk_sed_getgem(sed) (&((hawk_sed_alt_t*)(sed))->_gem)
#define hawk_sed_getmmgr(sed) (((hawk_sed_alt_t*)(sed))->_gem.mmgr)
#endif /* HAWK_HAVE_INLINE */


/**
 * The hawk_sed_getopt() function gets the value of an option
 * specified by \a id into the buffer pointed to by \a value.
 *
 * The \a value field is dependent on \a id:
 *  - #HAWK_SED_TRAIT - int*, 0 or bitwised-ORed of #hawk_sed_trait_t values
 *  - #HAWK_SED_TRACER - hawk_sed_tracer_t*
 *  - #HAWK_SED_LFORMATTER - hawk_sed_lformatter_t*
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_sed_getopt (
	hawk_sed_t*    sed,
	hawk_sed_opt_t id,
	void*         value
);

/**
 * The hawk_sed_setopt() function sets the value of an option 
 * specified by \a id to the value pointed to by \a value.
 *
 * The \a value field is dependent on \a id:
 *  - #HAWK_SED_TRAIT - const int*, 0 or bitwised-ORed of #hawk_sed_trait_t values
 *  - #HAWK_SED_TRACER - hawk_sed_tracer_t
 *  - #HAWK_SED_LFORMATTER - hawk_sed_lformatter_t
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_sed_setopt (
	hawk_sed_t*    sed,
	hawk_sed_opt_t id,
	const void*   value
);

/**
 * The hawk_sed_geterrnum() function returns the number of the last error 
 * occurred.
 * \return error number
 */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_errnum_t hawk_sed_geterrnum (hawk_sed_t* sed) { return ((hawk_sed_alt_t*)sed)->_gem.errnum; }
#else
#	define hawk_sed_geterrnum(sed) (((hawk_sed_alt_t*)(sed))->_gem.errnum)
#endif

/**
 * The hawk_sed_geterrloc() function gets the location where the last error 
 * has occurred.
 * @return error location
 */
HAWK_EXPORT const hawk_loc_t* hawk_sed_geterrloc (
	hawk_sed_t* sed /**< stream editor */
);

/**
 * The hawk_sed_geterrmsg() function gets a string describing the last error.
 * @return error message pointer
 */
HAWK_EXPORT const hawk_ooch_t* hawk_sed_geterrmsg (
	hawk_sed_t* sed /**< stream editor */
);

/**
 * The hawk_sed_geterror() function gets an error number, an error location, 
 * and an error message. The information is set to the memory area pointed 
 * to by each parameter.
 */
HAWK_EXPORT void hawk_sed_geterror (
	hawk_sed_t*         sed,    /**< stream editor */
	hawk_errnum_t*      errnum, /**< error number */
	const hawk_ooch_t** errmsg, /**< error message */
	hawk_loc_t*         errloc  /**< error location */
);

/**
 * The hawk_sed_seterrnum() function sets the error information omitting 
 * error location. You must pass a non-NULL for \a errarg if the specified
 * error number \a errnum requires one or more arguments to format an
 * error message.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_sed_seterrnum (hawk_sed_t* sed, const hawk_loc_t* errloc, hawk_errnum_t errnum) { hawk_gem_seterrnum (hawk_sed_getgem(sed), errloc, errnum); }
#else
#define hawk_sed_seterrnum(sed, errloc, errnum) hawk_sed_gem_seterrnum(hawk_sed_getgem(sed), errloc, errnum)
#endif

/**
 * The hawk_sed_seterrmsg() function sets error information with a customized 
 * message for a given error number.
 */
HAWK_EXPORT void hawk_sed_seterrmsg (
	hawk_sed_t*        sed,      /**< stream editor */
	hawk_errnum_t      errnum,   /**< error number */
	const hawk_ooch_t* errmsg,   /**< error message */
	const hawk_loc_t*  errloc /**< error location */
);

/**
 * The hawk_sed_seterror() function sets an error number, an error location, and
 * an error message. An error string is composed of a formatting string
 * and an array of formatting parameters.
 */
HAWK_EXPORT void hawk_sed_seterror (
	hawk_sed_t*        sed,    /**< stream editor */
	hawk_errnum_t      errnum, /**< error number */
	const hawk_oocs_t* errarg, /**< array of arguments for formatting an error message */
	const hawk_loc_t*  errloc  /**< error location */
);

/**
 * The hawk_sed_popecb() function pops an sed event callback set
 * and returns the pointer to it. If no callback set can be popped,
 * it returns #HAWK_NULL.
 */
HAWK_EXPORT hawk_sed_ecb_t* hawk_sed_popecb (
	hawk_sed_t* sed /**< sed */
);

/**
 * The hawk_sed_pushecb() function register a runtime callback set.
 */
HAWK_EXPORT void hawk_sed_pushecb (
	hawk_sed_t*     sed, /**< sed */
	hawk_sed_ecb_t* ecb  /**< callback set */
);

/**
 * The hawk_sed_comp() function compiles editing commands into an internal form.
 * @return 0 on success, -1 on error 
 */
HAWK_EXPORT int hawk_sed_comp (
	hawk_sed_t*        sed, /**< stream editor */
	hawk_sed_io_impl_t  inf  /**< script stream reader */
);

/**
 * The hawk_sed_exec() function executes the compiled commands.
 * @return 0 on success, -1 on error
 */
HAWK_EXPORT int hawk_sed_exec (
	hawk_sed_t*        sed,  /**< stream editor */
	hawk_sed_io_impl_t  inf,  /**< stream reader */
	hawk_sed_io_impl_t  outf  /**< stream writer */
);

/**
 * The hawk_sed_halt() function breaks running loop in hawk_sed_exec().
 * It doesn't affect blocking calls in stream handlers.
 */
HAWK_EXPORT void hawk_sed_halt (
	hawk_sed_t* sed   /**< stream editor */
);

/**
 * The hawk_sed_ishalt() functions tests if hawk_sed_halt() is called.
 */
HAWK_EXPORT int hawk_sed_ishalt (
	hawk_sed_t* sed   /**< stream editor */
);
	
/**
 * The hawk_sed_getcompid() function returns the latest
 * identifier successfully set with hawk_sed_setcompid(). 
 */
HAWK_EXPORT const hawk_ooch_t* hawk_sed_getcompid (
	hawk_sed_t* sed
);

/**
 * The hawk_sed_setcompid() functions duplicates a string
 * pointed to by @a id and stores it internally to identify
 * the script currently being compiled. The lid field of the 
 * current command being compiled in the script is set to the 
 * lastest identifer successfully set with this function.
 * If this function fails, the location set in the command
 * may be wrong.
 */
HAWK_EXPORT const hawk_ooch_t* hawk_sed_setcompid (
	hawk_sed_t*        sed,
	const hawk_ooch_t* id
);

/**
 * The hawk_sed_getlinnum() function gets the current input line number.
 * @return current input line number
 */
HAWK_EXPORT hawk_oow_t hawk_sed_getlinenum (
	hawk_sed_t* sed /**< stream editor */
);

/**
 * The hawk_sed_setlinenum() function changes the current input line number.
 */
HAWK_EXPORT void hawk_sed_setlinenum (
	hawk_sed_t* sed,   /**< stream editor */
	hawk_oow_t num    /**< a line number */
);


/**
 * The hawk_sed_allocmem() function allocates a chunk of memory using
 * the memory manager of \a sed.
 */
HAWK_EXPORT void* hawk_sed_allocmem (
	hawk_sed_t* sed,
	hawk_oow_t size
);

/**
 * The hawk_sed_allocmem() function allocates a chunk of memory using
 * the memory manager of \a sed and clears it to zeros.
 */
HAWK_EXPORT void* hawk_sed_callocmem (
	hawk_sed_t* sed,
	hawk_oow_t size
);

/**
 * The hawk_sed_allocmem() function reallocates a chunk of memory using
 * the memory manager of \a sed.
 */
HAWK_EXPORT void* hawk_sed_reallocmem (
	hawk_sed_t* sed,
	void*      ptr,
	hawk_oow_t size
);

/**
 * The hawk_sed_allocmem() function frees a chunk of memory using
 * the memory manager of \a sed.
 */
HAWK_EXPORT void hawk_sed_freemem (
	hawk_sed_t* sed,
	void*      ptr
);

/**
 * The hawk_sed_getspace() function gets the pointer and the length
 * to a buffer space specfied by \a space.
 */
HAWK_EXPORT void hawk_sed_getspace (
	hawk_sed_t*      sed,
	hawk_sed_space_t space,
	hawk_oocs_t*     str
);

#if defined(__cplusplus)
}
#endif

#endif
