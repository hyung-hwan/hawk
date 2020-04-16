/*
 * $Id$
 *
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

#if 1
#include "hawk-prv.h"
#include "utl-skad.h"
#include <hawk-sio.h>


#if defined(_WIN32)
#	include <winsock2.h>
#	include <ws2tcpip.h>
/*#	include <iphlpapi.h> */
#elif defined(__OS2__)
#	if defined(TCPV40HDRS)
#		define BSD_SELECT
#	endif
#	include <types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <sys/ioctl.h>
#	include <nerrno.h>
#	if defined(TCPV40HDRS)
#		define USE_SELECT
#		include <sys/select.h>
#	else
#		include <unistd.h>
#	endif
#elif defined(__DOS__)
	/* TODO: */
#else
#	include "syscall.h"
#	include <sys/socket.h>
#	include <netinet/in.h>
#	if defined(HAVE_SYS_IOCTL_H)
#		include <sys/ioctl.h>
#	endif
#	if defined(HAVE_NET_IF_H)
#		include <net/if.h>
#	endif
#	if defined(HAVE_NET_IF_DL_H)
#		include <net/if_dl.h>
#	endif
#	if defined(HAVE_SYS_SOCKIO_H)
#		include <sys/sockio.h>
#	endif
#	if defined(HAVE_IFADDRS_H)
#		include <ifaddrs.h>
#	endif
#	if defined(HAVE_SYS_SYSCTL_H)
#		include <sys/sysctl.h>
#	endif
#	if defined(HAVE_SYS_STROPTS_H)
#		include <sys/stropts.h> /* stream options */
#	endif
#	if defined(HAVE_SYS_MACSTAT_H)
#		include <sys/macstat.h>
#	endif
#	if defined(HAVE_LINUX_ETHTOOL_H)
#		include <linux/ethtool.h>
#	endif
#	if defined(HAVE_LINUX_SOCKIOS_H)
#		include <linux/sockios.h>
#	endif
#endif

#if defined(AF_INET6)
static int prefix_to_in6 (int prefix, struct in6_addr* in6)
{
	int i;

	if (prefix < 0 || prefix > HAWK_SIZEOF(*in6) * 8) return -1;

	HAWK_MEMSET (in6, 0, HAWK_SIZEOF(*in6));
	for (i = 0; ; i++)
	{
		 if (prefix > 8)
		 {
			in6->s6_addr[i] = 0xFF;
			prefix -= 8;
		 }
		 else
		 {
			in6->s6_addr[i] = 0xFF << (8 - prefix);
			break;
		 }

	}

	return 0;
}
#endif

static HAWK_INLINE void copy_to_skad (struct sockaddr* sa, hawk_skad_t* skad)
{
	HAWK_MEMCPY (skad, sa, HAWK_SIZEOF(*skad));
}
/*

#if defined(HAVE_NET_IF_DL_H)
#	include <net/if_dl.h>
#endif
 #ifndef SIOCGLIFHWADDR
#define SIOCGLIFHWADDR  _IOWR('i', 192, struct lifreq)
 #endif
 
    struct lifreq lif;

    memset(&lif, 0, sizeof(lif));
    strlcpy(lif.lifr_name, ifname, sizeof(lif.lifr_name));

    if (ioctl(sock, SIOCGLIFHWADDR, &lif) != -1) {
        struct sockaddr_dl *sp;
        sp = (struct sockaddr_dl *)&lif.lifr_addr;
        memcpy(buf, &sp->sdl_data[0], sp->sdl_alen);
        return sp->sdl_alen;
    }
*/


#if 0
#if defined(SIOCGLIFCONF) && defined(SIOCGLIFNUM) && \
    defined(HAVE_STRUCT_LIFCONF) && defined(HAVE_STRUCT_LIFREQ)
