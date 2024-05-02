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

#ifndef _HAWK_SED_PRV_H_
#define _HAWK_SED_PRV_H_

#include <hawk-sed.h>
#include <hawk-ecs.h>

#define HAWK_MAP_IS_RBT
#include <hawk-map.h>

/* structure to maintain data to append
 * at the end of each cycle, triggered by r, R, and a */
typedef struct hawk_sed_app_t hawk_sed_app_t;
struct hawk_sed_app_t
{
	hawk_sed_cmd_t* cmd;
	hawk_sed_app_t* next;
};

typedef struct hawk_sed_cmd_blk_t hawk_sed_cmd_blk_t;
struct hawk_sed_cmd_blk_t
{
	hawk_oow_t         len;
	hawk_sed_cmd_t      buf[256];
	hawk_sed_cmd_blk_t* next;
};

/* structure to maintain list of compiliation
 * identifiers */
typedef struct hawk_sed_cid_t hawk_sed_cid_t;
struct hawk_sed_cid_t
{
	hawk_sed_cid_t* next;
};

/* special structure to represent an unknown cid
 * used once the action of setting a new cid fails */
typedef struct hawk_sed_unknown_cid_t hawk_sed_unknown_cid_t;
struct hawk_sed_unknown_cid_t
{
	hawk_sed_cid_t* next;
	hawk_ooch_t buf[1];
};

/**
 * The hawk_sed_t type defines a stream editor
 */
struct hawk_sed_t
{
	HAWK_SED_HDR;

	struct
	{
		int                   trait;
		hawk_sed_tracer_t     tracer;
		hawk_sed_lformatter_t lformatter;

		struct
		{
			struct
			{
				hawk_oow_t build;
				hawk_oow_t match;
			} rex;
		} depth; /* useful only for rex.h */
	} opt;

	hawk_sed_ecb_t* ecb;

	/** source text pointers */
	struct
	{
		hawk_sed_io_impl_t fun; /**< input stream handler */
		hawk_sed_io_arg_t  arg;
		hawk_ooch_t        buf[1024];
		int                eof;

		hawk_sed_cid_t*         cid;
		hawk_sed_unknown_cid_t  unknown_cid;

		hawk_loc_t         loc; /**< location */
		hawk_ooci_t        cc;  /**< last character read */
		const hawk_ooch_t* ptr; /**< beginning of the source text */
		const hawk_ooch_t* end; /**< end of the source text */
		const hawk_ooch_t* cur; /**< current source text pointer */
	} src;

	/** temporary data for compiling */
	struct
	{
		hawk_ooecs_t rex; /**< regular expression buffer */
		hawk_ooecs_t lab; /**< label name buffer */

		/** data structure to compile command groups */
		struct
		{
			/** current level of command group nesting */
			int level;
			/** keeps track of the begining of nested groups */
			hawk_sed_cmd_t* cmd[128];
		} grp;

		/** a table storing labels seen */
		hawk_map_t labs;
	} tmp;

	/** compiled commands */
	struct
	{
		hawk_sed_cmd_blk_t  fb; /**< the first block is static */
		hawk_sed_cmd_blk_t* lb; /**< points to the last block */

		hawk_sed_cmd_t      quit;
		hawk_sed_cmd_t      quit_quiet;
		hawk_sed_cmd_t      again;
		hawk_sed_cmd_t      over;
	} cmd;

	/** data for execution */
	struct
	{
		/** data needed for output streams and files */
		struct
		{
			hawk_sed_io_impl_t fun; /**< an output handler */
			hawk_sed_io_arg_t arg; /**< output handling data */

			hawk_ooch_t buf[2048];
			hawk_oow_t len;
			int        eof;

			/*****************************************************/
			/* the following two fields are very tightly-coupled.
			 * don't make any partial changes */
			hawk_map_t  files;
			hawk_sed_t* files_ext;
			/*****************************************************/
		} out;

		/** data needed for input streams */
		struct
		{
			hawk_sed_io_impl_t fun; /**< input handler */
			hawk_sed_io_arg_t arg; /**< input handling data */

			hawk_ooch_t xbuf[1]; /**< read-ahead buffer */
			int xbuf_len; /**< data length in the buffer */

			hawk_ooch_t buf[2048]; /**< input buffer */
			hawk_oow_t len; /**< data length in the buffer */
			hawk_oow_t pos; /**< current position in the buffer */
			int        eof; /**< EOF indicator */

			hawk_ooecs_t line; /**< pattern space */
			hawk_oow_t num; /**< current line number */
		} in;

		struct
		{
			hawk_oow_t count; /* number of append entries in a static buffer. */
			hawk_sed_app_t s[16]; /* handle up to 16 appends in a static buffer */
			struct
			{
				hawk_sed_app_t* head;
				hawk_sed_app_t* tail;
			} d;
		} append;

		/** text buffers */
		struct
		{
			hawk_ooecs_t hold; /* hold space */
			hawk_ooecs_t scratch;
		} txt;

		struct
		{
			hawk_oow_t  nflds; /**< the number of fields */
			hawk_oow_t  cflds; /**< capacity of flds field */
			hawk_oocs_t  sflds[128]; /**< static field buffer */
			hawk_oocs_t* flds;
			int delimited;
		} cutf;

		/** indicates if a successful substitution has been made
		 *  since the last read on the input stream. */
		int subst_done;
		void* last_rex;

		/** halt requested */
		int haltreq;
	} e;
};

#if defined(__cplusplus)
extern "C" {
#endif

int hawk_sed_init (
	hawk_sed_t*  sed,
	hawk_mmgr_t* mmgr,
	hawk_cmgr_t* cmgr
);

void hawk_sed_fini (
	hawk_sed_t* sed
);

#if defined(__cplusplus)
}
#endif

#endif
