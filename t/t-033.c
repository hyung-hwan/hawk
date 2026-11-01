/* Standalone graph-regex engine: gem integration, matching, and ownership.
 *
 * Normal run: BRE/ERE matching, captures, diagnostics, and ownership tests.
 * --compare-posix: compare 1,338 generated ERE cases with an independent
 *                 bounded-language model over every {a,b} string through
 *                 length six in both dialects (339,852 comparisons).
 *
 * This is a C-locale, complete-match investigation, not a proof of full POSIX
 * conformance. The generated sweep uses common ERE
 * syntax; fixed cases also exercise captures and numeric backreferences.
 * Unsupported locale collation syntax is tested for explicit rejection.
 * No system regex API or external scripting language is required to run it.
 */
#include "hawk-prv.h"
#include <hawk-rex.h>
#include "tap.h"
#include <locale.h>

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
		{ HAWK_T("[\x7f-\xff]"), HAWK_T("\xff"), 0, 1, 0, 1 },
		{ HAWK_T("[\x7f-\xff]"), HAWK_T("\xff"), HAWK_REX_IGNORECASE, 1, 0, 1 },
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
		hawk_rex_setopt(&rex, cases[i].options | HAWK_REX_EXTENDED);
		if (!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)))
		{
			OK(0, "compile matching case");
			printf("# matching case %lu failed to compile\n", (unsigned long)i);
			continue;
		}
		str.ptr = (hawk_ooch_t*)cases[i].text;
		str.len = hawk_count_oocstr(str.ptr);
		match.ptr = HAWK_NULL; match.len = 123;
		n = hawk_rex_exec(&rex, &str, &str, &match, 0);
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
		{ HAWK_T("[\\"), HAWK_EREXBADPAT },
		{ HAWK_T("[^\\"), HAWK_EREXBADPAT },
		{ HAWK_T("[[:digit:]\\"), HAWK_EREXBADPAT },
		{ HAWK_T("[a-b\\"), HAWK_EREXBADPAT },
		{ HAWK_T("[z-a]"), HAWK_EREXRANGE },
		{ HAWK_T("[[:bad:]]"), HAWK_EREXCTYPE },
		{ HAWK_T("a{2,1}"), HAWK_EREXBADBR },
		{ HAWK_T("a{}"), HAWK_EREXBADBR },
		{ HAWK_T("a{999999999999999999999999999999}"), HAWK_EREXBADBR },
		{ HAWK_T("a{2"), HAWK_EREXBRACE },
		{ HAWK_T("\\"), HAWK_EREXBADPAT },
		{ HAWK_T("*"), HAWK_EREXBADPAT },
		{ HAWK_T("a)"), HAWK_EREXBADPAT },
		{ HAWK_T("\\1"), HAWK_EREXSUBREG },
		{ HAWK_T("\\9"), HAWK_EREXSUBREG },
		{ HAWK_T("(a)\\2"), HAWK_EREXSUBREG },
		{ HAWK_T("\\1(a)"), HAWK_EREXSUBREG },
		{ HAWK_T("(a\\1)"), HAWK_EREXSUBREG },
		{ HAWK_T("(a(\\1))"), HAWK_EREXSUBREG }
	};
	hawk_rex_t rex;
	hawk_rex_node_t* old;
	hawk_oocs_t str = { HAWK_T("kept"), 4 };
	hawk_oow_t i;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	OK(hawk_rex_exec(&rex, &str, &str, HAWK_NULL, 0) == -1 && hawk_gem_geterrnum(gem) == HAWK_EPERM,
		"execution without compiled code reports through gem");
	hawk_rex_setopt(&rex, HAWK_REX_STRICT | HAWK_REX_EXTENDED);
	old = hawk_rex_comp(&rex, str.ptr, str.len);
	if (!old) bail_out("cannot compile error-test baseline");
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		OK(!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)) &&
			hawk_rex_geterrnum(&rex) == cases[i].error && hawk_gem_geterrnum(gem) == cases[i].error,
			"compile errors use Hawk error codes in the shared gem");
		OK(rex.code == old && hawk_rex_exec(&rex, &str, &str, HAWK_NULL, 0) == 1,
			"failed compilation preserves the previous compiled expression");
	}
	OK(hawk_rex_geterrmsg(&rex) == hawk_gem_geterrmsg(gem), "regex diagnostic getter borrows the gem message");
	hawk_rex_fini(&rex);
	hawk_rex_fini(&rex);
}

/* Locale collation is outside rex's supported syntax. These cases verify
 * rejection, including where a prefix might otherwise become a literal
 * bracket member or a range endpoint. Both parser modes must reject it. */
static void test_unsupported_syntax (hawk_gem_t* gem, allocator_t* a)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		int equivalence;
	} cases[] =
	{
		{ HAWK_T("[[.a.]]"), 0 },
		{ HAWK_T("[[.ch.]]"), 0 },
		{ HAWK_T("[^[.a.]]"), 0 },
		{ HAWK_T("[x[.a.]]"), 0 },
		{ HAWK_T("[[:digit:][.a.]]"), 0 },
		{ HAWK_T("[a-b[.a.]]"), 0 },
		{ HAWK_T("[A-[.a.]]"), 0 },
		{ HAWK_T("[[.a.]-z]"), 0 },
		{ HAWK_T("[[=a=]]"), 1 },
		{ HAWK_T("[^x[=a=]]"), 1 },
		{ HAWK_T("[[:digit:][=a=]]"), 1 },
		{ HAWK_T("[a-b[=a=]]"), 1 },
		{ HAWK_T("[A-[=a=]]"), 1 },
		{ HAWK_T("[[=a=]-z]"), 1 }
	};
	hawk_rex_t rex;
	hawk_rex_node_t* old;
	hawk_oocs_t str = { HAWK_T("kept"), 4 };
	const hawk_ooch_t* message;
	hawk_oow_t i, baseline;
	int mode;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	old = hawk_rex_comp(&rex, str.ptr, str.len);
	if (!old) bail_out("cannot compile unsupported-syntax baseline");
	baseline = a->live;
	for (mode = 0; mode < 4; mode++)
	{
		hawk_rex_setopt(&rex, ((mode & 1)? HAWK_REX_STRICT: 0) | ((mode & 2)? HAWK_REX_EXTENDED: 0));
		for (i = 0; i < HAWK_COUNTOF(cases); i++)
		{
			message = cases[i].equivalence?
				HAWK_T("equivalence classes are not supported in regular expressions"):
				HAWK_T("collating symbols are not supported in regular expressions");
			OK(!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)) &&
				hawk_rex_geterrnum(&rex) == HAWK_ENOSUP,
				"locale collation syntax is explicitly unsupported in both parser modes");
			OK(hawk_comp_oocstr(hawk_rex_geterrmsg(&rex), message, 0) == 0,
				"unsupported collation diagnostic identifies the rejected construct");
			OK(rex.code == old && a->live == baseline && hawk_rex_exec(&rex, &str, &str, HAWK_NULL, 0) == 1,
				"unsupported syntax releases partial graphs and preserves usable old code");
		}
	}
	hawk_rex_fini(&rex);
}

