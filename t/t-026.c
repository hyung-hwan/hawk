/* Socket-address parser and formatter buffer boundaries. */
#include "hawk-prv.h"
#include "skad-prv.h"
#include "tap.h"

#if defined(HAVE_UNISTD_H) && !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
#include <unistd.h>
#endif

static void test_native_alignment (void)
{
	struct
	{
		hawk_uint8_t prefix;
		hawk_skad_t address;
	} embedded;
	hawk_skad_t addresses[2];
	hawk_skad_t* ptrs[3];
	hawk_oow_t alignment, i;

	alignment = HAWK_ALIGNOF(hawk_skad_alt_t);
	OK(HAWK_SIZEOF(hawk_skad_t) >= HAWK_SIZEOF(hawk_skad_alt_t),
		"public address storage accommodates the native address union");
	OK(HAWK_ALIGNOF(hawk_skad_t) >= alignment,
		"public address type provides native alignment");
	OK(HAWK_SIZEOF(hawk_skad_t) % alignment == 0,
		"public address array stride preserves native alignment");
	ptrs[0] = &addresses[0];
	ptrs[1] = &addresses[1];
	ptrs[2] = &embedded.address;
	for (i = 0; i < HAWK_COUNTOF(ptrs); i++)
	{
		OK((hawk_uintptr_t)ptrs[i] % alignment == 0,
			"array elements and an address following a byte field have native alignment");
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN > 0)
		hawk_skad_init_for_ip4(ptrs[i], 80, HAWK_NULL);
		OK(hawk_skad_get_family(ptrs[i]) == HAWK_AF_INET && hawk_skad_get_port(ptrs[i]) == 80,
			"IPv4 initialization and access work in arrays and embedded structures");
#else
		skip("IPv4 addresses are unavailable", 1);
#endif
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
		hawk_skad_init_for_ip6(ptrs[i], 443, HAWK_NULL, 7);
		OK(hawk_skad_get_family(ptrs[i]) == HAWK_AF_INET6 &&
		   hawk_skad_get_port(ptrs[i]) == 443 && hawk_skad_get_scope_id(ptrs[i]) == 7,
			"IPv6 initialization and access work in arrays and embedded structures");
#else
		skip("IPv6 addresses are unavailable", 1);
#endif
	}
}

static void test_invalid_lengths (void)
{
	hawk_uint8_t raw[32] = { 0 };
	hawk_bch_t bytes[36];
	hawk_uch_t wide[36];
	hawk_oow_t lengths[] = { 0, 1, 3, 5, 15, 17, 32 };
	hawk_oow_t capacities[] = { 0, 1, 2, 4, 32 };
	hawk_oow_t i, j, k, blen, byte_result, wide_result;
	int good;

	for (i = 0; i < HAWK_COUNTOF(lengths); i++)
	{
		for (j = 0; j < HAWK_COUNTOF(capacities); j++)
		{
			blen = capacities[j];
			for (k = 0; k < HAWK_COUNTOF(bytes); k++) bytes[k] = 'X';
			for (k = 0; k < HAWK_COUNTOF(wide); k++) wide[k] = 'X';
			byte_result = hawk_ipad_bytes_to_bcstr(raw, lengths[i], bytes, blen);
			wide_result = hawk_ipad_bytes_to_ucstr(raw, lengths[i], wide, blen);
			good = byte_result == 0 && wide_result == 0;
			for (k = 0; k < HAWK_COUNTOF(bytes); k++)
			{
				if (bytes[k] != (blen > 0 && k == 0? '\0': 'X')) good = 0;
				if (wide[k] != (blen > 0 && k == 0? '\0': 'X')) good = 0;
			}
			OK(good, "unsupported address length produces an empty string without changing surrounding storage");
		}
	}
	OK(hawk_ipad_bytes_to_bcstr(raw, 5, HAWK_NULL, 0) == 0 &&
	   hawk_ipad_bytes_to_ucstr(raw, 5, HAWK_NULL, 0) == 0,
		"unsupported address length accepts a null output with zero capacity");
}

static void test_unaligned_loopback (void)
{
	union
	{
		hawk_uint32_t align;
		hawk_uint8_t bytes[24];
	} storage;
	hawk_uint8_t addresses[][16] =
	{
		{ 127, 0, 0, 1 },
		{ 127, 255, 255, 255 },
		{ 126, 255, 255, 255 },
		{ 128, 0, 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 127, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 127, 255, 255, 255 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 126, 255, 255, 255 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 128, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 127, 0, 0, 1 },
		{ 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }
	};
	int expected[] = { 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0 };
	hawk_oow_t i, offset, len;
	int good;

	for (i = 0; i < HAWK_COUNTOF(addresses); i++)
	{
		len = i < 4? HAWK_IP4AD_LEN: HAWK_IP6AD_LEN;
		good = 1;
		for (offset = 0; offset < 8; offset++)
		{
			HAWK_MEMSET(storage.bytes, 0xa5, HAWK_SIZEOF(storage.bytes));
			HAWK_MEMCPY(storage.bytes + offset, addresses[i], len);
			if (hawk_ipad_bytes_is_loop_back(storage.bytes + offset, len) != expected[i]) good = 0;
			if (HAWK_MEMCMP(storage.bytes + offset, addresses[i], len) != 0) good = 0;
		}
		OK(good, "loopback classification accepts aligned and unaligned IPv4 and IPv6 bytes without modifying them");
	}
}

