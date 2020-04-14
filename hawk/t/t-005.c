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
	hawk_uch_t src[8] = {'a','b','c','d','e','f','g','h'};
	hawk_uch_t dst[6] = {'0','1','2','3','4','5'};
        hawk_oow_t q, i;

        q = hawk_copy_uchars_to_ucstr(dst, HAWK_COUNTOF(dst), src, 7);
	T_ASSERT0 (q == HAWK_COUNTOF(dst) - 1);
	T_ASSERT0 (dst[HAWK_COUNTOF(dst) - 1] == '\0');
	for (i = 0; i < q; i++) T_ASSERT0 (dst[i] == src[i]);

	return 0;

oops:
	return -1;
}

int main ()
{
	if (test1() <= -1) goto oops;
	if (test2() <= -1) goto oops;
	return 0;

oops:
	return -1;
}