static void test_captures (hawk_gem_t* gem)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		const hawk_ooch_t* text;
		int options, found;
		hawk_oow_t nsub;
		hawk_ooi_t offsets[12];
		hawk_oow_t lengths[12];
	} cases[] =
	{
		{ HAWK_T("(a)(b)"), HAWK_T("zab!"), 0, 1, 2, { 1, 1, 2 }, { 2, 1, 1 } },
		{ HAWK_T("(a(b|c))"), HAWK_T("ac"), 0, 1, 2, { 0, 0, 1 }, { 2, 2, 1 } },
		{ HAWK_T("(a)?(b)"), HAWK_T("b"), 0, 1, 2, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("(a*)(b*)"), HAWK_T("abbb"), 0, 1, 2, { 0, 0, 1 }, { 4, 1, 3 } },
		{ HAWK_T("()(a?)"), HAWK_T(""), 0, 1, 2, { 0, 0, 0 }, { 0, 0, 0 } },
		{ HAWK_T("(ab)+"), HAWK_T("abab"), 0, 1, 1, { 0, 2 }, { 4, 2 } },
		{ HAWK_T("((a|b)+)"), HAWK_T("ab"), 0, 1, 2, { 0, 0, 1 }, { 2, 2, 1 } },
		{ HAWK_T("(a|aa)(a?)"), HAWK_T("aa"), 0, 1, 2, { 0, 0, 2 }, { 2, 2, 0 } },
		{ HAWK_T("(aa|a)(a?)"), HAWK_T("aa"), 0, 1, 2, { 0, 0, 2 }, { 2, 2, 0 } },
		{ HAWK_T("(a|aa)*"), HAWK_T("aaaa"), 0, 1, 1, { 0, 2 }, { 4, 2 } },
		{ HAWK_T("((a|aa)*)"), HAWK_T("aaaa"), 0, 1, 2, { 0, 0, 2 }, { 4, 4, 2 } },
		{ HAWK_T("(a*){2}"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("(a?){2}"), HAWK_T("aa"), 0, 1, 1, { 0, 1 }, { 2, 1 } },
		{ HAWK_T("(a|(b))*"), HAWK_T("ba"), 0, 1, 2, { 0, 1, 0 }, { 2, 1, 1 } },
		{ HAWK_T("(a){0}(b)"), HAWK_T("b"), 0, 1, 2, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("()\\1*"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("(a*)\\1{1000}"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("^(a|aa)\\1$"), HAWK_T("aaaa"), 0, 1, 1, { 0, 0 }, { 4, 2 } },
		{ HAWK_T("(a|aa)\\1"), HAWK_T("aaa"), 0, 1, 1, { 0, 0 }, { 2, 1 } },
		{ HAWK_T("(a*)\\1"), HAWK_T("aaaa"), 0, 1, 1, { 0, 0 }, { 4, 2 } },
		{ HAWK_T("(a)?b\\1"), HAWK_T("b"), 0, 0, 1, { 0 }, { 0 } },
		{ HAWK_T("(a*)\\1"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("(ab)\\1{2}"), HAWK_T("ababab"), 0, 1, 1, { 0, 0 }, { 6, 2 } },
		{ HAWK_T("(a|b)+\\1"), HAWK_T("abb"), 0, 1, 1, { 0, 1 }, { 3, 1 } },
		{ HAWK_T("(a|aa)+\\1"), HAWK_T("aaa"), 0, 1, 1, { 0, 1 }, { 3, 1 } },
		{ HAWK_T("(aa|a)+\\1"), HAWK_T("aaa"), 0, 1, 1, { 0, 1 }, { 3, 1 } },
		{ HAWK_T("(a{1,2})+\\1"), HAWK_T("aaa"), 0, 1, 1, { 0, 1 }, { 3, 1 } },
		{ HAWK_T("(a?)+\\1"), HAWK_T("a"), 0, 1, 1, { 0, 1 }, { 1, 0 } },
		{ HAWK_T("(a*)+\\1"), HAWK_T("a"), 0, 1, 1, { 0, 1 }, { 1, 0 } },
		{ HAWK_T("(a?)?(\\1)*"), HAWK_T(""), 0, 1, 2, { 0, 0, 0 }, { 0, 0, 0 } },
		{ HAWK_T("(a?)?(\\1)*"), HAWK_T("a"), 0, 1, 2, { 0, 0, -1 }, { 1, 1, 0 } },
		{ HAWK_T("((a?)*)*\\2"), HAWK_T(""), 0, 1, 2, { 0, 0, 0 }, { 0, 0, 0 } },
		{ HAWK_T("((a?)\\2)*"), HAWK_T(""), 0, 1, 2, { 0, 0, 0 }, { 0, 0, 0 } },
		{ HAWK_T("((a*)+)*\\2"), HAWK_T("a"), 0, 1, 2, { 0, 0, 1 }, { 1, 1, 0 } },
		{ HAWK_T("((a|aa){1,2})\\1"), HAWK_T("aaaaaaaa"), 0, 1, 2, { 0, 0, 2 }, { 8, 4, 2 } },
		{ HAWK_T("(a)(b)\\2\\1"), HAWK_T("abba"), 0, 1, 2, { 0, 0, 1 }, { 4, 1, 1 } },
		{ HAWK_T("(a)(b)(c)(d)(e)(f)(g)(h)(i)\\9\\1"), HAWK_T("abcdefghiia"), 0, 1, 9,
			{ 0, 0, 1, 2, 3, 4, 5, 6, 7, 8 }, { 11, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
		{ HAWK_T("(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)"), HAWK_T("abcdefghij"), 0, 1, 10,
			{ 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }, { 10, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
		{ HAWK_T("(ab)\\1"), HAWK_T("abAB"), HAWK_REX_IGNORECASE, 1, 1, { 0, 0 }, { 4, 2 } },
		{ HAWK_T("(a|b)\\1"), HAWK_T("ab"), 0, 0, 1, { 0 }, { 0 } },
		{ HAWK_T("(a)|(b)\\1"), HAWK_T("bb"), 0, 0, 2, { 0 }, { 0 } },
		{ HAWK_T("(a)|(b)\\2"), HAWK_T("bb"), 0, 1, 2, { 0, -1, 0 }, { 2, 0, 1 } },
		{ HAWK_T("(a)\\1"), HAWK_T("a1"), 0, 0, 1, { 0 }, { 0 } },
		{ HAWK_T("([\\1])"), HAWK_T("1"), 0, 1, 1, { 0, 0 }, { 1, 1 } },
		{ HAWK_T("(a)\\x31"), HAWK_T("a1"), 0, 1, 1, { 0, 0 }, { 2, 1 } },
		{ HAWK_T("(a)\\\\1"), HAWK_T("a\\1"), 0, 1, 1, { 0, 0 }, { 3, 1 } },
		{ HAWK_T("((a)\\2)"), HAWK_T("aa"), 0, 1, 2, { 0, 0, 0 }, { 2, 2, 1 } },
		{ HAWK_T("a"), HAWK_T("za"), 0, 1, 0, { 1 }, { 1 } }
	};
	hawk_rex_t rex;
	hawk_oocs_t input, matches[12], old[12], whole;
	hawk_oocs_t window;
	hawk_ooch_t nul_pattern[] = { '(', 'a', 0, ')', '\\', '1' };
	hawk_ooch_t nul_text[] = { 'x', 'a', 0, 'a', 0, 'z' };
	hawk_oow_t i, j;
	int n, good;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	OK(hawk_rex_getnsub(&rex) == 0, "uncompiled regex has no capturing groups");
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		hawk_rex_setopt(&rex, cases[i].options | HAWK_REX_STRICT | HAWK_REX_EXTENDED);
		if (!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)))
		{
			OK(0, "compile capture/backreference case");
			printf("# capture case %lu failed to compile\n", (unsigned long)i);
			continue;
		}
		OK(hawk_rex_getnsub(&rex) == cases[i].nsub, "groups are numbered by opening parentheses, including groups beyond nine");
		input.ptr = (hawk_ooch_t*)cases[i].text;
		input.len = hawk_count_oocstr(input.ptr);
		HAWK_MEMSET(matches, 0, HAWK_SIZEOF(matches));
		for (j = 0; j < HAWK_COUNTOF(matches); j++) { matches[j].ptr = input.ptr; matches[j].len = 123; }
		HAWK_MEMCPY(old, matches, HAWK_SIZEOF(old));
		n = hawk_rex_execx(&rex, &input, &input, matches, HAWK_COUNTOF(matches), 0);
		good = n == cases[i].found;
		if (n == 1)
		{
			for (j = 0; j < HAWK_COUNTOF(matches); j++)
			{
				if (j > cases[i].nsub || cases[i].offsets[j] < 0)
				{
					if (matches[j].ptr || matches[j].len) good = 0;
				}
				else if (matches[j].ptr != input.ptr + cases[i].offsets[j] || matches[j].len != cases[i].lengths[j]) good = 0;
			}
		}
		else if (HAWK_MEMCMP(old, matches, HAWK_SIZEOF(old)) != 0) good = 0;
		OK(good, "capture spans, unmatched groups, empty groups, and surplus output entries are correct");
		if (!good) printf("# capture case %lu returned %d\n", (unsigned long)i, n);
		n = hawk_rex_exec(&rex, &input, &input, &whole, 0);
		OK(n == cases[i].found && (n != 1 ||
			(whole.ptr == input.ptr + cases[i].offsets[0] && whole.len == cases[i].lengths[0])),
			"existing whole-match API executes backreferences without capture output");
	}
	OK(hawk_rex_execx(&rex, &input, &input, HAWK_NULL, 1, 0) == -1 && hawk_rex_geterrnum(&rex) == HAWK_EINVAL,
		"positive output count requires capture storage");
	OK(hawk_rex_execx(&rex, &input, &input, HAWK_NULL, 0, 0) == 1, "matching without any output storage succeeds");
	OK(hawk_rex_matchx(gem, rex.code, 0, &input, &input, matches, 1, 0) == 1 && matches[0].ptr == input.ptr + 1,
		"borrowed-graph capture API supports a truncated output array");
	if (!hawk_rex_comp(&rex, nul_pattern, HAWK_COUNTOF(nul_pattern))) bail_out("cannot compile embedded-NUL backreference");
	input.ptr = nul_text; input.len = HAWK_COUNTOF(nul_text);
	window.ptr = nul_text + 1; window.len = 4;
	OK(hawk_rex_matchx(gem, rex.code, 0, &input, &window, matches, HAWK_COUNTOF(matches), 0) == 1 &&
		matches[0].ptr == nul_text + 1 && matches[0].len == 4 &&
		matches[1].ptr == nul_text + 1 && matches[1].len == 2,
		"captures and backreferences retain embedded NULs and original input offsets in a search window");
	HAWK_MEMCPY(old, matches, HAWK_SIZEOF(old));
	window.len = 3;
	OK(hawk_rex_execx(&rex, &input, &window, matches, HAWK_COUNTOF(matches), 0) == 0 &&
		HAWK_MEMCMP(old, matches, HAWK_SIZEOF(old)) == 0,
		"backreference cannot consume beyond its search window and no-match preserves capture output");
	hawk_rex_fini(&rex);
}

static void test_capture_failures (hawk_gem_t* gem, allocator_t* a)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		int options;
	} patterns[] =
	{
		{ HAWK_T("((a|aa)+)(b?)"), HAWK_REX_EXTENDED },
		{ HAWK_T("((a|aa)+)\\1"), HAWK_REX_EXTENDED },
		{ HAWK_T("\\(\\(a\\{1,2\\}\\)\\+\\)\\(b\\?\\)"), 0 },
		{ HAWK_T("\\(a\\{1,2\\}\\)\\+\\1"), 0 },
		{ HAWK_T("\\(a\\|aa\\)\\+\\1"), 0 }
	};
	hawk_oocs_t input = { HAWK_T("aaaa"), 4 }, matches[5], saved[5];
	hawk_rex_t rex;
	hawk_rex_node_t* old;
	hawk_oow_t p, i, j, before, allocations, baseline, outer = a->live;
	int n, good;

	for (p = 0; p < HAWK_COUNTOF(patterns); p++)
	{
		hawk_rex_init(&rex, gem, HAWK_NULL);
		hawk_rex_setopt(&rex, patterns[p].options);
		before = a->calls;
		if (!hawk_rex_comp(&rex, patterns[p].pattern, hawk_count_oocstr(patterns[p].pattern))) bail_out("cannot measure capture compiler allocations");
		allocations = a->calls - before;
		hawk_rex_fini(&rex);
		good = 1;
		for (i = 1; i <= allocations; i++)
		{
			hawk_rex_init(&rex, gem, HAWK_NULL);
			hawk_rex_setopt(&rex, patterns[p].options);
			old = hawk_rex_comp(&rex, HAWK_T("kept"), 4);
			if (!old) bail_out("cannot compile capture failure baseline");
			baseline = a->live;
			a->fail_at = a->calls + i;
			if (hawk_rex_comp(&rex, patterns[p].pattern, hawk_count_oocstr(patterns[p].pattern)) ||
			    hawk_rex_geterrnum(&rex) != HAWK_ENOMEM || rex.code != old ||
			    hawk_rex_getnsub(&rex) != 0 || a->live != baseline) good = 0;
			a->fail_at = 0;
			hawk_rex_fini(&rex);
		}
		OK(good && a->live == outer, "capture/backreference compiler allocation failures retain old graph and group count");

		hawk_rex_init(&rex, gem, HAWK_NULL);
		hawk_rex_setopt(&rex, patterns[p].options);
		if (!hawk_rex_comp(&rex, patterns[p].pattern, hawk_count_oocstr(patterns[p].pattern))) bail_out("cannot compile capture allocation probe");
		before = a->calls;
		if (hawk_rex_execx(&rex, &input, &input, matches, HAWK_COUNTOF(matches), 0) != 1)
			bail_out("cannot measure capture executor allocations");
		allocations = a->calls - before;
		baseline = a->live;
		good = 1;
		for (i = 1; i <= allocations; i++)
		{
			HAWK_MEMSET(matches, 0, HAWK_SIZEOF(matches));
			for (j = 0; j < HAWK_COUNTOF(matches); j++) { matches[j].ptr = input.ptr; matches[j].len = 123; }
			HAWK_MEMCPY(saved, matches, HAWK_SIZEOF(saved));
			a->fail_at = a->calls + i;
			n = hawk_rex_execx(&rex, &input, &input, matches, HAWK_COUNTOF(matches), 0);
			a->fail_at = 0;
			if (n != -1 || hawk_rex_geterrnum(&rex) != HAWK_ENOMEM || a->live != baseline ||
			    HAWK_MEMCMP(saved, matches, HAWK_SIZEOF(saved)) != 0)
			{
				good = 0;
				printf("# capture execution allocation failure %lu in case %lu returned %d\n", (unsigned long)i, (unsigned long)p, n);
			}
		}
		OK(good, "all capture snapshot allocation failures release state and preserve caller output");
		OK(hawk_rex_execx(&rex, &input, &input, matches, HAWK_COUNTOF(matches), 0) == 1,
			"compiled capture/backreference graph remains reusable after every allocation failure");
		hawk_rex_fini(&rex);
		OK(a->live == outer, "capture/backreference failure tests release every allocation");
	}
}

static void test_exec_flags (hawk_gem_t* gem)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		const hawk_ooch_t* text;
		int basic, flags;
		hawk_oow_t start, length;
		int found;
		hawk_ooi_t offsets[3];
		hawk_oow_t lengths[3];
	} cases[] =
	{
		{ HAWK_T("^a"), HAWK_T("a"), 0, 0, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("^a"), HAWK_T("a"), 0, HAWK_REX_NOTBOL, 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("^a"), HAWK_T("a"), 0, HAWK_REX_NOTEOL, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("^a"), HAWK_T("ba"), 0, 0, 1, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("a$"), HAWK_T("a"), 0, 0, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("a$"), HAWK_T("a"), 0, HAWK_REX_NOTEOL, 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("a$"), HAWK_T("a"), 0, HAWK_REX_NOTBOL, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("a$"), HAWK_T("ab"), 0, 0, 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("a$"), HAWK_T("ba"), 0, 0, 1, 1, 1, { 1 }, { 1 } },
		{ HAWK_T("a$"), HAWK_T("ba"), 0, HAWK_REX_NOTEOL, 1, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("^$"), HAWK_T(""), 0, 0, 0, 0, 1, { 0 }, { 0 } },
		{ HAWK_T("^$"), HAWK_T(""), 0, HAWK_REX_NOTBOL, 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("^$"), HAWK_T(""), 0, HAWK_REX_NOTEOL, 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("^$"), HAWK_T(""), 0, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T(""), HAWK_T("a"), 0, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 1, 0, 1, { 1 }, { 0 } },
		{ HAWK_T("^a|b$"), HAWK_T("b"), 0, HAWK_REX_NOTBOL, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("^a|b$"), HAWK_T("b"), 0, HAWK_REX_NOTEOL, 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("^a|b$"), HAWK_T("a"), 0, HAWK_REX_NOTEOL, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("^a|a"), HAWK_T("a"), 0, HAWK_REX_NOTBOL, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("a$|a"), HAWK_T("a"), 0, HAWK_REX_NOTEOL, 0, 1, 1, { 0 }, { 1 } },
		{ HAWK_T("a"), HAWK_T("ba"), 0, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 1, 1, 1, { 1 }, { 1 } },
		{ HAWK_T("\\^a\\$"), HAWK_T("^a$"), 0, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 0, 3, 1, { 0 }, { 3 } },
		{ HAWK_T("a^b$c"), HAWK_T("a^b$c"), 1, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 0, 5, 1, { 0 }, { 5 } },
		{ HAWK_T("^(a)(b)$"), HAWK_T("ab"), 0, 0, 0, 2, 1, { 0, 0, 1 }, { 2, 1, 1 } },
		{ HAWK_T("^(a)(b)$"), HAWK_T("ab"), 0, HAWK_REX_NOTBOL, 0, 2, 0, { 0 }, { 0 } },
		{ HAWK_T("^(a)(b)$"), HAWK_T("ab"), 0, HAWK_REX_NOTEOL, 0, 2, 0, { 0 }, { 0 } },
		{ HAWK_T("(^a)|(b$)"), HAWK_T("b"), 0, HAWK_REX_NOTBOL, 0, 1, 1, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("(^a)|(b$)"), HAWK_T("a"), 0, HAWK_REX_NOTEOL, 0, 1, 1, { 0, 0, -1 }, { 1, 1, 0 } },
		{ HAWK_T("\\(^a\\)\\|\\(b$\\)"), HAWK_T("b"), 1, HAWK_REX_NOTBOL, 0, 1, 1, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("\\(^a\\)\\|\\(b$\\)"), HAWK_T("a"), 1, HAWK_REX_NOTEOL, 0, 1, 1, { 0, 0, -1 }, { 1, 1, 0 } },
		{ HAWK_T("(^)*"), HAWK_T("a"), 0, HAWK_REX_NOTBOL, 0, 1, 1, { 0, -1 }, { 0, 0 } },
		{ HAWK_T("(^)*"), HAWK_T("a"), 0, 0, 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("($)+"), HAWK_T(""), 0, HAWK_REX_NOTEOL, 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("()"), HAWK_T(""), 0, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 0, 0, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("^(a)\\1$"), HAWK_T("aa"), 0, 0, 0, 2, 1, { 0, 0 }, { 2, 1 } },
		{ HAWK_T("^(a)\\1$"), HAWK_T("aa"), 0, HAWK_REX_NOTEOL, 0, 2, 0, { 0 }, { 0 } },
		{ HAWK_T("$"), HAWK_T("a"), 0, 0, 1, 0, 1, { 1 }, { 0 } },
		{ HAWK_T("$"), HAWK_T("a"), 0, HAWK_REX_NOTEOL, 1, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("^"), HAWK_T("a"), 0, 0, 0, 0, 1, { 0 }, { 0 } },
		{ HAWK_T("^"), HAWK_T("a"), 0, HAWK_REX_NOTBOL, 0, 0, 0, { 0 }, { 0 } }
	};
	static const int flag_sequence[] =
	{
		0, HAWK_REX_NOTBOL, 0, HAWK_REX_NOTEOL, 0, HAWK_REX_NOTBOL | HAWK_REX_NOTEOL, 0
	};
	hawk_rex_t rex;
	hawk_rex_node_t* code;
	hawk_oocs_t input, window, matches[3], saved[3];
	hawk_oow_t i, j, count, nsub;
	int api, n, good, options;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		options = HAWK_REX_STRICT | (cases[i].basic? 0: HAWK_REX_EXTENDED);
		hawk_rex_setopt(&rex, options);
		code = hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern));
		if (!code) bail_out("cannot compile execution-flags case");
		nsub = hawk_rex_getnsub(&rex);
		input.ptr = (hawk_ooch_t*)cases[i].text;
		input.len = hawk_count_oocstr(input.ptr);
		window.ptr = input.ptr + cases[i].start;
		window.len = cases[i].length;
		for (api = 0; api < 4; api++)
		{
			count = (api & 1)? HAWK_COUNTOF(matches): 1;
			HAWK_MEMSET(matches, 0, HAWK_SIZEOF(matches));
			for (j = 0; j < HAWK_COUNTOF(matches); j++) { matches[j].ptr = input.ptr; matches[j].len = 123; }
			HAWK_MEMCPY(saved, matches, HAWK_SIZEOF(saved));
			switch (api)
			{
				case 0: n = hawk_rex_exec(&rex, &input, &window, matches, cases[i].flags); break;
				case 1: n = hawk_rex_execx(&rex, &input, &window, matches, count, cases[i].flags); break;
				case 2: n = hawk_rex_match(gem, code, 0, &input, &window, matches, cases[i].flags); break;
				default: n = hawk_rex_matchx(gem, code, 0, &input, &window, matches, count, cases[i].flags); break;
			}
			good = n == cases[i].found && rex.code == code && hawk_rex_getopt(&rex) == options;
			if (n == 1)
			{
				for (j = 0; j < count; j++)
				{
					if (j > nsub || cases[i].offsets[j] < 0)
					{
						if (matches[j].ptr || matches[j].len) good = 0;
					}
					else if (matches[j].ptr != input.ptr + cases[i].offsets[j] || matches[j].len != cases[i].lengths[j]) good = 0;
				}
				for (; j < HAWK_COUNTOF(matches); j++)
					if (matches[j].ptr != saved[j].ptr || matches[j].len != saved[j].len) good = 0;
			}
			else if (HAWK_MEMCMP(saved, matches, HAWK_SIZEOF(saved)) != 0) good = 0;
			OK(good, "execution flags suppress only the requested anchors, preserve output on no-match, and leave graph/options unchanged");
			if (!good) printf("# execution-flags case %lu API %d flags %d returned %d\n", (unsigned long)i, api, cases[i].flags, n);
		}
	}

	hawk_rex_setopt(&rex, HAWK_REX_EXTENDED);
	code = hawk_rex_comp(&rex, HAWK_T("^a$"), 3);
	if (!code) bail_out("cannot compile execution-flag reuse baseline");
	input.ptr = HAWK_T("a"); input.len = 1;
	for (i = 0; i < HAWK_COUNTOF(flag_sequence); i++)
	{
		OK(hawk_rex_execx(&rex, &input, &input, matches, 3, flag_sequence[i]) == (flag_sequence[i]? 0: 1) &&
			rex.code == code && hawk_rex_getopt(&rex) == HAWK_REX_EXTENDED,
			"execution flags are not retained between calls on a shared compiled graph");
	}
	HAWK_MEMCPY(saved, matches, HAWK_SIZEOF(saved));
	OK(hawk_rex_execx(&rex, &input, &input, matches, 3, 0x4000) == -1 &&
		hawk_rex_geterrnum(&rex) == HAWK_EINVAL && HAWK_MEMCMP(saved, matches, HAWK_SIZEOF(saved)) == 0,
		"unknown execution flags report EINVAL and leave capture output unchanged");
	OK(hawk_rex_matchx(gem, code, 0, &input, &input, matches, 3, -1) == -1 &&
		hawk_rex_geterrnum(&rex) == HAWK_EINVAL && HAWK_MEMCMP(saved, matches, HAWK_SIZEOF(saved)) == 0,
		"borrowed-graph matching rejects invalid execution flags without publishing output");
	OK(hawk_rex_exec(&rex, &input, &input, matches, 0) == 1 && matches[0].len == 1,
		"invalid flags do not damage the borrowed compiled graph");
	input.ptr = HAWK_T("A");
	hawk_rex_setopt(&rex, HAWK_REX_EXTENDED | HAWK_REX_IGNORECASE);
	OK(hawk_rex_exec(&rex, &input, &input, HAWK_NULL, 0) == 1 &&
		hawk_rex_exec(&rex, &input, &input, HAWK_NULL, HAWK_REX_NOTBOL) == 0,
		"NOTBOL works without output storage and remains independent of IGNORECASE");
	OK(hawk_rex_match(gem, code, HAWK_REX_IGNORECASE, &input, &input, HAWK_NULL, HAWK_REX_NOTEOL) == 0 &&
		hawk_rex_match(gem, code, HAWK_REX_IGNORECASE, &input, &input, HAWK_NULL, 0) == 1,
		"borrowed-graph NOTEOL execution does not change case handling or subsequent calls");
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
	hawk_rex_setopt(rex, HAWK_REX_STRICT | HAWK_REX_IGNORECASE | HAWK_REX_EXTENDED);
	OK(hawk_rex_getopt(rex) == (HAWK_REX_STRICT | HAWK_REX_IGNORECASE | HAWK_REX_EXTENDED), "regex options round-trip");
	if (!hawk_rex_comp(rex, HAWK_T("abc"), 3)) bail_out("cannot compile ownership baseline");
	code = hawk_rex_yield(rex);
	OK(code && !rex->code && code->u.s.gem == gem, "yield detaches code while retaining its allocator identity");
	hawk_rex_close(rex);
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match, 0) == 1 && match.ptr == str.ptr + 1 && match.len == 3,
		"convenience matcher borrows detached code");
	hawk_rex_init(&adopted, gem, code);
	hawk_rex_fini(&adopted);
	OK(a->live == baseline, "adopting and finalizing detached code releases it exactly once");

	code = hawk_rex_build(gem, HAWK_REX_EXTENDED, HAWK_T("bc$"), 3);
	if (!code) bail_out("cannot build standalone graph");
	sub.ptr = str.ptr + 2; sub.len = 2;
	OK(hawk_rex_match(gem, code, 0, &str, &sub, &match, 0) == 0,
		"end anchor uses the full string rather than the search-window end");
	hawk_rex_freecode(code);
	code = hawk_rex_build(gem, HAWK_REX_EXTENDED, HAWK_T("bc"), 2);
	if (!code) bail_out("cannot build search-window graph");
	OK(hawk_rex_match(gem, code, 0, &str, &sub, &match, 0) == 1 && match.ptr == sub.ptr && match.len == 2,
		"matching respects an explicit search window");
	hawk_rex_freecode(code);

	code = hawk_rex_build(gem, HAWK_REX_EXTENDED, pattern, HAWK_COUNTOF(pattern));
	if (!code) bail_out("cannot compile embedded NUL pattern");
	str.ptr = bytes; str.len = HAWK_COUNTOF(bytes);
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match, 0) == 1 && match.ptr == bytes + 1 && match.len == 3,
		"explicit-length patterns and inputs retain embedded NUL characters");
	hawk_rex_freecode(code);
	pattern[1] = (hawk_ooch_t)0xff;
	bytes[2] = (hawk_ooch_t)0xff;
	code = hawk_rex_build(gem, HAWK_REX_EXTENDED, pattern, HAWK_COUNTOF(pattern));
	if (!code) bail_out("cannot compile high-bit character pattern");
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match, 0) == 1 && match.ptr == bytes + 1 && match.len == 3,
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
	code = hawk_rex_build(gem, HAWK_REX_EXTENDED, pattern, hawk_count_oocstr(pattern));
	if (!code) bail_out("cannot measure regex compiler allocations");
	allocations = a->calls - before;
	hawk_rex_freecode(code);
	for (i = 1; i <= allocations; i++)
	{
		hawk_rex_init(&rex, gem, HAWK_NULL);
		hawk_rex_setopt(&rex, HAWK_REX_EXTENDED);
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

	code = hawk_rex_build(gem, HAWK_REX_EXTENDED, pattern, hawk_count_oocstr(pattern));
	if (!code) bail_out("cannot compile execution allocation baseline");
	before = a->calls;
	if (hawk_rex_match(gem, code, 0, &str, &str, &match, 0) != 1) bail_out("cannot measure regex executor allocations");
	allocations = a->calls - before;
	baseline = a->live;
	good = 1;
	for (i = 1; i <= allocations; i++)
	{
		match.ptr = HAWK_NULL; match.len = 123;
		a->fail_at = a->calls + i;
		n = hawk_rex_match(gem, code, 0, &str, &str, &match, 0);
		a->fail_at = 0;
		if (n != -1 || hawk_gem_geterrnum(gem) != HAWK_ENOMEM || a->live != baseline ||
		    match.ptr != HAWK_NULL || match.len != 123)
		{
			printf("# executor allocation failure %lu returned %d, live delta %ld\n", (unsigned long)i, n, (long)(a->live - baseline));
			good = 0;
		}
	}
	OK(good, "all executor allocation failures report errors without leaking groups or modifying output");
	OK(hawk_rex_match(gem, code, 0, &str, &str, &match, 0) == 1, "borrowed compiled code remains reusable after execution failure");
	hawk_rex_freecode(code);
	OK(a->live == outer, "failure-injection suite releases all regex allocations");
}

