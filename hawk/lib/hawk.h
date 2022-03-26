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

#ifndef _HAWK_H_
#define _HAWK_H_

#include <hawk-cmn.h>
#include <hawk-ecs.h>
#include <hawk-gem.h>
#include <hawk-tre.h>
#include <hawk-utl.h>

#include <hawk-htb.h> /* for rtx->named */
#define HAWK_MAP_IS_RBT
#include <hawk-map.h>
#include <hawk-arr.h>
#include <stdarg.h>

/** \file
 * An embeddable AWK interpreter is defined in this header file.
 *
 * \todo
 * - make enhancement to treat a function as a value
 * - improve performance of hawk_rtx_readio() if RS is logner than 2 chars.
 * - consider something like ${1:3,5} => $1, $2, $3, and $5 concatenated
 */

/** \struct hawk_t
 * The #hawk_t type defines an AWK interpreter. It provides an interface
 * to parse an AWK script and run it to manipulate input and output data.
 *
 * In brief, you need to call APIs with user-defined handlers to run a typical
 * AWK script as shown below:
 * 
 * \code
 * hawk_t* hawk;
 * hawk_rtx_t* rtx;
 * hawk_sio_cbs_t sio; // need to initialize it with callback functions
 * hawk_rio_cbs_t rio; // need to initialize it with callback functions
 *
 * hawk = hawk_open(mmgr, 0, prm); // create an interpreter 
 * hawk_parse (hawk, &sio);          // parse a script 
 * rtx = hawk_rtx_open(hawk, 0, &rio); // create a runtime context 
 * retv = hawk_rtx_loop(rtx);     // run a standard AWK loop 
 * if (retv) hawk_rtx_refdownval (rtx, retv); // free return value
 * hawk_rtx_close (rtx);           // destroy the runtime context
 * hawk_close (hawk);               // destroy the interpreter
 * \endcode
 *
 * It provides an interface to change the conventional behavior of the 
 * interpreter; most notably, you can call a particular function with 
 * hawk_rtx_call() instead of entering the BEGIN, pattern-action blocks, END
 * loop. By doing this, you may utilize a script in an event-driven way.
 *
 * \sa hawk_rtx_t hawk_open hawk_close
 */
#define HAWK_HDR \
	hawk_oow_t _instsize; \
	hawk_gem_t _gem;

typedef struct hawk_alt_t hawk_alt_t;
struct hawk_alt_t
{
	/* ensure that hawk_alt_t matches the beginning part of hawk_t */
	HAWK_HDR;
};

/** \struct hawk_rtx_t
 * The #hawk_rtx_t type defines a runtime context. A runtime context 
 * maintains runtime state for a running script. You can create multiple
 * runtime contexts out of a single AWK interpreter; in other words, you 
 * can run the same script with different input and output data by providing
 * customized I/O handlers when creating a runtime context with 
 * hawk_rtx_open().
 *
 * I/O handlers are categoriezed into three kinds: console, file, pipe.
 * The #hawk_rio_t type defines as a callback a set of I/O handlers 
 * to handle runtime I/O:
 * - getline piped in from a command reads from a pipe.
 *   ("ls -l" | getline line)
 * - print and printf piped out to a command write to a pipe.
 *   (print 2 | "sort")
 * - getline redirected in reads from a file.
 *   (getline line < "file")
 * - print and printf redirected out write to a file.
 *   (print num > "file")
 * - The pattern-action loop and getline with no redirected input
 *   read from a console. (/susie/ { ... })
 * - print and printf write to a console. (print "hello, world")
 *
 * \sa hawk_t hawk_rtx_open hawk_rio_t
 */
typedef struct hawk_rtx_t hawk_rtx_t;

#define HAWK_RTX_HDR \
	hawk_oow_t _instsize; \
	hawk_gem_t _gem; \
	int id; \
	hawk_t* hawk

typedef struct hawk_rtx_alt_t hawk_rtx_alt_t;
struct hawk_rtx_alt_t
{
	/* ensure that hawk_rtx_alt_t matches the beginning part of hawk_rtx_t */
	HAWK_RTX_HDR;
};

/* ------------------------------------------------------------------------ */

/* garbage collection header */
typedef struct hawk_gch_t hawk_gch_t;
struct hawk_gch_t
{
	hawk_gch_t* gc_prev;
	hawk_gch_t* gc_next;
	hawk_uintptr_t  gc_refs;
};

#if defined(HAWK_HAVE_INLINE)

static HAWK_INLINE hawk_val_t* hawk_gch_to_val(hawk_gch_t* gch)
{
	return (hawk_val_t*)(gch + 1);
}

static HAWK_INLINE hawk_gch_t* hawk_val_to_gch(hawk_val_t* v)
{
	return ((hawk_gch_t*)v) - 1;
}

#else
#	define hawk_val_to_gch(v) (((hawk_gch_t*)(v)) - 1)
#	define hawk_gch_to_val(gch) ((hawk_val_t*)(((hawk_gch_t*)(gch)) + 1))
#endif

/**
 * The #HAWK_VAL_HDR defines the common header for a value.
 * Three common fields are:
 * - v_type - type of a value from #hawk_val_type_t
 * - v_refs - reference count
 * - v_static - static value indicator
 * - v_nstr - numeric string marker, 1 -> integer, 2 -> floating-point number
 * - v_gc - used for garbage collection together with v_refs
 */
/*
#define HAWK_VAL_HDR \
	unsigned int v_type: 4; \
	unsigned int v_refs: 24; \
	unsigned int v_static: 1; \
	unsigned int v_nstr: 2; \
	unsigned int v_gc: 1
*/
#define HAWK_VAL_HDR \
	hawk_uintptr_t v_type: 4; \
	hawk_uintptr_t v_refs: ((HAWK_SIZEOF_UINTPTR_T * 8) - 8); \
	hawk_uintptr_t v_static: 1; \
	hawk_uintptr_t v_nstr: 2; \
	hawk_uintptr_t v_gc: 1

/**
 * The hawk_val_t type is an abstract value type. A value commonly contains:
 * - type of a value
 * - reference count
 * - indicator for a numeric string
 */
struct hawk_val_t
{
	HAWK_VAL_HDR;
};

/**
 * The hawk_val_nil_t type is a nil value type. The type field is 
 * #HAWK_VAL_NIL.
 */
struct hawk_val_nil_t
{
	HAWK_VAL_HDR;
};
typedef struct hawk_val_nil_t  hawk_val_nil_t;

/**
 * The hawk_val_int_t type is an integer number type. The type field is
 * #HAWK_VAL_INT.
 */
struct hawk_val_int_t
{
	HAWK_VAL_HDR;
	hawk_int_t i_val;
	void*      nde;
};
typedef struct hawk_val_int_t hawk_val_int_t;

/**
 * The hawk_val_flt_t type is a floating-point number type. The type field
 * is #HAWK_VAL_FLT.
 */
struct hawk_val_flt_t
{
	HAWK_VAL_HDR;
	hawk_flt_t val;
	void*      nde;
};
typedef struct hawk_val_flt_t hawk_val_flt_t;

/**
 * The hawk_val_str_t type is a string type. The type field is
 * #HAWK_VAL_STR.
 */
struct hawk_val_str_t
{
	HAWK_VAL_HDR;
	hawk_oocs_t val;
};
typedef struct hawk_val_str_t  hawk_val_str_t;

/**
 * The hawk_val_str_t type is a string type. The type field is
 * #HAWK_VAL_MBS.
 */
struct hawk_val_mbs_t
{
	HAWK_VAL_HDR;
	hawk_bcs_t val;
};
typedef struct hawk_val_mbs_t hawk_val_mbs_t;

/**
 * The hawk_val_rex_t type is a regular expression type.  The type field 
 * is #HAWK_VAL_REX.
 */
struct hawk_val_rex_t
{
	HAWK_VAL_HDR;
	hawk_oocs_t str;
	hawk_tre_t* code[2];
};
typedef struct hawk_val_rex_t  hawk_val_rex_t;

/**
 * The hawk_val_map_t type defines a map type. The type field is 
 * #HAWK_VAL_MAP.
 */
struct hawk_val_map_t
{
	HAWK_VAL_HDR;

	/* TODO: make val_map to array if the indices used are all 
	 *       integers switch to map dynamically once the 
	 *       non-integral index is seen.
	 */
	hawk_map_t* map; 
};
typedef struct hawk_val_map_t  hawk_val_map_t;

/**
 * The hawk_val_arr_t type defines a arr type. The type field is 
 * #HAWK_VAL_MAP.
 */
struct hawk_val_arr_t
{
	HAWK_VAL_HDR;

	/* TODO: make val_arr to array if the indices used are all 
	 *       integers switch to arr dynamically once the 
	 *       non-integral index is seen.
	 */
	hawk_arr_t* arr; 
};
typedef struct hawk_val_arr_t  hawk_val_arr_t;

/**
 * The hawk_val_ref_t type defines a reference type that is used
 * internally only. The type field is #HAWK_VAL_REF.
 */
struct hawk_val_ref_t
{
	HAWK_VAL_HDR;

	enum
	{
		/* keep these items in the same order as corresponding items
		 * in hawk_nde_type_t. */
		HAWK_VAL_REF_NAMED,    /**< plain named variable */
		HAWK_VAL_REF_GBL,      /**< plain global variable */
		HAWK_VAL_REF_LCL,      /**< plain local variable */
		HAWK_VAL_REF_ARG,      /**< plain function argument */
		HAWK_VAL_REF_NAMEDIDX, /**< member of named map variable */
		HAWK_VAL_REF_GBLIDX,   /**< member of global map variable */
		HAWK_VAL_REF_LCLIDX,   /**< member of local map variable */
		HAWK_VAL_REF_ARGIDX,   /**< member of map argument */
		HAWK_VAL_REF_POS       /**< positional variable */
	} id;

	/* if id is HAWK_VAL_REF_POS, adr holds the index of a 
	 * positional variable. If id is HAWK_VAL_REF_GBL, adr hold
	 * the index of a global variable. Otherwise, adr points to the value 
	 * directly. */
	hawk_val_t** adr;
};
typedef struct hawk_val_ref_t  hawk_val_ref_t;

/**
 * The hawk_val_map_itr_t type defines the iterator to map value fields.
 */
typedef hawk_map_itr_t hawk_val_map_itr_t;

/**
 * The #HAWK_VAL_MAP_ITR_KEY macro get the pointer to the key part 
 * of a map value.
 */
#define HAWK_VAL_MAP_ITR_KEY(itr) ((const hawk_oocs_t*)HAWK_MAP_KPTL((itr)->pair))

/**
 * The #HAWK_VAL_MAP_ITR_VAL macro get the pointer to the value part 
 * of a map value.
 */
#define HAWK_VAL_MAP_ITR_VAL(itr) ((const hawk_val_t*)HAWK_MAP_VPTR((itr)->pair))


/**
 * The hawk_val_map_data_type_t type defines the type of
 * map value data for the #hawk_val_map_data_t structure.
 */
enum hawk_val_map_data_type_t
{
	HAWK_VAL_MAP_DATA_INT  = 0,
	HAWK_VAL_MAP_DATA_FLT,
	HAWK_VAL_MAP_DATA_OOCSTR,
	HAWK_VAL_MAP_DATA_BCSTR,
	HAWK_VAL_MAP_DATA_UCSTR,
	HAWK_VAL_MAP_DATA_OOCS,
	HAWK_VAL_MAP_DATA_BCS,
	HAWK_VAL_MAP_DATA_UCS
};
typedef enum hawk_val_map_data_type_t hawk_val_map_data_type_t;

/**
 * The hawk_val_map_data_t type defines a structure that
 * describes a key/value pair for a map value to be created
 * with hawk_makemapvalwithdata().
 */
struct hawk_val_map_data_t
{
	hawk_oocs_t               key;
	hawk_val_map_data_type_t  type: 8;
	unsigned int              type_size: 16; /* size information that supplements the type */
	/* maybe there are some unused bits here as this struct is not packed */
	void*                     vptr;
};

typedef struct hawk_val_map_data_t hawk_val_map_data_t;


/* ------------------------------------------------------------------------ */

/**
 * The hawk_nde_type_t defines the node types.
 */
enum hawk_nde_type_t
{
	HAWK_NDE_NULL,

	/* statement */
	HAWK_NDE_BLK,
	HAWK_NDE_IF,
	HAWK_NDE_WHILE,
	HAWK_NDE_DOWHILE,
	HAWK_NDE_FOR,
	HAWK_NDE_FORIN,
	HAWK_NDE_BREAK,
	HAWK_NDE_CONTINUE,
	HAWK_NDE_RETURN,
	HAWK_NDE_EXIT,
	HAWK_NDE_NEXT,
	HAWK_NDE_NEXTFILE,
	HAWK_NDE_DELETE,
	HAWK_NDE_RESET,

	/* expression */
	/* if you change the following values including their order,
	 * you should change __evaluator of eval_expression0()
	 * in run.c accordingly */
	HAWK_NDE_GRP, 
	HAWK_NDE_ASS,
	HAWK_NDE_EXP_BIN,
	HAWK_NDE_EXP_UNR,
	HAWK_NDE_EXP_INCPRE,
	HAWK_NDE_EXP_INCPST,
	HAWK_NDE_CND,
	HAWK_NDE_FNCALL_FNC,
	HAWK_NDE_FNCALL_FUN,
	HAWK_NDE_FNCALL_VAR,
	HAWK_NDE_CHAR,
	HAWK_NDE_BCHR,
	HAWK_NDE_INT,
	HAWK_NDE_FLT,
	HAWK_NDE_STR,
	HAWK_NDE_MBS,
	HAWK_NDE_REX,
	HAWK_NDE_XNIL,
	HAWK_NDE_FUN,

	/* keep this order for the following items otherwise, you may have 
	 * to change eval_incpre and eval_incpst in run.c as well as
	 * HAWK_VAL_REF_XXX in hawk_val_ref_t. also do_assignment_map()
	 * in run.c converts HAWK_NDE_XXXIDX to HAWK_NDE_XXX by
	 * decrementing by 4. */
	HAWK_NDE_NAMED,
	HAWK_NDE_GBL,
	HAWK_NDE_LCL,
	HAWK_NDE_ARG,
	HAWK_NDE_NAMEDIDX,
	HAWK_NDE_GBLIDX,
	HAWK_NDE_LCLIDX,
	HAWK_NDE_ARGIDX,
	HAWK_NDE_POS,
	/* ---------------------------------- */

	HAWK_NDE_GETLINE,
	HAWK_NDE_PRINT,
	HAWK_NDE_PRINTF
};
typedef enum hawk_nde_type_t hawk_nde_type_t;

#define HAWK_NDE_HDR \
	hawk_nde_type_t type; \
	hawk_loc_t      loc; \
	hawk_nde_t*     next

/** \struct hawk_nde_t
 * The hawk_nde_t type defines a common part of a node.
 */
typedef struct hawk_nde_t  hawk_nde_t;
struct hawk_nde_t
{
	HAWK_NDE_HDR;
};

/* ------------------------------------------------------------------------ */

/**
 * The hawk_fun_t type defines a structure to maintain functions
 * defined with the keyword 'function'.
 */
struct hawk_fun_t
{
	hawk_oocs_t     name;
	hawk_oow_t      nargs;
	hawk_ooch_t*    argspec; /* similar to  the argument spec of hawk_fnc_arg_t. supports v & r only. */
	hawk_nde_t*     body;
};
typedef struct hawk_fun_t hawk_fun_t;

struct hawk_val_fun_t
{
	HAWK_VAL_HDR;
	hawk_fun_t* fun;
};
typedef struct hawk_val_fun_t  hawk_val_fun_t;

/* ------------------------------------------------------------------------ */

typedef hawk_flt_t (*hawk_math1_t) (
	hawk_t* hawk,
	hawk_flt_t x
);

typedef hawk_flt_t (*hawk_math2_t) (
	hawk_t* hawk,
	hawk_flt_t x, 
	hawk_flt_t y
);

/* ------------------------------------------------------------------------ */

typedef struct hawk_mod_spec_t hawk_mod_spec_t;

struct hawk_mod_spec_t
{
	const hawk_ooch_t* libdir;
	const hawk_ooch_t* prefix;
	const hawk_ooch_t* postfix;
	const hawk_ooch_t* name;
};

typedef void* (*hawk_mod_open_t) (
	hawk_t*                hawk,
	const hawk_mod_spec_t* spec
);

typedef void* (*hawk_mod_getsym_t) (
	hawk_t*            hawk,
	void*              handle,
	const hawk_ooch_t* name
);

typedef void (*hawk_mod_close_t) (
	hawk_t*    hawk,
	void*      handle
);

/* ------------------------------------------------------------------------ */

typedef void (*hawk_log_write_t) (
	hawk_t*             hawk,
	hawk_bitmask_t      mask,
	const hawk_ooch_t*  msg,
	hawk_oow_t          len
);

/* ------------------------------------------------------------------------ */

#if 0
typedef void* (*hawk_buildrex_t) (
	hawk_t*            hawk,
	const hawk_ooch_t* ptn, 
	hawk_oow_t         len
);

typedef int (*hawk_matchrex_t) (
	hawk_t*             hawk,
	void*               code,
	int                 option,
	const hawk_ooch_t*  str,
	hawk_oow_t          len, 
	const hawk_ooch_t** mptr,
	hawk_oow_t*         mlen
);

typedef void (*hawk_freerex_t) (
	hawk_t* hawk,
	void*   code
);

