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

#include <hawk-skad.h>
#include <hawk-gem.h>
#include "hawk-prv.h"
#include "skad-prv.h"

/* ------------------------------------------------------------------------- */

static int uchars_to_ipv4 (const hawk_uch_t* str, hawk_oow_t len, struct in_addr* inaddr)
{
	const hawk_uch_t* end;
	int dots = 0, digits = 0;
	hawk_uint32_t acc = 0, addr = 0;
	hawk_uch_t c;

	end = str + len;

	do
	{
		if (str >= end)
		{
			if (dots < 3 || digits == 0) return -1;
			addr = (addr << 8) | acc;
			break;
		}

		c = *str++;

		if (c >= '0' && c <= '9')
		{
			if (digits > 0 && acc == 0) return -1;
			acc = acc * 10 + (c - '0');
			if (acc > 255) return -1;
			digits++;
		}
		else if (c == '.')
		{
			if (dots >= 3 || digits == 0) return -1;
			addr = (addr << 8) | acc;
			dots++; acc = 0; digits = 0;
		}
		else return -1;
	}
	while (1);

	inaddr->s_addr = hawk_hton32(addr);
	return 0;

}

static int bchars_to_ipv4 (const hawk_bch_t* str, hawk_oow_t len, struct in_addr* inaddr)
{
	const hawk_bch_t* end;
	int dots = 0, digits = 0;
	hawk_uint32_t acc = 0, addr = 0;
	hawk_bch_t c;

	end = str + len;

	do
	{
		if (str >= end)
		{
			if (dots < 3 || digits == 0) return -1;
			addr = (addr << 8) | acc;
			break;
		}

		c = *str++;

		if (c >= '0' && c <= '9')
		{
			if (digits > 0 && acc == 0) return -1;
			acc = acc * 10 + (c - '0');
			if (acc > 255) return -1;
			digits++;
		}
		else if (c == '.')
		{
			if (dots >= 3 || digits == 0) return -1;
			addr = (addr << 8) | acc;
			dots++; acc = 0; digits = 0;
		}
		else return -1;
	}
	while (1);

	inaddr->s_addr = hawk_hton32(addr);
	return 0;

}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
static int uchars_to_ipv6 (const hawk_uch_t* src, hawk_oow_t len, struct in6_addr* inaddr)
{
	hawk_uint8_t* tp, * endp, * colonp;
	const hawk_uch_t* curtok;
	hawk_uch_t ch;
	int saw_xdigit;
	unsigned int val;
	const hawk_uch_t* src_end;

	src_end = src + len;

	HAWK_MEMSET (inaddr, 0, HAWK_SIZEOF(*inaddr));
	tp = &inaddr->s6_addr[0];
	endp = &inaddr->s6_addr[HAWK_COUNTOF(inaddr->s6_addr)];
	colonp = HAWK_NULL;

	/* Leading :: requires some special handling. */
	if (src < src_end && *src == ':')
	{
		src++;
		if (src >= src_end || *src != ':') return -1;
	}

	curtok = src;
	saw_xdigit = 0;
	val = 0;

	while (src < src_end)
	{
		int v1;

		ch = *src++;

		v1 = HAWK_XDIGIT_TO_NUM(ch);
		if (v1 >= 0)
		{
			val <<= 4;
			val |= v1;
			if (val > 0xffff) return -1;
			saw_xdigit = 1;
			continue;
		}

		if (ch == ':')
		{
			curtok = src;
			if (!saw_xdigit)
			{
				if (colonp) return -1;
				colonp = tp;
				continue;
			}
			else if (src >= src_end)
			{
				/* a colon can't be the last character */
				return -1;
			}

			*tp++ = (hawk_uint8_t)(val >> 8) & 0xff;
			*tp++ = (hawk_uint8_t)val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}

		if (ch == '.' && ((tp + HAWK_SIZEOF(struct in_addr)) <= endp) &&
		    uchars_to_ipv4(curtok, src_end - curtok, (struct in_addr*)tp) == 0)
		{
			tp += HAWK_SIZEOF(struct in_addr*);
			saw_xdigit = 0;
			break;
		}

		return -1;
	}

	if (saw_xdigit)
	{
		if (tp + HAWK_SIZEOF(hawk_uint16_t) > endp) return -1;
		*tp++ = (hawk_uint8_t)(val >> 8) & 0xff;
		*tp++ = (hawk_uint8_t)val & 0xff;
	}
	if (colonp != HAWK_NULL)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		hawk_oow_t n = tp - colonp;
		hawk_oow_t i;

		for (i = 1; i <= n; i++)
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}

	if (tp != endp) return -1;

	return 0;
}

static int bchars_to_ipv6 (const hawk_bch_t* src, hawk_oow_t len, struct in6_addr* inaddr)
{
	hawk_uint8_t* tp, * endp, * colonp;
	const hawk_bch_t* curtok;
	hawk_bch_t ch;
	int saw_xdigit;
	unsigned int val;
	const hawk_bch_t* src_end;

	src_end = src + len;

	HAWK_MEMSET (inaddr, 0, HAWK_SIZEOF(*inaddr));
	tp = &inaddr->s6_addr[0];
	endp = &inaddr->s6_addr[HAWK_COUNTOF(inaddr->s6_addr)];
	colonp = HAWK_NULL;

	/* Leading :: requires some special handling. */
	if (src < src_end && *src == ':')
	{
		src++;
		if (src >= src_end || *src != ':') return -1;
	}

	curtok = src;
	saw_xdigit = 0;
	val = 0;

	while (src < src_end)
	{
		int v1;

		ch = *src++;

		v1 = HAWK_XDIGIT_TO_NUM(ch);
		if (v1 >= 0)
		{
			val <<= 4;
			val |= v1;
			if (val > 0xffff) return -1;
			saw_xdigit = 1;
			continue;
		}

		if (ch == ':')
		{
			curtok = src;
			if (!saw_xdigit)
			{
				if (colonp) return -1;
				colonp = tp;
				continue;
			}
			else if (src >= src_end)
			{
				/* a colon can't be the last character */
				return -1;
			}

			*tp++ = (hawk_uint8_t)(val >> 8) & 0xff;
			*tp++ = (hawk_uint8_t)val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}

		if (ch == '.' && ((tp + HAWK_SIZEOF(struct in_addr)) <= endp) &&
		    bchars_to_ipv4(curtok, src_end - curtok, (struct in_addr*)tp) == 0)
		{
			tp += HAWK_SIZEOF(struct in_addr*);
			saw_xdigit = 0;
			break;
		}

		return -1;
	}

	if (saw_xdigit)
	{
		if (tp + HAWK_SIZEOF(hawk_uint16_t) > endp) return -1;
		*tp++ = (hawk_uint8_t)(val >> 8) & 0xff;
		*tp++ = (hawk_uint8_t)val & 0xff;
	}
	if (colonp != HAWK_NULL)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		hawk_oow_t n = tp - colonp;
		hawk_oow_t i;

		for (i = 1; i <= n; i++)
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}

	if (tp != endp) return -1;

	return 0;
}
#endif

