/* Standalone graph-regex engine: gem integration, matching, and ownership. */
#include "hawk-prv.h"
#include <hawk-rex.h>
#include "tap.h"

typedef struct allocator_t allocator_t;
struct allocator_t
{
	hawk_oow_t live;
	hawk_oow_t calls;
	hawk_oow_t fail_at;
};

static void* allocate (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();
	void* ptr;

	if (++a->calls == a->fail_at) return HAWK_NULL;
	ptr = real->alloc(real, size);
	if (ptr) a->live++;
	return ptr;
}

static void* reallocate (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();
	int fresh = ptr == HAWK_NULL;
	void* result;

	if (++a->calls == a->fail_at) return HAWK_NULL;
	result = real->realloc(real, ptr, size);
	if (result && fresh) a->live++;
	return result;
}

static void deallocate (hawk_mmgr_t* mmgr, void* ptr)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	if (ptr) a->live--;
	real->free(real, ptr);
}

static void test_matches (hawk_gem_t* gem)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		const hawk_ooch_t* text;
		int options, result;
		hawk_oow_t offset, length;
	} cases[] =
	{
		{ HAWK_T("ab"), HAWK_T("zzabc"), 0, 1, 2, 2 },
		{ HAWK_T("ab"), HAWK_T("zzac"), 0, 0, 0, 0 },
		{ HAWK_T("(a|ab)"), HAWK_T("ab"), 0, 1, 0, 2 },
		{ HAWK_T("a(bc)+d"), HAWK_T("zabcbcd!"), 0, 1, 1, 6 },
		{ HAWK_T("((ab|a)+)[[:digit:]]{1,3}"), HAWK_T("xxabab123!"), 0, 1, 2, 7 },
		{ HAWK_T(""), HAWK_T("abc"), 0, 1, 0, 0 },
		{ HAWK_T("a|"), HAWK_T("x"), 0, 1, 0, 0 },
		{ HAWK_T("^$"), HAWK_T(""), 0, 1, 0, 0 },
		{ HAWK_T("^ab"), HAWK_T("zab"), 0, 0, 0, 0 },
		{ HAWK_T("ab$"), HAWK_T("zab"), 0, 1, 1, 2 },
		{ HAWK_T("a*"), HAWK_T("bbb"), 0, 1, 0, 0 },
		{ HAWK_T("a+"), HAWK_T("zaaa!"), 0, 1, 1, 3 },
		{ HAWK_T("ab?c"), HAWK_T("ac"), 0, 1, 0, 2 },
		{ HAWK_T("a{2,3}"), HAWK_T("zaaaa"), 0, 1, 1, 3 },
		{ HAWK_T("a{2,}"), HAWK_T("zaaaa"), 0, 1, 1, 4 },
		{ HAWK_T("a{0}b"), HAWK_T("ab"), 0, 1, 1, 1 },
		{ HAWK_T("(ab){0}"), HAWK_T("ab"), 0, 1, 0, 0 },
		{ HAWK_T("a{0,0}"), HAWK_T(""), 0, 1, 0, 0 },
		{ HAWK_T("a{2}"), HAWK_T("za{2}"), HAWK_REX_NOBOUND, 1, 1, 4 },
		{ HAWK_T("a.b"), HAWK_T("a\nb"), 0, 1, 0, 3 },
		{ HAWK_T("[a-c]+"), HAWK_T("zabc!"), 0, 1, 1, 3 },
		{ HAWK_T("[^0-9]+"), HAWK_T("123abc456"), 0, 1, 3, 3 },
		{ HAWK_T("[]-]+"), HAWK_T("ab]-"), 0, 1, 2, 2 },
		{ HAWK_T("[[:word:]]+"), HAWK_T("ab_8-"), 0, 1, 0, 4 },
		{ HAWK_T("[[:blank:]]+"), HAWK_T("x \t!"), 0, 1, 1, 2 },
		{ HAWK_T("[[:lower:]]+"), HAWK_T("12Ab!"), HAWK_REX_IGNORECASE, 1, 2, 2 },
		{ HAWK_T("[[:upper:]]"), HAWK_T("7a"), HAWK_REX_IGNORECASE, 1, 1, 1 },
		{ HAWK_T("abc"), HAWK_T("zABC"), HAWK_REX_IGNORECASE, 1, 1, 3 },
		{ HAWK_T("[a-c]+"), HAWK_T("zABC!"), HAWK_REX_IGNORECASE, 1, 1, 3 },
		{ HAWK_T("\\t"), HAWK_T("x\t"), 0, 1, 1, 1 },
		{ HAWK_T("[\\x7f-\\xff]"), HAWK_T("\xff"), 0, 1, 0, 1 },
		{ HAWK_T("[\\x7f-\\xff]"), HAWK_T("\xff"), HAWK_REX_IGNORECASE, 1, 0, 1 },
#if defined(HAWK_OOCH_IS_UCH)
		{ HAWK_T("\\x41\\u0042"), HAWK_T("AB"), 0, 1, 0, 2 },
#else
		{ HAWK_T("\\x41\\x42"), HAWK_T("AB"), 0, 1, 0, 2 },
#endif
		{ HAWK_T("*"), HAWK_T("*"), 0, 1, 0, 1 }
	};
	hawk_rex_t rex;
	hawk_oocs_t str, match;
	hawk_oow_t i;
	int n;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		hawk_rex_setopt(&rex, cases[i].options);
		if (!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)))
		{
			OK(0, "compile matching case");
			printf("# matching case %lu failed to compile\n", (unsigned long)i);
			continue;
		}
		str.ptr = (hawk_ooch_t*)cases[i].text;
		str.len = hawk_count_oocstr(str.ptr);
		match.ptr = HAWK_NULL; match.len = 123;
		n = hawk_rex_exec(&rex, &str, &str, &match);
		OK(n == cases[i].result && (n == 1?
			match.ptr == str.ptr + cases[i].offset && match.len == cases[i].length:
			match.ptr == HAWK_NULL && match.len == 123), "leftmost-longest matching and unchanged no-match output");
		if (n != cases[i].result) printf("# matching case %lu returned %d\n", (unsigned long)i, n);
	}
	hawk_rex_fini(&rex);
}