typedef int (*hawk_isemptyrex_t) (
	hawk_t* hawk,
	void*   code
);
#endif

/**
 * The hawk_sio_cmd_t type defines I/O commands for a script stream.
 */
enum hawk_sio_cmd_t
{
	HAWK_SIO_CMD_OPEN   = 0, /**< open a script stream */
	HAWK_SIO_CMD_CLOSE  = 1, /**< close a script stream */
	HAWK_SIO_CMD_READ   = 2, /**< read text from an input script stream */
	HAWK_SIO_CMD_WRITE  = 3  /**< write text to an output script stream */
};
typedef enum hawk_sio_cmd_t hawk_sio_cmd_t;

/**
 * The hawk_sio_lxc_t type defines a structure to store a character
 * with its location information.
 */
struct hawk_sio_lxc_t
{
	hawk_ooci_t        c;    /**< character */
	hawk_oow_t         line; /**< line */
	hawk_oow_t         colm; /**< column */
	const hawk_ooch_t* file; /**< file */
};
typedef struct hawk_sio_lxc_t hawk_sio_lxc_t;

typedef struct hawk_sio_arg_t hawk_sio_arg_t;

/**
 * The hawk_sio_arg_t type defines a structure to describe the source
 * stream.
 */
struct hawk_sio_arg_t 
{
	/** 
	 * [IN] name of I/O object. 
	 * It is #HAWK_NULL for the top-level stream. It points to a stream name
	 * for an included stream.
	 */
	const hawk_ooch_t* name;   

	/** 
	 * [OUT] I/O handle set by a handler. 
	 * The source stream handler can set this field when it opens a stream.
	 * All subsequent operations on the stream see this field as set
	 * during opening.
	 */
	void* handle;

	/**
	 * [OUT] path name resolved of the name above. the handler must set this
	 *       to a proper path if the name is not #HAWK_NULL.
	 */
	hawk_ooch_t* path;

	/**
	 * [OUT] unique id set by an input handler. it is used for a single time inclusion check.
	 */
	hawk_uint8_t unique_id[HAWK_SIZEOF_INTPTR_T * 2];

	/**
	 * [IN] points to the includer. #HAWK_NULL for the toplevel.
	 * 
	 */
	hawk_sio_arg_t* prev;

	/*-- from here down, internal use only --*/
	struct
	{
		hawk_ooch_t buf[1024];
		hawk_oow_t pos;
		hawk_oow_t len;
	} b;

	hawk_oow_t line;
	hawk_oow_t colm;

	hawk_sio_lxc_t last;
	int pragma_trait;
};

/**
 * The hawk_sio_impl_t type defines a source IO function
 */
typedef hawk_ooi_t (*hawk_sio_impl_t) (
	hawk_t*         hawk,
	hawk_sio_cmd_t  cmd, 
	hawk_sio_arg_t* arg,
	hawk_ooch_t*    data,
	hawk_oow_t      count
);

/**
 * The hawk_rio_cmd_t type defines runtime I/O request types.
 */
enum hawk_rio_cmd_t
{
	HAWK_RIO_CMD_OPEN        = 0, /**< open a stream */
	HAWK_RIO_CMD_CLOSE       = 1, /**< close a stream */
	HAWK_RIO_CMD_READ        = 2, /**< read a stream */
	HAWK_RIO_CMD_WRITE       = 3, /**< write to a stream */
	HAWK_RIO_CMD_READ_BYTES  = 4,
	HAWK_RIO_CMD_WRITE_BYTES = 5,
	HAWK_RIO_CMD_FLUSH       = 6, /**< flush buffered data to a stream */
	HAWK_RIO_CMD_NEXT        = 7  /**< close the current stream and 
	                                open the next stream (only for console) */
};
typedef enum hawk_rio_cmd_t hawk_rio_cmd_t;

/**
 * The hawk_rio_mode_t type defines the I/O modes used by I/O handlers.
 * Each I/O handler should inspect the requested mode and open an I/O
 * stream accordingly for subsequent operations.
 */
enum hawk_rio_mode_t
{
	HAWK_RIO_PIPE_READ      = 0, /**< open a pipe for read */
	HAWK_RIO_PIPE_WRITE     = 1, /**< open a pipe for write */
	HAWK_RIO_PIPE_RW        = 2, /**< open a pipe for read and write */

	HAWK_RIO_FILE_READ      = 0, /**< open a file for read */
	HAWK_RIO_FILE_WRITE     = 1, /**< open a file for write */
	HAWK_RIO_FILE_APPEND    = 2, /**< open a file for append */

	HAWK_RIO_CONSOLE_READ   = 0, /**< open a console for read */
	HAWK_RIO_CONSOLE_WRITE  = 1  /**< open a console for write */
};
typedef enum hawk_rio_mode_t hawk_rio_mode_t;

/*
 * The hawk_rio_rwcmode_t type defines I/O closing modes, especially for 
 * a two-way pipe.
 */
enum hawk_rio_rwcmode_t
{
	HAWK_RIO_CMD_CLOSE_FULL  = 0, /**< close both read and write end */
	HAWK_RIO_CMD_CLOSE_READ  = 1, /**< close the read end */
	HAWK_RIO_CMD_CLOSE_WRITE = 2  /**< close the write end */
};
typedef enum hawk_rio_rwcmode_t hawk_rio_rwcmode_t;

/**
 * The hawk_rio_arg_t defines the data structure passed to a runtime 
 * I/O handler. An I/O handler should inspect the \a mode field and the 
 * \a name field and store an open handle to the \a handle field when 
 * #HAWK_RIO_CMD_OPEN is requested. For other request type, it can refer
 * to the \a handle field set previously.
 */
struct hawk_rio_arg_t 
{
	/* read-only. a user handler shouldn't change any of these fields */
	hawk_rio_mode_t    mode;      /**< opening mode */
	hawk_ooch_t*       name;      /**< name of I/O object */
	hawk_rio_rwcmode_t rwcmode;   /**< closing mode for rwpipe */

	/* read-write. a user handler can do whatever it likes to do with these. */
	void*              handle;    /**< I/O handle set by a handler */
	int                uflags;    /**< user-flags set by a handler */

	/*--  from here down, internal use only --*/
	int type; 
	int rwcstate;   /* closing state for rwpipe */

	struct
	{
		struct
		{
			hawk_ooch_t buf[2048];
			hawk_bch_t bbuf[2048];
		} u;
		hawk_oow_t pos;
		hawk_oow_t len;
		int        eof;
		int        eos;
		int        mbs;
	} in;

	struct
	{
		int eof;
		int eos;
	} out;

	struct hawk_rio_arg_t* next;
};
typedef struct hawk_rio_arg_t hawk_rio_arg_t;

/**
 * The hawk_rio_impl_t type defines a runtime I/O handler.
 */
typedef hawk_ooi_t (*hawk_rio_impl_t) (
	hawk_rtx_t*      rtx,
	hawk_rio_cmd_t   cmd,
	hawk_rio_arg_t*  arg,
	void*            data,
	hawk_oow_t       count
);

/**
 * The hawk_prm_t type defines primitive functions required to perform
 * a set of primitive operations.
 */
struct hawk_prm_t
{
	struct
	{
		hawk_math2_t pow; /**< floating-point power function */
		hawk_math2_t mod; /**< floating-point remainder function */
	} math;

	hawk_mod_open_t modopen;
	hawk_mod_close_t modclose;
	hawk_mod_getsym_t modgetsym;

	hawk_log_write_t logwrite;
#if 0
	struct 
	{
		/* TODO: accept regular expression handling functions */
		hawk_buildrex_t build;
		hawk_matchrex_t match;
		hawk_freerex_t free;
		hawk_isemptyrex_t isempty;
	} rex;
#endif
};
typedef struct hawk_prm_t hawk_prm_t;

/* ------------------------------------------------------------------------ */

/**
 * The hawk_sio_cbs_t type defines a script stream handler set.
 * The hawk_parse() function calls the input and output handler to parse
 * a script and optionally deparse it. Typical input and output handlers 
 * are shown below:
 *
 * \code
 * hawk_ooi_t in (
 *    hawk_t* hawk, hawk_sio_cmd_t cmd,
 *    hawk_sio_arg_t* arg,
 *    hawk_ooch_t* buf, hawk_oow_t size)
 * {
 *    if (cmd == HAWK_SIO_CMD_OPEN) open input stream of arg->name;
 *    else if (cmd == HAWK_SIO_CMD_CLOSE) close input stream;
 *    else read input stream and fill buf up to size characters;
 * }
 *
 * hawk_ooi_t out (
 *    hawk_t* hawk, hawk_sio_cmd_t cmd,
 *    hawk_sio_arg_t* arg,
 *    hawk_ooch_t* data, hawk_oow_t size)
 * {
 *    if (cmd == HAWK_SIO_CMD_OPEN) open_output_stream of arg->name;
 *    else if (cmd == HAWK_SIO_CMD_CLOSE) close_output_stream;
 *    else write data of size characters to output stream;
 * }
 * \endcode
 *
 * For #HAWK_SIO_CMD_OPEN, a handler must return:
 * - -1 if it failed to open a stream.
 * - 0 if it has opened a stream but has reached the end.
 * - 1 if it has successfully opened a stream.
 * 
 * For #HAWK_SIO_CMD_CLOSE, a handler must return:
 * - -1 if it failed to close a stream.
 * - 0 if it has closed a stream.
 *
 * For #HAWK_SIO_CMD_READ and #HAWK_SIO_CMD_WRITE, a handler must return:
 * - -1 if there was an error occurred during operation.
 * - 0 if it has reached the end.
 * - the number of characters read or written on success.
 */
struct hawk_sio_cbs_t
{
	hawk_sio_impl_t in;  /**< input script stream handler */
	hawk_sio_impl_t out; /**< output script stream handler */
};
typedef struct hawk_sio_cbs_t hawk_sio_cbs_t;

/* ------------------------------------------------------------------------ */

/**
 * The hawk_rio_t type defines a runtime I/O handler set.
 * \sa hawk_rtx_t
 */
struct hawk_rio_cbs_t
{
	hawk_rio_impl_t pipe;    /**< pipe handler */
	hawk_rio_impl_t file;    /**< file handler */
	hawk_rio_impl_t console; /**< console handler */
};
typedef struct hawk_rio_cbs_t hawk_rio_cbs_t;

/* ------------------------------------------------------------------------ */

typedef struct hawk_fnc_t      hawk_fnc_t;
typedef struct hawk_fnc_info_t hawk_fnc_info_t;

/**
 * The hawk_fnc_impl_t type defines a intrinsic function handler.
 */
typedef int (*hawk_fnc_impl_t) (
	hawk_rtx_t*            rtx,  /**< runtime context */
	const hawk_fnc_info_t* fi    /**< function information */
);

/**
 * The hawk_fnc_marg_t type defines a structure to describe arguments
 * to an implicit function.
 */
struct hawk_fnc_marg_t
{
	/** minimum numbers of argument for a function */
	hawk_oow_t min; 

	/** maximum numbers of argument for a function */
	hawk_oow_t max; 

	/** 
	 * if min is greater than max, spec points to an external module
	 * name where the function is found. otherwise, spec can be #HAWK_NULL
	 * to indicate all arguments are passed by value or point to a
	 * argument specification string composed of 'max' characters.
	 * Each character can be one of:
	 *  - v: value
	 *  - r: reference
	 *  - x: regular expression
	 */
	const hawk_bch_t* spec;
};
typedef struct hawk_fnc_marg_t hawk_fnc_marg_t;

/**
 * The hawk_fnc_warg_t type defines a structure to describe arguments
 * to an implicit function.
 */
struct hawk_fnc_warg_t
{
	hawk_oow_t min; 
	hawk_oow_t max; 
	const hawk_uch_t* spec;
};
typedef struct hawk_fnc_warg_t hawk_fnc_warg_t;

/**
 * The hawk_fnc_mspec_t type defines a structure to hold the specification
 * of an intrinsic function or a module function.
 */ 
struct hawk_fnc_mspec_t
{
	/** argument descriptor */
	hawk_fnc_marg_t arg;

	/** pointer to the function implementing this function */
	hawk_fnc_impl_t impl;

	/** 
	 * when this field is set to a non-zero value bitwise-ORed of 
	 * #hawk_trait_t enumerators, the function is available if 
	 * this field bitwise-ANDed with the global trait option produces
	 * this field itself.
	 * 
	 * this field doesn't take effect for a module function.
	 */
	int trait; 
};
typedef struct hawk_fnc_mspec_t hawk_fnc_mspec_t;

/**
 * The hawk_fnc_wspec_t type defines a structure to hold the specification
 * of an intrinsic function or a module function.
 */ 
struct hawk_fnc_wspec_t
{
	/** argument descriptor */
	hawk_fnc_warg_t arg;

	/** pointer to the function implementing this function */
	hawk_fnc_impl_t impl;

	/** 
	 * when this field is set to a non-zero value bitwise-ORed of 
	 * #hawk_trait_t enumerators, the function is available if 
	 * this field bitwise-ANDed with the global trait option produces
	 * this field itself.
	 * 
	 * this field doesn't take effect for a module function.
	 */
	int trait; 
};
typedef struct hawk_fnc_wspec_t hawk_fnc_wspec_t;

#if defined(HAWK_OOCH_IS_BCH)
typedef hawk_fnc_marg_t hawk_fnc_arg_t;
typedef hawk_fnc_mspec_t hawk_fnc_spec_t;
#else
typedef hawk_fnc_warg_t hawk_fnc_arg_t;
typedef hawk_fnc_wspec_t hawk_fnc_spec_t;
#endif

/* ------------------------------------------------------------------------ */

typedef struct hawk_mod_t hawk_mod_t;
typedef struct hawk_mod_sym_t hawk_mod_sym_t;

struct hawk_fnc_info_t
{
	hawk_oocs_t name;

	/** #HAWK_NULL if the function is not registered from module */
	hawk_mod_t* mod; 
};


typedef int (*hawk_mod_load_t) (
	hawk_mod_t* mod,
	hawk_t*     hawk
);

typedef int (*hawk_mod_query_t) (
	hawk_mod_t*     mod,
	hawk_t*         hawk,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t* sym
);

typedef void (*hawk_mod_unload_t) (
	hawk_mod_t* mod,
	hawk_t*     hawk
);

typedef int (*hawk_mod_init_t) (
	hawk_mod_t* mod,
	hawk_rtx_t* rtx
);

typedef void (*hawk_mod_fini_t) (
	hawk_mod_t* mod,
	hawk_rtx_t* rtx
);

struct hawk_mod_t
{
	hawk_mod_query_t  query;
	hawk_mod_unload_t unload; /* unload the module */

	hawk_mod_init_t   init; /* per-rtx initialization */
	hawk_mod_fini_t   fini; /* per-rtx finalization */

	void*             ctx;
};

enum hawk_mod_sym_type_t
{
	HAWK_MOD_FNC = 0, 
	HAWK_MOD_INT, /* constant */
	HAWK_MOD_FLT  /* constant */
	/*HAWK_MOD_STR,
	HAWK_MOD_VAR,
	*/
};
typedef enum hawk_mod_sym_type_t hawk_mod_sym_type_t;
typedef hawk_fnc_spec_t hawk_mod_sym_fnc_t;
typedef struct hawk_mod_sym_int_t hawk_mod_sym_int_t;
typedef struct hawk_mod_sym_flt_t hawk_mod_sym_flt_t;

struct hawk_mod_sym_int_t
{
	hawk_int_t val;
	int trait;
};

struct hawk_mod_sym_flt_t
{
	hawk_flt_t val;
	int trait;
};

struct hawk_mod_sym_t
{
	hawk_mod_sym_type_t type; 
	union
	{
		hawk_mod_sym_fnc_t fnc_;
		hawk_mod_sym_int_t int_;
		hawk_mod_sym_flt_t flt_;
	} u;
};

/* ------------------------------------------------------------------------ */

typedef struct hawk_mod_fnc_tab_t hawk_mod_fnc_tab_t;
struct hawk_mod_fnc_tab_t
{
        const hawk_ooch_t* name;
        hawk_mod_sym_fnc_t info;
};

typedef struct hawk_mod_int_tab_t hawk_mod_int_tab_t;
struct  hawk_mod_int_tab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_int_t info;
};

typedef struct hawk_mod_flt_tab_t hawk_mod_flt_tab_t;
struct  hawk_mod_flt_tab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_flt_t info;
};

/* ------------------------------------------------------------------------ */

/**
 * The hawk_ecb_close_t type defines the callback function
 * called when an hawk object is closed. The hawk_close() function
 * calls this callback function after having called hawk_clear()
 * but before actual closing.
 */
typedef void (*hawk_ecb_close_t) (
	hawk_t* hawk  /**< hawk */
);

/**
 * The hawk_ecb_clear_t type defines the callback function
 * called when an hawk object is cleared. The hawk_clear() function
 * calls this calllback function before it performs actual clearing.
 */
typedef void (*hawk_ecb_clear_t) (
	hawk_t* hawk  /**< hawk */
);

/**
 * The hawk_ecb_t type defines an event callback set.
 * You can register a callback function set with
 * hawk_pushecb().  The callback functions in the registered
 * set are called in the reverse order of registration.
 */
typedef struct hawk_ecb_t hawk_ecb_t;
struct hawk_ecb_t
{
	/**
	 * called by hawk_close().
	 */
	hawk_ecb_close_t close;

