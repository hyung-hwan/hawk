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

#ifndef _HAWK_SH_H_
#define _HAWK_SH_H_

/* Embeddable simple-command shell. */

#include <hawk-cmn.h>
#include <hawk-gem.h>

typedef struct hawk_sh_t hawk_sh_t;
typedef struct hawk_sh_cmd_t hawk_sh_cmd_t;
typedef struct hawk_sh_redir_t hawk_sh_redir_t;
typedef struct hawk_sh_pipeline_t hawk_sh_pipeline_t;

#define HAWK_SH_HDR \
	hawk_oow_t instsize_; \
	hawk_gem_t gem_

typedef struct hawk_sh_alt_t hawk_sh_alt_t;
struct hawk_sh_alt_t
{
	HAWK_SH_HDR;
};

enum hawk_sh_mode_t
{
	HAWK_SH_PATH,
	HAWK_SH_REGISTERED
};
typedef enum hawk_sh_mode_t hawk_sh_mode_t;

enum hawk_sh_redir_type_t
{
	HAWK_SH_IN,
	HAWK_SH_OUT,
	HAWK_SH_APPEND,
	HAWK_SH_INOUT,
	HAWK_SH_DUP,
	HAWK_SH_HEREDOC
};
typedef enum hawk_sh_redir_type_t hawk_sh_redir_type_t;

struct hawk_sh_redir_t
{
	hawk_sh_redir_t* next;
	int type, fd, strip_tabs, quoted;
	hawk_ooch_t* target;
	hawk_ooch_t* body; /* HEREDOC only */
};

struct hawk_sh_cmd_t
{
	hawk_sh_cmd_t* next;
	hawk_oow_t argx; /* number of argument slots allocated in argv */
	hawk_oow_t argc; /* number of arguments */
	hawk_ooch_t** argv; /* expanded, NULL terminated */
	hawk_sh_redir_t* redirs; /* lexical order matters */
};

struct hawk_sh_pipeline_t
{
	hawk_sh_cmd_t* commands;
	int background;
};

/* Input returns 1 (character), 0 (EOF), -1 (error). Command I/O uses bytes,
 * including in UCH builds. getenv returns -1 (error), 0 (unset), or 1 (set);
 * its output string is borrowed until the next call. */
typedef int (*hawk_sh_input_t) (
	hawk_sh_t*   sh,
	hawk_ooch_t* ch,
	void*        ctx
);

typedef int (*hawk_sh_cmd_impl_t) (
	hawk_sh_t*                sh,
	hawk_oow_t                argc,
	const hawk_ooch_t* const* argv,
	void*                     ctx
);

typedef struct hawk_sh_prm_t hawk_sh_prm_t;

struct hawk_sh_prm_t
{
	int (*exec) (
		hawk_sh_t*                sh,
		const hawk_sh_pipeline_t* pipeline,
		void*                     ctx
	); /* status or -1 */

	hawk_ooi_t (*read) (
		hawk_sh_t* sh,
		int        fd,
		void*      ptr,
		hawk_oow_t size,
		void*      ctx
	);

	hawk_ooi_t (*write) (
		hawk_sh_t*  sh,
		int         fd,
		const void* ptr,
		hawk_oow_t  size,
		void*       ctx
	);

	int (*getenv) (
		hawk_sh_t*          sh,
		const hawk_ooch_t*  name,
		const hawk_ooch_t** result,
		void*               ctx
	);
};

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT hawk_sh_t* hawk_sh_open (
	hawk_mmgr_t*   mmgr,
	hawk_oow_t     xtnsize,
	hawk_cmgr_t*   cmgr,
	hawk_errinf_t* errinf
);

HAWK_EXPORT void hawk_sh_close (
	hawk_sh_t*     sh
);

HAWK_EXPORT hawk_gem_t* hawk_sh_getgem (
	hawk_sh_t* sh
);

#define hawk_sh_getxtn(sh) ((void*)((hawk_uint8_t*)(sh) + ((hawk_sh_alt_t*)(sh))->instsize_))
#define hawk_sh_geterrnum(sh) hawk_gem_geterrnum(hawk_sh_getgem(sh))
#define hawk_sh_geterrbmsg(sh) hawk_gem_geterrbmsg(hawk_sh_getgem(sh))
#define hawk_sh_geterrloc(sh) hawk_gem_geterrloc(hawk_sh_getgem(sh))

HAWK_EXPORT void hawk_sh_setprm (
	hawk_sh_t*           sh,
	const hawk_sh_prm_t* prm,
	void*                ctx
);

HAWK_EXPORT void hawk_sh_setmode (
	hawk_sh_t*     sh,
	hawk_sh_mode_t mode
);

HAWK_EXPORT hawk_sh_mode_t hawk_sh_getmode (
	hawk_sh_t* sh
);

/* Specify a path OR a callback. Registration is an allowlist, not a sandbox. */
HAWK_EXPORT int hawk_sh_addcmd (
	hawk_sh_t*         sh,
	const hawk_ooch_t* name,
	const hawk_ooch_t* path,
	hawk_sh_cmd_impl_t impl,
	void*              ctx
);

HAWK_EXPORT int hawk_sh_delcmd (
	hawk_sh_t*         sh,
	const hawk_ooch_t* name
);

HAWK_EXPORT const hawk_ooch_t* hawk_sh_findcmd (
	hawk_sh_t*         sh,
	const hawk_ooch_t* name
);

HAWK_EXPORT int hawk_sh_isbuiltin (
	hawk_sh_t*         sh,
	const hawk_ooch_t* name
);

/* Core builtin/callback: 1 handled, 0 external, -1 error. */
HAWK_EXPORT int hawk_sh_call (
	hawk_sh_t*           sh,
	const hawk_sh_cmd_t* c,
	int*                 status
);

HAWK_EXPORT hawk_ooi_t hawk_sh_read (
	hawk_sh_t* sh,
	int        fd,
	void*      ptr,
	hawk_oow_t len
);

HAWK_EXPORT hawk_ooi_t hawk_sh_write (
	hawk_sh_t*  sh,
	int         fd,
	const void* ptr,
	hawk_oow_t  len
);

HAWK_EXPORT int hawk_sh_exec (
	hawk_sh_t*      sh,
	hawk_sh_input_t input,
	void*           ctx
);

HAWK_EXPORT int hawk_sh_execbcs (
	hawk_sh_t*        sh,
	const hawk_bch_t* s
);

HAWK_EXPORT int hawk_sh_execucs (
	hawk_sh_t*        sh,
	const hawk_uch_t* s
);

#if defined(HAWK_OOCH_IS_UCH)
#define hawk_sh_execoocs hawk_sh_execucs
#else
#define hawk_sh_execoocs hawk_sh_execbcs
#endif

/* POSIX backend; other platforms report ENOIMPL. Noninteractive instances
 * don't modify parent cwd/environment/signal handlers. Interactive mode owns
 * the terminal and signal dispositions: standalone main-thread use only. */
HAWK_EXPORT hawk_sh_t* hawk_sh_openstd (
	hawk_mmgr_t*   mmgr,
	hawk_oow_t     xtnsize,
	hawk_cmgr_t*   cmgr,
	hawk_errinf_t* errinf
);

HAWK_EXPORT int hawk_sh_setinteractivestd (
	hawk_sh_t* sh,
	int        enabled
);

HAWK_EXPORT int hawk_sh_execstd (
	hawk_sh_t*         sh,
	const hawk_ooch_t* path /**< NULL = stdin */
);


#if defined(__cplusplus)
}
#endif

#endif
