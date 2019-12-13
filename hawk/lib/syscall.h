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

#ifndef _HAWK_SYSCALL_H_
#define _HAWK_SYSCALL_H_

#if defined(_WIN32) || defined(__OS2__) || defined(__DOS__)
#	error Do not include this file
#endif

/* This file defines unix/linux system calls */

#if defined(HAVE_SYS_TYPES_H)
#	include <sys/types.h>
#endif
#if defined(HAVE_UNISTD_H)
#	include <unistd.h>
#endif
#if defined(HAVE_SYS_WAIT_H)
#	include <sys/wait.h>
#endif
#if defined(HAVE_SIGNAL_H)
#	include <signal.h>
#endif
#if defined(HAVE_ERRNO_H)
#	include <errno.h>
#endif
#if defined(HAVE_FCNTL_H)
#	include <fcntl.h>
#endif
#if defined(HAVE_TIME_H)
#	include <time.h>
#endif
#if defined(HAVE_SYS_TIME_H)
#	include <sys/time.h>
#endif
#if defined(HAVE_UTIME_H)
#	include <utime.h>
#endif
#if defined(HAVE_SYS_RESOURCE_H)
#	include <sys/resource.h>
#endif
#if defined(HAVE_SYS_STAT_H)
#	include <sys/stat.h>
#endif
#if defined(HAVE_DIRENT_H)
#	include <dirent.h>
#endif
#if defined(HAVE_SYS_IOCTL_H)
#	include <sys/ioctl.h>
#endif

#if defined(HAWK_USE_SYSCALL) && defined(HAVE_SYS_SYSCALL_H)
#	include <sys/syscall.h>
#endif

#if defined(__cplusplus)
#	define	HAWK_LIBCALL0(x) ::x()
#	define	HAWK_LIBCALL1(x,a1) ::x(a1)
#	define	HAWK_LIBCALL2(x,a1,a2) ::x(a1,a2)
#	define	HAWK_LIBCALL3(x,a1,a2,a3) ::x(a1,a2,a3)
#	define	HAWK_LIBCALL4(x,a1,a2,a3,a4) ::x(a1,a2,a3,a4)
#	define	HAWK_LIBCALL5(x,a1,a2,a3,a4,a5) ::x(a1,a2,a3,a4,a5)
#else
#	define	HAWK_LIBCALL0(x) x
#	define	HAWK_LIBCALL1(x,a1) x(a1)
#	define	HAWK_LIBCALL2(x,a1,a2) x(a1,a2)
#	define	HAWK_LIBCALL3(x,a1,a2,a3) x(a1,a2,a3)
#	define	HAWK_LIBCALL4(x,a1,a2,a3,a4) x(a1,a2,a3,a4)
#	define	HAWK_LIBCALL5(x,a1,a2,a3,a4,a5) x(a1,a2,a3,a4,a5)
#endif

#if defined(SYS_open) && defined(HAWK_USE_SYSCALL)
#	define HAWK_OPEN(path,flags,mode) syscall(SYS_open,path,flags,mode)
#else
#	define HAWK_OPEN(path,flags,mode) HAWK_LIBCALL3(open,path,flags,mode)
#endif

#if defined(SYS_close) && defined(HAWK_USE_SYSCALL)
#	define HAWK_CLOSE(handle) syscall(SYS_close,handle)
#else
#	define HAWK_CLOSE(handle) HAWK_LIBCALL1(close,handle)
#endif

#if defined(SYS_read) && defined(HAWK_USE_SYSCALL)
#	define HAWK_READ(handle,buf,size) syscall(SYS_read,handle,buf,size)
#else
#	define HAWK_READ(handle,buf,size) HAWK_LIBCALL3(read,handle,buf,size)
#endif

#if defined(SYS_write) && defined(HAWK_USE_SYSCALL)
#	define HAWK_WRITE(handle,buf,size) syscall(SYS_write,handle,buf,size)
#else
#	define HAWK_WRITE(handle,buf,size) HAWK_LIBCALL3(write,handle,buf,size)
#endif

