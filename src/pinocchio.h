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

void astprocess_cli(ast_t);
void astprocess_latex(ast_t);
void user_error(const char *, ...)
#if __GNUC__
	__attribute__((format(printf, 1, 2)))
#endif
	;

#endif /* !PINOCCIO_H */
