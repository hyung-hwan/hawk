/*
 * $Id$
 *
    Copyright (c) 2006-2019 Chung, Hyung-Hwan. All rights reserved.

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

#include "mod-sys.h"
#include "hawk-prv.h"
#include <hawk-dir.h>

#if defined(_WIN32)
#	include <windows.h>
#	include <process.h>
#	include <tchar.h>
#elif defined(__OS2__)
#	define INCL_DOSPROCESS
#	define INCL_DOSEXCEPTIONS
#	define INCL_ERRORS
#	include <os2.h>
#elif defined(__DOS__)
#	include <dos.h>
#else
#	include "syscall.h"

#	if defined(HAVE_SYS_EPOLL_H)
#		include <sys/epoll.h>
#		if defined(HAVE_EPOLL_CREATE)
#			define USE_EPOLL
#		endif
#	endif

#	define ENABLE_SYSLOG
#	include <syslog.h>

#	include <sys/socket.h>

#endif

#include <stdlib.h> /* getenv, system */
#include <time.h>
#include <errno.h>
#include <string.h>

#define DEFAULT_MODE (0777)

#define CLOSE_KEEPFD (1 << 0)

/* these must not conflict with F_RDLCK, F_WRLCK, F_UNLCK */
#define FLOCK_GET (1 << 30)
#define FLOCK_WAIT (1 << 31)

/*
 * IMPLEMENTATION NOTE:
 *   - hard failure only if it cannot make a final return value. (e.g. fnc_errmsg, fnc_fork, fnc_getpid)
 *   - soft failure if it cannot make a value for pass-by-referece argument (e.g. fnc_pipe)
 *   - soft failure for all other types of errors
 */

/* ------------------------------------------------------------------------ */

typedef enum syslog_type_t syslog_type_t;
enum syslog_type_t
{
	SYSLOG_LOCAL,
	SYSLOG_REMOTE
};

struct mod_ctx_t
{
	hawk_rbt_t* rtxtab;

	struct
	{
		syslog_type_t type;
		char* ident;
		hawk_skad_t skad;
		int syslog_opened; // has openlog() been called?
		int opt;
		int fac;
		int sck;
		hawk_becs_t* dmsgbuf;
	} log;
};
typedef struct mod_ctx_t mod_ctx_t;

/* ------------------------------------------------------------------------ */

enum sys_node_data_type_t
{
	SYS_NODE_DATA_TYPE_FILE   = (1 << 0),
	SYS_NODE_DATA_TYPE_SCK  = (1 << 1),
	SYS_NODE_DATA_TYPE_DIR  = (1 << 2),
	SYS_NODE_DATA_TYPE_MUX  = (1 << 3)
};
typedef enum sys_node_data_type_t sys_node_data_type_t;

enum sys_node_data_flag_t
{
	SYS_NODE_DATA_FLAG_IN_MUX = (1 << 0)
};
typedef enum sys_node_data_flag_t sys_node_data_flag_t;


struct sys_node_data_file_t
{
	int fd;
	void* mux; /* if SYS_NODE_DATA_FLAG_IN_MUX is set, this is set to a valid pointer. it is of the void* type since sys_node_t is not available yet. */
	void* x_prev;
	void* x_next;
};
typedef struct sys_node_data_file_t sys_node_data_file_t;

struct sys_node_data_mux_t
{
#if defined(USE_EPOLL)
	int fd;
	struct epoll_event* x_evt;
#endif
	void* x_first;
	void* x_last;
	hawk_oow_t x_count;
	hawk_oow_t x_evt_max;
	hawk_oow_t x_evt_count;
};
typedef struct sys_node_data_mux_t sys_node_data_mux_t;

struct sys_node_data_t
{
	sys_node_data_type_t type;
	int flags;
	union
	{
		sys_node_data_file_t file;
		hawk_dir_t* dir;
		sys_node_data_mux_t mux;
	} u;
};
typedef struct sys_node_data_t sys_node_data_t;

struct sys_list_data_t
{
	hawk_ooch_t errmsg[256];
	hawk_bch_t* readbuf;
	hawk_oow_t readbuf_capa;
	hawk_ooch_t skadbuf[2][256];
};
typedef struct sys_list_data_t sys_list_data_t;

#define __IMAP_NODE_T_DATA sys_node_data_t ctx;
#define __IMAP_LIST_T_DATA sys_list_data_t ctx;
#define __IMAP_LIST_T sys_list_t
#define __IMAP_NODE_T sys_node_t
#define __MAKE_IMAP_NODE __new_sys_node
#define __FREE_IMAP_NODE __free_sys_node
#include "imap-imp.h"

struct rtx_data_t
{
	sys_list_t sys_list;
};
typedef struct rtx_data_t rtx_data_t;

enum mux_ctl_cmd_t
{
#if defined(USE_EPOLL)
	MUX_CTL_ADD = EPOLL_CTL_ADD,
	MUX_CTL_DEL = EPOLL_CTL_DEL,
	MUX_CTL_MOD = EPOLL_CTL_MOD
#else
	MUX_CTL_ADD,
	MUX_CTL_DEL,
	MUX_CTL_MOD
#endif
};
typedef enum mux_ctl_cmd_t mux_ctl_cmd_t;

enum mux_evt_code_t
{
#if defined(USE_EPOLL)
	MUX_EVT_IN  = EPOLLIN,
	MUX_EVT_OUT = EPOLLOUT,
	MUX_EVT_ERR = EPOLLERR,
	MUX_EVT_HUP = EPOLLHUP
#else
	MUX_EVT_IN  = (1 << 0),
	MUX_EVT_OUT = (1 << 1),
	MUX_EVT_ERR = (1 << 2),
	MUX_EVT_HUP = (1 << 3)
#endif
};

/* ------------------------------------------------------------------------ */

#define ERRNUM_TO_RC(errnum) (-((hawk_int_t)errnum))

static hawk_int_t copy_error_to_sys_list (hawk_rtx_t* rtx, sys_list_t* sys_list)
{
	hawk_errnum_t errnum = hawk_rtx_geterrnum(rtx);
	hawk_copy_oocstr (sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), hawk_rtx_geterrmsg(rtx));
	return ERRNUM_TO_RC(errnum);
}

static hawk_int_t set_error_on_sys_list (hawk_rtx_t* rtx, sys_list_t* sys_list, hawk_errnum_t errnum, const hawk_ooch_t* errfmt, ...)
{
	va_list ap;
	if (errfmt)
	{
		va_start (ap, errfmt);
		hawk_rtx_vfmttooocstr (rtx, sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_rtx_fmttooocstr (rtx, sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), HAWK_T("%js"), hawk_geterrstr(hawk_rtx_gethawk(rtx))(errnum));
	}
	return ERRNUM_TO_RC(errnum);
}

static hawk_int_t set_error_on_sys_list_with_errno (hawk_rtx_t* rtx, sys_list_t* sys_list, const hawk_ooch_t* title)
{
	int err = errno;

	if (title)
		hawk_rtx_fmttooocstr (rtx, sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), HAWK_T("%js - %hs"), title, strerror(err));
	else
		hawk_rtx_fmttooocstr (rtx, sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), HAWK_T("%hs"), strerror(err));
	return ERRNUM_TO_RC(hawk_syserr_to_errnum(err));
}

static void set_errmsg_on_sys_list (hawk_rtx_t* rtx, sys_list_t* sys_list, const hawk_ooch_t* errfmt, ...)
{
	if (errfmt)
	{
		va_list ap;
		va_start (ap, errfmt);
		hawk_rtx_vfmttooocstr (rtx, sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), errfmt, ap);
		va_end (ap);
	}
	else
	{
		hawk_copy_oocstr (sys_list->ctx.errmsg, HAWK_COUNTOF(sys_list->ctx.errmsg), hawk_rtx_geterrmsg(rtx));
	}
}

/* ------------------------------------------------------------------------ */

static sys_node_t* new_sys_node_fd (hawk_rtx_t* rtx, sys_list_t* list, int fd)
{
	sys_node_t* node;

	node = __new_sys_node(rtx, list);
	if (!node) return HAWK_NULL;

	node->ctx.type = SYS_NODE_DATA_TYPE_FILE;
	node->ctx.flags = 0;
	node->ctx.u.file.fd = fd;
	return node;
}

static sys_node_t* new_sys_node_dir (hawk_rtx_t* rtx, sys_list_t* list, hawk_dir_t* dir)
{
	sys_node_t* node;

	node = __new_sys_node(rtx, list);
	if (!node) return HAWK_NULL;

	node->ctx.type = SYS_NODE_DATA_TYPE_DIR;
	node->ctx.flags = 0;
	node->ctx.u.dir = dir;
	return node;
}

static sys_node_t* new_sys_node_mux (hawk_rtx_t* rtx, sys_list_t* list, int fd)
{
	sys_node_t* node;

	node = __new_sys_node(rtx, list);
	if (!node) return HAWK_NULL;

	node->ctx.type = SYS_NODE_DATA_TYPE_MUX;
	node->ctx.flags = 0;
#if defined(USE_EPOLL)
	node->ctx.u.mux.fd = fd;
#endif
	return node;
}

static void chain_sys_node_to_mux_node (sys_node_t* mux_node, sys_node_t* node)
{
	sys_node_data_mux_t* mux_data = &mux_node->ctx.u.mux;
	sys_node_data_file_t* file_data = &node->ctx.u.file;

	HAWK_ASSERT (!(node->ctx.flags & SYS_NODE_DATA_FLAG_IN_MUX));
	node->ctx.flags |= SYS_NODE_DATA_FLAG_IN_MUX;

	file_data->mux = mux_node;
	file_data->x_prev = HAWK_NULL;
	file_data->x_next = mux_data->x_first;
	if (mux_data->x_first) ((sys_node_t*)mux_data->x_first)->ctx.u.file.x_prev = node;
	else mux_data->x_last = node;
	mux_data->x_first = node;
	mux_data->x_count++;
}

static void unchain_sys_node_from_mux_node (sys_node_t* mux_node, sys_node_t* node)
{
	sys_node_data_mux_t* mux_data = &mux_node->ctx.u.mux;
	sys_node_data_file_t* file_data = &node->ctx.u.file;

	HAWK_ASSERT (node->ctx.flags & SYS_NODE_DATA_FLAG_IN_MUX);
	node->ctx.flags &= ~SYS_NODE_DATA_FLAG_IN_MUX;

	if (file_data->x_prev) ((sys_node_t*)file_data->x_prev)->ctx.u.file.x_next = file_data->x_next;
	else mux_data->x_first = file_data->x_next;
	if (file_data->x_next) ((sys_node_t*)file_data->x_next)->ctx.u.file.x_prev = file_data->x_prev;
	else mux_data->x_last = file_data->x_prev;
	mux_data->x_count--;

	file_data->mux = HAWK_NULL;
}

static void del_from_mux (hawk_rtx_t* rtx, sys_node_t* fd_node)
{
	if (fd_node->ctx.flags & SYS_NODE_DATA_FLAG_IN_MUX)
	{
		sys_node_t* mux_node;
	#if defined(USE_EPOLL)
		struct epoll_event ev;
	#endif

		switch (fd_node->ctx.type)
		{
			case SYS_NODE_DATA_TYPE_FILE:
			case SYS_NODE_DATA_TYPE_SCK:
			{
				hawk_oow_t i;
				sys_node_data_mux_t* mux_data;

				mux_node = (sys_node_t*)fd_node->ctx.u.file.mux;
				mux_data = &mux_node->ctx.u.mux;

				for (i = 0; i < mux_data->x_evt_count; i++)
				{
				#if defined(USE_EPOLL)
					if (mux_data->x_evt[i].data.ptr && fd_node->id == ((sys_node_t*)mux_data->x_evt[i].data.ptr)->id)
					{
						/* nullify the event in the event array to prevent normal access by sys::getmuxevt() */
						mux_data->x_evt[i].data.ptr = HAWK_NULL;
					}
				#endif
				}
			#if defined(USE_EPOLL)
				epoll_ctl (mux_node->ctx.u.mux.fd, MUX_CTL_DEL, fd_node->ctx.u.file.fd, &ev);
			#endif
				unchain_sys_node_from_mux_node (mux_node, fd_node);
				break;
			}

			default:
				/* do nothing */
				fd_node->ctx.flags &= ~SYS_NODE_DATA_FLAG_IN_MUX;
				break;
		}
	}
}

static void purge_mux_members (hawk_rtx_t* rtx, sys_node_t* mux_node)
{
	while (mux_node->ctx.u.mux.x_first)
	{
		del_from_mux (rtx, mux_node->ctx.u.mux.x_first);
	}
}

static void free_sys_node (hawk_rtx_t* rtx, sys_list_t* list, sys_node_t* node)
{
	switch (node->ctx.type)
	{
		case SYS_NODE_DATA_TYPE_FILE:
		case SYS_NODE_DATA_TYPE_SCK:
			if (node->ctx.u.file.fd >= 0) 
			{
				del_from_mux (rtx, node);
				close (node->ctx.u.file.fd);
				node->ctx.u.file.fd = -1;
			}
			break;

		case SYS_NODE_DATA_TYPE_DIR:
			if (node->ctx.u.dir)
			{
				hawk_dir_close (node->ctx.u.dir);
				node->ctx.u.dir = HAWK_NULL;
			}
			break;

		case SYS_NODE_DATA_TYPE_MUX:
		#if defined(USE_EPOLL)
			if (node->ctx.u.mux.fd >= 0)
			{
				/* TODO: delete all member FILE and SCK from mux */
				purge_mux_members (rtx, node);
				close (node->ctx.u.mux.fd);
				node->ctx.u.mux.fd = -1;

				if (node->ctx.u.mux.x_evt)
				{
					hawk_rtx_freemem (rtx, node->ctx.u.mux.x_evt);
					node->ctx.u.mux.x_evt = HAWK_NULL;
				}
				node->ctx.u.mux.x_evt_max = 0;
				node->ctx.u.mux.x_evt_count = 0;
			}
		#endif
			break;
	}
	__free_sys_node (rtx, list, node);
}

/* ------------------------------------------------------------------------ */

static HAWK_INLINE sys_list_t* rtx_to_sys_list (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	mod_ctx_t* mctx = (mod_ctx_t*)fi->mod->ctx;
	hawk_rbt_pair_t* pair;

	pair = hawk_rbt_search(mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx));
	HAWK_ASSERT (pair != HAWK_NULL);
	return (sys_list_t*)HAWK_RBT_VPTR(pair);
}

static HAWK_INLINE sys_node_t* get_sys_list_node (sys_list_t* sys_list, hawk_int_t id)
{
	if (id < 0 || id >= sys_list->map.high || !sys_list->map.tab[id]) return HAWK_NULL;
	return sys_list->map.tab[id];
}

/* ------------------------------------------------------------------------ */

