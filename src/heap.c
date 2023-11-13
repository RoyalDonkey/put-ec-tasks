#include "heap.h"
#include "helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Private functions */
void heapify(struct tsp_heap *heap);


struct tsp_heap *tsp_heap_create(size_t elem_size, size_t capacity, bool (*cmp)(const void*, const void*))
{
	assert(elem_size != 0);
	assert(capacity != 0);
	assert(elem_size < 64);  /* Because of stack memory used for swaps */
	struct tsp_heap *ret;
	ret = malloc_or_die(sizeof(struct tsp_heap));
	ret->elem_size = elem_size;
	ret->data = malloc_or_die(elem_size * capacity);
	ret->size = 0;
	ret->capacity = capacity;
	ret->cmp = cmp;
	return ret;
}

void tsp_heap_push(struct tsp_heap *heap, const void *elem)
{
	assert(heap != NULL);
	if (heap->size == heap->capacity) {
		heap->capacity *= 2;
		heap->data = realloc(heap->data, heap->elem_size * heap->capacity);
	}
	void *const dest = (char*)heap->data + (heap->size * heap->elem_size);
	memcpy(dest, elem, heap->elem_size);
	for (size_t i = heap->size; i != 0; i /= 2) {
		void *const current = (char*)heap->data + (i * heap->elem_size);
		void *const parent = (char*)heap->data + (i / 2 * heap->elem_size);
		if (heap->cmp(parent, current)) {
			char tmp[64];
			memcpy(tmp, parent, heap->elem_size);
			memcpy(parent, current, heap->elem_size);
			memcpy(current, tmp, heap->elem_size);
		}
	}
	++heap->size;
}

void *tsp_heap_get(const struct tsp_heap *heap)
{
	assert(heap != NULL);
	assert(heap->size != 0);
	return heap->data;
}

void tsp_heap_pop(struct tsp_heap *heap)
{
	assert(heap != NULL);
	assert(heap->size != 0);
	void *const src = (char*)heap->data + ((heap->size - 1) * heap->elem_size);
	memcpy(heap->data, src, heap->elem_size);
	--heap->size;
	heapify(heap);
}

void tsp_heap_print(const struct tsp_heap *heap, void (*func)(size_t, const void*))
{
	assert(heap != NULL);
	printf("tsp_heap_print()\n");
	printf("size/capacity: %zu/%zu  elem_size: %zu\n", heap->size, heap->capacity, heap->elem_size);
	for (size_t i = 0; i < heap->size; i++) {
		const void *const elem = (char*)heap->data + (i * heap->elem_size);
		func(i, elem);
	}
}

void tsp_heap_destroy(struct tsp_heap *heap)
{
	assert(heap != NULL);
	free(heap->data);
	free(heap);
}

void heapify(struct tsp_heap *heap)
{
	size_t index = 0;
	while (true) {
		const size_t left_idx = 2 * index + 1;
		const size_t right_idx = 2 * index + 2;
		void *const current = (char*)heap->data + (index * heap->elem_size);
		void *const left = (char*)heap->data + (left_idx * heap->elem_size);
		void *const right = (char*)heap->data + (right_idx * heap->elem_size);
		size_t min_idx = index;
		if (left_idx < heap->size && heap->cmp(current, left)) {
			min_idx = left_idx;
			if (right_idx < heap->size && heap->cmp(left, right)) {
				min_idx = right_idx;
			}
		} else if (right_idx < heap->size && heap->cmp(current, right)) {
			min_idx = right_idx;
			if (left_idx < heap->size && heap->cmp(right, left)) {
				min_idx = left_idx;
			}
		}
		if (min_idx != index) {
			void *const src = min_idx == left_idx ? left : right;
			char tmp[64];
			memcpy(tmp, current, heap->elem_size);
			memcpy(current, src, heap->elem_size);
			memcpy(src, tmp, heap->elem_size);
			index = min_idx;
		} else {
			break;
		}
	}
}
