/* test bit position functions */

#include <hawk-utl.h>
#include <hawk.h>
#include <stdio.h>
#include "tap.h"

static const hawk_bch_t* temp_dir_getenv (const hawk_bch_t* name, void* ctx)
{
	const hawk_bch_t** values = ctx;

	if (hawk_comp_bcstr(name, "TMPDIR", 0) == 0) return values[0];
	if (hawk_comp_bcstr(name, "TEMP", 0) == 0) return values[1];
	if (hawk_comp_bcstr(name, "TMP", 0) == 0) return values[2];
	return HAWK_NULL;
}

static void* fail_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	return HAWK_NULL;
}

static void test_temp_dir (void)
{
	hawk_t* hawk;
	hawk_gem_t* gem;
	hawk_bch_t* bdir;
	hawk_uch_t* udir;
	const hawk_bch_t* values[3] = { "first", "second", "third" };
	hawk_gem_t fail_gem;
	hawk_mmgr_t fail_mmgr;

	hawk = hawk_openstd(0, HAWK_NULL);
	OK (hawk != HAWK_NULL, "open interpreter for temporary directory tests");
	if (!hawk) return;
	gem = hawk_getgem(hawk);

	bdir = hawk_get_temp_dir_bcstr(gem, temp_dir_getenv, values);
	OK (bdir && bdir != values[0] && hawk_comp_bcstr(bdir, "first", 0) == 0, "temporary directory callback takes precedence and returns owned storage");
	if (bdir) hawk_gem_freemem(gem, bdir);

	values[0] = "";
	bdir = hawk_get_temp_dir_bcstr(gem, temp_dir_getenv, values);
	OK (bdir && hawk_comp_bcstr(bdir, "second", 0) == 0, "empty TMPDIR falls back to TEMP");
	if (bdir) hawk_gem_freemem(gem, bdir);

	values[1] = HAWK_NULL;
	bdir = hawk_get_temp_dir_bcstr(gem, temp_dir_getenv, values);
	OK (bdir && hawk_comp_bcstr(bdir, "third", 0) == 0, "missing TEMP falls back to TMP");
	if (bdir) hawk_gem_freemem(gem, bdir);

	values[0] = "caf\xC3\xA9";
	gem->cmgr = hawk_get_utf8_cmgr();
	udir = hawk_get_temp_dir_ucstr(gem, temp_dir_getenv, values);
	OK (udir && hawk_comp_ucstr(udir, HAWK_UT("caf\x00E9"), 0) == 0, "wide temporary directory converts callback bytes with gem's cmgr");
	if (udir) hawk_gem_freemem(gem, udir);

	values[0] = "\xFF";
	udir = hawk_get_temp_dir_ucstr(gem, temp_dir_getenv, values);
	OK (!udir && hawk_gem_geterrnum(gem) == HAWK_EECERR, "invalid temporary directory encoding reports conversion failure");
	if (udir) hawk_gem_freemem(gem, udir);

	values[0] = "first";
	fail_gem = *gem;
	fail_mmgr = *gem->mmgr;
	fail_mmgr.alloc = fail_alloc;
	fail_gem.mmgr = &fail_mmgr;
	bdir = hawk_get_temp_dir_bcstr(&fail_gem, temp_dir_getenv, values);
	OK (!bdir && hawk_gem_geterrnum(&fail_gem) == HAWK_ENOMEM, "temporary directory allocation failure sets gem error");
	udir = hawk_get_temp_dir_ucstr(&fail_gem, temp_dir_getenv, values);
	OK (!udir && hawk_gem_geterrnum(&fail_gem) == HAWK_ENOMEM, "wide temporary directory allocation failure sets gem error");
	bdir = hawk_get_temp_dir_bcstr(&fail_gem, HAWK_NULL, HAWK_NULL);
	OK (!bdir && hawk_gem_geterrnum(&fail_gem) == HAWK_ENOMEM, "native temporary directory allocation failure sets gem error");

	bdir = hawk_get_temp_dir_bcstr(gem, HAWK_NULL, HAWK_NULL);
	OK (bdir && bdir[0], "native byte temporary directory is nonempty");
	if (bdir) hawk_gem_freemem(gem, bdir);
	udir = hawk_get_temp_dir_ucstr(gem, HAWK_NULL, HAWK_NULL);
	OK (udir && udir[0], "native wide temporary directory is nonempty");
	if (udir) hawk_gem_freemem(gem, udir);

	hawk_close(hawk);
}

int main ()
{
	int i, j;
	hawk_oow_t v;

	no_plan ();
	test_temp_dir();

	/*printf ("QSE_OOW_BITS => %d, sizeof(hawk_oow_t)=%d\n", (int)HAWK_OOW_BITS, (int)sizeof(hawk_oow_t));*/
	for (i = 0; i < HAWK_OOW_BITS; i++)
	{
		v = ((hawk_oow_t)1 << i);
		j = hawk_get_pos_of_msb_set_pow2(v);
		/*printf ("msb(pow2) %d %d ==> %llx\n", i, j, (long long int)v);*/
		OK (i == j, "msb(pow2) position tester");
	}

	for (i = 0; i < HAWK_OOW_BITS; i++)
	{
		v = ((hawk_oow_t)1 << i);
		v |= 1;
		j = hawk_get_pos_of_msb_set(v);
		/*printf ("msb %d %d ==> %llx\n", i, j, (long long int)v);*/
		OK (i == j, "msb position tester");
	}


	return exit_status();
}
