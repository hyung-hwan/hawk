/* Exercise the interface-name lookup fallback with a deterministic interface.
 * Include system headers first so they cannot restore the ioctl macros. */
#include "hawk-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include <sys/socket.h>
#if defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif
#if defined(HAVE_NET_IF_H)
#include <net/if.h>
#endif
#if defined(HAVE_SYS_SOCKIO_H)
#include <sys/sockio.h>
#endif
#include <errno.h>

static hawk_bch_t* test_if_indextoname (unsigned int index, hawk_bch_t* name)
{
	if (index != 1)
	{
		errno = ENOENT;
		return HAWK_NULL;
	}
	hawk_copy_bcstr(name, 3, "lo");
	return name;
}

static unsigned int test_if_nametoindex (const hawk_bch_t* name)
{
	if (hawk_comp_bcstr(name, "lo", 0) == 0) return 1;
	errno = ENOENT;
	return 0;
}

#undef SIOCGIFINDEX
#undef SIOCGIFNAME
#undef HAVE_IF_NAMETOINDEX
#undef HAVE_IF_INDEXTONAME
#define HAVE_IF_NAMETOINDEX 1
#define HAVE_IF_INDEXTONAME 1
#define if_nametoindex test_if_nametoindex
#define if_indextoname test_if_indextoname
#endif

#include "../lib/gem-nwif.c"
#include "../lib/skad.c"
#include "t-026.c"
