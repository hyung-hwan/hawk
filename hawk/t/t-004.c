#include <hawk-std.h>
#include <hawk-utl.h>
#include <stdio.h>
#include "t.h"

int main ()
{
	hawk_t* hawk = HAWK_NULL;
	hawk_uch_t ufmt1[] = { '%', '0', '5', 'd', ' ', '%', '-', '9', 'h', 's', '\0' };
	hawk_uch_t ufmt2[] = { '%', '0', '5', 'd', ' ', '%', '-', '9', 'h', 's', ' ', '%','O','\0' };

	hawk = hawk_openstd(0, HAWK_NULL, HAWK_NULL);
	if (!hawk)
	{
		fprintf (stderr, "Unable to open hawk\n");
		return -1;
	}

	hawk_seterrbfmt (hawk, HAWK_EINVAL, "%d %ld %s %hs", 10, 20L, "hawk", "hawk");
	T_ASSERT1 (hawk_comp_oocstr_bcstr(hawk_geterrmsg(hawk), "10 20 hawk hawk") == 0, "hawk seterrbfmt #1");
	hawk_logbfmt (hawk, HAWK_LOG_STDERR, "[%js]\n", hawk_geterrmsg(hawk));

	hawk_seterrufmt (hawk, HAWK_EINVAL, ufmt1, 9923, "hawk");
	T_ASSERT1 (hawk_comp_oocstr_bcstr(hawk_geterrmsg(hawk), "09923 hawk      ") == 0, "hawk seterrufmt #1");
	hawk_logbfmt (hawk, HAWK_LOG_STDERR, "[%js]\n", hawk_geterrmsg(hawk));

	hawk_seterrufmt (hawk, HAWK_EINVAL, ufmt2, 9923, "hawk", HAWK_SMPTR_TO_OOP(0x12345678));
	T_ASSERT1 (hawk_comp_oocstr_bcstr(hawk_geterrmsg(hawk), "09923 hawk       #\\p12345678") == 0, "hawk seterrufmt #1");
	hawk_logbfmt (hawk, HAWK_LOG_STDERR, "[%js]\n", hawk_geterrmsg(hawk));

	hawk_close (hawk);
	return 0;

oops:
	if (hawk) hawk_close (hawk);
	return -1;
}
