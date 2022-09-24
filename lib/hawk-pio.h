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

#ifndef _HAWK_PIO_H_
#define _HAWK_PIO_H_

#include <hawk-cmn.h>
#include <hawk-tio.h>

/** \file 
 * This file defines a piped interface to a child process. You can execute
 * a child process, read and write to its stdin, stdout, stderr, and terminate
 * it. It provides more advanced interface than popen() and pclose().
 */

/**
 * The hawk_pio_flag_t defines enumerators to compose flags to hawk_pio_open().
 */
enum hawk_pio_flag_t
{
	/** enable text based I/O. */
	HAWK_PIO_TEXT          = (1 << 0),
	HAWK_PIO_IGNOREECERR = (1 << 1),
	HAWK_PIO_NOAUTOFLUSH   = (1 << 2),

	/** execute the command via a system shell 
	 * (/bin/sh on unix/linux, cmd.exe on windows and os2) */
	HAWK_PIO_SHELL         = (1 << 3),

	/** indicate that the command to hawk_pio_open() is a multi-byte string.
	 *  it is useful if #HAWK_OOCH_IS_UCH is defined. */
	HAWK_PIO_BCSTRCMD        = (1 << 4),

	/** don't attempt to close open file descriptors unknown to pio.
	 *  it is useful only on a unix-like systems where file descriptors
	 *  not set with FD_CLOEXEC are inherited by a child process.
	 *  you're advised to set this option if all normal file descriptors 
	 *  in your application are open with FD_CLOEXEC set. it can skip 
	 *  checking a bunch of file descriptors and arranging to close
	 *  them to prevent inheritance. */
	HAWK_PIO_NOCLOEXEC     = (1 << 5),

	/** indidate that the command to hawk_pio_open()/hawk_pio_init() is 
	 *  a pointer to a #hawk_pio_fnc_t structure. supported on unix/linux
	 *  only */
	HAWK_PIO_FNCCMD        = (1 << 6),

	/** write to stdin of a child process */
	HAWK_PIO_WRITEIN       = (1 << 8),
	/** read stdout of a child process */
	HAWK_PIO_READOUT       = (1 << 9),
	/** read stderr of a child process */
	HAWK_PIO_READERR       = (1 << 10),

	/** redirect stderr to stdout (2>&1, require #HAWK_PIO_READOUT) */
	HAWK_PIO_ERRTOOUT      = (1 << 11),
	/** redirect stdout to stderr (1>&2, require #HAWK_PIO_READERR) */
	HAWK_PIO_OUTTOERR      = (1 << 12),

	/** redirect stdin to the null device (</dev/null, <NUL) */
	HAWK_PIO_INTONUL       = (1 << 13),
	/** redirect stdin to the null device (>/dev/null, >NUL) */
	HAWK_PIO_ERRTONUL      = (1 << 14),
	/** redirect stderr to the null device (2>/dev/null, 2>NUL) */
	HAWK_PIO_OUTTONUL      = (1 << 15),

	/** drop stdin */
	HAWK_PIO_DROPIN        = (1 << 16), 
	/** drop stdout */
	HAWK_PIO_DROPOUT       = (1 << 17),
	/** drop stderr */
	HAWK_PIO_DROPERR       = (1 << 18),

	/** do not reread if read has been interrupted */
	HAWK_PIO_READNORETRY   = (1 << 21), 
	/** do not rewrite if write has been interrupted */
	HAWK_PIO_WRITENORETRY  = (1 << 22),
	/** return immediately from hawk_pio_wait() if a child has not exited */
	HAWK_PIO_WAITNOBLOCK   = (1 << 23),
	/** do not wait again if waitpid has been interrupted */
	HAWK_PIO_WAITNORETRY   = (1 << 24),

