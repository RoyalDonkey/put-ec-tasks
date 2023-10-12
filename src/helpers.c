#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

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

/* Sets the seed for the random numbers generator. Use -1 for `time(NULL)`. */
void random_seed(int seed)
{
	srand(seed == -1 ? time(NULL) : seed);
}

/* Returns a random integer between <min; max> (both-inclusive). */
int randint(int min, int max)
{
	return min == max ? min : min + (rand() % (max - min));
}