static sys_node_t* get_sys_list_node_with_arg (hawk_rtx_t* rtx, sys_list_t* sys_list, hawk_val_t* arg, int node_type, hawk_int_t* rx)
{
	hawk_int_t id;
	sys_node_t* sys_node;

	if (hawk_rtx_valtoint(rtx, arg, &id) <= -1)
	{
		*rx = ERRNUM_TO_RC(hawk_rtx_geterrnum(rtx));
		set_errmsg_on_sys_list (rtx, sys_list, HAWK_T("illegal handle value"));
		return HAWK_NULL;
	}
	else if (!(sys_node = get_sys_list_node(sys_list, id)))
	{
		*rx = ERRNUM_TO_RC(HAWK_EINVAL);
		set_errmsg_on_sys_list (rtx, sys_list, HAWK_T("invalid handle - %zd"), (hawk_oow_t)id);
		return HAWK_NULL;
	}

	if (!(sys_node->ctx.type & node_type))
	{
		/* the handle is found but is not of the desired type */
		*rx = ERRNUM_TO_RC(HAWK_EINVAL);
		set_errmsg_on_sys_list (rtx, sys_list, HAWK_T("wrong handle type"));
		return HAWK_NULL;
	}

	return sys_node;
}

/* ------------------------------------------------------------------------ */

static int fnc_errmsg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* retv;

	sys_list = rtx_to_sys_list(rtx, fi);
	retv = hawk_rtx_makestrvalwithoocstr(rtx, sys_list->ctx.errmsg);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */

static int fnc_close (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);
	hawk_int_t cflags = 0;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);

	if (sys_node)
	{
		/* although free_sys_node can handle other types, sys::close() is allowed to
		 * close nodes of the SYS_NODE_DATA_TYPE_FILE type only */
		if (hawk_rtx_getnargs(rtx) >= 2 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &cflags) <= -1 || cflags < 0)) cflags = 0;

		if (cflags & CLOSE_KEEPFD)  /* this flag applies to file descriptors only */
		{
			sys_node->ctx.u.file.fd = -1; /* you may leak the original file descriptor. */
		}

		free_sys_node (rtx, sys_list, sys_node);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/*
  BEGIN {
     f = sys::open("/tmp/test.txt", sys::O_RDONLY);
     while (sys::read(f, x, 10) > 0) printf (B"%s", x);
     sys::close (f);
  }
*/

static int fnc_open (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	
	hawk_int_t rx, oflags = 0, mode = DEFAULT_MODE;
	int fd;
	hawk_bch_t* pstr;
	hawk_oow_t plen;
	hawk_val_t* a0;

	sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &oflags) <= -1 || oflags < 0) oflags = O_RDONLY;
	if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &mode) <= -1 || mode < 0)) mode = DEFAULT_MODE;

#if defined(O_LARGEFILE)
	oflags |= O_LARGEFILE;
#endif

	a0 = hawk_rtx_getarg(rtx, 0);
	pstr = hawk_rtx_getvalbcstr(rtx, a0, &plen);
	if (pstr)
	{
		fd = open(pstr, oflags, mode);
		hawk_rtx_freevalbcstr (rtx, a0, pstr);

		if (fd >= 0)
		{
			sys_node_t* new_node;

			new_node = new_sys_node_fd(rtx, sys_list, fd);
			if (!new_node) 
			{
				close (fd);
				goto fail;
			}
			rx = new_node->id;
			HAWK_ASSERT (rx >= 0);
		}
		else
		{
			rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_T("unable to open"));
		}
	}
	else
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
	}

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/*
	a = sys::openfd(1);
	sys::write (a, B"let me write something here\n");
	sys::close (a, sys::C_KEEPFD); ## set C_KEEPFD to release 1 without closing it.
	##sys::close (a);
	print "done\n";
*/

static int fnc_openfd (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* wrap a raw system file descriptor into the internal management node */

	sys_list_t* sys_list;
	hawk_int_t rx;
	hawk_int_t fd;

	sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &fd) <= -1)
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
	}
	else if (fd >= 0 && fd <= HAWK_TYPE_MAX(int))
	{
		sys_node_t* sys_node;

		sys_node = new_sys_node_fd(rtx, sys_list, fd);
		if (!sys_node) goto fail;

		rx = sys_node->id;
		HAWK_ASSERT (rx >= 0);
	}
	else
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_T("invalid file descriptor %jd"), (hawk_intmax_t)fd);
	}

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_read (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_int_t reqsize = 8192;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &reqsize) <= -1 || reqsize <= 0)) reqsize = 8192;
		if (reqsize > HAWK_QUICKINT_MAX) reqsize = HAWK_QUICKINT_MAX;

		if (reqsize > sys_list->ctx.readbuf_capa)
		{
			hawk_bch_t* tmp = hawk_rtx_reallocmem(rtx, sys_list->ctx.readbuf, reqsize);
			if (!tmp)
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}
			sys_list->ctx.readbuf = tmp;
			sys_list->ctx.readbuf_capa = reqsize;
		}

		rx = read(sys_node->ctx.u.file.fd, sys_list->ctx.readbuf, reqsize);
		if (rx <= 0) 
		{
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_T("unable to read"));
			goto done;
		}
		else
		{
			hawk_val_t* sv;
			int x;

			sv = hawk_rtx_makembsval(rtx, sys_list->ctx.readbuf, rx);
			if (!sv) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}

			hawk_rtx_refupval (rtx, sv);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), sv);
			hawk_rtx_refdownval (rtx, sv);
			if (x <= -1) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}
		}
	}

done:
	/* the value in 'rx' never exceeds HAWK_QUICKINT_MAX as 'reqsize' has been limited to
	 * it before the call to 'read'. so it's safe not to check the result of hawk_rtx_makeintval() */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_write (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_val_t* retv;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_bch_t* dptr;
		hawk_oow_t dlen;
		hawk_ooi_t startpos = 0, maxlen = HAWK_TYPE_MAX(hawk_ooi_t);
		hawk_val_t* a1;

		if (hawk_rtx_getnargs(rtx) >= 3 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &startpos) <= -1 || startpos < 0) startpos = 0;
		else if (startpos > 1) startpos--; /* this position is 1-based */

		if (hawk_rtx_getnargs(rtx) >= 4 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 3), &maxlen) <= -1) maxlen = HAWK_TYPE_MAX(hawk_ooi_t);
		else if (maxlen < 0) maxlen = 0;

		a1 = hawk_rtx_getarg(rtx, 1);
		dptr = hawk_rtx_getvalbcstr(rtx, a1, &dlen);
		if (dptr)
		{
			if (dlen > maxlen) dlen = maxlen;
			if (startpos >= dlen) startpos = dlen;
			dlen -= startpos;

			rx = write(sys_node->ctx.u.file.fd, &dptr[startpos], dlen);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_T("unable to write"));
			hawk_rtx_freevalbcstr (rtx, a1, dptr);
		}
		else
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
	}

	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1; /* hard failure. unable to make a return value */
	hawk_rtx_setretval (rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */

/*
	a = sys::open("/etc/inittab", sys::O_RDONLY);
	x = sys::open("/etc/fstab", sys::O_RDONLY);

	b = sys::dup(a);
	sys::close(a);

	while (sys::read(b, abc, 100) > 0) printf (B"%s", abc);

	print "-------------------------------";

	c = sys::dup(x, b, sys::O_CLOEXEC);
	## assertion: b == c
	sys::close (x);

	while (sys::read(c, abc, 100) > 0) printf (B"%s", abc);
	sys::close (c);
*/

static int fnc_dup (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node, * sys_node2 = HAWK_NULL;
	hawk_int_t rx;
	hawk_int_t oflags = 0;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		int fd;

		if (hawk_rtx_getnargs(rtx) >= 2)
		{
			sys_node2 = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 1), SYS_NODE_DATA_TYPE_FILE, &rx);
			if (!sys_node2) goto done;
			if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &oflags) <= -1 || oflags < 0)) oflags = 0;

			if (sys_node->ctx.u.file.fd == sys_node2->ctx.u.file.fd)
			{
				rx = set_error_on_sys_list(rtx, sys_list, HAWK_EPERM, HAWK_T("same descriptor"));
				goto done;
			}
		}

		if (sys_node2)
		{
		#if defined(HAVE_DUP3)
			fd = dup3(sys_node->ctx.u.file.fd, sys_node2->ctx.u.file.fd, oflags);
		#else
			fd = dup2(sys_node->ctx.u.file.fd, sys_node2->ctx.u.file.fd);
		#endif
			if (fd >= 0)
			{
		#if defined(HAVE_DUP3)
				/* nothing extra for dup3 */
		#else
				if (oflags)
				{
					int nflags = 0;
				#if defined(O_CLOEXEC) && defined(FD_CLOEXEC)
					if (oflags & O_CLOEXEC) nflags |= FD_CLOEXEC;
				#endif
				#if defined(O_NONBLOCK) && defined(FD_NONBLOCK)
					/*if (oflags & O_NONBLOCK) nflags |= FD_NONBLOCK;  dup3() doesn't seem to support NONBLOCK. */
				#endif
					if (nflags) fcntl (fd, F_SETFD, nflags);
				
				}
		#endif
				/* dup2 or dup3 closes the descriptor sys_node2_.ctx.u.file.fd implicitly
				 * if it's registered in muxtipler, unregister it as well */
				del_from_mux (rtx, sys_node2);
				sys_node2->ctx.u.file.fd = fd; 
				sys_node2->ctx.type = sys_node->ctx.type;
				rx = sys_node2->id;
			}
			else
			{
				rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			}
		}
		else
		{
			fd = dup(sys_node->ctx.u.file.fd);
			if (fd >= 0)
			{
				sys_node_t* new_node;

				new_node = new_sys_node_fd(rtx, sys_list, fd);
				if (new_node) 
				{
					new_node->ctx.type = sys_node->ctx.type;
					rx = new_node->id;
				}
				else 
				{
					close (fd);
					rx = copy_error_to_sys_list(rtx, sys_list);
				}
			}
			else
			{
				rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			}
		}
	}

done:
	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx)); /* assume a file descriptor never gets larger than HAWK_QUICKINT_MAX */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_fcntl (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_val_t* retv;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_int_t  cmd;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &cmd) <= -1)
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		switch (cmd)
		{
			case F_GETFD:
			case F_GETFL:
			{
				rx = fcntl(sys_node->ctx.u.file.fd, cmd, 0);
				if (rx <= -1) 
				{
					set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
					goto done;
				}
				break;
			}

			case F_SETFD:
			case F_SETFL:
			{
				hawk_int_t v = 0;
				if (hawk_rtx_getnargs(rtx) >= 3 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &v) <= -1) goto fail;
				rx = fcntl(sys_node->ctx.u.file.fd, cmd, v);
				if (rx <= -1) 
				{
					set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
					goto done;
				}
				break;
			}

			default:
				rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
				break;
		}
	}

done:
	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1; /* hard failure. unable to make a return value */

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

/*
  BEGIN {
        f1 = sys::open("/tmp/x", sys::O_RDWR | sys::O_CREAT | sys::O_TRUNC, 0644);
        if (sys::flock(f1, sys::FLOCK_WRITE | sys::FLOCK_GET, 0, 0, sys::SEEK_SET, pid) != sys::FLOCK_UNLOCK) print "locked by", pid;
        sys::flock(f1, sys::FLOCK_WRITE | sys::FLOCK_WAIT, 0, 0, sys::SEEK_SET);
        sys::sleep(ARGV[1]);
        sys::flock(f1, sys::FLOCK_UNLOCK | sys::FLOCK_WAIT, 0, 0, sys::SEEK_SET);
        sys::sleep(ARGV[1]);
        sys::close (f1);
 }
 */
static int fnc_flock (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_val_t* retv;

	HAWK_STATIC_ASSERT (FLOCK_GET != F_RDLCK);
	HAWK_STATIC_ASSERT (FLOCK_GET != F_WRLCK);
	HAWK_STATIC_ASSERT (FLOCK_GET != F_UNLCK);
	HAWK_STATIC_ASSERT (FLOCK_WAIT != F_RDLCK);
	HAWK_STATIC_ASSERT (FLOCK_WAIT != F_WRLCK);
	HAWK_STATIC_ASSERT (FLOCK_WAIT != F_UNLCK);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE, &rx);
	if (sys_node)
	{
		hawk_int_t type, start, len, whence;
		int wait, get;
		struct flock fl;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &type) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &start) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 3), &len) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 4), &whence) <= -1)
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		wait = !!(type & FLOCK_WAIT);
		get = !!(type & FLOCK_GET);
		type &= ~(FLOCK_WAIT | FLOCK_GET);

		HAWK_MEMSET (&fl, 0, HAWK_SIZEOF(fl));
		fl.l_type = type;
		fl.l_whence = whence;
		fl.l_start = start;
		fl.l_len = len;

		rx = fcntl(sys_node->ctx.u.file.fd, (get? F_GETLK: (wait? F_SETLKW: F_SETLK)), &fl);
		if (rx <= -1) 
		{
			set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			goto done;
		}

		if (get && hawk_rtx_getnargs(rtx) >= 6)
		{
			hawk_val_t* sv;
			int x;

			sv = hawk_rtx_makeintval(rtx, fl.l_pid);
			if (!sv) goto fail;

			hawk_rtx_refupval (rtx, sv);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 5), sv);
			hawk_rtx_refdownval (rtx, sv);
			if (x <= -1) goto fail;

			rx = fl.l_type; /* for get, it returns the lock type */
		}
	}

done:
	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1; /* hard failure. unable to make a return value */

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_fseek (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi) /* this is actually lseek */
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_val_t* retv;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE, &rx);
	if (sys_node)
	{
		hawk_int_t offset, whence;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &offset) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &whence) <= -1)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		rx = lseek(sys_node->ctx.u.file.fd, offset, whence);
		if (rx <= -1) 
		{
			set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			goto done;
		}
	}

done:
	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1; /* hard failure. unable to make a return value */

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

/* ------------------------------------------------------------------------ */

/*
	##if (sys::pipe(p0, p1) <= -1)
	if (sys::pipe(p0, p1, sys::O_NONBLOCK | sys::O_CLOEXEC) <= -1)
	{
		 print "pipe error";
		 return -1;
	}
	a = sys::fork();
	if (a <= -1)
	{
		print "fork error";
		sys::close (p0);
		sys::close (p1);
	}
	else if (a == 0)
	{
		## child
		printf ("child.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close (p1);
		while (1)
		{
			n = sys::read(p0, k, 3);
			if (n <= 0)
			{
				if (n == sys::RC_EAGAIN) continue; ## nonblock but data not available
				if (n != 0) print "ERROR: " sys::errmsg();
				break;
			}
			print k;
		}
		sys::close (p0);
	}
	else
	{
		## parent
		printf ("parent.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close (p0);
		sys::write (p1, B"hello");
		sys::write (p1, B"world");
		sys::close (p1);
		sys::wait(a);
	}##if (sys::pipe(p0, p1) <= -1)
	if (sys::pipe(p0, p1, sys::O_NONBLOCK | sys::O_CLOEXEC) <= -1)
	{
		 print "pipe error";
		 return -1;
	}
	a = sys::fork();
	if (a <= -1)
	{
		print "fork error";
		sys::close (p0);
		sys::close (p1);
	}
	else if (a == 0)
	{
		## child
		printf ("child.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close (p1);
		while (1)
		{
			n = sys::read (p0, k, 3);
			if (n <= 0)
			{
				if (n == sys::RC_EAGAIN) continue; ## nonblock but data not available
				if (n != 0) print "ERROR: " sys::errmsg();
				break;
			}
			print k;
		}
		sys::close (p0);
	}
	else
	{
		## parent
		printf ("parent.... %d %d %d\n", sys::getpid(), p0, p1);
		sys::close (p0);
		sys::write (p1, B"hello");
		sys::write (p1, B"world");
		sys::close (p1);
		sys::wait(a);
	}
*/

