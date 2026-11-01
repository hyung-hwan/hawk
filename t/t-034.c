/* Shared rex primitives: engine selection, offsets, ownership, and integration. */
#include "hawk-prv.h"
#include <hawk-sed.h>
#include <hawk-rex.h>
#include "tap.h"

#if defined(HAVE_PTHREAD)
#include <pthread.h>
#endif

/* Test the actual wrapper even when loadable Sed modules are disabled. */
#define hawk_mod_sed hawk_test_mod_sed
#include "../mod/mod-sed.c"
#undef hawk_mod_sed

typedef struct observer_t observer_t;
struct observer_t
{
	int builds, frees, live, matches;
	int bch, uch, slices;
	int fail_build, fail_match, bad_match;
};

static int observed_build (
	hawk_gem_t* gem, void* ctx, const hawk_ooch_t* pattern,
	hawk_oow_t len, int flags, void** code, hawk_oow_t* nsub)
{
	observer_t* o = ctx;
	const hawk_rex_prm_t* rgx = hawk_rex_getrgxprm();
	int n;

	o->builds++;
	if (o->fail_build == o->builds)
	{
		hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_ENOMEM, "rex build fixture");
		return -1;
	}
	n = rgx->build(gem, rgx->ctx, pattern, len, flags, code, nsub);
	if (n == 0) o->live++;
	return n;
}

static int observed_match (
	hawk_gem_t* gem, void* ctx, const void* code,
	const hawk_rex_input_t* input, int flags,
	hawk_rex_match_t* matches, hawk_oow_t count)
{
	observer_t* o = ctx;
	const hawk_rex_prm_t* rgx = hawk_rex_getrgxprm();

	o->matches++;
	if (input->type == HAWK_REX_INPUT_BCH) o->bch++;
	else o->uch++;
	if (input->start > 0) o->slices++;
	if (o->fail_match)
	{
		hawk_gem_seterrbfmt(gem, HAWK_NULL, HAWK_EIOERR, "rex match fixture");
		return -1;
	}
	if (o->bad_match)
	{
		if (o->bad_match == 2) return 2;
		if (count)
		{
			matches[0].start = input->start;
			matches[0].end = input->end + 1;
		}
		return 1;
	}
	return rgx->match(gem, rgx->ctx, code, input, flags, matches, count);
}

static void observed_free (void* ctx, void* code)
{
	observer_t* o = ctx;
	const hawk_rex_prm_t* rgx = hawk_rex_getrgxprm();

	o->frees++;
	o->live--;
	rgx->free(rgx->ctx, code);
}

static void make_prm (hawk_rex_prm_t* prm, observer_t* observer)
{
	prm->build = observed_build;
	prm->match = observed_match;
	prm->free = observed_free;
	prm->ctx = observer;
}

