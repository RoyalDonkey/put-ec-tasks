#include "hashmap.h"
#include "helpers.h"

#define BUCKET_INIT_SIZE 16

struct hashmap_pair {
	size_t key;
	int value;
};

size_t hashmap_hash(size_t key);

struct hashmap *hashmap_create(size_t n_buckets)
{
	struct hashmap *const hm = malloc_or_die(sizeof(struct hashmap));
	hm->buckets = malloc_or_die(n_buckets * sizeof(struct sp_stack*));
	for (size_t i = 0; i < n_buckets; i++)
		hm->buckets[i] = sp_stack_create(sizeof(struct hashmap_pair), BUCKET_INIT_SIZE);
	hm->n_buckets = n_buckets;
	return hm;
}

void hashmap_copy(struct hashmap *dest, struct hashmap *src)
{
	if (dest->n_buckets != src->n_buckets) {
		for (size_t i = 0; i < dest->n_buckets; i++)
			sp_stack_destroy(dest->buckets[i], NULL);
		dest->buckets = realloc(dest->buckets, src->n_buckets * sizeof(struct sp_stack*));
		for (size_t i = 0; i < src->n_buckets; i++)
			dest->buckets[i] = sp_stack_create(sizeof(struct hashmap_pair), src->buckets[i]->size);
	}
	for (size_t i = 0; i < src->n_buckets; i++)
		sp_stack_copy(dest->buckets[i], src->buckets[i], NULL);
	dest->n_buckets = src->n_buckets;
}

bool hashmap_contains_key(struct hashmap *hashmap, size_t key)
{
	size_t idx = hashmap_hash(key) % hashmap->n_buckets;
	const struct sp_stack *const bucket = hashmap->buckets[idx];
	for (size_t i = 0; i < bucket->size; i++) {
		struct hashmap_pair pair = *(struct hashmap_pair*)sp_stack_get(bucket, i);
		if (pair.key == key)
			return true;
	}
	return false;
}

int hashmap_get(struct hashmap *hashmap, size_t key)
{
	size_t idx = hashmap_hash(key) % hashmap->n_buckets;
	const struct sp_stack *const bucket = hashmap->buckets[idx];
	for (size_t i = 0; i < bucket->size; i++) {
		struct hashmap_pair pair = *(struct hashmap_pair*)sp_stack_get(bucket, i);
		if (pair.key == key)
			return pair.value;
	}
	error(("key not found: { (%d;%d), (%d;%d) }", key));
}

void hashmap_set(struct hashmap *hashmap, size_t key, int value)
{
	size_t idx = hashmap_hash(key) % hashmap->n_buckets;
	struct sp_stack *const bucket = hashmap->buckets[idx];

	for (size_t i = 0; i < bucket->size; i++) {
		struct hashmap_pair *pair = sp_stack_get(bucket, i);
		if (pair->key == key) {
			pair->value = value;
			return;
		}
	}

	struct hashmap_pair pair;
	pair.key = key;
	pair.value = value;
	sp_stack_push(bucket, &pair);
}

int hashmap_unset(struct hashmap *hashmap, size_t key)
{
	size_t idx = hashmap_hash(key) % hashmap->n_buckets;
	struct sp_stack *const bucket = hashmap->buckets[idx];
	for (size_t i = 0; i < bucket->size; i++) {
		struct hashmap_pair pair = *(struct hashmap_pair*)sp_stack_get(bucket, i);
		if (pair.key == key) {
			sp_stack_qremove(bucket, i, NULL);
			return pair.value;
		}
	}
	error(("key not found: { (%d;%d), (%d;%d) }", key));
}

void hashmap_destroy(struct hashmap *hashmap)
{
	for (size_t i = 0; i < hashmap->n_buckets; i++)
		sp_stack_destroy(hashmap->buckets[i], NULL);
	free(hashmap->buckets);
	free(hashmap);
}

size_t hashmap_hash(size_t key)
{
	/* Adapted from the djb2 hash function. It was first reported
	 * by Dan Bernstein.
	 * Source: https://www.sparknotes.com/cs/searching/hashtables/section2/
	*/
	size_t ret = 5381;
	ret = ((ret << 5) + ret) + key;
	ret = ((ret << 5) + ret) + key;
	ret = ((ret << 5) + ret) + key;
	ret = ((ret << 5) + ret) + key;
	return ret * 33;
}

/* For debugging purposes */
void hashmap_stats(const struct hashmap *hashmap)
{
	size_t n_entries = 0;
	printf("hashmap stats:\n");
	printf("- no_buckets: %zu\n", hashmap->n_buckets);
	for (size_t i = 0; i < hashmap->n_buckets; i++) {
		const struct sp_stack *const bucket = hashmap->buckets[i];
		if (bucket->size == 0)
			continue;
		printf("    [%zu]  size: %zu\n", i, bucket->size);
		n_entries += bucket->size;
	}
	printf("- total no. entries: %zu\n", n_entries);
	printf("- approx. memory usage: %.2lf M\n", n_entries * sizeof(struct hashmap_pair) / (1024. * 1024.));
}