static void test_errors (hawk_gem_t* gem)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		hawk_errnum_t error;
	} cases[] =
	{
		{ HAWK_T("("), HAWK_EREXPAREN },
		{ HAWK_T("["), HAWK_EREXBADPAT },
		{ HAWK_T("[z-a]"), HAWK_EREXRANGE },
		{ HAWK_T("[[:bad:]]"), HAWK_EREXCTYPE },
		{ HAWK_T("a{2,1}"), HAWK_EREXBADBR },
		{ HAWK_T("a{}"), HAWK_EREXBADBR },
		{ HAWK_T("a{999999999999999999999999999999}"), HAWK_EREXBADBR },
		{ HAWK_T("a{2"), HAWK_EREXBRACE },
		{ HAWK_T("\\"), HAWK_EREXBADPAT },
		{ HAWK_T("*"), HAWK_EREXBADPAT },
		{ HAWK_T("a)"), HAWK_EREXBADPAT }
	};
	hawk_rex_t rex;
	hawk_rex_node_t* old;
	hawk_oocs_t str = { HAWK_T("kept"), 4 };
	hawk_oow_t i;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	OK(hawk_rex_exec(&rex, &str, &str, HAWK_NULL) == -1 && hawk_gem_geterrnum(gem) == HAWK_EPERM,
		"execution without compiled code reports through gem");
	hawk_rex_setopt(&rex, HAWK_REX_STRICT);
	old = hawk_rex_comp(&rex, str.ptr, str.len);
	if (!old) bail_out("cannot compile error-test baseline");
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		OK(!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)) &&
			hawk_rex_geterrnum(&rex) == cases[i].error && hawk_gem_geterrnum(gem) == cases[i].error,
			"compile errors use Hawk error codes in the shared gem");
		OK(rex.code == old && hawk_rex_exec(&rex, &str, &str, HAWK_NULL) == 1,
			"failed compilation preserves the previous compiled expression");
	}
	OK(hawk_rex_geterrmsg(&rex) == hawk_gem_geterrmsg(gem), "regex diagnostic getter borrows the gem message");
	hawk_rex_fini(&rex);
	hawk_rex_fini(&rex);
}

