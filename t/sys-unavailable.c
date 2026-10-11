/* Exercise unsupported sys APIs independently of descriptor fallback tests. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hawk-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include "syscall.h"
#endif

/* Include native headers before undefining their wait-status macros, so the
 * module's subsequent includes don't restore them. */
#undef HAVE_EPOLL_CREATE
#undef WIFEXITED
#undef WEXITSTATUS
#undef WIFSIGNALED
#undef WTERMSIG
#undef WCOREDUMP
#define hawk_mod_sys hawk_test_mod_sys_unavailable
#include "../lib/mod-sys.c"
