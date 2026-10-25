/* Run the address suite through the SCO interface-enumeration fallback. */
#include "hawk-prv.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(HAVE_NET_IF_H)
#include "syscall.h"
#include <sys/socket.h>
#include <net/if.h>
#if defined(HAVE_SYS_SOCKIO_H)
#include <sys/sockio.h>
#endif
#if defined(SIOCGIFCONF)
#define TEST_SCO_IFCONF
#endif
#endif

#if defined(TEST_SCO_IFCONF)
#undef SIOCGIFANUM
#define SIOCGIFANUM 0x7fffffffUL

static int test_socket (int domain, int type, int protocol)
{
	(void)domain;
	(void)type;
	(void)protocol;
	return 123;
}

static int test_ioctl (int fd, unsigned long request, void* data)
{
	struct ifconf* ifc;

	(void)fd;
	if (request == SIOCGIFANUM)
	{
		*(int*)data = 1;
		return 0;
	}
	if (request == SIOCGIFCONF)
	{
		ifc = data;
		if (!ifc->ifc_req || ifc->ifc_len < HAWK_SIZEOF(struct ifreq))
		{
			errno = EINVAL;
			return -1;
		}
		HAWK_MEMSET(ifc->ifc_req, 0, HAWK_SIZEOF(struct ifreq));
		hawk_copy_bcstr(ifc->ifc_req[0].ifr_name, HAWK_COUNTOF(ifc->ifc_req[0].ifr_name), "lo");
		ifc->ifc_len = HAWK_SIZEOF(struct ifreq);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

static int test_close (int fd)
{
	(void)fd;
	return 0;
}

#undef SIOCGIFINDEX
#undef SIOCGIFNAME
#undef HAVE_IF_NAMETOINDEX
#undef HAVE_IF_INDEXTONAME
#define socket test_socket
#define ioctl test_ioctl
#undef HAWK_CLOSE
#define HAWK_CLOSE(fd) test_close(fd)

#include "../lib/gem-nwif.c"
#include "../lib/skad.c"
#include "t-026.c"
#else
#include "tap.h"

int main (void)
{
	skip_all("interface-enumeration structures are unavailable");
	return 0;
}
#endif
