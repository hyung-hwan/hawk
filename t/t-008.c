#include <hawk-xma.h>
#include <hawk-std.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tap.h"

#define NUM_ITERATIONS 1000000
#define MIN_ALLOC_SIZE 16
#define MAX_ALLOC_SIZE 1024
				
#define OK_X(test) OK(test, #test)

static size_t random_size()
{
	return MIN_ALLOC_SIZE + rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE + 1);
}

int main()
{
	int test_bad = 0;
	hawk_mmgr_t xma_mmgr;

	clock_t start_time, end_time;
	double malloc_time = 0.0, free_time = 0.0;
	void **ptr_array;

	no_plan();
	hawk_init_xma_mmgr(&xma_mmgr, NUM_ITERATIONS * MAX_ALLOC_SIZE);

	srand((unsigned int)time(NULL));

	ptr_array = malloc(NUM_ITERATIONS * sizeof(void *));
	OK_X (ptr_array != NULL);
	if (!ptr_array) {
		fprintf(stderr, "malloc failed for pointer array\n");
		return -1;
	}

	start_time = clock();
	for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
		size_t size = random_size();
		/*ptr_array[i] = malloc(size);*/
		ptr_array[i] = HAWK_MMGR_ALLOC(&xma_mmgr, size);
		if (!ptr_array[i]) {
			fprintf(stderr, "malloc failed at iteration %zu\n", i);
			test_bad = 1;
			break;
		}
	}

	OK_X (test_bad == 0);
	if (test_bad) return -1;

	end_time = clock();
	malloc_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

	start_time = clock();
	for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
		/*free(ptr_array[i]);*/
		HAWK_MMGR_FREE(&xma_mmgr, ptr_array[i]);
	}
	end_time = clock();
	free_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

	free(ptr_array);

	printf("Performed %d allocations and frees\n", NUM_ITERATIONS);
	printf("Total malloc time: %.6f seconds\n", malloc_time);
	printf("Total free time  : %.6f seconds\n", free_time);
	printf("Average malloc time: %.9f seconds\n", malloc_time / NUM_ITERATIONS);
	printf("Average free time  : %.9f seconds\n", free_time / NUM_ITERATIONS);

	return exit_status();
}

