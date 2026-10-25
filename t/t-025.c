/* Array ownership, allocation failures, sparse operations, and heap ordering. */
#include "hawk-prv.h"
#include "tap.h"

typedef struct allocator_t allocator_t;
struct allocator_t
{
	hawk_oow_t live;
	hawk_oow_t realloc_calls;
	hawk_oow_t realloc_limit;
	hawk_oow_t free_calls;
	int fail_alloc;
	int fail_realloc;
	int custom_copy;
	void* protected_data;
	hawk_oow_t protected_frees;
};

static void* allocate (hawk_mmgr_t* mmgr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();
	void* ptr;

	if (a->fail_alloc) return HAWK_NULL;
	ptr = real->alloc(real, size);
	if (ptr) a->live++;
	return ptr;
}

static void* reallocate (hawk_mmgr_t* mmgr, void* ptr, hawk_oow_t size)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();
	int is_new = ptr == HAWK_NULL;
	void* result;

	a->realloc_calls++;
	if (a->fail_realloc || (a->realloc_limit > 0 && size > a->realloc_limit)) return HAWK_NULL;
	result = real->realloc(real, ptr, size);
	if (result && is_new) a->live++;
	return result;
}

static void deallocate (hawk_mmgr_t* mmgr, void* ptr)
{
	allocator_t* a = mmgr->ctx;
	hawk_mmgr_t* real = hawk_get_sys_mmgr();

	/* Record premature/repeated frees without crashing the rest of the suite.
	 * The ownership test releases this one buffer explicitly afterward. */
	if (ptr && ptr == a->protected_data)
	{
		a->protected_frees++;
		return;
	}
	if (ptr) a->live--;
	real->free(real, ptr);
}

static hawk_arr_t* open_array (hawk_t* hawk, allocator_t* a, hawk_oow_t capa)
{
	hawk_arr_t* arr;

	arr = hawk_arr_open(hawk_getgem(hawk), HAWK_SIZEOF(allocator_t*), capa);
	if (!arr)
	{
		bail_out("cannot allocate test array");
		return HAWK_NULL;
	}
	*(allocator_t**)hawk_arr_getxtn(arr) = a;
	return arr;
}

static void require_insert (hawk_arr_t* arr, hawk_oow_t index, void* data, hawk_oow_t len)
{
	if (hawk_arr_insert(arr, index, data, len) == HAWK_ARR_NIL) bail_out("cannot populate test array");
}

static void* copy_data (hawk_arr_t* arr, void* data, hawk_oow_t len)
{
	void* copy;

	copy = hawk_gem_allocmem(arr->gem, len);
	if (copy) HAWK_MEMCPY(copy, data, len);
	return copy;
}

static void count_free (hawk_arr_t* arr, void* data, hawk_oow_t len)
{
	allocator_t* a = *(allocator_t**)hawk_arr_getxtn(arr);

	a->free_calls++;
	if (a->custom_copy) hawk_gem_freemem(arr->gem, data);
}

static void free_owned (hawk_arr_t* arr, void* data, hawk_oow_t len)
{
	hawk_gem_freemem(arr->gem, data);
}

static hawk_oow_t small_sizer (hawk_arr_t* arr, hawk_oow_t hint)
{
	return 1;
}

static hawk_oow_t large_sizer (hawk_arr_t* arr, hawk_oow_t hint)
{
	return (hawk_oow_t)1048576;
}