/* ---------------------------------------------------------- */

int hawk_gem_ucharstoskad (hawk_gem_t* gem, const hawk_uch_t* str, hawk_oow_t len, hawk_skad_t* _skad)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;
	const hawk_uch_t* p;
	const hawk_uch_t* end;
	hawk_ucs_t tmp;

	p = str;
	end = str + len;

	if (p >= end)
	{
		hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "blank address");
		return -1;
	}

	/* use HAWK_SIZEOF(*_skad) instead of HAWK_SIZEOF(*skad) in case they are different */
	HAWK_MEMSET (skad, 0, HAWK_SIZEOF(*_skad));

	if (*p == '@' || *p == '/')
	{
#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
		/* @aaa,  @/tmp/aaa ... */
		hawk_oow_t srclen, dstlen;
		dstlen = HAWK_COUNTOF(skad->un.sun_path) - 1;
		srclen = len - (*p == '@');
		if (hawk_gem_convutobchars(gem, p + (*p == '@'), &srclen, skad->un.sun_path, &dstlen) <= -1) return -1;
		skad->un.sun_path[dstlen] = '\0';
		skad->un.sun_family = AF_UNIX;
		return 0;
#else
		hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_ENOIMPL, "unix address not supported");
		return -1;
#endif
	}


#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	if (*p == '[')
	{
		/* IPv6 address */
		tmp.ptr = (hawk_uch_t*)++p; /* skip [ and remember the position */
		while (p < end && *p != '%' && *p != ']') p++;

		if (p >= end) goto no_rbrack;

		tmp.len = p - tmp.ptr;
		if (*p == '%')
		{
			/* handle scope id */
			hawk_uint32_t x;

			p++; /* skip % */

			if (p >= end)
			{
				/* premature end */
				hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id blank");
				return -1;
			}

			if (*p >= '0' && *p <= '9')
			{
				/* numeric scope id */
				skad->in6.sin6_scope_id = 0;
				do
				{
					x = skad->in6.sin6_scope_id * 10 + (*p - '0');
					if (x < skad->in6.sin6_scope_id)
					{
						hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id too large");
						return -1; /* overflow */
					}
					skad->in6.sin6_scope_id = x;
					p++;
				}
				while (p < end && *p >= '0' && *p <= '9');
			}
			else
			{
				/* interface name as a scope id? */
				const hawk_uch_t* stmp = p;
				unsigned int index;
				do p++; while (p < end && *p != ']');
				if (hawk_gem_ucharstoifindex(gem, stmp, p - stmp, &index) <= -1) return -1;
				skad->in6.sin6_scope_id = index;
			}

			if (p >= end || *p != ']') goto no_rbrack;
		}
		p++; /* skip ] */

		if (uchars_to_ipv6(tmp.ptr, tmp.len, &skad->in6.sin6_addr) <= -1) goto unrecog;
		skad->in6.sin6_family = AF_INET6;
	}
	else
	{
#endif
		/* IPv4 address */
		tmp.ptr = (hawk_uch_t*)p;
		while (p < end && *p != ':') p++;
		tmp.len = p - tmp.ptr;

		if (uchars_to_ipv4(tmp.ptr, tmp.len, &skad->in4.sin_addr) <= -1)
		{
		#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
			/* check if it is an IPv6 address not enclosed in [].
			 * the port number can't be specified in this format. */
			if (p >= end || *p != ':')
			{
				/* without :, it can't be an ipv6 address */
				goto unrecog;
			}

			while (p < end && *p != '%') p++;
			tmp.len = p - tmp.ptr;

			if (uchars_to_ipv6(tmp.ptr, tmp.len, &skad->in6.sin6_addr) <= -1) goto unrecog;

			if (p < end && *p == '%')
			{
				/* handle scope id */
				hawk_uint32_t x;

				p++; /* skip % */

				if (p >= end)
				{
					/* premature end */
					hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id blank");
					return -1;
				}

				if (*p >= '0' && *p <= '9')
				{
					/* numeric scope id */
					skad->in6.sin6_scope_id = 0;
					do
					{
						x = skad->in6.sin6_scope_id * 10 + (*p - '0');
						if (x < skad->in6.sin6_scope_id)
						{
							hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id too large");
							return -1; /* overflow */
						}
						skad->in6.sin6_scope_id = x;
						p++;
					}
					while (p < end && *p >= '0' && *p <= '9');
				}
				else
				{
					/* interface name as a scope id? */
					const hawk_uch_t* stmp = p;
					unsigned int index;
					do p++; while (p < end);
					if (hawk_gem_ucharstoifindex(gem, stmp, p - stmp, &index) <= -1) return -1;
					skad->in6.sin6_scope_id = index;
				}
			}

			if (p < end) goto unrecog; /* some gargage after the end? */

			skad->in6.sin6_family = AF_INET6;
			return 0;
		#else
			goto unrecog;
		#endif
		}

		skad->in4.sin_family = AF_INET;
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	}
#endif

	if (p < end && *p == ':')
	{
		/* port number */
		hawk_uint32_t port = 0;

		p++; /* skip : */

		tmp.ptr = (hawk_uch_t*)p;
		while (p < end && *p >= '0' && *p <= '9')
		{
			port = port * 10 + (*p - '0');
			p++;
		}

		tmp.len = p - tmp.ptr;
		if (tmp.len <= 0 || tmp.len >= 6 ||
		    port > HAWK_TYPE_MAX(hawk_uint16_t))
		{
			hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "port number blank or too large");
			return -1;
		}

	#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		if (skad->in4.sin_family == AF_INET)
			skad->in4.sin_port = hawk_hton16(port);
		else
			skad->in6.sin6_port = hawk_hton16(port);
	#else
		skad->in4.sin_port = hawk_hton16(port);
	#endif
	}

	return 0;

