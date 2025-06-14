#include <hawk-xma.h>
#include <hawk-std.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tap.h"

#define NUM_OPERATIONS 1000000
#define MAX_ALLOCATIONS 10000
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

static size_t random_size()
{
	return MIN_ALLOC_SIZE + rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE + 1);
}

int main()
{
	int test_bad = 0;
	hawk_mmgr_t xma_mmgr;

	Allocation allocations[MAX_ALLOCATIONS] = {0}; /* pool of active allocations */
	size_t num_active = 0;

	clock_t start_time, end_time;
	double malloc_time = 0.0, free_time = 0.0;

	no_plan();
	hawk_init_xma_mmgr(&xma_mmgr, NUM_OPERATIONS * MAX_ALLOC_SIZE);

	srand((unsigned int)time(NULL));
	start_time = clock();

	for (size_t i = 0; i < NUM_OPERATIONS; ++i)
	{
		int do_alloc = (num_active == 0) || (rand() % 2 == 0 && num_active < MAX_ALLOCATIONS);

		if (do_alloc) {
			size_t size = random_size();
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
			/* Free a random active allocation */
			size_t index = rand() % num_active;
			void *ptr_to_free = allocations[index].ptr;

			clock_t t1 = clock();
			/*free(ptr_to_free); */
			HAWK_MMGR_FREE(&xma_mmgr, ptr_to_free);
			clock_t t2 = clock();
			free_time += (double)(t2 - t1) / CLOCKS_PER_SEC;

			/* Replace with last active allocation */
			allocations[index] = allocations[num_active - 1];
			--num_active;
		}
	}
	OK_X(test_bad == 0);

	/* hawk_xma_dump(xma_mmgr.ctx, print_xma, HAWK_NULL); */

	/* Free remaining allocations */
	for (size_t i = 0; i < num_active; ++i) {
		/* free(allocations[i].ptr); */
		HAWK_MMGR_FREE(&xma_mmgr, allocations[i].ptr);
	}

	end_time = clock();
	hawk_xma_dump(xma_mmgr.ctx, print_xma, HAWK_NULL);

	malloc_time = (double)(end_time - start_time) / CLOCKS_PER_SEC - free_time;

	printf("Performed %d interleaved malloc/free operations\n", NUM_OPERATIONS);
	printf("Total malloc time (estimated): %.6f seconds\n", malloc_time);
	printf("Total free time              : %.6f seconds\n", free_time);
	printf("Average time per operation   : %.9f seconds\n", (malloc_time + free_time) / NUM_OPERATIONS);

	return exit_status();
}

