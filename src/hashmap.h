#ifndef HASHMAP_H
#define HASHMAP_H

#include "../libstaple/src/staple.h"
#include <stdlib.h>
#include <stdbool.h>


struct hashmap {
	struct sp_stack **buckets;
	size_t n_buckets;
};

struct hashmap *hashmap_create(size_t n_buckets);
void hashmap_copy(struct hashmap *dest, struct hashmap *src);
bool hashmap_contains_key(struct hashmap *hashmap, size_t key);
int hashmap_get(struct hashmap *hashmap, size_t key);
void hashmap_set(struct hashmap *hashmap, size_t key, int value);
int hashmap_unset(struct hashmap *hashmap, size_t key);
void hashmap_destroy(struct hashmap *hashmap);
void hashmap_stats(const struct hashmap *hashmap);

#endif /* HASHMAP_H */
