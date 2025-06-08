
#include <hawk-json.h>
#include <hawk-str.h>
#include <stdio.h>
#include "tap.h"

#define OK_X(test) OK(test, #test)

static int on_json_element (hawk_json_t* json, hawk_json_inst_t inst, const hawk_oocs_t* str)
{
	static int phase = 0;

	if (phase == 0) OK_X(inst == HAWK_JSON_INST_START_DIC);
	if (phase == 1)
	{
		OK_X(inst == HAWK_JSON_INST_KEY);
		OK_X(hawk_comp_oochars_bcstr(str->ptr, str->len, "hello", 0) == 0);
	}
	if (phase == 2)
	{
		OK_X(inst == HAWK_JSON_INST_STRING);
		OK_X(hawk_comp_oochars_bcstr(str->ptr, str->len, "world", 0) == 0);
	}
	if (phase == 3)
	{
		OK_X(inst == HAWK_JSON_INST_KEY);
		OK_X(hawk_comp_oochars_bcstr(str->ptr, str->len, "key1", 0) == 0);
	}
	if (phase == 4)
	{
		OK_X(inst == HAWK_JSON_INST_NUMBER);
		OK_X(hawk_comp_oochars_bcstr(str->ptr, str->len, "12345", 0) == 0);
	}
	if (phase == 5) OK_X(inst == HAWK_JSON_INST_END_DIC);

	phase++;
	return 0;
}

int main ()
{
	hawk_json_t* json;
	hawk_json_prim_t prim;
	hawk_oow_t xlen;
	int n;


	no_plan();
	prim.instcb = on_json_element;

	json = hawk_json_openstd(0, &prim, HAWK_NULL);
	OK (json != HAWK_NULL, "instantiation must be successful");

	n = hawk_json_feed(json, HAWK_T("{\"hello\": \"world\", \"key1\": 12345}"), 33, &xlen);
	OK_X(n == 0);
	OK_X(xlen == 33);

	hawk_json_close(json);	
	return exit_status();
}