static void test_interface (void)
{
	hawk_t* hawk = hawk_openstd(0, HAWK_NULL);
	hawk_t* other = hawk_openstd(0, HAWK_NULL);
	hawk_gem_t* gem;
	hawk_gem_t* execgem;
	hawk_prm_t owner_prm, saved;
	hawk_rex_prm_t prm, bad;
	observer_t o;
	void* code = HAWK_NULL;
	void* icode = HAWK_NULL;
	hawk_oow_t nsub;
	hawk_rex_input_t input;
	hawk_rex_match_t matches[12];
	hawk_bch_t bytes[] = { '!', 'a', 'b', 0, 'a', 'b', '!' };
	hawk_uch_t wide[] = { '!', 'a', 'b', 0, 'a', 'b', '!' };
	int kind, n;

	if (!hawk || !other) bail_out("cannot open rex test owners");
	gem = hawk_getgem(hawk);
	execgem = hawk_getgem(other);
	HAWK_MEMSET(&o, 0, HAWK_SIZEOF(o));
	make_prm(&prm, &o);
	hawk_getprm(hawk, &owner_prm);
	OK(!owner_prm.rex.build && !owner_prm.rex.match && !owner_prm.rex.free, "standard owner defaults to native TRE");
	bad = prm;
	bad.free = HAWK_NULL;
	owner_prm.rex = bad;
	OK(hawk_setprm(hawk, &owner_prm) == -1 && hawk_geterrnum(hawk) == HAWK_EINVAL, "incomplete primitive table rejected");
	hawk_getprm(hawk, &saved);
	OK(!saved.rex.build, "failed registration retains previous primitives");
	owner_prm.rex = prm;
	OK(hawk_setprm(hawk, &owner_prm) == 0, "complete rex table installed");
	owner_prm.rex.match = HAWK_NULL;
	hawk_getprm(hawk, &saved);
	OK(saved.rex.match == observed_match, "owner copies the table rather than borrowing it");
	OK(hawk_buildrex(hawk, HAWK_T("(a)?(b)"), 7, &code, &icode) == 0 && o.live == 2, "Hawk builds both case-sensitive and ignorecase custom handles");
	hawk_freerex(hawk, code, icode);
	OK(o.live == 0 && o.frees == 2, "Hawk releases custom handles through their binding");
	code = icode = HAWK_NULL;
	o.fail_build = o.builds + 2;
	OK(hawk_buildrex(hawk, HAWK_T("ab"), 2, &code, &icode) == -1 && !code && !icode && o.live == 0,
		"second build failure frees first handle without publishing partial outputs");
	o.fail_build = 0;
	OK(hawk_rex_build(gem, &prm, HAWK_T("(a)?(b)"), 7, HAWK_REX_EXTENDED, &code, &nsub) == 0 && nsub == 2,
		"shared builder reports capture count");
	for (kind = 0; kind < 2; kind++)
	{
		input.type = kind == 0? HAWK_REX_INPUT_BCH: HAWK_REX_INPUT_UCH;
		input.ptr = kind == 0? (const void*)bytes: (const void*)wide;
		input.len = 7;
		input.start = 2;
		input.end = 6;
		n = hawk_rex_match(execgem, &prm, code, &input, 0, matches, HAWK_COUNTOF(matches));
		OK(n == 1 && matches[0].start == 2 && matches[0].end == 3 &&
			matches[1].start == HAWK_REX_NOMATCH && matches[1].end == HAWK_REX_NOMATCH &&
			matches[2].start == 2 && matches[2].end == 3 && matches[11].start == HAWK_REX_NOMATCH,
			kind == 0? "BCH slice offsets, unmatched capture, and surplus slots": "UCH slice offsets, unmatched capture, and surplus slots");
		input.start = 3;
		n = hawk_rex_match(execgem, &prm, code, &input, 0, matches, 3);
		OK(n == 1 && matches[0].start == 4 && matches[0].end == 6 && matches[1].start == 4,
			kind == 0? "BCH search continues past embedded NUL": "UCH search continues past embedded NUL");
		OK(hawk_rex_match(execgem, &prm, code, &input, 0, HAWK_NULL, 0) == 1, "boolean-only matching needs no capture output");
	}
	OK(bytes[3] == 0 && bytes[6] == '!' && wide[3] == 0 && wide[6] == '!', "adapter never modifies input storage");
	o.bad_match = 1;
	OK(hawk_rex_match(execgem, &prm, code, &input, 0, matches, 1) == -1 && hawk_geterrnum(other) == HAWK_EINVAL,
		"out-of-range custom offsets rejected before constructing slices");
	o.bad_match = 0;
	o.bad_match = 2;
	OK(hawk_rex_match(execgem, &prm, code, &input, 0, matches, 1) == -1,
		"unknown custom match return value rejected");
	o.bad_match = 0;
	o.fail_match = 1;
	hawk_gem_seterrnum(gem, HAWK_NULL, HAWK_ENOERR);
	OK(hawk_rex_match(execgem, &prm, code, &input, 0, matches, 1) == -1 &&
		hawk_geterrnum(other) == HAWK_EIOERR && hawk_geterrnum(hawk) == HAWK_ENOERR,
		"execution failure reports to execution gem, not compilation gem");
	o.fail_match = 0;
	input.end = input.len + 1;
	n = o.matches;
	OK(hawk_rex_match(execgem, &prm, code, &input, 0, matches, 1) == -1 && o.matches == n, "invalid input interval rejected before callback");
	prm.free(prm.ctx, code);
	code = HAWK_NULL;
	OK(hawk_rex_build(gem, &prm, HAWK_T("a"), 1, HAWK_REX_NEWLINE, &code, &nsub) == -1 &&
		hawk_geterrnum(hawk) == HAWK_ENOSUP && !code, "unsupported engine semantics explicitly rejected");
	OK(o.live == 0, "all custom handles released");
	hawk_close(other);
	hawk_close(hawk);
}