static void test_ipv4_buffers (void)
{
	const hawk_bch_t* inputs[] = { "0.0.0.0", "127.0.0.1", "192.0.2.1", "255.255.255.255" };
	hawk_uint8_t expected[][4] = { { 0 }, { 127, 0, 0, 1 }, { 192, 0, 2, 1 }, { 255, 255, 255, 255 } };
	hawk_oow_t capacities[] = { 0, 1, 3, 4, 5, 15, 16, 17, 32 };
	hawk_uch_t wide[HAWK_IP4AD_STRLEN + 1];
	hawk_uint8_t byte_result[40], wide_result[40];
	hawk_uint8_t value;
	hawk_oow_t i, j, k, len, capa;
	int byte_len, wide_len, good;

	for (i = 0; i < HAWK_COUNTOF(inputs); i++)
	{
		len = hawk_count_bcstr(inputs[i]);
		for (j = 0; j < len; j++) wide[j] = inputs[i][j];
		for (j = 0; j < HAWK_COUNTOF(capacities); j++)
		{
			capa = capacities[j];
			HAWK_MEMSET(byte_result, 0xa5, HAWK_SIZEOF(byte_result));
			HAWK_MEMSET(wide_result, 0xa5, HAWK_SIZEOF(wide_result));
			byte_len = hawk_bchars_to_ipad_bytes(inputs[i], len, byte_result + 1, capa);
			wide_len = hawk_uchars_to_ipad_bytes(wide, len, wide_result + 1, capa);
			good = byte_len == (capa >= 4? 4: -1) && wide_len == (capa >= 4? 4: -1);
			for (k = 0; k < HAWK_COUNTOF(byte_result); k++)
			{
				value = capa >= 4 && k >= 1 && k <= 4? expected[i][k - 1]: 0xa5;
				if (byte_result[k] != value || wide_result[k] != value) good = 0;
			}
			OK(good, "IPv4 parsing accepts every sufficient buffer size and writes only four address bytes");
		}
	}
	OK(hawk_bchars_to_ipad_bytes("127.0.0.1", 9, HAWK_NULL, 0) == -1 &&
	   hawk_uchars_to_ipad_bytes(HAWK_UT("127.0.0.1"), 9, HAWK_NULL, 0) == -1,
		"IPv4 parsing with zero capacity does not access a null output");
}

static void test_cstr_macros (void)
{
	hawk_uint8_t raw[16];
	hawk_uint8_t expected[] = { 127, 0, 0, 1 };

	OK(hawk_bcstr_to_ipad_bytes("127.0.0.1", raw, 4) == 4 &&
	   HAWK_MEMCMP(raw, expected, 4) == 0,
		"byte C-string conversion macro supplies the string length and output arguments");
	OK(hawk_ucstr_to_ipad_bytes(HAWK_UT("127.0.0.1"), raw, 4) == 4 &&
	   HAWK_MEMCMP(raw, expected, 4) == 0,
		"wide C-string conversion macro supplies the string length and output arguments");
	OK(hawk_oocstr_to_ipad_bytes(HAWK_T("127.0.0.1"), raw, HAWK_SIZEOF(raw)) == 4 &&
	   HAWK_MEMCMP(raw, expected, 4) == 0,
		"generic C-string conversion macro selects the configured character type");
	OK(hawk_bcstr_to_ipad_bytes("127.0.0.1", raw, 3) == -1 &&
	   hawk_ucstr_to_ipad_bytes(HAWK_UT("127.0.0.1"), raw, 3) == -1 &&
	   hawk_oocstr_to_ipad_bytes(HAWK_T("127.0.0.1"), raw, 3) == -1,
		"C-string conversion macros honor the output capacity");
	OK(hawk_bcstr_to_ipad_bytes("invalid", raw, HAWK_SIZEOF(raw)) == -1 &&
	   hawk_ucstr_to_ipad_bytes(HAWK_UT("invalid"), raw, HAWK_SIZEOF(raw)) == -1 &&
	   hawk_oocstr_to_ipad_bytes(HAWK_T("invalid"), raw, HAWK_SIZEOF(raw)) == -1,
		"C-string conversion macros preserve parsing failures");
}

static void test_address_suffixes (hawk_gem_t* gem, const hawk_bch_t* address, int family, int port, int scope_id)
{
	struct
	{
		const hawk_bch_t* ptr;
		hawk_oow_t len;
	} suffixes[] =
	{
		{ "junk", 4 },
		{ " ", 1 },
		{ "\t", 1 },
		{ "\n", 1 },
		{ "\0junk", 5 },
		{ "]", 1 },
		{ ":81junk", 7 }
	};
	hawk_bch_t bytes[96];
	hawk_uch_t wide[96];
	hawk_skad_t skad;
	hawk_oow_t i, j, len, input_len;
	int byte_good, wide_good;

	len = hawk_count_bcstr(address);
	for (i = 0; i < len; i++) wide[i] = bytes[i] = address[i];
	/* The parser must honor the supplied length, even without a terminator. */
	bytes[len] = '!';
	wide[len] = '!';
	byte_good = hawk_gem_bcharstoskad(gem, bytes, len, &skad) == 0 &&
		hawk_skad_get_family(&skad) == family && hawk_skad_get_port(&skad) == port &&
		hawk_skad_get_scope_id(&skad) == scope_id;
	wide_good = hawk_gem_ucharstoskad(gem, wide, len, &skad) == 0 &&
		hawk_skad_get_family(&skad) == family && hawk_skad_get_port(&skad) == port &&
		hawk_skad_get_scope_id(&skad) == scope_id;
	OK(byte_good && wide_good, "valid address slices preserve family, port, and scope without reading beyond their length");

	for (i = 0; i < HAWK_COUNTOF(suffixes); i++)
	{
		for (j = 0; j < suffixes[i].len; j++) wide[len + j] = bytes[len + j] = suffixes[i].ptr[j];
		input_len = len + suffixes[i].len;
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		byte_good = hawk_gem_bcharstoskad(gem, bytes, input_len, &skad) == -1 &&
			hawk_gem_geterrnum(gem) == HAWK_EINVAL;
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		wide_good = hawk_gem_ucharstoskad(gem, wide, input_len, &skad) == -1 &&
			hawk_gem_geterrnum(gem) == HAWK_EINVAL;
		OK(byte_good && wide_good, "byte and wide address parsers reject trailing text, whitespace, NULs, and extra delimiters");
	}
}