static int fnc_pipe (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	/* create low-level pipes */

	sys_list_t* sys_list;
	hawk_int_t rx;
	int fds[2];
	hawk_int_t flags = 0;

	sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &flags) <= -1 || flags < 0)) flags = 0;

#if defined(HAVE_PIPE2)
	if (pipe2(fds, flags) >= 0)
#else
	if (pipe(fds) >= 0)
#endif
	{
		sys_node_t* node1, * node2;

	#if defined(HAVE_PIPE2)
		/* do nothing extra */
	#else
		if (flags > 0)
		{
			int nflags = 0;

			/* needs translation from O_XXXX to FD_XXXX */
		#if defined(O_CLOEXEC) && defined(FD_CLOEXEC)
			if (flags & O_CLOEXEC) nflags |= FD_CLOEXEC;
		#endif
		#if defined(O_NONBLOCK) && defined(FD_NONBLOCK)
			if (flags & O_NONBLOCK) nflags |= FD_NONBLOCK;
		#endif

			if (nflags > 0)
			{
				/* don't care about failure */
				fcntl (fds[0], F_SETFD, nflags);
				fcntl (fds[1], F_SETFD, nflags);
			}
		}
	#endif
		node1 = new_sys_node_fd(rtx, sys_list, fds[0]);
		node2 = new_sys_node_fd(rtx, sys_list, fds[1]);
		if (node1 && node2)
		{
			hawk_val_t* v;
			int x;

			v = hawk_rtx_makeintval(rtx, node1->id);
			if (!v) goto fail;

			hawk_rtx_refupval (rtx, v);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 0), v);
			hawk_rtx_refdownval (rtx, v);
			if (x <= -1) goto fail;

			v = hawk_rtx_makeintval(rtx, node2->id);
			if (!v) goto fail;
			hawk_rtx_refupval (rtx, v);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), v);
			hawk_rtx_refdownval (rtx, v);
			if (x <= -1) goto fail;

			/* successful so far */
			rx =  ERRNUM_TO_RC(HAWK_ENOERR);
		}
		else
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);

			if (node2) free_sys_node (rtx, sys_list, node2);
			else close(fds[1]);
			if (node1) free_sys_node (rtx, sys_list, node1);
			else close(fds[0]);
		}
	}
	else
	{
		rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------------------ */

static int fnc_fchown (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE, &rx);
	if (sys_node)
	{
		hawk_int_t uid, gid;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &uid) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &gid) <= -1)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
		else
		{
			rx = fchown(sys_node->ctx.u.file.fd, uid, gid) <= -1?
				set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL):
				ERRNUM_TO_RC(HAWK_ENOERR);
		}
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_fchmod (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE, &rx);
	if (sys_node)
	{
		hawk_int_t mode;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &mode) <= -1)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
		else
		{
			rx = fchmod(sys_node->ctx.u.file.fd, mode) <= -1?
				set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL):
				ERRNUM_TO_RC(HAWK_ENOERR);
		}
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------------------ */

/*
        d = sys::opendir("/etc", sys::DIR_SORT);
        if (d >= 0)
        {
                while (sys::readdir(d,a) > 0) print a;
                sys::closedir(d);
        }

	################################################# 

	d = sys::opendir("/tmp");
	if (d <= -1) print "opendir error", sys::errmsg();
	for (i = 0; i < 10; i++) 
	{
		sys::readdir(d, a);
	       	print "[" a "]";
	}
	print "---";
	if (sys::resetdir(d, "/dev/mapper/fedora-root") <= -1) 
	{ 
		print "reset failure:", sys::errmsg();
	} 
	while (sys::readdir(d, a) > 0) print "[" a "]"; 
	sys::closedir(d);
*/

static int fnc_opendir (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node = HAWK_NULL;
	hawk_int_t flags = 0;
	hawk_ooch_t* pstr;
	hawk_oow_t plen;
	hawk_val_t* a0;
	hawk_dir_t* dir;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_EOTHER);

	sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_getnargs(rtx) >= 2 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &flags) <= -1 || flags < 0)) flags = 0;

	a0 = hawk_rtx_getarg(rtx, 0);
	pstr = hawk_rtx_getvaloocstr(rtx, a0, &plen);
	if (!pstr) goto fail;
	dir = hawk_dir_open(hawk_rtx_getgem(rtx), 0, pstr, flags);
	hawk_rtx_freevaloocstr (rtx, a0, pstr);

	if (dir)
	{
		sys_node = new_sys_node_dir(rtx, sys_list, dir);
		if (sys_node) 
		{
			rx = sys_node->id;
		}
		else 
		{
			hawk_dir_close(dir);
			goto fail;
		}
	}
	else
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
	}

	/*HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));*/
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_closedir (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_DIR, &rx);
	if (sys_node)
	{
		/* although free_sys_node() can handle other types, sys::closedir() is allowed to
		 * close nodes of the SYS_NODE_DATA_TYPE_DIR type only */
		free_sys_node (rtx, sys_list, sys_node);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_readdir (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = 0; /* no more entry */

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_DIR, &rx);
	if (sys_node)
	{
		hawk_dir_ent_t ent;

		rx = hawk_dir_read(sys_node->ctx.u.dir, &ent); /* assume -1 on error, 0 on no more entry, 1 when an entry is available */
		if (rx <= -1)
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
		else if (rx > 0)
		{
			hawk_val_t* tmp;
			int x;

			tmp = hawk_rtx_makestrvalwithoocstr(rtx, ent.name);
			if (!tmp) goto fail;

			hawk_rtx_refupval (rtx, tmp);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), tmp);
			hawk_rtx_refdownval (rtx, tmp);
			if (x <= -1) goto fail;
		}
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_resetdir (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_DIR, &rx);

	if (sys_node)
	{
		hawk_ooch_t* path;
		hawk_val_t* a1;

		a1 = hawk_rtx_getarg(rtx, 1);
		path = hawk_rtx_getvaloocstr(rtx, a1, HAWK_NULL);
		if (path)
		{
			if (hawk_dir_reset(sys_node->ctx.u.dir, path) <= -1) goto fail;
			hawk_rtx_freevaloocstr (rtx, a1, path);
		}
		else
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
	}

	/* no error check for hawk_rtx_makeintval() here since ret 
	 * is 0 or -1. it will never fail for those numbers */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------------------ */

static int fnc_fork (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
	rx = fork();
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1; /* hard failure. unable to create a return value */

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_wait (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t pid;
	hawk_val_t* retv;
	hawk_int_t rx;
	hawk_oow_t nargs;
	hawk_int_t opts = 0;
	int status;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

	nargs = hawk_rtx_getnargs(rtx);
	if (nargs >= 3 && hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &opts) <= -1) goto fail;
	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &pid) <= -1) goto fail;
	
#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	status = 0;
#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	status = 0;
#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	status = 0;
#else
	rx = waitpid(pid, &status, opts);
	if (rx <= -1) 
	{
		rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}
	else
	{
		if (nargs >= 2)
		{
			hawk_val_t* sv;
			int x;

			sv = hawk_rtx_makeintval(rtx, status);
			if (!sv) goto fail;

			hawk_rtx_refupval (rtx, sv);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), sv);
			hawk_rtx_refdownval (rtx, sv);
			if (x <= -1) 
			{
			fail:
				rx = copy_error_to_sys_list(rtx, sys_list);
			}
		}
	}
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1; /* hard failure. unable to make a return value */

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_wifexited (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t wstatus;
	int rv;
	rv = (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &wstatus) <= -1)? 0: !!WIFEXITED(wstatus);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rv));
	return 0;
}

static int fnc_wexitstatus (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t wstatus;
	int rv;

	hawk_val_t* retv;
	rv = (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &wstatus) <= -1)? -1: WEXITSTATUS(wstatus);

	retv = hawk_rtx_makeintval(rtx, rv);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_wifsignaled (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t wstatus;
	int rv;
	rv = (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &wstatus) <= -1)? 0: !!WIFSIGNALED(wstatus);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rv));
	return 0;
}

static int fnc_wtermsig (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t wstatus;
	int rv;

	hawk_val_t* retv;
	rv = (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &wstatus) <= -1)? -1: WTERMSIG(wstatus);

	retv = hawk_rtx_makeintval(rtx, rv);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_wcoredump (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t wstatus;
	int rv;
	rv = hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &wstatus) <= -1? 0: !!WCOREDUMP(wstatus);
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rv));
	return 0;
}

static int fnc_kill (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t pid, sig;
	hawk_val_t* retv;
	hawk_int_t rx;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &pid) <= -1 ||
	    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &sig) <= -1)
	{
		rx = copy_error_to_sys_list(rtx, sys_list);
	}
	else
	{
#if defined(_WIN32)
		/* TOOD: implement this*/
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
		/* TOOD: implement this*/
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
		/* TOOD: implement this*/
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
		rx = kill(pid, sig);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
#endif
	}

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_getpgid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
	/* TODO: support specifing calling process id other than 0 */
	#if defined(HAVE_GETPGID)
	rx = getpgid(0);
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	#elif defined(HAVE_GETPGRP)
	rx = getpgrp();
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	#else
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	#endif
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_getpid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	rx = GetCurrentProcessId();
	/* never fails */

#elif defined(__OS2__)
	PTIB tib;
	PPIB pib;
	APIRET rc;

	rc = DosGetInfoBlocks(&tib, &pib);
	if (rc == NO_ERROR)
	{
		rx = pib->pib_ulpid;
	}
	else
	{
		rx = set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(rc), HAWK_NULL);
	}

#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	rx = getpid ();
	/* getpid() never fails */
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_gettid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_intptr_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	rx = GetCurrentThreadId();
	/* never fails */
#elif defined(__OS2__)
	PTIB tib;
	PPIB pib;
	APIRET rc;

	rc = DosGetInfoBlocks(&tib, &pib);
	if (rc == NO_ERROR)
	{
		if (tib->tib_ptib2) rx = tib->tib_ptib2->tib2_ultid;
		else rx = set_error_on_sys_list(rtx, sys_list, HAWK_ESYSERR, HAWK_NULL);
	}
	else
	{
		rx = set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(rc), HAWK_NULL);
	}

#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	#if defined(SYS_gettid) && defined(HAWK_SYSCALL0)
	HAWK_SYSCALL0 (rx, SYS_gettid);
	#elif defined(SYS_gettid)
	rx = syscall(SYS_gettid);
	#else
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	#endif
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_getppid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	DWORD pid;
	HANDLE ps;
	PROCESSENTRY32 p;

	pid = GetCurrentPorcessId();
	ps = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (ps == INVALID_HANDLE_VALUE)
	{
		rx = set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(GetLastError()), HAWK_NULL);
	}
	else
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOENT, HAWK_NULL);

		p.dwSize = HAWK_SZIEOF(p);
		if (Process32First(ps, &p))
		{
			do
			{
				if (p.th32ProcessID == pid)
				{
					rx = p.th32ParentProcessID; /* got it */
					break;
				}
			}
			while (Process32Next(ps, &p));
		}

		CloseHandle (ps);
	}

#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	
#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	rx = getppid();
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_getuid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	
#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	
#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	rx = getuid();
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_getgid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	rx = getgid();
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_geteuid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	
#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	
#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	rx = geteuid();
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_getegid (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx;
	hawk_val_t* retv;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

#if defined(_WIN32)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
	
#elif defined(__OS2__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#elif defined(__DOS__)
	/* TOOD: implement this*/
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);

#else
	rx = getegid();
#endif

	retv = hawk_rtx_makeintval(rtx, rx);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int val_to_ntime (hawk_rtx_t* rtx, hawk_val_t* val, hawk_ntime_t* nt)
{
	hawk_int_t lv;
	hawk_flt_t fv;
	int x;

	x = hawk_rtx_valtonum(rtx, val, &lv, &fv);
	if (x == 0)
	{
		nt->sec = lv;
		nt->nsec = 0;
	}
	else if (x >= 1)
	{
		nt->sec = (hawk_int_t)fv;
		nt->nsec = HAWK_SEC_TO_NSEC(fv - nt->sec);
	}

	return x;
}

static int fnc_sleep (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_ntime_t nt;
	hawk_val_t* retv;
	hawk_int_t rx;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

	rx = val_to_ntime(rtx, hawk_rtx_getarg(rtx, 0), &nt);
	if (rx <= -1)
	{
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

#if defined(_WIN32)
	Sleep (HAWK_SECNSEC_TO_MSEC(nt.sec, nt.nsec));
	rx = 0;
#elif defined(__OS2__)
	DosSleep ((ULONG)HAWK_SECNSEC_TO_MSEC(nt.sec, nt.nsec)));
	rx = 0;
#elif defined(__DOS__)
	/* no high-resolution sleep() is available */
	#if (defined(__WATCOMC__) && (__WATCOMC__ < 1200))
	sleep (nt.sec);
	rx = 0;
	#else
	rx = sleep(nt.sec);
	#endif;
#elif defined(HAVE_NANOSLEEP)
	struct timespec req;
	req.tv_sec = nt.sec;
	req.tv_nsec = nt.nsec;
	rx = nanosleep(&req, HAWK_NULL);
#elif defined(HAVE_SELECT)
	struct timeval req;
	req.tv_sec = nt.sec;
	req.tv_usec = HAWK_NSEC_TO_USEC(nt.nsec);
	rx = select(0, HAWK_NULL, HAWK_NULL, HAWK_NULL, &req);
#else
	/* no high-resolution sleep() is available */
	rx = sleep(nt.sec);
#endif

done:
	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_gettime (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* retv;
	hawk_ntime_t now;

	if (hawk_get_time (&now) <= -1) now.sec = 0;

	retv = hawk_rtx_makeintval(rtx, now.sec);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_settime (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_val_t* retv;
	hawk_ntime_t now;
	hawk_int_t tmp;
	hawk_int_t rx;

	now.nsec = 0;

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg (rtx, 0), &tmp) <= -1) rx = -1;
	else
	{
		now.sec = tmp;
		if (hawk_set_time(&now) <= -1) rx = -1;
		else rx = 0;
	}

	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}

