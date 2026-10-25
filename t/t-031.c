/* Exercise the explicit error paths when interface lookup is unsupported. */
#include "hawk-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include "syscall.h"
#include <sys/socket.h>
#if defined(HAVE_NET_IF_H)
#include <net/if.h>
#endif
#if defined(HAVE_SYS_SOCKIO_H)
#include <sys/sockio.h>
#endif
#undef SIOCGIFINDEX
#undef SIOCGIFNAME
#undef SIOCGIFCONF
#undef SIOCGIFANUM
#undef SIOCGIFNUM
#undef HAVE_IF_NAMETOINDEX
#undef HAVE_IF_INDEXTONAME
#endif

#include "../lib/gem-nwif.c"
#include "../lib/skad.c"
#include "t-026.c"
