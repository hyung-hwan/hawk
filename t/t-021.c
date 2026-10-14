/* Mustache module diagnostics and ownership across independent runtimes. */
#include <hawk.h>
#include <hawk-utl.h>
#include "tap.h"

/* Exercise the module even in builds where mus:: is disabled. */
#define hawk_mod_mus hawk_test_mod_mus
#include "../mod/mod-mus.c"
#undef hawk_mod_mus

static int call_number (hawk_rtx_t* rtx, const hawk_bch_t* name, hawk_int_t expected)
{
	hawk_val_t* result;
	hawk_int_t number;
	int good;

	result = hawk_rtx_callwithbcstr(rtx, name, HAWK_NULL, 0);
	if (!result) return 0;
	good = hawk_rtx_valtoint(rtx, result, &number) == 0 && number == expected;
	hawk_rtx_refdownval(rtx, result);
	return good;
}

static int call_message (hawk_rtx_t* rtx, int nonempty)
{
	hawk_val_t* result;
	hawk_ooch_t* text;
	hawk_oow_t len;
	int good = 0;

	result = hawk_rtx_callwithbcstr(rtx, "message", HAWK_NULL, 0);
	if (!result) return 0;
	text = hawk_rtx_getvaloocstr(rtx, result, &len);
	if (text)
	{
		good = (len > 0) == nonempty;
		hawk_rtx_freevaloocstr(rtx, result, text);
	}
	hawk_rtx_refdownval(rtx, result);
	return good;
}

int main (void)
{
	hawk_t* hawk;
	hawk_rtx_t* first = HAWK_NULL;
	hawk_rtx_t* second = HAWK_NULL;
	hawk_parsestd_t input[2];
	const hawk_bch_t* source =
		"function fail(    output) { output=\"keep\"; "
		"if (mus::process(\"{{\", @nil, output, mus::STR_TO_STR) != -1) return 1; "
		"return output == \"keep\"? 0: 2; } "
		"function success(    data, output) { data[\"name\"]=\"HAWK\"; "
		"if (mus::process(\"{{name}}\", data, output, mus::STR_TO_STR) != 0) return 1; "
		"return output == \"HAWK\"? 0: 2; } "
		"function alias(    data) { data[\"name\"]=\"HAWK\"; "
		"if (mus::process(\"{{name}}\", data, data, mus::STR_TO_STR) != 0) return 1; "
		"return data == \"HAWK\"? 0: 2; } "
		"function message() { return mus::errmsg(); }";

	no_plan();
	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk)
	{
		bail_out("cannot open interpreter");
		return 1;
	}
	if (hawk_addstaticmodwithbcstr(hawk, "mus", hawk_test_mod_mus) <= -1) goto fail;
	HAWK_MEMSET(input, 0, HAWK_SIZEOF(input));
	input[0].type = HAWK_PARSESTD_BCS;
	input[0].u.bcs.ptr = (hawk_bch_t*)source;
	input[0].u.bcs.len = hawk_count_bcstr(source);
	if (hawk_parsestd(hawk, input, HAWK_NULL) <= -1) goto fail;
	first = hawk_rtx_openstd(hawk, 0, HAWK_T("first"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	second = hawk_rtx_openstd(hawk, 0, HAWK_T("second"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!first || !second) goto fail;
	OK(call_message(first, 0), "first runtime starts without a diagnostic");
	OK(call_message(second, 0), "second runtime starts without a diagnostic");
	OK(call_number(first, "fail", 0), "syntax failure is catchable and preserves the output");
	OK(call_message(first, 1), "first runtime retains its module diagnostic");
	OK(call_message(second, 0), "first runtime failure does not alter second runtime");
	OK(call_number(second, "success", 0), "second runtime renders independently");
	OK(call_message(first, 1), "second runtime success does not clear first diagnostic");
	OK(call_number(first, "success", 0), "failed runtime remains reusable");
	OK(call_message(first, 0), "success clears only the calling runtime diagnostic");
	OK(call_number(first, "alias", 0), "output can replace the input data after rendering");
	OK(call_number(second, "fail", 0), "second runtime can retain a failure");
	hawk_rtx_close(first);
	first = HAWK_NULL;
	OK(call_message(second, 1), "closing first runtime preserves second runtime state");
	OK(call_number(second, "success", 0), "second runtime works after first is closed");
	hawk_rtx_close(second);
	second = hawk_rtx_openstd(hawk, 0, HAWK_T("replacement"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	if (!second) goto fail;
	OK(call_message(second, 0), "replacement runtime has fresh diagnostic state");
	OK(call_number(second, "success", 0), "compiled interpreter can create another runtime");
	hawk_rtx_close(second);
	hawk_close(hawk);
	return exit_status();

fail:
	if (first) hawk_rtx_close(first);
	if (second) hawk_rtx_close(second);
	hawk_close(hawk);
	bail_out("cannot prepare mustache module runtimes");
	return 1;
}
