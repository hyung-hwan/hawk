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

#include <hawk-pio.h>
#include "hawk-prv.h"

#if defined(_WIN32)
#	include <windows.h>
#	include <tchar.h>
#elif defined(__OS2__)
#	define INCL_DOSQUEUES
#	define INCL_DOSPROCESS
#	define INCL_DOSERRORS
#	include <os2.h>
#elif defined(__DOS__)
#	include <io.h>
#	include <errno.h>
#else
#	include "syscall.h"
#	if defined(HAVE_SPAWN_H)
#		include <spawn.h>
#	endif
#	if defined(HAVE_CRT_EXTERNS_H)
#		include <crt_externs.h> /* MacOSX/darwin. _NSGetEnviron() */
#	endif
#endif

static hawk_ooi_t pio_input (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size);
static hawk_ooi_t pio_output (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size);

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
static int get_highest_fd (hawk_pio_t* pio)
{
#if defined(HAVE_GETRLIMIT)
	struct rlimit rlim;
#endif
	int fd = -1;
	HAWK_DIR* d;

#if defined(F_MAXFD)
	fd = HAWK_FCNTL(0, F_MAXFD, 0);
	if (fd >= 0) return fd;
#endif

	/* will getting the highest file descriptor be faster than
	 * attempting to close any files descriptors less than the 
	 * system limit? */	

	d = HAWK_OPENDIR(HAWK_BT("/proc/self/fd"));
	if (!d)
	{
		hawk_bch_t buf[64];
		hawk_gem_fmttobcstr (pio->gem, buf, HAWK_COUNTOF(buf), HAWK_BT("/proc/%d/fd"), HAWK_GETPID());
		d = HAWK_OPENDIR (buf);
		if (!d) d = HAWK_OPENDIR(HAWK_BT("/dev/fd")); /* Darwin, FreeBSD */
	}

	if (d)
	{
		int maxfd = -1;
		hawk_dirent_t* de;
		while ((de = HAWK_READDIR(d)))
		{
			hawk_int_t l;
			const hawk_bch_t* endptr;

			if (de->d_name[0] == HAWK_BT('.')) continue;

			l = hawk_bchars_to_int(de->d_name, hawk_count_bcstr(de->d_name), 10, &endptr, 0);
			if (*endptr == HAWK_BT('\0'))
			{
				fd = (int)l;
				if ((hawk_intptr_t)fd == l && fd != HAWK_DIRFD(d))
				{
					if (fd > maxfd) maxfd = fd;
				}
			}
		}

		HAWK_CLOSEDIR (d);
		return maxfd;
	}

/* TODO: should i also use getdtablesize() if available? */

#if defined(HAVE_GETRLIMIT)
	if (HAWK_GETRLIMIT(RLIMIT_NOFILE, &rlim) <= -1 ||
	    rlim.rlim_max == RLIM_INFINITY) 
	{
	#if defined(HAVE_SYSCONF)
		fd = sysconf(_SC_OPEN_MAX);
	#endif
	}
	else fd = rlim.rlim_max;
#elif defined(HAVE_SYSCONF)
	fd = sysconf(_SC_OPEN_MAX);
#endif
	if (fd <= -1) fd = 1024; /* fallback */

	/* F_MAXFD is the highest fd. but RLIMIT_NOFILE and 
	 * _SC_OPEN_MAX returnes the maximum number of file 
	 * descriptors. make adjustment */
	if (fd > 0) fd--; 

	return fd;
}

static int close_open_fds_using_proc (hawk_pio_t* pio, int* excepts, hawk_oow_t count)
{
	HAWK_DIR* d;

	/* will getting the highest file descriptor be faster than
	 * attempting to close any files descriptors less than the 
	 * system limit? */	

	d = HAWK_OPENDIR(HAWK_BT("/proc/self/fd"));
	if (!d)
	{
		hawk_bch_t buf[64];
		hawk_gem_fmttobcstr (pio->gem, buf, HAWK_COUNTOF(buf), HAWK_BT("/proc/%d/fd"), HAWK_GETPID());
		d = HAWK_OPENDIR(buf);
	#if !defined(_SCO_DS)
		/* on SCO OpenServer, a range of file descriptors starting from 0 are
		 * listed under /dev/fd regardless of opening state. And some high 
		 * numbered descriptors are not listed. not reliable */

		if (!d) d = HAWK_OPENDIR(HAWK_BT("/dev/fd")); /* Darwin, FreeBSD */
	#endif
	}

	if (d)
	{
		hawk_dirent_t* de;
		while ((de = HAWK_READDIR(d)))
		{
			hawk_int_t l;
			const hawk_bch_t* endptr;

			if (de->d_name[0] == HAWK_BT('.')) continue;

			l = hawk_bchars_to_int(de->d_name, hawk_count_bcstr(de->d_name), 10, &endptr, 0);
			if (*endptr == HAWK_BT('\0'))
			{
				int fd = (int)l;
				if ((hawk_intptr_t)fd == l && fd != HAWK_DIRFD(d) && fd > 2)
				{
					hawk_oow_t i;

					for (i = 0; i < count; i++)
					{
						if (fd == excepts[i]) goto skip_close;
					}

					HAWK_CLOSE (fd);

				skip_close:
					;
				}
			}
		}

		HAWK_CLOSEDIR (d);
		return 0;
	}

	return -1;
}
#endif

hawk_pio_t* hawk_pio_open (hawk_gem_t* gem, hawk_oow_t xtnsize, const hawk_ooch_t* cmd, int flags)
{
	hawk_pio_t* pio;

	pio = (hawk_pio_t*)hawk_gem_allocmem(gem, HAWK_SIZEOF(hawk_pio_t) + xtnsize);
	if (pio)
	{
		if (hawk_pio_init(pio, gem, cmd, flags) <= -1)
		{
			hawk_gem_freemem (gem, pio);
			pio = HAWK_NULL;
		}
		else HAWK_MEMSET (pio + 1, 0, xtnsize);
	}

	return pio;
}

void hawk_pio_close (hawk_pio_t* pio)
{
	hawk_pio_fini (pio);
	hawk_gem_freemem (pio->gem, pio);
}

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
struct param_t
{
	hawk_bch_t* mcmd;
	hawk_bch_t* fixed_argv[4];
	hawk_bch_t** argv;

#if defined(HAWK_OOCH_IS_BCH)
	/* nothing extra */
#else
	hawk_bch_t fixed_mbuf[64];
#endif
};
typedef struct param_t param_t;

static void free_param (hawk_pio_t* pio, param_t* param)
{
	if (param->argv && param->argv != param->fixed_argv) 
		hawk_gem_freemem (pio->gem, param->argv);
	if (param->mcmd) hawk_gem_freemem (pio->gem, param->mcmd);
}

static int make_param (hawk_pio_t* pio, const hawk_ooch_t* cmd, int flags, param_t* param)
{
#if defined(HAWK_OOCH_IS_BCH)
	hawk_bch_t* mcmd = HAWK_NULL;
#else
	hawk_bch_t* mcmd = HAWK_NULL;
	hawk_ooch_t* wcmd = HAWK_NULL;
#endif
	int fcnt = 0;

	HAWK_MEMSET (param, 0, HAWK_SIZEOF(*param));

#if defined(HAWK_OOCH_IS_BCH)
	if (flags & HAWK_PIO_SHELL) mcmd = (hawk_ooch_t*)cmd;
	else
	{
		mcmd = hawk_gem_dupoocstr(pio->gem, cmd, HAWK_NULL);
		if (mcmd == HAWK_NULL) goto oops;

		fcnt = hawk_split_oocstr(mcmd, HAWK_T(""), HAWK_T('\"'), HAWK_T('\"'), HAWK_T('\\')); 
		if (fcnt <= 0) 
		{
			/* no field or an error */
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
			goto oops; 
		}
	}
#else
	if (flags & HAWK_PIO_BCSTRCMD) 
	{
		/* the cmd is flagged to be of hawk_bch_t 
		 * while the default character type is hawk_uch_t. */

		if (flags & HAWK_PIO_SHELL) mcmd = (hawk_bch_t*)cmd;
		else
		{
			mcmd = hawk_gem_dupbcstr(pio->gem, (const hawk_bch_t*)cmd, HAWK_NULL);
			if (mcmd == HAWK_NULL) goto oops;

			fcnt = hawk_split_bcstr(mcmd, "", '\"', '\"', '\\'); 
			if (fcnt <= 0) 
			{
				/* no field or an error */
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
				goto oops; 
			}
		}
	}
	else
	{
		hawk_oow_t n, mn, wl;

		if (flags & HAWK_PIO_SHELL)
		{
			/* use the cmgr of the gem, not pio->cmgr */
			if (hawk_conv_ucstr_to_bcstr_with_cmgr(cmd, &wl, HAWK_NULL, &mn, pio->gem->cmgr) <= -1)
			{
				/* cmd has illegal sequence */
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
				goto oops;
			}
		}
		else
		{
			wcmd = hawk_gem_dupoocstr(pio->gem, cmd, HAWK_NULL);
			if (wcmd == HAWK_NULL) goto oops;

			fcnt = hawk_split_oocstr(wcmd, HAWK_T(""), HAWK_T('\"'), HAWK_T('\"'), HAWK_T('\\')); 
			if (fcnt <= 0)
			{
				/* no field or an error */
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
				goto oops;
			}
			
			/* calculate the length of the string after splitting */
			for (wl = 0, n = fcnt; n > 0; )
			{
				if (wcmd[wl++] == HAWK_T('\0')) n--;
			}

			if (hawk_conv_uchars_to_bchars_with_cmgr(wcmd, &wl, HAWK_NULL, &mn, pio->gem->cmgr) <= -1) 
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
				goto oops;
			}
		}

		/* prepare to reserve 1 more slot for the terminating '\0'
		 * by incrementing mn by 1. */
		mn = mn + 1;

		if (mn <= HAWK_COUNTOF(param->fixed_mbuf))
		{
			mcmd = param->fixed_mbuf;
			mn = HAWK_COUNTOF(param->fixed_mbuf);
		}
		else
		{
			mcmd = hawk_gem_allocmem(pio->gem, mn * HAWK_SIZEOF(*mcmd));
			if (mcmd == HAWK_NULL) goto oops;
		}

		if (flags & HAWK_PIO_SHELL)
		{
			/*HAWK_ASSERT (wcmd == HAWK_NULL);*/
			/* hawk_wcstombs() should succeed as 
			 * it was successful above */
			hawk_conv_ucstr_to_bcstr_with_cmgr (cmd, &wl, mcmd, &mn, pio->gem->cmgr);
			/* hawk_wcstombs() null-terminate mcmd */
		}
		else
		{
			HAWK_ASSERT (wcmd != HAWK_NULL);
			/* hawk_wcsntombsn() should succeed as 
			 * it was was successful above */
			hawk_conv_uchars_to_bchars_with_cmgr (wcmd, &wl, mcmd, &mn, pio->gem->cmgr);
			/* hawk_wcsntombsn() doesn't null-terminate mcmd */
			mcmd[mn] = '\0';

			hawk_gem_freemem (pio->gem, wcmd);
			wcmd = HAWK_NULL;
		}
	}