static void test_errors (void)
{
	static const hawk_bch_t source[] = "function test() { return \"ab\" ~ /ab/; }";
	hawk_t* hawk = hawk_openstd(0, HAWK_NULL);
	hawk_rtx_t* rtx;
	hawk_sed_t* sed = hawk_sed_openstd(0, HAWK_NULL);
	hawk_prm_t owner_prm;
	hawk_rex_prm_t prm, saved, bad;
	hawk_parsestd_t input[2];
	hawk_oocs_t str, out;
	observer_t o;
	hawk_val_t* value;
	int n;

	if (!hawk || !sed) bail_out("cannot open error test owners");
	HAWK_MEMSET(&o, 0, HAWK_SIZEOF(o));
	make_prm(&prm, &o);
	bad = prm;
	bad.match = HAWK_NULL;
	OK(hawk_sed_setopt(sed, HAWK_SED_REXPRM, &bad) == -1 &&
		hawk_sed_geterrnum(sed) == HAWK_EINVAL, "Sed rejects incomplete primitives");
	OK(hawk_sed_setopt(sed, HAWK_SED_REXPRM, &prm) == 0 &&
		hawk_sed_getopt(sed, HAWK_SED_REXPRM, &saved) == 0 && saved.match == prm.match && saved.ctx == &o,
		"Sed copies and exposes its primitive binding");
	OK(hawk_sed_compstdoocstr(sed, HAWK_T("s/[z-a]/x/")) == -1 &&
		hawk_sed_geterrnum(sed) == HAWK_EREXRANGE && o.live == 0, "Sed propagates custom compilation errors without leaks");
	if (hawk_sed_compstdoocstr(sed, HAWK_T("s/ab/X/")) <= -1) bail_out("cannot compile Sed error fixture");
	o.fail_match = 1;
	str.ptr = HAWK_T("ab\n");
	str.len = 3;
	out.ptr = HAWK_NULL;
	n = hawk_sed_execstdxstr(sed, &str, &out, HAWK_NULL);
	OK(n == -1 && hawk_sed_geterrnum(sed) == HAWK_EIOERR && hawk_sed_geterrloc(sed)->line > 0,
		"Sed propagates custom execution errors with source location");
	if (out.ptr) hawk_sed_freemem(sed, out.ptr);
	hawk_sed_close(sed);
	o.fail_match = 0;
	hawk_getprm(hawk, &owner_prm);
	owner_prm.rex = prm;
	if (hawk_setprm(hawk, &owner_prm) <= -1) bail_out("cannot set error fixture primitives");
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = HAWK_COUNTOF(source) - 1;
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1) bail_out("cannot parse Hawk error fixture");
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("rex-errors"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) bail_out("cannot open error fixture runtime");
	o.fail_match = 1;
	hawk_seterrnum(hawk, HAWK_NULL, HAWK_ENOERR);
	value = hawk_rtx_callwithbcstr(rtx, "test", HAWK_NULL, 0);
	OK(!value && hawk_rtx_geterrnum(rtx) == HAWK_EIOERR && hawk_geterrnum(hawk) == HAWK_ENOERR,
		"custom error reaches runtime handler without overwriting owner diagnostic");
	if (value) hawk_rtx_refdownval(rtx, value);
	hawk_rtx_close(rtx);
	hawk_close(hawk);
	OK(o.live == 0, "error paths release every custom compiled handle");
}