	/**
	 * called by hawk_clear().
	 */
	hawk_ecb_clear_t clear;

	/* internal use only. don't touch this field */
	hawk_ecb_t* next;
};

/* ------------------------------------------------------------------------ */

/**
 * The hawk_rtx_ecb_close_t type defines the callback function
 * called when the runtime context is closed.
 */
typedef void (*hawk_rtx_ecb_close_t) (
	hawk_rtx_t* rtx  /**< runtime context */
);


/**
 * The hawk_rtx_ecb_stmt_t type defines the callback function for each
 * statement.
 */
typedef void (*hawk_rtx_ecb_stmt_t) (
	hawk_rtx_t* rtx, /**< runtime context */
	hawk_nde_t* nde  /**< node */
);

/**
 * The hawk_rtx_ecb_gblset_t type defines the callback function 
 * executed when a global variable is set with a value. It is not
 * called when a global variable is changed implicitly. For example,
 * it is not called when FNR is updated for each record read.
 */
typedef void (*hawk_rtx_ecb_gblset_t) (
	hawk_rtx_t*     rtx, /**< runtime context */
	hawk_oow_t         idx, /**< global variable index */
	hawk_val_t*     val  /**< value */
);

/**
 * The hawk_rtx_ecb_t type defines an event callback set for a
 * runtime context. You can register a callback function set with
 * hawk_rtx_pushecb().  The callback functions in the registered
 * set are called in the reverse order of registration.
 */
typedef struct hawk_rtx_ecb_t hawk_rtx_ecb_t;
struct hawk_rtx_ecb_t
{
	/**
	 * called by hawk_rtx_close().
	 */
	hawk_rtx_ecb_close_t close;

	/**
	 * called by hawk_rtx_loop() and hawk_rtx_call() for
	 * each statement executed.
	 */
	hawk_rtx_ecb_stmt_t stmt;

	/**
	 * called when a global variable is set with a value.
	 */
	hawk_rtx_ecb_gblset_t gblset;

	/* internal use only. don't touch this field */
	hawk_rtx_ecb_t* next;
};

/* ------------------------------------------------------------------------ */

/**
 * The hawk_opt_t type defines various option types.
 */
enum hawk_opt_t
{
	/** trait option. 0 or bitwise-ORed of ::hawk_trait_t values */
	HAWK_OPT_TRAIT,  

	HAWK_OPT_MODLIBDIRS,
	HAWK_OPT_MODPREFIX,
	HAWK_OPT_MODPOSTFIX,

	HAWK_OPT_INCLUDEDIRS,

	HAWK_OPT_DEPTH_INCLUDE,
	HAWK_OPT_DEPTH_BLOCK_PARSE,
	HAWK_OPT_DEPTH_BLOCK_RUN,
	HAWK_OPT_DEPTH_EXPR_PARSE,
	HAWK_OPT_DEPTH_EXPR_RUN,
	HAWK_OPT_DEPTH_REX_BUILD,
	HAWK_OPT_DEPTH_REX_MATCH,

	HAWK_OPT_RTX_STACK_LIMIT,
	HAWK_OPT_LOG_MASK,
	HAWK_OPT_LOG_MAXCAPA
};
typedef enum hawk_opt_t hawk_opt_t;

#define HAWK_LOG_CAPA_ALIGN (512)
#define HAWK_DFL_LOG_MAXCAPA (HAWK_LOG_CAPA_ALIGN * 16)

/* ------------------------------------------------------------------------ */

/**
 * The hawk_trait_t type defines various options to change the behavior
 * of #hawk_t.
 */
enum hawk_trait_t
{ 
	/** allows undeclared variables */
	HAWK_IMPLICIT = (1 << 0),

	/** allows multiline string literals or regular expression literals */
	HAWK_MULTILINESTR = (1 << 1),

	/** enables nextofile and NEXTOFILE */
	HAWK_NEXTOFILE = (1 << 2),

	/** supports \b getline, \b print, \b printf, \b close, \b fflush,
	 *  piping, and file rediction */
	HAWK_RIO = (1 << 3), 

	/** enables the two-way pipe if #HAWK_RIO is on */
	HAWK_RWPIPE = (1 << 4),

	/** a new line can terminate a statement */
	HAWK_NEWLINE = (1 << 5),

	/** 
	 * removes empty fields when splitting a record if FS is a regular
	 * expression and the match is all spaces.
	 *
	 * \code
	 * BEGIN { FS="[[:space:]]+"; } 
	 * { 
	 *    print "NF=" NF; 
	 *    for (i = 0; i < NF; i++) print i " [" $(i+1) "]";
	 * }
	 * \endcode
	 * " a b c " is split to [a], [b], [c] if #HAWK_STRIPRECSPC is on.
	 * Otherwise, it is split to [], [a], [b], [c], [].
	 *
	 * \code
	 * BEGIN { 
	 *   n=split("   oh my  noodle  ", x, /[ o]+/); 
	 *   for (i=1;i<=n;i++) print "[" x[i] "]"; 
	 * }
	 * \endcode
	 * This example splits the string to [], [h], [my], [n], [dle]
	 * if #HAWK_STRIPRECSPC is on. Otherwise, it results in
	 * [], [h], [my], [n], [dle], []. Note that the first empty field is not 
	 * removed as the field separator is not all spaces. (space + 'o').
	 */
	HAWK_STRIPRECSPC = (1 << 6),

	/** strips off leading spaces when converting a string to a number. */
	HAWK_STRIPSTRSPC = (1 << 7),

	/** enable implicit concatenation. 
	 *  if this is off, you need %% for concatenation.  */
	HAWK_BLANKCONCAT = (1 << 8),

	/** CR + LF by default */
	HAWK_CRLF = (1 << 10),

	/** treats a map value more flexibly. a function can return
	 *  a map. you can override a map with a scalar value without 
	 *  'delete' or '\@reset'. 
	 */
	HAWK_FLEXMAP = (1 << 11),

	/** allows \b BEGIN, \b END, pattern-action blocks */
	HAWK_PABLOCK = (1 << 12),

	/** allows {n,m} in a regular expression. */
	HAWK_REXBOUND = (1 << 13),

	/** 
	 * performs numeric comparison when a string convertable
	 * to a number is compared with a number or vice versa.
	 *
	 * For an expression (9 > "10.9"),
	 * - 9 is greater if #HAWK_NCMPONSTR is off;
	 * - "10.9" is greater if #HAWK_NCMPONSTR is on
	 */
	HAWK_NCMPONSTR = (1 << 14),

	/**
	 * enables the strict naming rule.
	 * - a parameter name can not be the same as the owning function name.
	 * - a local variable name can not be the same as the owning 
	 *   function name.
	 */
	HAWK_STRICTNAMING = (1 << 15),

	/**
	 * makes AWK more fault-tolerant.
	 * - prevents termination due to print and printf failure.
	 * - achieves this by handling print and printf as if
	 *   they are functions like getline.
	 * - allows an expression group in a normal context
	 *   without the 'in' operator. the evaluation result
	 *   of the last expression is returned as that of
	 *   the expression group.
	 * - e.g.) a = (1, 3 * 3, 4, 5 + 1);  # a is assigned 6.
	 */
	HAWK_TOLERANT = (1 << 17),

	/*
	 * detect a numeric string and convert to a numeric type
	 * automatically
	 */
	HAWK_NUMSTRDETECT = (1 << 18),

	/** 
	 * makes #hawk_t to behave compatibly with classical AWK
	 * implementations
	 */
	HAWK_CLASSIC = 
		HAWK_IMPLICIT | HAWK_RIO | 
		HAWK_NEWLINE | HAWK_BLANKCONCAT | HAWK_PABLOCK | 
		HAWK_STRIPSTRSPC | HAWK_STRICTNAMING | HAWK_NUMSTRDETECT,

	HAWK_MODERN =
		HAWK_CLASSIC | HAWK_FLEXMAP | HAWK_REXBOUND |
		HAWK_RWPIPE | HAWK_TOLERANT | HAWK_NEXTOFILE  | HAWK_NUMSTRDETECT /*| HAWK_NCMPONSTR*/
};
typedef enum hawk_trait_t hawk_trait_t;

/* ------------------------------------------------------------------------ */

/**
 * The hawk_errstr_t type defines an error string getter. It should return 
 * an error formatting string for an error number requested. A new string
 * should contain the same number of positional parameters (${X}) as in the
 * default error formatting string. You can set a new getter into an hawk
 * object with the hawk_seterrstr() function to customize an error string.
 */
typedef const hawk_ooch_t* (*hawk_errstr_t) (
	hawk_errnum_t num    /**< error number */
);

/**
 * The hawk_gbl_id_t type defines intrinsic globals variable IDs.
 */
enum hawk_gbl_id_t
{
	/* this table should match gtab in parse.c.
	 *
	 * in addition, hawk_rtx_setgbl also counts 
	 * on the order of these values.
	 * 
	 * note that set_global() in run.c contains code 
	 * preventing these global variables from being assigned
	 * with a map value. if you happen to add one that can 
	 * be a map, don't forget to change code in set_global().
	 * but is this check really necessary???
	 */

	HAWK_GBL_CONVFMT,
	HAWK_GBL_FILENAME,
	HAWK_GBL_FNR,
	HAWK_GBL_FS,
	HAWK_GBL_IGNORECASE,
	HAWK_GBL_NF,
	HAWK_GBL_NR,
	HAWK_GBL_NUMSTRDETECT,
	HAWK_GBL_OFILENAME,
	HAWK_GBL_OFMT,
	HAWK_GBL_OFS,
	HAWK_GBL_ORS,
	HAWK_GBL_RLENGTH,
	HAWK_GBL_RS,
	HAWK_GBL_RSTART,
	HAWK_GBL_SCRIPTNAME,
	HAWK_GBL_STRIPRECSPC,
	HAWK_GBL_STRIPSTRSPC,
	HAWK_GBL_SUBSEP,

	/* these are not not the actual IDs and are used internally only 
	 * Make sure you update these values properly if you add more 
	 * ID definitions, however */
	HAWK_MIN_GBL_ID = HAWK_GBL_CONVFMT,
	HAWK_MAX_GBL_ID = HAWK_GBL_SUBSEP
};
typedef enum hawk_gbl_id_t hawk_gbl_id_t;

/**
 * The hawk_val_type_t type defines types of AWK values. Each value 
 * allocated is tagged with a value type in the \a type field.
 * \sa hawk_val_t HAWK_VAL_HDR
 */
enum hawk_val_type_t
{
	/* - the enumerators between HAWK_VAL_NIL and HAWK_VAL_ARR inclusive
	 *   must be synchronized with an internal table of the __cmp_val 
	 *   function in run.c.
	 * - all enumerators must be in sync with __val_type_name in val.c */
	HAWK_VAL_NIL     = 0, /**< nil */
	HAWK_VAL_CHAR    = 1, /**< character */
	HAWK_VAL_BCHR    = 2, /**< byte character */
	HAWK_VAL_INT     = 3, /**< integer */
	HAWK_VAL_FLT     = 4, /**< floating-pointer number */
	HAWK_VAL_STR     = 5, /**< string */
	HAWK_VAL_MBS     = 6, /**< byte array */
	HAWK_VAL_FUN     = 7, /**< function pointer */
	HAWK_VAL_MAP     = 8, /**< map */
	HAWK_VAL_ARR     = 9, /**< array */

	HAWK_VAL_REX     = 10, /**< regular expression */
	HAWK_VAL_REF     = 11 /**< reference to other types */
};
typedef enum hawk_val_type_t hawk_val_type_t;

/**
 * The values defined are used to set the type field of the 
 * #hawk_rtx_valtostr_out_t structure. The field should be one of the 
 * following values:
 *
 * - #HAWK_RTX_VALTOSTR_CPL
 * - #HAWK_RTX_VALTOSTR_CPLCPY
 * - #HAWK_RTX_VALTOSTR_CPLDUP
 * - #HAWK_RTX_VALTOSTR_STRP
 * - #HAWK_RTX_VALTOSTR_STRPCAT
 *
 * and it can optionally be ORed with #HAWK_RTX_VALTOSTR_PRINT.
 */
enum hawk_rtx_valtostr_type_t
{ 
	/** use u.cpl of #hawk_rtx_valtostr_out_t */
	HAWK_RTX_VALTOSTR_CPL       = 0x00, 
	/** use u.cplcpy of #hawk_rtx_valtostr_out_t */
	HAWK_RTX_VALTOSTR_CPLCPY    = 0x01, 
	/** use u.cpldup of #hawk_rtx_valtostr_out_t */
	HAWK_RTX_VALTOSTR_CPLDUP    = 0x02,
	/** use u.strp of #hawk_rtx_valtostr_out_t */
	HAWK_RTX_VALTOSTR_STRP      = 0x03,
	/** use u.strpcat of #hawk_rtx_valtostr_out_t */
	HAWK_RTX_VALTOSTR_STRPCAT   = 0x04,
	/** convert for print */
	HAWK_RTX_VALTOSTR_PRINT     = 0x10   
};

/**
 * The hawk_rtx_valtostr() function converts a value to a string as 
 * indicated in a parameter of the hawk_rtx_valtostr_out_t type.
 */
struct hawk_rtx_valtostr_out_t
{
	int type; /**< enum #hawk_rtx_valtostr_type_t */

	union
	{
		hawk_oocs_t  cpl;
		hawk_oocs_t  cplcpy;
		hawk_oocs_t  cpldup;  /* need to free cpldup.ptr */
		hawk_ooecs_t*  strp;
		hawk_ooecs_t*  strpcat;
	} u;
};
typedef struct hawk_rtx_valtostr_out_t hawk_rtx_valtostr_out_t;

/* record filter using NR */
struct hawk_nrflt_t
{
	hawk_int_t limit;
	hawk_int_t size;
	hawk_int_t rank;
};
typedef struct hawk_nrflt_t hawk_nrflt_t;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_open() function creates a new #hawk_t object. The object 
 * created can be passed to other hawk_xxx() functions and is valid until 
 * it is destroyed with the hawk_close() function. The function saves the 
 * memory manager pointer while it copies the contents of the primitive 
 * function structures. Therefore, you should keep the memory manager valid 
 * during the whole life cycle of an hawk_t object.
 *
 * \code
 * hawk_t* dummy()
 * {
 *     hawk_mmgr_t mmgr;
 *     hawk_prm_t prm;
 *     return hawk_open (
 *        &mmgr, // NOT OK because the contents of mmgr is 
 *               // invalidated when dummy() returns. 
 *        0, 
 *        &prm,  // OK 
 *        HAWK_NULL
 *     );
 * }
 * \endcode
 *
 * Upon failure, it stores the error number to a variable pointed to 
 * by \a errnum. if \a errnum is #HAWK_NULL, no error number is stored.
 *
 * \return a pointer to a hawk_t object on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_t* hawk_open ( 
	hawk_mmgr_t*      mmgr,    /**< memory manager */
	hawk_oow_t        xtnsize, /**< extension size in bytes */
	hawk_cmgr_t*      cmgr,    /**< character conversion manager */
	const hawk_prm_t* prm,     /**< pointer to a primitive function structure */
	hawk_errnum_t*    errnum   /**< pointer to an error number varaible */
);

/**
 *  The hawk_close() function destroys a hawk_t object.
 */