static void test_interface_lookup_errors (hawk_gem_t* gem)
{
	hawk_bch_t longname[512];
	hawk_bch_t message[128];
	hawk_uch_t wide[512];
	const hawk_bch_t* names[3];
	hawk_oow_t i, j, len;
	unsigned int index;
	int variant, n, good;

	for (i = 0; i < HAWK_COUNTOF(longname) - 1; i++) longname[i] = 'a';
	longname[i] = '\0';
	names[0] = "";
	names[1] = "no-hawk-if";
	names[2] = longname;
	for (i = 0; i < HAWK_COUNTOF(names); i++)
	{
		len = hawk_count_bcstr(names[i]);
		for (j = 0; j <= len; j++) wide[j] = names[i][j];
		for (variant = 0; variant < 4; variant++)
		{
			index = 12345;
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			switch (variant)
			{
				case 0: n = hawk_gem_bcstrtoifindex(gem, names[i], &index); break;
				case 1: n = hawk_gem_bcharstoifindex(gem, names[i], len, &index); break;
				case 2: n = hawk_gem_ucstrtoifindex(gem, wide, &index); break;
				default: n = hawk_gem_ucharstoifindex(gem, wide, len, &index); break;
			}
			if (i == 1 && n >= 0)
			{
				skip("the requested test interface exists", 1);
				continue;
			}
			good = n <= -1 && index == 12345 && hawk_gem_geterrnum(gem) != HAWK_ENOERR;
			if (!good)
			{
				hawk_gem_fmttobcstr(gem, message, HAWK_COUNTOF(message), "name case=%lu variant=%d result=%d errnum=%d", (unsigned long)i, variant, n, (int)hawk_gem_geterrnum(gem));
				diag(message);
			}
			OK(good, "all interface-name lookup variants report invalid names without modifying the output index");
		}
	}
}

static void test_interface_name_nuls (hawk_gem_t* gem)
{
	struct
	{
		const hawk_bch_t* text;
		hawk_oow_t len;
	} cases[] =
	{
		{ "lo\0junk", HAWK_SIZEOF("lo\0junk") - 1 },
		{ "\0lo", HAWK_SIZEOF("\0lo") - 1 },
		{ "lo\0", HAWK_SIZEOF("lo\0") - 1 },
		{ "l\0o", HAWK_SIZEOF("l\0o") - 1 },
		{ "\0", 1 }
	};
	hawk_bch_t name[128];
	hawk_uch_t wide[128];
	hawk_oow_t i, j, len;
	unsigned int index;
	hawk_errnum_t errnum;
	int variant, n;

	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		for (j = 0; j < cases[i].len; j++) wide[j] = cases[i].text[j];
		for (variant = 0; variant < 2; variant++)
		{
			index = 12345;
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			if (variant == 0)
				n = hawk_gem_bcharstoifindex(gem, cases[i].text, cases[i].len, &index);
			else
				n = hawk_gem_ucharstoifindex(gem, wide, cases[i].len, &index);
			errnum = hawk_gem_geterrnum(gem);
			OK(n <= -1 && index == 12345 && (errnum == HAWK_EINVAL || errnum == HAWK_ENOIMPL),
				"direct length-based interface lookups reject embedded NULs without changing the output index");
		}
	}

	n = hawk_gem_ifindextobcstr(gem, 1, name, HAWK_COUNTOF(name));
	if (n <= -1 || (hawk_oow_t)n > HAWK_COUNTOF(name) - 3)
	{
		skip("a suitable interface name is unavailable", 2);
		return;
	}
	len = n;
	for (i = 0; i < len; i++)
	{
		if ((hawk_uint8_t)name[i] >= 128)
		{
			skip("interface name is not ASCII", 2);
			return;
		}
		wide[i] = name[i];
	}
	name[len] = wide[len] = '\0';
	name[len + 1] = wide[len + 1] = '!';
	index = 12345;
	OK(hawk_gem_bcharstoifindex(gem, name, len, &index) >= 0 && index == 1,
		"a NUL outside a byte interface-name slice does not invalidate that slice");
	index = 12345;
	OK(hawk_gem_ucharstoifindex(gem, wide, len, &index) >= 0 && index == 1,
		"a NUL outside a wide interface-name slice does not invalidate that slice");
}

static void test_reverse_lookup_errors (hawk_gem_t* gem)
{
	unsigned int indices[] = { 0, HAWK_TYPE_MAX(unsigned int) };
	hawk_bch_t bytes[32];
	hawk_uch_t wide[32];
	hawk_oow_t i, j;
	int variant, n, good;

	for (i = 0; i < HAWK_COUNTOF(indices); i++)
	{
		for (variant = 0; variant < 2; variant++)
		{
			for (j = 0; j < HAWK_COUNTOF(bytes); j++) bytes[j] = wide[j] = 'X';
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			if (variant == 0)
				n = hawk_gem_ifindextobcstr(gem, indices[i], bytes, HAWK_COUNTOF(bytes));
			else
				n = hawk_gem_ifindextoucstr(gem, indices[i], wide, HAWK_COUNTOF(wide));
			good = n <= -1 && hawk_gem_geterrnum(gem) != HAWK_ENOERR;
			for (j = 0; j < HAWK_COUNTOF(bytes); j++)
			{
				if (bytes[j] != 'X' || wide[j] != 'X') good = 0;
			}
			OK(good, "invalid reverse lookups report a diagnostic and preserve both output buffers");
		}
	}
}

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
static void test_scope_lookup_errors (hawk_gem_t* gem)
{
	const hawk_bch_t* cases[] =
	{
		"fe80::1%no-hawk-if",
		"[fe80::1%no-hawk-if]",
		"[fe80::1%no-hawk-if]:80",
		"fe80::1%abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
	};
	hawk_skad_t skad;
	hawk_uch_t wide[128];
	hawk_oow_t i, j, len;
	int byte_result, wide_result, byte_good, wide_good;

	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		len = hawk_count_bcstr(cases[i]);
		for (j = 0; j < len; j++) wide[j] = cases[i][j];
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		byte_result = hawk_gem_bcharstoskad(gem, cases[i], len, &skad);
		byte_good = byte_result <= -1 && hawk_gem_geterrnum(gem) != HAWK_ENOERR;
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		wide_result = hawk_gem_ucharstoskad(gem, wide, len, &skad);
		wide_good = wide_result <= -1 && hawk_gem_geterrnum(gem) != HAWK_ENOERR;
		if (byte_result >= 0 && wide_result >= 0)
		{
			skip("the requested test interface exists", 1);
			continue;
		}
		OK(byte_good && wide_good, "bare and bracketed IPv6 scopes preserve interface lookup failure diagnostics");
	}
}