static int get_nwifs (hawk_gem_t* gem, int s, int f, hawk_xptl_t* nwifs)
{
	struct lifnum ifn;
	struct lifconf ifc;
	hawk_xptl_t b;
	hawk_oow_t ifcount;

	ifcount = 0;

	b.ptr = HAWK_NULL;
	b.len = 0;

	do
	{
		ifn.lifn_family = f;
		ifn.lifn_flags  = 0;
		if (ioctl(s, SIOCGLIFNUM, &ifn) <= -1) 
		{
			hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			goto oops;
		}

		if (b.ptr)
		{
			/* b.ptr won't be HAWK_NULL when retrying */
			if (ifn.lifn_count <= ifcount) break;
		}
		
		/* +1 for extra space to leave empty
		 * if SIOCGLIFCONF returns the same number of
		 * intefaces as SIOCLIFNUM */
		b.len = (ifn.lifn_count + 1) * HAWK_SIZEOF(struct lifreq);
		b.ptr = hawk_gem_allocmem(gem, b.len);
		if (b.ptr == HAWK_NULL)
		{
			hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			goto oops;
		}

		ifc.lifc_family = f;
		ifc.lifc_flags = 0;
		ifc.lifc_len = b.len;
		ifc.lifc_buf = b.ptr;

		if (ioctl(s, SIOCGLIFCONF, &ifc) <= -1) 
		{
			hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
			goto oops;
		}

		ifcount = ifc.lifc_len / HAWK_SIZEOF(struct lifreq);
	}
	while (ifcount > ifn.lifn_count); 
	/* the while condition above is for checking if
	 * the buffer got full. when it's full, there is a chance
	 * that there are more interfaces. */
		
	nwifs->ptr = b.ptr;
	nwifs->len = ifcount;
	return 0;

oops:
	if (b.ptr) hawk_gem_freemem (gem, b.ptr);
	return -1;
}
#endif
#endif

#if 0
static void free_nwifcfg (hawk_gem_t* gem, hawk_ifcfg_node_t* cfg)
{
	hawk_ifcfg_node_t* cur;

	while (cfg)
	{
		cur = cfg;
		cfg = cur->next;
		if (cur->name) hawk_gem_freemem (gem, cur->name);
		hawk_gem_freemem (gem, cur);
	}
}