static void test_lifetimes (hawk_gem_t* gem, allocator_t* a)
{
	hawk_rex_t *rex, adopted;
	hawk_rex_node_t* code;
	hawk_oocs_t str = { HAWK_T("zabc!"), 5 }, sub, match;
	hawk_ooch_t pattern[3] = { HAWK_T('a'), 0, HAWK_T('b') };
	hawk_ooch_t bytes[4] = { HAWK_T('x'), HAWK_T('a'), 0, HAWK_T('b') };
	hawk_oow_t i, baseline = a->live;
	int zeroed = 1;

	rex = hawk_rex_open(gem, 32, HAWK_NULL);
	if (!rex) bail_out("cannot open regex object");
	for (i = 0; i < 32; i++) if (((hawk_uint8_t*)hawk_rex_getxtn(rex))[i] != 0) zeroed = 0;
	OK(zeroed && hawk_rex_getgem(rex) == gem, "object borrows its gem and zeroes caller extension storage");
	hawk_rex_setopt(rex, HAWK_REX_STRICT | HAWK_REX_IGNORECASE);
	OK(hawk_rex_getopt(rex) == (HAWK_REX_STRICT | HAWK_REX_IGNORECASE), "regex options round-trip");
	if (!hawk_rex_comp(rex, HAWK_T("abc"), 3)) bail_out("cannot compile ownership baseline");
	code = hawk_rex_yield(rex);
	OK(code && !rex->code && code->u.s.gem == gem, "yield detaches code while retaining its allocator identity");
	hawk_rex_close(rex);
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match) == 1 && match.ptr == str.ptr + 1 && match.len == 3,
		"convenience matcher borrows detached code");
	hawk_rex_init(&adopted, gem, code);
	hawk_rex_fini(&adopted);
	OK(a->live == baseline, "adopting and finalizing detached code releases it exactly once");

	code = hawk_rex_build(gem, 0, HAWK_T("bc$"), 3);
	if (!code) bail_out("cannot build standalone graph");
	sub.ptr = str.ptr + 2; sub.len = 2;
	OK(hawk_rex_match(gem, code, 0, &str, &sub, &match) == 0,
		"end anchor uses the full string rather than the search-window end");
	hawk_rex_freecode(code);
	code = hawk_rex_build(gem, 0, HAWK_T("bc"), 2);
	if (!code) bail_out("cannot build search-window graph");
	OK(hawk_rex_match(gem, code, 0, &str, &sub, &match) == 1 && match.ptr == sub.ptr && match.len == 2,
		"matching respects an explicit search window");
	hawk_rex_freecode(code);

	code = hawk_rex_build(gem, 0, pattern, HAWK_COUNTOF(pattern));
	if (!code) bail_out("cannot compile embedded NUL pattern");
	str.ptr = bytes; str.len = HAWK_COUNTOF(bytes);
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match) == 1 && match.ptr == bytes + 1 && match.len == 3,
		"explicit-length patterns and inputs retain embedded NUL characters");
	hawk_rex_freecode(code);
	pattern[1] = (hawk_ooch_t)0xff;
	bytes[2] = (hawk_ooch_t)0xff;
	code = hawk_rex_build(gem, 0, pattern, HAWK_COUNTOF(pattern));
	if (!code) bail_out("cannot compile high-bit character pattern");
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match) == 1 && match.ptr == bytes + 1 && match.len == 3,
		"high-bit characters remain distinct from the integer EOF token");
	hawk_rex_freecode(code);
	hawk_rex_freecode(HAWK_NULL);
	OK(a->live == baseline, "standalone graph helpers leave no allocations behind");
}