	/** put stdin to non-blocking mode (only on supported platforms) */
	HAWK_PIO_INNOBLOCK     = (1 << 25),
	/** put stdout to non-blocking mode (only on supported platforms)*/
	HAWK_PIO_OUTNOBLOCK    = (1 << 26),
	/** put stderr to non-blocking mode (only on supported platforms) */
	HAWK_PIO_ERRNOBLOCK    = (1 << 27)
};

/**
 * The hawk_pio_hid_t type defines pipe IDs established to a child process.
 */
enum hawk_pio_hid_t
{
	HAWK_PIO_IN  = 0, /**< stdin of a child process */ 
	HAWK_PIO_OUT = 1, /**< stdout of a child process */
	HAWK_PIO_ERR = 2  /**< stderr of a child process */
};
typedef enum hawk_pio_hid_t hawk_pio_hid_t;


typedef int (*hawk_pio_fncptr_t) (void* ctx);

/**
 * The hawk_pio_fnc_t type defines a structure to point to the function
 * executed in a child process when #HAWK_PIO_FNCCMD is specified.
 */
typedef struct hawk_pio_fnc_t hawk_pio_fnc_t;
struct hawk_pio_fnc_t
{
	hawk_pio_fncptr_t ptr;
	void* ctx;
};

#if defined(_WIN32)
	/* <winnt.h> => typedef PVOID HANDLE; */
	typedef void* hawk_pio_hnd_t; /**< defines a pipe handle type */
	typedef void* hawk_pio_pid_t; /**< defines a process handle type */
#	define  HAWK_PIO_HND_NIL ((hawk_pio_hnd_t)HAWK_NULL)
#	define  HAWK_PIO_PID_NIL ((hawk_pio_pid_t)HAWK_NULL)
#elif defined(__OS2__)
	/* <os2def.h> => typedef LHANDLE HFILE;
	                 typedef LHANDLE PID;
	                 typedef unsigned long LHANDLE; */
	typedef unsigned long hawk_pio_hnd_t; /**< defines a pipe handle type */
	typedef unsigned long hawk_pio_pid_t; /**< defined a process handle type */
#	define  HAWK_PIO_HND_NIL ((hawk_pio_hnd_t)-1)
#	define  HAWK_PIO_PID_NIL ((hawk_pio_pid_t)-1)
#elif defined(__DOS__)
	typedef int hawk_pio_hnd_t; /**< defines a pipe handle type */
	typedef int hawk_pio_pid_t; /**< defines a process handle type */
#	define  HAWK_PIO_HND_NIL ((hawk_pio_hnd_t)-1)
#	define  HAWK_PIO_PID_NIL ((hawk_pio_pid_t)-1)
#else
	typedef int hawk_pio_hnd_t; /**< defines a pipe handle type */
	typedef int hawk_pio_pid_t; /**< defines a process handle type */
#	define  HAWK_PIO_HND_NIL ((hawk_pio_hnd_t)-1)
#	define  HAWK_PIO_PID_NIL ((hawk_pio_pid_t)-1)
#endif

typedef struct hawk_pio_t hawk_pio_t;
typedef struct hawk_pio_pin_t hawk_pio_pin_t;

struct hawk_pio_pin_t
{
	hawk_pio_hnd_t handle;
	hawk_tio_t*    tio;
	hawk_pio_t*    self;	
};


/**
 * The hawk_pio_t type defines a structure to store status for piped I/O
 * to a child process. The hawk_pio_xxx() funtions are written around this
 * type. Do not change the value of each field directly. 
 */
struct hawk_pio_t
{
	hawk_gem_t*       gem;
	int               flags;  /**< options */
	hawk_pio_pid_t    child;   /**< handle to a child process */
	hawk_pio_pin_t    pin[3];
};

