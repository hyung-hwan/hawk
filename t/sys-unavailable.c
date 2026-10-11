/* Exercise unsupported sys APIs independently of descriptor fallback tests. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hawk-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include "syscall.h"
#include <termios.h>
#endif

/* Include native headers before undefining their wait-status macros, so the
 * module's subsequent includes don't restore them. */
#undef HAVE_EPOLL_CREATE
#define HAWK_SYS_NO_MUX
#undef WIFEXITED
#undef WEXITSTATUS
#undef WIFSIGNALED
#undef WTERMSIG
#undef WCOREDUMP
#undef HAVE_OPENPTY
#undef HAVE_POSIX_OPENPT
#undef HAVE_GRANTPT
#undef HAVE_UNLOCKPT
#undef HAVE_PTSNAME
#undef HAVE_PTSNAME_R
#undef HAVE_TTYNAME
#undef HAVE_TTYNAME_R
#undef HAVE_SETSID
#undef HAVE_SETPGID
#undef HAVE_TCGETPGRP
#undef HAVE_TCSETPGRP
#undef TIOCGWINSZ
#undef TIOCSWINSZ
#undef TIOCSCTTY
#define hawk_mod_sys hawk_test_mod_sys_unavailable
#include "../lib/mod-sys.c"
