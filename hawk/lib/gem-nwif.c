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

#include "hawk-prv.h"

#if defined(_WIN32)
	/* TODO: */
#elif defined(__OS2__)
	/* TODO: */
#elif defined(__DOS__)
	/* TODO: */
#else
#	include "syscall.h"
#	include <sys/socket.h>
#	if defined(HAVE_SYS_IOCTL_H)
#		include <sys/ioctl.h>
#	endif
#	if defined(HAVE_NET_IF_H)
#		include <net/if.h>
#	endif
#	if defined(HAVE_SYS_SOCKIO_H)
#		include <sys/sockio.h>
#	endif
#	if !defined(IF_NAMESIZE)
#		define IF_NAMESIZE 63
#	endif
#endif

#if defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))
static int get_sco_ifconf (hawk_gem_t* gem, struct ifconf* ifc)
{
	/* SCO doesn't have have any IFINDEX thing.
	 * i emultate it using IFCONF */
	int h, num;
	struct ifreq* ifr;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	ifc->ifc_len = 0;
	ifc->ifc_buf = HAWK_NULL;

	#if defined(SIOCGIFANUM)
	if (ioctl(h, SIOCGIFANUM, &num) <= -1) goto oops;
	#else
	if (ioctl(h, SIOCGIFNUM, &num) <= -1) goto oops;
	#endif

	/* sco needs reboot when you add an network interface.
	 * it should be safe not to consider the case when the interface
	 * is added after SIOCGIFANUM above. 
	 * another thing to note is that SIOCGIFCONF ends with segfault
	 * if the buffer is not large enough unlike some other OSes
	 * like opensolaris which truncates the configuration. */

	ifc->ifc_len = num * HAWK_SIZEOF(*ifr);
	ifc->ifc_buf = hawk_gem_allocmem(gem, ifc->ifc_len);
	if (ifc->ifc_buf == HAWK_NULL) goto oops;

	if (ioctl(h, SIOCGIFCONF, ifc) <= -1) goto oops;
	HAWK_CLOSE (h); h = -1;

	return 0;

oops:
	if (ifc->ifc_buf) hawk_gem_freemem (gem, ifc->ifc_buf);
	if (h >= 0) HAWK_CLOSE (h);
	return -1;
}

static HAWK_INLINE void free_sco_ifconf (hawk_gem_t* gem, struct ifconf* ifc)
{
	hawk_gem_freemem (gem, ifc->ifc_buf);
}
#endif


int hawk_gem_bcstrtoifindex (hawk_gem_t* gem, const hawk_bch_t* ptr, unsigned int* index)
{
#if defined(_WIN32)
	/* TODO: */
	return -1;
#elif defined(__OS2__)
	/* TODO: */
	return -1;
#elif defined(__DOS__)
	/* TODO: */
	return -1;

#elif defined(SIOCGIFINDEX)
	int h, x;
	hawk_oow_t len;
	struct ifreq ifr;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	HAWK_MEMSET (&ifr, 0, HAWK_SIZEOF(ifr));
	len = hawk_copy_bcstr(ifr.ifr_name, HAWK_COUNTOF(ifr.ifr_name), ptr);
	if (ptr[len] != '\0') return -1; /* name too long */

	x = ioctl(h, SIOCGIFINDEX, &ifr);
	HAWK_CLOSE (h);

	if (x >= 0)
	{
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
		*index = ifr.ifr_ifindex;
	#else
		*index = ifr.ifr_index;
	#endif
	}

	return x;

#elif defined(HAVE_IF_NAMETOINDEX)
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	hawk_oow_t len;
	unsigned int tmpidx;

	len = hawk_copy_bcstr(tmp, HAWK_COUNTOF(tmp), ptr);
	if (ptr[len] != '\0') return -1; /* name too long */

	tmpidx = if_nametoindex(tmp);
	if (tmpidx == 0) return -1;
	*index = tmpidx;
	return 0;

#elif defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))

	struct ifconf ifc;
	int num, i;

	if (get_sco_ifconf(gem, &ifc) <= -1) return -1;

	num = ifc.ifc_len / HAWK_SIZEOF(struct ifreq);
	for (i = 0; i < num; i++)
	{
		if (hawk_comp_bcstr(ptr, ifc.ifc_req[i].ifr_name) == 0) 
		{
			free_sco_ifconf (gem, &ifc);
			*index = i + 1;
			return 0;
		}
	}

	free_sco_ifconf (gem, &ifc);
	return -1;

#else
	return -1;
#endif
}