#endif

	if (flags & HAWK_PIO_SHELL)
	{
		param->argv = param->fixed_argv;
		param->argv[0] = HAWK_BT("/bin/sh");
		param->argv[1] = HAWK_BT("-c");
		param->argv[2] = mcmd;
		param->argv[3] = HAWK_NULL;
	}
	else
	{
		int i;
		hawk_bch_t** argv;
		hawk_bch_t* mcmdptr;

		if (fcnt < HAWK_COUNTOF(param->fixed_argv))
		{
			param->argv = param->fixed_argv;
		}
		else
		{
			param->argv = hawk_gem_allocmem(pio->gem, (fcnt + 1) * HAWK_SIZEOF(argv[0]));
			if (param->argv == HAWK_NULL) goto oops;
		}

		mcmdptr = mcmd;
		for (i = 0; i < fcnt; i++)
		{
			param->argv[i] = mcmdptr;
			while (*mcmdptr != HAWK_BT('\0')) mcmdptr++;
			mcmdptr++;
		}
		param->argv[i] = HAWK_NULL;
	}

#if defined(HAWK_OOCH_IS_BCH)
	if (mcmd && mcmd != (hawk_bch_t*)cmd) param->mcmd = mcmd;
#else
	if (mcmd && mcmd != (hawk_bch_t*)cmd && 
	    mcmd != param->fixed_mbuf) param->mcmd = mcmd;
#endif
	return 0;

oops:
#if defined(HAWK_OOCH_IS_BCH)
	if (mcmd && mcmd != cmd) hawk_gem_freemem (pio->gem, mcmd);
#else
	if (mcmd && mcmd != (hawk_bch_t*)cmd && 
	    mcmd != param->fixed_mbuf) hawk_gem_freemem (pio->gem, mcmd);
	if (wcmd) hawk_gem_freemem (pio->gem, wcmd);
#endif
	return -1;
}

static int assert_executable (hawk_pio_t* pio, const hawk_bch_t* path)
{
	hawk_lstat_t st;

	if (HAWK_ACCESS(path, X_OK) <= -1) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}

	/*if (HAWK_LSTAT(path, &st) <= -1)*/
	if (HAWK_STAT(path, &st) <= -1)
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}

	if (!S_ISREG(st.st_mode)) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EACCES);
		return -1;
	}

	return 0;
}

static HAWK_INLINE int is_fd_valid (int fd)
{
	return HAWK_FCNTL (fd, F_GETFD, 0) != -1 || errno != EBADF;
}

static HAWK_INLINE int is_fd_valid_and_nocloexec (int fd)
{
	int flags = HAWK_FCNTL (fd, F_GETFD, 0);
	if (flags == -1)
	{
		if (errno == EBADF) return 0; /* invalid. return false */
		return -1; /* unknown. true but negative to indicate unknown */
	}
	return !(flags & FD_CLOEXEC)? 1: 0;
}

static hawk_pio_pid_t standard_fork_and_exec (hawk_pio_t* pio, int pipes[], param_t* param)
{
	hawk_pio_pid_t pid;

#if defined(HAVE_CRT_EXTERNS_H)
#	define environ (*(_NSGetEnviron()))
#else
	extern char** environ;
#endif

	pid = HAWK_FORK();
	if (pid <= -1) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}

	if (pid == 0)
	{
		/* child */
		hawk_pio_hnd_t devnull = -1;

		if (!(pio->flags & HAWK_PIO_NOCLOEXEC))
		{
			if (close_open_fds_using_proc(pio, pipes, 6) <= -1)
			{
				int fd = get_highest_fd(pio);

				/* close all other unknown open handles except 
				 * stdin/out/err and the pipes. */
				while (fd > 2)
				{
					if (fd != pipes[0] && fd != pipes[1] &&
					    fd != pipes[2] && fd != pipes[3] &&
					    fd != pipes[4] && fd != pipes[5]) 
					{
						HAWK_CLOSE (fd);
					}
					fd--;
				}
			}
		}

		if (pio->flags & HAWK_PIO_WRITEIN)
		{
			/* child should read */
			HAWK_CLOSE (pipes[1]);
			pipes[1] = HAWK_PIO_HND_NIL;
			if (HAWK_DUP2 (pipes[0], 0) <= -1) goto child_oops;
			HAWK_CLOSE (pipes[0]);
			pipes[0] = HAWK_PIO_HND_NIL;
		}

		if (pio->flags & HAWK_PIO_READOUT)
		{
			/* child should write */
			HAWK_CLOSE (pipes[2]);
			pipes[2] = HAWK_PIO_HND_NIL;
			if (HAWK_DUP2 (pipes[3], 1) <= -1) goto child_oops;

			if (pio->flags & HAWK_PIO_ERRTOOUT)
			{
				if (HAWK_DUP2 (pipes[3], 2) <= -1) goto child_oops;
			}

			HAWK_CLOSE (pipes[3]); 
			pipes[3] = HAWK_PIO_HND_NIL;
		}

		if (pio->flags & HAWK_PIO_READERR)
		{
			/* child should write */
			HAWK_CLOSE (pipes[4]); 
			pipes[4] = HAWK_PIO_HND_NIL;
			if (HAWK_DUP2 (pipes[5], 2) <= -1) goto child_oops;

			if (pio->flags & HAWK_PIO_OUTTOERR)
			{
				if (HAWK_DUP2 (pipes[5], 1) <= -1) goto child_oops;
			}

			HAWK_CLOSE (pipes[5]);
			pipes[5] = HAWK_PIO_HND_NIL;
		}

		if ((pio->flags & HAWK_PIO_INTONUL) || 
		    (pio->flags & HAWK_PIO_OUTTONUL) ||
		    (pio->flags & HAWK_PIO_ERRTONUL))
		{
		#if defined(O_LARGEFILE)
			devnull = HAWK_OPEN (HAWK_BT("/dev/null"), O_RDWR|O_LARGEFILE, 0);
		#else
			devnull = HAWK_OPEN (HAWK_BT("/dev/null"), O_RDWR, 0);
		#endif
			if (devnull <= -1) goto child_oops;
		}

		if ((pio->flags & HAWK_PIO_INTONUL)  &&
		    HAWK_DUP2(devnull,0) <= -1) goto child_oops;
		if ((pio->flags & HAWK_PIO_OUTTONUL) &&
		    HAWK_DUP2(devnull,1) <= -1) goto child_oops;
		if ((pio->flags & HAWK_PIO_ERRTONUL) &&
		    HAWK_DUP2(devnull,2) <= -1) goto child_oops;

		if ((pio->flags & HAWK_PIO_INTONUL) || 
		    (pio->flags & HAWK_PIO_OUTTONUL) ||
		    (pio->flags & HAWK_PIO_ERRTONUL)) 
		{
			HAWK_CLOSE (devnull);
			devnull = -1;
		}

		if (pio->flags & HAWK_PIO_DROPIN) HAWK_CLOSE(0);
		if (pio->flags & HAWK_PIO_DROPOUT) HAWK_CLOSE(1);
		if (pio->flags & HAWK_PIO_DROPERR) HAWK_CLOSE(2);


		if (pio->flags & HAWK_PIO_FNCCMD)
		{
			/* -----------------------------------------------
			 * the function pointer to execute has been given.
			 * -----------------------------------------------*/
			hawk_pio_fnc_t* fnc = (hawk_pio_fnc_t*)param;
			int retx;

			retx = fnc->ptr(fnc->ctx);
			if (devnull >= 0) HAWK_CLOSE (devnull);
			HAWK_EXIT (retx);
		}
		else
		{
			HAWK_EXECVE (param->argv[0], param->argv, environ);

			/* if exec fails, free 'param' parameter which is an inherited pointer */
			free_param (pio, param); 
		}

	child_oops:
		if (devnull >= 0) HAWK_CLOSE (devnull);
		HAWK_EXIT (128);
	}

	return pid;
}