int hawk_gem_getifcfg (hawk_gem_t* gem, hawk_ifcfg_t* cfg)
{
#if defined(SIOCGLIFCONF)
	struct lifreq* ifr, ifrbuf;
	hawk_oow_t i;
	int s, f;
	int s4 = -1;
	int s6 = -1;
	hawk_xptl_t nwifs = { HAWK_NULL, 0 };
	hawk_ifcfg_node_t* head = HAWK_NULL;
	hawk_ifcfg_node_t* tmp;

	HAWK_ASSERT (cfg->mmgr != HAWK_NULL);

#if defined(AF_INET) || defined(AF_INET6)
	#if defined(AF_INET)
	s4 = socket(AF_INET, SOCK_DGRAM, 0);
	if (s4 <= -1) goto sys_oops;
	#endif

	#if defined(AF_INET6)
	s6 = socket (AF_INET6, SOCK_DGRAM, 0);
	if (s6 <= -1) goto oops;
	#endif
#else
	/* no implementation */
	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	goto oops;
#endif
	
	if (get_nwifs(cfg->mmgr, s4, AF_UNSPEC, &nwifs) <= -1) goto oops;

	ifr = nwifs.ptr;
	for (i = 0; i < nwifs.len; i++, ifr++)
	{
		f = ifr->lifr_addr.ss_family;

		if (f == AF_INET) s = s4;
		else if (f == AF_INET6) s = s6;
		else continue;

		tmp = hawk_gem_allocmem(gem, HAWK_SIZEOF(*tmp));
		if (tmp == HAWK_NULL) goto oops;

		HAWK_MEMSET (tmp, 0, HAWK_SIZEOF(*tmp));
		tmp->next = head;
		head = tmp;

#if defined(HAWK_OOCH_IS_BCH)
		head->name = hawk_gem_dupbcstr(gem, ifr->lifr_name, HAWK_NULL);
#else
		head->name = hawk_gem_dupbtoucstr(gem, ifr->lifr_name, HAWK_NULL, 0);
#endif
		if (head->name == HAWK_NULL) goto oops;

		copy_to_skad ((struct sockaddr*)&ifr->lifr_addr, &head->addr);

		hawk_copy_bcstr (ifrbuf.lifr_name, HAWK_COUNTOF(ifrbuf.lifr_name), ifr->lifr_name);
		if (ioctl(s, SIOCGLIFFLAGS, &ifrbuf) <= -1) goto sys_oops;
		if (ifrbuf.lifr_flags & IFF_UP) head->flags |= HAWK_IFCFG_UP;
		if (ifrbuf.lifr_flags & IFF_BROADCAST) 
		{
			if (ioctl(s, SIOCGLIFBRDADDR, &ifrbuf) <= -1) goto sys_oops;
			copy_to_skad ((struct sockaddr*)&ifrbuf.lifr_addr, &head->bcast);
			head->flags |= HAWK_IFCFG_BCAST;
		}
		if (ifrbuf.lifr_flags & IFF_POINTOPOINT) 
		{
			if (ioctl(s, SIOCGLIFDSTADDR, &ifrbuf) <= -1) goto sys_oops;
			copy_to_skad ((struct sockaddr*)&ifrbuf.lifr_addr, &head->ptop);
			head->flags |= HAWK_IFCFG_PTOP;
		}

		if (ioctl(s, SIOCGLIFINDEX, &ifrbuf) <= -1) goto sys_oops;
		head->index = ifrbuf.lifr_index;

		if (ioctl(s, SIOCGLIFNETMASK, &ifrbuf) <= -1) goto sys_oops;
		copy_to_skad ((struct sockaddr*)&ifrbuf.lifr_addr, &head->mask);
	}

	hawk_gem_freemem (gem, nwifs.ptr);
	close (s6);
	close (s4);

	cfg->list = head;
	return 0;

sys_oops:
	hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
oops:
	if (head) free_nwifcfg (cfg->mmgr, head);
	if (nwifs.ptr) hawk_gem_freemem (gem, nwifs.ptr);
	if (s6 >= 0) close (s6);
	if (s4 >= 0) close (s4);
	return -1;
#else

	/* TODO  */
	return HAWK_NULL;
#endif
}

void hawk_freenwifcfg (hawk_ifcfg_t* cfg)
{
	free_nwifcfg (cfg->mmgr, cfg->list);
	cfg->list = HAWK_NULL;
}

#endif