unrecog:
	hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "unrecognized address");
	return -1;

no_rbrack:
	hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "missing right bracket");
	return -1;
}

/* ---------------------------------------------------------- */

int hawk_gem_bcharstoskad (hawk_gem_t* gem, const hawk_bch_t* str, hawk_oow_t len, hawk_skad_t* _skad)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;
	const hawk_bch_t* p;
	const hawk_bch_t* end;
	hawk_bcs_t tmp;

	p = str;
	end = str + len;

	if (p >= end)
	{
		hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "blank address");
		return -1;
	}

	/* use HAWK_SIZEOF(*_skad) instead of HAWK_SIZEOF(*skad) in case they are different */
	HAWK_MEMSET (skad, 0, HAWK_SIZEOF(*_skad));

	if (*p == '@' || *p == '/')
	{
#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
		/* @aaa,  @/tmp/aaa  /tmp/aaa ... */
		hawk_copy_bchars_to_bcstr (skad->un.sun_path, HAWK_COUNTOF(skad->un.sun_path), str + (*p == '@'), len - (*p == '@'));
		skad->un.sun_family = AF_UNIX;
		return 0;
#else
		hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_ENOIMPL, "unix address not supported");
		return -1;
#endif
	}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	if (*p == '[')
	{
		/* IPv6 address */
		tmp.ptr = (hawk_bch_t*)++p; /* skip [ and remember the position */
		while (p < end && *p != '%' && *p != ']') p++;

		if (p >= end) goto no_rbrack;

		tmp.len = p - tmp.ptr;
		if (*p == '%')
		{
			/* handle scope id */
			hawk_uint32_t x;

			p++; /* skip % */

			if (p >= end)
			{
				/* premature end */
				hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id blank");
				return -1;
			}

			if (*p >= '0' && *p <= '9')
			{
				/* numeric scope id */
				skad->in6.sin6_scope_id = 0;
				do
				{
					x = skad->in6.sin6_scope_id * 10 + (*p - '0');
					if (x < skad->in6.sin6_scope_id)
					{
						hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id too large");
						return -1; /* overflow */
					}
					skad->in6.sin6_scope_id = x;
					p++;
				}
				while (p < end && *p >= '0' && *p <= '9');
			}
			else
			{
				/* interface name as a scope id? */
				const hawk_bch_t* stmp = p;
				unsigned int index;
				do p++; while (p < end && *p != ']');
				if (hawk_gem_bcharstoifindex(gem, stmp, p - stmp, &index) <= -1) return -1;
				skad->in6.sin6_scope_id = index;
			}

			if (p >= end || *p != ']') goto no_rbrack;
		}
		p++; /* skip ] */

		if (bchars_to_ipv6(tmp.ptr, tmp.len, &skad->in6.sin6_addr) <= -1) goto unrecog;
		skad->in6.sin6_family = AF_INET6;
	}
	else
	{
#endif
		/* IPv4 address */
		tmp.ptr = (hawk_bch_t*)p;
		while (p < end && *p != ':') p++;
		tmp.len = p - tmp.ptr;

		if (bchars_to_ipv4(tmp.ptr, tmp.len, &skad->in4.sin_addr) <= -1)
		{
		#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
			/* check if it is an IPv6 address not enclosed in [].
			 * the port number can't be specified in this format. */
			if (p >= end || *p != ':')
			{
				/* without :, it can't be an ipv6 address */
				goto unrecog;
			}


			while (p < end && *p != '%') p++;
			tmp.len = p - tmp.ptr;

			if (bchars_to_ipv6(tmp.ptr, tmp.len, &skad->in6.sin6_addr) <= -1) goto unrecog;

			if (p < end && *p == '%')
			{
				/* handle scope id */
				hawk_uint32_t x;

				p++; /* skip % */

				if (p >= end)
				{
					/* premature end */
					hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id blank");
					return -1;
				}

				if (*p >= '0' && *p <= '9')
				{
					/* numeric scope id */
					skad->in6.sin6_scope_id = 0;
					do
					{
						x = skad->in6.sin6_scope_id * 10 + (*p - '0');
						if (x < skad->in6.sin6_scope_id)
						{
							hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "scope id too large");
							return -1; /* overflow */
						}
						skad->in6.sin6_scope_id = x;
						p++;
					}
					while (p < end && *p >= '0' && *p <= '9');
				}
				else
				{
					/* interface name as a scope id? */
					const hawk_bch_t* stmp = p;
					unsigned int index;
					do p++; while (p < end);
					if (hawk_gem_bcharstoifindex(gem, stmp, p - stmp, &index) <= -1) return -1;
					skad->in6.sin6_scope_id = index;
				}
			}

			if (p < end) goto unrecog; /* some gargage after the end? */

			skad->in6.sin6_family = AF_INET6;
			return 0;
		#else
			goto unrecog;
		#endif
		}

		skad->in4.sin_family = AF_INET;
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	}
#endif

	if (p < end && *p == ':')
	{
		/* port number */
		hawk_uint32_t port = 0;

		p++; /* skip : */

		tmp.ptr = (hawk_bch_t*)p;
		while (p < end && *p >= '0' && *p <= '9')
		{
			port = port * 10 + (*p - '0');
			p++;
		}

		tmp.len = p - tmp.ptr;
		if (tmp.len <= 0 || tmp.len >= 6 ||
		    port > HAWK_TYPE_MAX(hawk_uint16_t))
		{
			hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "port number blank or too large");
			return -1;
		}

	#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		if (skad->in4.sin_family == AF_INET)
			skad->in4.sin_port = hawk_hton16(port);
		else
			skad->in6.sin6_port = hawk_hton16(port);
	#else
		skad->in4.sin_port = hawk_hton16(port);
	#endif
	}

	return 0;