/** access the \a child field of the #hawk_pio_t structure */
#define HAWK_PIO_CHILD(pio)     ((pio)->child)
/** get the native handle from the #hawk_pio_t structure */
#define HAWK_PIO_HANDLE(pio,hid) ((pio)->pin[hid].handle)

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * The hawk_pio_open() function executes a command \a cmd and establishes
 * pipes to it. #HAWK_PIO_SHELL causes the function to execute \a cmd via 
 * the default shell of an underlying system: /bin/sh on *nix, cmd.exe on win32.
 * On *nix systems, a full path to the command is needed if it is not specified.
 * If \a env is #HAWK_NULL, the environment of \a cmd inherits that of the 
 * calling process. If you want to pass an empty environment, you can pass
 * an empty \a env object with no items inserted. If #HAWK_PIO_BCSTRCMD is 
 * specified in \a flags, \a cmd is treated as a multi-byte string whose 
 * character type is #hawk_bch_t.
 * \return #hawk_pio_t object on success, #HAWK_NULL on failure
 */
HAWK_EXPORT hawk_pio_t* hawk_pio_open (
	hawk_gem_t*        gem,    /**< gem */
	hawk_oow_t         ext,    /**< extension size */
	const hawk_ooch_t* cmd,    /**< command to execute */
	int                flags   /**< 0 or a number OR'ed of the #hawk_pio_flag_t enumerators*/
);

/**
 * The hawk_pio_close() function closes pipes to a child process and waits for
 * the child process to exit.
 */
HAWK_EXPORT void hawk_pio_close (
	hawk_pio_t* pio /**< pio object */
);

/**
 * The hawk_pio_init() functions performs the same task as the hawk_pio_open()
 * except that you need to allocate a #hawk_pio_t structure and pass it to the
 * function.
 * \return 0 on success, -1 on failure
 */
HAWK_EXPORT int hawk_pio_init (
	hawk_pio_t*        pio,    /**< pio object */
	hawk_gem_t*        gem,    /**< gem */
	const hawk_ooch_t* cmd,    /**< command to execute */
	int                flags   /**< 0 or a number OR'ed of the #hawk_pio_flag_t enumerators*/
);

/**
 * The hawk_pio_fini() function performs the same task as hawk_pio_close()
 * except that it does not destroy a #hawk_pio_t structure pointed to by \a pio.
 */
HAWK_EXPORT void hawk_pio_fini (
	hawk_pio_t* pio /**< pio object */
);

#if defined(HAWK_HAVE_INLINE)
static HAWK_INLINE void* hawk_pio_getxtn (hawk_pio_t* pio) { return (void*)(pio + 1); }
#else
#define hawk_pio_getxtn(pio) ((void*)((hawk_pio_t*)(pio) + 1))
#endif

/**
 * The hawk_pio_getcmgr() function returns the current character manager.
 * It returns #HAWK_NULL is \a pio is not opened with #HAWK_PIO_TEXT.
 */
HAWK_EXPORT hawk_cmgr_t* hawk_pio_getcmgr (
	hawk_pio_t*    pio,
	hawk_pio_hid_t hid
);

/**
 * The hawk_pio_setcmgr() function changes the character manager to \a cmgr.
 * The character manager is used only if \a pio is opened with #HAWK_PIO_TEXT.
 */
HAWK_EXPORT void hawk_pio_setcmgr (
	hawk_pio_t*    pio,
	hawk_pio_hid_t hid,
	hawk_cmgr_t*   cmgr
);

/**
 * The hawk_pio_gethnd() function gets a pipe handle.
 * \return pipe handle
 */
HAWK_EXPORT hawk_pio_hnd_t hawk_pio_gethnd (
	const hawk_pio_t* pio, /**< pio object */
	hawk_pio_hid_t    hid  /**< handle ID */
);

/**
 * The hawk_pio_getchild() function gets a process handle.
 * \return process handle
 */
HAWK_EXPORT hawk_pio_pid_t hawk_pio_getchild (
	const hawk_pio_t* pio /**< pio object */
);

/**
 * The hawk_pio_read() fucntion reads at most \a size bytes/characters
 * and stores them to the buffer pointed to by \a buf.
 * \return -1 on failure, 0 on EOF, data length read on success
 */