static void test_scope_embedded_nuls (hawk_gem_t* gem)
{
	struct
	{
		const hawk_bch_t* text;
		hawk_oow_t len;
	} cases[] =
	{
		{ "fe80::1%lo\0junk", HAWK_SIZEOF("fe80::1%lo\0junk") - 1 },
		{ "[fe80::1%lo\0junk]", HAWK_SIZEOF("[fe80::1%lo\0junk]") - 1 },
		{ "[fe80::1%lo\0junk]:80", HAWK_SIZEOF("[fe80::1%lo\0junk]:80") - 1 },
		{ "fe80::1%\0lo", HAWK_SIZEOF("fe80::1%\0lo") - 1 },
		{ "[fe80::1%\0lo]:80", HAWK_SIZEOF("[fe80::1%\0lo]:80") - 1 },
		{ "fe80::1%lo\0", HAWK_SIZEOF("fe80::1%lo\0") - 1 },
		{ "[fe80::1%lo\0]:80", HAWK_SIZEOF("[fe80::1%lo\0]:80") - 1 }
	};
	hawk_skad_t skad;
	hawk_uch_t wide[32];
	hawk_oow_t i, j;
	int byte_good, wide_good;

	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		for (j = 0; j < cases[i].len; j++) wide[j] = cases[i].text[j];
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		byte_good = hawk_gem_bcharstoskad(gem, cases[i].text, cases[i].len, &skad) == -1 &&
			hawk_gem_geterrnum(gem) == HAWK_EINVAL;
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		wide_good = hawk_gem_ucharstoskad(gem, wide, cases[i].len, &skad) == -1 &&
			hawk_gem_geterrnum(gem) == HAWK_EINVAL;
		OK(byte_good && wide_good, "interface-name scopes reject embedded NULs before OS lookup in both character widths");
	}
}

static void test_scope_output_lengths (hawk_gem_t* gem)
{
	hawk_skad_t skad;
	hawk_bch_t name[128], input[160], bytes[164];
	hawk_uch_t source[160], wide[164];
	hawk_oow_t i, j, namelen, inputlen, byte_len, wide_len, cap, expected;
	unsigned int index = 1;
	int n, good, flags;

	n = hawk_gem_ifindextobcstr(gem, index, name, HAWK_COUNTOF(name));
	if (n < 2)
	{
		skip("an interface with a name of at least two characters is unavailable", 3);
		return;
	}
	namelen = n;
	for (i = 0; i < namelen; i++)
	{
		if ((hawk_uint8_t)name[i] >= 128)
		{
			skip("interface name is not ASCII", 3);
			return;
		}
	}
	inputlen = hawk_copy_bcstr(input, HAWK_COUNTOF(input), "[fe80::1%");
	inputlen += hawk_copy_bcstr(input + inputlen, HAWK_COUNTOF(input) - inputlen, name);
	inputlen += hawk_copy_bcstr(input + inputlen, HAWK_COUNTOF(input) - inputlen, "]:80");
	for (i = 0; i < inputlen; i++) source[i] = input[i];
	source[inputlen] = '!'; /* A terminator outside the input slice is unnecessary. */
	good = hawk_gem_bcharstoskad(gem, input, inputlen, &skad) == 0 &&
		(hawk_uint32_t)hawk_skad_get_scope_id(&skad) == index &&
		hawk_gem_ucharstoskad(gem, source, inputlen, &skad) == 0 &&
		(hawk_uint32_t)hawk_skad_get_scope_id(&skad) == index;
	OK(good, "valid byte and wide interface-name scopes preserve the interface index");
	if (!good)
	{
		skip("cannot parse the selected interface name", 2);
		return;
	}

	good = 1;
	for (cap = 2; cap <= namelen + 2; cap++)
	{
		for (i = 0; i < HAWK_COUNTOF(wide); i++) wide[i] = 'X';
		n = hawk_gem_ifindextoucstr(gem, index, wide + 1, cap);
		expected = namelen < cap? namelen: cap - 1;
		if (n <= -1 || (hawk_oow_t)n != expected || wide[expected + 1] != '\0') good = 0;
		for (i = 0; i < expected; i++) if (wide[i + 1] != (hawk_uch_t)name[i]) good = 0;
		if (wide[0] != 'X' || wide[cap + 1] != 'X') good = 0;
	}
	OK(good, "wide interface-name truncation returns the character count excluding its terminator");

	flags = HAWK_SKAD_TO_BCSTR_ADDR | HAWK_SKAD_TO_BCSTR_PORT;
	good = 1;
	for (cap = 0; cap <= inputlen + 3; cap++)
	{
		for (i = 0; i < HAWK_COUNTOF(bytes); i++) bytes[i] = wide[i] = 'X';
		byte_len = hawk_gem_skadtobcstr(gem, &skad, bytes + 1, cap, flags);
		wide_len = hawk_gem_skadtoucstr(gem, &skad, wide + 1, cap, flags);
		if (byte_len != wide_len) good = 0;
		if (cap > 0)
		{
			if (byte_len >= cap || wide_len >= cap ||
			    hawk_count_bcstr_limited(bytes + 1, cap) != byte_len ||
			    hawk_count_ucstr_limited(wide + 1, cap) != wide_len) good = 0;
		}
		for (j = 0; j < cap; j++) if (wide[j + 1] != (hawk_uch_t)bytes[j + 1]) good = 0;
		if (bytes[0] != 'X' || wide[0] != 'X' || bytes[cap + 1] != 'X' || wide[cap + 1] != 'X') good = 0;
	}
	OK(good, "byte and wide scoped-address formatting agree on output and lengths at every buffer boundary");
}