#endif

static int set_pipe_nonblock (hawk_pio_t* pio, hawk_pio_hnd_t fd, int enabled)
{
#if defined(O_NONBLOCK)

	int flag = HAWK_FCNTL (fd, F_GETFL, 0);
	if (flag >= 0) flag = HAWK_FCNTL (fd, F_SETFL, (enabled? (flag | O_NONBLOCK): (flag & ~O_NONBLOCK)));
	if (flag <= -1) hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
	return flag;

#else
	hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
#endif
}


int hawk_pio_init (hawk_pio_t* pio, hawk_gem_t* gem, const hawk_ooch_t* cmd, int flags)
{
	hawk_pio_hnd_t handle[6] /*= 
	{ 
		HAWK_PIO_HND_NIL, 
		HAWK_PIO_HND_NIL,
		HAWK_PIO_HND_NIL,
		HAWK_PIO_HND_NIL,
		HAWK_PIO_HND_NIL,
		HAWK_PIO_HND_NIL
	}*/;

	hawk_tio_t* tio[3] /*= 
	{ 
		HAWK_NULL, 
		HAWK_NULL, 
		HAWK_NULL 
	}*/;

	int i, minidx = -1, maxidx = -1;

#if defined(_WIN32)
	SECURITY_ATTRIBUTES secattr; 
	PROCESS_INFORMATION procinfo;
	STARTUPINFO startup;
	HANDLE windevnul = INVALID_HANDLE_VALUE;
	BOOL apiret;
	hawk_ooch_t* dupcmd;
	int create_retried;

#elif defined(__OS2__)
	APIRET rc;
	ULONG pipe_size = 4096;
	UCHAR load_error[CCHMAXPATH];
	RESULTCODES child_rc;
	HFILE old_in = HAWK_PIO_HND_NIL;
	HFILE old_out = HAWK_PIO_HND_NIL;
	HFILE old_err = HAWK_PIO_HND_NIL;
	HFILE std_in = 0, std_out = 1, std_err = 2;
	hawk_bch_t* cmd_line = HAWK_NULL;
	hawk_bch_t* cmd_file;
	HFILE os2devnul = (HFILE)-1;

#elif defined(__DOS__)

	/* DOS not multi-processed. can't support pio */

#elif defined(HAVE_POSIX_SPAWN) && !(defined(HAWK_SYSCALL0) && defined(SYS_vfork)) 
	posix_spawn_file_actions_t fa;
	int fa_inited = 0;
	int pserr;
	posix_spawnattr_t psattr;
	hawk_pio_pid_t pid;
	param_t param;
	#if defined(HAVE_CRT_EXTERNS_H)
	#define environ (*(_NSGetEnviron()))
	#else
	extern char** environ;
	#endif
#elif defined(HAWK_SYSCALL0) && defined(SYS_vfork)
	hawk_pio_pid_t pid;
	param_t param;
	#if defined(HAVE_CRT_EXTERNS_H)
	#define environ (*(_NSGetEnviron()))
	#else
	extern char** environ;
	#endif
	int highest_fd;
	int dummy;
#else
	hawk_pio_pid_t pid;
	param_t param;
	#if defined(HAVE_CRT_EXTERNS_H)
	#define environ (*(_NSGetEnviron()))
	#else
	extern char** environ;
	#endif
#endif

	HAWK_MEMSET (pio, 0, HAWK_SIZEOF(*pio));
	pio->gem = gem;
	pio->flags = flags;

	handle[0] = HAWK_PIO_HND_NIL;
	handle[1] = HAWK_PIO_HND_NIL;
	handle[2] = HAWK_PIO_HND_NIL;
	handle[3] = HAWK_PIO_HND_NIL;
	handle[4] = HAWK_PIO_HND_NIL;
	handle[5] = HAWK_PIO_HND_NIL;

	tio[0] = HAWK_NULL;
	tio[1] = HAWK_NULL;
	tio[2] = HAWK_NULL;

#if defined(_WIN32)
	/* http://msdn.microsoft.com/en-us/library/ms682499(VS.85).aspx */

	secattr.nLength = HAWK_SIZEOF(secattr);
	secattr.bInheritHandle = TRUE;
	secattr.lpSecurityDescriptor = HAWK_NULL;

	if (flags & HAWK_PIO_WRITEIN)
	{
		/* child reads, parent writes */
		if (CreatePipe(&handle[0], &handle[1], &secattr, 0) == FALSE) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			goto oops;
		}

		/* don't inherit write handle */
		if (SetHandleInformation(handle[1], HANDLE_FLAG_INHERIT, 0) == FALSE) 
		{
			DWORD e = GetLastError();
			if (e != ERROR_CALL_NOT_IMPLEMENTED)
			{
				/* SetHandleInformation() is not implemented on win9x.
				 * so let's care only if it is implemented */
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(e));
				goto oops;
			}
		}

		minidx = 0; maxidx = 1;
	}

	if (flags & HAWK_PIO_READOUT)
	{
		/* child writes, parent reads */
		if (CreatePipe(&handle[2], &handle[3], &secattr, 0) == FALSE) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			goto oops;
		}

		/* don't inherit read handle */
		if (SetHandleInformation(handle[2], HANDLE_FLAG_INHERIT, 0) == FALSE) 
		{
			DWORD e = GetLastError();
			if (e != ERROR_CALL_NOT_IMPLEMENTED)
			{
				/* SetHandleInformation() is not implemented on win9x.
				 * so let's care only if it is implemented */
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(e));
				goto oops;
			}
		}

		if (minidx == -1) minidx = 2;
		maxidx = 3;
	}

	if (flags & HAWK_PIO_READERR)
	{
		/* child writes, parent reads */
		if (CreatePipe(&handle[4], &handle[5], &secattr, 0) == FALSE)
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			goto oops;
		}

		/* don't inherit read handle */
		if (SetHandleInformation(handle[4], HANDLE_FLAG_INHERIT, 0) == FALSE)
		{
			DWORD e = GetLastError();
			if (e != ERROR_CALL_NOT_IMPLEMENTED)
			{
				/* SetHandleInformation() is not implemented on win9x.
				 * so let's care only if it is implemented */
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(e));
				goto oops;
			}
		}

		if (minidx == -1) minidx = 4;
		maxidx = 5;
	}

	if (maxidx == -1) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
		goto oops;
	}

	if ((flags & HAWK_PIO_INTONUL) || 
	    (flags & HAWK_PIO_OUTTONUL) ||
	    (flags & HAWK_PIO_ERRTONUL))
	{
		windevnul = CreateFile(
			HAWK_T("NUL"), GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 
			&secattr, OPEN_EXISTING, 0, NULL
		);
		if (windevnul == INVALID_HANDLE_VALUE) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
			goto oops;
		}
	}

	HAWK_MEMSET (&procinfo, 0, HAWK_SIZEOF(procinfo));
	HAWK_MEMSET (&startup, 0, HAWK_SIZEOF(startup));

	startup.cb = HAWK_SIZEOF(startup);

	/*
	startup.hStdInput = INVALID_HANDLE_VALUE;
	startup.hStdOutput = INVALID_HANDLE_VALUE;
	startup.hStdOutput = INVALID_HANDLE_VALUE;
	*/

	startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	startup.hStdOutput = GetStdHandle(STD_ERROR_HANDLE);
	if (startup.hStdInput == INVALID_HANDLE_VALUE ||
	    startup.hStdOutput == INVALID_HANDLE_VALUE ||
	    startup.hStdError == INVALID_HANDLE_VALUE) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		goto oops;
	}

	if (flags & HAWK_PIO_WRITEIN) 
	{
		startup.hStdInput = handle[0];
	}

	if (flags & HAWK_PIO_READOUT)
	{
		startup.hStdOutput = handle[3];
		if (flags & HAWK_PIO_ERRTOOUT) startup.hStdError = handle[3];
	}

	if (flags & HAWK_PIO_READERR)
	{
		startup.hStdError = handle[5];
		if (flags & HAWK_PIO_OUTTOERR) startup.hStdOutput = handle[5];
	}

	if (flags & HAWK_PIO_INTONUL) startup.hStdInput = windevnul;
	if (flags & HAWK_PIO_OUTTONUL) startup.hStdOutput = windevnul;
	if (flags & HAWK_PIO_ERRTONUL) startup.hStdError = windevnul;

	if (flags & HAWK_PIO_DROPIN) startup.hStdInput = INVALID_HANDLE_VALUE;
	if (flags & HAWK_PIO_DROPOUT) startup.hStdOutput = INVALID_HANDLE_VALUE;
	if (flags & HAWK_PIO_DROPERR) startup.hStdError = INVALID_HANDLE_VALUE;

	startup.dwFlags |= STARTF_USESTDHANDLES;

	/* there is nothing to do for HAWK_PIO_SHELL as CreateProcess
	 * takes the entire command line */

	create_retried = 0;

