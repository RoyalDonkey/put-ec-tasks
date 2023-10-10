#include "helpers.h"
#include <stdio.h>
#include <stdarg.h>

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
