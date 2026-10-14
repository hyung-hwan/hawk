/* HTTP module host callbacks, validation, and per-runtime binding lifecycle. */
#include "../mod/hawk-http.h"
#include <hawk-utl.h>
#include <stdlib.h>
#include <string.h>
#include "tap.h"

/* Exercise the actual module even when optional modules are disabled. */
#define hawk_mod_http hawk_test_mod_http
#include "../mod/mod-http.c"
#undef hawk_mod_http

#define AUX_ID_EXTRA 9

typedef struct host_t host_t;
struct host_t
{
	int calls;
	int closed;
	int status;
	int fail;
	int early_commit;
	const hawk_ooch_t* path;
	int committed;
};

static int host_impl (hawk_rtx_t* rtx, hawk_oow_t cmd, void* data, void* ptr)
{
	host_t* host = ptr;
	const hawk_http_arg_t* arg = data;
	hawk_val_t* value;
	hawk_val_t* array;
	hawk_val_t* text;

	if (host->committed && (cmd == HAWK_HTTP_SETSTATUS || cmd == HAWK_HTTP_SETHEADER ||
	    cmd == HAWK_HTTP_ADDHEADER || cmd == HAWK_HTTP_DELHEADER))
	{
		hawk_rtx_seterrnum(rtx, HAWK_NULL, HAWK_EPERM);
		return -1;
	}
	host->calls++;
	if (host->early_commit) host->committed = 1;
	if (host->fail) return -1; /* Core supplies a diagnostic when host omitted it. */
	switch (cmd)
	{
		case HAWK_HTTP_GETMETHOD:
			value = hawk_rtx_makestrvalwithoocstr(rtx, HAWK_T("POST"));
			break;
		case HAWK_HTTP_GETPATH:
			value = hawk_rtx_makestrvalwithoocstr(rtx, host->path);
			break;
		case HAWK_HTTP_GETHEADER:
			value = hawk_rtx_makestrvalwithoocstr(rtx, arg->index == 2? HAWK_T("second"): HAWK_T(""));
			break;
		case HAWK_HTTP_GETHEADERS:
			value = hawk_rtx_makemapval(rtx);
			if (!value) return -1;
			hawk_rtx_refupval(rtx, value);
			array = hawk_rtx_makearrval(rtx, 2);
			if (!array) { hawk_rtx_refdownval(rtx, value); return -1; }
			hawk_rtx_refupval(rtx, array);
			text = hawk_rtx_makestrvalwithoocstr(rtx, HAWK_T("second"));
			if (!text || !hawk_rtx_setarrvalfld(rtx, array, 2, text) ||
			    !hawk_rtx_setmapvalfld(rtx, value, HAWK_T("X-Test"), 6, array))
			{
				hawk_rtx_refdownval(rtx, array);
				hawk_rtx_refdownval(rtx, value);
				return -1;
			}
			hawk_rtx_setretval(rtx, value);
			hawk_rtx_refdownval(rtx, array);
			hawk_rtx_refdownval(rtx, value);
			return 0;
		case HAWK_HTTP_GETSTATUS:
			value = hawk_rtx_makeintval(rtx, host->status);
			break;
		case HAWK_HTTP_FLUSH:
			host->committed = 1;
			return 0;
		case HAWK_HTTP_SETSTATUS:
			host->status = arg->status;
			return 0;
		default:
			return 0;
	}
	if (!value) return -1;
	hawk_rtx_setretval(rtx, value);
	return 0;
}

static void host_close (hawk_rtx_t* rtx, void* ptr)
{
	host_t* host = ptr;

	host->closed++;
}

/* Test hosts use the same public aux protocol as Go/CGI hosts. */
static int bind_http (hawk_rtx_t* rtx, const hawk_aux_cbs_t* cbs, void* ctx)
{
	hawk_uint16_t id;

	if (hawk_rtx_getauxid(rtx, HAWK_HTTP_AUX_NAME, &id) <= -1) return -1;
	return hawk_rtx_setauxctx(rtx, id, cbs, ctx);
}

static int call_http (hawk_rtx_t* rtx, hawk_http_cmd_t cmd, const hawk_http_arg_t* arg)
{
	hawk_uint16_t id;
	hawk_http_arg_t empty = { { 0 }, { 0 }, 0, 0 };

	if (hawk_rtx_getauxid(rtx, HAWK_HTTP_AUX_NAME, &id) <= -1) return -1;
	return hawk_rtx_auxcall(rtx, id, cmd, (void*)(arg? arg: &empty));
}