unrecog:
	hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "unrecognized address");
	return -1;

no_rbrack:
	hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "missing right bracket");
	return -1;
}

/* ---------------------------------------------------------- */

#define __BTOA(type_t,b,p,end) \
	do { \
		type_t* sp = p; \
		do {  \
			if (p >= end) { \
				if (p == sp) break; \
				if (p - sp > 1) p[-2] = p[-1]; \
				p[-1] = (b % 10) + '0'; \
			} \
			else *p++ = (b % 10) + '0'; \
			b /= 10; \
		} while (b > 0); \
		if (p - sp > 1) { \
			type_t t = sp[0]; \
			sp[0] = p[-1]; \
			p[-1] = t; \
		} \
	} while (0);

#define __ADDDOT(p, end) \
	do { \
		if (p >= end) break; \
		*p++ = '.'; \
	} while (0)

/* ---------------------------------------------------------- */

static hawk_oow_t ip4ad_to_ucstr (const struct in_addr* ipad, hawk_uch_t* buf, hawk_oow_t size)
{
	hawk_uint8_t b;
	hawk_uch_t* p, * end;
	hawk_uint32_t ip;

	if (size <= 0) return 0;

	ip = ipad->s_addr;

	p = buf;
	end = buf + size - 1;

#if defined(HAWK_ENDIAN_BIG)
	b = (ip >> 24) & 0xFF; __BTOA (hawk_uch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >> 16) & 0xFF; __BTOA (hawk_uch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >>  8) & 0xFF; __BTOA (hawk_uch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >>  0) & 0xFF; __BTOA (hawk_uch_t, b, p, end);
#elif defined(HAWK_ENDIAN_LITTLE)
	b = (ip >>  0) & 0xFF; __BTOA (hawk_uch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >>  8) & 0xFF; __BTOA (hawk_uch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >> 16) & 0xFF; __BTOA (hawk_uch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >> 24) & 0xFF; __BTOA (hawk_uch_t, b, p, end);
#else
#	error Unknown Endian
#endif

	*p = '\0';
	return p - buf;
}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
static hawk_oow_t ip6ad_to_ucstr (const struct in6_addr* ipad, hawk_uch_t* buf, hawk_oow_t size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */

#define IP6ADDR_NWORDS (HAWK_SIZEOF(ipad->s6_addr) / HAWK_SIZEOF(hawk_uint16_t))

	hawk_uch_t tmp[HAWK_COUNTOF("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")], *tp;
	struct { int base, len; } best, cur;
	hawk_uint16_t words[IP6ADDR_NWORDS];
	int i;

	if (size <= 0) return 0;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	HAWK_MEMSET (words, 0, HAWK_SIZEOF(words));
	for (i = 0; i < HAWK_SIZEOF(ipad->s6_addr); i++)
		words[i / 2] |= (ipad->s6_addr[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;

	for (i = 0; i < IP6ADDR_NWORDS; i++)
	{
		if (words[i] == 0)
		{
			if (cur.base == -1)
			{
				cur.base = i;
				cur.len = 1;
			}
			else
			{
				cur.len++;
			}
		}
		else
		{
			if (cur.base != -1)
			{
				if (best.base == -1 || cur.len > best.len) best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1)
	{
		if (best.base == -1 || cur.len > best.len) best = cur;
	}
	if (best.base != -1 && best.len < 2) best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for (i = 0; i < IP6ADDR_NWORDS; i++)
	{
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len))
		{
			if (i == best.base) *tp++ = ':';
			continue;
		}

		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0) *tp++ = ':';

		/* Is this address an encapsulated IPv4? ipv4-compatible or ipv4-mapped */
		if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		{
			struct in_addr ip4ad;
			HAWK_MEMCPY (&ip4ad.s_addr, ipad->s6_addr + 12, HAWK_SIZEOF(ip4ad.s_addr));
			tp += ip4ad_to_ucstr(&ip4ad, tp, HAWK_COUNTOF(tmp) - (tp - tmp));
			break;
		}

		tp += hawk_fmt_uintmax_to_ucstr(tp, HAWK_COUNTOF(tmp) - (tp - tmp), words[i], 16, 0, '\0', HAWK_NULL);
	}

	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == IP6ADDR_NWORDS) *tp++ = ':';
	*tp++ = '\0';

	return hawk_copy_ucstr(buf, size, tmp);

#undef IP6ADDR_NWORDS
}
#endif


