/* Test SHA-256 and source include identity handling. */

#include <hawk.h>
#include <hawk-sha256.h>
#include <hawk-str.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

struct source_t
{
	const hawk_ooch_t* name;
	const hawk_ooch_t* text;
	hawk_oow_t pos;
	hawk_uint8_t id[2];
	hawk_uint8_t id_len;
};

static struct source_t source[] =
{
	{ HAWK_NULL, HAWK_NULL, 0, { 0x00, 0x00 }, 0 },
	{ HAWK_T("a"), HAWK_T("function fa() { return 1; }\n"), 0, { 0x11, 0x00 }, 1 },
	{ HAWK_T("alias-a"), HAWK_T("function fa() { return 1; }\n"), 0, { 0x11, 0x00 }, 1 },
	{ HAWK_T("b"), HAWK_T("function fb() { return 2; }\n"), 0, { 0x11, 0x00 }, 2 }
};

static hawk_ooi_t source_io (hawk_t* hawk, hawk_sio_cmd_t cmd, hawk_sio_arg_t* arg, hawk_ooch_t* data, hawk_oow_t size)
{
	struct source_t* src;
	hawk_oow_t i, len, rem;

	switch (cmd)
	{
		case HAWK_SIO_CMD_OPEN:
			src = &source[0];
			if (arg->name)
			{
				for (i = 1; i < HAWK_COUNTOF(source); i++)
				{
					if (hawk_comp_oocstr(arg->name, source[i].name, 0) == 0)
					{
						src = &source[i];
						break;
					}
				}
				if (i >= HAWK_COUNTOF(source))
				{
					hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOENT);
					return -1;
				}
				arg->path = (hawk_ooch_t*)src->name;
				memcpy(arg->unique_id, src->id, src->id_len);
				arg->unique_id_len = src->id_len;
			}
			src->pos = 0;
			arg->handle = src;
			return 1;

		case HAWK_SIO_CMD_CLOSE:
			return 0;

		case HAWK_SIO_CMD_READ:
			src = (struct source_t*)arg->handle;
			len = hawk_count_oocstr(src->text);
			rem = len - src->pos;
			if (size > rem) size = rem;
			if (size > 0)
			{
				memcpy(data, &src->text[src->pos], size * HAWK_SIZEOF(*data));
				src->pos += size;
			}
			return size;

		default:
			hawk_seterrnum(hawk, HAWK_NULL, HAWK_EINTERN);
			return -1;
	}
}

static int parse_source (hawk_t* hawk, const hawk_ooch_t* text)
{
	hawk_sio_cbs_t sio;
	source[0].text = text;
	sio.in = source_io;
	sio.out = HAWK_NULL;
	return hawk_parse(hawk, &sio);
}

static int hex_digit (hawk_bch_t c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

static int digest_matches (const hawk_uint8_t* digest, const hawk_bch_t* expected)
{
	hawk_oow_t i;
	for (i = 0; i < HAWK_SHA256_DIGEST_LEN; i++)
	{
		int hi = hex_digit(expected[i * 2]);
		int lo = hex_digit(expected[i * 2 + 1]);
		if (hi < 0 || lo < 0 || digest[i] != (hawk_uint8_t)((hi << 4) | lo)) return 0;
	}
	return expected[HAWK_SHA256_DIGEST_LEN * 2] == '\0';
}

int main (void)
{
	static const hawk_bch_t abc[] = "abc";
	static const hawk_bch_t long_input[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	static const hawk_ooch_t distinct_includes[] = HAWK_T(
		"@pragma implicit off\n"
		"@include_once \"a\";\n"
		"@include_once \"b\";\n"
		"BEGIN { fa(); fb(); }\n");
	static const hawk_ooch_t duplicate_once[] = HAWK_T(
		"@include_once \"a\";\n"
		"@include_once \"alias-a\";\n"
		"BEGIN { fa(); }\n");
	static const hawk_ooch_t include_then_once[] = HAWK_T(
		"@include \"a\";\n"
		"@include_once \"alias-a\";\n"
		"BEGIN { fa(); }\n");
	static const hawk_ooch_t once_then_include[] = HAWK_T(
		"@include_once \"a\";\n"
		"@include \"alias-a\";\n");
	hawk_sha256_ctx_t ctx;
	hawk_uint8_t digest[HAWK_SHA256_DIGEST_LEN];
	hawk_bch_t thousand_a[1000];
	hawk_t* hawk;
	int i;

	no_plan();

	hawk_sha256_digest(digest, "", 0);
	OK(digest_matches(digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "SHA-256 empty input");

	hawk_sha256_digest(digest, abc, HAWK_SIZEOF(abc) - 1);
	OK(digest_matches(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 abc");

	hawk_sha256_init(&ctx);
	hawk_sha256_update(&ctx, &abc[0], 1);
	hawk_sha256_update(&ctx, &abc[1], 1);
	hawk_sha256_update(&ctx, &abc[2], 1);
	hawk_sha256_final(&ctx, digest);
	OK(digest_matches(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 incremental updates");

	hawk_sha256_digest(digest, long_input, HAWK_SIZEOF(long_input) - 1);
	OK(digest_matches(digest, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "SHA-256 two-block padding");

	memset(thousand_a, 'a', HAWK_SIZEOF(thousand_a));
	hawk_sha256_init(&ctx);
	for (i = 0; i < 1000; i++) hawk_sha256_update(&ctx, thousand_a, HAWK_SIZEOF(thousand_a));
	hawk_sha256_final(&ctx, digest);
	OK(digest_matches(digest, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"), "SHA-256 one million bytes");

	hawk = hawk_openstd(0, HAWK_NULL);
	OK(hawk != HAWK_NULL, "open interpreter");
	if (hawk)
	{
		OK(parse_source(hawk, distinct_includes) >= 0, "include IDs with a common prefix and different lengths stay distinct");
		OK(parse_source(hawk, duplicate_once) >= 0, "include_once skips an alias with the same ID");
		OK(parse_source(hawk, include_then_once) >= 0, "include_once observes an earlier regular include");
		OK(parse_source(hawk, once_then_include) <= -1, "regular include is not suppressed by include_once history");
		hawk_close(hawk);
	}

	return exit_status();
}
