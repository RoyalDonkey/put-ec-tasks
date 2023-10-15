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
bool hashmap_contains_key(struct hashmap *hashmap, int x1, int y1, int x2, int y2);
int hashmap_get(struct hashmap *hashmap, int x1, int y1, int x2, int y2);
void hashmap_set(struct hashmap *hashmap, int x1, int y1, int x2, int y2, int value);
int hashmap_unset(struct hashmap *hashmap, int x1, int y1, int x2, int y2);
void hashmap_destroy(struct hashmap *hashmap);
size_t hashmap_hash(int x1, int y1, int x2, int y2);
void hashmap_stats(const struct hashmap *hashmap);

#endif /* HASHMAP_H */