#if defined(__linux)
static void read_proc_net_if_inet6 (hawk_gem_t* gem, hawk_ifcfg_t* cfg, struct ifreq* ifr)
{
	/*
     *
	 * # cat /proc/net/if_inet6
	 * 00000000000000000000000000000001 01 80 10 80 lo
	 * +------------------------------+ ++ ++ ++ ++ ++
	 * |                                |  |  |  |  |
	 * 1                                2  3  4  5  6
	 * 
	 * 1. IPv6 address displayed in 32 hexadecimal chars without colons as separator
	 * 2. Netlink device number (interface index) in hexadecimal (see “ip addr” , too)
	 * 3. Prefix length in hexadecimal
	 * 4. Scope value (see kernel source “ include/net/ipv6.h” and “net/ipv6/addrconf.c” for more)
	 * 5. Interface flags (see “include/linux/rtnetlink.h” and “net/ipv6/addrconf.c” for more)
	 * 6. Device name
	 */

	hawk_sio_t* sio;
	hawk_bch_t line[128];
	hawk_bch_t* ptr, * ptr2;
	hawk_ooi_t len;
	hawk_bcs_t tok[6];
	int count, index;

	/* TODO */

	sio = hawk_sio_open(gem, 0, HAWK_T("/proc/net/if_inet6"), HAWK_SIO_IGNOREECERR | HAWK_SIO_READ); 
	if (sio)
	{
		
		while (1)
		{
			len = hawk_sio_getbcstr(sio, line, HAWK_COUNTOF(line));
			if (len <= 0) break;

			count = 0;
			ptr = line;

			while (ptr && count < 6)
			{
				ptr2 = hawk_tokenize_bchars(ptr, len, " \t", 2, &tok[count], 0);

				len -= ptr2 - ptr;
				ptr = ptr2;
				count++;
			}

			if (count >= 6)
			{
				index = hawk_bchars_to_int(tok[1].ptr, tok[1].len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(1, 1, 16), HAWK_NULL, HAWK_NULL);
				if (index == cfg->index)
				{
					int ti;
					hawk_skad_alt_t* skad;

					skad = (hawk_skad_alt_t*)&cfg->addr;
					if (hawk_bchars_to_bin(tok[0].ptr, tok[0].len, (hawk_uint8_t*)&skad->in6.sin6_addr, HAWK_SIZEOF(skad->in6.sin6_addr)) <= -1) break;
					/* tok[3] is the scope type, not the actual scope. 
					 * i leave this code for reference only.
					skad->in6.sin6_scope_id = hawk_bchars_to_int(tok[3].ptr, tok[3].len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(1, 1, 16), HAWK_NULL, HAWK_NULL); */
					skad->in6.sin6_family = HAWK_AF_INET6;

					skad = (hawk_skad_alt_t*)&cfg->mask;
					ti = hawk_bchars_to_int(tok[2].ptr, tok[0].len, HAWK_OOCHARS_TO_INT_MAKE_OPTION(1, 1, 16), HAWK_NULL, HAWK_NULL);
					prefix_to_in6 (ti, &skad->in6.sin6_addr);
					skad->in6.sin6_family = HAWK_AF_INET6;
					goto done;
				}
			}
		}

	done:
		hawk_sio_close (sio);
	}
}
#endif