#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS__llseek) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LLSEEK(handle,hoffset,loffset,out,whence) syscall(SYS__llseek,handle,hoffset,loffset,out,whence)
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE__LLSEEK) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LLSEEK(handle,hoffset,loffset,out,whence) _llseek(handle,hoffset,loffset,out,whence)
#endif

#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS_lseek64) && defined(HAWK_USE_SYSCALL) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_LSEEK(handle,offset,whence) syscall(SYS_lseek64,handle,offset,whence)
#elif defined(SYS_lseek) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LSEEK(handle,offset,whence) syscall(SYS_lseek,handle,offset,whence)
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_LSEEK64) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_LSEEK(handle,offset,whence) lseek64(handle,offset,whence)
#else
#	define HAWK_LSEEK(handle,offset,whence) lseek(handle,offset,whence)
#endif

#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS_ftruncate64) && defined(HAWK_USE_SYSCALL) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_FTRUNCATE(handle,size) syscall(SYS_ftruncate64,handle,size)
#elif defined(SYS_ftruncate) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FTRUNCATE(handle,size) syscall(SYS_ftruncate,handle,size)
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_FTRUNCATE64) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_FTRUNCATE(handle,size) ftruncate64(handle,size)
#else
#	define HAWK_FTRUNCATE(handle,size) ftruncate(handle,size)
#endif

#if defined(SYS_fsync) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FSYNC(handle) syscall(SYS_fsync,handle)
#else
#	define HAWK_FSYNC(handle) fsync(handle)
#endif

#if defined(SYS_fcntl) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FCNTL(handle,cmd,arg) syscall(SYS_fcntl,handle,cmd,arg)
#else
#	define HAWK_FCNTL(handle,cmd,arg) fcntl(handle,cmd,arg)
#endif

#if defined(SYS_dup2) && defined(HAWK_USE_SYSCALL)
#	define HAWK_DUP2(ofd,nfd) syscall(SYS_dup2,ofd,nfd)
#else
#	define HAWK_DUP2(ofd,nfd) dup2(ofd,nfd)
#endif

#if defined(SYS_pipe) && defined(HAWK_USE_SYSCALL)
#	define HAWK_PIPE(pfds) syscall(SYS_pipe,pfds)
#else
#	define HAWK_PIPE(pfds) pipe(pfds)
#endif

#if defined(SYS_exit) && defined(HAWK_USE_SYSCALL)
#	define HAWK_EXIT(code) syscall(SYS_exit,code)
#else
#	define HAWK_EXIT(code) _exit(code)
#endif

#if defined(SYS_fork) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FORK() syscall(SYS_fork)
#else
#	define HAWK_FORK() fork()
#endif

#if defined(SYS_vfork) && defined(HAWK_USE_SYSCALL)
#	define HAWK_VFORK() syscall(SYS_vfork)
#else
#	define HAWK_VFORK() vfork()
#endif

#if defined(SYS_execve) && defined(HAWK_USE_SYSCALL)
#	define HAWK_EXECVE(path,argv,envp) syscall(SYS_execve,path,argv,envp)
#else
#	define HAWK_EXECVE(path,argv,envp) execve(path,argv,envp)
#endif

#if defined(SYS_waitpid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_WAITPID(pid,status,options) syscall(SYS_waitpid,pid,status,options)
#else
#	define HAWK_WAITPID(pid,status,options) waitpid(pid,status,options)
#endif

#if defined(SYS_kill) && defined(HAWK_USE_SYSCALL)
#	define HAWK_KILL(pid,sig) syscall(SYS_kill,pid,sig)
#else
#	define HAWK_KILL(pid,sig) kill(pid,sig)
#endif

#if defined(SYS_getpid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETPID() syscall(SYS_getpid)
#else
#	define HAWK_GETPID() getpid()
#endif

