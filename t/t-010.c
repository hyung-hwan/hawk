#include <hawk.h>
#include <hawk-utl.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

#define OK_X(test) OK(test, #test)

static const hawk_bch_t* src =
	"BEGIN {"
	"	aa = 10;"
	"	bb = 20;"
	"	@reset bb;"
	"	cc = 30;"
	"}";

static void check_int_value (hawk_rtx_t* rtx, hawk_val_t* v, hawk_int_t expected, const char* name)
{
	hawk_int_t lv;

	if (!v)
	{
		OK(0, name);
		return;
	}

	if (hawk_rtx_valtoint(rtx, v, &lv) <= -1)
	{
		OK(0, name);
		return;
	}

	OK(lv == expected, name);
}

int main ()
{
	hawk_t* hawk = HAWK_NULL;
	hawk_rtx_t* rtx = HAWK_NULL;
	hawk_val_t* retv;
	hawk_parsestd_t psin[2];
	hawk_rtx_nv_itr_t itr;
	hawk_val_t* nv;
	hawk_uch_t rt1_name[] = { 'r', 't', '1', '\0' };
	int count = 0;
	int aa_seen = 0, bb_seen = 0, cc_seen = 0, rt1_seen = 0, rt2_seen = 0;
	int ret = -1;

	no_plan ();

	hawk = hawk_openstd(0, HAWK_NULL);
	OK_X(hawk != HAWK_NULL);
	if (!hawk) goto oops;

	memset (psin, 0, HAWK_SIZEOF(psin));
	psin[0].type = HAWK_PARSESTD_BCS;
	psin[0].u.bcs.ptr = (hawk_bch_t*)src;
	psin[0].u.bcs.len = hawk_count_bcstr(src);
	psin[1].type = HAWK_PARSESTD_NULL;

	OK_X(hawk_parsestd(hawk, psin, HAWK_NULL) >= 0);

	rtx = hawk_rtx_openstd(hawk, 0, HAWK_T("t-010"), HAWK_NULL, HAWK_NULL, HAWK_NULL);
	OK_X(rtx != HAWK_NULL);
	if (!rtx) goto oops;

	OK_X(hawk_rtx_setgbltostrbyname(rtx, HAWK_T("rt1"), HAWK_T("40")) >= 0);
	OK_X(hawk_rtx_setgbltostrbyname(rtx, HAWK_T("rt2"), HAWK_T("50")) >= 0);

	retv = hawk_rtx_loop(rtx);
	OK_X(retv != HAWK_NULL);
	if (!retv) goto oops;
	hawk_rtx_refdownval(rtx, retv);

	check_int_value(rtx, hawk_rtx_findnvbybcstr(rtx, "aa"), 10, "find aa by bcstr");
	OK_X(hawk_rtx_findnvbybcstr(rtx, "bb") != HAWK_NULL);
	OK_X(hawk_rtx_isnilval(rtx, hawk_rtx_findnvbybcstr(rtx, "bb")));
	check_int_value(rtx, hawk_rtx_findnvbybcstr(rtx, "cc"), 30, "find cc by bcstr");
	check_int_value(rtx, hawk_rtx_findnvbyucstr(rtx, rt1_name), 40, "find rt1 by ucstr");
	check_int_value(rtx, hawk_rtx_findnvbybcstr(rtx, "rt2"), 50, "find rt2 by bcstr");
	OK_X(hawk_rtx_findnvbybcstr(rtx, "missing") == HAWK_NULL);

	hawk_init_rtx_nv_itr(&itr);
	for (nv = hawk_rtx_getfirstnv(rtx, &itr); nv; nv = hawk_rtx_getnextnv(rtx, &itr))
	{
		count++;

		if (hawk_comp_oochars_bcstr(itr.name.ptr, itr.name.len, "aa", 0) == 0)
		{
			aa_seen = 1;
			check_int_value(rtx, nv, 10, "iterate aa");
		}
		else if (hawk_comp_oochars_bcstr(itr.name.ptr, itr.name.len, "bb", 0) == 0)
		{
			bb_seen = 1;
			OK_X(hawk_rtx_isnilval(rtx, nv));
		}
		else if (hawk_comp_oochars_bcstr(itr.name.ptr, itr.name.len, "cc", 0) == 0)
		{
			cc_seen = 1;
			check_int_value(rtx, nv, 30, "iterate cc");
		}
		else if (hawk_comp_oochars_bcstr(itr.name.ptr, itr.name.len, "rt1", 0) == 0)
		{
			rt1_seen = 1;
			check_int_value(rtx, nv, 40, "iterate rt1");
		}
		else if (hawk_comp_oochars_bcstr(itr.name.ptr, itr.name.len, "rt2", 0) == 0)
		{
			rt2_seen = 1;
			check_int_value(rtx, nv, 50, "iterate rt2");
		}
		else
		{
			OK(0, "unexpected named variable in iterator");
		}
	}

	OK_X(count == 5);
	OK_X(aa_seen == 1);
	OK_X(bb_seen == 1);
	OK_X(cc_seen == 1);
	OK_X(rt1_seen == 1);
	OK_X(rt2_seen == 1);

	ret = 0;

oops:
	if (rtx) hawk_rtx_close(rtx);
	if (hawk) hawk_close(hawk);
	return (ret == 0)? exit_status(): -1;
}
