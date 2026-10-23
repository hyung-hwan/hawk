/* Bounded, iterative value rendering and output-buffer ownership. */
#include "hawk-prv.h"
#include "tap.h"

typedef struct allocator_t allocator_t;
struct allocator_t
{
	hawk_oow_t live, calls, fail_at, largest;
	int active;
};

static int allocation_fails (allocator_t* a, hawk_oow_t size)
{
	if (!a->active) return 0;
	if (size > a->largest) a->largest = size;
	return ++a->calls == a->fail_at;
}

static void* allocate (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();
	void* ptr;

	if (allocation_fails(a, size)) return HAWK_NULL;
	ptr = real->alloc(real, size);
	if (ptr) a->live++;
	return ptr;
}

static void* reallocate (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();
	int is_new = ptr == HAWK_NULL;
	void* result;

	if (allocation_fails(a, size)) return HAWK_NULL;
	result = real->realloc(real, ptr, size);
	if (result && is_new) a->live++;
	return result;
}

static void deallocate (hawk_mmgr_t* mmgr, void* ptr)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	if (ptr) a->live--;
	real->free(real, ptr);
}

static int equals (hawk_rtx_t* rtx, hawk_val_t* val, const hawk_rtx_valrepr_opt_t* opt, const hawk_ooch_t* expected)
{
	hawk_rtx_valtostr_out_t out;
	int result;

	out.type = HAWK_RTX_VALTOSTR_CPLDUP;
	if (hawk_rtx_valtorepr(rtx, val, &out, opt) <= -1) return 0;
	result = hawk_comp_oocstr(out.u.cpldup.ptr, expected, 0) == 0;
	hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
	return result;
}

int main (void)
{
	allocator_t a = { 0, 0, 0, 0, 0 };
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &a };
	hawk_t* hawk;
	hawk_rtx_t* rtx;
	hawk_val_t* arr;
	hawk_val_t* str;
	hawk_val_t* chain[1024];
	hawk_rtx_valtostr_out_t out;
	hawk_rtx_valrepr_opt_t opt = { 0, 0, 0 };
	hawk_ooecs_t text;
	hawk_ooch_t buf[128];
	hawk_ooch_t long_text[10000];
	hawk_oow_t i, j, baseline, count;
	int n, good;

	no_plan();
	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return 1; }
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("repr"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) { bail_out("cannot open runtime"); return 1; }
	arr = hawk_rtx_makearrval(rtx, 0);
	if (!arr) { bail_out("cannot create array"); return 1; }
	hawk_rtx_refupval(rtx, arr);
	str = hawk_rtx_makestrvalwithoocstr(rtx, HAWK_T("x"));
	if (!str || !hawk_rtx_setarrvalfld(rtx, arr, 2, str) ||
	    !hawk_rtx_setarrvalfld(rtx, arr, 1, hawk_rtx_makeintval(rtx, 7)))
	{
		bail_out("cannot populate array");
		return 1;
	}
	OK(equals(rtx, arr, HAWK_NULL, HAWK_T("@[7, \"x\"]")), "C API renders dense arrays");
	OK(equals(rtx, str, HAWK_NULL, HAWK_T("\"x\"")), "explicit scalar representations quote strings");
#if defined(HAWK_OOCH_IS_UCH)
	{
		hawk_ooch_t unicode[] = { 0xe9, 0x3a9 };
		hawk_val_t* value;

		value = hawk_rtx_makestrvalwithoochars(rtx, unicode, HAWK_COUNTOF(unicode));
		if (!value) return 1;
		hawk_rtx_refupval(rtx, value);
		OK(equals(rtx, value, HAWK_NULL, HAWK_T("\"\\xe9\\u03a9\"")),
			"wide characters render as ASCII escapes without encoding loss");
		hawk_rtx_refdownval(rtx, value);
	}