#if defined(SYS_getuid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETUID() syscall(SYS_getuid)
#else
#	define HAWK_GETUID() getuid()
#endif

#if defined(SYS_geteuid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETEUID() syscall(SYS_geteuid)
#else
#	define HAWK_GETEUID() geteuid()
#endif

#if defined(SYS_getgid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETGID() syscall(SYS_getgid)
#else
#	define HAWK_GETGID() getgid()
#endif

#if defined(SYS_getegid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETEGID() syscall(SYS_getegid)
#else
#	define HAWK_GETEGID() getegid()
#endif

#if defined(SYS_getsid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETSID(pid) syscall(SYS_getsid,pid)
#else
#	define HAWK_GETSID(pid) getsid(pid)
#endif

#if defined(SYS_setsid) && defined(HAWK_USE_SYSCALL)
#	define HAWK_SETSID() syscall(SYS_setsid)
#else
#	define HAWK_SETSID() setsid()
#endif

#if defined(SYS_signal) && defined(HAWK_USE_SYSCALL)
#	define HAWK_SIGNAL(signum,handler) syscall(SYS_signal,signum,handler)
#else
#	define HAWK_SIGNAL(signum,handler) signal(signum,handler)
#endif

#if defined(SYS_gettimeofday) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETTIMEOFDAY(tv,tz) syscall(SYS_gettimeofday,tv,tz)
#else
#	define HAWK_GETTIMEOFDAY(tv,tz) gettimeofday(tv,tz)
#endif

#if defined(SYS_settimeofday) && defined(HAWK_USE_SYSCALL)
#	define HAWK_SETTIMEOFDAY(tv,tz) syscall(SYS_settimeofday,tv,tz)
#else
#	define HAWK_SETTIMEOFDAY(tv,tz) settimeofday(tv,tz)
#endif

#if defined(SYS_time) && defined(HAWK_USE_SYSCALL)
#	define HAWK_TIME(tv) syscall(SYS_time,tv)
#else
#	define HAWK_TIME(tv) time(tv)
#endif

#if defined(SYS_stime) && defined(HAWK_USE_SYSCALL)
#	define HAWK_STIME(tv) syscall(SYS_stime,tv)
#else
#	define HAWK_STIME(tv) stime(tv)
#endif

#if defined(SYS_getrlimit) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETRLIMIT(res,lim) syscall(SYS_getrlimit,res,lim)
#else
#	define HAWK_GETRLIMIT(res,lim) getrlimit(res,lim)
#endif

#if defined(SYS_setrlimit) && defined(HAWK_USE_SYSCALL)
#	define HAWK_SETRLIMIT(res,lim) syscall(SYS_setrlimit,res,lim)
#else
#	define HAWK_SETRLIMIT(res,lim) setrlimit(res,lim)
#endif

#if defined(SYS_ioctl) && defined(HAWK_USE_SYSCALL)
#	define HAWK_IOCTL(fd,req,arg) syscall(SYS_ioctl,fd,req,arg)
#else
#	define HAWK_IOCTL(fd,req,arg) ioctl(fd,req,arg)
#endif


/* ===== FILE SYSTEM CALLS ===== */

#if defined(SYS_chmod) && defined(HAWK_USE_SYSCALL)
#	define HAWK_CHMOD(path,mode) syscall(SYS_chmod,path,mode)
#else
#	define HAWK_CHMOD(path,mode) chmod(path,mode)
#endif

#if defined(SYS_fchmod) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FCHMOD(handle,mode) syscall(SYS_fchmod,handle,mode)
#else
#	define HAWK_FCHMOD(handle,mode) fchmod(handle,mode)
#endif

#if defined(SYS_fchmodat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FCHMODAT(dirfd,path,mode,flags) syscall(SYS_fchmodat,dirfd,path,mode,flags)
#else
#	define HAWK_FCHMODAT(dirfd,path,mode,flags) fchmodat(dirfd,path,mode,flags)
#endif