hawk_oow_t hawk_gem_skadtoucstr (hawk_gem_t* gem, const hawk_skad_t* _skad, hawk_uch_t* buf, hawk_oow_t len, int flags)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	hawk_oow_t xlen = 0;

	/* unsupported types will result in an empty string */

	switch (hawk_skad_get_family(_skad))
	{
		case HAWK_AF_INET:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				xlen += ip4ad_to_ucstr(&skad->in4.sin_addr, buf, len);
			}

			if (flags & HAWK_SKAD_TO_BCSTR_PORT)
			{
				if (!(flags & HAWK_SKAD_TO_BCSTR_ADDR) || skad->in4.sin_port != 0)
				{
					if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
					{
						if (xlen + 1 >= len) goto done;
						buf[xlen++] = ':';
					}

					if (xlen + 1 >= len) goto done;
					xlen += hawk_fmt_uintmax_to_ucstr(&buf[xlen], len - xlen, hawk_ntoh16(skad->in4.sin_port), 10, 0, '\0', HAWK_NULL);
				}
			}
			break;

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case HAWK_AF_INET6:
			if (flags & HAWK_SKAD_TO_BCSTR_PORT)
			{
				if (!(flags & HAWK_SKAD_TO_BCSTR_ADDR) || skad->in6.sin6_port != 0)
				{
					if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
					{
						if (xlen + 1 >= len) goto done;
						buf[xlen++] = '[';
					}
				}
			}

			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				xlen += ip6ad_to_ucstr(&skad->in6.sin6_addr, &buf[xlen], len - xlen);

				if (skad->in6.sin6_scope_id != 0)
				{
					int tmp;

					if (xlen + 1 >= len) goto done;
					buf[xlen++] = '%';

					if (xlen + 1 >= len) goto done;

					tmp = hawk_gem_ifindextoucstr(gem, skad->in6.sin6_scope_id, &buf[xlen], len - xlen);
					if (tmp <= -1)
					{
						xlen += hawk_fmt_uintmax_to_ucstr(&buf[xlen], len - xlen, skad->in6.sin6_scope_id, 10, 0, '\0', HAWK_NULL);
					}
					else xlen += tmp;
				}
			}

			if (flags & HAWK_SKAD_TO_BCSTR_PORT)
			{
				if (!(flags & HAWK_SKAD_TO_BCSTR_ADDR) || skad->in6.sin6_port != 0)
				{
					if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
					{
						if (xlen + 1 >= len) goto done;
						buf[xlen++] = ']';

						if (xlen + 1 >= len) goto done;
						buf[xlen++] = ':';
					}

					if (xlen + 1 >= len) goto done;
					xlen += hawk_fmt_uintmax_to_ucstr(&buf[xlen], len - xlen, hawk_ntoh16(skad->in6.sin6_port), 10, 0, '\0', HAWK_NULL);
				}
			}

			break;
#endif

		case HAWK_AF_UNIX:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				if (flags & HAWK_SKAD_TO_BCSTR_UNIXAT) buf[xlen++] = '@';

				if (xlen + 1 >= len) goto done;
				else
				{
					hawk_oow_t mbslen, wcslen = len - xlen;
					hawk_gem_convbtoucstr (gem, skad->un.sun_path, &mbslen, &buf[xlen], &wcslen, 1);
					/* i don't care about conversion errors */
					xlen += wcslen;
				}
			}

			break;
	}

done:
	if (xlen < len) buf[xlen] = '\0';
	return xlen;
}

/* ---------------------------------------------------------- */

static hawk_oow_t ip4ad_to_bcstr (const struct in_addr* ipad, hawk_bch_t* buf, hawk_oow_t size)
{
	hawk_uint8_t b;
	hawk_bch_t* p, * end;
	hawk_uint32_t ip;

	if (size <= 0) return 0;

	ip = ipad->s_addr;

	p = buf;
	end = buf + size - 1;

#if defined(HAWK_ENDIAN_BIG)
	b = (ip >> 24) & 0xFF; __BTOA (hawk_bch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >> 16) & 0xFF; __BTOA (hawk_bch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >>  8) & 0xFF; __BTOA (hawk_bch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >>  0) & 0xFF; __BTOA (hawk_bch_t, b, p, end);
#elif defined(HAWK_ENDIAN_LITTLE)
	b = (ip >>  0) & 0xFF; __BTOA (hawk_bch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >>  8) & 0xFF; __BTOA (hawk_bch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >> 16) & 0xFF; __BTOA (hawk_bch_t, b, p, end); __ADDDOT (p, end);
	b = (ip >> 24) & 0xFF; __BTOA (hawk_bch_t, b, p, end);
#else
#	error Unknown Endian
#endif

	*p = '\0';
	return p - buf;
}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
static hawk_oow_t ip6ad_to_bcstr (const struct in6_addr* ipad, hawk_bch_t* buf, hawk_oow_t size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */

#define IP6ADDR_NWORDS (HAWK_SIZEOF(ipad->s6_addr) / HAWK_SIZEOF(hawk_uint16_t))

	hawk_bch_t tmp[HAWK_COUNTOF("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")], *tp;
	struct { int base, len; } best, cur;
	hawk_uint16_t words[IP6ADDR_NWORDS];
	int i;

	if (size <= 0) return 0;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	HAWK_MEMSET (words, 0, HAWK_SIZEOF(words));
	for (i = 0; i < HAWK_SIZEOF(ipad->s6_addr); i++)
		words[i / 2] |= (ipad->s6_addr[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;

	for (i = 0; i < IP6ADDR_NWORDS; i++)
	{
		if (words[i] == 0)
		{
			if (cur.base == -1)
			{
				cur.base = i;
				cur.len = 1;
			}
			else
			{
				cur.len++;
			}
		}
		else
		{
			if (cur.base != -1)
			{
				if (best.base == -1 || cur.len > best.len) best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1)
	{
		if (best.base == -1 || cur.len > best.len) best = cur;
	}
	if (best.base != -1 && best.len < 2) best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for (i = 0; i < IP6ADDR_NWORDS; i++)
	{
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len))
		{
			if (i == best.base) *tp++ = ':';
			continue;
		}

		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0) *tp++ = ':';

		/* Is this address an encapsulated IPv4? ipv4-compatible or ipv4-mapped */
		if (i == 6 && best.base == 0 && (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		{
			struct in_addr ip4ad;
			HAWK_MEMCPY (&ip4ad.s_addr, ipad->s6_addr + 12, HAWK_SIZEOF(ip4ad.s_addr));
			tp += ip4ad_to_bcstr(&ip4ad, tp, HAWK_COUNTOF(tmp) - (tp - tmp));
			break;
		}

		tp += hawk_fmt_uintmax_to_bcstr(tp, HAWK_COUNTOF(tmp) - (tp - tmp), words[i], 16, 0, '\0', HAWK_NULL);
	}

	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == IP6ADDR_NWORDS) *tp++ = ':';
	*tp++ = '\0';

	return hawk_copy_bcstr(buf, size, tmp);

#undef IP6ADDR_NWORDS
}
#endif


hawk_oow_t hawk_gem_skadtobcstr (hawk_gem_t* gem, const hawk_skad_t* _skad, hawk_bch_t* buf, hawk_oow_t len, int flags)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	hawk_oow_t xlen = 0;

	/* unsupported types will result in an empty string */

	switch (hawk_skad_get_family(_skad))
	{
		case HAWK_AF_INET:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				xlen += ip4ad_to_bcstr(&skad->in4.sin_addr, buf, len);
			}

			if (flags & HAWK_SKAD_TO_BCSTR_PORT)
			{
				if (!(flags & HAWK_SKAD_TO_BCSTR_ADDR) || skad->in4.sin_port != 0)
				{
					if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
					{
						if (xlen + 1 >= len) goto done;
						buf[xlen++] = ':';
					}

					if (xlen + 1 >= len) goto done;
					xlen += hawk_fmt_uintmax_to_bcstr(&buf[xlen], len - xlen, hawk_ntoh16(skad->in4.sin_port), 10, 0, '\0', HAWK_NULL);
				}
			}
			break;

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case HAWK_AF_INET6:
			if (flags & HAWK_SKAD_TO_BCSTR_PORT)
			{
				if (!(flags & HAWK_SKAD_TO_BCSTR_ADDR) || skad->in6.sin6_port != 0)
				{
					if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
					{
						if (xlen + 1 >= len) goto done;
						buf[xlen++] = '[';
					}
				}
			}

			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{

				if (xlen + 1 >= len) goto done;
				xlen += ip6ad_to_bcstr(&skad->in6.sin6_addr, &buf[xlen], len - xlen);

				if (skad->in6.sin6_scope_id != 0)
				{
					int tmp;

					if (xlen + 1 >= len) goto done;
					buf[xlen++] = '%';

					if (xlen + 1 >= len) goto done;

					tmp = hawk_gem_ifindextobcstr(gem, skad->in6.sin6_scope_id, &buf[xlen], len - xlen);
					if (tmp <= -1)
					{
						xlen += hawk_fmt_uintmax_to_bcstr(&buf[xlen], len - xlen, skad->in6.sin6_scope_id, 10, 0, '\0', HAWK_NULL);
					}
					else xlen += tmp;
				}
			}

			if (flags & HAWK_SKAD_TO_BCSTR_PORT)
			{
				if (!(flags & HAWK_SKAD_TO_BCSTR_ADDR) || skad->in6.sin6_port != 0)
				{
					if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
					{
						if (xlen + 1 >= len) goto done;
						buf[xlen++] = ']';

						if (xlen + 1 >= len) goto done;
						buf[xlen++] = ':';
					}

					if (xlen + 1 >= len) goto done;
					xlen += hawk_fmt_uintmax_to_bcstr(&buf[xlen], len - xlen, hawk_ntoh16(skad->in6.sin6_port), 10, 0, '\0', HAWK_NULL);
				}
			}

			break;
#endif

		case HAWK_AF_UNIX:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				if (flags & HAWK_SKAD_TO_BCSTR_UNIXAT) buf[xlen++] = '@';

				if (xlen + 1 >= len) goto done;
				xlen += hawk_copy_bcstr(&buf[xlen], len - xlen, skad->un.sun_path);
#if 0
				if (xlen + 1 >= len) goto done;
				else
				{
					hawk_oow_t wcslen, mbslen = len - xlen;
					hawk_gem_convutobcstr (gem, skad->un.sun_path, &wcslen, &buf[xlen], &mbslen);
					/* i don't care about conversion errors */
					xlen += mbslen;
				}
#endif
			}

			break;
	}