static int fnc_mktime (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_ntime_t nt;
	hawk_oow_t nargs;
	hawk_val_t* retv;

	nargs = hawk_rtx_getnargs(rtx);
	if (nargs >= 1)
	{
		int sign;
		hawk_ooch_t* str, * p, * end;
		hawk_oow_t len;
		hawk_val_t* a0;
		struct tm tm;

		a0 = hawk_rtx_getarg(rtx, 0);
		str = hawk_rtx_getvaloocstr(rtx, a0, &len);
		if (str == HAWK_NULL) return -1;

		/* the string must be of the format  YYYY MM DD HH MM SS[ DST] */
		p = str;
		end = str + len;
		HAWK_MEMSET (&tm, 0, HAWK_SIZEOF(tm));

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_year = tm.tm_year * 10 + (*p++ - '0');
		tm.tm_year *= sign;
		tm.tm_year -= 1900;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_mon = tm.tm_mon * 10 + (*p++ - '0');
		tm.tm_mon *= sign;
		tm.tm_mon -= 1;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_mday = tm.tm_mday * 10 + (*p++ - '0');
		tm.tm_mday *= sign;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_hour = tm.tm_hour * 10 + (*p++ - '0');
		tm.tm_hour *= sign;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_min = tm.tm_min * 10 + (*p++ - '0');
		tm.tm_min *= sign;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_sec = tm.tm_sec * 10 + (*p++ - '0');
		tm.tm_sec *= sign;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		sign = 1;
		if (p < end && *p == '-') { sign = -1; p++; }
		while (p < end && hawk_is_ooch_digit(*p)) tm.tm_isdst = tm.tm_isdst * 10 + (*p++ - '0');
		tm.tm_isdst *= sign;
		while (p < end && (hawk_is_ooch_space(*p) || *p == '\0')) p++;

		hawk_rtx_freevaloocstr (rtx, a0, str);
	#if defined(HAVE_TIMELOCAL)
		nt.sec = timelocal(&tm);
	#else
		nt.sec = mktime(&tm);
	#endif
	}
	else
	{
		/* get the current time when no argument is given */
		hawk_get_time (&nt);
	}

	retv = hawk_rtx_makeintval(rtx, nt.sec);
	if (retv == HAWK_NULL) return -1;

	hawk_rtx_setretval (rtx, retv);
	return 0;
}


#define STRFTIME_UTC (1 << 0)

static int fnc_strftime (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{

	/*
	sys::strftime("%Y-%m-%d %H:%M:%S %z", sys::gettime()); 
	sys::strftime("%Y-%m-%d %H:%M:%S %z", sys::gettime(), sys::STRFTIME_UTC);
	*/

	hawk_bch_t* fmt;
	hawk_oow_t len;
	hawk_val_t* retv;

	fmt = hawk_rtx_valtobcstrdup(rtx, hawk_rtx_getarg(rtx, 0), &len);
	if (fmt) 
	{
		hawk_ntime_t nt;
		struct tm tm, * tmx;
		hawk_int_t tmpsec, flags = 0;

		nt.nsec = 0;
		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &tmpsec) <= -1) 
		{
			nt.sec = 0;
		}
		else
		{
			nt.sec = tmpsec;
		}

		if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &flags) <= -1 || flags < 0)) flags = 0;

		if (flags & STRFTIME_UTC)
		{
			time_t t = nt.sec;
		#if defined(HAVE_GMTIME_R)
			tmx = gmtime_r(&t, &tm);
		#else
			tmx = gmtime(&t.sec);
		#endif
		}
		else
		{
			time_t t = nt.sec;
		#if defined(HAVE_LOCALTIME_R)
			tmx = localtime_r(&t, &tm);
		#else
			tmx = localtime(&t.sec);
		#endif
		}

		if (tmx)
		{
			hawk_bch_t tmpbuf[64], * tmpptr;
			hawk_oow_t sl;

#if 0
			if (flags & STRFTIME_UTC)
			{
			#if defined(HAVE_STRUCT_TM_TM_ZONE)
				tm.tm_zone = "GMT";
			#elif defined(HAVE_STRUCT_TM___TM_ZONE)
				tm.__tm_zone = "GMT";
			#endif
			}
#endif

			sl = strftime(tmpbuf, HAWK_COUNTOF(tmpbuf), fmt, tmx);
			if (sl <= 0 || sl >= HAWK_COUNTOF(tmpbuf))
			{
				/* buffer too small */
				hawk_bch_t* tmp;
				hawk_oow_t tmpcapa, i, count = 0;

/*
man strftime >>>

RETURN VALUE
       The strftime() function returns the number of bytes placed in the array s, not including the  terminating  null  byte,  provided  the
       string,  including  the  terminating  null  byte,  fits.  Otherwise, it returns 0, and the contents of the array is undefined.  (This
       behavior applies since at least libc 4.4.4; very old versions of libc, such as libc 4.4.1, would return max  if  the  array  was  too
       small.)

       Note that the return value 0 does not necessarily indicate an error; for example, in many locales %p yields an empty string.
 
--------------------------------------------------------------------------------------
* 
I use 'count' to limit the maximum number of retries when 0 is returned.
*/

				for (i = 0; i < len;)
				{
					if (fmt[i] == HAWK_BT('%')) 
					{
						count++; /* the nubmer of % specifier */
						i++;
						if (i < len) i++;
					}
					else i++;
				}

				tmpptr = HAWK_NULL;
				tmpcapa = HAWK_COUNTOF(tmpbuf);
				if (tmpcapa < len) tmpcapa = len;

				do
				{
					if (count <= 0) 
					{
						if (tmpptr) hawk_rtx_freemem (rtx, tmpptr);
						tmpbuf[0] = HAWK_BT('\0');
						tmpptr = tmpbuf;
						break;
					}
					count--;

					tmpcapa *= 2;
					tmp = (hawk_bch_t*)hawk_rtx_reallocmem(rtx, tmpptr, tmpcapa * HAWK_SIZEOF(*tmpptr));
					if (!tmp) 
					{
						if (tmpptr) hawk_rtx_freemem (rtx, tmpptr);
						tmpbuf[0] = HAWK_BT('\0');
						tmpptr = tmpbuf;
						break;
					}

					tmpptr = tmp;
					sl = strftime(tmpptr, tmpcapa, fmt, &tm);
				}
				while (sl <= 0 || sl >= tmpcapa);
			}
			else
			{
				tmpptr = tmpbuf;
			}
			hawk_rtx_freemem (rtx, fmt);

			retv = hawk_rtx_makestrvalwithbcstr(rtx, tmpptr);
			if (tmpptr && tmpptr != tmpbuf) hawk_rtx_freemem (rtx, tmpptr);
			if (retv == HAWK_NULL) return -1;

			hawk_rtx_setretval (rtx, retv);
		}
		else
		{
			hawk_rtx_freemem (rtx, fmt);
		}
	}

	return 0;
}

/*
	if (sys::getenv("PATH", v) <= -1) print "error -", sys::errmsg();
	else print v;
*/
static int fnc_getenv (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_bch_t* var;
	hawk_oow_t len;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);

	a0 = hawk_rtx_getarg(rtx, 0);
	var = hawk_rtx_getvalbcstr(rtx, a0, &len);
	if (var)
	{
		hawk_bch_t* val;

		if (hawk_find_bchar(var, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			hawk_rtx_freevalbcstr (rtx, a0, var);
		}
		else
		{
			val = getenv(var);
			hawk_rtx_freevalbcstr (rtx, a0, var);

			if (val) 
			{
				hawk_val_t* tmp;
				int x;

				tmp = hawk_rtx_makestrvalwithbcstr(rtx, val);
				if (!tmp) goto fail;

				hawk_rtx_refupval (rtx, tmp);
				x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), tmp);
				hawk_rtx_refdownval (rtx, tmp);
				if (x <= -1) goto fail;

				rx = ERRNUM_TO_RC(HAWK_ENOERR);
			}
			else
			{
				rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOENT, HAWK_NULL);
			}
		}
	}
	else
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_setenv (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0, * a1;
	hawk_bch_t* var = HAWK_NULL, * val = HAWK_NULL;
	hawk_oow_t var_len, val_len;
	hawk_int_t rx;
	hawk_int_t overwrite = 1;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);
	a1 = hawk_rtx_getarg(rtx, 1);

	var = hawk_rtx_getvalbcstr(rtx, a0, &var_len);
	val = hawk_rtx_getvalbcstr(rtx, a1, &val_len);
	if (!var || !val)
	{
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

	/* the target name contains a null character. */
	if (hawk_find_bchar(var, var_len, '\0') ||
	    hawk_find_bchar(val, val_len, '\0'))
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
		goto done;
	}

	if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &overwrite) <= -1)) overwrite = 0;

	rx = setenv(var, val, overwrite);
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);

done:
	if (val) hawk_rtx_freevalbcstr (rtx, a1, val);
	if (var) hawk_rtx_freevalbcstr (rtx, a0, var);

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_unsetenv (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_bch_t* str;
	hawk_oow_t len;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

	str = hawk_rtx_getvalbcstr(rtx, a0, &len);
	if (!str)
	{
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

	/* the target name contains a null character. */
	if (hawk_find_bchar(str, len, '\0'))
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
		goto done;
	}

	rx = unsetenv(str);
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);

done:
	if (str) hawk_rtx_freevalbcstr (rtx, a0, str);

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------ */

/*
	if (sys::getnwifcfg("eth0", sys::NWIFCFG_IN6, x) >= 0) 
	{ 
	    for (i in x) print i, x[i]; 
	}
	else 
	{
	    print "Error:", sys::errmsg();
	}
*/
static int fnc_getifcfg (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_ifcfg_t cfg;
	hawk_rtx_valtostr_out_t out;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);

	HAWK_MEMSET (&cfg, 0, HAWK_SIZEOF(cfg));

	out.type = HAWK_RTX_VALTOSTR_CPLCPY;
	out.u.cplcpy.ptr = cfg.name;
	out.u.cplcpy.len = HAWK_COUNTOF(cfg.name);
	if (hawk_rtx_valtostr(rtx, hawk_rtx_getarg(rtx, 0), &out) >= 0)
	{
		hawk_int_t type;
		hawk_int_t index, mtu;
		hawk_ooch_t ethw[32];
		hawk_val_map_data_t md[7];
		hawk_val_t* tmp;
		int x;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &type) <= -1) goto fail;

		cfg.type = type;
		if (hawk_gem_getifcfg(hawk_rtx_getgem(rtx), &cfg) <= -1) goto fail;
		
		/* make a map value containg configuration */
		HAWK_MEMSET (md, 0, HAWK_SIZEOF(md));

		md[0].key.ptr = HAWK_T("index");
		md[0].key.len = 5;
		md[0].type = HAWK_VAL_MAP_DATA_INT;
		index = cfg.index;
		md[0].vptr = &index;

		md[1].key.ptr = HAWK_T("mtu");
		md[1].key.len = 3;
		md[1].type = HAWK_VAL_MAP_DATA_INT;
		mtu = cfg.mtu;
		md[1].vptr = &mtu;

		md[2].key.ptr = HAWK_T("addr");
		md[2].key.len = 4;
		md[2].type = HAWK_VAL_MAP_DATA_STR;
		hawk_gem_skadtooocstr (hawk_rtx_getgem(rtx), &cfg.addr, sys_list->ctx.skadbuf[0], HAWK_COUNTOF(sys_list->ctx.skadbuf[0]), HAWK_SKAD_TO_OOCSTR_ADDR);
		md[2].vptr = sys_list->ctx.skadbuf[0];

		md[3].key.ptr = HAWK_T("mask");
		md[3].key.len = 4;
		md[3].type = HAWK_VAL_MAP_DATA_STR;
		hawk_gem_skadtooocstr (hawk_rtx_getgem(rtx), &cfg.mask, sys_list->ctx.skadbuf[1], HAWK_COUNTOF(sys_list->ctx.skadbuf[1]), HAWK_SKAD_TO_OOCSTR_ADDR);
		md[3].vptr = sys_list->ctx.skadbuf[1];

		md[4].key.ptr = HAWK_T("ethw");
		md[4].key.len = 4;
		md[4].type = HAWK_VAL_MAP_DATA_STR;
		hawk_rtx_fmttooocstr (rtx, ethw, HAWK_COUNTOF(ethw), HAWK_T("%02X:%02X:%02X:%02X:%02X:%02X"), 
			cfg.ethw[0], cfg.ethw[1], cfg.ethw[2], cfg.ethw[3], cfg.ethw[4], cfg.ethw[5]);
		md[4].vptr = ethw;

		if (cfg.flags & (HAWK_IFCFG_LINKUP | HAWK_IFCFG_LINKDOWN))
		{
			md[5].key.ptr = HAWK_T("link");
			md[5].key.len = 4;
			md[5].type = HAWK_VAL_MAP_DATA_STR;
			md[5].vptr = (cfg.flags & HAWK_IFCFG_LINKUP)? HAWK_T("up"): HAWK_T("down");
		}

		tmp = hawk_rtx_makemapvalwithdata(rtx, md);
		if (!tmp) goto fail;

		hawk_rtx_refupval (rtx, tmp);
		x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), tmp);
		hawk_rtx_refdownval (rtx, tmp);
		if (x <= -1) goto fail;

		rx = ERRNUM_TO_RC(HAWK_ENOERR);
	}
	else
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
	}

	/* no error check for hawk_rtx_makeintval() since ret is 0 or -1 */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}
/* ------------------------------------------------------------ */

static int fnc_system (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_ooch_t* str;
	hawk_oow_t len;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);

	a0 = hawk_rtx_getarg(rtx, 0);
	str = hawk_rtx_getvaloocstr(rtx, a0, &len);
	if (!str)
	{
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

	/* the target name contains a null character. make system return -1 */
	if (hawk_find_oochar(str, len, '\0'))
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
		goto done;
	}

#if defined(_WIN32)
	rx = _tsystem(str);
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
#elif defined(HAWK_OOCH_IS_BCH)
	rx = system(str);
	if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
#else

	{
		hawk_bch_t* mbs;
		mbs = hawk_rtx_duputobcstr(rtx, str, HAWK_NULL);
		if (mbs)
		{
			rx = system(mbs);
			hawk_rtx_freemem (rtx, mbs);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		}
		else
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
	}

#endif

done:
	if (str) hawk_rtx_freevaloocstr (rtx, a0, str);

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------ */

static int fnc_chroot (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

#if defined(_WIN32)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
	{
		hawk_bch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvalbcstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_bchar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		rx = HAWK_CHROOT(str);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);

	done:
		if (str) hawk_rtx_freevalbcstr (rtx, a0, str);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------ */

static int fnc_chmod (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_int_t rx;
	hawk_int_t mode = DEFAULT_MODE;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

	if (hawk_rtx_getnargs(rtx) >= 2 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &mode) <= -1 || mode < 0)) mode = DEFAULT_MODE;

#if defined(_WIN32)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
	{
		hawk_bch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvalbcstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_bchar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		rx = HAWK_CHMOD(str, mode);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);

	done:
		if (str) hawk_rtx_freevalbcstr (rtx, a0, str);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_mkdir (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_int_t rx;
	hawk_int_t mode = DEFAULT_MODE;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

	if (hawk_rtx_getnargs(rtx) >= 2 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &mode) <= -1 || mode < 0)) mode = DEFAULT_MODE;

#if defined(_WIN32)
	{
		hawk_ooch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvaloocstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_oochar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		rx = (CreateDirectory(str, HAWK_NULL) == FALSE)? set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(GetLastError()), HAWK_NULL): 0;

	done:
		if (str) hawk_rtx_freevaloocstr (rtx, a0, str);
	}
