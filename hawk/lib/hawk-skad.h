/*
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

#ifndef _HAWK_SKAD_H_
#define _HAWK_SKAD_H_

#include <hawk-cmn.h>

#define HAWK_SIZEOF_SKAD_T 1
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > HAWK_SIZEOF_SKAD_T)
#	undef HAWK_SIZEOF_SKAD_T
#	define HAWK_SIZEOF_SKAD_T HAWK_SIZEOF_STRUCT_SOCKADDR_IN
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > HAWK_SIZEOF_SKAD_T)
#	undef HAWK_SIZEOF_SKAD_T
#	define HAWK_SIZEOF_SKAD_T HAWK_SIZEOF_STRUCT_SOCKADDR_IN6
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_LL > HAWK_SIZEOF_SKAD_T)
#	undef HAWK_SIZEOF_SKAD_T
#	define HAWK_SIZEOF_SKAD_T HAWK_SIZEOF_STRUCT_SOCKADDR_LL
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_DL > HAWK_SIZEOF_SKAD_T)
#	undef HAWK_SIZEOF_SKAD_T
#	define HAWK_SIZEOF_SKAD_T HAWK_SIZEOF_STRUCT_SOCKADDR_DL
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > HAWK_SIZEOF_SKAD_T)
#	undef HAWK_SIZEOF_SKAD_T
#	define HAWK_SIZEOF_SKAD_T HAWK_SIZEOF_STRUCT_SOCKADDR_UN
#endif

#if (HAWK_SIZEOF_SA_FAMILY_T == 1) && !defined(HAWK_SA_FAMILY_T_IS_SIGNED)
#	if !defined(HAWK_AF_UNIX)
#		define HAWK_AF_UNIX (254)
#	endif
#elif (HAWK_SIZEOF_SA_FAMILY_T == 1) && defined(HAWK_SA_FAMILY_T_IS_SIGNED)
#	if !defined(HAWK_AF_UNIX)
#		define HAWK_AF_UNIX (-2)
#	endif
#else
#	if !defined(HAWK_AF_UNIX)
		/* this is a fake value */
#		define HAWK_AF_UNIX (65534)
#	endif
#endif

struct hawk_skad_t
{
	hawk_uint8_t data[HAWK_SIZEOF_SKAD_T];
};
typedef struct hawk_skad_t hawk_skad_t;

#define HAWK_IP4AD_STRLEN (15) /* not including the terminating '\0' */
#define HAWK_IP6AD_STRLEN (45) /* not including the terminating '\0'. pure IPv6 address, not including the scope(e.g. %10, %eth0) */

/* size large enough to hold the ip address plus port number. 
 * [IPV6ADDR%SCOPE]:PORT -> 9 for [] % : and PORT 
 * Let's reserve 16 for SCOPE and not include the terminting '\0'
 */
#define HAWK_SKAD_IP_STRLEN (HAWK_IP6AD_STRLEN + 25)

/* -------------------------------------------------------------------- */

#define HAWK_ETHAD_LEN (6)
#define HAWK_IP4AD_LEN (4)
#define HAWK_IP6AD_LEN (16)

#include <hawk-pack1.h>
struct HAWK_PACKED hawk_ethad_t
{
	hawk_uint8_t v[HAWK_ETHAD_LEN]; 
};
typedef struct hawk_ethad_t hawk_ethad_t;

struct HAWK_PACKED hawk_ip4ad_t
{
	hawk_uint8_t v[HAWK_IP4AD_LEN];
};
typedef struct hawk_ip4ad_t hawk_ip4ad_t;

struct HAWK_PACKED hawk_ip6ad_t
{
	hawk_uint8_t v[HAWK_IP6AD_LEN]; 
};
typedef struct hawk_ip6ad_t hawk_ip6ad_t;
#include <hawk-unpack.h>