int hawk_gem_bcharstoifindex (hawk_gem_t* gem, const hawk_bch_t* ptr, hawk_oow_t len, unsigned int* index)
{
#if defined(_WIN32)
	/* TODO: */
	return -1;
#elif defined(__OS2__)
	/* TODO: */
	return -1;
#elif defined(__DOS__)
	/* TODO: */
	return -1;

#elif defined(SIOCGIFINDEX)
	int h, x;
	struct ifreq ifr;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	HAWK_MEMSET (&ifr, 0, HAWK_SIZEOF(ifr));
	if (hawk_copy_bchars_to_bcstr(ifr.ifr_name, HAWK_COUNTOF(ifr.ifr_name), ptr, len) < len) return -1; /* name too long */

	x = ioctl(h, SIOCGIFINDEX, &ifr);
	HAWK_CLOSE (h);

	if (x >= 0)
	{
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
		*index = ifr.ifr_ifindex;
	#else
		*index = ifr.ifr_index;
	#endif
	}

	return x;

#elif defined(HAVE_IF_NAMETOINDEX)
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	unsigned int tmpidx;

	if (hawk_copy_bchars_to_bcstr(tmp, HAWK_COUNTOF(tmp), ptr, len) < len) return -1;

	tmpidx = if_nametoindex (tmp);
	if (tmpidx == 0) return -1;
	*index = tmpidx;
	return 0;

#elif defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))
	struct ifconf ifc;
	int num, i;

	if (get_sco_ifconf(gem, &ifc) <= -1) return -1;

	num = ifc.ifc_len / HAWK_SIZEOF(struct ifreq);
	for (i = 0; i < num; i++)
	{
		if (hawk_comp_bchars_bcstr(ptr, len, ifc.ifc_req[i].ifr_name) == 0) 
		{
			free_sco_ifconf (gem, &ifc);
			*index = i + 1;
			return 0;
		}
	}

	free_sco_ifconf (gem, &ifc);
	return -1;

#else
	return -1;
#endif
}

int hawk_gem_ucstrtoifindex (hawk_gem_t* gem, const hawk_uch_t* ptr, unsigned int* index)
{
#if defined(_WIN32)
	/* TODO: */
	return -1;
#elif defined(__OS2__)
	/* TODO: */
	return -1;
#elif defined(__DOS__)
	/* TODO: */
	return -1;

#elif defined(SIOCGIFINDEX)
	int h, x;
	struct ifreq ifr;
	hawk_oow_t wl, ml;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	ml = HAWK_COUNTOF(ifr.ifr_name);
	if (hawk_gem_convutobcstr(gem, ptr, &wl, ifr.ifr_name, &ml) <= -1) return -1;

	x = ioctl(h, SIOCGIFINDEX, &ifr);
	HAWK_CLOSE (h);

	if (x >= 0)
	{
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
		*index = ifr.ifr_ifindex;
	#else
		*index = ifr.ifr_index;
	#endif
	}

	return x;

#elif defined(HAVE_IF_NAMETOINDEX)
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	hawk_oow_t wl, ml;
	unsigned int tmpidx;

	ml = HAWK_COUNTOF(tmp);
	if (hawk_gem_convutobcstr(gem, ptr, &wl, tmp, &ml) <= -1) return -1;

	tmpidx = if_nametoindex(tmp);
	if (tmpidx == 0) return -1;
	*index = tmpidx;
	return 0;

#elif defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))

	struct ifconf ifc;
	int num, i;
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	hawk_oow_t wl, ml;

	ml = HAWK_COUNTOF(tmp);
	if (hawk_gem_convutobcstr(gem, ptr, &wl, tmp, &ml) <= -1) return -1;

	if (get_sco_ifconf(gem, &ifc) <= -1) return -1;

	num = ifc.ifc_len / HAWK_SIZEOF(struct ifreq);
	for (i = 0; i < num; i++)
	{
		if (hawk_comp_bcstr(tmp, ifc.ifc_req[i].ifr_name) == 0) 
		{
			free_sco_ifconf (gem, &ifc);
			*index = i + 1;
			return 0;
		}
	}

	free_sco_ifconf (gem, &ifc);
	return -1;

#else
	return -1;
#endif
}