#else
	{
		hawk_bch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvalbcstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_bchar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

	#if defined(__OS2__)
		{
			APIRET rc;
			rc = DosCreateDir(str, HAWK_NULL);
			rx = (rc == NO_ERROR)? 0: set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(rc), HAWK_NULL);
		}
	#elif defined(__DOS__)
		rx = mkdir(str);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	#else
		rx = HAWK_MKDIR(str, mode);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	#endif

	done:
		if (str) hawk_rtx_freevalbcstr (rtx, a0, str);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_rmdir (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

#if defined(_WIN32)
	{
		hawk_ooch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvaloocstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_oochar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		rx = (RemoveDirectory(str) == FALSE)? set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(GetLastError()), HAWK_NULL): 0;

	done:
		if (str) hawk_rtx_freevaloocstr (rtx, a0, str);
	}
#else
	{
		hawk_bch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvalbcstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_bchar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		#if defined(__OS2__)
		{
			APIRET rc;
			rc = DosDeleteDir(str);
			rx = (rc == NO_ERROR)? 0: set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(rc), HAWK_NULL);
		}
		#elif defined(__DOS__)
			rx = rmdir(str);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		#else
			rx = HAWK_RMDIR(str);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		#endif

	done:
		if (str) hawk_rtx_freevalbcstr (rtx, a0, str);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------ */

static int fnc_unlink (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

#if defined(_WIN32)
	{
		hawk_ooch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvaloocstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_oochar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		rx = (DeleteFile(str) == FALSE)? set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(GetLastError()), HAWK_NULL): 0;

	done:
		if (str) hawk_rtx_freevaloocstr (rtx, a0, str);
	}
#else
	{
		hawk_bch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvalbcstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_bchar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		#if defined(__OS2__)
		{
			APIRET rc;
			rc = DosDelete(str, HAWK_NULL);
			rx = (rc == NO_ERROR)? 0: set_error_on_sys_list(rtx, sys_list, hawk_syserr_to_errnum(rc), HAWK_NULL);
		}
		#elif defined(__DOS__)
			rx = unlink(str);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		#else
			rx = HAWK_UNLINK(str);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		#endif

	done:
		if (str) hawk_rtx_freevalbcstr (rtx, a0, str);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_symlink (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0, * a1;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);
	a1 = hawk_rtx_getarg(rtx, 1);

#if defined(_WIN32)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
	{
		hawk_bch_t* str1, * str2;
		hawk_oow_t len1, len2;

		str1 = hawk_rtx_getvalbcstr(rtx, a0, &len1);
		str2 = hawk_rtx_getvalbcstr(rtx, a1, &len2);
		if (!str1 || !str2)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		if (hawk_find_bchar(str1, len1, '\0') || hawk_find_bchar(str2, len2, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		rx = HAWK_SYMLINK(str1, str2);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	done:
		if (str2) hawk_rtx_freevalbcstr (rtx, a0, str2);
		if (str1) hawk_rtx_freevalbcstr (rtx, a0, str1);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------ */
/* TODO: fnc_stat */

#if 0
TODO: fnc_utime
static int fnc_utime (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_val_t* a0;
	hawk_int_t rx;
	hawk_int_t mode = DEFAULT_MODE;

	sys_list = rtx_to_sys_list(rtx, fi);
	a0 = hawk_rtx_getarg(rtx, 0);

	if (hawk_rtx_getnargs(rtx) >= 2 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &mode) <= -1 || mode < 0)) mode = DEFAULT_MODE;

#if defined(_WIN32)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__OS2__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#elif defined(__DOS__)
	rx = set_error_on_sys_list(rtx, sys_list, HAWK_ENOIMPL, HAWK_NULL);
#else
	{
		hawk_bch_t* str;
		hawk_oow_t len;

		str = hawk_rtx_getvalbcstr(rtx, a0, &len);
		if (!str)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		/* the target name contains a null character. */
		if (hawk_find_bchar(str, len, '\0'))
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

	#if defined(HAVE_UTIMENSAT)
		/* TODO: */
	#else
		{
			struct timeval tv[2];

			tv[0].tv_sec = xxx;
			tv[1].tv_sec = xxx;
			tv[1].tv_sec = xxx;
			tv[1].tv_sec = xxx;

			rx = utimes(str, tv);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		}
	#endif

	done:
		if (str) hawk_rtx_freevalbcstr (rtx, a0, str);
	}
#endif

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}
#endif

/* ------------------------------------------------------------ */


static int fnc_openmux (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	sys_list_t* sys_list;
	sys_node_t* sys_node = HAWK_NULL;
	hawk_int_t rx;
	int fd;

	sys_list = rtx_to_sys_list(rtx, fi);

#if defined(USE_EPOLL)
	#if defined(HAVE_EPOLL_CREATE1) && defined(O_CLOEXEC)
	fd = epoll_create1(O_CLOEXEC);
	#else
	fd = epoll_create(4096);
	#endif
	if (fd >= 0)
	{
		#if defined(HAVE_EPOLL_CREATE1) && defined(O_CLOEXEC)
		/* nothing to do */
		#elif defined(FD_CLOEXEC)
		{
			int flag = fcntl(fd, F_GETFD);
			if (flag >= 0) fcntl (mux->fd, F_SETFD, flag | FD_CLOEXEC);
		}
		#endif
	}
#else
	/* TODO: */
#endif

	if (fd >= 0)
	{
		sys_node = new_sys_node_mux(rtx, sys_list, fd);
		if (sys_node) 
		{
			rx = sys_node->id;
		}
		else 
		{
			close (fd);
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
	}
	else
	{
		rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

	/*HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));*/
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

static int fnc_closemux (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_MUX, &rx);

	if (sys_node) free_sys_node (rtx, sys_list, sys_node);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

#if defined(USE_EPOLL)
static HAWK_INLINE int ctl_epoll (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi, mux_ctl_cmd_t cmd)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node, * sys_node2;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_MUX, &rx);

	if (sys_node)
	{
		sys_node2 = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 1), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);
		if (sys_node2)
		{
			struct epoll_event ev;
			int evfd;

			if (cmd == MUX_CTL_DEL)
			{
				if (!(sys_node2->ctx.flags & SYS_NODE_DATA_FLAG_IN_MUX))
				{
					rx = set_error_on_sys_list(rtx, sys_list, HAWK_EPERM, HAWK_T("not in mux"));
					goto done;
				}
			}
			else
			{
				hawk_int_t events;

				if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &events) <= -1)
				{
					rx = copy_error_to_sys_list(rtx, sys_list);
					goto done;
				}

				ev.data.ptr = sys_node2;
				ev.events = events;
				ev.events &= ~EPOLLET; /* disable edge trigger if set */

				if (cmd == MUX_CTL_MOD)
				{
					if (!(sys_node2->ctx.flags & SYS_NODE_DATA_FLAG_IN_MUX) || sys_node2->ctx.u.file.mux != sys_node)
					{
						/* not in the multiplxer or not in the right multiplexer */
						rx = set_error_on_sys_list(rtx, sys_list, HAWK_EPERM, HAWK_T("not in mux"));
						goto done;
					}
				}
				else
				{
					if (sys_node2->ctx.flags & SYS_NODE_DATA_FLAG_IN_MUX)
					{
						/* in actual operating systems, a file descriptor can
						 * be watched under multiple I/O multiplexers. but this
						 * implementation only allows 1 multiplexer for each
						 * file descriptor for simplicity */
						rx = set_error_on_sys_list(rtx, sys_list, HAWK_EPERM, HAWK_T("already in mux"));
						goto done;
					}
				}
			}

			switch (sys_node2->ctx.type)
			{
				case SYS_NODE_DATA_TYPE_FILE:
				case SYS_NODE_DATA_TYPE_SCK:
					evfd = sys_node2->ctx.u.file.fd;
					break;

				default:
					/* internal error */
					rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINTERN, HAWK_NULL);
					goto done;
			}

			if (epoll_ctl(sys_node->ctx.u.mux.fd, cmd, evfd, &ev) <= -1)
			{
				rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			}
			else
			{
				switch (sys_node2->ctx.type)
				{
					case SYS_NODE_DATA_TYPE_FILE:
					case SYS_NODE_DATA_TYPE_SCK:
						switch (cmd)
						{
							case MUX_CTL_ADD:
								chain_sys_node_to_mux_node (sys_node, sys_node2);
								break;
							case MUX_CTL_DEL:
								unchain_sys_node_from_mux_node(sys_node, sys_node2);
								break;
							case MUX_CTL_MOD:
								break;
						}
						break;

					default:
						/* internal error */
						rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINTERN, HAWK_NULL);
						goto done;
				}
				
			}
		}
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}
#endif

static int fnc_addtomux (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	return ctl_epoll(rtx, fi, MUX_CTL_ADD);
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

static int fnc_delfrommux (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	return ctl_epoll(rtx, fi, MUX_CTL_DEL);
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

static int fnc_modinmux (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	return ctl_epoll(rtx, fi, MUX_CTL_MOD);
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

static int fnc_waitonmux (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_MUX, &rx);

	if (sys_node)
	{
		sys_node_data_mux_t* mux_data = &sys_node->ctx.u.mux;
		hawk_ntime_t tmout;

		if (val_to_ntime(rtx, hawk_rtx_getarg(rtx, 1), &tmout) <= -1 || tmout.sec <= -1) { tmout.sec = 0; tmout.nsec = HAWK_MSEC_TO_NSEC(-1); }

		if (mux_data->x_evt_max < mux_data->x_count)
		{
			struct epoll_event* tmp;

			tmp = hawk_rtx_reallocmem(rtx, mux_data->x_evt, HAWK_SIZEOF(*tmp) * HAWK_ALIGN(mux_data->x_count, 64));
			if (!tmp)
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}

			mux_data->x_evt_max = HAWK_ALIGN(mux_data->x_count, 64);
			mux_data->x_evt = tmp;
		}

		/* once this function is called, invalid the exising event data regardless of success or failure */
		mux_data->x_evt_count = 0; 

		if ((rx = epoll_wait(sys_node->ctx.u.mux.fd, mux_data->x_evt, mux_data->x_evt_max, HAWK_SECNSEC_TO_MSEC(tmout.sec, tmout.nsec))) <= -1)
		{
			rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
		}
		else 
		{
			/* 0 on timeout, >0 if a file descriptor is ready */
			mux_data->x_evt_count = rx;
		}
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

static int fnc_getmuxevt (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
#if defined(USE_EPOLL)
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_ENOERR);

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_MUX, &rx);

	if (sys_node)
	{
		sys_node_data_mux_t* mux_data = &sys_node->ctx.u.mux;
		sys_node_t* file_node;
		hawk_int_t index, id, evts;
		int x;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &index) <= -1)
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		if (index < 0 || index >= mux_data->x_evt_count)
		{
			/* invalid index */
			rx = set_error_on_sys_list (rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
			goto done;
		}

		file_node = mux_data->x_evt[index].data.ptr;
		if (!file_node)
		{
			/* has it been closed before it's retrieved with sys::getmuxevt()? */
			rx = set_error_on_sys_list (rtx, sys_list, HAWK_ENOENT, HAWK_NULL);
			goto done;
		}

		HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(file_node->id));
		x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), hawk_rtx_makeintval(rtx, file_node->id));
		if (x <= -1) goto fail;

		HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(mux_data->x_evt[index].events));
		x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 3), hawk_rtx_makeintval(rtx, mux_data->x_evt[index].events));
		if (x <= -1) goto fail;
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
#else
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, ERRNUM_TO_RC(HAWK_ENOIMPL)));
	return 0;
#endif
}

/* ------------------------------------------------------------ */

static int fnc_socket (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	hawk_int_t rx, domain = 0, type = 0, proto = 0;
	int fd;

	sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &domain) <= -1 || domain < 0) domain = 0;
	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &type) <= -1 || type < 0) type = 0;
	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &proto) <= -1 || proto < 0) proto = 0;

/* TOOD: SOCK_CLOEXEC, SOCK_NONBLOCK */
	fd = socket(domain, type, proto);
	if (fd >= 0)
	{
		sys_node_t* new_node;

		new_node = new_sys_node_fd(rtx, sys_list, fd);
		if (!new_node) 
		{
			close (fd);
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
		else
		{
			new_node->ctx.type = SYS_NODE_DATA_TYPE_SCK; /* override the type to socket */
			rx = new_node->id;
			HAWK_ASSERT (rx >= 0);
		}
	}
	else
	{
		rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

	HAWK_ASSERT (HAWK_IN_QUICKINT_RANGE(rx));
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_connect (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_val_t* a1;
		hawk_ooch_t* addr;
		hawk_oow_t len;
		hawk_skad_t skad;
		
		a1 = hawk_rtx_getarg(rtx, 1);
		addr = hawk_rtx_getvaloocstr(rtx, a1, &len);
		if (addr)
		{
			if (hawk_gem_oocharstoskad(hawk_rtx_getgem(rtx), addr, len, &skad) <= -1) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				hawk_rtx_freevaloocstr (rtx, a1, addr);
				goto done;
			}

			hawk_rtx_freevaloocstr (rtx, a1, addr);
			rx = connect(sys_node->ctx.u.file.fd, (struct sockaddr*)&skad, hawk_skad_size(&skad));
			if (rx <= -1) 
			{
				rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
				goto done;
			}
		}
		else
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_recvfrom (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_int_t reqsize = 8192;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_FILE | SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_skad_t skad;
		socklen_t addrlen;

		if (hawk_rtx_getnargs(rtx) >= 3 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &reqsize) <= -1 || reqsize <= 0)) reqsize = 8192;
		if (reqsize > HAWK_QUICKINT_MAX) reqsize = HAWK_QUICKINT_MAX;

		if (reqsize > sys_list->ctx.readbuf_capa)
		{
			hawk_bch_t* tmp = hawk_rtx_reallocmem(rtx, sys_list->ctx.readbuf, reqsize);
			if (!tmp)
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}
			sys_list->ctx.readbuf = tmp;
			sys_list->ctx.readbuf_capa = reqsize;
		}

		addrlen = HAWK_SIZEOF(skad);
		rx = recvfrom(sys_node->ctx.u.file.fd, sys_list->ctx.readbuf, reqsize, 0, (struct sockaddr*)&skad, &addrlen);
		if (rx <= 0) 
		{
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_T("unable to read"));
			goto done;
		}
		else
		{
			hawk_val_t* sv;
			int x;

			sv = hawk_rtx_makembsval(rtx, sys_list->ctx.readbuf, rx);
			if (!sv) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}

			hawk_rtx_refupval (rtx, sv);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 1), sv);
			hawk_rtx_refdownval (rtx, sv);
			if (x <= -1) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}

			if (hawk_rtx_getnargs(rtx) >= 4)
			{
				hawk_gem_skadtooocstr (hawk_rtx_getgem(rtx), &skad, sys_list->ctx.skadbuf[0], HAWK_COUNTOF(sys_list->ctx.skadbuf[0]), HAWK_SKAD_TO_OOCSTR_ADDR | HAWK_SKAD_TO_OOCSTR_PORT);
				sv = hawk_rtx_makestrvalwithoocstr(rtx, sys_list->ctx.skadbuf[0]);
				if (!sv)
				{
					rx = copy_error_to_sys_list(rtx, sys_list);
					goto done;
				}

				hawk_rtx_refupval (rtx, sv);
				x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 3), sv);
				hawk_rtx_refdownval (rtx, sv);
				if (x <= -1) 
				{
					rx = copy_error_to_sys_list(rtx, sys_list);
					goto done;
				}
			}
		}
	}

