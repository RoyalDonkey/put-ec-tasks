#ifndef TSP_HELPERS_H
#define TSP_HELPERS_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Macros */
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define ARRLEN(X) (sizeof((X)) / sizeof(*(X)))
#define ROUND(X) ((unsigned long)((X) + 0.5))
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
double euclidean_dist(double x1, double y1, double x2, double y2);
void random_seed(int seed);
int randint(int min, int max);
void shuffle(void *buf, size_t elem_size, size_t buf_size, size_t n);


#endif /* TSP_HELPERS_H */