#if defined(SYS_chown) && defined(HAWK_USE_SYSCALL)
#	define HAWK_CHOWN(path,owner,group) syscall(SYS_chown,path,owner,group)
#else
#	define HAWK_CHOWN(path,owner,group) chown(path,owner,group)
#endif

#if defined(SYS_fchown) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FCHOWN(handle,owner,group) syscall(SYS_fchown,handle,owner,group)
#else
#	define HAWK_FCHOWN(handle,owner,group) HAWK_LIBCALL3(fchown,handle,owner,group)
#endif

#if defined(SYS_fchownat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FCHOWNAT(dirfd,path,uid,gid,flags) syscall(SYS_fchownat,dirfd,path,uid,gid,flags)
#else
#	define HAWK_FCHOWNAT(dirfd,path,uid,gid,flags) HAWK_LIBCALL5(fchownat,dirfd,path,uid,gid,flags)
#endif

#if defined(SYS_chroot) && defined(HAWK_USE_SYSCALL)
#	define HAWK_CHROOT(path) syscall(SYS_chroot,path)
#else
#	define HAWK_CHROOT(path) HAWK_LIBCALL1(chroot,path)
#endif

#if !defined(SYS_lchown) && !defined(HAVE_LCHOWN) && defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)
	/* special handling for old darwin/macosx sdks */
#	define HAWK_LCHOWN(path,owner,group) syscall(364,path,owner,group)
#elif defined(SYS_lchown) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LCHOWN(path,owner,group) syscall(SYS_lchown,path,owner,group)
#else
#	define HAWK_LCHOWN(path,owner,group) HAWK_LIBCALL3(lchown,path,owner,group)
#endif

#if defined(SYS_link) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LINK(oldpath,newpath) syscall(SYS_link,oldpath,newpath)
#else
#	define HAWK_LINK(oldpath,newpath) HAWK_LIBCALL2(link,oldpath,newpath)
#endif


#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS_fstat64) && defined(HAWK_USE_SYSCALL) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_FSTAT(handle,stbuf) syscall(SYS_fstat64,handle,stbuf)
	typedef struct stat64 hawk_fstat_t;
#elif defined(SYS_fstat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FSTAT(handle,stbuf) syscall(SYS_fstat,handle,stbuf)
	typedef struct stat hawk_fstat_t;
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_FSTAT64) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_FSTAT(handle,stbuf) fstat64(handle,stbuf)
	typedef struct stat64 hawk_fstat_t;
#else
#	define HAWK_FSTAT(handle,stbuf) fstat(handle,stbuf)
	typedef struct stat hawk_fstat_t;
#endif

#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS_stat64) && defined(HAWK_USE_SYSCALL) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_STAT(path,stbuf) syscall(SYS_stat64,path,stbuf)
	typedef struct stat64 hawk_stat_t;
#elif defined(SYS_stat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_STAT(path,stbuf) syscall(SYS_stat,path,stbuf)
	typedef struct stat hawk_stat_t;
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_STAT64) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_STAT(path,stbuf) stat64(path,stbuf)
	typedef struct stat64 hawk_stat_t;
#else
#	define HAWK_STAT(path,stbuf) stat(path,stbuf)
	typedef struct stat hawk_stat_t;
#endif

#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS_lstat64) && defined(HAWK_USE_SYSCALL) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_LSTAT(path,stbuf) syscall(SYS_lstat,path,stbuf)
	typedef struct stat64 hawk_lstat_t;
#elif defined(SYS_lstat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LSTAT(path,stbuf) syscall(SYS_lstat,path,stbuf)
	typedef struct stat hawk_lstat_t;
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_LSTAT64) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_LSTAT(path,stbuf) lstat64(path,stbuf)
	typedef struct stat64 hawk_lstat_t;
#else
#	define HAWK_LSTAT(path,stbuf) lstat(path,stbuf)
	typedef struct stat hawk_lstat_t;
#endif


