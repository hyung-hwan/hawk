/* Socket-address parser and formatter buffer boundaries. */
#include "hawk-prv.h"
#include "tap.h"

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

#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
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

static void test_valid_ipv6 (hawk_gem_t* gem)
{
	const hawk_bch_t* inputs[] =
	{
		"::", "::1", "2001:db8::1", "1:2:3:4:5:6:7:8",
		"::ffff:192.0.2.1", "::192.0.2.1",
		"1:2:3:4:5:6:192.0.2.1", "2001:db8::192.0.2.1"
	};
	const hawk_bch_t* canonical[] =
	{
		"::", "::1", "2001:db8::1", "1:2:3:4:5:6:7:8",
		"::ffff:192.0.2.1", "::192.0.2.1",
		"1:2:3:4:5:6:c000:201", "2001:db8::c000:201"
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
		{ 0x20, 1, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 2, 1 }
	};
	hawk_skad_t skad;
	hawk_uint8_t raw[16];
	hawk_uch_t wide[32];
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
	test_invalid_lengths();
	test_unaligned_loopback();
	OK(hawk_ipad_bytes_to_bcstr(ip4, 4, bytes, HAWK_COUNTOF(bytes)) == 9 &&
	   hawk_comp_bcstr(bytes, "192.0.2.1", 0) == 0 &&
	   hawk_ipad_bytes_to_ucstr(ip4, 4, wide, HAWK_COUNTOF(wide)) == 9 &&
	   hawk_comp_ucstr(wide, HAWK_UT("192.0.2.1"), 0) == 0,
		"valid IPv4 byte and wide formatting remain unchanged");
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return 1; }
#if (HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 > 0)
	test_excess_groups(hawk_getgem(hawk));
	test_valid_ipv6(hawk_getgem(hawk));
#else
	skip("IPv6 is unavailable", 36);
#endif
	hawk_close(hawk);
	return exit_status();
}