static void test_scope_descriptor_cleanup (hawk_gem_t* gem)
{
#if defined(HAVE_UNISTD_H) && !defined(_WIN32) && !defined(__OS2__) && !defined(__DOS__)
	hawk_uch_t name[512], address[544];
	hawk_bch_t ifname[128];
	hawk_skad_t skad;
	const hawk_bch_t* prefix = "fe80::1%";
	hawk_oow_t i, j, len;
	unsigned int index;
	int marker[2], before, after, form, repeat, good;

	if (hawk_gem_ifindextobcstr(gem, 1, ifname, HAWK_COUNTOF(ifname)) <= -1 || pipe(marker) <= -1)
	{
		skip("interface lookup or descriptor probes are unavailable", 1);
		return;
	}
	before = dup(marker[0]);
	if (before <= -1)
	{
		close(marker[0]);
		close(marker[1]);
		skip("descriptor duplication is unavailable", 1);
		return;
	}
	close(before);
	for (i = 0; i < HAWK_COUNTOF(name) - 1; i++) name[i] = 'a';
	name[i] = '\0';
	good = 1;
	for (repeat = 0; repeat < 10; repeat++)
	{
		if (hawk_gem_ucstrtoifindex(gem, name, &index) >= 0) good = 0;
		if (hawk_gem_ucharstoifindex(gem, name, HAWK_COUNTOF(name) - 1, &index) >= 0) good = 0;
		for (form = 0; form < 2; form++)
		{
			len = 0;
			if (form) address[len++] = '[';
			for (j = 0; prefix[j]; j++) address[len++] = prefix[j];
			for (j = 0; j < HAWK_COUNTOF(name) - 1; j++) address[len++] = name[j];
			if (form) address[len++] = ']';
			if (hawk_gem_ucharstoskad(gem, address, len, &skad) >= 0) good = 0;
		}
	}
	after = dup(marker[0]);
	if (after >= 0) close(after);
	close(marker[0]);
	close(marker[1]);
	OK(good && after == before, "rejected wide names and bracketed or bare IPv6 scopes do not leak descriptors");
#else
	skip("descriptor probes are unavailable", 1);
#endif
}
#endif

#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
static void test_unix_embedded_nuls (hawk_gem_t* gem)
{
	struct
	{
		const hawk_bch_t* text;
		hawk_oow_t len;
	} cases[] =
	{
		{ "/tmp/a\0ignored", HAWK_SIZEOF("/tmp/a\0ignored") - 1 },
		{ "@/tmp/a\0ignored", HAWK_SIZEOF("@/tmp/a\0ignored") - 1 },
		{ "@relative\0ignored", HAWK_SIZEOF("@relative\0ignored") - 1 },
		{ "/tmp/a\0", HAWK_SIZEOF("/tmp/a\0") - 1 },
		{ "@\0a", HAWK_SIZEOF("@\0a") - 1 }
	};
	hawk_skad_alt_t skad;
	hawk_uch_t wide[32];
	hawk_oow_t i, j, prefixlen;
	int byte_good, wide_good;

	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		for (j = 0; j < cases[i].len; j++) wide[j] = cases[i].text[j];
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		byte_good = hawk_gem_bcharstoskad(gem, cases[i].text, cases[i].len, (hawk_skad_t*)&skad) == -1 &&
			hawk_gem_geterrnum(gem) == HAWK_EINVAL;
		hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
		wide_good = hawk_gem_ucharstoskad(gem, wide, cases[i].len, (hawk_skad_t*)&skad) == -1 &&
			hawk_gem_geterrnum(gem) == HAWK_EINVAL;
		OK(byte_good && wide_good, "Unix pathname parsers reject NULs within the supplied input length");

		prefixlen = hawk_count_bcstr(cases[i].text);
		byte_good = hawk_gem_bcharstoskad(gem, cases[i].text, prefixlen, (hawk_skad_t*)&skad) == 0 &&
			skad.un.sun_family == AF_UNIX;
		wide_good = hawk_gem_ucharstoskad(gem, wide, prefixlen, (hawk_skad_t*)&skad) == 0 &&
			skad.un.sun_family == AF_UNIX;
		OK(byte_good && wide_good, "a NUL outside the supplied Unix pathname slice does not invalidate the slice");
	}
}

static void test_unix_read_bounds (hawk_gem_t* gem)
{
	enum { PATH_CAPA = HAWK_SIZEOF(((struct sockaddr_un*)0)->sun_path) };
	hawk_skad_t* a;
	hawk_skad_t* b;
	struct sockaddr_un* un_a;
	struct sockaddr_un* un_b;
	hawk_bch_t bytes[PATH_CAPA + 10];
	hawk_uch_t wide[PATH_CAPA + 10];
	hawk_bch_t expected[PATH_CAPA + 2];
	hawk_oow_t capacities[] = { 0, 1, 2, PATH_CAPA / 2, PATH_CAPA + 1, PATH_CAPA + 8 };
	hawk_oow_t i, j, n, byte_len, wide_len, alloc_size;
	int prefix, flags, good;
	hawk_uch_t value;

	/* Different suffixes outside sun_path expose unbounded reads without
	 * requiring a crash. The final allocation byte terminates the suffix. */
	alloc_size = HAWK_SIZEOF(*a) + 8;
	a = hawk_gem_allocmem(gem, alloc_size);
	b = hawk_gem_allocmem(gem, alloc_size);
	if (!a || !b) { bail_out("cannot allocate native Unix addresses"); return; }
	HAWK_MEMSET(a, 'A', alloc_size);
	HAWK_MEMSET(b, 'B', alloc_size);
	HAWK_MEMSET(a, 0, HAWK_OFFSETOF(struct sockaddr_un, sun_path));
	HAWK_MEMSET(b, 0, HAWK_OFFSETOF(struct sockaddr_un, sun_path));
	((hawk_uint8_t*)a)[alloc_size - 1] = '\0';
	((hawk_uint8_t*)b)[alloc_size - 1] = '\0';
	un_a = (struct sockaddr_un*)a;
	un_b = (struct sockaddr_un*)b;
	un_a->sun_family = AF_UNIX;
	un_b->sun_family = AF_UNIX;
	HAWK_MEMSET(un_a->sun_path, 'a', PATH_CAPA);
	HAWK_MEMSET(un_b->sun_path, 'a', PATH_CAPA);
	un_a->sun_path[0] = '/';
	un_b->sun_path[0] = '/';

	for (prefix = 0; prefix < 2; prefix++)
	{
		flags = HAWK_SKAD_TO_BCSTR_ADDR | (prefix? HAWK_SKAD_TO_BCSTR_UNIXAT: 0);
		if (prefix) expected[0] = '@';
		HAWK_MEMCPY(expected + prefix, un_a->sun_path, PATH_CAPA);
		for (i = 0; i < HAWK_COUNTOF(capacities); i++)
		{
			for (j = 0; j < HAWK_COUNTOF(bytes); j++) wide[j] = bytes[j] = '!';
			byte_len = hawk_gem_skadtobcstr(gem, a, bytes, capacities[i], flags);
			wide_len = hawk_gem_skadtoucstr(gem, a, wide, capacities[i], flags);
			n = capacities[i] == 0? 0: capacities[i] - 1;
			if (n > PATH_CAPA + prefix) n = PATH_CAPA + prefix;
			good = byte_len == n && wide_len == n;
			for (j = 0; j < HAWK_COUNTOF(bytes); j++)
			{
				value = j < n? expected[j]: (capacities[i] > 0 && j == n? '\0': '!');
				if (bytes[j] != value || wide[j] != value) good = 0;
			}
			OK(good, "Unix formatting bounds reads by sun_path and terminates every nonempty output buffer");
		}
	}
	OK(hawk_equal_skads(a, b, 0) && hawk_equal_skads(b, a, 1),
		"full unterminated Unix paths compare independently of bytes following sun_path");
	un_b->sun_path[PATH_CAPA - 1] = 'b';
	OK(!hawk_equal_skads(a, b, 0) && !hawk_equal_skads(b, a, 1),
		"Unix comparison includes the last byte of a full unterminated path");
	HAWK_MEMSET(un_b->sun_path, 'b', PATH_CAPA);
	HAWK_MEMCPY(un_a->sun_path, "/tmp/a", HAWK_SIZEOF("/tmp/a"));
	HAWK_MEMCPY(un_b->sun_path, "/tmp/a", HAWK_SIZEOF("/tmp/a"));
	OK(hawk_equal_skads(a, b, 0) && hawk_equal_skads(b, a, 1),
		"terminated Unix paths compare independently of unused bytes after the terminator");
	hawk_gem_freemem(gem, a);
	hawk_gem_freemem(gem, b);
}