create_process:
	if (flags & HAWK_PIO_SHELL) 
	{
		static const hawk_ooch_t* cmdname[] =
		{
			HAWK_T("cmd.exe /c "),
			HAWK_T("command.com /c ")
		};
		static const hawk_bch_t* mcmdname[] =
		{
			HAWK_BT("cmd.exe /c "),
			HAWK_BT("command.com /c ")
		};

	#if defined(HAWK_OOCH_IS_UCH)
		if (flags & HAWK_PIO_BCSTRCMD)
		{
			const hawk_bch_t* x[3];
			x[0] = mcmdname[create_retried];
			x[1] = (const hawk_bch_t*)cmd;
			x[2] = HAWK_NULL;
			dupcmd = hawk_gem_dupbcstrarrtoucstr(pio->gem, x, HAWK_NULL, 0);
		}
		else
	#endif
		{
			const hawk_ooch_t* x[3];
			x[0] = cmdname[create_retried];
			x[1] = cmd;
			x[2] = HAWK_NULL;
			dupcmd = hawk_gem_dupoocstrarr(pio->gem, x, HAWK_NULL);
		}
	}
	else 
	{
	#if defined(HAWK_OOCH_IS_UCH)
		if (flags & HAWK_PIO_BCSTRCMD)
		{
			dupcmd = hawk_gem_dupbtoucstr(pio->gem, (const hawk_bch_t*)cmd, HAWK_NULL, 0);
		}
		else
		{
	#endif
			/* CreateProcess requires command buffer to be read-write. */
			dupcmd = hawk_gem_dupoocstr(pio->gem, cmd, HAWK_NULL);
	#if defined(HAWK_OOCH_IS_UCH)
		}
	#endif
	}

	if (dupcmd == HAWK_NULL) goto oops;

	apiret = CreateProcess (
		HAWK_NULL,  /* LPCTSTR lpApplicationName */
		dupcmd,    /* LPTSTR lpCommandLine */
		HAWK_NULL,  /* LPSECURITY_ATTRIBUTES lpProcessAttributes */
		HAWK_NULL,  /* LPSECURITY_ATTRIBUTES lpThreadAttributes */
		TRUE,      /* BOOL bInheritHandles */
	#if defined(HAWK_OOCH_IS_BCH)
		0,         /* DWORD dwCreationFlags */
	#else
		CREATE_UNICODE_ENVIRONMENT, /* DWORD dwCreationFlags */
	#endif
		HAWK_NULL, /* LPVOID lpEnvironment */
		HAWK_NULL, /* LPCTSTR lpCurrentDirectory */
		&startup, /* LPSTARTUPINFO lpStartupInfo */
		&procinfo /* LPPROCESS_INFORMATION lpProcessInformation */
	);

	hawk_gem_freemem (pio->gem, dupcmd); 
	if (apiret == FALSE) 
	{
		DWORD e = GetLastError();
		if (create_retried == 0 && (flags & HAWK_PIO_SHELL) && 
		    e == ERROR_FILE_NOT_FOUND)
		{
			/* if it failed to exeucte cmd.exe, 
			 * attempt to execute command.com.
			 * this is provision for old windows platforms */
			create_retried = 1;
			goto create_process;
		}

		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(e));
		goto oops;
	}

	if (windevnul != INVALID_HANDLE_VALUE)
	{
		CloseHandle (windevnul); 
		windevnul = INVALID_HANDLE_VALUE;
	}

	if (flags & HAWK_PIO_WRITEIN)
	{
		CloseHandle (handle[0]);
		handle[0] = HAWK_PIO_HND_NIL;
	}
	if (flags & HAWK_PIO_READOUT)
	{
		CloseHandle (handle[3]);
		handle[3] = HAWK_PIO_HND_NIL;
	}
	if (flags & HAWK_PIO_READERR)
	{
		CloseHandle (handle[5]);
		handle[5] = HAWK_PIO_HND_NIL;
	}

	CloseHandle (procinfo.hThread);
	pio->child = procinfo.hProcess;