static int get_ifcfg (hawk_gem_t* gem, int s, hawk_ifcfg_t* cfg, struct ifreq* ifr)
{
#if defined(_WIN32)
	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;

#elif defined(__OS2__)

	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;

#elif defined(__DOS__)

	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;


#elif defined(SIOCGLIFADDR) && defined(SIOCGLIFINDEX) && defined(HAVE_STRUCT_LIFCONF) && defined(HAVE_STRUCT_LIFREQ)
	/* opensolaris */
	struct lifreq lifrbuf;
	hawk_oow_t ml, wl;
	
	HAWK_MEMSET (&lifrbuf, 0, HAWK_SIZEOF(lifrbuf));

	hawk_copy_bcstr (lifrbuf.lifr_name, HAWK_SIZEOF(lifrbuf.lifr_name), ifr->ifr_name);

	if (ioctl(s, SIOCGLIFINDEX, &lifrbuf) <= -1) return -1;
	cfg->index = lifrbuf.lifr_index;

	if (ioctl(s, SIOCGLIFFLAGS, &lifrbuf) <= -1) return -1;
	cfg->flags = 0;
	if (lifrbuf.lifr_flags & IFF_UP) cfg->flags |= HAWK_IFCFG_UP;
	if (lifrbuf.lifr_flags & IFF_RUNNING) cfg->flags |= HAWK_IFCFG_RUNNING;
	if (lifrbuf.lifr_flags & IFF_BROADCAST) cfg->flags |= HAWK_IFCFG_BCAST;
	if (lifrbuf.lifr_flags & IFF_POINTOPOINT) cfg->flags |= HAWK_IFCFG_PTOP;

	if (ioctl (s, SIOCGLIFMTU, &lifrbuf) <= -1) return -1;
	cfg->mtu = lifrbuf.lifr_mtu;
	
	hawk_clear_skad (&cfg->addr);
	hawk_clear_skad (&cfg->mask);
	hawk_clear_skad (&cfg->bcast);
	hawk_clear_skad (&cfg->ptop);
	HAWK_MEMSET (cfg->ethw, 0, HAWK_SIZEOF(cfg->ethw));

	if (ioctl (s, SIOCGLIFADDR, &lifrbuf) >= 0) 
		copy_to_skad ((struct sockaddr*)&lifrbuf.lifr_addr, &cfg->addr);

	if (ioctl (s, SIOCGLIFNETMASK, &lifrbuf) >= 0) 
		copy_to_skad ((struct sockaddr*)&lifrbuf.lifr_addr, &cfg->mask);

	if ((cfg->flags & HAWK_IFCFG_BCAST) &&
	    ioctl (s, SIOCGLIFBRDADDR, &lifrbuf) >= 0)
	{
		copy_to_skad ((struct sockaddr*)&lifrbuf.lifr_broadaddr, &cfg->bcast);
	}
	if ((cfg->flags & HAWK_IFCFG_PTOP) &&
	    ioctl (s, SIOCGLIFDSTADDR, &lifrbuf) >= 0)
	{
		copy_to_skad ((struct sockaddr*)&lifrbuf.lifr_dstaddr, &cfg->ptop);
	}

	#if defined(SIOCGENADDR)
	{
		if (ioctl(s, SIOCGENADDR, ifr) >= 0 && 
		    HAWK_SIZEOF(ifr->ifr_enaddr) >= HAWK_SIZEOF(cfg->ethw))
		{
			HAWK_MEMCPY (cfg->ethw, ifr->ifr_enaddr, HAWK_SIZEOF(cfg->ethw));
		}
		/* TODO: try DLPI if SIOCGENADDR fails... */
	}
	#endif

	return 0;

#elif defined(SIOCGLIFADDR) && defined(HAVE_STRUCT_IF_LADDRREQ) && !defined(SIOCGLIFINDEX)
	/* freebsd */
	hawk_oow_t ml, wl;
	
	#if defined(SIOCGIFINDEX)
	if (ioctl(s, SIOCGIFINDEX, ifr) <= -1) 
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
	cfg->index = ifr->ifr_ifindex;
	#else
	cfg->index = ifr->ifr_index;
	#endif
	#else
	cfg->index = 0;
	#endif

	if (ioctl(s, SIOCGIFFLAGS, ifr) <= -1) 
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}
	cfg->flags = 0;
	if (ifr->ifr_flags & IFF_UP) cfg->flags |= HAWK_IFCFG_UP;
	if (ifr->ifr_flags & IFF_RUNNING) cfg->flags |= HAWK_IFCFG_RUNNING;
	if (ifr->ifr_flags & IFF_BROADCAST) cfg->flags |= HAWK_IFCFG_BCAST;
	if (ifr->ifr_flags & IFF_POINTOPOINT) cfg->flags |= HAWK_IFCFG_PTOP;

	if (ioctl(s, SIOCGIFMTU, ifr) <= -1) 
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}
	#if defined(HAVE_STRUCT_IFREQ_IFR_MTU)
	cfg->mtu = ifr->ifr_mtu;
	#else
	/* well, this is a bit dirty. but since all these are unions, 
	 * the name must not really matter. some OSes just omitts defining
	 * the MTU field */
	cfg->mtu = ifr->ifr_metric; 
	#endif
	
	hawk_clear_skad (&cfg->addr);
	hawk_clear_skad (&cfg->mask);
	hawk_clear_skad (&cfg->bcast);
	hawk_clear_skad (&cfg->ptop);
	HAWK_MEMSET (cfg->ethw, 0, HAWK_SIZEOF(cfg->ethw));

	if (cfg->type == HAWK_IFCFG_IN6)
	{
		struct if_laddrreq iflrbuf;
		HAWK_MEMSET (&iflrbuf, 0, HAWK_SIZEOF(iflrbuf));
		hawk_copy_bcstr (iflrbuf.iflr_name, HAWK_SIZEOF(iflrbuf.iflr_name), ifr->ifr_name);

		if (ioctl(s, SIOCGLIFADDR, &iflrbuf) >= 0) 
		{
			hawk_skad_alt_t* skad;
			copy_to_skad ((struct sockaddr*)&iflrbuf.addr, &cfg->addr);

			skad = (hawk_skad_alt_t*)&cfg->mask;
			skad->in6.sin6_family = HAWK_AF_INET6;
			prefix_to_in6 (iflrbuf.prefixlen, &skad->in6.sin6_addr);

			if (cfg->flags & HAWK_IFCFG_PTOP)
				copy_to_skad ((struct sockaddr*)&iflrbuf.dstaddr, &cfg->ptop);
		}
	}
	else
	{
		if (ioctl(s, SIOCGIFADDR, ifr) >= 0)
			copy_to_skad ((struct sockaddr*)&ifr->ifr_addr, &cfg->addr);

		if (ioctl(s, SIOCGIFNETMASK, ifr) >= 0)
			copy_to_skad ((struct sockaddr*)&ifr->ifr_addr, &cfg->mask);

		if ((cfg->flags & HAWK_IFCFG_BCAST) && ioctl(s, SIOCGIFBRDADDR, ifr) >= 0) 
		{
			copy_to_skad ((struct sockaddr*)&ifr->ifr_broadaddr, &cfg->bcast);
		}

		if ((cfg->flags & HAWK_IFCFG_PTOP) && ioctl(s, SIOCGIFDSTADDR, ifr) >= 0) 
		{
			copy_to_skad ((struct sockaddr*)&ifr->ifr_dstaddr, &cfg->ptop);
		}
	}

	#if defined(CTL_NET) && defined(AF_ROUTE) && defined(AF_LINK)
	{
		int mib[6];
		size_t len;

		mib[0] = CTL_NET;
		mib[1] = AF_ROUTE;
		mib[2] = 0;
		mib[3] = AF_LINK;
		mib[4] = NET_RT_IFLIST;
		mib[5] = cfg->index;
		if (sysctl(mib, HAWK_COUNTOF(mib), HAWK_NULL, &len, HAWK_NULL, 0) >= 0)
		{
			hawk_mmgr_t* mmgr = HAWK_MMGR_GETDFL();
			void* buf;

			buf = hawk_gem_allocmem(gem, len);
			if (buf)
			{
				if (sysctl(mib, HAWK_COUNTOF(mib), buf, &len, HAWK_NULL, 0) >= 0)
				{
					struct sockaddr_dl* sadl;
					sadl = ((struct if_msghdr*)buf + 1);

					/* i don't really care if it's really ethernet
					 * so long as the data is long enough */
					if (sadl->sdl_alen >= HAWK_COUNTOF(cfg->ethw))
						HAWK_MEMCPY (cfg->ethw, LLADDR(sadl), HAWK_SIZEOF(cfg->ethw));
				}

				hawk_gem_freemem (gem, buf);
			}
		}
	}
	#endif

	return 0;

