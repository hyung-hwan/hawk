#include <hawk.h>
#include <hawk-utl.h>
#include <stdio.h>
#include "tap.h"

int main ()
{
	hawk_t* hawk = HAWK_NULL;
	hawk_uch_t ufmt1[] = { '%', '0', '5', 'd', ' ', '%', '-', '9', 'h', 's', '\0' };
#if 0
	hawk_uch_t ufmt2[] = { '%', '0', '5', 'd', ' ', '%', '-', '9', 'h', 's', ' ', '%','O','\0' };
#endif
	no_plan ();

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk)
	{
		fprintf (stderr, "Unable to open hawk\n");
		return -1;
	}

	hawk_seterrbfmt (hawk, HAWK_NULL, HAWK_EINVAL, "%d %ld %s %hs", 10, 20L, "hawk", "hawk");
	OK (hawk_comp_oocstr_bcstr(hawk_geterrmsg(hawk), "10 20 hawk hawk", 0) == 0, "hawk seterrbfmt #1");
	hawk_logbfmt (hawk, HAWK_LOG_STDERR, "[%js]\n", hawk_geterrmsg(hawk));

	hawk_seterrufmt (hawk, HAWK_NULL, HAWK_EINVAL, ufmt1, 9923, "hawk");
	OK (hawk_comp_oocstr_bcstr(hawk_geterrmsg(hawk), "09923 hawk     ", 0) == 0, "hawk seterrufmt #1");
	hawk_logbfmt (hawk, HAWK_LOG_STDERR, "[%js]\n", hawk_geterrmsg(hawk));

#if 0 
	hawk_seterrufmt (hawk, HAWK_NULL, HAWK_EINVAL, ufmt2, 9923, "hawk", HAWK_SMPTR_TO_OOP(0x12345678));
	OK (hawk_comp_oocstr_bcstr(hawk_geterrmsg(hawk), "09923 hawk       #\\p12345678", 0) == 0, "hawk seterrufmt #1");
	hawk_logbfmt (hawk, HAWK_LOG_STDERR, "[%js]\n", hawk_geterrmsg(hawk));
#endif

	hawk_close (hawk);
	return exit_status();
}