#elif defined(__OS2__)

	#define DOS_DUP_HANDLE(x,y) HAWK_BLOCK ( \
		rc = DosDupHandle(x,y); \
		if (rc != NO_ERROR) \
		{ \
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc)); \
			goto oops; \
		} \
	)

	if (flags & HAWK_PIO_WRITEIN)
	{
		/* child reads, parent writes */
		rc = DosCreatePipe (&handle[0], &handle[1], pipe_size);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}

		/* the parent writes to handle[1] and the child reads from 
		 * handle[0] inherited. set the flag not to inherit handle[1]. */
		rc = DosSetFHState (handle[1], OPEN_FLAGS_NOINHERIT);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}

		/* Need to do somthing like this to set the flag instead? 
		ULONG state;               
		DosQueryFHState (handle[1], &state);
		DosSetFHState (handle[1], state | OPEN_FLAGS_NOINHERIT); */

		minidx = 0; maxidx = 1;
	}

	if (flags & HAWK_PIO_READOUT)
	{
		/* child writes, parent reads */
		rc = DosCreatePipe (&handle[2], &handle[3], pipe_size);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}

		/* the parent reads from handle[2] and the child writes to 
		 * handle[3] inherited. set the flag not to inherit handle[2] */
		rc = DosSetFHState(handle[2], OPEN_FLAGS_NOINHERIT);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}

		if (minidx == -1) minidx = 2;
		maxidx = 3;
	}

	if (flags & HAWK_PIO_READERR)
	{
		/* child writes, parent reads */
		rc = DosCreatePipe(&handle[4], &handle[5], pipe_size);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}

		/* the parent reads from handle[4] and the child writes to 
		 * handle[5] inherited. set the flag not to inherit handle[4] */
		rc = DosSetFHState (handle[4], OPEN_FLAGS_NOINHERIT);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}

		if (minidx == -1) minidx = 4;
		maxidx = 5;
	}

	if (maxidx == -1) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
		goto oops;
	}

	if ((flags & HAWK_PIO_INTONUL) || 
	    (flags & HAWK_PIO_OUTTONUL) ||
	    (flags & HAWK_PIO_ERRTONUL))
	{
		ULONG action_taken;
		/*
		LONGLONG zero;

		zero.ulLo = 0;
		zero.ulHi = 0;
		*/

		/* TODO: selective between DosOpenL and DosOpen */
		rc = DosOpen/*DosOpenL*/(
			HAWK_BT("NUL"),
			&os2devnul,
			&action_taken,
			0, /*zero,*/
			FILE_NORMAL,
			OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
			OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYNONE,
			0L
		);
		if (rc != NO_ERROR) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
			goto oops;
		}
	}

	/* duplicate the current stdin/out/err to old_in/out/err as a new handle */
	
	rc = DosDupHandle(std_in, &old_in);
	if (rc != NO_ERROR) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		goto oops;
	}
	rc = DosDupHandle(std_out, &old_out);
	if (rc != NO_ERROR) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		DosClose (old_in); old_in = HAWK_PIO_HND_NIL;
		goto oops;
	}
	rc = DosDupHandle(std_err, &old_err);
	if (rc != NO_ERROR) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		DosClose (old_out); old_out = HAWK_PIO_HND_NIL;
		DosClose (old_in); old_in = HAWK_PIO_HND_NIL;
		goto oops;
	}

	/* we must not let our own stdin/out/err duplicated 
	 * into old_in/out/err be inherited */
	DosSetFHState (old_in, OPEN_FLAGS_NOINHERIT);
	DosSetFHState (old_out, OPEN_FLAGS_NOINHERIT); 
	DosSetFHState (old_err, OPEN_FLAGS_NOINHERIT);

	if (flags & HAWK_PIO_WRITEIN)
	{
		/* the child reads from handle[0] inherited and expects it to
		 * be stdin(0). so we duplicate handle[0] to stdin */
		DOS_DUP_HANDLE (handle[0], &std_in);

		/* the parent writes to handle[1] but does not read from handle[0].
		 * so we close it */
		DosClose (handle[0]); handle[0] = HAWK_PIO_HND_NIL;
	}

	if (flags & HAWK_PIO_READOUT)
	{
		/* the child writes to handle[3] inherited and expects it to
		 * be stdout(1). so we duplicate handle[3] to stdout. */
		DOS_DUP_HANDLE (handle[3], &std_out);
		if (flags & HAWK_PIO_ERRTOOUT) DOS_DUP_HANDLE (handle[3], &std_err);
		/* the parent reads from handle[2] but does not write to handle[3].
		 * so we close it */
		DosClose (handle[3]); handle[3] = HAWK_PIO_HND_NIL;
	}

	if (flags & HAWK_PIO_READERR)
	{
		DOS_DUP_HANDLE (handle[5], &std_err);
		if (flags & HAWK_PIO_OUTTOERR) DOS_DUP_HANDLE (handle[5], &std_out);
		DosClose (handle[5]); handle[5] = HAWK_PIO_HND_NIL;
	}

	if (flags & HAWK_PIO_INTONUL) DOS_DUP_HANDLE (os2devnul, &std_in);
	if (flags & HAWK_PIO_OUTTONUL) DOS_DUP_HANDLE (os2devnul, &std_out);
	if (flags & HAWK_PIO_ERRTONUL) DOS_DUP_HANDLE (os2devnul, &std_err);

	if (os2devnul != HAWK_PIO_HND_NIL)
	{
	    	/* close NUL early as we've duplicated it already */
		DosClose (os2devnul); 
		os2devnul = HAWK_PIO_HND_NIL;
	}
	
	/* at this moment, stdin/out/err are already redirected to pipes
	 * if proper flags have been set. we close them selectively if 
	 * dropping is requested */
	if (flags & HAWK_PIO_DROPIN) DosClose (std_in);
	if (flags & HAWK_PIO_DROPOUT) DosClose (std_out);
	if (flags & HAWK_PIO_DROPERR) DosClose (std_err);

	if (flags & HAWK_PIO_SHELL) 
	{
		hawk_oow_t n, mn;

	#if defined(HAWK_OOCH_IS_BCH)
		mn = hawk_count_oocstr(cmd);
	#else
		if (flags & HAWK_PIO_BCSTRCMD)
		{
			mn = hawk_count_bcstr((const hawk_bch_t*)cmd);
		}
		else
		{
			if (hawk_gem_convutobcstr(pio->gem, cmd, &n, HAWK_NULL, &mn) <= -1) goto oops; /* illegal sequence found */
		}
	#endif
		cmd_line = hawk_gem_allocmem(pio->gem, ((11+mn+1+1) * HAWK_SIZEOF(*cmd_line)));
		if (cmd_line == HAWK_NULL) goto oops;

		hawk_copy_bcstr_unlimited (cmd_line, "cmd.exe"); /* cmd.exe\0/c */ 
		hawk_copy_bcstr_unlimited (&cmd_line[8], "/c ");
	#if defined(HAWK_OOCH_IS_BCH)
		hawk_copy_bcstr_unlimited (&cmd_line[11], cmd);
	#else
		if (flags & HAWK_PIO_BCSTRCMD)
		{
			hawk_copy_bcstr_unlimited (&cmd_line[11], (const hawk_bch_t*)cmd);
		}
		else
		{
			mn = mn + 1; /* update the buffer size */
			hawk_gem_convutobcstr (pio->gem, cmd, &n, &cmd_line[11], &mn);
		}
	#endif
		cmd_line[11+mn+1] = '\0'; /* additional \0 after \0 */    
		
		cmd_file = "cmd.exe";
	}
	else
	{
		hawk_bch_t* mptr;
		hawk_oow_t mn;

	#if defined(HAWK_OOCH_IS_BCH)
		const hawk_ooch_t* strarr[3];

		strarr[0] = cmd;
		strarr[1] = HAWK_T(" ");
		strarr[2] = HAWK_NULL;

		mn = hawk_count_oocstr(cmd);
		cmd_line = hawk_dupucstrarr(pio->gem, strarr HAWK_NULL);
		if (cmd_line == HAWK_NULL) goto oops;
	#else
		if (flags & HAWK_PIO_BCSTRCMD)
		{
			const hawk_bch_t* mbsarr[3];

			mbsarr[0] = (const hawk_bch_t*)cmd;
			mbsarr[1] = " ";
			mbsarr[2] = HAWK_NULL;

			mn = hawk_count_bcstr((const hawk_bch_t*)cmd);
			cmd_line = hawk_dupbcstrarr(pio->gem, mbsarr, HAWK_NULL);
			if (cmd_line == HAWK_NULL) goto oops;
		}
		else
		{
			cmd_line = hawk_gem_duputobcstr(gem, cmd, &mn);
			if (!cmd_line) goto oops; /* illegal sequence in cmd or out of memory */
		}
	#endif

		/* TODO: enhance this part by:
		 *          supporting file names containing whitespaces.
		 *          detecting the end of the file name better.
		 *          doing better parsing of the command line.
		 */

		/* NOTE: you must separate the command name and the parameters 
		 *       with a space. "pstat.exe /c" is ok while "pstat.exe/c"
		 *       is not. */
		mptr = hawk_mbspbrk(cmd_line, HAWK_BT(" \t"));
		if (mptr) *mptr = '\0';
		cmd_line[mn+1] = '\0'; /* the second '\0' at the end */
		cmd_file = cmd_line;
	}

	/* execute the command line */
	rc = DosExecPgm (
		&load_error,
		HAWK_SIZEOF(load_error), 
		EXEC_ASYNCRESULT,
		cmd_line,
		HAWK_NULL,
		&child_rc,
		cmd_file
	);

	hawk_gem_freemem (pio->gem, cmd_line);
	cmd_line = HAWK_NULL;

	/* Once execution is completed regardless of success or failure,
	 * Restore stdin/out/err using handles duplicated into old_in/out/err */
	DosDupHandle (old_in, &std_in); /* I can't do much if this fails */
	DosClose (old_in); old_in = HAWK_PIO_HND_NIL;
	DosDupHandle (old_out, &std_out);
	DosClose (old_out); old_out = HAWK_PIO_HND_NIL;
	DosDupHandle (old_err, &std_err);
	DosClose (old_err); old_err = HAWK_PIO_HND_NIL;

	if (rc != NO_ERROR) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		goto oops;
	}
	pio->child = child_rc.codeTerminate;

#elif defined(__DOS__)

	/* DOS not multi-processed. can't support pio */
	hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;