#elif defined(SIOCGIFADDR)

	#if defined(SIOCGIFINDEX)
	if (ioctl(s, SIOCGIFINDEX, ifr) <= -1)
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}

	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
	cfg->index = ifr->ifr_ifindex;
	#else
	cfg->index = ifr->ifr_index;
	#endif

	#else
	cfg->index = 0;
	#endif

	if (ioctl (s, SIOCGIFFLAGS, ifr) <= -1)
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}
	cfg->flags = 0;
	if (ifr->ifr_flags & IFF_UP) cfg->flags |= HAWK_IFCFG_UP;
	if (ifr->ifr_flags & IFF_RUNNING) cfg->flags |= HAWK_IFCFG_RUNNING;
	if (ifr->ifr_flags & IFF_BROADCAST) cfg->flags |= HAWK_IFCFG_BCAST;
	if (ifr->ifr_flags & IFF_POINTOPOINT) cfg->flags |= HAWK_IFCFG_PTOP;

	if (ioctl(s, SIOCGIFMTU, ifr) <= -1)
	{
		hawk_gem_seterrnum (gem, HAWK_NULL, hawk_syserr_to_errnum(errno));
		return -1;
	}
	#if defined(HAVE_STRUCT_IFREQ_IFR_MTU)
	cfg->mtu = ifr->ifr_mtu;
	#else
	/* well, this is a bit dirty. but since all these are unions, 
	 * the name must not really matter. SCO just omits defining
	 * the MTU field, and uses ifr_metric instead */
	cfg->mtu = ifr->ifr_metric; 
	#endif

	hawk_clear_skad (&cfg->addr);
	hawk_clear_skad (&cfg->mask);
	hawk_clear_skad (&cfg->bcast);
	hawk_clear_skad (&cfg->ptop);
	HAWK_MEMSET (cfg->ethw, 0, HAWK_SIZEOF(cfg->ethw));

	if (ioctl(s, SIOCGIFADDR, ifr) >= 0) copy_to_skad ((struct sockaddr*)&ifr->ifr_addr, &cfg->addr);
	if (ioctl(s, SIOCGIFNETMASK, ifr) >= 0) copy_to_skad ((struct sockaddr*)&ifr->ifr_addr, &cfg->mask);

	#if defined(__linux)
	if (hawk_skad_family(&cfg->addr) == HAWK_AF_UNSPEC && hawk_skad_family(&cfg->mask) == HAWK_AF_UNSPEC && cfg->type == HAWK_IFCFG_IN6)
	{
		/* access /proc/net/if_inet6 */
		read_proc_net_if_inet6 (gem, cfg, ifr);
	}
	#endif

	if ((cfg->flags & HAWK_IFCFG_BCAST) && ioctl(s, SIOCGIFBRDADDR, ifr) >= 0)
	{
		copy_to_skad ((struct sockaddr*)&ifr->ifr_broadaddr, &cfg->bcast);
	}

	if ((cfg->flags & HAWK_IFCFG_PTOP) && ioctl(s, SIOCGIFDSTADDR, ifr) >= 0)
	{
		copy_to_skad ((struct sockaddr*)&ifr->ifr_dstaddr, &cfg->ptop);
	}

	#if defined(SIOCGIFHWADDR)
	if (ioctl(s, SIOCGIFHWADDR, ifr) >= 0)
	if (ioctl(s, SIOCGIFHWADDR, ifr) >= 0)
	{ 
		HAWK_MEMCPY (cfg->ethw, ifr->ifr_hwaddr.sa_data, HAWK_SIZEOF(cfg->ethw));
	}
	#elif defined(MACIOC_GETADDR)
	{
		/* sco openserver
		 * use the streams interface to get the hardware address. 
		 */
		int strfd;
		hawk_bch_t devname[HAWK_COUNTOF(ifr->ifr_name) + 5 + 1];

		hawk_copy_bcstr_unlimited (devname, HAWK_MT("/dev/"));
		hawk_copy_bcstr_unlimited (&devname[5], ifr->ifr_name);
		if ((strfd = HAWK_OPEN(devname, O_RDONLY, 0)) >= 0)
		{
			hawk_uint8_t buf[HAWK_SIZEOF(cfg->ethw)];
			struct strioctl strioc;

			strioc.ic_cmd = MACIOC_GETADDR;
			strioc.ic_timout = -1;
			strioc.ic_len = HAWK_SIZEOF (buf);
			strioc.ic_dp = buf;
			if (ioctl(strfd, I_STR, (char *) &strioc) >= 0) 
				HAWK_MEMCPY (cfg->ethw, buf, HAWK_SIZEOF(cfg->ethw));

			HAWK_CLOSE (strfd);
		}
	}
	#endif

	return 0;
