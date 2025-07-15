#include <hawk-xma.h>
#include <hawk-std.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tap.h"

#define NUM_ITERATIONS 20000
#define MAX_ALLOC_ACTIVE 1000
#define MIN_ALLOC_SIZE 16
#define MAX_ALLOC_SIZE 1024

#define OK_X(test) OK(test, #test)


typedef struct {
	void *ptr;
	size_t size;
} Allocation;


static void print_xma (void* ctx, const hawk_bch_t* fmt, ...)
{
     va_list ap;
     va_start (ap, fmt);
     vfprintf (stderr, fmt, ap);
     va_end (ap);
}

static size_t random_size(hawk_oow_t max_alloc_size)
{
	return MIN_ALLOC_SIZE + rand() % (max_alloc_size - MIN_ALLOC_SIZE + 1);
}

static void test1(hawk_mmgr_t* mmgr)
{
	void* x1, * x2, * x3;

	x1 = HAWK_MMGR_ALLOC(mmgr, 416);
	OK_X (x1 != HAWK_NULL);
	x2 = HAWK_MMGR_ALLOC(mmgr, 688);
	OK_X (x2 != HAWK_NULL);

	HAWK_MMGR_FREE(mmgr, x1);
	HAWK_MMGR_FREE(mmgr, x2);
	x3 = HAWK_MMGR_ALLOC(mmgr, 144);
	OK_X (x2 != HAWK_NULL);

	HAWK_MMGR_FREE(mmgr, x3);
	/*hawk_xma_dump(mmgr.ctx, print_xma, HAWK_NULL);*/
}

static void test2(hawk_mmgr_t* mmgr)
{
	int x[] =
	{
		960, 688, -688, -960, 560, 32, -560, -32, 336, 624, -624, -336, 672, -672, 304, -304, 992, -992, 608, -608,
		576, 304, 256,  -304, -256, 416,
	};
	int i, j;
	int count;
	void *p[100];
	int sz[100];
	int test2_bad = 0;
	
	count = 0;
	for (i = 0; i < HAWK_COUNTOF(x); i++)
	{
		if (x[i] > 0)
		{
			p[count] = HAWK_MMGR_ALLOC(mmgr, x[i]);
			if (!p[count])
			{
				test2_bad = 1;
				break;
			}
			sz[count] = x[i];
			count++;
		}
		else if (x[i] < 0)
		{
			for (j = 0; j < count; j++)
			{
				if (sz[j] == -x[i])
				{
					HAWK_MMGR_FREE(mmgr, p[j]);
					count--;
					p[j] = p[count];
					sz[j] = sz[count];	
				}
			}	
		}
	}
	/*hawk_xma_dump(mmgr->ctx, print_xma, HAWK_NULL);*/

	OK_X(test2_bad == 0);
	for (j = 0; j < count; j++) HAWK_MMGR_FREE(mmgr, p[j]);
}

int main(int argc, char* argv[])
{
	int test_bad = 0;
	hawk_mmgr_t xma_mmgr;
	hawk_oow_t max_alloc_size = MAX_ALLOC_SIZE;
	hawk_oow_t num_iterations = NUM_ITERATIONS;
	hawk_oow_t max_alloc_active = MAX_ALLOC_ACTIVE;
	hawk_oow_t pool_size = 1000000;

	Allocation* allocations; /* pool of active allocations */
	size_t num_active = 0;
	size_t i;

	clock_t start_time, end_time;
	double malloc_time = 0.0, free_time = 0.0;

	if (argc >= 5)
	{
		num_iterations = strtoul(argv[1], NULL, 10);
		max_alloc_size = strtoul(argv[2], NULL, 10);
		max_alloc_active = strtoul(argv[3], NULL, 10);
		pool_size = strtoul(argv[4], NULL, 10);
	}
	else if (argc != 1)
	{
		fprintf (stderr, "Usage: %s num_iterations max_alloc_size max_alloc_active pool_size\n", argv[0]);
		return -1;
	}

	no_plan();
	hawk_init_xma_mmgr(&xma_mmgr, pool_size);

	allocations = HAWK_MMGR_ALLOC(&xma_mmgr, max_alloc_active * sizeof(*allocations));
	if (!allocations)
	{
		bail_out("base allocation failed");
		return -1;	
	}
	
	/* ------------------------------- */
	test1(&xma_mmgr);
	test2(&xma_mmgr);

	/* ------------------------------- */

	srand((unsigned int)time(NULL));
	start_time = clock();

	for (i = 0; i < num_iterations; ++i)
	{
		int do_alloc = (num_active == 0) || (rand() % 2 == 0 && num_active < max_alloc_active);

		if (do_alloc) {
			size_t size = random_size(max_alloc_size);
			/*void *ptr = malloc(size);*/
			void *ptr = HAWK_MMGR_ALLOC(&xma_mmgr, size);
			if (!ptr) {
				fprintf(stderr, "malloc failed at operation %zu\n", i);
				test_bad = 1;
				break;
			}

			allocations[num_active].ptr = ptr;
			allocations[num_active].size = size;
			++num_active;
		} else {
			/* free a random active allocation */
			clock_t t1, t2;
			size_t index = rand() % num_active;
			void *ptr_to_free = allocations[index].ptr;

			t1 = clock();
			/*free(ptr_to_free); */
			HAWK_MMGR_FREE(&xma_mmgr, ptr_to_free);
			t2 = clock();
			free_time += (double)(t2 - t1) / CLOCKS_PER_SEC;

			/* replace with last active allocation */
			allocations[index] = allocations[num_active - 1];
			--num_active;
		}
	}
	OK_X(test_bad == 0);

	/* hawk_xma_dump(xma_mmgr.ctx, print_xma, HAWK_NULL); */

	/* Free remaining allocations */
	for (i = 0; i < num_active; ++i) {
		/* free(allocations[i].ptr); */
		HAWK_MMGR_FREE(&xma_mmgr, allocations[i].ptr);
	}

	end_time = clock();
	hawk_xma_dump(xma_mmgr.ctx, print_xma, HAWK_NULL);

	malloc_time = (double)(end_time - start_time) / CLOCKS_PER_SEC - free_time;

	printf("Performed %d interleaved malloc/free operations\n", num_iterations);
	printf("Total malloc time (estimated): %.6f seconds\n", malloc_time);
	printf("Total free time              : %.6f seconds\n", free_time);
	printf("Average time per operation   : %.9f seconds\n", (malloc_time + free_time) / num_iterations);

	HAWK_MMGR_FREE(&xma_mmgr, allocations);
	hawk_fini_xma_mmgr(&xma_mmgr);
	return exit_status();
}

