/* test bit position functions */

#include <hawk-utl.h>
#include <stdio.h>
#include "t.h"

int main ()
{
	int i, j;
	hawk_oow_t v;

	printf ("QSE_OOW_BITS => %d, sizeof(hawk_oow_t)=%d\n", (int)HAWK_OOW_BITS, (int)sizeof(hawk_oow_t));
	for (i = 0; i < HAWK_OOW_BITS; i++)
	{
		v = ((hawk_oow_t)1 << i);
		j = hawk_get_pos_of_msb_set_pow2(v);
		printf ("msb(pow2) %d %d ==> %llx\n", i, j, (long long int)v);
		T_ASSERT1 (i == j, "msb(pow2) position tester");
	}

	for (i = 0; i < HAWK_OOW_BITS; i++)
	{
		v = ((hawk_oow_t)1 << i);
		v |= 1;
		j = hawk_get_pos_of_msb_set(v);
		printf ("msb %d %d ==> %llx\n", i, j, (long long int)v);
		T_ASSERT1 (i == j, "msb position tester");
	}


	return 0;

oops:
	return -1;
}