#else

	if (flags & HAWK_PIO_WRITEIN)
	{
		if (HAWK_PIPE(&handle[0]) <= -1) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			goto oops;
		}
		minidx = 0; maxidx = 1;
	}

	if (flags & HAWK_PIO_READOUT)
	{
		if (HAWK_PIPE(&handle[2]) <= -1) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			goto oops;
		}
		if (minidx == -1) minidx = 2;
		maxidx = 3;
	}

	if (flags & HAWK_PIO_READERR)
	{
		if (HAWK_PIPE(&handle[4]) <= -1) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			goto oops;
		}
		if (minidx == -1) minidx = 4;
		maxidx = 5;
	}

	if (maxidx == -1) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
		goto oops;
	}

	if (pio->flags & HAWK_PIO_FNCCMD)
	{
		/* i know i'm abusing typecasting here.
		 * cmd is supposed to be hawk_pio_fnc_t*, anyway */
		pid = standard_fork_and_exec(pio, handle, (param_t*)cmd);
		if (pid <= -1) goto oops;
		pio->child = pid;
	}
	else
	{
	#if defined(HAVE_POSIX_SPAWN) && !(defined(HAWK_SYSCALL0) && defined(SYS_vfork))

		if ((pserr = posix_spawn_file_actions_init(&fa)) != 0) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
			goto oops;
		}
		fa_inited = 1;

		if (flags & HAWK_PIO_WRITEIN)
		{
			/* child should read */
			if ((pserr = posix_spawn_file_actions_addclose(&fa, handle[1])) != 0) 
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((pserr = posix_spawn_file_actions_adddup2(&fa, handle[0], 0)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((pserr = posix_spawn_file_actions_addclose(&fa, handle[0])) != 0) 
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
		}

		if (flags & HAWK_PIO_READOUT)
		{
			/* child should write */
			if ((pserr = posix_spawn_file_actions_addclose(&fa, handle[2])) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((pserr = posix_spawn_file_actions_adddup2(&fa, handle[3], 1)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((flags & HAWK_PIO_ERRTOOUT) &&
				(pserr = posix_spawn_file_actions_adddup2 (&fa, handle[3], 2)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((pserr = posix_spawn_file_actions_addclose(&fa, handle[3])) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
		}

		if (flags & HAWK_PIO_READERR)
		{
			/* child should write */
			if ((pserr = posix_spawn_file_actions_addclose (&fa, handle[4])) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((pserr = posix_spawn_file_actions_adddup2 (&fa, handle[5], 2)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((flags & HAWK_PIO_OUTTOERR) &&
				(pserr = posix_spawn_file_actions_adddup2 (&fa, handle[5], 1)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((pserr = posix_spawn_file_actions_addclose (&fa, handle[5])) != 0) 
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
		}

		{
			int oflags = O_RDWR;
		#if defined(O_LARGEFILE)
			oflags |= O_LARGEFILE;
		#endif

			if ((flags & HAWK_PIO_INTONUL) &&
			    (pserr = posix_spawn_file_actions_addopen (&fa, 0, HAWK_BT("/dev/null"), oflags, 0)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((flags & HAWK_PIO_OUTTONUL) &&
			    (pserr = posix_spawn_file_actions_addopen (&fa, 1, HAWK_BT("/dev/null"), oflags, 0)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
			if ((flags & HAWK_PIO_ERRTONUL) &&
			    (pserr = posix_spawn_file_actions_addopen (&fa, 2, HAWK_BT("/dev/null"), oflags, 0)) != 0)
			{
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
				goto oops;
			}
		}

		/* there remains the chance of race condition that
		 * 0, 1, 2 can be closed between addclose() and posix_spawn().
		 * so checking the file descriptors with is_fd_valid() is
		 * just on the best-effort basis.
		 */
		if ((flags & HAWK_PIO_DROPIN) && is_fd_valid(0) &&
		    (pserr = posix_spawn_file_actions_addclose (&fa, 0)) != 0) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
			goto oops;
		}
		if ((flags & HAWK_PIO_DROPOUT) && is_fd_valid(1) &&
		    (pserr = posix_spawn_file_actions_addclose (&fa, 1)) != 0) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
			goto oops;
		}
		if ((flags & HAWK_PIO_DROPERR) && is_fd_valid(2) &&
		    (pserr = posix_spawn_file_actions_addclose (&fa, 2)) != 0)
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
			goto oops;
		}

		if (!(flags & HAWK_PIO_NOCLOEXEC))
		{
			int fd = get_highest_fd(pio);
			while (fd > 2)
			{
				if (fd != handle[0] && fd != handle[1] &&
				    fd != handle[2] && fd != handle[3] &&
				    fd != handle[4] && fd != handle[5]) 
				{
					/* closing attempt on a best-effort basis.
					 * posix_spawn() fails if the file descriptor added
					 * with addclose() is closed before posix_spawn().
					 * addclose() if no FD_CLOEXEC is set or it's unknown. */
					if (is_fd_valid_and_nocloexec(fd) && 
					    (pserr = posix_spawn_file_actions_addclose (&fa, fd)) != 0) 
					{
						hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
						goto oops;
					}
				}
				fd--;
			}
		}

		if (make_param (pio, cmd, flags, &param) <= -1) goto oops;

		/* check if the command(the command requested or /bin/sh) is 
		 * exectuable to return an error without trying to execute it
		 * though this check alone isn't sufficient */
		if (assert_executable (pio, param.argv[0]) <= -1)
		{
			free_param (pio, &param); 
			goto oops;
		}

		posix_spawnattr_init (&psattr);

		#if defined(__linux)
		#if !defined(POSIX_SPAWN_USEVFORK)
		#	define POSIX_SPAWN_USEVFORK 0x40
		#endif
		posix_spawnattr_setflags (&psattr, POSIX_SPAWN_USEVFORK);
		#endif

		pserr = posix_spawn(&pid, param.argv[0], &fa, &psattr, param.argv, environ);

		#if defined(__linux)
		posix_spawnattr_destroy (&psattr);
		#endif

		free_param (pio, &param); 
		if (fa_inited) 
		{
			posix_spawn_file_actions_destroy (&fa);
			fa_inited = 0;
		}
		if (pserr != 0) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(pserr));
			goto oops;
		}

		pio->child = pid;

	#elif defined(HAWK_SYSCALL0) && defined(SYS_vfork)

		if (make_param (pio, cmd, flags, &param) <= -1) goto oops;

		/* check if the command(the command requested or /bin/sh) is 
		 * exectuable to return an error without trying to execute it
		 * though this check alone isn't sufficient */
		if (assert_executable(pio, param.argv[0]) <= -1)
		{
			free_param (pio, &param); 
			goto oops;
		}

		/* prepare some data before vforking for vfork limitation.
		 * the child in vfork should not make function calls or 
		 * change data shared with the parent. */
		if (!(flags & HAWK_PIO_NOCLOEXEC)) highest_fd = get_highest_fd (pio);

		HAWK_SYSCALL0 (pid, SYS_vfork);
		if (pid <= -1) 
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINVAL);
			free_param (pio, &param);
			goto oops;
		}

		if (pid == 0)
		{
			/* the child after vfork should not make function calls.
			 * since the system call like close() are also normal
			 * functions, i have to use assembly macros to make
			 * system calls. */

			hawk_pio_hnd_t devnull = -1;

			if (!(flags & HAWK_PIO_NOCLOEXEC))
			{
				/* cannot call close_open_fds_using_proc() in the vfork() context */

				int fd = highest_fd;

				/* close all other unknown open handles except 
				 * stdin/out/err and the pipes. */
				while (fd > 2)
				{
					if (fd != handle[0] && fd != handle[1] &&
					    fd != handle[2] && fd != handle[3] &&
					    fd != handle[4] && fd != handle[5]) 
					{
						HAWK_SYSCALL1 (dummy, SYS_close, fd);
					}
					fd--;
				}
			}

			if (flags & HAWK_PIO_WRITEIN)
			{
				/* child should read */
				HAWK_SYSCALL1 (dummy, SYS_close, handle[1]);
				HAWK_SYSCALL2 (dummy, SYS_dup2, handle[0], 0);
				if (dummy <= -1) goto child_oops;
				HAWK_SYSCALL1 (dummy, SYS_close, handle[0]);
			}

			if (flags & HAWK_PIO_READOUT)
			{
				/* child should write */
				HAWK_SYSCALL1 (dummy, SYS_close, handle[2]);
				HAWK_SYSCALL2 (dummy, SYS_dup2, handle[3], 1);
				if (dummy <= -1) goto child_oops;

				if (flags & HAWK_PIO_ERRTOOUT)
				{
					HAWK_SYSCALL2 (dummy, SYS_dup2, handle[3], 2);
					if (dummy <= -1) goto child_oops;
				}

				HAWK_SYSCALL1 (dummy, SYS_close, handle[3]);
			}

			if (flags & HAWK_PIO_READERR)
			{
				/* child should write */
				HAWK_SYSCALL1 (dummy, SYS_close, handle[4]);
				HAWK_SYSCALL2 (dummy, SYS_dup2, handle[5], 2);
				if (dummy <= -1) goto child_oops;

				if (flags & HAWK_PIO_OUTTOERR)
				{
					HAWK_SYSCALL2 (dummy, SYS_dup2, handle[5], 1);
					if (dummy <= -1) goto child_oops;
				}

				HAWK_SYSCALL1 (dummy, SYS_close, handle[5]);
			}

			if (flags & (HAWK_PIO_INTONUL | HAWK_PIO_OUTTONUL | HAWK_PIO_ERRTONUL))
			{
			#if defined(O_LARGEFILE)
				HAWK_SYSCALL3 (devnull, SYS_open, HAWK_BT("/dev/null"), O_RDWR|O_LARGEFILE, 0);
			#else
				HAWK_SYSCALL3 (devnull, SYS_open, HAWK_BT("/dev/null"), O_RDWR, 0);
			#endif
				if (devnull <= -1) goto child_oops;
			}

			if (flags & HAWK_PIO_INTONUL)
			{
				HAWK_SYSCALL2 (dummy, SYS_dup2, devnull, 0);
				if (dummy <= -1) goto child_oops;
			}
			if (flags & HAWK_PIO_OUTTONUL)
			{
				HAWK_SYSCALL2 (dummy, SYS_dup2, devnull, 1);
				if (dummy <= -1) goto child_oops;
			}
			if (flags & HAWK_PIO_ERRTONUL)
			{
				HAWK_SYSCALL2 (dummy, SYS_dup2, devnull, 2);
				if (dummy <= -1) goto child_oops;
			}

			if (flags & (HAWK_PIO_INTONUL | HAWK_PIO_OUTTONUL | HAWK_PIO_ERRTONUL))
			{
				HAWK_SYSCALL1 (dummy, SYS_close, devnull);
				devnull = -1;
			}

			if (flags & HAWK_PIO_DROPIN) HAWK_SYSCALL1 (dummy, SYS_close, 0);
			if (flags & HAWK_PIO_DROPOUT) HAWK_SYSCALL1 (dummy, SYS_close, 1);
			if (flags & HAWK_PIO_DROPERR) HAWK_SYSCALL1 (dummy, SYS_close, 2);

			HAWK_SYSCALL3 (dummy, SYS_execve, param.argv[0], param.argv, environ);
			/*free_param (pio, &param); don't free this in the vfork version */

		child_oops:
			if (devnull >= 0) HAWK_SYSCALL1 (dummy, SYS_close, devnull);
			HAWK_SYSCALL1 (dummy, SYS_exit, 128);
		}

		/* parent */
		free_param (pio, &param);
		pio->child = pid;

	#else

		if (make_param (pio, cmd, flags, &param) <= -1) goto oops;

		/* check if the command(the command requested or /bin/sh) is 
		 * exectuable to return an error without trying to execute it
		 * though this check alone isn't sufficient */
		if (assert_executable (pio, param.argv[0]) <= -1)
		{
			free_param (pio, &param); 
			goto oops;
		}

		pid = standard_fork_and_exec(pio, handle, &param);
		if (pid <= -1)
		{
			free_param (pio, &param);
			goto oops;
		}

		/* parent */
		free_param (pio, &param);
		pio->child = pid;
	#endif

	}

	if (flags & HAWK_PIO_WRITEIN)
	{
		/* 
		 * 012345
		 * rw----
		 * X  
		 * WRITE => 1
		 */
		HAWK_CLOSE (handle[0]);
		handle[0] = HAWK_PIO_HND_NIL;
	}

	if (flags & HAWK_PIO_READOUT)
	{
		/* 
		 * 012345
		 * --rw--
		 *    X
		 * READ => 2
		 */
		HAWK_CLOSE (handle[3]);
		handle[3] = HAWK_PIO_HND_NIL;
	}

	if (flags & HAWK_PIO_READERR)
	{
		/* 
		 * 012345
		 * ----rw
		 *      X   
		 * READ => 4
		 */
		HAWK_CLOSE (handle[5]);
		handle[5] = HAWK_PIO_HND_NIL;
	}
#endif

	if (((flags & HAWK_PIO_INNOBLOCK) && set_pipe_nonblock(pio, handle[1], 1) <= -1) ||
	    ((flags & HAWK_PIO_OUTNOBLOCK) && set_pipe_nonblock(pio, handle[2], 1) <= -1) ||
	    ((flags & HAWK_PIO_ERRNOBLOCK) && set_pipe_nonblock(pio, handle[4], 1) <= -1))
	{
		goto oops;
	}

	/* store back references */
	pio->pin[HAWK_PIO_IN].self = pio;
	pio->pin[HAWK_PIO_OUT].self = pio;
	pio->pin[HAWK_PIO_ERR].self = pio;

	/* store actual pipe handles */
	pio->pin[HAWK_PIO_IN].handle = handle[1];
	pio->pin[HAWK_PIO_OUT].handle = handle[2];
	pio->pin[HAWK_PIO_ERR].handle = handle[4];


	if (flags & HAWK_PIO_TEXT)
	{
		int topt = 0;

		if (flags & HAWK_PIO_IGNOREECERR) topt |= HAWK_TIO_IGNOREECERR;
		if (flags & HAWK_PIO_NOAUTOFLUSH) topt |= HAWK_TIO_NOAUTOFLUSH;

		HAWK_ASSERT (HAWK_COUNTOF(tio) == HAWK_COUNTOF(pio->pin));
		for (i = 0; i < HAWK_COUNTOF(tio); i++)
		{
			int r;

			tio[i] = hawk_tio_open(pio->gem, HAWK_SIZEOF(&pio->pin[i]), topt);
			if (tio[i] == HAWK_NULL) goto oops;

			*(hawk_pio_pin_t**)hawk_tio_getxtn(tio[i]) = &pio->pin[i];

			r = (i == HAWK_PIO_IN)?
				hawk_tio_attachout(tio[i], pio_output, HAWK_NULL, 4096):
				hawk_tio_attachin(tio[i], pio_input, HAWK_NULL, 4096);
			if (r <= -1) goto oops;

			pio->pin[i].tio = tio[i];
		}
	}

	return 0;

oops:
#if defined(_WIN32)
	if (windevnul != INVALID_HANDLE_VALUE) CloseHandle (windevnul);

#elif defined(__OS2__)
	if (cmd_line) hawk_gem_freemem (pio->gem, cmd_line);
	if (old_in != HAWK_PIO_HND_NIL)
	{
		DosDupHandle (old_in, &std_in);
		DosClose (old_in); 
	}
	if (old_out != HAWK_PIO_HND_NIL)
	{
		DosDupHandle (old_out, &std_out);
		DosClose (old_out);
	}
	if (old_err != HAWK_PIO_HND_NIL)
	{
		DosDupHandle (old_err, &std_err);
		DosClose (old_err);
	}
	if (os2devnul != HAWK_PIO_HND_NIL) DosClose (os2devnul);
#endif

	for (i = 0; i < HAWK_COUNTOF(tio); i++) 
	{
		if (tio[i]) hawk_tio_close (tio[i]);
	}

#if defined(_WIN32)
	for (i = minidx; i < maxidx; i++) CloseHandle (handle[i]);
#elif defined(__OS2__)
	for (i = minidx; i < maxidx; i++) 
	{
		if (handle[i] != HAWK_PIO_HND_NIL) DosClose (handle[i]);
	}
#elif defined(__DOS__)

	/* DOS not multi-processed. can't support pio */
#elif defined(HAVE_POSIX_SPAWN) && !(defined(HAWK_SYSCALL0) && defined(SYS_vfork))
	if (fa_inited) 
	{
		posix_spawn_file_actions_destroy (&fa);
		fa_inited = 0;
	}
	for (i = minidx; i < maxidx; i++) 
	{
		if (handle[i] != HAWK_PIO_HND_NIL) HAWK_CLOSE (handle[i]);
	}
#elif defined(HAWK_SYSCALL0) && defined(SYS_vfork)
	for (i = minidx; i < maxidx; i++) 
	{
		if (handle[i] != HAWK_PIO_HND_NIL) HAWK_CLOSE (handle[i]);
	}
#else
	for (i = minidx; i < maxidx; i++) 
	{
		if (handle[i] != HAWK_PIO_HND_NIL) HAWK_CLOSE (handle[i]);
	}
#endif

	return -1;
}

void hawk_pio_fini (hawk_pio_t* pio)
{
	hawk_pio_end (pio, HAWK_PIO_ERR);
	hawk_pio_end (pio, HAWK_PIO_OUT);
	hawk_pio_end (pio, HAWK_PIO_IN);

	/* when closing, enable blocking and retrying */
	pio->flags &= ~HAWK_PIO_WAITNOBLOCK;
	pio->flags &= ~HAWK_PIO_WAITNORETRY;
	hawk_pio_wait (pio);
}

hawk_cmgr_t* hawk_pio_getcmgr (hawk_pio_t* pio, hawk_pio_hid_t hid)
{
	return pio->pin[hid].tio? hawk_tio_getcmgr(pio->pin[hid].tio): HAWK_NULL;
}

void hawk_pio_setcmgr (hawk_pio_t* pio, hawk_pio_hid_t hid, hawk_cmgr_t* cmgr)
{
	if (pio->pin[hid].tio) hawk_tio_setcmgr (pio->pin[hid].tio, cmgr);
}

hawk_pio_hnd_t hawk_pio_gethnd (const hawk_pio_t* pio, hawk_pio_hid_t hid)
{
	return pio->pin[hid].handle;
}

hawk_pio_pid_t hawk_pio_getchild (const hawk_pio_t* pio)
{
	return pio->child;
}

static hawk_ooi_t pio_read (hawk_pio_t* pio, void* buf, hawk_oow_t size, hawk_pio_hnd_t hnd)
{
#if defined(_WIN32)
	DWORD count;
#elif defined(__OS2__)
	ULONG count;
	APIRET rc;
#elif defined(__DOS__)
	int n;
#else
	hawk_ooi_t n;
#endif

	if (hnd == HAWK_PIO_HND_NIL) 
	{
		/* the stream is already closed */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ENOHND);
		return (hawk_ooi_t)-1;
	}

#if defined(_WIN32)

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(DWORD)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(DWORD);

	if (ReadFile(hnd, buf, (DWORD)size, &count, HAWK_NULL) == FALSE) 
	{
		/* ReadFile receives ERROR_BROKEN_PIPE when the write end
		 * is closed in the child process */
		if (GetLastError() == ERROR_BROKEN_PIPE) return 0;
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}
	return (hawk_ooi_t)count;

#elif defined(__OS2__)

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(ULONG)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(ULONG);

	rc = DosRead (hnd, buf, (ULONG)size, &count);
	if (rc != NO_ERROR)
	{
    		if (rc == ERROR_BROKEN_PIPE) return 0; /* TODO: check this */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
    		return -1;
    	}
	return (hawk_ooi_t)count;

#elif defined(__DOS__)
	/* TODO: verify this */

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(unsigned int)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(unsigned int);

	n = read (hnd, buf, size);
	if (n <= -1) hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
	return n;

#else

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(size_t)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(size_t);

reread:
	n = HAWK_READ(hnd, buf, size);
	if (n <= -1) 
	{
		if (errno == EINTR)
		{
			if (pio->flags & HAWK_PIO_READNORETRY) 
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINTR);
			else goto reread;
		}
		else
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		}
	}

	return n;
#endif
}

hawk_ooi_t hawk_pio_read (hawk_pio_t* pio, hawk_pio_hid_t hid, void* buf, hawk_oow_t size)
{
	return (pio->pin[hid].tio == HAWK_NULL)?
		pio_read(pio, buf, size, pio->pin[hid].handle):
		hawk_tio_read(pio->pin[hid].tio, buf, size);
}

hawk_ooi_t hawk_pio_readbytes (hawk_pio_t* pio, hawk_pio_hid_t hid, void* buf, hawk_oow_t size)
{
	return (pio->pin[hid].tio == HAWK_NULL)?
		pio_read(pio, buf, size, pio->pin[hid].handle):
		hawk_tio_readbchars(pio->pin[hid].tio, buf, size);
}


static hawk_ooi_t pio_write (hawk_pio_t* pio, const void* data, hawk_oow_t size, hawk_pio_hnd_t hnd)
{
#if defined(_WIN32)
	DWORD count;
#elif defined(__OS2__)
	ULONG count;
	APIRET rc;
#elif defined(__DOS__)
	int n;
#else
	hawk_ooi_t n;
#endif

	if (hnd == HAWK_PIO_HND_NIL) 
	{
		/* the stream is already closed */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ENOHND);
		return (hawk_ooi_t)-1;
	}

#if defined(_WIN32)

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(DWORD)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(DWORD);

	if (WriteFile (hnd, data, (DWORD)size, &count, HAWK_NULL) == FALSE)
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}
	return (hawk_ooi_t)count;

#elif defined(__OS2__)

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(ULONG)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(ULONG);

	rc = DosWrite (hnd, (PVOID)data, (ULONG)size, &count);
	if (rc != NO_ERROR)
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
    		return -1;
	}
	return (hawk_ooi_t)count;

#elif defined(__DOS__)

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(unsigned int)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(unsigned int);

	n = write (hnd, data, size);
	if (n <= -1) hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
	return n;

#else

	if (size > (HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(size_t)))
		size = HAWK_TYPE_MAX(hawk_ooi_t) & HAWK_TYPE_MAX(size_t);

rewrite:
	n = HAWK_WRITE(hnd, data, size);
	if (n <= -1) 
	{
		if (errno == EINTR)
		{
			if (pio->flags & HAWK_PIO_WRITENORETRY)
				hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_EINTR);
			else goto rewrite;
		}
		else
		{
			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		}
	}
	return n;

#endif
}

hawk_ooi_t hawk_pio_write (hawk_pio_t* pio, hawk_pio_hid_t hid, const void* data, hawk_oow_t size)
{
	return (pio->pin[hid].tio == HAWK_NULL)?
		pio_write(pio, data, size, pio->pin[hid].handle):
		hawk_tio_write(pio->pin[hid].tio, data, size);
}

hawk_ooi_t hawk_pio_writebytes (hawk_pio_t* pio, hawk_pio_hid_t hid, const void* data, hawk_oow_t size)
{
	return (pio->pin[hid].tio == HAWK_NULL)?
		pio_write(pio, data, size, pio->pin[hid].handle):
		hawk_tio_writebchars(pio->pin[hid].tio, data, size);
}

hawk_ooi_t hawk_pio_flush (hawk_pio_t* pio, hawk_pio_hid_t hid)
{
	if (pio->pin[hid].tio == HAWK_NULL) return 0;
	return hawk_tio_flush(pio->pin[hid].tio);
}

void hawk_pio_drain (hawk_pio_t* pio, hawk_pio_hid_t hid)
{
	if (pio->pin[hid].tio) hawk_tio_drain (pio->pin[hid].tio);
}

void hawk_pio_end (hawk_pio_t* pio, hawk_pio_hid_t hid)
{
	if (pio->pin[hid].tio)
	{
		hawk_tio_close (pio->pin[hid].tio);
		pio->pin[hid].tio = HAWK_NULL;
	}

	if (pio->pin[hid].handle != HAWK_PIO_HND_NIL)
	{
#if defined(_WIN32)
		CloseHandle (pio->pin[hid].handle);
#elif defined(__OS2__)
		DosClose (pio->pin[hid].handle);
#elif defined(__DOS__)
		close (pio->pin[hid].handle);
#else
		HAWK_CLOSE (pio->pin[hid].handle);
#endif
		pio->pin[hid].handle = HAWK_PIO_HND_NIL;
	}
}

int hawk_pio_wait (hawk_pio_t* pio)
{
#if defined(_WIN32)

	DWORD ecode, w;

	if (pio->child == HAWK_PIO_PID_NIL) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ECHILD);
		return -1;
	}

	w = WaitForSingleObject (pio->child, 
		((pio->flags & HAWK_PIO_WAITNOBLOCK)? 0: INFINITE)
	);
	if (w == WAIT_TIMEOUT)
	{
		/* the child process is still alive */
		return 255 + 1;
	}
	if (w != WAIT_OBJECT_0)
	{
		/* WAIT_FAILED, WAIT_ABANDONED */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}

	HAWK_ASSERT (w == WAIT_OBJECT_0);
	
	if (GetExitCodeProcess(pio->child, &ecode) == FALSE) 
	{
		/* close the handle anyway to prevent further 
		 * errors when this function is called again */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		CloseHandle (pio->child); 
		pio->child = HAWK_PIO_PID_NIL;
		return -1;
	}

	/* close handle here to emulate waitpid() as much as possible. */
	CloseHandle (pio->child); 
	pio->child = HAWK_PIO_PID_NIL;

	if (ecode == STILL_ACTIVE)
	{
		/* this should not happen as the control reaches here
		 * only when WaitforSingleObject() is successful.
		 * if it happends,  close the handle and return an error */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ESYSERR);
		return -1;
	}

	return ecode;