static void script_case (const hawk_bch_t* name, const hawk_bch_t* source, int attach, int failure, int expected_calls)
{
	hawk_t* hawk;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_parsestd_t input[2] = { { 0 } };
	hawk_aux_cbs_t cbs = { host_impl, host_close };
	host_t host = { 0, 0, 200, 0, 0, HAWK_T("/test") };
	hawk_val_t* result;
	hawk_int_t number;
	hawk_uint16_t other_id;
	int good = 0;

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) goto done;
	if (hawk_addstaticmodwithbcstr(hawk, "http", hawk_test_mod_http) <= -1) goto done;
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = strlen(source);
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1) goto done;
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("t-018"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) goto done;
	/* Ensure the actual module resolves the host name rather than assuming ID 0. */
	if (hawk_rtx_getauxid(rtx, HAWK_T("other-service"), &other_id) <= -1) goto done;
	if (attach && bind_http(rtx, &cbs, &host) <= -1) goto done;
	result = hawk_rtx_callwithbcstr(rtx, "test", HAWK_NULL, 0);
	if (result)
	{
		good = !failure && hawk_rtx_valtoint(rtx, result, &number) == 0 && number == 0;
		hawk_rtx_refdownval(rtx, result);
	}
	else good = failure && hawk_rtx_geterrnum(rtx) != HAWK_ENOERR;
	good = good && host.calls == expected_calls;
	if (!good) fprintf(stderr, "%s: calls=%d, error=%s\n", name, host.calls, hawk_rtx_geterrbmsg(rtx));

done:
	if (rtx) hawk_rtx_close(rtx);
	if (hawk) hawk_close(hawk);
	OK(good && host.closed == attach, name);
}

static int extra_service (hawk_rtx_t* rtx, hawk_oow_t cmd, void* data, void* ctx)
{
	return 42;
}

static void lifecycle (void)
{
	hawk_t* hawk;
	hawk_rtx_t* first;
	hawk_rtx_t* second;
	hawk_aux_cbs_t cbs = { host_impl, host_close };
	hawk_aux_cbs_t bad = { HAWK_NULL, host_close };
	hawk_aux_cbs_t extra = { extra_service, HAWK_NULL };
	hawk_uint16_t http_id;
	hawk_uint16_t second_id;
	host_t a = { 0, 0, 200, 0, 0, HAWK_T("/a") };
	host_t b = { 0, 0, 201, 0, 0, HAWK_T("/b") };
	host_t c = { 0, 0, 202, 0, 0, HAWK_T("/c") };
	hawk_http_arg_t arg = { { 0 }, { 0 }, 0, 204 };

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	first = hawk_rtx_openstd(hawk, 0, HAWK_T("first"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	second = hawk_rtx_openstd(hawk, 0, HAWK_T("second"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!first || !second) { bail_out("cannot open runtime"); return; }
	if (hawk_rtx_getauxid(second, HAWK_T("another-service"), &second_id) <= -1) { bail_out("cannot reserve aux ID"); return; }
	OK(bind_http(first, &cbs, &a) == 0 && bind_http(second, &cbs, &b) == 0, "independent runtime bindings");
	hawk_rtx_getauxid(first, HAWK_HTTP_AUX_NAME, &http_id);
	hawk_rtx_getauxid(second, HAWK_HTTP_AUX_NAME, &second_id);
	OK(http_id != second_id, "HTTP name resolves independently of runtime registration order");
	OK(call_http(first, HAWK_HTTP_SETSTATUS, &arg) == 0 && a.status == 204 && b.status == 201, "runtime contexts remain isolated");
	OK(hawk_rtx_setauxctx(first, AUX_ID_EXTRA, &extra, HAWK_NULL) == 0 &&
	   hawk_rtx_auxcall(first, AUX_ID_EXTRA, 1000, HAWK_NULL) == 42 &&
	   hawk_rtx_auxcall(first, http_id, HAWK_HTTP_SETSTATUS, &arg) == 0 && a.status == 204,
	   "HTTP dispatch uses generic bridge alongside another service");
	OK(bind_http(first, &bad, &c) <= -1 && a.closed == 0 && call_http(first, HAWK_HTTP_SETSTATUS, &arg) == 0, "invalid replacement preserves old binding");
	OK(bind_http(first, &cbs, &c) == 0 && a.closed == 1, "replacement closes previous context once");
	c.fail = 1;
	OK(call_http(first, HAWK_HTTP_FLUSH, HAWK_NULL) <= -1 && hawk_rtx_geterrnum(first) == HAWK_EOTHER, "host failure gets a diagnostic");
	c.fail = 0;
	OK(call_http(first, HAWK_HTTP_SETSTATUS, &arg) == 0, "failed uncommitted flush permits mutation");
	c.early_commit = 1;
	c.fail = 1;
	call_http(first, HAWK_HTTP_FLUSH, HAWK_NULL);
	c.fail = 0;
	OK(call_http(first, HAWK_HTTP_SETSTATUS, &arg) <= -1 && hawk_rtx_geterrnum(first) == HAWK_EPERM, "host commitment survives a failed flush");
	OK(bind_http(first, HAWK_NULL, HAWK_NULL) == 0 && c.closed == 1 && call_http(first, HAWK_HTTP_GETSTATUS, HAWK_NULL) <= -1, "detachment closes context and prevents calls");
	hawk_rtx_close(first);
	hawk_rtx_close(second);
	OK(a.closed == 1 && b.closed == 1 && c.closed == 1, "runtime close does not double-close detached bindings");
	hawk_close(hawk);
}

static void* test_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	if (*(int*)mmgr->ctx > 0 && --*(int*)mmgr->ctx == 0) return HAWK_NULL;
	return real->alloc(real, size);
}

static void* test_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	if (*(int*)mmgr->ctx > 0 && --*(int*)mmgr->ctx == 0) return HAWK_NULL;
	return real->realloc(real, ptr, size);
}

static void test_free (hawk_mmgr_t* mmgr, void* ptr)
{
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	real->free(real, ptr);
}

static void allocation_failure (void)
{
	int fail = 0;
	hawk_mmgr_t mmgr = { test_alloc, test_realloc, test_free, &fail };
	hawk_t* hawk;
	hawk_rtx_t* rtx;
	hawk_aux_cbs_t cbs = { host_impl, host_close };
	host_t a = { 0, 0, 200, 0, 0, HAWK_T("/a") };
	host_t b = { 0, 0, 201, 0, 0, HAWK_T("/b") };
	hawk_http_arg_t arg = { { 0 }, { 0 }, 0, 204 };
	int n;

	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return; }
	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("allocation"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!rtx) { bail_out("cannot open runtime"); return; }
	bind_http(rtx, &cbs, &a);
	/* The service name is already reserved; direct binding needs no allocation. */
	fail = 1;
	n = bind_http(rtx, &cbs, &b);
	OK(n == 0 && fail == 1 && a.closed == 1 && b.closed == 0,
	   "HTTP replacement needs no adapter allocation and closes the old context");
	fail = 0;
	OK(call_http(rtx, HAWK_HTTP_SETSTATUS, &arg) == 0 && b.status == 204,
	   "replacement HTTP context remains usable");
	hawk_rtx_close(rtx);
	OK(a.closed == 1 && b.closed == 1, "runtime closes only the currently attached HTTP context");
	hawk_close(hawk);
}