#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(SYS_fstatat64) && defined(HAWK_USE_SYSCALL) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_FSTATAT(dirfd,path,stbuf,flags) syscall(SYS_fstatat,dirfd,path,stbuf,flags)
	typedef struct stat64 hawk_fstatat_t;
#elif defined(SYS_fstatat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FSTATAT(dirfd,path,stbuf,flags) syscall(SYS_fstatat,dirfd,path,stbuf,flags)
	typedef struct stat hawk_fstatat_t;
#elif !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_FSTATAT64) && defined(__THIS_LINE_IS_DISABLED__)
#	define HAWK_FSTATAT(dirfd,path,stbuf,flags) fstatat64(dirfd,path,stbuf,flags)
	typedef struct stat64 hawk_fstatat_t;
#else
#	define HAWK_FSTATAT(dirfd,path,stbuf,flags) fstatat(dirfd,path,stbuf,flags)
	typedef struct stat hawk_fstatat_t;
#endif

#if defined(SYS_access) && defined(HAWK_USE_SYSCALL)
#	define HAWK_ACCESS(path,mode) syscall(SYS_access,path,mode)
#else
#	define HAWK_ACCESS(path,mode) access(path,mode)
#endif

#if defined(SYS_faccessat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FACCESSAT(dirfd,path,mode,flags) syscall(SYS_faccessat,dirfd,path,mode,flags)
#elif defined(HAVE_FACCESSAT)
#	define HAWK_FACCESSAT(dirfd,path,mode,flags) faccessat(dirfd,path,mode,flags)
#endif

#if defined(SYS_rename) && defined(HAWK_USE_SYSCALL)
#	define HAWK_RENAME(oldpath,newpath) syscall(SYS_rename,oldpath,newpath)
#else
	extern int rename(const char *oldpath, const char *newpath); /* not to include stdio.h */
#	define HAWK_RENAME(oldpath,newpath) rename(oldpath,newpath)
#endif

#if defined(SYS_umask) && defined(HAWK_USE_SYSCALL)
#	define HAWK_UMASK(mode) syscall(SYS_umask,mode)
#else
#	define HAWK_UMASK(mode) umask(mode)
#endif

#if defined(SYS_mkdir) && defined(HAWK_USE_SYSCALL)
#	define HAWK_MKDIR(path,mode) syscall(SYS_mkdir,path,mode)
#else
#	define HAWK_MKDIR(path,mode) mkdir(path,mode)
#endif

#if defined(SYS_rmdir) && defined(HAWK_USE_SYSCALL)
#	define HAWK_RMDIR(path) syscall(SYS_rmdir,path)
#else
#	define HAWK_RMDIR(path) rmdir(path)
#endif

#if defined(SYS_chdir) && defined(HAWK_USE_SYSCALL)
#	define HAWK_CHDIR(path) syscall(SYS_chdir,path)
#else
#	define HAWK_CHDIR(path) chdir(path)
#endif

#if defined(SYS_fchdir) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FCHDIR(handle) syscall(SYS_fchdir,handle)
#else
#	define HAWK_FCHDIR(handle) fchdir(handle)
#endif

#if defined(SYS_symlink) && defined(HAWK_USE_SYSCALL)
#	define HAWK_SYMLINK(oldpath,newpath) syscall(SYS_symlink,oldpath,newpath)
#else
#	define HAWK_SYMLINK(oldpath,newpath) symlink(oldpath,newpath)
#endif

#if defined(SYS_readlink) && defined(HAWK_USE_SYSCALL)
#	define HAWK_READLINK(path,buf,size) syscall(SYS_readlink,path,buf,size)
#else
#	define HAWK_READLINK(path,buf,size) readlink(path,buf,size)
#endif

#if defined(SYS_unlink) && defined(HAWK_USE_SYSCALL)
#	define HAWK_UNLINK(path) syscall(SYS_unlink,path)
#else
#	define HAWK_UNLINK(path) unlink(path)
#endif