done:
	if (xlen < len) buf[xlen] = '\0';
	return xlen;
}


/* ------------------------------------------------------------------------- */


int hawk_skad_get_family (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	/*HAWK_STATIC_ASSERT (HAWK_SIZEOF(*_skad) >= HAWK_SIZEOF(*skad));*/
	return skad->sa.sa_family;
}

int hawk_skad_get_size (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	/*HAWK_STATIC_ASSERT (HAWK_SIZEOF(*_skad) >= HAWK_SIZEOF(*skad));*/

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
	#if defined(AF_LINK) && (HAWK_SIZEOF_STRUCT_SOCKADDR_DL > 0)
		case AF_LINK: return HAWK_SIZEOF(struct sockaddr_dl);
	#endif
	#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
		case AF_UNIX: return HAWK_SIZEOF(struct sockaddr_un);
	#endif
	}

	return 0;
}

int hawk_skad_get_port (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;

	switch (skad->sa.sa_family)
	{
	#if defined(AF_INET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
		case AF_INET: return hawk_ntoh16(skad->in4.sin_port);
	#endif
	#if defined(AF_INET6) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case AF_INET6: return hawk_ntoh16(skad->in6.sin6_port);
	#endif
	}
	return 0;
}

int hawk_skad_get_ifindex (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;

#if defined(AF_PACKET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_LL > 0)
	if (skad->sa.sa_family == AF_PACKET) return skad->ll.sll_ifindex;

#elif defined(AF_LINK) && (HAWK_SIZEOF_STRUCT_SOCKADDR_DL > 0)
	if (skad->sa.sa_family == AF_LINK)  return skad->dl.sdl_index;
#endif

	return 0;
}

int hawk_skad_get_scope_id (const hawk_skad_t* _skad)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;

#if defined(AF_INET6) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	if (skad->sa.sa_family == AF_INET6)  return skad->in6.sin6_scope_id;
#endif

	return 0;
}

void hawk_skad_set_scope_id (hawk_skad_t* _skad, int scope_id)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;

#if defined(AF_INET6) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	if (skad->sa.sa_family == AF_INET6) skad->in6.sin6_scope_id = scope_id;
#endif
}

hawk_oow_t hawk_skad_get_ipad_bytes (hawk_skad_t* _skad, void* buf, hawk_oow_t len)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;
	hawk_oow_t outlen = 0;

	switch (skad->sa.sa_family)
	{
	#if defined(AF_INET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
		case AF_INET:
			outlen = len < HAWK_SIZEOF(skad->in4.sin_addr)? len: HAWK_SIZEOF(skad->in4.sin_addr);
			HAWK_MEMCPY (buf, &skad->in4.sin_addr, outlen);
			break;
	#endif
	#if defined(AF_INET6) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case AF_INET6:
			outlen = len < HAWK_SIZEOF(skad->in6.sin6_addr)? len: HAWK_SIZEOF(skad->in6.sin6_addr);
			HAWK_MEMCPY (buf, &skad->in6.sin6_addr, outlen);
			break;
	#endif
	}

	return outlen;
}

