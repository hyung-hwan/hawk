/* Exercise the dup2 fallback and interface socket failures deterministically. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hawk-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include <sys/socket.h>
#include <errno.h>

extern int hawk_test_sys_fcntl (int fd, int cmd, ...);

static int fail_ifcfg_socket (int domain, int type, int protocol)
{
	errno = EACCES;
	return -1;
}

#define socket fail_ifcfg_socket
#define hawk_gem_getifcfg hawk_test_gem_getifcfg
#include "../lib/gem-nwif2.c"
#undef socket

#undef HAVE_DUP3
#undef HAVE_OPENPTY /* exercise openpty() assembled from the POSIX PTY APIs */
#define fcntl hawk_test_sys_fcntl
#define hawk_mod_sys hawk_test_mod_sys_fallback
#include "../lib/mod-sys.c"
#else
extern int hawk_test_mod_sys_fault (hawk_mod_t* mod, hawk_t* hawk);
int hawk_test_mod_sys_fallback (hawk_mod_t* mod, hawk_t* hawk)
{
	return hawk_test_mod_sys_fault(mod, hawk);
}
#endif