#elif defined(__OS2__)

	APIRET rc;
	RESULTCODES child_rc;
	PID ppid;

	if (pio->child == HAWK_PIO_PID_NIL) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ECHILD);
		return -1;
	}

	rc = DosWaitChild (
		DCWA_PROCESSTREE,
		((pio->flags & HAWK_PIO_WAITNOBLOCK)? DCWW_NOWAIT: DCWW_WAIT),
		&child_rc,
		&ppid,
		pio->child
	);
	if (rc == ERROR_CHILD_NOT_COMPLETE)
	{
		/* the child process is still alive */
		return 255 + 1;
	}
	if (rc != NO_ERROR)
	{
		/* WAIT_FAILED, WAIT_ABANDONED */
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}

	/* close handle here to emulate waitpid() as much as possible. */
	/*DosClose (pio->child);*/
	pio->child = HAWK_PIO_PID_NIL;

	return (child_rc.codeTerminate == TC_EXIT)? 
		child_rc.codeResult: (255 + 1 + child_rc.codeTerminate);

#elif defined(__DOS__)

	hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;

#else

	int opt = 0;
	int ret = -1;

	if (pio->child == HAWK_PIO_PID_NIL) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ECHILD);
		return -1;
	}

	if (pio->flags & HAWK_PIO_WAITNOBLOCK) opt |= WNOHANG;

	while (1)
	{
		int status, n;

		n = HAWK_WAITPID (pio->child, &status, opt);
		if (n <= -1)
		{
			if (errno == ECHILD)
			{
				/* most likely, the process has already been 
				 * waitpid()ed on. */
				pio->child = HAWK_PIO_PID_NIL;
			}
			else if (errno == EINTR)
			{
				if (!(pio->flags & HAWK_PIO_WAITNORETRY)) continue;
			}

			hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			break;
		}

		if (n == 0) 
		{
			/* when WNOHANG is not specified, 0 can't be returned */
			/*HAWK_ASSERT (pio->flags & HAWK_PIO_WAITNOBLOCK);*/

			ret = 255 + 1;
			/* the child process is still alive */
			break;
		}

		if (n == pio->child)
		{
			if (WIFEXITED(status))
			{
				/* the child process ended normally */
				ret = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status))
			{
				/* the child process was killed by a signal */
				ret = 255 + 1 + WTERMSIG (status);
			}
			else
			{
				/* not interested in WIFSTOPPED & WIFCONTINUED.
				 * in fact, this else-block should not be reached
				 * as WIFEXITED or WIFSIGNALED must be true.
				 * anyhow, just set the return value to 0. */
				ret = 0;
			}

			pio->child = HAWK_PIO_PID_NIL;
			break;
		}
	}

	return ret;
