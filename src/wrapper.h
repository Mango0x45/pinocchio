#ifndef PINOCCHIO_WRAPPER_H
#define PINOCCHIO_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
bool streq(const char *, const char *);

#endif /* !PINOCCHIO_WRAPPER_H */
