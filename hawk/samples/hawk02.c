#include <hawk-std.h>
#include <stdio.h>
#include <string.h>

static const hawk_bch_t* src =
    "BEGIN {"
    "   for (i=2;i<=9;i++)"
    "   {"
    "       for (j=1;j<=9;j++)"
    "           print i \"*\" j \"=\" i * j;"
    "       print \"---------------------\";"
    "   }"
    "}";

int main ()
{
	hawk_t* hawk = HAWK_NULL;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_val_t* retv;

	hawk_parsestd_t psin[2];

	int ret;

	hawk = hawk_openstd(0, HAWK_NULL);
	if (!hawk)  
	{
		fprintf (stderr, "ERROR: cannot open hawk\n");
		ret = -1; goto oops;
	}

	memset (&psin, 0, HAWK_SIZEOF(psin));
	psin[0].type = HAWK_PARSESTD_BCS;
	psin[0].u.bcs.ptr = (hawk_bch_t*)src;
	psin[0].u.bcs.len = hawk_count_bcstr(src);
	psin[1].type = HAWK_PARSESTD_NULL;

	ret = hawk_parsestd(hawk, psin, HAWK_NULL);
	if (ret <= -1)
	{
		hawk_logbfmt (hawk, HAWK_LOG_STDERR, "ERROR(parse): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	rtx = hawk_rtx_openstd (
		hawk, 
		0,
		HAWK_T("hawk02"),
		HAWK_NULL,  /* stdin */
		HAWK_NULL,  /* stdout */
		HAWK_NULL   /* default cmgr */
	);
	if (!rtx)
	{
		hawk_logbfmt (hawk, HAWK_LOG_STDERR, "ERROR(rtx_open): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	retv = hawk_rtx_loop(rtx);
	if (!retv)
	{
		hawk_logbfmt (hawk, HAWK_LOG_STDERR, "ERROR(rtx_loop): %js\n", hawk_geterrmsg(hawk));
		ret = -1; goto oops;
	}

	hawk_rtx_refdownval (rtx, retv);
	ret = 0;

oops:
	if (rtx) hawk_rtx_close (rtx);
	if (hawk) hawk_close (hawk);
	return -1;
}
 