static void test_resize (hawk_t* hawk, allocator_t* a)
{
	hawk_arr_t* arr;
	hawk_arr_style_t style;
	hawk_arr_slot_t** slots;
	hawk_oow_t i;

	arr = open_array(hawk, a, 5);
	style = *hawk_get_arr_style(HAWK_ARR_STYLE_INLINE_COPIER);
	style.freeer = count_free;
	hawk_arr_setstyle(arr, &style);
	for (i = 0; i < 5; i++) require_insert(arr, i, "x", 1);
	OK(hawk_arr_rsearch(arr, 0, 5, "x", 1) == 4,
		"reverse search includes the last element of its half-open range");
	OK(hawk_arr_rsearch(arr, 0, 0, "x", 1) == HAWK_ARR_NIL,
		"an empty reverse-search range does not underflow");
	OK(hawk_arr_rsearch(arr, 0, HAWK_TYPE_MAX(hawk_oow_t), "x", 1) == 4,
		"reverse search clamps an end beyond the array size");
	a->free_calls = 0;
	OK(hawk_arr_delete(arr, 1, 0) == 0 && hawk_arr_uplete(arr, 1, 0) == 0 &&
	   arr->size == 5 && arr->tally == 5 && a->free_calls == 0,
		"zero-count delete and uplete retain all elements");
	slots = arr->slot;
	a->fail_alloc = 1;
	OK(hawk_arr_setcapa(arr, 2) == HAWK_NULL, "failed shrink reports allocation failure");
	a->fail_alloc = 0;
	OK(arr->slot == slots && arr->capa == 5 && arr->size == 5 && arr->tally == 5 && a->free_calls == 0,
		"failed shrink preserves storage and elements without invoking freeers");
	OK(hawk_arr_setcapa(arr, 2) == arr && arr->capa == 2 && arr->size == 2 &&
	   arr->tally == 2 && a->free_calls == 3 && *(hawk_bch_t*)HAWK_ARR_DPTR(arr, 1) == 'x',
		"successful shrink retains the prefix and destroys only truncated elements");
	OK(hawk_arr_setcapa(arr, 0) == arr && arr->slot == HAWK_NULL && arr->capa == 0 &&
	   arr->size == 0 && arr->tally == 0 && a->free_calls == 5,
		"zero capacity clears elements and releases slot storage");
	OK(hawk_arr_rsearch(arr, 0, 0, "x", 1) == HAWK_ARR_NIL,
		"reverse search on an empty array is safe");
	hawk_arr_close(arr);
}

static void test_insert_failure (hawk_t* hawk, allocator_t* a)
{
	hawk_arr_t* arr;
	hawk_arr_style_t style;
	hawk_oow_t result, baseline;
	int mode;

	for (mode = 0; mode < 3; mode++)
	{
		baseline = a->live;
		arr = open_array(hawk, a, 1);
		style = *hawk_get_arr_style(HAWK_ARR_STYLE_DEFAULT);
		style.copier = mode == 0? HAWK_ARR_COPIER_SIMPLE: mode == 1? HAWK_ARR_COPIER_INLINE: copy_data;
		style.freeer = count_free;
		a->custom_copy = mode == 2;
		hawk_arr_setstyle(arr, &style);
		OK(hawk_arr_getcopier(arr) == style.copier, "copier getter returns the configured copier");
		require_insert(arr, 0, "old", 3);
		a->free_calls = 0;
		a->fail_alloc = 1;
		result = hawk_arr_insert(arr, 1, "new", 3);
		a->fail_alloc = 0;
		OK(result == HAWK_ARR_NIL && arr->size == 1 && arr->tally == 1 && a->free_calls == 0,
			"slot allocation failure preserves contents and caller ownership");
		a->fail_realloc = 1;
		result = hawk_arr_insert(arr, 1, "new", 3);
		a->fail_realloc = 0;
		OK(result == HAWK_ARR_NIL && arr->size == 1 && arr->tally == 1 &&
		   HAWK_ARR_DLEN(arr, 0) == 3 && HAWK_MEMCMP(HAWK_ARR_DPTR(arr, 0), "old", 3) == 0,
			"failed insertion preserves the previous element in each copier mode");
		OK(a->free_calls == (mode == 2? 1: 0),
			"failed insertion releases custom copies but retains caller-owned data");
		hawk_arr_close(arr);
		OK(a->live == baseline, "failed insertion leaves no allocations behind");
	}
	a->custom_copy = 0;
}