static void test_adapter (void)
{
	static const struct
	{
		const hawk_ooch_t* pattern;
		const hawk_bch_t* text;
		int cflags, eflags, result;
		hawk_oow_t start, end, so, eo;
	} cases[] =
	{
		{ HAWK_T("^ab$"), "ab", HAWK_REX_EXTENDED, 0, 1, 0, 2, 0, 2 },
		{ HAWK_T("^ab$"), "ab", HAWK_REX_EXTENDED, HAWK_REX_NOTBOL, 0, 0, 2, 0, 0 },
		{ HAWK_T("^ab$"), "ab", HAWK_REX_EXTENDED, HAWK_REX_NOTEOL, 0, 0, 2, 0, 0 },
		{ HAWK_T("^ab"), "!ab", HAWK_REX_EXTENDED, 0, 0, 1, 3, 0, 0 },
		{ HAWK_T("ab$"), "ab!", HAWK_REX_EXTENDED, 0, 0, 0, 2, 0, 0 },
		{ HAWK_T("\\(ab\\)\\1"), "!abab!", 0, 0, 1, 1, 5, 1, 5 },
		{ HAWK_T("(ab)\\1"), "!aBAB!", HAWK_REX_EXTENDED | HAWK_REX_IGNORECASE, 0, 1, 1, 5, 1, 5 },
		{ HAWK_T("a{2}"), "aa", HAWK_REX_EXTENDED, 0, 1, 0, 2, 0, 2 },
		{ HAWK_T("a{2}"), "a{2}", HAWK_REX_EXTENDED | HAWK_REX_NOBOUND, 0, 1, 0, 4, 0, 4 },
		{ HAWK_T(""), "", HAWK_REX_EXTENDED, 0, 1, 0, 0, 0, 0 },
		{ HAWK_T("\xff"), "!\xff!", HAWK_REX_EXTENDED, 0, 1, 0, 3, 1, 2 }
	};
	hawk_t* hawk = hawk_openstd(0, HAWK_NULL);
	hawk_gem_t* gem;
	const hawk_rex_prm_t* prm = hawk_rex_getrgxprm();
	void* code;
	hawk_oow_t i, nsub;
	hawk_rex_input_t input;
	hawk_rex_match_t matches[2];
	hawk_ooch_t nul_pattern[] = { 'a', 0, 'b' };
	hawk_bch_t nul_text[] = { '!', 'a', 0, 'b', '!' };
	hawk_uch_t nonbyte[] = { 0x100 };
	int n;

	if (!hawk) bail_out("cannot open adapter test owner");
	gem = hawk_getgem(hawk);
	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		code = HAWK_NULL;
		n = hawk_rex_build(gem, prm, cases[i].pattern, hawk_count_oocstr(cases[i].pattern), cases[i].cflags, &code, &nsub);
		input.type = HAWK_REX_INPUT_BCH;
		input.ptr = cases[i].text;
		input.len = hawk_count_bcstr(cases[i].text);
		input.start = cases[i].start;
		input.end = cases[i].end;
		if (n == 0) n = hawk_rex_match(gem, prm, code, &input, cases[i].eflags, matches, 2);
		OK(n == cases[i].result && (n != 1 || (matches[0].start == cases[i].so && matches[0].end == cases[i].eo)),
			"RGX adapter honors dialect, flags, input width, and subject boundaries");
		if (code) prm->free(prm->ctx, code);
	}
	code = HAWK_NULL;
	n = hawk_rex_build(gem, prm, nul_pattern, 3, HAWK_REX_EXTENDED, &code, &nsub);
	input.type = HAWK_REX_INPUT_BCH;
	input.ptr = nul_text;
	input.len = input.end = 5;
	input.start = 0;
	if (n == 0) n = hawk_rex_match(gem, prm, code, &input, 0, matches, 1);
	OK(n == 1 && matches[0].start == 1 && matches[0].end == 4, "explicit-length pattern and subject preserve embedded NUL");
	if (code) prm->free(prm->ctx, code);
	code = HAWK_NULL;
	n = hawk_rex_build(gem, prm, HAWK_T("."), 1, HAWK_REX_EXTENDED, &code, &nsub);
	input.type = HAWK_REX_INPUT_UCH;
	input.ptr = nonbyte;
	input.len = input.end = 1;
	if (n == 0) n = hawk_rex_match(gem, prm, code, &input, 0, matches, 1);
#if defined(HAWK_OOCH_IS_UCH)
	OK(n == 1 && matches[0].start == 0 && matches[0].end == 1, "wide adapter matches UCH values above 255 without narrowing");
#else
	OK(n == -1 && hawk_gem_geterrnum(gem) == HAWK_ENOSUP, "byte adapter explicitly rejects unrepresentable UCH input");
