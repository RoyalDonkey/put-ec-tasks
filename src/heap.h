#ifndef TSP_HEAP
#define TSP_HEAP

#include <stdlib.h>
#include <stdbool.h>

struct tsp_heap {
	void *data;
	size_t elem_size;
	size_t size;
	size_t capacity;
	/* cmp(parent, child) must return true iff they should be swapped */
	bool (*cmp)(const void*, const void*);
};

struct tsp_heap *tsp_heap_create(size_t elem_size, size_t capacity, bool (*cmp)(const void*, const void*));
void tsp_heap_push(struct tsp_heap *heap, const void *elem);
void *tsp_heap_get(const struct tsp_heap *heap);
void tsp_heap_pop(struct tsp_heap *heap);
void tsp_heap_print(const struct tsp_heap *heap, void (*func)(size_t, const void*));
void tsp_heap_destroy(struct tsp_heap *heap);

#endif /* TSP_HEAP */