void hawk_skad_init_for_ip4 (hawk_skad_t* skad, hawk_uint16_t port, hawk_ip4ad_t* ip4ad)
{
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
	struct sockaddr_in* sin = (struct sockaddr_in*)skad;
	HAWK_MEMSET (sin, 0, HAWK_SIZEOF(*sin));
	sin->sin_family = AF_INET;
	sin->sin_port = hawk_hton16(port);
	if (ip4ad) HAWK_MEMCPY (&sin->sin_addr, ip4ad->v, HAWK_IP4AD_LEN);
#endif
}

void hawk_skad_init_for_ip6 (hawk_skad_t* skad, hawk_uint16_t port, hawk_ip6ad_t* ip6ad, int scope_id)
{
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	struct sockaddr_in6* sin = (struct sockaddr_in6*)skad;
	HAWK_MEMSET (sin, 0, HAWK_SIZEOF(*sin));
	sin->sin6_family = AF_INET6;
	sin->sin6_port = hawk_hton16(port);
	sin->sin6_scope_id = scope_id;
	if (ip6ad) HAWK_MEMCPY (&sin->sin6_addr, ip6ad->v, HAWK_IP6AD_LEN);
#endif
}

void hawk_skad_init_for_ip_with_bytes (hawk_skad_t* skad, hawk_uint16_t port, const hawk_uint8_t* bytes, hawk_oow_t len)
{
	switch (len)
	{
	#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
		case HAWK_IP4AD_LEN:
		{
			struct sockaddr_in* sin = (struct sockaddr_in*)skad;
			HAWK_MEMSET (sin, 0, HAWK_SIZEOF(*sin));
			sin->sin_family = AF_INET;
			sin->sin_port = hawk_hton16(port);
			HAWK_MEMCPY (&sin->sin_addr, bytes, len);
			break;
		}
	#endif
	#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case HAWK_IP6AD_LEN:
		{
			struct sockaddr_in6* sin = (struct sockaddr_in6*)skad;
			HAWK_MEMSET (sin, 0, HAWK_SIZEOF(*sin));
			sin->sin6_family = AF_INET6;
			sin->sin6_port = hawk_hton16(port);
			HAWK_MEMCPY (&sin->sin6_addr, bytes, len);
			break;
		}
	#endif
		default:
			break;
	}
}


void hawk_skad_init_for_eth (hawk_skad_t* skad, int ifindex, hawk_ethad_t* ethad)
{
#if defined(AF_PACKET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_LL > 0)
	struct sockaddr_ll* sll = (struct sockaddr_ll*)skad;
	HAWK_MEMSET (sll, 0, HAWK_SIZEOF(*sll));
	sll->sll_family = AF_PACKET;
	sll->sll_ifindex = ifindex;
	if (ethad)
	{
		sll->sll_halen = HAWK_ETHAD_LEN;
		HAWK_MEMCPY (sll->sll_addr, ethad, HAWK_ETHAD_LEN);
	}

#elif defined(AF_LINK) && (HAWK_SIZEOF_STRUCT_SOCKADDR_DL > 0)
	struct sockaddr_dl* sll = (struct sockaddr_dl*)skad;
	HAWK_MEMSET (sll, 0, HAWK_SIZEOF(*sll));
	sll->sdl_family = AF_LINK;
	sll->sdl_index = ifindex;
	if (ethad)
	{
		sll->sdl_alen = HAWK_ETHAD_LEN;
		HAWK_MEMCPY (sll->sdl_data, ethad, HAWK_ETHAD_LEN);
	}
#else
#	error UNSUPPORTED DATALINK SOCKET ADDRESS
#endif
}

void hawk_clear_skad (hawk_skad_t* _skad)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;
	/*HAWK_STATIC_ASSERT (HAWK_SIZEOF(*_skad) >= HAWK_SIZEOF(*skad));*/
	/* use HAWK_SIZEOF(*_skad) instead of HAWK_SIZEOF(*skad) in case they are different */
	HAWK_MEMSET (skad, 0, HAWK_SIZEOF(*_skad));
	skad->sa.sa_family = HAWK_AF_UNSPEC;
}

int hawk_equal_skads (const hawk_skad_t* addr1, const hawk_skad_t* addr2, int strict)
{
	int f1;

	if ((f1 = hawk_skad_get_family(addr1)) != hawk_skad_get_family(addr2) ||
	    hawk_skad_get_size(addr1) != hawk_skad_get_size(addr2)) return 0;

	switch (f1)
	{
	#if defined(AF_INET) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
		case AF_INET:
			return ((struct sockaddr_in*)addr1)->sin_addr.s_addr == ((struct sockaddr_in*)addr2)->sin_addr.s_addr &&
			       ((struct sockaddr_in*)addr1)->sin_port == ((struct sockaddr_in*)addr2)->sin_port;
	#endif

	#if defined(AF_INET6) && (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case AF_INET6:

			if (strict)
			{
				/* don't care about scope id */
				return HAWK_MEMCMP(&((struct sockaddr_in6*)addr1)->sin6_addr, &((struct sockaddr_in6*)addr2)->sin6_addr, HAWK_SIZEOF(((struct sockaddr_in6*)addr2)->sin6_addr)) == 0 &&
				       ((struct sockaddr_in6*)addr1)->sin6_port == ((struct sockaddr_in6*)addr2)->sin6_port &&
				       ((struct sockaddr_in6*)addr1)->sin6_scope_id == ((struct sockaddr_in6*)addr2)->sin6_scope_id;
			}
			else
			{
				return HAWK_MEMCMP(&((struct sockaddr_in6*)addr1)->sin6_addr, &((struct sockaddr_in6*)addr2)->sin6_addr, HAWK_SIZEOF(((struct sockaddr_in6*)addr2)->sin6_addr)) == 0 &&
				       ((struct sockaddr_in6*)addr1)->sin6_port == ((struct sockaddr_in6*)addr2)->sin6_port;
			}
	#endif

	#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
		case AF_UNIX:
			return hawk_comp_bcstr(((struct sockaddr_un*)addr1)->sun_path, ((struct sockaddr_un*)addr2)->sun_path, 0) == 0;
	#endif

		default:
			return HAWK_MEMCMP(addr1, addr2, hawk_skad_get_size(addr1)) == 0;
	}
}