static void test_growth (hawk_t* hawk, allocator_t* a)
{
	hawk_arr_t* arr;
	hawk_arr_style_t style;
	hawk_oow_t result;
	int i;

	arr = open_array(hawk, a, 2);
	require_insert(arr, 0, "x", 1);
	require_insert(arr, 1, "y", 1);
	style = *hawk_get_arr_style(HAWK_ARR_STYLE_DEFAULT);
	style.sizer = small_sizer;
	hawk_arr_setstyle(arr, &style);
	OK(hawk_arr_insert(arr, 2, "z", 1) == 2 && arr->size == 3 &&
	   HAWK_MEMCMP(HAWK_ARR_DPTR(arr, 0), "x", 1) == 0 && HAWK_MEMCMP(HAWK_ARR_DPTR(arr, 1), "y", 1) == 0,
		"an undersized sizer result cannot truncate existing elements");
	hawk_arr_close(arr);

	arr = open_array(hawk, a, 1);
	require_insert(arr, 0, "x", 1);
	style.sizer = large_sizer;
	hawk_arr_setstyle(arr, &style);
	a->realloc_calls = 0;
	a->realloc_limit = 4 * HAWK_SIZEOF(hawk_arr_slot_t*);
	result = hawk_arr_insert(arr, 1, "y", 1);
	a->realloc_limit = 0;
	OK(result == 1 && arr->size == 2 && arr->capa >= 2 && arr->capa <= 4,
		"oversized sizer falls back to an affordable capacity");
	OK(a->realloc_calls <= HAWK_BITSOF(hawk_oow_t) + 1,
		"allocation retries decrease geometrically rather than one slot at a time");
	hawk_arr_close(arr);

	for (i = 0; i < 2; i++)
	{
		arr = open_array(hawk, a, 2);
		require_insert(arr, 0, "x", 1);
		require_insert(arr, 1, "y", 1);
		/* Huge indices need only a one-byte source; no huge allocation is needed. */
		result = hawk_arr_insert(arr, HAWK_TYPE_MAX(hawk_oow_t) - i, "z", 1);
		OK(result == HAWK_ARR_NIL, "an unrepresentable insertion index is rejected");
		OK(arr->size == 2 && arr->tally == 2 && arr->capa == 2 &&
		   HAWK_MEMCMP(HAWK_ARR_DPTR(arr, 0), "x", 1) == 0 &&
		   HAWK_MEMCMP(HAWK_ARR_DPTR(arr, 1), "y", 1) == 0,
			"rejecting an unrepresentable index preserves existing contents");
		hawk_arr_close(arr);
	}
}

static void test_same_pointer (hawk_t* hawk, allocator_t* a)
{
	hawk_arr_t* arr;
	hawk_arr_style_t style;
	hawk_bch_t* data;
	hawk_oow_t result;

	arr = open_array(hawk, a, 1);
	style = *hawk_get_arr_style(HAWK_ARR_STYLE_DEFAULT);
	style.freeer = free_owned;
	hawk_arr_setstyle(arr, &style);
	data = hawk_gem_allocmem(arr->gem, 6);
	if (!data) { bail_out("cannot allocate owned array data"); return; }
	HAWK_MEMCPY(data, "hello", 6);
	a->protected_data = data;
	a->protected_frees = 0;
	require_insert(arr, 0, data, 5);
	OK(hawk_arr_update(arr, 0, data, 5) == 0 && a->protected_frees == 0,
		"identical pointer and length retain the owned allocation");
	result = hawk_arr_update(arr, 0, data, 3);
	OK(result == 0 && HAWK_ARR_DPTR(arr, 0) == data && HAWK_ARR_DLEN(arr, 0) == 3,
		"same-pointer update changes the stored length");
	OK(a->protected_frees == 0,
		"same-pointer length update does not free data still stored in the array");
	hawk_arr_close(arr);
	OK(a->protected_frees == 1, "owned data is freed exactly once when the array closes");
	a->protected_data = HAWK_NULL;
	hawk_gem_freemem(hawk_getgem(hawk), data);
}

