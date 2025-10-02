#include <hawk.h>
#include <hawk-xma.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tap.h"

#define NUM_ITERATIONS 10000
#define MIN_ALLOC_SIZE 16
#define MAX_ALLOC_SIZE 1024
				
#define OK_X(test) OK(test, #test)

static size_t random_size(hawk_oow_t max_alloc_size)
{
	return MIN_ALLOC_SIZE + rand() % (max_alloc_size - MIN_ALLOC_SIZE + 1);
}

int main(int argc, char* argv[])
{
	int test_bad = 0;
	hawk_mmgr_t xma_mmgr;
	hawk_oow_t num_iterations = NUM_ITERATIONS;
	hawk_oow_t max_alloc_size = MAX_ALLOC_SIZE;

	clock_t start_time, end_time;
	double malloc_time = 0.0, free_time = 0.0;
	void **ptr_array;
	size_t i;

	if (argc >= 3)
	{
		num_iterations = strtoul(argv[1], NULL, 10);
		max_alloc_size = strtoul(argv[2], NULL, 10);
	}

	no_plan();
	hawk_init_xma_mmgr(&xma_mmgr, num_iterations * max_alloc_size);

	srand((unsigned int)time(NULL));

	ptr_array = malloc(num_iterations * sizeof(void *));
	OK_X (ptr_array != NULL);
	if (!ptr_array) {
		fprintf(stderr, "malloc failed for pointer array\n");
		return -1;
	}

	start_time = clock();
	for (i = 0; i < num_iterations; ++i) {
		size_t size = random_size(max_alloc_size);
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
	for (i = 0; i < num_iterations; ++i) {
		/*free(ptr_array[i]);*/
		HAWK_MMGR_FREE(&xma_mmgr, ptr_array[i]);
	}
	end_time = clock();
	free_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

	free(ptr_array);

	printf("Performed %lu allocations and frees - min alloc size %lu max_alloc_size %lu\n",
		(unsigned long)num_iterations, (unsigned long)MIN_ALLOC_SIZE, (unsigned long)max_alloc_size);
	printf("Total malloc time: %.6f seconds\n", malloc_time);
	printf("Total free time  : %.6f seconds\n", free_time);
	printf("Average malloc time: %.9f seconds\n", malloc_time / num_iterations);
	printf("Average free time  : %.9f seconds\n", free_time / num_iterations);

	hawk_fini_xma_mmgr(&xma_mmgr);
	return exit_status();
}

