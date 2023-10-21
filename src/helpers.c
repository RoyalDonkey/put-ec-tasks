#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <string.h>

void stderr_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void *malloc_or_die(size_t n_bytes)
{
	void *ret;
	ret = malloc(n_bytes);
	if (ret == NULL) {
		error(("malloc failed -- out of memory!"));
		exit(EXIT_FAILURE);
	}
	return ret;
}

double euclidean_dist(double x1, double y1, double x2, double y2)
{
	const double dx = x1 - x2;
	const double dy = y1 - y2;
	return sqrt((dx * dx) + (dy * dy));
}

/* Sets the seed for the random numbers generator. Use -1 for `time(NULL)`. */
void random_seed(int seed)
{
	srand(seed == -1 ? time(NULL) : seed);
}

/* Returns a random integer between <min; max> (both-inclusive). */
int randint(int min, int max)
{
	return min == max ? min : min + (rand() % (max - min + 1));
}

/* Shuffle a buffer so that its first n elements are random */
void shuffle(void *buf, size_t elem_size, size_t buf_size, size_t n)
{
	assert(buf_size >= n);
	assert(elem_size <= 64);
	char tmp[64];  /* Use stack memory for faster access */
	for (size_t i = 0; i < n; i++) {
		const size_t idx = randint(i, buf_size - 1);
		void *const src = (char*)buf + (idx * elem_size);
		void *const dest = (char*)buf + (i * elem_size);
		memcpy(tmp, dest, elem_size);
		memcpy(dest, src, elem_size);
		memcpy(src, tmp, elem_size);
	}
}