#endif
	if (code) prm->free(prm->ctx, code);
	hawk_close(hawk);
}

typedef struct allocator_t allocator_t;
struct allocator_t
{
	hawk_oow_t calls, fail_at, live;
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

static void test_allocations (void)
{
	static const hawk_ooch_t pattern[] = HAWK_T("(a)(b)(c)(d)(e)(f)(g)(h)(i)(j)(k)");
	allocator_t a = { 0, 0, 0 };
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &a };
	hawk_t* owner = hawk_openstd(0, HAWK_NULL);
	hawk_t* executor = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	const hawk_rex_prm_t* prm = hawk_rex_getrgxprm();
	hawk_rex_input_t input;
	hawk_rex_match_t matches[13];
	void* code = HAWK_NULL;
	hawk_oow_t nsub, calls, baseline, i;
	hawk_gem_t* gem;
	int n, good = 1;
	hawk_bch_t bytes[] = "abcdefghijk";
	hawk_uch_t wide[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k' };

	if (!owner || !executor) bail_out("cannot open allocation test owners");
	gem = hawk_getgem(executor);
	baseline = a.live;
	n = hawk_rex_build(hawk_getgem(owner), prm, pattern, HAWK_COUNTOF(pattern) - 1, HAWK_REX_EXTENDED, &code, &nsub);
	if (n <= -1) bail_out("cannot compile allocation test pattern");
	/* Select the opposite width to exercise conversion and >10 captures. */
#if defined(HAWK_OOCH_IS_UCH)
	input.type = HAWK_REX_INPUT_BCH;
	input.ptr = bytes;
	(void)wide;
#else
	input.type = HAWK_REX_INPUT_UCH;
	input.ptr = wide;
	(void)bytes;
#endif
	input.len = input.end = 11;
	input.start = 0;
	a.calls = 0;
	n = hawk_rex_match(gem, prm, code, &input, 0, matches, HAWK_COUNTOF(matches));
	calls = a.calls;
	OK(n == 1 && nsub == 11 && matches[11].start == 10 && matches[11].end == 11 &&
		matches[12].start == HAWK_REX_NOMATCH && a.live == baseline, "opposite-width conversion and capture overflow storage are released on success");
	for (i = 1; i <= calls; i++)
	{
		a.calls = 0;
		a.fail_at = i;
		n = hawk_rex_match(gem, prm, code, &input, 0, matches, HAWK_COUNTOF(matches));
		if (n != -1 || hawk_gem_geterrnum(gem) != HAWK_ENOMEM || a.live != baseline) good = 0;
	}
	a.fail_at = 0;
	OK(good && calls > 1, "each execution allocation failure releases conversion and capture scratch");
	prm->free(prm->ctx, code);
	/* Also fail every build allocation, including the adapter's RGX object. */
	a.calls = 0;
	n = hawk_rex_build(gem, prm, pattern, HAWK_COUNTOF(pattern) - 1, HAWK_REX_EXTENDED, &code, &nsub);
	calls = a.calls;
	if (n == 0) prm->free(prm->ctx, code);
	else good = 0;
	for (i = 1; i <= calls; i++)
	{
		a.calls = 0;
		a.fail_at = i;
		code = HAWK_NULL;
		n = hawk_rex_build(gem, prm, pattern, HAWK_COUNTOF(pattern) - 1, HAWK_REX_EXTENDED, &code, &nsub);
		if (n != -1 || code || a.live != baseline) good = 0;
		if (n == 0) prm->free(prm->ctx, code);
	}
	a.fail_at = 0;
	OK(good, "each build allocation failure cleans up without publishing a handle");
	hawk_close(executor);
	hawk_close(owner);
	OK(a.live == 0, "separate execution allocator is empty after owner shutdown");
}

static void test_hawk (int custom)
{
	static const hawk_bch_t source[] =
		"function test() { @local s,a,p;"
		"if (!(\"ab\" ~ /a|ab/) || \"x\" ~ /a+/) return 1;"
		"p=\"(a)?(b)\"; if (match(\"b\",p,a)!=1 || (1 in a) || a[2]!=\"b\") return 2;"
		"if (match(\"zzab\",/(a)(b)/,a)!=3 || a[0]!=\"ab\" || a[1]!=\"a\" || a[2]!=\"b\") return 3;"
		"if (match(@b\"zzab\",/(a)(b)/,a)!=3 || a[1]!=@b\"a\" || a[2]!=@b\"b\") return 4;"
		"IGNORECASE=1; if (!(\"AB\" ~ /ab/)) return 5; IGNORECASE=0;"
		"s=\"ab ab\"; if (gsub(/ab/,\"X\",s)!=2 || s!=\"X X\") return 6;"
		"s=\"ab\"; if (gsub(/^/,\"X\",s)!=1 || s!=\"Xab\") return 7;"
		"s=@b\"ab ab\"; if (gsub(/ab/,@b\"X\",s)!=2 || s!=@b\"X X\") return 8;"
		"if (split(\"one,,two;three\",a,/[,;]+/)!=3 || a[2]!=\"two\") return 9;"
		"FS=\"[,;]+\"; $0=\"one,,two;three\"; if (NF!=3 || $2!=\"two\") return 10;"
		"RS=\"::+\"; RS=\"--+\";"
		"if (sed::process(\"s/ab/X/g\",\"ab ab\\n\",s,sed::STR_TO_STR)!=0 || s!=\"X X\\n\") return 11;"
		"return 0; }";
	hawk_t* hawk = hawk_openstd(0, HAWK_NULL);
	hawk_rtx_t* rtx[2] = { HAWK_NULL, HAWK_NULL };
	hawk_parsestd_t input[2];
	hawk_prm_t prm;
	observer_t o;
	hawk_val_t* value;
	hawk_int_t number = -1;
	int i, good = 1;

	if (!hawk) bail_out("cannot open Hawk integration owner");
	if (hawk_addstaticmodwithbcstr(hawk, "sed", hawk_test_mod_sed) <= -1) bail_out("cannot register Sed test module");
	HAWK_MEMSET(&o, 0, HAWK_SIZEOF(o));
	if (custom)
	{
		hawk_getprm(hawk, &prm);
		make_prm(&prm.rex, &o);
		if (hawk_setprm(hawk, &prm) <= -1) good = 0;
	}
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = HAWK_COUNTOF(source) - 1;
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1)
	{
		fprintf(stderr, "Hawk parse: %s\n", hawk_geterrbmsg(hawk));
		good = 0;
	}
	for (i = 0; good && i < 2; i++)
	{
		rtx[i] = hawk_rtx_openstd(hawk, 0, HAWK_T("rex-test"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
		if (!rtx[i]) good = 0;
	}
	/* Keep both contexts live, and reuse the same immutable literal handles. */
	for (i = 0; good && i < 4; i++)
	{
		value = hawk_rtx_callwithbcstr(rtx[i % 2], "test", HAWK_NULL, 0);
		if (!value) good = 0;
		else
		{
			good = hawk_rtx_valtoint(rtx[i % 2], value, &number) == 0 && number == 0;
			hawk_rtx_refdownval(rtx[i % 2], value);
		}
		if (!good) fprintf(stderr, "Hawk rex=%d: test result=%ld, error=%s\n", custom, (long)number, hawk_rtx_geterrbmsg(rtx[i % 2]));
	}
	for (i = 0; i < 2; i++) if (rtx[i]) hawk_rtx_close(rtx[i]);
	hawk_close(hawk);
	OK(good, custom? "Hawk expressions, captures, MBS, substitutions, split, FS/RS, IGNORECASE, and shared literals with RGX":
		"same Hawk integration cases retain native TRE behavior");
	if (custom)
	{
		OK(o.matches > 0 && o.bch > 0 && o.slices > 0, "interpreter dispatches byte subjects and repeated search slices");
		OK(o.live == 0 && o.builds == o.frees, "Hawk frees literal and runtime custom handles exactly once");
	}
}

static void test_sed (int custom)
{
	static const struct
	{
		const hawk_ooch_t* script;
		const hawk_ooch_t* input;
		const hawk_ooch_t* output;
		int trait;
	} cases[] =
	{
		{ HAWK_T("s/\\(ab\\)/[\\1]/g"), HAWK_T("ab ab\n"), HAWK_T("[ab] [ab]\n"), 0 },
		{ HAWK_T("s/(a|ab)/[\\1]/g"), HAWK_T("ab ab\n"), HAWK_T("[ab] [ab]\n"), HAWK_SED_EXTENDEDREX },
		{ HAWK_T("s/^/X/g"), HAWK_T("ab\n"), HAWK_T("Xab\n"), 0 },
		{ HAWK_T("s/ab/X/gi"), HAWK_T("AB ab\n"), HAWK_T("X X\n"), 0 },
		{ HAWK_T("/ab/s//X/g"), HAWK_T("ab ab\n"), HAWK_T("X X\n"), 0 },
		{ HAWK_T("s/(a)?(b)/\\2/g"), HAWK_T("b ab\n"), HAWK_T("b b\n"), HAWK_SED_EXTENDEDREX }
	};
	hawk_sed_t* sed;
	hawk_rex_prm_t prm;
	observer_t o;
	hawk_oocs_t input, output;
	hawk_oow_t i;
	int good;

	for (i = 0; i < HAWK_COUNTOF(cases); i++)
	{
		HAWK_MEMSET(&o, 0, HAWK_SIZEOF(o));
		sed = hawk_sed_openstd(0, HAWK_NULL);
		if (!sed) bail_out("cannot open Sed integration owner");
		make_prm(&prm, &o);
		good = hawk_sed_setopt(sed, HAWK_SED_TRAIT, &cases[i].trait) == 0;
		if (custom) good = good && hawk_sed_setopt(sed, HAWK_SED_REXPRM, &prm) == 0;
		good = good && hawk_sed_compstdoocstr(sed, cases[i].script) == 0;
		input.ptr = (hawk_ooch_t*)cases[i].input;
		input.len = hawk_count_oocstr(input.ptr);
		output.ptr = HAWK_NULL;
		if (good) good = hawk_sed_execstdxstr(sed, &input, &output, HAWK_NULL) == 0;
		if (good) good = output.len == hawk_count_oocstr(cases[i].output) &&
			hawk_comp_oochars(output.ptr, output.len, cases[i].output, output.len, 0) == 0;
		if (!good) fprintf(stderr, "Sed rex=%d case=%lu: %s\n", custom, (unsigned long)i, hawk_sed_geterrbmsg(sed));
		if (output.ptr) hawk_sed_freemem(sed, output.ptr);
		hawk_sed_close(sed);
		OK(good && (!custom || (o.live == 0 && o.builds == o.frees && o.matches > 0)),
			custom? "Sed BRE/ERE, captures, repeated substitution, anchors, ignorecase, and previous regex with RGX":
			"same Sed integration case retains native TRE behavior");
	}
}

static hawk_ooi_t record_input (
	hawk_rtx_t* rtx, hawk_rio_cmd_t cmd, hawk_rio_arg_t* arg,
	void* data, hawk_oow_t count)
{
	static const hawk_ooch_t text[] = HAWK_T("one,alpha::two;beta:::three,gamma");
	const hawk_ooch_t* ptr;
	hawk_oow_t i;

	switch (cmd)
	{
		case HAWK_RIO_CMD_OPEN:
			arg->handle = (void*)text;
			return 1;
		case HAWK_RIO_CMD_READ:
			ptr = arg->handle;
			/* Deliberately split separators across successive I/O reads. */
			if (count > 2) count = 2;
			for (i = 0; i < count && ptr[i]; i++) ((hawk_ooch_t*)data)[i] = ptr[i];
			arg->handle = (void*)(ptr + i);
			return i;
		default:
			return 0;
	}
}

static void test_records (int custom)
{
	static const hawk_bch_t source[] =
		"BEGIN { RS=\"::+\"; FS=\"[,;]+\"; n=0; }"
		"{ if (NF!=2) exit 1; n++; } END { exit(n==3?0:1); }";
	hawk_t* hawk = hawk_openstd(0, HAWK_NULL);
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_prm_t prm;
	hawk_parsestd_t input[2];
	hawk_rio_cbs_t rio;
	hawk_val_t* value;
	hawk_int_t number = -1;
	int good = 0;

	if (!hawk) bail_out("cannot open record test owner");
	if (custom)
	{
		hawk_getprm(hawk, &prm);
		prm.rex = *hawk_rex_getrgxprm();
		if (hawk_setprm(hawk, &prm) <= -1) goto done;
	}
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = HAWK_COUNTOF(source) - 1;
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1) goto done;
	HAWK_MEMSET(&rio, 0, HAWK_SIZEOF(rio));
	rio.console = record_input;
	rtx = hawk_rtx_open(hawk, 0, &rio);
	if (!rtx) goto done;
	value = hawk_rtx_loop(rtx);
	if (value)
	{
		good = hawk_rtx_valtoint(rtx, value, &number) == 0 && number == 0;
		hawk_rtx_refdownval(rtx, value);
	}

done:
	if (!good) fprintf(stderr, "record reader rex=%d: %s\n", custom, rtx? hawk_rtx_geterrbmsg(rtx): hawk_geterrbmsg(hawk));
	if (rtx) hawk_rtx_close(rtx);
	hawk_close(hawk);
	OK(good, custom? "RGX handles regex RS and FS across split input buffers": "native TRE retains regex RS and FS input handling");
}

#if defined(HAVE_PTHREAD)
struct match_job_t
{
	hawk_gem_t* gem;
	const void* code;
	const hawk_bch_t* text;
	hawk_oow_t start;
	int good;
};

static void* match_thread (void* data)
{
	struct match_job_t* job = data;
	const hawk_rex_prm_t* prm = hawk_rex_getrgxprm();
	hawk_rex_input_t input;
	hawk_rex_match_t matches[3];
	int i;

	input.type = HAWK_REX_INPUT_BCH;
	input.ptr = job->text;
	input.len = input.end = hawk_count_bcstr(job->text);
	input.start = 0;
	job->good = 1;
	for (i = 0; i < 200; i++)
	{
		if (hawk_rex_match(job->gem, prm, job->code, &input, 0, matches, 3) != 1 ||
		    matches[0].start != job->start || matches[0].end != 4 ||
		    matches[1].start != job->start || matches[1].end != 3 ||
		    matches[2].start != 3 || matches[2].end != 4)
		{
			job->good = 0;
			break;
		}
	}
	return HAWK_NULL;
}
#endif

static void test_threads (void)
{
#if defined(HAVE_PTHREAD)
	hawk_t* owners[3];
	struct match_job_t jobs[2];
	pthread_t threads[2];
	const hawk_rex_prm_t* prm = hawk_rex_getrgxprm();
	void* code;
	hawk_oow_t nsub;
	int i, started = 0, good = 1;

	for (i = 0; i < 3; i++)
	{
		owners[i] = hawk_openstd(0, HAWK_NULL);
		if (!owners[i]) bail_out("cannot open threaded test owner");
	}
	if (hawk_rex_build(hawk_getgem(owners[0]), prm, HAWK_T("(a+)(b)"), 7, HAWK_REX_EXTENDED, &code, &nsub) <= -1)
		bail_out("cannot compile threaded test pattern");
	for (i = 0; i < 2; i++)
	{
		jobs[i].gem = hawk_getgem(owners[i + 1]);
		jobs[i].code = code;
		jobs[i].text = i == 0? "aaab": "xaabx";
		jobs[i].start = i;
		jobs[i].good = 0;
		if (pthread_create(&threads[i], HAWK_NULL, match_thread, &jobs[i]) != 0)
		{
			good = 0;
			break;
		}
		started++;
	}
	for (i = 0; i < started; i++)
	{
		if (pthread_join(threads[i], HAWK_NULL) != 0 || !jobs[i].good) good = 0;
	}
	prm->free(prm->ctx, code);
	for (i = 0; i < 3; i++) hawk_close(owners[i]);
	OK(good, "two threads reuse one immutable RGX handle with independent execution gems");
#else
	skip("pthread concurrency test is unavailable", 1);
#endif
}

int main (void)
{
	no_plan();
	test_interface();
	test_errors();
	test_adapter();
	test_allocations();
	test_hawk(0);
	test_hawk(1);
	test_records(0);
	test_records(1);
	test_sed(0);
	test_sed(1);
	test_threads();
	return exit_status();
}