done:
	/* the value in 'rx' never exceeds HAWK_QUICKINT_MAX as 'reqsize' has been limited to
	 * it before the call to 'read'. so it's safe not to check the result of hawk_rtx_makeintval() */
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_sendto (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;
	hawk_val_t* retv;
	
	hawk_skad_t skad;
	struct sockaddr* sa = HAWK_NULL;
	hawk_oow_t salen = 0;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_bch_t* dptr;
		hawk_oow_t dlen;
		hawk_val_t* a1;

		if (hawk_rtx_getnargs(rtx) >= 3)
		{
			hawk_val_t* a2;
			hawk_ooch_t* addr;
			hawk_oow_t len;
			
			a2 = hawk_rtx_getarg(rtx, 2);
			addr = hawk_rtx_getvaloocstr(rtx, a2, &len);
			if (!addr)
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}
			
			if (hawk_gem_oocharstoskad(hawk_rtx_getgem(rtx), addr, len, &skad) <= -1) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				hawk_rtx_freevaloocstr (rtx, a2, addr);
				goto done;
			}
			hawk_rtx_freevaloocstr (rtx, a2, addr);
				
			sa = (struct sockaddr*)&skad;
			salen = hawk_skad_size(&skad);
		}

		a1 = hawk_rtx_getarg(rtx, 1);
		dptr = hawk_rtx_getvalbcstr(rtx, a1, &dlen);
		if (dptr)
		{			
			rx = sendto(sys_node->ctx.u.file.fd, dptr, dlen, 0, sa, salen);
			if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			hawk_rtx_freevalbcstr (rtx, a1, dptr);
		}
		else
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
		}
	}

done:
	retv = hawk_rtx_makeintval(rtx, rx);
	if (!retv) return -1; /* hard failure. unable to make a return value */
	hawk_rtx_setretval (rtx, retv);
	return 0;
}


static int fnc_shutdown (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_int_t how = 0;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &how) <= -1 || how < 0) how = 0;

		rx = shutdown(sys_node->ctx.u.file.fd, how);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_bind (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_val_t* a1;
		hawk_ooch_t* addr;
		hawk_oow_t len;
		hawk_skad_t skad;

		a1 = hawk_rtx_getarg(rtx, 1);
		addr = hawk_rtx_getvaloocstr(rtx, a1, &len);
		if (!addr)
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}
		
		if (hawk_gem_oocharstoskad(hawk_rtx_getgem(rtx), addr, len, &skad) <= -1) 
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			hawk_rtx_freevaloocstr (rtx, a1, addr);
			goto done;
		}
		hawk_rtx_freevaloocstr (rtx, a1, addr);

		rx = bind(sys_node->ctx.u.file.fd, (struct sockaddr*)&skad, hawk_skad_size(&skad));
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_listen (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_int_t backlog = 0;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &backlog) <= -1 || backlog < 0) backlog = 0;

		rx = listen(sys_node->ctx.u.file.fd, backlog);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* sys::accept (s, flags, from); */
static int fnc_accept (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_skad_t skad;
		socklen_t addrlen;
		sys_node_t* new_node;
		int fd, fd_flags;
		hawk_int_t flags = 0;

		if (hawk_rtx_getnargs(rtx) >= 2 && (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &flags) <= -1 || flags < 0)) flags = 0;

		addrlen = HAWK_SIZEOF(skad);
	#if defined(HAVE_ACCEPT4)
		fd = accept4(sys_node->ctx.u.file.fd, (struct sockaddr*)&skad, &addrlen, flags);
	#else
		fd = accept(sys_node->ctx.u.file.fd, (struct sockaddr*)&skad, &addrlen);
	#endif
		if (fd <= -1) 
		{
			rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
			goto done;
		}

	#if defined(HAVE_ACCEPT4)
		/* nothing to do */
	#else
		fd_flags = fcntl(fd, F_GETFD, 0);
		if (fd_flags >= 0)
		{
		#if defined(FD_CLOEXEC)
			if (flags & SOCK_NONBLOCK) fd_flags |= FD_CLOEXEC;
		#endif
		#if defined(O_NONBLOCK)
			if (flags & SOCK_CLOEXEC) fd_flags |= O_NONBLOCK;
		#endif
			fcntl(fd, F_SETFD, fd_flags);
		}
	#endif

		if (hawk_rtx_getnargs(rtx) >= 3)
		{
			hawk_val_t* sv;
			int x;

			hawk_gem_skadtooocstr (hawk_rtx_getgem(rtx), &skad, sys_list->ctx.skadbuf[0], HAWK_COUNTOF(sys_list->ctx.skadbuf[0]), HAWK_SKAD_TO_OOCSTR_ADDR | HAWK_SKAD_TO_OOCSTR_PORT);
			sv = hawk_rtx_makestrvalwithoocstr(rtx, sys_list->ctx.skadbuf[0]);
			if (!sv)
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}

			hawk_rtx_refupval (rtx, sv);
			x = hawk_rtx_setrefval(rtx, (hawk_val_ref_t*)hawk_rtx_getarg(rtx, 2), sv);
			hawk_rtx_refdownval (rtx, sv);
			if (x <= -1) 
			{
				rx = copy_error_to_sys_list(rtx, sys_list);
				goto done;
			}
		}

		new_node = new_sys_node_fd(rtx, sys_list, fd);
		if (!new_node) 
		{
			close (fd);
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		new_node->ctx.type = SYS_NODE_DATA_TYPE_SCK; /* override the type to socket */
		rx = new_node->id;
		HAWK_ASSERT (rx >= 0);
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_setsockopt (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	sys_list_t* sys_list;
	sys_node_t* sys_node;
	hawk_int_t rx;

	sys_list = rtx_to_sys_list(rtx, fi);
	sys_node = get_sys_list_node_with_arg(rtx, sys_list, hawk_rtx_getarg(rtx, 0), SYS_NODE_DATA_TYPE_SCK, &rx);
	if (sys_node)
	{
		hawk_int_t level, optname;
		int iv;
		struct timeval tv;
		void* vptr;
		hawk_oow_t vlen;

		if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &level) <= -1 ||
		    hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &optname) <= -1)
		{
		fail:
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}

		switch (optname)
		{
		/* TODO:
			case SO_BINDTODEVICE:
		*/
			case SO_BROADCAST:
			case SO_DONTROUTE:
			case SO_KEEPALIVE:
			case SO_RCVBUF:
			case SO_REUSEADDR:
			case SO_REUSEPORT:
			case SO_SNDBUF:
			{
				hawk_int_t tmp;
				if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 3), &tmp) <= -1) goto fail;
				iv = tmp;
				vptr = &iv;
				vlen = HAWK_SIZEOF(iv);
				break;
			}

			case SO_RCVTIMEO:
			case SO_SNDTIMEO:
			{
				hawk_ntime_t tmp;
				if (val_to_ntime(rtx, hawk_rtx_getarg(rtx, 3), &tmp) <= -1) goto fail;
				tv.tv_sec = tmp.sec;
				tv.tv_usec = HAWK_NSEC_TO_MSEC(tmp.nsec);
				vptr = &tv;
				vlen = HAWK_SIZEOF(tv);
				break;
			}

			default:
				rx = set_error_on_sys_list(rtx, sys_list, EINVAL, HAWK_NULL);
				goto done;
		}

		rx = setsockopt(sys_node->ctx.u.file.fd, level, optname, vptr, vlen);
		if (rx <= -1) rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_NULL);
	}

done:
	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}
/* ------------------------------------------------------------ */

/*
 sys::openlog("remote://192.168.1.23:1234/test", sys::LOG_OPT_PID | sys::LOG_OPT_NDELAY, sys::LOG_FAC_LOCAL0);
 for (i = 0; i < 10; i++) sys::writelog(sys::LOG_PRI_DEBUG, "hello world " i);
 sys::closelog(); 
 */
static void open_remote_log_socket (hawk_rtx_t* rtx, mod_ctx_t* mctx)
{
#if defined(_WIN32)
	/* TODO: implement this */
#else
	int sck, flags;
	int domain = hawk_skad_family(&mctx->log.skad);
	int type = SOCK_DGRAM;

	HAWK_ASSERT (mctx->log.sck <= -1);

#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
	type |= SOCK_NONBLOCK;
	type |= SOCK_CLOEXEC;
open_socket:
#endif
	sck = socket(domain, type, 0); 
	if (sck <= -1)
	{
	#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
		if (errno == EINVAL && (type & (SOCK_NONBLOCK | SOCK_CLOEXEC)))
		{
			type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
			goto open_socket;
		}
	#endif
		return;
	}
	else
	{
	#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
		if (type & (SOCK_NONBLOCK | SOCK_CLOEXEC)) goto done;
	#endif
	}

	flags = fcntl(sck, F_GETFD, 0);
	if (flags <= -1) return;
#if defined(FD_CLOEXEC)
	flags |= FD_CLOEXEC;
#endif
#if defined(O_NONBLOCK)
	flags |= O_NONBLOCK;
#endif
	if (fcntl(sck, F_SETFD, flags) <= -1) return;

done:
	mctx->log.sck = sck;

#endif
}

static int fnc_openlog (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_EOTHER);
	hawk_int_t opt, fac;
	hawk_ooch_t* ident = HAWK_NULL, * actual_ident;
	hawk_oow_t ident_len;
	hawk_bch_t* mbs_ident;
	mod_ctx_t* mctx = (mod_ctx_t*)fi->mod->ctx;
	hawk_skad_t skad;
	syslog_type_t log_type = SYSLOG_LOCAL;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

	ident = hawk_rtx_getvaloocstr(rtx, hawk_rtx_getarg(rtx, 0), &ident_len);
	if (!ident) 
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

	/* the target name contains a null character.
	 * make system return -1 */
	if (hawk_find_oochar(ident, ident_len, '\0')) 
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_T("invalid identifier of length %zu containing '\\0'"), ident_len);
		goto done;
	}

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 1), &opt) <= -1) goto fail;
	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 2), &fac) <= -1) goto fail;

	if (hawk_comp_oocstr_limited(ident, HAWK_T("remote://"), 9, 0) == 0)
	{
		hawk_ooch_t* slash;
		/* "remote://remote-addr:remote-port/syslog-identifier" */

		log_type = SYSLOG_REMOTE;
		actual_ident = ident + 9;
		slash = hawk_find_oochar_in_oocstr(actual_ident, '/');
		if (!slash) 
		{
			rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_T("invalid identifier '%js' with remote address"), ident);
			goto done;
		}
		if (hawk_gem_oocharstoskad(hawk_rtx_getgem(rtx), actual_ident, slash - actual_ident, &skad) <= -1) 
		{
			rx = copy_error_to_sys_list(rtx, sys_list);
			goto done;
		}
		actual_ident = slash + 1;
	}
	else if (hawk_comp_oocstr_limited(ident, HAWK_T("local://"), 8, 0) == 0)
	{
		/* "local://syslog-identifier" */
		actual_ident = ident + 8;
	}
	else
	{
		actual_ident = ident;
	}

#if defined(HAWK_OOCH_IS_BCH)
	mbs_ident = hawk_rtx_dupbcstr(rtx, actual_ident, HAWK_NULL);
#else
	mbs_ident = hawk_rtx_duputobcstr(rtx, actual_ident, HAWK_NULL);
#endif
	if (!mbs_ident) 
	{
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

	if (mctx->log.ident) hawk_rtx_freemem (rtx, mctx->log.ident);
	mctx->log.ident = mbs_ident;

#if defined(ENABLE_SYSLOG)
	if (mctx->log.syslog_opened)
	{
		closelog ();
		mctx->log.syslog_opened = 0;
	}
#endif
	if (mctx->log.sck >= 0)
	{
	#if defined(_WIN32)
		/* TODO: impelement this */
	#else
		close (mctx->log.sck);
	#endif
		mctx->log.sck = -1;
	}

	mctx->log.type = log_type;
	mctx->log.opt = opt;
	mctx->log.fac = fac;
	if (mctx->log.type == SYSLOG_LOCAL)
	{
	#if defined(ENABLE_SYSLOG)
		openlog(mbs_ident, opt, fac);
		mctx->log.syslog_opened = 1;
	#endif
	}
	else if (mctx->log.type == SYSLOG_REMOTE)
	{
		mctx->log.skad = skad;
		if ((opt & LOG_NDELAY) && mctx->log.sck <= -1) open_remote_log_socket (rtx, mctx);
	}

	rx = ERRNUM_TO_RC(HAWK_ENOERR);

done:
	if (ident) hawk_rtx_freevaloocstr(rtx, hawk_rtx_getarg(rtx, 0), ident);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

static int fnc_closelog (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_EOTHER);
	mod_ctx_t* mctx = (mod_ctx_t*)fi->mod->ctx;

	switch (mctx->log.type)
	{
		case SYSLOG_LOCAL:
		#if defined(ENABLE_SYSLOG)
			closelog ();
			/* closelog() might be called without openlog(). so there is no 
			 * check if syslog_opened is true.
			 * it is just used as an indicator to decide wheter closelog()
			 * should be called upon module finalization(fini). */
			mctx->log.syslog_opened = 0;
		#endif
			break;

		case SYSLOG_REMOTE:
			if (mctx->log.sck >= 0)
			{
			#if defined(_WIN32)
				/* TODO: impelement this */
			#else
				close (mctx->log.sck);
			#endif
				mctx->log.sck = -1;
			}

			if (mctx->log.dmsgbuf)
			{
				hawk_becs_close (mctx->log.dmsgbuf);
				mctx->log.dmsgbuf = HAWK_NULL;
			}

			break;
	}

	if (mctx->log.ident)
	{
		hawk_rtx_freemem (rtx, mctx->log.ident);
		mctx->log.ident = HAWK_NULL;
	}

	/* back to the local syslog in case writelog() is called
	 * without another openlog() after this closelog() */
	mctx->log.type = SYSLOG_LOCAL;

	rx = ERRNUM_TO_RC(HAWK_ENOERR);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}