HAWK_EXPORT void hawk_close (
	hawk_t* hawk /**< hawk */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_getxtn (hawk_t* hawk) { return (void*)((hawk_uint8_t*)hawk + ((hawk_alt_t*)hawk)->_instsize); }
static HAWK_INLINE hawk_gem_t* hawk_getgem (hawk_t* hawk) { return &((hawk_alt_t*)hawk)->_gem; }
static HAWK_INLINE hawk_mmgr_t* hawk_getmmgr (hawk_t* hawk) { return ((hawk_alt_t*)hawk)->_gem.mmgr; }
static HAWK_INLINE hawk_cmgr_t* hawk_getcmgr (hawk_t* hawk) { return ((hawk_alt_t*)hawk)->_gem.cmgr; }
static HAWK_INLINE void hawk_setcmgr (hawk_t* hawk, hawk_cmgr_t* cmgr) { ((hawk_alt_t*)hawk)->_gem.cmgr = cmgr; }
#else
#define hawk_getxtn(hawk) ((void*)((hawk_uint8_t*)hawk + ((hawk_alt_t*)hawk)->_instsize))
#define hawk_getgem(hawk) (&((hawk_alt_t*)(hawk))->_gem)
#define hawk_getmmgr(hawk) (((hawk_alt_t*)(hawk))->_gem.mmgr)
#define hawk_getcmgr(hawk) (((hawk_alt_t*)(hawk))->_gem.cmgr)
#define hawk_setcmgr(hawk,_cmgr) (((hawk_alt_t*)(hawk))->_gem.cmgr = (_cmgr))
#endif /* HAWK_HAVE_INLINE */

/**
 * The hawk_getprm() function retrieves primitive functions
 * associated. Actual function pointers are copied into a 
 * structure specified by \a prm.
 */
HAWK_EXPORT void hawk_getprm (
	hawk_t*     hawk,
	hawk_prm_t* prm
);

/**
 * The hawk_setprm() function changes existing primitive
 * functions. 
 */
HAWK_EXPORT void hawk_setprm (
	hawk_t*           hawk,
	const hawk_prm_t* prm
);

/**
 * The hawk_clear() clears the internal state of \a hawk. If you want to
 * reuse a hawk_t instance that finished being used, you may call 
 * hawk_clear() instead of destroying and creating a new
 * #hawk_t instance using hawk_close() and hawk_open().

 */
HAWK_EXPORT void hawk_clear (
	hawk_t* hawk 
);

/**
 * The hawk_geterrstr() gets an error string getter.
 */
HAWK_EXPORT hawk_errstr_t hawk_geterrstr (
	hawk_t* hawk    /**< hawk */
);

/**
 * The hawk_geterrnum() function returns the number of the last error 
 * occurred.
 * \return error number
 */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_errnum_t hawk_geterrnum (hawk_t* hawk) { return ((hawk_alt_t*)hawk)->_gem.errnum; }
#else
#	define hawk_geterrnum(hawk) (((hawk_alt_t*)(hawk))->_gem.errnum)
#endif

/**
 * The hawk_geterrloc() function returns the location where the
 * last error has occurred.
 */
HAWK_EXPORT const hawk_loc_t* hawk_geterrloc (
	hawk_t* hawk /**< hawk */
);

/**
 * The hawk_geterrbmsg() function returns the error message describing
 * the last error occurred. 
 *
 * \return error message
 */
HAWK_EXPORT const hawk_bch_t* hawk_geterrbmsg (
	hawk_t* hawk /**< hawk */
);

/**
 * The hawk_geterrumsg() function returns the error message describing
 * the last error occurred. 
 *
 * \return error message
 */
HAWK_EXPORT const hawk_uch_t* hawk_geterrumsg (
	hawk_t* hawk /**< hawk */
);


#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_geterrmsg hawk_geterrbmsg
#else
#	define hawk_geterrmsg hawk_geterrumsg
#endif


HAWK_EXPORT const hawk_ooch_t* hawk_backuperrmsg (
	hawk_t* hawk /**< hawk */
);


/**
 * The hawk_geterrinf() function copies error information into memory
 * pointed to by \a errinf from \a hawk.
 */
HAWK_EXPORT void hawk_geterrinf (
	hawk_t*        hawk,   /**< hawk */
	hawk_errinf_t* errinf /**< error information buffer */
);

/**
 * The hawk_seterrnum() function sets the error information omitting 
 * error location. You must pass a non-NULL for \a errarg if the specified
 * error number \a errnum requires one or more arguments to format an
 * error message.
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_seterrnum (hawk_t* hawk, const hawk_loc_t* errloc, hawk_errnum_t errnum) { hawk_gem_seterrnum (hawk_getgem(hawk), errloc, errnum); }
#else
#define hawk_seterrnum(hawk, errloc, errnum) hawk_gem_seterrnum(hawk_getgem(hawk), errloc, errnum)
#endif

HAWK_EXPORT void hawk_seterrbfmt (
	hawk_t*             hawk,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_bch_t*   errfmt,
	...
);

HAWK_EXPORT void hawk_seterrufmt (
	hawk_t*             hawk,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_uch_t*   errfmt,
	...
);

HAWK_EXPORT void hawk_seterrbvfmt (
	hawk_t*             hawk,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_bch_t*   errfmt,
	va_list             ap
);

HAWK_EXPORT void hawk_seterruvfmt (
	hawk_t*             hawk,
	const hawk_loc_t*   errloc,
	hawk_errnum_t       errnum,
	const hawk_uch_t*   errfmt,
	va_list             ap
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_seterrfmt hawk_seterrufmt
#	define hawk_seterrvfmt hawk_seterruvfmt
#else
#	define hawk_seterrfmt hawk_seterrbfmt
#	define hawk_seterrvfmt hawk_seterrbvfmt
#endif

/**
 * The hawk_seterrinf() function sets the error information. This function
 * may be useful if you want to set a custom error message rather than letting
 * it automatically formatted.
 */
HAWK_EXPORT void hawk_seterrinf (
	hawk_t*              hawk,   /**< hawk */
	const hawk_errinf_t* errinf /**< error information */
);


/**
 * The hawk_geterror() function gets error information via parameters.
 */
HAWK_EXPORT void hawk_geterror (
	hawk_t*             hawk,    /**< hawk */
	hawk_errnum_t*      errnum, /**< error number */
	const hawk_ooch_t** errmsg, /**< error message */
	hawk_loc_t*         errloc  /**< error location */
);

/**
 * The hawk_getopt() function gets the value of an option
 * specified by \a id into the buffer pointed to by \a value.
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_getopt (
	hawk_t*      hawk,
	hawk_opt_t   id,
	void*        value
);

/**
 * The hawk_setopt() function sets the value of an option 
 * specified by \a id to the value pointed to by \a value.
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_setopt (
	hawk_t*       hawk,
	hawk_opt_t    id,
	const void*   value
);

/**
 * The hawk_popecb() function pops an hawk event callback set
 * and returns the pointer to it. If no callback set can be popped,
 * it returns #HAWK_NULL.
 */
HAWK_EXPORT hawk_ecb_t* hawk_popecb (
	hawk_t* hawk /**< hawk */
);

/**
 * The hawk_pushecb() function register a runtime callback set.
 */
HAWK_EXPORT void hawk_pushecb (
	hawk_t*     hawk, /**< hawk */
	hawk_ecb_t* ecb  /**< callback set */
);

/**
 * The hawk_addgblwithbcstr() function adds an intrinsic global variable.
 * \return the ID of the global variable added on success, -1 on failure.
 */
HAWK_EXPORT int hawk_addgblwithbcstr (
	hawk_t*           hawk,  /**< hawk */
	const hawk_bch_t* name   /**< variable name */
);

/**
 * The hawk_addgblwithucstr() function adds an intrinsic global variable.
 * \return the ID of the global variable added on success, -1 on failure.
 */
HAWK_EXPORT int hawk_addgblwithucstr (
	hawk_t*           hawk,  /**< hawk */
	const hawk_uch_t* name   /**< variable name */
);

/**
 * The hawk_delgblwithbcstr() function deletes an intrinsic global variable by name.
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_delgblwithbcstr (
	hawk_t*           hawk, /**< hawk */
	const hawk_bch_t* name  /**< variable name */
);

/**
 * The hawk_delgblwithucstr() function deletes an intrinsic global variable by name.
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_delgblwithucstr (
	hawk_t*           hawk, /**< hawk */
	const hawk_uch_t* name  /**< variable name */
);

/**
 * The hawk_findgblwithbcstr() function returns the numeric ID of a global variable.
 * \return number >= 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_findgblwithbcstr (
	hawk_t*           hawk, /**< hawk */
	const hawk_bch_t* name,  /**< variable name */
	int               inc_builtins /**< include builtin global variables like FS */
);

/**
 * The hawk_findgblwithucstr() function returns the numeric ID of a global variable.
 * \return number >= 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_findgblwithucstr (
	hawk_t*           hawk, /**< hawk */
	const hawk_uch_t* name,  /**< variable name */
	int               inc_builtins /**< include builtin globals variables like FS */
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_addgblwithoocstr hawk_addgblwithbcstr
#	define hawk_delgblwithoocstr hawk_delgblwithbcstr
#	define hawk_findgblwithoocstr hawk_findgblwithbcstr
#else
#	define hawk_addgblwithoocstr hawk_addgblwithucstr
#	define hawk_delgblwithoocstr hawk_delgblwithucstr
#	define hawk_findgblwithoocstr hawk_findgblwithucstr
#endif

/**
 * The hawk_addfncwithbcstr() function adds an intrinsic function.
 */
HAWK_EXPORT hawk_fnc_t* hawk_addfncwithbcstr (
	hawk_t*                 hawk,
	const hawk_bch_t*         name,
	const hawk_fnc_mspec_t* spec
);

/**
 * The hawk_addfncwithucstr() function adds an intrinsic function.
 */
HAWK_EXPORT hawk_fnc_t* hawk_addfncwithucstr (
	hawk_t*                 hawk,
	const hawk_uch_t*         name,
	const hawk_fnc_wspec_t* spec
);

/**
 * The hawk_delfncwithbcstr() function deletes an intrinsic function by name.
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_delfncwithbcstr (
	hawk_t*         hawk,  /**< hawk */
	const hawk_bch_t* name  /**< function name */
);

/**
 * The hawk_delfncwithucstr() function deletes an intrinsic function by name.
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_delfncwithucstr (
	hawk_t*         hawk,  /**< hawk */
	const hawk_uch_t* name  /**< function name */
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_addfnc hawk_addfncwithbcstr
#	define hawk_delfnc hawk_delfncwithbcstr
#else
#	define hawk_addfnc hawk_addfncwithucstr
#	define hawk_delfnc hawk_delfncwithucstr
#endif

/**
 * The hawk_clrfnc() function deletes all intrinsic functions
 */
HAWK_EXPORT void hawk_clrfnc (
	hawk_t* hawk /**< hawk */
);

/**
 * The hawk_parse() function parses a source script, and optionally 
 * deparses it back. 
 *
 * It reads a source script by calling \a sio->in as shown in the pseudo code 
 * below:
 *
 * \code
 * n = sio->in(hawk, HAWK_SIO_CMD_OPEN);
 * if (n >= 0)
 * {
 *    while (n > 0)
 *       n = sio->in (hawk, HAWK_SIO_CMD_READ, buf, buf_size);
 *    sio->in (hawk, HAWK_SIO_CMD_CLOSE);
 * }
 * \endcode
 *
 * A negative number returned causes hawk_parse() to return failure;
 * 0 returned indicates the end of a stream; A positive number returned 
 * indicates successful opening of a stream or the length of the text read.
 *
 * If \a sio->out is not #HAWK_NULL, it deparses the internal parse tree
 * composed of a source script and writes back the deparsing result by 
 * calling \a sio->out as shown below:
 *
 * \code
 * n = sio->out (hawk, HAWK_SIO_CMD_OPEN);
 * if (n >= 0)
 * {
 *    while (n > 0)
 *       n = sio->out (hawk, HAWK_SIO_CMD_WRITE, text, text_size);
 *    sio->out (hawk, HAWK_SIO_CMD_CLOSE);
 * }
 * \endcode
 * 
 * Unlike \a sf->in, the return value of 0 from \a sf->out is treated as
 * premature end of a stream; therefore, it causes hawk_parse() to return
 * failure.
 *
 * \return 0 on success, -1 on failure.
 */
HAWK_EXPORT int hawk_parse (
	hawk_t*         hawk, /**< hawk */
	hawk_sio_cbs_t* sio  /**< source script I/O handler */
);


HAWK_EXPORT int hawk_isvalidident (
	hawk_t*            hawk,
	const hawk_ooch_t* str
);

/* ----------------------------------------------------------------------- */

HAWK_EXPORT int hawk_findmodsymfnc_noseterr (
	hawk_t*             hawk,
	hawk_mod_fnc_tab_t* fnctab,
	hawk_oow_t          count,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t*     sym
);

HAWK_EXPORT int hawk_findmodsymint_noseterr (
	hawk_t*             hawk,
	hawk_mod_int_tab_t* inttab,
	hawk_oow_t          count,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t*     sym
);

HAWK_EXPORT int hawk_findmodsymflt_noseterr (
	hawk_t*             hawk,
	hawk_mod_flt_tab_t* flttab,
	hawk_oow_t          count,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t*     sym
);

HAWK_EXPORT int hawk_findmodsymfnc (
	hawk_t*             hawk,
	hawk_mod_fnc_tab_t* fnctab,
	hawk_oow_t          count,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t*     sym
);

HAWK_EXPORT int hawk_findmodsymint (
	hawk_t*             hawk,
	hawk_mod_int_tab_t* inttab,
	hawk_oow_t          count,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t*     sym
);

