/* Interface lookup errors must survive a failure in descriptor cleanup. */
#include "hawk-prv.h"
#include "tap.h"

#if !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__) && defined(HAVE_NET_IF_H)
#include "syscall.h"
#include <sys/socket.h>
#include <net/if.h>
#if defined(HAVE_SYS_SOCKIO_H)
#include <sys/sockio.h>
#endif
#if defined(SIOCGIFINDEX) && defined(SIOCGIFNAME)
#define TEST_LOOKUP_IOCTLS
#endif
#endif

#if defined(TEST_LOOKUP_IOCTLS)
static int test_socket (int domain, int type, int protocol)
{
	(void)domain;
	(void)type;
	(void)protocol;
	return 123;
}

static int test_ioctl (int fd, unsigned long request, void* data)
{
	(void)fd;
	(void)request;
	(void)data;
	errno = ENOENT;
	return -1;
}

static int test_close (int fd)
{
	(void)fd;
	errno = EBADF;
	return -1;
}

#define socket test_socket
#define ioctl test_ioctl
#undef HAWK_CLOSE
#define HAWK_CLOSE(fd) test_close(fd)
#include "../lib/gem-nwif.c"

int main (void)
{
	hawk_t* hawk;
	hawk_gem_t* gem;
	hawk_bch_t bytes[32];
	hawk_uch_t wide[32];
	const hawk_bch_t* labels[] =
	{
		"byte C-string lookup preserves the ioctl error across close failure",
		"byte-slice lookup preserves the ioctl error across close failure",
		"wide C-string lookup preserves the ioctl error across close failure",
		"wide-slice lookup preserves the ioctl error across close failure",
		"byte reverse lookup preserves the ioctl error across close failure",
		"wide reverse lookup preserves the ioctl error across close failure"
	};
	hawk_oow_t i, j;
	unsigned int index;
	int n, good;

	no_plan();
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return 1; }
	gem = hawk_getgem(hawk);
	for (i = 0; i < HAWK_COUNTOF(labels); i++)
	{
		index = 12345;
		for (j = 0; j < HAWK_COUNTOF(bytes); j++) bytes[j] = wide[j] = 'X';
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		switch (i)
		{
			case 0: n = hawk_gem_bcstrtoifindex(gem, "bad-if", &index); break;
			case 1: n = hawk_gem_bcharstoifindex(gem, "bad-if", 6, &index); break;
			case 2: n = hawk_gem_ucstrtoifindex(gem, HAWK_UT("bad-if"), &index); break;
			case 3: n = hawk_gem_ucharstoifindex(gem, HAWK_UT("bad-if"), 6, &index); break;
			case 4: n = hawk_gem_ifindextobcstr(gem, 99, bytes, HAWK_COUNTOF(bytes)); break;
			default: n = hawk_gem_ifindextoucstr(gem, 99, wide, HAWK_COUNTOF(wide)); break;
		}
		good = n <= -1 && hawk_gem_geterrnum(gem) == HAWK_ENOENT && index == 12345;
		for (j = 0; j < HAWK_COUNTOF(bytes); j++)
		{
			if (bytes[j] != 'X' || wide[j] != 'X') good = 0;
		}
		OK(good, labels[i]);
	}
	hawk_close(hawk);
	return exit_status();
}
#else
int main (void)
{
	skip_all("interface lookup ioctls are unavailable");
	return 0;
}
#endif