static int fnc_writelog (hawk_rtx_t* rtx, const hawk_fnc_info_t* fi)
{
	hawk_int_t rx = ERRNUM_TO_RC(HAWK_EOTHER);
	hawk_int_t pri;
	hawk_ooch_t* msg = HAWK_NULL;
	hawk_oow_t msglen;
	mod_ctx_t* mctx = (mod_ctx_t*)fi->mod->ctx;
	sys_list_t* sys_list = rtx_to_sys_list(rtx, fi);

	if (hawk_rtx_valtoint(rtx, hawk_rtx_getarg(rtx, 0), &pri) <= -1)
	{
	fail:
		rx = copy_error_to_sys_list(rtx, sys_list);
		goto done;
	}

	msg = hawk_rtx_getvaloocstr(rtx, hawk_rtx_getarg(rtx, 1), &msglen);
	if (!msg) goto fail;

	if (hawk_find_oochar(msg, msglen, '\0')) 
	{
		rx = set_error_on_sys_list(rtx, sys_list, HAWK_EINVAL, HAWK_NULL);
		goto done;
	}

	if (mctx->log.type == SYSLOG_LOCAL)
	{
	#if defined(ENABLE_SYSLOG)
		#if defined(HAWK_OOCH_IS_BCH)
		syslog(pri, "%s", msg);
		#else
		{
			hawk_bch_t* mbs;
			mbs = hawk_rtx_duputobcstr(rtx, msg, HAWK_NULL);
			if (!mbs) goto fail;
			syslog(pri, "%s", mbs);
			hawk_rtx_freemem (rtx, mbs);
		}
		#endif
	#endif
	}
	else if (mctx->log.type == SYSLOG_REMOTE)
	{
	#if defined(_WIN32)
		/* TODO: implement this */
	#else

		static const hawk_bch_t* __syslog_month_names[] =
		{
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
		};

		if (mctx->log.sck <= -1) open_remote_log_socket (rtx, mctx);

		if (mctx->log.sck >= 0)
		{
			hawk_ntime_t now;
			struct tm tm, * tmx;
			time_t t;

			if (!mctx->log.dmsgbuf) 
			{
				mctx->log.dmsgbuf = hawk_becs_open(hawk_rtx_getgem(rtx), 0, 0);
				if (!mctx->log.dmsgbuf) goto fail;
			}

			if (hawk_get_time(&now) <= -1)
			{
				rx = set_error_on_sys_list(rtx, sys_list, HAWK_ESYSERR, HAWK_T("unable to get time"));
				goto done;
			}

			t = now.sec;
		#if defined(HAVE_LOCALTIME_R)
			tmx = localtime_r(&t, &tm);
		#else
			tmx = localtime(&t);
		#endif
			if (!tmx) 
			{
				rx = set_error_on_sys_list_with_errno(rtx, sys_list, HAWK_T("unable to get local time"));
				goto done;
			}

			if (hawk_becs_fmt(
				mctx->log.dmsgbuf, HAWK_BT("<%d>%hs %02d %02d:%02d:%02d "), 
				(int)(mctx->log.fac | pri),
				__syslog_month_names[tmx->tm_mon], tmx->tm_mday, 
				tmx->tm_hour, tmx->tm_min, tmx->tm_sec) == (hawk_oow_t)-1) goto fail;

			if (mctx->log.ident || (mctx->log.opt & LOG_PID))
			{
				/* if the identifier is set or LOG_PID is set, the produced tag won't be empty.
				 * so appending ':' is kind of ok */
				if (hawk_becs_fcat(mctx->log.dmsgbuf, HAWK_BT("%hs"), (mctx->log.ident? mctx->log.ident: HAWK_BT(""))) == (hawk_oow_t)-1) goto fail;
				if ((mctx->log.opt & LOG_PID) && hawk_becs_fcat(mctx->log.dmsgbuf, HAWK_BT("[%d]"), (int)HAWK_GETPID()) == (hawk_oow_t)-1) goto fail;
				if (hawk_becs_fcat(mctx->log.dmsgbuf, HAWK_BT(": ")) == (hawk_oow_t)-1) goto fail;
			}

		#if defined(HAWK_OOCH_IS_BCH)
			if (hawk_becs_fcat(mctx->log.dmsgbuf, HAWK_BT("%hs"), msg) == (hawk_oow_t)-1) goto fail;
		#else
			if (hawk_becs_fcat(mctx->log.dmsgbuf, HAWK_BT("%ls"), msg) == (hawk_oow_t)-1) goto fail;
		#endif

			/* don't care about output failure */
			sendto (mctx->log.sck, HAWK_BECS_PTR(mctx->log.dmsgbuf), HAWK_BECS_LEN(mctx->log.dmsgbuf),
			        0, (struct sockaddr*)&mctx->log.skad, hawk_skad_size(&mctx->log.skad));
		}
	#endif
	}

	rx = ERRNUM_TO_RC(HAWK_ENOERR);

done:
	if (msg) hawk_rtx_freevaloocstr(rtx, hawk_rtx_getarg(rtx, 1), msg);

	hawk_rtx_setretval (rtx, hawk_rtx_makeintval(rtx, rx));
	return 0;
}

/* ------------------------------------------------------------ */

typedef struct fnctab_t fnctab_t;
struct fnctab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_fnc_t info;
};

typedef struct inttab_t inttab_t;
struct inttab_t
{
	const hawk_ooch_t* name;
	hawk_mod_sym_int_t info;
};

static fnctab_t fnctab[] =
{
	/* keep this table sorted for binary search in query(). */

	{ HAWK_T("WCOREDUMP"),   { { 1, 1, HAWK_NULL       }, fnc_wcoredump,   0  } },
	{ HAWK_T("WEXITSTATUS"), { { 1, 1, HAWK_NULL       }, fnc_wexitstatus, 0  } },
	{ HAWK_T("WIFEXITED"),   { { 1, 1, HAWK_NULL       }, fnc_wifexited,   0  } },
	{ HAWK_T("WIFSIGNALED"), { { 1, 1, HAWK_NULL       }, fnc_wifsignaled, 0  } },
	{ HAWK_T("WTERMSIG"),    { { 1, 1, HAWK_NULL       }, fnc_wtermsig,    0  } },
	{ HAWK_T("accept"),      { { 1, 3, HAWK_T("vvr")   }, fnc_accept,      0  } },
	{ HAWK_T("addtomux"),    { { 3, 3, HAWK_NULL       }, fnc_addtomux,    0  } },
	{ HAWK_T("bind"),        { { 2, 2, HAWK_NULL       }, fnc_bind,        0  } },
	{ HAWK_T("chmod"),       { { 2, 2, HAWK_NULL       }, fnc_chmod,       0  } },
	{ HAWK_T("chroot"),      { { 1, 1, HAWK_NULL       }, fnc_chroot,      0  } },
	{ HAWK_T("close"),       { { 1, 2, HAWK_NULL       }, fnc_close,       0  } },
	{ HAWK_T("closedir"),    { { 1, 1, HAWK_NULL       }, fnc_closedir,    0  } },
	{ HAWK_T("closelog"),    { { 0, 0, HAWK_NULL       }, fnc_closelog,    0  } },
	{ HAWK_T("closemux"),    { { 1, 1, HAWK_NULL       }, fnc_closemux,    0  } },
	{ HAWK_T("connect"),     { { 2, 2, HAWK_NULL       }, fnc_connect,     0  } },
	{ HAWK_T("delfrommux"),  { { 2, 2, HAWK_NULL       }, fnc_delfrommux,  0  } },
	{ HAWK_T("dup"),         { { 1, 3, HAWK_NULL       }, fnc_dup,         0  } },
	{ HAWK_T("errmsg"),      { { 0, 0, HAWK_NULL       }, fnc_errmsg,      0  } },
	{ HAWK_T("fchmod"),      { { 2, 2, HAWK_NULL       }, fnc_fchmod,      0  } },
	{ HAWK_T("fchown"),      { { 3, 3, HAWK_NULL       }, fnc_fchown,      0  } },
	{ HAWK_T("fcntl"),       { { 2, 3, HAWK_NULL       }, fnc_fcntl,       0  } },
	{ HAWK_T("flock"),       { { 5, 6, HAWK_T("vvvvvr")}, fnc_flock,       0  } },
	{ HAWK_T("fork"),        { { 0, 0, HAWK_NULL       }, fnc_fork,        0  } },
	{ HAWK_T("fseek"),       { { 3, 3, HAWK_NULL       }, fnc_fseek,       0  } },
	{ HAWK_T("getegid"),     { { 0, 0, HAWK_NULL       }, fnc_getegid,     0  } },
	{ HAWK_T("getenv"),      { { 2, 2, HAWK_T("vr")    }, fnc_getenv,      0  } },
	{ HAWK_T("geteuid"),     { { 0, 0, HAWK_NULL       }, fnc_geteuid,     0  } },
	{ HAWK_T("getgid"),      { { 0, 0, HAWK_NULL       }, fnc_getgid,      0  } },
	{ HAWK_T("getifcfg"),    { { 3, 3, HAWK_T("vvr")   }, fnc_getifcfg,    0  } },
	{ HAWK_T("getmuxevt"),   { { 4, 4, HAWK_T("vvrr")  }, fnc_getmuxevt,   0  } },
	{ HAWK_T("getnwifcfg"),  { { 3, 3, HAWK_T("vvr")   }, fnc_getifcfg,    0  } }, /* backward compatibility */
	{ HAWK_T("getpgid"),     { { 0, 0, HAWK_NULL       }, fnc_getpgid,     0  } },
	{ HAWK_T("getpid"),      { { 0, 0, HAWK_NULL       }, fnc_getpid,      0  } },
	{ HAWK_T("getppid"),     { { 0, 0, HAWK_NULL       }, fnc_getppid,     0  } },
	{ HAWK_T("gettid"),      { { 0, 0, HAWK_NULL       }, fnc_gettid,      0  } },
	{ HAWK_T("gettime"),     { { 0, 0, HAWK_NULL       }, fnc_gettime,     0  } },
	{ HAWK_T("getuid"),      { { 0, 0, HAWK_NULL       }, fnc_getuid,      0  } },
	{ HAWK_T("kill"),        { { 2, 2, HAWK_NULL       }, fnc_kill,        0  } },
	{ HAWK_T("listen"),      { { 2, 2, HAWK_NULL       }, fnc_listen,        0  } },
	{ HAWK_T("mkdir"),       { { 1, 2, HAWK_NULL       }, fnc_mkdir,       0  } },
	{ HAWK_T("mktime"),      { { 0, 1, HAWK_NULL       }, fnc_mktime,      0  } },
	{ HAWK_T("modinmux"),    { { 3, 3, HAWK_NULL       }, fnc_modinmux,   0  } },
	{ HAWK_T("open"),        { { 2, 3, HAWK_NULL       }, fnc_open,        0  } },
	{ HAWK_T("opendir"),     { { 1, 2, HAWK_NULL       }, fnc_opendir,     0  } },
	{ HAWK_T("openfd"),      { { 1, 1, HAWK_NULL       }, fnc_openfd,      0  } },
	{ HAWK_T("openlog"),     { { 3, 3, HAWK_NULL       }, fnc_openlog,     0  } },
	{ HAWK_T("openmux"),     { { 0, 0, HAWK_NULL       }, fnc_openmux,     0  } },
	{ HAWK_T("pipe"),        { { 2, 3, HAWK_T("rrv")   }, fnc_pipe,        0  } },
	{ HAWK_T("read"),        { { 2, 3, HAWK_T("vrv")   }, fnc_read,        0  } },
	{ HAWK_T("readdir"),     { { 2, 2, HAWK_T("vr")    }, fnc_readdir,     0  } },
	{ HAWK_T("recvfrom"),    { { 2, 4, HAWK_T("vrvr")  }, fnc_recvfrom,    0  } },
	{ HAWK_T("resetdir"),    { { 2, 2, HAWK_NULL       }, fnc_resetdir,    0  } },
	{ HAWK_T("rmdir"),       { { 1, 1, HAWK_NULL       }, fnc_rmdir,       0  } },
	{ HAWK_T("sendto"),      { { 2, 3, HAWK_NULL       }, fnc_sendto,      0  } },
	{ HAWK_T("setenv"),      { { 2, 3, HAWK_NULL       }, fnc_setenv,      0  } },
	{ HAWK_T("setsockopt"),  { { 4, 4, HAWK_NULL       }, fnc_setsockopt,  0  } },
	{ HAWK_T("settime"),     { { 1, 1, HAWK_NULL       }, fnc_settime,     0  } },
	{ HAWK_T("shutdown"),    { { 2, 2, HAWK_NULL       }, fnc_shutdown,    0  } },
	{ HAWK_T("sleep"),       { { 1, 1, HAWK_NULL       }, fnc_sleep,       0  } },
	{ HAWK_T("socket"),      { { 3, 3, HAWK_NULL       }, fnc_socket,      0  } },
	{ HAWK_T("strftime"),    { { 2, 3, HAWK_NULL       }, fnc_strftime,    0  } },
	{ HAWK_T("symlink"),     { { 2, 2, HAWK_NULL       }, fnc_symlink,     0  } },
	{ HAWK_T("system"),      { { 1, 1, HAWK_NULL       }, fnc_system,      0  } },
	{ HAWK_T("systime"),     { { 0, 0, HAWK_NULL       }, fnc_gettime,     0  } }, /* alias to gettime() */
	{ HAWK_T("unlink"),      { { 1, 1, HAWK_NULL       }, fnc_unlink,      0  } },
	{ HAWK_T("unsetenv"),    { { 1, 1, HAWK_NULL       }, fnc_unsetenv,    0  } },
	{ HAWK_T("wait"),        { { 1, 3, HAWK_T("vrv")   }, fnc_wait,        0  } },
	{ HAWK_T("waitonmux"),   { { 2, 2, HAWK_T("vv")    }, fnc_waitonmux,   0  } },
	{ HAWK_T("write"),       { { 2, 4, HAWK_NULL       }, fnc_write,       0  } },
	{ HAWK_T("writelog"),    { { 2, 2, HAWK_NULL       }, fnc_writelog,    0  } }
};

#if !defined(SIGHUP)
#	define SIGHUP 1
#endif
#if !defined(SIGINT)
#	define SIGINT 2
#endif
#if !defined(SIGQUIT)
#	define SIGQUIT 3
#endif
#if !defined(SIGABRT)
#	define SIGABRT 6
#endif
#if !defined(SIGKILL)
#	define SIGKILL 9
#endif
#if !defined(SIGSEGV)
#	define SIGSEGV 11
#endif
#if !defined(SIGALRM)
#	define SIGALRM 14
#endif
#if !defined(SIGTERM)
#	define SIGTERM 15
#endif