#if defined(SYS_utime) && defined(HAWK_USE_SYSCALL)
#	define HAWK_UTIME(path,t) syscall(SYS_utime,path,t)
#else
#	define HAWK_UTIME(path,t) utime(path,t)
#endif

#if defined(SYS_utimes) && defined(HAWK_USE_SYSCALL)
#	define HAWK_UTIMES(path,t) syscall(SYS_utimes,path,t)
#else
#	define HAWK_UTIMES(path,t) utimes(path,t)
#endif

#if defined(SYS_futimes) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FUTIMES(fd,t) syscall(SYS_futimes,fd,t)
#else
#	define HAWK_FUTIMES(fd,t) futimes(fd,t)
#endif

#if defined(SYS_lutimes) && defined(HAWK_USE_SYSCALL)
#	define HAWK_LUTIMES(fd,t) syscall(SYS_lutimes,fd,t)
#else
#	define HAWK_LUTIMES(fd,t) lutimes(fd,t)
#endif

#if defined(SYS_futimens) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FUTIMENS(fd,t) syscall(SYS_futimens,fd,t)
#else
#	define HAWK_FUTIMENS(fd,t) futimens(fd,t)
#endif

#if defined(SYS_utimensat) && defined(HAWK_USE_SYSCALL)
#	define HAWK_FUTIMENS(dirfd,path,times,flags) syscall(SYS_futimens,dirfd,path,times,flags)
#else
#	define HAWK_UTIMENSAT(dirfd,path,times,flags) utimensat(dirfd,path,times,flags)
#endif

/*
the library's getcwd() returns char* while the system call returns int.
so it's not practical to define HAWK_GETCWD().
#if defined(SYS_getcwd) && defined(HAWK_USE_SYSCALL)
#	define HAWK_GETCWD(buf,size) syscall(SYS_getcwd,buf,size)
#else
#	define HAWK_GETCWD(buf,size) getcwd(buf,size)
#endif
*/

/* ===== DIRECTORY - not really system calls ===== */
#define HAWK_OPENDIR(name) opendir(name)
#define HAWK_CLOSEDIR(dir) closedir(dir)
#define HAWK_REWINDDIR(dir) rewinddir(dir)
#if defined(HAVE_DIRFD)
#	define HAWK_DIRFD(dir) dirfd(dir)
#elif defined(HAVE_DIR_DD_FD)
#	define HAWK_DIRFD(dir) ((dir)->dd_fd)
#elif defined(HAVE_DIR_D_FD)
#	define HAWK_DIRFD(dir) ((dir)->d_fd)
#else
#	if defined(dirfd)
		/* mac os x 10.1 defines dirfd as a macro */
#		define HAWK_DIRFD(dir) dirfd(dir)
#	else
#		error OUCH!!! NO DIRFD AVAILABLE
#	endif
#endif
#define HAWK_DIR DIR

#if !defined(_LP64) && (HAWK_SIZEOF_VOID_P<8) && defined(HAVE_READDIR64) && defined(__THIS_LINE_IS_DIABLED__)
	typedef struct dirent64 hawk_dirent_t;
#	define HAWK_READDIR(x) readdir64(x)
#else
	typedef struct dirent hawk_dirent_t;
#	define HAWK_READDIR(x) readdir(x)
#endif

/* ------------------------------------------------------------------------ */

#if defined(__linux) && defined(__GNUC__) && defined(__x86_64) 

#include <sys/syscall.h>

/*

http://www.x86-64.org/documentation/abi.pdf

A.2 AMD64 Linux Kernel Conventions
The section is informative only.

A.2.1 Calling Conventions
The Linux AMD64 kernel uses internally the same calling conventions as user-
level applications (see section 3.2.3 for details). User-level applications 
that like to call system calls should use the functions from the C library. 
The interface between the C library and the Linux kernel is the same as for
the user-level applications with the following differences:

1. User-level applications use as integer registers for passing the sequence
   %rdi, %rsi, %rdx, %rcx, %r8 and %r9. The kernel interface uses %rdi,
   %rsi, %rdx, %r10, %r8 and %r9.
2. A system-call is done via the syscall instruction. The kernel destroys
   registers %rcx and %r11.
3. The number of the syscall has to be passed in register %rax.
4. System-calls are limited to six arguments, no argument is passed directly 
   on the stack.
5. Returning from the syscall, register %rax contains the result of the
   system-call. A value in the range between -4095 and -1 indicates an error,
   it is -errno.
6. Only values of class INTEGER or class MEMORY are passed to the kernel.

*/