#endif
	out.type = HAWK_RTX_VALTOSTR_CPLDUP;
	n = hawk_rtx_valtostr(rtx, arr, &out);
	OK(n == 0 && hawk_comp_oocstr(out.u.cpldup.ptr, HAWK_T("<ARRAY>"), 0) == 0,
		"implicit C conversion remains compatible");
	if (n == 0) hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
	out.type = HAWK_RTX_VALTOSTR_CPLDUP | HAWK_RTX_VALTOSTR_REPR;
	n = hawk_rtx_valtostr(rtx, arr, &out);
	OK(n == 0 && hawk_comp_oocstr(out.u.cpldup.ptr, HAWK_T("@[7, \"x\"]"), 0) == 0,
		"VALTOSTR_REPR selects the shared renderer");
	if (n == 0) hawk_rtx_freemem(rtx, out.u.cpldup.ptr);

	out.type = HAWK_RTX_VALTOSTR_CPL;
	out.u.cpl.ptr = buf;
	out.u.cpl.len = HAWK_COUNTOF(buf);
	n = hawk_rtx_valtorepr(rtx, arr, &out, HAWK_NULL);
	OK(n == 0 && out.u.cpl.ptr == buf && out.u.cpl.len == 9 &&
	   hawk_comp_oocstr(buf, HAWK_T("@[7, \"x\"]"), 0) == 0,
		"CPL copies generated text into the caller buffer without a dangling pointer");
	out.type = HAWK_RTX_VALTOSTR_CPLCPY;
	out.u.cplcpy.ptr = buf;
	out.u.cplcpy.len = 4;
	buf[0] = '!';
	OK(hawk_rtx_valtorepr(rtx, arr, &out, HAWK_NULL) <= -1 &&
	   hawk_rtx_geterrnum(rtx) == HAWK_EBUFFULL && out.u.cplcpy.len == 10 && buf[0] == '!',
		"short destination reports required capacity and preserves its contents");
	out.u.cplcpy.len = HAWK_COUNTOF(buf);
	OK(hawk_rtx_valtorepr(rtx, arr, &out, HAWK_NULL) == 0 && out.u.cplcpy.len == 9,
		"CPLCPY returns the length excluding NUL");
	if (hawk_ooecs_init(&text, hawk_rtx_getgem(rtx), 0) <= -1) return 1;
	hawk_ooecs_cat(&text, HAWK_T("old"));
	out.type = HAWK_RTX_VALTOSTR_STRP;
	out.u.strp = &text;
	OK(hawk_rtx_valtorepr(rtx, arr, &out, HAWK_NULL) == 0 &&
	   hawk_comp_oocstr(HAWK_OOECS_PTR(&text), HAWK_T("@[7, \"x\"]"), 0) == 0,
		"STRP replaces the destination");
	out.type = HAWK_RTX_VALTOSTR_STRPCAT;
	out.u.strpcat = &text;
	opt.max_chars = 16;
	OK(hawk_rtx_valtorepr(rtx, arr, &out, &opt) == 0 &&
	   hawk_comp_oocstr(HAWK_OOECS_PTR(&text), HAWK_T("@[7, \"x\"]@[<truncated>]"), 0) == 0,
		"STRPCAT limits the new representation independently of its prefix");
	hawk_ooecs_fini(&text);
	opt.max_chars = 15;
	out.type = HAWK_RTX_VALTOSTR_CPLDUP;
	OK(hawk_rtx_valtorepr(rtx, arr, &out, &opt) <= -1 && hawk_rtx_geterrnum(rtx) == HAWK_EINVAL,
		"invalid limits fail before allocation");
	opt.max_chars = HAWK_TYPE_MAX(hawk_oow_t);
	OK(hawk_rtx_valtorepr(rtx, arr, &out, &opt) <= -1 && hawk_rtx_geterrnum(rtx) == HAWK_EINVAL,
		"overflowing output capacity is rejected before allocation");
	opt.max_chars = 0;
	opt.max_steps = 1;
	OK(equals(rtx, arr, &opt, HAWK_T("@[<truncated>]")), "tiny work budget still closes containers");
	opt.max_steps = 0;
	good = 1;
	for (i = 16; i <= 64; i++)
	{
		for (j = 1; j <= 16; j++)
		{
			opt.max_chars = i;
			opt.max_steps = j;
			n = hawk_rtx_valtorepr(rtx, arr, &out, &opt);
			if (n <= -1) { good = 0; continue; }
			if (out.u.cpldup.len > i ||
			    (hawk_comp_oocstr(out.u.cpldup.ptr, HAWK_T("@[7, \"x\"]"), 0) != 0 &&
			     hawk_comp_oocstr(out.u.cpldup.ptr, HAWK_T("@[<truncated>]"), 0) != 0 &&
			     hawk_comp_oocstr(out.u.cpldup.ptr, HAWK_T("@[7, <truncated>]"), 0) != 0)) good = 0;
			hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
		}
	}
	OK(good, "size/work boundary combinations never leave a partial member or exceed limits");
	opt.max_chars = opt.max_steps = 0;

	/* Keep a separate reference to every node, so cleanup is also nonrecursive. */
	for (i = 0; i < HAWK_COUNTOF(chain); i++)
	{
		chain[i] = hawk_rtx_makearrval(rtx, 0);
		if (!chain[i]) { bail_out("cannot create deep array"); return 1; }
		hawk_rtx_refupval(rtx, chain[i]);
		if (i > 0 && !hawk_rtx_setarrvalfld(rtx, chain[i - 1], 1, chain[i])) return 1;
	}
	opt.max_depth = 1;
	OK(equals(rtx, chain[0], &opt, HAWK_T("@[<depth-limit>]")), "caller can reduce maximum depth");
	opt.max_depth = 0;
	a.active = 1;
	a.calls = a.largest = 0;
	n = hawk_rtx_valtorepr(rtx, chain[0], &out, &opt);
	a.active = 0;
	OK(n == 0 && hawk_find_oocstr_in_oochars(out.u.cpldup.ptr, out.u.cpldup.len, HAWK_T("<depth-limit>"), 0),
		"1024 nested arrays use bounded heap traversal");
	OK(a.largest <= (HAWK_RTX_VALREPR_DEFAULT_MAX_CHARS + 1) * HAWK_SIZEOF(hawk_ooch_t) && a.calls < 16,
		"default traversal allocates only small bounded buffers");
	if (n == 0) hawk_rtx_freemem(rtx, out.u.cpldup.ptr);

	opt.max_depth = 2048;
	opt.max_steps = 2000000;
	n = hawk_rtx_valtorepr(rtx, chain[0], &out, &opt);
	OK(n == 0 && out.u.cpldup.len == HAWK_COUNTOF(chain) * 3,
		"explicitly raised budgets render all 1024 levels without C recursion");
	if (n == 0) hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
	opt.max_depth = opt.max_steps = 0;

	/* Exercise every allocation in both frame and text growth. */
	for (i = 0; i < HAWK_COUNTOF(long_text); i++) long_text[i] = 'x';
	str = hawk_rtx_makestrvalwithoochars(rtx, long_text, HAWK_COUNTOF(long_text));
	if (!str || !hawk_rtx_setarrvalfld(rtx, chain[20], 2, str)) return 1;
	baseline = a.live;
	a.calls = a.largest = 0;
	a.active = 1;
	n = hawk_rtx_valtorepr(rtx, chain[0], &out, &opt);
	a.active = 0;
	count = a.calls;
	OK(n == 0 && out.u.cpldup.len <= HAWK_RTX_VALREPR_DEFAULT_MAX_CHARS &&
	   hawk_find_oocstr_in_oochars(out.u.cpldup.ptr, out.u.cpldup.len, HAWK_T("<truncated>"), 0),
		"large leaves cannot exceed the output budget");
	if (n == 0) hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
	good = 1;
	for (i = 1; i <= count; i++)
	{
		a.active = 1;
		a.calls = 0;
		a.fail_at = i;
		n = hawk_rtx_valtorepr(rtx, chain[0], &out, &opt);
		a.active = 0;
		if (n >= 0)
		{
			hawk_rtx_freemem(rtx, out.u.cpldup.ptr);
			good = 0;
		}
		else if (hawk_rtx_geterrnum(rtx) != HAWK_ENOMEM) good = 0;
		if (a.live != baseline) good = 0;
	}
	a.fail_at = 0;
	OK(good, "renderer allocation failures report ENOMEM without leaking");
	for (i = 0; i < HAWK_COUNTOF(chain); i++) hawk_rtx_refdownval(rtx, chain[i]);
	hawk_rtx_refdownval(rtx, arr);
	hawk_rtx_close(rtx);
	hawk_close(hawk);
	OK(a.live == 0, "all runtime and rendering allocations are released");
	return exit_status();
}