static void test_failures (hawk_gem_t* gem, allocator_t* a)
{
	const hawk_ooch_t* pattern = HAWK_T("((ab|a)+)[[:digit:]]{1,3}");
	hawk_oocs_t str = { HAWK_T("xxabab123!"), 10 }, match;
	hawk_rex_t rex;
	hawk_rex_node_t* code;
	hawk_oow_t before, allocations, baseline, outer = a->live, i;
	int n, good = 1;

	a->fail_at = a->calls + 1;
	OK(!hawk_rex_open(gem, 0, HAWK_NULL) && hawk_gem_geterrnum(gem) == HAWK_ENOMEM,
		"object allocation failure reaches the gem");
	a->fail_at = 0;
	before = a->calls;
	code = hawk_rex_build(gem, 0, pattern, hawk_count_oocstr(pattern));
	if (!code) bail_out("cannot measure regex compiler allocations");
	allocations = a->calls - before;
	hawk_rex_freecode(code);
	for (i = 1; i <= allocations; i++)
	{
		hawk_rex_init(&rex, gem, HAWK_NULL);
		code = hawk_rex_comp(&rex, HAWK_T("kept"), 4);
		if (!code) bail_out("cannot compile allocation baseline");
		baseline = a->live;
		a->fail_at = a->calls + i;
		if (hawk_rex_comp(&rex, pattern, hawk_count_oocstr(pattern)) || rex.code != code ||
		    hawk_gem_geterrnum(gem) != HAWK_ENOMEM || a->live != baseline)
		{
			printf("# compiler allocation failure %lu did not preserve state\n", (unsigned long)i);
			good = 0;
		}
		a->fail_at = 0;
		hawk_rex_fini(&rex);
		if (a->live != outer) good = 0;
	}
	OK(good, "all compiler allocation failures preserve old code and release partial graphs");

	code = hawk_rex_build(gem, 0, pattern, hawk_count_oocstr(pattern));
	if (!code) bail_out("cannot compile execution allocation baseline");
	before = a->calls;
	if (hawk_rex_match(gem, code, 0, &str, &str, &match) != 1) bail_out("cannot measure regex executor allocations");
	allocations = a->calls - before;
	baseline = a->live;
	good = 1;
	for (i = 1; i <= allocations; i++)
	{
		match.ptr = HAWK_NULL; match.len = 123;
		a->fail_at = a->calls + i;
		n = hawk_rex_match(gem, code, 0, &str, &str, &match);
		a->fail_at = 0;
		if (n != -1 || hawk_gem_geterrnum(gem) != HAWK_ENOMEM || a->live != baseline ||
		    match.ptr != HAWK_NULL || match.len != 123)
		{
			printf("# executor allocation failure %lu returned %d, live delta %ld\n", (unsigned long)i, n, (long)(a->live - baseline));
			good = 0;
		}
	}
	OK(good, "all executor allocation failures report errors without leaking groups or modifying output");
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match) == 1, "borrowed compiled code remains reusable after execution failure");
	hawk_rex_freecode(code);
	OK(a->live == outer, "failure-injection suite releases all regex allocations");
}

int main (void)
{
	allocator_t a = { 0, 0, 0 };
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &a };
	hawk_t* hawk;
	hawk_gem_t* gem;
	hawk_oow_t baseline;

	no_plan();
	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) { bail_out("cannot open test interpreter"); return 1; }
	gem = hawk_getgem(hawk);
	baseline = a.live;
	test_matches(gem);
	test_errors(gem);
	test_lifetimes(gem, &a);
	test_failures(gem, &a);
	OK(a.live == baseline, "regex engine leaves the embedding gem's other allocations intact");
	hawk_close(hawk);
	OK(a.live == 0, "custom allocator has no live allocations after closing its owner");
	return exit_status();
}
