#ifndef PINOCCIO_H
#define PINOCCIO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct eqn {
	int type;
	union {
		struct {
			struct eqn *lhs, *rhs;
		};
		char ch;
	};
} eqn_t;

typedef struct {
	eqn_t    *eqn;
	uint64_t vars;
} ast_t;

void astprocess(ast_t);

#endif /* !PINOCCIO_H */
