#ifndef HASHMAP_H
#define HASHMAP_H

#include "../libstaple/src/staple.h"
#include <stdlib.h>
#include <stdbool.h>


struct hashmap {
	struct sp_stack **buckets;
	size_t n_buckets;
	size_t size;
};

struct hashmap_pair {
	size_t key;
	int value;
};

struct hashmap *hashmap_create(size_t n_buckets);
void hashmap_copy(struct hashmap *dest, struct hashmap *src);
bool hashmap_contains_key(const struct hashmap *hashmap, size_t key);
int hashmap_get(const struct hashmap *hashmap, size_t key);
struct hashmap_pair hashmap_pop_next(struct hashmap *hashmap);
void hashmap_set(struct hashmap *hashmap, size_t key, int value);
int hashmap_unset(struct hashmap *hashmap, size_t key);
void hashmap_destroy(struct hashmap *hashmap);
void hashmap_stats(const struct hashmap *hashmap);

#endif /* HASHMAP_H */