int main (void)
{
	/* Preserve diagnostics if a library assertion terminates the process. */
	setvbuf(stdout, HAWK_NULL, _IONBF, 0);
	no_plan();
	script_case("getters and indexed header values", "function test(){ @local h; h=http::getheaders(); return !(http::getmethod()==\"POST\" && http::getpath()==\"/test\" && http::getheader(\"X-Test\",2)==\"second\" && http::getheader(\"Missing\")==\"\" && h[\"X-Test\"][2]==\"second\"); }", 1, 0, 5);
	script_case("all response setters", "function test(){ http::setstatus(201); http::setheader(\"X-Test\",\"a\"); http::addheader(\"X-Test\",\"b\"); http::delheader(\"X-Test\"); return http::getstatus()!=201; }", 1, 0, 5);
	script_case("missing context fails", "function test(){ return http::getmethod(); }", 0, 1, 0);
	script_case("flush commits status", "function test(){ http::flush(); http::setstatus(201); }", 1, 1, 1);
	script_case("flush commits header replacement", "function test(){ http::flush(); http::setheader(\"X\",\"a\"); }", 1, 1, 1);
	script_case("flush commits header addition", "function test(){ http::flush(); http::addheader(\"X\",\"a\"); }", 1, 1, 1);
	script_case("flush commits header deletion", "function test(){ http::flush(); http::delheader(\"X\"); }", 1, 1, 1);
	script_case("getters and repeated flush remain available", "function test(){ http::flush(); http::flush(); return http::getstatus()!=200; }", 1, 0, 3);
	script_case("invalid status rejected before host", "function test(){ http::setstatus(199); }", 1, 1, 0);
	script_case("large status rejected before narrowing", "function test(){ http::setstatus(4294967296); }", 1, 1, 0);
	script_case("floating status rejected", "function test(){ http::setstatus(200.5); }", 1, 1, 0);
	script_case("empty header name rejected", "function test(){ http::getheader(\"\"); }", 1, 1, 0);
	script_case("separator in header name rejected", "function test(){ http::setheader(\"a:b\",\"x\"); }", 1, 1, 0);
	script_case("NUL in header name rejected", "function test(){ http::delheader(\"a\\0b\"); }", 1, 1, 0);
	script_case("CRLF in header value rejected", "function test(){ http::addheader(\"X\",\"a\\r\\nb\"); }", 1, 1, 0);
	script_case("NUL in header value rejected", "function test(){ http::setheader(\"X\",\"a\\0b\"); }", 1, 1, 0);
	script_case("non-string header value rejected", "function test(){ http::setheader(\"X\",123); }", 1, 1, 0);
	script_case("zero header index rejected", "function test(){ http::getheader(\"X\",0); }", 1, 1, 0);
	script_case("floating header index rejected", "function test(){ http::getheader(\"X\",1.5); }", 1, 1, 0);
	script_case("byte strings and tabs accepted", "function test(){ return http::setheader(@b\"X\",@b\"a\\tb\"); }", 1, 0, 1);
	lifecycle();
	allocation_failure();
	return exit_status();
}