static inttab_t inttab[] =
{
	/* keep this table sorted for binary search in query(). */

	{ HAWK_T("AF_INET"),            { AF_INET } },
	{ HAWK_T("AF_INET6"),           { AF_INET6 } },

	{ HAWK_T("C_KEEPFD"),           { CLOSE_KEEPFD } },

	{ HAWK_T("DIR_SORT"),           { HAWK_DIR_SORT } },

	{ HAWK_T("FD_CLOEXEC"),         { FD_CLOEXEC } },

	{ HAWK_T("FLOCK_GET"),          { FLOCK_GET } }, /* bitwise-OR with another FLOCK_READ/WRITE/WAIT value */
	{ HAWK_T("FLOCK_READ"),         { F_RDLCK } },
	{ HAWK_T("FLOCK_UNLOCK"),       { F_UNLCK } },
	{ HAWK_T("FLOCK_WAIT"),         { FLOCK_WAIT } }, /* bitwise-OR with another FLOCK_READ/WRITE/WAIT value */
	{ HAWK_T("FLOCK_WRITE"),        { F_WRLCK } },

	{ HAWK_T("F_GETFD"),            { F_GETFD } },
	{ HAWK_T("F_GETFL"),            { F_GETFL } },
	{ HAWK_T("F_SETFD"),            { F_SETFD } },
	{ HAWK_T("F_SETFL"),            { F_SETFL } },

	{ HAWK_T("IFCFG_IN4"),          { HAWK_IFCFG_IN4 } },
	{ HAWK_T("IFCFG_IN6"),          { HAWK_IFCFG_IN6 } },

#if defined(ENABLE_SYSLOG)
	{ HAWK_T("LOG_FAC_AUTH"),       { LOG_AUTH } },
	{ HAWK_T("LOG_FAC_AUTHPRIV"),   { LOG_AUTHPRIV } },
	{ HAWK_T("LOG_FAC_CRON"),       { LOG_CRON } },
	{ HAWK_T("LOG_FAC_DAEMON"),     { LOG_DAEMON } },
	{ HAWK_T("LOG_FAC_FTP"),        { LOG_FTP } },
	{ HAWK_T("LOG_FAC_KERN"),       { LOG_KERN } },
	{ HAWK_T("LOG_FAC_LOCAL0"),     { LOG_LOCAL0 } },
	{ HAWK_T("LOG_FAC_LOCAL1"),     { LOG_LOCAL1 } },
	{ HAWK_T("LOG_FAC_LOCAL2"),     { LOG_LOCAL2 } },
	{ HAWK_T("LOG_FAC_LOCAL3"),     { LOG_LOCAL3 } },
	{ HAWK_T("LOG_FAC_LOCAL4"),     { LOG_LOCAL4 } },
	{ HAWK_T("LOG_FAC_LOCAL5"),     { LOG_LOCAL5 } },
	{ HAWK_T("LOG_FAC_LOCAL6"),     { LOG_LOCAL6 } },
	{ HAWK_T("LOG_FAC_LOCAL7"),     { LOG_LOCAL7 } },
	{ HAWK_T("LOG_FAC_LPR"),        { LOG_LPR } },
	{ HAWK_T("LOG_FAC_MAIL"),       { LOG_MAIL } },
	{ HAWK_T("LOG_FAC_NEWS"),       { LOG_NEWS } },
	{ HAWK_T("LOG_FAC_SYSLOG"),     { LOG_SYSLOG } },
	{ HAWK_T("LOG_FAC_USER"),       { LOG_USER } },
	{ HAWK_T("LOG_FAC_UUCP"),       { LOG_UUCP } },

	{ HAWK_T("LOG_OPT_CONS"),       { LOG_CONS } },
	{ HAWK_T("LOG_OPT_NDELAY"),     { LOG_NDELAY } },
	{ HAWK_T("LOG_OPT_NOWAIT"),     { LOG_NOWAIT } },
	{ HAWK_T("LOG_OPT_PID"),        { LOG_PID } },

	{ HAWK_T("LOG_PRI_ALERT"),      { LOG_ALERT } },
	{ HAWK_T("LOG_PRI_CRIT"),       { LOG_CRIT } },
	{ HAWK_T("LOG_PRI_DEBUG"),      { LOG_DEBUG } },
	{ HAWK_T("LOG_PRI_EMERG"),      { LOG_EMERG } },
	{ HAWK_T("LOG_PRI_ERR"),        { LOG_ERR } },
	{ HAWK_T("LOG_PRI_INFO"),       { LOG_INFO } },
	{ HAWK_T("LOG_PRI_NOTICE"),     { LOG_NOTICE } },
	{ HAWK_T("LOG_PRI_WARNING"),    { LOG_WARNING } },
#endif

	{ HAWK_T("MUX_EVT_ERR"),  { MUX_EVT_ERR } },
	{ HAWK_T("MUX_EVT_HUP"),  { MUX_EVT_HUP } },
	{ HAWK_T("MUX_EVT_IN"),   { MUX_EVT_IN } },
	{ HAWK_T("MUX_EVT_OUT"),  { MUX_EVT_OUT } },

	{ HAWK_T("NWIFCFG_IN4"),  { HAWK_IFCFG_IN4 } }, /* for backward compatibility */
	{ HAWK_T("NWIFCFG_IN6"),  { HAWK_IFCFG_IN6 } }, /* for backward compatibility */

#if defined(O_APPEND)
	{ HAWK_T("O_APPEND"),     { O_APPEND } },
#endif
#if defined(O_ASYNC)
	{ HAWK_T("O_ASYNC"),      { O_ASYNC } },
#endif
#if defined(O_CLOEXEC)
	{ HAWK_T("O_CLOEXEC"),    { O_CLOEXEC } },
#endif
#if defined(O_CREAT)
	{ HAWK_T("O_CREAT"),      { O_CREAT } },
#endif
#if defined(O_DIRECTORY)
	{ HAWK_T("O_DIRECTORY"),  { O_DIRECTORY } },
#endif
#if defined(O_DSYNC)
	{ HAWK_T("O_DSYNC"),      { O_DSYNC } },
#endif
#if defined(O_EXCL)
	{ HAWK_T("O_EXCL"),       { O_EXCL } },
#endif
#if defined(O_NOATIME)
	{ HAWK_T("O_NOATIME"),    { O_NOATIME} },
#endif
#if defined(O_NOCTTY)
	{ HAWK_T("O_NOCTTY"),     { O_NOCTTY} },
#endif
#if defined(O_NOFOLLOW)
	{ HAWK_T("O_NOFOLLOW"),   { O_NOFOLLOW } },
#endif
#if defined(O_NONBLOCK)
	{ HAWK_T("O_NONBLOCK"),   { O_NONBLOCK } },
#endif
#if defined(O_RDONLY)
	{ HAWK_T("O_RDONLY"),     { O_RDONLY } },
#endif
#if defined(O_RDWR)
	{ HAWK_T("O_RDWR"),       { O_RDWR } },
#endif
#if defined(O_SYNC)
	{ HAWK_T("O_SYNC"),       { O_SYNC } },
#endif
#if defined(O_TRUNC)
	{ HAWK_T("O_TRUNC"),      { O_TRUNC } },
#endif
#if defined(O_WRONLY)
	{ HAWK_T("O_WRONLY"),     { O_WRONLY } },
#endif

	{ HAWK_T("RC_EACCES"),     { -HAWK_EACCES } },
	{ HAWK_T("RC_EAGAIN"),     { -HAWK_EAGAIN } },
	{ HAWK_T("RC_EBUFFULL"),   { -HAWK_EBUFFULL} },
	{ HAWK_T("RC_EBUSY"),      { -HAWK_EBUSY} },
	{ HAWK_T("RC_ECHILD"),     { -HAWK_ECHILD } },
	{ HAWK_T("RC_EECERR"),     { -HAWK_EECERR } },
	{ HAWK_T("RC_EEXIST"),     { -HAWK_EEXIST } },
	{ HAWK_T("RC_EINPROG"),    { -HAWK_EINPROG } },
	{ HAWK_T("RC_EINTERN"),    { -HAWK_EINTERN } },
	{ HAWK_T("RC_EINTR"),      { -HAWK_EINTR } },
	{ HAWK_T("RC_EINVAL"),     { -HAWK_EINVAL } },
	{ HAWK_T("RC_EIOERR"),     { -HAWK_EIOERR } },
	{ HAWK_T("RC_EISDIR"),     { -HAWK_EISDIR } },
	{ HAWK_T("RC_ENOENT"),     { -HAWK_ENOENT } },
	{ HAWK_T("RC_ENOHND"),     { -HAWK_ENOHND } },
	{ HAWK_T("RC_ENOIMPL"),    { -HAWK_ENOIMPL } },
	{ HAWK_T("RC_ENOMEM"),     { -HAWK_ENOMEM } },
	{ HAWK_T("RC_ENOSUP"),     { -HAWK_ENOSUP } },
	{ HAWK_T("RC_ENOTDIR"),    { -HAWK_ENOTDIR } },
	{ HAWK_T("RC_EOTHER"),     { -HAWK_EOTHER } },
	{ HAWK_T("RC_EPERM"),      { -HAWK_EPERM } },
	{ HAWK_T("RC_EPIPE"),      { -HAWK_EPIPE } },
	{ HAWK_T("RC_ESTATE"),     { -HAWK_ESTATE } },
	{ HAWK_T("RC_ESYSERR"),    { -HAWK_ESYSERR } },
	{ HAWK_T("RC_ETMOUT"),     { -HAWK_ETMOUT } },

	{ HAWK_T("SEEK_CUR"),      { SEEK_CUR } },
	{ HAWK_T("SEEK_END"),      { SEEK_END } },
	{ HAWK_T("SEEK_SET"),      { SEEK_SET } },

	{ HAWK_T("SHUT_RD"),       { SHUT_RD } },
	{ HAWK_T("SHUT_RDWR"),     { SHUT_RDWR } },
	{ HAWK_T("SHUT_WR"),       { SHUT_WR } },

	{ HAWK_T("SIGABRT"),       { SIGABRT } },
	{ HAWK_T("SIGALRM"),       { SIGALRM } },
	{ HAWK_T("SIGHUP"),        { SIGHUP } },
	{ HAWK_T("SIGINT"),        { SIGINT } },
	{ HAWK_T("SIGKILL"),       { SIGKILL } },
	{ HAWK_T("SIGQUIT"),       { SIGQUIT } },
	{ HAWK_T("SIGSEGV"),       { SIGSEGV } },
	{ HAWK_T("SIGTERM"),       { SIGTERM } },

	{ HAWK_T("SOCK_CLOEXEC"),  { SOCK_CLOEXEC } },
	{ HAWK_T("SOCK_DGRAM"),    { SOCK_DGRAM } },
	{ HAWK_T("SOCK_NONBLOCK"), { SOCK_NONBLOCK } },
	{ HAWK_T("SOCK_STREAM"),   { SOCK_STREAM } },

	{ HAWK_T("SOL_SOCKET"),    { SOL_SOCKET } },

	{ HAWK_T("SO_BROADCAST"),  { SO_BROADCAST } },
	{ HAWK_T("SO_DONTROUTE"),  { SO_DONTROUTE } },
	{ HAWK_T("SO_KEEPALIVE"),  { SO_KEEPALIVE } },
	{ HAWK_T("SO_RCVBUF"),     { SO_RCVBUF } },
	{ HAWK_T("SO_RCVTIMEO"),   { SO_RCVTIMEO } },
	{ HAWK_T("SO_REUSEADDR"),  { SO_REUSEADDR } },
	{ HAWK_T("SO_REUSEPORT"),  { SO_REUSEPORT } },
	{ HAWK_T("SO_SNDBUF"),     { SO_SNDBUF } },
	{ HAWK_T("SO_SNDTIMEO"),   { SO_SNDTIMEO } },

	{ HAWK_T("STRFTIME_UTC"),  { STRFTIME_UTC } },

	{ HAWK_T("WNOHANG"),       { WNOHANG } }
};

static int query (hawk_mod_t* mod, hawk_t* awk, const hawk_ooch_t* name, hawk_mod_sym_t* sym)
{
	int left, right, mid, n;

	left = 0; right = HAWK_COUNTOF(fnctab) - 1;

	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = hawk_comp_oocstr(fnctab[mid].name, name, 0);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_FNC;
			sym->u.fnc = fnctab[mid].info;
			return 0;
		}
	}

	left = 0; right = HAWK_COUNTOF(inttab) - 1;
	while (left <= right)
	{
		mid = left + (right - left) / 2;

		n = hawk_comp_oocstr(inttab[mid].name, name, 0);
		if (n > 0) right = mid - 1; 
		else if (n < 0) left = mid + 1;
		else
		{
			sym->type = HAWK_MOD_INT;
			sym->u.in = inttab[mid].info;
			return 0;
		}
	}

	hawk_seterrfmt (awk, HAWK_NULL, HAWK_ENOENT, HAWK_T("'%js' not found"), name);
	return -1;
}

/* TODO: proper resource management */

static int init (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	mod_ctx_t* mctx = (mod_ctx_t*)mod->ctx;
	rtx_data_t data;

	mctx->log.type = SYSLOG_LOCAL;
	mctx->log.syslog_opened = 0;
	mctx->log.sck = -1; 

	HAWK_MEMSET (&data, 0, HAWK_SIZEOF(data));
	if (hawk_rbt_insert(mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx), &data, HAWK_SIZEOF(data)) == HAWK_NULL) return -1;

	return 0;
}

static void fini (hawk_mod_t* mod, hawk_rtx_t* rtx)
{
	/* TODO: 
	for (each pid for rtx) kill (pid, SIGKILL);
	for (each pid for rtx) waitpid (pid, HAWK_NULL, 0);
	*/

	mod_ctx_t* mctx = (mod_ctx_t*)mod->ctx;
	hawk_rbt_pair_t* pair;

	/* garbage clean-up */
	pair = hawk_rbt_search(mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx));
	if (pair)
	{
		rtx_data_t* data;
		sys_node_t* sys_node, * sys_next;

		data = (rtx_data_t*)HAWK_RBT_VPTR(pair);

		sys_node = data->sys_list.head;
		while (sys_node)
		{
			sys_next = sys_node->next;
			free_sys_node (rtx, &data->sys_list, sys_node);
			sys_node = sys_next;
		}

		if (data->sys_list.ctx.readbuf)
		{
			hawk_rtx_freemem (rtx, data->sys_list.ctx.readbuf);
			data->sys_list.ctx.readbuf = HAWK_NULL;
			data->sys_list.ctx.readbuf_capa = 0;
		}
		hawk_rbt_delete (mctx->rtxtab, &rtx, HAWK_SIZEOF(rtx));
	}


#if defined(ENABLE_SYSLOG)
	if (mctx->log.syslog_opened) 
	{
		/* closelog() only if openlog() has been called explicitly.
		 * if you call writelog() functions without openlog() and
		 * end yoru program without closelog(), the program may leak
		 * some resources created by the writelog() function. (e.g.
		 * socket to /dev/log) */
		closelog ();
		mctx->log.syslog_opened = 0;
	}
#endif
	
	if (mctx->log.sck >= 0)
	{
	#if defined(_WIN32)
		/* TODO: implement this */
	#else
		close (mctx->log.sck);
	#endif
		mctx->log.sck = -1;
	}

	if (mctx->log.dmsgbuf)
	{
		hawk_becs_close (mctx->log.dmsgbuf);
		mctx->log.dmsgbuf = HAWK_NULL;
	}

	if (mctx->log.ident) 
	{
		hawk_rtx_freemem (rtx, mctx->log.ident);
		mctx->log.ident = HAWK_NULL;
	}
}

static void unload (hawk_mod_t* mod, hawk_t* awk)
{
	mod_ctx_t* mctx = (mod_ctx_t*)mod->ctx;

	HAWK_ASSERT (HAWK_RBT_SIZE(mctx->rtxtab) == 0);
	hawk_rbt_close (mctx->rtxtab);

	hawk_freemem (awk, mctx);
}

int hawk_mod_sys (hawk_mod_t* mod, hawk_t* awk)
{
	hawk_rbt_t* rbt;

	mod->query = query;
	mod->unload = unload;

	mod->init = init;
	mod->fini = fini;

	mod->ctx = hawk_callocmem(awk, HAWK_SIZEOF(mod_ctx_t));
	if (!mod->ctx) return -1;

	rbt = hawk_rbt_open(hawk_getgem(awk), 0, 1, 1);
	if (rbt == HAWK_NULL) 
	{
		hawk_freemem (awk, mod->ctx);
		return -1;
	}
	hawk_rbt_setstyle (rbt, hawk_get_rbt_style(HAWK_RBT_STYLE_INLINE_COPIERS));

	((mod_ctx_t*)mod->ctx)->rtxtab = rbt;
	return 0;
}