int hawk_gem_ucharstoifindex (hawk_gem_t* gem, const hawk_uch_t* ptr, hawk_oow_t len, unsigned int* index)
{
#if defined(_WIN32)
	/* TODO: */
	return -1;
#elif defined(__OS2__)
	/* TODO: */
	return -1;
#elif defined(__DOS__)
	/* TODO: */
	return -1;

#elif defined(SIOCGIFINDEX)
	int h, x;
	struct ifreq ifr;
	hawk_oow_t wl, ml;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	wl = len; ml = HAWK_COUNTOF(ifr.ifr_name) - 1;
	if (hawk_gem_convutobchars(gem, ptr, &wl, ifr.ifr_name, &ml) <= -1) return -1;
	ifr.ifr_name[ml] = '\0';

	x = ioctl(h, SIOCGIFINDEX, &ifr);
	HAWK_CLOSE (h);

	if (x >= 0)
	{
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
		*index = ifr.ifr_ifindex;
	#else
		*index = ifr.ifr_index;
	#endif
	}

	return x;

#elif defined(HAVE_IF_NAMETOINDEX)
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	hawk_oow_t wl, ml;
	unsigned int tmpidx;

	wl = len; ml = HAWK_COUNTOF(tmp) - 1;
	if (hawk_gem_convutobchars(gem, ptr, &wl, tmp, &ml) <= -1) return -1;
	tmp[ml] = '\0';

	tmpidx = if_nametoindex(tmp);
	if (tmpidx == 0) return -1;
	*index = tmpidx;
	return 0;

#elif defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))
	struct ifconf ifc;
	int num, i;
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	hawk_oow_t wl, ml;

	wl = len; ml = HAWK_COUNTOF(tmp) - 1;
	if (hawk_gem_convutobchars(ptr, &wl, tmp, &ml) <= -1) return -1;
	tmp[ml] = '\0';

	if (get_sco_ifconf(gem, &ifc) <= -1) return -1;

	num = ifc.ifc_len / HAWK_SIZEOF(struct ifreq);
	for (i = 0; i < num; i++)
	{
		if (hawk_comp_bcstr(tmp, ifc.ifc_req[i].ifr_name) == 0) 
		{
			free_sco_ifconf (gem, &ifc);
			*index = i + 1;
			return 0;
		}
	}

	free_sco_ifconf (gem, &ifc);
	return -1;
#else
	return -1;
#endif
}

/* ---------------------------------------------------------- */

