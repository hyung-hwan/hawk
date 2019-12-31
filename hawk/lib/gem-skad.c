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
#include "utl-skad.h"

static int str_to_ipv4 (const hawk_ooch_t* str, hawk_oow_t len, struct in_addr* inaddr)
{
	const hawk_ooch_t* end;
	int dots = 0, digits = 0;
	hawk_uint32_t acc = 0, addr = 0;
	hawk_ooch_t c;

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
static int str_to_ipv6 (const hawk_ooch_t* src, hawk_oow_t len, struct in6_addr* inaddr)
{
	hawk_uint8_t* tp, * endp, * colonp;
	const hawk_ooch_t* curtok;
	hawk_ooch_t ch;
	int saw_xdigit;
	unsigned int val;
	const hawk_ooch_t* src_end;

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

		if (ch >= '0' && ch <= '9')
			v1 = ch - '0';
		else if (ch >= 'A' && ch <= 'F')
			v1 = ch - 'A' + 10;
		else if (ch >= 'a' && ch <= 'f')
			v1 = ch - 'a' + 10;
		else v1 = -1;
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
		    str_to_ipv4(curtok, src_end - curtok, (struct in_addr*)tp) == 0) 
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

int hawk_gem_oocharstoskad (hawk_gem_t* gem, const hawk_ooch_t* str, hawk_oow_t len, hawk_skad_t* _skad)
{
	hawk_skad_alt_t* skad = (hawk_skad_alt_t*)_skad;
	const hawk_ooch_t* p;
	const hawk_ooch_t* end;
	hawk_oocs_t tmp;

	p = str;
	end = str + len;

	if (p >= end) 
	{
		hawk_gem_seterrbfmt (gem, HAWK_NULL, HAWK_EINVAL, "blank address");
		return -1;
	}

	HAWK_MEMSET (skad, 0, HAWK_SIZEOF(*skad));

#if defined(AF_UNIX)
	if (*p == '/' && len >= 2)
	{
	#if defined(HAWK_OOCH_IS_BCH)
		hawk_copy_bcstr (skad->un.sun_path, HAWK_COUNTOF(skad->un.sun_path), str);
	#else
		hawk_oow_t dstlen;
		dstlen = HAWK_COUNTOF(skad->un.sun_path) - 1;
		if (hawk_gem_convutobchars(gem, p, &len, skad->un.sun_path, &dstlen) <= -1) return -1;
		skad->un.sun_path[dstlen] = '\0';
	#endif
		skad->un.sun_family = AF_UNIX;
		return 0;
	}
#endif

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	if (*p == '[')
	{
		/* IPv6 address */
		tmp.ptr = (hawk_ooch_t*)++p; /* skip [ and remember the position */
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
				const hawk_ooch_t* stmp = p;
				unsigned int index;
				do p++; while (p < end && *p != ']');
				if (hawk_gem_ucharstoifindex(gem, stmp, p - stmp, &index) <= -1) return -1;
				skad->in6.sin6_scope_id = index;
			}

			if (p >= end || *p != ']') goto no_rbrack;
		}
		p++; /* skip ] */

		if (str_to_ipv6(tmp.ptr, tmp.len, &skad->in6.sin6_addr) <= -1) goto unrecog;
		skad->in6.sin6_family = AF_INET6;
	}
	else
	{
#endif
		/* IPv4 address */
		tmp.ptr = (hawk_ooch_t*)p;
		while (p < end && *p != ':') p++;
		tmp.len = p - tmp.ptr;

		if (str_to_ipv4(tmp.ptr, tmp.len, &skad->in4.sin_addr) <= -1)
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

			if (str_to_ipv6(tmp.ptr, tmp.len, &skad->in6.sin6_addr) <= -1) goto unrecog;

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
					const hawk_ooch_t* stmp = p;
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

		tmp.ptr = (hawk_ooch_t*)p;
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

static hawk_oow_t ip4addr_to_ucstr (const struct in_addr* ipad, hawk_uch_t* buf, hawk_oow_t size)
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


static hawk_oow_t ip6addr_to_ucstr (const struct in6_addr* ipad, hawk_uch_t* buf, hawk_oow_t size)
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
			tp += ip4addr_to_ucstr(&ip4ad, tp, HAWK_COUNTOF(tmp) - (tp - tmp));
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


hawk_oow_t hawk_gem_skadtoucstr (hawk_gem_t* gem, const hawk_skad_t* _skad, hawk_uch_t* buf, hawk_oow_t len, int flags)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	hawk_oow_t xlen = 0;

	/* unsupported types will result in an empty string */

	switch (hawk_skad_family(_skad))
	{
		case HAWK_AF_INET:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				xlen += ip4addr_to_ucstr(&skad->in4.sin_addr, buf, len);
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
				xlen += ip6addr_to_ucstr(&skad->in6.sin6_addr, &buf[xlen], len - xlen);

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

		case HAWK_AF_UNIX:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				buf[xlen++] = '@';

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

static hawk_oow_t ip4addr_to_bcstr (const struct in_addr* ipad, hawk_bch_t* buf, hawk_oow_t size)
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


static hawk_oow_t ip6addr_to_bcstr (const struct in6_addr* ipad, hawk_bch_t* buf, hawk_oow_t size)
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
			tp += ip4addr_to_bcstr(&ip4ad, tp, HAWK_COUNTOF(tmp) - (tp - tmp));
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


hawk_oow_t hawk_gem_skadtobcstr (hawk_gem_t* gem, const hawk_skad_t* _skad, hawk_bch_t* buf, hawk_oow_t len, int flags)
{
	const hawk_skad_alt_t* skad = (const hawk_skad_alt_t*)_skad;
	hawk_oow_t xlen = 0;

	/* unsupported types will result in an empty string */

	switch (hawk_skad_family(_skad))
	{
		case HAWK_AF_INET:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				xlen += ip4addr_to_bcstr(&skad->in4.sin_addr, buf, len);
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
				xlen += ip6addr_to_bcstr(&skad->in6.sin6_addr, &buf[xlen], len - xlen);

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

		case HAWK_AF_UNIX:
			if (flags & HAWK_SKAD_TO_BCSTR_ADDR)
			{
				if (xlen + 1 >= len) goto done;
				buf[xlen++] = '@';

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
