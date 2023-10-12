#ifndef TSP_HELPERS_H
#define TSP_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Macros */
#define info(x) do { \
	stderr_printf x; \
	fputc('\n', stderr); \
} while (0)
#define warn(x) do { \
	fprintf(stderr, "%s:%u: warning: ", __FILE__, __LINE__); \
	stderr_printf x; \
	fputc('\n', stderr); \
} while (0)
#define error(x) do { \
	fprintf(stderr, "%s:%u: error: ", __FILE__, __LINE__); \
	stderr_printf x; \
	fputc('\n', stderr); \
	exit(EXIT_FAILURE); \
} while (0)


/* Functions */
void stderr_printf(const char *fmt, ...);
void *malloc_or_die(size_t n_bytes);
void random_seed(int seed);
int randint(int min, int max);


#endif /* TSP_HELPERS_H */