hawk_oow_t hawk_ipad_bytes_to_ucstr (const hawk_uint8_t* iptr, hawk_oow_t ilen, hawk_uch_t* buf, hawk_oow_t blen)
{
	switch (ilen)
	{
		case HAWK_IP4AD_LEN:
		{
			struct in_addr ip4ad;
			HAWK_MEMCPY (&ip4ad.s_addr, iptr, ilen);
			return ip4ad_to_ucstr(&ip4ad, buf, blen);
		}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case HAWK_IP6AD_LEN:
		{
			struct in6_addr ip6ad;
			HAWK_MEMCPY (&ip6ad.s6_addr, iptr, ilen);
			return ip6ad_to_ucstr(&ip6ad, buf, blen);
		}
#endif

		default:
			if (blen > 0) buf[blen] = '\0';
			return 0;
	}
}

hawk_oow_t hawk_ipad_bytes_to_bcstr (const hawk_uint8_t* iptr, hawk_oow_t ilen, hawk_bch_t* buf, hawk_oow_t blen)
{
	switch (ilen)
	{
		case HAWK_IP4AD_LEN:
		{
			struct in_addr ip4ad;
			HAWK_MEMCPY (&ip4ad.s_addr, iptr, ilen);
			return ip4ad_to_bcstr(&ip4ad, buf, blen);
		}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		case HAWK_IP6AD_LEN:
		{
			struct in6_addr ip6ad;
			HAWK_MEMCPY (&ip6ad.s6_addr, iptr, ilen);
			return ip6ad_to_bcstr(&ip6ad, buf, blen);
		}
#endif

		default:
			if (blen > 0) buf[blen] = '\0';
			return 0;
	}
}

int hawk_uchars_to_ipad_bytes (const hawk_uch_t* str, hawk_oow_t slen, hawk_uint8_t* buf, hawk_oow_t blen)
{
	if (blen >= HAWK_IP6AD_LEN)
	{
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		struct in6_addr i6;
		if (uchars_to_ipv6(str, slen, &i6) <= -1) goto ipv4;
		HAWK_MEMCPY (buf, i6.s6_addr, 16);
		return HAWK_IP6AD_LEN;
#endif
	}
	else if (blen >= HAWK_IP4AD_LEN)
	{
		struct in_addr i4;
	ipv4:
		if (uchars_to_ipv4(str, slen, &i4) <= -1) return -1;
		HAWK_MEMCPY (buf, &i4.s_addr, 4);
		return HAWK_IP4AD_LEN;
	}

	return -1;
}

int hawk_bchars_to_ipad_bytes (const hawk_bch_t* str, hawk_oow_t slen, hawk_uint8_t* buf, hawk_oow_t blen)
{
	if (blen >= HAWK_IP6AD_LEN)
	{
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		struct in6_addr i6;
		if (bchars_to_ipv6(str, slen, &i6) <= -1) goto ipv4;
		HAWK_MEMCPY (buf, i6.s6_addr, 16);
		return HAWK_IP6AD_LEN;
#endif
	}
	else if (blen >= HAWK_IP4AD_LEN)
	{
		struct in_addr i4;
	ipv4:
		if (bchars_to_ipv4(str, slen, &i4) <= -1) return -1;
		HAWK_MEMCPY (buf, &i4.s_addr, 4);
		return HAWK_IP4AD_LEN;
	}

	return -1;
}

int hawk_ipad_bytes_is_v4_mapped (const hawk_uint8_t* iptr, hawk_oow_t ilen)
{
	if (ilen != HAWK_IP6AD_LEN) return 0;

	return iptr[0] == 0x00 && iptr[1] == 0x00 &&
	       iptr[2] == 0x00 && iptr[3] == 0x00 &&
	       iptr[4] == 0x00 && iptr[5] == 0x00 &&
	       iptr[6] == 0x00 && iptr[7] == 0x00 &&
	       iptr[8] == 0x00 && iptr[9] == 0x00 &&
	       iptr[10] == 0xFF && iptr[11] == 0xFF;
}

int hawk_ipad_bytes_is_loop_back (const hawk_uint8_t* iptr, hawk_oow_t ilen)
{
	switch (ilen)
	{
		case HAWK_IP4AD_LEN:
		{
			// 127.0.0.0/8
			return iptr[0] == 0x7F;
		}

		case HAWK_IP6AD_LEN:
		{
			hawk_uint32_t* x = (hawk_uint32_t*)iptr;
			return (x[0] == 0 && x[1] == 0 && x[2] == 0 && x[3] == HAWK_CONST_HTON32(1)) || /* TODO: is this alignment safe?  */
			       (hawk_ipad_bytes_is_v4_mapped(iptr, ilen) && (x[3] & HAWK_CONST_HTON32(0xFF000000u)) == HAWK_CONST_HTON32(0x7F000000u));
		}

		default:
			return 0;
	}
}

int hawk_ipad_bytes_is_link_local (const hawk_uint8_t* iptr, hawk_oow_t ilen)
{
	switch (ilen)
	{
		case HAWK_IP4AD_LEN:
		{
			// 169.254.0.0/16
			return iptr[0] == 0xA9 && iptr[1] == 0xFE;
		}

		case HAWK_IP6AD_LEN:
		{
			/* FE80::/10 */
			return iptr[0] == 0xFE && (iptr[1] & 0xC0) == 0x80;
		}

		default:
			return 0;
	}
}