/* ASCII diagnostics work in byte, 16-bit, and 32-bit character builds. */
static void print_pattern (const hawk_ooch_t* text)
{
	hawk_oow_t i;
	unsigned int c;

	putchar('"');
	for (i = 0; text[i]; i++)
	{
		c = text[i];
		if (c == '\\' || c == '"') { putchar('\\'); putchar(c); }
		else if (c >= 32 && c < 127) putchar(c);
		else printf("\\x%02x", c);
	}
	putchar('"');
}

static void posix_diagnostic (const hawk_ooch_t* pattern, const hawk_ooch_t* text,
	int expected, hawk_oow_t offset, hawk_oow_t length, int result,
	const hawk_oocs_t* input, const hawk_oocs_t* match)
{
	printf("# pattern="); print_pattern(pattern);
	printf(" input="); print_pattern(text);
	printf(" expected=%d [%lu,%lu) actual=%d", expected,
		(unsigned long)offset, (unsigned long)(offset + length), result);
	if (result == 1)
		printf(" [%lu,%lu)", (unsigned long)(match->ptr - input->ptr),
			(unsigned long)(match->ptr - input->ptr + match->len));
	putchar('\n');
}

static void test_bre (hawk_gem_t* gem)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		const hawk_ooch_t* text;
		int options, found;
		hawk_oow_t nsub;
		hawk_ooi_t offsets[4];
		hawk_oow_t lengths[4];
	} cases[] =
	{
		{ HAWK_T("a{2}"), HAWK_T("a{2}"), 0, 1, 0, { 0 }, { 4 } },
		{ HAWK_T("(a)"), HAWK_T("(a)"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("a+?|(b){2}"), HAWK_T("a+?|(b){2}"), 0, 1, 0, { 0 }, { 10 } },
		{ HAWK_T("a|b"), HAWK_T("a|b"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("a\\|b"), HAWK_T("a|b"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("a\\|b"), HAWK_T("b"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("a\\|aa"), HAWK_T("aa"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("aa\\|a"), HAWK_T("aa"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("a\\|bb"), HAWK_T("abb"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("a\\|^b"), HAWK_T("xb"), 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("a\\|^b"), HAWK_T("b"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("a$\\|b"), HAWK_T("ax"), 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("a$\\|b"), HAWK_T("xa"), 0, 1, 0, { 1 }, { 1 } },
		{ HAWK_T("a$b\\|c^d"), HAWK_T("xa$b"), 0, 1, 0, { 1 }, { 3 } },
		{ HAWK_T("a\\|^*b"), HAWK_T("*b"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("a\\|"), HAWK_T("b"), 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("\\|a"), HAWK_T("a"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("\\|"), HAWK_T(""), 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("\\(a\\|ab\\)"), HAWK_T("ab"), 0, 1, 1, { 0, 0 }, { 2, 2 } },
		{ HAWK_T("\\(a\\|b\\)\\+"), HAWK_T("abba"), 0, 1, 1, { 0, 3 }, { 4, 1 } },
		{ HAWK_T("\\(a\\|\\)"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("\\(a\\)\\|\\(b\\)"), HAWK_T("b"), 0, 1, 2, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("\\(a\\|aa\\)\\1"), HAWK_T("aaaa"), 0, 1, 1, { 0, 0 }, { 4, 2 } },
		{ HAWK_T("\\(a\\)\\|\\(b\\)\\1"), HAWK_T("bb"), 0, 0, 2, { 0 }, { 0 } },
		{ HAWK_T("\\(a\\)\\|\\(b\\)\\2"), HAWK_T("bb"), 0, 1, 2, { 0, -1, 0 }, { 2, 0, 1 } },
		{ HAWK_T("[\\|]\\+"), HAWK_T("\\|"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("a\\x7cb"), HAWK_T("a|b"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("a\\\\|b"), HAWK_T("a\\|b"), 0, 1, 0, { 0 }, { 4 } },
		{ HAWK_T("a$\\\\|b"), HAWK_T("a$\\|b"), 0, 1, 0, { 0 }, { 5 } },
		{ HAWK_T("a\\+"), HAWK_T("aaa"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("a\\?"), HAWK_T(""), 0, 1, 0, { 0 }, { 0 } },
		{ HAWK_T("a\\?"), HAWK_T("aa"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("\\(ab\\)\\+"), HAWK_T("abab"), 0, 1, 1, { 0, 2 }, { 4, 2 } },
		{ HAWK_T("a\\{2\\}"), HAWK_T("zaaa"), 0, 1, 0, { 1 }, { 2 } },
		{ HAWK_T("a\\{1,3\\}"), HAWK_T("aaaa"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("a\\{2,\\}"), HAWK_T("aaaa"), 0, 1, 0, { 0 }, { 4 } },
		{ HAWK_T("a\\{0,2\\}"), HAWK_T("aaa"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("a\\{0\\}b"), HAWK_T("ab"), 0, 1, 0, { 1 }, { 1 } },
		{ HAWK_T("a\\{2\\}"), HAWK_T("a{2}"), 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("a\\{2\\}"), HAWK_T("a{2}"), HAWK_REX_NOBOUND, 1, 0, { 0 }, { 4 } },
		{ HAWK_T("\\(ab\\)\\{0\\}"), HAWK_T("ab"), 0, 1, 1, { 0, -1 }, { 0, 0 } },
		{ HAWK_T("a^b"), HAWK_T("a^b"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("a$b"), HAWK_T("a$b"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("^a"), HAWK_T("ba"), 0, 0, 0, { 0 }, { 0 } },
		{ HAWK_T("a$"), HAWK_T("aba"), 0, 1, 0, { 2 }, { 1 } },
		{ HAWK_T("^*a"), HAWK_T("*a"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("*a"), HAWK_T("*a"), 0, 1, 0, { 0 }, { 2 } },
		{ HAWK_T("\\(^a$\\)"), HAWK_T("a"), 0, 1, 1, { 0, 0 }, { 1, 1 } },
		{ HAWK_T("\\(a$\\)"), HAWK_T("a"), 0, 1, 1, { 0, 0 }, { 1, 1 } },
		{ HAWK_T("\\(a^b\\)"), HAWK_T("a^b"), 0, 1, 1, { 0, 0 }, { 3, 3 } },
		{ HAWK_T("\\(^\\)*"), HAWK_T("abc"), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("\\($\\)*"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("\\($\\)\\+"), HAWK_T("abc"), 0, 1, 1, { 3, 3 }, { 0, 0 } },
		{ HAWK_T("^^"), HAWK_T("^"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("$$"), HAWK_T("$"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("\\x28a\\x29"), HAWK_T("(a)"), 0, 1, 0, { 0 }, { 3 } },
		{ HAWK_T("\\x2b"), HAWK_T("+"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("[\\+?(){}|]*"), HAWK_T("\\+?(){}|"), 0, 1, 0, { 0 }, { 8 } },
		{ HAWK_T("[\\]"), HAWK_T("\\"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("\\(AB\\)\\1"), HAWK_T("abAB"), HAWK_REX_IGNORECASE, 1, 1, { 0, 0 }, { 4, 2 } },
		{ HAWK_T("\\\\(a)"), HAWK_T("\\(a)"), 0, 1, 0, { 0 }, { 4 } },
		{ HAWK_T(")"), HAWK_T(")"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("}"), HAWK_T("}"), 0, 1, 0, { 0 }, { 1 } },
		{ HAWK_T("\\(a\\)\\?\\(b\\)"), HAWK_T("b"), 0, 1, 2, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("\\(a\\)\\1"), HAWK_T("aa"), 0, 1, 1, { 0, 0 }, { 2, 1 } },
		{ HAWK_T("\\(a*\\)\\1"), HAWK_T("aaaa"), 0, 1, 1, { 0, 0 }, { 4, 2 } },
		{ HAWK_T("\\(\\)\\1*"), HAWK_T(""), 0, 1, 1, { 0, 0 }, { 0, 0 } },
		{ HAWK_T("\\(a\\?\\)\\+\\1"), HAWK_T("a"), 0, 1, 1, { 0, 1 }, { 1, 0 } },
		{ HAWK_T("\\(\\(a\\?\\)*\\)*\\2"), HAWK_T(""), 0, 1, 2, { 0, 0, 0 }, { 0, 0, 0 } },
		{ HAWK_T("\\(a\\)\\{2,3\\}"), HAWK_T("aaaa"), 0, 1, 1, { 0, 2 }, { 3, 1 } },
		{ HAWK_T("\\(a\\)\\{0\\}\\(b\\)"), HAWK_T("b"), 0, 1, 2, { 0, -1, 0 }, { 1, 0, 1 } },
		{ HAWK_T("\\(\\(a\\)\\2\\)"), HAWK_T("aa"), 0, 1, 2, { 0, 0, 0 }, { 2, 2, 1 } }
	};
	static const struct
	{
		const hawk_ooch_t* pattern;
		hawk_errnum_t error;
	} errors[] =
	{
		{ HAWK_T("\\("), HAWK_EREXPAREN },
		{ HAWK_T("\\)"), HAWK_EREXPAREN },
		{ HAWK_T("a\\)"), HAWK_EREXPAREN },
		{ HAWK_T("\\(a)"), HAWK_EREXPAREN },
		{ HAWK_T("a\\{2}"), HAWK_EREXBRACE },
		{ HAWK_T("a\\{2"), HAWK_EREXBRACE },
		{ HAWK_T("a\\{,2\\}"), HAWK_EREXBADBR },
		{ HAWK_T("a\\{2,1\\}"), HAWK_EREXBADBR },
		{ HAWK_T("a\\{999999999999999999999999999\\}"), HAWK_EREXBADBR },
		{ HAWK_T("\\1"), HAWK_EREXSUBREG },
		{ HAWK_T("\\(a\\)\\2"), HAWK_EREXSUBREG },
		{ HAWK_T("\\(a\\1\\)"), HAWK_EREXSUBREG },
		{ HAWK_T("a\\|\\)"), HAWK_EREXPAREN },
		{ HAWK_T("\\(a\\|b"), HAWK_EREXPAREN },
		{ HAWK_T("a\\|\\1"), HAWK_EREXSUBREG }
	};
	hawk_rex_t rex;
	hawk_rex_node_t* old;
	hawk_oocs_t input, matches[4], saved[4];
	hawk_oow_t i, j;
	int mode, n, good;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	OK(hawk_rex_getopt(&rex) == 0, "fresh regex objects default to BRE syntax");
	for (mode = 0; mode < 2; mode++)
	{
		for (i = 0; i < HAWK_COUNTOF(cases); i++)
		{
			hawk_rex_setopt(&rex, cases[i].options | (mode? HAWK_REX_STRICT: 0));
			if (!hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern)))
			{
				OK(0, "compile BRE matching case");
				printf("# BRE case %lu mode %d failed to compile\n", (unsigned long)i, mode);
				continue;
			}
			input.ptr = (hawk_ooch_t*)cases[i].text;
			input.len = hawk_count_oocstr(input.ptr);
			HAWK_MEMSET(matches, 0, HAWK_SIZEOF(matches));
			for (j = 0; j < HAWK_COUNTOF(matches); j++) { matches[j].ptr = input.ptr; matches[j].len = 123; }
			HAWK_MEMCPY(saved, matches, HAWK_SIZEOF(saved));
			n = hawk_rex_execx(&rex, &input, &input, matches, HAWK_COUNTOF(matches), 0);
			good = n == cases[i].found && hawk_rex_getnsub(&rex) == cases[i].nsub;
			if (n == 1)
			{
				for (j = 0; j < HAWK_COUNTOF(matches); j++)
				{
					if (j > cases[i].nsub || cases[i].offsets[j] < 0)
					{
						if (matches[j].ptr || matches[j].len) good = 0;
					}
					else if (matches[j].ptr != input.ptr + cases[i].offsets[j] || matches[j].len != cases[i].lengths[j]) good = 0;
				}
			}
			else if (HAWK_MEMCMP(saved, matches, HAWK_SIZEOF(saved)) != 0) good = 0;
			OK(good, "BRE literals, GNU quantifiers, anchors, intervals, and capture spans are correct");
			if (!good)
			{
				printf("# BRE case %lu mode %d ", (unsigned long)i, mode);
				print_pattern(cases[i].pattern);
				printf(" returned %d\n", n);
			}
		}
		hawk_rex_setopt(&rex, mode? HAWK_REX_STRICT: 0);
		old = hawk_rex_comp(&rex, HAWK_T("kept"), 4);
		if (!old) bail_out("cannot compile BRE error baseline");
		for (i = 0; i < HAWK_COUNTOF(errors); i++)
		{
			OK(!hawk_rex_comp(&rex, errors[i].pattern, hawk_count_oocstr(errors[i].pattern)) &&
				hawk_rex_geterrnum(&rex) == errors[i].error && rex.code == old && hawk_rex_getnsub(&rex) == 0,
				"BRE syntax errors preserve the previous graph and report the appropriate diagnostic");
		}
	}
	input.ptr = HAWK_T("abab"); input.len = 4;
	hawk_rex_setopt(&rex, 0);
	if (!hawk_rex_comp(&rex, HAWK_T("\\(ab\\)\\1"), 8)) bail_out("cannot compile BRE option-change baseline");
	hawk_rex_setopt(&rex, HAWK_REX_EXTENDED);
	OK(hawk_rex_execx(&rex, &input, &input, matches, 4, 0) == 1 && matches[0].len == 4 && matches[1].len == 2,
		"changing the dialect option after compilation does not reinterpret an existing BRE graph");
	hawk_rex_fini(&rex);
}

/* Fixed C-locale vectors cover ERE syntax and match selection.
 *
 * Matching regressions protect against two previously fixed state-loss bugs:
 * - comp_cand() merged candidates with equal node/input/atom occurrence,
 *   even when their enclosing group stacks had different occurrence counts.
 * - addcands() stopped when it revisited a GROUPEND without consuming input,
 *   even if empty iterations were still needed to satisfy the group's minimum.
 * The standard's whole-match rule chooses the earliest starting position,
 * then the longest match beginning there. Missing empty matches can therefore
 * be just as incorrect as choosing a shorter nonempty match. */
static void test_posix_cases (hawk_gem_t* gem)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		const hawk_ooch_t* text;
		int found;
		hawk_oow_t offset, length;
		const hawk_bch_t* name;
	} cases[] =
	{
#define ERE(p,t,f,o,n,description) { HAWK_T(p), HAWK_T(t), f, o, n, description }
		ERE("a", "baba", 1, 1, 1, "POSIX ERE literal uses the earliest start"),
		ERE("a|bb", "abb", 1, 0, 1, "POSIX leftmost start takes priority over a longer later match"),
		ERE("a|aa", "aa", 1, 0, 2, "POSIX alternation chooses the longest complete match"),
		ERE("aa|a", "aa", 1, 0, 2, "POSIX longest match is independent of alternative order"),
		ERE("a|ab", "ab", 1, 0, 2, "POSIX prefers a longer alternative over an earlier short alternative"),
		ERE("a*", "baaa", 1, 0, 0, "POSIX empty match at the start beats a later nonempty match"),
		ERE("a*", "aaa", 1, 0, 3, "POSIX repetition takes the longest match at the earliest start"),
		ERE("(a|ab)*", "abab", 1, 0, 4, "POSIX repeated alternation keeps the longer path"),
		ERE("(ab|a)*", "abab", 1, 0, 4, "POSIX repeated alternatives are order-independent"),
		ERE("(a|aa){2,3}", "aaaaaa", 1, 0, 6, "POSIX short-first bounded alternatives control case"),
		ERE("(a{1,2}){1,2}", "aaaa", 1, 0, 4, "POSIX bounded atom inside a bounded group control case"),
		ERE("(a*)(a*)", "aaaa", 1, 0, 4, "POSIX concatenation maximizes the entire match"),
		ERE("(a|aa)(a|aa)", "aaaa", 1, 0, 4, "POSIX multiple ambiguous groups maximize the entire match"),
		ERE("a.*b", "a1b2b", 1, 0, 5, "POSIX dot repetition extends to the last usable suffix"),
		ERE("a.*b|a", "a1b", 1, 0, 3, "POSIX shorter alternative cannot end the search prematurely"),
		ERE("a.b", "a\nb", 1, 0, 3, "POSIX dot matches newline without REG_NEWLINE"),
		ERE("[^a]+", "\nb", 1, 0, 2, "POSIX negated bracket matches newline without REG_NEWLINE"),
		ERE("^a", "ba", 0, 0, 0, "POSIX beginning anchor rejects an interior match"),
		ERE("a$", "aba", 1, 2, 1, "POSIX ending anchor refers to the input end"),
		ERE("^a$", "a\n", 0, 0, 0, "POSIX default end anchor does not ignore a final newline"),
		ERE("^$", "", 1, 0, 0, "POSIX anchored empty input"),
		ERE("a^", "a", 0, 0, 0, "POSIX ERE interior beginning anchor remains an anchor"),
		ERE("a$b", "ab", 0, 0, 0, "POSIX ERE interior ending anchor remains an anchor"),
		ERE("a?b", "b", 1, 0, 1, "POSIX zero-or-one atom"),
		ERE("a+b", "aaab", 1, 0, 4, "POSIX one-or-more atom"),
		ERE("a{2}", "zaaa", 1, 1, 2, "POSIX exact repetition"),
		ERE("a{1,3}", "aaaa", 1, 0, 3, "POSIX closed repetition interval"),
		ERE("a{2,}", "zaaaa", 1, 1, 4, "POSIX open repetition interval"),
		ERE("a{0,2}", "aaa", 1, 0, 2, "POSIX zero lower bound"),
		ERE("(ab){0}c", "abc", 1, 2, 1, "POSIX exact-zero group consumes no input"),
		ERE("[abc]+", "zabcc!", 1, 1, 4, "POSIX bracket character list"),
		ERE("[^abc]+", "abxyzac", 1, 2, 3, "POSIX negated bracket character list"),
		ERE("[a-c]+", "zabcc!", 1, 1, 4, "POSIX character range"),
		ERE("[]a]+", "]aa", 1, 0, 3, "POSIX leading closing bracket is literal"),
		ERE("[-a]+", "-aa", 1, 0, 3, "POSIX leading hyphen is literal"),
		ERE("[a-]+", "a--", 1, 0, 3, "POSIX trailing hyphen is literal"),
		ERE("[.a]+", ".aa", 1, 0, 3, "ordinary period in a bracket is not a collating-symbol prefix"),
		ERE("[=a]+", "=aa", 1, 0, 3, "ordinary equals sign in a bracket is not an equivalence-class prefix"),
		ERE("[[a]+", "[aa", 1, 0, 3, "ordinary opening bracket remains a literal bracket member"),
		ERE("\\[\\.a\\.\\]", "[.a.]", 1, 0, 5, "escaped collating-symbol-like text outside brackets is literal"),
		ERE("\\[=a=\\]", "[=a=]", 1, 0, 5, "escaped equivalence-class-like text outside brackets is literal"),
		ERE("[.^$*+?(){}|]+", ".^$*+?(){}|", 1, 0, 11, "POSIX metacharacters are literal inside brackets"),
		ERE("[[:alnum:]]+", "!?aB09_#", 1, 2, 4, "POSIX alnum class"),
		ERE("[[:alpha:]]+", "12aB_!", 1, 2, 2, "POSIX alpha class"),
		ERE("[[:blank:]]+", "x \t!", 1, 1, 2, "POSIX blank class"),
		ERE("[[:cntrl:]]+", "x\001\177y", 1, 1, 2, "POSIX cntrl class"),
		ERE("[[:digit:]]+", "a019z", 1, 1, 3, "POSIX digit class"),
		ERE("[[:graph:]]+", " \tAz09!~\n", 1, 2, 6, "POSIX graph class"),
		ERE("[[:lower:]]+", "AazZ", 1, 1, 2, "POSIX lower class"),
		ERE("[[:print:]]+", "\001Az 09!\177", 1, 1, 6, "POSIX print class"),
		ERE("[[:punct:]]+", "a!_~b", 1, 1, 3, "POSIX punct class"),
		ERE("[[:space:]]+", "x \t\n\r\v\fy", 1, 1, 6, "POSIX space class"),
		ERE("[[:upper:]]+", "aAZz", 1, 1, 2, "POSIX upper class"),
		ERE("[[:xdigit:]]+", "g09aAfFz", 1, 1, 6, "POSIX xdigit class"),
		ERE("[[:alpha:]0-9]+", "_a0B9!", 1, 1, 4, "POSIX bracket class and range union"),
		ERE("\\(a\\)", "x(a)", 1, 1, 3, "POSIX escaped parentheses are literal"),
		ERE("a\\+b", "a+b", 1, 0, 3, "POSIX escaped plus is literal"),
		ERE("a\\|b", "a|b", 1, 0, 3, "ERE escaped pipe remains literal"),
		ERE("a\\{2\\}", "a{2}", 1, 0, 4, "POSIX escaped braces are literal"),
		ERE("\\\\", "x\\", 1, 1, 1, "POSIX escaped backslash is literal"),
		ERE("(aa|a){2,3}", "aaaaaa", 1, 0, 6, "POSIX bounded alternatives retain the longest path"),
		ERE("(a{1,2}){2,3}", "aaaaaa", 1, 0, 6, "POSIX atom and group counters remain independent"),
		ERE("((a|b){1,2}){2}", "aaaa", 1, 0, 4, "POSIX nested intervals retain the longest path"),
		ERE("((a|b){1,2}){1,2}", "aaaa", 1, 0, 4, "POSIX nested optional intervals retain the longest path"),
		ERE("^((a|b){1,2}){2}$", "aaaa", 1, 0, 4, "POSIX anchored nested intervals do not lose a valid match"),
		ERE("((a|b){1,2}){2}b", "aaaab", 1, 0, 5, "POSIX failed merged path cannot move the match start"),
		ERE("(a?){2}", "aa", 1, 0, 2, "POSIX repeated optional group consumes both characters"),
		ERE("(a?){2,3}", "aaa", 1, 0, 3, "POSIX repeated optional group reaches its upper bound"),
		ERE("(a?){2}b", "aab", 1, 0, 3, "POSIX repeated optional group preserves the earliest start"),
		ERE("(a*){2}", "", 1, 0, 0, "POSIX repeated nullable group can satisfy its lower bound"),
		ERE("(a?){2}", "", 1, 0, 0, "POSIX optional group can repeat twice without consuming input"),
		ERE("(a*){2}", "ba", 1, 0, 0, "POSIX nullable group must not skip an earlier empty match"),
		ERE("[\\]", "\\", 1, 0, 1, "POSIX backslash inside a bracket is literal"),
		ERE("[^\\]", "\\", 0, 0, 0, "POSIX negated bracket excludes a literal backslash"),
		ERE("[^\\]", "a", 1, 0, 1, "POSIX negated backslash bracket accepts other characters"),
		ERE("[a\\]+", "xa\\aa!", 1, 1, 4, "POSIX backslash remains literal after another bracket member"),
		ERE("[\\n]+", "n\\", 1, 0, 2, "POSIX bracket backslash and escape letter are separate members"),
		ERE("[\\n]", "\n", 0, 0, 0, "POSIX bracket backslash does not introduce a newline escape"),
		ERE("[\\t]", "\t", 0, 0, 0, "POSIX bracket backslash does not introduce a tab escape"),
		ERE("[\\x41]", "A", 0, 0, 0, "POSIX bracket backslash does not introduce a hexadecimal escape"),
		ERE("[\\x41]+", "\\x41", 1, 0, 4, "POSIX bracket retains every character of hexadecimal-looking text"),
		ERE("[[:digit:]\\]+", "\\1", 1, 0, 2, "POSIX backslash remains literal after a character class"),
		ERE("[a-b\\]+", "a\\b", 1, 0, 3, "POSIX backslash remains literal after a range"),
		ERE("[]\\]+", "]\\", 1, 0, 2, "POSIX leading closing bracket and backslash are literal members"),
		ERE("[\\-]", "\\", 1, 0, 1, "POSIX backslash does not escape a trailing hyphen"),
		ERE("[\\^]", "^", 1, 0, 1, "POSIX caret after a backslash member does not negate the bracket"),
		ERE("[Z-\\]+", "Y[\\Z]", 1, 1, 3, "POSIX backslash can be a range endpoint"),
		ERE("[\\]\\t", "\\\t", 1, 0, 2, "escape processing resumes after the closing bracket"),
		ERE("\\[[\\]\\]", "[\\]", 1, 0, 3, "escaped brackets outside a bracket expression remain literal"),
#undef ERE
	};
	hawk_rex_t rex;
	hawk_oocs_t input, match;
	hawk_oow_t i;
	int n, good;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	hawk_rex_setopt(&rex, HAWK_REX_STRICT | HAWK_REX_EXTENDED);
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		input.ptr = (hawk_ooch_t*)cases[i].text;
		input.len = hawk_count_oocstr(input.ptr);
		match.ptr = HAWK_NULL; match.len = 123;
		n = hawk_rex_comp(&rex, cases[i].pattern, hawk_count_oocstr(cases[i].pattern))?
			hawk_rex_exec(&rex, &input, &input, &match, 0): -1;
		good = n == cases[i].found && (n == 1?
			match.ptr == input.ptr + cases[i].offset && match.len == cases[i].length:
			match.ptr == HAWK_NULL && match.len == 123);
		OK(good, cases[i].name);
		if (!good) posix_diagnostic(cases[i].pattern, cases[i].text,
			cases[i].found, cases[i].offset, cases[i].length, n, &input, &match);
	}
	hawk_rex_fini(&rex);
}

/* Independent reference for the generated subset. A row maps each possible
 * starting position to every possible ending position using a seven-bit set.
 * Concatenation composes relations, alternatives union them, and repetition
 * unions powers of a relation. There are no candidate queues, mutable group
 * counters, or GROUPEND shortcuts shared with the implementation being tested.
 *
 * This tiny model supports only the generated a/b expressions and single-digit
 * interval bounds. It is not a replacement for a general POSIX regex parser.
 * Its complete generated corpus was also compared against libc regexec in the
 * C locale. TRE is deliberately not the oracle: it disagrees with libc for
 * some of these otherwise unambiguous complete-match cases. */
typedef struct posix_relation_t posix_relation_t;
struct posix_relation_t
{
	unsigned int ends[7];
};

typedef struct posix_model_t posix_model_t;
struct posix_model_t
{
	const hawk_ooch_t* next;
	const hawk_ooch_t* text;
	hawk_oow_t length;
};

static void relation_identity (posix_relation_t* r, hawk_oow_t length)
{
	hawk_oow_t i;

	HAWK_MEMSET(r, 0, HAWK_SIZEOF(*r));
	for (i = 0; i <= length; i++) r->ends[i] = 1u << i;
}

static void relation_compose (const posix_relation_t* left, const posix_relation_t* right,
	posix_relation_t* out, hawk_oow_t length)
{
	posix_relation_t result = { { 0 } };
	hawk_oow_t i, j;

	for (i = 0; i <= length; i++)
		for (j = i; j <= length; j++)
			if (left->ends[i] & (1u << j)) result.ends[i] |= right->ends[j];
	*out = result;
}

static void relation_expression (posix_model_t* model, posix_relation_t* out);

static void relation_atom (posix_model_t* model, posix_relation_t* out)
{
	posix_relation_t power, result = { { 0 } };
	hawk_ooch_t c = *model->next++;
	hawk_oow_t i;
	int minimum = 0, maximum = 0, count;

	HAWK_MEMSET(out, 0, HAWK_SIZEOF(*out));
	if (c == HAWK_T('('))
	{
		relation_expression(model, out);
		if (*model->next++ != HAWK_T(')')) bail_out("invalid group in generated POSIX model");
	}
	else if (c == HAWK_T('a') || c == HAWK_T('b'))
	{
		for (i = 0; i < model->length; i++) if (model->text[i] == c) out->ends[i] = 1u << (i + 1);
	}
	else bail_out("unsupported atom in generated POSIX model");

	c = *model->next;
	if (c == HAWK_T('*')) { minimum = 0; maximum = -1; model->next++; }
	else if (c == HAWK_T('+')) { minimum = 1; maximum = -1; model->next++; }
	else if (c == HAWK_T('?')) { minimum = 0; maximum = 1; model->next++; }
	else if (c == HAWK_T('{'))
	{
		model->next++;
		if (*model->next < HAWK_T('0') || *model->next > HAWK_T('9'))
			bail_out("unsupported minimum in generated POSIX model");
		minimum = *model->next++ - HAWK_T('0');
		maximum = minimum;
		if (*model->next == HAWK_T(','))
		{
			model->next++;
			maximum = -1;
			if (*model->next >= HAWK_T('0') && *model->next <= HAWK_T('9'))
				maximum = *model->next++ - HAWK_T('0');
		}
		if (*model->next++ != HAWK_T('}')) bail_out("invalid interval in generated POSIX model");
	}
	else return;

	/* In this acyclic position domain, at most length consuming iterations
	 * are possible. Empty iterations may satisfy the minimum, but cannot add
	 * new endpoints beyond minimum + length iterations. */
	if (maximum < 0) maximum = minimum + (int)model->length;
	relation_identity(&power, model->length);
	for (count = 0; count <= maximum; count++)
	{
		if (count >= minimum)
			for (i = 0; i <= model->length; i++) result.ends[i] |= power.ends[i];
		if (count < maximum) relation_compose(&power, out, &power, model->length);
	}
	*out = result;
}

static void relation_branch (posix_model_t* model, posix_relation_t* out)
{
	posix_relation_t right;

	relation_identity(out, model->length);
	while (*model->next && *model->next != HAWK_T(')') && *model->next != HAWK_T('|'))
	{
		relation_atom(model, &right);
		relation_compose(out, &right, out, model->length);
	}
}

static void relation_expression (posix_model_t* model, posix_relation_t* out)
{
	posix_relation_t right;
	hawk_oow_t i;

	relation_branch(model, out);
	while (*model->next == HAWK_T('|'))
	{
		model->next++;
		relation_branch(model, &right);
		for (i = 0; i <= model->length; i++) out->ends[i] |= right.ends[i];
	}
}

static int posix_reference (const hawk_ooch_t* pattern, const hawk_ooch_t* text,
	hawk_oow_t length, hawk_oow_t* offset, hawk_oow_t* match_length)
{
	posix_model_t model;
	posix_relation_t relation;
	hawk_oow_t i, end;

	if (length > 6) bail_out("POSIX model input exceeds its six-character limit");
	model.next = pattern; model.text = text; model.length = length;
	relation_expression(&model, &relation);
	if (*model.next) bail_out("unexpected suffix in generated POSIX model");
	for (i = 0; i <= length; i++)
	{
		if (!relation.ends[i]) continue;
		for (end = length; !(relation.ends[i] & (1u << end)); end--) /* find longest */;
		*offset = i; *match_length = end - i;
		return 1;
	}
	*offset = *match_length = 0;
	return 0;
}

/* Check complete-match offsets with and without capture tracking. The
 * independent model has no capture semantics; fixed vectors check those.
 * No external regex implementation is needed. */
static void compare_posix_pattern_mode (hawk_gem_t* gem, const hawk_ooch_t* pattern,
	const hawk_ooch_t* model_pattern, int options, hawk_oow_t* comparisons, hawk_oow_t* differences)
{
	hawk_rex_t rex;
	hawk_oocs_t input, match, captures[9];
	hawk_ooch_t text[7];
	hawk_oow_t len, bits, i, offset, length;
	int n, cn, expected, good = 1, equal;

	hawk_rex_init(&rex, gem, HAWK_NULL);
	hawk_rex_setopt(&rex, HAWK_REX_STRICT | options);
	if (!hawk_rex_comp(&rex, pattern, hawk_count_oocstr(pattern)))
	{
		OK(0, "generated regular expression compiles in its selected dialect");
		printf("# pattern="); print_pattern(pattern); putchar('\n');
		goto done;
	}
	input.ptr = text;
	for (len = 0; len <= 6; len++)
	{
		input.len = len;
		for (bits = 0; bits < ((hawk_oow_t)1 << len); bits++)
		{
			for (i = 0; i < len; i++) text[i] = ((bits >> (len - i - 1)) & 1)? HAWK_T('b'): HAWK_T('a');
			text[len] = 0;
			expected = posix_reference(model_pattern, text, len, &offset, &length);
			match.ptr = HAWK_NULL; match.len = 123;
			n = hawk_rex_exec(&rex, &input, &input, &match, 0);
			equal = n == expected && (n == 1?
				match.ptr == input.ptr + offset && match.len == length:
				match.ptr == HAWK_NULL && match.len == 123);
			/* Capturing must not change which whole match is selected. */
			HAWK_MEMSET(captures, 0, HAWK_SIZEOF(captures));
			cn = hawk_rex_execx(&rex, &input, &input, captures, HAWK_COUNTOF(captures), 0);
			if (cn != expected || (cn == 1 &&
			    (captures[0].ptr != input.ptr + offset || captures[0].len != length)))
			{
				if (equal) { n = cn; match = captures[0]; }
				equal = 0;
			}
			(*comparisons)++;
			if (!equal)
			{
				(*differences)++;
				/* Report the shortest failing input for each pattern. */
				if (good) posix_diagnostic(pattern, text, expected, offset, length, n, &input, &match);
				good = 0;
			}
		}
	}
	OK(good, "generated BRE/ERE agrees with independent model with and without capture tracking");
done:
	hawk_rex_fini(&rex);
}

/* The generated alphabet has no escapes or brackets. Translate its ERE
 * operators into BRE spellings, using GNU \\+, \\?, and \\| extensions. */
static void compare_posix_pattern (hawk_gem_t* gem, const hawk_ooch_t* pattern,
	hawk_oow_t* comparisons, hawk_oow_t* differences)
{
	hawk_ooch_t basic[256], c;
	hawk_oow_t i, len = 0;

	compare_posix_pattern_mode(gem, pattern, pattern, HAWK_REX_EXTENDED, comparisons, differences);
	for (i = 0; pattern[i]; i++)
	{
		c = pattern[i];
		if (len + 2 >= HAWK_COUNTOF(basic)) bail_out("generated BRE exceeds its buffer");
		if (c == HAWK_T('(') || c == HAWK_T(')') || c == HAWK_T('{') ||
		    c == HAWK_T('}') || c == HAWK_T('+') || c == HAWK_T('?') ||
		    c == HAWK_T('|')) basic[len++] = HAWK_T('\\');
		basic[len++] = c;
	}
	basic[len] = 0;
	compare_posix_pattern_mode(gem, basic, pattern, 0, comparisons, differences);
}

#if defined(HAWK_OOCH_IS_UCH)
#	define format_pattern hawk_gem_fmttoucstr
#else
#	define format_pattern hawk_gem_fmttobcstr
#endif

static void test_posix_sweep (hawk_gem_t* gem)
{
	static const hawk_ooch_t* atoms[] =
	{
		HAWK_T("a"), HAWK_T("b"), HAWK_T("a|aa"), HAWK_T("aa|a"),
		HAWK_T("a|ab"), HAWK_T("ab|a"), HAWK_T("a|b"), HAWK_T("a*"),
		HAWK_T("a?"), HAWK_T("a*b"), HAWK_T("ab*"), HAWK_T("a?b"),
		HAWK_T("(a|aa)b"), HAWK_T("(a|aa)*"), HAWK_T("(a|b)*")
	};
	static const hawk_ooch_t* repetitions[] =
	{
		HAWK_T("*"), HAWK_T("+"), HAWK_T("?"), HAWK_T("{2}"), HAWK_T("{1,2}"), HAWK_T("{2,3}")
	};
	static const hawk_ooch_t* prefixes[] = { HAWK_T(""), HAWK_T(""), HAWK_T("a"), HAWK_T(""), HAWK_T("b") };
	static const hawk_ooch_t* suffixes[] = { HAWK_T(""), HAWK_T("b"), HAWK_T(""), HAWK_T("ab"), HAWK_T("b") };
	static const hawk_ooch_t* nonnull[] =
	{
		HAWK_T("a|aa"), HAWK_T("aa|a"), HAWK_T("a|ab"), HAWK_T("ab|a"),
		HAWK_T("ab|aba"), HAWK_T("aba|ab"), HAWK_T("(a|aa){1,2}"),
		HAWK_T("(aa|a){1,2}"), HAWK_T("a{1,2}"), HAWK_T("(a|b){1,2}"),
		HAWK_T("(a|aa)+"), HAWK_T("(aa|a)+"), HAWK_T("(a|b)+"),
		HAWK_T("ab|ba"), HAWK_T("a{1,3}b")
	};
	hawk_ooch_t pattern[128];
	hawk_oow_t a, b, q, w, mode, patterns = 0, comparisons = 0, differences = 0;

	for (mode = 0; mode < 2; mode++)
	{
		for (a = 0; a < (mode? HAWK_COUNTOF(nonnull): HAWK_COUNTOF(atoms)); a++)
		{
			for (q = 0; q < HAWK_COUNTOF(repetitions); q++)
			{
				if (mode && q == 2) continue;
				for (w = 0; w < HAWK_COUNTOF(prefixes); w++)
				{
					format_pattern(gem, pattern, HAWK_COUNTOF(pattern), HAWK_T("%js(%js)%js%js"),
						prefixes[w], mode? nonnull[a]: atoms[a], repetitions[q], suffixes[w]);
					compare_posix_pattern(gem, pattern, &comparisons, &differences);
					patterns++;
				}
			}
		}
	}
	for (a = 0; a < 12; a++)
	{
		for (b = 0; b < 12; b++)
		{
			format_pattern(gem, pattern, HAWK_COUNTOF(pattern), HAWK_T("(%js)*(%js)*"), atoms[a], atoms[b]);
			compare_posix_pattern(gem, pattern, &comparisons, &differences); patterns++;
			format_pattern(gem, pattern, HAWK_COUNTOF(pattern), HAWK_T("(%js|%js){2}"), atoms[a], atoms[b]);
			compare_posix_pattern(gem, pattern, &comparisons, &differences); patterns++;
		}
	}
	for (a = 0; a < HAWK_COUNTOF(nonnull); a++)
	{
		for (b = 0; b < HAWK_COUNTOF(nonnull); b++)
		{
			format_pattern(gem, pattern, HAWK_COUNTOF(pattern), HAWK_T("(%js)+(%js)+"), nonnull[a], nonnull[b]);
			compare_posix_pattern(gem, pattern, &comparisons, &differences); patterns++;
		}
	}
	printf("# POSIX sweep: %lu pattern cases, %lu comparisons, %lu mismatches\n",
		(unsigned long)patterns, (unsigned long)comparisons, (unsigned long)differences);
}

int main (int argc, char* argv[])
{
	allocator_t a = { 0, 0, 0 };
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &a };
	hawk_t* hawk;
	hawk_gem_t* gem;
	hawk_oow_t baseline;
	int i, sweep = 0;

	no_plan();
	for (i = 1; i < argc; i++)
	{
		if (hawk_comp_bcstr(argv[i], "--compare-posix", 0) == 0) sweep = 1;
		else bail_out("usage: t-033 [--compare-posix]");
	}
	setlocale(LC_ALL, "C");
	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) { bail_out("cannot open test interpreter"); return 1; }
	gem = hawk_getgem(hawk);
	baseline = a.live;
	test_matches(gem);
	test_bre(gem);
	test_errors(gem);
	test_unsupported_syntax(gem, &a);
	test_captures(gem);
	test_exec_flags(gem);
	test_capture_failures(gem, &a);
	test_lifetimes(gem, &a);
	test_posix_cases(gem);
	if (sweep) test_posix_sweep(gem);
	test_failures(gem, &a);
	OK(a.live == baseline, "regex engine leaves the embedding gem's other allocations intact");
	hawk_close(hawk);
	OK(a.live == 0, "custom allocator has no live allocations after closing its owner");
	return exit_status();
}
