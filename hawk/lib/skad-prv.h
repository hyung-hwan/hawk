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

#ifndef _HAWK_SKAD_PRV_H_
#define _HAWK_SKAD_PRV_H_

#include <sys/types.h>
#include <sys/socket.h>
#if defined(HAVE_NETINET_IN_H)
#	include <netinet/in.h>
#endif
#if defined(HAVE_SYS_UN_H)
#	include <sys/un.h>
#endif
#if defined(HAVE_NETPACKET_PACKET_H)
#	include <netpacket/packet.h>
#endif
#if defined(HAVE_NET_IF_DL_H)
#	include <net/if_dl.h>
#endif

union hawk_skad_alt_t
{
	struct sockaddr    sa;
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
	struct sockaddr_in in4;
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	struct sockaddr_in6 in6;
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_LL > 0)
	struct sockaddr_ll ll;
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
	struct sockaddr_un un;
#endif
};
typedef union hawk_skad_alt_t hawk_skad_alt_t;


#endif