static void test_unix_path_limits (hawk_gem_t* gem)
{
	enum { PATH_CAPA = HAWK_SIZEOF(((struct sockaddr_un*)0)->sun_path) };
	hawk_bch_t bytes[PATH_CAPA * 2 + 2];
	hawk_uch_t wide[PATH_CAPA * 2 + 2];
	hawk_oow_t lengths[] = { 1, PATH_CAPA - 1, PATH_CAPA, PATH_CAPA + 1, PATH_CAPA * 2 };
	hawk_skad_alt_t skad;
	hawk_oow_t i, j, pathlen, prefixlen, len;
	int form, result, byte_good, wide_good;

	/* Absolute paths, @-prefixed absolute paths, and @-prefixed relative paths. */
	for (form = 0; form < 3; form++)
	{
		for (i = 0; i < HAWK_COUNTOF(lengths); i++)
		{
			pathlen = lengths[i];
			prefixlen = form > 0? 1: 0;
			if (prefixlen > 0) bytes[0] = '@';
			for (j = 0; j < pathlen; j++) bytes[prefixlen + j] = 'a';
			if (form != 2) bytes[prefixlen] = '/';
			len = prefixlen + pathlen;
			for (j = 0; j < len; j++) wide[j] = bytes[j];
			bytes[len] = '!';
			wide[len] = '!';

			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			result = hawk_gem_bcharstoskad(gem, bytes, len, (hawk_skad_t*)&skad);
			if (pathlen < PATH_CAPA)
			{
				byte_good = result == 0 && skad.un.sun_family == AF_UNIX &&
					HAWK_MEMCMP(skad.un.sun_path, bytes + prefixlen, pathlen) == 0 &&
					skad.un.sun_path[pathlen] == '\0';
			}
			else byte_good = result == -1 && hawk_gem_geterrnum(gem) == HAWK_EBUFFULL;

			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			result = hawk_gem_ucharstoskad(gem, wide, len, (hawk_skad_t*)&skad);
			if (pathlen < PATH_CAPA)
			{
				wide_good = result == 0 && skad.un.sun_family == AF_UNIX &&
					HAWK_MEMCMP(skad.un.sun_path, bytes + prefixlen, pathlen) == 0 &&
					skad.un.sun_path[pathlen] == '\0';
			}
			else wide_good = result == -1 && hawk_gem_geterrnum(gem) == HAWK_EBUFFULL;
			OK(byte_good && wide_good, pathlen < PATH_CAPA?
				"Unix paths fitting sun_path retain every byte and their terminator":
				"Unix paths exceeding sun_path fail without silently truncating the path");
		}
	}
}
#endif

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
static void test_scope_id_limits (hawk_gem_t* gem)
{
	struct
	{
		const hawk_bch_t* text;
		int valid;
		hawk_uint32_t value;
	} cases[] =
	{
		{ "0", 1, 0 },
		{ "1", 1, 1 },
		{ "4294967294", 1, HAWK_TYPE_MAX(hawk_uint32_t) - 1 },
		{ "4294967295", 1, HAWK_TYPE_MAX(hawk_uint32_t) },
		{ "00000000004294967295", 1, HAWK_TYPE_MAX(hawk_uint32_t) },
		{ "4294967296", 0, 0 },
		{ "4772185884", 0, 0 },
		{ "5000000000", 0, 0 },
		{ "8589934591", 0, 0 },
		{ "10000000000", 0, 0 },
		{ "18446744073709551615", 0, 0 },
		{ "00000000005000000000", 0, 0 }
	};
	const hawk_bch_t* prefix = "fe80::1%";
	hawk_bch_t bytes[96];
	hawk_uch_t wide[96];
	hawk_skad_t skad;
	hawk_oow_t i, j, len;
	int form, result, byte_good, wide_good;

	/* Cover bare IPv6, brackets, and brackets followed by a port. */
	for (form = 0; form < 3; form++)
	{
		for (i = 0; i < HAWK_COUNTOF(cases); i++)
		{
			len = 0;
			if (form > 0) bytes[len++] = '[';
			for (j = 0; prefix[j] != '\0'; j++) bytes[len++] = prefix[j];
			for (j = 0; cases[i].text[j] != '\0'; j++) bytes[len++] = cases[i].text[j];
			if (form > 0) bytes[len++] = ']';
			if (form == 2)
			{
				bytes[len++] = ':';
				bytes[len++] = '8';
				bytes[len++] = '0';
			}
			for (j = 0; j < len; j++) wide[j] = bytes[j];

			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			result = hawk_gem_bcharstoskad(gem, bytes, len, &skad);
			if (cases[i].valid)
			{
				byte_good = result == 0 && hawk_skad_get_family(&skad) == HAWK_AF_INET6 &&
					(hawk_uint32_t)hawk_skad_get_scope_id(&skad) == cases[i].value &&
					hawk_skad_get_port(&skad) == (form == 2? 80: 0);
			}
			else byte_good = result == -1 && hawk_gem_geterrnum(gem) == HAWK_EINVAL;

			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			result = hawk_gem_ucharstoskad(gem, wide, len, &skad);
			if (cases[i].valid)
			{
				wide_good = result == 0 && hawk_skad_get_family(&skad) == HAWK_AF_INET6 &&
					(hawk_uint32_t)hawk_skad_get_scope_id(&skad) == cases[i].value &&
					hawk_skad_get_port(&skad) == (form == 2? 80: 0);
			}
			else wide_good = result == -1 && hawk_gem_geterrnum(gem) == HAWK_EINVAL;
			OK(byte_good && wide_good, "numeric scope IDs accept the full uint32 range and reject overflow before arithmetic");
		}
	}
}

