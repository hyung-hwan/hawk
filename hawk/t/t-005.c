#include <hawk-ecs.h>
#include <hawk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "t.h"

static void* sys_alloc (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	return malloc(size);
}

static void* sys_realloc (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	return realloc(ptr, size);
}

static void sys_free (hawk_mmgr_t* mmgr, void* ptr)
{
	free (ptr);
}

static hawk_mmgr_t sys_mmgr =
{
        sys_alloc,
        sys_realloc,
        sys_free,
        HAWK_NULL
};

static int test1 (void)
{
	hawk_becs_t* b;
	hawk_gem_t g;

	g.mmgr = &sys_mmgr;

	b = hawk_becs_open(&g,  0, 5);
	hawk_becs_cat (b, "hello");
	T_ASSERT0 (hawk_becs_getlen(b) == 5);

	hawk_becs_cat (b, "hello again");
	T_ASSERT0 (hawk_becs_getlen(b) == 16);

	hawk_becs_del (b, 11, 3);
	T_ASSERT0 (hawk_becs_getlen(b) == 13);
	T_ASSERT0 (HAWK_BECS_CHAR(b, 12) == 'n');
	T_ASSERT0 (HAWK_BECS_CHAR(b, 13) == '\0');

	printf ("[%s]\n", HAWK_BECS_PTR(b)); 
	hawk_becs_close (b);
	return 0;

oops:
	return -1;
}

static int test2 (void)
{
	const hawk_uch_t src[8] = {'a','b','c','d','e','f','g','h'};
	const hawk_uch_t sxx[6] = {'0','1','2', '\0'};
	hawk_uch_t dst[6] = {'0','1','2','3','4','5'};
	
	hawk_oow_t q, i;

	q = hawk_copy_uchars_to_ucstr(dst, HAWK_COUNTOF(dst), src, 7);
	T_ASSERT0 (q == HAWK_COUNTOF(dst) - 1);
	T_ASSERT0 (dst[HAWK_COUNTOF(dst) - 1] == '\0');
	for (i = 0; i < q; i++) T_ASSERT0 (dst[i] == src[i]);

	q = hawk_copy_ucstr_to_uchars(dst, HAWK_COUNTOF(dst), sxx);
	T_ASSERT0 (q == 3);
	T_ASSERT0 (dst[q] == src[q]);
	for (i = 0; i < q; i++) T_ASSERT0 (dst[i] == sxx[i]);

	return 0;

oops:
	return -1;
}

hawk_bch_t* subst (hawk_bch_t* buf, hawk_oow_t bsz, const hawk_bcs_t* ident, void* ctx)
{ 
	if (hawk_comp_bchars_bcstr(ident->ptr, ident->len, "USER") == 0)
	{
		return buf + ((buf == HAWK_SUBST_NOBUF)? 3: hawk_copy_bcstr_to_bchars(buf, bsz, "sam"));
	}
	else if (hawk_comp_bchars_bcstr(ident->ptr, ident->len, "GROUP") == 0)
		return buf + ((buf == HAWK_SUBST_NOBUF)? 6: hawk_copy_bcstr_to_bchars(buf, bsz, "coders"));
	return buf; 
}

static int test3 (void)
{
	hawk_bch_t buf[25], * ptr;
	hawk_oow_t n;
	n = hawk_subst_for_bcstr_to_bcstr (buf, HAWK_COUNTOF(buf), "user=${USER},group=${GROUP}", subst, HAWK_NULL);
	T_ASSERT0 (n == 21);
	T_ASSERT0 (hawk_count_bcstr(buf) == 21);
	T_ASSERT0 (hawk_comp_bcstr(buf, "user=sam,group=coders", 0) == 0);

	n = hawk_subst_for_bcstr_to_bcstr(HAWK_SUBST_NOBUF, 0, "USERNAME=${USER},GROUPNAME=${GROUP}", subst, HAWK_NULL);
	T_ASSERT0 (n == 29);
	ptr = malloc(n + 1);
	n = hawk_subst_for_bcstr_to_bcstr(ptr, n + 1, "USERNAME=${USER},GROUPNAME=${GROUP}", subst, HAWK_NULL);
	T_ASSERT0 (n == 29);
	T_ASSERT0 (hawk_count_bcstr(ptr) == 29);
	T_ASSERT0 (hawk_comp_bcstr(ptr, "USERNAME=sam,GROUPNAME=coders", 0) == 0);
	free (ptr);

	return 0;

oops:
	return -1;
}

int main ()
{
	if (test1() <= -1) goto oops;
	if (test2() <= -1) goto oops;
	if (test3() <= -1) goto oops;
	return 0;

oops:
	return -1;
}