/*
#define HAWK_SYSCALL0(ret,num) \
	__asm__ volatile ( \
		"movq %1, %%rax\n\t" \
		"syscall\n"  \
		: "=&a"(ret)  \
		: "g"((hawk_uint64_t)num) \
		: "%rcx", "%r11")

#define HAWK_SYSCALL1(ret,num,arg1) \
	__asm__ volatile ( \
		"movq %1, %%rax\n\t" \
		"movq %2, %%rdi\n\t" \
		"syscall\n"  \
		: "=&a"(ret)  \
		: "g"((hawk_uint64_t)num), "g"((hawk_uint64_t)arg1) \
		: "%rdi", "%rcx", "%r11")

#define HAWK_SYSCALL2(ret,num,arg1,arg2) \
	__asm__ volatile ( \
		"movq %1, %%rax\n\t" \
		"movq %2, %%rdi\n\t" \
		"movq %3, %%rsi\n\t" \
		"syscall\n"  \
		: "=&a"(ret) \
		: "g"((hawk_uint64_t)num), "g"((hawk_uint64_t)arg1), "g"((hawk_uint64_t)arg2) \
		: "%rdi", "%rsi", "%rcx", "%r11")

#define HAWK_SYSCALL3(ret,num,arg1,arg2,arg3) \
	__asm__ volatile ( \
		"movq %1, %%rax\n\t" \
		"movq %2, %%rdi\n\t" \
		"movq %3, %%rsi\n\t" \
		"movq %4, %%rdx\n\t" \
		"syscall\n"  \
		: "=&a"(ret)  \
		: "g"((hawk_uint64_t)num), "g"((hawk_uint64_t)arg1), "g"((hawk_uint64_t)arg2), "g"((hawk_uint64_t)arg3) \
		: "%rdi", "%rsi", "%rdx", "%rcx", "%r11")
*/

#define HAWK_SYSCALL0(ret,num) \
	__asm__ volatile ( \
		"syscall\n" \
		: "=a"(ret) \
		: "a"((hawk_uint64_t)num) \
		: "memory", "cc", "%rcx", "%r11" \
	)
 

#define HAWK_SYSCALL1(ret,num,arg1) \
	__asm__ volatile ( \
		"syscall\n" \
		: "=a"(ret) \
		: "a"((hawk_uint64_t)num), "D"((hawk_uint64_t)arg1) \
		: "memory", "cc", "%rcx", "%r11" \
	)

#define HAWK_SYSCALL2(ret,num,arg1,arg2) \
	__asm__ volatile ( \
		"syscall\n"  \
		: "=a"(ret) \
		: "a"((hawk_uint64_t)num), "D"((hawk_uint64_t)arg1), "S"((hawk_uint64_t)arg2) \
		: "memory", "cc", "%rcx", "%r11" \
	)

#define HAWK_SYSCALL3(ret,num,arg1,arg2,arg3) \
	__asm__ volatile ( \
		"syscall\n" \
		: "=a"(ret) \
		: "a"((hawk_uint64_t)num), "D"((hawk_uint64_t)arg1), "S"((hawk_uint64_t)arg2), "d"((hawk_uint64_t)arg3) \
		: "memory", "cc", "%rcx", "%r11" \
	)