HAWK_EXPORT hawk_ooi_t hawk_pio_read (
	hawk_pio_t*    pio,  /**< pio object */
	hawk_pio_hid_t hid,  /**< handle ID */
	void*          buf,  /**< buffer to fill */
	hawk_oow_t     size  /**< buffer size */
);

HAWK_EXPORT hawk_ooi_t hawk_pio_readbytes (
	hawk_pio_t*    pio,  /**< pio object */
	hawk_pio_hid_t hid,  /**< handle ID */
	void*          buf,  /**< buffer to fill */
	hawk_oow_t     size  /**< buffer size */
);


/**
 * The hawk_pio_write() function writes up \a size bytes/characters 
 * from the buffer pointed to by \a data. If #HAWK_PIO_TEXT is used 
 * and the \a size parameter is (hawk_oow_t)-1, the function treats 
 * the \a data parameter as a pointer to a null-terminated string.
 * (hawk_oow_t)-1 into \a size is not treated specially if #HAWK_PIO_TEXT
 * is not set.
 *
 * \return -1 on failure, data length written on success
 */
HAWK_EXPORT hawk_ooi_t hawk_pio_write (
	hawk_pio_t*    pio,   /**< pio object */
	hawk_pio_hid_t hid,   /**< handle ID */
	const void*    data,  /**< data to write */
	hawk_oow_t     size   /**< data size */
);

HAWK_EXPORT hawk_ooi_t hawk_pio_writebytes (
	hawk_pio_t*    pio,   /**< pio object */
	hawk_pio_hid_t hid,   /**< handle ID */
	const void*    data,  /**< data to write */
	hawk_oow_t     size   /**< data size */
);

/**
 * The hawk_pio_flush() flushes buffered data if #HAWK_PIO_TEXT has been 
 * specified to hawk_pio_open() and hawk_pio_init().
 */
HAWK_EXPORT hawk_ooi_t hawk_pio_flush (
	hawk_pio_t*    pio, /**< pio object */
	hawk_pio_hid_t hid  /**< handle ID */
);

/**
 * The hawk_pio_drain() drops unflushed input and output data in the 
 * buffer. 
 */
HAWK_EXPORT void hawk_pio_drain (
	hawk_pio_t*    pio, /**< pio object */
	hawk_pio_hid_t hid  /**< handle ID */
);

/**
 * The hawk_pio_end() function closes a pipe to a child process
 */
HAWK_EXPORT void hawk_pio_end (
	hawk_pio_t*    pio, /**< pio object */
	hawk_pio_hid_t hid  /**< handle ID */
);

/**
 * The hawk_pio_wait() function waits for a child process to terminate.
 * #HAWK_PIO_WAIT_NORETRY causes the function to return an error and set the 
 * \a pio->errnum field to #HAWK_PIO_EINTR if the underlying system call has
 * been interrupted. If #HAWK_PIO_WAIT_NOBLOCK is used, the return value of 256
 * indicates that the child process has not terminated. Otherwise, 256 is never
 * returned.
 *
 * \return
 *  -1 on error, 256 if the child is alive and #HAWK_PIO_WAIT_NOBLOCK is used,
 *  a number between 0 and 255 inclusive if the child process ends normally,
 *  256 + signal number if the child process is terminated by a signal.
 */
HAWK_EXPORT int hawk_pio_wait (
	hawk_pio_t* pio /**< pio object */
);

/**
 * The hawk_pio_kill() function terminates a child process by force.
 * You should know the danger of calling this function as the function can
 * kill a process that is not your child process if it has terminated but
 * there is a new process with the same process handle.
 * \return 0 on success, -1 on failure
 */ 
HAWK_EXPORT int hawk_pio_kill (
	hawk_pio_t* pio /**< pio object */
);

#if defined(__cplusplus)
}
#endif

#endif