#else

	/* TODO  */
	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
#endif
}

static void get_moreinfo (int s, hawk_ifcfg_t* cfg, struct ifreq* ifr)
{
#if defined(ETHTOOL_GLINK)
	{
		/* get link status */
		struct ethtool_value ev;
		HAWK_MEMSET (&ev, 0, HAWK_SIZEOF(ev));
		ev.cmd= ETHTOOL_GLINK;
		ifr->ifr_data = &ev;
		if (ioctl(s, SIOCETHTOOL, ifr) >= 0) cfg->flags |= ev.data? HAWK_IFCFG_LINKUP: HAWK_IFCFG_LINKDOWN;
	}
#endif

#if 0

#if defined(ETHTOOL_GSTATS)
	{
		/* get link statistics */
		struct ethtool_drvinfo drvinfo;

		drvinfo.cmd = ETHTOOL_GDRVINFO;
		ifr->ifr_data = &drvinfo;
		if (ioctl (s, SIOCETHTOOL, ifr) >= 0)
		{
			struct ethtool_stats *stats;
			hawk_uint8_t buf[1000]; /* TODO: make this dynamic according to drvinfo.n_stats */

			stats = buf;
			stats->cmd = ETHTOOL_GSTATS;
			stats->n_stats = drvinfo.n_stats * HAWK_SIZEOF(stats->data[0]);
			ifr->ifr_data = (caddr_t) stats;
			if (ioctl (s, SIOCETHTOOL, ifr) >= 0)
			{
for (i = 0; i  < drvinfo.n_stats; i++)
{
	hawk_printf (HAWK_T(">>> %llu \n"), stats->data[i]);
}
			}
		}
	}
#endif
#endif
}