int hawk_gem_ifindextobcstr (unsigned int index, hawk_bch_t* buf, hawk_oow_t len)
{
#if defined(_WIN32)
	/* TODO: */
	return -1;
#elif defined(__OS2__)
	/* TODO: */
	return -1;
#elif defined(__DOS__)
	/* TODO: */
	return -1;

#elif defined(SIOCGIFNAME)

	int h, x;
	struct ifreq ifr;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	HAWK_MEMSET (&ifr, 0, HAWK_SIZEOF(ifr));
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
	ifr.ifr_ifindex = index;
	#else
	ifr.ifr_index = index;
	#endif
	
	x = ioctl(h, SIOCGIFNAME, &ifr);
	HAWK_CLOSE (h);

	return (x <= -1)? -1: hawk_copy_bcstr(buf, len, ifr.ifr_name);

#elif defined(HAVE_IF_INDEXTONAME)
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	if (if_indextoname (index, tmp) == HAWK_NULL) return -1;
	return hawk_copy_bcstr(buf, len, tmp);

#elif defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))

	struct ifconf ifc;
	hawk_oow_t ml;
	int num;

	if (index <= 0) return -1;
	if (get_sco_ifconf(gem, &ifc) <= -1) return -1;

	num = ifc.ifc_len / HAWK_SIZEOF(struct ifreq);
	if (index > num) 
	{
		free_sco_ifconf (gem, &ifc);
		return -1;
	}

	ml = hawk_copy_bcstr(buf, len, ifc.ifc_req[index - 1].ifr_name);
	free_sco_ifconf (gem, &ifc);
	return ml;

#else
	return -1;
#endif
}

int hawk_gem_ifindextoucstr (hawk_gem_t* gem, unsigned int index, hawk_uch_t* buf, hawk_oow_t len)
{
#if defined(_WIN32)
	/* TODO: */
	return -1;
#elif defined(__OS2__)
	/* TODO: */
	return -1;
#elif defined(__DOS__)
	/* TODO: */
	return -1;

#elif defined(SIOCGIFNAME)

	int h, x;
	struct ifreq ifr;
	hawk_oow_t wl, ml;

	h = socket(AF_INET, SOCK_DGRAM, 0); 
	if (h <= -1) return -1;

	HAWK_MEMSET (&ifr, 0, HAWK_SIZEOF(ifr));
	#if defined(HAVE_STRUCT_IFREQ_IFR_IFINDEX)
	ifr.ifr_ifindex = index;
	#else
	ifr.ifr_index = index;
	#endif
	
	x = ioctl(h, SIOCGIFNAME, &ifr);
	HAWK_CLOSE (h);

	if (x <= -1) return -1;

	wl = len;
	x = hawk_gem_convbtoucstr(gem, ifr.ifr_name, &ml, buf, &wl, 0);
	if (x == -2 && wl > 1) buf[wl - 1] = '\0';
	else if (x != 0) return -1;
	return wl;

#elif defined(HAVE_IF_INDEXTONAME)
	hawk_bch_t tmp[IF_NAMESIZE + 1];
	hawk_oow_t ml, wl;
	int x;

	if (if_indextoname(index, tmp) == HAWK_NULL) return -1;
	wl = len;
	x = hawk_gem_convbtoucstr(gem, tmp, &ml, buf, &wl, 0);
	if (x == -2 && wl > 1) buf[wl - 1] = '\0';
	else if (x != 0) return -1;
	return wl;

#elif defined(SIOCGIFCONF) && (defined(SIOCGIFANUM) || defined(SIOCGIFNUM))

	struct ifconf ifc;
	hawk_oow_t wl, ml;
	int num, x;

	if (index <= 0) return -1;
	if (get_sco_ifconf(gem, &ifc) <= -1) return -1;

	num = ifc.ifc_len / HAWK_SIZEOF(struct ifreq);
	if (index > num) 
	{
		free_sco_ifconf (gem, &ifc);
		return -1;
	}

	wl = len;
	x = hawk_gem_convbtoucstr(ifc.ifc_req[index - 1].ifr_name, &ml, buf, &wl, 0);
	free_sco_ifconf (gem, &ifc);

	if (x == -2 && wl > 1) buf[wl - 1] = '\0';
	else if (x != 0) return -1;

	return wl;
#else
	return -1;
#endif
}