static hawk_uint32_t next_random (hawk_uint32_t* state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return *state;
}

static int test_array_model (hawk_t* hawk, allocator_t* a)
{
	hawk_arr_t* arr;
	hawk_uint32_t state = 1234567;
	hawk_bch_t choices[] = { 'a', 'b', 'c', 'd', 'e' };
	void* model[512];
	void* value;
	hawk_oow_t size = 0, tally, i, j, index, count, left, right, expected, step;
	hawk_arr_itr_t itr;
	hawk_ptl_t* elem;
	int good = 0;

	arr = open_array(hawk, a, 0);
	for (step = 0; step < 20000; step++)
	{
		index = next_random(&state) % 128;
		value = &choices[next_random(&state) % HAWK_COUNTOF(choices)];
		switch (next_random(&state) % 7)
		{
			case 0:
				if (hawk_arr_upsert(arr, index, value, 1) == HAWK_ARR_NIL) goto done;
				for (i = size; i < index; i++) model[i] = HAWK_NULL;
				model[index] = value;
				if (index >= size) size = index + 1;
				break;

			case 1:
				if (size >= 400) break;
				if (hawk_arr_insert(arr, index, value, 1) == HAWK_ARR_NIL) goto done;
				for (i = size; i < index; i++) model[i] = HAWK_NULL;
				for (i = size; i > index; i--) model[i] = model[i - 1];
				model[index] = value;
				size = index >= size? index + 1: size + 1;
				break;

			case 2:
				count = next_random(&state) % 129;
				if (index >= size) count = 0;
				else if (count > size - index) count = size - index;
				if (hawk_arr_delete(arr, index, count) != count) goto done;
				for (i = index + count; i < size; i++) model[i - count] = model[i];
				size -= count;
				break;

			case 3:
				count = next_random(&state) % 129;
				if (index >= size) count = 0;
				else if (count > size - index) count = size - index;
				if (hawk_arr_uplete(arr, index, count) != count) goto done;
				for (i = index; i < index + count; i++) model[i] = HAWK_NULL;
				break;

			case 4:
				if (!hawk_arr_setcapa(arr, index)) goto done;
				if (size > index) size = index;
				break;

			case 5:
				if (size > 0)
				{
					index %= size;
					if (hawk_arr_update(arr, index, value, 1) == HAWK_ARR_NIL) goto done;
					model[index] = value;
				}
				break;

			case 6:
				index = size? index % (size + 1): 0;
				if (!hawk_arr_setsize(arr, index)) goto done;
				size = index;
				break;
		}
		if (arr->size != size) goto done;
		tally = 0;
		for (i = 0; i < size; i++)
		{
			if (model[i]) tally++;
			if ((arr->slot[i] == HAWK_NULL) != (model[i] == HAWK_NULL)) goto done;
			if (model[i] && HAWK_ARR_DPTR(arr, i) != model[i]) goto done;
		}
		if (arr->tally != tally) goto done;
		i = 0;
		j = 0;
		for (elem = hawk_arr_getfirstelem(arr, &itr); elem; elem = hawk_arr_getnextelem(arr, &itr))
		{
			while (i < size && !model[i]) i++;
			if (i >= size || itr.idx != i || elem->ptr != model[i] || elem->len != 1) goto done;
			i++;
			j++;
		}
		if (j != tally) goto done;
		left = next_random(&state) % (size + 1);
		right = next_random(&state) % (size + 1);
		if (left > right) { i = left; left = right; right = i; }
		expected = HAWK_ARR_NIL;
		for (i = left; i < right; i++)
		{
			if (model[i] == value) { expected = i; break; }
		}
		if (hawk_arr_search(arr, left, right, value, 1) != expected) goto done;
		expected = HAWK_ARR_NIL;
		for (i = right; i > left; )
		{
			--i;
			if (model[i] == value) { expected = i; break; }
		}
		if (hawk_arr_rsearch(arr, left, right, value, 1) != expected) goto done;
	}
	good = 1;

done:
	hawk_arr_close(arr);
	return good;
}