#define HAWK_SYSCALL4(ret,num,arg1,arg2,arg3,arg4) \
	__asm__ volatile ( \
		"syscall\n" \
		: "=a"(ret) \
		: "a"((hawk_uint64_t)num), "D"((hawk_uint64_t)arg1), "S"((hawk_uint64_t)arg2), "d"((hawk_uint64_t)arg3),  "c"((hawk_uint64_t)arg4) \
		: "memory", "cc", "%rcx", "%r11" \
	)

#elif defined(__linux) && defined(__GNUC__) && defined(__i386) 

#include <sys/syscall.h>

#define HAWK_SYSCALL0(ret,num) \
	__asm__ volatile ( \
		"int $0x80\n" \
		: "=a"(ret) \
		: "a"((hawk_uint32_t)num) \
		: "memory" \
	)

/*
#define HAWK_SYSCALL1(ret,num,arg1) \
	__asm__ volatile ( \
		"int $0x80\n"  \
		: "=a"(ret)  \
		: "a"((hawk_uint32_t)num), "b"((hawk_uint32_t)arg1) \
		: "memory" \
	)

GCC in x86 PIC mode uses ebx to store the GOT table. so the macro shouldn't
clobber the ebx register. this modified version stores ebx before interrupt
and restores it after interrupt.
*/
#define HAWK_SYSCALL1(ret,num,arg1) \
	__asm__ volatile ( \
		"push %%ebx\n\t" \
		"movl %2, %%ebx\n\t" \
		"int $0x80\n\t" \
		"pop %%ebx\n" \
		: "=a"(ret) \
		: "a"((hawk_uint32_t)num), "r"((hawk_uint32_t)arg1) \
		: "memory" \
	)

/*
#define HAWK_SYSCALL2(ret,num,arg1,arg2) \
	__asm__ volatile ( \
		"int $0x80\n"  \
		: "=a"(ret) \
		: "a"((hawk_uint32_t)num), "b"((hawk_uint32_t)arg1), "c"((hawk_uint32_t)arg2) \
		: "memory" \
	)
*/
#define HAWK_SYSCALL2(ret,num,arg1,arg2) \
	__asm__ volatile ( \
		"push %%ebx\n\t" \
		"movl %2, %%ebx\n\t" \
		"int $0x80\n\t"  \
		"pop %%ebx\n" \
		: "=a"(ret) \
		: "a"((hawk_uint32_t)num), "r"((hawk_uint32_t)arg1), "c"((hawk_uint32_t)arg2) \
		: "memory" \
	)

/*
#define HAWK_SYSCALL3(ret,num,arg1,arg2,arg3) \
	__asm__ volatile ( \
		"int $0x80\n"  \
		: "=a"(ret)  \
		: "a"((hawk_uint32_t)num), "b"((hawk_uint32_t)arg1), "c"((hawk_uint32_t)arg2), "d"((hawk_uint32_t)arg3) \
		: "memory" \
	)
*/
#define HAWK_SYSCALL3(ret,num,arg1,arg2,arg3) \
	__asm__ volatile ( \
		"push %%ebx\n\t" \
		"movl %2, %%ebx\n\t" \
		"int $0x80\n\t" \
		"pop %%ebx\n" \
		: "=a"(ret) \
		: "a"((hawk_uint32_t)num), "r"((hawk_uint32_t)arg1), "c"((hawk_uint32_t)arg2), "d"((hawk_uint32_t)arg3) \
		: "memory" \
	)

#define HAWK_SYSCALL4(ret,num,arg1,arg2,arg3,arg4) \
	__asm__ volatile ( \
		"push %%ebx\n\t" \
		"movl %2, %%ebx\n\t" \
		"int $0x80\n\t" \
		"pop %%ebx\n" \
		: "=a"(ret) \
		: "a"((hawk_uint32_t)num), "r"((hawk_uint32_t)arg1), "c"((hawk_uint32_t)arg2), "d"((hawk_uint32_t)arg3), "S"((hawk_uint32_t)arg4) \
		: "memory" \
	)

#endif

/* ------------------------------------------------------------------------ */

#endif