/* TOOD: consider how to handle multiple IPv6 addresses on a single interfce.
 *       consider how to get IPv4 addresses on an aliased interface? so mutliple ipv4 addresses */

int hawk_gem_getifcfg (hawk_gem_t* gem, hawk_ifcfg_t* cfg)
{
#if defined(_WIN32)
	/* TODO */
	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
#elif defined(__OS2__)
	/* TODO */
	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
#elif defined(__DOS__)
	/* TODO */
	hawk_gem_seterrnum (gem, HAWK_NULL, HAWK_ENOIMPL);
	return -1;
#else
	int x = -1, s = -1;
	struct ifreq ifr;
	hawk_oow_t ml, wl;

	if (cfg->type == HAWK_IFCFG_IN4)
	{
	#if defined(AF_INET)
		s = socket (AF_INET, SOCK_DGRAM, 0);
	#endif
	}
	else if (cfg->type == HAWK_IFCFG_IN6)
	{
	#if defined(AF_INET6)
		s = socket(AF_INET6, SOCK_DGRAM, 0);
	#endif
	}
	if (s <= -1) return -1;
	
	if (cfg->name[0] == HAWK_T('\0')&& cfg->index >= 1)
	{
/* TODO: support lookup by ifindex */
	}

	HAWK_MEMSET (&ifr, 0, sizeof(ifr));
	#if defined(HAWK_OOCH_IS_BCH)
	hawk_copy_bcstr (ifr.ifr_name, HAWK_SIZEOF(ifr.ifr_name), cfg->name);
	#else
	ml = HAWK_COUNTOF(ifr.ifr_name);
	if (hawk_gem_convutobcstr(gem, cfg->name, &wl, ifr.ifr_name, &ml) <= -1)  goto oops;
	#endif

	x = get_ifcfg(gem, s, cfg, &ifr);

	if (x >= 0) get_moreinfo (s, cfg, &ifr);

oops:
	HAWK_CLOSE (s);
	return x;
#endif
}

#endif