struct heap_item_t
{
	int priority;
	hawk_oow_t serial;
	hawk_oow_t position;
};

static int compare_priority (hawk_arr_t* arr, const void* x, hawk_oow_t xlen, const void* y, hawk_oow_t ylen)
{
	const struct heap_item_t* a = x;
	const struct heap_item_t* b = y;

	return (a->priority > b->priority) - (a->priority < b->priority);
}

static int test_heap_model (hawk_t* hawk, allocator_t* a)
{
	hawk_arr_t* arr;
	hawk_arr_style_t style;
	hawk_uint32_t state = 1234567;
	struct heap_item_t incoming;
	struct heap_item_t* stored;
	struct heap_item_t* parent;
	hawk_oow_t step, i, index;
	unsigned int op;
	int good = 0;

	arr = open_array(hawk, a, 0);
	style = *hawk_get_arr_style(HAWK_ARR_STYLE_INLINE_COPIER);
	style.comper = compare_priority;
	hawk_arr_setstyle(arr, &style);
	hawk_arr_setheapposoffset(arr, HAWK_OFFSETOF(struct heap_item_t, position));
	for (step = 0; step < 100000; step++)
	{
		op = next_random(&state) % 3;
		incoming.priority = next_random(&state) % 50;
		incoming.serial = step;
		incoming.position = HAWK_ARR_NIL;
		if (arr->size == 0 || (op == 0 && arr->size < 128))
		{
			if (hawk_arr_pushheap(arr, &incoming, HAWK_SIZEOF(incoming)) == HAWK_ARR_NIL) goto done;
		}
		else if (op == 1)
		{
			index = next_random(&state) % arr->size;
			if (hawk_arr_updateheap(arr, index, &incoming, HAWK_SIZEOF(incoming)) == HAWK_ARR_NIL) goto done;
			for (i = 0; i < arr->size; i++)
			{
				stored = HAWK_ARR_DPTR(arr, i);
				if (stored->serial == incoming.serial && stored->priority == incoming.priority) break;
			}
			if (i == arr->size) goto done;
		}
		else hawk_arr_deleteheap(arr, next_random(&state) % arr->size);
		if (arr->size != arr->tally) goto done;
		for (i = 0; i < arr->size; i++)
		{
			stored = HAWK_ARR_DPTR(arr, i);
			if (stored->position != i) goto done;
			if (i > 0)
			{
				parent = HAWK_ARR_DPTR(arr, (i - 1) / 2);
				if (stored->priority > parent->priority) goto done;
			}
		}
	}
	good = 1;

done:
	hawk_arr_close(arr);
	return good;
}

int main (void)
{
	allocator_t a;
	hawk_mmgr_t mmgr = { allocate, reallocate, deallocate, &a };
	hawk_t* hawk;
	hawk_oow_t baseline;

	HAWK_MEMSET(&a, 0, HAWK_SIZEOF(a));
	no_plan();
	hawk = hawk_openstdwithmmgr(&mmgr, 0, HAWK_NULL, HAWK_NULL);
	if (!hawk) { bail_out("cannot open interpreter"); return 1; }
	baseline = a.live;
	test_resize(hawk, &a);
	test_insert_failure(hawk, &a);
	test_growth(hawk, &a);
	test_same_pointer(hawk, &a);
	OK(test_array_model(hawk, &a), "20000 sparse array operations agree with an independent model");
	OK(test_heap_model(hawk, &a), "100000 heap operations preserve ordering, replacements, and stored positions");
	OK(a.live == baseline, "array tests release all temporary allocations");
	hawk_close(hawk);
	OK(a.live == 0, "test allocator has no live blocks after interpreter cleanup");
	return exit_status();
}
