
#include <hawk-json.h>
#include <stdio.h>
#include "tap.h"

static int on_json_element (hawk_json_t* json, hawk_json_inst_t inst, const hawk_oocs_t* str)
{
printf ("%d\n", inst);
	return 0;
}

int main ()
{
	hawk_json_t* json;
	hawk_json_prim_t prim;

	no_plan();
	prim.instcb = on_json_element;

	json = hawk_json_openstd(0, &prim, HAWK_NULL);
	OK (json != HAWK_NULL, "instantiation must be successful");
	
	hawk_json_close(json);	
	return exit_status();
}