static void test_excess_groups (hawk_gem_t* gem)
{
	/* Extra storage lets the old implementation report failed assertions
	 * instead of corrupting unrelated memory. Guards surround the public
	 * address object, including any native padding. */
	union
	{
		hawk_uintmax_t align;
		hawk_uint8_t bytes[256 + HAWK_SIZEOF(hawk_skad_t) + 256];
	} storage;
	hawk_skad_t* skad = (hawk_skad_t*)(storage.bytes + 256);
	hawk_bch_t bytes[132];
	hawk_uch_t wide[132];
	hawk_oow_t counts[] = { 9, 10, 64 };
	hawk_oow_t i, j, len;
	int compressed, byte_result, wide_result, byte_guard, wide_guard;

	for (compressed = 0; compressed < 2; compressed++)
	{
		for (i = 0; i < HAWK_COUNTOF(counts); i++)
		{
			len = 0;
			if (compressed)
			{
				bytes[len++] = ':';
				bytes[len++] = ':';
			}
			for (j = 0; j < counts[i]; j++)
			{
				if (j > 0) bytes[len++] = ':';
				bytes[len++] = '1';
			}
			for (j = 0; j < len; j++) wide[j] = bytes[j];

			HAWK_MEMSET(storage.bytes, 0xa5, HAWK_SIZEOF(storage.bytes));
			byte_result = hawk_gem_bcharstoskad(gem, bytes, len, skad);
			byte_guard = 1;
			for (j = 0; j < HAWK_SIZEOF(storage.bytes); j++)
			{
				if ((j < 256 || j >= 256 + HAWK_SIZEOF(*skad)) && storage.bytes[j] != 0xa5) byte_guard = 0;
			}

			HAWK_MEMSET(storage.bytes, 0xa5, HAWK_SIZEOF(storage.bytes));
			wide_result = hawk_gem_ucharstoskad(gem, wide, len, skad);
			wide_guard = 1;
			for (j = 0; j < HAWK_SIZEOF(storage.bytes); j++)
			{
				if ((j < 256 || j >= 256 + HAWK_SIZEOF(*skad)) && storage.bytes[j] != 0xa5) wide_guard = 0;
			}
			OK(byte_result == -1 && wide_result == -1,
				"byte and wide parsers reject excess IPv6 groups with and without compression");
			OK(byte_guard && wide_guard, "excess IPv6 groups cannot overwrite the address object's guard");
		}
	}
}

static void test_invalid_ipv6_syntax (hawk_gem_t* gem)
{
	const hawk_bch_t* inputs[] =
	{
		"1::2:3:4:5:6:7:8", "::1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8::",
		"1:2:3:4:5:6::192.0.2.1", "::1:2:3:4:5:6:192.0.2.1",
		"00000:1:2:3:4:5:6:7", "1:2:3:4:5:6:7:00000", "1:2:00001::3",
		"::00000", "00000000000000000000000000000000::",
		"1::2::3", ":::1", "1:2:3:4:5:6:7:", "1:2:3:4:5:6:7"
	};
	hawk_bch_t bytes[128];
	hawk_uch_t wide[128];
	hawk_uint8_t raw[16];
	hawk_skad_t skad;
	hawk_oow_t i, j, len, bare_len;
	int form, byte_good, wide_good;

	for (i = 0; i < HAWK_COUNTOF(inputs); i++)
	{
		bare_len = hawk_count_bcstr(inputs[i]);
		for (j = 0; j < bare_len; j++) wide[j] = inputs[i][j];
		byte_good = hawk_bchars_to_ipad_bytes(inputs[i], bare_len, raw, HAWK_SIZEOF(raw)) == -1;
		wide_good = hawk_uchars_to_ipad_bytes(wide, bare_len, raw, HAWK_SIZEOF(raw)) == -1;
		OK(byte_good && wide_good, "raw IPv6 parsers reject empty compression, overlong groups, and malformed separators");

		for (form = 0; form < 3; form++)
		{
			len = 0;
			if (form > 0) bytes[len++] = '[';
			for (j = 0; j < bare_len; j++) bytes[len++] = inputs[i][j];
			if (form > 0) bytes[len++] = ']';
			if (form == 2)
			{
				bytes[len++] = ':';
				bytes[len++] = '8';
				bytes[len++] = '0';
			}
			for (j = 0; j < len; j++) wide[j] = bytes[j];
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			byte_good = hawk_gem_bcharstoskad(gem, bytes, len, &skad) == -1 &&
				hawk_gem_geterrnum(gem) == HAWK_EINVAL;
			hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
			wide_good = hawk_gem_ucharstoskad(gem, wide, len, &skad) == -1 &&
				hawk_gem_geterrnum(gem) == HAWK_EINVAL;
			OK(byte_good && wide_good, "socket-address parsers reject malformed IPv6 in bare and bracketed forms");
		}
	}
}