#endif
}

int hawk_pio_kill (hawk_pio_t* pio)
{
#if defined(_WIN32)
	DWORD n;
#elif defined(__OS2__)
	APIRET rc;
#elif defined(__DOS__)
	/* TODO: implement this */
#else
	int n;
#endif

	if (pio->child == HAWK_PIO_PID_NIL) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ECHILD);
		return -1;
	}

#if defined(_WIN32)
	/* 9 was chosen below to treat TerminateProcess as kill -KILL. */
	n = TerminateProcess(pio->child, 255 + 1 + 9);
	if (n == FALSE) 
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(GetLastError()));
		return -1;
	}
	return 0;

#elif defined(__OS2__)
/*TODO: must use DKP_PROCESS? */
	rc = DosKillProcess(pio->child, DKP_PROCESSTREE);
	if (rc != NO_ERROR)
	{
		hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(rc));
		return -1;
	}
	return 0;	

#elif defined(__DOS__)

	hawk_gem_seterrnum (pio->gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;

#else
	n = HAWK_KILL(pio->child, SIGKILL);
	if (n <= -1) hawk_gem_seterrnum (pio->gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
	return n;
#endif
}

static hawk_ooi_t pio_input (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size)
{
	if (cmd == HAWK_TIO_DATA) 
	{
		hawk_pio_pin_t* pin = *(hawk_pio_pin_t**)hawk_tio_getxtn(tio);
		HAWK_ASSERT (pin != HAWK_NULL);
		HAWK_ASSERT (pin->self != HAWK_NULL);
		return pio_read(pin->self, buf, size, pin->handle);
	}

	/* take no actions for OPEN and CLOSE as they are handled
	 * by pio */
	return 0;
}

static hawk_ooi_t pio_output (hawk_tio_t* tio, hawk_tio_cmd_t cmd, void* buf, hawk_oow_t size)
{
	if (cmd == HAWK_TIO_DATA) 
	{
		hawk_pio_pin_t* pin = *(hawk_pio_pin_t**)hawk_tio_getxtn(tio);
		HAWK_ASSERT (pin != HAWK_NULL);
		HAWK_ASSERT (pin->self != HAWK_NULL);
		return pio_write(pin->self, buf, size, pin->handle);
	}

	/* take no actions for OPEN and CLOSE as they are handled
	 * by pio */
	return 0;
}
