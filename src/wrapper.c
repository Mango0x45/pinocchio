#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void *
xmalloc(size_t n)
{
	void *p = malloc(n);
	if (p == NULL)
		err(1, "malloc");
	return p;
}

void *
xrealloc(void *p, size_t n)
{
	if ((p = realloc(p, n)) == NULL)
		err(1, "realloc");
	return p;
}

bool
streq(const char *x, const char *y)
{
	return strcmp(x, y) == 0;
}