static void test_valid_ipv6 (hawk_gem_t* gem)
{
	const hawk_bch_t* inputs[] =
	{
		"::", "::1", "2001:db8::1", "1:2:3:4:5:6:7:8",
		"::ffff:192.0.2.1", "::192.0.2.1",
		"1:2:3:4:5:6:192.0.2.1", "2001:db8::192.0.2.1",
		"::1:2:3:4:5:6:7", "1:2:3:4:5:6:7::", "1:2:3::4:5:6:7",
		"0000:0001:0002:0003:0004:0005:0006:ffff", "1:2:3:4:5::192.0.2.1"
	};
	const hawk_bch_t* canonical[] =
	{
		"::", "::1", "2001:db8::1", "1:2:3:4:5:6:7:8",
		"::ffff:192.0.2.1", "::192.0.2.1",
		"1:2:3:4:5:6:c000:201", "2001:db8::c000:201",
		"0:1:2:3:4:5:6:7", "1:2:3:4:5:6:7:0", "1:2:3:0:4:5:6:7",
		"0:1:2:3:4:5:6:ffff", "1:2:3:4:5:0:c000:201"
	};
	hawk_uint8_t expected[][16] =
	{
		{ 0 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0x20, 1, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 0, 2, 1 },
		{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 2, 1 },
		{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 192, 0, 2, 1 },
		{ 0x20, 1, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 2, 1 },
		{ 0, 0, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7 },
		{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 0 },
		{ 0, 1, 0, 2, 0, 3, 0, 0, 0, 4, 0, 5, 0, 6, 0, 7 },
		{ 0, 0, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 255, 255 },
		{ 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 0, 192, 0, 2, 1 }
	};
	hawk_skad_t skad;
	hawk_uint8_t raw[16];
	hawk_uch_t wide[HAWK_IP6AD_STRLEN + 1];
	hawk_bch_t rendered[64];
	hawk_oow_t i, j, len;
	int byte_good, wide_good;

	for (i = 0; i < HAWK_COUNTOF(inputs); i++)
	{
		len = hawk_count_bcstr(inputs[i]);
		for (j = 0; j < len; j++) wide[j] = inputs[i][j];
		byte_good = hawk_gem_bcharstoskad(gem, inputs[i], len, &skad) == 0 &&
			hawk_skad_get_family(&skad) == HAWK_AF_INET6 &&
			hawk_skad_get_ipad_bytes(&skad, raw, HAWK_SIZEOF(raw)) == 16 &&
			HAWK_MEMCMP(raw, expected[i], 16) == 0;
		wide_good = hawk_gem_ucharstoskad(gem, wide, len, &skad) == 0 &&
			hawk_skad_get_family(&skad) == HAWK_AF_INET6 &&
			hawk_skad_get_ipad_bytes(&skad, raw, HAWK_SIZEOF(raw)) == 16 &&
			HAWK_MEMCMP(raw, expected[i], 16) == 0;
		OK(byte_good && wide_good, "valid IPv6 addresses, including IPv4 tails, retain all address bytes");
		OK(hawk_bchars_to_ipad_bytes(inputs[i], len, raw, HAWK_SIZEOF(raw)) == 16 &&
		   HAWK_MEMCMP(raw, expected[i], 16) == 0 &&
		   hawk_uchars_to_ipad_bytes(wide, len, raw, HAWK_SIZEOF(raw)) == 16 &&
		   HAWK_MEMCMP(raw, expected[i], 16) == 0,
			"byte and wide raw-address parsers preserve all sixteen IPv6 bytes");
		OK(hawk_ipad_bytes_to_bcstr(expected[i], 16, rendered, HAWK_COUNTOF(rendered)) == hawk_count_bcstr(canonical[i]) &&
		   hawk_comp_bcstr(rendered, canonical[i], 0) == 0,
			"valid IPv6 byte-to-string conversion remains unchanged");
	}
}
#endif

int main (void)
{
	hawk_t* hawk;
	hawk_uint8_t ip4[] = { 192, 0, 2, 1 };
	hawk_bch_t bytes[16];
	hawk_uch_t wide[16];

	no_plan();
	test_native_alignment();
	test_invalid_lengths();
	test_unaligned_loopback();
	test_ipv4_buffers();
	test_cstr_macros();
	OK(hawk_ipad_bytes_to_bcstr(ip4, 4, bytes, HAWK_COUNTOF(bytes)) == 9 &&
	   hawk_comp_bcstr(bytes, "192.0.2.1", 0) == 0 &&
	   hawk_ipad_bytes_to_ucstr(ip4, 4, wide, HAWK_COUNTOF(wide)) == 9 &&
	   hawk_comp_ucstr(wide, HAWK_UT("192.0.2.1"), 0) == 0,
		"valid IPv4 byte and wide formatting remain unchanged");
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return 1; }
	test_interface_lookup_errors(hawk_getgem(hawk));
	test_interface_name_nuls(hawk_getgem(hawk));
	test_reverse_lookup_errors(hawk_getgem(hawk));
#if defined(AF_UNIX) && (HAWK_SIZEOF_STRUCT_SOCKADDR_UN > 0)
	test_unix_path_limits(hawk_getgem(hawk));
	test_unix_embedded_nuls(hawk_getgem(hawk));
	test_unix_read_bounds(hawk_getgem(hawk));
#else
	skip("Unix addresses are unavailable", 40);
#endif
	test_address_suffixes(hawk_getgem(hawk), "127.0.0.1", HAWK_AF_INET, 0, 0);
	test_address_suffixes(hawk_getgem(hawk), "127.0.0.1:80", HAWK_AF_INET, 80, 0);
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	test_scope_lookup_errors(hawk_getgem(hawk));
	test_scope_embedded_nuls(hawk_getgem(hawk));
	test_scope_output_lengths(hawk_getgem(hawk));
	test_scope_descriptor_cleanup(hawk_getgem(hawk));
	test_scope_id_limits(hawk_getgem(hawk));
	test_excess_groups(hawk_getgem(hawk));
	test_invalid_ipv6_syntax(hawk_getgem(hawk));
	test_valid_ipv6(hawk_getgem(hawk));
	test_address_suffixes(hawk_getgem(hawk), "[::1]", HAWK_AF_INET6, 0, 0);
	test_address_suffixes(hawk_getgem(hawk), "[::1]:80", HAWK_AF_INET6, 80, 0);
	test_address_suffixes(hawk_getgem(hawk), "[fe80::1%7]", HAWK_AF_INET6, 0, 7);
	test_address_suffixes(hawk_getgem(hawk), "[fe80::1%7]:80", HAWK_AF_INET6, 80, 7);
	test_address_suffixes(hawk_getgem(hawk), "::1", HAWK_AF_INET6, 0, 0);
	test_address_suffixes(hawk_getgem(hawk), "fe80::1%7", HAWK_AF_INET6, 0, 7);
#else
	skip("IPv6 is unavailable", 206);
#endif
	hawk_close(hawk);
	return exit_status();
}
