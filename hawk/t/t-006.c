
#include <hawk-utl.h>
#include <stdio.h>
#include "t.h"


struct
{
	hawk_ntime_sec_t s1;   /* input x */
	hawk_ntime_nsec_t ns1;

	hawk_ntime_sec_t s2;   /* input y */
	hawk_ntime_nsec_t ns2;

	hawk_ntime_sec_t add_s; /* expected z after addition */
	hawk_ntime_nsec_t add_ns;

	hawk_ntime_sec_t sub_s;  /* expected z after subtraction */
	hawk_ntime_nsec_t sub_ns;
} tab[] = 
{
	/* 0 */
	{ 12345678,  HAWK_NSECS_PER_SEC - 1,
	  0,  10,
          12345679,  9,
          12345678,  HAWK_NSECS_PER_SEC - 11 },

	{ 0,  0,
	  12345678,  10,
          12345678,  10,
          -12345679,  HAWK_NSECS_PER_SEC - 10 },

	{ 0,  0,
	  -12345678,  10,
          -12345678,  10,
          12345677,  HAWK_NSECS_PER_SEC - 10 },

	{ HAWK_TYPE_MAX(hawk_ntime_sec_t) - 1, HAWK_NSECS_PER_SEC - 1,
	  1, 0,  
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1, 
	  HAWK_TYPE_MAX(hawk_ntime_sec_t) - 2, HAWK_NSECS_PER_SEC - 1  },

	{ HAWK_TYPE_MAX(hawk_ntime_sec_t) - 1, HAWK_NSECS_PER_SEC - 1,
	  1, 1,
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1, 
	  HAWK_TYPE_MAX(hawk_ntime_sec_t) - 2, HAWK_NSECS_PER_SEC - 2  },

	/* 5 */
	{ HAWK_TYPE_MAX(hawk_ntime_sec_t) - 1, HAWK_NSECS_PER_SEC - 1,
	  1, 2,
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1, 
	  HAWK_TYPE_MAX(hawk_ntime_sec_t) - 2, HAWK_NSECS_PER_SEC - 3  },

	{ HAWK_TYPE_MAX(hawk_ntime_sec_t), 0,
	  0, 0,  
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), 0, 
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), 0  },

	{ HAWK_TYPE_MIN(hawk_ntime_sec_t), 0, 
	  0, 0,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0, 
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0  },

	{ HAWK_TYPE_MIN(hawk_ntime_sec_t) + 1, 0,
	  1, 2,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t) + 2, 2,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0 },

	{ HAWK_TYPE_MIN(hawk_ntime_sec_t) + 1, 0,
	  2, 2,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t) + 3, 2,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0 },

	/* 10 */
	{ HAWK_TYPE_MIN(hawk_ntime_sec_t) + 1, 0,
	  2, HAWK_NSECS_PER_SEC - 1,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t) + 3, HAWK_NSECS_PER_SEC - 1,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0 },

	{ 0, 0,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0, 
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0, 
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1  },

	{ HAWK_TYPE_MAX(hawk_ntime_sec_t), 0,
	  1, 0,  
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1, 
	  HAWK_TYPE_MAX(hawk_ntime_sec_t) - 1, 0  },

	{ HAWK_TYPE_MAX(hawk_ntime_sec_t), 0,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0,  
	  -1, 0,
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1  },

	{ HAWK_TYPE_MIN(hawk_ntime_sec_t), 0,
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), 0,  
	  -1, 0,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0 },

	/* 15 */
	{ HAWK_TYPE_MIN(hawk_ntime_sec_t), 0,
	  HAWK_TYPE_MAX(hawk_ntime_sec_t), HAWK_NSECS_PER_SEC - 1,  
	  -1, HAWK_NSECS_PER_SEC - 1,
	  HAWK_TYPE_MIN(hawk_ntime_sec_t), 0 }
};

int main ()
{
	hawk_ntime_t x, y, z;
	int i;
	char buf1[64], buf2[64];

	for (i = 0; i < HAWK_COUNTOF(tab); i++)
	{
		sprintf (buf1, "add - index %d", i);
		sprintf (buf2, "sub - index %d", i);

		x.sec = tab[i].s1;
		x.nsec = tab[i].ns1;
		y.sec = tab[i].s2;
		y.nsec = tab[i].ns2;

		hawk_add_ntime (&z, &x, &y);
		T_ASSERT1 (z.sec == tab[i].add_s, buf1);
		T_ASSERT1 (z.nsec == tab[i].add_ns, buf1);

		hawk_sub_ntime (&z, &x, &y);
		T_ASSERT1 (z.sec == tab[i].sub_s, buf2);
		T_ASSERT1 (z.nsec == tab[i].sub_ns, buf2);

	}

	return 0;

oops:
	return -1;
}