#if defined(__cplusplus)
extern "C" {
#endif

HAWK_EXPORT void hawk_skad_init_for_ip4 (
	hawk_skad_t*        skad,
	hawk_uint16_t       port,
	hawk_ip4ad_t*       ip4ad
);

HAWK_EXPORT void hawk_skad_init_for_ip6 (
	hawk_skad_t*        skad,
	hawk_uint16_t       port,
	hawk_ip6ad_t*       ip6ad,
	int                scope_id
);

HAWK_EXPORT void hawk_skad_init_for_ip_with_bytes (
	hawk_skad_t*        skad,
	hawk_uint16_t       port,
	const hawk_uint8_t* bytes,
	hawk_oow_t          len 
);

HAWK_EXPORT void hawk_skad_init_for_eth (
	hawk_skad_t*        skad,
	int                ifindex,
	hawk_ethad_t*       ethad
);

HAWK_EXPORT int hawk_skad_get_family (
	const hawk_skad_t* skad
);

HAWK_EXPORT int hawk_skad_get_size (
	const hawk_skad_t* skad
);

HAWK_EXPORT int hawk_skad_get_port (
	const hawk_skad_t* skad
);

/* for link-level addresses */
HAWK_EXPORT int hawk_skad_get_ifindex (
	const hawk_skad_t* skad
);

/* for ipv6 */
HAWK_EXPORT int hawk_skad_get_scope_id (
	const hawk_skad_t* skad
);

HAWK_EXPORT void hawk_skad_set_scope_id (
	hawk_skad_t*       skad,
	int               scope_id
);

HAWK_EXPORT hawk_oow_t hawk_skad_get_ipad_bytes (
	hawk_skad_t*       skad,
	void*              buf,
	hawk_oow_t         len
);

HAWK_EXPORT void hawk_clear_skad (
	hawk_skad_t*       skad
);

HAWK_EXPORT int hawk_equal_skads (
	const hawk_skad_t* addr1,
	const hawk_skad_t* addr2,
	int                strict
);

HAWK_EXPORT hawk_oow_t hawk_ipad_bytes_to_ucstr (
	const hawk_uint8_t* iptr,
	hawk_oow_t          ilen,
	hawk_uch_t*         buf,
	hawk_oow_t          blen
);

HAWK_EXPORT hawk_oow_t hawk_ipad_bytes_to_bcstr (
	const hawk_uint8_t* iptr,
	hawk_oow_t          ilen,
	hawk_bch_t*         buf,
	hawk_oow_t          blen
);


HAWK_EXPORT int hawk_uchars_to_ipad_bytes (
	const hawk_uch_t*   str,
	hawk_oow_t          slen,
	hawk_uint8_t*       buf,
	hawk_oow_t          blen
);

HAWK_EXPORT int hawk_bchars_to_ipad_bytes (
	const hawk_bch_t*   str,
	hawk_oow_t          slen,
	hawk_uint8_t*       buf,
	hawk_oow_t          blen
);

#define hawk_ucstr_to_ipad_bytes(str,buf,blen) hawk_uchars_to_ipad_bytes(str, hawk_count_ucstr(str,buf,len)
#define hawk_bcstr_to_ipad_bytes(str,buf,blen) hawk_bchars_to_ipad_bytes(str, hawk_count_bcstr(str,buf,len)

#if defined(HAWK_OOCH_IS_UCH)
#	define hawk_ipad_bytes_to_oocstr hawk_ipad_bytes_to_ucstr
#	define hawk_oochars_to_ipad_bytes hawk_uchars_to_ipad_bytes
#	define hawk_oocstr_to_ipad_bytes hawk_ucstr_to_ipad_bytes
#else
#	define hawk_ipad_bytes_to_oocstr hawk_ipad_bytes_to_bcstr
#	define hawk_oochars_to_ipad_bytes hawk_bchars_to_ipad_bytes
#	define hawk_oocstr_to_ipad_bytes hawk_bcstr_to_ipad_bytes
#endif

HAWK_EXPORT int hawk_ipad_bytes_is_v4_mapped (
	const hawk_uint8_t* iptr,
	hawk_oow_t          ilen
);

HAWK_EXPORT int hawk_ipad_bytes_is_loop_back (
	const hawk_uint8_t* iptr,
	hawk_oow_t          ilen
);

HAWK_EXPORT int hawk_ipad_bytes_is_link_local (
	const hawk_uint8_t* iptr,
	hawk_oow_t          ilen
);

#if defined(__cplusplus)
}
#endif

#endif
