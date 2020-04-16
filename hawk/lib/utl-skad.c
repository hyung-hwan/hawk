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

#include <hawk-utl.h>
#include "hawk-prv.h"
#include "utl-skad.h"

int hawk_skad_family (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	return skad->sa.sa_family;
}

int hawk_skad_size (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;

	switch (skad->sa.sa_family)
	{
	#if defined(AF_INET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
		case AF_INET: return HAWK_SIZEOF(struct sockaddr_in);
	#endif
	#if defined(AF_INET6) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case AF_INET6: return HAWK_SIZEOF(struct sockaddr_in6);
	#endif
	#if defined(AF_PACKET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_LL > 0)
		case AF_PACKET: return HAWK_SIZEOF(struct sockaddr_ll);
	#endif
	#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
		case AF_UNIX: return HAWK_SIZEOF(struct sockaddr_un);
	#endif
	}

	return 0;
}

void hawk_clear_skad (hawk_skad_t* _skad)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;
	HAWK_MEMSET (skad, 0, HAWK_SIZEOF(*skad));
	skad->sa.sa_family = HAWK_AF_UNSPEC;
}