HAWK_EXPORT int hawk_findmodsymflt (
	hawk_t*             hawk,
	hawk_mod_flt_tab_t* flttab,
	hawk_oow_t          count,
	const hawk_ooch_t*  name,
	hawk_mod_sym_t*     sym
);

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_allocmem (hawk_t* hawk, hawk_oow_t size) { return hawk_gem_allocmem(hawk_getgem(hawk), size); }
static HAWK_INLINE void* hawk_reallocmem (hawk_t* hawk, void* ptr, hawk_oow_t size) { return hawk_gem_reallocmem(hawk_getgem(hawk), ptr, size); }
static HAWK_INLINE void* hawk_callocmem (hawk_t* hawk, hawk_oow_t size) { return hawk_gem_callocmem(hawk_getgem(hawk), size); }
static HAWK_INLINE void hawk_freemem (hawk_t* hawk, void* ptr) { hawk_gem_freemem (hawk_getgem(hawk), ptr); }
#else
#define hawk_allocmem(hawk, size) hawk_gem_allocmem(hawk_getgem(hawk), size)
#define hawk_reallocmem(hawk, ptr, size) hawk_gem_reallocmem(hawk_getgem(hawk), ptr, size)
#define hawk_callocmem(hawk, size) hawk_gem_callocmem(hawk_getgem(hawk), size)
#define hawk_freemem(hawk, ptr) hawk_gem_freemem(hawk_getgem(hawk), ptr)
#endif

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_uch_t* hawk_dupucstr (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t* ucslen) { return hawk_gem_dupucstr(hawk_getgem(hawk), ucs, ucslen); }
static HAWK_INLINE hawk_bch_t* hawk_dupbcstr (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t* bcslen) { return hawk_gem_dupbcstr(hawk_getgem(hawk), bcs, bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_dupuchars (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t ucslen) { return hawk_gem_dupuchars(hawk_getgem(hawk), ucs, ucslen); }
static HAWK_INLINE hawk_bch_t* hawk_dupbchars (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t bcslen) { return hawk_gem_dupbchars(hawk_getgem(hawk), bcs, bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_dupucs (hawk_t* hawk, const hawk_ucs_t* ucs) { return hawk_gem_dupucs(hawk_getgem(hawk), ucs); }
static HAWK_INLINE hawk_bch_t* hawk_dupbcs (hawk_t* hawk, const hawk_bcs_t* bcs) { return hawk_gem_dupbcs(hawk_getgem(hawk), bcs); }
static HAWK_INLINE hawk_uch_t* hawk_dupucstrarr (hawk_t* hawk, const hawk_uch_t* strs[], hawk_oow_t* len) { return hawk_gem_dupucstrarr(hawk_getgem(hawk), strs, len); }
static HAWK_INLINE hawk_bch_t* hawk_dupbcstrarr (hawk_t* hawk, const hawk_bch_t* strs[], hawk_oow_t* len) { return hawk_gem_dupbcstrarr(hawk_getgem(hawk), strs, len); }
#else
#define hawk_dupucstr(hawk, ucs, ucslen) hawk_gem_dupucstr(hawk_getgem(hawk), ucs, ucslen)
#define hawk_dupbcstr(hawk, bcs, bcslen) hawk_gem_dupbcstr(hawk_getgem(hawk), bcs, bcslen)
#define hawk_dupuchars(hawk, ucs, ucslen) hawk_gem_dupuchars(hawk_getgem(hawk), ucs, ucslen)
#define hawk_dupbchars(hawk, bcs, bcslen) hawk_gem_dupbchars(hawk_getgem(hawk), bcs, bcslen)
#define hawk_dupucs(hawk, ucs) hawk_gem_dupucs(hawk_getgem(hawk), ucs)
#define hawk_dupbcs(hawk, bcs) hawk_gem_dupbcs(hawk_getgem(hawk), bcs)
#define hawk_dupucstrarr(hawk, strs, len) hawk_gem_dupucstrarr(hawk_getgem(hawk), strs, len)
#define hawk_dupbcstrarr(hawk, strs, len) hawk_gem_dupbcstrarr(hawk_getgem(hawk), strs, len)
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_dupoocstr     hawk_dupucstr
#	define hawk_dupoochars    hawk_dupuchars
#	define hawk_dupoocs       hawk_dupucs
#	define hawk_dupoocstrarr  hawk_dupucstrarr
#else
#	define hawk_dupoocstr     hawk_dupbcstr
#	define hawk_dupoochars    hawk_dupbchars
#	define hawk_dupoocs       hawk_dupbcs
#	define hawk_dupoocstrarr  hawk_dupbcstrarr
#endif

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_convbtouchars (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all) { return hawk_gem_convbtouchars(hawk_getgem(hawk), bcs, bcslen, ucs, ucslen, all); }
static HAWK_INLINE int hawk_convutobchars (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen) { return hawk_gem_convutobchars(hawk_getgem(hawk), ucs, ucslen, bcs, bcslen); }
static HAWK_INLINE int hawk_convbtoucstr (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all) { return hawk_gem_convbtoucstr(hawk_getgem(hawk), bcs, bcslen, ucs, ucslen, all); }
static HAWK_INLINE int hawk_convutobcstr (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen) { return hawk_gem_convutobcstr(hawk_getgem(hawk), ucs, ucslen, bcs, bcslen); }
#else
#define hawk_convbtouchars(hawk, bcs, bcslen, ucs, ucslen, all) hawk_gem_convbtouchars(hawk_getgem(hawk), bcs, bcslen, ucs, ucslen, all)
#define hawk_convutobchars(hawk, ucs, ucslen, bcs, bcslen) hawk_gem_convutobchars(hawk_getgem(hawk), ucs, ucslen, bcs, bcslen)
#define hawk_convbtoucstr(hawk, bcs, bcslen, ucs, ucslen, all) hawk_gem_convbtoucstr(hawk_getgem(hawk), bcs, bcslen, ucs, ucslen, all)
#define hawk_convutobcstr(hawk, ucs, ucslen, bcs, bcslen) hawk_gem_convutobcstr(hawk_getgem(hawk), ucs, ucslen, bcs, bcslen)
#endif

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_uch_t* hawk_dupbtouchars (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t _bcslen, hawk_oow_t* _ucslen, int all) { return hawk_gem_dupbtouchars(hawk_getgem(hawk), bcs, _bcslen, _ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_duputobchars (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t _ucslen, hawk_oow_t* _bcslen) { return hawk_gem_duputobchars(hawk_getgem(hawk), ucs, _ucslen, _bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_dupb2touchars (hawk_t* hawk, const hawk_bch_t* bcs1, hawk_oow_t bcslen1, const hawk_bch_t* bcs2, hawk_oow_t bcslen2, hawk_oow_t* ucslen, int all) { return hawk_gem_dupb2touchars(hawk_getgem(hawk), bcs1, bcslen1, bcs2, bcslen2, ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_dupu2tobchars (hawk_t* hawk, const hawk_uch_t* ucs1, hawk_oow_t ucslen1, const hawk_uch_t* ucs2, hawk_oow_t ucslen2, hawk_oow_t* bcslen) { return hawk_gem_dupu2tobchars(hawk_getgem(hawk), ucs1, ucslen1, ucs2, ucslen2, bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_dupbtoucstr (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t* _ucslen, int all) { return hawk_gem_dupbtoucstr(hawk_getgem(hawk), bcs, _ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_duputobcstr (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t* _bcslen) { return hawk_gem_duputobcstr(hawk_getgem(hawk), ucs, _bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_dupbtoucharswithcmgr (hawk_t* hawk, const hawk_bch_t* bcs, hawk_oow_t _bcslen, hawk_oow_t* _ucslen, hawk_cmgr_t* cmgr, int all) { return hawk_gem_dupbtoucharswithcmgr(hawk_getgem(hawk), bcs, _bcslen, _ucslen, cmgr, all); }
static HAWK_INLINE hawk_bch_t* hawk_duputobcharswithcmgr (hawk_t* hawk, const hawk_uch_t* ucs, hawk_oow_t _ucslen, hawk_oow_t* _bcslen, hawk_cmgr_t* cmgr) { return hawk_gem_duputobcharswithcmgr(hawk_getgem(hawk), ucs, _ucslen, _bcslen, cmgr); }
static HAWK_INLINE hawk_uch_t* hawk_dupbcstrarrtoucstr (hawk_t* hawk, const hawk_bch_t* bcsarr[], hawk_oow_t* ucslen, int all) { return hawk_gem_dupbcstrarrtoucstr(hawk_getgem(hawk), bcsarr, ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_dupucstrarrtobcstr (hawk_t* hawk, const hawk_uch_t* ucsarr[], hawk_oow_t* bcslen) { return hawk_gem_dupucstrarrtobcstr(hawk_getgem(hawk), ucsarr, bcslen); }
#else
#define hawk_dupbtouchars(hawk, bcs, _bcslen, _ucslen, all) hawk_gem_dupbtouchars(hawk_getgem(hawk), bcs, _bcslen, _ucslen, all)
#define hawk_duputobchars(hawk, ucs, _ucslen, _bcslen) hawk_gem_duputobchars(hawk_getgem(hawk), ucs, _ucslen, _bcslen)
#define hawk_dupb2touchars(hawk, bcs1, bcslen1, bcs2, bcslen2, ucslen, all) hawk_gem_dupb2touchars(hawk_getgem(hawk), bcs1, bcslen1, bcs2, bcslen2, ucslen, all)
#define hawk_dupu2tobchars(hawk, ucs1, ucslen1, ucs2, ucslen2, bcslen) hawk_gem_dupu2tobchars(hawk_getgem(hawk), ucs1, ucslen1, ucs2, ucslen2, bcslen)
#define hawk_dupbtoucstr(hawk, bcs, _ucslen, all) hawk_gem_dupbtoucstr(hawk_getgem(hawk), bcs, _ucslen, all)
#define hawk_duputobcstr(hawk, ucs, _bcslen) hawk_gem_duputobcstr(hawk_getgem(hawk), ucs, _bcslen)
#define hawk_dupbtoucharswithcmgr(hawk, bcs, _bcslen, _ucslen, cmgr, all) hawk_gem_dupbtoucharswithcmgr(hawk_getgem(hawk), bcs, _bcslen, _ucslen, cmgr, all)
#define hawk_duputobcharswithcmgr(hawk, ucs, _ucslen, _bcslen, cmgr) hawk_gem_duputobcharswithcmgr(hawk_getgem(hawk), ucs, _ucslen, _bcslen, cmgr)
#define hawk_dupbcstrarrtoucstr(hawk, bcsarr, ucslen, all) hawk_gem_dupbcstrarrtoucstr(hawk_getgem(hawk), bcsarr, ucslen, all)
#define hawk_dupucstrarrtobcstr(hawk, ucsarr, bcslen) hawk_gem_dupucstrarrtobcstr(hawk_getgem(hawk), ucsarr, bcslen)
#endif

/* ----------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_fmttoucstr_ (
	hawk_t*           hawk,
	hawk_uch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_uch_t* fmt,
	...
);

HAWK_EXPORT hawk_oow_t hawk_fmttobcstr_ (
	hawk_t*           hawk,
	hawk_bch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_bch_t* fmt,
	...
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_oow_t hawk_vfmttoucstr (hawk_t* hawk, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, va_list ap) { return hawk_gem_vfmttoucstr(hawk_getgem(hawk), buf, bufsz, fmt, ap); }
static HAWK_INLINE hawk_oow_t hawk_fmttoucstr (hawk_t* hawk, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, ...) { va_list ap; hawk_oow_t n; va_start(ap, fmt); n = hawk_gem_vfmttoucstr(hawk_getgem(hawk), buf, bufsz, fmt, ap); va_end(ap); return n; }
static HAWK_INLINE hawk_oow_t hawk_vfmttobcstr (hawk_t* hawk, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, va_list ap) { return hawk_gem_vfmttobcstr(hawk_getgem(hawk), buf, bufsz, fmt, ap); }
static HAWK_INLINE hawk_oow_t hawk_fmttobcstr (hawk_t* hawk, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, ...) { va_list ap; hawk_oow_t n; va_start(ap, fmt); n = hawk_gem_vfmttobcstr(hawk_getgem(hawk), buf, bufsz, fmt, ap); va_end(ap); return n; }
#else
#define hawk_vfmttoucstr(hawk, buf, bufsz, fmt, ap) hawk_gem_vfmttoucstr(hawk_getgem(hawk), buf, bufsz, fmt, ap)
#define hawk_fmttoucstr hawk_fmttoucstr_
#define hawk_vfmttobcstr(hawk, buf, bufsz, fmt, ap) hawk_gem_vfmttobcstr(hawk_getgem(hawk), buf, bufsz, fmt, ap)
#define hawk_fmttobcstr hawk_fmttobcstr_
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_vfmttooocstr hawk_vfmttoucstr
#	define hawk_fmttooocstr hawk_fmttoucstr
#else
#	define hawk_vfmttooocstr hawk_vfmttobcstr
#	define hawk_fmttooocstr hawk_fmttobcstr
#endif

/* ----------------------------------------------------------------------- */

HAWK_EXPORT int hawk_buildrex (
	hawk_t*            hawk, 
	const hawk_ooch_t* ptn,
	hawk_oow_t         len,
	hawk_tre_t**       code, 
	hawk_tre_t**       icode
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_freerex (hawk_t* hawk, hawk_tre_t* code, hawk_tre_t* icode) { hawk_gem_freerex (hawk_getgem(hawk), code, icode); }
#else
#define hawk_freerex(hawk, code, icode) hawk_gem_freerex(hawk_getgem(hawk), code, icode)
#endif

/* ----------------------------------------------------------------------- */

HAWK_EXPORT hawk_ooi_t hawk_logufmtv (
	hawk_t*           hawk,
	hawk_bitmask_t    mask,
	const hawk_uch_t* fmt,
	va_list           ap
);

HAWK_EXPORT hawk_ooi_t hawk_logufmt (
	hawk_t*           hawk,
	hawk_bitmask_t    mask,
	const hawk_uch_t* fmt,
	...
);


HAWK_EXPORT hawk_ooi_t hawk_logbfmtv (
	hawk_t*           hawk,
	hawk_bitmask_t    mask,
	const hawk_bch_t* fmt,
	va_list           ap
);

HAWK_EXPORT hawk_ooi_t hawk_logbfmt (
	hawk_t*           hawk,
	hawk_bitmask_t    mask,
	const hawk_bch_t* fmt,
	...
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_logfmtv hawk_logufmtv
#	define hawk_logfmt hawk_logufmt
#else
#	define hawk_logfmtv hawk_logbfmtv
#	define hawk_logfmt hawk_logbfmt
#endif

/* ----------------------------------------------------------------------- */


/**
 * The hawk_rtx_open() creates a runtime context associated with \a hawk.
 * It also allocates an extra memory block as large as the \a xtn bytes.
 * You can get the pointer to the beginning of the block with 
 * hawk_rtx_getxtn(). The block is destroyed when the runtime context is
 * destroyed. 
 *
 * \return new runtime context on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_rtx_t* hawk_rtx_open (
	hawk_t*         hawk, /**< hawk */
	hawk_oow_t      xtn, /**< size of extension in bytes */
	hawk_rio_cbs_t* rio  /**< runtime IO handlers */
);

/**
 * The hawk_rtx_close() function destroys a runtime context.
 */
HAWK_EXPORT void hawk_rtx_close (
	hawk_rtx_t* rtx /**< runtime context */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_t* hawk_rtx_gethawk (hawk_rtx_t* rtx) { return ((hawk_rtx_alt_t*)rtx)->hawk; }
static HAWK_INLINE void* hawk_rtx_getxtn (hawk_rtx_t* rtx) { return (void*)((hawk_uint8_t*)rtx + ((hawk_rtx_alt_t*)rtx)->_instsize); }
static HAWK_INLINE hawk_gem_t* hawk_rtx_getgem (hawk_rtx_t* rtx) { return &((hawk_rtx_alt_t*)rtx)->_gem; }
static HAWK_INLINE hawk_mmgr_t* hawk_rtx_getmmgr (hawk_rtx_t* rtx) { return ((hawk_rtx_alt_t*)rtx)->_gem.mmgr; }
static HAWK_INLINE hawk_cmgr_t* hawk_rtx_getcmgr (hawk_rtx_t* rtx) { return ((hawk_rtx_alt_t*)rtx)->_gem.cmgr; }
static HAWK_INLINE void hawk_rtx_setcmgr (hawk_rtx_t* rtx, hawk_cmgr_t* cmgr) { ((hawk_rtx_alt_t*)rtx)->_gem.cmgr = cmgr; }
#else
#define hawk_rtx_gethawk(rtx) (((hawk_rtx_alt_t*)(rtx))->hawk)
#define hawk_rtx_getxtn(rtx) ((void*)((hawk_uint8_t*)rtx + ((hawk_rtx_alt_t*)rtx)->_instsize))
#define hawk_rtx_getgem(rtx) (&((hawk_rtx_alt_t*)(rtx))->_gem)
#define hawk_rtx_getmmgr(rtx) (((hawk_rtx_alt_t*)(rtx))->_gem.mmgr)
#define hawk_rtx_getcmgr(rtx) (((hawk_rtx_alt_t*)(rtx))->_gem.cmgr)
#define hawk_rtx_setcmgr(rtx,_cmgr) (((hawk_rtx_alt_t*)(rtx))->_gem.cmgr = (_cmgr))
#endif /* HAWK_HAVE_INLINE */

/**
 * The hawk_rtx_loop() function executes the BEGIN block, pattern-action
 * blocks and the END blocks in an AWK program. It returns the global return 
 * value of which the reference count must be decremented when not necessary. 
 * Multiple invocations of the function for the lifetime of a runtime context 
 * is not desirable.
 *
 * The example shows typical usage of the function.
 * \code
 * rtx = hawk_rtx_open(hawk, 0, rio);
 * if (rtx)
 * {
 *    retv = hawk_rtx_loop (rtx);
 *    if (retv) hawk_rtx_refdownval (rtx, retv);
 *    hawk_rtx_close (rtx);
 * }
 * \endcode
 *
 * \return return value on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_loop (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_findfunwithbcstr() function finds the function structure by 
 * name and returns the pointer to it if one is found. It returns #HAWK_NULL
 * if it fails to find a function by the \a name.
 */
HAWK_EXPORT hawk_fun_t* hawk_rtx_findfunwithbcstr (
	hawk_rtx_t*       rtx, /**< runtime context */
	const hawk_bch_t* name /**< function name */
);

/**
 * The hawk_rtx_findfunwithucstr() function finds the function structure by 
 * name and returns the pointer to it if one is found. It returns #HAWK_NULL
 * if it fails to find a function by the \a name.
 */
HAWK_EXPORT hawk_fun_t* hawk_rtx_findfunwithucstr (
	hawk_rtx_t*       rtx, /**< runtime context */
	const hawk_uch_t* name /**< function name */
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_rtx_findfunwithoocstr hawk_rtx_findfunwithbcstr
#else
#	define hawk_rtx_findfunwithoocstr hawk_rtx_findfunwithucstr
#endif

/**
 * The hawk_rtx_callfun() function invokes an AWK function described by
 * the structure pointed to by \a fun.
 * \sa hawk_rtx_call
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_callfun (
	hawk_rtx_t*  rtx,     /**< runtime context */
	hawk_fun_t*  fun,     /**< function */
	hawk_val_t*  args[],  /**< arguments to the function */
	hawk_oow_t   nargs    /**< the number of arguments */
);

/**
 * The hawk_rtx_callwithucstr() function invokes an AWK function named \a name. 
 * However, it is not able to invoke an intrinsic function such as split(). 
 * The #HAWK_PABLOCK option can be turned off to make illegal the BEGIN 
 * blocks, the pattern-action blocks, and the END blocks.
 *
 * The example shows typical usage of the function.
 * \code
 * rtx = hawk_rtx_open(hawk, 0, rio);
 * if (rtx)
 * {
 *     v = hawk_rtx_callwithucstr (rtx, HAWK_UT("init"), HAWK_NULL, 0);
 *     if (v) hawk_rtx_refdownval (rtx, v);
 *     hawk_rtx_callwithucstr (rtx, HAWK_UT("fini"), HAWK_NULL, 0);
 *     if (v) hawk_rtx_refdownval (rtx, v);
 *     hawk_rtx_close (rtx);
 * }
 * \endcode
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_callwithucstr (
	hawk_rtx_t*       rtx,    /**< runtime context */
	const hawk_uch_t* name,   /**< function name */
	hawk_val_t*       args[], /**< arguments to the function */
	hawk_oow_t        nargs   /**< the number of arguments */
);

/**
 * The hawk_rtx_callwithbcstr() function invokes an AWK function named \a name. 
 * However, it is not able to invoke an intrinsic function such as split(). 
 * The #HAWK_PABLOCK option can be turned off to make illegal the BEGIN 
 * blocks, the pattern-action blocks, and the END blocks.
 *
 * The example shows typical usage of the function.
 * \code
 * rtx = hawk_rtx_open(hawk, 0, rio);
 * if (rtx)
 * {
 *     v = hawk_rtx_callwithbcstr (rtx, HAWK_BT("init"), HAWK_NULL, 0);
 *     if (v) hawk_rtx_refdownval (rtx, v);
 *     hawk_rtx_callwithbcstr (rtx, HAWK_BT("fini"), HAWK_NULL, 0);
 *     if (v) hawk_rtx_refdownval (rtx, v);
 *     hawk_rtx_close (rtx);
 * }
 * \endcode
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_callwithbcstr (
	hawk_rtx_t*       rtx,    /**< runtime context */
	const hawk_bch_t* name,   /**< function name */
	hawk_val_t*       args[], /**< arguments to the function */
	hawk_oow_t        nargs   /**< the number of arguments */
);

/**
 * The hawk_rtx_callwithargarr() function is the same as hawk_rtx_call()
 * except that you pass pointers to null-terminated strings. It creates values
 * from the null-terminated strings and calls hawk_rtx_call() with the 
 * values created.
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_callwithucstrarr (
	hawk_rtx_t*        rtx,    /**< runtime context */
	const hawk_uch_t*  name,   /**< function name */
	const hawk_uch_t*  args[], /**< arguments to the function */
	hawk_oow_t         nargs   /**< the number of arguments */
);

HAWK_EXPORT hawk_val_t* hawk_rtx_callwithbcstrarr (
	hawk_rtx_t*        rtx,    /**< runtime context */
	const hawk_bch_t*  name,   /**< function name */
	const hawk_bch_t*  args[], /**< arguments to the function */
	hawk_oow_t         nargs   /**< the number of arguments */
);

HAWK_EXPORT hawk_val_t* hawk_rtx_callwithooucstrarr (
	hawk_rtx_t*        rtx,    /**< runtime context */
	const hawk_ooch_t* name,   /**< function name */
	const hawk_uch_t*  args[], /**< arguments to the function */
	hawk_oow_t         nargs   /**< the number of arguments */
);

HAWK_EXPORT hawk_val_t* hawk_rtx_callwithoobcstrarr (
	hawk_rtx_t*        rtx,    /**< runtime context */
	const hawk_ooch_t* name,   /**< function name */
	const hawk_bch_t*  args[], /**< arguments to the function */
	hawk_oow_t         nargs   /**< the number of arguments */
);

/**
 * The hawk_rtx_execwithucstrarr() function calls the starup function
 * if the @pragma startup directive is found in a top-level source
 * code or run hawk_rtx_loop() to enter the standard pattern loop
 * otherwise */
HAWK_EXPORT hawk_val_t* hawk_rtx_execwithucstrarr (
	hawk_rtx_t*        rtx,    /**< runtime context */
	const hawk_uch_t*  args[], /**< arguments to the function */
	hawk_oow_t         nargs   /**< the number of arguments */
);

HAWK_EXPORT hawk_val_t* hawk_rtx_execwithbcstrarr (
	hawk_rtx_t*        rtx,    /**< runtime context */
	const hawk_bch_t*  args[], /**< arguments to the function */
	hawk_oow_t         nargs   /**< the number of arguments */
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_rtx_callwithoocstr hawk_rtx_callwithucstr
#	define hawk_rtx_callwithoocstrarr hawk_rtx_callwithucstrarr
#	define hawk_rtx_execwithoocstrarr hawk_rtx_execwithucstrarr
#else
#	define hawk_rtx_callwithoocstr hawk_rtx_callwithbcstr
#	define hawk_rtx_callwithoocstrarr hawk_rtx_callwithbcstrarr
#	define hawk_rtx_execwithoocstrarr hawk_rtx_execwithbcstrarr
#endif

/**
 * The hawk_haltall() function aborts all active runtime contexts
 * associated with \a hawk.
 */
HAWK_EXPORT void hawk_haltall (
	hawk_t* hawk /**< hawk */
);

/**
 * The hawk_rtx_ishalt() function tests if hawk_rtx_halt() has been 
 * called.
 */
HAWK_EXPORT int hawk_rtx_ishalt (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_halt() function causes an active runtime context \a rtx to 
 * be aborted. 
 */
HAWK_EXPORT void hawk_rtx_halt (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_getrio() function copies runtime I/O handlers
 * to the memory buffer pointed to by \a rio.
 */
HAWK_EXPORT void hawk_rtx_getrio (
	hawk_rtx_t*     rtx,
	hawk_rio_cbs_t* rio
);

/**
 * The hawk_rtx_getrio() function sets runtime I/O handlers
 * with the functions pointed to by \a rio.
 */
HAWK_EXPORT void hawk_rtx_setrio (
	hawk_rtx_t*           rtx,
	const hawk_rio_cbs_t* rio
);

/**
 * The hawk_rtx_popecb() function pops a runtime callback set
 * and returns the pointer to it. If no callback set can be popped,
 * it returns #HAWK_NULL.
 */
HAWK_EXPORT hawk_rtx_ecb_t* hawk_rtx_popecb (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_pushecb() function register a runtime callback set.
 */
HAWK_EXPORT void hawk_rtx_pushecb (
	hawk_rtx_t*     rtx, /**< runtime context */
	hawk_rtx_ecb_t* ecb  /**< callback set */
);

/**
 * The hawk_rtx_getnargs() gets the number of arguments passed to an 
 * intrinsic functon.
 */
HAWK_EXPORT hawk_oow_t hawk_rtx_getnargs (
	hawk_rtx_t* rtx
);

/**
 * The hawk_rtx_getarg() function gets an argument passed to an intrinsic 
 * function. it doesn't touch the reference count of the value.
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_getarg (
	hawk_rtx_t* rtx,
	hawk_oow_t     idx
);

/**
 * The hawk_rtx_getsubsep() function returns the
 * pointer to the internal value of SUBSEP. It's a specialized
 * version of hawk_rtx_getgbl (rtx, HAWK_GBL_SUBSEP).
 */
HAWK_EXPORT const hawk_oocs_t* hawk_rtx_getsubsep (
	hawk_rtx_t* rtx  /**< runtime context */
);

/**
 * The hawk_rtx_getgbl() gets the value of a global variable.
 * The global variable ID \a id is one of the predefined global 
 * variable IDs or a value returned by hawk_addgbl().
 * This function never fails so long as the ID is valid. Otherwise, 
 * you may get into trouble.
 *
 * \return value pointer
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_getgbl (
	hawk_rtx_t* rtx, /**< runtime context */
	int            id   /**< global variable ID */
);

/**
 * The hawk_rtx_setgbl() sets the value of a global variable.
 */
HAWK_EXPORT int hawk_rtx_setgbl (
	hawk_rtx_t* rtx, 
	int         id,
	hawk_val_t* val
);

HAWK_EXPORT int hawk_rtx_setgbltostrbyname (
	hawk_rtx_t*        rtx,
	const hawk_ooch_t* name,
	const hawk_ooch_t* val
);

/**
 * The hawk_rtx_setretval() sets the return value of a function
 * when called from within a function handler. The caller doesn't
 * have to invoke hawk_rtx_refupval() and hawk_rtx_refdownval() 
 * with the value to be passed to hawk_rtx_setretval(). 
 * The hawk_rtx_setretval() will update its reference count properly
 * once the return value is set. 
 */
HAWK_EXPORT void hawk_rtx_setretval (
	hawk_rtx_t* rtx, /**< runtime context */
	hawk_val_t* val  /**< return value */
);

/**
 * The hawk_rtx_setfilename() function sets FILENAME.
 */
HAWK_EXPORT int hawk_rtx_setfilenamewithuchars (
	hawk_rtx_t*        rtx, /**< runtime context */
	const hawk_uch_t*  str, /**< name pointer */
	hawk_oow_t         len  /**< name length */
);

HAWK_EXPORT int hawk_rtx_setfilenamewithbchars (
	hawk_rtx_t*        rtx, /**< runtime context */
	const hawk_bch_t*  str, /**< name pointer */
	hawk_oow_t         len  /**< name length */
);

/**
 * The hawk_rtx_setofilename() function sets OFILENAME.
 */
HAWK_EXPORT int hawk_rtx_setofilenamewithuchars (
	hawk_rtx_t*        rtx, /**< runtime context */
	const hawk_uch_t*  str, /**< name pointer */
	hawk_oow_t         len  /**< name length */
);

HAWK_EXPORT int hawk_rtx_setofilenamewithbchars (
	hawk_rtx_t*        rtx, /**< runtime context */
	const hawk_bch_t*  str, /**< name pointer */
	hawk_oow_t         len  /**< name length */
);


HAWK_EXPORT int hawk_rtx_setscriptnamewithuchars (
	hawk_rtx_t*        rtx, /**< runtime context */
	const hawk_uch_t*  str, /**< name pointer */
	hawk_oow_t         len  /**< name length */
);

HAWK_EXPORT int hawk_rtx_setscriptnamewithbchars (
	hawk_rtx_t*        rtx, /**< runtime context */
	const hawk_bch_t*  str, /**< name pointer */
	hawk_oow_t         len  /**< name length */
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_rtx_setfilenamewithoochars hawk_rtx_setfilenamewithuchars
#	define hawk_rtx_setofilenamewithoochars hawk_rtx_setofilenamewithuchars
#	define hawk_rtx_setscriptnamewithoochars hawk_rtx_setscriptnamewithuchars
#else
#	define hawk_rtx_setfilenamewithoochars hawk_rtx_setfilenamewithbchars
#	define hawk_rtx_setofilenamewithoochars hawk_rtx_setofilenamewithbchars
#	define hawk_rtx_setscriptnamewithoochars hawk_rtx_setscriptnamewithbchars
#endif

/**
 * The hawk_rtx_getnvmap() gets the map of named variables 
 */
HAWK_EXPORT hawk_htb_t* hawk_rtx_getnvmap (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_geterrnum() function gets the number of the last error
 * occurred during runtime.
 * \return error number
 */
#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_errnum_t hawk_rtx_geterrnum (hawk_rtx_t* rtx) { return ((hawk_rtx_alt_t*)rtx)->_gem.errnum; }
#else
#	define hawk_rtx_geterrnum(hawk) (((hawk_rtx_alt_t*)(rtx))->_gem.errnum)
#endif

/**
 * The hawk_rtx_geterrloc() function gets the location of the last error
 * occurred during runtime. The 
 * \return error location
 */
HAWK_EXPORT const hawk_loc_t* hawk_rtx_geterrloc (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_geterrbmsg() function gets the string describing the last 
 * error occurred during runtime.
 * \return error message
 */
HAWK_EXPORT const hawk_bch_t* hawk_rtx_geterrbmsg (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_geterrumsg() function gets the string describing the last 
 * error occurred during runtime.
 * \return error message
 */
HAWK_EXPORT const hawk_uch_t* hawk_rtx_geterrumsg (
	hawk_rtx_t* rtx /**< runtime context */
);

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_rtx_geterrmsg hawk_rtx_geterrbmsg
#else
#	define hawk_rtx_geterrmsg hawk_rtx_geterrumsg
#endif

HAWK_EXPORT const hawk_ooch_t* hawk_rtx_backuperrmsg (
	hawk_rtx_t* rtx /**< runtime context */
);

/**
 * The hawk_rtx_geterrinf() function copies error information into memory
 * pointed to by \a errinf from a runtime context \a rtx.
 */
HAWK_EXPORT void hawk_rtx_geterrinf (
	hawk_rtx_t*       rtx,   /**< runtime context */
	hawk_errinf_t*    errinf /**< error information */
);

/**
 * The hawk_rtx_geterror() function retrieves error information from a 
 * runtime context \a rtx. The error number is stored into memory pointed
 * to by \a errnum; the error message pointer into memory pointed to by 
 * \a errmsg; the error line into memory pointed to by \a errlin.
 */
HAWK_EXPORT void hawk_rtx_geterror (
	hawk_rtx_t*         rtx,    /**< runtime context */
	hawk_errnum_t*      errnum, /**< error number */
	const hawk_ooch_t** errmsg, /**< error message */
	hawk_loc_t*         errloc  /**< error location */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_rtx_seterrnum (hawk_rtx_t* rtx, const hawk_loc_t* errloc, hawk_errnum_t errnum) { hawk_gem_seterrnum (hawk_rtx_getgem(rtx), errloc, errnum); }
#else
#define hawk_rtx_seterrnum(rtx, errloc, errnum) hawk_gem_seterrnum(hawk_rtx_getgem(rtx), errloc, errnum)
#endif

/** 
 * The hawk_rtx_seterrinf() function sets error information.
 */
HAWK_EXPORT void hawk_rtx_seterrinf (
	hawk_rtx_t*          rtx,   /**< runtime context */
	const hawk_errinf_t* errinf /**< error information */
);

HAWK_EXPORT void hawk_rtx_seterrbfmt (
	hawk_rtx_t*       rtx,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_bch_t* errfmt,
	...
);

HAWK_EXPORT void hawk_rtx_seterrufmt (
	hawk_rtx_t*       rtx,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_uch_t* errfmt,
	...
);

HAWK_EXPORT void hawk_rtx_seterrbvfmt (
	hawk_rtx_t*       rtx,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_bch_t* errfmt,
	va_list           ap
);

HAWK_EXPORT void hawk_rtx_seterruvfmt (
	hawk_rtx_t*       rtx,
	const hawk_loc_t* errloc,
	hawk_errnum_t     errnum,
	const hawk_uch_t* errfmt,
	va_list           ap
);

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_rtx_seterrfmt hawk_rtx_seterrufmt
#	define hawk_rtx_seterrvfmt hawk_rtx_seterruvfmt
#else
#	define hawk_rtx_seterrfmt hawk_rtx_seterrbfmt
#	define hawk_rtx_seterrvfmt hawk_rtx_seterrbvfmt
#endif

/**
 * The hawk_rtx_errortohawk() function copies the error information stored
 * in the \a rtx object to the \a hawk object.
 */
HAWK_EXPORT void hawk_rtx_errortohawk (
	hawk_rtx_t* rtx,
	hawk_t*     hawk
);

/**
 * The hawk_rtx_clrrec() function clears the input record ($0) 
 * and fields ($1 to $N).
 */
HAWK_EXPORT int hawk_rtx_clrrec (
	hawk_rtx_t*  rtx, /**< runtime context */
	int          skip_inrec_line 
);

/**
 * The hawk_rtx_setrec() function sets the input record ($0) or 
 * input fields ($1 to $N).
 */
HAWK_EXPORT int hawk_rtx_setrec (
	hawk_rtx_t*        rtx, /**< runtime context */
	hawk_oow_t         idx, /**< 0 for $0, N for $N */
	const hawk_oocs_t* str,  /**< string */
	int                prefer_number /* if true, a numeric string makes an int or flt value */
);

/**
 * The hawk_rtx_truncrec() function lowered the number of fields in a record.
 * The caller must ensure that \a nflds is less than the current number of fields
 */
HAWK_EXPORT int hawk_rtx_truncrec (
	hawk_rtx_t* rtx,
	hawk_oow_t  nflds
);

/**
 * The hawk_rtx_isnilval() function determines if a value
 * is a nil value.
 */
HAWK_EXPORT int hawk_rtx_isnilval (
	hawk_rtx_t* rtx,
	hawk_val_t* val
);

/**
 * The hawk_rtx_makenilval() function creates a nil value.
 * It always returns the pointer to the statically allocated
 * nil value. So it never fails.
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makenilval (
	hawk_rtx_t* rtx
);


HAWK_EXPORT hawk_val_t* hawk_rtx_makecharval (
	hawk_rtx_t* rtx,
	hawk_ooch_t v
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makebchrval (
	hawk_rtx_t* rtx,
	hawk_bch_t v
);

/**
 * The hawk_rtx_makeintval() function creates an integer value.
 * If \a v is one of -1, 0, 1, this function never fails.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makeintval (
	hawk_rtx_t* rtx,
	hawk_int_t  v
);

/**
 * The hawk_rtx_makefltval() function creates a floating-point value.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makefltval (
	hawk_rtx_t* rtx,
	hawk_flt_t  v
);

/* -------------------------------------------------------------------------- */


HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithuchars (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ucs,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithbchars (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* bcs,
	hawk_oow_t        len
);

/**
 * The hawk_rtx_makestrvalwithbcstr() function creates a string value
 * from a null-terminated multibyte string.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithbcstr (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* bcs
);

/**
 * The hawk_rtx_makestrvalwithucstr() function creates a string value
 * from a null-terminated wide-character string.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithucstr (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ucs
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithbcs (
	hawk_rtx_t*         rtx,
	const hawk_bcs_t* bcs
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithucs (
	hawk_rtx_t*         rtx,
	const hawk_ucs_t*   ucs
);


HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithuchars2 (
	hawk_rtx_t*        rtx,
	const hawk_uch_t*  str1,
	hawk_oow_t         len1, 
	const hawk_uch_t*  str2,
	hawk_oow_t         len2
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makestrvalwithbchars2 (
	hawk_rtx_t*        rtx,
	const hawk_bch_t*  str1,
	hawk_oow_t         len1, 
	const hawk_bch_t*  str2,
	hawk_oow_t         len2
);

/* -------------------------------------------------------------------------- */

HAWK_EXPORT hawk_val_t* hawk_rtx_makenumorstrvalwithuchars (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ptr,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makenumorstrvalwithbchars (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* ptr,
	hawk_oow_t        len
);

/* -------------------------------------------------------------------------- */

HAWK_EXPORT hawk_val_t* hawk_rtx_makenstrvalwithuchars (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ptr,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makenstrvalwithbchars (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* ptr,
	hawk_oow_t        len
);

/**
 * The hawk_rtx_makenstrvalwithucstr() function creates a numeric string 
 * value from a null-terminated string. A numeric string is a string value 
 * whose one of the header fields \b nstr is 1.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makenstrvalwithucstr (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* str
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makenstrvalwithbcstr (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* str
);

/**
 * The hawk_rtx_makenstrvalwithucs() function creates a numeric string 
 * value. A numeric string is a string value whose one of the header fields
 * \b nstr is 1.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makenstrvalwithucs (
	hawk_rtx_t*       rtx,
	const hawk_ucs_t* str 
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makenstrvalwithbcs (
	hawk_rtx_t*       rtx,
	const hawk_bcs_t* str 
);

#if defined (HAWK_OOCH_IS_UCH)
#	define hawk_rtx_makestrvalwithoochars hawk_rtx_makestrvalwithuchars
#	define hawk_rtx_makestrvalwithoocstr hawk_rtx_makestrvalwithucstr
#	define hawk_rtx_makestrvalwithoocs hawk_rtx_makestrvalwithucs
#	define hawk_rtx_makestrvalwithoochars2 hawk_rtx_makestrvalwithuchars2

#	define hawk_rtx_makenumorstrvalwithoochars hawk_rtx_makenumorstrvalwithuchars

#	define hawk_rtx_makenstrvalwithoochars hawk_rtx_makenstrvalwithuchars
#	define hawk_rtx_makenstrvalwithoocstr hawk_rtx_makenstrvalwithucstr
#	define hawk_rtx_makenstrvalwithoocs hawk_rtx_makenstrvalwithucs
#else
#	define hawk_rtx_makestrvalwithoochars hawk_rtx_makestrvalwithbchars
#	define hawk_rtx_makestrvalwithoocstr hawk_rtx_makestrvalwithbcstr
#	define hawk_rtx_makestrvalwithoocs hawk_rtx_makestrvalwithbcs
#	define hawk_rtx_makestrvalwithoochars2 hawk_rtx_makestrvalwithbchars2

#	define hawk_rtx_makenumorstrvalwithoochars hawk_rtx_makenumorstrvalwithbchars

#	define hawk_rtx_makenstrvalwithoochars hawk_rtx_makenstrvalwithbchars
#	define hawk_rtx_makenstrvalwithoocstr hawk_rtx_makenstrvalwithbcstr
#	define hawk_rtx_makenstrvalwithoocs hawk_rtx_makenstrvalwithbcs
#endif

/* -------------------------------------------------------------------------- */

/**
 * The hawk_rtx_makembsvalwithbchars() function create a byte array value.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithbchars (
	hawk_rtx_t*        rtx,
	const hawk_bch_t*  ptr,
	hawk_oow_t         len
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithuchars (
	hawk_rtx_t*        rtx,
	const hawk_uch_t*  ptr,
	hawk_oow_t         len
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithbcs (
	hawk_rtx_t*       rtx,
	const hawk_bcs_t* bcs
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithucs (
	hawk_rtx_t*       rtx,
	const hawk_ucs_t* ucs
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithbcstr (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* bcs
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithucstr (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ucs
);


HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithuchars2 (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ucs1,
	hawk_oow_t        len1,
	const hawk_uch_t* ucs2,
	hawk_oow_t        len2
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makembsvalwithbchars2 (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* bcs1,
	hawk_oow_t        len1,
	const hawk_bch_t* bcs2,
	hawk_oow_t        len2
);

/* -------------------------------------------------------------------------- */

HAWK_EXPORT hawk_val_t* hawk_rtx_makenumormbsvalwithuchars (
	hawk_rtx_t*       rtx,
	const hawk_uch_t* ptr,
	hawk_oow_t        len
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makenumormbsvalwithbchars (
	hawk_rtx_t*       rtx,
	const hawk_bch_t* ptr,
	hawk_oow_t        len
);


/* -------------------------------------------------------------------------- */

/**
 * The hawk_rtx_makerexval() function creates a regular expression value.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makerexval (
	hawk_rtx_t*        rtx,
	const hawk_oocs_t* str,
	hawk_tre_t*        code[2]
);

/**
 * The hawk_rtx_makemapval() function creates an empty array value.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makearrval (
	hawk_rtx_t* rtx,
	hawk_ooi_t  init_capa
);

/**
 * The hawk_rtx_makemapval() function creates an empty map value.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makemapval (
	hawk_rtx_t* rtx
);

/**
 * The hawk_rtx_makemapvalwithdata() function creates a map value
 * containing key/value pairs described in the structure array \a data.
 * It combines hawk_rtx_makemapval() an hawk_rtx_setmapvalfld()
 * for convenience sake.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makemapvalwithdata (
	hawk_rtx_t*         rtx,
	hawk_val_map_data_t data[],
	hawk_oow_t          count
);

/**
 * The hawk_rtx_setmapvalfld() function sets a field value in a map.
 * You must make sure that the type of \a map is #HAWK_VAL_MAP.
 * \return value \a v on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_setmapvalfld (
	hawk_rtx_t*        rtx,
	hawk_val_t*        map,
	const hawk_ooch_t* kptr,
	hawk_oow_t         klen,
	hawk_val_t*        v
);

/**
 * The hawk_rtx_setmapvalfld() function gets the field value in a map.
 * You must make sure that the type of \a map is #HAWK_VAL_MAP.
 * If the field is not found, the function fails and sets the error number
 * to #HAWK_EINVAL. The function does not fail for other reasons.
 * \return field value on success, #HAWK_NULL on failure.
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_getmapvalfld (
	hawk_rtx_t*         rtx,
	hawk_val_t*         map,
	const hawk_ooch_t*  kptr,
	hawk_oow_t          klen
);

/**
 * The hawk_rtx_getfirstmapvalitr() returns the iterator to the
 * first pair in the map. It returns #HAWK_NULL and sets the pair field of 
 * \a itr to #HAWK_NULL if the map contains no pair. Otherwise, it returns
 * \a itr pointing to the first pair.
 */
HAWK_EXPORT hawk_val_map_itr_t* hawk_rtx_getfirstmapvalitr (
	hawk_rtx_t*         rtx,
	hawk_val_t*         map,
	hawk_val_map_itr_t* itr
);

/**
 * The hawk_rtx_getnextmapvalitr() returns the iterator to the
 * next pair to \a itr in the map. It returns #HAWK_NULL and sets the pair 
 * field of \a itr to #HAWK_NULL if \a itr points to the last pair.
 * Otherwise, it returns \a itr pointing to the next pair.
 */
HAWK_EXPORT hawk_val_map_itr_t* hawk_rtx_getnextmapvalitr (
	hawk_rtx_t*         rtx,
	hawk_val_t*         map,
	hawk_val_map_itr_t* itr
);


HAWK_EXPORT hawk_val_t* hawk_rtx_setarrvalfld (
	hawk_rtx_t* rtx,
	hawk_val_t* arr,
	hawk_ooi_t  index,
	hawk_val_t* v
);

HAWK_EXPORT hawk_val_t* hawk_rtx_getarrvalfld (
	hawk_rtx_t* rtx,
	hawk_val_t* arr,
	hawk_ooi_t  index
);

/**
 * The hawk_rtx_makerefval() function creates a reference value.
 * \return value on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_val_t* hawk_rtx_makerefval (
	hawk_rtx_t*  rtx,
	int             id,
	hawk_val_t** adr
);

HAWK_EXPORT hawk_val_t* hawk_rtx_makefunval (
	hawk_rtx_t*       rtx,
	const hawk_fun_t* fun
);

/**
 * The hawk_rtx_isstaticval() function determines if a value is static.
 * A static value is allocated once and reused until a runtime context @ rtx 
 * is closed.
 * \return HAWK_TRUE if \a val is static, HAWK_FALSE if \a val is false
 */
HAWK_EXPORT int hawk_rtx_isstaticval (
	hawk_rtx_t* rtx, /**< runtime context */
	hawk_val_t* val  /**< value to check */
);

HAWK_EXPORT int hawk_rtx_getvaltype (
	hawk_rtx_t* rtx,
	hawk_val_t* val
);

HAWK_EXPORT const hawk_ooch_t* hawk_rtx_getvaltypename (
	hawk_rtx_t* rtx,
	hawk_val_t* val
);

HAWK_EXPORT int hawk_rtx_getintfromval (
	hawk_rtx_t* rtx,
	hawk_val_t* val
);

/**
 * The hawk_rtx_refupval() function increments a reference count of a 
 * value \a val.
 */
HAWK_EXPORT void hawk_rtx_refupval (
	hawk_rtx_t* rtx, /**< runtime context */
	hawk_val_t* val  /**< value */
);

/**
 * The hawk_rtx_refdownval() function decrements a reference count of
 * a value \a val. It destroys the value if it has reached the count of 0.
 */
HAWK_EXPORT void hawk_rtx_refdownval (
	hawk_rtx_t* rtx, /**< runtime context */
	hawk_val_t* val  /**< value pointer */
);

/**
 * The hawk_rtx_refdownval() function decrements a reference count of
 * a value \a val. It does not destroy the value if it has reached the 
 * count of 0.
 */
HAWK_EXPORT void hawk_rtx_refdownval_nofree (
	hawk_rtx_t* rtx, /**< runtime context */
	hawk_val_t* val  /**< value pointer */
);


#define HAWK_RTX_GC_GEN_FULL (HAWK_TYPE_MAX(int))
#define HAWK_RTX_GC_GEN_AUTO (-1)

/* 
 * The hawk_rtc_gc() function triggers garbage collection.
 * It returns the generation number collected and never fails
 */
HAWK_EXPORT int hawk_rtx_gc (
	hawk_rtx_t* rtx,
	int         gen
);

/**
 * The hawk_rtx_valtobool() function converts a value \a val to a boolean
 * value.
 */
HAWK_EXPORT int hawk_rtx_valtobool (
	hawk_rtx_t*       rtx, /**< runtime context */
	const hawk_val_t* val  /**< value pointer */
);

/**
 * The hawk_rtx_valtostr() function converts a value \a val to a string as 
 * instructed in the parameter out. Before the call to the function, you 
 * should initialize a variable of the #hawk_rtx_valtostr_out_t type.
 *
 * The type field is one of the following hawk_rtx_valtostr_type_t values:
 *
 * - #HAWK_RTX_VALTOSTR_CPL
 * - #HAWK_RTX_VALTOSTR_CPLCPY
 * - #HAWK_RTX_VALTOSTR_CPLDUP
 * - #HAWK_RTX_VALTOSTR_STRP
 * - #HAWK_RTX_VALTOSTR_STRPCAT
 *
 * It can optionally be ORed with #HAWK_RTX_VALTOSTR_PRINT. The option
 * causes the function to use OFMT for real number conversion. Otherwise,
 * it uses \b CONVFMT. 
 *
 * You should initialize or free other fields before and after the call 
 * depending on the type field as shown below:
 *  
 * If you have a static buffer, use #HAWK_RTX_VALTOSTR_CPLCPY.
 * the resulting string is copied to the buffer.
 * \code
 * hawk_rtx_valtostr_out_t out;
 * hawk_ooch_t buf[100];
 * out.type = HAWK_RTX_VALTOSTR_CPLCPY;
 * out.u.cplcpy.ptr = buf;
 * out.u.cplcpy.len = HAWK_COUNTOF(buf);
 * if (hawk_rtx_valtostr (rtx, v, &out) <= -1) goto oops;
 * hawk_printf (HAWK_T("%.*s\n"), out.u.cplcpy.len, out.u.cplcpy.ptr);
 * \endcode
 *
 * #HAWK_RTX_VALTOSTR_CPL is different from #HAWK_RTX_VALTOSTR_CPLCPY
 * in that it doesn't copy the string to the buffer if the type of the value
 * is #HAWK_VAL_STR. It copies the resulting string to the buffer if
 * the value type is not #HAWK_VAL_STR. 
 * \code
 * hawk_rtx_valtostr_out_t out;
 * hawk_ooch_t buf[100];
 * out.type = HAWK_RTX_VALTOSTR_CPL;
 * out.u.cpl.ptr = buf;
 * out.u.cpl.len = HAWK_COUNTOF(buf);
 * if (hawk_rtx_valtostr (rtx, v, &out) <= -1) goto oops;
 * hawk_printf (HAWK_T("%.*s\n"), ut.u.cpl.len, out.u.cpl.ptr);
 * \endcode
 * 
 * When unsure of the size of the string after conversion, you can use
 * #HAWK_RTX_VALTOSTR_CPLDUP. However, you should free the memory block
 * pointed to by the u.cpldup.ptr field after use.
 * \code
 * hawk_rtx_valtostr_out_t out;
 * out.type = HAWK_RTX_VALTOSTR_CPLDUP;
 * if (hawk_rtx_valtostr (rtx, v, &out) <= -1) goto oops;
 * hawk_printf (HAWK_T("%.*s\n"), out.u.cpldup.len, out.u.cpldup.ptr);
 * hawk_rtx_free (rtx, out.u.cpldup.ptr);
 * \endcode
 *
 * You may like to store the result in a dynamically resizable string.
 * Consider #HAWK_RTX_VALTOSTR_STRP.
 * \code
 * hawk_rtx_valtostr_out_t out;
 * hawk_ooecs_t str;
 * hawk_str_init (&str, hawk_rtx_getmmgr(rtx), 100);
 * out.type = HAWK_RTX_VALTOSTR_STRP;
 * out.u.strp = str;
 * if (hawk_rtx_valtostr (rtx, v, &out) <= -1) goto oops;
 * hawk_printf (HAWK_T("%.*s\n"), HAWK_STR_LEN(out.u.strp), HAWK_STR_PTR(out.u.strp));
 * hawk_str_fini (&str);
 * \endcode
 * 
 * If you want to append the converted string to an existing dynamically 
 * resizable string, #HAWK_RTX_VALTOSTR_STRPCAT is the answer. The usage is
 * the same as #HAWK_RTX_VALTOSTR_STRP except that you have to use the 
 * u.strpcat field instead of the u.strp field.
 *
 * In the context where \a val is determined to be of the type
 * #HAWK_VAL_STR, you may access its string pointer and length directly
 * instead of calling this function.
 *
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_rtx_valtostr (
	hawk_rtx_t*              rtx, /**< runtime context */
	const hawk_val_t*        val, /**< value to convert */
	hawk_rtx_valtostr_out_t* out  /**< output buffer */
);

/**
 * The hawk_rtx_valtooocstrdupwithcmgr() function duplicates a string value and returns
 * the pointer to it. If the given value is not a string, it converts the non-string
 * value to a string and duplicates it. It stores the length of the resulting
 * string in memory pointed to by \a len.
 * You should free the returned memory block after use. See the code snippet below
 * for a simple usage.
 *
 * \code
 * ptr = hawk_rtx_valtoucstrdupwithcmgr(rtx, v, &len, hawk_rtx_getcmgr(rtx));
 * if (!str) handle_error();
 * hawk_printf (HAWK_T("%.*ls\n"), (int)len, ptr);
 * hawk_rtx_free (rtx, ptr);
 * \endcode
 *
 * \return character pointer to a duplicated string on success,
 *         #HAWK_NULL on failure
 */

HAWK_EXPORT hawk_bch_t* hawk_rtx_valtobcstrdupwithcmgr (
	hawk_rtx_t*       rtx, /**< runtime context */
	const hawk_val_t* val, /**< value to convert */
	hawk_oow_t*       len, /**< result length */
	hawk_cmgr_t*      cmgr
);

HAWK_EXPORT hawk_uch_t* hawk_rtx_valtoucstrdupwithcmgr (
	hawk_rtx_t*       rtx, /**< runtime context */
	const hawk_val_t* val, /**< value to convert */
	hawk_oow_t*       len, /**< result length */
	hawk_cmgr_t*      cmgr
);


#define hawk_rtx_valtobcstrdup(rtx, val, len) hawk_rtx_valtobcstrdupwithcmgr(rtx, val, len, hawk_rtx_getcmgr(rtx))
#define hawk_rtx_valtoucstrdup(rtx, val, len) hawk_rtx_valtoucstrdupwithcmgr(rtx, val, len, hawk_rtx_getcmgr(rtx))

#if defined(HAWK_OOCH_IS_BCH)
#	define hawk_rtx_valtooocstrdupwithcmgr(rtx, val, len, cmgr) hawk_rtx_valtobcstrdupwithcmgr(rtx, val, len, cmgr)
#	define hawk_rtx_valtooocstrdup(rtx, val, len) hawk_rtx_valtobcstrdup(rtx, val, len)
#else
#	define hawk_rtx_valtooocstrdupwithcmgr(rtx, val, len, cmgr) hawk_rtx_valtoucstrdupwithcmgr(rtx, val, len, cmgr)
#	define hawk_rtx_valtooocstrdup(rtx, val, len) hawk_rtx_valtoucstrdup(rtx, val, len)
#endif

/**
 * The hawk_rtx_getvaloocstr() function returns a string
 * pointer converted from a value \a val. If the value 
 * type is #HAWK_VAL_STR, it simply returns the internal
 * pointer without duplication. Otherwise, it calls
 * hawk_rtx_valtooocstrdup(). The length of the returned
 * string is stored into the location pointed to by \a len.
 */
HAWK_EXPORT hawk_ooch_t* hawk_rtx_getvaloocstrwithcmgr (
	hawk_rtx_t*       rtx, /**< runtime context */
	hawk_val_t*       val, /**< value to convert */
	hawk_oow_t*       len, /**< result length */
	hawk_cmgr_t*      cmgr
);

#define hawk_rtx_getvaloocstr(rtx,val,len) hawk_rtx_getvaloocstrwithcmgr(rtx, val, len, hawk_rtx_getcmgr(rtx))

/**
 * The hawk_rtx_freevaloocstr() function frees the memory pointed 
 * to by \a str if \a val is not of the #HAWK_VAL_STR type. 
 * This function expects a value pointer and a string pointer
 * passed to and returned by hawk_rtx_getvaloocstr() respectively.
 */
HAWK_EXPORT void hawk_rtx_freevaloocstr (
	hawk_rtx_t*       rtx, /**< runtime context */
	hawk_val_t*       val, /**< value to convert */
	hawk_ooch_t*      str  /**< string pointer */
);


HAWK_EXPORT hawk_bch_t* hawk_rtx_getvalbcstrwithcmgr (
	hawk_rtx_t*       rtx, /**< runtime context */
	hawk_val_t*       val, /**< value to convert */
	hawk_oow_t*       len, /**< result length */
	hawk_cmgr_t*      cmgr
);

#define hawk_rtx_getvalbcstr(rtx, val, len) hawk_rtx_getvalbcstrwithcmgr(rtx, val, len, hawk_rtx_getcmgr(rtx))

HAWK_EXPORT void hawk_rtx_freevalbcstr (
	hawk_rtx_t*       rtx, /**< runtime context */
	hawk_val_t*       val, /**< value to convert */
	hawk_bch_t*       str  /**< string pointer */
);

/**
 * The hawk_rtx_valtonum() function converts a value to a number. 
 * If the value is converted to an integer, it is stored in the memory
 * pointed to by l and 0 is returned. If the value is converted to a real 
 * number, it is stored in the memory pointed to by r and 1 is returned.
 * The function never fails as long as \a val points to a valid value.
 *
 * The code below shows how to convert a value to a number and determine
 * if it is an integer or a floating-point number.
 *
 * \code
 * hawk_int_t l;
 * hawk_flt_t r;
 * int n;
 * n = hawk_rtx_valtonum (v, &l, &r);
 * if (n <= -1) error ();
 * else if (n == 0) print_int (l);
 * else if (n >= 1) print_flt (r);
 * \endcode
 *
 * \return -1 on failure, 0 if converted to an integer, 1 if converted to 
 *         a floating-point number.
 */
HAWK_EXPORT int hawk_rtx_valtonum (
	hawk_rtx_t*       rtx,
	const hawk_val_t* val,
	hawk_int_t*       l,
	hawk_flt_t*       r
);

HAWK_EXPORT int hawk_rtx_valtoint (
	hawk_rtx_t*       rtx,
	const hawk_val_t* val,
	hawk_int_t*       l
);

HAWK_EXPORT int hawk_rtx_valtoflt (
	hawk_rtx_t*       rtx,
	const hawk_val_t* val,
	hawk_flt_t*       r
);

HAWK_EXPORT hawk_fun_t* hawk_rtx_valtofun (
	hawk_rtx_t* rtx,
	hawk_val_t* val
);

/**
 * The hawk_rtx_hashval() function hashes a simple value
 * to a positive integer. It returns -1 for a inhashable value.
 */
HAWK_EXPORT hawk_int_t hawk_rtx_hashval (
	hawk_rtx_t*   rtx,
	hawk_val_t*   v
);

/**
 * The hawk_rtx_getrefvaltype() function returns the type of the value
 * that the given reference points to.
 */
HAWK_EXPORT hawk_val_type_t hawk_rtx_getrefvaltype (
	hawk_rtx_t*     rtx,
	hawk_val_ref_t* ref
);

HAWK_EXPORT hawk_val_t* hawk_rtx_getrefval (
	hawk_rtx_t*     rtx,
	hawk_val_ref_t* ref
);

/**
 * The hawk_rtx_setrefval() function changes the value
 * of a variable referenced in \a ref. 
 * \return 0 on success, -1 on failure.
 */
HAWK_EXPORT int hawk_rtx_setrefval (
	hawk_rtx_t*     rtx,
	hawk_val_ref_t* ref,
	hawk_val_t*     val
);

HAWK_EXPORT void hawk_rtx_setnrflt (
	hawk_rtx_t*         rtx,
	const hawk_nrflt_t* nrflt
);

HAWK_EXPORT void hawk_rtx_getnrflt (
	hawk_rtx_t*         rtx,
	hawk_nrflt_t*       nrflt
);


#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_rtx_allocmem (hawk_rtx_t* rtx, hawk_oow_t size) { return hawk_gem_allocmem(hawk_rtx_getgem(rtx), size); }
static HAWK_INLINE void* hawk_rtx_reallocmem (hawk_rtx_t* rtx, void* ptr, hawk_oow_t size) { return hawk_gem_reallocmem(hawk_rtx_getgem(rtx), ptr, size); }
static HAWK_INLINE void* hawk_rtx_callocmem (hawk_rtx_t* rtx, hawk_oow_t size) { return hawk_gem_callocmem(hawk_rtx_getgem(rtx), size); }
static HAWK_INLINE void hawk_rtx_freemem (hawk_rtx_t* rtx, void* ptr) { hawk_gem_freemem (hawk_rtx_getgem(rtx), ptr); }
#else
#define hawk_rtx_allocmem(rtx, size) hawk_gem_allocmem(hawk_rtx_getgem(rtx), size)
#define hawk_rtx_reallocmem(rtx, ptr, size) hawk_gem_reallocmem(hawk_rtx_getgem(rtx), ptr, size)
#define hawk_rtx_callocmem(rtx, size) hawk_gem_callocmem(hawk_rtx_getgem(rtx), size)
#define hawk_rtx_freemem(rtx, ptr) hawk_gem_freemem(hawk_rtx_getgem(rtx), ptr)
#endif

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupucstr (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t* ucslen) { return hawk_gem_dupucstr(hawk_rtx_getgem(rtx), ucs, ucslen); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_dupbcstr (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t* bcslen) { return hawk_gem_dupbcstr(hawk_rtx_getgem(rtx), bcs, bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupuchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t ucslen) { return hawk_gem_dupuchars(hawk_rtx_getgem(rtx), ucs, ucslen); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_dupbchars (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t bcslen) { return hawk_gem_dupbchars(hawk_rtx_getgem(rtx), bcs, bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupucs (hawk_rtx_t* rtx, const hawk_ucs_t* ucs) { return hawk_gem_dupucs(hawk_rtx_getgem(rtx), ucs); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_dupbcs (hawk_rtx_t* rtx, const hawk_bcs_t* bcs) { return hawk_gem_dupbcs(hawk_rtx_getgem(rtx), bcs); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupucstrarr (hawk_rtx_t* rtx, const hawk_uch_t* strs[], hawk_oow_t* len) { return hawk_gem_dupucstrarr(hawk_rtx_getgem(rtx), strs, len); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_dupbcstrarr (hawk_rtx_t* rtx, const hawk_bch_t* strs[], hawk_oow_t* len) { return hawk_gem_dupbcstrarr(hawk_rtx_getgem(rtx), strs, len); }
#else
#define hawk_rtx_dupucstr(rtx, ucs, ucslen) hawk_gem_dupucstr(hawk_rtx_getgem(rtx), ucs, ucslen)
#define hawk_rtx_dupbcstr(rtx, bcs, bcslen) hawk_gem_dupbcstr(hawk_rtx_getgem(rtx), bcs, bcslen)
#define hawk_rtx_dupuchars(rtx, ucs, ucslen) hawk_gem_dupuchars(hawk_rtx_getgem(rtx), ucs, ucslen)
#define hawk_rtx_dupbchars(rtx, bcs, bcslen) hawk_gem_dupbchars(hawk_rtx_getgem(rtx), bcs, bcslen)
#define hawk_rtx_dupucs(rtx, ucs) hawk_gem_dupucs(hawk_rtx_getgem(rtx), ucs)
#define hawk_rtx_dupbcs(rtx, bcs) hawk_gem_dupbcs(hawk_rtx_getgem(rtx), bcs)
#define hawk_rtx_dupucstrarr(rtx, strs, len) hawk_gem_dupucstrarr(hawk_rtx_getgem(rtx), strs, len)
#define hawk_rtx_dupbcstrarr(rtx, strs, len) hawk_gem_dupbcstrarr(hawk_rtx_getgem(rtx), strs, len)
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_rtx_dupoocstr     hawk_rtx_dupucstr
#	define hawk_rtx_dupoochars    hawk_rtx_dupuchars
#	define hawk_rtx_dupoocs       hawk_rtx_dupucs
#	define hawk_rtx_dupoocstrarr  hawk_rtx_dupucstrarr
#else
#	define hawk_rtx_dupoocstr     hawk_rtx_dupbcstr
#	define hawk_rtx_dupoochars    hawk_rtx_dupbchars
#	define hawk_rtx_dupoocs       hawk_rtx_dupbcs
#	define hawk_rtx_dupoocstrarr  hawk_rtx_dupbcstrarr
#endif

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE int hawk_rtx_convbtouchars (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all) { return hawk_gem_convbtouchars(hawk_rtx_getgem(rtx), bcs, bcslen, ucs, ucslen, all); }
static HAWK_INLINE int hawk_rtx_convutobchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen) { return hawk_gem_convutobchars(hawk_rtx_getgem(rtx), ucs, ucslen, bcs, bcslen); }
static HAWK_INLINE int hawk_rtx_convbtoucstr (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t* bcslen, hawk_uch_t* ucs, hawk_oow_t* ucslen, int all) { return hawk_gem_convbtoucstr(hawk_rtx_getgem(rtx), bcs, bcslen, ucs, ucslen, all); }
static HAWK_INLINE int hawk_rtx_convutobcstr (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t* ucslen, hawk_bch_t* bcs, hawk_oow_t* bcslen) { return hawk_gem_convutobcstr(hawk_rtx_getgem(rtx), ucs, ucslen, bcs, bcslen); }
#else
#define hawk_rtx_convbtouchars(rtx, bcs, bcslen, ucs, ucslen, all) hawk_gem_convbtouchars(hawk_rtx_getgem(rtx), bcs, bcslen, ucs, ucslen, all)
#define hawk_rtx_convutobchars(rtx, ucs, ucslen, bcs, bcslen) hawk_gem_convutobchars(hawk_rtx_getgem(rtx), ucs, ucslen, bcs, bcslen)
#define hawk_rtx_convbtoucstr(rtx, bcs, bcslen, ucs, ucslen, all) hawk_gem_convbtoucstr(hawk_rtx_getgem(rtx), bcs, bcslen, ucs, ucslen, all)
#define hawk_rtx_convutobcstr(rtx, ucs, ucslen, bcs, bcslen) hawk_gem_convutobcstr(hawk_rtx_getgem(rtx), ucs, ucslen, bcs, bcslen)
#endif

/* ----------------------------------------------------------------------- */

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupbtouchars (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t _bcslen, hawk_oow_t* _ucslen, int all) { return hawk_gem_dupbtouchars(hawk_rtx_getgem(rtx), bcs, _bcslen, _ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_duputobchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t _ucslen, hawk_oow_t* _bcslen) { return hawk_gem_duputobchars(hawk_rtx_getgem(rtx), ucs, _ucslen, _bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupb2touchars (hawk_rtx_t* rtx, const hawk_bch_t* bcs1, hawk_oow_t bcslen1, const hawk_bch_t* bcs2, hawk_oow_t bcslen2, hawk_oow_t* ucslen, int all) { return hawk_gem_dupb2touchars(hawk_rtx_getgem(rtx), bcs1, bcslen1, bcs2, bcslen2, ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_dupu2tobchars (hawk_rtx_t* rtx, const hawk_uch_t* ucs1, hawk_oow_t ucslen1, const hawk_uch_t* ucs2, hawk_oow_t ucslen2, hawk_oow_t* bcslen) { return hawk_gem_dupu2tobchars(hawk_rtx_getgem(rtx), ucs1, ucslen1, ucs2, ucslen2, bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupbtoucstr (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t* _ucslen, int all) { return hawk_gem_dupbtoucstr(hawk_rtx_getgem(rtx), bcs, _ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_duputobcstr (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t* _bcslen) { return hawk_gem_duputobcstr(hawk_rtx_getgem(rtx), ucs, _bcslen); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupbtoucharswithcmgr (hawk_rtx_t* rtx, const hawk_bch_t* bcs, hawk_oow_t _bcslen, hawk_oow_t* _ucslen, hawk_cmgr_t* cmgr, int all) { return hawk_gem_dupbtoucharswithcmgr(hawk_rtx_getgem(rtx), bcs, _bcslen, _ucslen, cmgr, all); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_duputobcharswithcmgr (hawk_rtx_t* rtx, const hawk_uch_t* ucs, hawk_oow_t _ucslen, hawk_oow_t* _bcslen, hawk_cmgr_t* cmgr) { return hawk_gem_duputobcharswithcmgr(hawk_rtx_getgem(rtx), ucs, _ucslen, _bcslen, cmgr); }
static HAWK_INLINE hawk_uch_t* hawk_rtx_dupbcstrarrtoucstr (hawk_rtx_t* rtx, const hawk_bch_t* bcsarr[], hawk_oow_t* ucslen, int all) { return hawk_gem_dupbcstrarrtoucstr(hawk_rtx_getgem(rtx), bcsarr, ucslen, all); }
static HAWK_INLINE hawk_bch_t* hawk_rtx_dupucstrarrtobcstr (hawk_rtx_t* rtx, const hawk_uch_t* ucsarr[], hawk_oow_t* bcslen) { return hawk_gem_dupucstrarrtobcstr(hawk_rtx_getgem(rtx), ucsarr, bcslen); }
#else
#define hawk_rtx_dupbtouchars(rtx, bcs, _bcslen, _ucslen, all) hawk_gem_dupbtouchars(hawk_rtx_getgem(rtx), bcs, _bcslen, _ucslen, all)
#define hawk_rtx_duputobchars(rtx, ucs, _ucslen, _bcslen) hawk_gem_duputobchars(hawk_rtx_getgem(rtx), ucs, _ucslen, _bcslen)
#define hawk_rtx_dupb2touchars(rtx, bcs1, bcslen1, bcs2, bcslen2, ucslen, all) hawk_gem_dupb2touchars(hawk_rtx_getgem(rtx), bcs1, bcslen1, bcs2, bcslen2, ucslen, all)
#define hawk_rtx_dupu2tobchars(rtx, ucs1, ucslen1, ucs2, ucslen2, bcslen) hawk_gem_dupu2tobchars(hawk_rtx_getgem(rtx), ucs1, ucslen1, ucs2, ucslen2, bcslen)
#define hawk_rtx_dupbtoucstr(rtx, bcs, _ucslen, all) hawk_gem_dupbtoucstr(hawk_rtx_getgem(rtx), bcs, _ucslen, all)
#define hawk_rtx_duputobcstr(rtx, ucs, _bcslen) hawk_gem_duputobcstr(hawk_rtx_getgem(rtx), ucs, _bcslen)
#define hawk_rtx_dupbtoucharswithcmgr(rtx, bcs, _bcslen, _ucslen, cmgr, all) hawk_gem_dupbtoucharswithcmgr(hawk_rtx_getgem(rtx), bcs, _bcslen, _ucslen, cmgr, all)
#define hawk_rtx_duputobcharswithcmgr(rtx, ucs, _ucslen, _bcslen, cmgr) hawk_gem_duputobcharswithcmgr(hawk_rtx_getgem(rtx), ucs, _ucslen, _bcslen, cmgr)
#define hawk_rtx_dupbcstrarrtoucstr(rtx, bcsarr, ucslen, all) hawk_gem_dupbcstrarrtoucstr(hawk_rtx_getgem(hawk), bcsarr, ucslen, all)
#define hawk_rtx_dupucstrarrtobcstr(rtx, ucsarr, bcslen) hawk_gem_dupucstrarrtobcstr(hawk_rtx_getgem(rtx), ucsarr, bcslen)
#endif

/* ----------------------------------------------------------------------- */

HAWK_EXPORT hawk_oow_t hawk_rtx_fmttoucstr_ (
	hawk_rtx_t*       rtx,
	hawk_uch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_uch_t* fmt,
	...
);

HAWK_EXPORT hawk_oow_t hawk_rtx_fmttobcstr_ (
	hawk_rtx_t*       rtx,
	hawk_bch_t*       buf,
	hawk_oow_t        bufsz,
	const hawk_bch_t* fmt,
	...
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE hawk_oow_t hawk_rtx_vfmttoucstr (hawk_rtx_t* rtx, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, va_list ap) { return hawk_gem_vfmttoucstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap); }
static HAWK_INLINE hawk_oow_t hawk_rtx_fmttoucstr (hawk_rtx_t* rtx, hawk_uch_t* buf, hawk_oow_t bufsz, const hawk_uch_t* fmt, ...) { va_list ap; hawk_oow_t n; va_start(ap, fmt); n = hawk_gem_vfmttoucstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap); va_end(ap); return n; }
static HAWK_INLINE hawk_oow_t hawk_rtx_vfmttobcstr (hawk_rtx_t* rtx, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, va_list ap) { return hawk_gem_vfmttobcstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap); }
static HAWK_INLINE hawk_oow_t hawk_rtx_fmttobcstr (hawk_rtx_t* rtx, hawk_bch_t* buf, hawk_oow_t bufsz, const hawk_bch_t* fmt, ...) { va_list ap; hawk_oow_t n; va_start(ap, fmt); n = hawk_gem_vfmttobcstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap); va_end(ap); return n; }
#else
#define hawk_rtx_vfmttoucstr(rtx, buf, bufsz, fmt, ap) hawk_gem_vfmttoucstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap)
#define hawk_rtx_fmttoucstr hawk_rtx_fmttoucstr_
#define hawk_rtx_vfmttobcstr(rtx, buf, bufsz, fmt, ap) hawk_gem_vfmttobcstr(hawk_rtx_getgem(rtx), buf, bufsz, fmt, ap)
#define hawk_rtx_fmttobcstr hawk_rtx_fmttobcstr_
#endif

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_rtx_vfmttooocstr hawk_rtx_vfmttoucstr
#	define hawk_rtx_fmttooocstr hawk_rtx_fmttoucstr
#else
#	define hawk_rtx_vfmttooocstr hawk_rtx_vfmttobcstr
#	define hawk_rtx_fmttooocstr hawk_rtx_fmttobcstr
#endif

/* ----------------------------------------------------------------------- */


HAWK_EXPORT int hawk_rtx_buildrex (
	hawk_rtx_t*        rtx, 
	const hawk_ooch_t* ptn,
	hawk_oow_t         len,
	hawk_tre_t**       code, 
	hawk_tre_t**       icode
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void hawk_rtx_freerex (hawk_rtx_t* rtx, hawk_tre_t* code, hawk_tre_t* icode) { hawk_gem_freerex (hawk_rtx_getgem(rtx), code, icode); }
#else
#define hawk_rtx_freerex(rtx, code, icode) hawk_gem_freerex(hawk_rtx_getgem(rtx), code, icode)
#endif


/* ----------------------------------------------------------------------- */

/**
 * The hawk_get_nil_val() function returns the pointer to the predefined
 * nil value. you can call this without creating a runtime context. 
 */
HAWK_EXPORT hawk_val_t* hawk_get_nil_val (
	void
);

HAWK_EXPORT int hawk_get_val_type (
	hawk_val_t* val
);

#if defined(__cplusplus)
}
#endif

#endif
